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
// #include <linux/gpio.h>
#include <asm/system.h> /* cli(), *_flags */
#include <asm/uaccess.h> /* copy_from/to_user */
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/delay.h>

#include "gpio_uart.h"

MODULE_LICENSE("Dual BSD/GPL");

#define UART_BUFFER_SIZE 4096

//#define BIT_BUFFER_SIZE 192
#define BIT_BUFFER_SIZE 22

// Make this buffer fairly small, so that values do not get
// stuck in here for too long, but big enough to allow some interesting
// moving around of the time values.
#define BIT_SPAN_BUFFER_SIZE 5

#define RAW_BIT_BUFFER_SIZE 16

// Atomic GCC primitive function
// Sets variable to newValue if it is still equal to oldValue
// (type* variable, type oldValue, type newValue)
//bool __sync_bool_compare_and_swap (...);

// Structure that contains all the state that governs how the GPIO UART works
typedef struct
{
    int rxPin;
    int txPin;
    
    // ADDED FOR TESTING
    bool rxPinValue;
    int rxPinBitNumber;
    struct timespec rxBitTimeA;
    struct timespec rxBitTimeB;
    
    int baudRate;
    bool invertingLogic;
    bool parityBit;
    bool secondStopBit;
    
    // This is modified as timings are received to attempt to correct for
    // imperfect oscillator timings on the remote device
    int modifiedBaudRate;
    
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
    
    // Circular buffer for bit values and scores
    bool bitValueBuffer[BIT_BUFFER_SIZE];
    int bitScoreBuffer[BIT_BUFFER_SIZE];
    // Since this buffer will never be explicitly removed from,
    // we will only have a tail for it. It will always be "full".
    int bitBufferTail;
    
    // Circular buffer for raw bit time spans and their values. This is where they may be processed and modified,
    // unlike the plain rawBitTimeBuffer which is strictly for getting these values from the top half of the interrupt handler.
    long bitSpanTimeBuffer[BIT_SPAN_BUFFER_SIZE];
    // We also keep the original unmassaged times for comparison
    long bitSpanOriginalTimeBuffer[BIT_SPAN_BUFFER_SIZE];
    bool bitSpanValueBuffer[BIT_SPAN_BUFFER_SIZE];
    // This buffer operates just like the "bitBuffer"
    int bitSpanBufferTail;
    
    // Circular buffer for communication from the top half to the bottom half of the rxIsr handler.
    // This is necessary so that the bottom half can lock its data without losing data from the top half
    // And allowing there to be no troubles with mutliple bottom halves running concurrently.
    long rawBitTimeBuffer[RAW_BIT_BUFFER_SIZE];
    bool rawBitValueBuffer[RAW_BIT_BUFFER_SIZE];
    int rawBitBufferStart;
    int rawBitBufferTail;
    
    // The tasklet structure that runs the bottom half of the rx interrupt handling
    struct tasklet_struct rxIsrBottomHalfTasklet;
    
    // Lock on the processing of bit time data, that is, bitTimeBuffer and all the related buffers
    // This lock makes sure that multiple instances of the bottom half do not execute concurrently
    // and meddle all the data up.
    spinlock_t rxProcessingLock;
    
    // Individual bits that have been received by rx interrupts
    //int rxBitBuffer;
    // The time the last interrupt was fired on the rx line
    struct timespec rxLastInterruptTime;
    // The value of the rx pin at the time of the last interrupt
    bool rxLastValue;
    
    // This timer will schedule the transfer of bytes
    // That is, a byte or two will be sent every time the timer hits
    struct timer_list txTimer;
    
    // A flag that tells whether the uart is currently operating
    bool isRunning;
    
    // For TESTING
    int interruptsReceived;
    int interruptErrors;
} GpioUart;

// Adds a bit with the value and a score of 0 to the circular bit buffer.
// No thread-safety stuff in this method itself.
// Locking should be done separately...
void addBitWithValue(GpioUart* uart, bool bitValue)
{
    int nextTail = (uart->bitBufferTail + 1) % BIT_BUFFER_SIZE;
    
    uart->bitValueBuffer[nextTail] = bitValue;
    uart->bitScoreBuffer[nextTail] = 0;
    uart->bitBufferTail = nextTail;
}

// Index may range from 0 to BIT_BUFFER_SIZE - 1
// Where 0 will retrieve the oldest element.
bool getBitValueAt(GpioUart* uart, int index)
{
    // The tail + 1 is the next element to be overwritten, hence the oldest. They get slowly newer from there.
    return uart->bitValueBuffer[(uart->bitBufferTail + 1 + index) % BIT_BUFFER_SIZE];
}

