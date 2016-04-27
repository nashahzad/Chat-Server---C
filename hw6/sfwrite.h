#ifndef sfwrite
#define sfwrite

#include <pthread.h>
#include <stdio.h>
#include <sys/file.h>

#include <stdarg.h>


void sf_write(pthread_mutex_t *c, FILE *stream, char *fmt, ...);

#endif