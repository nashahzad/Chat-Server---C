#include "chat.h"

#define GREEN "\x1B[1;32m"
#define BLUE "\x1B[1;36m"
#define RED "\x1B[1;31m"
#define NORMAL "\x1B[0m"

int main(int argc, char *argv[]){
	memset(buffer, 0, MAX_INPUT);
	int fd = atoi(argv[1]);

	fd_set set, readSet;

	FD_ZERO(&set);
	FD_SET(fd, &set);
	FD_SET(0, &set);


	while(1){
		readSet = set;
		
		select(FD_SETSIZE, &readSet, NULL, NULL, NULL);
		
		memset(buffer, 0, MAX_INPUT);

		if(FD_ISSET(0, &readSet)){
			fgets(buffer, MAX_INPUT, stdin);

			write(0, "\033[1A", 4);
			write(0, "\033[K", 3);
			

			if(strcmp("/close", buffer) == 0){
				close(fd);
				exit(EXIT_SUCCESS);
			}

			else{
				send(fd, buffer, strlen(buffer), 0);
			}
		}

		else if(FD_ISSET(fd, &readSet)){
			recv(fd, buffer, MAX_INPUT, 0);
			removeNewline(buffer, strlen(buffer));

			if(strcmp(buffer, "UOFF") == 0){
				FD_ZERO(&set);
				FD_SET(fd, &set);
				fprintf(stdout, "%sReceive %s: User had logged off or disconnect.%s\n", RED, buffer, NORMAL);
				read(0, buffer, 1);
				exit(EXIT_SUCCESS);
			}

			else{
				if(buffer[0] == '>'){
					fprintf(stdout, "%s%s%s\n", GREEN, buffer, NORMAL);
				}
				else if(buffer[0] == '<'){
					fprintf(stdout, "%s%s%s\n", BLUE, buffer, NORMAL);
				}
				else{
					fprintf(stdout, "%s%s%s\n", RED, buffer, NORMAL);
				}
				
			}
			
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