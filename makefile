PROGS = main test 
APP = mem_alloc
FLAGS = -Wall -Werror -std=gnu99
CC = gcc

all: $(PROGS)

main: $(APP).c main.c
	$(CC) $(FLAGS) $^ -o $@

test: $(APP).c test_main.c
	$(CC) $(FLAGS) $^ -o $@

clean: 
	rm -f $(PROGS)
