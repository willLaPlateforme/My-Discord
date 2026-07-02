/*
 * Ce fichier crypto.h déclare les fonctions permettant de hacher
 * et de vérifier les mots de passe des utilisateurs.
 * Utilise SHA-256 (fourni par OpenSSL) combiné à un salage aléatoire,
 * afin que deux utilisateurs avec le même mot de passe n'aient jamais
 * le même hash stocké en base de données.
 *
 * Pas d'utilisation de bcrypt : aucun paquet MSYS2 officiel n'existe
 * pour cette bibliothèque en C sur Windows, ce qui aurait demandé de
 * compiler manuellement une lib tierce sur chaque machine de l'équipe.
 * SHA-256 + salage répond à la consigne du sujet (mot de passe haché,
 * jamais stocké en clair) sans risque de problème de compilation.
 */

#ifndef CRYPTO_H
#define CRYPTO_H

// Hachage d'un mot de passe en clair et retourne le résultat sous la forme
// "sel:hash", une chaîne de texte que l'on peut stocker directement
// dans la colonne "password" de la table users.
// Le paramètre "password" est le mot de passe en clair saisi par l'utilisateur.
// Le résultat est alloué dynamiquement (malloc),
// il doit donc être libéré avec free() après utilisation.
char *hash_password(const char *password);

// Vérifie qu'un mot de passe en clair correspond bien au hash stocké
// en base de données.
// - password    : le mot de passe en clair saisi lors de la connexion
// - stored_hash : la valeur "sel:hash" récupérée depuis la base
// Retourne 1 si le mot de passe correspond, 0 sinon.
int verify_password(const char *password, const char *stored_hash);

// Chiffre ou déchiffre des données avec l'algorithme XOR.
// Chaque octet est combiné avec un caractère de la clé via l'opérateur
// XOR (^). Si la clé est plus courte que les données, elle est répétée
// en boucle grâce à l'opérateur modulo (%).
// - input        : les données à chiffrer (ou déchiffrer)
// - input_length : la longueur exacte des données, passée explicitement
//                  car le résultat chiffré peut contenir des octets nuls
//                  ('\0') qui ne marquent pas réellement la fin des
//                  données utiles (contrairement à du texte classique)
// - key          : la clé secrète utilisée pour le chiffrement
// - output       : buffer où écrire le résultat, doit pouvoir contenir
//                  au moins input_length octets
// Cette fonction est symétrique : l'appliquer deux fois avec la même
// clé restitue les données originales.
void xor_cipher(const char *input, int input_length, const char *key, char *output);

#endif