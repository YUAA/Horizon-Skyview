#include <stdint.h>
#include <unistd.h>
#include <sstream>

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
    // Note that UART3 is write only.
    Uart(int uartNumber, int32_t baudRate);
    
    bool isReady() const;
    
    // Reads a single byte from the UART or -1 if there are none available.
    int32_t readByte();
    
    // Writes a single byte to the UART
    void writeByte(uint8_t value);
    
    // Write the given null terminated string.
    void writeString(const char* str);
    
    template<class T>
    inline Uart& operator << (T val)
    {
        std::stringstream convertOutput;
        convertOutput << val;
        write(uartHandle, convertOutput.str().c_str(), convertOutput.str().length());
        return this;
    }

    private:
    
    // configures the kernel mux settings by simple writing the given settings to the given mux files.
    int configMux(const char* mux1, const char* setting1, size_t setting1Len, const char* mux2, const char* setting2, size_t setting2Len);

    // File handle to our /dev/ttyO# device
    int uartHandle;
    
    // Whether we initialized correctly, value returned by isReady
    bool isInitialized;

};

#endif
