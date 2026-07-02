/*
 * Ce fichier ui.c gère l'intégralité de l'interface graphique du client GTK4.
 * Il construit tous les écrans de l'application SyncUp : login, affichage
 * des canaux et de l'historique, zone de saisie, réactions emoji, demande
 * d'accès canal privé, choix d'avatar, panneau modérateur, panneau admin,
 * interface vocale et plateau Puissance 4.
 *
 * L'esthétique que j'ai choisis : thème sombre avec accents néon, inspiré de Punishing Gray
 * Raven et Arknight. Le style visuel détaillé est dans UI/style.css.
 */

#include <gtk/gtk.h>
#include <string.h>

/*
 * Voici la structure qui regroupe tout les widgets principaux de l'application.
 * On la transmet entre les fonctions pour pouvoir naviguer d'un écran à
 * l'autre sans perdre les références aux widgets importants.
 */
typedef struct {
    GtkWidget *window;          // La fenêtre principale
    GtkWidget *stack;           // Le conteneur de la navigation entre écrans
    GtkWidget *channel_list;    // La liste des canaux à gauche
    GtkWidget *message_list;    // La liste des messages au centre
    GtkWidget *message_entry;   // Le champ de saisie de message
    GtkWidget *avatar_image;    // L'avatar de l'utilisateur connecté
    // Chemin vers l'avatar choisi par l'utilisateur:
    // soit un avatar par défaut, soit une image uploadée depuis son PC.
    char selected_avatar_path[512];
} AppWidgets;

// ============================================================
// ECRAN 1 : Le LOGIN
// ============================================================

/*
 * Callback appelé quand l'utilisateur clique sur "SE CONNECTER".
 * Récupère l'email et le mot de passe saisis, les affiches dans
 * La console pour l'instant sera connecté à socket_client.c.
 */
static void on_login_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *email_entry = GTK_WIDGET(g_object_get_data(G_OBJECT(data), "email_entry"));
    GtkWidget *password_entry = GTK_WIDGET(g_object_get_data(G_OBJECT(data), "password_entry"));

    const char *email = gtk_editable_get_text(GTK_EDITABLE(email_entry));
    const char *password = gtk_editable_get_text(GTK_EDITABLE(password_entry));

    g_print("Tentative de connexion : %s\n", email);
    // TODO : envoyer ces indentifiants au serveur via socket_client.c
}

/*
 * Le Callback est appelé quand l'utilisateur clique sur "S'INSCRIRE".
 * Navigue vers l'écran d'inscription (choix avatar + formulaire).
 */
static void on_register_clicked(GtkWidget *widget, gpointer data) {
    AppWidgets *app = (AppWidgets *)data;
    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "register");
}

// Construit l'écran du login: champ email, champ mot de passe,
// Bouton de connexion et lien vers l'inscription.
static GtkWidget *build_login_screen(AppWidgets *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_top(box, 80);
    gtk_widget_set_margin_bottom(box, 80);
    gtk_widget_set_margin_start(box, 60);
    gtk_widget_set_margin_end(box, 60);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    // Le titre principal de l'application.
    GtkWidget *title = gtk_label_new("SyncUp");
    gtk_widget_add_css_class(title, "app-title");
    gtk_box_append(GTK_BOX(box), title);

    // Le sous-titre de l'écran de connexion de l'application.
    GtkWidget *subtitle = gtk_label_new("Connexion à votre compte");
    gtk_widget_add_css_class(subtitle, "app-subtitle");
    gtk_box_append(GTK_BOX(box), subtitle);

    // La création du champ email.
    GtkWidget *email_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(email_entry), "Adresse email");
    gtk_widget_add_css_class(email_entry, "login-entry");
    gtk_box_append(GTK_BOX(box), email_entry);

    // La création du champ mot de passe, caractères masqués.
    GtkWidget *password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry), "Mot de passe");
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    gtk_widget_add_css_class(password_entry, "login-entry");
    gtk_box_append(GTK_BOX(box), password_entry);

    // Bouton de connexion principal.
    GtkWidget *login_button = gtk_button_new_with_label("SE CONNECTER");
    gtk_widget_add_css_class(login_button, "login-button");
    g_object_set_data(G_OBJECT(login_button), "email_entry", email_entry);
    g_object_set_data(G_OBJECT(login_button), "password_entry", password_entry);
    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_clicked), login_button);
    gtk_box_append(GTK_BOX(box), login_button);

    // Le lien vers l'écran d'inscription.
    GtkWidget *register_button = gtk_button_new_with_label("Pas encore de compte ? S'inscrire");
    gtk_widget_add_css_class(register_button, "link-button");
    g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_clicked), app);
    gtk_box_append(GTK_BOX(box), register_button);

    return box;
}

// ============================================================
// ECRAN 2 : INSCRIPTION + CHOIX AVATAR
// ============================================================

