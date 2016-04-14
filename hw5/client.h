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
#include <errno.h>
#include <ctype.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/epoll.h>



#define MAX_INPUT 1024

#define USAGE "./client [-hcv] NAME SERVER_IP SERVER_PORT\n-h              Displays this help menu, and returns EXIT_SUCCESS.\n-c              Requests to server to create a new user.\n-v              Verbose print all traffic.\nNAME            Username to display\nSERVER_IP       IP to connect to\nSERVER_PORT     Port to connect to.\n"

#define HELP "HELP MENU\n" \
"/time - Will ask the server for how long you have been connected.\n"\
"/help - Will pop open help menu of of commands that you are reading right now :D\n"\
"/logout - Will disconnect with the server.\n"\
"/listu - It will ask the server to find out who else has been connected to server.\n"\

bool verboseFlag = false;

char buffer[MAX_INPUT];

bool checkProtocol();



void removeNewline(char *string, int length);


#endif