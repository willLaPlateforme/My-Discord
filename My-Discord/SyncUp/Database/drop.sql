/*
 * Ce fichier drop.sql permet de supprimer les tables de la base de données.
 * Il est généralement utilisé avant une réinitialisation complète
 * de la base de données ou lors de la désinstallation de l'application.
 *
 * Les tables sont supprimées dans l'ordre inverse de leurs dépendances
 * afin d'éviter les erreurs liées aux clés étrangères.
 */

-- Suppression de la table des scores de Puissance 4.
DROP TABLE IF EXISTS p4_scores;

-- Suppression de la table des coups de Puissance 4.
DROP TABLE IF EXISTS p4_moves;

-- Suppression de la table des parties de Puissance 4.
DROP TABLE IF EXISTS p4_games;

-- Suppression de la table contenant les avatars des utilisateurs.
DROP TABLE IF EXISTS avatars;

-- Suppression de la table contenant les changements de rôle des utilisateurs.
DROP TABLE IF EXISTS role_changes;

-- Suppression de la table contenant les utilisateurs bannis.
DROP TABLE IF EXISTS bans;

-- Suppression de la table contenant les timeouts des utilisateurs.
DROP TABLE IF EXISTS timeouts;

-- Suppression de la table contenant les messages supprimés.
DROP TABLE IF EXISTS deleted_messages;

-- Suppression de la table contenant les pièces jointes.
DROP TABLE IF EXISTS attachments;

-- Suppression de la table contenant les demandes d'accès aux canaux.
DROP TABLE IF EXISTS channel_requests;

-- Suppression de la table contenant les réactions aux messages.
DROP TABLE IF EXISTS reactions;

-- Suppression de la table contenant les messages.
DROP TABLE IF EXISTS messages;

-- Suppression de la table contenant les canaux de discussion.
DROP TABLE IF EXISTS channels;

-- Suppression de la table contenant les utilisateurs.
DROP TABLE IF EXISTS users;

/*
 * IF EXISTS permet d'éviter une erreur si la table n'existe pas.
 * Les tables sont supprimées dans l'ordre inverse de leur création :
 * p4_scores → p4_moves → p4_games → avatars → role_changes → bans →
 * timeouts → deleted_messages → attachments → channel_requests →
 * reactions → messages → channels → users.
 * Cet ordre respecte les dépendances entre tables via les clés étrangères.
 * L'exécution de ce script entraîne la perte de toutes les données
 * stockées dans ces tables.
 */