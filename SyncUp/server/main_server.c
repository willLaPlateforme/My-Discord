/*
 * main_server.c — Point d'entrée du serveur MyDiscord.
 *
 * Rôle : initialiser la DB, le socket, accepter les connexions
 *        et router les messages vers le bon gestionnaire.
 *
 * Structure du projet :
 *   server/main_server.c   ← ici
 *   server/commandes.c/h   ← login, msg, join, ban...
 *   server/moderation.c/h  ← timeout
 *   server/channel_req.c/h ← demandes d'accès canaux privés
 *   server/role_manager.c/h← changement de rôle
 *   include/db.h + db.c    ← connexion PostgreSQL
 *   include/crypto.h + crypto.c ← hash + XOR
 *   client/protocole.h     ← protocole partagé client/serveur
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "../client/protocole.h"  /* protocole partagé          */
#include "../include/db.h"        /* db_connect, db_disconnect  */
#include "commandes.h"            /* Client, envoyer, cmd_*     */
#include "moderation.h"           /* cmd_timeout, est_en_timeout*/
#include "channel_req.h"          /* cmd_request_channel, ...   */
#include "role_manager.h"         /* cmd_set_role               */

/* ws2_32 est passé via le Makefile : -lws2_32 */

#define PORT       8080
#define MAX_BUFFER 1024

/* ═══════════════════════════════════════════════════════════════════════
 * VARIABLES GLOBALES
 * Déclarées ici, extern dans commandes.h pour les autres fichiers.
 * ═══════════════════════════════════════════════════════════════════════ */
SOCKET          g_socket_ecoute = INVALID_SOCKET;
Client          clients[MAX_CLIENTS];
int             nb_clients = 0;
pthread_mutex_t mutex_clients = PTHREAD_MUTEX_INITIALIZER;
PGconn         *g_conn = NULL;

/* ═══════════════════════════════════════════════════════════════════════
 * FERMETURE PROPRE (Ctrl+C)
 * ═══════════════════════════════════════════════════════════════════════ */
void fermeture_propre(int signum) {
    (void)signum;
    printf("\nArrêt du serveur...\n");
    if (g_socket_ecoute != INVALID_SOCKET) closesocket(g_socket_ecoute);
    if (g_conn != NULL) db_disconnect(g_conn);
    WSACleanup();
    exit(0);
}

/* ═══════════════════════════════════════════════════════════════════════
 * ROUTEUR — reçoit un message brut et appelle le bon gestionnaire
 * ═══════════════════════════════════════════════════════════════════════ */
void traiter_message(Client *client, char *message_brut) {
    char commande[TAILLE_MAX_MESSAGE];
    char args[8][TAILLE_MAX_MESSAGE];
    int  nb_args = 0;

    if (!parser_commande(message_brut, commande, args, &nb_args)) return;

    if      (strcmp(commande, CMD_LOGIN)          == 0) cmd_login(client, args, nb_args);
    else if (strcmp(commande, CMD_REGISTER)       == 0) cmd_register(client, args, nb_args);
    else if (strcmp(commande, CMD_SEND_MSG)       == 0) cmd_msg(client, args, nb_args);
    else if (strcmp(commande, CMD_PRIVATE_MSG)    == 0) cmd_privmsg(client, args, nb_args);
    else if (strcmp(commande, CMD_JOIN_CHANNEL)   == 0) cmd_join(client, args, nb_args);
    else if (strcmp(commande, CMD_LIST_CHANNELS)  == 0) cmd_list(client);
    else if (strcmp(commande, CMD_LIST_USERS)     == 0) cmd_users(client);
    else if (strcmp(commande, CMD_BAN)            == 0) cmd_ban(client, args, nb_args);
    else if (strcmp(commande, CMD_TIMEOUT)        == 0) cmd_timeout(client, args, nb_args);
    else if (strcmp(commande, CMD_REQUEST_CHANNEL)== 0) cmd_request_channel(client, args, nb_args);
    else if (strcmp(commande, CMD_LIST_REQUESTS)  == 0) cmd_list_requests(client, args, nb_args);
    else if (strcmp(commande, CMD_APPROVE_REQUEST)== 0) cmd_approve_request(client, args, nb_args);
    else if (strcmp(commande, CMD_REJECT_REQUEST) == 0) cmd_reject_request(client, args, nb_args);
    else if (strcmp(commande, CMD_SET_ROLE)       == 0) cmd_set_role(client, args, nb_args);
    else if (strcmp(commande, CMD_LOGOUT)         == 0) {
        printf("[LOGOUT] %s déconnecté\n", client->nom);
        envoyer(client->socket, RESP_OK, "À bientôt");
        client->actif = 0;
    }
    else envoyer(client->socket, RESP_ERROR, "Commande inconnue");
}

