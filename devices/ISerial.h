#include "CppInterfaces.h"

DeclareInterface(ISerial)
    // Writes a single byte to this serial byte stream.
    virtual bool writeByte(int8_t value) = 0;
    
    // Reads a single byte from this serial byte stream,
    // returning -1 if there is no further data available.
    virtual int32_t readByte() = 0;
EndInterface