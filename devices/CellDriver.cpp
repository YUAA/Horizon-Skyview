#include "CellDriver.h"
#include <string.h>

CellDriver::CellDriver(Uart* uart)
{
    this->uart = uart;
    //think about size of buffer - may cut off message if more than 1 is sent
	responseBuffer.reserve(200);
}


//the update function switches between checking if there is something in the buffer from a last
//send AT command and sending out the next AT command.
//0 is incomplete parse
int8_t = CellDriver::update()
{
	for(byte = uart->readByte(); byte != -1; byte = uart->readByte())
	{
		//once it finds a new line character in the buffer then it removes the new line 
		if (byte == ‘\r’)
{
			responseBuffer.pop_back();
			//Parses it line by line looking for something useful to do with string
			parseResponse = parse(responseBuffer); //does this syntax work for a string?
			//clense the string
responseBuffer.erase();
return(parseResponse);
		}
		//otherwise adds new byte to the string
		responseBuffer += byte;

	}
	
	//If the flag this.ready is true it means that it is ready to send the
	//cell a new command.
	if(true == this.ready)
{
	this.ready = false; 
	//Send out the older element in the commandQueue
	 (*uart) << commandQueue.front();
	//After command is sent then removes it from the Queue
	commandQueue.pop_front();
}
}


//This part is responsible for adding sms message to the list of commands.
void CellDriver::queueTextMessage(const char* recipientPhoneNumber, const char* textMessage)
{
    std::stringstream totalMessage;
    totalMessage << "AT+CMGS=\"" << recipientPhoneNumber << "\"\r\x1A";
    totalMessage << textMessage << "\x1A";
    
    //Add sending sms and the message to the command queue
    commandQueue.push_back(totalMessage);
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


//just copy the string from the internal buffer to the external buffer
void CellDriver::getTextMessage(char* destination, size_t bufferSize) const
{
  destination, &textMessageBuffer);
}


//Sends the command to get information about the nearby cell
//towers that it is close to the cell shield
//Response format
//+CNCI: Index of Cell, BCCH, BSIC, LAC, Rxlev, Cell ID, MCC, MNC 
void queuePositionFix()
{
    //Add get cell tower info command to end of queue
    commandQueue.push_back(“AT+CNCI?”);
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


//
//Beginning of the parsing function of the buffer
//
int8_t CellDriver::parse(string inputResponse)
{

//If it returns error as the response, give out -1
if (std::stcmp(inputResponse.substr(0,5), “ERROR”))
{
	//an error occurred. Are we using error handling?
//throw 1;
}

if (aboutToReceiveMessage && (std::strcmp(inputResponse.substr(0,2), “OK”) || std::stcmp(inputResponse.substr(0,5), “+CMGL”))
{
	messageQueue.push(textMessage(messageType, number, name, time, messageData))
	//reset fields?
	aboutToReceiveMessage = false;
}

//Ok response
if (std::strcmp(inputResponse.substr(0,2), “OK”))
{
	this.ready = true;
	
	aboutToReceiveMessage = false;
	
return (1);
}



if (std::stcmp(inputResponse.substr(0,5), “+CMGL”))
{

	std::stringstream textInfo(inputResponse.substr(5));
	getline(textInfo, messsageType, ‘,’);
	getline(textInfo, number, ‘,’);
	getline(textInfo, name, ‘,’);
	getline(textInfo, time, ‘,’);

	aboutToReceiveMessage = true;
	return (2);
}

if (aboutToReceiveMessage)
{
strcpy(&textMessage.messageData, &inputResponse);
return(5);
}



if (std::stcmp(inputResponse.substr(0,5), “+CNCI”))
{           //This is **WRONG**. WE MIGHT HAVE MORE THAN 9 CELL TOWERS
if(!numTowers) //&& inputResponse(9) == ‘\n’ && inputResponse(10) ==’\r’
{
	towerInfoList.erase();
numTowers = inputResponse(8); //I would also like to check that the next character is a /n/r

updatedTowers = true;
} 
else 
{
if(std::stcmp(inputResponse.substr(0,5), “+CNCI”))
{
towerInfoList.append(inputResponse);
numTowers--;
}
else
{
//error! all responses should start with +CNCI
//return(-1); 
//throw 2;
}
}
}