// Callback appelé quand l'utilisateur valide son inscription.
static void on_register_submit_clicked(GtkWidget *widget, gpointer data) {
    g_print("Inscription soumise\n");
    // TODO : envoyer les données d'inscription au serveur via socket_client.c
}

// Option B — callback appelé quand l'utilisateur clique sur un avatar
// par défaut dans la galerie. Met à jour selected_avatar_path avec le
// chemin de l'avatar cliqué, et affiche une confirmation visuelle.
static void on_default_avatar_selected(GtkWidget *widget, gpointer data) {
    AppWidgets *app = (AppWidgets *)g_object_get_data(G_OBJECT(widget), "app");
    const char *path = (const char *)data;
    strncpy(app->selected_avatar_path, path, sizeof(app->selected_avatar_path) - 1);
    g_print("Avatar par défaut sélectionné : %s\n", path);
    // TODO : prévisualiser l'avatar sélectionné dans un GtkImage
}

// Option B — callback appelé quand l'utilisateur confirme le choix
// d'un fichier image depuis son PC via le sélecteur de fichiers GTK4.
// Copie le chemin du fichier choisi dans selected_avatar_path.
static void on_file_dialog_response(GObject *source, GAsyncResult *result, gpointer data) {
    AppWidgets *app = (AppWidgets *)data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);

    // gtk_file_dialog_open_finish récupère le fichier choisi par
    // l'utilisateur. Si l'utilisateur a annulé, file sera NULL.
    GFile *file = gtk_file_dialog_open_finish(dialog, result, NULL);
    if (file == NULL) return;

    // On récupère le chemin complet du fichier sur le disque.
    const char *path = g_file_get_path(file);
    if (path != NULL) {
        strncpy(app->selected_avatar_path, path, sizeof(app->selected_avatar_path) - 1);
        g_print("Avatar custom sélectionné : %s\n", path);
        // TODO : prévisualiser l'image choisie dans un GtkImage
        // TODO : envoyer ce fichier au serveur via socket_client.c lors de l'inscription
    }

    g_object_unref(file);
}

// Option B — callback appelé quand l'utilisateur clique sur
// "Choisir depuis mon ordinateur". Ouvre un sélecteur de fichiers
// GTK4 (GtkFileDialog) filtré sur les images uniquement.
static void on_upload_avatar_clicked(GtkWidget *widget, gpointer data) {
    AppWidgets *app = (AppWidgets *)data;

    // GtkFileDialog est le sélecteur de fichiers natif de GTK4.
    // Il s'ouvre de façon asynchrone pour ne pas bloquer l'interface.
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Choisir un avatar");

    // On filtre pour n'afficher que les images (png, jpg, gif).
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Images");
    gtk_file_filter_add_mime_type(filter, "image/png");
    gtk_file_filter_add_mime_type(filter, "image/jpeg");
    gtk_file_filter_add_mime_type(filter, "image/gif");

    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));

    // open_async ouvre le dialogue sans bloquer l'interface.
    // Quand l'utilisateur choisit un fichier, on_file_dialog_response
    // est appelé automatiquement avec le résultat.
    gtk_file_dialog_open(dialog, GTK_WINDOW(app->window), NULL,
                         on_file_dialog_response, app);

    g_object_unref(filter);
    g_object_unref(filters);
}

