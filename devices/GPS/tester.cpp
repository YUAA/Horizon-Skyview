#include "GPSDecoder.h"
#include <iostream> 
#include <fstream>
#include <string>
using namespace std;
int main() {
	GPSDecoder myGPS=GPSDecoder();
	ifstream in;
	ofstream out;
	in.open("../Yuaa/oldDATA.txt");
	char data;
	for(int i=0; i<1000; i++){
		in >> data;
		if (in.fail()) {
			break;
		}
		myGPS.decodeByte(data);
		if (myGPS.getSpeed() != INT32_MIN)
		{
			cout << myGPS.getSpeed() << " ";
		}

	}
	system("pause");
	return 0;
	/*for(int i =0; i<200; i++){
	a.getHDOP();
	a.getAltitude();
	a.getSatelliteCount();
	a.getTrueHeading();
	a.getSpeed();
	a.getLatitude();
	a.getLongitude();
	a.getAltitude();
	a.getMagneticHeading();
	}*/
}