/*
 * Ce fichier init.sql permet d'initialiser la base de données.
 * Il crée les différentes tables nécessaires au fonctionnement
 * de l'application de messagerie si elles n'existent pas déjà.
 */

-- Création de la table des utilisateurs.
-- Chaque utilisateur possède un identifiant unique,
-- des informations personnelles et un rôle.
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,                  -- Identifiant unique généré automatiquement
    firstname VARCHAR(50),                  -- Prénom de l'utilisateur
    lastname VARCHAR(50),                   -- Nom de famille de l'utilisateur
    email VARCHAR(100) UNIQUE,              -- Adresse email unique
    password VARCHAR(255),                  -- Mot de passe stocké sous forme chiffrée
    role VARCHAR(20) DEFAULT 'member'       -- Rôle de l'utilisateur (member par défaut)
);

-- Création de la table des canaux de discussion.
-- Un canal permet de regrouper les messages des utilisateurs.
CREATE TABLE IF NOT EXISTS channels (
    id SERIAL PRIMARY KEY,                  -- Identifiant unique du canal
    name VARCHAR(50) UNIQUE,                -- Nom unique du canal
    is_private BOOLEAN DEFAULT FALSE        -- Indique si le canal est privé ou public
);

-- Création de la table des messages.
-- Chaque message est associé à un utilisateur et à un canal.
CREATE TABLE IF NOT EXISTS messages (
    id SERIAL PRIMARY KEY,                  -- Identifiant unique du message
    user_id INT REFERENCES users(id),       -- Référence vers l'auteur du message
    channels_id INT REFERENCES channels(id),-- Référence vers le canal concerné
    content TEXT,                           -- Contenu du message
    created_at TIMESTAMP DEFAULT NOW()      -- Date et heure d'envoi du message
);

-- Création de la table des réactions.
-- Une réaction est associée à un utilisateur et à un message.
CREATE TABLE IF NOT EXISTS reactions (
    id SERIAL PRIMARY KEY,                  -- Identifiant unique de la réaction
    user_id INT REFERENCES users(id),       -- Utilisateur ayant ajouté la réaction
    message_id INT REFERENCES messages(id), -- Message concerné par la réaction
    emoji VARCHAR(10)                       -- Réaction enregistrée sous forme de texte
);

-- Création de la table des demandes d'accès aux canaux.
-- Une demande est créée lorsqu'un utilisateur souhaite rejoindre un canal privé.
CREATE TABLE IF NOT EXISTS channel_requests (
    id SERIAL PRIMARY KEY,                      -- Identifiant unique de la demande
    user_id INT REFERENCES users(id),           -- Utilisateur faisant la demande
    channels_id INT REFERENCES channels(id),    -- Canal concerné par la demande
    status VARCHAR(20) DEFAULT 'pending',       -- Statut de la demande (pending ou approved)
    created_at TIMESTAMP DEFAULT NOW()          -- Date et heure de la demande
);

-- Création de la table des messages supprimés.
-- Permet de garder une trace des messages supprimés à des fins d'audit.
CREATE TABLE IF NOT EXISTS deleted_messages (
    id SERIAL PRIMARY KEY,                      -- Identifiant unique de l'entrée
    message_id INT,                             -- Identifiant du message supprimé
    user_id INT REFERENCES users(id),           -- Utilisateur ayant supprimé le message
    content TEXT,                               -- Contenu du message supprimé
    deleted_at TIMESTAMP DEFAULT NOW()          -- Date et heure de la suppression
);

-- Création de la table des timeouts.
-- Permet de temporairement empêcher un utilisateur d'envoyer des messages.
CREATE TABLE IF NOT EXISTS timeouts (
    id SERIAL PRIMARY KEY,                          -- Identifiant unique du timeout
    user_id INT REFERENCES users(id),               -- Utilisateur concerné par le timeout
    channel_id INT REFERENCES channels(id),         -- Canal concerné par le timeout
    reason TEXT,                                    -- Raison du timeout
    expires_at TIMESTAMP,                           -- Date et heure d'expiration du timeout
    created_at TIMESTAMP DEFAULT NOW()              -- Date et heure du timeout
);

-- Création de la table des pièces jointes.
-- Permet d'associer des fichiers (images, vidéos, liens) à un message.
CREATE TABLE IF NOT EXISTS attachments (
    id SERIAL PRIMARY KEY,                       -- Identifiant unique de la pièce jointe
    message_id INT REFERENCES messages(id),      -- Message associé à la pièce jointe
    file_path VARCHAR(255),                      -- Chemin vers le fichier stocké
    file_type VARCHAR(50),                       -- Type de fichier (image, video, gif, link...)
    file_name VARCHAR(255),                      -- Nom original du fichier
    file_size INT CHECK (file_size <= 26214400), -- Taille max du fichier à 25 MB en octets
    created_at TIMESTAMP DEFAULT NOW()           -- Date d'envoi de la pièce jointe
);

