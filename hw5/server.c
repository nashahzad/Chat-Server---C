#include "server.h"

struct connected_user {
  int socket;
  char * username;
  time_t loginTime;
  struct connected_user * next;
  struct connected_user * prev;
};
typedef struct connected_user connected_user;

connected_user * list_head = NULL;
static char motd[MAX_INPUT] = {0};
static int cThread = 0;
static int commPipe[2];

int main(int argc, char *argv[]) {
  int verboseFlag = 0;
  //first check the # of arg's to see if any flags were given
  int opt;
  int portNumber;
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
  int commandFlag = 1;
  //create the pipe
  if (pipe(commPipe)) {
    printf("Pipe failed. Quitting...\n");
    exit(EXIT_FAILURE);
  }
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
            printf("/users test\n");
            fflush(stdout);
            connected_user * iterator = list_head;
            while (iterator != NULL) {
              printf("%s\n", iterator->username);
              fflush(stdout);
              iterator = iterator->next;
            }
          }
          else if (strncmp("/shutdown\n", test, 10) == 0) {
            printf("/shutdown test\n");
            fflush(stdout);
            connected_user * iterator = list_head;
            while (iterator != NULL) {
              connected_user * temp = iterator;
              iterator = iterator->next;
              send(temp->socket, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
              close(temp->socket);
              free(temp->username);
              free(temp);
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
  recvData = recv(client, input, MAX_INPUT, 0);

  //check if client started login protocol correctly
  if (recvData > 0) {
    if (strcmp(input, "WOLFIE\r\n\r\n") == 0) {
      send(client, "EIFLOW\r\n\r\n", strlen("EIFLOW\r\n\r\n"), 0);
    }
  }

  memset(input, 0, MAX_INPUT - 1);
  recvData = recv(client, input, MAX_INPUT, 0);
  if (recvData > 0) {
    char check1[5] = {0};
    char check2[5] = {0};
    char name[100] = {0};

    //check if the message is IAM <name> \r\n\r\n
    if (checkEOM(input)) {
      int checkWolfieProtocol = sscanf(input, "%s %s %s", check1, name, check2);
      if ((strcmp(check1, "IAM") == 0) /*&& strcmp(check2, "\r\n\r\n") */&& (checkWolfieProtocol == 2)) {

        //first check if the name is taken
        if (checkAvailability(name)) {
          char hiResponse[200] = {0};
          sprintf(hiResponse, "%s", "HI ");
          strcat(hiResponse, name);
          strcat(hiResponse, " \r\n\r\n");
          send(client, hiResponse, strlen(hiResponse), 0);

          //and send MOTD
          char sendMOTD[200] = {0};
          strcpy(sendMOTD, "MOTD ");
          strcat(sendMOTD, motd);
          strcat(sendMOTD, " \r\n\r\n");
          send(client, sendMOTD, strlen(sendMOTD), 0);

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
            pthread_t cid;
            pthread_create(&cid, NULL, communicationThread, &cThread);
          }
          //if it already exists, need to write to the pipe so the communication thread knows to update accordingly
          else {
            int PID = fork();
            if(PID == 0){
              close(commPipe[0]);
              write(commPipe[1], "a", 1);
              close(commPipe[1]);
              exit(0);
            }
            waitpid(PID, NULL, 0);
          }
        }
        //name already taken, send a different packet
        else {
          send(client, "ERR 00 USER NAME TAKEN \r\n\r\n", strlen("ERR 00 USER NAME TAKEN \r\n\r\n"), 0);
          send(client, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
          close(client);
        }
      }
    }
  }
  return NULL;
}

//communication thread. the pointer passed to it is the cThread flag, indicating if it's already running
void * communicationThread(void * param) {
  //set cThread to 1 to prevent multiple communicationThreads from spawning
  *((int *) param) = 1;
  int c;
  //since the head of the connected_user list is global, it doesn't need to be passed
  connected_user * iterator;
  fd_set clientList, zeroedList;
  FD_ZERO(&zeroedList);
  //add the read end of the pipe to detect when to update fd's
  while (list_head != NULL) {
    iterator = list_head;
    clientList = zeroedList;
    FD_SET(commPipe[0], &clientList);
    while (iterator != NULL) {
      FD_SET(iterator->socket, &clientList);
      iterator = iterator->next;
    }
    if (select(FD_SETSIZE, &clientList, NULL, NULL, NULL) == -1) {
      printf("Select failed.\n");
      exit(EXIT_FAILURE);
    }
    //new connection?
    if (FD_ISSET(commPipe[0], &clientList)) {
      char stuff[3];
      read(commPipe[0], stuff, 1);
      iterator = list_head;
      clientList = zeroedList;
      while (iterator != NULL) {
        FD_SET(iterator->socket, &clientList);
        iterator = iterator->next;
      }
    }
    for(c = 0; c < FD_SETSIZE; ++c) {
      if (FD_ISSET(c, &clientList)) {
        iterator = list_head;
        while (iterator != NULL) {
          //is this socket the one with input?
          if (iterator->socket == c) {
            char input[MAX_INPUT] = {0};
            //since already connected, just listen for communication
            int data = recv(c, input, MAX_INPUT, 0);
            if (data > 0) {
              if (checkEOM(input)) {
                //was the response a /logout?
                if (strcmp(input, "BYE") == 0) {
                  //need to remove the socket from the list and free up the memory it took up....but where is the user on the list?
                  //only one element in the list
                  if ((iterator->prev == NULL) && (iterator->next == NULL)) {
                    list_head = NULL;
                    free(iterator->username);
                    free(iterator);
                    close(c);
                    *((int *) param) = 0;
                    //if there's no user, then end the thread
                    return NULL;
                  }
                  //if at beginning
                  if (iterator->prev == NULL) {
                    iterator->next->prev = NULL;
                    list_head = iterator->next;
                    free(iterator->username);
                    free(iterator);
                    close(c);
                  }
                  //if at end
                  else if (iterator->next == NULL) {
                    iterator->prev->next = NULL;
                    free(iterator->username);
                    free(iterator);
                    close(c);
                  }
                  //if in between
                  else {
                    iterator->prev->next = iterator->next;
                    iterator->next->prev = iterator->prev;
                    free(iterator->username);
                    free(iterator);
                    close(c);
                  }
                  write(commPipe[1], "a", 1);
                }
                else if (strcmp(input, "TIME") == 0) {
                  time_t now = time(NULL);
                  int difference = now - iterator->loginTime;
                  char emitMessage[50] = {0};
                  sprintf(emitMessage, "%s %d %s", "EMIT", difference, "\r\n\r\n");
                  send(iterator->socket, emitMessage, strlen(emitMessage), 0);
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
                }
              }
            }
          }
          //if it doesn't, go to the next one in the list
          else {
            iterator = iterator->next;
          }
        }
      }
    }
  }
  return NULL;
}
