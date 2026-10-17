all: client server

CC=gcc
CFLAGS=-I. -lm -g

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

client.o: common.h
game.o: game.h common.h
server.o: game.h save.h common.h
save.o: save.h common.h

client: client.o
	$(CC) -o $@ $^ $(CFLAGS)

server: server.o game.o save.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f *.o server client
