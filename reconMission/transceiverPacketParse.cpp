#include "transceiverPacketParse.h"

bool parseTransceiverByte(unsigned char c, TransceiverPacketParseData* tppData)
{
    if (!tppData->hasDelimeter)
    {
        if (c == 0x7e)
        {
            tppData->hasDelimeter = true;
            // Set all other values as not found yet.
            tppData->lengthByte1 = tppData->lengthByte2 = -1;
            tppData->apiIdentifier = -1;
            tppData->addressByte1 = tppData->addressByte2 = -1;
            tppData->signalStrength = -1;
            tppData->options = -1;
            tppData->dataByteOn = 0;
        }
    }
    else if (tppData->lengthByte1 == -1)
    {
        tppData->lengthByte1 = c;
    }
    else if (tppData->lengthByte2 == -1)
    {
        tppData->lengthByte2 = c;
        tppData->totalLength = (tppData->lengthByte1 << 8) | tppData->lengthByte2;
    }
    else if (tppData->apiIdentifier == -1)
    {
        tppData->apiIdentifier = c;
        // First byte counted as part of length
        tppData->dataByteOn++;
    }
    else
    {
        // All these bytes are counted as part of length as well
        tppData->dataByteOn++;
        
        // Special handling for the receive data packet
        if (tppData->apiIdentifier == 0x81)
        {
            if (tppData->addressByte1 == -1)
            {
                tppData->addressByte1 = c;
            }
            else if (tppData->addressByte2 == -1)
            {
                tppData->addressByte2 = c;
            }
            else if (tppData->signalStrength == -1)
            {
                tppData->signalStrength = c;
            }
            else if (tppData->options == -1)
            {
                tppData->options = c;
            }
            else if (tppData->dataByteOn <= tppData->totalLength)
            {
                int tagByteOn = tppData->dataByteOn - 6;
                switch (tagByteOn)
                {
                    case 0:
                        tppData->tag[0] = c;
                        break;
                    case 1:
                        tppData->tag[1] = c;
                        tppData->tag[2] = '\0';
                        break;
                    default:
                        // Only let it all work if we have space for it all!
                        // Otherwise, we will just let the whole tag die out!
                        if (tagByteOn < sizeof(tppData->data))
                        {
                            tppData->data[tagByteOn - 2] = c;
                        }
                        break;
                }
                if (tppData->dataByteOn == tppData->totalLength)
                {
                    // We got everything! Null terminate it and go!
                    tppData->data[tagByteOn - 1] = '\0';
                    return true;
                }
            }
            else
            {
                // Time to ignore the checksum! We reset for next time!
                tppData->hasDelimeter = false;
            }
        }
        else if (tppData->dataByteOn > tppData->totalLength)
        {
            // If we have a non-receive data packet, we basically ignore it,
            // but when we get through all the data, we ignore the checksum too, and go on
            tppData->hasDelimeter = false;
        }
    }
    return false;
}