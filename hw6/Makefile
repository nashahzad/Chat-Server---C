CFLAGS = -Wall -Werror -g -pthread
OBJECT = sfwrite.o

all: sfwrite.o server client chat

$(OBJECT): sfwrite.c
	gcc $(CFLAGS) -c $^

server: $(OBJECT)
	gcc $(CFLAGS) $(OBJECT) server.c -o $@ -lncurses -lssl -lcrypto

client: $(OBJECT)
	gcc $(CFLAGS) $(OBJECT) client.c -o $@ -lncurses

chat: $(OBJECT)
	gcc $(CFLAGS) $(OBJECT) chat.c -o $@ -lncurses

mac: $(OBJECT)
	gcc $(CFLAGS) $(OBJECT) server.c -o server -lncurses -lssl -lcrypto -Wno-error=deprecated-declarations

clean:
	rm -f server client chat $(OBJECT)