// Index may range from 0 to BIT_BUFFER_SIZE - 1
// Where 0 will retrieve the oldest element.
int getBitScoreAt(GpioUart* uart, int index)
{
    // The tail + 1 is the next element to be overwritten, hence the oldest. They get slowly newer from there.
    return uart->bitScoreBuffer[(uart->bitBufferTail + 1 + index) % BIT_BUFFER_SIZE];
}

// Index may range from 0 to BIT_BUFFER_SIZE - 1
// Where 0 will retrieve the oldest element.
void setBitScoreAt(GpioUart* uart, int index, int newScore)
{
    // The tail + 1 is the next element to be overwritten, hence the oldest. They get slowly newer from there.
    uart->bitScoreBuffer[(uart->bitBufferTail + 1 + index) % BIT_BUFFER_SIZE] = newScore;
}

// Adds a bit span with the value and time given the circular bit span buffer.
// No thread-safety stuff in this method itself.
// Locking should be done separately...
void addSpanTimeAndValue(GpioUart* uart, long time, bool value)
{
    int nextTail = (uart->bitSpanBufferTail + 1) % BIT_SPAN_BUFFER_SIZE;
    
    uart->bitSpanValueBuffer[nextTail] = value;
    uart->bitSpanTimeBuffer[nextTail] = time;
    uart->bitSpanOriginalTimeBuffer[nextTail] = time;
    uart->bitSpanBufferTail = nextTail;
}

// Index may range from 0 to BIT_SPAN_BUFFER_SIZE - 1
// Where 0 will retrieve the oldest element.
bool getSpanValueAt(GpioUart* uart, int index)
{
    // The tail + 1 is the next element to be overwritten, hence the oldest. They get slowly newer from there.
    return uart->bitSpanValueBuffer[(uart->bitSpanBufferTail + 1 + index) % BIT_SPAN_BUFFER_SIZE];
}

// Index may range from 0 to BIT_SPAN_BUFFER_SIZE - 1
// Where 0 will retrieve the oldest element.
long getSpanTimeAt(GpioUart* uart, int index)
{
    // The tail + 1 is the next element to be overwritten, hence the oldest. They get slowly newer from there.
    return uart->bitSpanTimeBuffer[(uart->bitSpanBufferTail + 1 + index) % BIT_SPAN_BUFFER_SIZE];
}

// Index may range from 0 to BIT_SPAN_BUFFER_SIZE - 1
// Where 0 will retrieve the oldest element.
long getOriginalSpanTimeAt(GpioUart* uart, int index)
{
    // The tail + 1 is the next element to be overwritten, hence the oldest. They get slowly newer from there.
    return uart->bitSpanOriginalTimeBuffer[(uart->bitSpanBufferTail + 1 + index) % BIT_SPAN_BUFFER_SIZE];
}

// Index may range from 0 to BIT_SPAN_BUFFER_SIZE - 1
// Where 0 will retrieve the oldest element.
void setSpanTimeAt(GpioUart* uart, int index, long newTime)
{
    // The tail + 1 is the next element to be overwritten, hence the oldest. They get slowly newer from there.
    uart->bitSpanTimeBuffer[(uart->bitSpanBufferTail + 1 + index) % BIT_SPAN_BUFFER_SIZE] = newTime;
}

// Adds a byte to the circular tx buffer. This method is quasi-thread safe.
// It is ok for this method and the corresponding removeTxByte method to execute
// concurrently, but it is not safe multiple instances of this method to execute concurrently.
void addRawBitTimeAndValue(GpioUart* uart, long time, bool value)
{
    int nextTail = (uart->rawBitBufferTail + 1) % RAW_BIT_BUFFER_SIZE;
    
    // Are we full?
    if (uart->rawBitBufferStart == nextTail)
    {
        // It is safe to set this value, because this byte spot is unused.
        uart->rawBitTimeBuffer[uart->rawBitBufferTail] = time;
        uart->rawBitValueBuffer[uart->rawBitBufferTail] = value;
        
        // Attempt to set the new start value if it hasn't changed
        // if it has already, then that is ok
        // (then the buffer is not really full and this reduces to the else case)
        // (the byte we were going to remove to make more room has already been sent).
        int nextStart = (nextTail + 1) % UART_BUFFER_SIZE;
        __sync_bool_compare_and_swap(&uart->rawBitBufferStart, nextTail, nextStart);
        
        // We set the tail after the start so that the buffer never appears empty (tail == start)
        // This is safe because the remove function does not modify the tail.
        uart->rawBitBufferTail = nextTail;
    }
    else
    {
        uart->rawBitTimeBuffer[uart->rawBitBufferTail] = time;
        uart->rawBitValueBuffer[uart->rawBitBufferTail] = value;
        uart->rawBitBufferTail = nextTail;
    }
}

