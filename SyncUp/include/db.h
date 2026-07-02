/*
 * Ce fichier db.h déclare les fonctions permettant de gérer
 * la connexion à la base de données PostgreSQL.
 * Toutes les fonctions liées à la base de données (auth, channels,
 * messages...) utilisent cette connexion partagée pour exécuter
 * leurs requêtes SQL.
 */

// Garde d'inclusion : empêche ce fichier d'être lu plusieurs fois
// si plusieurs fichiers .c font #include "db.h". Sans ça, le
// compilateur verrait les mêmes déclarations en double et renverrait
// une erreur de redéfinition.
#ifndef DB_H
#define DB_H

// Bibliothèque officielle de PostgreSQL en C.
// Elle fournit les types PGconn (une connexion) et PGresult
// (le résultat d'une requête), ainsi que toutes les fonctions
// PQ... (PQconnectdb, PQexec, PQfinish...) utilisées dans db.c.
#include <libpq-fe.h>

// Établit la connexion à la base de données PostgreSQL.
// Ne prend aucun paramètre (void) car les infos de connexion
// (host, user, password, dbname) seront codées directement
// dans db.c.
// Retourne un pointeur vers la connexion (PGconn), qui sera
// ensuite transmis à toutes les autres fonctions du serveur
// (auth.c, channel_manager.c...) pour qu'elles utilisent
// la même connexion au lieu d'en ouvrir une nouvelle chaque fois.
// Retourne NULL en cas d'échec de connexion.
PGconn *db_connect(void);

// Ferme la connexion à la base de données proprement.
// Prend en paramètre le pointeur "conn" retourné par db_connect,
// pour savoir quelle connexion fermer.
// Ne retourne rien (void) car il n'y a rien à récupérer après
// une fermeture de connexion.
void db_disconnect(PGconn *conn);

// Exécute une requête SQL qui ne renvoie pas de données à lire
// (INSERT, UPDATE, DELETE).
// - conn  : la connexion à utiliser (obtenue via db_connect)
// - query : la requête SQL sous forme de texte, par exemple
//           "INSERT INTO users (...) VALUES (...)"
// Le "const" devant "char *query" indique que la fonction ne
// modifiera jamais le contenu de la chaîne reçue, c'est une
// sécurité pour le compilateur et pour qui lit le code.
// Retourne 1 si la requête a réussi, 0 sinon, pour pouvoir
// tester facilement le résultat dans le code appelant.
int db_execute(PGconn *conn, const char *query);

// Exécute une requête SQL qui renvoie des données à lire (SELECT).
// Retourne un PGresult, une structure qui contient toutes les
// lignes et colonnes renvoyées par PostgreSQL. On pourra ensuite
// parcourir ce résultat avec des fonctions comme PQntuples()
// (nombre de lignes) ou PQgetvalue() (valeur d'une cellule précise).
// Important : ce résultat doit être libéré avec PQclear() une fois
// utilisé, sinon la mémoire reste occupée inutilement (fuite mémoire).
PGresult *db_query(PGconn *conn, const char *query);

// Fin de la garde d'inclusion ouverte à la ligne #ifndef DB_H.
#endif