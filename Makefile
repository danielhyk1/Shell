CC=gcc
CFLAGS=-std=c11 -Wall -pedantic -I.
NAME=Bash

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(NAME): process.o main.o parse.o
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: all
all: $(NAME)

.PHONY: clean
clean:
	rm -f process.o main.o $(NAME)
