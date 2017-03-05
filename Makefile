CC = gcc
CFLAGS = -g -O2 -Wall -Wundef
OBJECTS = 
UTILSPATH = utils/

all: client server

client: client.c $(UTILSPATH)utils.o
	$(CC) $(CFLAGS) -o client client.c $(UTILSPATH)utils.o $(UTILSPATH)client_stack.c -lpthread

server: server.c $(UTILSPATH)utils.o kissdb.o
	$(CC) $(CFLAGS) -o server server.c $(UTILSPATH)utils.o kissdb.o $(UTILSPATH)server_stack.c -lpthread

%.o : %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o client server *.db
