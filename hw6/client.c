#include "client.h"

int main(int argc, char *argv[]) {
  //INITIALIZE MUTEX FOR AUDIT LOG
  auditLock = malloc(sizeof(pthread_mutex_t));
  memset(auditLock, 0, sizeof(pthread_mutex_t));
  if(pthread_mutex_init(auditLock, NULL) != 0){
    sfwrite(auditLock, stderr, "Error failed to intialize pthread_mutex_t *lock!\n");
    free(auditLock);
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, SIGINTHandler);
  //first check the # of arg's to see if any flags were given
  int opt;
  while((opt = getopt(argc, argv, "hcva")) != -1) {
      switch(opt) {
        case 'h':
          sfwrite(auditLock, stderr, USAGE);
          exit(EXIT_SUCCESS);
          break;
        case 'v':
          verboseFlag = true;
          break;
        case 'c':
          createFlag = true;
          break;
        case 'a':
          auditBool = true;
          break;
        case '?':
        default:
          sfwrite(auditLock, stderr, USAGE);
          exit(EXIT_FAILURE);
          break;
      }
    }
   
    //AUDIT FILE WAS ADDED AND RIGHT NUMBER OF ARGUMENTS
    if(auditBool && (argc - optind) == 4){
      auditFile = argv[optind];
      strcpy(name, argv[optind+1]);
      strcpy(serverIP, argv[optind + 2]);
      serverPort = atoi(argv[optind + 3]);
    }

    //CLAIMED TO HAVE AUDIT FILE ADDED BUT NOT ENOUGH OR TOO MANY ARGUMENTS
    else if(auditBool){
      sfwrite(auditLock, stderr, USAGE);
        exit(EXIT_FAILURE);
    }

    //MEANING THAT NO AUDIT FILE WAS SPECIFIED
    else if(!auditBool){
      //CORRECT NUMBER OF ARGUMENTS PROVIDED
      if((argc - optind) == 3){
        auditFile = "audit.log\0";
        strcpy(name, argv[optind]);
        strcpy(serverIP, argv[optind + 1]);
        serverPort = atoi(argv[optind + 2]);
      }

      //ELSE NOT ENOUGH OR TOO MANY ARGUMENTS
      else{
        printf(USAGE);
        exit(EXIT_FAILURE);
      }
    }

  //SETUP UP FILE *audit
  auditFileOpen();

  //make socket
  clientSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (clientSocket == -1) {
    printf("Failed to make server socket.\n");
    fclose(audit);
    exit(EXIT_FAILURE);
  }

  //enter server's info
  struct sockaddr_in serverInfo;
  memset(&serverInfo, 0, sizeof(serverInfo));
  serverInfo.sin_family = AF_INET; //AF_INET IPv4 Protocol
  serverInfo.sin_addr.s_addr = inet_addr(serverIP);
  serverInfo.sin_port = htons(serverPort);

  //then try to connect, begin login sequence by sending WOLFIE \r\n\r\n
  int connection = connect(clientSocket, (struct sockaddr * ) &serverInfo, sizeof(serverInfo));
  if (connection == -1) {
    printf("Connect failed.\n");
    fclose(audit);
    exit(EXIT_FAILURE);
  }

  send(clientSocket, "WOLFIE \r\n\r\n", 11, 0);
  if(verboseFlag){
      sfwrite(auditLock, stderr, "%sSENT TO SERVER: %s%s\n", BLUE, "WOLFIE", NORMAL);
    }

  //SET UP I/O MULTIPLEXING
  //int maxfd = clientSocket + 1;
  fd_set set, readSet;

  FD_ZERO(&set);
  FD_SET(clientSocket, &set);
  FD_SET(0, &set);

  loginProcedure(set, readSet);

  int wait = 0;
  //BLOCK UNTIL THERE IS SOMETHING TO ACTUALLY READ FROM STDIN OR SERVER
  while(1){
    write(1, ">", 1);
    readSet = set;

    //ADD CHAT FD'S TO THE FD_SET TO MULTIPLEX ON
    for(chat *iterator = head; iterator != NULL; iterator = iterator->next){
      FD_SET(iterator->fd, &readSet);
    }

    wait = select(FD_SETSIZE, &readSet, NULL, NULL, NULL);
    if(wait == -1){}

  else{

      if(FD_ISSET(clientSocket, &readSet)) {
          readBuffer(clientSocket, true);

        //IF SOMEONE CTRL-C THE SERVER JUST SHUTDOWN
        if(buffer[0] == '\0'){
          sfwrite(auditLock, stderr, "Select was triggered, but nothing in socket came through.\n");
          char *message = "BYE \r\n\r\n\0";
          send(clientSocket, message, strlen(message), 0);
          if(verboseFlag){
            sfwrite(auditLock, stderr, "%sSENT TO SERVER: %s%s\n", BLUE, "BYE", NORMAL);
          }
          close(clientSocket);
          for(chat *iterator = head; iterator != NULL;){
            close(iterator->fd);
            close(iterator->fdChat);
            kill(iterator->PID, 9);
            waitpid(iterator->PID, NULL, 0);
            removeChat(iterator);
            iterator = head;
            if(iterator == NULL){
              break;
            }
          }
          exit(EXIT_SUCCESS);
        }

        if(strlen(buffer) == 1){
          if(buffer[0] == '\n'){
            memset(buffer, 0, 1);
          }
        }

        if(!checkProtocol()){
          sfwrite(auditLock, stderr, "\nBAD PACKET: %s\n", buffer);
        }

        //CHECK TO SEE IF ITS A RESPONSE TO CLIENT COMMAND FROM SERVER
        if(clientCommandCheck()){
          continue;
        }

        //CHECK TO SEE IF THE SERVER SHUTDOWN AND SENT BYE
        char *error = malloc(strlen("BYE "));
        memset(error, 0, strlen("BYE "));
        strcat(error, "BYE ");
        if(strcmp(buffer, error) == 0){
          sfwrite(auditLock, stdout, "%s\n", "The server has now SHUTDOWN, thus closing client now. Goodbye!");
          send(clientSocket, "BYE \r\n\r\n", 8, 0);
          if(verboseFlag){
            sfwrite(auditLock, stderr, "%sSENT TO SERVER: BYE%s\n",BLUE, NORMAL);
          }
          free(error);
          close(clientSocket);
          for(chat *iterator = head; iterator != NULL;){
            close(iterator->fd);
            close(iterator->fdChat);
            kill(iterator->PID, 9);
            waitpid(iterator->PID, NULL, 0);
            removeChat(iterator);
            iterator = head;
            if(iterator == NULL){
              break;
            }
          }
          exit(EXIT_SUCCESS);
        }
        free(error);
        sfwrite(auditLock, stdout, "%s\n", buffer);
      }

      //ELSE IF IS THERE SOMETHING ON STDIN
      else if (FD_ISSET(0, &readSet)) {
        readBuffer(0, false);
        removeNewline(buffer, strlen(buffer));

        if(verboseFlag){
          sfwrite(auditLock, stderr, "%sRECEIVED FROM STDIN: %s%s\n", GREEN, buffer, NORMAL);
        }

        if(strcmp("/help", buffer) == 0){
          sfwrite(auditLock, stdout, HELP);
          char *t = timestamp();
          sfwrite(auditLock, audit, "%s, %s, CMD, %s, success, client\n", t, name, buffer);
          free(t);
          continue;
        }

        if(strcmp("/logout", buffer) == 0){
          char *message = "BYE \r\n\r\n\0";
          send(clientSocket, message, strlen(message), 0);
          if(verboseFlag){
            sfwrite(auditLock, stderr, "%sSENT TO SERVER: %s%s\n", BLUE, "BYE", NORMAL);
          }
          close(clientSocket);
          for(chat *iterator = head; iterator != NULL;){
            close(iterator->fd);
            close(iterator->fdChat);
            kill(iterator->PID, 9);
            waitpid(iterator->PID, NULL, 0);
            removeChat(iterator);
            iterator = head;
            if(iterator == NULL){
              break;
            }
          }
          char *t = timestamp();
          sfwrite(auditLock, audit, "%s, %s, LOGOUT, intentional\n", t, name);
          free(t);
	        free(auditLock);
          fclose(audit);
          exit(EXIT_SUCCESS);
        }

        if(strcmp("/time", buffer) == 0){
          char *message = "TIME \r\n\r\n\0";
          send(clientSocket, message, strlen(message), 0);
          if(verboseFlag){
            sfwrite(auditLock, stderr, "%sSENT TO SERVER: %s%s\n", BLUE, "TIME", NORMAL);
          }
          char *t = timestamp();
          sfwrite(auditLock, audit, "%s, %s, CMD, %s, success, client\n", t, name, buffer);
          free(t);
          continue;
        }

        if(strcmp("/listu", buffer) == 0){
          char *message = "LISTU \r\n\r\n\0";
          send(clientSocket, message, strlen(message), 0);
          if(verboseFlag){
            sfwrite(auditLock, stderr, "%sSENT TO SERVER: %s%s\n", BLUE, "LISTU", NORMAL);
          }
          char *t = timestamp();
          sfwrite(auditLock, audit, "%s, %s, CMD, %s, success, client\n", t, name, buffer);
          free(t);
          continue;
        }

        if(strcmp("/audit", buffer) == 0){
          char byte[1] = {'\0'};
      	 
          flock(fileno(audit), LOCK_EX);          
         
          fclose(audit);
          int fd = open(auditFile, O_RDONLY);
          for(int bytes_read = 0; (bytes_read = read(fd, byte, 1)) != 0;){
            sfwrite(auditLock, stdout, byte);
          }
          close(fd);
          audit = fopen(auditFile, "a+");
	       
          flock(fileno(audit), LOCK_UN);

          char *t = timestamp();
          sfwrite(auditLock, audit, "%s, %s, CMD, %s, success, client\n", t, name, buffer);
          free(t);
          continue;
        }

        if(strcmp("loop", buffer) == 0){
          while(1){
            sfwrite(auditLock, audit, "Woah there!!!\n");
          }
        }

        char *comp = malloc(6);
        memset(comp, 0, 6);
        strncpy(comp, buffer, 5);
        if(strcmp("/chat", comp) == 0) {
          handleChatMessageSTDIN();
          free(comp);
          continue;
        }

        //MISTYPED OR ERRONEOUS COMMANDS HERE
        else{
          free(comp);
          char *t = timestamp();
          sfwrite(auditLock, audit, "%s, %s, CMD, %s, failure, client\n", t, name, buffer);
          free(t);
        }
        
      }

      //MAYBE THERE'S SOMETHING FROM CHAT FD'S
      else{
        for(chat *iterator = head; iterator != NULL; iterator = iterator->next){
          if(FD_ISSET(iterator->fd, &readSet)){
            readBuffer(iterator->fd, false);
            removeNewline(buffer, strlen(buffer));
            
            //PROBABLY JUST MESSAGE TO PERSON
            char *message = malloc(MAX_INPUT);
      	    memset(message, 0, MAX_INPUT);
      	    sprintf(message, "MSG %s %s %s \r\n\r\n", iterator->name, name, buffer);
            send(clientSocket, message, strlen(message), 0);
            if(verboseFlag){
              char *verbose = malloc(strlen(message));
              memset(verbose, 0, strlen(message));
              strncpy(verbose, message, strlen(message) - 5);
              sfwrite(auditLock, stderr, "%sSENT TO SERVER: %s%s\n", BLUE, verbose, NORMAL);
              free(verbose);
            }
            free(message);          
          }
        }
      }

    }
  }
}

