#include "CellDriver.h"
#include <string.h>

CellDriver::CellDriver(Uart* uart)
{
    this->uart = uart;
}

bool CellDriver::readyToSendTextMessage() const
{
    writeString("AT+CPAS\r\0");
        
    //insert some code to read the response
    //Maybe do if statement depending on the response
    
    //return number of status
}

void CellDriver::queueTextMessage(const char* recipientPhoneNumber, const char* textMessage)
{
    std::stringstream totalMessage;
    totalMessage << "AT+CMGS=\"" << recipientPhoneNumber << "\"\r\x1A";
    totalMessage << textMessage << "\x1A";
    
    (*uart) << totalMessage.str();
}

bool CellDriver::update()
{
     
}

void CellDriver::getTextMessage(char* destination, size_t bufferSize) const
{
    //Read all messages if returns "ok" then no messages
    (*uart) << "AT+CMGL=\"ALL\"\r";
    
    //
    //Reads the UART of messages here!!!!
    //
    
    //Clean out all read messages
    (*uart) << "AT+CMGD=1,1\r";
    //uart->writeString("AT+CMGD=1,1\r");
}

int32_t CellDriver::getCID() const
{
    
}

int32_t CellDriver::getMCC() const
{
    
}
