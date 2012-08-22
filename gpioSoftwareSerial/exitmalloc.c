#include "exitmalloc.h"

//A malloc that prints out an error message to stderr
//on error and exits.
void* exitmalloc(size_t bytes)
{
    void* p = malloc(bytes);
    if (!p)
    {
        perror("Malloc");
        exit(EXIT_FAILURE);
    }
    return p;
}

//A realloc that prints out an error message to stderr
//on error and exits.
void* exitrealloc(void* ptr, size_t bytes)
{
    void* p = realloc(ptr, bytes);
    if (!p)
    {
        free(ptr);
        perror("Realloc");
        exit(EXIT_FAILURE);
    }
    return p;
}
