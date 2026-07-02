/*
 * ui.c — Interface graphique GTK4 du client SyncUp.
 * Les TODO sont maintenant branchés sur socket_client.c via ui_reseau.h.
 */

#include <gtk/gtk.h>
#include <string.h>
#include <time.h>

#include "ui_reseau.h"

/* Canal actuel sélectionné par l'utilisateur */
static int canal_actuel = 0;

typedef struct {
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *channel_list;
    GtkWidget *message_list;
    GtkWidget *message_entry;
    GtkWidget *avatar_image;
    GtkWidget *channel_header;   /* label du canal actif */
    GtkWidget *error_label;      /* label d'erreur sur l'écran login */
    char selected_avatar_path[512];
    char nom_utilisateur[64];
} AppWidgets;

/* Instance globale accessible depuis les callbacks UI réseau */
static AppWidgets *g_app = NULL;

/* ═══════════════════════════════════════════════════════════════════════
 * CALLBACKS RÉSEAU → UI
 * Appelés par socket_client.c via g_idle_add (thread-safe)
 * ═══════════════════════════════════════════════════════════════════════ */

void ui_on_login_ok(const char *nom) {
    if (!g_app) return;
    strncpy(g_app->nom_utilisateur, nom, sizeof(g_app->nom_utilisateur) - 1);
    /* Bascule vers l'écran de chat et charge les canaux */
    gtk_stack_set_visible_child_name(GTK_STACK(g_app->stack), "chat");
    reseau_lister_canaux();
}

void ui_on_erreur(const char *message) {
    if (!g_app) return;
    if (g_app->error_label) {
        gtk_label_set_text(GTK_LABEL(g_app->error_label), message);
        gtk_widget_set_visible(g_app->error_label, TRUE);
    }
}

void ui_on_message_recu(const char *auteur, const char *texte, int canal_id) {
    if (!g_app || canal_id != canal_actuel) return;

    /* Heure courante pour l'affichage */
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char heure[16];
    strftime(heure, sizeof(heure), "%H:%M", tm_info);

    /* Ajoute le message dans la liste */
    GtkWidget *msg_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(msg_box, "message-box");

    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *author_label = gtk_label_new(auteur);
    gtk_widget_add_css_class(author_label, "message-author");
    GtkWidget *time_label = gtk_label_new(heure);
    gtk_widget_add_css_class(time_label, "message-time");
    gtk_box_append(GTK_BOX(header_box), author_label);
    gtk_box_append(GTK_BOX(header_box), time_label);

    GtkWidget *content_label = gtk_label_new(texte);
    gtk_label_set_wrap(GTK_LABEL(content_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(content_label), 0);
    gtk_widget_add_css_class(content_label, "message-content");

    gtk_box_append(GTK_BOX(msg_box), header_box);
    gtk_box_append(GTK_BOX(msg_box), content_label);
    gtk_box_append(GTK_BOX(g_app->message_list), msg_box);
}

void ui_on_canaux_recus(const char *liste) {
    if (!g_app) return;

    /* Vide la liste actuelle */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(g_app->channel_list)) != NULL)
        gtk_list_box_remove(GTK_LIST_BOX(g_app->channel_list),
                            gtk_widget_get_parent(child) ? gtk_widget_get_parent(child) : child);

    /* Reconstruit la liste depuis "id:nom|id:nom|..." */
    char copie[1024];
    strncpy(copie, liste, sizeof(copie) - 1);
    char *token = strtok(copie, "|");
    while (token) {
        /* Format : "1:general" */
        char *sep = strchr(token, ':');
        if (sep) {
            int  id  = atoi(token);
            char *nom = sep + 1;
            char label[128];
            snprintf(label, sizeof(label), "# %s", nom);

            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *lbl = gtk_label_new(label);
            gtk_widget_add_css_class(lbl, "channel-item");
            gtk_label_set_xalign(GTK_LABEL(lbl), 0);
            g_object_set_data(G_OBJECT(row), "canal_id", GINT_TO_POINTER(id));
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), lbl);
            gtk_list_box_append(GTK_LIST_BOX(g_app->channel_list), row);
        }
        token = strtok(NULL, "|");
    }
}

