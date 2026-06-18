/*
 * Ce fichier drop.sql permet de supprimer les tables de la base de données.
 * Il est généralement utilisé avant une réinitialisation complète
 * de la base de données ou lors de la désinstallation de l'application.
 *
 * Les tables sont supprimées dans l'ordre inverse de leurs dépendances
 * afin d'éviter les erreurs liées aux clés étrangères.
 */

-- Suppression de la table contenant les réactions aux messages.
DROP TABLE IF EXISTS reactions;

-- Suppression de la table contenant les messages.
DROP TABLE IF EXISTS messages;

-- Suppression de la table contenant les canaux de discussion.
DROP TABLE IF EXISTS channels;

-- Suppression de la table contenant les utilisateurs.
DROP TABLE IF EXISTS users;

/*
 *
 *IF EXISTS permet d'éviter une erreur si la table n'existe pas.
 *Les tables sont supprimées dans l'ordre reactions → messages → channels → users car certaines dépendent d'autres via des clés étrangères.
 *L'exécution de ce script entraîne la perte de toutes les données stockées dans ces tables.
 *
 */