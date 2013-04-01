#define  __STDC_LIMIT_MACROS
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "nmeaParse/nmeaParse.h"

#ifndef GPS_DECODER
#define GPS_DECODER

class GPSDecoder
{
	public:

	/*
	real in variable name indicates it is not fixed point
	Make use of int32_t, int16_t, int8_t (32-bits, 16-bits, or 8-bits) 
	instead of int, short, or char. 
	This will ensure that the length of the integer is always the same on different platforms.
	*/

	GPSDecoder();

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

	int32_t getSatelliteCount() const;

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


		NmeaData nmeaPos;
		int nmeaPosIndices[7];
		char* nmeaPosDatums[7];

				NmeaData nmeaVelocity;
		int nmeaVelocityIndices[3];
		char* nmeaVelocityDatums[3];

		char latitudeString[10];
		char NSlatitudeString[1];
		char longitudeString[10];
		char EWlongitudeString[1];
		char altitudeString[10];
		char numSatellitesString[10];
		char hdopString[10];

		char speedString[10];
		char trueheadingString[10];
		char magneticheadingString[10];

		int32_t latitude;
		int32_t longitude;
		int32_t altitute;
		int32_t realNumSatellites;
		int32_t realHdop;
		int32_t speed;  
		int32_t trueheading;
		int32_t magneticheading;


};

#endif //GPSDecoder
/* assume NEMAinterperate gives 
static char* datums[] = {gpsD.utc, latitude, latitudeDirection, longitude, longitudeDirection,
                                 gpsD.satellites, gpsD.hdop, gpsD.altitude};
								 */
