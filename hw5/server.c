#include "server.h"

void * handleClient(void * param) {
  int * parameter = (int *) param;
  int client = *parameter;
  //set up holding area for data
  char input[MAX_INPUT] = {0};
  int recvData, inputCounter = 0;
  while ((recvData = recv(client, &input[inputCounter], MAX_INPUT, 0)) > 0) {
    inputCounter += recvData;
    write(client, input, strlen(input));
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  int verboseFlag = 0;
  //first check the # of arg's to see if any flags were given
  int opt;
  int portNumber;
  char message[MAX_INPUT] = {0};
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
    strcpy(message, argv[optind + 1]);
  }
  else if (argc != 3) {
    printf(USAGE);
    exit(EXIT_FAILURE);
  }
  //now it is definitely known that argc is 3 here. the verbose flag (if found, is set)
  else {
    portNumber = atoi(argv[1]);
    strcpy(message, argv[2]);
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
  //now listen for any connection requests
  listen(serverSocket, 20); //arbitrary queue length
  printf("Currently listening on port %d\n", portNumber);
  while(1) {
    //accept an incoming connection
    unsigned int clientLength;
    write(1, "test\n", 5);
    int clientSocket = accept(serverSocket, (struct sockaddr *) &clientInfo, &clientLength);
    if (clientSocket == -1) {
      printf("Accept error.\n");
      continue;
    }
    //create thread
    pthread_t tid;
    pthread_create(&tid, NULL, (void *) &handleClient, (void *) &clientSocket);

    //write(clientSocket, message, strlen(message));

  }
}
