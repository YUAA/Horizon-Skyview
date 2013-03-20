//This define needed to help out KDevelop
#ifndef __KERNEL__
#define __KERNEL__
#endif

//Device driver includes
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <asm/system.h> /* cli(), *_flags */
#include <asm/uaccess.h> /* copy_from/to_user */
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/delay.h>

#include "pwm_in.h"

MODULE_LICENSE("Dual BSD/GPL");

#define UART_BUFFER_SIZE 4096

int pwm_in_init(void);
void pwm_in_exit(void);

int pwm_in_open(struct inode* inode, struct file* filePointer);
int pwm_in_release(struct inode* inode, struct file* filePointer);
ssize_t pwm_in_read(struct file* filePointer, char* dataBuffer, size_t dataLength, loff_t* filePosition);
ssize_t pwm_in_write(struct file* filePointer, const char* dataBuffer, size_t dataLength, loff_t* filePosition);
long pwm_in_ioctl(struct file* filePointer, unsigned int cmd, unsigned long arg);

module_init(pwm_in_init)
module_exit(pwm_in_exit)

//Access functions for file access of this device
struct file_operations pwm_in_operations = {
    .read = pwm_in_read,
    .write = pwm_in_write,
    .open = pwm_in_open,
    .release = pwm_in_release,
    .unlocked_ioctl = pwm_in_ioctl
};

//Major Number of the driver, used for linking to a file by linux.
int majorNumber = 445;

//The number of times this device has been opened
//Use to keep track of interrupt enabling/disabling
int deviceOpens = 0;

// Structure that contains all the state that governs how the GPIO UART works
typedef struct
{
    // -1 before system is started.
    int pwmGpio;
    
    struct timespec lastRisingEdge;
    
    // The tasklet structure that runs the bottom half of the interrupt handling
    struct tasklet_struct pwmIsrBottomHalfTasklet;
    
    // Microseconds measured pulse width. -1 if there is none or it has already been read.
    long pulseWidth;
    
    // For TESTING
    int interruptsReceived;
    int interruptErrors;
    int interruptLastValue;
} PwmIn;


void addTime(struct timespec* time, long nanoseconds)
{
    time->tv_nsec += nanoseconds;
    if (time->tv_nsec < 0)
    {
        time->tv_sec += time->tv_nsec / 1000000000L - 1;
        time->tv_nsec %= 1000000000L;
        time->tv_nsec += 1000000000L;
    }
    else if (time->tv_nsec >= 1000000000L)
    {
        time->tv_sec += time->tv_nsec / 1000000000L;
        time->tv_nsec %= 1000000000L;
    }
}

long timeDifference(const struct timespec* currentTime, const struct timespec* oldTime)
{
    return  ((currentTime->tv_sec - oldTime->tv_sec) * 1000000000L +
             (currentTime->tv_nsec - oldTime->tv_nsec));
}

irqreturn_t pwmIsr(int irq, void* dev_id, struct pt_regs* regs);

// The bottom half of the px pin interrupt handler
// This bottom half is implemented as a tasklet
void pwmIsrBottomHalfFunction(unsigned long data)
{
    PwmIn* pwmIn = (PwmIn*)data;
    
    printk(KERN_INFO "PWM In: Measured pulse width of: %ld", pwmIn->pulseWidth);
}

// Interrupt handler for the rx pin
irqreturn_t pwmIsr(int irq, void* dev_id, struct pt_regs* regs)
{
    PwmIn* pwmIn = (PwmIn*)dev_id;
    
    // First, sample the pin
    int pinValue = gpio_get_value(pwmIn->pwmGpio);

    // TESTING
    pwmIn->interruptsReceived++;
    if (pinValue == pwmIn->interruptLastValue)
    {
        pwmIn->interruptErrors++;
    }

    // Get current time
    struct timespec currentTime;
    get_monotonic_boottime(&currentTime);
    
    // Falling edge? Without lost interrupt?
    if (pinValue == 0 && pwmIn->interruptLastValue != 0)
    {
        long timePassed = timeDifference(&currentTime, &pwmIn->lastRisingEdge);
        pwmIn->pulseWidth = timePassed / 1000; // nano to micro
    }
    else if (pinValue == 1)
    {
        pwmIn->lastRisingEdge = currentTime;
    }
    
    pwmIn->interruptLastValue = pinValue;
    
    // Schedule tasklet bottom half
    // This will do the job of processing the passed time and making sense out of it
    // turning it into bits and bits into uart frames and those into bytes.
    tasklet_schedule(&pwmIn->pwmIsrBottomHalfTasklet);
    
    return IRQ_HANDLED;
}

