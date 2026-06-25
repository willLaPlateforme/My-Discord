/*
 * Ce fichier db.c implémente les fonctions déclarées dans db.h.
 * Il gère la connexion à la base de données PostgreSQL, ainsi que
 * l'exécution des requêtes SQL utilisées par tout le reste du serveur
 * (auth.c, channel_manager.c, notification_system.c...).
 */

#include <stdio.h>
#include <stdlib.h>
#include "../include/db.h"

// Informations de connexion à la base de données.
// Adapte ces valeurs selon ta configuration PostgreSQL locale
// (nom d'utilisateur, mot de passe, nom de la base).
#define DB_HOST "localhost"
#define DB_PORT "5432"
#define DB_NAME "syncup_data"
#define DB_USER "postgres"
#define DB_PASSWORD "ton_mot_de_passe_ici"

// Établit la connexion à la base de données PostgreSQL.
// Construit une chaîne de connexion avec les paramètres définis
// ci-dessus, puis utilise PQconnectdb pour se connecter.
PGconn *db_connect(void) {
    char conninfo[256];

    // snprintf construit la chaîne de connexion attendue par libpq,
    // sous la forme "host=... port=... dbname=... user=... password=..."
    snprintf(conninfo, sizeof(conninfo),
             "host=%s port=%s dbname=%s user=%s password=%s",
             DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWORD);

    // PQconnectdb tente la connexion et retourne un PGconn,
    // que la connexion ait réussi ou échoué.
    PGconn *conn = PQconnectdb(conninfo);

    // PQstatus permet de vérifier si la connexion a réellement réussi.
    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Erreur de connexion à la base de données : %s\n",
                PQerrorMessage(conn));
        PQfinish(conn);
        return NULL;
    }

    printf("Connexion à la base de données réussie.\n");
    return conn;
}

// Ferme la connexion à la base de données proprement.
void db_disconnect(PGconn *conn) {
    if (conn != NULL) {
        PQfinish(conn);
        printf("Connexion à la base de données fermée.\n");
    }
}

// Exécute une requête SQL sans attendre de résultat exploitable
// (INSERT, UPDATE, DELETE).
int db_execute(PGconn *conn, const char *query) {
    PGresult *res = PQexec(conn, query);

    // PQresultStatus indique si la requête s'est bien exécutée.
    // PGRES_COMMAND_OK correspond au succès d'une requête sans
    // résultat à lire (INSERT, UPDATE, DELETE).
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Erreur lors de l'exécution de la requête : %s\n",
                PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }

    // On libère le résultat même en cas de succès, car PQexec
    // alloue toujours de la mémoire pour le résultat.
    PQclear(res);
    return 1;
}

// Exécute une requête SQL et retourne le résultat (SELECT).
// Le résultat doit être libéré avec PQclear() par la fonction
// appelante une fois qu'elle a fini de l'utiliser.
PGresult *db_query(PGconn *conn, const char *query) {
    PGresult *res = PQexec(conn, query);

    // PGRES_TUPLES_OK correspond au succès d'une requête qui
    // renvoie des lignes de données (SELECT).
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Erreur lors de la requête SELECT : %s\n",
                PQerrorMessage(conn));
        PQclear(res);
        return NULL;
    }

    return res;
}