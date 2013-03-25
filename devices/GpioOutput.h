#ifndef GPIO_OUTPUT
#define GPIO_OUTPUT

// Takes measurements from a physical temperature sensor device.
class GpioOutput
{
    public:

    /*
    Make use of int32_t, int16_t, int8_t (32-bits, 16-bits, or 8-bits) 
    instead of int, short, or char.
    This will ensure that the length of the integer is always the same on different platforms.
    */
    
    // Set up the given gpio number pin for writing.
    GpioOutput(int gpioNumber);
    
    // Set the value of the gpio pin to either 0 or 1 (everything else besides 0).
    // Returns -1 on error. 0 otherwise.
    int setValue(int value);

    private:

    std::string gpioFileName;

};

#endif
