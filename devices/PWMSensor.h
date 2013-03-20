#define __STDC_LIMIT_MACROS
#include <stdint.h>

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
    
    // Opens up a pwm input on the given numbered gpio pin. Because this uses interrupts,
    // the beaglebone says it is only able to have two interrupts on each gpio bank. Take care
    // with choices and which gpios are being used for interrupts.
    PWMSensor(int gpioNumber);
    
    // Value returned has the decimal point fixed at the 1000s place.
    // Value is in the range of -1 to 1. (-1000 to 1000)
    // Where -1 corresponds to 500us pulse-width, and 1 to 2500us.
    // Values measured beyond this are saturated into that range.
    // This will return the most recent valid (non INT32_MIN) value returned
    // by readValue(), or INT32_MIN anyway if there have not been any valid reads yet.
    int32_t getValue() const;
    
    // Value returned has the decimal point fixed at the 1000s place.
    // Value is in the range of -1 to 1. (-1000 to 1000)
    // Where -1 corresponds to 500us pulse-width, and 1 to 2500us.
    // Values measured beyond this are saturated into that range.
    // Queries the sensor to read the pulse-width; if there is not a new reading
    // since the last time this was called, INT32_MIN is returned.
    int32_t readValue();

    private:

    // File handle to the pwm input driver
    int pwmHandle;
    
    // Value for getValue to return
    int32_t lastValue;

};

#endif
