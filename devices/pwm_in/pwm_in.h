#include <linux/ioctl.h>

#ifndef PWM_IN_H
#define PWM_IN_H

#define PWM_IN_IOC_MAGIC '!'

// Set the interrupt number and start the beast. You can only call this once. (unless it fails, then you can try again)
#define PWM_IN_SET_GPIO _IO(PWM_IN_IOC_MAGIC, 0)
// Get the interrupt number
#define PWM_IN_GET_GPIO _IO(PWM_IN_IOC_MAGIC, 1)

// Read the most recent new pulse width from the PWM interrupt.
// -1 if there has not been a new one since the last time this was called.
#define PWM_IN_READ_PULSE_WIDTH _IO(PWM_IN_IOC_MAGIC, 2)


#endif