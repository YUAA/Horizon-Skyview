#include "GPSDecoder.h"

GPSDecoder::GPSDecoder()
{
	nmeaPositionIndices[0] = 1;
	nmeaPositionIndices[1] = 2;
	nmeaPositionIndices[2] = 3;
	nmeaPositionIndices[3] = 4;
	nmeaPositionIndices[4] = 6;
	nmeaPositionIndices[5] = 7;
	nmeaPositionIndices[6] = 8;
	nmeaPositionDatums[0] = latitudeString;
	nmeaPositionDatums[1] = NSlatitudeString;
	nmeaPositionDatums[2] = longitudeString;
	nmeaPositionDatums[3] = EWlongitudeString;
	nmeaPositionDatums[4] = numSatellitesString;
	nmeaPositionDatums[5] = hdopString;
	nmeaPositionDatums[6] = altitudeString;
	initNmea(&nmeaPosition, "GPGGA,", 1, nmeaPositionIndices, nmeaPositionDatums);

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
	if (parseNmea(&nmeaPosition, newByte)) 
	{//lat comes in form AA.BBCCC  A degrees B minutes C decimal minutes  
	//lat retruned in form AA.BBB A degrees B decimal degrees decimal fixed between A and B
		latitude = atoi(latitudeString) * 1000;  //lat = AABBC
		latitude = ((int)latitude / 1000) * 1000 + (int)(latitude % 1000) / 600;
		if (0 == strcmp( NSlatitudeString, "S") ) {
			latitude = -latitude;
		}
		longitude= atoi(longitudeString)*1000;	
		longitude= ((int)longitude/1000)*1000+ (int)(longitude%1000)/600;
		if (0 == strcmp( EWlongitudeString, "E") ) {
			latitude = -latitude;
		}

		altitute=atoi( altitudeString)/1000;
		
		realNumSatellites =atoi( numSatellitesString);
		//strtol(numSatellitesString, &endPointer, 10);
		realHdop = atoi (hdopString);
		success = true;
	}
	if (parseNmea(&nmeaVelocity, newByte)){ 
		char* endpointer; 
		speed =(int32_t) strtol(speedString, &endpointer, 10)*10000/36;
		if(endpointer!=NULL){
			speed=speed+ (int32_t) strtol(endpointer+1, &endpointer, 10)*1000/36;
		}
		trueheading= atoi(trueheadingString);
		magneticheading= atoi( magneticheadingString);
		success = true;
	}
	return success;
 }