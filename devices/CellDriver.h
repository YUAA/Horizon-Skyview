#include "Uart.h"
#include <String>
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



    
    // Sets up this cell driver to communicate with the physical cellular module through the given serial/uart device.
    CellDriver(Uart* uart);
    
    // Does incremental work on sending or receiving text messages.
    // This function should be called periodically.
    // Returns true if a new text message has just been made avaialable.
    int8_t  update();
    
    // Starts the process of sending a text message to the cellular module.
    // This function should be asynchronous (that, is, it does not wait on the module's response.)
    void queueTextMessage(const char* recipientPhoneNumber, const char* textMessage);

    //Sends command to retrieve all messages from cell shield; returns "ok" if there are no
    //messages to the commandQueue
    void CellDriver::retrieveTextMessage();
    
    //Sends command to delete all read messages to the commandQueue
    void CellDriver::deleteReadMessage();

    // Copies up to (bufferSize - 1) bytes of the last received text message to destination,
    // guaranteeing that the destination string is null terminated.
    void CellDriver::getTextMessage(char* destination, size_t bufferSize) const;

   //Sends the command to get information about the nearby cell
   //towers that it is close to the cell shield 
   //Response format
    //+CNCI: Index of Cell, BCCH, BSIC, LAC, Rxlev, Cell ID, MCC, MNC
    void queuePositionFix();
        

    // And whatever other relevant codes exist for triangulating location or something like that.

	private:

    Uart* uart;

    bool hasConfirmedAT;
    bool checkingForNewMessage;
    bool readyToSendMessage; //ready to send a new text message
    bool isWaitingForOk;

    std::deque<std::stringstream> commandQueue;
    std::queue<textMessage> messageQueue;

    string responseBuffer;
    string towerInfoList;
    int8_t numTowers;
    int8_t parseResponse;
   int8_t CellDriver::parser(string responseBuffer);


	//for text messages
	string messageType, number, name, time, messageData;



};

class textMessage()
{
	explicit textMessage( string messageType, string number, string unknown, string time, string messageData) : messageType (messageType), number(number), name(name), time(time), messageData (messageData) {}

private:
	string messageType, number, name, time, messageData;
}

#endif


