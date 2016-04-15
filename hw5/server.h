#ifndef server
#define server

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <termios.h>
#include <errno.h>
#include <ctype.h>
#include <netinet/in.h>
#include <pthread.h>

#define _GNU_SOURCE
#define MAX_INPUT 1024
#define USAGE "./server [-h|-v] PORT_NUMBER MOTD\n-h            Displays help menu & returns EXIT_SUCCESS.\n-v            Verbose print all incoming and outgoing protocol verbs and content.\nPORT_NUMBER   Port number to listen on.\nMOTD          Message to display to the client when they connect.\n"

void * handleClient(void * param);
int checkEOM(char * start);
int checkAvailability(char * toCheck);

#endif
