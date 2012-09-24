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

#include "gpio_uart.h"

#ifndef OMAP_GPIO_IRQ
#define OMAP_GPIO_IRQ(nr)       (nr)
#endif

MODULE_LICENSE("Dual BSD/GPL");

#define UART_BUFFER_SIZE 4096

// Atomic GCC primitive function
// Sets variable to newValue if it is still equal to oldValue
// (type* variable, type oldValue, type newValue)
//bool __sync_bool_compare_and_swap (...);

// Structure that contains all the state that governs how the GPIO UART works
typedef struct
{
    int rxPin;
    int txPin;
    
    int baudRate;
    bool invertingLogic;
    bool parityBit;
    bool secondStopBit;
    
    // We have two circular buffers for data
    unsigned char rxBuffer[UART_BUFFER_SIZE];
    unsigned char txBuffer[UART_BUFFER_SIZE];
    
    // The index of the first byte of data in the buffer
    int rxBufferStart;
    int txBufferStart;
    
    // The index of the byte following the last byte of data in the buffer
    // The buffer is considered full when this points to the the bufferStart - 1
    // Thus, one byte of the buffer between the start and tail is unused when full.
    int rxBufferTail;
    int txBufferTail;
    
    // Individual bits that have been received by rx interrupts
    int rxBitBuffer;
    // The time the last interrupt was fired on the rx line
    struct timespec rxLastInterruptTime;
    // The value of the rx pin at the time of the last interrupt
    bool rxLastValue;
    
    // This timer will schedule the transfer of bytes
    // That is, a byte or two will be sent every time the timer hits
    struct timer_list txTimer;
    
    // A flag that tells whether the uart is currently operating
    bool isRunning;
    
} GpioUart;

// Adds a byte to the circular tx buffer. This method is quasi-thread safe.
// It is ok for this method and the corresponding removeTxByte method to execute
// concurrently, but it is not safe multiple instances of this method to execute concurrently.
void addTxByte(GpioUart* uart, unsigned char value)
{
    int nextTail = (uart->txBufferTail + 1) % UART_BUFFER_SIZE;
    
    // Are we full?
    if (uart->txBufferStart == nextTail)
    {
        // It is safe to set this value, because this byte spot is unused.
        uart->txBuffer[uart->txBufferTail] = value;
        
        // Attempt to set the new start value if it hasn't changed
        // if it has already, then that is ok
        // (then the buffer is not really full and this reduces to the else case)
        // (the byte we were going to remove to make more room has already been sent).
        int nextStart = (nextTail + 1) % UART_BUFFER_SIZE;
        __sync_bool_compare_and_swap(&uart->txBufferStart, nextTail, nextStart);
        
        // We set the tail after the start so that the buffer never appears empty (tail == start)
        // This is safe because the remove function does not modify the tail.
        uart->txBufferTail = nextTail;
    }
    else
    {
        uart->txBuffer[uart->txBufferTail] = value;
        uart->txBufferTail = nextTail;
    }
}

// Removes and returns a byte from the circular tx buffer. This method is thread safe.
unsigned char removeTxByte(GpioUart* uart)
{    
    // Are we empty?
    if (uart->txBufferStart == uart->txBufferTail)
    {
        return -1;
    }
    else
    {
        // Loop until we succeed in popping a value.
        while (true)
        {
            int start = uart->txBufferStart;
            int value = uart->txBuffer[start];
            
            int nextStart = (start + 1) % UART_BUFFER_SIZE;
            
            // Attempt to update start
            if (__sync_bool_compare_and_swap(&uart->txBufferStart, start, nextStart))
            {
                // Start did not get changed in the interim, so our value is correct.
                return value;
            }
        }
    }
}

