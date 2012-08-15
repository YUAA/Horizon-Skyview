<?php

$crc8Table = array();

//Creates the CRC-8 table
//For each possible byte value...
for ($i = 0;$i < 256;$i++)
{
    //For "each bit" in that value, from high to low
    $valueBits = $i;
    for ($j = 0;$j < 8;$j++)
    {
        //If that bit is set
        if ($valueBits & 128)
        {
            $valueBits = ($valueBits << 1) & 0xFF;
            //The remaining amount is xored with
            //A magical number that messes everything up! =]
            $valueBits ^= 0xD5;
        }
        else
        {
            //Shift that bit out (also multiple remainder)
            $valueBits = ($valueBits << 1) & 0xFF;
        }
    }
    $crc8Table[$i] = $valueBits;
}

//Calculates the CRC-8 checksum of the given data string
//starting with a given initialChecksum so that multiple
//calls may be strung together. Use 0 as a default.
function crc8($data, $initialChecksum)
{
    global $crc8Table;
    $checksum = $initialChecksum;
    for ($i = 0;$i < strlen($data);$i++)
    {
        $checksum = $crc8Table[$checksum ^ ord($data[$i])];
    }
    return $checksum;
}

?>
