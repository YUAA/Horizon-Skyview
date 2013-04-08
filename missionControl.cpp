#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <iostream>
#include <termios.h>
#include <time.h>
#include <string>

#include "devices/Uart.h"
#include "devices/GPSDecoder.h"
#include "devices/IMUDecoder.h"
#include "devices/ADCSensor3008.h"
#include "devices/CellDriver.h"
#include "devices/PWMSensor.h"
#include "devices/ServoDriver.h"
#include "devices/HumiditySensor.h"
#include "devices/TemperatureSensor.h"
#include "devices/GpioOutput.h"

#include "akp/cAkpParser/crc8.h"
#include "akp/cAkpParser/cAkpParser.h"

#define STAY_ALIVE_PIN 2
#define CELL_MAX_TAGS 6

#define STATUS_LED_2_PIN 33
#define STATUS_LED_3_PIN 37
#define STATUS_LED_4_PIN 63
#define STATUS_LED_5_PIN 46
#define STATUS_LED_6_PIN 44
#define STATUS_LED_7_PIN 34
#define STATUS_LED_8_PIN 35
#define STATUS_LED_9_PIN 39

#define PWM_INPUT_1_PIN 26
#define PWM_INPUT_2_PIN 27
#define PWM_INPUT_3_PIN 38
#define PWM_INPUT_4_PIN 61
#define PWM_INPUT_5_PIN 65
#define PWM_INPUT_6_PIN 86
#define PWM_INPUT_7_PIN 117
#define PWM_INPUT_8_PIN 115

struct termios oldTerminalSettings;

Uart transceiverUart(1, 9600);
Uart imuUart(2, 115200);
Uart servoDriverUart(3, 9600);
Uart cellUart(4, 115200);
Uart gpsUart(5, 9600);

IMUDecoder imuDecoder;
GPSDecoder gpsDecoder;
CellDriver cellDriver(&cellUart);

//HumiditySensor humiditySensor;

ADCSensor3008 temperatureAdc(1);
TemperatureSensor temperatureSensor(&temperatureAdc);

GpioOutput stayAliveGpio(STAY_ALIVE_PIN);

PWMSensor throttleIn(43);
ServoDriver throttleOut(&servoDriverUart);

//Keep track of the last of these critical values
int32_t lastLatitude;
int32_t lastLongitude;
int32_t lastAltitude;
int32_t lastSatelliteCount;

int32_t lastCellMmc;
int32_t lastCellMnc;
int32_t lastCellLac;
int32_t lastCellCid;

//Killswitch timeout, initialized to 10 minutes at startup
long secondsToTimeout = 600;
bool hasKickedBucket = false;

//Tags to be forwarded from cell-shield
//Tag data can be 9 chars long, tags are always 2 chars.
char cellStoredTags[CELL_MAX_TAGS][3];
char cellStoredData[CELL_MAX_TAGS][10];
int cellStoredTagOn = 0;

//Keep track of time, second by second
uint64_t secondStartTime;

// What to current echo/output to the console - with flags!
// 1 = console
// 2 = transceiverUart
// 4 = GPS
// 8 = IMU
// 16 = CELL_SHIELD
// 32 = tag data
// Default to everything!
// The characters that manipulate this from the console are
// ) to turn everything off
// ! for console
// @ for transceiver
// # for gps
// $ for imu
// % for cell shield
// ^ for tag data
// These correspond to the characters that are Shift+(0-5)
int debugEchoMode = 64 - 1;

uint64_t millis()
{
    timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * 1000 + currentTime.tv_usec / 1000;
}

void setup()
{
    //Configure stay-alive pin, start low
    //Preferably, this pin will have a resistor
    //between it and the killswitch circuit
    stayAliveGpio.setValue(0);

    //Start keeping track of time
    secondStartTime = millis();
}

//Handles general-from-anywhere things
void baseHandleTag(const char* tag, const char* data)
{
    if (strcmp(tag, "KL") == 0)
    {
        //Immediate kill
        hasKickedBucket = true;
    }
    else if (strcmp(tag, "ST") == 0)
    {
        //Try to parse time value
        char* endPtr;
        long seconds = strtol(data, &endPtr, 10);
        //We have parsed the time value correcly if
        //endPtr points to the null-terminator of the string.
        if (*endPtr == '\0')
        {
            secondsToTimeout = seconds;
        }
    }
}

// Only does it if there is space...
void forwardTag(const char* tag, const char* data)
{
    //Set to be forwarded if there is space
    if (cellStoredTagOn < CELL_MAX_TAGS)
    {
        strncpy(cellStoredTags[cellStoredTagOn], tag, sizeof(*cellStoredTags));
        strncpy(cellStoredData[cellStoredTagOn], data, sizeof(*cellStoredData));
        cellStoredTagOn++;
    }
    else
    {
        std::cout << "Could not forward tag: " << tag << " with data: " << data << " due to lack of space.\n";
    }
}

