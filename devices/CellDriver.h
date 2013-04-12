#include "Uart.h"
#include <string>
#include <deque>
#include <queue>
#include <sys/time.h>

#ifndef CELL_DRIVER
#define CELL_DRIVER

class TextMessage
{
public:
	explicit TextMessage( std::string index, std::string messageType, std::string number, std::string name, std::string time, std::string messageData) : index (index), messageType (messageType), number(number), name(name), time(time), messageData (messageData) {}

//private:
	std::string index, messageType, number, name, time, messageData;
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
    int8_t update();
    
    // Starts the process of sending a text message to the cellular module.
    // This function should be asynchronous (that, is, it does not wait on the module's response.)
    void queueTextMessage(const char* recipientPhoneNumber, const char* TextMessage);
    
    //Sends command to delete the given text message from the module's internal memory
    void deleteMessage(TextMessage textMessage);

    // Pops a text message from the internal queue and returns it.
    // If there are no messages to return, it returns a TextMessage with all "" fields
    TextMessage getTextMessage();
        
    // And whatever other relevant codes exist for triangulating location or something like that.

    bool shouldEchoUartToStdout;

    private:
    
    Uart* uart;

    //bool hasConfirmedAT;
    bool checkingForNewMessage;
    bool isWaitingForOk;
    bool isReceivingTextMessage;
    bool isReceivingCellTowers;

    bool hasConfirmedInit;
    
    bool isWaitingForPrompt;
    
    // So we can meter making requests for cell tower information
    uint64_t lastCellTowerTime;
    
    // Metering requests for received texts
    uint64_t lastRetrieveMessagesTime;
    
    // So we can time out if the module is not responding to us, but we are waiting on an OK/prompt
    uint64_t lastWaitingForOKTime;  

    std::deque<std::string> commandQueue;
    std::queue<TextMessage> messageQueue;

    std::string responseBuffer;
    std::string towerInfoList;
    int totalTowersToReceive;
    
    // Temporary storage of text message data
    std::string index, messageType, number, name, time, messageData;
    
    void setupCellModule();
    int8_t parse(std::string responseBuffer);
    
    //Sends the command to get information about the nearby cell
    //towers that it is close to the cell shield 
    //Response format
    //+CNCI: Index of Cell, BCCH, BSIC, LAC, Rxlev, Cell ID, MCC, MNC
    void queuePositionFix();

    //Sends command to retrieve all messages from cell shield; returns "ok" if there are no
    //messages to the commandQueue
    void retrieveTextMessages();
    
	
};


#endif


