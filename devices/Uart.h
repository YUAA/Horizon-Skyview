#include <stdint.h>

#ifndef UART_WRAPPER
#define UART_WRAPPER

// A wrapper for the Linux's UART serial devices
class Uart
{
    public:

    /*
    Make use of int32_t, int16_t, int8_t (32-bits, 16-bits, or 8-bits) 
    instead of int, short, or char.
    This will ensure that the length of the integer is always the same on different platforms.
    */
    
    // Opens up the internal /dev/ttyO# device and sets it to use the given baud rate
    // Acceptable uart numbers are from 1 to 5. Different values are saturated to that range.
    // If anything goes wrong, the file handle is released and isReady() will return false.
    Uart(int uartNumber, int32_t baudRate);
    
    bool isReady() const;
    
    // Reads a single byte from the UART or -1 if there are none available.
    int32_t readByte();
    
    // Writes a single byte to the UART
    void writeByte(uint8_t value);

    private:

    // File handle to our /dev/ttyO# device
    int uartHandle;
    
    // Whether we initialized correctly, value returned by isReady
    bool isInitialized;

};

#endif
