#include <stdint.h>
#include "Uart.h"

#ifndef SERVO_DRIVER
#define SERVO_DRIVER

// Controls a single servo motor (of the hobbyist PWM type)
class ServoDriver
{
	public:

	// Sets up the servo driver to communicate with the physical servo controller module
	ServoDriver(Uart* uart);
	/*
	Make use of int32_t, int16_t, int8_t (32-bits, 16-bits, or 8-bits) 
	instead of int, short, or char.
	This will ensure that the length of the integer is always the same on different platforms.
	*/
    
    // You will likely want to give this constructor parameters that distinguish this specific motor
    // from any other one.
  
	// Sets the angle of this servo motor to the given number of degrees.
	bool setAngle(int8_t servoNumber, int16_t degrees);
	
    // Returns the angle that this motor was last set to. Nothing written currently.
	int16_t getAngle(int8_t servoNumber);

	// Sets the speed setting of the servo motor
	// Accepts values from 0 to 127
	bool setSpeed(int8_t servoNumber, int16_t speed);
	
	// Returns the speed setting of the servo motor. Nothing written currently. 
	int8_t getSpeed(int8_t servoNumber);

	private:

	Uart* uart;


};

#endif

#endif
