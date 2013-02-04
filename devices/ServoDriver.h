#ifndef SERVO_DRIVER
#define SERVO_DRIVER

// Controls a single servo motor (of the hobbyist PWM type)
class ServoDriver
{
	public:

	/*
	Make use of int32_t, int16_t, int8_t (32-bits, 16-bits, or 8-bits) 
	instead of int, short, or char.
	This will ensure that the length of the integer is always the same on different platforms.
	*/
    
    // You will likely want to give this constructor parameters that distinguish this specific motor
    // from any other one.
    ServoDriver();

	// Sets the angle of this servo motor to the given number of degrees.
	void setAngle(int16_t degrees);
	
    // Returns the angle that this motor was last set to.
	int16_t getAngle() const;


	private:

	// Your code here

};

#endif
