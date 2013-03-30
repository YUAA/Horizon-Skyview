//#include <stdbool.h>

#ifndef NMEA_PARSE
#define NMEA_PARSE

// The structure for parsed NMEA 0183 data
// numDatums is the size of both datumIndices and datums,
// where datumIndices indicates which datums in the Nmea
// sentence are of interest and should be put into datums.
// Each datum should have space for 10 bytes.
// All values will be null-terminated and
// if for some reason the actual parsed data is longer,
// the extra characters beyond 9 will be discarded.
// This structure also stores the state of the parser,
// which state should not be modified by anything but the parser.
// This should be initialized with initNmea.
typedef struct
{
    const char* tag;
    int numDatums;
    const int* datumIndices;
    char** datums;
    // State, do not touch!
    char runningChecksum;
    bool hasBegunUtterance;
    int tagIndex;
    bool datumBegun;
    int datumOn;
    char datumData[10];
    int datumDataIndex;
    bool checksumBegun;
    int readChecksum;
} NmeaData;

// Updates internal state with the new character
// Returns true if an entire sentence/utterance has
// just finished being read and checksummed correctly.
// Data output locations are only defined after true is returned.
// The checksum is done between the $ and * characters
bool parseNmea(NmeaData* nmea, char newChar);

// Initializes an NmeaData structure. This should be called
// before using the structure.
// tag is the "name" of the NMEA sentence, and it should include the comma at the end,
// this may be, for example, "VNYMR,".
// numDatums is the number of individual pieces of data that you want to parse out of the NMEA sentence.
// For example, if you have the sentence: "$GPGGA,121505,4807.038,N,01131.324,E,1,08,0.9,133.4,M,46.9,M,,*48"
// you may only be interested in the first (121505) and fourth (01131.324) bits of text.
// In this case, you set numDatums to 2.
// datumIndices is where you specify "first and fourth". You can do this with a literal list if you cast it.
// so in this case we would use "(const int[]) {0, 3}".
// datums is a list of pointers to where we want the parser to put the data it found. We can do this with
// a literal list as well. In our case, perhaps, "(char*[]) {data1, data2}". These pointers should point to
// locations with 10 bytes (or more, but the parser will use exactly 10).
// These locations will have undefined values while the parser is in the middle of a NMEA sentence.
void initNmea(NmeaData* nmea, const char* tag, int numDatums, const int* datumIndices, char** datums);

#endif