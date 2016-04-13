#include "client.h"

int main(int argc, char *argv[]) {
  int createFlag = 0;
  char name[MAX_INPUT] = {0};
  int serverPort;
  char serverIP[MAX_INPUT] = {0};
  //first check the # of arg's to see if any flags were given
  int opt;
  if ((argc == 5) || (argc == 6) || (argc == 7)) {
    while((opt = getopt(argc, argv, "hv")) != -1) {
      switch(opt) {
        case 'h':
          printf(USAGE);
          exit(EXIT_SUCCESS);
          break;
        case 'v':
          verboseFlag = true;
          printf("%d", verboseFlag);
          break;
        case 'c':
          createFlag = 1;
          printf("%d", createFlag);
          break;
        case '?':
        default:
          printf(USAGE);
          exit(EXIT_FAILURE);
          break;
      }
    }
    strcpy(name, argv[optind]);
    strcpy(serverIP, argv[optind + 1]);
    serverPort = atoi(argv[optind + 2]);
  }
  else if (argc != 4) {
    printf(USAGE);
    exit(EXIT_FAILURE);
  }
  else {
    strcpy(name, argv[1]);
    strcpy(serverIP, argv[2]);
    serverPort = atoi(argv[3]);
  }
  //make socket
  int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (clientSocket == -1) {
    printf("Failed to make server socket.\n");
    exit(EXIT_FAILURE);
  }
  //enter server's info
  struct sockaddr_in serverInfo;
  memset(&serverInfo, 0, sizeof(serverInfo));
  serverInfo.sin_family = AF_INET;
  serverInfo.sin_addr.s_addr = inet_addr(serverIP);
  serverInfo.sin_port = htons(serverPort);
  //then try to connect
  int connection = connect(clientSocket, (struct sockaddr * ) &serverInfo, sizeof(serverInfo));
  if (connection == -1) {
    printf("Connect failed.\n");
    exit(EXIT_FAILURE);
  }
  printf("%li", send(clientSocket, name, strlen(name), 0));
  char reply[MAX_INPUT] = {0};
  recv(clientSocket, reply, MAX_INPUT, 0);
  write(1, reply, strlen(reply));
}
