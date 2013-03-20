#include "nmeaParse.h"
#include <string.h>

void resetNmea(NmeaData* nmea)
{
    nmea->hasBegunUtterance = nmea->datumBegun = nmea->checksumBegun = false;
    nmea->runningChecksum = nmea->tagIndex = nmea->datumOn = nmea->datumDataIndex = 0;
    nmea->readChecksum = -1;
}

void initNmea(NmeaData* nmea, const char* tag, int numDatums, const int* datumIndices, char** datums)
{
    nmea->tag = tag;
    nmea->numDatums = numDatums;
    nmea->datumIndices = datumIndices;
    nmea->datums = datums;
    resetNmea(nmea);
}

bool parseNmea(NmeaData* nmea, char newChar)
{
    // Do we need to find the $ marker of an utterance?
    if (!nmea->hasBegunUtterance)
    {
        if (newChar == '$')
        {
            nmea->hasBegunUtterance = true;
            // Clear old data
            for (int i = 0;i < nmea->numDatums; i++)
            {
                nmea->datums[i][0] = '\n';
            }
        }
        return false;
    }
    // Check all characters till we get a null
    else if (nmea->tag[nmea->tagIndex])
    {
        // We ought to have the next char of the tag now
        if (newChar == nmea->tag[nmea->tagIndex])
        {
            nmea->tagIndex++;
            // And we keep up with the checksum too, now
            nmea->runningChecksum ^= newChar;
            return false;
        }
    }
    // Read in central data until a * occurs, signifying a checksum
    else if (!nmea->checksumBegun)
    {
        // $ causes abort and restart....
        if (newChar != '$')
        {
            // Read in chars for a datum until terminated with ',' or '*'
            if (newChar == ',' || newChar == '*')
            {
                // Null-terminate the datum
                nmea->datumData[nmea->datumDataIndex] = '\0';
    
                // Is this datum requested?
                // Ignore unrequested ones...
                for (int i = 0;i < nmea->numDatums; i++)
                {
                    if (nmea->datumOn == nmea->datumIndices[i])
                    {
                        strncpy(nmea->datums[i], nmea->datumData, sizeof(nmea->datumData));
                    }
                }
                
                if (newChar == '*')
                {
                    nmea->checksumBegun = true;
                    // Also, make sure readChecksum is -1, as it may have initialized to 0
                    nmea->readChecksum = -1;
                }
                else
                {
                    nmea->datumOn++;
                    nmea->datumDataIndex = 0;
                    // Add to checksum
                    nmea->runningChecksum ^= newChar;
                }
                return false;
            }
            // We leave one spot for the null-terminator
            else if (nmea->datumDataIndex < sizeof(nmea->datumData) - 1)
            {
                nmea->datumData[nmea->datumDataIndex++] = newChar;
            }
            nmea->runningChecksum ^= newChar;
            return false;
        }
    }
    // Checksums here use uppercase hex
    else
    {
        int checkNum = -1;
        if (newChar >= 'A' && newChar <= 'F')
        {
            checkNum = (newChar - 'A') + 10;
        }
        else if (newChar >= '0' && newChar <= '9')
        {
            checkNum = (newChar - '0');
        }
        if (checkNum != -1)
        {
            // Are we on the first byte?
            if (nmea->readChecksum == -1)
            {
                nmea->readChecksum = checkNum * 16;
                return false;
            }
            else
            {
                nmea->readChecksum += checkNum;
                if (nmea->readChecksum == nmea->runningChecksum)
                {
                    // reset state before returning...
                    resetNmea(nmea);
                    return true;
                }
            }
        }
    }
    // If we fell through without returning, an error occurred and
    // we ought to reset the state.
    resetNmea(nmea);

    // Give this a chance on failure to start a new sentence
    if (newChar == '$')
    {
        parseNmea(nmea, newChar);
    }
    
    return false;
}