// Removes and returns a bit time and value pair from the circular tx buffer. This method is thread safe.
// Because there are two values to return, the bitValue is put in the pointer given and the bitTime is directly returned.
// Returns -1 for no more bytes. (an invalid bitTime)
long removeRawBitTimeAndValue(GpioUart* uart, bool* bitValue)
{    
    // Are we empty?
    if (uart->rawBitBufferStart == uart->rawBitBufferTail)
    {
        return -1;
    }
    else
    {
        // Loop until we succeed in popping a value.
        while (true)
        {
            int start = uart->rawBitBufferStart;
            int time = uart->rawBitTimeBuffer[start];
            int value = uart->rawBitValueBuffer[start];
            
            int nextStart = (start + 1) % RAW_BIT_BUFFER_SIZE;
            
            // Attempt to update start
            if (__sync_bool_compare_and_swap(&uart->rawBitBufferStart, start, nextStart))
            {
                // Start did not get changed in the interim, so our values are correct.
                *bitValue = value;
                return time;
            }
        }
    }
}

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
// Returns -1 for no more bytes
int removeTxByte(GpioUart* uart)
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
// Returns -1 for no more bytes.
int removeRxByte(GpioUart* uart)
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

long timeDifference(const struct timespec* currentTime, const struct timespec* oldTime)
{
    return  ((currentTime->tv_sec - oldTime->tv_sec) * 1000000000L +
             (currentTime->tv_nsec - oldTime->tv_nsec));
}

void addTimeAndBusyWait(struct timespec* time, long nanoseconds)
{
    addTime(time, nanoseconds);
    struct timespec actualTime;
    do
    {
        get_monotonic_boottime(&actualTime);
    } while (timespec_compare(time, &actualTime) > 0);
}

irqreturn_t rxIsr(int irq, void* dev_id, struct pt_regs* regs);

// Holds a GPIO pin to its old value until the needed time has past.
// Then sets the gpio to a new value and updates the lastBitTime value.
// Also takes inverting logic into account
void holdAndSetTx(GpioUart* uart, bool value, struct timespec* lastBitTime, long nanosecondsNeeded)
{
    // Save last time and value TESTING
    struct timespec startTime = *lastBitTime;
    bool oldValue = uart->rxPinValue;
    
    addTimeAndBusyWait(lastBitTime, nanosecondsNeeded);
    
    // Check on quality of timing! TESTING
    /*struct timespec endTime;
    get_monotonic_boottime(&endTime);
    printk(KERN_INFO "Time Quality Offset %ld", timeDifference(&endTime, lastBitTime));
    */
    
    // REMOVED FOR TESTING
    // gpio_set_value(uart->txPin, uart->invertingLogic ? !value : value);
    // ADDED FOR TESTING
    // Normalize to one and zero
    value = value ? 1 : 0;
    uart->rxPinValue = uart->invertingLogic ? !value : value;
    
    // TESTING ADD
    // Get current time
    /*struct timespec currentTime;
    get_monotonic_boottime(&currentTime);
    long bitTime = timeDifference(&currentTime, &startTime);
    long sinceLast = timeDifference(&currentTime, &uart->rxLastInterruptTime);
    printk(KERN_INFO "HCurrent time: %ld last int.: %ld", currentTime.tv_nsec, uart->rxLastInterruptTime.tv_nsec);
    get_monotonic_boottime(&currentTime);
    printk(KERN_INFO "ICurrent time: %ld", currentTime.tv_nsec);*/
    uart->rxBitTimeA = startTime;
    uart->rxBitTimeB = *lastBitTime;
    
    // To be realistic to ourselves, only trigger an "interrupt" if we are actually changing the value
    if (oldValue != uart->rxPinValue)
    {
        rxIsr(uart->rxPin, uart, NULL);
    }    

    // TESTING
    //printk(KERN_INFO "Held value %d for %ld (%ld desired), %ld last interrupt", oldValue, bitTime, nanosecondsNeeded, sinceLast);
    /*static int iterations = 0;
    static long bitTimeAverage = 0;
    static long sinceLastAverage = 0;
    bitTimeAverage = (bitTimeAverage * iterations + bitTime) / (iterations + 1);
    sinceLastAverage = (sinceLastAverage * iterations + sinceLast) / (iterations + 1);
    iterations++;
    
    // Report every once in a while!
    if (iterations >= 25)
    {
        printk(KERN_INFO "Average hold time %ld (%ld desired), %ld last interrupt", bitTimeAverage, nanosecondsNeeded, sinceLastAverage);
        iterations = 0;
        bitTimeAverage = 0;
        sinceLastAverage = 0;
    }*/
}

