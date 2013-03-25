#include "CellDriver.h"     
#include "Uart.h"    

CellDriver::CellDriver(Uart* uart)
{
    
}

    //Send AT+CPAS command Phone activity status
    //return 0 if ready 1 if AT commands not available for other check documentation
    int readyStatus() const;
    {

        writeString("AT+CPAS\r\0");
        
        //insert some code to read the response
        //Maybe do if statement depending on the response
        
        //return number of status
     
    }

    // True if this driver is ready to send another text message.
    // This would be false if last queued message has not been sent yet.
    bool readyToSendTextMessage() const;



    
    // Starts the process of sending a text message to the cellular module.
    void queueTextMessage(const char* recipientPhoneNumber, const char* textMessage);
    {  
	//Send Message Function
	//put command and cell phone together
	//Need string.h probably
	char msg_cmd [30];
	//cmd for sending short sms
	strcpy (msg_cmd, "AT+CMGS=\"");
	//add phone number 1#######
	strcat (msg_cmd, recipientPhoneNumber);
	//add the ending part
	strcat (msg_cmd, "\"\r\x1A\0");

	//Format sms message
	strcat (textMessage, "\x1A\0")

	//Send message
	writeString(msg_cmd);
	writeString(texMessage);

    } 

    // Does incremental work on sending or receiving text messages.
    // This function should be called periodically.
    // Returns true if a new text message has just been made avaialable.
    //Maybe just read all messages and deletes them read or unread at the end
    bool update();    
    {

    }     
    
    // Copies up to (bufferSize - 1) bytes of the last received text message to destination,
    // guaranteeing that the destination string is null terminated.
    void getTextMessage(char* destination, size_t bufferSize) const;
    {
	//Read all messages if returns "ok" then no messages
	writeString("AT+CMGL=\"ALL\"\r\0");
	
    
    //
    //Reads the UART of messages here!!!!
    //
    
    
    
	//Clean out all read messages
	writeString("AT+CMGD=1,1\r\0");
	  	 	
    }
    

