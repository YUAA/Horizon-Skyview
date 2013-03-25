#include "GpioOutput.h"
#include <fcntl.h>
#include <unistd.h>
#include <sstream>

GpioOutput::GpioOutput(int gpioNumber)
{
    std::stringstream convertOutput;
    convertOutput << gpioNumber;
    std::string numberString = convertOutput.str();
     
    // Export the desired gpio
    // We ignore individual errors, because each file is separate,
    // so setValue may still work if these individually have problems...
    int exportFile = open("/sys/class/gpio/export", O_WRONLY);
    if (exportFile != -1)
    {
        write(exportFile, numberString.c_str(), numberString.length());
        close(exportFile);
    }
    
    convertOutput.str() = "";
    convertOutput << "/sys/class/gpio/gpio" << gpioNumber << "/direction";
    
    int directionFile = open(convertOutput.str().c_str(), O_WRONLY);
    if (directionFile != -1)
    {
        write(directionFile, "in", 2);
        close(directionFile);
    }
    
    convertOutput.str() = "";
    convertOutput << "/sys/class/gpio/gpio" << gpioNumber << "/value";
    gpioFileName = convertOutput.str();
}

int GpioOutput::setValue(int value)
{
    int gpioFile = open(gpioFileName.c_str(), O_WRONLY);
    if (gpioFile != -1)
    {
        write(gpioFile, value ? "1" : "0", 1);
        close(gpioFile);
        return 0;
    }
    return -1;
}