void txSender(unsigned long argument)
{
    GpioUart* uart = (GpioUart*)argument;
    
    spinlock_t locker;
    spin_lock_init(&locker);
    
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
            // We use the spinlock to disable interrupts so that we can busy-wait exact timing
            // TESTING
            //spin_lock_irqsave(&locker, savedFlags);
            spin_lock_bh(&locker);
            
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
            
            // The byte is done!
            //TESTING
            //spin_unlock_irqrestore(&locker, savedFlags);
            spin_unlock_bh(&locker);
        }
    }
    
    // Have it trigger again in a jiffie! =]
    uart->txTimer.expires = jiffies + 1;
    add_timer(&uart->txTimer);
    
    // Because the receiver rx interrupt only fires on changes, if there are no changes,
    // the last value will not be registered. We call the interrupt here as well, to get
    // periodical updates when the value is not actually changing.
    //TESTING rxIsr(uart->rxPin, uart, NULL);
}

// Check whether the bit buffer currently holds a valid byte at the location index.
// If so, we return the value. If not, we return -1.
int getByteInBitBufferAt(GpioUart* uart, int index)
{
    // What is the size of a valid frame for us?
    // We start with at least one stop bit, then a start bit, then 8 data bits, 1 possible parity, then 1 or 2 stop bits
    int frameSize = 11 + (uart->secondStopBit ? 2 : 0) + (uart->parityBit ? 1 : 0);
    
    // The very first thing we do is extract the desired bits from the bit buffer
    // The bit at index is the oldest (chronoligically) of the bits in the buffer being checked.
    // We want the oldest bit at bit 0 of targetBits, since we want the data bytes to be already in the right order.
    // The bits come in least significant first time-wise, so we want the oldest data bits in the least significant bits of targetBits.
    int targetBits = 0;
    // Go from newest to oldest
    for (int i = frameSize; i-- > 0;)
    {
        // Shift existing bits
        targetBits <<= 1;
        if (getBitValueAt(uart, index + i))
        {
            // This bit is on!
            targetBits |= 1;
        }
    }
    //printk(KERN_INFO "Checking target bits with value of %d\n", targetBits);
    
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
    if ((targetBits & startStopBitmask) == stopBitmask)
    {
        // The data are bits 2 through 9 or 3 through 10 
        int dataByte = (targetBits >> (2 + (uart->secondStopBit ? 1 : 0))) & 0xff;
        
        // If we have a parity bit, is it correct?
        if (uart->parityBit)
        {
            int parityBitValue = 0;
            // Parity bit is the second or third to last
            if (targetBits & (1 << (frameSize - 2 - (uart->secondStopBit ? 1 : 0))))
            {
                parityBitValue = 1;
            }
            
            //int bits = dataByte;
            //int parity = 0;
            //for (int i = 0;i < 8; i++)
            //{
            //    parity ^= (bits & 1);
            //    bits >>= 1;
            //}
            // Quicker parity calculation from http://graphics.stanford.edu/~seander/bithacks.html#ParityParallel
            int parity = dataByte;
            parity ^= parity >> 4;
            parity &= 0xf;
            parity = (0x6996 >> parity) & 1;
            
            if (parityBitValue == parity)
            {
                // We have a byte!
                return dataByte;
            }
            else
            {
                //printf("Parity failed: %d\n", bitBuffer);
            }
        }
        else
        {
            return dataByte;
        }
    }
    // No byte for us!
    return -1;
}

