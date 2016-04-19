#include "server.h"

struct connected_user {
  int socket;
  char * username;
  time_t loginTime;
  struct connected_user * next;
  struct connected_user * prev;
};
typedef struct connected_user connected_user;

struct user_account {
  char * username;
  char * password;
  struct user_account * next;
  //struct user_account * prev;
};
typedef struct user_account user_account;

connected_user * list_head = NULL;
user_account * account_head = NULL;
static char motd[MAX_INPUT] = {0};
static int cThread = 0;
static int verboseFlag = 0;
static int commandFlag = 1;
//static int commPipe[2];

int main(int argc, char *argv[]) {
  //first check the # of arg's to see if any flags were given
  int opt;
  int portNumber;
  if ((argc == 4) || (argc == 5) || (argc == 6)) {
    while((opt = getopt(argc, argv, "hv")) != -1) {
      switch(opt) {
        case 'h':
          printf(USAGE);
          exit(EXIT_SUCCESS);
          break;
        case 'v':
          verboseFlag = 1;
          //printf("%d", verboseFlag);
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
    //is there a file to load?
    if ((optind + 2) != argc) {
      struct stat testFile;
      int statTest = stat(argv[optind + 2], &testFile);
      if (!statTest) {
        FILE * readFile = fopen(argv[optind + 2], "r");
        char readline[MAX_INPUT] = {0};
        while (fgets(readline, sizeof(readline), readFile) != NULL) {
          //remove the new line if it exists
          char * findNewline = strchr(readline, 10);
          if (findNewline != NULL)
            * findNewline = '\0';
          //now tokenize the input once to separate the username and pw
          char * userName = strtok(readline, " ");
          char * passWord = &readline[strlen(readline) + 1];
          user_account * theAccount = malloc(sizeof(user_account));
          theAccount->username = malloc(strlen(userName) + 1);
          theAccount->password = malloc(strlen(passWord) + 1);
          strcpy(theAccount->username, userName);
          strcpy(theAccount->password, passWord);
          theAccount->next = NULL;
          if (account_head == NULL) {
            account_head = theAccount;
          }
          else {
            user_account * iterator = account_head;
            while (iterator->next != NULL) {
              iterator = iterator->next;
            }
            iterator->next = theAccount;
          }
          memset(readline, 0, MAX_INPUT);
        }
      }
      else {
        printf("Specified file %s does not exist.\n", argv[optind + 2]);
        exit(EXIT_FAILURE);
      }
    }
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
    printf("Failed to make server socket. Quitting...\n");
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
    printf("Failed to bind to port %d. Quitting...\n", portNumber);
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
  //int commandFlag = 1;
  //create the pipe
  //if (pipe(commPipe)) {
//    printf("Pipe failed. Quitting...\n");
//    exit(EXIT_FAILURE);
//  }
  //run forever until receive /shutdown
  while(1) {
    if (commandFlag)
      write(1, ">", 1);
    readFdSet = activeFdSet;
    if (select(FD_SETSIZE, &readFdSet, NULL, NULL, NULL) == -1) {
      printf("Select failed. Quitting...\n");
      exit(EXIT_FAILURE);
    }
    for(counter = 0; counter < FD_SETSIZE; ++counter) {
      if (FD_ISSET(counter, &readFdSet)) {
        if (counter == serverSocket) {
          //accept an incoming connection
          commandFlag = 0;
          unsigned int clientLength;
          int clientSocket = accept(serverSocket, (struct sockaddr *) &clientInfo, &clientLength);
          if (clientSocket == -1) {
            int errcode = errno;
            printf("Accept error. Code %d.\n", errcode);
            fflush(stdout);
          }
          else {
            //login thread
            pthread_t tid;
            pthread_create(&tid, NULL, handleClient, &clientSocket);
            pthread_join(tid, NULL);
          }
        }
        //is there something on stdin for the server?
        else if (counter == 0) {
          commandFlag = 1;
          char test[MAX_INPUT] = {0};
          fgets(test, MAX_INPUT - 1, stdin);
          if (strncmp("/help\n", test, 6) == 0) {
            printf("--------------------------------------\nCOMMAND LIST\n/users          Prints list of current users\n/help           Prints this prompt\n/shutdown       Shuts down server\n--------------------------------------\n");
            fflush(stdout);
          }
          else if (strncmp("/users\n", test, 7) == 0) {
            connected_user * iterator = list_head;
            while (iterator != NULL) {
              printf("%s\n", iterator->username);
              fflush(stdout);
              iterator = iterator->next;
            }
          }
          else if (strncmp("/accts\n", test, 7) == 0) {
            user_account * iterator = account_head;
            printf("------------------------------------------------\nACCOUNTS\n");
            fflush(stdout);
            while (iterator != NULL) {
              printf("User: %20s      ", iterator->username);
              fflush(stdout);
              printf("Password: %20s\n", iterator->password);
              fflush(stdout);
              iterator = iterator->next;
            }
            printf("------------------------------------------------\n");
            fflush(stdout);
          }
          else if (strncmp("/shutdown\n", test, 10) == 0) {
            connected_user * iterator = list_head;
            while (iterator != NULL) {
              connected_user * temp = iterator;
              iterator = iterator->next;
              send(temp->socket, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
              close(temp->socket);
              free(temp->username);
              free(temp);
            }
            user_account * iterator2 = account_head;
            while (iterator2 != NULL) {
              user_account * temp = iterator2;
              iterator2 = iterator2->next;
              free(temp->username);
              free(temp->password);
              free(temp);
            }
            if (verboseFlag) {
              write(1, "Sent to all users: ", 19);
              write(1, "BYE \r\n\r\n", 8);
              write(1, "\n", 1);
              commandFlag = 1;
            }
            exit(EXIT_SUCCESS);
          }
          else {
            test[strlen(test) - 1] = '\0';
            printf("Unknown command %s. Type /help for more information.\n", test);
            fflush(stdout);
          }
        }
        //????
        else {

        }
      }
    }
  }
}

//check if a username is already taken. returns 0 if taken, 1 if not.
int checkAvailability(char * toCheck) {
  connected_user * iterator = list_head;
  while (iterator != NULL) {
    if (strcmp(iterator->username, toCheck) == 0) {
      return 0;
    }
    iterator = iterator->next;
  }
  return 1;
}

//check if a received message has the correct EOM. returns either 0 or 1
int checkEOM(char * start) {
  int size;
  int counter;
  if ((size = strlen(start)) < 4)
    return 0;
  for (counter = 0; counter < size; counter++) {
    if ((start[counter] == '\r') && ((size - counter) >= 4)) {
      char check[4] = {0};
      strncpy(check, start + counter, 4);
      if (strcmp(check, "\r\n\r\n") == 0) {
        memset(start + counter, 0, MAX_INPUT - counter - 1);
        return 1;
      }
    }
  }
  return 0;
}

//handle login
void * handleClient(void * param) {
  int client = *((int *) param);
  //set up holding area for data
  char input[MAX_INPUT] = {0};
  int recvData;
  int addClient = 0;
  //int addNew = 0;
  recvData = recv(client, input, MAX_INPUT, 0);

  //check if client started login protocol correctly
  if (recvData > 0) {
    if (verboseFlag) {
      write(1, "Received: ", 10);
      write(1, input, strlen(input));
      write(1, "\n", 1);
      commandFlag = 1;
    }
    if (strcmp(input, "WOLFIE\r\n\r\n") == 0) {
      send(client, "EIFLOW\r\n\r\n", strlen("EIFLOW\r\n\r\n"), 0);
      if (verboseFlag) {
        write(1, "Sent: ", 6);
        write(1, "EIFLOW\r\n\r\n", 10);
        write(1, "\n", 1);
        commandFlag = 1;
      }
    }
    //incorrect protocol
    else {

    }
  }
  //client closed unexpectedly
  else {
    close(client);
    return NULL;
  }

  memset(input, 0, MAX_INPUT);
  recvData = recv(client, input, MAX_INPUT, 0);
  if (recvData > 0) {
    if (verboseFlag) {
      write(1, "Received: ", 10);
      write(1, input, strlen(input));
      write(1, "\n", 1);
      commandFlag = 1;
    }
    char check1[10] = {0};
    char check2[10] = {0};
    char name[100] = {0};

    //check if the message has \r\n\r\n
    if (checkEOM(input)) {
      int checkWolfieProtocol = sscanf(input, "%s %s %s", check1, name, check2);
      if ((strcmp(check1, "IAM") == 0) && (checkWolfieProtocol == 2)) {

        //first check if the name is taken
        if (checkAvailability(name)) {
          //then check if it exists in the user accounts list
          if (verifyUser(name, NULL)) {
            //if it does, then send AUTH
            char authResponse[200] = {0};
            strcpy(authResponse, "AUTH ");
            strcat(authResponse, name);
            strcat(authResponse, " \r\n\r\n");
            send(client, authResponse, strlen(authResponse), 0);
            if (verboseFlag) {
              write(1, "Sent: ", 6);
              write(1, authResponse, strlen(authResponse));
              write(1, "\n", 1);
              commandFlag = 1;
            }

            //memset the input to recv again, since we are expecting the password next
            memset(input, 0, MAX_INPUT);
            recvData = recv(client, input, MAX_INPUT, 0);
            if (recvData > 0) {
              if (checkEOM(input)) {
                if (strncmp(input, "PASS ", 5) == 0) {
                  //PASS <password> \r\n\r\n
                  //012345 < strncpy from here, but ignore the space right before the \r\n\r\n
                  char password[200] = {0};
                  strncpy(password, &input[5], strlen(input) - 6);
                  if (verifyUser(name, password)) {
                    //if pw is correct, send SSAP, HI, and MOTD
                    send(client, "SSAP \r\n\r\n", strlen("SSAP \r\n\r\n"), 0);
                    if (verboseFlag) {
                      write(1, "Sent: ", 6);
                      write(1, "SSAP \r\n\r\n", strlen("SSAP \r\n\r\n"));
                      write(1, "\n", 1);
                      commandFlag = 1;
                    }

                    //HI
                    char hiResponse[200] = {0};
                    sprintf(hiResponse, "%s", "HI ");
                    strcat(hiResponse, name);
                    strcat(hiResponse, " \r\n\r\n");
                    send(client, hiResponse, strlen(hiResponse), 0);
                    if (verboseFlag) {
                      write(1, "Sent: ", 6);
                      write(1, hiResponse, strlen(hiResponse));
                      write(1, "\n", 1);
                      commandFlag = 1;
                    }

                    //MOTD
                    char sendMOTD[200] = {0};
                    strcpy(sendMOTD, "MOTD ");
                    strcat(sendMOTD, motd);
                    strcat(sendMOTD, " \r\n\r\n");
                    send(client, sendMOTD, strlen(sendMOTD), 0);
                    if (verboseFlag) {
                      write(1, "Sent: ", 6);
                      write(1, sendMOTD, strlen(sendMOTD));
                      write(1, "\n", 1);
                      commandFlag = 1;
                    }

                    addClient = 1;
                  }
                }
                //not correct protocol
                else {

                }
              }
            }
            //client closed unexpectedly
            else {
              close(client);
              return NULL;
            }
          }
          //name doesn't exist in the list, send ERR 01
          else {
            send(client, "ERR 01 USER NOT AVAILABLE \r\n\r\n", strlen("ERR 01 USER NOT AVAILABLE \r\n\r\n"), 0);
            if (verboseFlag) {
              write(1, "Sent: ", 6);
              write(1, "ERR 01 USER NOT AVAILABLE \r\n\r\n", strlen("ERR 01 USER NOT AVAILABLE \r\n\r\n"));
              write(1, "\n", 1);
              commandFlag = 1;
            }
            send(client, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
            if (verboseFlag) {
              write(1, "Sent: ", 6);
              write(1, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"));
              write(1, "\n", 1);
              commandFlag = 1;
            }
            close(client);
          }
        }
        //name already taken, send ERR 00
        else {
          send(client, "ERR 00 USER NAME TAKEN \r\n\r\n", strlen("ERR 00 USER NAME TAKEN \r\n\r\n"), 0);
          if (verboseFlag) {
            write(1, "Sent: ", 6);
            write(1, "ERR 00 USER NAME TAKEN \r\n\r\n", strlen("ERR 00 USER NAME TAKEN \r\n\r\n"));
            write(1, "\n", 1);
            commandFlag = 1;
          }
          send(client, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
          if (verboseFlag) {
            write(1, "Sent: ", 6);
            write(1, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"));
            write(1, "\n", 1);
            commandFlag = 1;
          }
          close(client);
        }
      }
      else if ((strcmp(check1, "IAMNEW") == 0) && (checkWolfieProtocol == 2)) {
        //does the name already exist?
        if (verifyUser(name, NULL)) {

          //send HINEW
          char hiResponse[200] = {0};
          sprintf(hiResponse, "%s", "HINEW ");
          strcat(hiResponse, name);
          strcat(hiResponse, " \r\n\r\n");
          send(client, hiResponse, strlen(hiResponse), 0);
          if (verboseFlag) {
            write(1, "Sent: ", 6);
            write(1, hiResponse, strlen(hiResponse));
            write(1, "\n", 1);
            commandFlag = 1;
          }

          //wait for NEWPASS
          memset(input, 0, MAX_INPUT);
          recvData = recv(client, input, MAX_INPUT, 0);
          if (recvData > 0) {
            if (checkEOM(input)) {
              if (strncmp(input, "NEWPASS ", 8) == 0) {
                char password[200] = {0};
                strncpy(password, &input[8], strlen(input) - 6);
                if (verifyPass(password)) {

                  //send SSAPWEN
                  char * ssapwenMessage = "SSAPWEN \r\n\r\n";
                  send(client, ssapwenMessage, strlen(ssapwenMessage), 0);
                  if (verboseFlag) {
                    write(1, "Sent: ", 6);
                    write(1, ssapwenMessage, strlen(ssapwenMessage));
                    write(1, "\n", 1);
                    commandFlag = 1;
                  }

                  //send HI
                  char hiMessage[200] = {0};
                  strcpy(hiMessage, "HI ");
                  strcat(hiMessage, name);
                  strcat(hiMessage, " \r\n\r\n");
                  send(client, hiMessage, strlen(hiMessage), 0);
                  if (verboseFlag) {
                    write(1, "Sent: ", 6);
                    write(1, hiMessage, strlen(hiMessage));
                    write(1, "\n", 1);
                    commandFlag = 1;
                  }

                  //send MOTD
                  char motdMessage[400] = {0};
                  strcpy(motdMessage, "MOTD ");
                  strcat(motdMessage, motd);
                  strcat(motdMessage, " \r\n\r\n");
                  send(client, motdMessage, strlen(motdMessage), 0);
                  if (verboseFlag) {
                    write(1, "Sent: ", 6);
                    write(1, motdMessage, strlen(motdMessage));
                    write(1, "\n", 1);
                    commandFlag = 1;
                  }
                  //finally set the addClient flag to 1
                  addClient = 1;
                }
                else {
                  //bad password, send ERR 02
                  send(client, "ERR 02 BAD PASSWORD \r\n\r\n", strlen("ERR 02 BAD PASSWORD \r\n\r\n"), 0);
                  if (verboseFlag) {
                    write(1, "Sent: ", 6);
                    write(1, "ERR 02 BAD PASSWORD \r\n\r\n", strlen("ERR 02 BAD PASSWORD \r\n\r\n"));
                    write(1, "\n", 1);
                    commandFlag = 1;
                  }
                  send(client, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
                  if (verboseFlag) {
                    write(1, "Sent: ", 6);
                    write(1, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"));
                    write(1, "\n", 1);
                    commandFlag = 1;
                  }
                  close(client);
                }
              }
              //incorrect protocol
              else {

              }
            }
            //incorrect protocol
            else {

            }
          }
          //client closed unexpectedly
          else {
            close(client);
            return NULL;
          }
        }
        //if name already exists, then send ERR 00
        else {
          send(client, "ERR 00 USER NAME TAKEN \r\n\r\n", strlen("ERR 00 USER NAME TAKEN \r\n\r\n"), 0);
          if (verboseFlag) {
            write(1, "Sent: ", 6);
            write(1, "ERR 00 USER NAME TAKEN \r\n\r\n", strlen("ERR 00 USER NAME TAKEN \r\n\r\n"));
            write(1, "\n", 1);
            commandFlag = 1;
          }
          send(client, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
          if (verboseFlag) {
            write(1, "Sent: ", 6);
            write(1, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"));
            write(1, "\n", 1);
            commandFlag = 1;
          }
          close(client);
        }
      }
      //incorrect protocol
      else {

      }
    }
    //add to client list if applicable
    if (addClient) {
      //now add the user and his/her information to the list
      connected_user * currentlyConnected = malloc(sizeof(connected_user));
      memset(currentlyConnected, 0, sizeof(connected_user));
      currentlyConnected->socket = client;
      currentlyConnected->username = malloc(strlen(name) + 1);
      strcpy(currentlyConnected->username, name);
      //is the list empty?
      if (list_head == NULL) {
        currentlyConnected->prev = NULL;
        currentlyConnected->next = NULL;
        currentlyConnected->loginTime = time(NULL);
        list_head = currentlyConnected;
      }
      //otherwise go to end and add it there
      else {
        connected_user * iterator = list_head;
        while(iterator->next != NULL) {
          iterator = iterator->next;
        }
        currentlyConnected->prev = iterator;
        currentlyConnected->next = NULL;
        currentlyConnected->loginTime = time(NULL);
        iterator->next = currentlyConnected;
      }
      //then run the communication thread
      if ((!cThread) && (list_head != NULL)) {
        pthread_create(&cid, NULL, communicationThread, &cThread);
        pthread_detach(cid);
      }
      //if it already exists, need to write to the pipe so the communication thread knows to update accordingly
      else {
        pthread_cancel(cid);
        pthread_create(&cid, NULL, communicationThread, &cThread);
        pthread_detach(cid);
      }
    }
  }
  //client closed unexpectedly
  else {
    close(client);
    return NULL;
  }
  return NULL;
}

//communication thread. the pointer passed to it is the cThread flag, indicating if it's already running
void * communicationThread(void * param) {
  //set cThread to 1 to prevent multiple communicationThreads from spawning
  *((int *) param) = 1;
  //since the head of the connected_user list is global, it doesn't need to be passed
  connected_user * iterator;
  fd_set clientList, zeroedList;
  FD_ZERO(&zeroedList);
  //add the read end of the pipe to detect when to update fd's
  while (list_head != NULL) {
    iterator = list_head;
    clientList = zeroedList;
    //FD_SET(commPipe[0], &clientList);
    while (iterator != NULL) {
      FD_SET(iterator->socket, &clientList);
      iterator = iterator->next;
    }
    if (select(FD_SETSIZE, &clientList, NULL, NULL, NULL) == -1) {
      printf("Select failed.\n");
      exit(EXIT_FAILURE);
    }
    //new connection?
    /*if (FD_ISSET(commPipe[0], &clientList)) {
      char stuff[3];
      read(commPipe[0], stuff, 1);
      iterator = list_head;
      clientList = zeroedList;
      while (iterator != NULL) {
        FD_SET(iterator->socket, &clientList);
        iterator = iterator->next;
      }
    }*/
    for(iterator = list_head; iterator != NULL; iterator = iterator->next) {
      if (FD_ISSET(iterator->socket, &clientList)) {
          //is this socket the one with input?
            char input[MAX_INPUT] = {0};
            //since already connected, just listen for communication
            int data = recv(iterator->socket, input, MAX_INPUT, 0);
            if (data > 0) {
              if (verboseFlag) {
                write(1, "Received: ", 10);
                write(1, input, strlen(input));
                write(1, "\n", 1);
                commandFlag = 1;
              }
              if (checkEOM(input)) {
                //was the response a /logout?
                if (strcmp(input, "BYE") == 0) {
                  //take note of the name
                  char * storeName;
                  //need to remove the socket from the list and free up the memory it took up....but where is the user on the list?
                  //only one element in the list
                  if ((iterator->prev == NULL) && (iterator->next == NULL)) {
                    list_head = NULL;
                    close(iterator->socket);
                    storeName = iterator->username;
                    free(iterator);
                    *((int *) param) = 0;
                    //if there's no user, then end the thread
                    return NULL;
                  }
                  //if at beginning
                  if (iterator->prev == NULL) {
                    iterator->next->prev = NULL;
                    list_head = iterator->next;
                    close(iterator->socket);
                    storeName = iterator->username;
                    free(iterator);
                  }
                  //if at end
                  else if (iterator->next == NULL) {
                    iterator->prev->next = NULL;
                    close(iterator->socket);
                    storeName = iterator->username;
                    free(iterator);
                  }
                  //if in between
                  else {
                    iterator->prev->next = iterator->next;
                    iterator->next->prev = iterator->prev;
                    close(iterator->socket);
                    storeName = iterator->username;
                    free(iterator);
                  }
                  //need to send UOFF to the other users afterwards
                  connected_user * temp = list_head;
                  char uoffMessage[MAX_INPUT] = {0};
                  strcpy(uoffMessage, "UOFF ");
                  strcat(uoffMessage, storeName);
                  strcat(uoffMessage, " \r\n\r\n");
                  while (temp != NULL) {
                    send(temp->socket, uoffMessage, strlen(uoffMessage), 0);
                    temp = temp->next;
                  }
                  if (verboseFlag) {
                    write(1, "Sent to all users: ", 19);
                    write(1, uoffMessage, strlen(uoffMessage));
                    write(1, "\n", 1);
                    commandFlag = 1;
                  }
                  free(storeName);
                  //write(commPipe[1], "a", 1);
                }
                else if (strcmp(input, "TIME") == 0) {
                  time_t now = time(NULL);
                  int difference = now - iterator->loginTime;
                  char emitMessage[50] = {0};
                  sprintf(emitMessage, "%s %d %s", "EMIT", difference, "\r\n\r\n");
                  send(iterator->socket, emitMessage, strlen(emitMessage), 0);
                  if (verboseFlag) {
                    write(1, "Sent: ", 6);
                    write(1, emitMessage, strlen(emitMessage));
                    write(1, "\n", 1);
                    commandFlag = 1;
                  }
                }
                else if (strcmp(input, "LISTU") == 0) {
                  //random length (hope that the message isn't that long).
                  int mallocSize = 101;
                  char * utsilMessage = malloc(mallocSize);
                  memset(utsilMessage, 0, mallocSize);
                  strcpy(utsilMessage, "UTSIL ");
                  connected_user * temp = list_head;
                  while (temp != NULL) {
                    //if we're approaching the end of the malloc's space, need to realloc
                    if ((strlen(utsilMessage) + strlen(temp->username) + 6) > mallocSize) {
                      mallocSize += 100;
                      utsilMessage = realloc(utsilMessage, mallocSize);
                    }
                    strcat(utsilMessage, temp->username);
                    if (temp->next != NULL)
                      strcat(utsilMessage, " \r\n ");
                    temp = temp->next;
                  }
                  //do we have enough space for the \r\n\r\n?
                  if ((strlen(utsilMessage) + 6) > mallocSize) {
                    mallocSize += 10;
                    utsilMessage = realloc(utsilMessage, mallocSize);
                  }
                  strcat(utsilMessage, " \r\n\r\n");
                  //now have the full message
                  send(iterator->socket, utsilMessage, strlen(utsilMessage), 0);
                  if (verboseFlag) {
                    write(1, "Sent: ", 6);
                    write(1, utsilMessage, strlen(utsilMessage));
                    write(1, "\n", 1);
                    commandFlag = 1;
                  }
                  //free the pointer
                  free(utsilMessage);
                }
                else if (strncmp(input, "MSG", 3) == 0) {
                  //MSG <to> <from>
                  //0123^ start parsing here
                  char * startParse = &input[4];
                  char * to;
                  char * from;
                  int test = parseMSG(startParse, &to, &from);
                  if (test) {
                    //if successful, simply send the input back to both the sender and receiver
                    connected_user * temp = list_head;
                    int sender = 0;
                    int receiver = 0;
                    while (temp != NULL) {
                      if (strcmp(temp->username, to) == 0)
                        receiver = temp->socket;
                      if (strcmp(temp->username, from) == 0)
                        sender = temp->socket;
                      temp = temp->next;
                    }
                    //need to check if the sockets were set before sending (aka does the user actually exist)
                    //or if the user is trying to talk to themselves
                    if ((sender == 0) || (receiver == 0)) {
                      send(iterator->socket, "ERR 01 USER NOT AVAILABLE \r\n\r\n", strlen("ERR 01 USER NOT AVAILABLE \r\n\r\n"), 0);
                      if (verboseFlag) {
                        write(1, "Sent: ", 6);
                        write(1, "ERR 01 USER NOT AVAILABLE \r\n\r\n", 30);
                        write(1, "\n", 1);
                        commandFlag = 1;
                      }
                    }
                    else {
                      //need to add back the \r\n\r\n
                      strcat(input, "\r\n\r\n");
                      send(sender, input, strlen(input), 0);
                      if (verboseFlag) {
                        write(1, "Sent: ", 6);
                        write(1, input, strlen(input));
                        write(1, "\n", 1);
                        commandFlag = 1;
                      }
                      send(receiver, input, strlen(input), 0);
                      if (verboseFlag) {
                        write(1, "Sent: ", 6);
                        write(1, input, strlen(input));
                        write(1, "\n", 1);
                        commandFlag = 1;
                      }
                    }
                    //free malloc space afterwards
                    free(to);
                    free(from);
                  }
                  else {
                    //otherwise..? this means server ran out of memory..idk
                  }
                }
              }
            }
            //the socket closed unexpectedly without BYE
            if (data == 0) {
              //iterator already contains the user information
              char uoffMessage[MAX_INPUT] = {0};
              connected_user * temp = list_head;
              char * storeName;
              //still need to remove the user from the list
              if ((iterator->prev == NULL) && (iterator->next == NULL)) {
                list_head = NULL;
                close(iterator->socket);
                storeName = iterator->username;
                free(iterator);
                *((int *) param) = 0;
                //if there's no user, then end the thread
                return NULL;
              }
              if (iterator->prev == NULL) {
                iterator->next->prev = NULL;
                list_head = iterator->next;
                close(iterator->socket);
                storeName = iterator->username;
                free(iterator);
              }
              else if (iterator->next == NULL) {
                iterator->prev->next = NULL;
                close(iterator->socket);
                storeName = iterator->username;
                free(iterator);
              }
              else {
                iterator->prev->next = iterator->next;
                iterator->next->prev = iterator->prev;
                close(iterator->socket);
                storeName = iterator->username;
                free(iterator);
              }
              strcpy(uoffMessage, "UOFF ");
              strcat(uoffMessage, storeName);
              strcat(uoffMessage, " \r\n\r\n");
              while (temp != NULL) {
                send(temp->socket, uoffMessage, strlen(uoffMessage), 0);
                temp = temp->next;
              }
              if (verboseFlag) {
                write(1, "Sent to all users: ", 19);
                write(1, uoffMessage, strlen(uoffMessage));
                write(1, "\n", 1);
                commandFlag = 1;
              }
              free(storeName);
            }
          }
        }
      }
  return NULL;
}

//looks through the input string for the sender and receiver
//MSG <to> <from>
//    ^input starts at to
int parseMSG(char * input, char ** to, char ** from) {
  int toSize = 1;
  int fromSize = 1;
  char * toUser = malloc(toSize);
  char * fromUser = malloc(fromSize);
  int counter = 0;
  //if malloc fails, immediately return
  if ((toUser == NULL) || (fromUser == NULL))
    return 0;

  //first get the receiver
  while (input[counter] != ' ') {
    //we're still reading the to field, why would there be a null terminator here? (aka message is ill-formed)
    if (input[counter] == '\0') {
      free(toUser);
      free(fromUser);
      return 0;
    }
    toUser[counter] = input[counter];
    counter++;
    //have we reached the end of the space we malloc'd?
    if (counter == toSize) {
      //add a small amount
      toSize += 16;
      char * temp = realloc(toUser, toSize);
      //if realloc fails, immediately return. when realloc fails, need to free the pointer
      if (temp == NULL) {
        free(toUser);
        free(fromUser);
        return 0;
      }
      else {
        toUser = temp;
      }
    }
  }
  //by the end of this loop, the entire <to> username will be copied.
  //need to add a null terminator to the end of the string. there will always be space
  //to add it since realloc is called whenever counter reaches the malloc size
  toUser[counter++] = '\0';

  //now get the sender.
  char * temp = input + counter;
  counter = 0;
  while (temp[counter] != ' ') {
    if (temp[counter] == '\0') {
      fromUser[counter++] = '\0';
      break;
    }
    fromUser[counter] = temp[counter];
    counter++;
    if (counter == fromSize) {
      fromSize += 16;
      char * temp2 = realloc(fromUser, fromSize);
      if (temp2 == NULL) {
        free(toUser);
        free(fromUser);
        return 0;
      }
      else {
        fromUser = temp2;
      }
    }
  }
  //don't need to add null terminator, while loop already does that.
  //realloc so they only occupy just enough space
  char * temp3 = realloc(toUser, strlen(toUser) + 1);
  char * temp4 = realloc(fromUser, strlen(fromUser) + 1);
  if ((temp3 == NULL) || (temp4 == NULL)) {
    free(toUser);
    free(fromUser);
    return 0;
  }
  else {
    * to = toUser;
    * from = fromUser;
    return 1;
  }
}

//looks for a user with name user. if its second arg is not null, it additionally verifies its password
int verifyUser(char * user, char * pass) {
  user_account * iterator = account_head;
  while (iterator != NULL) {
    if (strcmp(iterator->username, user) == 0) {
      if (pass != NULL) {
        if (strcmp(iterator->password, pass) == 0) {
          return 1;
        }
      }
      else {
        return 1;
      }
    }
    iterator = iterator->next;
  }
  return 0;
}

//makes sure a given password passes the criteria
int verifyPass(char * pass) {
  if (strlen(pass) < 5) {
    return 0;
  }
  int upperFlag = 0;
  int numberFlag = 0;
  int symbolFlag = 0;
  char iterator = * pass;
  while (iterator != '\0') {
    if ((iterator >= 'A') && (iterator <= 'Z')) {
      upperFlag = 1;
    }
    if ((iterator >= '0') && (iterator <= '9')) {
      numberFlag = 1;
    }
    if (((iterator >= '!') && (iterator <= '/')) || ((iterator >= ':') && (iterator <= '@')) || ((iterator >= '[') && (iterator <= '`')) || ((iterator >= '{') && (iterator <= '~')))
      symbolFlag = 1;
    iterator = * (pass++);
  }
  if (upperFlag && numberFlag && symbolFlag)
    return 1;
  else
    return 0;
}
