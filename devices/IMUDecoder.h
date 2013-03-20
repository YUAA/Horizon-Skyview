#define __STDC_LIMIT_MACROS
#include <stdint.h>

#include "nmeaParse/nmeaParse.h"

#ifndef IMU_DECODER
#define IMU_DECODER

typedef struct
{
    // In meters
    double coordX;
    double coordY;
    double coordZ;
    // In seconds
    double timestamp;
} Vector3D;

typedef struct
{
    // In degrees
    double yaw;
    double pitch;
    double roll;

    Vector3D accel;
    Vector3D spaceAccel;
    Vector3D spaceVel;
    Vector3D vel;
    Vector3D spacePos;
    Vector3D pos;
} ImuCalcData;

// Decodes bytes obtained from the Inertial Measurement Unit (IMU) so that relevant details may be accessed.
// Made to work with the VectorNav VN-100.
class IMUDecoder
{
	public:

	/*
	Make use of int32_t, int16_t, int8_t (32-bits, 16-bits, or 8-bits) 
	instead of int, short, or char. 
	This will ensure that the length of the integer is always the same on different platforms.
	*/
    
    // Initialize a new IMU decoder.
    IMUDecoder();

	// In degrees.
	double getYaw() const;

	// In degrees.
	double getPitch() const;

	// In degrees.
	double getRoll() const;

	// In meters/second^2.
	Vector3D getAcceleration() const;

	// In meters/second. An approximation calculated from acceleration.
	Vector3D getIntegratedVelocity() const;

	// In meters. An approximation calculated from acceleration.
	Vector3D getIntegratedPosition() const;
	
    // Passes the IMUDecoder an additional byte from the the IMU's output stream
    // to decode.
    // Returns true if the IMUDecoder has updated its parameters.
	bool decodeByte(int8_t newByte);

	private:
        
    // Parses strings to actual doubles, etc...
    void convertToCalcData();
    // Does integration with current and lastCalcData
    void calculateIntegratedData();
    // performs rotation transformations between absolute (based on gyro readings) and relative (to device) coordinates.
    static void rotateCoordinatesBodytoSpace(Vector3D* vectorIn, Vector3D* vectorOut, ImuCalcData* imuCalcData);
    static void rotateCoordinatesSpacetoBody(Vector3D* vectorIn, Vector3D* vectorOut, ImuCalcData* imuCalcData);
    static void integrateWithTime(Vector3D* vectorNew, Vector3D* vectorOld, Vector3D* vectorIntegrated);
    
    // Calculation type data
    ImuCalcData currentCalcData;
    ImuCalcData lastCalcData;

    // A NMEA parser for each tag type we want to interpret
	NmeaData nmeaData1;
    const int* nmeaIndices1;
    char** nmeaDatums1;
    
    // Directly parsed data
    char yawString[10];
    char pitchString[10];
    char rollString[10];
    char magnetXString[10];
    char magnetYString[10];
    char magnetZString[10];
    char accelXString[10];
    char accelYString[10];
    char accelZString[10];
    char gyroXString[10];
    char gyroYString[10];
    char gyroZString[10];

};

#endif