// On constrruit l'écran d'inscritpion avec formulaire et choix d'avatar.
// L'utilisateur entre ses informations et choisit une photo de profil
// parmi une galerie d'images proposées (sélecteur d'avatar).
static GtkWidget *build_register_screen(AppWidgets *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(box, 40);
    gtk_widget_set_margin_bottom(box, 40);
    gtk_widget_set_margin_start(box, 60);
    gtk_widget_set_margin_end(box, 60);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    GtkWidget *title = gtk_label_new("CRÉER UN COMPTE");
    gtk_widget_add_css_class(title, "app-title");
    gtk_box_append(GTK_BOX(box), title);

    // Création du champ de formulaire d'inscription.
    GtkWidget *firstname_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(firstname_entry), "Prénom");
    gtk_widget_add_css_class(firstname_entry, "login-entry");
    gtk_box_append(GTK_BOX(box), firstname_entry);

    GtkWidget *lastname_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(lastname_entry), "Nom");
    gtk_widget_add_css_class(lastname_entry, "login-entry");
    gtk_box_append(GTK_BOX(box), lastname_entry);

    GtkWidget *email_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(email_entry), "Adresse email");
    gtk_widget_add_css_class(email_entry, "login-entry");
    gtk_box_append(GTK_BOX(box), email_entry);

    GtkWidget *password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry), "Mot de passe");
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    gtk_widget_add_css_class(password_entry, "login-entry");
    gtk_box_append(GTK_BOX(box), password_entry);

    // Création de la section du choix de l'avatar : galerie de boutons image.
    // L'utilisateur clique sur un avatar pour le sélectionner.
    GtkWidget *avatar_label = gtk_label_new("Veuillez choisir un avatar : ");
    gtk_widget_add_css_class(avatar_label, "app-subtitle");
    gtk_box_append(GTK_BOX(box), avatar_label);

    // Grille d'avatars (3 colonnes) pour la galerie de sélection.
    GtkWidget *avatar_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(avatar_grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(avatar_grid), 10);

    // Avatars par défaut proposés au choix (chemins dans server/uploads/avatars/).
    const char *avatars[] = {
        "server/uploads/avatars/default1.png",
        "server/uploads/avatars/default2.png",
        "server/uploads/avatars/default3.png",
        "server/uploads/avatars/default4.png",
        "server/uploads/avatars/default5.png",
        "server/uploads/avatars/default6.png"
    };

    // Création d'un bouton pour chaque avatar dans la grille.
    for (int i = 0; i < 6; i++) {
        GtkWidget *avatar_btn = gtk_button_new();
        GtkWidget *avatar_img = gtk_image_new_from_file(avatars[i]);
        gtk_image_set_pixel_size(GTK_IMAGE(avatar_img), 48);
        gtk_button_set_child(GTK_BUTTON(avatar_btn), avatar_img);
        gtk_widget_add_css_class(avatar_btn, "avatar-btn");
        // Option B — on attache le chemin ET la référence app au bouton
        // pour pouvoir les récupérer dans on_default_avatar_selected.
        g_object_set_data(G_OBJECT(avatar_btn), "app", app);
        g_signal_connect(avatar_btn, "clicked",
                         G_CALLBACK(on_default_avatar_selected),
                         (gpointer)avatars[i]);
        gtk_grid_attach(GTK_GRID(avatar_grid), avatar_btn, i % 3, i / 3, 1, 1);
    }
    gtk_box_append(GTK_BOX(box), avatar_grid);

    // Option B — bouton pour uploader un avatar personnalisé depuis le PC.
    // Ouvre un sélecteur de fichiers filtré sur les images.
    GtkWidget *upload_btn = gtk_button_new_with_label("Choisir depuis mon ordinateur...");
    gtk_widget_add_css_class(upload_btn, "link-button");
    g_signal_connect(upload_btn, "clicked", G_CALLBACK(on_upload_avatar_clicked), app);
    gtk_box_append(GTK_BOX(box), upload_btn);

    // Bouton de validation de l'inscription.
    GtkWidget *submit_button = gtk_button_new_with_label("CRÉER MON COMPTE");
    gtk_widget_add_css_class(submit_button, "login-button");
    g_signal_connect(submit_button, "clicked", G_CALLBACK(on_register_submit_clicked), app);
    gtk_box_append(GTK_BOX(box), submit_button);

    // Lien de retour vers le login.
    GtkWidget *back_button = gtk_button_new_with_label("Déjà un compte ? Se connecter");
    gtk_widget_add_css_class(back_button, "link-button");
    g_signal_connect(back_button, "clicked", G_CALLBACK(on_register_clicked), app);
    gtk_box_append(GTK_BOX(box), back_button);

    return box;
}

// ============================================================
// ECRAN 3 : CHAT PRINCIPAL (canaux + historique + saisie)
// ============================================================

// Callback appelé quand l'utilisateur envoie un message
// (clic sur le bouton Envoyer ou touche Entrée).
static void on_send_message_clicked(GtkWidget *widget, gpointer data) {
    AppWidgets *app = (AppWidgets *)data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(app->message_entry));

    if (strlen(text) == 0) return;

    g_print("Message envoyé : %s\n", text);
    // TODO : envoyer le message au serveur via socket_client.c

    // Vide le champ de saisie après envoi.
    gtk_editable_set_text(GTK_EDITABLE(app->message_entry), "");
}

// Le callback est appelé quand l'utilisateur appuie sur Entrée dans
// le champ de saisie même effet que cliquer sur Envoyer.
static void on_message_entry_activate(GtkWidget *widget, gpointer data) {
    on_send_message_clicked(widget, data);
}

// Callback appelé quand l'utilisateur clique sur le bouton
// "Demander accès" pour un canal privé.
static void on_request_access_clicked(GtkWidget *widget, gpointer data) {
    g_print("Demande d'accès au canal privé envoyée\n");
    // TODO : envoyer la demande au serveur via socket_client.c
}

