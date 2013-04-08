#include "CellDriver.h"
#include <string.h>

CellDriver::CellDriver(Uart* uart)
{
    this->uart = uart;
    //think about size of buffer - may cut off message if more than 1 is sent
    responseBuffer.reserve(200);
    this->isWaitingForOk = false;
}

int8_t CellDriver::update()
{
    // Process information from the cell module
    for(int32_t byte = uart->readByte(); byte != -1; byte = uart->readByte())
    {
        //once it finds a new line character in the buffer then it removes the new line 
        if (byte == '\r')
        {
            //responseBuffer.pop_back();
            //Parses it line by line looking for something useful to do with string
            int8_t parseResponse = parse(responseBuffer); //does this syntax work for a string?
            //clense the string
            responseBuffer.erase();
            return parseResponse;
        }
        else if (byte != '\n')
        {
            // add byte to string if not on a end of line character.
            responseBuffer += byte;
        }
    }
    
    //We can send a new command if we are not waiting for the shield to finish an old one
    if(!this->isWaitingForOk)
    {
        //Send out the oldest element in the commandQueue, if any
        if (commandQueue.size() > 0)
        {
            this->isWaitingForOk = true;
            (*uart) << commandQueue.front();
            //After command is sent then removes it from the Queue
            commandQueue.pop_front();
        }
    }
}


//This part is responsible for adding sms message to the list of commands.
void CellDriver::queueTextMessage(const char* recipientPhoneNumber, const char* textMessage)
{
    std::stringstream totalMessage;
    totalMessage << "AT+CMGS=\"" << recipientPhoneNumber << "\"\r\x1A";
    totalMessage << textMessage << "\x1A";
    
    //Add sending sms and the message to the command queue
    commandQueue.push_back(totalMessage.str());
}


//Sends command to retrieve all messages from cell shield; returns "ok" if there are no messages
//as the messages maybe broken up to multiple ones.
void CellDriver::retrieveTextMessage()
{ 
    commandQueue.push_back("AT+CMGL=\"ALL\"\r");
}  


//Sends command to delete all read messages
void CellDriver::deleteReadMessage()
{
    commandQueue.push_front("AT+CMGD=1,1\r");   
}


TextMessage CellDriver::getTextMessage()
{
    if (messageQueue.size() > 0)
    {
        TextMessage text = messageQueue.front();
        messageQueue.pop();
        return text;
    }
    TextMessage text("", "", "", "", "");
    return text;
}


//Sends the command to get information about the nearby cell
//towers that it is close to the cell shield
//Response format
//+CNCI: Index of Cell, BCCH, BSIC, LAC, Rxlev, Cell ID, MCC, MNC 
void CellDriver::queuePositionFix()
{
    //Add get cell tower info command to end of queue
    commandQueue.push_back("AT+CNCI?");
}

//Response example for +CNCI
/*
+CNCI: 6
+CNCI: 0,240,10,1395,49,d7d4,310,0
+CNCI: 1,251,50,1395,31,d7d5,310,0
+CNCI: 2,33372,20,1395,49,d7d6,310,0
+CNCI: 3,33365,21,1395,22,c76b,310,0
+CNCI: 4,247,59,1395,22,4f76,310,0
+CNCI: 5,33364,3,1395,25,d7d8,310,0

OK
*/


// Handles a single line of input from the cell shield
int8_t CellDriver::parse(std::string inputResponse)
{
    // If this 
    if (isReceivingTextMessage)
    {
        messageData = inputResponse;
        messageQueue.push(TextMessage(messageType, number, name, time, messageData));
        isReceivingTextMessage = false;
        return true;
    }

    if (strcmp(inputResponse.substr(0,2).c_str(), "OK") == 0)
    {
        this->isWaitingForOk = false;
        return false;
    }

    // Response to a request to list all text messages
    // This details exactly one text message (there may be multiple of these responses)
    if (strcmp(inputResponse.substr(0,5).c_str(), "+CMGL") == 0)
    {
        // There is a colon immediately following the message the +CMGL, so we skip over it to the comma separated data
        std::stringstream textInfo(inputResponse.substr(6));
        getline(textInfo, messageType, ',');
        getline(textInfo, number, ',');
        getline(textInfo, name, ',');
        getline(textInfo, time, ',');
        messageData.erase();

        isReceivingTextMessage = true;
        return false;
    }

    // Response to a request for cell tower information
    if (strcmp(inputResponse.substr(0,5).c_str(), "+CNCI") == 0)
    {
        if(!isReceivingCellTowers)
        {
            isReceivingCellTowers = true;
            
            // The first +CNCI response gives us the number of towers we will get info on
            // First clear out old info
            towerInfoList.erase();
            
            // Returning 0 towers to receive on error is perfectly acceptable here...
            // And we skip the first 6 characters (+CNCI:)
            totalTowersToReceive = strtol(inputResponse.substr(6).c_str(), NULL, 10);
        } 
        else
        {
            towerInfoList.append(inputResponse);
            if (towerInfoList.size() >= totalTowersToReceive)
            {
                // We have received them all!
                isReceivingCellTowers = false;
            }
        }
        return false;
    }
    
    return false;
}
