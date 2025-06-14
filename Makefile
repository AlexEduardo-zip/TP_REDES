# Compilador
CC = gcc
# Opções de compilação
CFLAGS = -Wall -Wextra -pedantic -std=c99
# Bibliotecas
LIBS = -lm

# Arquivos objeto
OBJS = server.o sensor.o common.o

# Alvo principal
all: server sensor

# Regra para o servidor
server: server.o common.o
	$(CC) $(CFLAGS) -o $@ server.o common.o $(LIBS)

# Regra para o sensor
sensor: sensor.o common.o
	$(CC) $(CFLAGS) -o $@ sensor.o common.o $(LIBS)

# Regra genérica para arquivos .o
%.o: %.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

# Limpeza
clean:
	rm -f $(OBJS) server sensor

.PHONY: all clean