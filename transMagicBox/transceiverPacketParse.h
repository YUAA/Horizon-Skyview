#include <stdbool.h>

#ifndef TRANSCEIVER_PACKET_PARSE_H
#define TRANSCEIVER_PACKET_PARSE_H

//The struct that stores the state of the AKP parser and
//the results (tag and data).
typedef struct
{
    char tag[3];
    char data[513];
    // The delimeter byte is 0x7e
    bool hasDelimeter;
    // Each of the following are single bytes, in order they should come in
    int lengthByte1;
    int lengthByte2;
    int apiIdentifier;
    int addressByte1;
    int addressByte2;
    int signalStrength;
    int options;
    // The byte of data we should receive next
    // Once this value is equal to the value of the total length,
    // we have received all the data!
    int dataByteOn;
    // Simply the combined values of lengthByte1 (MSB) and lengthByte2 (LSB)
    // for convenience
    int totalLength;
    // After the data there is a checksum. This checksum, however, will be ignored.
} TransceiverPacketParseData;

// Returns true if and only if a new tag and data pair are available in tppData.
// When this happens, signalStrength should also be good for querying.
// Values may be invalidated after false is returned.
bool parseTransceiverByte(unsigned char c, TransceiverPacketParseData* tppData);

#endif