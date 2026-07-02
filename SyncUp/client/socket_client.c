/*
 * socket_client.c — Couche réseau du client SyncUp.
 *
 * Ce fichier gère :
 *   - La connexion au serveur (TCP/socket)
 *   - L'envoi des commandes du protocole
 *   - La réception des messages dans un thread séparé
 *   - L'appel des callbacks UI quand un message arrive
 *
 * L'UI (ui.c) appelle les fonctions reseau_*() déclarées dans ui_reseau.h.
 * Ce fichier appelle les fonctions ui_on_*() pour mettre à jour l'interface.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <gtk/gtk.h>

#include "protocole.h"
#include "ui_reseau.h"

#define MAX_BUFFER 1024

static SOCKET  sock = INVALID_SOCKET;
static int     reseau_actif = 0;

/* ═══════════════════════════════════════════════════════════════════════
 * ENVOI INTERNE
 * ═══════════════════════════════════════════════════════════════════════ */

static void envoyer_brut(const char *message) {
    if (sock == INVALID_SOCKET || !reseau_actif) return;
    send(sock, message, (int)strlen(message), 0);
}

/* ═══════════════════════════════════════════════════════════════════════
 * THREAD DE RÉCEPTION
 * g_idle_add() permet d'appeler les callbacks UI depuis le thread réseau
 * de façon thread-safe (GTK ne supporte qu'un seul thread).
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct { char auteur[64]; char texte[1024]; int canal_id; } DonneesMessage;
typedef struct { char texte[256]; } DonneesErreur;
typedef struct { char nom[64];   } DonneesLogin;
typedef struct { char liste[1024]; } DonneesListe;

static gboolean cb_message(gpointer data) {
    DonneesMessage *d = (DonneesMessage *)data;
    ui_on_message_recu(d->auteur, d->texte, d->canal_id);
    free(d); return G_SOURCE_REMOVE;
}
static gboolean cb_erreur(gpointer data) {
    DonneesErreur *d = (DonneesErreur *)data;
    ui_on_erreur(d->texte);
    free(d); return G_SOURCE_REMOVE;
}
static gboolean cb_login_ok(gpointer data) {
    DonneesLogin *d = (DonneesLogin *)data;
    ui_on_login_ok(d->nom);
    free(d); return G_SOURCE_REMOVE;
}
static gboolean cb_canaux(gpointer data) {
    DonneesListe *d = (DonneesListe *)data;
    ui_on_canaux_recus(d->liste);
    free(d); return G_SOURCE_REMOVE;
}
static gboolean cb_utilisateurs(gpointer data) {
    DonneesListe *d = (DonneesListe *)data;
    ui_on_utilisateurs_recus(d->liste);
    free(d); return G_SOURCE_REMOVE;
}

static void traiter_reponse(char *message) {
    char commande[TAILLE_MAX_MESSAGE];
    char args[8][TAILLE_MAX_MESSAGE];
    int  nb_args = 0;

    if (!parser_commande(message, commande, args, &nb_args)) return;

    if (strcmp(commande, RESP_BROADCAST) == 0 && nb_args >= 3) {
        DonneesMessage *d = malloc(sizeof(DonneesMessage));
        strncpy(d->auteur,  args[1], sizeof(d->auteur)  - 1);
        strncpy(d->texte,   args[2], sizeof(d->texte)   - 1);
        d->canal_id = atoi(args[0]);
        g_idle_add(cb_message, d);

    } else if (strcmp(commande, RESP_OK) == 0 && nb_args > 0) {
        if (strncmp(args[0], "Bienvenue ", 10) == 0) {
            DonneesLogin *d = malloc(sizeof(DonneesLogin));
            strncpy(d->nom, args[0] + 10, sizeof(d->nom) - 1);
            g_idle_add(cb_login_ok, d);
        }

    } else if (strcmp(commande, RESP_ERROR) == 0 && nb_args > 0) {
        DonneesErreur *d = malloc(sizeof(DonneesErreur));
        strncpy(d->texte, args[0], sizeof(d->texte) - 1);
        g_idle_add(cb_erreur, d);

    } else if (strcmp(commande, RESP_CHANNEL_LIST) == 0 && nb_args > 0) {
        DonneesListe *d = malloc(sizeof(DonneesListe));
        strncpy(d->liste, args[0], sizeof(d->liste) - 1);
        g_idle_add(cb_canaux, d);

    } else if (strcmp(commande, RESP_USER_LIST) == 0 && nb_args > 0) {
        DonneesListe *d = malloc(sizeof(DonneesListe));
        strncpy(d->liste, args[0], sizeof(d->liste) - 1);
        g_idle_add(cb_utilisateurs, d);
    }
}

static void *thread_reception(void *arg) {
    (void)arg;
    char buffer[MAX_BUFFER];
    int  octets;

    while (reseau_actif &&
           (octets = recv(sock, buffer, MAX_BUFFER - 1, 0)) > 0) {
        buffer[octets] = '\0';
        traiter_reponse(buffer);
    }

    reseau_actif = 0;
    DonneesErreur *d = malloc(sizeof(DonneesErreur));
    strncpy(d->texte, "Connexion au serveur perdue.", sizeof(d->texte) - 1);
    g_idle_add(cb_erreur, d);
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════
 * FONCTIONS PUBLIQUES — appelées par ui.c
 * ═══════════════════════════════════════════════════════════════════════ */

