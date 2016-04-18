#include "chat.h"

int main(int argc, char *argv[]){
	memset(buffer, 0, MAX_INPUT);
	int fdWrite = atoi(argv[1]);
	int fdRead = atoi(argv[2]);

	fd_set set, readSet;

	FD_ZERO(&set);
	FD_SET(fdRead, &set);
	FD_SET(0, &set);

	while(1){
		readSet = set;
		select(FD_SETSIZE, &readSet, NULL, NULL, NULL);
		memset(buffer, 0, MAX_INPUT);

		if(FD_ISSET(0, &set)){
			fgets(buffer, MAX_INPUT, stdin);
			removeNewline(buffer, strlen(buffer));

			if(strcmp("/close", buffer) == 0){
				close(fdRead);
				close(fdWrite);
				exit(EXIT_SUCCESS);
			}

			else{
				send(fdWrite, buffer, strlen(buffer), 0);
			}
		}

		else if(FD_ISSET(fdRead, &readSet)){
			recv(fdRead, buffer, MAX_INPUT, 0);
			removeNewline(buffer, strlen(buffer));
			fprintf(stdout, ">%s\n", buffer);
		}
	}
	return 0;
}


void removeNewline(char *string, int length){
  for(int i = 0; i < length; i++){
    if(string[i] == '\n'){
      string[i] = '\0';
    }
  }
}