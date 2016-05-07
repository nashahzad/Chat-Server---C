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
  //char * password;
  unsigned char * salt;
  unsigned char * hash;
  struct user_account * next;
};
typedef struct user_account user_account;

struct next_request {
  int socket;
  struct next_request * next;
};
typedef struct next_request next_request;

connected_user * list_head = NULL;
user_account * account_head = NULL;
next_request * queue_head = NULL;

static char motd[MAX_INPUT] = {0};
static int cThread = 0;
static int verboseFlag = 0;
static int commandFlag = 1;
//static int commPipe[2];

int main(int argc, char *argv[]) {
  //install signal handler for SIGINT
  signal(SIGINT, handleSigInt);
  //initialize mutex for stdout
  stdoutLock = malloc(sizeof(pthread_mutex_t));
  memset(stdoutLock, 0, sizeof(pthread_mutex_t));
  if(pthread_mutex_init(stdoutLock, NULL) != 0){
    sfwrite(stdoutLock, stderr, "Error failed to intialize pthread_mutex_t *lock!\n");
    free(stdoutLock);
    exit(EXIT_FAILURE);
  }
  //first check the # of arg's to see if any flags were given
  int opt;
  int portNumber;
  args = argc;
  args2 = argv;
  if ((argc == 4) || (argc == 5) || (argc == 6) || (argc == 7)) {
    while((opt = getopt(argc, argv, "hvt:")) != -1) {
      switch(opt) {
        case 'h':
          sfwrite(stdoutLock, stderr, USAGE);
          exit(EXIT_SUCCESS);
          break;
        case 'v':
          verboseFlag = 1;
          //printf("%d", verboseFlag);
          break;
        case 't':
          threadCount = atoi(optarg);
          if (threadCount == 0)
            threadCount = 2;
          break;
        case '?':
        default:
          sfwrite(stdoutLock, stderr, USAGE);
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
        FILE * readFile = fopen(argv[optind + 2], "rb");
        //File will be of the form
        //username salt(16 bytes)hash(32 bytes, immediately after salt)
        //username, etc.
        char * accountUsername;
        unsigned char * accountSalt;
        unsigned char * accountHash;
        int readValue = readRecord(readFile, &accountUsername, &accountSalt, &accountHash);
        while (readValue > 0) {
          if (readValue == 2)
            break;
          user_account * theAccount = malloc(sizeof(user_account));
          theAccount->username = accountUsername;
          //theAccount->password = malloc(strlen(passWord) + 1);
          theAccount->salt = accountSalt;
          theAccount->hash = accountHash;
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
          readValue = readRecord(readFile, &accountUsername, &accountSalt, &accountHash);
        }
        fclose(readFile);
      }
      else {
        sfwrite(stdoutLock, stderr, "Specified file %s does not exist.\n", argv[optind + 2]);
        exit(EXIT_FAILURE);
      }
    }
  }
  else if (argc != 3) {
    sfwrite(stdoutLock, stderr, USAGE);
    exit(EXIT_FAILURE);
  }
  //now it is definitely known that argc is 3 here. the verbose flag (if found, is set)
  else {
    portNumber = atoi(argv[1]);
    strcpy(motd, argv[2]);
  }

  //create socket
  int setOption = 1;
  serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket == -1) {
    sfwrite(stdoutLock, stderr, "Failed to make server socket. Quitting...\n");
    exit(EXIT_FAILURE);
  }
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (void *) &setOption, sizeof(setOption));

  //then bind it to a port
  struct sockaddr_in serverInfo, clientInfo;
  memset(&serverInfo, 0, sizeof(serverInfo));
  memset(&clientInfo, 0, sizeof(clientInfo));
  serverInfo.sin_family = AF_INET;
  serverInfo.sin_port = htons(portNumber);
  serverInfo.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(serverSocket, (struct sockaddr *) &serverInfo, sizeof(serverInfo)) == -1) {
    sfwrite(stdoutLock, stderr, "Failed to bind to port %d. Quitting...\n", portNumber);
    exit(EXIT_FAILURE);
  }

  //create the fd_set to monitor multiple file descriptors (sockets), including stdin to look for server commands
  fd_set activeFdSet, readFdSet;
  FD_ZERO(&activeFdSet);
  FD_SET(serverSocket, &activeFdSet);
  FD_SET(0, &activeFdSet);

  //now listen for any connection requests
  listen(serverSocket, 128); //arbitrary queue length
  sfwrite(stdoutLock, stderr, "Currently listening on port %d\n", portNumber);
  int counter;
  int commandFlag = 1;
  //create the pipe
  if (pipe(commPipe)) {
   sfwrite(stdoutLock, stderr, "Pipe failed. Quitting...\n");
   exit(EXIT_FAILURE);
  }

  //initialize the queue and semaphore
  sem_init(&items_sem, 0, 0);

  //now startup the login threads
  int loginArrayCounter = 0;
  loginArray = malloc(sizeof(pthread_t) * threadCount);
  for(; loginArrayCounter < threadCount; loginArrayCounter++) {
    pthread_create((loginArray + loginArrayCounter), NULL, handleClient, NULL);
  }

  //run forever until receive /shutdown
  while(1) {
    if (commandFlag)
      sfwrite(stdoutLock, stdout, ">");
    readFdSet = activeFdSet;
    if (select(FD_SETSIZE, &readFdSet, NULL, NULL, NULL) == -1) {
      sfwrite(stdoutLock, stderr, "Select failed. Quitting...\n");
      exit(EXIT_FAILURE);
    }
    for(counter = 0; counter < FD_SETSIZE; ++counter) {
      if (FD_ISSET(counter, &readFdSet)) {
        if (counter == serverSocket) {
          //accept an incoming connection
          commandFlag = 0;
          unsigned int clientLength = sizeof((struct sockaddr *) &clientInfo);
          int clientSocket = accept(serverSocket, (struct sockaddr *) &clientInfo, &clientLength);
          if (clientSocket == -1) {
            int errcode = errno;
            sfwrite(stdoutLock, stderr, "Accept error. Code %d.\n", errcode);
          }
          else {
            //create the next_request struct for this connector and add it to the queue
            next_request * temp = malloc(sizeof(next_request));
            temp->socket = clientSocket;
            temp->next = NULL;

            //first acquire mutex for the queue
            pthread_mutex_lock(&queueLock);
            //then insert into queue
            if (queue_head == NULL) {
              queue_head = temp;
            }
            else {
              next_request * iterator = queue_head;
              while(iterator->next != NULL) {
                iterator = iterator->next;
              }
              iterator->next = temp;
            }
            //release mutex
            pthread_mutex_unlock(&queueLock);
            sem_post(&items_sem);

            //login thread
            //pthread_t tid;
            //pthread_create(&tid, NULL, handleClient, &clientSocket);
            //pthread_join(tid, NULL);
          }
        }
        //is there something on stdin for the server?
        else if (counter == 0) {
          commandFlag = 1;
          char test[MAX_INPUT] = {0};
          fgets(test, MAX_INPUT - 1, stdin);
          if (strncmp("/help\n", test, 6) == 0) {
            sfwrite(stdoutLock, stdout, "--------------------------------------\nCOMMAND LIST\n/accts          Prints list of user accounts\n/users          Prints list of current users\n/help           Prints this prompt\n/shutdown       Shuts down server\n--------------------------------------\n");
          }
          else if (strncmp("/users\n", test, 7) == 0) {
            pthread_mutex_lock(&usersLock);
            connected_user * iterator = list_head;
            while (iterator != NULL) {
              sfwrite(stdoutLock, stdout, "%s\n", iterator->username);
              iterator = iterator->next;
            }
            pthread_mutex_unlock(&usersLock);
          }
          else if (strncmp("/accts\n", test, 7) == 0) {
            pthread_mutex_lock(&accountsLock);
            user_account * iterator = account_head;
            sfwrite(stdoutLock, stdout, "------------------------------------------------\nACCOUNTS\n");
            while (iterator != NULL) {
              sfwrite(stdoutLock, stdout, "User: %12s      ", iterator->username);
              sfwrite(stdoutLock, stdout, "Salt: ");
              int i;
              for(i = 0; i < SALT_LENGTH; i++) {
                sfwrite(stdoutLock, stdout, "%02x", iterator->salt[i]);
              }
              sfwrite(stdoutLock, stdout, " Hash: ");
              for(i = 0; i < SHA256_DIGEST_LENGTH; i++) {
                sfwrite(stdoutLock, stdout, "%02x", iterator->hash[i]);
              }
              sfwrite(stdoutLock, stdout, "\n");
              iterator = iterator->next;
            }
            sfwrite(stdoutLock, stdout, "------------------------------------------------\n");
            pthread_mutex_unlock(&accountsLock);
          }
          else if (strncmp("/shutdown\n", test, 10) == 0) {
            //acquire mutexes to ensure previous requests are complete
            pthread_mutex_lock(&queueLock);
            pthread_mutex_lock(&accountsLock);
            pthread_mutex_lock(&usersLock);
            connected_user * iterator = list_head;
            while (iterator != NULL) {
              connected_user * temp = iterator;
              iterator = iterator->next;
              send(temp->socket, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
              close(temp->socket);
              free(temp->username);
              free(temp);
            }
            //the server should save the user accounts in case someone created a new account
            user_account * iterator2 = account_head;
            FILE * writeToFile;
            if ((optind + 2) != argc) {
              writeToFile = fopen(argv[optind + 2], "wb");
            }
            else {
              writeToFile = fopen("accts.txt", "wb");
            }
            while (iterator2 != NULL) {
              fprintf(writeToFile, "%s", iterator2->username);
              fprintf(writeToFile, " ");
              fwrite(iterator2->salt, 16, 1, writeToFile);
              fwrite(iterator2->hash, 32, 1, writeToFile);
              fprintf(writeToFile, "\n");
              user_account * temp = iterator2;
              iterator2 = iterator2->next;
              free(temp->username);
              free(temp->salt);
              free(temp->hash);
              free(temp);
            }
            fclose(writeToFile);
            if (verboseFlag) {
              sfwrite(stdoutLock, stdout, "Sent to all users: BYE \r\n\r\n\n");
              commandFlag = 1;
            }
            //free the login requests if any exist
            next_request * iterator3 = queue_head;
            while (iterator3 != NULL) {
              next_request * temp = iterator3;
              iterator3 = iterator3->next;
              free(temp);
            }
            close(serverSocket);
            free(stdoutLock);
            free(loginArray);
            //quit_signal = 1;
            //don't need to free mutexes since exiting anyway
            exit(EXIT_SUCCESS);
          }
          else {
            test[strlen(test) - 1] = '\0';
            sfwrite(stdoutLock, stdout, "Unknown command %s. Type /help for more information.\n", test);
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
  //acquire mutexes
  pthread_mutex_lock(&usersLock);
  connected_user * iterator = list_head;
  while (iterator != NULL) {
    if (strcmp(iterator->username, toCheck) == 0) {
      pthread_mutex_unlock(&usersLock);
      return 0;
    }
    iterator = iterator->next;
  }
  //if username is available, preemptively create an active user record for them
  connected_user * temp = malloc(sizeof(connected_user));
  temp->username = malloc(strlen(toCheck) + 1);
  strcpy(temp->username, toCheck);
  temp->next = NULL;
  if (list_head == NULL) {
    temp->prev = NULL;
    list_head = temp;
  }
  else {
    iterator = list_head;
    while(iterator->next != NULL) {
      iterator = iterator->next;
    }
    temp->prev = iterator;
    iterator->next = temp;
  }
  pthread_mutex_unlock(&usersLock);
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
  while(1) {

    sem_wait(&items_sem);
    //acquire mutex
    pthread_mutex_lock(&queueLock);
    //assume queue has something in it; semaphore takes care of the rest
    next_request * tempQueueHead = queue_head;
    //remove this request from the queue
    queue_head = queue_head->next;
    int client = tempQueueHead->socket;
    //free the request since we took what we need (the socket)
    free(tempQueueHead);
    //release queue mutex
    pthread_mutex_unlock(&queueLock);

    //int client = *((int *) param);

    //set up holding area for data
    char input[MAX_INPUT] = {0};
    int recvData;
    int addClient = 0;
    int addNew = 0;
    recvData = recv(client, input, MAX_INPUT, 0);

    //check if client started login protocol correctly
    if (recvData > 0) {
      if (verboseFlag) {
        sfwrite(stdoutLock, stdout, "Received: %s\n", input);
        commandFlag = 1;
      }
      if (strcmp(input, "WOLFIE \r\n\r\n") == 0) {
        send(client, "EIFLOW \r\n\r\n", strlen("EIFLOW \r\n\r\n"), 0);
        if (verboseFlag) {
          sfwrite(stdoutLock, stdout, "Sent: EIFLOW \r\n\r\n\n");
          commandFlag = 1;
        }
      }
      //incorrect protocol
      else {
        send(client, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
        if (verboseFlag) {
          sfwrite(stdoutLock, stdout, "Sent: BYE \r\n\r\n\n");
          commandFlag = 1;
        }
      }
    }
    //client closed unexpectedly
    else {
      close(client);
      continue;
    }

    memset(input, 0, MAX_INPUT);
    recvData = recv(client, input, MAX_INPUT, 0);
    if (recvData > 0) {
      if (verboseFlag) {
        sfwrite(stdoutLock, stdout, "Received: %s\n", input);
        commandFlag = 1;
      }
      char check1[10] = {0};
      char check2[10] = {0};
      char name[100] = {0};
      char password[200] = {0};
      char newPassword[200] = {0};

      //check if the message has \r\n\r\n
      if (checkEOM(input)) {
        int checkWolfieProtocol = sscanf(input, "%s %s %s", check1, name, check2);
        if ((strcmp(check1, "IAM") == 0) && (checkWolfieProtocol == 2)) {

          //acquire mutexes
          //pthread_mutex_lock(&accountsLock);
          //pthread_mutex_lock(&usersLock);

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
                sfwrite(stdoutLock, stdout, "Sent: %s\n", authResponse);
                commandFlag = 1;
              }

              //memset the input to recv again, since we are expecting the password next
              memset(input, 0, MAX_INPUT);
              recvData = recv(client, input, MAX_INPUT, 0);
              if (recvData > 0) {
                if (verboseFlag) {
                  sfwrite(stdoutLock, stdout, "Received: %s\n", input);
                  commandFlag = 1;
                }
                if (checkEOM(input)) {
                  if (strncmp(input, "PASS ", 5) == 0) {
                    //PASS <password> \r\n\r\n
                    //012345 < strncpy from here, but ignore the space right before the \r\n\r\n
                    strncpy(password, &input[5], strlen(input) - 5);
                    unsigned char enteredPw[SHA256_DIGEST_LENGTH];
                    SHA256_CTX context;
                    SHA256_Init(&context);
                    SHA256_Update(&context, (unsigned char *) password, strlen(password));
                    user_account * iterator = account_head;
                    while (iterator != NULL) {
                      if (strcmp(iterator->username, name) == 0) {
                        SHA256_Update(&context, iterator->salt, SALT_LENGTH);
                        break;
                      }
                      iterator = iterator->next;
                    }
                    SHA256_Final(enteredPw, &context);
                    if (verifyUser(name, enteredPw)) {
                      //if pw is correct, send SSAP, HI, and MOTD
                      send(client, "SSAP \r\n\r\n", strlen("SSAP \r\n\r\n"), 0);
                      if (verboseFlag) {
                        sfwrite(stdoutLock, stdout, "Sent: SSAP \r\n\r\n\n");
                        commandFlag = 1;
                      }

                      //HI
                      char hiResponse[200] = {0};
                      sprintf(hiResponse, "%s", "HI ");
                      strcat(hiResponse, name);
                      strcat(hiResponse, " \r\n\r\n");
                      send(client, hiResponse, strlen(hiResponse), 0);
                      if (verboseFlag) {
                        sfwrite(stdoutLock, stdout, "Sent: %s\n", hiResponse);
                        commandFlag = 1;
                      }

                      //MOTD
                      char sendMOTD[200] = {0};
                      strcpy(sendMOTD, "MOTD ");
                      strcat(sendMOTD, motd);
                      strcat(sendMOTD, " \r\n\r\n");
                      send(client, sendMOTD, strlen(sendMOTD), 0);
                      if (verboseFlag) {
                        sfwrite(stdoutLock, stdout, "Sent: %s\n", sendMOTD);
                        commandFlag = 1;
                      }

                      addClient = 1;
                    }
                    //incorrect password
                    else {
                      send(client, "ERR 02 BAD PASSWORD \r\n\r\n", strlen("ERR 02 BAD PASSWORD \r\n\r\n"), 0);
                      if (verboseFlag) {
                        sfwrite(stdoutLock, stdout, "Sent: ERR 02 BAD PASSWORD \r\n\r\n\n");
                        commandFlag = 1;
                      }
                      send(client, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
                      if (verboseFlag) {
                        sfwrite(stdoutLock, stdout, "Sent: BYE \r\n\r\n\n");
                        commandFlag = 1;
                      }
                      continue;
                    }
                  }
                  //not correct protocol
                  else {
                    send(client, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
                    if (verboseFlag) {
                      sfwrite(stdoutLock, stdout, "Sent: BYE \r\n\r\n\n");
                      commandFlag = 1;
                    }
                    continue;
                  }
                }
              }
              //client closed unexpectedly
              else {
                close(client);
                continue;
              }
            }
            //name doesn't exist in the list, send ERR 01
            else {
              send(client, "ERR 01 USER NOT AVAILABLE \r\n\r\n", strlen("ERR 01 USER NOT AVAILABLE \r\n\r\n"), 0);
              if (verboseFlag) {
                sfwrite(stdoutLock, stdout, "Sent: ERR 01 USER NOT AVAILABLE \r\n\r\n\n");
                commandFlag = 1;
              }
              send(client, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
              if (verboseFlag) {
                sfwrite(stdoutLock, stdout, "Sent: BYE \r\n\r\n\n");
                commandFlag = 1;
              }
              close(client);
              continue;
            }
          }
          //name already taken, send ERR 00
          else {
            send(client, "ERR 00 USER NAME TAKEN \r\n\r\n", strlen("ERR 00 USER NAME TAKEN \r\n\r\n"), 0);
            if (verboseFlag) {
              sfwrite(stdoutLock, stdout, "Sent: ERR 00 USER NAME TAKEN \r\n\r\n\n");
              commandFlag = 1;
            }
            send(client, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
            if (verboseFlag) {
              sfwrite(stdoutLock, stdout, "Sent: BYE \r\n\r\n\n");
              commandFlag = 1;
            }
            close(client);
            continue;
          }
        }
        else if ((strcmp(check1, "IAMNEW") == 0) && (checkWolfieProtocol == 2)) {
          
          if(checkAvailability(name)){}
          else{
            send(client, "ERR 00 USER NAME TAKEN \r\n\r\n", strlen("ERR 00 USER NAME TAKEN \r\n\r\n"), 0);
            if (verboseFlag) {
              sfwrite(stdoutLock, stdout, "Sent: ERR 00 USER NAME TAKEN \r\n\r\n\n");
              commandFlag = 1;
            }
            send(client, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
            if (verboseFlag) {
              sfwrite(stdoutLock, stdout, "Sent: BYE \r\n\r\n\n");
              commandFlag = 1;
            }
            close(client);
            continue;
          }

          //does the name already exist?
          if (!(verifyUser(name, NULL))) {
            checkAvailability(name);

            //send HINEW
            char hiResponse[200] = {0};
            sprintf(hiResponse, "%s", "HINEW ");
            strcat(hiResponse, name);
            strcat(hiResponse, " \r\n\r\n");
            send(client, hiResponse, strlen(hiResponse), 0);
            if (verboseFlag) {
              sfwrite(stdoutLock, stdout, "Sent: %s\n", hiResponse);
              commandFlag = 1;
            }

            //wait for NEWPASS
            memset(input, 0, MAX_INPUT);
            recvData = recv(client, input, MAX_INPUT, 0);
            if (recvData > 0) {
              if (verboseFlag) {
                sfwrite(stdoutLock, stdout, "Received: %s\n", input);
                commandFlag = 1;
              }
              if (checkEOM(input)) {
                if (strncmp(input, "NEWPASS ", 8) == 0) {
                  strncpy(newPassword, &input[8], strlen(input) - 8);
                  if (verifyPass(newPassword)) {

                    //send SSAPWEN
                    char * ssapwenMessage = "SSAPWEN \r\n\r\n";
                    send(client, ssapwenMessage, strlen(ssapwenMessage), 0);
                    if (verboseFlag) {
                      sfwrite(stdoutLock, stdout, "Sent: %s\n", ssapwenMessage);
                      commandFlag = 1;
                    }

                    //send HI
                    char hiMessage[200] = {0};
                    strcpy(hiMessage, "HI ");
                    strcat(hiMessage, name);
                    strcat(hiMessage, " \r\n\r\n");
                    send(client, hiMessage, strlen(hiMessage), 0);
                    if (verboseFlag) {
                      sfwrite(stdoutLock, stdout, "Sent: %s\n", hiMessage);
                      commandFlag = 1;
                    }

                    //send MOTD
                    char motdMessage[400] = {0};
                    strcpy(motdMessage, "MOTD ");
                    strcat(motdMessage, motd);
                    strcat(motdMessage, " \r\n\r\n");
                    send(client, motdMessage, strlen(motdMessage), 0);
                    if (verboseFlag) {
                      sfwrite(stdoutLock, stdout, "Sent: %s\n", motdMessage);
                      commandFlag = 1;
                    }
                    //finally set the addClient as wellas addNew (since new account) flag to 1
                    addClient = 1;
                    addNew = 1;
                  }
                  else {
                    //bad password, send ERR 02
                    send(client, "ERR 02 BAD PASSWORD \r\n\r\n", strlen("ERR 02 BAD PASSWORD \r\n\r\n"), 0);
                    if (verboseFlag) {
                      sfwrite(stdoutLock, stdout, "Sent: ERR 02 BAD PASSWORD \r\n\r\n\n");
                      commandFlag = 1;
                    }
                    send(client, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
                    if (verboseFlag) {
                      sfwrite(stdoutLock, stdout, "Sent: BYE \r\n\r\n\n");
                      commandFlag = 1;
                    }
                    close(client);
                  }
                }
                //incorrect protocol
                else {
                  send(client, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
                  if (verboseFlag) {
                    sfwrite(stdoutLock, stdout, "Sent: BYE \r\n\r\n\n");
                    commandFlag = 1;
                  }
                }
              }
              //incorrect protocol
              else {
                send(client, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
                if (verboseFlag) {
                  sfwrite(stdoutLock, stdout, "Sent: BYE \r\n\r\n\n");
                  commandFlag = 1;
                }
              }
            }
            //client closed unexpectedly
            else {
              int PID = fork();
              if(PID == 0){
                close(commPipe[0]);
                write(commPipe[1], "a", 1);
                close(commPipe[1]);
                exit(EXIT_SUCCESS);
              }
              waitpid(PID, NULL, 0);
              // pthread_cancel(cid);
              // pthread_create(&cid, NULL, communicationThread, &cThread);
              // pthread_detach(cid);
            }
          }
          //if name already exists, then send ERR 00
          else {
            send(client, "ERR 00 USER NAME TAKEN \r\n\r\n", strlen("ERR 00 USER NAME TAKEN \r\n\r\n"), 0);
            if (verboseFlag) {
              sfwrite(stdoutLock, stdout, "Sent: ERR 00 USER NAME TAKEN \r\n\r\n\n");
              commandFlag = 1;
            }
            send(client, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
            if (verboseFlag) {
              sfwrite(stdoutLock, stdout, "Sent: BYE \r\n\r\n\n");
              commandFlag = 1;
            }
            close(client);
          }
        }
        //incorrect protocol
        else {
          send(client, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
          if (verboseFlag) {
            sfwrite(stdoutLock, stdout, "Sent: BYE \r\n\r\n\n");
            commandFlag = 1;
          }
        }
      }
      //add to accounts list if applicable
      if (addNew) {
        //acquire mutex
        pthread_mutex_lock(&accountsLock);

        user_account * newAccount = malloc(sizeof(user_account));
        memset(newAccount, 0, sizeof(user_account));
        newAccount->username = malloc(strlen(name) + 1);
        strcpy(newAccount->username, name);
        newAccount->salt = malloc(SALT_LENGTH);
        newAccount->hash = malloc(SHA256_DIGEST_LENGTH);
        //generate the salt for the new user
        unsigned char newSalt[SALT_LENGTH];
        RAND_bytes(newSalt, SALT_LENGTH);
        memcpy(newAccount->salt, newSalt, SALT_LENGTH);
        //generate the hash for the new user
        unsigned char newHash[SHA256_DIGEST_LENGTH];
        SHA256_CTX context;
        SHA256_Init(&context);
        SHA256_Update(&context, (unsigned char * ) newPassword, strlen(newPassword));
        SHA256_Update(&context, newSalt, SALT_LENGTH);
        SHA256_Final(newHash, &context);
        memcpy(newAccount->hash, newHash, SHA256_DIGEST_LENGTH);

        newAccount->next = NULL;
        if (account_head == NULL) {
          account_head = newAccount;
        }
        else {
          user_account * iterator = account_head;
          while (iterator->next != NULL) {
            iterator = iterator->next;
          }
          iterator->next = newAccount;
        }

        //release mutex
        pthread_mutex_unlock(&accountsLock);
      }
      //add to client list if applicable
      if (addClient) {
        //acquire mutex
        pthread_mutex_lock(&usersLock);
        //since checkAvailability already entered a record, all we need to do here is give the socket
        //the record checkAvailabilty entered is at the end of the list
        connected_user * temp = list_head;
        while (temp->next != NULL) {
          temp = temp->next;
        }
        temp->socket = client;
        //now add the user and his/her information to the list
        /*connected_user * currentlyConnected = malloc(sizeof(connected_user));
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
        }*/
        //then run the communication thread
        if ((!cThread) && (list_head != NULL)) {
          pthread_create(&cid, NULL, communicationThread, &cThread);
        }
        //if it already exists, need to write to the pipe so the communication thread knows to update accordingly
        else {
          int PID = fork();
          if (PID == 0) {
            close(commPipe[0]);
            write(commPipe[1], "a", 1);
            close(commPipe[1]);
            exit(EXIT_SUCCESS);
          }
          waitpid(PID, NULL, 0);
        }
        //release mutex
        pthread_mutex_unlock(&usersLock);
      }
      //if failed, then that means authentication failed. need to remove the last record in the active users list
      else {
        connected_user * temp = list_head;
        if ((temp->prev == NULL) && (temp->next == NULL)) {
          list_head = NULL;
          free(temp);
        }
        else {
          while (temp->next != NULL) {
            temp = temp->next;
          }
          temp->prev->next = NULL;
          free(temp);
        }
      }
    }
    //client closed unexpectedly
    else {
      close(client);
    }
    //if (quit_signal) {
//      pthread_mutex_unlock(&accountsLock);
      //pthread_mutex_unlock(&usersLock);
      //return NULL;
    //}
    //release mutexes
    //pthread_mutex_unlock(&accountsLock);
    //pthread_mutex_unlock(&usersLock);
  }
  //continue;
}

//communication thread. the pointer passed to it is the cThread flag, indicating if it's already running
void * communicationThread(void * param) {
  //set cThread to 1 to prevent multiple communicationThreads from spawning
  *((int *) param) = 1;
  //since the head of the connected_user list is global, it doesn't need to be passed
  connected_user * iterator;
  fd_set clientList, zeroedList;
  FD_ZERO(&zeroedList);
  FD_SET(commPipe[0], &zeroedList);
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
      sfwrite(stdoutLock, stdout, "Select failed.\n");
      exit(EXIT_FAILURE);
    }
    //new connection?
    if (FD_ISSET(commPipe[0], &clientList)) {
      char stuff[3];
      read(commPipe[0], stuff, 1);
      fprintf(stderr, "%s\n", "Scooby-Doo!");
      continue;
    }

    for(iterator = list_head; iterator != NULL; iterator = iterator->next) {

      if (FD_ISSET(iterator->socket, &clientList)) {
        //acquire mutex for current users as well as disable cancelability
        pthread_mutex_lock(&usersLock);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
          //is this socket the one with input?
            char input[MAX_INPUT] = {0};
            //since already connected, just listen for communication
            int data = recv(iterator->socket, input, MAX_INPUT, 0);
            if (data > 0) {
              if (verboseFlag) {
                sfwrite(stdoutLock, stdout, "Received: ");
                sfwrite(stdoutLock, stdout, input);
                sfwrite(stdoutLock, stdout, "\n");
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
                    sfwrite(stdoutLock, stdout, "Sent to all users: %s\n", uoffMessage);
                    commandFlag = 1;
                  }
                  free(storeName);
                  //write(commPipe[1], "a", 1);
                }
                else if (strcmp(input, "TIME ") == 0) {
                  time_t now = time(NULL);
                  int difference = now - iterator->loginTime;
                  char emitMessage[50] = {0};
                  sprintf(emitMessage, "%s %d %s", "EMIT", difference, "\r\n\r\n");
                  send(iterator->socket, emitMessage, strlen(emitMessage), 0);
                  if (verboseFlag) {
                    sfwrite(stdoutLock, stdout, "Sent: %s\n", emitMessage);
                    commandFlag = 1;
                  }
                }
                else if (strcmp(input, "LISTU ") == 0) {
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
                    sfwrite(stdoutLock, stdout, "Sent: %s\n", utsilMessage);
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
                        sfwrite(stdoutLock, stdout, "Sent: ERR 01 USER NOT AVAILABLE \r\n\r\n\n");
                        commandFlag = 1;
                      }
                    }
                    else {
                      //need to add back the \r\n\r\n
                      strcat(input, "\r\n\r\n");
                      send(sender, input, strlen(input), 0);
                      if (verboseFlag) {
                        sfwrite(stdoutLock, stdout, "Sent: %s\n", input);
                        commandFlag = 1;
                      }
                      send(receiver, input, strlen(input), 0);
                      if (verboseFlag) {
                        sfwrite(stdoutLock, stdout, "Sent: %s\n", input);
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
              }
              else if (iterator->prev == NULL) {
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
                sfwrite(stdoutLock, stdout, "Sent to all users: %s\n", uoffMessage);
                commandFlag = 1;
              }
              free(storeName);
            }
            pthread_mutex_unlock(&usersLock);
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
            if (list_head == NULL) {
              *((int *) param) = 0;
              //if there's no user, then end the thread
              return NULL;
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
int verifyUser(char * user, unsigned char * pass) {
  pthread_mutex_lock(&accountsLock);
  user_account * iterator = account_head;
  while (iterator != NULL) {
    if (strcmp(iterator->username, user) == 0) {
      if (pass != NULL) {
        if (memcmp(iterator->hash, pass, SHA256_DIGEST_LENGTH) == 0) {
          pthread_mutex_unlock(&accountsLock);
          return 1;
        }
      }
      else {
        pthread_mutex_unlock(&accountsLock);
        return 1;
      }
    }
    iterator = iterator->next;
  }
  pthread_mutex_unlock(&accountsLock);
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

//the records are of the form <user> <salt><hash> (hash is immediately after salt)
int readRecord(FILE * file, char ** username, unsigned char ** salt, unsigned char ** hash) {
  int ret = 0;
  unsigned char * theSalt = malloc(SALT_LENGTH);
  unsigned char * theHash = malloc(SHA256_DIGEST_LENGTH);
  int usernameLength = 15;
  char * theUsername = malloc(usernameLength);
  char currentChar;
  int lengthCounter = 0;
  //first read the username
  while(fread(&currentChar, 1, 1, file) == 1) {
    if (currentChar == ' ') {
      break;
    }
    theUsername[lengthCounter++] = currentChar;
    if (lengthCounter == usernameLength) {
      usernameLength += 16;
      char * temp = realloc(theUsername, usernameLength);
      if (temp == NULL) {
        free(theSalt);
        free(theHash);
        free(theUsername);
        return 0;
      }
      else {
        theUsername = temp;
      }
    }
  }
  theUsername[lengthCounter++] = '\0';
  //now read the salt (fixed size)
  if (fread(theSalt, 1, SALT_LENGTH, file) == 16) {
    * salt = theSalt;
  }
  else {
    ret = 3;
  }
  //as well as the hash (fixed size)
  if (fread(theHash, 1, SHA256_DIGEST_LENGTH, file) == SHA256_DIGEST_LENGTH) {
    * hash = theHash;
  }
  else {
    ret = 3;
  }
  //since the format separates records by \n, read the \n to move the file pointer up
  int c;
  c = fgetc(file);
  if (c == '\n') {
    ret = 1;
  }
  else if (c == EOF) {
    ret = 2;
  }
  //unexpected character
  else {

  }
  //realloc the char * so it only uses just enough space
  char * temp = realloc(theUsername, strlen(theUsername) + 1);
  if (temp == NULL) {
    free(theSalt);
    free(theHash);
    free(theUsername);
    return 0;
  }
  else {
    theUsername = temp;
    * username = theUsername;
  }

  return ret;
}

void handleSigInt(int sig) {
  pthread_mutex_lock(&queueLock);
  pthread_mutex_lock(&accountsLock);
  pthread_mutex_lock(&usersLock);
  char * caughtSigInt = "\nCaught SIGINT. Quitting...\n";
  sfwrite(stdoutLock, stdout, caughtSigInt);
  //ctrl c should function similarly to /shutdown with some additions
  connected_user * iterator = list_head;
  while (iterator != NULL) {
    connected_user * temp = iterator;
    iterator = iterator->next;
    //send bye to all users
    send(temp->socket, "BYE \r\n\r\n", strlen("BYE \r\n\r\n"), 0);
    //then close sockets and free memory
    close(temp->socket);
    free(temp->username);
    free(temp);
  }
  //the server should save the user accounts in case someone created a new account
  user_account * iterator2 = account_head;
  FILE * writeToFile;
  if ((optind + 2) != args) {
    writeToFile = fopen(args2[optind + 2], "wb");
  }
  else {
    writeToFile = fopen("accts.txt", "wb");
  }
  while (iterator2 != NULL) {
    fprintf(writeToFile, "%s", iterator2->username);
    fprintf(writeToFile, " ");
    fwrite(iterator2->salt, 16, 1, writeToFile);
    fwrite(iterator2->hash, 32, 1, writeToFile);
    fprintf(writeToFile, "\n");
    user_account * temp = iterator2;
    iterator2 = iterator2->next;
    free(temp->username);
    free(temp->salt);
    free(temp->hash);
    free(temp);
  }
  //free the login requests if any exist
  next_request * iterator3 = queue_head;
  while (iterator3 != NULL) {
    next_request * temp = iterator3;
    iterator3 = iterator3->next;
    free(temp);
  }
  fclose(writeToFile);
  if (verboseFlag) {
    sfwrite(stdoutLock, stdout, "Sent to all users: BYE \r\n\r\n\n");
    commandFlag = 1;
  }
  close(serverSocket);
  //finally, need to kill the communication thread if it exists
  pthread_cancel(cid);
  free(stdoutLock);
  free(loginArray);
  //quit_signal = 1;
  exit(EXIT_SUCCESS);
}