bool checkProtocol(){
  int size;
  if((size = strlen(buffer)) < 4){
    sfwrite(auditLock, stderr, "%s\n", "Bad packet sent from server!");
    return false;
  }

  bool flag = false;
  for(int i = 0; i < size; i++){
    if(buffer[i] == '\r' && (size - i) >= 4){
      char *check = malloc(5);
      memset(check, 0, 5);
      check[4] = '\0';
      strncpy(check, buffer + i, 4);

      if(strcmp(check, "\r\n\r\n") == 0){
        //IF IT GETS HERE THEN PACKET FOLLOWED PROTOCOL
        //JUST MEMSET PROTOCOL
        memset(buffer + i, 0, 4);
        flag = true;
	free(check);
	break;
      }
      free(check);
    }
  }

  if(verboseFlag){
      sfwrite(auditLock, stderr, "%sRECEIVED FROM SERVER: %s%s\n", GREEN, buffer, NORMAL);
  }

  if(flag){
    return true;
  }

  //REACHES HERE, THEN WENT THROUGH LOOP WITHOUT EVER FINDING PROTOCOL
  sfwrite(auditLock, stderr, "%s\n", "Bad packet from server, didn't follow protocol!");
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
      sfwrite(auditLock, stdout, "%s\n", "USERS:");
      strcpy(token, buffer);
      token = strtok(token, "\r\n");
      for(; token != NULL ;token = strtok(NULL, "\r\n")){
        if(token == NULL){
          break;
        }

        sfwrite(auditLock, stdout, "%s\n", token);;
      }
      return true;
    }

    //CHECK TO SEE IF SERVER HAS SAID THAT A USER HAS LOGGED OFF
    verb = "UOFF\0";
    temp = malloc(5);
    memset(temp, 0, 5);
    strncpy(temp, buffer, 4);
    temp[4] = '\0';
    if(strcmp(verb, temp) == 0){
      free(temp);

      char *token = malloc(strlen(buffer));
      memset(token, 0, strlen(buffer));
      strcpy(token, buffer);

      token = strtok(token, " ");
      token = strtok(NULL, " ");
      if(token == NULL){
        sfwrite(auditLock, stderr, "%s\n", "There was no name passed with UOFF verb!");
      }

      for(chat *iterator = head; iterator != NULL; iterator = iterator->next){
        if(strcmp(iterator->name, token) == 0){
          send(iterator->fd, "UOFF", 4, 0);
          if(verboseFlag){
            sfwrite(auditLock, stderr, "%sSENT TO CHAT: %s%s\n", BLUE, "UOFF", NORMAL);
          }
          return true;
        }
      }

      return true;
    }


    verb = "MSG\0";
    temp = malloc(4);
    memset(temp, 0, 4);
    strncpy(temp, buffer, 3);
    temp[3] = '\0';
    if(strcmp(verb, temp) == 0){
      char *token = malloc(MAX_INPUT);
      
      memset(token, 0, MAX_INPUT);
      strcpy(token, buffer);
      
      token = strtok(token, " ");
      token = strtok(NULL, " ");

      char *to = malloc(strlen(token));
      strcpy(to, token);

      token = strtok(NULL, " ");
      char *from = malloc(strlen(token));
      strcpy(from, token);

      if(strcmp(from, to) == 0){
        free(from);
        free(to);
        free(token);
        sfwrite(auditLock, stderr, "%s\n", "Messaging yourself?!");
        sfwrite(auditLock, stdout, "\n>");
      }

      token = strtok(NULL, " ");

      char *message = malloc(MAX_INPUT);
      memset(message, 0, MAX_INPUT);
      for(; token != NULL; token = strtok(NULL, " ")){
        strcat(message, token);
        strcat(message, " \0");
      }

      //THIS CLIENT INITIATED CHAT
      if(strcmp(from, name) == 0){
        char *t = timestamp();
        sfwrite(auditLock, audit, "%s, %s, MS, from, %s, %s\n", t, name, name, message);
        free(t);
        //CHECK TO SEE IF A WINDOW IS ALREADY OPEN BY CHECKING LIST
        bool flag = false;
        for(chat *iterator = head; iterator != NULL; iterator = iterator->next){
          if(strcmp(iterator->name, to) == 0){
            if(waitpid(iterator->PID, NULL, WNOHANG) != 0){
              close(iterator->fd);
              close(iterator->fdChat);
              removeChat(iterator);
              break;
            }

            else{
              flag = true;
              char *temp = malloc(MAX_INPUT);
              memset(temp, 0, MAX_INPUT);
              sprintf(temp, "<%s", message);
              send(iterator->fd, temp, strlen(temp), 0);
              if(verboseFlag){
                sfwrite(auditLock, stderr, "%sSENT TO CHAT: %s%s\n", BLUE, message, NORMAL);
              }
              free(temp);
              break;
            }
            
          }
        }
        //IF NOT IN LIST THEN FORK EXEC, NEED DOMAIN SOCKET
        if(!flag){
          int socketPair[2];
          socketpair(AF_UNIX, SOCK_STREAM, 0, socketPair);
          XTERM(rand() * 1000, to, socketPair[1], fileno(audit), name);
          addChat(to, socketPair[0], socketPair[1], PID);
          char *temp = malloc(MAX_INPUT);
          memset(temp, 0, MAX_INPUT);
          sprintf(temp, "<%s", message);
          send(socketPair[0], temp, strlen(temp), 0);
          if(verboseFlag){
            sfwrite(auditLock, stderr, "%sSENT TO CHAT: %s%s\n", BLUE, message, NORMAL);
          }
          free(temp);
        }
      }

      if(strcmp(to, name) == 0){
        char *t = timestamp();
        sfwrite(auditLock, audit, "%s, %s, MS, from, %s, %s\n", t, name, name, message);
        free(t);
        //CHECK TO SEE IF A WINDOW IS ALREADY OPEN BY CHECKING LIST
        bool flag = false;
        for(chat *iterator = head; iterator != NULL; iterator = iterator->next){
          if(strcmp(iterator->name, from) == 0){
            if(waitpid(iterator->PID, NULL, WNOHANG) != 0){
              close(iterator->fd);
              close(iterator->fdChat);
              removeChat(iterator);
              break;
            }

            else{
              flag = true;
              char *temp = malloc(MAX_INPUT);
              memset(temp, 0, MAX_INPUT);
              sprintf(temp, ">%s", message);
              send(iterator->fd, temp, strlen(temp), 0);
              if(verboseFlag){
                sfwrite(auditLock, stderr, "%sSENT TO CHAT: %s%s\n", BLUE, message, NORMAL);
              }
              free(temp);
              break;
            }
            
          }
        }
        //IF NOT IN LIST THEN FORK EXEC, NEED DOMAIN SOCKET
        if(!flag){
          int socketPair[2];
          socketpair(AF_UNIX, SOCK_STREAM, 0, socketPair);
          XTERM(rand() * 1000, from, socketPair[1], fileno(audit), name);
          addChat(from, socketPair[0], socketPair[1], PID);
          char *temp = malloc(MAX_INPUT);
          memset(temp, 0, MAX_INPUT);
          sprintf(temp, ">%s", message);
          send(socketPair[0], temp, strlen(temp), 0);
          if(verboseFlag){
            sfwrite(auditLock, stderr, "%sSENT TO CHAT: %s%s\n", BLUE, message, NORMAL);
          }
          free(temp);
        }
      }

      free(from);
      free(to);
      free(token);
      free(message);
      return true;
    }
    return false;
  }

  return false;
}

