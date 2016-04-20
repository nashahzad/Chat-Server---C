#ifndef client
#define client

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <termios.h>
#include  <errno.h>
#include <ctype.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <termios.h>

struct chat{
	int fd;
	int fdChat;
	int PID;
	char *name;
	struct chat *next;
	struct chat *prev;
};
typedef struct chat chat;
chat *head = NULL;

#define MAX_INPUT 1024

#define USAGE "./client [-hcv] NAME SERVER_IP SERVER_PORT\n-h              Displays this help menu, and returns EXIT_SUCCESS.\n-c              Requests to server to create a new user.\n-v              Verbose print all traffic.\nNAME            Username to display\nSERVER_IP       IP to connect to\nSERVER_PORT     Port to connect to.\n"

#define HELP "\nHELP MENU\n" \
"/time - Will ask the server for how long you have been connected.\n"\
"/help - Will pop open help menu of of commands that you are reading right now :D\n"\
"/logout - Will disconnect with the server.\n"\
"/listu - It will ask the server to find out who else has been connected to server.\n\n"\

#define TIME(hours, minutes, seconds) fprintf(stdout, "connected for %d hour(s), %d minute(s), and %d second(s)\n", hours, minutes, seconds);

#define XTERM(offset, name, fd) char *arg[9]; \
	arg[8] = NULL; \
	for(int i = 0; i < 8; i++){ \
		arg[i] = malloc(MAX_INPUT); \
		memset(arg[i], 0, MAX_INPUT); \
	}	\
	strcat(arg[0], "xterm"); \
	sprintf(arg[1], "-geometry"); \
	sprintf(arg[2], "45x35+%d", offset); \
	sprintf(arg[3], "-T"); \
	sprintf(arg[4], "%s", name); \
	sprintf(arg[5], "-e"); \
	sprintf(arg[6], "./chat"); \
	sprintf(arg[7], "%d", fd); \
	int PID = fork(); \
	if(PID == 0){ \
		execvp(arg[0], arg); \
		exit(EXIT_FAILURE); \
	} \
	for(int i = 0; i < 8; i++){ \
		free(arg[i]);\
	} \

bool verboseFlag = false;

int createFlag = false;

char name[MAX_INPUT] = {0};

int clientSocket;

int offset = 10;

char *buffer = NULL;

bool checkProtocol();

bool clientCommandCheck();

void addChat(char *name, int fdRead, int fdWrite, int PID);

void handleChatMessageSTDIN();

void removeNewline(char *string, int length);

void removeChat(chat *iterator);

void loginProcedure(fd_set set, fd_set readSet);

void readBuffer(int fd, bool socket);


#endif