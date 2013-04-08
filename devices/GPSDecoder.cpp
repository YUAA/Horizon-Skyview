#include "GPSDecoder.h"

GPSDecoder::GPSDecoder()
{
	nmeaPosIndices[0] = 1;
	nmeaPosIndices[1] = 2;
	nmeaPosIndices[2] = 3;
	nmeaPosIndices[3] = 4;
	nmeaPosIndices[4] = 6;
	nmeaPosIndices[5] = 7;
	nmeaPosIndices[6] = 8;
	nmeaPosDatums[0] = latitudeString;
	nmeaPosDatums[1] = NSlatitudeString;
	nmeaPosDatums[2] = longitudeString;
	nmeaPosDatums[3] = EWlongitudeString;
	nmeaPosDatums[4] = numSatellitesString;
	nmeaPosDatums[5] = hdopString;
	nmeaPosDatums[6] = altitudeString;
	initNmea(&nmeaPos, "GPGGA,", 7, nmeaPosIndices, nmeaPosDatums);

	nmeaVelocityIndices[0] = 0;
	nmeaVelocityIndices[1] = 2;
	nmeaVelocityIndices[2] = 4;
	nmeaVelocityDatums[0] = trueheadingString;
	nmeaVelocityDatums[1] = magneticheadingString;
	nmeaVelocityDatums[2] = speedString;
	initNmea(&nmeaVelocity, "GPVTG,", 3, nmeaVelocityIndices, nmeaVelocityDatums);

	latitude = INT32_MIN;
	longitude = INT32_MIN;
	altitute = INT32_MIN;
	realNumSatellites = INT32_MIN;
	realHdop = INT32_MIN;
	speed = INT32_MIN;
	trueheading = INT32_MIN;
	magneticheading = INT32_MIN;
}

int32_t GPSDecoder::getLatitude() const
{//lat comes in form AA.BBCCC  A degrees B minutes C decimal minutes  
	//lat retruned in form +-AABBB A degrees B decimal degrees decimal fixed between A and B
	return latitude;
}

int32_t GPSDecoder::getLongitude() const
{//lat comes in form AA.BBCCC  A degrees B minutes C decimal minutes  
	//lat retruned in form +-AABBBB A degrees B decimal degrees decimal fixed between A and B
		return longitude;
}

int32_t GPSDecoder::getAltitude() const
{
  return altitute;//in milli metters
}

int32_t GPSDecoder::getSatelliteCount() const
{
  return realNumSatellites;
}

int32_t GPSDecoder::getTrueHeading() const
{
  return trueheading;  // degres from true north increasing eastwardly
}

int32_t GPSDecoder::getMagneticHeading() const
{
	return magneticheading;
}

int32_t GPSDecoder::getSpeed() const
{
  //speed in milimeters per second
  return speed;
}

int32_t GPSDecoder::getHDOP() const
{
  return realHdop;
	//return realHdop;// Your code here
}

	

	

bool GPSDecoder::decodeByte(int8_t newByte)
{
	bool success = false;
	if (parseNmea(&nmeaPos, newByte)) 
	{//lat comes in form AA.BBCCC  A degrees B minutes C decimal minutes  
	//lat retruned in form AA.BBB A degrees B decimal degrees decimal fixed between A and B
		char* decimal;
		int temp;
		temp = strtol(latitudeString, &decimal, 10) * 1000;  //lat = AABBC
		if (temp!=0){
			int i=0;
		}
		if (decimal != latitudeString)
		{
			latitude = temp;
			latitude = latitude + (int)strtol(decimal+1, &decimal, 10) / 600;
		
			if  (NSlatitudeString[0] == 'S')  {
				latitude = -latitude;
			}
		}
		temp = strtol(longitudeString, &decimal, 10) * 1000;
		if(decimal != longitudeString)
		{
			longitude = temp;
			longitude = longitude + (int)strtol(decimal+1, &decimal, 10) / 600;
			if ( EWlongitudeString[0] == 'E') {
				latitude = -latitude;
			}
		}
		temp = strtol( altitudeString, &decimal, 10)*1000;
		if (decimal != altitudeString)
		{
			altitute = temp + strtol( decimal, &decimal, 10);
		}
		 temp =strtol( numSatellitesString, &decimal, 10);
		if (decimal != numSatellitesString) realNumSatellites = temp;
		//strtol(numSatellitesString, &endPointer, 10);
		temp = strtol (hdopString, &decimal, 10);
		if (decimal != hdopString) realHdop = temp;
		success = true;
	}
	if (parseNmea(&nmeaVelocity, newByte)){ 
		char* endpointer; 
		int temp;
		temp =(int32_t) strtol(speedString, &endpointer, 10)*10000/36;
		if (endpointer != speedString)
		{
			if(endpointer!=NULL){
				speed=temp+ (int32_t) strtol(endpointer+1, &endpointer, 10)*1000/36;
			}else speed = temp;
		}
		temp = strtol(trueheadingString, &endpointer, 10);
		if (endpointer != trueheadingString )
		{
			trueheading= temp + strtol( endpointer+1, &endpointer, 10);
		}

		temp= strtol( magneticheadingString, &endpointer, 10);
		if (endpointer != magneticheadingString )
		{
			magneticheading = temp + strtol( endpointer+1, &endpointer, 10);
		}
		success = true;
	}
	return success;
 }