#include "formattedstring.h"

//Mallocs a formatted string based on printf
//Uses exitmalloc for fail-safe memory operations
char* formattedString(char* format, ...)
{
    va_list args;
    va_start(args, format);
    //Include null byte in length
    int length = 1 + vsnprintf(NULL, 0, format, args);
    va_end(args);
    
    char* formatted = exitmalloc(sizeof(char) * length);
    va_start(args, format);
    vsnprintf(formatted, length, format, args);
    va_end(args);
    
    return formatted;
}