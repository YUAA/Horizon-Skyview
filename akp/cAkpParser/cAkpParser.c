#include "cAkpParser.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "exitmalloc.h"

bool islowerhex(int c)
{
    return isdigit(c) || (c >= 'a' && c <= 'f');
}

bool addByteForNormalTag(char currentByte, TagParseData* tpData)
{
    //Add another data character (if we have a colon, we are onto the checksum)
    if (!tpData->hasColon && currentByte != ':')
    {
        //Expand the data buffer if necessary (leaving space for null-terminator)
        if (tpData->bufferLength - 1 <= tpData->dataIndex)
        {
            tpData->bufferLength *= 2;
            tpData->dataBuffer = (char*)exitrealloc(tpData->dataBuffer, tpData->bufferLength);
        }
        
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
                //So we make malloc-ments for output!
                tpData->tag = (char*)exitmalloc(3);
                strncpy(tpData->tag, tagBytes, 3);
                
                tpData->dataLength = tpData->dataIndex;
                tpData->data = (char*)exitmalloc(tpData->dataLength);
                strncpy(tpData->data, tpData->dataBuffer, tpData->dataLength);
                
                //We must also make sure the strings are safely terminated...
                //This part is not so totally necessary because of our own checks,
                //but is nevertheless a good idea in regard to strncpy
                tpData->tag[2] = '\0';
                tpData->data[tpData->dataLength] = '\0';
                
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

//Performs final output work and state changes
//for the completion of the DD tag.
void finalizeDdTag(TagParseData* tpData)
{
    //Make malloc-ments for output
    tpData->tag = (char*)exitmalloc(3);
    tpData->tag[0] = tpData->tagByte1;
    tpData->tag[1] = tpData->tagByte2;
    tpData->tag[2] = '\0';
    
    tpData->dataLength = tpData->aDataLength1;
    tpData->data = (char*)exitmalloc(tpData->dataLength);
    memcpy(tpData->data, tpData->dataBuffer, tpData->dataLength);
    
    //Reset our presence in any ongoing tag
    tpData->lengthByteOn = -1;
    tpData->dataIndex = -1;
}

bool addByteForDdTag(char currentByte, TagParseData* tpData)
{
    //Add a new length byte if we still need more
    if (tpData->lengthByteOn < 8)
    {
        //We first read in two 16-bit unsigned lengths from lowercase bigendian hexadecimal
        //Abort (fall through) if they are not lowercase hex.
        if (islowerhex(currentByte))
        {
            int value = (isdigit(currentByte)) ? (currentByte - '0') : (currentByte - 'a' + 10);
            //Add value to whichever of the lengths we are on, shifting the previous value appropriately
            if (tpData->lengthByteOn < 4)
            {
                tpData->aDataLength1 <<= 4;
                tpData->aDataLength1 += value;
            }
            else
            {
                tpData->aDataLength2 <<= 4;
                tpData->aDataLength2 += value;
            }
            
            if (tpData->lengthByteOn++ >= 7)
            {
                //Are the two lengths the same? abort if not.
                if (tpData->aDataLength1 == tpData->aDataLength2)
                {
                    //Then we can go on and get the data itself!
                    //Make sure our buffer is large enough
                    if (tpData->bufferLength < tpData->aDataLength1)
                    {
                        tpData->dataBuffer = (char*)exitrealloc(tpData->dataBuffer, tpData->aDataLength1);
                        tpData->bufferLength = tpData->aDataLength1;
                    }
                    
                    //Handle the special case of having arbitrary data of 0 length...
                    if (tpData->aDataLength1 == 0)
                    {
                        finalizeDdTag(tpData);
                        return true;
                    }
                    
                    //Good to start getting that data
                    tpData->dataIndex = 0;
                    return false;
                }
            }
            else
            {
                //Till the next byte!
                return false;
            }
        }
    }
    else if (tpData->dataIndex < tpData->aDataLength1)
    {
        //We have another byte of arbitrary data! YAY!!!
        tpData->dataBuffer[tpData->dataIndex++] = currentByte;
        
        //Is that it?
        if (tpData->dataIndex >= tpData->aDataLength1)
        {
            finalizeDdTag(tpData);
            return true;
        }
        return false;
    }
    //If we have fallen through then we have an error in the parsing,
    //so we abort the DD tag and set is as not found yet
    //we must also reset the data index to show that we are not in a normal tag either
    tpData->lengthByteOn = -1;
    tpData->dataIndex = -1;
    return false;
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
        tpData->bufferLength = 32;
        tpData->dataBuffer = (char*)exitmalloc(tpData->bufferLength);
        tpData->hasColon = false;
        tpData->checkByte1 = -1;
        tpData->checkByte2 = -1;
        tpData->lengthByteOn = -1;
        tpData->aDataLength1 = -1;
        tpData->aDataLength2 = -1;
    }
    
    //Do the rotation of bytes in the parse data
    tpData->previousByte1 = tpData->previousByte2;
    tpData->previousByte2 = tpData->currentByte;
    tpData->currentByte = currentByte;
    
    //If we are not in an arbitrary data (DD) tag (i.e. we are not on a length byte)...
    //Do we have the start of a new tag? (Two uppercase letters followed by a ^)
    //If we were in one before, it must have
    //been corrupt for a new one to show up. We will not, however,
    //kill any otherwise good tag just because it has a ^ in it.
    if (tpData->lengthByteOn == -1 &&
        isupper(tpData->previousByte1) && isupper(tpData->previousByte2) && (currentByte == '^'))
    {
        //Wonderful! We have a new tag begun!
        tpData->tagByte1 = tpData->previousByte1;
        tpData->tagByte2 = tpData->previousByte2;
        
        if (tpData->tagByte1 == 'D' && tpData->tagByte2 == 'D')
        {
            //Start getting length bytes for the arbitrary data
            tpData->lengthByteOn = 0;
            tpData->aDataLength1 = 0;
            tpData->aDataLength2 = 0;
        }
        else
        {
            //Start collecting data
            tpData->dataIndex = 0;
            
            //Mark remaining needed parts as not found yet
            tpData->hasColon = false;
            tpData->checkByte1 = -1;
            tpData->checkByte2 = -1;
        }
        
        return false;
    }
    
    //We must check for lengthByteOn first and whether we are in an arbitrary data (DD) tag,
    //because dataIndex is also used by DD.
    if (tpData->lengthByteOn != -1)
    {
        return addByteForDdTag(currentByte, tpData);
    }
    else if (tpData->dataIndex != -1)
    {
        return addByteForNormalTag(currentByte, tpData);
    }
    else
    {
        return false;
    }
}
