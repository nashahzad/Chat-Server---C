CFLAGS = -Wall -Werror -g

all: server client chat

server:
	gcc $(CFLAGS) server.c -o $@

client:
	gcc $(CFLAGS) client.c -o $@

chat:
	gcc $(CFLAGS) chat.c -o $@

clean:
	rm -f server
	rm -f client
	rm -f chat