void ui_on_utilisateurs_recus(const char *liste) {
    /* Pour l'instant on affiche juste dans la console */
    g_print("Utilisateurs connectes : %s\n", liste);
}

/* ═══════════════════════════════════════════════════════════════════════
 * ECRAN LOGIN
 * ═══════════════════════════════════════════════════════════════════════ */

static void on_login_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *email_entry    = GTK_WIDGET(g_object_get_data(G_OBJECT(data), "email_entry"));
    GtkWidget *password_entry = GTK_WIDGET(g_object_get_data(G_OBJECT(data), "password_entry"));

    const char *email    = gtk_editable_get_text(GTK_EDITABLE(email_entry));
    const char *password = gtk_editable_get_text(GTK_EDITABLE(password_entry));

    if (strlen(email) == 0 || strlen(password) == 0) {
        ui_on_erreur("Veuillez remplir tous les champs.");
        return;
    }

    /* Cache le message d'erreur précédent */
    if (g_app && g_app->error_label)
        gtk_widget_set_visible(g_app->error_label, FALSE);

    reseau_login(email, password);
}

static void on_register_nav_clicked(GtkWidget *widget, gpointer data) {
    AppWidgets *app = (AppWidgets *)data;
    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "register");
}

static GtkWidget *build_login_screen(AppWidgets *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_top(box, 80);
    gtk_widget_set_margin_bottom(box, 80);
    gtk_widget_set_margin_start(box, 60);
    gtk_widget_set_margin_end(box, 60);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    GtkWidget *title = gtk_label_new("SyncUp");
    gtk_widget_add_css_class(title, "app-title");
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *subtitle = gtk_label_new("Connexion a votre compte");
    gtk_widget_add_css_class(subtitle, "app-subtitle");
    gtk_box_append(GTK_BOX(box), subtitle);

    GtkWidget *email_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(email_entry), "Adresse email");
    gtk_widget_add_css_class(email_entry, "login-entry");
    gtk_box_append(GTK_BOX(box), email_entry);

    GtkWidget *password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry), "Mot de passe");
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    gtk_widget_add_css_class(password_entry, "login-entry");
    gtk_box_append(GTK_BOX(box), password_entry);

    /* Label d'erreur (caché par défaut) */
    app->error_label = gtk_label_new("");
    gtk_widget_add_css_class(app->error_label, "error-label");
    gtk_widget_set_visible(app->error_label, FALSE);
    gtk_box_append(GTK_BOX(box), app->error_label);

    GtkWidget *login_button = gtk_button_new_with_label("SE CONNECTER");
    gtk_widget_add_css_class(login_button, "login-button");
    g_object_set_data(G_OBJECT(login_button), "email_entry", email_entry);
    g_object_set_data(G_OBJECT(login_button), "password_entry", password_entry);
    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_clicked), login_button);
    gtk_box_append(GTK_BOX(box), login_button);

    GtkWidget *register_button = gtk_button_new_with_label("Pas encore de compte ? S'inscrire");
    gtk_widget_add_css_class(register_button, "link-button");
    g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_nav_clicked), app);
    gtk_box_append(GTK_BOX(box), register_button);

    return box;
}

/* ═══════════════════════════════════════════════════════════════════════
 * ECRAN INSCRIPTION
 * ═══════════════════════════════════════════════════════════════════════ */

static void on_register_submit_clicked(GtkWidget *widget, gpointer data) {
    AppWidgets *app = (AppWidgets *)data;
    GtkWidget *fn = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "firstname"));
    GtkWidget *ln = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "lastname"));
    GtkWidget *em = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "email"));
    GtkWidget *pw = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "password"));

    const char *prenom   = gtk_editable_get_text(GTK_EDITABLE(fn));
    const char *nom      = gtk_editable_get_text(GTK_EDITABLE(ln));
    const char *email    = gtk_editable_get_text(GTK_EDITABLE(em));
    const char *password = gtk_editable_get_text(GTK_EDITABLE(pw));

    if (!strlen(prenom) || !strlen(nom) || !strlen(email) || !strlen(password)) {
        ui_on_erreur("Veuillez remplir tous les champs.");
        return;
    }

    reseau_register(nom, prenom, email, password);
    /* Retour au login après inscription */
    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "login");
}

