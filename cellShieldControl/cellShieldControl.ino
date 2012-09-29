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
#define NORMAL_MESSAGING_INTERVAL (10UL * 60 * 1000)
#define FAST_MESSAGING_INTERVAL (2UL * 60 * 1000)

// Interval for sending tag information to arduino
#define ARDUINO_SEND_INTERVAL (10 * 1000)

#define MAX_SUBSCRIBERS 6

#define BIG_ARDUINO Serial
#define CELL_SHIELD cellShield
SoftwareSerial cellShield(7, 8);

// 12033470933 is the YUAA twilio
char subscribedPhoneNumbers[MAX_SUBSCRIBERS][12] = {"14018649488"};//, "12033470933"};
// The index of the next slot to fill with a subscriber
// This will cycle and replace older numbers if there is no space
int subscribedNumberIndex = 1;
// The number of subscribed numbers. This will max out at MAX_SUBSCRIBERS.
int subscribedNumberCount = 1;

// 3-character Mobile Country Code
char mccBuffer[4];
// 2-character Mobile Network Code
char mncBuffer[3];

// 4-character (or so?) Location Area Code
char lacBuffer[5];
// 4-character Caller IDentifier. Can be quite long?
char cidBuffer[5];

// Latitude routed from the GPS for text message relay
char latitudeBuffer[16];
// Latitude routed from the GPS for text message relay
char longitudeBuffer[16];

unsigned long millisAtLastMessageRelay;
unsigned long millisAtLastArduinoSend;

// Whether we have received a LV tag of 0 from the main controller (for lack of liveliness)
bool hasBalloonBeenKilled = false;
// Whether we need to send text messages at a faster rate
bool inAlertMode = false;

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
    
    // For nice observation
    BIG_ARDUINO.println();
}

