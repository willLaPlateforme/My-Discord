#ifndef CHANNEL_REQ_H
#define CHANNEL_REQ_H

/*
 * channel_req.h — Workflow de demande d'accès aux canaux privés.
 *
 * Flux : REQUEST → LISTREQ → APPREQ ou REJREQ
 */

#include "commandes.h"

void cmd_request_channel (Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args);
void cmd_list_requests   (Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args);
void cmd_approve_request (Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args);
void cmd_reject_request  (Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args);

#endif /* CHANNEL_REQ_H */