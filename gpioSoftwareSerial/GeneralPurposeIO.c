#include "GeneralPurposeIO.h"

// Requests the kernel to export the given gpio pin,
// which may already have been exported
// Returns zero on success, or non-zero on failure
int gpioOpen(const char* pin)
{
    return openWriteClose("/sys/class/gpio/export", pin);
}

// Requests the kernel to unexport the given gpio pin
// Returns zero on success, or non-zero on failure
int gpioClose(const char* pin)
{
    return openWriteClose("/sys/class/gpio/unexport", pin);
}

// Sets the given pin to input mode.
// This will fail if the kernel does not support
// changing the direction of the given pin
// Returns zero on success, or non-zero on failure
int gpioSetInput(const char* pin)
{
    char* string = formattedString("/sys/class/gpio/gpio%s/direction", pin);
    int result = openWriteClose(string, "in");
    free(string);
    return result;
}

// Sets the given pin to output mode with an initial value of high
// This will fail if the kernel does not support
// changing the direction of the given pin
// Returns zero on success, or non-zero on failure
int gpioSetOutputHigh(const char* pin){
    char* string = formattedString("/sys/class/gpio/gpio%s/direction", pin);
    int result = openWriteClose(string, "high");
    free(string);
    return result;
}

// Sets the given pin to output mode with an initial value of high
// This will fail if the kernel does not support
// changing the direction of the given pin
// Returns zero on success, or non-zero on failure
int gpioSetOutputLow(const char* pin)
{
    char* string = formattedString("/sys/class/gpio/gpio%s/direction", pin);
    int result = openWriteClose(string, "low");
    free(string);
    return result;
}

// Writes a new output value to the given pin
// Returns zero on success, or non-zero on failure
int gpioWrite(const char* pin, bool value)
{
    char* string = formattedString("/sys/class/gpio/gpio%s/value", pin);
    int result = openWriteClose(string, value ? "1" : "0");
    free(string);
    return result;
}

// Reads a new input value from the given pin
// Returns zero on success, or non-zero on failure
int gpioRead(const char* pin, bool* value)
{
    char readCharacter;
    
    char* string = formattedString("/sys/class/gpio/gpio%s/value", pin);
    int result = openReadClose(string, &readCharacter, sizeof(readCharacter));
    free(string);
    
    return readCharacter == '1';
}