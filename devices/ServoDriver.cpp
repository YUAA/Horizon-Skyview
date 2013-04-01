#include "ServoDriver.h"

ServoDriver::ServoDriver(Uart* uart)
{
    this->uart = uart;   
}

bool ServoDriver::setAngle(int8_t servoNumber, int16_t degrees)
{
    if (servoNumber > 8 || servoNumber < 1)
    {
        return false;
    }

    int16_t position = (degrees < 0) ? 0 : ((degrees > 5000) ? 5000 : degrees);
    position += 500;
    //Calculate data bytes from position
    int8_t data2 = position & 127;
    int8_t data1 = position >> 7;

    //Start Byte
    uart->writeByte(0x80);

    //Device ID - Device ID number 0x01 for 8-Servo Controller
    uart->writeByte(0x01);

    //Command: 0x04 is set position mode
    uart->writeByte(0x04);

    //Servo Number
    uart->writeByte(servoNumber);
    
    //First Data Byte
    uart->writeByte(data1);

    //Second Data Byte
    uart->writeByte(data2);
    
    //It might be useful to store the last servo positions in the class    
    
    return true;
}

int16_t ServoDriver::getAngle(int8_t servoNumber) 
{
 //To be written if we stor the positions in the class   
}

bool ServoDriver::setSpeed(int8_t servoNumber, int16_t speed)
{
    if (servoNumber < 1 || servoNumber > 8)
    {
        return false;
    }

    int8_t data3 = (speed < 0) ? 0 : ((speed > 127) ? 127 : speed);
    
    //Start Byte
    uart->writeByte(0x80);

    //Device ID - Device ID number 0x01 for 8-Servo Controller
    uart->writeByte(0x01);

    //Command: 0x01 is set speed mode
    uart->writeByte(0x01);

    //Servo Number
    uart->writeByte(servoNumber);

    //Pass the speed data
    uart->writeByte(data3);
    
    return true;
}

int8_t getSpeed(int8_t servoNumber)
{
    
//Only possible if store the values in the class and return them
}


