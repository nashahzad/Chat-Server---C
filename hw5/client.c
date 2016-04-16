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

  send(clientSocket, "WOLFIE\r\n\r\n", strlen("WOLFIE\r\n\r\n"), 0);

  //SET UP I/O MULTIPLEXING
  //int maxfd = clientSocket + 1;
  fd_set set, readSet;

  FD_ZERO(&set);
  FD_SET(clientSocket, &set);
  FD_SET(0, &set);

  int wait = 0;
  //BLOCK UNTIL THERE IS SOMETHING TO ACTUALLY READ FROM STDIN OR SERVER
  while(1){
    write(1, ">", 1);
    readSet = set;
    wait = select(FD_SETSIZE, &readSet, NULL, NULL, NULL);
    if(wait == -1){}

  else{

        if(FD_ISSET(clientSocket, &readSet)) {
          recv(clientSocket, buffer, MAX_INPUT, 0);

        //IF SOMEONE CTRL-C THE SERVER JUST SHUTDOWN
        if(buffer[0] == '\0'){
          fprintf(stderr, "Select was triggered, but nothing in socket came through.\n");
          close(clientSocket);
          exit(EXIT_FAILURE);
        }

        if(strlen(buffer) == 1){
          if(buffer[0] == '\n'){
            memset(buffer, 0, 1);
          }
        }

        if(verboseFlag){
          fprintf(stderr, "received from server: %s\n", buffer);
        }
        //PART OF LOGIN PROCEDURE SEND BACK TO SERVER IAM <NAME>\r\n\r\n
        if(strcmp(buffer, "EIFLOW\r\n\r\n") == 0){
          if(verboseFlag){
            fprintf(stdout, "%s\n", buffer);
          }
          char *message = malloc(9 + strlen(name));
          memset(message, 0, 9 + strlen(name));
          strcat(message, "IAM ");
          strcat(message, name);
          strcat(message, " \r\n\r\n");
          send(clientSocket, message, strlen(message), 0);

          if(verboseFlag){
            fprintf(stderr, "sending to server: %s\n", message);
          }
          free(message);
        }

        else{
          if(!checkProtocol()){
            fprintf(stderr, "\nBAD PACKET: %s\n", buffer);
          }

          else{
            char *login = malloc(4 + strlen(name));
            memset(login, 0, 3 + strlen(name));
            strcat(login, "HI ");
            strcat(login, name);
            strcat(login, " ");

            //SECOND PART TO LOGIN PROCEDURE SERVER SAYS HI
            if(strcmp(buffer, login) == 0){
              fprintf(stdout, "%s\n", buffer);
            }

            //NOT PART OF LOGIN PROCESS
            else{

              //CHECK TO SEE IF ITS A RESPONSE TO CLIENT COMMAND FROM SERVER
              if(clientCommandCheck()){
                continue;
              }

              //CHECK TO SEE IF SERVER SAID THAT USER NAME WAS ALREADY TAKEN
              char *error = malloc(strlen("ERR 00 USER NAME TAKEN "));
              memset(error, 0, strlen("ERR 00 USER NAME TAKEN "));
              strcat(error, "ERR 00 USER NAME TAKEN ");
              if(strcmp(buffer, error) == 0){
                fprintf(stderr, "%s\n", "Username already taken, due to this error will now be closing the client.");
                close(clientSocket);
                free(error);
                exit(EXIT_FAILURE);
              }
              free(error);

              //CHECK TO SEE IF THE SERVER SHUTDOWN AND SENT BYE
              error = malloc(strlen("BYE "));
              memset(error, 0, strlen("BYE "));
              strcat(error, "BYE ");
              if(strcmp(buffer, error) == 0){
                fprintf(stdout, "%s\n", "The server has now SHUTDOWN, thus closing client now. Goodbye!");
                close(clientSocket);
                free(error);
                exit(EXIT_SUCCESS);
              }
              free(error);

              fprintf(stdout, "%s\n", buffer);
            }

            free(login);
          }  
        } 
        //JUST TO MAKE SURE TO THAT BUFFER GETS SET BACK TO ALL NULL TERMINATORS
        memset(buffer, 0, MAX_INPUT);    
      }
      //is there something on stdin for the server?
      else if (FD_ISSET(0, &readSet)) {
        fgets(buffer, MAX_INPUT - 1, stdin);
        fflush(stdin);
        fflush(stdout);
        removeNewline(buffer, MAX_INPUT);

        if(verboseFlag){
          fprintf(stderr, "received from stdin: %s\n", buffer);
        }

        if(strcmp("/help", buffer) == 0){
          fprintf(stdout, HELP);
        }

        if(strcmp("/logout", buffer) == 0){
          char *message = "BYE\r\n\r\n\0";
          send(clientSocket, message, strlen(message), 0);
          close(clientSocket);
          exit(EXIT_SUCCESS);
        }

        if(strcmp("/time", buffer) == 0){
          char *message = "TIME\r\n\r\n\0";
          send(clientSocket, message, strlen(message), 0);
        }

        if(strcmp("/listu", buffer) == 0){
          char *message = "LISTU\r\n\r\n\0";
          send(clientSocket, message, strlen(message), 0);
        }

        memset(buffer, 0, MAX_INPUT);
      }     
      
    }
  }
}

