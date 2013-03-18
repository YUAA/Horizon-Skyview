#include <stdint.h>
#include <mutex>

#ifndef ADC_SENSOR_3008
#define ADC_SENSOR_3008

// Takes analog measurements from a physical MCP3008 ADC (analog to digital) device.
// This class will grab on to the SPI device /dev/spidev2.0 to communicate with the MCP3008.
// It will never let go of this device.
class ADCSensor3008
{
    public:

    /*
    Make use of int32_t, int16_t, int8_t (32-bits, 16-bits, or 8-bits) 
    instead of int, short, or char.
    This will ensure that the length of the integer is always the same on different platforms.
    */
    
    // Creates an ADCSensor3008 to read from the specified adc number of the physical device,
    // which is from 0 to 7. Different values will be saturated into this range.
    ADCSensor3008(int adcNumber);
    
    // Value is the 10-bit ADC conversion.
    // Returns the last converted value read from the sensor.
    // return -1 if no conversion has yet been made.
    int32_t getConversion() const;
    
    // Value is the 10-bit ADC conversion
    // Queries the sensor to convert an ADC value according to the adcNumber of this ADCSensor3008.
    // Returns -1 if the conversion is unable to succeed.
    int32_t readConversion();

    private:
        
    static int32_t initializeSpi();
    
    // The adc pin number to read from the physical device
    int adcNumber;
    
    // The last 10-bit value we converted, made by readConversion.
    int32_t lastConvertedValue;
    
    // A handle to the linux SPI device file, shared by all ADCSensor3008 instances
    static int spiAdcHandle;
    // A mutex to prevent problems with access to the shared spiAdcHandle
    static std::mutex spiAdcMutex;
    

};

#endif
