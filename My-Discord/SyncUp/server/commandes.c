/*
 * commandes.c — Implémente les gestionnaires de commandes du serveur.
 * Chaque fonction correspond à une commande du protocole (LOGIN, MSG, etc.)
 * Elle lit les arguments, interroge la DB si besoin, et répond au client.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>


#include "commandes.h"
#include "../include/crypto.h"

#define MAX_BUFFER 1024

/* ═══════════════════════════════════════════════════════════════════════
 * UTILITAIRES
 * ═══════════════════════════════════════════════════════════════════════ */

int role_db_vers_entier(const char *role_str) {
    if (strcmp(role_str, "admin")     == 0) return ROLE_ADMIN;
    if (strcmp(role_str, "moderator") == 0) return ROLE_MODERATEUR;
    return ROLE_MEMBRE;
}

void envoyer(SOCKET sock, const char *code, const char *message) {
    char reponse[MAX_BUFFER];
    if (message && strlen(message) > 0)
        snprintf(reponse, sizeof(reponse), "%s|%s\n", code, message);
    else
        snprintf(reponse, sizeof(reponse), "%s\n", code);
    send(sock, reponse, (int)strlen(reponse), 0);
}

void broadcast_canal(const char *message, int expediteur_id, int canal_id) {
    pthread_mutex_lock(&mutex_clients);
    for (int i = 0; i < nb_clients; i++) {
        if (clients[i].actif
            && clients[i].connecte
            && clients[i].client_id != expediteur_id
            && clients[i].canal_id  == canal_id)
        {
            send(clients[i].socket, message, (int)strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&mutex_clients);
}

/* ═══════════════════════════════════════════════════════════════════════
 * LOGIN|email|password
 * Vérifie les identifiants dans la DB et authentifie le client.
 * ═══════════════════════════════════════════════════════════════════════ */
void cmd_login(Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args) {
    if (nb_args < 2) {
        envoyer(client->socket, RESP_ERROR, "Usage: LOGIN|email|password");
        return;
    }

    char *email    = args[0];
    char *password = args[1];

    printf("[LOGIN] Client %d tente : %s\n", client->client_id, email);

    char query[512];
    snprintf(query, sizeof(query),
             "SELECT id, firstname, password, role FROM users WHERE email='%s'",
             email);

    PGresult *res = db_query(g_conn, query);
    if (res == NULL || PQntuples(res) == 0) {
        if (res) PQclear(res);
        envoyer(client->socket, RESP_ERROR, "Email ou mot de passe incorrect");
        return;
    }

    int         user_id     = atoi(PQgetvalue(res, 0, 0));
    const char *firstname   = PQgetvalue(res, 0, 1);
    const char *stored_hash = PQgetvalue(res, 0, 2);
    const char *role_str    = PQgetvalue(res, 0, 3);

    if (!verify_password(password, stored_hash)) {
        PQclear(res);
        envoyer(client->socket, RESP_ERROR, "Email ou mot de passe incorrect");
        return;
    }

    client->user_id  = user_id;
    client->connecte = 1;
    client->canal_id = 0;
    client->role     = role_db_vers_entier(role_str);
    strncpy(client->email, email,     sizeof(client->email) - 1);
    strncpy(client->nom,   firstname, sizeof(client->nom)   - 1);

    PQclear(res);

    printf("[LOGIN] '%s' authentifié (rôle: %s)\n", client->nom, role_str);

    char rep[128];
    snprintf(rep, sizeof(rep), "Bienvenue %s", client->nom);
    envoyer(client->socket, RESP_OK, rep);
}

/* ═══════════════════════════════════════════════════════════════════════
 * REGISTER|nom|prenom|email|password
 * Crée un compte utilisateur avec mot de passe haché.
 * ═══════════════════════════════════════════════════════════════════════ */
void cmd_register(Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args) {
    if (nb_args < 4) {
        envoyer(client->socket, RESP_ERROR, "Usage: REGISTER|nom|prenom|email|password");
        return;
    }

    char *nom      = args[0];
    char *prenom   = args[1];
    char *email    = args[2];
    char *password = args[3];

    char *hashed = hash_password(password);
    if (hashed == NULL) {
        envoyer(client->socket, RESP_ERROR, "Erreur interne lors du hachage");
        return;
    }

    char query[512];
    snprintf(query, sizeof(query),
             "INSERT INTO users (firstname, lastname, email, password, role) "
             "VALUES ('%s', '%s', '%s', '%s', 'member')",
             prenom, nom, email, hashed);
    free(hashed);

    if (!db_execute(g_conn, query)) {
        envoyer(client->socket, RESP_ERROR, "Email déjà utilisé ou erreur DB");
        return;
    }

    printf("[REGISTER] Nouvel utilisateur : %s %s (%s)\n", prenom, nom, email);
    envoyer(client->socket, RESP_OK, "Compte créé, vous pouvez vous connecter");
}

/* ═══════════════════════════════════════════════════════════════════════
 * MSG|canal_id|texte
 * Sauvegarde le message en DB et le diffuse au canal.
 * ═══════════════════════════════════════════════════════════════════════ */
void cmd_msg(Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args) {
    if (!client->connecte) {
        envoyer(client->socket, RESP_ERROR, "Vous devez être connecté");
        return;
    }
    if (nb_args < 2) {
        envoyer(client->socket, RESP_ERROR, "Usage: MSG|canal_id|texte");
        return;
    }

    int   canal_id = atoi(args[0]);
    char *texte    = args[1];

    /* Sauvegarde en DB */
    char query[1024];
    snprintf(query, sizeof(query),
             "INSERT INTO messages (user_id, channels_id, content) "
             "VALUES (%d, %d, '%s')",
             client->user_id, canal_id, texte);
    db_execute(g_conn, query);

    /* Diffusion aux autres membres du canal */
    char broadcast_msg[MAX_BUFFER];
    snprintf(broadcast_msg, sizeof(broadcast_msg),
             "%s|%d|%s|%s\n", RESP_BROADCAST, canal_id, client->nom, texte);
    broadcast_canal(broadcast_msg, client->client_id, canal_id);

    printf("[MSG] %s → canal %d : %s\n", client->nom, canal_id, texte);
}

/* ═══════════════════════════════════════════════════════════════════════
 * PRIVMSG|destinataire_id|texte
 * Chiffre le message avec XOR et l'envoie à un seul utilisateur.
 * ═══════════════════════════════════════════════════════════════════════ */
void cmd_privmsg(Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args) {
    if (!client->connecte) {
        envoyer(client->socket, RESP_ERROR, "Vous devez être connecté");
        return;
    }
    if (nb_args < 2) {
        envoyer(client->socket, RESP_ERROR, "Usage: PRIVMSG|destinataire_id|texte");
        return;
    }

    int   dest_id = atoi(args[0]);
    char *texte   = args[1];
    int   taille  = (int)strlen(texte);

    char chiffre[TAILLE_MAX_MESSAGE];
    xor_cipher(texte, taille, CLE_XOR_PRIVE, chiffre);

    pthread_mutex_lock(&mutex_clients);
    int trouve = 0;
    for (int i = 0; i < nb_clients; i++) {
        if (clients[i].actif && clients[i].client_id == dest_id) {
            /* Buffer agrandi : RESP_PRIVATE + nom + message chiffré
             * peuvent dépasser MAX_BUFFER si le message est long */
            char msg_prive[TAILLE_MAX_MESSAGE * 2];
            snprintf(msg_prive, sizeof(msg_prive),
                     "%s|%s|%s\n", RESP_PRIVATE, client->nom, chiffre);
            send(clients[i].socket, msg_prive, (int)strlen(msg_prive), 0);
            trouve = 1;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_clients);

    if (!trouve) envoyer(client->socket, RESP_ERROR, "Utilisateur introuvable");
    else         envoyer(client->socket, RESP_OK,    "Message privé envoyé");
}

/* ═══════════════════════════════════════════════════════════════════════
 * JOIN|canal_id
 * Change le canal actuel du client après vérification en DB.
 * ═══════════════════════════════════════════════════════════════════════ */
void cmd_join(Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args) {
    if (!client->connecte) {
        envoyer(client->socket, RESP_ERROR, "Vous devez être connecté");
        return;
    }
    if (nb_args < 1) {
        envoyer(client->socket, RESP_ERROR, "Usage: JOIN|canal_id");
        return;
    }

    int canal_id = atoi(args[0]);

    char query[256];
    snprintf(query, sizeof(query),
             "SELECT id, is_private FROM channels WHERE id=%d", canal_id);

    PGresult *res = db_query(g_conn, query);
    if (res == NULL || PQntuples(res) == 0) {
        if (res) PQclear(res);
        envoyer(client->socket, RESP_ERROR, "Canal introuvable");
        return;
    }

    const char *is_private = PQgetvalue(res, 0, 1);
    PQclear(res);

    if (strcmp(is_private, "t") == 0 && client->role < ROLE_MODERATEUR) {
        envoyer(client->socket, RESP_ERROR, "Canal privé : accès refusé");
        return;
    }

    client->canal_id = canal_id;
    printf("[JOIN] %s rejoint le canal %d\n", client->nom, canal_id);

    char rep[64];
    snprintf(rep, sizeof(rep), "Canal %d rejoint", canal_id);
    envoyer(client->socket, RESP_OK, rep);
}

/* ═══════════════════════════════════════════════════════════════════════
 * LIST — Envoie la liste des canaux depuis la DB.
 * ═══════════════════════════════════════════════════════════════════════ */
void cmd_list(Client *client) {
    if (!client->connecte) {
        envoyer(client->socket, RESP_ERROR, "Vous devez être connecté");
        return;
    }

    PGresult *res = db_query(g_conn, "SELECT id, name FROM channels ORDER BY id");
    if (res == NULL) {
        envoyer(client->socket, RESP_ERROR, "Erreur DB");
        return;
    }

    char liste[MAX_BUFFER] = "";
    int  nb = PQntuples(res);
    for (int i = 0; i < nb; i++) {
        char canal[128];
        snprintf(canal, sizeof(canal), "%s:%s",
                 PQgetvalue(res, i, 0), PQgetvalue(res, i, 1));
        strncat(liste, canal, sizeof(liste) - strlen(liste) - 2);
        if (i < nb - 1) strncat(liste, "|", sizeof(liste) - strlen(liste) - 1);
    }
    PQclear(res);

    envoyer(client->socket, RESP_CHANNEL_LIST, liste);
}

/* ═══════════════════════════════════════════════════════════════════════
 * USERS — Envoie la liste des utilisateurs connectés.
 * ═══════════════════════════════════════════════════════════════════════ */
void cmd_users(Client *client) {
    if (!client->connecte) {
        envoyer(client->socket, RESP_ERROR, "Vous devez être connecté");
        return;
    }

    char liste[MAX_BUFFER] = "";
    pthread_mutex_lock(&mutex_clients);
    for (int i = 0; i < nb_clients; i++) {
        if (clients[i].actif && clients[i].connecte) {
            strncat(liste, clients[i].nom,  sizeof(liste) - strlen(liste) - 2);
            strncat(liste, "|",             sizeof(liste) - strlen(liste) - 1);
        }
    }
    pthread_mutex_unlock(&mutex_clients);

    envoyer(client->socket, RESP_USER_LIST, liste);
}

/* ═══════════════════════════════════════════════════════════════════════
 * BAN|utilisateur_id
 * Bannis un utilisateur (modérateur/admin uniquement).
 * ═══════════════════════════════════════════════════════════════════════ */
void cmd_ban(Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args) {
    if (!client->connecte) {
        envoyer(client->socket, RESP_ERROR, "Vous devez être connecté");
        return;
    }
    if (client->role < ROLE_MODERATEUR) {
        envoyer(client->socket, RESP_ERROR, "Permission refusée");
        return;
    }
    if (nb_args < 1) {
        envoyer(client->socket, RESP_ERROR, "Usage: BAN|utilisateur_id");
        return;
    }

    int cible_id = atoi(args[0]);

    char query[512];
    snprintf(query, sizeof(query),
             "INSERT INTO bans (user_id, banned_by, reason, is_active) "
             "VALUES (%d, %d, 'Banni par modérateur', TRUE)",
             cible_id, client->user_id);
    db_execute(g_conn, query);

    pthread_mutex_lock(&mutex_clients);
    for (int i = 0; i < nb_clients; i++) {
        if (clients[i].actif && clients[i].user_id == cible_id) {
            printf("[BAN] %s banni par %s\n", clients[i].nom, client->nom);
            envoyer(clients[i].socket, RESP_ERROR, "Vous avez été banni");
            closesocket(clients[i].socket);
            clients[i].actif = 0;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_clients);

    envoyer(client->socket, RESP_OK, "Utilisateur banni");
}