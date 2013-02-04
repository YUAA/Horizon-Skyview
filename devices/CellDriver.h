#include "ISerial.h"

#ifndef CELL_DRIVER
#define CELL_DRIVER

// Controls a serially connected cellular module
class CellDriver
{
	public:

	/*
	Make use of int32_t, int16_t, int8_t (32-bits, 16-bits, or 8-bits) 
	instead of int, short, or char.
	This will ensure that the length of the integer is always the same on different platforms.
	*/
    
    // Sets up this cell driver to communicate with the physical cellular module through the given serial device.
    CellDriver(ISerial serial);
    
    // True if this driver is ready to send another text message.
    // This would be false if last queued message has not been sent yet.
    bool readyToSendTextMessage() const;
    
    // Starts the process of sending a text message to the cellular module.
    // This function should be asynchronous (that, is, it does not wait on the module's response.)
    void queueTextMessage(const char* recipientPhoneNumber, const char* textMessage);
    
    // Does incremental work on sending or receiving text messages.
    // This function should be called periodically.
    // Returns true if a new text message has just been made avaialable.
    bool update();
    
    // Copies up to (bufferSize - 1) bytes of the last received text message to destination,
    // guaranteeing that the destination string is null terminated.
    void getTextMessage(char* destination, size_t bufferSize) const;
    
    // Gets the latest value from the module.
    int32_t getCID() const;
    
    // Gets the latest value from the module.
    int32_t getMCC() const;
    
    // And whatever other relevant codes exist for triangulating location or something like that.

	private:

	// Your code here

};

#endif
