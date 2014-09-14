CC=gcc
CFLAGS=-Wall -Werror -g
LDFLAGS=

all: lisod.o server.o io.o log.o http_client.o http_parser.o

lisod.o: lisod.c config.h server.h log.h
	$(CC) $(CFLAGS) -c $^

server.o: server.c server.h io.h log.h http_client.h http_parser.h
	$(CC) $(CFLAGS) -c $^

io.o: io.c io.h log.h
	$(CC) $(CFLAGS) -c $^

log.o: log.c log.h
	$(CC) $(CFLAGS) -c $^

http_parser.o: http_parser.c http_parser.h http_client.h request_handler.h log.h
	$(CC) $(CFLAGS) -c $^

http_client.o: http_client.c http_client.h io.h log.h
	$(CC) $(CFLAGS) -c $^

request_handler.o: request_handler.c request_handler.h http_client.h log.h
	$(CC) $(CFLAGS) -c $^

clean:
	rm -rf *.o *.gch
