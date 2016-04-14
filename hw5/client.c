#include "client.h"

int main(int argc, char *argv[]) {
  memset(buffer, 0, MAX_INPUT);
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
  serverInfo.sin_family = AF_INET; //AF_INET IPv4 Protocol
  serverInfo.sin_addr.s_addr = inet_addr(serverIP);
  serverInfo.sin_port = htons(serverPort);

  //then try to connect, begin login sequence by sending WOLFIE\r\n\r\n
  int connection = connect(clientSocket, (struct sockaddr * ) &serverInfo, sizeof(serverInfo));
  if (connection == -1) {
    printf("Connect failed.\n");
    exit(EXIT_FAILURE);
  }
  printf("%li", send(clientSocket, "WOLFIE\r\n\r\n", strlen("WOLFIE\r\n\r\n"), 0));

  //SET UP I/O MULTIPLEXING
  int poll = epoll_create(1);
  struct epoll_event *event = malloc(sizeof(event));
  epoll_ctl(poll, EPOLL_CTL_ADD, 0, event);
  epoll_ctl(poll, EPOLL_CTL_ADD, clientSocket, event);

  //BLOCK UNTIL THERE IS SOMETHING TO ACTUALLY READ FROM STDIN OR SERVER
  while(1){
    if(epoll_wait(poll, event, 1, 500) != -1){

      //SOMETHING WAS WRITTEN IN ON STDIN
      if(event->data.fd == 0){
        read(0, buffer, MAX_INPUT);
        removeNewline(buffer, MAX_INPUT);
        
        if(strcmp("/help", buffer) == 0){
          fprintf(stdout, HELP);
        }

        memset(buffer, 0, MAX_INPUT);
      }

      //Received Input from Server
      else{
        recv(clientSocket, buffer, MAX_INPUT, 0);

        //PART OF LOGIN PROCEDURE SEND BACK TO SERVER IAM <NAME>\r\n\r\n
        if(strcmp(buffer, "EIFLOW\r\n\r\n") == 0){
          char *message = malloc(9 + strlen(name));
          memset(message, 0, 9 + strlen(name));
          strcat(message, "IAM ");
          strcat(message, name);
          strcat(message, " \r\n\r\n");
          send(clientSocket, message, strlen(message), 0);
          free(message);
        }

        strtok(buffer, "\r\n\r\n");

        char *login = malloc(3 + strlen(name));
        memset(login, 0, 3 + strlen(name));
        strcat(login, "HI ");
        strcat(login, name);

        //SECOND PART TO LOGIN PROCEDURE SERVER SAYS HI
        if(strcmp(buffer, name) == 0){
          fprintf(stdout, "%s\n", buffer);
        }

        //NOT PART OF LOGIN PROCESS
        else{
          fprintf(stdout, "%s\n", buffer);
        }

        free(login);
        memset(buffer, 0, MAX_INPUT);
      }
      
    }
  }
  
}

void removeNewline(char *string, int length){
  for(int i = 0; i < length; i++){
    if(string[i] == '\n'){
      string[i] = '\0';
      return;
    }
  }
}
