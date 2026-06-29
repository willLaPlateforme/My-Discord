#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "10.10.47.87"
#define PORT 8080
#define MAX_BUFFER 1024

SOCKET sock;

/* Thread dédié à la réception : tourne en parallèle de la saisie utilisateur,
   sinon le programme serait bloqué soit en lecture, soit en écriture, jamais les deux */
void *recevoir_messages(void *arg) {
    char buffer[MAX_BUFFER];
    int octets_recus;

    while ((octets_recus = recv(sock, buffer, MAX_BUFFER - 1, 0)) > 0) {
        buffer[octets_recus] = '\0';
        printf("\n%s\n> ", buffer);
        fflush(stdout);
    }

    /* Le serveur a fermé la connexion (recv renvoie 0) ou une erreur réseau
       est survenue (recv renvoie -1) : on ferme proprement notre socket ici
       aussi, car le main() reste bloqué sur fgets() et ne le ferait jamais */
    printf("\nConnexion au serveur perdue.\n");
    closesocket(sock);
    WSACleanup();
    exit(0);
}

int main() {
    WSADATA wsa;
    struct sockaddr_in serveur_addr;
    char message[MAX_BUFFER];

    /* 1. Initialisation Winsock côté client aussi */
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Erreur WSAStartup: %d\n", WSAGetLastError());
        return 1;
    }

    /* 2. Création du socket */
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        printf("Erreur socket(): %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    /* 3. Adresse du serveur à joindre */
    serveur_addr.sin_family = AF_INET;
    serveur_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serveur_addr.sin_addr);

    /* 4. Connexion au serveur (bloquant jusqu'à succès ou échec) */
    if (connect(sock, (struct sockaddr *)&serveur_addr, sizeof(serveur_addr)) == SOCKET_ERROR) {
        printf("Erreur connect(): %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("Connecter au serveur MyDiscord.\n> ");
    fflush(stdout);

    /* 5. Thread séparé pour écouter les messages entrants en continu */
    pthread_t thread_recep;
    pthread_create(&thread_recep, NULL, recevoir_messages, NULL);

    /* 6. Boucle principale : lecture clavier et envoi */
    while (1) {
        if (fgets(message, MAX_BUFFER, stdin) == NULL) break;
        send(sock, message, (int)strlen(message), 0);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}