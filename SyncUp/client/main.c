/*
 * main.c — Point d'entrée du client SyncUp.
 *
 * Lance la connexion au serveur puis démarre l'interface graphique.
 * L'UI et le réseau communiquent via ui_reseau.h.
 */

#include <stdio.h>
#include "ui_reseau.h"

#define SERVER_IP   "127.0.0.1"  /* Changer par l'IP du PC serveur */
#define SERVER_PORT 8080

/* run_ui() est définie dans ui.c — lance la boucle GTK */
int run_ui(int argc, char **argv);

int main(int argc, char **argv) {
    printf("Connexion au serveur %s:%d...\n", SERVER_IP, SERVER_PORT);

    if (!reseau_connecter(SERVER_IP, SERVER_PORT)) {
        fprintf(stderr, "Impossible de se connecter au serveur.\n");
        fprintf(stderr, "Verifie que server.exe tourne sur %s:%d\n",
                SERVER_IP, SERVER_PORT);
        return 1;
    }

    printf("Connecte ! Lancement de l'interface...\n");

    reseau_demarrer_reception();

    int status = run_ui(argc, argv);

    reseau_deconnecter();
    return status;
}