static void on_default_avatar_selected(GtkWidget *widget, gpointer data) {
    AppWidgets *app = (AppWidgets *)g_object_get_data(G_OBJECT(widget), "app");
    const char *path = (const char *)data;
    strncpy(app->selected_avatar_path, path, sizeof(app->selected_avatar_path) - 1);
    g_print("Avatar selectionne : %s\n", path);
}

static void on_file_dialog_response(GObject *source, GAsyncResult *result, gpointer data) {
    AppWidgets *app = (AppWidgets *)data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GFile *file = gtk_file_dialog_open_finish(dialog, result, NULL);
    if (!file) return;
    const char *path = g_file_get_path(file);
    if (path)
        strncpy(app->selected_avatar_path, path, sizeof(app->selected_avatar_path) - 1);
    g_object_unref(file);
}

static void on_upload_avatar_clicked(GtkWidget *widget, gpointer data) {
    AppWidgets *app = (AppWidgets *)data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Choisir un avatar");
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Images");
    gtk_file_filter_add_mime_type(filter, "image/png");
    gtk_file_filter_add_mime_type(filter, "image/jpeg");
    gtk_file_filter_add_mime_type(filter, "image/gif");
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    gtk_file_dialog_open(dialog, GTK_WINDOW(app->window), NULL,
                         on_file_dialog_response, app);
    g_object_unref(filter);
    g_object_unref(filters);
}

static GtkWidget *build_register_screen(AppWidgets *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(box, 40);
    gtk_widget_set_margin_bottom(box, 40);
    gtk_widget_set_margin_start(box, 60);
    gtk_widget_set_margin_end(box, 60);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    GtkWidget *title = gtk_label_new("CREER UN COMPTE");
    gtk_widget_add_css_class(title, "app-title");
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *fn = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(fn), "Prenom");
    gtk_widget_add_css_class(fn, "login-entry");
    gtk_box_append(GTK_BOX(box), fn);

    GtkWidget *ln = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ln), "Nom");
    gtk_widget_add_css_class(ln, "login-entry");
    gtk_box_append(GTK_BOX(box), ln);

    GtkWidget *em = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(em), "Adresse email");
    gtk_widget_add_css_class(em, "login-entry");
    gtk_box_append(GTK_BOX(box), em);

    GtkWidget *pw = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(pw), "Mot de passe");
    gtk_entry_set_visibility(GTK_ENTRY(pw), FALSE);
    gtk_widget_add_css_class(pw, "login-entry");
    gtk_box_append(GTK_BOX(box), pw);

    GtkWidget *avatar_label = gtk_label_new("Choisir un avatar :");
    gtk_widget_add_css_class(avatar_label, "app-subtitle");
    gtk_box_append(GTK_BOX(box), avatar_label);

    GtkWidget *avatar_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(avatar_grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(avatar_grid), 10);

    const char *avatars[] = {
        "server/uploads/avatars/default1.png",
        "server/uploads/avatars/default2.png",
        "server/uploads/avatars/default3.png",
        "server/uploads/avatars/default4.png",
        "server/uploads/avatars/default5.png",
        "server/uploads/avatars/default6.png"
    };
    for (int i = 0; i < 6; i++) {
        GtkWidget *btn = gtk_button_new();
        GtkWidget *img = gtk_image_new_from_file(avatars[i]);
        gtk_image_set_pixel_size(GTK_IMAGE(img), 48);
        gtk_button_set_child(GTK_BUTTON(btn), img);
        gtk_widget_add_css_class(btn, "avatar-btn");
        g_object_set_data(G_OBJECT(btn), "app", app);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_default_avatar_selected),
                         (gpointer)avatars[i]);
        gtk_grid_attach(GTK_GRID(avatar_grid), btn, i % 3, i / 3, 1, 1);
    }
    gtk_box_append(GTK_BOX(box), avatar_grid);

    GtkWidget *upload_btn = gtk_button_new_with_label("Choisir depuis mon ordinateur...");
    gtk_widget_add_css_class(upload_btn, "link-button");
    g_signal_connect(upload_btn, "clicked", G_CALLBACK(on_upload_avatar_clicked), app);
    gtk_box_append(GTK_BOX(box), upload_btn);

    GtkWidget *submit = gtk_button_new_with_label("CREER MON COMPTE");
    gtk_widget_add_css_class(submit, "login-button");
    g_object_set_data(G_OBJECT(submit), "firstname", fn);
    g_object_set_data(G_OBJECT(submit), "lastname",  ln);
    g_object_set_data(G_OBJECT(submit), "email",     em);
    g_object_set_data(G_OBJECT(submit), "password",  pw);
    g_signal_connect(submit, "clicked", G_CALLBACK(on_register_submit_clicked), app);
    gtk_box_append(GTK_BOX(box), submit);

    GtkWidget *back = gtk_button_new_with_label("Deja un compte ? Se connecter");
    gtk_widget_add_css_class(back, "link-button");
    g_signal_connect(back, "clicked", G_CALLBACK(on_register_nav_clicked), app);
    gtk_box_append(GTK_BOX(box), back);

    return box;
}

