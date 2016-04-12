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

#define _GNU_SOURCE
#define MAX_INPUT 1024
#define USAGE "./server [-h|-v] PORT_NUMBER MOTD\n-h            Displays help menu & returns EXIT_SUCCESS.\n-v            Verbose print all incoming and outgoing protocol verbs and content.\nPORT_NUMBER   Port number to listen on.\nMOTD          Message to display to the client when they connect.\n"

int main(int argc, char *argv[]) {
  int verboseFlag = 0;
  //first check the # of arg's to see if any flags were given
  int opt;
  if ((argc == 4) || (argc == 5)) {
    while((opt = getopt(argc, argv, "hv")) != -1) {
      switch(opt) {
        case 'h':
          printf(USAGE);
          exit(EXIT_SUCCESS);
          break;
        case 'v':
          verboseFlag = 1;
          printf("%d", verboseFlag);
          break;
        case '?':
        default:
          printf(USAGE);
          exit(EXIT_FAILURE);
          break;
      }
    }
  }
  else if (argc != 3) {
    printf(USAGE);
    exit(EXIT_FAILURE);
  }
  //now it is definitely known that argc is 3 here. the verbose flag (if found, is set)
  
  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket == -1) {
    printf("Failed to make server socket.");
    exit(EXIT_FAILURE);
  }

}