void addChat(char *name, int fd, int fdChat, int PID){
  if(head == NULL){
    head = malloc(sizeof(chat));
    head->name = malloc(strlen(name) + 1);
    strncpy(head->name, name, strlen(name));
    head->name[strlen(name)] = '\0';
    head->fd = fd;
    head->fdChat = fdChat;
    head->PID = PID;

    head->next = NULL;
    head->prev = NULL;
  }

  else{
    chat *tempHead = head;

    head = malloc(sizeof(chat));
    head->name = malloc(strlen(name) + 1);
    strncpy(head->name, name, strlen(name));
    head->name[strlen(name)] = '\0';
    head->fd = fd;
    head->fdChat = fdChat;
    head->PID = PID;

    head->next = tempHead;
    head->prev = NULL;
    tempHead->prev = head;
  }
}

void removeChat(chat *iterator){
  if(iterator->next == NULL && iterator->prev == NULL){
    head = NULL;
    free(iterator->name);
    free(iterator);
  }
  else if(iterator->next != NULL && iterator->prev == NULL){
    iterator->next->prev = NULL;
    head = iterator->next;
    free(iterator->name);
    free(iterator);
  }
  else if(iterator->next == NULL && iterator->prev != NULL){
    iterator->prev->next = NULL;
    free(iterator->name);
    free(iterator);
  }
  else{
    iterator->prev->next = iterator->next;
    iterator->next->prev = iterator->prev;
    free(iterator->name);
    free(iterator);
  }
}

