CC=gcc
CFLAGS=-Wall -Werror -g
LDFLAGS=
SRCS=server.h server.c config.h lisod.c MakeFile

all: lisod

lisod: lisod.o server.o

lisod.o: lisod.c config.h server.h
	$(CC) $(CFLAGS) -c lisod.c config.h server.h

server.o: server.c server.h
	$(CC) $(CFLAGS) -c server.c server.h

clean: 
	rm -rf *.o *.gch lisod