// Ajoute un message dans la liste des messages affichés.
// Crée une ligne avec l'auteur, l'heure et le contenu.
void ui_add_message(AppWidgets *app, const char *author,
                    const char *timestamp, const char *content) {
    // Chaque message est une boîte horizontale :
    // [auteur + heure] à gauche, [contenu] à droite.
    GtkWidget *msg_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(msg_box, "message-box");

    // Ligne d'en-tête : auteur + heure.
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *author_label = gtk_label_new(author);
    gtk_widget_add_css_class(author_label, "message-author");
    GtkWidget *time_label = gtk_label_new(timestamp);
    gtk_widget_add_css_class(time_label, "message-time");
    gtk_box_append(GTK_BOX(header_box), author_label);
    gtk_box_append(GTK_BOX(header_box), time_label);

    // Contenu du message.
    GtkWidget *content_label = gtk_label_new(content);
    gtk_label_set_wrap(GTK_LABEL(content_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(content_label), 0);
    gtk_widget_add_css_class(content_label, "message-content");

    gtk_box_append(GTK_BOX(msg_box), header_box);
    gtk_box_append(GTK_BOX(msg_box), content_label);

    // Ajoute le message à la liste des messages (en bas).
    gtk_box_append(GTK_BOX(app->message_list), msg_box);
}

// Construit l'écran principal de chat : liste des canaux à gauche,
// historique des messages au centre, zone de saisie en bas.
static GtkWidget *build_chat_screen(AppWidgets *app) {
    // Conteneur horizontal principal : sidebar gauche + zone centrale.
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(main_box, "chat-main");

    // ---- SIDEBAR GAUCHE : liste des canaux ----
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(sidebar, "sidebar");
    gtk_widget_set_size_request(sidebar, 220, -1);

    // En-tête de la sidebar avec le nom du serveur.
    GtkWidget *server_name = gtk_label_new("SyncUp");
    gtk_widget_add_css_class(server_name, "server-name");
    gtk_box_append(GTK_BOX(sidebar), server_name);

    // Liste scrollable des canaux.
    GtkWidget *channel_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(channel_scroll, TRUE);

    // La liste des canaux est une GtkListBox : chaque ligne
    // est un canal cliquable. Les canaux privés affichent un
    // cadenas, les canaux publics un dièse (#).
    app->channel_list = gtk_list_box_new();
    gtk_widget_add_css_class(app->channel_list, "channel-list");

    // Canaux par défaut — seront remplacés par les vrais canaux
    // reçus depuis le serveur via socket_client.c.
    const char *channels[] = {"# général", "# random", "🔒 privé"};
    for (int i = 0; i < 3; i++) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(channels[i]);
        gtk_widget_add_css_class(label, "channel-item");
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        gtk_list_box_append(GTK_LIST_BOX(app->channel_list), row);
    }

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(channel_scroll), app->channel_list);
    gtk_box_append(GTK_BOX(sidebar), channel_scroll);

    // Bouton de demande d'accès à un canal privé, en bas de la sidebar.
    GtkWidget *request_btn = gtk_button_new_with_label("Demander accès privé");
    gtk_widget_add_css_class(request_btn, "request-btn");
    g_signal_connect(request_btn, "clicked", G_CALLBACK(on_request_access_clicked), app);
    gtk_box_append(GTK_BOX(sidebar), request_btn);

    gtk_box_append(GTK_BOX(main_box), sidebar);

    // ---- ZONE CENTRALE : historique + saisie ----
    GtkWidget *center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(center_box, TRUE);

    // En-tête du canal actif.
    GtkWidget *channel_header = gtk_label_new("# général");
    gtk_widget_add_css_class(channel_header, "channel-header");
    gtk_box_append(GTK_BOX(center_box), channel_header);

    // Zone scrollable pour l'historique des messages.
    GtkWidget *msg_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(msg_scroll, TRUE);

    // La liste des messages est une GtkBox verticale dans
    // laquelle on append dynamiquement chaque nouveau message
    // via ui_add_message().
    app->message_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(app->message_list, "message-list");
    gtk_widget_set_margin_start(app->message_list, 16);
    gtk_widget_set_margin_end(app->message_list, 16);
    gtk_widget_set_margin_top(app->message_list, 16);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(msg_scroll), app->message_list);
    gtk_box_append(GTK_BOX(center_box), msg_scroll);

    // La Zone de saisie de message en bas : champ texte + bouton Envoyer.
    GtkWidget *input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(input_box, "input-box");
    gtk_widget_set_margin_start(input_box, 16);
    gtk_widget_set_margin_end(input_box, 16);
    gtk_widget_set_margin_top(input_box, 8);
    gtk_widget_set_margin_bottom(input_box, 16);

    app->message_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->message_entry), "Envoyer un message...");
    gtk_widget_add_css_class(app->message_entry, "message-entry");
    gtk_widget_set_hexpand(app->message_entry, TRUE);

    // Envoyer avec la touche Entrée.
    g_signal_connect(app->message_entry, "activate",
                     G_CALLBACK(on_message_entry_activate), app);

    GtkWidget *send_btn = gtk_button_new_with_label("➤");
    gtk_widget_add_css_class(send_btn, "send-button");
    g_signal_connect(send_btn, "clicked", G_CALLBACK(on_send_message_clicked), app);

    gtk_box_append(GTK_BOX(input_box), app->message_entry);
    gtk_box_append(GTK_BOX(input_box), send_btn);
    gtk_box_append(GTK_BOX(center_box), input_box);

    gtk_box_append(GTK_BOX(main_box), center_box);

    return main_box;
}

