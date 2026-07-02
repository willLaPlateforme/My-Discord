/*
*Ce fichier crypto.c implémente les fonctions déclarées dans crypto.h.
*Il permet de hacher un mot de passe avant son stockage en base de données,
*Et de vérifier un mot de passe saisi lors de la connexion
*En le comparant au hash stocké.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include "../include/crypto.h"

/*
*Taille du sel en octets. Un sel plus long renforce la sécurité
*En rendant les attaques par dictionnaire précalculé (rainbow tables)
*quasiment impossibles.
*/
#define SALT_LENGTH 16

/*
*Le pepper est une valeur secrète, identique pour tous les utilisateurs,
*qui n'est jamais stockée en base de données (contrairement au sel).
*Même si la base de données est volée, les mots de passe restent
*protégés car le pepper n'existe que dans le code source du serveur.
*A modifier avant le rendu final pour une valeur propre à l'équipe.
*/
#define PEPPER "SyncUp_Secret_Pepper_2026"

/*
*Convertit des octets bruts en une chaîne hexadécimale lisible.
*Par exemple {0xA3, 0xF1} devient la chaîne "a3f1".
*"output" doit être assez grand pour contenir (length * 2) + 1 caractères.
*/
static void bytes_to_hex(const unsigned char *bytes, int length, char *output) {
    for (int i = 0; i < length; i++) {
        sprintf(output + (i * 2), "%02x", bytes[i]);
    }
    output[length * 2] = '\0';
}

/*
*Hache un mot de passe en clair avec un sel aléatoire et un pepper secret.
*Retourne une chaîne sous la forme "sel:hash", où le sel et le hash
*Sont tous les deux encodés en hexadécimal. Le pepper n'apparaît jamais
*dans le résultat, ni en base de données.
*/
char *hash_password(const char *password) {
    unsigned char salt[SALT_LENGTH];

    /*
    * RAND_bytes génère des octets vraiment aléatoires,
    * Fournis par OpenSSL, pour créer un sel unique à chaque hachage.
    */
   if (RAND_bytes(salt, SALT_LENGTH) != 1) {
       fprintf(stderr, "Erreur lors de la génération du sel.\n");
       return NULL;
   }

   char salt_hex[SALT_LENGTH * 2 + 1];
   bytes_to_hex(salt, SALT_LENGTH, salt_hex);

   /*
   *On concatène le sel, le pepper et le mot de passe avant de les
   *hacher ensemble, pour que le résultat dépende des trois éléments.
   */
   char salted_password[300];
   snprintf(salted_password, sizeof(salted_password), "%s%s%s", salt_hex, PEPPER, password);

   /*SHA256 calcule le hash du mot de passe salé et poivré.*/
   unsigned char hash[SHA256_DIGEST_LENGTH];
   SHA256((unsigned char *)salted_password, strlen(salted_password), hash);

   char hash_hex[SHA256_DIGEST_LENGTH * 2 + 1];
   bytes_to_hex(hash, SHA256_DIGEST_LENGTH, hash_hex);

   /*
   * On alloue dynamiquement le résultat final "sel:hash",
   * Car sa taille dépend de la longueur du sel et du hash.
   * Le pepper n'est jamais inclus dans ce résultat.
   */
   char *result = malloc(strlen(salt_hex) + strlen(hash_hex) + 2);
   if (result == NULL) {
       fprintf(stderr, "Erreur d'allocation mémoire.\n");
       return NULL;
   }

   sprintf(result, "%s:%s", salt_hex, hash_hex);
   return result;
}

/*
*Vérifie qu'un mot de passe en clair correspond au hash stocké.
*Récupère le sel depuis "stored_hash", hache le mot de passe fourni
*avec ce même sel et le pepper, puis compare le résultat avec le hash stocké.
*/
int verify_password(const char *password, const char *stored_hash) {
    /*On copie stored_hash car strtok modifie la chaîne qu'on lui passe. */
    char stored_copy[256];
    strncpy(stored_copy, stored_hash, sizeof(stored_copy) - 1);
    stored_copy[sizeof(stored_copy) - 1] = '\0';

    /*strtok découpe la chaîne "sel:hash" au niveau du ":". */
    char *salt_hex = strtok(stored_copy, ":");
    char *original_hash_hex = strtok(NULL, ":");

    if (salt_hex == NULL || original_hash_hex == NULL) {
        fprintf(stderr, "Format de hash invalide.\n");
        return 0;
    }

    /*
    *On refait le même calcul que dans hash_password, mais en
    *réutilisant le sel déjà stocké plutôt qu'en générant un nouveau.
    *Le pepper utilisé doit être identique à celui de hash_password.
    */
   char salted_password[300];
   snprintf(salted_password, sizeof(salted_password), "%s%s%s", salt_hex, PEPPER, password);

   unsigned char hash[SHA256_DIGEST_LENGTH];
   SHA256((unsigned char *)salted_password, strlen(salted_password), hash);

   char hash_hex[SHA256_DIGEST_LENGTH * 2 + 1];
   bytes_to_hex(hash, SHA256_DIGEST_LENGTH, hash_hex);

   /*
   *Si les deux hash hexadécimaux sont identiques, le mot de passe
   *saisi est correct.
   */
  return strcmp(hash_hex, original_hash_hex) == 0;
}

/*
*Chiffre ou déchiffre des données avec l'algorithme XOR.
*Chaque octet est combiné avec un caractère de la clé via l'opérateur
*XOR (^). Si la clé est plus courte que les données, elle est répétée
*en boucle grâce à l'opérateur modulo (%).
*Contrairement à une version basée sur strlen(), cette fonction prend
*la longueur explicitement en paramètre, car le résultat chiffré peut
*contenir des octets nuls ('\0') qui ne marquent pas réellement la fin
*des données utiles.
*Cette fonction est symétrique : l'appliquer deux fois avec la même
*clé restitue les données originales.
*/
void xor_cipher(const char *input, int input_length, const char *key, char *output) {
    int key_length = strlen(key);

    for (int i = 0; i < input_length; i++) {
        output[i] = input[i] ^ key[i % key_length];
    }
}