void handleChatMessageSTDIN(){
  //Didn't write in any anything for <to> and <message>
  if(strlen(buffer) == 5 || strlen(buffer) == 6){
    return;
  }

  char *token = malloc(strlen(buffer));
  memset(token, 0, strlen(buffer));
  strcpy(token, buffer);

  token = strtok(token, " ");
  token = strtok(NULL, " ");

  //no <to>
  if(token == NULL){
    free(token);
    return;
  }
  char *to = malloc(strlen(token));
  strcpy(to, token);

  if(strcmp(to, name) == 0){
    free(to);
    sfwrite(auditLock, stderr, "%s\n", "Attempting to talk to yourself?!");
    char *t = timestamp();
    sfwrite(auditLock, audit, "%s, %s, CMD, %s, failure, client\n", t, name, buffer);
    free(t);
    return;
  }


  token = strtok(NULL, " ");
  //no <message>
  if(token == NULL){
    free(to);
    char *t = timestamp();
    sfwrite(auditLock, audit, "%s, %s, CMD, %s, failure, client\n", t, name, buffer);
    free(t);
    return;
  }

  char *message = malloc(MAX_INPUT);
  memset(message, 0, MAX_INPUT);
  for(; token != NULL; token = strtok(NULL, " ")){
    strcat(message, token);
    strcat(message, " \0");
  }

  char *serverSend = malloc(MAX_INPUT);
  memset(serverSend, 0, MAX_INPUT);
  sprintf(serverSend, "MSG %s %s %s \r\n\r\n", to, name, message);

  send(clientSocket, serverSend, strlen(serverSend), 0);
  if(verboseFlag){
    char *verbose = malloc(strlen(serverSend));
    memset(verbose, 0, strlen(serverSend));
    strncpy(verbose, serverSend, strlen(serverSend) - 5);
    sfwrite(auditLock, stderr, "%sSENT TO SERVER: %s%s\n", BLUE, verbose, NORMAL);
    free(verbose);
  }

  free(message);
  free(to);
  free(serverSend);

  char *t = timestamp();
  sfwrite(auditLock, audit, "%s, %s, CMD, %s, success, client\n", t, name, buffer);
  free(t);

  return;
}

