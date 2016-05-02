#ifndef sf_write
#define sf_write

#include <pthread.h>
#include <stdio.h>
#include <sys/file.h>

#include <stdarg.h>


void sfwrite(pthread_mutex_t *c, FILE *stream, char *fmt, ...);

#endif