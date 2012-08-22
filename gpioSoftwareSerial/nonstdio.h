#define _GNU_SOURCE

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#ifndef NON_STD_IO
#define NON_STD_IO

//Writes to the given file handle like fprintf.
//Limited by buffer to 512 byte maximum output
//Returns number of bytes truncated/unable to write
int writef(int fileHandle, const char* format, ...);

//Writes to the given file handle like vfprintf.
//Limited by buffer to 512 byte maximum output
//Returns number of bytes truncated/unable to write
int vwritef(int fileHandle, const char* format, va_list args);

//Opens the path with open, writes to it, as printf, and closes.
//Returns zero on success, or non-zero on failure
//Limited to writing only 512 bytes or less at a time.
//Error messages are written to stderr
int openWriteClose(const char* path, const char* format, ...);

//Opens the path with open, reads from it, and closes.
//Returns the number of bytes read, -1 for errors, and 0 for EOF.
//Error messages are written to stderr
int openReadClose(const char* path, void* buffer, size_t numberBytes);

#endif