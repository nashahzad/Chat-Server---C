CFLAGS = -Wall -Werror -g -pthread

all: server client chat

server:
	gcc $(CFLAGS) server.c -o $@ -lncurses -lssl -lcrypto

client:
	gcc $(CFLAGS) client.c -o $@ -lncurses

chat:
	gcc $(CFLAGS) chat.c -o $@ -lncurses

clean:
	rm -f server client chat
