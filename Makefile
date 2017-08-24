CC = gcc
CFLAGS = -Wall -g -std=gnu99
LDFLAGS = 

OBJ_DIR = ./obj

SERVER_BIN = server
CLIENT_BIN = client

OBJ = $(OBJ_DIR)/server.o
CLIENT_OBJ = $(OBJ_DIR)/client.o


all: $(CLIENT_BIN) $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_OBJ)
	$(CC) $(LDFLAGS) $^ -o $@

$(SERVER_BIN):	$(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

.PHONY:	clean
clean:
	rm -rf $(SERVER_BIN) $(CLIENT_BIN) $(OBJ)