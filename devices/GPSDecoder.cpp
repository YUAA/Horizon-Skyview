#include "GPSDecoder.h"
//need to find out how to start GPS http://gpsd.berlios.de/vendor-docs/trimble/trimble-copernicus.pdf 107 140
 NmeaData  nmeaposition;
NmeaData 	nmeavelocity;							 

GPSDecoder::GPSDecoder()
{const int a []= {1,2, 3, 4, 6, 7, 8};
  char* b []={latitudeString, NSlatitudeString, longitudeString, EWlongitudeString,
		numSatellitesString,  hdopString, altitudeString};
	initNmea(&nmeaposition, "GPGGA,", 7, a, b);
const int c [] ={0, 1, 4};
 char* d []={ trueheadingString, magneticheadingString, speedString, };
	initNmea(&nmeavelocity, "GPVTG,", 3, c, d ); //179 speed in km/h
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
{return altitute;//in milli metters
}

int8_t GPSDecoder::getSatelliteCount() const
{ return realNumSatellites;
}

int32_t GPSDecoder::getTrueHeading() const
{		return trueheading;  // degres from true north increasing eastwardly
}

int32_t GPSDecoder::getMagneticHeading() const
{
	return magneticheading;
}

int32_t GPSDecoder::getSpeed() const
{//speed in milimeters per second
	return speed;
}

int32_t GPSDecoder::getHDOP() const
{
	return realHdop;// Your code here

}

	

	

bool GPSDecoder::decodeByte(int8_t newByte)
{
	if (parseNmea(&nmeaposition, newByte)) 
	{//lat comes in form AA.BBCCC  A degrees B minutes C decimal minutes  
	//lat retruned in form AA.BBB A degrees B decimal degrees decimal fixed between A and B
		latitude = atoi(latitudeString) * 1000;  //lat = AABBC
		latitude = ((int)latitude / 1000) * 1000 + (int)(latitude % 1000) / 600;
		if (0 == strcmp( NSlatitudeString, "S") ) latitude= latitude*-1;
			longitude= atoi(longitudeString)*1000;	
		longitude= ((int)longitude/1000)*1000+ (int)(longitude%1000)/600;
		if (0 == strcmp( EWlongitudeString, "E") ) latitude= latitude*-1;

		altitute=atoi( altitudeString)/1000;

		realNumSatellites =atoi( numSatellitesString);

		realHdop = atoi (hdopString);
	}
	if (parseNmea(&nmeavelocity, newByte)){ 
		speed =(int32_t) atoi(speedString)*10000/36;
		trueheading= atoi(trueheadingString);
		magneticheading= atoi( magneticheadingString);
	}
	return true;
 }
//all dicnances in Km angles converted from degrees to radians TNV is vector to true north from center of earth
	//there is a fast and ruff solition for CT magnetic delination is about -13 degrees 
	//for a more general solution I need to use the angle betwen the planes formed by MN earth center and position and TN earth's center and posstion 
	// the trig fuctions are writen to require and out put doubles how acurate do we need to be??
	/*const int32_t PI 3141;
	int32_t TNV[3]={0, 0, 6378}
	int32_t MNV[3]={(int)6378*sin(81.3*PI/180)*cos(110.8*PI/180),    
				(int) 6378*sin(81.3*PI/180)*sin(110.8*PI/180),
				(int) 6378* cos(81.3*PI/180)};
	int32_t PV[3]={(int)6378*sin(Lat*PI/180)*cos(lng*PI/180),    
				(int) 6378*sin(lat*PI/180)*sin(lng.8*PI/180),
				(int) 6378* cos(lat*PI/180)};
	int32_t MNXP[3]=CrossProduct(MNV, PV);
	int32_t TNXP[3]=CrossProduct(TNV, PV);
	int32_t finnal= (int) (DotProduct(MNXP, TNXP)/Vectorlenght(TNXP))/Vectorlenght(MNXP);
	final= (int) acos((double) finnal)*/
//do i need to send requests for the information?
/*int32_t CrossProduct(int32_t a[], int32_t b[]){
	int32_t Product[3];
	Product[0] = (a[1] * b[2]) - (a[2] * b[1]);
	Product[1] = (a[2] * b[0]) - (a[0] * b[2]);
	Product[2] = (a[0] * b[1]) - (a[1] * b[0]);
	return product;
}
int32_t DotProduct (int32_t a[], int32_t b[]){
	int32_t Product[3];
	for (int32_t i=0; i<3; i++) Product[i]= a[i]*b[i];
	return Product 
}
int32_t Vectorlenght (int32_t a[]){
	int32_t sum=0;
	for(int32_t i=0; i<3; i++) sum= sum+ a[i]*a[i];
	return (int)sqrt( (double) sum);
}*/


