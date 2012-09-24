#include "arduinoAkpParser.h"

#include <Streaming.h>
#include <SoftwareSerial.h>

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

// Text message signal to KILL
#define KILL_TEXT "kill"
// Text message signal to relay all information to subscribed numbers
#define GET_INFO_TEXT "info"
// Text message to 
#define SUBSCRIBE_TEXT "subscribe"

// Buffer size for reading lines in from the cell shield
#define BUFFER_SIZE 256

// Intervals for sending out text messages
#define NORMAL_MESSAGING_INTERVAL (10 * 60 * 1000)
#define FAST_MESSAGING_INTERVAL (2 * 60 * 1000)

// Interval for sending tag information to arduino
#define ARDUINO_SEND_INTERVAL (10 * 1000)

#define MAX_SUBSCRIBERS 6

#define BIG_ARDUINO Serial
#define CELL_SHIELD cellShield
SoftwareSerial cellShield(7, 8);

// 12033470933 is the YUAA twilio
char subscribedPhoneNumbers[MAX_SUBSCRIBERS][12] = {"14018649488", "12033470933"};
// The index of the next slot to fill with a subscriber
// This will cycle and replace older numbers if there is no space
int subscribedNumberIndex = 2;
// The number of subscribed numbers. This will max out at MAX_SUBSCRIBERS.
int subscribedNumberCount = 2;

// 3-character Mobile Country Code
char mccBuffer[4];
// 2-character Mobile Network Code
char mncBuffer[3];

// 4-character (or so?) Location Area Code
char lacBuffer[5];
// ?-character Caller IDentifier. Can be quite long? Max 20 chars...
char cidBuffer[21];

// Latitude routed from the GPS for text message relay
char latitudeBuffer[16];
// Latitude routed from the GPS for text message relay
char longitudeBuffer[16];

unsigned long millisAtLastMessageRelay;
unsigned long millisAtLastArduinoSend;

// Whether we have received a LV tag of 0 from the main controller (for lack of liveliness)
bool hasBalloonBeenKilled = false;

char getHexOfNibble(char c)
{
    c = c & 0x0f;
    if (c < 10)
    {
        return '0' + c;
    }
    else
    {
        return 'a' + c - 10;
    }
}

void sendTagCellShield(const char* tag, const char* data)
{
    unsigned char checksum = crc8(tag, 0);
    checksum = crc8(data, checksum);
    //Get hex of checksum
    char hex1 = getHexOfNibble(checksum >> 4);
    char hex2 = getHexOfNibble(checksum);

    CELL_SHIELD << tag << '^' << data << ':' << hex1 << hex2;
}

void sendTagArduino(const char* tag, const char* data)
{
    unsigned char checksum = crc8(tag, 0);
    checksum = crc8(data, checksum);
    //Get hex of checksum
    char hex1 = getHexOfNibble(checksum >> 4);
    char hex2 = getHexOfNibble(checksum);

    BIG_ARDUINO << tag << '^' << data << ':' << hex1 << hex2;
}

void sendInfoTagsToCellShield()
{
    sendTagCellShield("MC", mccBuffer);
    sendTagCellShield("MN", mncBuffer);
    sendTagCellShield("LC", lacBuffer);
    sendTagCellShield("CD", cidBuffer);
    sendTagCellShield("LA", latitudeBuffer);
    sendTagCellShield("LO", longitudeBuffer);
    sendTagCellShield("LV", hasBalloonBeenKilled ? "0" : "1");
}

void sendInfoTagsToArduino()
{
    sendTagArduino("MC", mccBuffer);
    sendTagArduino("MN", mncBuffer);
    sendTagArduino("LC", lacBuffer);
    sendTagArduino("CD", cidBuffer);
    sendTagArduino("LA", latitudeBuffer);
    sendTagArduino("LO", longitudeBuffer);
    sendTagArduino("LV", hasBalloonBeenKilled ? "0" : "1");
}

