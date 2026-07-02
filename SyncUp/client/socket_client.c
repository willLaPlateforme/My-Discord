/*
 * Ce fichier socket_client.c gère la connexion réseau du client SyncUp.
 * Il établit la connexion TCP au serveur, envoie les messages et commandes,
 * et reçoit les données du serveur dans un thread séparé pour ne pas
 * bloquer l'interface graphique GTK4.
 *
 * La communication entre ce fichier et ui.c se fait via des callbacks :
 * quand un message arrive du serveur, on appelle ui_add_message() pour
 * l'afficher dans l'interface sans avoir à toucher aux sockets depuis ui.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "../include/socket_server.h"

// Adresse IP et port du serveur auquel le client doit se connecter.
// A modifier selon la configuration réseau de votre équipe.
#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 8080

// Taille maximale d'un message échangé entre client et serveur.
#define BUFFER_SIZE 1024

// Socket de connexion au serveur. Déclarée globalement pour
// pouvoir être utilisée depuis toutes les fonctions de ce fichier
// (envoi, réception, fermeture).
static SOCKET client_socket = INVALID_SOCKET;

// Indicateur d'état de la connexion : 1 = connecté, 0 = déconnecté.
// Utilisé par le thread de réception pour savoir quand s'arrêter.
static int is_connected = 0;

// ============================================================
// INITIALISATION DE WINSOCK
// ============================================================

/*
 * Initialise la bibliothèque Winsock, obligatoire sur Windows avant
 * toute utilisation des sockets. WSAStartup charge la version 2.2
 * de Winsock. Doit être appelée une seule fois au démarrage du client,
 * avant tout appel à socket(), connect(), send() ou recv().
 */
static int init_winsock(void) {
    WSADATA wsa_data;

    // WSAStartup initialise Winsock avec la version 2.2.
    // MAKEWORD(2, 2) encode la version sous forme d'un mot de 16 bits.
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "Erreur : impossible d'initialiser Winsock.\n");
        return 0;
    }

    return 1;
}

// ============================================================
// CONNEXION AU SERVEUR
// ============================================================

/*
 * Établit la connexion TCP au serveur SyncUp.
 * Crée un socket, configure l'adresse du serveur (IP + port),
 * et appelle connect() pour établir la liaison.
 * Retourne 1 si la connexion a réussi, 0 sinon.
 */
int socket_client_connect(void) {
    if (!init_winsock()) return 0;

    // Création du socket TCP (SOCK_STREAM = TCP, AF_INET = IPv4).
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        fprintf(stderr, "Erreur : impossible de créer le socket client.\n");
        WSACleanup();
        return 0;
    }

    // Configuration de l'adresse du serveur : famille IPv4, port et IP.
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    // inet_pton convertit l'adresse IP en format binaire réseau.
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Erreur : adresse IP du serveur invalide : %s\n", SERVER_IP);
        closesocket(client_socket);
        WSACleanup();
        return 0;
    }

    // connect() tente d'établir la connexion TCP avec le serveur.
    if (connect(client_socket, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "Erreur : connexion au serveur %s:%d échouée.\n",
                SERVER_IP, SERVER_PORT);
        closesocket(client_socket);
        WSACleanup();
        return 0;
    }

    is_connected = 1;
    printf("Connecté au serveur SyncUp (%s:%d)\n", SERVER_IP, SERVER_PORT);
    return 1;
}

// ============================================================
// ENVOI DE DONNEES AU SERVEUR
// ============================================================

/*
 * Envoie une chaîne de caractères au serveur via le socket.
 * Utilisé par ui.c pour envoyer les messages, commandes de login,
 * demandes d'accès aux canaux privés, réactions, etc.
 * Retourne 1 si l'envoi a réussi, 0 sinon.
 */
int socket_client_send(const char *message) {
    if (!is_connected || client_socket == INVALID_SOCKET) {
        fprintf(stderr, "Erreur : pas de connexion active au serveur.\n");
        return 0;
    }

    // send() envoie les octets du message via le socket TCP.
    // On cast en int car send() retourne le nombre d'octets envoyés.
    int sent = send(client_socket, message, (int)strlen(message), 0);
    if (sent == SOCKET_ERROR) {
        fprintf(stderr, "Erreur lors de l'envoi du message au serveur.\n");
        return 0;
    }

    return 1;
}

