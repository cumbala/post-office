CC=gcc
FLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic -pthread
PROJECT=proj2

default: all

all:
	$(CC) $(FLAGS) -o $(PROJECT) $(PROJECT).c

debug:
	$(CC) $(FLAGS) -DDEBUG -o $(PROJECT) $(PROJECT).c

run: all
	./$(PROJECT) 3 2 100 100 100

out: run
	cat $(PROJECT).out

zip:
	rm -f xroman18.zip
	zip xroman18.zip proj2.c Makefile