CC      = gcc
CFLAGS  = -Wall -Wextra -std=c17 -g
LDLIBS  = -lpthread
SRC     = src/main.c src/parser.c
OBJ     = $(SRC:.c=.o)

all: planificador

planificador: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

src/%.o: src/%.c src/dag.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) planificador

.PHONY: all clean