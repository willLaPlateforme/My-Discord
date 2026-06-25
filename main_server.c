#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define MAX_BUFFER 1024
#define MAX_CLIENTS 50

/* Socket global pour pouvoir le fermer depuis le gestionnaire de signal */
SOCKET g_socket_ecoute = INVALID_SOCKET;

/* Appelé sur Ctrl+C (SIGINT) : on ferme proprement avant de quitter,
   sinon le port reste "occupé" un moment côté OS et empêche un redémarrage rapide */
void fermeture_propre(int signum) {
    (void)signum;
    printf("\nArrêt du serveur, fermeture du socket...\n");
    if (g_socket_ecoute != INVALID_SOCKET) {
        closesocket(g_socket_ecoute);
    }
    WSACleanup();
    exit(0);
}

/* Représente un client connecté : son socket et son id de thread */
typedef struct {
    SOCKET socket;
    int client_id;
    int actif;
} Client;

Client clients[MAX_CLIENTS];
int nb_clients = 0;
pthread_mutex_t mutex_clients = PTHREAD_MUTEX_INITIALIZER;

/* Diffuse un message à tous les clients sauf l'expéditeur */
void broadcast(const char *message, int expediteur_id) {
    pthread_mutex_lock(&mutex_clients);
    for (int i = 0; i < nb_clients; i++) {
        if (clients[i].actif && clients[i].client_id != expediteur_id) {
            send(clients[i].socket, message, (int)strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&mutex_clients);
}

/* Fonction exécutée par chaque thread, une par client connecté */
void *gerer_client(void *arg) {
    Client *client = (Client *)arg;
    char buffer[MAX_BUFFER];
    int octets_recus;

    printf("[+] Client %d connecter.\n", client->client_id);

    while ((octets_recus = recv(client->socket, buffer, MAX_BUFFER - 1, 0)) > 0) {
        buffer[octets_recus] = '\0';
        printf("[Client %d] %s\n", client->client_id, buffer);

        char message_formate[MAX_BUFFER + 32];
        snprintf(message_formate, sizeof(message_formate), "[Client %d] %s", client->client_id, buffer);
        broadcast(message_formate, client->client_id);
    }

    /* recv() renvoie 0 si le client a fermé la connexion proprement,
       ou -1 en cas d'erreur réseau */
    printf("[-] Client %d deconnecter.\n", client->client_id);

    pthread_mutex_lock(&mutex_clients);
    client->actif = 0;
    pthread_mutex_unlock(&mutex_clients);

    closesocket(client->socket);
    free(client);
    return NULL;
}

int main() {
    WSADATA wsa;
    SOCKET socket_ecoute, socket_client;
    struct sockaddr_in serveur_addr, client_addr;
    int taille_addr = sizeof(client_addr);

    /* 1. Initialisation de Winsock (obligatoire sur Windows avant tout appel socket) */
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Erreur WSAStartup: %d\n", WSAGetLastError());
        return 1;
    }

    /* 2. Création du socket TCP (SOCK_STREAM) en IPv4 (AF_INET) */
    socket_ecoute = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ecoute == INVALID_SOCKET) {
        printf("Erreur socket(): %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    g_socket_ecoute = socket_ecoute;

    /* Intercepte Ctrl+C pour fermer le socket avant de quitter */
    signal(SIGINT, fermeture_propre);

    /* 3. Configuration de l'adresse d'écoute */
    serveur_addr.sin_family = AF_INET;
    serveur_addr.sin_addr.s_addr = INADDR_ANY;  /* écoute sur toutes les interfaces */
    serveur_addr.sin_port = htons(PORT);        /* htons = conversion en network byte order */

    /* 4. Liaison du socket au port */
    if (bind(socket_ecoute, (struct sockaddr *)&serveur_addr, sizeof(serveur_addr)) == SOCKET_ERROR) {
        printf("Erreur bind(): %d\n", WSAGetLastError());
        closesocket(socket_ecoute);
        WSACleanup();
        return 1;
    }

    /* 5. Mise en écoute, file d'attente de connexions max = 10 */
    if (listen(socket_ecoute, 10) == SOCKET_ERROR) {
        printf("Erreur listen(): %d\n", WSAGetLastError());
        closesocket(socket_ecoute);
        WSACleanup();
        return 1;
    }

    printf("Serveur MyDiscord en ecoute sur le port %d...\n", PORT);

    int prochain_id = 1;

    /* 6. Boucle d'acceptation : bloque jusqu'à une nouvelle connexion */
    while (1) {
        socket_client = accept(socket_ecoute, (struct sockaddr *)&client_addr, &taille_addr);
        if (socket_client == INVALID_SOCKET) {
            printf("Erreur accept(): %d\n", WSAGetLastError());
            continue;
        }

        Client *nouveau_client = malloc(sizeof(Client));
        nouveau_client->socket = socket_client;
        nouveau_client->client_id = prochain_id++;
        nouveau_client->actif = 1;

        pthread_mutex_lock(&mutex_clients);
        clients[nb_clients++] = *nouveau_client;
        pthread_mutex_unlock(&mutex_clients);

        /* 7. Un thread dédié par client : le serveur reste libre d'accepter
              de nouvelles connexions sans attendre que celui-ci finisse */
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, gerer_client, nouveau_client);
        pthread_detach(thread_id); /* libère les ressources du thread à sa fin sans join() */
    }

    closesocket(socket_ecoute);
    WSACleanup();
    return 0;
}