#ifndef PROTOCOL_H
#define PROTOCOL_H

/*
 * protocol.h — Définit le "langage commun" entre client et serveur.
 *
 * Actuellement, le serveur de base (main_server.c) traite tout comme
 * du texte brut. Pour avancer, chaque message envoyé sur le réseau
 * devra commencer par un de ces codes, suivi de ses données.
 *
 * Exemple de message complet envoyé sur le socket :
 *   "LOGIN|email@test.com|motdepasse123\n"
 *   "MSG|canal_general|Salut tout le monde\n"
 *
 * Le caractère '|' sépare les champs, '\n' marque la fin du message.
 */

/* Codes de commande envoyés par le CLIENT vers le serveur */
#define CMD_LOGIN        "LOGIN"        /* LOGIN|email|password */
#define CMD_REGISTER     "REGISTER"     /* REGISTER|nom|prenom|email|password */
#define CMD_SEND_MSG     "MSG"          /* MSG|canal_id|texte */
#define CMD_JOIN_CHANNEL "JOIN"         /* JOIN|canal_id */
#define CMD_REACT        "REACT"        /* REACT|message_id|emoji */
#define CMD_LOGOUT       "LOGOUT"       /* LOGOUT (pas d'argument) */

/* Codes de réponse envoyés par le SERVEUR vers le client */
#define RESP_OK          "OK"           /* OK|message_optionnel */
#define RESP_ERROR       "ERR"          /* ERR|raison */
#define RESP_BROADCAST   "BROADCAST"    /* BROADCAST|canal_id|auteur|texte|timestamp */

#define TAILLE_MAX_MESSAGE 1024
#define SEPARATEUR "|"

#endif