# Ce Makefile permet de compiler les deux parties du projet :
# le serveur et le client.

# Cible exécutée par défaut lorsque l'on tape simplement "make".
# Elle lance la compilation du serveur puis celle du client.
all:
	make -f "Makefile server"
	make -f "Makefile client"

# Initialise la base de données.
# Exécute le script SQL init.sql avec l'utilisateur PostgreSQL "postgres".
db-init:
	psql -U postgres -f Database/init.sql

# Supprime les tables ou les données de la base.
# Exécute le script SQL drop.sql.
db-drop:
	psql -U postgres -f Database/drop.sql

# Réinitialise complètement la base de données.
# Commence par supprimer son contenu puis la recrée.
db-reset: db-drop db-init

# Supprime les fichiers générés lors de la compilation
# pour le serveur et le client (fichiers objets, exécutables, etc.).
clean:
	make -f "Makefile server" clean
	make -f "Makefile client" clean