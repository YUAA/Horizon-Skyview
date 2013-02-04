#ifndef IMU_DECODER
#define IMU_DECODER

// Decodes bytes obtained from the Inertial Measurement Unit (IMU) so that relevant details may be accessed.
class IMUDecoder
{
	public:

	/*
	Make use of int32_t, int16_t, int8_t (32-bits, 16-bits, or 8-bits) 
	instead of int, short, or char. 
	This will ensure that the length of the integer is always the same on different platforms.
	*/

	// Value returned has the decimal point fixed at the 1000s place.
	// In degrees.
	int32_t getYaw() const;

	// Value returned has the decimal point fixed at the 1000s place.
	// In degrees.
	int32_t getPitch() const;

	// Value returned has the decimal point fixed at the 1000s place.
	// In degrees.
	int32_t getRoll() const;

	// Value returned has the decimal point fixed at the 1000s place.
	// In meters/second^2.
	int32_t getAccelerationX() const;

	// Value returned has the decimal point fixed at the 1000s place.
	// In meters/second^2.
	int32_t getAccelerationY() const;

	// Value returned has the decimal point fixed at the 1000s place.
	// In meters/second^2.
	int32_t getAccelerationZ() const;

	// Value returned has the decimal point fixed at the 1000s place.
	// In meters/second. An approximation calculated from acceleration.
	int32_t getIntegratedVelocityX() const;

    // Value returned has the decimal point fixed at the 1000s place.
	// In meters/second. An approximation calculated from acceleration.
	int32_t getIntegratedVelocityY() const;

	// Value returned has the decimal point fixed at the 1000s place.
	// In meters/second. An approximation calculated from acceleration.
	int32_t getIntegratedVelocityZ() const;

	// Value returned has the decimal point fixed at the 1000s place.
	// In meters/second. An approximation calculated from acceleration.
	int32_t getIntegratedPositionX() const;

	// Value returned has the decimal point fixed at the 1000s place.
	// In meters/second. An approximation calculated from acceleration.
	int32_t getIntegratedPositionY() const;

	// Value returned has the decimal point fixed at the 1000s place.
	// In meters/second. An approximation calculated from acceleration.
	int32_t getIntegratedPositionZ() const;
	
    // Passes the IMUDecoder an additional byte from the the IMU's output stream
    // to decode.
    // Returns true if the IMUDecoder has updated its parameters.
	bool decodeByte(int8_t newByte);

	private:

	// Your code here

};

#endif