-- Création de la table des bannissements.
-- Enregistre les utilisateurs bannis ainsi que la raison du bannissement.
CREATE TABLE IF NOT EXISTS bans (
    id SERIAL PRIMARY KEY,                      -- Identifiant unique du bannissement
    user_id INT REFERENCES users(id),           -- Utilisateur banni
    banned_by INT REFERENCES users(id),         -- Administrateur ayant effectué le ban
    reason TEXT,                                -- Raison du bannissement
    is_active BOOLEAN DEFAULT TRUE,             -- le ban est-il toujours en vigueur ?
    unbanned_by INT REFERENCES users(id),       -- qui a débanni (NULL si toujours banni)
    unbanned_at TIMESTAMP,                      -- quand a eu lieu le déban (NULL si toujours banni)
    created_at TIMESTAMP DEFAULT NOW()          -- Date et heure du bannissement
);

-- Création de la table des changements de rôle.
-- Permet de garder une trace des modifications de rôle à des fins d'audit.
CREATE TABLE IF NOT EXISTS role_changes (
    id SERIAL PRIMARY KEY,                      -- Identifiant unique du changement
    user_id INT REFERENCES users(id),           -- Utilisateur dont le rôle a été modifié
    changed_by INT REFERENCES users(id),        -- Administrateur ayant modifié le rôle
    old_role VARCHAR(20),                       -- Ancien rôle de l'utilisateur
    new_role VARCHAR(20),                       -- Nouveau rôle de l'utilisateur
    changed_at TIMESTAMP DEFAULT NOW()          -- Date et heure du changement de rôle
);

-- Création de la table des avatars.
-- Permet de stocker la photo de profil de chaque utilisateur.
-- L'historique des anciennes photos de profil est conservé.
CREATE TABLE IF NOT EXISTS avatars (
    id SERIAL PRIMARY KEY,                          -- Identifiant unique de l'avatar
    user_id INT REFERENCES users(id),               -- Utilisateur concerné
    file_path VARCHAR(255),                         -- Chemin vers le fichier
    file_name VARCHAR(255),                         -- Nom original du fichier
    file_type VARCHAR(10),                          -- Type : image ou gif
    file_size INT CHECK (file_size <= 5242880),     -- Taille max 5 MB en octets
    is_animated BOOLEAN DEFAULT FALSE,              -- Indique si c'est un gif animé
    is_active BOOLEAN DEFAULT TRUE,                 -- PP actuellement utilisée
    created_at TIMESTAMP DEFAULT NOW()              -- Date d'upload
);

-- Création de la table des parties de Puissance 4.
-- Enregistre chaque partie jouée entre deux utilisateurs.
CREATE TABLE IF NOT EXISTS p4_games (
    id SERIAL PRIMARY KEY,                      -- Identifiant unique de la partie
    player1_id INT REFERENCES users(id),        -- Premier joueur
    player2_id INT REFERENCES users(id),        -- Deuxième joueur
    winner_id INT REFERENCES users(id),         -- Gagnant de la partie (NULL si match nul)
    channel_id INT REFERENCES channels(id),     -- Canal où la partie se déroule
    status VARCHAR(20) DEFAULT 'ongoing',       -- Statut : ongoing, finished, abandoned
    created_at TIMESTAMP DEFAULT NOW()          -- Date de début de la partie
);

-- Création de la table des coups joués.
-- Enregistre chaque coup joué durant une partie.
CREATE TABLE IF NOT EXISTS p4_moves (
    id SERIAL PRIMARY KEY,                      -- Identifiant unique du coup
    game_id INT REFERENCES p4_games(id),        -- Partie concernée
    player_id INT REFERENCES users(id),         -- Joueur ayant joué le coup
    column_played INT,                          -- Colonne jouée (0 à 6)
    move_number INT,                            -- Numéro du coup dans la partie
    played_at TIMESTAMP DEFAULT NOW()           -- Date et heure du coup
);

-- Création de la table des scores de Puissance 4.
-- Enregistre les statistiques de chaque joueur.
CREATE TABLE IF NOT EXISTS p4_scores (
    id SERIAL PRIMARY KEY,                      -- Identifiant unique du score
    user_id INT REFERENCES users(id),           -- Joueur concerné
    wins INT DEFAULT 0,                         -- Nombre de victoires
    losses INT DEFAULT 0,                       -- Nombre de défaites
    draws INT DEFAULT 0,                        -- Nombre de matchs nuls
    updated_at TIMESTAMP DEFAULT NOW()          -- Date de dernière mise à jour
);

/*
 * La base de données est organisée autour de quatorze tables principales :
 * users pour les utilisateurs, channels pour les salons de discussion,
 * messages pour les messages échangés, reactions pour les réactions,
 * channel_requests pour les demandes d'accès aux canaux privés,
 * deleted_messages pour l'audit des suppressions, timeouts pour les
 * mises en silence temporaires, attachments pour les pièces jointes,
 * bans pour les bannissements, role_changes pour l'audit des
 * changements de rôle, avatars pour les photos de profil,
 * p4_games pour les parties de Puissance 4, p4_moves pour les
 * coups joués et p4_scores pour les statistiques des joueurs.
 * Les clés étrangères permettent de garantir l'intégrité des relations
 * entre ces différentes tables.
 */