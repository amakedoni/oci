
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra

all: parent child

parent: parent.c
	$(CC) $(CFLAGS) parent.c -o parent

child: child.c
	$(CC) $(CFLAGS) child.c -o child

clean:
	rm -f parent child

.PHONY: all clean
