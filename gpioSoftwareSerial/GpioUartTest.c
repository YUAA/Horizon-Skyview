#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <time.h>

#include "GpioUart.h"

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
    
    // Save old temrinal settings
    struct termios oldSettings = terminal_settings;
    
    // disable canonical mode processing in the line discipline driver
    // So everything is read in instantly from stdin!
    terminal_settings.c_lflag &= ~ICANON;
    // We do not want to block with getchar
    // We have no minimum break in receiving characters (VTIME = 0)
    // and we have no minimum number of characters to receive (VMIN = 0)
    terminal_settings.c_cc[VTIME] = 0;
    terminal_settings.c_cc[VMIN] = 0;
    
    if (tcsetattr(0, TCSANOW, &terminal_settings) < 0)
    {
        perror("Error setting terminal settings");
    }
    
    GpioUart uart;
    gpioUartStart(&uart, "1", "2", 60);
    
    // We can only exit by Ctrl-C
    while (true)
    {
        int terminalByte;
        // Do we have characters from the terminal?
        while ((terminalByte = getchar()) != -1)
        {
            // End the program on escape=27
            if (terminalByte == 27)
            {
                // Set terminal settings back
                tcsetattr(0, TCSANOW, &oldSettings);
                return 0;
            }
            gpioUartSendByte(&uart, terminalByte);
        }
        
        int uartByte;
        // Do we have characters from the uart?
        while ((uartByte = gpioUartReceiveByte(&uart)) != -1)
        {
            //printf("%c=%d", uartByte, uartByte);
            putchar(uartByte);
        }
        
        // Give them some time to rest...
        struct timespec sleepTime;
        sleepTime.tv_sec = 0;
        sleepTime.tv_nsec = 1000000; // a millisecond
        nanosleep(&sleepTime, NULL);
    }
    return 0;
}