// Adds a byte to the circular rx buffer. This method is quasi-thread safe.
// It is ok for this method and the corresponding removeRxByte method to execute
// concurrently, but it is not safe multiple instances of this method to execute concurrently.
void addRxByte(GpioUart* uart, unsigned char value)
{
    int nextTail = (uart->rxBufferTail + 1) % UART_BUFFER_SIZE;
    
    // Are we full?
    if (uart->rxBufferStart == nextTail)
    {
        // It is safe to set this value, because this byte spot is unused.
        uart->rxBuffer[uart->rxBufferTail] = value;
        
        // Attempt to set the new start value if it hasn't changed
        // if it has already, then that is ok
        // (then the buffer is not really full and this reduces to the else case)
        // (the byte we were going to remove to make more room has already been sent).
        int nextStart = (nextTail + 1) % UART_BUFFER_SIZE;
        __sync_bool_compare_and_swap(&uart->rxBufferStart, nextTail, nextStart);
        
        // We set the tail after the start so that the buffer never appears empty (tail == start)
        // This is safe because the remove function does not modify the tail.
        uart->rxBufferTail = nextTail;
    }
    else
    {
        uart->rxBuffer[uart->rxBufferTail] = value;
        uart->rxBufferTail = nextTail;
    }
}

// Removes and returns a byte from the circular rx buffer. This method is thread safe.
unsigned char removeRxByte(GpioUart* uart)
{    
    // Are we empty?
    if (uart->rxBufferStart == uart->rxBufferTail)
    {
        return -1;
    }
    else
    {
        // Loop until we succeed in popping a value.
        while (true)
        {
            int start = uart->rxBufferStart;
            int value = uart->rxBuffer[start];
            
            int nextStart = (start + 1) % UART_BUFFER_SIZE;
            
            // Attempt to update start
            if (__sync_bool_compare_and_swap(&uart->rxBufferStart, start, nextStart))
            {
                // Start did not get changed in the interim, so our value is correct.
                return value;
            }
        }
    }
}

int uart_init(void);
void uart_exit(void);

int uart_open(struct inode* inode, struct file* filePointer);
int uart_release(struct inode* inode, struct file* filePointer);
ssize_t uart_read(struct file* filePointer, char* dataBuffer, size_t dataLength, loff_t* filePosition);
ssize_t uart_write(struct file* filePointer, const char* dataBuffer, size_t dataLength, loff_t* filePosition);
long uart_ioctl(struct file* filePointer, unsigned int cmd, unsigned long arg);

module_init(uart_init)
module_exit(uart_exit)

//Access functions for file access of this device
struct file_operations uart_operations = {
    .read = uart_read,
    .write = uart_write,
    .open = uart_open,
    .release = uart_release,
    .unlocked_ioctl = uart_ioctl
};

//Major Number of the driver, used for linking to a file by linux.
int majorNumber = 441;

//The number of times this device has been opened
//Use to keep track of interrupt enabling/disabling
int deviceOpens = 0;

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

void addTimeAndBusyWait(struct timespec* time, long nanoseconds)
{
    addTime(time, nanoseconds);
    struct timespec actualTime;
    do
    {
        get_monotonic_boottime(&actualTime);
    } while (actualTime.tv_sec < time->tv_sec || (actualTime.tv_sec == time->tv_sec && actualTime.tv_nsec < time->tv_nsec));
}

// Holds a GPIO pin to its old value until the needed time has past.
// Then sets the gpio to a new value and updates the lastBitTime value.
// Also takes inverting logic into account
void holdAndSetTx(GpioUart* uart, bool value, struct timespec* lastBitTime, long nanosecondsNeeded)
{
    addTimeAndBusyWait(lastBitTime, nanosecondsNeeded);
    gpio_set_value(uart->txPin, uart->invertingLogic ? !value : value);
}

