#include "client.h"

int main(int argc, char *argv[]){
	time_t rawtime;
	struct tm *info;
	char *s = malloc(18);
	memset(s, 0, 18);
	
	time(&rawtime);
	info = localtime(&rawtime);

	strftime(s, 18, "%m-%d-%y-%l:%M%p", info);

	fprintf(stderr, "%s\n", s);

	return 0;

}