/* ═══════════════════════════════════════════════════════════════════════
 * ECRAN CHAT
 * ═══════════════════════════════════════════════════════════════════════ */

static void on_send_message_clicked(GtkWidget *widget, gpointer data) {
    AppWidgets *app = (AppWidgets *)data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(app->message_entry));
    if (strlen(text) == 0) return;

    reseau_envoyer_message(canal_actuel, text);

    /* Affiche aussi le message localement */
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char heure[16];
    strftime(heure, sizeof(heure), "%H:%M", tm_info);
    ui_on_message_recu(app->nom_utilisateur, text, canal_actuel);

    gtk_editable_set_text(GTK_EDITABLE(app->message_entry), "");
}

static void on_message_entry_activate(GtkWidget *widget, gpointer data) {
    on_send_message_clicked(widget, data);
}

static void on_channel_selected(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    AppWidgets *app = (AppWidgets *)data;
    if (!row) return;

    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "canal_id"));
    canal_actuel = id;
    reseau_rejoindre_canal(id);

    /* Met à jour le header */
    GtkWidget *child = gtk_list_box_row_get_child(row);
    if (child) {
        const char *nom = gtk_label_get_text(GTK_LABEL(child));
        gtk_label_set_text(GTK_LABEL(app->channel_header), nom);
    }

    /* Vide les messages du canal précédent */
    GtkWidget *msg;
    while ((msg = gtk_widget_get_first_child(app->message_list)) != NULL)
        gtk_box_remove(GTK_BOX(app->message_list), msg);
}

static void on_request_access_clicked(GtkWidget *widget, gpointer data) {
    reseau_demander_acces(canal_actuel);
}