//Handles tags from cell shield
void cellShieldHandleTag(const char* tag, const char* data)
{
    //Specifically, we would like to forward all non-base tags
    //Keep special track of these two...
    //Forward everything else...
    if (strcmp(tag, "KL") != 0 &&
        strcmp(tag, "ST") != 0)
    {
        forwardTag(tag, data);
    }
    else
    {
        //Handle base tags
        baseHandleTag(tag, data);
    }
}

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

void sendTag(const char* tag, const char* data, Uart& uart)
{
    if (data && *data)
    {
        unsigned char checksum = crc8(tag, 0);
        checksum = crc8(data, checksum);
        //Get hex of checksum
        char hex1 = getHexOfNibble(checksum >> 4);
        char hex2 = getHexOfNibble(checksum);

        uart << tag << '^' << data << ':' << hex1 << hex2;
    }
}

void sendTag(const char* tag, const char* data, std::ostream& stream)
{
    if (data && *data)
    {
        unsigned char checksum = crc8(tag, 0);
        checksum = crc8(data, checksum);
        //Get hex of checksum
        char hex1 = getHexOfNibble(checksum >> 4);
        char hex2 = getHexOfNibble(checksum);

        stream << tag << '^' << data << ':' << hex1 << hex2;
    }
}

template<class T>
void sendTag(const char* tag, T data, std::ostream& stream)
{
    std::stringstream convertOutput;
    convertOutput << data;
    sendTag(tag, convertOutput.str().c_str(), stream);
}

template<class T>
void sendTag(const char* tag, T data, Uart& uart)
{
    std::stringstream convertOutput;
    convertOutput << data;
    sendTag(tag, convertOutput.str().c_str(), uart);
}

//Sends tag with data to the transceiver and possible std::cout for debugging
//Avoids sending tags with NULL or empty data (for convenience)
template<class T>
void mainSendTag(const char* tag, T data)
{
    sendTag(tag, data, transceiverUart);
    if (debugEchoMode & 32)
    {
        sendTag(tag, data, std::cout);
    }
}

void cellShieldSendInformation()
{
    std::stringstream completeText;
    
    static int sendStateOn = 0;
    switch (sendStateOn)
    {
        case 0:
            sendTag("LA", lastLatitude, completeText);
            sendTag("LO", lastLongitude, completeText);
            sendStateOn = 1;
            break;
        case 1:
            sendTag("GS", lastSatelliteCount, completeText);
            sendStateOn = 2;
            break;
        case 2:
            sendTag("DT", secondsToTimeout, completeText);
            sendTag("LV", hasKickedBucket ? "0" : "1", completeText);
            sendStateOn = 0;
            break;
        default:
            // Impossible, but good form anyway
            sendStateOn = 0;
            break;
    }
    
    cellDriver.queueTextMessage("12537408798", completeText.str().c_str());
}