void processSpanTimeAndValue(GpioUart* uart, long spanTime, long originalSpanTime, bool spanValue)
{
    // Divide it into bits...
    
    // The length of time a single bit should occupy ideally.
    long bitDelay = 1000000000L / uart->baudRate;
    
    // How many bit times have there been?
    // We round this up to help with slight misalignment, so 0.5 -> 1, 1.5 -> 2
    long bitNumber = (spanTime + bitDelay / 2) / bitDelay;
                    
    // What is the size of a valid frame for us?
    // We start with one or two stop bits (not technically in the frame), then a start bit, then 8 data bits, 1 possible parity, then 1 or 2 stop bits
    int frameSize = 11 + (uart->secondStopBit ? 2 : 0) + (uart->parityBit ? 1 : 0);
    // The base frame size does not include the beginning stop bits, which do not technically belong to the frame.
    // This base frame size makes more sense to use, if say, you want to go up to the next frame; this is the number of bits away it is.
    int baseFrameSize = 10 + (uart->secondStopBit ? 1 : 0) + (uart->parityBit ? 1 : 0);
    
    // No point in flushing our buffer out with more bits than it holds.
    bitNumber = (bitNumber > BIT_BUFFER_SIZE) ? BIT_BUFFER_SIZE : bitNumber;
    
    // Give out our numbers!
    printk(KERN_INFO "Processed time: %ld (was %ld) at value: %d -- %d bits\n", spanTime, originalSpanTime, (int)spanValue, (int)bitNumber);
    
    // We add in each one at a time...
    for (int i = 0; i < bitNumber; i++)
    {
        // Add the bit to our bit buffer, use either the firstLastBitTime or bitDelay as the time, depending
        addBitWithValue(uart, spanValue);
        
        // There will now be another bit whose UART frame this new bit may have just completed.
        int newlyCompletedFrameBitIndex = BIT_BUFFER_SIZE - frameSize;
        // But we must make sure it is a valid bit (score of 0, not -1)
        if (getBitScoreAt(uart, newlyCompletedFrameBitIndex) == 0 &&
            getByteInBitBufferAt(uart, newlyCompletedFrameBitIndex) != -1)
        {
            // The score of this bit is 1 plus the score of the bit preceding it by exactly one frame
            // This score gauges how "sure" we can be that a real byte is contained in the frame starting at this bit.
            // Of course, if the previous frame bit has a score of -1 (meaning invalid) we don't add that
            int previousFrameBitScore = getBitScoreAt(uart, newlyCompletedFrameBitIndex - baseFrameSize);
            int newBitScore = 1 + ((previousFrameBitScore != -1) ? previousFrameBitScore : 0);
            
            setBitScoreAt(uart, newlyCompletedFrameBitIndex, newBitScore);
            printk(KERN_INFO "We got a valid frame at %d, scored at %d\n", newlyCompletedFrameBitIndex, newBitScore);
        }
        
        // Only check for a byte at the end of the buffer if we do not have any bits marked with a score of -1
        // If we do, these bits have already been interpreted as a byte and we want to finish filling up the bit buffer
        // before we try to look for another byte. (they would be the oldest bits, and contiguous, hence we only check index 0)
        if (getBitScoreAt(uart, 0) != -1)
        {
            int bestScore = 0;
            int bestScoreIndex = 0;
            // Check all the oldest entries in the bit buffer for a byte
            for (int k = 0; k < baseFrameSize; k++)
            {
                int score = getBitScoreAt(uart, k);
                if (score > bestScore)
                {
                    bestScore = score;
                    bestScoreIndex = k;
                }
            }
            
            // Do we have a byte at all? If so, let us take it!
            if (bestScore >= 1)
            {
                int dataByte = getByteInBitBufferAt(uart, bestScoreIndex);
                if (dataByte == -1)
                {
                    printk(KERN_ERR "Something has gone terribly wrong! Bit buffer corrupt!\n");
                }
                else
                {
                    addRxByte(uart, dataByte);
                }
                
                // Clear the scores of the frame bits that we read as a bit to invalid (-1), so they can't be interpreted again.
                // The frame has bits from the base index (the oldest bit) to the newer ones, at higher indexes.
                for (int k = 0; k < baseFrameSize; k++)
                {
                    int index = bestScoreIndex + k;
                    setBitScoreAt(uart, index, -1);
                }
            }
        }
    }
}