// ============================================================
// ECRAN 4 : REACTIONS EMOJI
// ============================================================

/*
 * Affiche un sélecteur d'emoji sous forme de popover (fenêtre
 * flottante) que l'utilisateur peut ouvrir pour réagir à un message.
 */
static void on_reaction_clicked(GtkWidget *widget, gpointer data) {
    const char *emoji = (const char *)data;
    g_print("Réaction ajoutée : %s\n", emoji);
    // TODO : envoyer la réaction au serveur via socket_client.c
}

// Crée une ligne de boutons emoji sous un message pour permettre
// à l'utilisateur de réagir. Retourne le widget de réactions.
GtkWidget *build_reaction_bar(void) {
    GtkWidget *reaction_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(reaction_box, "reaction-bar");

    // Emojis disponibles pour les réactions.
    const char *emojis[] = {"👍", "❤️", "😂", "😮", "😢", "🔥"};
    for (int i = 0; i < 6; i++) {
        GtkWidget *btn = gtk_button_new_with_label(emojis[i]);
        gtk_widget_add_css_class(btn, "reaction-btn");
        g_signal_connect(btn, "clicked", G_CALLBACK(on_reaction_clicked),
                         (gpointer)emojis[i]);
        gtk_box_append(GTK_BOX(reaction_box), btn);
    }

    return reaction_box;
}

// ============================================================
// ECRAN 5 : PANNEAU MODERATEUR
// ============================================================

/*
 * Callback pour mettre un utilisateur en timeout depuis le panneau
 * modérateur. Envoie la commande au serveur.
 */
static void on_timeout_clicked(GtkWidget *widget, gpointer data) {
    g_print("Timeout appliqué\n");
    // TODO : envoyer la commande timeout au serveur via socket_client.c
}

// Callback pour supprimer un message depuis le panneau modérateur.
static void on_delete_message_clicked(GtkWidget *widget, gpointer data) {
    g_print("Message supprimé\n");
    // TODO : envoyer la commande de suppression au serveur via socket_client.c
}

/*
 * Construit le panneau modérateur : timeout utilisateur + suppression
 * de messages. Accessible uniquement aux modérateurs et admins.
 */
static GtkWidget *build_moderator_panel(AppWidgets *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(box, "panel-box");
    gtk_widget_set_margin_top(box, 40);
    gtk_widget_set_margin_bottom(box, 40);
    gtk_widget_set_margin_start(box, 40);
    gtk_widget_set_margin_end(box, 40);

    GtkWidget *title = gtk_label_new("PANNEAU MODÉRATEUR");
    gtk_widget_add_css_class(title, "panel-title");
    gtk_box_append(GTK_BOX(box), title);

    // Section timeout : champ pour l'ID utilisateur + durée.
    GtkWidget *timeout_label = gtk_label_new("Mettre en timeout :");
    gtk_widget_add_css_class(timeout_label, "panel-section-label");
    gtk_box_append(GTK_BOX(box), timeout_label);

    GtkWidget *timeout_user = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(timeout_user), "ID ou nom d'utilisateur");
    gtk_widget_add_css_class(timeout_user, "login-entry");
    gtk_box_append(GTK_BOX(box), timeout_user);

    GtkWidget *timeout_duration = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(timeout_duration), "Durée (en minutes)");
    gtk_widget_add_css_class(timeout_duration, "login-entry");
    gtk_box_append(GTK_BOX(box), timeout_duration);

    GtkWidget *timeout_btn = gtk_button_new_with_label("APPLIQUER TIMEOUT");
    gtk_widget_add_css_class(timeout_btn, "action-button");
    g_signal_connect(timeout_btn, "clicked", G_CALLBACK(on_timeout_clicked), app);
    gtk_box_append(GTK_BOX(box), timeout_btn);

    // Section suppression de message : champ pour l'ID du message.
    GtkWidget *delete_label = gtk_label_new("Supprimer un message :");
    gtk_widget_add_css_class(delete_label, "panel-section-label");
    gtk_box_append(GTK_BOX(box), delete_label);

    GtkWidget *message_id_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(message_id_entry), "ID du message à supprimer");
    gtk_widget_add_css_class(message_id_entry, "login-entry");
    gtk_box_append(GTK_BOX(box), message_id_entry);

    GtkWidget *delete_btn = gtk_button_new_with_label("SUPPRIMER MESSAGE");
    gtk_widget_add_css_class(delete_btn, "action-button");
    g_signal_connect(delete_btn, "clicked", G_CALLBACK(on_delete_message_clicked), app);
    gtk_box_append(GTK_BOX(box), delete_btn);

    // Bouton retour vers le chat principal.
    GtkWidget *back_btn = gtk_button_new_with_label("← Retour au chat");
    gtk_widget_add_css_class(back_btn, "link-button");
    g_signal_connect(back_btn, "clicked",
                     G_CALLBACK(gtk_stack_set_visible_child_name), app->stack);
    gtk_box_append(GTK_BOX(box), back_btn);

    return box;
}

