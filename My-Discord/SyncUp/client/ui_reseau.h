#ifndef UI_RESEAU_H
#define UI_RESEAU_H

/*
 * ui_reseau.h — Pont entre l'interface graphique (ui.c) et le réseau (socket_client.c).
 *
 * ui.c appelle ces fonctions quand l'utilisateur clique sur un bouton.
 * socket_client.c les implémente et envoie la commande au serveur.
 */

/* ─── Connexion ──────────────────────────────────────────────────────── */

/* Connecte le socket au serveur. Retourne 1 si OK, 0 si échec. */
int  reseau_connecter(const char *ip, int port);

/* Démarre le thread de réception des messages du serveur. */
void reseau_demarrer_reception(void);

/* Déconnecte proprement le socket. */
void reseau_deconnecter(void);

/* ─── Authentification ───────────────────────────────────────────────── */
void reseau_login(const char *email, const char *password);
void reseau_register(const char *nom, const char *prenom,
                     const char *email, const char *password);
void reseau_logout(void);

/* ─── Messages ───────────────────────────────────────────────────────── */
void reseau_envoyer_message(int canal_id, const char *texte);
void reseau_envoyer_prive(int dest_id, const char *texte);
void reseau_reagir(int message_id, const char *emoji);

/* ─── Canaux ─────────────────────────────────────────────────────────── */
void reseau_rejoindre_canal(int canal_id);
void reseau_demander_acces(int canal_id);
void reseau_lister_canaux(void);
void reseau_lister_utilisateurs(void);

/* ─── Modération ─────────────────────────────────────────────────────── */
void reseau_timeout(int user_id, int canal_id, int duree, const char *raison);
void reseau_ban(int user_id);
void reseau_changer_role(int user_id, const char *role);

/* ─── Callback UI ────────────────────────────────────────────────────── */
/*
 * Ces fonctions sont appelées par le thread réseau quand un message
 * arrive du serveur. Elles mettent à jour l'interface graphique.
 * Elles sont implémentées dans ui.c.
 */
void ui_on_message_recu(const char *auteur, const char *texte, int canal_id);
void ui_on_login_ok(const char *nom);
void ui_on_erreur(const char *message);
void ui_on_canaux_recus(const char *liste);
void ui_on_utilisateurs_recus(const char *liste);

#endif /* UI_RESEAU_H */