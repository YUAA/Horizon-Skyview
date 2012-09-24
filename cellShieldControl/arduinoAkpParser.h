#include "crc8.h"
#include <stdbool.h>

#ifndef AKP_PARSER_H
#define AKP_PARSER_H

#define MAX_DATA_SIZE 31
#define INIT_MAGIC 0xafedbeef

//The struct that stores the state of the AKP parser and
//the results (tag and data).
typedef struct
{
    //These two are only for output when data is successfully parsed
    char tag[3];
    char data[MAX_DATA_SIZE + 1];
    //State -- should not be modified outside of parseTag
    //We set hasInited to a special magic number to indicate when initalization has occurred
    unsigned int hasInitedValue;
    int previousByte1;
    int previousByte2;
    int currentByte;
    int tagByte1;
    int tagByte2;
    //While dataIndex is -1, we have not yet started a tag
    int dataIndex;
    char dataBuffer[MAX_DATA_SIZE + 1];
    //The ':' precedes the "check bytes" which make one hexadecimal byte
    bool hasColon;
    int checkByte1;
    int checkByte2;
} TagParseData;

//Parses an AKP tag byte by byte as bytes are passed in from
//each call to this method. The internal state of the parser is
//maintained in tpData and this method will only return true for a properly
//parsed tag when the tag has been received and parsed in full.
//At that time, tag and data of tpData will be filled.
//If the parser just updates its internal state because it gets
//more of a tag, or regresses as it found a tag was invalid,
//false is returned and tag and data are not touched.
bool parseTag(char currentByte, TagParseData* tpData);

#endif
