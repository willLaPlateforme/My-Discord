#ifndef MODERATION_H
#define MODERATION_H

/*
 * moderation.h — Timeout réseau (silence temporaire sur un canal).
 */

#include "commandes.h"

void cmd_timeout    (Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args);
int  est_en_timeout (int user_id, int canal_id);

#endif /* MODERATION_H */