bool checkProtocol(){
  int size;
  if((size = strlen(buffer)) < 4){
    fprintf(stderr, "%s\n", "Bad packet sent from server!");
    return false;
  }

  bool flag = false;
  for(int i = 0; i < size; i++){
    if(buffer[i] == '\r' && (size - i) >= 4){
      char *check = malloc(4);
      memset(check, 0, 4);
      strncpy(check, buffer + i, 4);

      if(strcmp(check, "\r\n\r\n") == 0){
        //IF IT GETS HERE THEN PACKET FOLLOWED PROTOCOL
        //JUST MEMSET PROTOCOL 
        memset(buffer + i, 0, 4);
        flag = true;
      }
      free(check);
    }
  }

  if(flag){
    return true;
  }

  //REACHES HERE, THEN WENT THROUGH LOOP WITHOUT EVER FINDING PROTOCOL
  fprintf(stderr, "%s\n", "Bad packet from server, didn't follow protocol!");
  return false;
}

bool clientCommandCheck(){
  if(strlen(buffer) >= 5){
    
    //CHECK FOR TIME VERB, EMIT FROM SERVER
    char *verb = "EMIT";
    char *temp = malloc(4);
    strncpy(temp, buffer, 4);
    if(strcmp(temp, verb) == 0){
      free(temp);
      strcpy(buffer, buffer + 5);

      int seconds = atoi(buffer);

      int minutes = seconds / 60;
      seconds = seconds % 60;

      if(minutes == 0){
        TIME(0, 0, seconds);
        return true;;
      }

      int hours = minutes / 60;
      minutes = minutes % 60;

      TIME(hours, minutes, seconds);
      return true;
    }
    free(temp);


    //CHECK FOR LIST USERS VERB, UTSIL
    verb = "UTSIL";
    temp = malloc(5);
    strncpy(temp, buffer, 5);
    if(strcmp(temp, verb) == 0){
      free(temp);
      strcpy(buffer, buffer + 6);

      char *token = malloc(sizeof(char) * strlen(buffer));
      fprintf(stdout, "%s\n", "USERS:");
      strcpy(token, buffer);
      token = strtok(token, "\r\n");
      for(; token != NULL ;token = strtok(NULL, "\r\n")){        
        if(token == NULL){
          break;
        }

        fprintf(stdout, "%s\n", token);;
      }
      return true;
    }
    return false;
  }
  return false;
}

void removeNewline(char *string, int length){
  for(int i = 0; i < length; i++){
    if(string[i] == '\n'){
      string[i] = '\0';
    }
  }
}
