#include "IMUDecoder.h"
#include <math.h>
#include <stdlib.h>
#include <sys/time.h>

#define PI 3.1415926535897932384
#define IMU_TAG "VNYMR,"

IMUDecoder::IMUDecoder()
{
    nmeaIndices[0] = 0;
    nmeaIndices[1] = 1;
    nmeaIndices[2] = 2;
    nmeaIndices[3] = 3;
    nmeaIndices[4] = 4;
    nmeaIndices[5] = 5;
    nmeaIndices[6] = 6;
    nmeaIndices[7] = 7;
    nmeaIndices[8] = 8;
    nmeaIndices[9] = 9;
    nmeaIndices[10] = 10;
    nmeaIndices[11] = 11;
    nmeaDatums[0] = yawString;
    nmeaDatums[1] = pitchString;
    nmeaDatums[2] = rollString;
    nmeaDatums[3] = magnetXString;
    nmeaDatums[4] = magnetYString;
    nmeaDatums[5] = magnetZString;
    nmeaDatums[6] = accelXString;
    nmeaDatums[7] = accelYString;
    nmeaDatums[8] = accelZString;
    nmeaDatums[9] = gyroXString;
    nmeaDatums[10] = gyroYString;
    nmeaDatums[11] = gyroZString;
    initNmea(&nmeaData, IMU_TAG, sizeof(nmeaDatums) / sizeof(sizeof(*nmeaDatums)), nmeaIndices, nmeaDatums);
    /*
    initNmea(&nmeaData, IMU_TAG, sizeof(nmeaDatums), 
             (const int[]) {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
             (char*[]) {yawString, pitchString, rollString, magnetXString, magnetYString, magnetZString, accelXString, accelYString, accelZString, gyroXString, gyroYString, gyroZString});
    */
}

bool IMUDecoder::decodeByte(int8_t newByte)
{
    if (parseNmea(&nmeaData, newByte))
    {
        // It parsed! Something happened!
        lastCalcData = currentCalcData;
        convertToCalcData();
        
        // Time stamp our just converted time!
        struct timeval currentTime;
        gettimeofday(&currentTime, NULL);
        currentCalcData.accel.timestamp = (double)currentTime.tv_sec + (double)currentTime.tv_usec / 1000000;
        
        calculateIntegratedData();
        return true;
    }
    
    return false;
}

void IMUDecoder::convertToCalcData()
{
    currentCalcData.yaw = strtod(yawString, NULL);
    currentCalcData.pitch = strtod(pitchString, NULL);
    currentCalcData.roll = strtod(rollString, NULL);
    currentCalcData.accel.coordX = strtod(accelXString, NULL);
    currentCalcData.accel.coordY = strtod(accelYString, NULL);
    currentCalcData.accel.coordZ = strtod(accelZString, NULL);
}

void IMUDecoder::calculateIntegratedData()
{
    rotateCoordinatesBodytoSpace(&currentCalcData.accel, &currentCalcData.spaceAccel, &currentCalcData);
    integrateWithTime(&currentCalcData.spaceAccel, &lastCalcData.spaceAccel, &currentCalcData.spaceVel);
    rotateCoordinatesSpacetoBody(&currentCalcData.spaceVel, &currentCalcData.vel, &currentCalcData);
    integrateWithTime(&currentCalcData.spaceVel, &lastCalcData.spaceVel, &currentCalcData.spacePos);
    rotateCoordinatesSpacetoBody(&currentCalcData.spacePos, &currentCalcData.pos, &currentCalcData); 
}

void IMUDecoder::rotateCoordinatesBodytoSpace(Vector3D* vectorIn, Vector3D* vectorOut, ImuCalcData* imuCalcData)
{
    double cosYaw = cos(imuCalcData->yaw);
    double sinYaw = sin(imuCalcData->yaw);
    double cosPitch = cos(imuCalcData->pitch);
    double sinPitch = sin(imuCalcData->pitch);
    double cosRoll = cos(imuCalcData->roll);
    double sinRoll = sin(imuCalcData->roll);
    vectorOut->coordX = cosYaw*(cosPitch*vectorIn->coordX + sinPitch*(sinRoll*vectorIn->coordY + cosRoll*vectorIn->coordZ)) - sinYaw*(cosRoll*vectorIn->coordY - sinRoll*vectorIn->coordZ);
    vectorOut->coordY = sinYaw*(cosPitch*vectorIn->coordX + sinPitch*(sinRoll*vectorIn->coordY + cosRoll*vectorIn->coordZ)) + cosYaw*(cosRoll*vectorIn->coordY - sinRoll*vectorIn->coordZ);
    vectorOut->coordZ = -sinPitch*vectorIn->coordX + cosPitch*(sinRoll*vectorIn->coordY + cosRoll*vectorIn->coordZ);
    vectorOut->timestamp = vectorIn->timestamp;
}  
   
void IMUDecoder::rotateCoordinatesSpacetoBody(Vector3D* vectorIn, Vector3D* vectorOut, ImuCalcData* imuCalcData)
{
    double cosYaw = cos(imuCalcData->yaw);
    double sinYaw = sin(imuCalcData->yaw);
    double cosPitch = cos(imuCalcData->pitch);
    double sinPitch = sin(imuCalcData->pitch);
    double cosRoll = cos(imuCalcData->roll);
    double sinRoll = sin(imuCalcData->roll);
    vectorOut->coordX =  cosPitch*(cosYaw*vectorIn->coordX + sinYaw*vectorIn->coordY) - sinPitch*vectorIn->coordZ;
    vectorOut->coordY =  cosRoll*(-sinYaw*vectorIn->coordX + cosYaw*vectorIn->coordY) + sinRoll*(sinPitch*(cosYaw*vectorIn->coordX + sinYaw*vectorIn->coordY) + cosPitch*vectorIn->coordZ);
    vectorOut->coordZ = -sinRoll*(-sinYaw*vectorIn->coordX + cosYaw*vectorIn->coordY) + cosRoll*(sinPitch*(cosYaw*vectorIn->coordX + sinYaw*vectorIn->coordY) + cosPitch*vectorIn->coordZ);
    vectorOut->timestamp = vectorIn->timestamp;
}  

void IMUDecoder::integrateWithTime(Vector3D* vectorNew, Vector3D* vectorOld, Vector3D* vectorIntegrated)
{
    //trapazoid integration scheme 
    vectorIntegrated->coordX = (vectorNew->timestamp - vectorOld->timestamp)*(vectorNew->coordX + vectorOld->coordX)/2;
    vectorIntegrated->coordY = (vectorNew->timestamp - vectorOld->timestamp)*(vectorNew->coordY + vectorOld->coordY)/2;
    vectorIntegrated->coordZ = (vectorNew->timestamp - vectorOld->timestamp)*(vectorNew->coordZ + vectorOld->coordZ)/2;
    vectorIntegrated->timestamp = vectorNew->timestamp;
}

double IMUDecoder::getYaw() const
{
    return currentCalcData.yaw;
}

double IMUDecoder::getPitch() const
{
    return currentCalcData.pitch;
}

double IMUDecoder::getRoll() const
{
    return currentCalcData.roll;
}

Vector3D IMUDecoder::getAcceleration() const
{
    return currentCalcData.accel;
}

Vector3D IMUDecoder::getIntegratedVelocity() const
{
    return currentCalcData.vel;
}

Vector3D IMUDecoder::getIntegratedPosition() const
{
    return currentCalcData.pos;
}
