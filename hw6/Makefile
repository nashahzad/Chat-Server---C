CFLAGS = -Wall -Werror -g -pthread

all: server client chat

server:
	gcc $(CFLAGS) server.c -o $@ -lncurses -lssl -lcrypto

client:
	gcc $(CFLAGS) client.c -o $@ -lncurses

chat:
	gcc $(CFLAGS) chat.c -o $@ -lncurses

mac:
	gcc $(CFLAGS) server.c -o server -lncurses -lssl -lcrypto -Wno-error=deprecated-declarations

clean:
	rm -f server client chat
