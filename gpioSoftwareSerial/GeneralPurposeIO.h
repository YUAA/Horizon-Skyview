#include "nonstdio.h"
#include "formattedstring.h"

#include <stdbool.h>

#ifndef GENERAL_PURPOSE_IO
#define GENERAL_PURPOSE_IO

// Requests the kernel to export the given gpio pin,
// which may already have been exported
// Returns zero on success, or non-zero on failure
int gpioOpen(const char* pin);

// Requests the kernel to unexport the given gpio pin
// Returns zero on success, or non-zero on failure

int gpioClose(const char* pin);

// Sets the given pin to input mode.
// This will fail if the kernel does not support
// changing the direction of the given pin
// Returns zero on success, or non-zero on failure
int gpioSetInput(const char* pin);

// Sets the given pin to output mode with an initial value of high
// This will fail if the kernel does not support
// changing the direction of the given pin
// Returns zero on success, or non-zero on failure
int gpioSetOutputHigh(const char* pin);

// Sets the given pin to output mode with an initial value of high
// This will fail if the kernel does not support
// changing the direction of the given pin
// Returns zero on success, or non-zero on failure
int gpioSetOutputLow(const char* pin);

// Writes a new output value to the given pin
// Returns zero on success, or non-zero on failure
int gpioWrite(const char* pin, bool value);

// Reads a new input value from the given pin
// Returns zero on success, or non-zero on failure
int gpioRead(const char* pin, bool* value);

#endif