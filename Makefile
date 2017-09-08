CC = gcc
CFLAGS = -Wall -g -std=gnu99
LDFLAGS = 

OBJ_DIR = ./obj

SERVER_BIN = tftp_server

OBJ += $(OBJ_DIR)/server.o
OBJ += $(OBJ_DIR)/tftp.o
OBJ += $(OBJ_DIR)/file_op.o

all: $(SERVER_BIN)

$(SERVER_BIN):	$(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: %.c
	@ mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $^ -o $@

.PHONY:	clean
clean:
	rm -rf $(SERVER_BIN) $(OBJ_DIR)