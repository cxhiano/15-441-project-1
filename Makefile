CC=gcc
CFLAGS=-Wall -Werror -g
LDFLAGS=

all: lisod

lisod: src/io.o src/server.o src/lisod.o
	$(CC) $^ -o lisod

clean:
	rm -rf lisod
	cd src; make clean

tar:
	(make clean; cd ..; tar cvf 15-441-project-1.tar 15-441-project-1)