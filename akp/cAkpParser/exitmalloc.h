#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#ifndef EXIT_MALLOC_H
#define EXIT_MALLOC_H

//A malloc that prints out an error message to stderr
//on error and exits.
void* exitmalloc(size_t bytes);

//A realloc that prints out an error message to stderr
//on error and exits.
void* exitrealloc(void* ptr, size_t bytes);

#endif