void removeNewline(char *string, int length){
  for(int i = 0; i < length; i++){
    if(string[i] == '\n'){
      string[i] = '\0';
    }
  }
}

void readBuffer(int fd, bool socket){
  //IF READING FROM SOCKET
  if(buffer == NULL){
    buffer = malloc(1);
    *buffer = '\0';
  }

  else if(buffer[0] == '\0'){
    char *s = malloc(1);
    buffer = s;
    *buffer = '\0';
  }

  else{
    buffer = realloc(buffer, 0);
    buffer = malloc(1);
    *buffer = '\0';
  }

  if(socket){
    int size = 1;
    char last_char[1] = {'\0'};
    char temp_protocol[5];
    memset(temp_protocol, 0, 5);
    bool protocol = false;
    for(; recv(fd, last_char, 1, 0) == 1;){
      if(*last_char == '\r'){
        strncpy(temp_protocol, last_char, 1);
        if(recv(fd, last_char, 1, 0) == 1){
          if(*last_char == '\n'){
            strncpy(temp_protocol + 1, last_char, 1);
            if(recv(fd, last_char, 1, 0) == 1){
              if(*last_char == '\r'){
                strncpy(temp_protocol + 2, last_char, 1);
                if(recv(fd, last_char, 1, 0) == 1){
                  if(*last_char == '\n'){
                    strncpy(temp_protocol + 3, last_char, 1);
                    protocol = true;
                    break;
                  }
                }
              }
            }
          }
        }
      }

      if(strlen(temp_protocol) > 0){
        buffer = realloc(buffer, size + strlen(temp_protocol));
        strncpy(buffer + size - 1, temp_protocol, strlen(temp_protocol));
        size = size + strlen(temp_protocol);
        memset(temp_protocol, 0, 5);
      }

      else{
        strncpy(buffer + size - 1, last_char, 1);
        buffer = realloc(buffer, size + 1);
        size++;
      }  
    }
    //IF PROTOCOL WAS NOT FOUND
    if(!protocol){
      sfwrite(auditLock, stderr, "Protocol was not found! Packet: %s\n", buffer);
    }
    //ELSE ATTACH PROTOCOL ONTO END OF BUFFER
    else{
      buffer = realloc(buffer, size + strlen(temp_protocol));
      strncpy(buffer + size - 1, temp_protocol, strlen(temp_protocol));
      size = size + strlen(temp_protocol);
    }
    buffer[size-1] = '\0';
  }

  //COULD BE SOMETHING LIKE STDIN OR CHAT FD WITH NO PROTOCOL
  else{
    int size = 1;
    char last_char[1] = {'\0'};
    for(;read(fd, last_char, 1) == 1;){
      strncpy(buffer + size - 1, last_char, 1);
      buffer = realloc(buffer, size + 1);
      size++;
      if(*last_char == '\n'){
        break;
      }
    }
    buffer[size-1] = '\0';
    if(verboseFlag){
      if(fd == 0){
        sfwrite(auditLock, stderr, "%sRECEIVED FROM STDIN: %s%s\n", GREEN, buffer, NORMAL);
      }else{
        sfwrite(auditLock, stderr, "%sRECEIVED FROM A CHAT: %s%s\n", GREEN, buffer, NORMAL);
      }
    }
  }
}

