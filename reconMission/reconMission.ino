#include <Serial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Streaming.h>
#include <string.h>
#include <stdlib.h>
#include <SoftwareSerial.h>

#include "fmtDouble.h"
#include "arduinoAkpParser.h"
#include "transceiverPacketParse.h"

#include "gpsimu.h"

#define STAY_ALIVE_PIN 2

#define CELL_SHIELD_ADDRESS 4
#define CELL_MAX_TAGS 6

#define CONSOLE Serial
#define TRANSCEIVER Serial1
#define GPS Serial2
#define CELL_SHIELD Serial3
#define IMU softSerial

SoftwareSerial softSerial(12, 13);

//Parser data
TransceiverPacketParseData transceiverPacketData;
TagParseData cellShieldData;
ImuData imuData;
GpsData gpsData;

//1-Wire on pin 5 for temperatures
OneWire oneWire(5);
DallasTemperature tempSensors(&oneWire);

//Addresses of the 1-wire devices. These are unique. (and need to be set)
DeviceAddress insideTempAdr = { 
    0x28, 0x5B, 0xD3, 0x49, 0x03, 0x00, 0x00, 0x4F };
DeviceAddress outsideTempAdr = { 
    0x28, 0xE8, 0xAC, 0x49, 0x03, 0x00, 0x00, 0x13 };

//Keep track of the last of these critical values
char lastLatitude[10];
char lastLongitude[10];
char lastAltitude[10];
char lastCellMmc[10];
char lastCellMnc[10];
char lastCellLac[10];
char lastCellCid[10];
char lastSatelliteCount[10];

//Killswitch timeout, initialized to 10 minutes at startup
long secondsToTimeout = 600;
bool hasKickedBucket = false;

//Tags to be forwarded from cell-shield
//Tag data can be 9 chars long, tags are always 2 chars.
char cellStoredTags[CELL_MAX_TAGS][3];
char cellStoredData[CELL_MAX_TAGS][10];
int cellStoredTagOn = 0;

//Keep track of time, second by second
unsigned long secondStartTime;

// What to current echo/output to the console - with flags!
// 1 = CONSOLE
// 2 = TRANSCEIVER
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

void setup()
{
    //The debugging monitor
    CONSOLE.begin(9600);
    //Transceiver!
    TRANSCEIVER.begin(9600);
    //GPS!
    GPS.begin(4800);
    //IMU! -- we need to make sure it only gives out reading 1 per second!
    IMU.begin(115200);

    //The cell shield!
    CELL_SHIELD.begin(28800);

    discoverOneWireDevices();

    //1-wire 12-bit temperature devices!
    tempSensors.begin();
    tempSensors.setResolution(12);
    //Asynchronous reading
    tempSensors.setWaitForConversion(false);
    //Request conversions to start
    tempSensors.requestTemperatures();

    //Configure stay-alive pin, start low
    //Preferably, this pin will have a resistor
    //between it and the killswitch circuit
    pinMode(STAY_ALIVE_PIN, OUTPUT);
    digitalWrite(STAY_ALIVE_PIN, LOW);
    
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);

    //Start keeping track of time
    secondStartTime = millis();
}

void discoverOneWireDevices(void) {
    byte i;
    byte present = 0;
    byte data[12];
    byte addr[8];

    CONSOLE.print("Looking for 1-Wire devices...\n\r");
    while(oneWire.search(addr)) {
        CONSOLE.print("\n\rFound \'1-Wire\' device with address:\n\r");
        for( i = 0; i < 8; i++) {
            CONSOLE.print("0x");
            if (addr[i] < 16) {
                CONSOLE.print('0');
            }
            CONSOLE.print(addr[i], HEX);
            if (i < 7) {
                CONSOLE.print(", ");
            }
        }
        if ( OneWire::crc8( addr, 7) != addr[7]) {
            CONSOLE.print("CRC is not valid!\n");
            return;
        }
    }
    CONSOLE.print("\n\r\n\rThat's it.\r\n");
    oneWire.reset_search();
    return;
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
        if (*endPtr == NULL)
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
        CONSOLE << "Could not forward tag: " << tag << " with data: " << data << " due to lack of space.\n";
    }
}