void txSender(unsigned long argument)
{
    GpioUart* uart = (GpioUart*)argument;
    
    unsigned long savedFlags;
    spinlock_t locker;
    spin_lock_init(&locker);
    
    // We use the spinlock to disable interrupts so that we can busy-wait exact timing
    spin_lock_irqsave(&locker, savedFlags);
    
    struct timespec lastBitTime;
    get_monotonic_boottime(&lastBitTime);
    
    // What is the delay for a bit in nanoseconds?
    long bitDelay = 1000000000L / uart->baudRate;
    
    // We can send multiple bytes at once, if it will not take too long.
    // Let us take up to half a millisecond if our first byte will take less than that
    // (so we will go as long as necessary to get one byte out)
    int bitsFrame = 10 + (uart->secondStopBit ? 1 : 0) + (uart->parityBit ? 1 : 0);
    int bytesToDo = 500000 / bitDelay / bitsFrame;
    // But at least one byte...
    bytesToDo = (bytesToDo == 0) ? 1 : bytesToDo;
    
    for (int i = 0;i < bytesToDo; i++)
    {
        // Have we a byte to send?
        int byteToSend = removeTxByte(uart);
        if (byteToSend != -1)
        {
            // Make sure we have at least the required stop bits before our start bit
            long stopBitNanoseconds = bitDelay + (uart->secondStopBit ? bitDelay : 0);
            
            // Start bit
            holdAndSetTx(uart, false, &lastBitTime, stopBitNanoseconds);
            
            int bits = byteToSend;
            int parity = 0;
            // UART is Least Significant Bit first
            for (int i = 0;i < 8; i++)
            {
                // Keep track of parity
                parity ^= (bits & 1);
                
                // Send the value of the lowest bit
                holdAndSetTx(uart, bits & 1, &lastBitTime, bitDelay);
                // Shift out the just-transmitted bit
                bits >>= 1;
            }
            
            // Parity bit
            if (uart->parityBit)
            {
                holdAndSetTx(uart, parity, &lastBitTime, bitDelay);
            }
            
            // Stop bit(s), though we don't wait for them...
            holdAndSetTx(uart, true, &lastBitTime, bitDelay);
        }
    }
    
    // Have it trigger again in a jiffie! =]
    uart->txTimer.expires = jiffies + 1;
    
    // We are all done!
    spin_unlock_irqrestore(&locker, savedFlags);
}

// Check whether the bit buffer currently holds a valid byte.
// If so, we return the value. If not, we return -1.
int getByteInBitBuffer(GpioUart* uart)
{
    // What is the size of a valid frame for us?
    // We start with at least one stop bit, then a start bit, then 8 data bits, 1 possible parity, then 1 or 2 stop bits
    int frameSize = 11 + (uart->secondStopBit ? 2 : 0) + (uart->parityBit ? 1 : 0);
    
    // Are the stop bit(s) and start bit in position?
    
    // This is the mask of just the stop (high) bits, depending on frame configuration
    // With one stop bit, we have stop bits at the beginning and end of the frame
    int stopBitmask = 1 | (1 << (frameSize - 1));
    
    // This mask selects all the start and stop bits, depending on frame configuration
    int startStopBitmask = 0;
    
    if (uart->secondStopBit)
    {
        // The second and second to last bits are stop bits
        stopBitmask |= 2 | (1 << (frameSize - 2));
        // The third bit will be the start bit if there are two stop bits
        startStopBitmask |= stopBitmask | 4;
    }
    else
    {
        // The second bit is the start bit
        startStopBitmask = stopBitmask | 2;
    }
    
    // Now, looking at just these specific bits, are all (and only) the stop bits high?
    if ((uart->rxBitBuffer & startStopBitmask) == stopBitmask)
    {
        // The data are bits 2 through 9 or 3 through 10 
        int dataByte = uart->rxBitBuffer >> (2 + (uart->secondStopBit ? 1 : 0));
        
        // If we have a parity bit, is it correct?
        if (uart->parityBit)
        {
            int parityBitValue = 0;
            // Parity bit is the second or third to last
            if (uart->rxBitBuffer & (1 << (frameSize - 2 - (uart->secondStopBit ? 1 : 0))))
            {
                parityBitValue = 1;
            }
            
            int bits = dataByte;
            int parity = 0;
            for (int i = 0;i < 8; i++)
            {
                parity ^= (bits & 1);
                bits >>= 1;
            }
            
            if (parityBitValue == parity)
            {
                // We have a byte!
                // Clear the bit buffer besides the stop bits
                uart->rxBitBuffer >>= frameSize - 1 - (uart->secondStopBit ? 1 : 0);
                return dataByte;
            }
            else
            {
                //printf("Parity failed: %d\n", bitBuffer);
            }
        }
        else
        {
            // Clear the bit buffer besides the stop bits
            uart->rxBitBuffer >>= frameSize - 1 - (uart->secondStopBit ? 1 : 0);
            return dataByte;
        }
    }
    // No byte for us!
    return -1;
}

