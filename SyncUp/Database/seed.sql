-- Insérer les utilisateurs
INSERT INTO users (firstname, lastname, email, password, role)
VALUES
('Jane', 'Doe', 'janeDoe@test.com', '$2b$10$fakehashedpassword', 'member'),
('Miyagi', 'Mee', 'Miyagi@test.com', '$2b$10$fakehashedpassword2', 'moderator'),
('Yuri', 'Capitaliste', 'YuriLeCapitaliste@test.com', '$2b$10$fakehashedpassword3', 'admin');

-- Insérer les avatars par défaut pour chaque utilisateur
INSERT INTO avatars (user_id, file_path, file_name, file_type, file_size, is_animated, is_active)
VALUES
(1, 'server/uploads/avatars/JaneDoe.png', 'JaneDoe.png', 'image', 0, FALSE, TRUE),
(2, 'server/uploads/avatars/MiyagiMee.png', 'MiyagiMee.png', 'image', 0, FALSE, TRUE),
(3, 'server/uploads/avatars/YuriCapitaliste.png', 'YuriCapitaliste.png', 'image', 0, FALSE, TRUE);

-- Insérer les canaux
INSERT INTO channels (name, is_private)
VALUES
('general', FALSE),
('random', FALSE),
('privé', TRUE);

-- Insérer les messages
INSERT INTO messages (user_id, channels_id, content)
VALUES
(1, 1, 'Bonjour tout le monde !'),
(2, 1, 'Salut Jane !'),
(3, 1, 'Bienvenue sur SyncUp !'),
(1, 2, 'Canal random ici !');

-- Insérer les réactions
INSERT INTO reactions (user_id, message_id, emoji)
VALUES
(2, 1, '👋'),
(3, 1, '😊');