// ============================================================
// RECEPTION DES DONNEES DU SERVEUR (thread séparé)
// ============================================================

/*
 * Fonction exécutée dans un thread séparé pour recevoir en continu
 * les données envoyées par le serveur.
 *
 * Elle tourne dans une boucle infinie tant que is_connected == 1.
 * Pour chaque message reçu, elle l'affiche dans la console pour
 * l'instant (TODO : appeler ui_add_message() pour l'afficher dans GTK4).
 *
 * Le thread séparé est indispensable : si on appelait recv() directement
 * dans le fil principal de GTK4, l'interface serait bloquée en attente
 * de messages et ne répondrait plus aux clics/saisies de l'utilisateur.
 */
static DWORD WINAPI receive_thread(LPVOID param) {
    char buffer[BUFFER_SIZE];

    while (is_connected) {
        memset(buffer, 0, sizeof(buffer));

        // recv() bloque jusqu'à ce qu'un message arrive du serveur.
        // Elle retourne le nombre d'octets reçus, ou 0/SOCKET_ERROR
        // si la connexion est fermée ou en erreur.
        int received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        if (received <= 0) {
            // received == 0 : le serveur a fermé la connexion proprement.
            // received < 0  : erreur réseau.
            if (received == 0)
                printf("Le serveur a fermé la connexion.\n");
            else
                fprintf(stderr, "Erreur lors de la réception d'un message.\n");

            is_connected = 0;
            break;
        }

        buffer[received] = '\0';
        printf("Message reçu du serveur : %s\n", buffer);

        // TODO : parser le message reçu (JSON ou format texte simple)
        // et appeler ui_add_message(app, auteur, heure, contenu)
        // pour afficher le message dans l'interface GTK4.
        // Attention : les appels GTK doivent se faire depuis le thread
        // principal via g_idle_add() pour être thread-safe.
    }

    return 0;
}

/*
 * Lance le thread de réception en arrière-plan.
 * Doit être appelée juste après socket_client_connect() pour commencer
 * à écouter les messages du serveur sans bloquer l'interface GTK4.
 * Retourne 1 si le thread a démarré avec succès, 0 sinon.
 */
int socket_client_start_receive_thread(void) {
    // CreateThread crée un nouveau fil d'exécution Windows qui appelle
    // receive_thread en parallèle du fil principal GTK4.
    HANDLE thread = CreateThread(
        NULL,           // Attributs de sécurité par défaut
        0,              // Taille de pile par défaut
        receive_thread, // Fonction à exécuter dans le thread
        NULL,           // Paramètre passé à la fonction (non utilisé ici)
        0,              // Le thread démarre immédiatement
        NULL            // On n'a pas besoin de l'identifiant du thread
    );

    if (thread == NULL) {
        fprintf(stderr, "Erreur : impossible de créer le thread de réception.\n");
        return 0;
    }

    // CloseHandle libère le handle du thread côté parent.
    // Le thread continue de tourner en arrière-plan malgré ça.
    CloseHandle(thread);
    return 1;
}

// ============================================================
// FERMETURE DE LA CONNEXION
// ============================================================

/*
 * Ferme proprement la connexion au serveur et libère les ressources
 * Winsock. Doit être appelée quand l'utilisateur se déconnecte
 * ou quand l'application se ferme.
 */
void socket_client_disconnect(void) {
    is_connected = 0;

    if (client_socket != INVALID_SOCKET) {
        // shutdown() signale au serveur qu'on ne veut plus envoyer
        // ni recevoir de données (SD_BOTH = les deux directions).
        shutdown(client_socket, SD_BOTH);
        closesocket(client_socket);
        client_socket = INVALID_SOCKET;
    }

    // WSACleanup libère toutes les ressources Winsock allouées
    // par WSAStartup au début. Symétrique à WSAStartup.
    WSACleanup();
    printf("Déconnecté du serveur SyncUp.\n");
}