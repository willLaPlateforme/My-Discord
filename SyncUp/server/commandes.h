#ifndef COMMANDES_H
#define COMMANDES_H

/*
 * commandes.h — Déclare la structure Client et toutes les fonctions
 * partagées entre les fichiers du serveur.
 *
 * Inclus par : commandes.c, moderation.c, channel_req.c, role_manager.c
 */

#include <pthread.h>
#include <winsock2.h>
#include "../include/db.h"       /* PGconn, db_query, db_execute  */
#include "../client/protocole.h" /* TAILLE_MAX_MESSAGE, RESP_*, CMD_*, ROLE_* */

/* ─── Structure Client ───────────────────────────────────────────────── */
typedef struct {
    SOCKET  socket;
    int     client_id;   /* Ordre de connexion                  */
    int     user_id;     /* ID réel dans la table users (DB)    */
    int     actif;
    char    nom[64];     /* Prénom affiché dans le chat         */
    char    email[128];
    int     role;        /* ROLE_MEMBRE / MODERATEUR / ADMIN    */
    int     connecte;    /* 1 = authentifié, 0 = pas encore     */
    int     canal_id;    /* Canal actuel                        */
} Client;

/* ─── Variables globales (définies dans main_server.c) ──────────────── */
#define MAX_CLIENTS 50
extern Client          clients[MAX_CLIENTS];
extern int             nb_clients;
extern pthread_mutex_t mutex_clients;
extern PGconn         *g_conn;

/* ─── Clé XOR pour les messages privés ──────────────────────────────── */
#define CLE_XOR_PRIVE "SyncUp_XOR_Key_2026"

/* ─── Utilitaires ────────────────────────────────────────────────────── */
int  role_db_vers_entier(const char *role_str);
void envoyer(SOCKET sock, const char *code, const char *message);
void broadcast_canal(const char *message, int expediteur_id, int canal_id);

/* ─── Gestionnaires de commandes ─────────────────────────────────────── */
void cmd_login   (Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args);
void cmd_register(Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args);
void cmd_msg     (Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args);
void cmd_privmsg (Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args);
void cmd_join    (Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args);
void cmd_list    (Client *client);
void cmd_users   (Client *client);
void cmd_ban     (Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args);

#endif /* COMMANDES_H */