// Interrupt handler for the rx pin
irqreturn_t rxIsr(int irq, void* dev_id, struct pt_regs* regs)
{
    GpioUart* uart = (GpioUart*)dev_id;
    
    // Get current time
    struct timespec currentTime;
    get_monotonic_boottime(&currentTime);
    
    // What is the delay for a bit in nanoseconds?
    long bitDelay = 1000000000L / uart->baudRate;
    
    // How many bit times have there been on the old value? (since the last interrupt)
    // We round this up to help with slight misalignment, so 0.5 -> 1, 1.5 -> 2
    int bitNumber = ((currentTime.tv_sec - uart->rxLastInterruptTime.tv_sec) * 1000000000L +
                    (currentTime.tv_nsec - uart->rxLastInterruptTime.tv_nsec) + bitDelay / 2) / bitDelay;
                    
    // Then update the stored time
    uart->rxLastInterruptTime = currentTime;
                    
    // What is the size of a valid frame for us?
    // We start with at least one stop bit, then a start bit, then 8 data bits, 1 possible parity, then 1 or 2 stop bits
    int frameSize = 11 + (uart->secondStopBit ? 2 : 0) + (uart->parityBit ? 1 : 0);
    
    // It doesn't make sense to attempt adding more bits than our frame is large
    bitNumber = (bitNumber > frameSize) ? frameSize : bitNumber;
                    
    for (int i = 0;i < bitNumber; i++)
    {
        // Shift existing bits
        uart->rxBitBuffer >>= 1;
        if (uart->rxLastValue)
        {
            // Set the new bit in the highest location of the frame in the bit buffer
            uart->rxBitBuffer |= 1 << (frameSize - 1);
        }
        
        // Process potential byte in the bit buffer
        int dataByte = getByteInBitBuffer(uart);
        if (dataByte != -1)
        {
            // Byteage!
            addRxByte(uart, dataByte);
        }
    }
    
    // Read and set the new value that came from this interrupt
    uart->rxLastValue = gpio_get_value(uart->rxPin);
    uart->rxLastValue = uart->invertingLogic ? !uart->rxLastValue : uart->rxLastValue;
        
    return IRQ_HANDLED;
}

// Opens and configures the needed GPIO pins
// And sets up the appropraite interrupts on them
int startUart(GpioUart* uart)
{
    // Do not allow start if we are already going
    if (uart->isRunning)
    {
        return -EPERM;
    }
    
    // Do not allow start if we do not have gpio pins set yet
    if (uart->rxPin == -1 || uart->txPin == -1)
    {
        return -EPERM;
    }
    
    // Get the GPIOs for our interrupt handler
    if (gpio_request(uart->rxPin, "GPIO UART rx") ||
        gpio_direction_input(uart->rxPin) ||
        gpio_export(uart->rxPin, 0))
    {
        printk(KERN_ERR "Could not obtain rx GPIO UART pin\n");
        return -1;
    }
    
    // Output initializes high
    if (gpio_request(uart->txPin, "GPIO UART tx") ||
        gpio_direction_output(uart->txPin, 1) ||
        gpio_export(uart->txPin, 0))
    {
        printk(KERN_ERR "Could not obtain tx GPIO UART pin\n");
        return -1;
    }
    
    // Register our interrupt handler
    // We want to get interrupts on both the rising and falling edges so we can get the full picture
    // of what the input signal is on the rx pin.
    if(request_irq(OMAP_GPIO_IRQ(uart->rxPin),
                         (irq_handler_t)rxIsr,
                         IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "GPIO UART rx IRQ", uart))
    {
        printk(KERN_ERR "Could not register irq for GPIO UART rx\n");
        return -1;
    }
    
    // Register our timer, which will do the job of sending out tx bytes
    // Have it trigger in a jiffie =]
    uart->txTimer.expires = jiffies + 1;
    add_timer(&uart->txTimer);
    
    uart->isRunning = true;
    
    return 0;
}

int stopUart(GpioUart* uart)
{
    if (uart->isRunning)
    {
        // Release everything!
        gpio_free(uart->rxPin);
        gpio_free(uart->txPin);
        free_irq(OMAP_GPIO_IRQ(uart->rxPin), uart);
        del_timer(&uart->txTimer);
        
        uart->isRunning = false;
    }
    
    return 0;
}

