#include "server.h"
#include <ncurses.h>

#define RED "\x1B[31m"
#define BLUE "\x1B[1;36m"
#define NORMAL "\x1B[0m"
#define BG "\033[44m\033[31m"
#define NORMALBG "\033[44m\033[0m"

int main(int argc, char *argv[]){
	fprintf(stdout, "%s blarg blarg blarg %s\n", RED, NORMAL);

	fprintf(stdout, "%d\n", rand() % 50);
	
	// fd_set set, readSet;
	// FD_ZERO(&set);
	// FD_SET(0, &set);

	// while(1){
	// 	readSet = set;
	// 	if(select(FD_SETSIZE, &readSet, NULL, NULL, NULL) == -1){
	// 		fprintf(stderr, "%s\n", "Error with select!");
	// 		exit(EXIT_FAILURE);
	// 	}
	// 	else{
	// 		memset(buffer, 0, MAX_INPUT);
	// 		if(FD_ISSET(0, &readSet)){
	// 			refresh();
	// 			scanw
	// 			printw("From STDIN: ");
	// 			printw(buffer);
	// 			refresh();
	// 		}
	// 	}
	// }
	

	return 0;

}
