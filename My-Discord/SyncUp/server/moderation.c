/*
 * moderation.c — Implémente les fonctions de modération réseau.
 * Gère les timeouts temporaires sur les canaux.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "moderation.h"

#define MAX_BUFFER 1024

/* ═══════════════════════════════════════════════════════════════════════
 * est_en_timeout()
 * Interroge la DB pour savoir si user_id est encore en timeout
 * sur canal_id (expires_at > maintenant).
 * Retourne 1 si oui, 0 sinon.
 * ═══════════════════════════════════════════════════════════════════════ */
int est_en_timeout(int user_id, int canal_id) {
    char query[512];
    snprintf(query, sizeof(query),
             "SELECT id FROM timeouts "
             "WHERE user_id=%d AND channel_id=%d AND expires_at > NOW()",
             user_id, canal_id);

    PGresult *res = db_query(g_conn, query);
    if (res == NULL) return 0;

    int en_timeout = (PQntuples(res) > 0);
    PQclear(res);
    return en_timeout;
}

/* ═══════════════════════════════════════════════════════════════════════
 * cmd_timeout()
 * Commande : TIMEOUT|user_id|canal_id|duree_secondes|raison
 *
 * - Vérifie que l'appelant est modérateur ou admin
 * - Insère un timeout en DB avec une date d'expiration calculée
 * - Notifie l'utilisateur ciblé s'il est connecté
 * ═══════════════════════════════════════════════════════════════════════ */
void cmd_timeout(Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args) {
    if (!client->connecte) {
        envoyer(client->socket, RESP_ERROR, "Vous devez être connecté");
        return;
    }
    if (client->role < ROLE_MODERATEUR) {
        envoyer(client->socket, RESP_ERROR, "Permission refusée");
        return;
    }
    if (nb_args < 4) {
        envoyer(client->socket, RESP_ERROR,
                "Usage: TIMEOUT|user_id|canal_id|duree_secondes|raison");
        return;
    }

    int  cible_id = atoi(args[0]);
    int  canal_id = atoi(args[1]);
    int  duree    = atoi(args[2]);  /* durée en secondes */
    char *raison  = args[3];

    /* Empêche de se timeout soi-même ou un admin */
    if (cible_id == client->user_id) {
        envoyer(client->socket, RESP_ERROR, "Vous ne pouvez pas vous timeout vous-même");
        return;
    }

    /* Vérifie que la cible n'est pas admin (un modo ne peut pas timeout un admin) */
    char query_check[256];
    snprintf(query_check, sizeof(query_check),
             "SELECT role FROM users WHERE id=%d", cible_id);
    PGresult *res_check = db_query(g_conn, query_check);
    if (res_check && PQntuples(res_check) > 0) {
        const char *role_cible = PQgetvalue(res_check, 0, 0);
        if (strcmp(role_cible, "admin") == 0 && client->role < ROLE_ADMIN) {
            PQclear(res_check);
            envoyer(client->socket, RESP_ERROR, "Impossible de timeout un administrateur");
            return;
        }
    }
    if (res_check) PQclear(res_check);

    /* Insère le timeout en DB.
     * NOW() + duree * interval '1 second' calcule la date d'expiration
     * directement dans PostgreSQL, sans avoir à gérer le temps côté C. */
    char query[512];
    snprintf(query, sizeof(query),
             "INSERT INTO timeouts (user_id, channel_id, reason, expires_at) "
             "VALUES (%d, %d, '%s', NOW() + %d * interval '1 second')",
             cible_id, canal_id, raison, duree);

    if (!db_execute(g_conn, query)) {
        envoyer(client->socket, RESP_ERROR, "Erreur DB lors du timeout");
        return;
    }

    printf("[TIMEOUT] user %d timeout %d secondes sur canal %d par %s\n",
           cible_id, duree, canal_id, client->nom);

    /* Notifie l'utilisateur ciblé s'il est actuellement connecté */
    pthread_mutex_lock(&mutex_clients);
    for (int i = 0; i < nb_clients; i++) {
        if (clients[i].actif && clients[i].user_id == cible_id) {
            char notif[MAX_BUFFER];
            snprintf(notif, sizeof(notif),
                     "Vous avez été mis en timeout sur le canal %d "
                     "pendant %d secondes. Raison : %s",
                     canal_id, duree, raison);
            envoyer(clients[i].socket, RESP_ERROR, notif);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_clients);

    char rep[128];
    snprintf(rep, sizeof(rep),
             "Utilisateur %d timeout pendant %d secondes", cible_id, duree);
    envoyer(client->socket, RESP_OK, rep);
}