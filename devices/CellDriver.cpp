#include "CellDriver.h"
#include <string.h>
#include <iostream>
#include <algorithm>

uint64_t cellMillis()
{
    timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * 1000 + currentTime.tv_usec / 1000;
}


CellDriver::CellDriver(Uart* uart)
{
    this->uart = uart;
    //think about size of buffer - may cut off message if more than 1 is sent
    responseBuffer.reserve(200);
    this->isWaitingForOk = false;
}

void CellDriver::setupCellModule(Uart* uart)
{

(*uart) << "AT+CMGF=1\r";

}


//the update function switches between checking if there is something in the buffer from a last
//send AT command and sending out the next AT command.
//0 is incomplete parse
int8_t CellDriver::update()
{
 
    if (commandQueue.size() > 0)
    {    
    //std::cout << commandQueue.size() << std::endl;
    //std::cout << isWaitingForOk << std::endl;    
    }
    if (cellFirstTime)
    {
        cellTime = cellMillis();
        cellFirstTime = 0;
    }

    if (cellMillis() > cellTime + 10000)
    {
        retrieveTextMessage();
        cellFirstTime = 1;
        std::cout << "Calling Retreive Text Message" << std::endl;
        (*uart) << "AT\r";
    }

    for(int32_t byte = uart->readByte(); byte != -1; byte = uart->readByte())
    {
    //    std::cout << (char)byte;
    //once it finds a new line character in the buffer then it removes the new line 
    if (isWaitingForPrompt)
    {
        std::cout << "Waiting for Prompt" << std::endl;
    }     
    if ( aboutToReceiveMessage && byte == '\x0A')
        {
        std::cout << "Found the end of the Text Message" << std::endl;
        aboutToReceiveMessage = false;
        responseBuffer.erase();
        }

       
    if (isWaitingForPrompt && (byte == '>'))
        {
        std::cout << "Found the Prompt Signal" << std::endl;
        std::cout << commandQueue.size() << std::endl;
        std::cout << commandQueue.front() << std::endl;
        (*uart) << commandQueue.front();
        commandQueue.pop_front();  
        isWaitingForPrompt = false;
        isWaitingForOk = true;
        responseBuffer.erase();         
        }

    if (byte == '\n')
        {
            std::cout << responseBuffer << std::endl;
            //Parses it line by line looking for something useful to do with string
            parseResponse = parse(responseBuffer); //does this syntax work for a string?
            //clense the string
        responseBuffer.erase();
        //std::cout << "Response Buffer: " << responseBuffer << std::endl;
        return(parseResponse);
        }
        //otherwise adds new byte to the string
        responseBuffer += byte;
    //std::cout << responseBuffer << std::endl;
    return((int) 3);
    }
    
    //We can send a new command if we are not waiting for the  to finish an old one
    if(!this->isWaitingForOk)
    {
        //Waiting for either something to go in the buffer or read
        std::cout << "Not waiting for OK, Did not get Info" << std::endl;
   
    //Send out the oldest element in the commandQueue, if there is one!!!
    if ((commandQueue.size() > 0) && isWaitingForPrompt == false)
    {
     this->isWaitingForOk = true;
     std::cout << commandQueue.size() << std::endl;
     std::cout << commandQueue.front() << std::endl;
    
     (*uart) << commandQueue.front();
        //After command is sent then removes it from the Queue
    if (0 == strcmp(commandQueue.front().substr(0,7).c_str(),"AT+CMGS"))
    {
        isWaitingForPrompt = true;
        std::cout << "Recognized First Part of Text Message" << std::endl;
    } 
   
    commandQueue.pop_front();
    return ((int) 50);
    //}


    }
    }
    return(20);
}


//This part is responsible for adding sms message to the list of commands.
void CellDriver::queueTextMessage(const char* recipientPhoneNumber, const char* textMessage)
{
    //Needs to be split into two messages
    //Sent before prompt
    std::stringstream firstHalf;
    firstHalf << "AT+CMGS=\"" << recipientPhoneNumber << "\"\r";
    commandQueue.push_back(firstHalf.str());
    
    //Sent after prompt
    std::stringstream secondHalf;
    secondHalf << textMessage << '\x1A';
    commandQueue.push_back(secondHalf.str());
    std::cout << "Queued the Text Message" << std::endl;
}


//Sends command to retrieve all messages from cell shield; returns "ok" if there 
//are no messages
//as the messages maybe broken up to multiple ones.
void CellDriver::retrieveTextMessage()
{ 
    commandQueue.push_back("AT+CMGL=\"ALL\"\r");
}  


