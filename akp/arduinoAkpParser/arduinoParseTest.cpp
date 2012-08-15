#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include "arduinoAkpParser.h"

void reparseData(char* data, int length)
{
    TagParseData tpData = {};
    for (int i = 0; i < length; i++)
    {
        if (parseTag(data[i], &tpData))
        {
            if (strcmp(tpData.tag, "DD") == 0)
            {
                reparseData(tpData.data, 0);
            }
            else
            {
                printf("%s%s\n", tpData.tag, tpData.data);
            }
        }
    }
}

int main(int argc, char* argv[])
{
    //Unbuffered output, so the file can be read in as streamed.
    setvbuf(stdout, NULL, _IONBF, 0);
    
    //Don't wait for newline to get stdin input
    struct termios terminal_settings;
    if (tcgetattr(0, &terminal_settings) < 0)
    {
        perror("Error getting terminal settings");
    }
    
    // disable canonical mode processing in the line discipline driver
    // So everything is read in instantly from stdin!
    terminal_settings.c_lflag &= ~ICANON;
    
    if (tcsetattr(0, TCSANOW, &terminal_settings) < 0)
    {
        perror("Error setting terminal settings");
    }
    
    TagParseData tpData = {};
    int nextByte;
    while ((nextByte = getchar()) != -1)
    {
        if (parseTag((char)nextByte, &tpData))
        {
            if (strcmp(tpData.tag, "DD") == 0)
            {
                reparseData(tpData.data, 0);
            }
            else
            {
                printf("%s%s\n", tpData.tag, tpData.data);
            }
        }
    }
    return 0;
}
