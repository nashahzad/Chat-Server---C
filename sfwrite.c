#include "sfwrite.h"


void sfwrite(pthread_mutex_t *lock, FILE *stream, char *fmt, ...){
	va_list list;
	va_start(list, fmt);

	//LOCK IT FIRST THEN WRITE TO FILE
	pthread_mutex_lock(lock);
	flock(fileno(stream), LOCK_SH);
	
	
		vfprintf(stream, fmt, list);
		fflush(stream);
	
	//THEN IMMEDIATELY UNLOCK THE LOCK AFTER WRITING TO FILE
	pthread_mutex_unlock(lock);
	flock(fileno(stream), LOCK_UN);
	
}
