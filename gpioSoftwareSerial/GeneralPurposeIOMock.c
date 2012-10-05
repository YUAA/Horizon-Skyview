#include "GeneralPurposeIO.h"

volatile bool gpioValue;

// Requests the kernel to export the given gpio pin,
// which may already have been exported
// Returns zero on success, or non-zero on failure
int gpioOpen(const char* pin)
{
    return 0;
}

// Requests the kernel to unexport the given gpio pin
// Returns zero on success, or non-zero on failure
int gpioClose(const char* pin)
{
    return 0;
}

// Sets the given pin to input mode.
// This will fail if the kernel does not support
// changing the direction of the given pin
// Returns zero on success, or non-zero on failure
int gpioSetInput(const char* pin)
{
    return 0;
}

// Sets the given pin to output mode with an initial value of high
// This will fail if the kernel does not support
// changing the direction of the given pin
// Returns zero on success, or non-zero on failure
int gpioSetOutputHigh(const char* pin)
{
    gpioValue = true;
    return 0;
}

// Sets the given pin to output mode with an initial value of high
// This will fail if the kernel does not support
// changing the direction of the given pin
// Returns zero on success, or non-zero on failure
int gpioSetOutputLow(const char* pin)
{
    gpioValue = false;
    return 0;
}

// Writes a new output value to the given pin
// Returns zero on success, or non-zero on failure
int gpioWrite(const char* pin, bool value)
{
    //printf("%d", value);
    gpioValue = value;
    return 0;
}

// Reads a new input value from the given pin
// Returns zero on success, or non-zero on failure
int gpioRead(const char* pin, bool* value)
{
    //printf("%d", gpioValue);
    *value = gpioValue;
    return 0;
}