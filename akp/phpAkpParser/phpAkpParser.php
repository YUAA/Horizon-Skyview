<?php
require 'crc8.php';

class AkpParser
{
    //These two are only for output when data is successfully parsed
    public $tag;
    public $data;
    
    //State -- should not be modified outside of parseTag
    private $previousByte1;
    private $previousByte2;
    private $currentByte;
    private $tagByte1;
    private $tagByte2;
    //While dataIndex is -1, we have not yet started a tag
    //If dataIndex is not -1 and lengthByteOn is -1, we have a normal tag
    private $dataIndex;
    private $dataBuffer;
    //The ':' precedes the "check bytes" which make one hexadecimal byte
    private $hasColon;
    private $checkByte1;
    private $checkByte2;
    //These values are specific to the arbitrary data tag (DD)
    //While lengthByteOn is -1, we have not yet started a DD tag
    private $lengthByteOn;
    private $aDataLength1;
    private $aDataLength2;
    
    function __construct()
    {
        $this->previousByte1 = -1;
        $this->previousByte2 = -1;
        $this->currentByte = -1;
        $this->tagByte1 = -1;
        $this->tagByte2 = -1;
        $this->dataIndex = -1;
        $this->dataBuffer = '';
        $this->hasColon = false;
        $this->checkByte1 = -1;
        $this->checkByte2 = -1;
        $this->lengthByteOn = -1;
        $this->aDataLength1 = -1;
        $this->aDataLength2 = -1;
    }
    
    function islowerhex($c)
    {
        return ctype_digit($c) || (ctype_xdigit($c) && ctype_lower($c));
    }

    function addByteForNormalTag($currentByte)
    {
        //Add another data character (if we have a colon, we are onto the checksum)
        if (!$this->hasColon && $currentByte != ':')
        {
            $this->dataIndex++;
            $this->dataBuffer .= $currentByte;
            return false;
        }
        else if (!$this->hasColon)
        {
            //We must have a colon here to terminate the data or abort.
            if ($currentByte == ':')
            {
                $this->hasColon = true;
                return false;
            }
        }
        else if ($this->checkByte1 == -1)
        {
            //Check bytes must be lower case hexadecimal or we abort
            if ($this->islowerhex($currentByte))
            {
                $this->checkByte1 = $currentByte;
                return false;
            }
        }
        else if ($this->checkByte2 == -1)
        {
            //Check bytes must be lower case hexadecimal or we abort
            if ($this->islowerhex($currentByte))
            {
                $this->checkByte2 = $currentByte;
                //Good, now we have everything and can check the entire tag!
                //Convert checksum from two hex digits to a number
                $readChecksum = ('0x' . $this->checkByte1 . $this->checkByte2) + 0;

                //Prepare a calculated checksum
                $tagBytes = $this->tagByte1 . $this->tagByte2;
                
                $calculatedChecksum = crc8($tagBytes, 0);
                $calculatedChecksum = crc8($this->dataBuffer, $calculatedChecksum);

                //If they aren't good we are out of the game!
                if ($readChecksum == $calculatedChecksum)
                {
                    //We have successfully parsed a tag!
                    $this->tag = $tagBytes;
                    $this->data = $this->dataBuffer;
                    
                    //Reset the fact that we were in a tag
                    $this->dataIndex = -1;
                    $this->dataBuffer = '';
                    
                    return true;
                }
                $data = $this->dataBuffer;
            }
        }
        //If something fell through, then we had some failure,
        //so we abort by setting the tag as not yet found
        $this->dataIndex = -1;
        $this->dataBuffer = '';
        return false;
    }

