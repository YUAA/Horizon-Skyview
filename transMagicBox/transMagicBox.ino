#include <Serial.h>
#include <SoftwareSerial.h>
#include <Streaming.h>
#include "arduinoAkpParser.h"
#include "transceiverPacketParse.h"

#define CONSOLE Serial
#define TRANSCEIVER softwareSerial

SoftwareSerial softwareSerial(8, 9);

TagParseData tagParseData;
TransceiverPacketParseData transceiverPacketData;

unsigned long lastSignalStrengthTime;

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
    TRANSCEIVER.write((uint8_t)0x52);
    // Broadcast the packet with 0xffff
    TRANSCEIVER.write((uint8_t)0xff);
    TRANSCEIVER.write((uint8_t)0xff);
    // Again, disable acknowledgement
    TRANSCEIVER.write((uint8_t)0x00);
    
    // Now for the data!
    TRANSCEIVER << tag << data;
    
    // And now the checksum!
    // Add all non-delimeter, non-length bytes and subtract 8-bits from 0xff
    unsigned long totalSum = 0x01 + 0x52 + 0xff + 0xff + tag[0] + tag[1];
    const char* dataCharOn = data;
    while (*data)
    {
        totalSum += *data;
        data++;
    }
    
    int checksum = 0xff - (totalSum & 0xff);
    
    TRANSCEIVER.write(checksum);
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

void sendTag(const char* tag, const char* data)
{
    unsigned char checksum = crc8(tag, 0);
    checksum = crc8(data, checksum);
    //Get hex of checksum
    char hex1 = getHexOfNibble(checksum >> 4);
    char hex2 = getHexOfNibble(checksum);

    CONSOLE << tag << '^' << data << ':' << hex1 << hex2;
}

void setup()
{
    CONSOLE.begin(9600);
    TRANSCEIVER.begin(9600);
    
    lastSignalStrengthTime = millis();
}

void loop()
{
    int consoleBytes = CONSOLE.available();
    for (int i = 0; i < consoleBytes; i++)
    {
        int c = CONSOLE.read();
        if (c != -1)
        {
            if (parseTag(c, &tagParseData))
            {
                sendTransceiverPacketTag(tagParseData.tag, tagParseData.data);
            }
        }
    }
    
    int transceiverBytes = TRANSCEIVER.available();
    for (int i = 0;i < transceiverBytes; i++)
    {
        int c = TRANSCEIVER.read();
        if (c != -1)
        {
            if (parseTransceiverByte(c, &transceiverPacketData))
            {
                // We only want to send signal strength tag once every second
                if (millis() - lastSignalStrengthTime >= 1000)
                {
                    lastSignalStrengthTime = millis();
                    // Forward a signal strength tag!
                    // We have to make it a string... darrr!
                    // A single byte number can be at most three decimal digits.
                    char signalNumber[4];
                    signalNumber[0] = (transceiverPacketData.signalStrength / 100) + '0';
                    signalNumber[1] = (transceiverPacketData.signalStrength / 10) % 10 + '0';
                    signalNumber[2] = transceiverPacketData.signalStrength % 10 + '0';
                    signalNumber[3] = '\0';
                    
                    sendTag("RS", signalNumber);
                    // Merci newline for stephan
                    CONSOLE.println();
                }
                
                sendTag(transceiverPacketData.tag, transceiverPacketData.data);
            }
        }
    }
}
