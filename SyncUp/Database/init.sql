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

/*
 *
 *La base de données est organisée autour de quatre tables principales : users pour les utilisateurs,
 *channels pour les salons de discussion, messages pour les messages échangés et reactions pour les réactions associées aux messages.
 *Les clés étrangères permettent de garantir l'intégrité des relations entre ces différentes tables.
 *
 */