void ui_add_message(AppWidgets *app, const char *author,
                    const char *timestamp, const char *content) {
    GtkWidget *msg_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(msg_box, "message-box");
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *author_label = gtk_label_new(author);
    gtk_widget_add_css_class(author_label, "message-author");
    GtkWidget *time_label = gtk_label_new(timestamp);
    gtk_widget_add_css_class(time_label, "message-time");
    gtk_box_append(GTK_BOX(header_box), author_label);
    gtk_box_append(GTK_BOX(header_box), time_label);
    GtkWidget *content_label = gtk_label_new(content);
    gtk_label_set_wrap(GTK_LABEL(content_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(content_label), 0);
    gtk_widget_add_css_class(content_label, "message-content");
    gtk_box_append(GTK_BOX(msg_box), header_box);
    gtk_box_append(GTK_BOX(msg_box), content_label);
    gtk_box_append(GTK_BOX(app->message_list), msg_box);
}

static GtkWidget *build_chat_screen(AppWidgets *app) {
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(main_box, "chat-main");

    /* Sidebar */
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(sidebar, "sidebar");
    gtk_widget_set_size_request(sidebar, 220, -1);

    GtkWidget *server_name = gtk_label_new("SyncUp");
    gtk_widget_add_css_class(server_name, "server-name");
    gtk_box_append(GTK_BOX(sidebar), server_name);

    GtkWidget *channel_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(channel_scroll, TRUE);

    app->channel_list = gtk_list_box_new();
    gtk_widget_add_css_class(app->channel_list, "channel-list");
    g_signal_connect(app->channel_list, "row-activated",
                     G_CALLBACK(on_channel_selected), app);

    /* Canaux par défaut — remplacés à la connexion */
    const char *channels[] = {"# general", "# random", "# prive"};
    for (int i = 0; i < 3; i++) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(channels[i]);
        gtk_widget_add_css_class(label, "channel-item");
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        g_object_set_data(G_OBJECT(row), "canal_id", GINT_TO_POINTER(i));
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        gtk_list_box_append(GTK_LIST_BOX(app->channel_list), row);
    }

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(channel_scroll), app->channel_list);
    gtk_box_append(GTK_BOX(sidebar), channel_scroll);

    GtkWidget *request_btn = gtk_button_new_with_label("Demander acces prive");
    gtk_widget_add_css_class(request_btn, "request-btn");
    g_signal_connect(request_btn, "clicked", G_CALLBACK(on_request_access_clicked), app);
    gtk_box_append(GTK_BOX(sidebar), request_btn);

    gtk_box_append(GTK_BOX(main_box), sidebar);

    /* Zone centrale */
    GtkWidget *center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(center_box, TRUE);

    app->channel_header = gtk_label_new("# general");
    gtk_widget_add_css_class(app->channel_header, "channel-header");
    gtk_box_append(GTK_BOX(center_box), app->channel_header);

    GtkWidget *msg_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(msg_scroll, TRUE);

    app->message_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(app->message_list, "message-list");
    gtk_widget_set_margin_start(app->message_list, 16);
    gtk_widget_set_margin_end(app->message_list, 16);
    gtk_widget_set_margin_top(app->message_list, 16);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(msg_scroll), app->message_list);
    gtk_box_append(GTK_BOX(center_box), msg_scroll);

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
    g_signal_connect(app->message_entry, "activate",
                     G_CALLBACK(on_message_entry_activate), app);

    GtkWidget *send_btn = gtk_button_new_with_label(">");
    gtk_widget_add_css_class(send_btn, "send-button");
    g_signal_connect(send_btn, "clicked", G_CALLBACK(on_send_message_clicked), app);

    gtk_box_append(GTK_BOX(input_box), app->message_entry);
    gtk_box_append(GTK_BOX(input_box), send_btn);
    gtk_box_append(GTK_BOX(center_box), input_box);
    gtk_box_append(GTK_BOX(main_box), center_box);

    return main_box;
}

/* ═══════════════════════════════════════════════════════════════════════
 * RÉACTIONS EMOJI
 * ═══════════════════════════════════════════════════════════════════════ */

static void on_reaction_clicked(GtkWidget *widget, gpointer data) {
    const char *emoji = (const char *)data;
    reseau_reagir(0, emoji); /* TODO : passer le vrai message_id */
    g_print("Reaction : %s\n", emoji);
}

GtkWidget *build_reaction_bar(void) {
    GtkWidget *reaction_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(reaction_box, "reaction-bar");
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

/* ═══════════════════════════════════════════════════════════════════════
 * PANNEAU MODÉRATEUR
 * ═══════════════════════════════════════════════════════════════════════ */

static void on_timeout_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *user_entry = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "user"));
    GtkWidget *dur_entry  = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "duree"));
    const char *user_str  = gtk_editable_get_text(GTK_EDITABLE(user_entry));
    const char *dur_str   = gtk_editable_get_text(GTK_EDITABLE(dur_entry));
    int user_id = atoi(user_str);
    int duree   = atoi(dur_str) * 60; /* minutes → secondes */
    reseau_timeout(user_id, canal_actuel, duree, "Timeout via panneau moderateur");
}

static void on_delete_message_clicked(GtkWidget *widget, gpointer data) {
    g_print("Suppression de message (non implemente cote serveur)\n");
}

