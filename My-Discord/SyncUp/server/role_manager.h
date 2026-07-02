#ifndef ROLE_MANAGER_H
#define ROLE_MANAGER_H

/*
 * role_manager.h — Changement de rôle (admin uniquement).
 * Chaque changement est tracé dans role_changes.
 */

#include "commandes.h"

void cmd_set_role(Client *client, char args[][TAILLE_MAX_MESSAGE], int nb_args);

#endif /* ROLE_MANAGER_H */