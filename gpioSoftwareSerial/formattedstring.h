#include "exitmalloc.h"

#ifndef FORMATTEDSTRING_H
#define FORMATTEDSTRING_H

//Mallocs a formatted string based on printf
//Uses exitmalloc for fail-safe memory operations
char* formattedString(char* format, ...);

#endif