// Checks for characters from the cell shield and parses them into individual "commands"
// Returns the parsed command if it has just been finished. Returns null otherwise.
// (note that the char* points to an internal buffer and will thus always be the same value)
char* checkForCellShieldInputLine()
{
    static char commandBuffer[BUFFER_SIZE];
    static int bufferIndex = 0;
    
    while  (CELL_SHIELD.available())
    {
        int c = CELL_SHIELD.read();
        if (c != -1)
        {
            BIG_ARDUINO.write(c);
        }
        switch (c)
        {
            case -1:
                // Ain't nothing here. Probably won't happen because of available check, but hey!
                return NULL;
            case '\n':
                // Ignore the newline character, which always is paired with the carriage return
                break;
            case '\r':
                // Commands are delimited by a carriage return, so we are done.
                commandBuffer[bufferIndex] = '\0';
                bufferIndex = 0;
                return commandBuffer;
            default:
                // A normal character: we add it to the current command if we haven't run out of space
                if (bufferIndex < BUFFER_SIZE)
                {
                    commandBuffer[bufferIndex++] = c;
                }
                else
                {
                    // We have bizarely run out of buffer space! Complain!
                    BIG_ARDUINO.println("AHH! Our AT command buffer is full!");
                }
                break;
        }
    }
    // Out of bytes and no command yet
    return NULL;
}

// A blocking version of checkForCellShieldInputLine
char* cellShieldReadInputLine()
{
    char* inputLine = NULL;
    while ((inputLine = checkForCellShieldInputLine()) == NULL) {}
    return inputLine;
}

// Parses a phone number out of the given command and places it in the given buffer
// The number is found after the first " mark.
// The number, still as a string, is put into the incoming phone number variable
void parsePhoneNumber(const char* command, char* phoneNumberBuffer) {
    char *startCharacter = strchr(command, '"') + 1;
    // Take at most a full 11 characters
    strncpy(phoneNumberBuffer, startCharacter, min(11, strlen(command)));
}

// Waits until a command is received from the cell shield (anything!)
// Returns 0 for everything fine, -1 for cell shield fatal errors!
int cellShieldWaitForResponse()
{
    while (1)
    {
        char* inputLine = checkForCellShieldInputLine();
        if (inputLine)
        {
            BIG_ARDUINO << "Waiting with line " << inputLine << '\n';
            return handleCellShieldCommand(inputLine);
        }
    }
}

// Waits for a < character as the prompt for when the cell shield is ready to send a text message
// If instead, a + is found first, we abort and return -1 instead 0 as on success.
// Not receiving a < would be indicative of a failure of the cell shield.
int cellShieldWaitForConfirm() {
    while (1) {
        if (CELL_SHIELD.available() > 0) {
            char c = CELL_SHIELD.read();
            BIG_ARDUINO.write(c);
            if (c == '>')
            {
                return 0;
            }
            else if (c == '+')
            {
                // Also throw out the remainder of this line
                checkForCellShieldInputLine();
                return -1;
            }
        }
    }
}

int startTextMessage(const char* phoneNumber) {
    BIG_ARDUINO << "Starting to send a text to " << phoneNumber << "\n";
    CELL_SHIELD << "AT+CMGS=\"" << phoneNumber << "\r\n";
    return cellShieldWaitForConfirm();
}

int endTextMessage() {
    // A special stop byte
    CELL_SHIELD.write(26);
    
    if (cellShieldWaitForResponse()) return -1;
    
    BIG_ARDUINO.println("Text ended");
    return 0;
}

int sendTextMessages()
{
    for (int i = 0; i < subscribedNumberCount; i++) {
        if (startTextMessage(subscribedPhoneNumbers[i])) return -1;
        sendInfoTagsToCellShield();
        
        if (endTextMessage()) return -1;
    }
}

int sendTextMessage(const char* phoneNumber)
{
    if (startTextMessage(phoneNumber)) return -1;
    sendInfoTagsToCellShield();
    return endTextMessage();
}