long uart_ioctl(struct file* filePointer, unsigned int cmd, unsigned long arg)
{
    GpioUart* uart = (GpioUart*)filePointer->private_data;
    switch (cmd)
    {
        case GPIO_UART_IOC_SETBAUD:
            uart->baudRate = arg;
            return 0;
        case GPIO_UART_IOC_GETBAUD:
            return uart->baudRate;
        case GPIO_UART_IOC_SETRX:
            if (!uart->isRunning)
            {
                uart->rxPin = arg;
                return 0;
            }
            else
            {
                return -EPERM;
            }
        case GPIO_UART_IOC_GETRX:
            return uart->rxPin;
        case GPIO_UART_IOC_SETTX:
            if (!uart->isRunning)
            {
                uart->txPin = arg;
                return 0;
            }
            else
            {
                return -EPERM;
            }
        case GPIO_UART_IOC_GETTX:
            return uart->txPin;
        case GPIO_UART_IOC_SETINVERTINGLOGIC:
            uart->invertingLogic = arg;
            return 0;
        case GPIO_UART_IOC_GETINVERTINGLOGIC:
            return uart->invertingLogic;
        case GPIO_UART_IOC_SETPARITYBIT:
            uart->parityBit = arg;
            return 0;
        case GPIO_UART_IOC_GETPARITYBIT:
            return uart->parityBit;
        case GPIO_UART_IOC_SETSECONDSTOPBIT:
            uart->secondStopBit = arg;
            return 0;
        case GPIO_UART_IOC_GETSECONDSTOPBIT:
            return uart->secondStopBit;
        case GPIO_UART_IOC_START:
            return startUart(uart);
        case GPIO_UART_IOC_STOP:
            return stopUart(uart);
    }
    return -ENOTTY;
}

int uart_init(void) {
    //Register the device
    int result = register_chrdev(majorNumber, "gpio_uart", &uart_operations);
    if (result < 0) {
        printk(
            KERN_ERR "GPIO UART Device: Cannot obtain major number %d\n", majorNumber);
        return result;
    }
    
    printk(KERN_INFO "Inserting gpio_uart module\n");
    return 0;
}

void uart_exit(void) {
    //Unregister the device
    unregister_chrdev(majorNumber, "gpio_uart");
    
    printk(KERN_INFO "Removing gpio_uart module\n");
}


int uart_open(struct inode* inode, struct file* filePointer)
{
    // Allocate memory for the GpioUart structure
    // GFP_KERNEL for an allocation that can take its time
    filePointer->private_data = kmalloc(sizeof(GpioUart), GFP_KERNEL);
    if (!filePointer->private_data)
    {
        // Failed to open the uart... No memory!
        return -ENOMEM;
    }
    
    // SO MUCH INITIALIZATION!!!
    GpioUart* uart = (GpioUart*)filePointer->private_data;
    // Defaults!
    uart->rxPin = -1;
    uart->txPin = -1;
    uart->baudRate = 9600;
    uart->invertingLogic = false;
    uart->parityBit = false;
    uart->secondStopBit = false;
    uart->rxBufferStart = uart->rxBufferTail = 0;
    uart->txBufferStart = uart->txBufferTail = 0;
    
    uart->rxBitBuffer = 0;
    get_monotonic_boottime(&uart->rxLastInterruptTime);
    uart->rxLastValue = true; // Line should default high (unused)
    
    init_timer(&uart->txTimer);
    uart->txTimer.function = txSender;
    uart->txTimer.data = (unsigned long)uart;
    
    return 0;
}

int uart_release(struct inode* inode, struct file* filePointer)
{
    // Release the memory
    kfree(filePointer->private_data);
    // Just for extra safety...
    filePointer->private_data = NULL;
    
    return 0;
}

ssize_t uart_read(struct file* filePointer, char* dataBuffer, size_t dataLength, loff_t* filePosition)
{
    GpioUart* uart = (GpioUart*)filePointer->private_data;
    
    // Copy byte by byte
    for (int i = 0;i < dataLength; i++)
    {
        unsigned char byteValue = removeRxByte(uart);
        if (byteValue == -1)
        {
            // No more bytes to copy...
            return i;
        }
        copy_to_user(dataBuffer + i, &byteValue, 1);
    }
    
    return dataLength;
}

ssize_t uart_write(struct file* filePointer, const char* dataBuffer, size_t dataLength, loff_t* filePosition)
{
    GpioUart* uart = (GpioUart*)filePointer->private_data;
    
    // Copy byte by byte
    for (int i = 0;i < dataLength; i++)
    {
        unsigned char byteValue;
        copy_from_user(&byteValue, dataBuffer, 1);
        addTxByte(uart, byteValue);
    }
    
    return dataLength;
}