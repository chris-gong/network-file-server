CC = gcc
CFLAGS = -g -Wall

all: libnetfiles netfileserver netclient

netclient: netclient.c libnetfiles.o src/misc.c
	$(CC) $(CFLAGS) -o netclient netclient.c libnetfiles.o src/misc.c

libnetfiles: src/libnetfiles.c src/libnetfiles.h src/misc.h src/misc.c
	$(CC) $(CFLAGS) -c src/libnetfiles.c src/misc.c
	
netfileserver: src/netfileserver.c src/linkedlist.h src/linkedlist.c src/misc.h src/misc.c
	$(CC) $(CFLAGS) -o netfileserver src/netfileserver.c src/linkedlist.c src/misc.c -pthread


clean:
	rm -f netclient libnetfiles.o netfileserver
