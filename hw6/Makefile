CFLAGS = -Wall -Werror -g -pthread
OBJECT = sfwrite.o

all: $(OBJECT) server client chat

$(OBJECT): sfwrite.c
	gcc $(CFLAGS) -c $^

server: $(OBJECT)
	gcc $(CFLAGS) $(OBJECT) server.c -o $@  -lssl -lcrypto

client: $(OBJECT)
	gcc $(CFLAGS) $(OBJECT) client.c -o $@ 

chat: $(OBJECT)
	gcc $(CFLAGS) $(OBJECT) chat.c -o $@ 

mac: $(OBJECT)
	gcc $(CFLAGS) $(OBJECT) server.c -o server  -lssl -lcrypto -Wno-error=deprecated-declarations

clean:
	rm -f server client chat $(OBJECT)