// Opens and configures the needed GPIO pins
// And sets up the appropraite interrupts on them
int initInterrupt(PwmIn* pwmIn, int interruptNumber)
{
    // Do not allow init if we already have done it
    if (pwmIn->pwmGpio != -1)
    {
        return -EPERM;
    }
    
    // Get the GPIOs for our interrupt handler
    if (gpio_request(interruptNumber, "PWM In Interrupt") ||
        gpio_direction_input(interruptNumber) ||
        gpio_export(interruptNumber, 0))
    {
        printk(KERN_ERR "PWM In: Could not obtain pwm interrupt pin\n");
        return -1;
    }
    
    // Register our interrupt handler
    // We want to get interrupts on both the rising and falling edges so we can get the full picture
    // of what the input signal is on the rx pin.
    if(request_irq(gpio_to_irq(interruptNumber),
                         (irq_handler_t)pwmIsr, //TESTING
                         (1*IRQF_TRIGGER_RISING) | IRQF_TRIGGER_FALLING, "PWM In interrupt ISR", pwmIn))
    {
        printk(KERN_ERR "PWM In: Could not register irq\n");
        return -1;
    }
    
    pwmIn->pwmGpio = interruptNumber;
    
    printk(KERN_INFO "PWM In: Started\n");
    
    return 0;
}

int releasePwmIn(PwmIn* pwmIn)
{
    if (pwmIn->pwmGpio != -1)
    {
        // Release everything!
        gpio_free(pwmIn->pwmGpio);
        free_irq(gpio_to_irq(pwmIn->pwmGpio), pwmIn);
        pwmIn->pwmGpio = -1;
    }
    
    return 0;
}

long pwm_in_ioctl(struct file* filePointer, unsigned int cmd, unsigned long arg)
{
    PwmIn* pwmIn = (PwmIn*)filePointer->private_data;
    switch (cmd)
    {
        case PWM_IN_SET_GPIO:
            return initInterrupt(pwmIn, arg);
        case PWM_IN_GET_GPIO:
            return pwmIn->pwmGpio;
        case PWM_IN_READ_PULSE_WIDTH:;
            long result = pwmIn->pulseWidth;
            pwmIn->pulseWidth = -1;
            return result;
    }
    printk(KERN_ERR "PWM In: IOCTL unknown");
    return -ENOTTY;
}

int pwm_in_init(void) {
    //Register the device
    int result = register_chrdev(majorNumber, "pwm_in", &pwm_in_operations);
    if (result < 0) {
        printk(
            KERN_ERR "PWM In: Cannot obtain major number %d\n", majorNumber);
        return result;
    }
    
    printk(KERN_INFO "PWM In: Inserting pwm_in module\n");
    return 0;
}

void pwm_in_exit(void) {
    //Unregister the device
    unregister_chrdev(majorNumber, "pwm_in");
    
    printk(KERN_INFO "PWM In: Removing pwm_in module\n");
}


int pwm_in_open(struct inode* inode, struct file* filePointer)
{
    // Allocate memory for the PwmIn structure
    // GFP_KERNEL for an allocation that can take its time
    filePointer->private_data = kmalloc(sizeof(PwmIn), GFP_KERNEL);
    if (!filePointer->private_data)
    {
        // Failed to open the uart... No memory!
        return -ENOMEM;
    }
    
    // SO MUCH INITIALIZATION!!!
    PwmIn* pwmIn = (PwmIn*)filePointer->private_data;
    // No pin set, no pulse width measured yet
    pwmIn->pwmGpio = -1;
    pwmIn->pulseWidth = -1;
    
    tasklet_init(&pwmIn->pwmIsrBottomHalfTasklet, pwmIsrBottomHalfFunction, (unsigned long)pwmIn);
    
    pwmIn->interruptLastValue = false; // No pulse yet
   
    // TESTING
    pwmIn->interruptsReceived = 0;
    pwmIn->interruptErrors = 0;
 
    printk(KERN_INFO "PWM In: Device opened");
    
    return 0;
}

int pwm_in_release(struct inode* inode, struct file* filePointer)
{
    PwmIn* pwmIn = (PwmIn*)filePointer->private_data;
    
    // Make sure to stop it!
    releasePwmIn(pwmIn);
 
    // Release the memory
    kfree(filePointer->private_data);
    // Just for extra safety...
    filePointer->private_data = NULL;
    
    printk(KERN_INFO "PWM In: Interrupts received: %d", pwmIn->interruptsReceived);
    printk(KERN_INFO "PWM In: Interrupt errors: %d", pwmIn->interruptErrors);
    
    printk(KERN_INFO "PWM In: Device closed");
    
    return 0;
}

ssize_t pwm_in_read(struct file* filePointer, char* dataBuffer, size_t dataLength, loff_t* filePosition)
{
    return -1;
}

ssize_t pwm_in_write(struct file* filePointer, const char* dataBuffer, size_t dataLength, loff_t* filePosition)
{
    return -1;
}

