#ifndef GPS_DECODER
#define GPS_DECODER

// Decodes bytes obtained from the GPS so that relevant details may be accessed.
class GPSDecoder
{
    public:

    /*
    Make use of int32_t, int16_t, int8_t (32-bits, 16-bits, or 8-bits) 
    instead of int, short, or char. 
    This will ensure that the length of the integer is always the same on different platforms.
    */

    // Value returned has the decimal point fixed at the 1000s place.
    // Note that it is not in degrees.minutes form, but the numbers
    // following the decimal point are a fraction of a degree.
    int32_t getLatitude() const;

    // Value returned has the decimal point fixed at the 1000s place.
    // Note that it is not in degrees.minutes form, but the numbers
    // following the decimal point are a fraction of a degree.
    int32_t getLongitude() const;

    // In millimeters. (Alternatively fixed-point meters, with the decimal at the 1000s place).
    int32_t getAltitude() const;

    int8_t getSatelliteCount() const;

    // In degrees. From true-north.
    int32_t getTrueHeading() const;

    // In degrees. From magnetic-north.
    int32_t getMagneticHeading() const;

    // In millimeters/second.
    int32_t getSpeed() const;

    // Horizontal Degrees of Precision
    int32_t getHDOP() const;
    
    // Passes the GPSDecoder an additional byte from the the GPS's output stream
    // to decode.
    // Returns true if the GPSDecoder has updated its parameters.
	bool decodeByte(int8_t newByte);

    private:

    // Your code here

};

#endif
