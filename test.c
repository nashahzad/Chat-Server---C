#include "client.h"

struct test{
	int data;
	struct test *next;
};
typedef struct test test;

int main(int argc, char *argv[]){
	test *head = malloc(sizeof(test));
	head->data = 1;
	head->next = malloc(sizeof(test));
	head->next->data=2;
	head->next->next = NULL;

	test *next = head;
	head = head->next;

	
	return 0;

}
