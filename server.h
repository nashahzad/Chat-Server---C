#ifndef server
#define server

#define _GNU_SOURCE

#include "sfwrite.h"
//#include <stdio.h>
#include <string.h>
#include <stdbool.h>
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
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <semaphore.h>

//#include <pthread.h>
#include <time.h>
#define MAX_INPUT 1024
#define SALT_LENGTH 16
#define USAGE "./server [-h|-v] [-t THREAD_COUNT] PORT_NUMBER MOTD [ACCOUNTS_FILE]\n-h            Displays help menu & returns EXIT_SUCCESS.\n-t THREAD_COUNT The number of threads used for the login queue.\n-v            Verbose print all incoming and outgoing protocol verbs and content.\nPORT_NUMBER   Port number to listen on.\nMOTD          Message to display to the client when they connect.\nACCOUNTS_FILE File containing user data to be loaded upon execution.\n"

void * handleClient(void * param);
void * communicationThread(void * param);
int checkEOM(char * start);
int checkAvailability(char * toCheck);
int parseMSG(char * input, char ** to, char ** from);
int verifyUser(char * user, unsigned char * pass);
int verifyPass(char * pass);
int readRecord(FILE * file, char ** username, unsigned char ** salt, unsigned char ** hash);
void handleSigInt(int sig);

pthread_t cid;
int commPipe[2];
int serverSocket;
int args;
char ** args2;
int threadCount = 2;
bool commThreadCount = false;
pthread_mutex_t *stdoutLock;
pthread_mutex_t queueLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t accountsLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t usersLock = PTHREAD_MUTEX_INITIALIZER;
sem_t items_sem;
int quit_signal = 0;
pthread_t * loginArray;

#endif
