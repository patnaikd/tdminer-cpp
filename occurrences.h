#ifndef OCCURRENCES_H_
#define OCCURRENCES_H_

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <cctype>

#include "kernel.h"

extern FILE* f_occurrences;
extern pthread_mutex_t file_lock;
extern UINT numCandidatesBuffer;

UINT getSymbol(char* sym);
void resize_buffer(UINT level);
void roll_buffer(void);
char *strstrip(char *s);

#endif //OCCURRENCES_H_