//Handles tags from cell shield
void cellShieldHandleTag(const char* tag, const char* data)
{
    //Specifically, we would like to forward all non-base tags
    //Keep special track of these two...
    if (strcmp(tag, "MC") == 0)
    {
        strncpy(lastCellMmc, data, sizeof(lastCellMmc - 1));
        lastCellMmc[sizeof(lastCellMmc - 1)] = '\0';
    }
    else if (strcmp(tag, "MN") == 0)
    {
        strncpy(lastCellMnc, data, sizeof(lastCellMnc - 1));
        lastCellMnc[sizeof(lastCellMnc - 1)] = '\0';
    }
    else if (strcmp(tag, "LC") == 0)
    {
        strncpy(lastCellLac, data, sizeof(lastCellLac - 1));
        lastCellLac[sizeof(lastCellLac - 1)] = '\0';
    }
    else if (strcmp(tag, "CD") == 0)
    {
        strncpy(lastCellCid, data, sizeof(lastCellCid - 1));
        lastCellCid[sizeof(lastCellCid - 1)] = '\0';
    }
    //Forward everything else...
    else if (strcmp(tag, "KL") != 0 &&
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

void sendTransceiverPacketTag(const char* tag, const char* data)
{
    // Packet start delimeter
    TRANSCEIVER.write(0x7e);
    
    // Packet length is 5 bytes frame structure overhead + tag bytes + data bytes
    // We remove the excess because we are sending tags in individual own packets
    int packetLength = 5 + 2 + strlen(data);
    
    int packetLengthMsb = ((packetLength >> 8) & 0xff);
    int packetLengthLsb = packetLength & 0xff;
    TRANSCEIVER.write(packetLengthMsb);
    TRANSCEIVER.write(packetLengthLsb);
    
    // Send request
    TRANSCEIVER.write((uint8_t)0x01);
    // We want no response verification frames
    TRANSCEIVER.write((uint8_t)0x00);
    // Broadcast the packet with 0xffff
    TRANSCEIVER.write((uint8_t)0xff);
    TRANSCEIVER.write((uint8_t)0xff);
    // Again, disable acknowledgement
    TRANSCEIVER.write((uint8_t)0x00);
    
    // Now for the data!
    TRANSCEIVER << tag << data;
    
    // And now the checksum!
    // Add all non-delimeter, non-length bytes and subtract 8-bits from 0xff
    unsigned long totalSum = 1 + 0xff + 0xff + tag[0] + tag[1];
    const char* dataCharOn = data;
    while (*data)
    {
        totalSum += *data;
        data++;
    }
    
    int checksum = 0xff - (totalSum & 0xff);
    
    TRANSCEIVER.write(checksum);
}

//Sends tag with data to the transceiver and possible console for debugging
//Avoids sending tags with NULL or empty data (for convenience)
void mainSendTag(const char* tag, const char* data)
{
    if (data && *data)
    {
        unsigned char checksum = crc8(tag, 0);
        checksum = crc8(data, checksum);
        //Get hex of checksum
        char hex1 = getHexOfNibble(checksum >> 4);
        char hex2 = getHexOfNibble(checksum);

        if (debugEchoMode & 32)
        {
            CONSOLE << tag << '^' << data << ':' << hex1 << hex2;
        }
        //TRANSCEIVER << tag << '^' << data << ':' << hex1 << hex2;
        //cellShield << tag << '^' << data << ':' << hex1 << hex2;
        
        sendTransceiverPacketTag(tag, data);
    }
}

// Sends the tag to the cell shield arduino.
// Doesn't send for null or empty data for convenience.
void cellShieldSendTag(const char* tag, const char* data)
{
    if (data && *data)
    {
        unsigned char checksum = crc8(tag, 0);
        checksum = crc8(data, checksum);
        //Get hex of checksum
        char hex1 = getHexOfNibble(checksum >> 4);
        char hex2 = getHexOfNibble(checksum);

        CELL_SHIELD << tag << '^' << data << ':' << hex1 << hex2;
        
        // The arduino cell shield also needs some delays as help
        delay(50);
    }
}

// Sends several tags to the arduino cell shield each time this is called
// This is because of issues with serial buffer overflow/timing problems
void cellShieldSendInformation()
{
    static int sendStateOn = 0;
    switch (sendStateOn)
    {
        case 0:
            cellShieldSendTag("LA", lastLatitude);
            cellShieldSendTag("LO", lastLongitude);
            sendStateOn = 1;
            break;
        case 1:
            cellShieldSendTag("GS", lastSatelliteCount);
            sendStateOn = 2;
            break;
        case 2:
            char lifeLeft[10];
            fmtUnsigned(secondsToTimeout, lifeLeft, 10);
            
            cellShieldSendTag("DT", lifeLeft);
            cellShieldSendTag("LV", hasKickedBucket ? "0" : "1");
            sendStateOn = 0;
            break;
        default:
            // Impossible, but good form anyway
            sendStateOn = 0;
            break;
    }
}

void loop()
{
    //Toggle LED on each loop
    static bool ledOn = false;
    ledOn = !ledOn;
    digitalWrite(13, ledOn ? HIGH : LOW);
  
    char insideTemperature[10];
    char outsideTemperature[10];
    bool gottenInsideTemp = false;
    bool gottenOutsideTemp = false;
    //Wait for temperatures to finish converting, then read the temperatures
    //Don't depend on them actually being connected, though
    if (tempSensors.isConnected(insideTempAdr))
    {
        //Do not let it take more than the max of 750ms
        while (!tempSensors.isConversionAvailable(insideTempAdr) &&
            secondStartTime + 750 > millis())
        {
        }
        //Only if we didn't run out of time...
        if (secondStartTime + 750 > millis())
        {
            float temp = tempSensors.getTempC(insideTempAdr);
            //Make it into a string!
            fmtDouble(temp, 1, insideTemperature, sizeof(insideTemperature));
            gottenInsideTemp = true;
        }
    }

    if (tempSensors.isConnected(outsideTempAdr))
    {
        //Do not let it take more than the max of 750ms
        while (!tempSensors.isConversionAvailable(outsideTempAdr) &&
            secondStartTime + 750 > millis())
        {
        }
        //Only if we didn't run out of time...
        if (secondStartTime + 750 > millis())
        {
            float temp = tempSensors.getTempC(outsideTempAdr);
            fmtDouble(temp, 1, outsideTemperature, sizeof(outsideTemperature));
            gottenOutsideTemp = true;
        }
    }
    //Request the temp sensors to begin another conversion
    tempSensors.requestTemperatures();

    //Keep track of what new data we have gotten
    bool gottenGps = false;
    bool gottenImu = false;

    //We will loop around getting data from all sources until a second has passed.
    while (secondStartTime + 1000 > millis())
    {
        //Check for data from all sources...
        int debuggingBytes = CONSOLE.available();
        static TagParseData debuggingData;
        for (int i = 0;i < debuggingBytes; i++)
        {
            int c = CONSOLE.read();
            if (c != -1)
            {
                // See the commendts of debugEchoMode for details...
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
                    CONSOLE.print((char)c);
                }
                if (parseTag(c, &debuggingData))
                {
                    baseHandleTag(debuggingData.tag, debuggingData.data);
                }
            }
        }
        
        int cellShieldBytes = CELL_SHIELD.available();
        for (int i = 0;i < cellShieldBytes; i++)
        {
            int c = CELL_SHIELD.read();
            if (c != -1)
            {
                if (debugEchoMode & 16)
                {
                    CONSOLE.print((char)c);
                }
                if (parseTag(c, &cellShieldData))
                {
                    //CONSOLE << "Got tag from cell shield: " << cellShieldData.tag << " with data: " << cellShieldData.data << '\n';
                    cellShieldHandleTag(cellShieldData.tag, cellShieldData.data);
                }
            }
        }
        
        int transceiverBytes = TRANSCEIVER.available();
        for (int i = 0;i < transceiverBytes; i++)
        {
            int c = TRANSCEIVER.read();
            if (c != -1)
            {
                if (debugEchoMode & 2)
                {
                    CONSOLE.print((char)c);
                }
                
                if (parseTransceiverByte(c, &transceiverPacketData))
                {
                    // Forward a signal strength tag!
                    // We have to make it a string... darrr!
                    // A single byte number can be at most three decimal digits.
                    char signalNumber[4];
                    signalNumber[0] = (transceiverPacketData.signalStrength / 100) + '0';
                    signalNumber[1] = (transceiverPacketData.signalStrength / 10) % 10 + '0';
                    signalNumber[2] = transceiverPacketData.signalStrength % 10 + '0';
                    signalNumber[3] = '\0';
                    
                    forwardTag("BS", signalNumber);
                    
                    // Handle the tag we just marvelously got!
                    baseHandleTag(transceiverPacketData.tag, transceiverPacketData.data);
                }
            }
        }

        int gpsBytes = GPS.available();
        for (int i = 0;i < gpsBytes; i++)
        {
            int c = GPS.read();
            if (c != -1)
            {
                if (debugEchoMode & 4)
                {
                    CONSOLE.print((char)c);
                }
                if (parseGps(c, &gpsData))
                {
                    gottenGps = true;
                }
            }
        }

        int imuBytes = IMU.available();
        for (int i = 0;i < imuBytes; i++)
        {
            int c = IMU.read();
            if (c != -1)
            {
                if (debugEchoMode & 8)
                {
                    CONSOLE.print((char)c);
                }
                if (parseImu(c, &imuData))
                {
                    gottenImu = true;
                }
            }
        }
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
            digitalWrite(STAY_ALIVE_PIN, stayAliveUp ? HIGH : LOW);
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
        mainSendTag("TM", gpsData.utc);
        mainSendTag("HD", gpsData.hdop);
        mainSendTag("GS", gpsData.satellites);
        mainSendTag("LO", gpsData.longitude);
        mainSendTag("LA", gpsData.latitude);
        mainSendTag("AL", gpsData.altitude);
        mainSendTag("EV", gpsData.eastVelocity);
        mainSendTag("NV", gpsData.northVelocity);
        mainSendTag("UV", gpsData.upVelocity);
        
        strncpy(lastLongitude, gpsData.longitude, sizeof(lastLongitude) - 1);
        lastLongitude[sizeof(lastLongitude) - 1] = '\0';
        
        strncpy(lastLatitude, gpsData.latitude, sizeof(lastLatitude) - 1);
        lastLatitude[sizeof(lastLatitude) - 1] = '\0';
        
        strncpy(lastSatelliteCount, gpsData.satellites, sizeof(lastSatelliteCount) - 1);
        lastSatelliteCount[sizeof(lastSatelliteCount) - 1] = '\0';
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
        mainSendTag("YA", imuData.yaw);
        mainSendTag("PI", imuData.pitch);
        mainSendTag("RO", imuData.roll);
        mainSendTag("AX", imuData.accelX);
        mainSendTag("AY", imuData.accelY);
        mainSendTag("AZ", imuData.accelZ);
    }
    mainSendTag("MC", lastCellMmc);
    mainSendTag("MN", lastCellMnc);
    mainSendTag("LC", lastCellLac);
    mainSendTag("CD", lastCellCid);
    //Send extra cell shield tags
    while (cellStoredTagOn > 0)
    {
        cellStoredTagOn--;
        mainSendTag(cellStoredTags[cellStoredTagOn], cellStoredData[cellStoredTagOn]);
    }

    //Life left...
    char lifeLeft[10];
    fmtUnsigned(secondsToTimeout, lifeLeft, 10);
    mainSendTag("DT", lifeLeft);

    //Liveliness!
    mainSendTag("LV", hasKickedBucket ? "0" : "1");

    //Request GPS velocity data
    GPS.print(GPS_VELOCITY_REQUEST);

    // Meant for deliminating lines of tags...
    if (debugEchoMode & 32)
    {
        CONSOLE.println();
    }
    
    // Information sent to the cell shield arduino must be done separately to avoid overworking him.
    cellShieldSendInformation();
}