static GtkWidget *build_moderator_panel(AppWidgets *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(box, "panel-box");
    gtk_widget_set_margin_top(box, 40); gtk_widget_set_margin_bottom(box, 40);
    gtk_widget_set_margin_start(box, 40); gtk_widget_set_margin_end(box, 40);

    GtkWidget *title = gtk_label_new("PANNEAU MODERATEUR");
    gtk_widget_add_css_class(title, "panel-title");
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *timeout_label = gtk_label_new("Mettre en timeout :");
    gtk_widget_add_css_class(timeout_label, "panel-section-label");
    gtk_box_append(GTK_BOX(box), timeout_label);

    GtkWidget *timeout_user = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(timeout_user), "ID utilisateur");
    gtk_widget_add_css_class(timeout_user, "login-entry");
    gtk_box_append(GTK_BOX(box), timeout_user);

    GtkWidget *timeout_duration = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(timeout_duration), "Duree (minutes)");
    gtk_widget_add_css_class(timeout_duration, "login-entry");
    gtk_box_append(GTK_BOX(box), timeout_duration);

    GtkWidget *timeout_btn = gtk_button_new_with_label("APPLIQUER TIMEOUT");
    gtk_widget_add_css_class(timeout_btn, "action-button");
    g_object_set_data(G_OBJECT(timeout_btn), "user",  timeout_user);
    g_object_set_data(G_OBJECT(timeout_btn), "duree", timeout_duration);
    g_signal_connect(timeout_btn, "clicked", G_CALLBACK(on_timeout_clicked), app);
    gtk_box_append(GTK_BOX(box), timeout_btn);

    GtkWidget *delete_label = gtk_label_new("Supprimer un message :");
    gtk_widget_add_css_class(delete_label, "panel-section-label");
    gtk_box_append(GTK_BOX(box), delete_label);

    GtkWidget *message_id_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(message_id_entry), "ID du message");
    gtk_widget_add_css_class(message_id_entry, "login-entry");
    gtk_box_append(GTK_BOX(box), message_id_entry);

    GtkWidget *delete_btn = gtk_button_new_with_label("SUPPRIMER MESSAGE");
    gtk_widget_add_css_class(delete_btn, "action-button");
    g_signal_connect(delete_btn, "clicked", G_CALLBACK(on_delete_message_clicked), app);
    gtk_box_append(GTK_BOX(box), delete_btn);

    GtkWidget *back_btn = gtk_button_new_with_label("<- Retour au chat");
    gtk_widget_add_css_class(back_btn, "link-button");
    g_signal_connect(back_btn, "clicked",
                     G_CALLBACK(gtk_stack_set_visible_child_name), app->stack);
    gtk_box_append(GTK_BOX(box), back_btn);

    return box;
}

/* ═══════════════════════════════════════════════════════════════════════
 * PANNEAU ADMIN
 * ═══════════════════════════════════════════════════════════════════════ */

static void on_ban_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *entry = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "user"));
    int user_id = atoi(gtk_editable_get_text(GTK_EDITABLE(entry)));
    reseau_ban(user_id);
}

static void on_role_change_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *entry    = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "user"));
    GtkWidget *dropdown = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "dropdown"));
    int user_id = atoi(gtk_editable_get_text(GTK_EDITABLE(entry)));
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(dropdown));
    const char *roles[] = {"member", "moderator", "admin"};
    if (selected < 3)
        reseau_changer_role(user_id, roles[selected]);
}

static void on_create_channel_clicked(GtkWidget *widget, gpointer data) {
    g_print("Creation de canal (a implementer cote serveur)\n");
}

static void on_delete_channel_clicked(GtkWidget *widget, gpointer data) {
    g_print("Suppression de canal (a implementer cote serveur)\n");
}