void loginProcedure(fd_set set, fd_set readSet){
  readSet = set;

  int wait = select(FD_SETSIZE, &readSet, NULL, NULL, NULL);
  if(wait == -1){
    sfwrite(auditLock, stderr, "%s\n", "Error on select, exiting!");
    fclose(audit);
    exit(EXIT_FAILURE);
  }

  readBuffer(clientSocket, true);
  //NO PROTOCOL SENT ON MESSAGE ABORT LOGIN
  if(!checkProtocol()){
    sfwrite(auditLock, stderr, "NO PROTOCOL ATTACHED, ABROTING LOGIN AND CLOSING CLIENT!\n");
    close(clientSocket);
    fclose(audit);
    exit(EXIT_FAILURE);
  }

  //IF BUFFER IS LESS THAN 6 CAN'T BE EIFLOW
  if(strlen(buffer) < 6){
    sfwrite(auditLock, stderr, "Buffer less than 6, can't possibly be EIFLOW\n");
    close(clientSocket);
    fclose(audit);
    exit(EXIT_FAILURE);
  }

  char *verb = malloc(6);
  strncpy(verb, buffer, 6);
  if(strcmp(verb, "EIFLOW") != 0){
    sfwrite(auditLock, stderr, "Wrong verb, verb: %s\n", verb);
    free(verb);
    close(clientSocket);
    fclose(audit);
    exit(EXIT_FAILURE);
  }
  free(verb);

  //PROCEDURE FOR MAKING NEW USER
  if(createFlag){
    char *message = malloc(2 + 6 + strlen(name) + 4 + 1);
    memset(message, 0, 2 + 6 + strlen(name) + 4 + 1);

    //SENDING OVER IAMNEW <NAME> \R\N\R\N
    sprintf(message, "IAMNEW %s \r\n\r\n", name);
    send(clientSocket, message, strlen(message), 0);
    if(verboseFlag){
      char *verbose = malloc(strlen(message));
      memset(verbose, 0, strlen(message));
      strncpy(verbose, message, strlen(message) - 5);
      sfwrite(auditLock, stderr, "%sSENT TO SERVER: %s%s\n", BLUE, verbose, NORMAL);
      free(verbose);
    }
    free(message);

    //WAIT FOR HINEW <NAME> \R\N\R\N
    readSet = set;
    wait = select(FD_SETSIZE, &readSet, NULL, NULL, NULL);
    if(wait == -1){
      sfwrite(auditLock, stderr, "%s\n", "Error on select, exiting!");
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    readBuffer(clientSocket, true);
    //NO PROTOCOL SENT ON MESSAGE ABORT LOGIN
    if(!checkProtocol()){
      sfwrite(auditLock, stderr, "NO PROTOCOL ATTACHED, ABORTING LOGIN AND CLOSING CLIENT!\n");
      close(clientSocket);
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    verb = malloc(5 + 1 + strlen(name) + 1);
    memset(verb, 0, 5 + 1 + strlen(name) + 1);
    sprintf(verb, "HINEW %s", name);
    char *temp = malloc(5 + 1 + strlen(name) + 1);
    memset(temp, 0, 5 + 1 + strlen(name) + 1);
    strncpy(temp, buffer, 5 + 1 + strlen(name));
    if(strcmp(verb, temp) != 0){
      sfwrite(auditLock, stderr, "Did not get correct message from server, shutting down client. Packet: %s\n", buffer);
      close(clientSocket);
      free(verb);
      free(temp);
      fclose(audit);
      exit(EXIT_FAILURE);
    }
    free(verb);
    free(temp);

    //NOW TO GET PASSWORD FROM USER AND MAKE SURE ITS VALID NEW PASSWORD
    sfwrite(auditLock, stdout, "PASSWORD: ");
    struct termios oflags, nflags;
    tcgetattr(fileno(stdin), &oflags);
    nflags = oflags;
    nflags.c_lflag &= ~ECHO;
    nflags.c_lflag |= ECHONL;

    tcsetattr(fileno(stdin), TCSANOW, &nflags);

    char *password = malloc(MAX_INPUT);
    memset(password, 0, MAX_INPUT);
    fgets(password, MAX_INPUT, stdin);

    tcsetattr(fileno(stdin), TCSANOW, &oflags);

    removeNewline(password, strlen(password));

    //NOW TO CHECK VALIDITY
    bool upper = false, symbol = false, number = false;
    for(int i = 0; i < strlen(password); i++){
      if(password[i] >= 'A' && password[i] <= 'Z'){
        upper = true;
      }
      else if(password[i] >= '0' && password[i] <= '9'){
        number = true;
      }
      else if(!(password[i] >= 'A' && password[i] <= 'Z') && !(password[i] >= 'a' && password[i] <= 'z')
              && !(password[i] >= '0' && password[i] <= '9')){
        symbol = true;
      }
    }//END OF FOR LOOP

    //IF NOT ALL TRUE, THEN SAY INVALID PASSWORD AND CLOSE
    if(!(upper && symbol && number)){
      sfwrite(auditLock, stderr, "Typed in an invalid password! Closing down client.\n");
      close(clientSocket);
      free(password);
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    message = malloc(7 + 1 + strlen(password) + 1 + 4 + 1);
    memset(message, 0, 7 + 1 + strlen(password) + 1 + 4 + 1);
    sprintf(message, "NEWPASS %s \r\n\r\n", password);
    send(clientSocket, message, strlen(message), 0);
    if(verboseFlag){
      char *verbose = malloc(strlen(message));
      memset(verbose, 0, strlen(message));
      strncpy(verbose, message, strlen(message) - 5);
      sfwrite(auditLock, stderr, "%sSENT TO SERVER: %s%s\n", BLUE, verbose, NORMAL);
      free(verbose);
    }
    free(message);
    free(password);

    //NEED TO CHECK FOR SSAPWEN \R\N\R\N
    readSet = set;
    wait = select(FD_SETSIZE, &readSet, NULL, NULL, NULL);
    if(wait == -1){
      sfwrite(auditLock, stderr, "%s\n", "Error on select, exiting!");
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    readBuffer(clientSocket, true);
    //NO PROTOCOL SENT ON MESSAGE ABORT LOGIN
    if(!checkProtocol()){
      sfwrite(auditLock, stderr, "NO PROTOCOL ATTACHED, ABORTING LOGIN AND CLOSING CLIENT!\n");
      close(clientSocket);
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    if(strlen(buffer) < 7){
      sfwrite(auditLock, stderr, "Packet sent is too small to contain SSAPWEN, closing client.\n");
      close(clientSocket);
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    verb = malloc(7 + 1);
    memset(verb, 0 , 8);
    strncpy(verb, buffer, 7);
    if(strcmp(verb, "SSAPWEN") != 0){
      sfwrite(auditLock, stderr, "Received wrong verb from server, PACKET: %s\n", buffer);
      free(verb);
      close(clientSocket);
      fclose(audit);
      exit(EXIT_FAILURE);
    }  
    free(verb);  

    readSet = set;
    wait = select(FD_SETSIZE, &readSet, NULL, NULL, NULL);
    if(wait == -1){
      sfwrite(auditLock, stderr, "%s\n", "Error on select, exiting!");
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    readBuffer(clientSocket, true);
    //NO PROTOCOL SENT ON MESSAGE ABORT LOGIN
    if(!checkProtocol()){
      sfwrite(auditLock, stderr, "NO PROTOCOL ATTACHED, ABORTING LOGIN AND CLOSING CLIENT!\n");
      close(clientSocket);
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    sfwrite(auditLock, stdout, "%s\n", buffer);

    readSet = set;
    wait = select(FD_SETSIZE, &readSet, NULL, NULL, NULL);
    if(wait == -1){
      sfwrite(auditLock, stderr, "%s\n", "Error on select, exiting!");
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    readBuffer(clientSocket, true);
    //NO PROTOCOL SENT ON MESSAGE ABORT LOGIN
    if(!checkProtocol()){
      sfwrite(auditLock, stderr, "NO PROTOCOL ATTACHED, ABORTING LOGIN AND CLOSING CLIENT!\n");
      close(clientSocket);
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    //SHOULD BE PRINTING MESSAGE OF DAY NOW, DONE WITH LOGIN PROCEDURE OF NEW USER
    sfwrite(auditLock, stdout, "%s\n", buffer);
    char *t = timestamp();
    sfwrite(auditLock, audit, "%s, %s, LOGIN, %s:%d, success, %s\n", t, name, serverIP, serverPort, buffer);
    free(t);

    return;
  }

  //ELSE LOGGING IN AN EXISTING USER
  else{
    char *message = malloc(2 + 3 + strlen(name) + 4 + 1);
    memset(message, 0, 2 + 3 + strlen(name) + 4 + 1);

    //SENDING OVER IAMNEW <NAME> \R\N\R\N
    sprintf(message, "IAM %s \r\n\r\n", name);
    send(clientSocket, message, strlen(message), 0);
    if(verboseFlag){
      char *verbose = malloc(strlen(message));
      memset(verbose, 0, strlen(message));
      strncpy(verbose, message, strlen(message) - 5);
      sfwrite(auditLock, stderr, "%sSENT TO SERVER: %s%s\n", BLUE, verbose, NORMAL);
      free(verbose);
    }
    free(message);

    //WAIT FOR HINEW <NAME> \R\N\R\N
    readSet = set;
    wait = select(FD_SETSIZE, &readSet, NULL, NULL, NULL);
    if(wait == -1){
      sfwrite(auditLock, stderr, "%s\n", "Error on select, exiting!");
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    readBuffer(clientSocket, true);
    //NO PROTOCOL SENT ON MESSAGE ABORT LOGIN
    if(!checkProtocol()){
      sfwrite(auditLock, stderr, "NO PROTOCOL ATTACHED, ABORTING LOGIN AND CLOSING CLIENT!\n");
      close(clientSocket);
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    verb = malloc(4 + 1 + strlen(name) + 1);
    memset(verb, 0, 4 + 1 + strlen(name) + 1);
    sprintf(verb, "AUTH %s", name);
    char *temp = malloc(4 + 1 + strlen(name) + 1);
    memset(temp, 0, 4 + 1 + strlen(name) + 1);
    strncpy(temp, buffer, 4 + 1 + strlen(name));
    if(strcmp(verb, temp) != 0){
      sfwrite(auditLock, stderr, "Did not get correct message from server, shutting down client. Packet: %s\n", buffer);
      close(clientSocket);
      free(verb);
      free(temp);
      fclose(audit);
      exit(EXIT_FAILURE);
    }
    free(verb);

    //NOW TO GET PASSWORD FROM USER AND JUST SEND IT TO SERVER
    sfwrite(auditLock, stdout, "PASSWORD: ");
    struct termios oflags, nflags;
    tcgetattr(fileno(stdin), &oflags);
    nflags = oflags;
    nflags.c_lflag &= ~ECHO;
    nflags.c_lflag |= ECHONL;

    tcsetattr(fileno(stdin), TCSANOW, &nflags);

    char *password = malloc(MAX_INPUT);
    memset(password, 0, MAX_INPUT);
    fgets(password, MAX_INPUT, stdin);

    tcsetattr(fileno(stdin), TCSANOW, &oflags);

    removeNewline(password, strlen(password));

    message = malloc(4 + 1 + strlen(password) + 4 + 1);
    memset(message, 0, 10 + strlen(password));
    sprintf(message, "PASS %s \r\n\r\n", password);
    send(clientSocket, message, strlen(message), 0);
    if(verboseFlag){
      char *verbose = malloc(strlen(message));
      memset(verbose, 0, strlen(message));
      strncpy(verbose, message, strlen(message) - 5);
      sfwrite(auditLock, stderr, "%sSENT TO SERVER: %s%s\n", BLUE, verbose, NORMAL);
      free(verbose);
    }
    free(password);
    free(message);

    //NOW WAIT FOR SERVER RESPONSE AGAIN
    readSet = set;

    wait = select(FD_SETSIZE, &readSet, NULL, NULL, NULL);
    if(wait == -1){
      sfwrite(auditLock, stderr, "%s\n", "Error on select, exiting!");
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    readBuffer(clientSocket, true);
    //NO PROTOCOL SENT ON MESSAGE ABORT LOGIN
    if(!checkProtocol()){
      sfwrite(auditLock, stderr, "NO PROTOCOL ATTACHED, ABORTING LOGIN AND CLOSING CLIENT!\n");
      close(clientSocket);
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    if(strlen(buffer) < 4){
      sfwrite(auditLock, stderr, "Message sent from server too small to be right message\n");
      close(clientSocket);
      free(message);
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    verb = malloc(5);
    memset(verb, 0, 5);
    strncpy(verb, buffer, 4);
    if(strcmp(verb, "SSAP") != 0){
      sfwrite(auditLock, stderr, "Invalid verb sent or error sent, closing down client!\n");
      free(verb);
      close(clientSocket);
      fclose(audit);
      exit(EXIT_FAILURE);
    }
    free(verb);

    //NOW WAIT FOR SERVER RESPONSE AGAIN
    readSet = set;

    wait = select(FD_SETSIZE, &readSet, NULL, NULL, NULL);
    if(wait == -1){
      sfwrite(auditLock, stderr, "%s\n", "Error on select, exiting!");
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    readBuffer(clientSocket, true);
    //NO PROTOCOL SENT ON MESSAGE ABORT LOGIN
    if(!checkProtocol()){
      sfwrite(auditLock, stderr, "NO PROTOCOL ATTACHED, ABORTING LOGIN AND CLOSING CLIENT!\n");
      close(clientSocket);
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    sfwrite(auditLock, stdout, "%s\n", buffer);

    //NOW WAIT FOR SERVER RESPONSE AGAIN
    readSet = set;

    wait = select(FD_SETSIZE, &readSet, NULL, NULL, NULL);
    if(wait == -1){
      sfwrite(auditLock, stderr, "%s\n", "Error on select, exiting!");
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    readBuffer(clientSocket, true);
    //NO PROTOCOL SENT ON MESSAGE ABORT LOGIN
    if(!checkProtocol()){
      sfwrite(auditLock, stderr, "NO PROTOCOL ATTACHED, ABORTING LOGIN AND CLOSING CLIENT!\n");
      close(clientSocket);
      fclose(audit);
      exit(EXIT_FAILURE);
    }

    //MOTD HERE
    sfwrite(auditLock, stdout, "%s\n", buffer);
    char *t = timestamp();
    sfwrite(auditLock, audit, "%s, %s, LOGIN, %s:%d, success, %s\n", t, name, serverIP, serverPort, buffer);
    free(t);

    //DONE WITH LOGIN PROCESS OF EXISTING USER
    return;
  }


}

void SIGINTHandler(int sig){
  char *message = "BYE \r\n\r\n\0";
  send(clientSocket, message, strlen(message), 0);
  write(0, "\033[1A", 4);
  write(0, "\033[K", 3);
  write(1, "\n", 1);
  if(verboseFlag){
    sfwrite(auditLock, stderr, "%sSENT TO SERVER: %s%s\n", BLUE, "BYE", NORMAL);
  }
  close(clientSocket);
  for(chat *iterator = head; iterator != NULL;){
    close(iterator->fd);
    close(iterator->fdChat);
    kill(iterator->PID, 9);
    waitpid(iterator->PID, NULL, 0);
    removeChat(iterator);
    iterator = head;
    if(iterator == NULL){
      break;
    }
  }
  char *t = timestamp();
  sfwrite(auditLock, audit, "%s, %s, LOGOUT, CTRL-C\n", t, name);
  free(t);
  free(auditLock);
  fclose(audit);
  exit(EXIT_SUCCESS);
}

void auditFileOpen(){
  struct stat buf;

  //FILE *audit
  //a+ denotes reading and appending to end of file
  //CHECK TO SEE IF FILE ALREADY EXISTS
  if(stat(auditFile, &buf) == 0){
    audit = fopen(auditFile, "a+");
  }

  //ELSE NEED TO CREATE FILE
  else{
    audit = fopen(auditFile, "a+");
  }
}


char *timestamp(){
  time_t rawtime;
  struct tm *info;
  char *s = malloc(18);
  memset(s, 0, 18);
  
  time(&rawtime);
  info = localtime(&rawtime);

  strftime(s, 18, "%m-%d-%y-%l:%M%p", info);
  return s;
}
