/*
 * channel_req.c — Workflow de demande d'accès aux canaux privés.
 *
 * Flux complet :
 *   Membre      → REQUEST|canal_id
 *   Modo/Admin  → LISTREQ|canal_id  (voir les demandes)
 *   Modo/Admin  → APPREQ|demande_id (accepter)
 *   Modo/Admin  → REJREQ|demande_id (refuser)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "channel_req.h"

#define MAX_BUFFER 1024

/* ═══════════════════════════════════════════════════════════════════════
 * cmd_request_channel()
 * Commande : REQUEST|canal_id
 * Le membre demande l'accès à un canal privé.
 * ═══════════════════════════════════════════════════════════════════════ */
void cmd_request_channel(Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args) {
    if (!client->connecte) {
        envoyer(client->socket, RESP_ERROR, "Vous devez être connecté");
        return;
    }
    if (nb_args < 1) {
        envoyer(client->socket, RESP_ERROR, "Usage: REQUEST|canal_id");
        return;
    }

    int canal_id = atoi(args[0]);

    /* Vérifie que le canal existe et est bien privé */
    char query_check[256];
    snprintf(query_check, sizeof(query_check),
             "SELECT is_private FROM channels WHERE id=%d", canal_id);
    PGresult *res = db_query(g_conn, query_check);

    if (res == NULL || PQntuples(res) == 0) {
        if (res) PQclear(res);
        envoyer(client->socket, RESP_ERROR, "Canal introuvable");
        return;
    }

    const char *is_private = PQgetvalue(res, 0, 0);
    PQclear(res);

    if (strcmp(is_private, "f") == 0) {
        envoyer(client->socket, RESP_ERROR, "Ce canal est public, utilisez JOIN directement");
        return;
    }

    /* Vérifie qu'il n'y a pas déjà une demande en attente */
    char query_exist[512];
    snprintf(query_exist, sizeof(query_exist),
             "SELECT id FROM channel_requests "
             "WHERE user_id=%d AND channels_id=%d AND status='pending'",
             client->user_id, canal_id);
    PGresult *res_exist = db_query(g_conn, query_exist);

    if (res_exist && PQntuples(res_exist) > 0) {
        PQclear(res_exist);
        envoyer(client->socket, RESP_ERROR, "Vous avez déjà une demande en attente");
        return;
    }
    if (res_exist) PQclear(res_exist);

    /* Insère la demande en DB avec le statut 'pending' */
    char query[512];
    snprintf(query, sizeof(query),
             "INSERT INTO channel_requests (user_id, channels_id, status) "
             "VALUES (%d, %d, 'pending')",
             client->user_id, canal_id);

    if (!db_execute(g_conn, query)) {
        envoyer(client->socket, RESP_ERROR, "Erreur DB lors de la demande");
        return;
    }

    printf("[REQUEST] %s demande accès au canal %d\n", client->nom, canal_id);

    /* Notifie tous les modérateurs/admins connectés */
    char notif[MAX_BUFFER];
    snprintf(notif, sizeof(notif),
             "%s demande l'accès au canal %d (LISTREQ|%d pour voir les demandes)",
             client->nom, canal_id, canal_id);

    pthread_mutex_lock(&mutex_clients);
    for (int i = 0; i < nb_clients; i++) {
        if (clients[i].actif && clients[i].connecte
            && clients[i].role >= ROLE_MODERATEUR)
        {
            envoyer(clients[i].socket, RESP_OK, notif);
        }
    }
    pthread_mutex_unlock(&mutex_clients);

    envoyer(client->socket, RESP_OK, "Demande envoyée, en attente de validation");
}

/* ═══════════════════════════════════════════════════════════════════════
 * cmd_list_requests()
 * Commande : LISTREQ|canal_id
 * Liste les demandes en attente pour un canal (modo/admin seulement).
 * ═══════════════════════════════════════════════════════════════════════ */
void cmd_list_requests(Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args) {
    if (!client->connecte) {
        envoyer(client->socket, RESP_ERROR, "Vous devez être connecté");
        return;
    }
    if (client->role < ROLE_MODERATEUR) {
        envoyer(client->socket, RESP_ERROR, "Permission refusée");
        return;
    }
    if (nb_args < 1) {
        envoyer(client->socket, RESP_ERROR, "Usage: LISTREQ|canal_id");
        return;
    }

    int canal_id = atoi(args[0]);

    /* Récupère les demandes en attente avec le prénom de l'utilisateur */
    char query[512];
    snprintf(query, sizeof(query),
             "SELECT cr.id, u.firstname, u.lastname "
             "FROM channel_requests cr "
             "JOIN users u ON cr.user_id = u.id "
             "WHERE cr.channels_id=%d AND cr.status='pending'",
             canal_id);

    PGresult *res = db_query(g_conn, query);
    if (res == NULL) {
        envoyer(client->socket, RESP_ERROR, "Erreur DB");
        return;
    }

    int nb = PQntuples(res);
    if (nb == 0) {
        PQclear(res);
        envoyer(client->socket, RESP_OK, "Aucune demande en attente");
        return;
    }

    /* Format : "id:Prenom Nom|id:Prenom Nom|..." */
    char liste[MAX_BUFFER] = "";
    for (int i = 0; i < nb; i++) {
        char entree[128];
        snprintf(entree, sizeof(entree), "%s:%s %s",
                 PQgetvalue(res, i, 0),   /* id demande  */
                 PQgetvalue(res, i, 1),   /* firstname   */
                 PQgetvalue(res, i, 2));  /* lastname    */
        strncat(liste, entree, sizeof(liste) - strlen(liste) - 2);
        if (i < nb - 1) strncat(liste, "|", sizeof(liste) - strlen(liste) - 1);
    }
    PQclear(res);

    envoyer(client->socket, RESP_OK, liste);
}

