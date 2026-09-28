COMPILADOR = gcc
BANDERAS = -Wall -Wextra -std=c17 -g
LIBS = -lpthread


FUENTES = $(wildcard src/*.c)
OBJETOS = $(FUENTES:.c=.o)
EJECUTABLE = planificador

all: $(EJECUTABLE)

$(EJECUTABLE): $(OBJETOS)
	$(COMPILADOR) $(BANDERAS) -o $@ $^ $(LIBS)

src/%.o: src/%.c src/dag.h
	$(COMPILADOR) $(BANDERAS) -c $< -o $@

clean:
	rm -f $(OBJETOS) $(EJECUTABLE)