//Sends command to delete all read messages
void CellDriver::deleteReadMessage()
{
    commandQueue.push_front("AT+CMGD=1,1\r");   
    std::cout << commandQueue.front() << std::endl;
}


//just copy the string from the internal buffer to the external buffer
const TextMessage * CellDriver::getTextMessage() const
{
    if(messageQueue.size())
    {
    std::cout << "message queue size" << messageQueue.size() << std::endl;
        return &messageQueue.front();
    }
    return 0;
}

void CellDriver::deleteOldestMessage()
{
    messageQueue.pop();
}

//Sends the command to get information about the nearby cell
//towers that it is close to the cell shield
//Response format
//+CNCI: Index of Cell, BCCH, BSIC, LAC, Rxlev, Cell ID, MCC, MNC 
void CellDriver::queuePositionFix()
{
    //Add get cell tower info command to end of queue
    commandQueue.push_back("AT+CNCI=A");
}

//Response example for +CNCI
/*
+CNCI: 6
+CNCI: 0,240,10,1395,49,d7d4,310,0
+CNCI: 1,251,50,1395,31,d7d5,310,0
+CNCI: 2,33372,20,1395,49,d7d6,310,0
+CNCI: 3,3./3365,21,1395,22,c76b,310,0
+CNCI: 4,247,59,1395,22,4f76,310,0
+CNCI: 5,33364,3,1395,25,d7d8,310,0

OK
*/


//
//Beginning of the parsing function of the buffer
//
int8_t CellDriver::parse(std::string inputResponse)
{
    std::cout << "Parse Called" << std::endl;
    
    if (0 == strcmp(inputResponse.substr(0,5).c_str(), "ERROR"))
    {
        //an error occurred. Are we using error handling?
    return ((int) 10);
    }



    if (aboutToReceiveMessage && ((0 == 
strcmp(inputResponse.substr(0,5).c_str(), "+CMGL")) || (0 ==
strcmp(inputResponse.substr(0,2).c_str(), "OK"))))
    {
    std::cout << "About to Recieve and got +CMGL" << std::endl;
        //messageQueue.push(TextMessage(messageType, number, name, time, messageData));
        //messageQueue.push(TextMessage("","","","",""));
        //reset fields?
        aboutToReceiveMessage = false;
        return ((int) 12);
    }


/*
    if (aboutToReceiveMessage && (0 == strcmp(inputResponse.substr(0,2).c_str(), "OK")))
    {
    aboutToReceiveMessage = false;
    return ((int) 11);
    }
*/

    //std::cout << "substring:" << inputResponse.substr(0,2).c_str() << std::endl;

    //Ok response
    if (0 == strcmp(inputResponse.substr(0,2).c_str(), "OK"))
    {
        std::cout << "WE GOT AN OK!" << std::endl;
        this->isWaitingForOk = false;
    
        aboutToReceiveMessage = false;
    
    return ((int) 2);
    }



    if (0 == strcmp(inputResponse.substr(0,5).c_str(), "+CMGL"))
    {

        std::stringstream textInfo(inputResponse.substr(5));
        getline(textInfo, messageType, ',');
        getline(textInfo, number, ',');
        getline(textInfo, name, ',');
        getline(textInfo, time, ',');
        messageData.erase();

        aboutToReceiveMessage = true;
        return ((int) 11);
    }

    if (aboutToReceiveMessage)
    {
        if (0 == strcmp(inputResponse.substr(0,1).c_str(), "\x0A"))
        {    
            std::cout << "Found the End of Message" << std::endl;    
            aboutToReceiveMessage = false;
            return ((int) 14);
        }
//FIX THIS
    std::cout << "Appending Data" << std::endl;
        messageData.append(inputResponse);
        return ((int) 13);
    }



    if (0 == strcmp(inputResponse.substr(0,5).c_str(), "+CNCI"))
    {           //This is **WRONG**. WE MIGHT HAVE MORE THAN 9 CELL TOWERS
        if(!numTowers) //&& inputResponse(9) == '\n' && inputResponse(10) =='\r'
        {
            towerInfoList.erase();
            numTowers = inputResponse.substr(8,1).c_str()[0]; //I would also like to check that the next character is a /n/r

            updatedTowers = true;
        } 
        else 
        {
            if(0 == strcmp(inputResponse.substr(0,5).c_str(),"+CNCI"))
            {
                towerInfoList.append(inputResponse);
                numTowers--;
            }
            else
            {
                //error! all responses should start with +CNCI
                
            }
        }
    return ((int) 30);
    }

    std::cout << "Failed to Find Anything" << std::endl;
    return((int) 1);
}