// This does span time messaging, but only gives time to spans that are at least at level percent.
// So if level = 90, only spans at say 190% or 290% of bitDelay, will steal more time.
// However, if spans with less than 100% will always steal time.
void relaxSpanTimesAtLevel(GpioUart* uart, int level)
{
    // The length of time a single bit should occupy ideally.
    long bitDelay = 1000000000L / uart->baudRate;
    
    // printk(KERN_INFO "Now at level %d", level);
    
    // Go through all but the last element in the buffer
    // Those will either get their turn when more elements are added, or have already.
    // This way we do not have to worry about going out of index.
    // We go through them backwards, since we are mostly pulling time from the previous element
    for (int i = BIT_SPAN_BUFFER_SIZE; i-- > 1;)
    {
        long spanTime = getSpanTimeAt(uart, i);
        // Do not bother trying to give time to the span if it is very long (> 12 bits worth, the maximum uart frame size)
        if (spanTime > bitDelay * 12)
        {
            continue;
        }
        // Do nothing if the span before it (and thus this one, too) is invalid
        if (getSpanTimeAt(uart, i - 1) != -1)
        {
            // Is this span a single bit that has been short changed?
            // If so, we want to get it to full time
            // This is independant of our "level" -- it just takes the highest priority
            // Though this also assumes we do not have false interrupts.
            if (spanTime < bitDelay)
            {
                //printk(KERN_INFO "Victim timespan start: %ld", getSpanTimeAt(uart, i - 1));
                //printk(KERN_INFO "Thief timespan start: %ld", getSpanTimeAt(uart, i));
                
                // We take the loss from the span preceding it. That seems to be where
                // the losses are from. (this may not hold generally... but does in virtual machine testing)
                int missing = bitDelay - spanTime;
                setSpanTimeAt(uart, i - 1, getSpanTimeAt(uart, i - 1) - missing);
                setSpanTimeAt(uart, i, spanTime + missing);
                
                //printk(KERN_INFO "Victim timespan end: %ld", getSpanTimeAt(uart, i - 1));
                //printk(KERN_INFO "Thief timespan end: %ld", getSpanTimeAt(uart, i));
            }
            // Does this span have more than a single bit, but only level% or more of the last bit?
            else if (spanTime % bitDelay > bitDelay * level / 100)
            {
                //printk(KERN_INFO "Victim timespan start: %ld", getSpanTimeAt(uart, i - 1));
                //printk(KERN_INFO "Thief timespan start: %ld", getSpanTimeAt(uart, i));
                
                int missing = bitDelay - (spanTime % bitDelay);
                setSpanTimeAt(uart, i - 1, getSpanTimeAt(uart, i - 1) - missing);
                setSpanTimeAt(uart, i, spanTime + missing);
                
                //printk(KERN_INFO "Victim timespan end: %ld", getSpanTimeAt(uart, i - 1));
                //printk(KERN_INFO "Thief timespan end: %ld", getSpanTimeAt(uart, i));
            }
        }
    }
}

// This takes the spans in the bit span buffer and massages the times around
// If we have a time with less than a bits worth of time, for example, we can tell
// that there has been an error, and we pull time from the surrounding spans.
// We message like this hoping to improve on noisey timings.
// Since this is always based on original timings, calling it multiple times
// will allways yield the same result, instead of them stacking.
void relaxSpanTimes(GpioUart* uart)
{
    // Reset all times to the original times
    /*for (int i = 0; i < BIT_SPAN_BUFFER_SIZE; i++)
    {
        setSpanTimeAt(uart, i, getOriginalSpanTimeAt(uart, i));
    }*/
    
    // Progressively relax times from levels of 100% down to 50%
    // This will allow the spans that are more sure (say 90%) to steal from those
    // that are less sure (say 50%), such that then the 50% one may be out of the game.
    // 100% is done first because regardless of level, times with less than a single bit
    // should always win.
    for (int level = 100; level >= 50; level -= 5)
    {
        relaxSpanTimesAtLevel(uart, level);
    }
}

