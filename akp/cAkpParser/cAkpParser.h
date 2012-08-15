#include "crc8.h"
#include <stdbool.h>

#ifndef AKP_PARSER_H
#define AKP_PARSER_H

#define INIT_MAGIC 0xafedbeef

//The struct that stores the state of the AKP parser and
//the results (tag and data).
//When a struct is needed for use, it should be zero-initalized, with {} where possible (c++),
//or with {.hasInited = false} (vanilla c).
//If there is a need to release memory stored internally by this structure,
//then it is appropriate to free dataBuffer, after which the structure will be unusable.
typedef struct
{
    //These three are only for output when data is successfully parsed
    char* tag;
    char* data;
    int dataLength;
    //State -- should not be modified outside of parseTag
    //We set hasInited to a special magic number to indicate when initalization has occurred
    unsigned int hasInitedValue;
    int previousByte1;
    int previousByte2;
    int currentByte;
    int tagByte1;
    int tagByte2;
    //While dataIndex is -1, we have not yet started a tag
    //If dataIndex is not -1 and lengthByteOn is -1, we have a normal tag
    int dataIndex;
    int bufferLength;
    char* dataBuffer;
    //The ':' precedes the "check bytes" which make one hexadecimal byte
    bool hasColon;
    int checkByte1;
    int checkByte2;
    //These values are specific to the arbitrary data tag (DD)
    //While lengthByteOn is -1, we have not yet started a DD tag
    int lengthByteOn;
    int aDataLength1;
    int aDataLength2;
    
} TagParseData;

//Parses an AKP tag byte by byte as bytes are passed in from
//each call to this method. The internal state of the parser is
//maintained in tpData (may be zero-initalized with {} or {.hasInited = false})
//and this method will only return true for a properly
//parsed tag when the tag has been received and parsed in full.
//At that time, tag and data of tpData will be malloced.
//It is the responsibility of the caller to free these when it is done with them.
//If the parser just updates its internal state because it gets
//more of a tag, or regresses as it found a tag was invalid,
//false is returned and tag and data are not touched.
bool parseTag(char currentByte, TagParseData* tpData);

#endif
