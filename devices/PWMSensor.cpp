#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "PWMSensor.h"
#include "pwm_in/pwm_in.h"

PWMSensor::PWMSensor(int gpioNumber)
{
    lastValue = INT32_MIN;
    
    pwmHandle = open("/dev/pwm_in", O_RDWR);
    if (pwmHandle == -1)
    {
        perror("PWM Sensor: opening /dev/pwm_in");
        return;
    }

    // Configure it
    if (ioctl(pwmHandle, PWM_IN_SET_GPIO, gpioNumber))
    {
        perror("PWM Sensor: setting gpio");
        
        close(pwmHandle);
        pwmHandle = -1;
    }
}

int32_t PWMSensor::getValue() const
{
    return lastValue;
}

int32_t PWMSensor::readValue()
{
    long pulseWidthValue = ioctl(pwmHandle, PWM_IN_READ_PULSE_WIDTH);
    if (pulseWidthValue == -1)
    {
        return INT32_MIN;
    }
    lastValue = pulseWidthValue - 1500;
    return lastValue;
}