// The bottom half of the px pin interrupt handler
// This bottom half is implemented as a tasklet
void rxIsrBottomHalfFunction(unsigned long data)
{
    GpioUart* uart = (GpioUart*)data;
    
    // We do not want two of these bottom halves to run concurrently
    spin_lock(&uart->rxProcessingLock);
    
    // Process all raw value pairs from the interrupts
    bool rawBitValue;
    long rawBitTime;
    while ((rawBitTime = removeRawBitTimeAndValue(uart, &rawBitValue)) != -1)
    {
        // We take our very raw timings and modify them with our hopefully more correct baud rate
        // To calibrate this, the modifiedBaudRate is originally the same as the set value.
        // Every time we get a single bit timing in the range of 75% to 125% of the current modified baud rate,
        // then we slightly modify our modified baud rate value to incorporate the new timing. The single timing
        // has a low relative weight to insure that changes happen only "slowly"
        long bitDelay = 1000000000L / uart->modifiedBaudRate;
        if (rawBitTime >= bitDelay * 75 / 100 && rawBitTime <= bitDelay * 125 / 100)
        {
            long modifiedBitDelay = (bitDelay * 63 + rawBitTime) / 64;
            uart->modifiedBaudRate = 1000000000L / modifiedBitDelay;
        }
        
        // To keep use of this modified baud rate local, we will normalize our times using it to be
        // in terms of the original baud rate again. This way the times we record for testing will not
        // be based on different baud rates over time.
        printk(KERN_INFO "Raw time (original): %ld at value: %d\n", rawBitTime, (int)rawBitValue);
        rawBitTime = rawBitTime * uart->modifiedBaudRate / uart->baudRate;
        printk(KERN_INFO "Raw time (modified): %ld at value: %d\n", rawBitTime, (int)rawBitValue);
        
        //printk(KERN_INFO "Raw time: %ld at value: %d\n", rawBitTime, (int)rawBitValue);
        
        
        // We read out time spans when they are in the second-to-last position.
        // We do this because when the last position time spans are in the "relaxing" process,
        // they are not able to steal time. We want our read values to have just had this opportunity.
        long removedSpanTime = getSpanTimeAt(uart, 1);
        long removedOriginalSpanTime = getOriginalSpanTimeAt(uart, 1);
        bool removedSpanValue = getSpanValueAt(uart, 1);
        
        // Now we overwrite the oldest values by placing our new ones into the buffer
        addSpanTimeAndValue(uart, rawBitTime, rawBitValue);
        
        // Do the relaxation of time values with our new value.
        relaxSpanTimes(uart);
        
        // Now let the removed time and value be processed into bits if they are valid
        if (removedSpanTime != -1)
        {
            processSpanTimeAndValue(uart, removedSpanTime, removedOriginalSpanTime, removedSpanValue);
        }
    }
    
    spin_unlock(&uart->rxProcessingLock);
}

// Interrupt handler for the rx pin
irqreturn_t rxIsr(int irq, void* dev_id, struct pt_regs* regs)
{
    GpioUart* uart = (GpioUart*)dev_id;
    
    // First, sample the pin
    // TESTING
    //int rxPinValue = gpio_get_value(uart->rxPin);
    int rxPinValue = uart->rxPinValue;
    // invert the value if need be.
    rxPinValue = uart->invertingLogic ? !rxPinValue : rxPinValue;

    // TESTING
    uart->interruptsReceived++;
    if (rxPinValue == uart->rxLastValue)
    {
        uart->interruptErrors++;
    }
    //uart->rxLastValue = rxPinValue;
    //return IRQ_HANDLED; 

    // Get current time
    struct timespec currentTime;
    get_monotonic_boottime(&currentTime);
    
    // What is the delay for a bit in nanoseconds?
    /*long bitDelay = 1000000000L / uart->baudRate;
    
    // How many bit times have there been on the old value? (since the last interrupt)
    // We round this up to help with slight misalignment, so 0.5 -> 1, 1.5 -> 2
    int bitNumber = (timeDifference(&currentTime, &uart->rxLastInterruptTime) + bitDelay / 2) / bitDelay;
                    
    // What is the size of a valid frame for us?
    // We start with at least one stop bit, then a start bit, then 8 data bits, 1 possible parity, then 1 or 2 stop bits
    int frameSize = 11 + (uart->secondStopBit ? 2 : 0) + (uart->parityBit ? 1 : 0);
    
    // It doesn't make sense to attempt adding more bits than our frame is large
    bitNumber = (bitNumber > frameSize) ? frameSize : bitNumber;
    
    // TESTING
   //printk(KERN_INFO "We got %d bits of %d and our value is %d\n", bitNumber, uart->rxLastValue, rxPinValue); 

    // Then update the stored time
    uart->rxLastInterruptTime = currentTime;
                    
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
    }*/
    
    long timePassed = timeDifference(&currentTime, &uart->rxLastInterruptTime);
    
    // Add the record of passed time on the old value.
    addRawBitTimeAndValue(uart, timePassed, uart->rxLastValue);
    
    // Then update the stored time
    uart->rxLastInterruptTime = currentTime;
    // Update value from this interrupt
    uart->rxLastValue = rxPinValue;
    
    // Schedule tasklet bottom half
    // This will do the job of processing the passed time and making sense out of it
    // turning it into bits and bits into uart frames and those into bytes.
    tasklet_schedule(&uart->rxIsrBottomHalfTasklet);
    
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
    
    // REMOVED FOR TESTING
    /*
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
    if(request_irq(gpio_to_irq(uart->rxPin),
                         (irq_handler_t)rxIsr, //TESTING
                         (1*IRQF_TRIGGER_RISING) | IRQF_TRIGGER_FALLING, "GPIO UART rx IRQ", uart))
    {
        printk(KERN_ERR "Could not register irq for GPIO UART rx\n");
        return -1;
    }*/
    
    // Register our timer, which will do the job of sending out tx bytes
    // Have it trigger in a jiffie =]
    uart->txTimer.expires = jiffies + 1;
    add_timer(&uart->txTimer);
    
    printk(KERN_INFO "Uart Started\n");
    
    uart->isRunning = true;
    
    return 0;
}

