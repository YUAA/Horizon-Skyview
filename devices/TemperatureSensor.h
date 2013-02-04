#ifndef TEMPERATURE_SENSOR
#define TEMPERATURE_SENSOR

// Takes measurements from a physical temperature sensor device.
class TemperatureSensor
{
    public:

    /*
    Make use of int32_t, int16_t, int8_t (32-bits, 16-bits, or 8-bits) 
    instead of int, short, or char.
    This will ensure that the length of the integer is always the same on different platforms.
    */
    
    // You will likely want to give this constructor parameters that distinguish this specific sensor
    // from any other one.
    TemperatureSensor();
    
    // Value returned has the decimal point fixed at the 1000s place.
    // Value is in degrees celsius.
    // Returns the last temperature value read by the sensor.
    int32_t getTemperature() const;
    
    // Value returned has the decimal point fixed at the 1000s place.
    // Value is in degrees celsius.
    // Queries the sensor to determine its current temperature.
    // If the query is taking too long, the query should be cut off,
    // and INT32_MIN returned.
    int32_t readTemperature();

    private:

    // Your code here

};

#endif
