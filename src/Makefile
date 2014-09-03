CC=gcc
CFLAGS=-Wall -Werror -g
LDFLAGS=

all: lisod.o server.o io.o

lisod.o: lisod.c config.h server.h
	$(CC) $(CFLAGS) -c lisod.c config.h server.h

server.o: server.c server.h io.h
	$(CC) $(CFLAGS) -c server.c server.h io.h

io.o: io.c io.h
	$(CC) $(CFLAGS) -c io.h io.c

clean:
	rm -rf *.o *.gch
