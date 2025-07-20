MODULES = main.c other.c

SRC = src
BLD = build

SRC_FILES = $(addprefix $(SRC)/, $(MODULES))
OBJ_FILES = $(addprefix $(BLD)/, $(MODULES:.c=.o))

PROGRAM = main

CC = gcc
CFLAGS = -Wall -Wextra -O2

.PHONY: all clean

# Compile objects
all: $(PROGRAM)

$(PROGRAM): $(OBJ_FILES) | $(BLD)
	$(CC) $(CFLAGS) -o $@ $(OBJ_FILES)

$(BLD)/%.o: $(SRC)/%.c | $(BLD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BLD):
	mkdir -p $(BLD)

clean:
	rm -rf $(BLD) $(PROGRAM)