    function addByteForDdTag($currentByte)
    {
        //Add a new length byte if we still need more
        if ($this->lengthByteOn < 8)
        {
            //We first read in two 16-bit unsigned lengths from lowercase bigendian hexadecimal
            //Abort (fall through) if they are not lowercase hex.
            if ($this->islowerhex($currentByte))
            {
                $value = ('0x' . $currentByte) + 0;
                //Add value to whichever of the lengths we are on, shifting the previous value appropriately
                if ($this->lengthByteOn < 4)
                {
                    $this->aDataLength1 <<= 4;
                    $this->aDataLength1 += $value;
                }
                else
                {
                    $this->aDataLength2 <<= 4;
                    $this->aDataLength2 += $value;
                }
                
                if ($this->lengthByteOn++ >= 7)
                {
                    //Are the two lengths the same? abort if not.
                    if ($this->aDataLength1 == $this->aDataLength2)
                    {
                        //Handle the special case of having arbitrary data of 0 length
                        if ($this->aDataLength1 == 0)
                        {
                            $this->tag = $this->tagByte1 . $this->tagByte2;
                            $this->data = $this->dataBuffer;
                            
                            //Reset our presence in any ongoing tag
                            $this->lengthByteOn = -1;
                            $this->dataIndex = -1;
                            $this->dataBuffer = '';

                            return true;
                        }
                    
                        //Good to start getting that data
                        $this->dataIndex = 0;
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
        else if ($this->dataIndex < $this->aDataLength1)
        {
            //We have another byte of arbitrary data! YAY!!!
            $this->dataIndex++;
            $this->dataBuffer .= $currentByte;
            
            //Is that it?
            if ($this->dataIndex >= $this->aDataLength1)
            {
                //Tada!
                $this->tag = $this->tagByte1 . $this->tagByte2;
                $this->data = $this->dataBuffer;
                
                //Reset our presence in any ongoing tag
                $this->lengthByteOn = -1;
                $this->dataIndex = -1;
                $this->dataBuffer = '';
                return true;
            }
            return false;
        }
        //If we have fallen through then we have an error in the parsing,
        //so we abort the DD tag and set is as not found yet
        //we must also reset the data index to show that we are not in a normal tag either
        $this->lengthByteOn = -1;
        $this->dataIndex = -1;
        $this->dataBuffer = '';
        return false;
    }

    //Parses an AKP tag byte by byte as bytes are passed in from
    //each call to this method. The internal state of the parser is
    //maintained in $this and this method will only return true for a properly
    //parsed tag when the tag has been received and parsed in full.
    //At that time, tag and data of $this will be filled.
    //If the parser just updates its internal state because it gets
    //more of a tag, or regresses as it found a tag was invalid,
    //false is returned and tag and data are not touched.
    function parseTag($currentByte)
    {
        //Do the rotation of bytes in the parse data
        $this->previousByte1 = $this->previousByte2;
        $this->previousByte2 = $this->currentByte;
        $this->currentByte = $currentByte;
        
        //If we are not in an arbitrary data (DD) tag (i.e. we are not on a length byte)...
        //Do we have the start of a new tag? (Two uppercase letters followed by a ^)
        //If we were in one before, it must have
        //been corrupt for a new one to show up. We will not, however,
        //kill any otherwise good tag just because it has a ^ in it.
        if ($this->lengthByteOn == -1 &&
            ctype_upper($this->previousByte1) && ctype_upper($this->previousByte2) && ($currentByte == '^'))
        {
            //Wonderful! We have a new tag begun!
            $this->tagByte1 = $this->previousByte1;
            $this->tagByte2 = $this->previousByte2;
            
            if ($this->tagByte1 == 'D' && $this->tagByte2 == 'D')
            {
                //Start getting length bytes for the arbitrary data
                $this->lengthByteOn = 0;
                $this->aDataLength1 = 0;
                $this->aDataLength2 = 0;
            }
            else
            {
                //Start collecting data
                $this->dataIndex = 0;
                
                //Mark remaining needed parts as not found yet
                $this->hasColon = false;
                $this->checkByte1 = -1;
                $this->checkByte2 = -1;
            }
            
            return false;
        }
        
        //We must check for lengthByteOn first and whether we are in an arbitrary data (DD) tag,
        //because dataIndex is also used by DD.
        if ($this->lengthByteOn != -1)
        {
            return $this->addByteForDdTag($currentByte);
        }
        else if ($this->dataIndex != -1)
        {
            return $this->addByteForNormalTag($currentByte);
        }
        else
        {
            return false;
        }
    }
}
?>