// Handles a command
// Returns 0 if nothing bad has happened
// and -1 if the cell shield has errored and needs to be reset
int handleCellShieldCommand(const char* command) {
    if (strstr(command,"ERROR"))
    {
        BIG_ARDUINO.println("Cell shield error");
        return -1;
    }
    
    // Did we just get a text message?
    if (strstr(command,"+CMT"))
    {
        BIG_ARDUINO.println("Parsing a phone number");
        char incomingPhoneNumber[12];
        parsePhoneNumber(command, incomingPhoneNumber);
        
        // We want to read in another line (command)
        // This line is the contents of the text message itself
        command = cellShieldReadInputLine();
        
        if (strstr(command, KILL_TEXT))
        {
            // Send the kill tag to the arduino
            sendTagArduino("KL", "");
            // We will inform people about the killing once it is successful!
        }
        if (strstr(command, GET_INFO_TEXT))
        {
            BIG_ARDUINO << "Sending out text message response to " << incomingPhoneNumber << '\n';
            if (sendTextMessage(incomingPhoneNumber)) return -1;
        }
        if (strstr(command, SUBSCRIBE_TEXT))
        {
            BIG_ARDUINO << "Subscribing phone number to texts: " << incomingPhoneNumber << '\n';
            addSubscriber(incomingPhoneNumber);
        }
        
        // Delete the text message we just received
        CELL_SHIELD.println("AT+CMGD=1,4");
        if (cellShieldWaitForResponse()) return -1;
    }
    
    if (strstr(command, "+CREG"))
    {
        BIG_ARDUINO.println("Received CREG");
        storeLacAndCid(command);
    }
    
    if (strstr(command, "+COPS"))
    {
        BIG_ARDUINO.println("Received COPS");
        storeMccAndMnc(command);
    }
    
    return 0;
}

// Sends the cell shield the given command plus \r\n
// Reads in commands and allows them to be handled,
// waiting for an OK response before returning
// Times out and resends the command if no OK is received in 1 second
// Returns -1 for cell shield fatal error, 0 otherwise
int sendToCellShieldAndConfirm(const char* command)
{
    while (1)
    {
        unsigned long startTime = millis();
        CELL_SHIELD.println(command);
        
        while (startTime + 1000 > millis())
        {
            const char* inputLine = checkForCellShieldInputLine();
            if (inputLine)
            {
                if (strstr(inputLine, "OK"))
                {
                    return 0;
                }
                else
                {
                    if (handleCellShieldCommand(inputLine)) return -1;
                }
            }
        }
    }
}

int requestAndStoreMccAndMnc() {
    while (1)
    {
        unsigned long startTime = millis();
        
        if (sendToCellShieldAndConfirm("AT+COPS=0")) return -1;
        CELL_SHIELD.println("AT+COPS?");
        
        while (startTime + 1000 > millis())
        {
            const char* inputLine = checkForCellShieldInputLine();
            if (inputLine)
            {
                if (handleCellShieldCommand(inputLine)) return -1;
                if (strstr(inputLine, "COPS"))
                {
                    return 0;
                }
            }
        }
    }
}

void storeMccAndMnc(const char* inputLine)
{
    char *firstComma = strchr(inputLine,',');
    if (firstComma)
    {
        // The codes we are looking for are located after the second comma
        char *mccLocation = strchr(firstComma + 1, ',') + 1;
        
        // Extract MCC
        memcpy(mccBuffer, mccLocation, 3);
        mccBuffer[3] = '\0';
        
        // Extract MNC
        memcpy(mncBuffer, mccLocation + 3, 2);
        mncBuffer[2] = '\0';
        
        BIG_ARDUINO << "MCC: " << mccBuffer << " MNC: " << mncBuffer << '\n';
    }
}

void storeLacAndCid(const char* command) {
    char *lacLocation = strchr(command, ',') + 1;
    
    // Extract LAC
    memcpy(lacBuffer, lacLocation, 4);
    lacBuffer[4] = '\0';
    
    // Extract CID (there is a comma between the lac and cid)
    memcpy(cidBuffer, lacLocation + 5, 4);
    cidBuffer[4] = '\0';
    
    BIG_ARDUINO << "LAC: " << lacBuffer << " CID: " << cidBuffer << '\n';
}

