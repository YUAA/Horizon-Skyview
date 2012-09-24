#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Streaming.h>
#include <string.h>
#include <stdlib.h>

#include "fmtDouble.h"
#include "tagparser.h"
#include "gpsimu.h"

#define STAY_ALIVE_PIN 2

#define CELL_SHIELD_ADDRESS 4
#define CELL_MAX_TAGS 6

//Parser data
TagParseData transceiverData;
TagParseData cellShieldData;
ImuData imuData;
GpsData gpsData;

//1-Wire on pin 10 for temperatures
OneWire oneWire(10);
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
char lastCellCid[10];

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

void setup()
{
    //The debugging monitor
    Serial.begin(9600);
    //Transceiver!
    Serial1.begin(9600);
    //GPS!
    Serial2.begin(4800);
    //IMU! -- we need to make sure it only gives out reading 1 per second!
    Serial3.begin(115200);

    //The cell shield!
    Wire.begin();

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

    Serial.print("Looking for 1-Wire devices...\n\r");
    while(oneWire.search(addr)) {
        Serial.print("\n\rFound \'1-Wire\' device with address:\n\r");
        for( i = 0; i < 8; i++) {
            Serial.print("0x");
            if (addr[i] < 16) {
                Serial.print('0');
            }
            Serial.print(addr[i], HEX);
            if (i < 7) {
                Serial.print(", ");
            }
        }
        if ( OneWire::crc8( addr, 7) != addr[7]) {
            Serial.print("CRC is not valid!\n");
            return;
        }
    }
    Serial.print("\n\r\n\rThat's it.\r\n");
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

//Handles tags from cell shield
void cellShieldHandleTag(const char* tag, const char* data)
{
    //Specifically, we would like to forward all non-base tags
    //Keep special track of these two...
    if (strcmp(tag, "MC") == 0)
    {
        strncpy(lastCellMmc, data, sizeof(lastCellMmc));
    }
    else if (strcmp(tag, "CD") == 0)
    {
        strncpy(lastCellCid, data, sizeof(lastCellCid));
    }
    //Forward everything else...
    else if (strcmp(tag, "KL") != 0 &&
        strcmp(tag, "ST") != 0)
    {
        //Set to be forwarded if there is space
        if (cellStoredTagOn < CELL_MAX_TAGS)
        {
            strncpy(cellStoredTags[cellStoredTagOn++], tag, sizeof(*cellStoredTags));
            strncpy(cellStoredData[cellStoredTagOn], data, sizeof(*cellStoredData));
        }
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

//Sends tag with data to both the transceiver and cell shield arduino
//Avoids sending tags with NULL or empty data (for convenience)
void sendTag(const char* tag, const char* data)
{
    if (data && *data)
    {
        unsigned char checksum = crc8(tag, 0);
        checksum = crc8(data, checksum);
        //Get hex of checksum
        char hex1 = getHexOfNibble(checksum >> 4);
        char hex2 = getHexOfNibble(checksum);

        Serial << tag << data << ':' << hex1 << hex2;
        Serial1 << tag << data << ':' << hex1 << hex2;
        Wire << tag << data << ':' << hex1 << hex2;
    }
}

bool isWireAvailable(int address)
{
    Wire.beginTransmission(address);
    Wire.write((byte)0);
    return Wire.endTransmission() != 2;
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

    //Get data from cell shield:
    //We will request the max and then take however much we are given.
    //We will check for the data over the main data acquisition loop
    //with everything else, so that it can come in at any rate.
    Wire.requestFrom(CELL_SHIELD_ADDRESS, 64);

    //Keep track of what new data we have gotten
    bool gottenGps = false;
    bool gottenImu = false;

    //We will loop around getting data from all sources until a second has passed.
    while (secondStartTime + 1000 > millis())
    {
        //Check for data from all sources...
        
        int cellShieldBytes = Wire.available();
        for (int i = 0;i < cellShieldBytes; i++)
        {
            int c = Wire.read();
            if (c != -1)
            {
                Serial.print((char)c);
                if (parseTag(c, &cellShieldData))
                {
                    cellShieldHandleTag(cellShieldData.tag, cellShieldData.data);
                }
            }
        }
        
        int transceiverBytes = Serial1.available();
        for (int i = 0;i < transceiverBytes; i++)
        {
            int c = Serial1.read();
            if (c != -1)
            {
                Serial.print((char)c);
                if (parseTag(c, &transceiverData))
                {
                    baseHandleTag(transceiverData.tag, transceiverData.data);
                }
            }
        }

        int gpsBytes = Serial2.available();
        for (int i = 0;i < gpsBytes; i++)
        {
            int c = Serial2.read();
            if (c != -1)
            {
                Serial.print((char)c);
                if (parseGps(c, &gpsData))
                {
                    gottenGps = true;
                }
            }
        }

        int imuBytes = Serial3.available();
        for (int i = 0;i < imuBytes; i++)
        {
            int c = Serial3.read();
            if (c != -1)
            {
                Serial.print((char)c);
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
        sendTag("TI", insideTemperature);
    }
    if (gottenOutsideTemp)
    {
        sendTag("TO", outsideTemperature);
    }
    if (gottenGps)
    {
        sendTag("TM", gpsData.utc);
        sendTag("HD", gpsData.hdop);
        sendTag("GS", gpsData.satellites);
        sendTag("LO", gpsData.longitude);
        sendTag("LA", gpsData.latitude);
        sendTag("AL", gpsData.altitude);
        sendTag("EV", gpsData.eastVelocity);
        sendTag("NV", gpsData.northVelocity);
        sendTag("UV", gpsData.upVelocity);
    }
    else
    {
        //Send the last found ones if we have nothing new
        sendTag("LO", lastLongitude);
        sendTag("LA", lastLatitude);
        sendTag("AL", lastAltitude);
    }
    if (gottenImu)
    {
        sendTag("YA", imuData.yaw);
        sendTag("PI", imuData.pitch);
        sendTag("RO", imuData.roll);
        sendTag("AX", imuData.accelX);
        sendTag("AY", imuData.accelY);
        sendTag("AZ", imuData.accelZ);
    }
    sendTag("MC", lastCellMmc);
    sendTag("CD", lastCellCid);
    //Send extra cell shield tags
    while (cellStoredTagOn > 0)
    {
        cellStoredTagOn--;
        sendTag(cellStoredTags[cellStoredTagOn], cellStoredData[cellStoredTagOn]);
    }

    //Life left...
    char lifeLeft[10];
    fmtUnsigned(secondsToTimeout, lifeLeft, 10);
    sendTag("DT", lifeLeft);

    //Liveliness!
    sendTag("LV", hasKickedBucket ? "0" : "1");

    //Request GPS velocity data
    Serial2.print(GPS_VELOCITY_REQUEST);

    Serial.println();
}