static GtkWidget *build_admin_panel(AppWidgets *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(box, "panel-box");
    gtk_widget_set_margin_top(box, 40); gtk_widget_set_margin_bottom(box, 40);
    gtk_widget_set_margin_start(box, 40); gtk_widget_set_margin_end(box, 40);

    GtkWidget *title = gtk_label_new("PANNEAU ADMIN");
    gtk_widget_add_css_class(title, "panel-title");
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *ban_label = gtk_label_new("Bannir un utilisateur :");
    gtk_widget_add_css_class(ban_label, "panel-section-label");
    gtk_box_append(GTK_BOX(box), ban_label);

    GtkWidget *ban_user = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ban_user), "ID utilisateur");
    gtk_widget_add_css_class(ban_user, "login-entry");
    gtk_box_append(GTK_BOX(box), ban_user);

    GtkWidget *ban_btn = gtk_button_new_with_label("BANNIR");
    gtk_widget_add_css_class(ban_btn, "danger-button");
    g_object_set_data(G_OBJECT(ban_btn), "user", ban_user);
    g_signal_connect(ban_btn, "clicked", G_CALLBACK(on_ban_clicked), app);
    gtk_box_append(GTK_BOX(box), ban_btn);

    GtkWidget *role_label = gtk_label_new("Gerer les roles :");
    gtk_widget_add_css_class(role_label, "panel-section-label");
    gtk_box_append(GTK_BOX(box), role_label);

    GtkWidget *role_user = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(role_user), "ID utilisateur");
    gtk_widget_add_css_class(role_user, "login-entry");
    gtk_box_append(GTK_BOX(box), role_user);

    GtkWidget *role_dropdown = gtk_drop_down_new_from_strings(
        (const char *[]){"member", "moderator", "admin", NULL});
    gtk_widget_add_css_class(role_dropdown, "role-dropdown");
    gtk_box_append(GTK_BOX(box), role_dropdown);

    GtkWidget *role_btn = gtk_button_new_with_label("CHANGER ROLE");
    gtk_widget_add_css_class(role_btn, "action-button");
    g_object_set_data(G_OBJECT(role_btn), "user",     role_user);
    g_object_set_data(G_OBJECT(role_btn), "dropdown", role_dropdown);
    g_signal_connect(role_btn, "clicked", G_CALLBACK(on_role_change_clicked), app);
    gtk_box_append(GTK_BOX(box), role_btn);

    GtkWidget *ch_label = gtk_label_new("Gerer les canaux :");
    gtk_widget_add_css_class(ch_label, "panel-section-label");
    gtk_box_append(GTK_BOX(box), ch_label);

    GtkWidget *ch_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ch_entry), "Nom du canal");
    gtk_widget_add_css_class(ch_entry, "login-entry");
    gtk_box_append(GTK_BOX(box), ch_entry);

    GtkWidget *create_btn = gtk_button_new_with_label("CREER CANAL");
    gtk_widget_add_css_class(create_btn, "action-button");
    g_signal_connect(create_btn, "clicked", G_CALLBACK(on_create_channel_clicked), app);
    gtk_box_append(GTK_BOX(box), create_btn);

    GtkWidget *del_btn = gtk_button_new_with_label("SUPPRIMER CANAL");
    gtk_widget_add_css_class(del_btn, "danger-button");
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_channel_clicked), app);
    gtk_box_append(GTK_BOX(box), del_btn);

    GtkWidget *back_btn = gtk_button_new_with_label("<- Retour au chat");
    gtk_widget_add_css_class(back_btn, "link-button");
    gtk_box_append(GTK_BOX(box), back_btn);

    return box;
}

/* ═══════════════════════════════════════════════════════════════════════
 * INTERFACE VOCALE (placeholder)
 * ═══════════════════════════════════════════════════════════════════════ */

static void on_join_voice_clicked(GtkWidget *widget, gpointer data) {
    g_print("Canal vocal (non implemente)\n");
}
static void on_leave_voice_clicked(GtkWidget *widget, gpointer data) {
    g_print("Quitter canal vocal (non implemente)\n");
}

