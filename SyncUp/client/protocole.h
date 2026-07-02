#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string.h>  /* strlen, strncpy, strtok, strcmp */

/*
 * protocole.h — Définit le "langage commun" entre client et serveur.
 * Ce fichier est partagé : client/ et server/ l'incluent tous les deux.
 *
 * Format d'un message : COMMANDE|arg1|arg2\n
 * Exemple : "LOGIN|email@test.com|motdepasse123\n"
 */

/* ─── Commandes CLIENT → SERVEUR ─────────────────────────────────────── */
#define CMD_LOGIN           "LOGIN"       /* LOGIN|email|password                      */
#define CMD_REGISTER        "REGISTER"    /* REGISTER|nom|prenom|email|password        */
#define CMD_LOGOUT          "LOGOUT"      /* LOGOUT                                    */
#define CMD_SEND_MSG        "MSG"         /* MSG|canal_id|texte                        */
#define CMD_PRIVATE_MSG     "PRIVMSG"     /* PRIVMSG|destinataire_id|texte             */
#define CMD_JOIN_CHANNEL    "JOIN"        /* JOIN|canal_id                             */
#define CMD_LIST_CHANNELS   "LIST"        /* LIST                                      */
#define CMD_LIST_USERS      "USERS"       /* USERS                                     */
#define CMD_REACT           "REACT"       /* REACT|message_id|emoji                   */
#define CMD_BAN             "BAN"         /* BAN|user_id                               */
#define CMD_TIMEOUT         "TIMEOUT"     /* TIMEOUT|user_id|canal_id|duree|raison     */
#define CMD_REQUEST_CHANNEL "REQUEST"     /* REQUEST|canal_id                          */
#define CMD_LIST_REQUESTS   "LISTREQ"     /* LISTREQ|canal_id                          */
#define CMD_APPROVE_REQUEST "APPREQ"      /* APPREQ|demande_id                         */
#define CMD_REJECT_REQUEST  "REJREQ"      /* REJREQ|demande_id                         */
#define CMD_SET_ROLE        "SETROLE"     /* SETROLE|user_id|membre/moderator/admin    */

/* ─── Réponses SERVEUR → CLIENT ─────────────────────────────────────── */
#define RESP_OK             "OK"          /* OK|message_optionnel                      */
#define RESP_ERROR          "ERR"         /* ERR|raison                                */
#define RESP_BROADCAST      "BROADCAST"   /* BROADCAST|canal_id|auteur|texte           */
#define RESP_PRIVATE        "PRIVATE"     /* PRIVATE|expediteur|texte_chiffre          */
#define RESP_HISTORY        "HISTORY"     /* HISTORY|canal_id|auteur|texte|timestamp   */
#define RESP_USER_LIST      "USERLIST"    /* USERLIST|user1|user2|...                  */
#define RESP_CHANNEL_LIST   "CHANNELLIST" /* CHANNELLIST|id:nom|id:nom|...             */
#define RESP_REACT          "REACTION"    /* REACTION|message_id|emoji|auteur          */

/* ─── Constantes ─────────────────────────────────────────────────────── */
#define TAILLE_MAX_MESSAGE  1024
#define SEPARATEUR          "|"

/* ─── Rôles utilisateur ──────────────────────────────────────────────── */
#define ROLE_MEMBRE         0
#define ROLE_MODERATEUR     1
#define ROLE_ADMIN          2

/*
 * parser_commande()
 * Découpe un message reçu en commande + arguments.
 *   "MSG|1|Bonjour\n"  →  commande="MSG", args[0]="1", args[1]="Bonjour"
 * Retourne 1 si OK, 0 si message vide ou invalide.
 */
static inline int parser_commande(char *message, char *commande,
                                   char args[][TAILLE_MAX_MESSAGE], int *nb_args) {
    *nb_args = 0;

    int len = (int)strlen(message);
    if (len > 0 && message[len - 1] == '\n') message[len - 1] = '\0';
    if (len == 0) return 0;

    char copie[TAILLE_MAX_MESSAGE];
    strncpy(copie, message, TAILLE_MAX_MESSAGE - 1);
    copie[TAILLE_MAX_MESSAGE - 1] = '\0';

    char *token = strtok(copie, SEPARATEUR);
    if (token == NULL) return 0;
    strncpy(commande, token, TAILLE_MAX_MESSAGE - 1);

    while ((token = strtok(NULL, SEPARATEUR)) != NULL && *nb_args < 8) {
        strncpy(args[*nb_args], token, TAILLE_MAX_MESSAGE - 1);
        (*nb_args)++;
    }
    return 1;
}

#endif /* PROTOCOL_H */