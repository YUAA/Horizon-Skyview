#include "Uart.h"
#include <string>
#include <deque>
#include <queue>

#ifndef CELL_DRIVER
#define CELL_DRIVER

class TextMessage
{
public:
	explicit TextMessage( std::string messageType, std::string number, std::string unknown, std::string time, std::string messageData) : messageType (messageType), number(number), name(name), time(time), messageData (messageData) {}

//private:
	std::string messageType, number, name, time, messageData;
};


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
    void queueTextMessage(const char* recipientPhoneNumber, const char* TextMessage);

    //Sends command to retrieve all messages from cell shield; returns "ok" if there are no
    //messages to the commandQueue
    void retrieveTextMessage();
    
    //Sends command to delete all read messages to the commandQueue
    void deleteReadMessage();

    // returns the most recent message in the queue
    TextMessage getTextMessage();

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
    bool aboutToReceiveMessage;
    bool updatedTowers;

    std::deque<std::string> commandQueue;
    std::queue<TextMessage> messageQueue;

    std::string responseBuffer;
    std::string towerInfoList;
    int8_t numTowers;
    int8_t parseResponse;
    int8_t parse(std::string responseBuffer);


	//for text messages
	std::string messageType, number, name, time, messageData;



};


#endif