void loop()
{
    char insideTemperature[10];
    char outsideTemperature[10];
    bool gottenInsideTemp = false;
    bool gottenOutsideTemp = false;
    
    int32_t temperature = temperatureSensor.readTemperature();

    //Keep track of what new data we have gotten
    bool gottenGps = false;
    bool gottenImu = false;

    //We will loop around getting data from all sources until a second has passed.
    while (secondStartTime + 1000 > millis())
    {
        //Check for data from all sources...
        static TagParseData debuggingData;
        int c = -1;
        while ((c = getchar()) != -1)
        {
            // See the comments of debugEchoMode for details...
            switch (c)
            {
                case ')':
                    debugEchoMode = 0;
                    break;
                case '!':
                    debugEchoMode ^= 1;
                    break;
                case '@':
                    debugEchoMode ^= 2;
                    break;
                case '#':
                    debugEchoMode ^= 4;
                    break;
                case '$':
                    debugEchoMode ^= 8;
                    break;
                case '%':
                    debugEchoMode ^= 16;
                    break;
                case '^':
                    debugEchoMode ^= 32;
                    break;
            }
            if (debugEchoMode & 1)
            {
                std::cout << (char)c;
            }
            if (parseTag(c, &debuggingData))
            {
                baseHandleTag(debuggingData.tag, debuggingData.data);
            }
        }
        
        static TagParseData transceiverData;
        c = -1;
        while ((c = transceiverUart.readByte()) != -1)
        {
            if (debugEchoMode & 2)
            {
                std::cout << (char)c;
            }
            
            if (parseTag(c, &transceiverData))
            {
                // Handle the tag we just marvelously got!
                baseHandleTag(transceiverData.tag, transceiverData.data);
            }
        }

        c = -1;
        while ((c = gpsUart.readByte()) != -1)
        {
            if (debugEchoMode & 4)
            {
                std::cout << (char)c;
            }
            if (gpsDecoder.decodeByte(c))
            {
                gottenGps = true;
            }
        }

        c = -1;
        while ((c = imuUart.readByte()) != -1)
        {
            if (debugEchoMode & 8)
            {
                std::cout << (char)c;
            }
            if (imuDecoder.decodeByte(c))
            {
                gottenImu = true;
            }
        }
        
        static TagParseData cellData;
        if (cellDriver.update())
        {
            TextMessage textMessage = cellDriver.getTextMessage();
            const char* messageData = textMessage.messageData.c_str();
            int length = textMessage.messageData.length();

            for (int i = 0; i < length; i++)
            {
                if (parseTag(messageData[i], &cellData))
                {
                    cellShieldHandleTag(cellData.tag, cellData.data);
                }
            }
        }
        
        // Give up a little time to the system...
        struct timespec sleepTime = {0, 1000};
        nanosleep(&sleepTime, NULL);
    }
    //Notice the start time of this next second
    secondStartTime += 1000;

    //Actions for the living
    if (!hasKickedBucket)
    {
        //Update and check timeout
        if (--secondsToTimeout <= 0)
        {
            //Time to die!
            hasKickedBucket = true;
        }
        static int secondsSinceToggle = 0;
        static bool stayAliveUp = false;
        //To indicate that the arduino is running correctly,
        //we send out a 5-second high, 5-second low pulse
        if (++secondsSinceToggle >= 5)
        {
            //Do a toggle!
            secondsSinceToggle = 0;
            stayAliveUp = !stayAliveUp;
            stayAliveGpio.setValue(stayAliveUp);
        }
    }

    //Send out data -- ALL the data!
    if (gottenInsideTemp)
    {
        mainSendTag("TI", insideTemperature);
    }
    if (gottenOutsideTemp)
    {
        mainSendTag("TO", outsideTemperature);
    }
    if (gottenGps)
    {
        //mainSendTag("TM", gpsDecoder.getTime());
        mainSendTag("HD", gpsDecoder.getHDOP());
        mainSendTag("GS", gpsDecoder.getSatelliteCount());
        mainSendTag("LO", gpsDecoder.getLongitude());
        mainSendTag("LA", gpsDecoder.getLatitude());
        mainSendTag("AL", gpsDecoder.getAltitude());
        
        mainSendTag("SP", gpsDecoder.getSpeed());
        mainSendTag("TH", gpsDecoder.getTrueHeading());
        mainSendTag("MH", gpsDecoder.getMagneticHeading());
        
        lastLongitude = gpsDecoder.getLongitude();
        lastLatitude = gpsDecoder.getLatitude();
        lastSatelliteCount = gpsDecoder.getSatelliteCount();
    }
    else
    {
        //Send the last found ones if we have nothing new
        mainSendTag("LO", lastLongitude);
        mainSendTag("LA", lastLatitude);
        mainSendTag("AL", lastAltitude);
    }
    if (gottenImu)
    {
        mainSendTag("YA", imuDecoder.getYaw());
        mainSendTag("PI", imuDecoder.getPitch());
        mainSendTag("RO", imuDecoder.getRoll());
        mainSendTag("AX", imuDecoder.getAcceleration().coordX);
        mainSendTag("AY", imuDecoder.getAcceleration().coordY);
        mainSendTag("AZ", imuDecoder.getAcceleration().coordZ);
    }
    mainSendTag("MC", lastCellMmc);
    mainSendTag("MN", lastCellMnc);
    mainSendTag("LC", lastCellLac);
    mainSendTag("CD", lastCellCid);
    //Send extra tags passed from the cellular connection
    while (cellStoredTagOn > 0)
    {
        cellStoredTagOn--;
        mainSendTag(cellStoredTags[cellStoredTagOn], cellStoredData[cellStoredTagOn]);
    }


    //Life left...
    mainSendTag("DT", secondsToTimeout);

    //Liveliness!
    mainSendTag("LV", hasKickedBucket ? "0" : "1");

    // Meant for deliminating lines of tags...
    if (debugEchoMode & 32)
    {
        std::cout << "\n";
    }
    
    // Information sent to the cell shield arduino must be done separately to avoid overworking him.
    cellShieldSendInformation();
}

void restoreTerminal()
{
    tcsetattr(0, TCSANOW, &oldTerminalSettings);
}

int main(int argc, char* argv[])
{
    //Unbuffered output, so the file can be read in as streamed.
    setvbuf(stdout, NULL, _IONBF, 0);
    
    //Don't wait for newline to get stdin input
    struct termios terminalSettings;
    if (tcgetattr(0, &terminalSettings) < 0)
    {
        perror("Error getting terminal settings");
    }
    
    // Save old terminal settings
    oldTerminalSettings = terminalSettings;
    
    // disable canonical mode processing in the line discipline driver
    // So everything is read in instantly from stdin!
    // Also, don't echo back characters... so we only see what we receive!
    terminalSettings.c_lflag &= ~(ICANON | ECHO);
    // We do not want to block with getchar
    // We have no minimum break in receiving characters (VTIME = 0)
    // and we have no minimum number of characters to receive (VMIN = 0)
    terminalSettings.c_cc[VTIME] = 0;
    terminalSettings.c_cc[VMIN] = 0;
    
    if (tcsetattr(0, TCSANOW, &terminalSettings) < 0)
    {
        perror("Error setting terminal settings");
    }
    
    atexit(restoreTerminal);
    
    // do initialization
    setup();
    
    // Perform our main loop FOREVER!
    while (1)
    {
        loop();
    }
}
