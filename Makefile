# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -O3 -I./include
LDFLAGS = -L./lib -Wl,-rpath=./lib -lrouting -lpthread

# Directories
SRC_DIR = src
INC_DIR = include
LIB_DIR = lib
BIN_DIR = bin

# Targets
all: prep $(BIN_DIR)/dispatcher $(LIB_DIR)/librouting.so

prep:
	mkdir -p $(BIN_DIR) $(LIB_DIR)

# Build the dynamic library
$(LIB_DIR)/librouting.so: $(SRC_DIR)/routing.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $^

# Build the main dispatcher process
$(BIN_DIR)/dispatcher: $(SRC_DIR)/main.c $(LIB_DIR)/librouting.so
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/main.c $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR)/* $(LIB_DIR)/*
	
.PHONY: all clean prep