static GtkWidget *build_voice_screen(AppWidgets *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(box, "panel-box");
    gtk_widget_set_margin_top(box, 60); gtk_widget_set_margin_bottom(box, 60);
    gtk_widget_set_margin_start(box, 60); gtk_widget_set_margin_end(box, 60);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    GtkWidget *title = gtk_label_new("CANAL VOCAL");
    gtk_widget_add_css_class(title, "panel-title");
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *status = gtk_label_new("Non connecte a un canal vocal");
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

/* ═══════════════════════════════════════════════════════════════════════
 * PUISSANCE 4
 * ═══════════════════════════════════════════════════════════════════════ */

#define P4_ROWS 6
#define P4_COLS 7
static int p4_board[P4_ROWS][P4_COLS] = {0};
static GtkWidget *p4_buttons[P4_ROWS][P4_COLS];

static void on_p4_cell_clicked(GtkWidget *widget, gpointer data) {
    int col = GPOINTER_TO_INT(data);
    for (int row = P4_ROWS - 1; row >= 0; row--) {
        if (p4_board[row][col] == 0) {
            p4_board[row][col] = 1;
            gtk_widget_add_css_class(p4_buttons[row][col], "p4-cell-player1");
            g_print("P4 coup : colonne %d ligne %d\n", col, row);
            /* TODO : envoyer le coup via reseau */
            return;
        }
    }
}

static GtkWidget *build_p4_screen(AppWidgets *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_add_css_class(box, "panel-box");
    gtk_widget_set_margin_top(box, 40); gtk_widget_set_margin_bottom(box, 40);
    gtk_widget_set_margin_start(box, 40); gtk_widget_set_margin_end(box, 40);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);

    GtkWidget *title = gtk_label_new("PUISSANCE 4");
    gtk_widget_add_css_class(title, "panel-title");
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
    gtk_widget_add_css_class(grid, "p4-grid");

    for (int row = 0; row < P4_ROWS; row++) {
        for (int col = 0; col < P4_COLS; col++) {
            GtkWidget *cell = gtk_button_new_with_label(" ");
            gtk_widget_add_css_class(cell, "p4-cell");
            gtk_widget_set_size_request(cell, 60, 60);
            g_signal_connect(cell, "clicked", G_CALLBACK(on_p4_cell_clicked),
                             GINT_TO_POINTER(col));
            p4_buttons[row][col] = cell;
            gtk_grid_attach(GTK_GRID(grid), cell, col, row, 1, 1);
        }
    }
    gtk_box_append(GTK_BOX(box), grid);

    GtkWidget *back_btn = gtk_button_new_with_label("<- Retour au chat");
    gtk_widget_add_css_class(back_btn, "link-button");
    gtk_box_append(GTK_BOX(box), back_btn);

    return box;
}

/* ═══════════════════════════════════════════════════════════════════════
 * ASSEMBLAGE + LANCEMENT
 * ═══════════════════════════════════════════════════════════════════════ */

static void load_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "UI/style.css");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void on_activate(GtkApplication *gtk_app, gpointer user_data) {
    AppWidgets *app = (AppWidgets *)user_data;
    g_app = app;

    load_css();

    app->window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app->window), "SyncUp");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1100, 700);
    gtk_widget_add_css_class(app->window, "main-window");

    app->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(app->stack),
                                   GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(app->stack), 200);

    gtk_stack_add_named(GTK_STACK(app->stack), build_login_screen(app),     "login");
    gtk_stack_add_named(GTK_STACK(app->stack), build_register_screen(app),  "register");
    gtk_stack_add_named(GTK_STACK(app->stack), build_chat_screen(app),      "chat");
    gtk_stack_add_named(GTK_STACK(app->stack), build_moderator_panel(app),  "moderator");
    gtk_stack_add_named(GTK_STACK(app->stack), build_admin_panel(app),      "admin");
    gtk_stack_add_named(GTK_STACK(app->stack), build_voice_screen(app),     "voice");
    gtk_stack_add_named(GTK_STACK(app->stack), build_p4_screen(app),        "p4");

    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "login");
    gtk_window_set_child(GTK_WINDOW(app->window), app->stack);
    gtk_window_present(GTK_WINDOW(app->window));
}

int run_ui(int argc, char **argv) {
    AppWidgets app = {0};
    GtkApplication *gtk_app = gtk_application_new(
        "com.syncup.client", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(on_activate), &app);
    int status = g_application_run(G_APPLICATION(gtk_app), argc, argv);
    g_object_unref(gtk_app);
    return status;
}