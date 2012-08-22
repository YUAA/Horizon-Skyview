#include "nonstdio.h"

//Writes to the given file handle like fprintf.
//Limited by buffer to 512 byte maximum output
//Returns number of bytes truncated/unable to write
int writef(int fileHandle, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int bytesTruncated = vwritef(fileHandle, format, args);
    va_end(args);
    return bytesTruncated;
}

//Writes to the given file handle like vfprintf.
//Limited by buffer to 512 byte maximum output
//Returns number of bytes truncated/unable to write
int vwritef(int fileHandle, const char* format, va_list args)
{
    // 513 bytes with one extra for the null byte
    char output[513];
    int bytesToBeWritten = vsnprintf(output, 513, format, args);
    
    // For safety
    output[512] = '\0';
    
    int bytesTruncated = 0;
    if (bytesToBeWritten > 512)
    {
        bytesTruncated = bytesToBeWritten - 512;
    }
    write(fileHandle, output, strlen(output));
    return bytesTruncated;
}

//Opens the path with open, writes to it, as printf, and closes.
//Returns zero on success, or non-zero on failure.
//Error messages are written to stderr
//Limited to writing only 512 bytes or less at a time.
int openWriteClose(const char* path, const char* format, ...)
{
    //Open it up
    int file = open(path, O_WRONLY);
    if (file == -1)
    {
        fprintf(stderr, "NonStdio: %s: %s\n", path, strerror(errno));
        return -3;
    }

    va_list args;
    va_start(args, format);
    int bytesTruncated = vwritef(file, format, args);
    va_end(args);

    if (bytesTruncated > 0)
    {
        fprintf(stderr, "NonStdio: Badly formatted string being written to %s, too long.\n", path);
        //Close file too, we already have an error, so don't worry about one here.
        close(file);
        return -2;//bad formatted string, not all written
    }

    if (close(file) == -1)
    {
        fprintf(stderr, "NonStdio: %s: %s\n", path, strerror(errno));
        return -1;
    }
    return 0;
}

//Opens the path with open, reads from it, and closes.
//Returns the number of bytes read, -1 for errors, and 0 for EOF.
//Error messages are written to stderr
int openReadClose(const char* path, void* buffer, size_t numberBytes)
{
    //Open it up
    int file = open(path, O_RDONLY);
    if (file == -1)
    {
        fprintf(stderr, "NonStdio: %s: %s\n", path, strerror(errno));
        return -1;
    }

    int result = read(file, buffer, numberBytes);

    if (close(file) == -1)
    {
        fprintf(stderr, "NonStdio: %s: %s\n", path, strerror(errno));
        return -1;
    }
    return result;
}
