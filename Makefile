CC = gcc
CFLAGS = -g -O2 -Wall -Wundef
OBJECTS = 
UTILSPATH = src/utils/
SRCPATH = src/

all: client server

client: $(SRCPATH)client.c $(UTILSPATH)utils.o
	$(CC) $(CFLAGS) -o client $(SRCPATH)client.c $(UTILSPATH)utils.o $(UTILSPATH)client_stack.c -lpthread

server: $(SRCPATH)server.c $(UTILSPATH)utils.o $(SRCPATH)kissdb.o
	$(CC) $(CFLAGS) -o server $(SRCPATH)server.c $(UTILSPATH)utils.o $(SRCPATH)kissdb.o $(UTILSPATH)server_stack.c -lpthread

%.o : %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o client server *.db
