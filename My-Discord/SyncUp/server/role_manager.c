/*
 * role_manager.c — Gestion des changements de rôle réseau.
 *
 * Seul un admin peut changer le rôle d'un autre utilisateur.
 * Chaque changement est tracé dans la table role_changes pour audit.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "role_manager.h"

#define MAX_BUFFER 1024

/* ═══════════════════════════════════════════════════════════════════════
 * role_valide()
 * Vérifie que la chaîne correspond à un rôle connu.
 * Retourne 1 si valide, 0 sinon.
 * ═══════════════════════════════════════════════════════════════════════ */
static int role_valide(const char *role) {
    return (strcmp(role, "member")    == 0 ||
            strcmp(role, "moderator") == 0 ||
            strcmp(role, "admin")     == 0);
}

/* ═══════════════════════════════════════════════════════════════════════
 * cmd_set_role()
 * Commande : SETROLE|user_id|nouveau_role
 *
 * Étapes :
 *   1. Vérifie que l'appelant est admin
 *   2. Récupère le rôle actuel de la cible en DB
 *   3. Met à jour le rôle dans users
 *   4. Enregistre le changement dans role_changes (audit)
 *   5. Notifie l'utilisateur ciblé s'il est connecté
 *   6. Met à jour son rôle en mémoire s'il est connecté
 * ═══════════════════════════════════════════════════════════════════════ */
void cmd_set_role(Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args) {
    if (!client->connecte) {
        envoyer(client->socket, RESP_ERROR, "Vous devez être connecté");
        return;
    }
    if (client->role < ROLE_ADMIN) {
        envoyer(client->socket, RESP_ERROR, "Réservé aux administrateurs");
        return;
    }
    if (nb_args < 2) {
        envoyer(client->socket, RESP_ERROR,
                "Usage: SETROLE|user_id|nouveau_role (member/moderator/admin)");
        return;
    }

    int   cible_id    = atoi(args[0]);
    char *nouveau_role = args[1];

    /* Vérifie que le rôle demandé est valide */
    if (!role_valide(nouveau_role)) {
        envoyer(client->socket, RESP_ERROR,
                "Rôle invalide. Valeurs acceptées : member, moderator, admin");
        return;
    }

    /* Empêche de changer son propre rôle */
    if (cible_id == client->user_id) {
        envoyer(client->socket, RESP_ERROR, "Vous ne pouvez pas modifier votre propre rôle");
        return;
    }

    /* Récupère le rôle actuel de la cible */
    char query_get[256];
    snprintf(query_get, sizeof(query_get),
             "SELECT role FROM users WHERE id=%d", cible_id);
    PGresult *res = db_query(g_conn, query_get);

    if (res == NULL || PQntuples(res) == 0) {
        if (res) PQclear(res);
        envoyer(client->socket, RESP_ERROR, "Utilisateur introuvable");
        return;
    }

    char ancien_role[32];
    strncpy(ancien_role, PQgetvalue(res, 0, 0), sizeof(ancien_role) - 1);
    PQclear(res);

    /* Inutile de changer si c'est déjà le bon rôle */
    if (strcmp(ancien_role, nouveau_role) == 0) {
        envoyer(client->socket, RESP_ERROR, "L'utilisateur a déjà ce rôle");
        return;
    }

    /* 1. Met à jour le rôle dans la table users */
    char query_upd[256];
    snprintf(query_upd, sizeof(query_upd),
             "UPDATE users SET role='%s' WHERE id=%d",
             nouveau_role, cible_id);

    if (!db_execute(g_conn, query_upd)) {
        envoyer(client->socket, RESP_ERROR, "Erreur DB lors de la mise à jour du rôle");
        return;
    }

    /* 2. Enregistre le changement dans role_changes pour audit */
    char query_log[512];
    snprintf(query_log, sizeof(query_log),
             "INSERT INTO role_changes (user_id, changed_by, old_role, new_role) "
             "VALUES (%d, %d, '%s', '%s')",
             cible_id, client->user_id, ancien_role, nouveau_role);
    db_execute(g_conn, query_log);

    printf("[SETROLE] user %d : %s → %s (par %s)\n",
           cible_id, ancien_role, nouveau_role, client->nom);

    /* 3. Met à jour le rôle en mémoire et notifie si l'utilisateur est connecté */
    pthread_mutex_lock(&mutex_clients);
    for (int i = 0; i < nb_clients; i++) {
        if (clients[i].actif && clients[i].user_id == cible_id) {
            clients[i].role = role_db_vers_entier(nouveau_role);

            char notif[MAX_BUFFER];
            snprintf(notif, sizeof(notif),
                     "Votre rôle a été changé : %s → %s",
                     ancien_role, nouveau_role);
            envoyer(clients[i].socket, RESP_OK, notif);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_clients);

    char rep[128];
    snprintf(rep, sizeof(rep),
             "Rôle de l'utilisateur %d changé : %s → %s",
             cible_id, ancien_role, nouveau_role);
    envoyer(client->socket, RESP_OK, rep);
}