/* ═══════════════════════════════════════════════════════════════════════
 * cmd_approve_request()
 * Commande : APPREQ|demande_id
 * Accepte une demande et notifie l'utilisateur.
 * ═══════════════════════════════════════════════════════════════════════ */
void cmd_approve_request(Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args) {
    if (!client->connecte) {
        envoyer(client->socket, RESP_ERROR, "Vous devez être connecté");
        return;
    }
    if (client->role < ROLE_MODERATEUR) {
        envoyer(client->socket, RESP_ERROR, "Permission refusée");
        return;
    }
    if (nb_args < 1) {
        envoyer(client->socket, RESP_ERROR, "Usage: APPREQ|demande_id");
        return;
    }

    int demande_id = atoi(args[0]);

    /* Récupère user_id et canal_id depuis la demande */
    char query_get[256];
    snprintf(query_get, sizeof(query_get),
             "SELECT user_id, channels_id FROM channel_requests WHERE id=%d",
             demande_id);
    PGresult *res = db_query(g_conn, query_get);

    if (res == NULL || PQntuples(res) == 0) {
        if (res) PQclear(res);
        envoyer(client->socket, RESP_ERROR, "Demande introuvable");
        return;
    }

    int user_id  = atoi(PQgetvalue(res, 0, 0));
    int canal_id = atoi(PQgetvalue(res, 0, 1));
    PQclear(res);

    /* Met à jour le statut en 'approved' */
    char query_upd[256];
    snprintf(query_upd, sizeof(query_upd),
             "UPDATE channel_requests SET status='approved' WHERE id=%d",
             demande_id);

    if (!db_execute(g_conn, query_upd)) {
        envoyer(client->socket, RESP_ERROR, "Erreur DB");
        return;
    }

    printf("[APPREQ] Demande %d approuvée par %s\n", demande_id, client->nom);

    /* Notifie l'utilisateur et met à jour son canal s'il est connecté */
    pthread_mutex_lock(&mutex_clients);
    for (int i = 0; i < nb_clients; i++) {
        if (clients[i].actif && clients[i].user_id == user_id) {
            char notif[MAX_BUFFER];
            snprintf(notif, sizeof(notif),
                     "Votre demande pour le canal %d a été acceptée ! "
                     "Utilisez JOIN|%d pour le rejoindre.", canal_id, canal_id);
            envoyer(clients[i].socket, RESP_OK, notif);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_clients);

    envoyer(client->socket, RESP_OK, "Demande approuvée");
}

/* ═══════════════════════════════════════════════════════════════════════
 * cmd_reject_request()
 * Commande : REJREQ|demande_id
 * Refuse une demande et notifie l'utilisateur.
 * ═══════════════════════════════════════════════════════════════════════ */
void cmd_reject_request(Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args) {
    if (!client->connecte) {
        envoyer(client->socket, RESP_ERROR, "Vous devez être connecté");
        return;
    }
    if (client->role < ROLE_MODERATEUR) {
        envoyer(client->socket, RESP_ERROR, "Permission refusée");
        return;
    }
    if (nb_args < 1) {
        envoyer(client->socket, RESP_ERROR, "Usage: REJREQ|demande_id");
        return;
    }

    int demande_id = atoi(args[0]);

    /* Récupère user_id et canal_id depuis la demande */
    char query_get[256];
    snprintf(query_get, sizeof(query_get),
             "SELECT user_id, channels_id FROM channel_requests WHERE id=%d",
             demande_id);
    PGresult *res = db_query(g_conn, query_get);

    if (res == NULL || PQntuples(res) == 0) {
        if (res) PQclear(res);
        envoyer(client->socket, RESP_ERROR, "Demande introuvable");
        return;
    }

    int user_id  = atoi(PQgetvalue(res, 0, 0));
    int canal_id = atoi(PQgetvalue(res, 0, 1));
    PQclear(res);

    /* Supprime la demande de la DB */
    char query_del[256];
    snprintf(query_del, sizeof(query_del),
             "DELETE FROM channel_requests WHERE id=%d", demande_id);

    if (!db_execute(g_conn, query_del)) {
        envoyer(client->socket, RESP_ERROR, "Erreur DB");
        return;
    }

    printf("[REJREQ] Demande %d refusée par %s\n", demande_id, client->nom);

    /* Notifie l'utilisateur s'il est connecté */
    pthread_mutex_lock(&mutex_clients);
    for (int i = 0; i < nb_clients; i++) {
        if (clients[i].actif && clients[i].user_id == user_id) {
            char notif[MAX_BUFFER];
            snprintf(notif, sizeof(notif),
                     "Votre demande pour le canal %d a été refusée.", canal_id);
            envoyer(clients[i].socket, RESP_ERROR, notif);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_clients);

    envoyer(client->socket, RESP_OK, "Demande refusée");
}