void addSubscriber(const char* phoneNumber)
{
    strncpy(subscribedPhoneNumbers[subscribedNumberIndex], phoneNumber, 11);
    subscribedPhoneNumbers[subscribedNumberIndex][11] = '\0';
    
    subscribedNumberIndex = (subscribedNumberIndex + 1) % MAX_SUBSCRIBERS;
    subscribedNumberCount = (subscribedNumberCount >= MAX_SUBSCRIBERS) ? MAX_SUBSCRIBERS : subscribedNumberCount + 1;
}

int sendInfoIfItsTimeTo()
{
    long unsigned delayTime = hasBalloonBeenKilled ? FAST_MESSAGING_INTERVAL : NORMAL_MESSAGING_INTERVAL;

    if (millis() - millisAtLastMessageRelay >= delayTime)
    {
        millisAtLastMessageRelay = millis();
        if (sendTextMessages()) return -1;
    }
    
    if (millis() - millisAtLastArduinoSend >= ARDUINO_SEND_INTERVAL)
    {
        millisAtLastArduinoSend = millis();
        sendInfoTagsToArduino();
    }
    
    return 0;
}

int setupSim()
{
    // Setting text mode
    BIG_ARDUINO.println("Configuring SIM");
    
    BIG_ARDUINO.println("Setting to text mode");
    if (sendToCellShieldAndConfirm("AT+CMGF=1")) return -1;
    
    // Delete all pre-existing text messages
    BIG_ARDUINO.println("Deleting old messages");
    if (sendToCellShieldAndConfirm("AT+CMGD=1,4")) return -1;
    
    // Request general carrier and country specific codes now.
    // (They really shouldn't change)
    if (requestAndStoreMccAndMnc()) return -1;
    
    // Settings for receiving text messages
    BIG_ARDUINO.println("Setting text message settings");
    if (sendToCellShieldAndConfirm("AT+CNMI=3,3,0,0")) return -1;
    
    // Request a report with the LAC and CID
    BIG_ARDUINO.println("Requesting periodic CREGs");
    if (sendToCellShieldAndConfirm("AT+CREG=2")) return -1;
    
    return 0;
}

void setup() {
    BIG_ARDUINO.begin(9600);
    CELL_SHIELD.begin(9600);
    
    BIG_ARDUINO.println("Starting init sequence");
    
    // Keep up trying to setup the cell shield as much as necessary!
    while (setupSim());
    // We call this twice to make sure all the settings are set correctly (only catostrophic errors will return -1)
    // But it is very likely that on start up the wrong order of things will cause the first message sent to be lost
    while (setupSim());
            
    millisAtLastMessageRelay = millis();
    millisAtLastArduinoSend = millis();
    
    BIG_ARDUINO.println("Init complete");
}

void checkForControllerInput()
{
    static TagParseData tpData;
    
    // Handle all the bytes that are currently available
    while (BIG_ARDUINO.available())
    {
        // Read a character from the controller and give it to the parser
        // If we have a parsed tag finished with this character, then respond accordingly
        if (parseTag(BIG_ARDUINO.read(), &tpData))
        {
            if (strncmp(tpData.tag, "LV", 2) == 0)
            {
                // We only care for the first byte on the liveliness tag, and we only care whether it is not 0
                hasBalloonBeenKilled = (tpData.data[0] == '0');
            }
            else if (strncmp(tpData.tag, "LA", 2) == 0)
            {
                strncpy(latitudeBuffer, tpData.data, 15);
                latitudeBuffer[15] = '\0';
            }
            else if (strncmp(tpData.tag, "LO", 2) == 0)
            {
                strncpy(longitudeBuffer, tpData.data, 15);
                longitudeBuffer[15] = '\0';
            }
        }
    }
}

void loop()
{
    char* inputLine = checkForCellShieldInputLine();
    if (inputLine)
    {
        if (handleCellShieldCommand(inputLine))
        {
            
            // Cell-shield has had a bad error. Try to reset it and continue.
            // If it fails... just keep on trying!
            while (setupSim());
        }
    }
    
    checkForControllerInput();
    
    if (sendInfoIfItsTimeTo())
    {
        // Cell-shield has had a bad error. Try to reset it and continue.
        // If it fails... just keep on trying!
        while (setupSim());
    }
}

