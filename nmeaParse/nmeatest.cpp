#include "nmeaparse.h"
#include <stdio.h>

int main()
{
    NmeaData nmea;
    
    // These are where we want to get our output
    char data1[10];
    char data2[10];
    
    // This is how we initialize the nmea structure
    // The type casts are a little convoluted, but they let the compiler know what to expect,
    // and are thus required to not get a compiler error. 
    initNmea(&nmea, "GPGGA,", 2, (const int[]) {0, 3}, (char*[]) {data1, data2});
    
    // This is just testing data we are using with a loop. 
    const char input[] = "$GPGGA,121505,4807.038,N,01131.324,E,1,08,0.9,133.4,M,46.9,M,,*48";
    for (int i = 0; i < sizeof(input); i++)
    {
        // This is what we will really do in general: pass on an additional character,
        // and if we return true, that means we are done and can get our data.
        if (parseNmea(&nmea, input[i]))
        {
            printf("We parsed out %s and %s.\n", data1, data2);
        }
    }
}