int reseau_connecter(const char *ip, int port) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 0;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) { WSACleanup(); return 0; }

    struct sockaddr_in serveur_addr;
    serveur_addr.sin_family = AF_INET;
    serveur_addr.sin_port   = htons(port);
    inet_pton(AF_INET, ip, &serveur_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serveur_addr,
                sizeof(serveur_addr)) == SOCKET_ERROR) {
        closesocket(sock); WSACleanup(); return 0;
    }

    reseau_actif = 1;
    return 1;
}

void reseau_demarrer_reception(void) {
    pthread_t tid;
    pthread_create(&tid, NULL, thread_reception, NULL);
    pthread_detach(tid);
}

void reseau_deconnecter(void) {
    reseau_actif = 0;
    if (sock != INVALID_SOCKET) {
        envoyer_brut(CMD_LOGOUT "\n");
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    WSACleanup();
}

void reseau_login(const char *email, const char *password) {
    char msg[MAX_BUFFER];
    snprintf(msg, sizeof(msg), "%s|%s|%s\n", CMD_LOGIN, email, password);
    envoyer_brut(msg);
}

void reseau_register(const char *nom, const char *prenom,
                     const char *email, const char *password) {
    char msg[MAX_BUFFER];
    snprintf(msg, sizeof(msg), "%s|%s|%s|%s|%s\n",
             CMD_REGISTER, nom, prenom, email, password);
    envoyer_brut(msg);
}

void reseau_logout(void) { envoyer_brut(CMD_LOGOUT "\n"); }

void reseau_envoyer_message(int canal_id, const char *texte) {
    char msg[TAILLE_MAX_MESSAGE * 2];
    snprintf(msg, sizeof(msg), "%s|%d|%s\n", CMD_SEND_MSG, canal_id, texte);
    envoyer_brut(msg);
}

void reseau_envoyer_prive(int dest_id, const char *texte) {
    char msg[TAILLE_MAX_MESSAGE * 2];
    snprintf(msg, sizeof(msg), "%s|%d|%s\n", CMD_PRIVATE_MSG, dest_id, texte);
    envoyer_brut(msg);
}

void reseau_reagir(int message_id, const char *emoji) {
    char msg[MAX_BUFFER];
    snprintf(msg, sizeof(msg), "%s|%d|%s\n", CMD_REACT, message_id, emoji);
    envoyer_brut(msg);
}

void reseau_rejoindre_canal(int canal_id) {
    char msg[MAX_BUFFER];
    snprintf(msg, sizeof(msg), "%s|%d\n", CMD_JOIN_CHANNEL, canal_id);
    envoyer_brut(msg);
}

void reseau_demander_acces(int canal_id) {
    char msg[MAX_BUFFER];
    snprintf(msg, sizeof(msg), "%s|%d\n", CMD_REQUEST_CHANNEL, canal_id);
    envoyer_brut(msg);
}

void reseau_lister_canaux(void)       { envoyer_brut(CMD_LIST_CHANNELS "\n"); }
void reseau_lister_utilisateurs(void) { envoyer_brut(CMD_LIST_USERS "\n");    }

void reseau_timeout(int user_id, int canal_id, int duree, const char *raison) {
    char msg[TAILLE_MAX_MESSAGE * 2];
    snprintf(msg, sizeof(msg), "%s|%d|%d|%d|%s\n",
             CMD_TIMEOUT, user_id, canal_id, duree, raison);
    envoyer_brut(msg);
}

void reseau_ban(int user_id) {
    char msg[MAX_BUFFER];
    snprintf(msg, sizeof(msg), "%s|%d\n", CMD_BAN, user_id);
    envoyer_brut(msg);
}

void reseau_changer_role(int user_id, const char *role) {
    char msg[MAX_BUFFER];
    snprintf(msg, sizeof(msg), "%s|%d|%s\n", CMD_SET_ROLE, user_id, role);
    envoyer_brut(msg);
}