// ============================================================
// ECRAN 6 : LA CREATION DU PANNEAU ADMIN
// ============================================================

// Callback pour bannir un utilisateur depuis le panneau admin.
static void on_ban_clicked(GtkWidget *widget, gpointer data) {
    g_print("Utilisateur banni\n");
    // TODO : envoyer la commande ban au serveur via socket_client.c
}

// Callback pour changer le rôle d'un utilisateur.
static void on_role_change_clicked(GtkWidget *widget, gpointer data) {
    g_print("Rôle modifié\n");
    // TODO : envoyer la commande de changement de rôle au serveur
}

// Callback pour créer un nouveau canal.
static void on_create_channel_clicked(GtkWidget *widget, gpointer data) {
    g_print("Canal créé\n");
    // TODO : envoyer la commande de création de canal au serveur
}

// Callback pour supprimer un canal.
static void on_delete_channel_clicked(GtkWidget *widget, gpointer data) {
    g_print("Canal supprimé\n");
    // TODO : envoyer la commande de suppression de canal au serveur
}

/*
 * Construit le panneau admin : bannissement, gestion des rôles,
 * création et suppression de canaux. Accessible uniquement aux admins.
 */
static GtkWidget *build_admin_panel(AppWidgets *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(box, "panel-box");
    gtk_widget_set_margin_top(box, 40);
    gtk_widget_set_margin_bottom(box, 40);
    gtk_widget_set_margin_start(box, 40);
    gtk_widget_set_margin_end(box, 40);

    GtkWidget *title = gtk_label_new("PANNEAU ADMIN");
    gtk_widget_add_css_class(title, "panel-title");
    gtk_box_append(GTK_BOX(box), title);

    // Section bannissement.
    GtkWidget *ban_label = gtk_label_new("Bannir un utilisateur :");
    gtk_widget_add_css_class(ban_label, "panel-section-label");
    gtk_box_append(GTK_BOX(box), ban_label);

    GtkWidget *ban_user_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ban_user_entry), "ID ou nom d'utilisateur");
    gtk_widget_add_css_class(ban_user_entry, "login-entry");
    gtk_box_append(GTK_BOX(box), ban_user_entry);

    GtkWidget *ban_reason_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ban_reason_entry), "Raison du bannissement");
    gtk_widget_add_css_class(ban_reason_entry, "login-entry");
    gtk_box_append(GTK_BOX(box), ban_reason_entry);

    GtkWidget *ban_btn = gtk_button_new_with_label("BANNIR");
    gtk_widget_add_css_class(ban_btn, "danger-button");
    g_signal_connect(ban_btn, "clicked", G_CALLBACK(on_ban_clicked), app);
    gtk_box_append(GTK_BOX(box), ban_btn);

    // Section gestion des rôles.
    GtkWidget *role_label = gtk_label_new("Gérer les rôles :");
    gtk_widget_add_css_class(role_label, "panel-section-label");
    gtk_box_append(GTK_BOX(box), role_label);

    GtkWidget *role_user_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(role_user_entry), "ID ou nom d'utilisateur");
    gtk_widget_add_css_class(role_user_entry, "login-entry");
    gtk_box_append(GTK_BOX(box), role_user_entry);

    // Le Menu déroulant pour choisir le nouveau rôle.
    GtkWidget *role_dropdown = gtk_drop_down_new_from_strings(
        (const char *[]){"member", "moderator", "admin", NULL}
    );
    gtk_widget_add_css_class(role_dropdown, "role-dropdown");
    gtk_box_append(GTK_BOX(box), role_dropdown);

    GtkWidget *role_btn = gtk_button_new_with_label("CHANGER RÔLE");
    gtk_widget_add_css_class(role_btn, "action-button");
    g_signal_connect(role_btn, "clicked", G_CALLBACK(on_role_change_clicked), app);
    gtk_box_append(GTK_BOX(box), role_btn);

    // Section gestion des canaux.
    GtkWidget *channel_label = gtk_label_new("Gérer les canaux :");
    gtk_widget_add_css_class(channel_label, "panel-section-label");
    gtk_box_append(GTK_BOX(box), channel_label);

    GtkWidget *channel_name_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(channel_name_entry), "Nom du canal");
    gtk_widget_add_css_class(channel_name_entry, "login-entry");
    gtk_box_append(GTK_BOX(box), channel_name_entry);

    GtkWidget *create_channel_btn = gtk_button_new_with_label("CRÉER CANAL");
    gtk_widget_add_css_class(create_channel_btn, "action-button");
    g_signal_connect(create_channel_btn, "clicked",
                     G_CALLBACK(on_create_channel_clicked), app);
    gtk_box_append(GTK_BOX(box), create_channel_btn);

    GtkWidget *delete_channel_btn = gtk_button_new_with_label("SUPPRIMER CANAL");
    gtk_widget_add_css_class(delete_channel_btn, "danger-button");
    g_signal_connect(delete_channel_btn, "clicked",
                     G_CALLBACK(on_delete_channel_clicked), app);
    gtk_box_append(GTK_BOX(box), delete_channel_btn);

    // Bouton retour.
    GtkWidget *back_btn = gtk_button_new_with_label("← Retour au chat");
    gtk_widget_add_css_class(back_btn, "link-button");
    gtk_box_append(GTK_BOX(box), back_btn);

    return box;
}

