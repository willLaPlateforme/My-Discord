# ═══════════════════════════════════════════════════════════════════════
#  Makefile — SyncUp  (Windows / MSYS2 UCRT64)
#  Depuis le dossier SyncUp/ : make, make server, make client, make clean
# ═══════════════════════════════════════════════════════════════════════

CC     = gcc
LIBS_BASE  = -lws2_32 -lpthread -lpq -lssl -lcrypto

# Flags GTK4 (via pkg-config)
GTK_FLAGS = $(shell pkg-config --cflags --libs gtk4)

CFLAGS_SERVER = -Wall -Wextra -g -I"C:/msys64/ucrt64/include"
CFLAGS_CLIENT = -Wall -Wextra -g $(GTK_FLAGS)

SERVER_SRCS = server/main_server.c  \
              server/commandes.c    \
              server/moderation.c   \
              server/channel_req.c  \
              server/role_manager.c \
              server/db.c           \
              server/crypto.c

CLIENT_SRCS = client/main.c         \
              client/socket_client.c \
              client/ui.c

all: server client

server: $(SERVER_SRCS)
	$(CC) $(CFLAGS_SERVER) -o server/server.exe $(SERVER_SRCS) $(LIBS_BASE)
	@echo "[OK] server/server.exe compile"

client: $(CLIENT_SRCS)
	$(CC) $(CFLAGS_CLIENT) -o client/client.exe $(CLIENT_SRCS) $(LIBS_BASE)
	@echo "[OK] client/client.exe compile"

clean:
	del /f server\server.exe client\client.exe 2>nul || true
	@echo "[OK] Nettoye"

.PHONY: all server client clean