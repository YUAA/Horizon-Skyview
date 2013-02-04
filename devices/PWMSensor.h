#ifndef PWM_SENSOR
#define PWM_SENSOR

// Measures the duty-cycle of a 50hz PWM signal, as used with hobbyist servos and radio controls.
// Reports this measurement as an angle in degrees as it would control a hobbyist servo.
class PWMSensor
{
    public:

    /*
    Make use of int32_t, int16_t, int8_t (32-bits, 16-bits, or 8-bits) 
    instead of int, short, or char.
    This will ensure that the length of the integer is always the same on different platforms.
    */
    
    // You will likely want to give this constructor parameters that distinguish this specific sensor
    // from any other one.
    PWMSensor();
    
    // Value returned has the decimal point fixed at the 1000s place.
    // Value is in degrees, interpreting the PWM 
    // Returns the last value read by the sensor.
    int32_t getAngle() const;
    
    // Value returned has the decimal point fixed at the 1000s place.
    // Value is in percentage, so 100% would be 100 _to the left_ of the decimal point.
    // Queries the sensor to determine its current humidity.
    // If the query is taking too long, the query should be cut off,
    // and INT32_MIN returned.
    int32_t readAngle();

    private:

    // Your code here

};

#endif