/* ═══════════════════════════════════════════════════════════════════════
 * THREAD CLIENT — une instance par client connecté
 * ═══════════════════════════════════════════════════════════════════════ */
void *gerer_client(void *arg) {
    Client *client = (Client *)arg;
    char    buffer[MAX_BUFFER];
    int     octets_recus;

    printf("[+] Client %d connecté.\n", client->client_id);

    while ((octets_recus = recv(client->socket, buffer, MAX_BUFFER - 1, 0)) > 0) {
        buffer[octets_recus] = '\0';
        traiter_message(client, buffer);
    }

    printf("[-] Client %d déconnecté.\n", client->client_id);

    pthread_mutex_lock(&mutex_clients);
    client->actif = 0;
    pthread_mutex_unlock(&mutex_clients);

    closesocket(client->socket);
    free(client);
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════
 * MAIN
 * ═══════════════════════════════════════════════════════════════════════ */
int main() {
    WSADATA wsa;
    SOCKET  socket_ecoute, socket_client;
    struct  sockaddr_in serveur_addr, client_addr;
    int     taille_addr = sizeof(client_addr);

    /* 1. Connexion PostgreSQL */
    g_conn = db_connect();
    if (g_conn == NULL) {
        fprintf(stderr, "Impossible de se connecter à la base de données.\n");
        return 1;
    }

    /* 2. Winsock */
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Erreur WSAStartup: %d\n", WSAGetLastError());
        db_disconnect(g_conn);
        return 1;
    }

    /* 3. Création du socket TCP */
    socket_ecoute = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ecoute == INVALID_SOCKET) {
        printf("Erreur socket(): %d\n", WSAGetLastError());
        db_disconnect(g_conn);
        WSACleanup();
        return 1;
    }
    g_socket_ecoute = socket_ecoute;
    signal(SIGINT, fermeture_propre);

    /* 4. Adresse d'écoute */
    serveur_addr.sin_family      = AF_INET;
    serveur_addr.sin_addr.s_addr = INADDR_ANY;
    serveur_addr.sin_port        = htons(PORT);

    /* 5. bind + listen */
    if (bind(socket_ecoute, (struct sockaddr *)&serveur_addr, sizeof(serveur_addr)) == SOCKET_ERROR) {
        printf("Erreur bind(): %d\n", WSAGetLastError());
        closesocket(socket_ecoute); db_disconnect(g_conn); WSACleanup();
        return 1;
    }
    if (listen(socket_ecoute, 10) == SOCKET_ERROR) {
        printf("Erreur listen(): %d\n", WSAGetLastError());
        closesocket(socket_ecoute); db_disconnect(g_conn); WSACleanup();
        return 1;
    }

    printf("====================================\n");
    printf("  Serveur MyDiscord -- port %d\n", PORT);
    printf("  Base de donnees : connectee\n");
    printf("  Ctrl+C pour arreter\n");
    printf("====================================\n");

    int prochain_id = 1;

    /* 6. Boucle d'acceptation */
    while (1) {
        socket_client = accept(socket_ecoute, (struct sockaddr *)&client_addr, &taille_addr);
        if (socket_client == INVALID_SOCKET) {
            printf("Erreur accept(): %d\n", WSAGetLastError());
            continue;
        }

        Client *nouveau_client = malloc(sizeof(Client));
        memset(nouveau_client, 0, sizeof(Client));
        nouveau_client->socket    = socket_client;
        nouveau_client->client_id = prochain_id++;
        nouveau_client->actif     = 1;
        nouveau_client->connecte  = 0;
        nouveau_client->canal_id  = 0;
        nouveau_client->role      = ROLE_MEMBRE;

        pthread_mutex_lock(&mutex_clients);
        clients[nb_clients++] = *nouveau_client;
        pthread_mutex_unlock(&mutex_clients);

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, gerer_client, nouveau_client);
        pthread_detach(thread_id);
    }

    closesocket(socket_ecoute);
    db_disconnect(g_conn);
    WSACleanup();
    return 0;
}