// ============================================================
// ECRAN 7 : L'INTERFACE VOCALE
// ============================================================

// Callback pour rejoindre un canal vocal.
static void on_join_voice_clicked(GtkWidget *widget, gpointer data) {
    g_print("Rejoint le canal vocal\n");
    // TODO : connexion UDP au serveur vocal via voice_client.c
}

// Callback pour quitter un canal vocal.
static void on_leave_voice_clicked(GtkWidget *widget, gpointer data) {
    g_print("Quitté le canal vocal\n");
    // TODO : déconnexion du canal vocal
}

// Construit l'interface vocale : rejoindre/quitter un canal vocal.
static GtkWidget *build_voice_screen(AppWidgets *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(box, "panel-box");
    gtk_widget_set_margin_top(box, 60);
    gtk_widget_set_margin_bottom(box, 60);
    gtk_widget_set_margin_start(box, 60);
    gtk_widget_set_margin_end(box, 60);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    GtkWidget *title = gtk_label_new("CANAL VOCAL");
    gtk_widget_add_css_class(title, "panel-title");
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *status = gtk_label_new("Non connecté à un canal vocal");
    gtk_widget_add_css_class(status, "app-subtitle");
    gtk_box_append(GTK_BOX(box), status);

    GtkWidget *join_btn = gtk_button_new_with_label("REJOINDRE LE CANAL VOCAL");
    gtk_widget_add_css_class(join_btn, "login-button");
    g_signal_connect(join_btn, "clicked", G_CALLBACK(on_join_voice_clicked), app);
    gtk_box_append(GTK_BOX(box), join_btn);

    GtkWidget *leave_btn = gtk_button_new_with_label("QUITTER LE CANAL VOCAL");
    gtk_widget_add_css_class(leave_btn, "danger-button");
    g_signal_connect(leave_btn, "clicked", G_CALLBACK(on_leave_voice_clicked), app);
    gtk_box_append(GTK_BOX(box), leave_btn);

    return box;
}

// ============================================================
// ECRAN 8 : Le PLATEAU PUISSANCE 4
// ============================================================

// Dimensions du plateau de Puissance 4 : 6 lignes x 7 colonnes,
// comme dans le jeu original.
#define P4_ROWS 6
#define P4_COLS 7

/*
 * Tableau représentant l'état du plateau : 0 = vide, 1 = joueur 1,
 * 2 = joueur 2. Mis à jour à chaque coup joué.
 */
static int p4_board[P4_ROWS][P4_COLS] = {0};

/*
 * Référence aux boutons de la grille pour pouvoir les mettre à
 * jour visuellement quand un coup est joué.
 */
static GtkWidget *p4_buttons[P4_ROWS][P4_COLS];

/*
 * Callback appelé quand le joueur clique sur une colonne du plateau.
 * Détermine la ligne la plus basse disponible dans cette colonne
 * et y place le jeton du joueur actif.
 */
static void on_p4_cell_clicked(GtkWidget *widget, gpointer data) {
    int col = GPOINTER_TO_INT(data);

    // Cherche la ligne la plus basse vide dans cette colonne.
    for (int row = P4_ROWS - 1; row >= 0; row--) {
        if (p4_board[row][col] == 0) {
            p4_board[row][col] = 1; // 1 = joueur local pour l'instant
            gtk_widget_add_css_class(p4_buttons[row][col], "p4-cell-player1");
            g_print("Coup joué : colonne %d, ligne %d\n", col, row);
            // TODO : envoyer le coup au serveur via socket_client.c
            return;
        }
    }
    g_print("Colonne %d pleine\n", col);
}

