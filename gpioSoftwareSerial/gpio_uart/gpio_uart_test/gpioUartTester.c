#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "gpio_uart.h"

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
    // Also, don't echo back characters... so we only see what we receive!
    terminal_settings.c_lflag &= ~(ICANON | ECHO);
    // We do not want to block with getchar
    // We have no minimum break in receiving characters (VTIME = 0)
    // and we have no minimum number of characters to receive (VMIN = 0)
    terminal_settings.c_cc[VTIME] = 0;
    terminal_settings.c_cc[VMIN] = 0;
    
    if (tcsetattr(0, TCSANOW, &terminal_settings) < 0)
    {
        perror("Error setting terminal settings");
    }
    
    // Open up the uart simulator
    int uart = open("/dev/gpio_uart", O_RDWR);
    if (uart == -1)
    {
        perror("Opening /dev/gpio_uart");
        // Set terminal settings back
        tcsetattr(0, TCSANOW, &oldSettings);
        return -1;
    }
    // Configure it
    if (ioctl(uart, GPIO_UART_IOC_SETBAUD, 9600))
    {
        perror("Uart setting baud");
        // Set terminal settings back
        tcsetattr(0, TCSANOW, &oldSettings);
        return -1;
    }
    if (ioctl(uart, GPIO_UART_IOC_SETRX, 8))
    {
        perror("Uart setting rx pin");
        // Set terminal settings back
        tcsetattr(0, TCSANOW, &oldSettings);
        return -1;
    }
    if (ioctl(uart, GPIO_UART_IOC_SETTX, 11))
    {
        perror("Uart setting tx pin");
        // Set terminal settings back
        tcsetattr(0, TCSANOW, &oldSettings);
        return -1;
    }
    if (ioctl(uart, GPIO_UART_IOC_START))
    {
        perror("Uart starting");
        // Set terminal settings back
        tcsetattr(0, TCSANOW, &oldSettings);
        return -1;
    }
    
    // We can only exit by Ctrl-C or escape
    while (1)
    {
        int terminalByte;
        // Do we have characters from the terminal?
        while ((terminalByte = getchar()) != -1)
        {
            // End the program on escape=27
            if (terminalByte == 27)
            {
                // Close it too!
                close(uart);
                // Set terminal settings back
                tcsetattr(0, TCSANOW, &oldSettings);
                return 0;
            }
            // Write a byte
            write(uart, &terminalByte, 1);
            //gpioUartSendByte(&uart, terminalByte);
        }
        
        unsigned char uartByte;
        // Do we have characters from the uart?
        while (read(uart, &uartByte, 1) == 1)
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