int stopUart(GpioUart* uart)
{
    if (uart->isRunning)
    {
        // Release everything!
        // TESTING
        //gpio_free(uart->rxPin);
        //gpio_free(uart->txPin);
        //free_irq(gpio_to_irq(uart->rxPin), uart);
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
            uart->modifiedBaudRate = uart->baudRate;
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
    printk(KERN_ERR "Uart IOCTL unknown, not %d or similar\n", GPIO_UART_IOC_START);
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
    uart->isRunning = false;
    uart->rxPin = -1;
    uart->txPin = -1;
    uart->baudRate = 9600;
    uart->invertingLogic = false;
    uart->parityBit = false;
    uart->secondStopBit = false;
    uart->rxBufferStart = uart->rxBufferTail = 0;
    uart->txBufferStart = uart->txBufferTail = 0;
    
    uart->modifiedBaudRate = uart->baudRate;
    
    uart->bitBufferTail = 0;
    uart->bitSpanBufferTail = 0;
    uart->rawBitBufferStart = uart->rawBitBufferTail = 0;
    tasklet_init(&uart->rxIsrBottomHalfTasklet, rxIsrBottomHalfFunction, (unsigned long)uart);
    spin_lock_init(&uart->rxProcessingLock);
    
    // Prefill the bitScoreBuffer with -1 values to indicate none of the bits in it are valid
    // Also prefill the bitValueBuffer with 1/true values (since the line held high is inactive)
    // And just for fun (and prudence), the times to -1...
    for (int i = 0; i < BIT_BUFFER_SIZE; i++)
    {
        uart->bitScoreBuffer[i] = -1;
        uart->bitValueBuffer[i] = true;
    }
    
    // Prefill the bit span buffer times to -1, to indicate invalid.
    // The bools to "-1" not really a bool value, for debugging help
    for (int i = 0; i < BIT_SPAN_BUFFER_SIZE; i++)
    {
        uart->bitSpanTimeBuffer[i] = -1;
        uart->bitSpanOriginalTimeBuffer[i] = -1;
        uart->bitSpanValueBuffer[i] = -1;
    }
    
    // Same as above for these values
    for (int i = 0; i < RAW_BIT_BUFFER_SIZE; i++)
    {
        uart->rawBitTimeBuffer[i] = -1;
        uart->rawBitValueBuffer[i] = -1;
    }
    
    get_monotonic_boottime(&uart->rxLastInterruptTime);
    uart->rxLastValue = true; // Line should default high (unused)
    
    init_timer(&uart->txTimer);
    uart->txTimer.function = txSender;
    uart->txTimer.data = (unsigned long)uart;
   
    // TESTING
    uart->interruptsReceived = 0;
    uart->interruptErrors = 0;
 
    printk(KERN_INFO "Uart opened");
    
    return 0;
}

int uart_release(struct inode* inode, struct file* filePointer)
{
    GpioUart* uart = (GpioUart*)filePointer->private_data;
    
    // Make sure to stop it!
    stopUart(uart);
   
    // TESTING
    printk(KERN_INFO "Time per bit (original): %ld\n", 1000000000L / uart->baudRate);
    printk(KERN_INFO "Time per bit (modified): %ld\n", 1000000000L / uart->modifiedBaudRate);
    printk(KERN_INFO "We have %d interrupts at end!\n", uart->interruptsReceived);
    printk(KERN_INFO "We have %d interrupt errors at end!\n",uart->interruptErrors);
 
    // Release the memory
    kfree(filePointer->private_data);
    // Just for extra safety...
    filePointer->private_data = NULL;
    
    printk(KERN_INFO "Uart closed");
    
    return 0;
}

ssize_t uart_read(struct file* filePointer, char* dataBuffer, size_t dataLength, loff_t* filePosition)
{
    GpioUart* uart = (GpioUart*)filePointer->private_data;
    
    // Copy byte by byte
    for (int i = 0;i < dataLength; i++)
    {
        int value = removeRxByte(uart);
        if (value == -1)
        {
            // No more bytes to copy...
            return i;
        }
        unsigned char byteValue = value;
        
        printk(KERN_INFO "Byte %d transferred to user.\n", byteValue);
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
        printk(KERN_INFO "Byte %d transferred from user.\n", byteValue);
        addTxByte(uart, byteValue);
    }
    
    return dataLength;
}