// Construit le plateau de Puissance 4 : une grille GTK de boutons
// cliquables, 6 lignes x 7 colonnes.
// Chaque bouton représente une cellule du plateau.
static GtkWidget *build_p4_screen(AppWidgets *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(box, "panel-box");
    gtk_widget_set_margin_top(box, 40);
    gtk_widget_set_margin_bottom(box, 40);
    gtk_widget_set_margin_start(box, 40);
    gtk_widget_set_margin_end(box, 40);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);

    GtkWidget *title = gtk_label_new("PUISSANCE 4");
    gtk_widget_add_css_class(title, "panel-title");
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *status = gtk_label_new("À votre tour — joueur 1");
    gtk_widget_add_css_class(status, "app-subtitle");
    gtk_box_append(GTK_BOX(box), status);

    // Grille GTK qui représente le plateau de jeu.
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
    gtk_widget_add_css_class(grid, "p4-grid");

    // Création des boutons de la grille, ligne par ligne.
    for (int row = 0; row < P4_ROWS; row++) {
        for (int col = 0; col < P4_COLS; col++) {
            GtkWidget *cell = gtk_button_new_with_label(" ");
            gtk_widget_add_css_class(cell, "p4-cell");
            gtk_widget_set_size_request(cell, 60, 60);

            // On passe le numéro de colonne en paramètre du callback,
            // pour savoir dans quelle colonne le joueur a cliqué.
            g_signal_connect(cell, "clicked", G_CALLBACK(on_p4_cell_clicked),
                             GINT_TO_POINTER(col));

            p4_buttons[row][col] = cell;
            gtk_grid_attach(GTK_GRID(grid), cell, col, row, 1, 1);
        }
    }

    gtk_box_append(GTK_BOX(box), grid);

    GtkWidget *back_btn = gtk_button_new_with_label("← Retour au chat");
    gtk_widget_add_css_class(back_btn, "link-button");
    gtk_box_append(GTK_BOX(box), back_btn);

    return box;
}

// ============================================================
// ASSEMBLAGE GLOBAL : fenêtre principale + navigation
// ============================================================

// Charge la feuille de style CSS et l'applique à toute l'application.
static void load_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "UI/style.css");

    // GTK_STYLE_PROVIDER_PRIORITY_APPLICATION : priorité haute,
    // le style de l'app écrase les styles par défaut de GTK4.
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

/*
 * Fonction appelée au démarrage de l'application GTK.
 * Assemble tous les écrans dans un GtkStack (conteneur de navigation)
 * et affiche d'abord l'écran de login.
 */
static void on_activate(GtkApplication *gtk_app, gpointer user_data) {
    AppWidgets *app = (AppWidgets *)user_data;

    load_css();

    // Création de la fenêtre principale.
    app->window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app->window), "SyncUp");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1100, 700);
    gtk_widget_add_css_class(app->window, "main-window");

    /*
     * GtkStack est le conteneur de navigation : il contient tous les
     * écrans, mais n'en affiche qu'un seul à la fois. On navigue entre
     * eux avec gtk_stack_set_visible_child_name().
     */
    app->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(app->stack),
                                   GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(app->stack), 200);

    // Ajout de chaque écran dans le stack avec un nom unique.
    gtk_stack_add_named(GTK_STACK(app->stack),
                        build_login_screen(app), "login");
    gtk_stack_add_named(GTK_STACK(app->stack),
                        build_register_screen(app), "register");
    gtk_stack_add_named(GTK_STACK(app->stack),
                        build_chat_screen(app), "chat");
    gtk_stack_add_named(GTK_STACK(app->stack),
                        build_moderator_panel(app), "moderator");
    gtk_stack_add_named(GTK_STACK(app->stack),
                        build_admin_panel(app), "admin");
    gtk_stack_add_named(GTK_STACK(app->stack),
                        build_voice_screen(app), "voice");
    gtk_stack_add_named(GTK_STACK(app->stack),
                        build_p4_screen(app), "p4");

    // L'écran affiché au démarrage est le login.
    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "login");

    gtk_window_set_child(GTK_WINDOW(app->window), app->stack);
    gtk_window_present(GTK_WINDOW(app->window));
}

/*
 * Point d'entrée de l'interface graphique.
 * Initialise la structure AppWidgets, crée l'application GTK,
 * et lance la boucle principale de l'interface.
 */
int run_ui(int argc, char **argv) {
    AppWidgets app = {0};

    GtkApplication *gtk_app = gtk_application_new(
        "com.syncup.client",
        G_APPLICATION_DEFAULT_FLAGS
    );

    g_signal_connect(gtk_app, "activate", G_CALLBACK(on_activate), &app);

    int status = g_application_run(G_APPLICATION(gtk_app), argc, argv);
    g_object_unref(gtk_app);

    return status;
}