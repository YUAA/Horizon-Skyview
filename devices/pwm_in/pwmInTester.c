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
#include <stdio.h>
#include "pwm_in.h"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "%s: <gpio number>\n", argv[0]);
        return -1;
    }
    
    int pwmIn = open("/dev/pwm_in", O_RDWR);
    if (pwmIn == -1)
    {
        perror("Opening /dev/pwm_in\n");
        return -1;
    }
    
    // Doesn't matter if we got an error, because then SET_GPIO will fail!
    int gpioNumber = (int)strtol(argv[1], NULL, 10);

    // Configure it
    if (ioctl(pwmIn, PWM_IN_SET_GPIO, gpioNumber))
    {
        
        printf("Pwm In: setting interrupt\n");
        return -1;
    }
    
    // We can only exit by Ctrl-C or escape
    while (1)
    {
        long pulseWidthValue;
        // Do we have characters from the terminal?
        while ((pulseWidthValue = ioctl(pwmIn, PWM_IN_READ_PULSE_WIDTH)) != -1)
        {
            printf("Pulse width: %ld\n", pulseWidthValue);
        }
        
        // Give them some time to rest...
        struct timespec sleepTime;
        sleepTime.tv_sec = 0;
        sleepTime.tv_nsec = 1000000; // a millisecond
        nanosleep(&sleepTime, NULL);
    }
    return 0;
}