// Checks to see if after this additional character is added whether we have a finished input line.
// Returns the line if that is so, null if not.
char* checkForCellShieldInputLineAddChar(char c)
{
    static char commandBuffer[BUFFER_SIZE];
    static int bufferIndex = 0;
    
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
    // Out of bytes and no command yet
    return NULL;
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
        char* result = checkForCellShieldInputLineAddChar(c);
        if (result)
        {
            return result;
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
// The number is found after the first " and the + that follow it.
// The number, still as a string, is put into the incoming phone number variable
void parsePhoneNumber(const char* command, char* phoneNumberBuffer) {
    char* startCharacter = strchr(command, '"') + 2;
    // Take at most a full 11 characters
    strncpy(phoneNumberBuffer, startCharacter, min(11, strlen(startCharacter)));
    // Make sure final null byte is placed
    phoneNumberBuffer[11] = '\0';
}

// Waits up to waitMillis time for a specific response to be contained in a line from the cell shield
// Returns 0 for receiving it fine, -1 for cell shield fatal errors, 1 for requested reattempt/timeout
int cellShieldWaitForResponse(const char* response, int waitMillis)
{
    unsigned long startTime = millis();
    
    while (startTime + waitMillis > millis())
    {
        const char* inputLine = checkForCellShieldInputLine();
        if (inputLine)
        {
            if (strstr(inputLine, response))
            {
                return 0;
            }
            else
            {
                int status = handleCellShieldCommand(inputLine);
                if (status) return status;
            }
        }
    }
    
    BIG_ARDUINO << "Response wait timeout\n";
    return 1;
}

// Waits for a < character as the prompt for when the cell shield is ready to send a text message
// Processes received lines normally in the mean time.
// Returns 0 on success, -1 for cell failure, 1 for no cell service/to retry sending the text message
int cellShieldWaitForTextReady() {
    unsigned long startTime = millis();
    
    while (startTime + 1000 > millis())
    {
        if (CELL_SHIELD.available() > 0) {
            char c = CELL_SHIELD.read();
            if (c == '>')
            {
                return 0;
            }
            else
            {
                // Keep track of the bytes anyhow and see if we get something interesting
                char* inputLine = checkForCellShieldInputLineAddChar(c);
                int result = handleCellShieldCommand(inputLine);
                // If we get an unusual result, return it!
                if (result)
                {
                    return result;
                }
            }
        }
    }
    
    // Try again?
    return 1;
}

int startTextMessage(const char* phoneNumber) {
    BIG_ARDUINO << "Starting to send a text to " << phoneNumber << "\n";
    CELL_SHIELD << "AT+CMGS=\"" << phoneNumber << "\"\r\n";
    return cellShieldWaitForTextReady();
}

int endTextMessage() {
    // A special stop byte
    CELL_SHIELD.write(26);
    
    // We wait until we have received the returned CMGS indicating the entire operation is completed
    // This may take a while!
    int status = cellShieldWaitForResponse("CMGS", 4000);
    if (status) return status;
    
    status = cellShieldWaitForResponse("OK", 1000);
    if (status) return status;
    
    BIG_ARDUINO.println("Text successfully sent!");
    
    return 0;
}

int sendTextMessageTry(const char* phoneNumber)
{
    int result = startTextMessage(phoneNumber);
    if (result) return result;
    
    BIG_ARDUINO << "Proceeding with text\n";
    
    sendInfoTagsToCellShield();
    return endTextMessage();
}

int sendTextMessage(const char* phoneNumber)
{
    int result = sendTextMessageTry(phoneNumber);
    while (result == 1)
    {
        BIG_ARDUINO << "No cell service or PUK; retrying...\n";
        delay(1000);
        result = sendTextMessageTry(phoneNumber);
    }
    return result;
}

int sendTextMessages()
{
    for (int i = 0; i < subscribedNumberCount; i++)
    {
        if (sendTextMessage(subscribedPhoneNumbers[i])) return -1;
    }
    BIG_ARDUINO << "Text messages sent to all subscribers\n";
    return 0;
}

// Handles a command
// Returns 0 if nothing bad has happened
// and -1 if the cell shield has errored and needs to be reset
// and 1 if the cell shield has no service and ought to try sending a text again.
int handleCellShieldCommand(const char* command) {
    if (strstr(command,"ERROR"))
    {
        if (strstr(command, "CME ERROR: 30"))
        {
            // Error 30 means we have no cellular service
            return 1;
        }
        else if (strstr(command, "CMS ERROR: 313"))
        {
            // Weird PUK thing.. we should try again
            return 1;
        }
        else
        {
            BIG_ARDUINO.println("Cell shield error");
            return -1;
        }
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
            // In the mean time, we do want to enter alert mode!
            inAlertMode = true;
            BIG_ARDUINO << "Entering alert mode\n";
        }
        if (strstr(command, GET_INFO_TEXT))
        {
            BIG_ARDUINO << "Sending info to " << incomingPhoneNumber << '\n';
            if (sendTextMessage(incomingPhoneNumber)) return -1;
        }
        if (strstr(command, SUBSCRIBE_TEXT))
        {
            BIG_ARDUINO << "Subscribing to texts: " << incomingPhoneNumber << '\n';
            addSubscriber(incomingPhoneNumber);
        }
        
        // Delete the text message we just received
        BIG_ARDUINO << "Deleting received text\n";
        if (sendToCellShieldAndConfirm("AT+CMGD=1,4")) return -1;
        BIG_ARDUINO << "Done\n";
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
        BIG_ARDUINO << "Sending to cell shield: " << command << "\n";
        
        while (startTime + 1000 > millis())
        {
            const char* inputLine = checkForCellShieldInputLine();
            if (inputLine)
            {
                if (strstr(inputLine, "OK"))
                {
                    // Delay a little to give it preparation before another command
                    delay(500);
                    return 0;
                }
                else
                {
                    if (handleCellShieldCommand(inputLine)) return -1;
                }
            }
        }
        BIG_ARDUINO << "Cell shield response timed out. Resending command.\n";
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
                    // Delay in prep of the next thing
                    delay(500);
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
    // We want to skip past the ,0x
    char *lacLocation = strchr(command, ',') + 3;
    
    // Extract LAC
    memcpy(lacBuffer, lacLocation, 4);
    lacBuffer[4] = '\0';
    
    // Extract CID (there is a comma between the lac and cid)
    // Also, we again skip the 0x
    memcpy(cidBuffer, lacLocation + 7, 4);
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
    long unsigned delayTime = inAlertMode ? FAST_MESSAGING_INTERVAL : NORMAL_MESSAGING_INTERVAL;
    
    if (millis() - millisAtLastArduinoSend >= ARDUINO_SEND_INTERVAL)
    {
        millisAtLastArduinoSend = millis();
        sendInfoTagsToArduino();
    }
    
    //BIG_ARDUINO << "DelayTime: " << delayTime << " lastRelay: " << millisAtLastMessageRelay << '\n';
    
    if (millis() - millisAtLastMessageRelay >= delayTime)
    {
        millisAtLastMessageRelay = millis();
        if (sendTextMessages()) return -1;
    }
    
    return 0;
}

int setupSim()
{
    // A nice little delay for when this is called repeatedly because of errors
    delay(500);
    
    // Now make sure we have parsed all incoming data and gotten it through and out of our system!
    char* inputLine;
    while (inputLine = checkForCellShieldInputLine())
    {
        handleCellShieldCommand(inputLine);
    }
    
    // Setting text mode
    BIG_ARDUINO.println("(re)Configuring SIM");
    
    BIG_ARDUINO.println("Setting to AT&T frequency band");
    if (sendToCellShieldAndConfirm("AT+SBAND=7")) return -1;
    BIG_ARDUINO.println("Done");
    
    BIG_ARDUINO.println("Setting to text mode");
    if (sendToCellShieldAndConfirm("AT+CMGF=1")) return -1;
    BIG_ARDUINO.println("Done");
    
    // Delete all pre-existing text messages
    BIG_ARDUINO.println("Deleting old messages");
    if (sendToCellShieldAndConfirm("AT+CMGD=1,4")) return -1;
    BIG_ARDUINO.println("Done");
    
    // Request general carrier and country specific codes now.
    // (They really shouldn't change)
    if (requestAndStoreMccAndMnc()) return -1;
    
    // Settings for receiving text messages
    BIG_ARDUINO.println("Setting text message settings");
    if (sendToCellShieldAndConfirm("AT+CNMI=3,3,0,0")) return -1;
    BIG_ARDUINO.println("Done");
    
    // Request a report with the LAC and CID
    BIG_ARDUINO.println("Requesting periodic CREGs");
    if (sendToCellShieldAndConfirm("AT+CREG=2")) return -1;
    BIG_ARDUINO.println("Done");
    
    return 0;
}

void setup() {
    BIG_ARDUINO.begin(9600);
    CELL_SHIELD.begin(9600);
    
    BIG_ARDUINO.println("Starting init sequence");
    
    millisAtLastMessageRelay = millis();
    millisAtLastArduinoSend = millis();
    
    // Keep up trying to setup the cell shield as much as necessary!
    while (setupSim());
    
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
        int c = BIG_ARDUINO.read();
        BIG_ARDUINO.write((char)c);
        if (parseTag(c, &tpData))
        {
            BIG_ARDUINO << "Got tag: " << tpData.tag << " with data: " << tpData.data << '\n';
            if (strcmp(tpData.tag, "LV") == 0)
            {
                // We only care for the first byte on the liveliness tag, and we only care whether it is not 0
                hasBalloonBeenKilled = (tpData.data[0] == '0');
                // If the balloon is killed, we want to enter alert mode!
                if (hasBalloonBeenKilled)
                {
                    inAlertMode = true;
                    BIG_ARDUINO << "Entering alert mode\n";
                }
            }
            else if (strcmp(tpData.tag, "LA") == 0)
            {
                strncpy(latitudeBuffer, tpData.data, 15);
                latitudeBuffer[15] = '\0';
            }
            else if (strcmp(tpData.tag, "LO") == 0)
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
        while (setupSim());
    }
}
