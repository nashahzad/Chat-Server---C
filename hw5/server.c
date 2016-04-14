#include "server.h"

static char * users[50] = {0};
static int userCounter = 0;
static char motd[MAX_INPUT] = {0};

//handle login
void * handleClient(void * param) {
  int * parameter = (int *) param;
  int client = *parameter;
  //set up holding area for data
  char input[MAX_INPUT] = {0};
  int recvData;
  recvData = recv(client, input, MAX_INPUT, 0);
  //check if client started login protocol correctly
  if (recvData > 0) {
    if (strcmp(input, "WOLFIE\r\n\r\n") == 0) {
      send(client, "EIFLOW\r\n\r\n", strlen("EIFLOW\r\n\r\n"), 0);
    }
  }
  write(1, input, strlen(input));
  memset(input, 0, MAX_INPUT - 1);
  recvData = recv(client, input, MAX_INPUT, 0);
  write(1, "abc", 3);
  printf("%lu", strlen(input));
  fflush(stdout);
  if (recvData > 0) {
    char check1[5] = {0};
    char check2[5] = {0};
    char name[100] = {0};
    write(1, input, strlen(input));
    //check if the message is IAM <name> \r\n\r\n
    int checkWolfieProtocol = sscanf(input, "%s %s %s", check1, name, check2);
    if ((strcmp(check1, "IAM") == 0) && strcmp(check2, "\r\n\r\n") && (checkWolfieProtocol == 3)) {
      char hiResponse[200] = {0};
      sprintf(hiResponse, "%s", "HI ");
      strcat(hiResponse, name);
      strcat(hiResponse, " \r\n\r\n");
      send(client, hiResponse, strlen(hiResponse), 0);
      //add user to list
      users[userCounter++] = input;
      //and send MOTD
      send(client, motd, strlen(motd), 0);
    }
  }
  return NULL;
}

//handle communication (after a login)
void * handleClient2(void * param) {
  return NULL;
}

int main(int argc, char *argv[]) {
  int verboseFlag = 0;
  //first check the # of arg's to see if any flags were given
  int opt;
  int portNumber;
  //char message[MAX_INPUT] = {0};
  //char * users[50] = {0};
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
    portNumber = atoi(argv[optind]);
    strcpy(motd, argv[optind + 1]);
  }
  else if (argc != 3) {
    printf(USAGE);
    exit(EXIT_FAILURE);
  }
  //now it is definitely known that argc is 3 here. the verbose flag (if found, is set)
  else {
    portNumber = atoi(argv[1]);
    strcpy(motd, argv[2]);
  }

  //create socket
  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket == -1) {
    printf("Failed to make server socket.\n");
    exit(EXIT_FAILURE);
  }

  //then bind it to a port
  struct sockaddr_in serverInfo, clientInfo;
  memset(&serverInfo, 0, sizeof(serverInfo));
  memset(&clientInfo, 0, sizeof(clientInfo));
  serverInfo.sin_family = AF_INET;
  serverInfo.sin_port = htons(portNumber);
  serverInfo.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(serverSocket, (struct sockaddr *) &serverInfo, sizeof(serverInfo)) == -1) {
    printf("Failed to bind to port %d\n", portNumber);
    exit(EXIT_FAILURE);
  }

  //create the fd_set to monitor multiple file descriptors (sockets), including stdin to look for server commands
  fd_set activeFdSet, readFdSet;
  FD_ZERO(&activeFdSet);
  FD_SET(serverSocket, &activeFdSet);
  FD_SET(0, &activeFdSet);

  //now listen for any connection requests
  listen(serverSocket, 20); //arbitrary queue length
  printf("Currently listening on port %d\n", portNumber);
  int counter;
  //run forever until receive /shutdown
  while(1) {
    write(1, ">", 1);
    readFdSet = activeFdSet;
    if (select(FD_SETSIZE, &readFdSet, NULL, NULL, NULL) == -1) {
      printf("Select failed.\n");
      exit(EXIT_FAILURE);
    }
    for(counter = 0; counter < FD_SETSIZE; ++counter) {
      if (FD_ISSET(counter, &readFdSet)) {
        if (counter == serverSocket) {
          //accept an incoming connection
          unsigned int clientLength;
          int clientSocket = accept(serverSocket, (struct sockaddr *) &clientInfo, &clientLength);
          if (clientSocket == -1) {
            printf("Accept error.\n");
            fflush(stdout);
          }
          else {
            //login thread
            pthread_t tid;
            pthread_create(&tid, NULL, (void *) &handleClient, (void *) &clientSocket);
          }
        }
        //is there something on stdin for the server?
        else if (counter == 0) {
          char test[MAX_INPUT] = {0};
          fgets(test, MAX_INPUT - 1, stdin);
          if (strncmp("/help\n", test, 6) == 0) {
            printf("--------------------------------------\nCOMMAND LIST\n/users          Prints list of current users\n/help           Prints this prompt\n/shutdown       Shuts down server\n--------------------------------------\n");
            fflush(stdout);
          }
          else if (strncmp("/users\n", test, 7) == 0) {
            printf("/users test\n");
            fflush(stdout);
            int count = 0;
            for(; count < userCounter; count++) {
              printf("%s\n", users[count]);
            }
          }
          else if (strncmp("/shutdown\n", test, 10) == 0) {
            printf("/shutdown test\n");
            fflush(stdout);
          }
          else {
            test[strlen(test) - 1] = '\0';
            printf("Unknown command %s. Type /help for more information.\n", test);
            fflush(stdout);
          }
        }
        else {

        }
      }
    }
  }
}
