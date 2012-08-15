#include "arduinoAkpParser.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

bool islowerhex(int c)
{
    return isdigit(c) || (c >= 'a' && c <= 'f');
}

//Parses an AKP tag byte by byte as bytes are passed in from
//each call to this method. The internal state of the parser is
//maintained in tpData and this method will only return true for a properly
//parsed tag when the tag has been received and parsed in full.
//At that time, tag and data of tpData will be filled.
//If the parser just updates its internal state because it gets
//more of a tag, or regresses as it found a tag was invalid,
//false is returned and tag and data are not touched.
bool parseTag(char currentByte, TagParseData* tpData)
{
    //Make sure state is initialized
    if (tpData->hasInitedValue != INIT_MAGIC)
    {
        tpData->hasInitedValue = INIT_MAGIC;
        tpData->previousByte1 = -1;
        tpData->previousByte2 = -1;
        tpData->currentByte = -1;
        tpData->tagByte1 = -1;
        tpData->tagByte2 = -1;
        tpData->dataIndex = -1;
        tpData->hasColon = false;
        tpData->checkByte1 = -1;
        tpData->checkByte2 = -1;
    }
    
    //Do the rotation of bytes in the parse data
    tpData->previousByte1 = tpData->previousByte2;
    tpData->previousByte2 = tpData->currentByte;
    tpData->currentByte = currentByte;
    
    //Do we have the start of a new tag? If we were in one before, it must have
    //been corrupt for a new one to show up. We will not, however,
    //kill any otherwise good tag just because it has a ^ in it.
    if (isupper(tpData->previousByte1) && isupper(tpData->previousByte2) &&
        (currentByte == '^') &&
        !(tpData->previousByte1 == 'D' && tpData->previousByte2 == 'D'))
    {
        //Wonderful! We have a new tag begun!
        tpData->tagByte1 = tpData->previousByte1;
        tpData->tagByte2 = tpData->previousByte2;
        tpData->dataIndex = 0;
        
        //And some state things to take care of too...
        tpData->hasColon = false;
        tpData->checkByte1 = -1;
        tpData->checkByte2 = -1;
        
        return false;
    }
    
    //If we have no tag yet, we have no further business
    if (tpData->dataIndex == -1)
    {
        return false;
    }

    //Add another data character if we can (if we have a colon, we are onto the checksum)
    if (!tpData->hasColon && currentByte != ':' && tpData->dataIndex < MAX_DATA_SIZE)
    {
        tpData->dataBuffer[tpData->dataIndex++] = currentByte;
        return false;
    }
    else if (!tpData->hasColon)
    {
        //We must have a colon here to terminate the data or abort.
        if (currentByte == ':')
        {
            tpData->hasColon = true;
            //Done with data, null terminate it
            tpData->dataBuffer[tpData->dataIndex] = '\0';
            return false;
        }
    }
    else if (tpData->checkByte1 == -1)
    {
        //Check bytes must be lower case hexadecimal or we abort
        if (islowerhex(currentByte))
        {
            tpData->checkByte1 = currentByte;
            return false;
        }
    }
    else if (tpData->checkByte2 == -1)
    {
        //Check bytes must be lower case hexadecimal or we abort
        if (islowerhex(currentByte))
        {
            tpData->checkByte2 = currentByte;
            //Good, now we have everything and can check the entire tag!
            //Convert checksum from two hex digits to a number
            char checkBytes[3];
            checkBytes[0] = tpData->checkByte1;
            checkBytes[1] = tpData->checkByte2;
            checkBytes[2] = '\0';

            //As we have already validated the check bytes
            //We cannot get an error here unless we have one there
            int readChecksum = strtol(checkBytes, NULL, 16);

            //Prepare a calculated checksum
            char tagBytes[3];
            tagBytes[0] = tpData->tagByte1;
            tagBytes[1] = tpData->tagByte2;
            tagBytes[2] = '\0';
            
            int calculatedChecksum = crc8(tagBytes, 0);
            calculatedChecksum = crc8(tpData->dataBuffer, calculatedChecksum);

            //If they aren't good we are out of the game!
            if (readChecksum == calculatedChecksum)
            {
                //We have successfully parsed a tag!
                strncpy(tpData->tag, tagBytes, sizeof(tpData->tag));
                strncpy(tpData->data, tpData->dataBuffer, sizeof(tpData->data));
                //We must also make sure the strings are safely terminated...
                tpData->tag[sizeof(tpData->tag) - 1] = '\0';
                tpData->data[sizeof(tpData->data) - 1] = '\0';
                
                //Reset the fact that we were in a tag
                tpData->dataIndex = -1;
                
                return true;
            }
        }
    }
    //If something fell through, then we had some failure,
    //so we abort by setting the tag as not yet found
    tpData->dataIndex = -1;
    return false;
}
