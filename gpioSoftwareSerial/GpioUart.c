#define _GNU_SOURCE

#include "GpioUart.h"
#include <time.h>


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

void addTimeAndWait(struct timespec* time, long nanoseconds)
{
    // Since the operating system cannot get back to us quickly enough when we use something
    // like clock_nanosleep and can in fact get to us up to 2.5 milliseconds late,
    // We will instead try a spin-wait
    
    // Trying a half os wait first...
    /*addTime(time, nanoseconds / 4);
    int errorValue = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, time, NULL);
    if (errorValue)
    {
        fprintf(stderr, "GPIO UART warning: sleep failed or was interrupted: %s\n", strerror(errorValue));
    }
    
    // Then using the spin-wait for precision
    addTime(time, nanoseconds * 3 / 4);
    
    struct timespec actualTime;
    do
    {
        clock_gettime(CLOCK_MONOTONIC, &actualTime);
    } while (actualTime.tv_sec < time->tv_sec || (actualTime.tv_sec == time->tv_sec && actualTime.tv_nsec < time->tv_nsec));*/
    
    
    addTime(time, nanoseconds);
    // We use an absolute sleep end-time to ensure error does not accumulate
    int errorValue = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, time, NULL);
    if (errorValue)
    {
        fprintf(stderr, "GPIO UART warning: sleep failed or was interrupted: %s\n", strerror(errorValue));
    }
}

// Pushes value onto the receive buffer. If it is full, the oldest value is removed.
void pushReceivedByte(GpioUart* uart, unsigned char value)
{
    sem_wait(&uart->rxBufferLock);
    
    if (uart->rxBufferStart == uart->rxBufferTail && uart->rxBufferFull)
    {
        uart->rxBuffer[uart->rxBufferTail] = value;
        uart->rxBufferTail = (uart->rxBufferTail + 1) % UART_BUFFER_SIZE;
        uart->rxBufferStart = uart->rxBufferTail;
    }
    else
    {
        uart->rxBuffer[uart->rxBufferTail] = value;
        uart->rxBufferTail = (uart->rxBufferTail + 1) % UART_BUFFER_SIZE;
        uart->rxBufferFull = (uart->rxBufferStart == uart->rxBufferTail);
    }
    sem_post(&uart->rxBufferLock);
}

// Reads a bit from the given pin by sampling it at least 32 times and sychronizing to changing values.
bool receiveGpioBit(const char* pin, struct timespec* lastBitTime, long nanosecondsBitLength, int bitOn)
{
    // The number of times to sample the bit value
    int samplingTimes = 2;
    int samplingMajority = samplingTimes / 2;
    
    // We will sample the bit that many times
    long nanosecondsPerPart = nanosecondsBitLength / samplingTimes;
    
    // Get the very first bit
    addTimeAndWait(lastBitTime, nanosecondsPerPart);
    bool firstBitValue;
    if (gpioRead(pin, &firstBitValue))
    {
        fprintf(stderr, "GPIO UART warning: individual rx pin read failed\n");
    }
    
    // And start keeping track of what values we have gotten
    bool lastBitValue;
    
    // These are the bits with the same value as the first bit
    int consecutiveFirstBits = 1;
    // These are the bits the opposite value
    int consecutiveSecondBits = 0;
    
    bool bitValue = firstBitValue;
    // Tend to the remaining samples
    for (int i = 0; i < samplingTimes - 1; i++)
    {
        lastBitValue = bitValue;
        
        addTimeAndWait(lastBitTime, nanosecondsPerPart);
        /*if (true)
        {
            struct timespec actualTime;
            clock_gettime(CLOCK_MONOTONIC, &actualTime);
            long nanosecondsOff = (actualTime.tv_sec - lastBitTime->tv_sec) * 1000000000L + (actualTime.tv_nsec - lastBitTime->tv_nsec);
            printf("We are %ld nanoseconds off (our wait is %ld)\n", nanosecondsOff, nanosecondsPerPart);
        }*/
        if (gpioRead(pin, &bitValue))
        {
            fprintf(stderr, "GPIO UART warning: individual rx pin read failed\n");
        }
        
        // Are we consecutive?
        if (bitValue == lastBitValue)
        {
            if (bitValue == firstBitValue)
            {
                consecutiveFirstBits++;
            }
            else
            {
                consecutiveSecondBits++;
            }
        }
        else
        {
            // Is this the first bit switch?
            if (bitValue != firstBitValue)
            {
                //printf("Bit %d: first bit switch to %d after %d read(s)\n", bitOn, bitValue, consecutiveFirstBits);
                // Is the value still mostly the first value? We need 50%
                if (consecutiveFirstBits >= samplingMajority)
                {
                    // Good, we count that as the value and set the time back to the last reading of it
                    addTime(lastBitTime, -nanosecondsPerPart);
                    return firstBitValue;
                }
                else
                {
                    // Well, let us try instead seeing if we can get something from this different value!
                    consecutiveSecondBits = 1;
                    // Sync up to the change of value by discounting the time of the first-valued bits
                    i -= consecutiveFirstBits;
                }
            }
            else
            {
                //printf("Bit %d: second bit switch to %d after %d read(s)\n", bitOn, bitValue, consecutiveSecondBits);
                // Have we managed to get a decent amount (50%) of this second value?
                if (consecutiveSecondBits >= samplingMajority)
                {
                    // Good, we count that as the value and set the time back to the last reading of it
                    addTime(lastBitTime, -nanosecondsPerPart);
                    return !firstBitValue;
                }
                else
                {
                    //printf("Bit %d: collosal mess!\n", bitOn);
                    // We just have a big collosal mess...
                    // Let us return a high bit, as continuous high indicates no communication
                    return true;
                }
            }
        }
    }
    
    if (consecutiveSecondBits)
    {
        //printf("Bit %d: strong salvaged read of %d with %d reads\n", bitOn, bitValue, consecutiveSecondBits);
    }
    // If we have gotten here, we have maintained solidly on our recent value
    return bitValue;
}

void* gpioUartReceiveMain(void* arg)
{
    GpioUart* uart = (GpioUart*)arg;
    
    // We open then initialize the receive pin
    if (gpioOpen(uart->rxPin))
    {
        fprintf(stderr, "GPIO UART fatal error: rx pin failed to open\n");
        return NULL;
    }
    
    if (gpioSetInput(uart->rxPin))
    {
        fprintf(stderr, "GPIO UART warning: rx pin failed to set pin direction to input\n");
    }
    
    // Make sure we can read the pin
    bool testValue;
    if (gpioRead(uart->rxPin, &testValue))
    {
        fprintf(stderr, "GPIO UART fatal error: rx pin failed to be read\n");
        return NULL;
    }
    
    // Get the initial time to time ourselves relative to
    struct timespec startTime;
    if (clock_gettime(CLOCK_MONOTONIC, &startTime))
    {
        perror("GPIO UART fatal error: failed to get time");
        return NULL;
    }
    
    // The time that the last bit being read finished
    struct timespec lastBitTime = startTime;
    
    // We want to sychronize to the incoming data even when we start in the middle of an incoming transmission.
    // Therefore, we read in bits and do bit sychonization independently of synchonizing and reading in frames.
    // We use a bit buffer that we shift bits into from bit 9 (or 10) to the right to help us do this.
    int bitBuffer = 0;
    // The number of bits shifted into the buffer so far.
    int bitBufferCount = 0;
    
    while (uart->shouldExecute)
    {
        // What is the delay for a bit in nanoseconds?
        long bitDelay = 1000000000L / uart->baudRate;
        
        // What is the size of a valid frame for us?
        int frameSize = 10 + (uart->secondStopBit ? 1 : 0) + (uart->parityBit ? 1 : 0);
        
        // Shift in a new bit
        bitBuffer >>= 1;
        if (receiveGpioBit(uart->rxPin, &lastBitTime, bitDelay, bitBufferCount))
        {
            // Set the highest bit for our frame size
            bitBuffer |= 1 << (frameSize - 1);
        }
        bitBufferCount++;
        
        // Do we have enough bits yet?
        if (bitBufferCount >= frameSize)
        {
            // Are the stop bit(s) and start bit in position?
            // Start bit is bit 0, stop bit(s) are the highest bits
            // This mask selects all these bits
            int startStopBitmask = 1 | (1 << (frameSize - 1));
            if (uart->secondStopBit)
            {
                startStopBitmask |= (1 << (frameSize - 2));
            }
            // Now, looking at just these specific bits, is the start bit low and the rest high?
            if ((bitBuffer & startStopBitmask) == (startStopBitmask - 1))
            {
                // If we have a parity bit, is it correct?
                if (uart->parityBit)
                {
                    int parityBitValue = 0;
                    if (bitBuffer & (1 << (frameSize - 2 - (uart->secondStopBit ? 1 : 0))))
                    {
                        parityBitValue = 1;
                    }
                    
                    // The data are bits 1 through 8
                    int bits = bitBuffer >> 1;
                    int parity = 0;
                    for (int i = 0;i < 8; i++)
                    {
                        parity ^= (bits & 1);
                        bits >>= 1;
                    }
                    
                    if (parityBitValue == parity)
                    {
                        // We have a byte!
                        pushReceivedByte(uart, bitBuffer >> 1);
                        bitBufferCount = 0;
                    }
                    else
                    {
                        //printf("Parity failed: %d\n", bitBuffer);
                    }
                }
                else
                {
                    pushReceivedByte(uart, bitBuffer >> 1);
                    bitBufferCount = 0;
                }
            }
        }
    }
    
    return NULL;
}

// Holds a GPIO pin to its old value until the needed time has past.
// Then sets the gpio to a new value and updates the lastBitTime value.
void holdAndSetGpio(const char* pin, bool value, struct timespec* lastBitTime, long nanosecondsNeeded)
{
    addTimeAndWait(lastBitTime, nanosecondsNeeded);
    /*if (true)
    {
        struct timespec actualTime;
        clock_gettime(CLOCK_MONOTONIC, &actualTime);
        long nanosecondsOff = (actualTime.tv_sec - lastBitTime->tv_sec) * 1000000000L + (actualTime.tv_nsec - lastBitTime->tv_nsec);
        printf("Setter is %ld nanoseconds off (%ld long pulse)\n", nanosecondsOff, nanosecondsNeeded);
    }*/
    
    if (gpioWrite(pin, value))
    {
        fprintf(stderr, "GPIO UART warning: individual tx pin write failed\n");
    }
}

// Removes and returns the oldest byte from the transfer buffer
// returns -1 if the buffer is empty
int popTransferByte(GpioUart* uart)
{
    sem_wait(&uart->txBufferLock);
    // Are we empty?
    if (uart->txBufferStart == uart->txBufferTail && !uart->txBufferFull)
    {
        sem_post(&uart->txBufferLock);
        return -1;
    }
    else
    {
        int value = uart->txBuffer[uart->txBufferStart];
        
        // Move the position, and we cannot be full
        uart->txBufferStart = (uart->txBufferStart + 1) % UART_BUFFER_SIZE;
        uart->txBufferFull = false;
        
        sem_post(&uart->txBufferLock);
        return value;
    }
}

void* gpioUartSendMain(void* arg)
{
    GpioUart* uart = (GpioUart*)arg;
    
    // Non-sending UART state is to hold a pin high.
    // We open then initialize the transfer pin that way
    if (gpioOpen(uart->txPin))
    {
        fprintf(stderr, "GPIO UART fatal error: tx pin failed to open\n");
        return NULL;
    }
    
    if (gpioSetOutputHigh(uart->txPin))
    {
        fprintf(stderr, "GPIO UART warning: tx pin failed to set pin direction to output\n");
    }
    
    // Make sure we can write to the pin!
    if (gpioWrite(uart->txPin, true))
    {
        fprintf(stderr, "GPIO UART fatal error: tx pin failed to be written to\n");
        return NULL;
    }
    
    // Get the initial time to time ourselves relative to
    struct timespec startTime;
    if (clock_gettime(CLOCK_MONOTONIC, &startTime))
    {
        perror("GPIO UART fatal error: failed to get time");
        return NULL;
    }
    
    // The time that the last bit was finished being held
    struct timespec lastBitTime = startTime;
    
    // The number of nanoseconds needed by the last stop bit before another gpio value can be set.
    long stopBitNanoseconds;
    
    // Ensure contrast for the first start bit for the high value set above
    stopBitNanoseconds = 1000000000L / uart->baudRate;
    
    while (uart->shouldExecute)
    {
        // Have we a byte to send?
        int byteToSend = popTransferByte(uart);
        if (byteToSend != -1)
        {
            // What is the delay for a bit in nanoseconds?
            // We recalculate this for every byte in case the setting changes
            long bitDelay = 1000000000L / uart->baudRate;
            
            // Start bit
            //holdGpio(uart->txPin, false, &lastBitTime, bitDelay);
            holdAndSetGpio(uart->txPin, false, &lastBitTime, stopBitNanoseconds);
            
            int bits = byteToSend;
            int parity = 0;
            // UART is Least Significant Bit first
            for (int i = 0;i < 8; i++)
            {
                // Keep track of parity
                parity ^= (bits & 1);
                
                // Send the value of the lowest bit
                holdAndSetGpio(uart->txPin, bits & 1, &lastBitTime, bitDelay);
                // Shift out the just-transmitted bit
                bits >>= 1;
            }
            
            // Parity bit
            if (uart->parityBit)
            {
                holdAndSetGpio(uart->txPin, parity, &lastBitTime, bitDelay);
            }
            
            // Stop bit(s)
            holdAndSetGpio(uart->txPin, true, &lastBitTime, bitDelay);
            stopBitNanoseconds = bitDelay + (uart->secondStopBit ? bitDelay : 0);
        }
        else
        {
            // Get some good waiting in!
            addTimeAndWait(&lastBitTime, 1000000);
        }
    }
    
    return NULL;
}

// Starts the GPIO UART operation on the given pins at the given baud rate
// by starting up two threads to manage the receiving and transfering ends
// and setting up related resources.
// Returns 0 on success, non-zero on failure.
int gpioUartStart(GpioUart* uart, const char* rxPin, const char* txPin, int baudRate)
{
    uart->rxPin = rxPin;
    uart->txPin = txPin;
    uart->baudRate = baudRate;
    
    // Set all the default settings
    uart->invertingLogic = false;
    uart->parityBit = false;
    uart->secondStopBit = false;
    uart->rxBufferStart = 0;
    uart->txBufferStart = 0;
    uart->rxBufferTail = 0;
    uart->txBufferTail = 0;
    uart->rxBufferFull = false;
    uart->txBufferFull = false;
    
    // Keep our threads alive once they start
    uart->shouldExecute = true;
    
    // Start our threads!
    // Error if they fail!
    if (pthread_create(&uart->rxThread, NULL, &gpioUartReceiveMain, uart) != 0)
    {
        return -1;
    }
    
    if (pthread_create(&uart->txThread, NULL, &gpioUartSendMain, uart) != 0)
    {
        // One has managed to live, but should now apoptosize
        uart->shouldExecute = false;
        return -1;
    }
    
    // Initialize our locks
    if (sem_init(&uart->rxBufferLock, 0, 1) != 0)
    {
        uart->shouldExecute = false;
        return -1;
    }
    
    if (sem_init(&uart->txBufferLock, 0, 1) != 0)
    {
        uart->shouldExecute = false;
        // And destroy the lock that managed to be created successfully
        sem_destroy(&uart->rxBufferLock);
        return -1;
    }
    
    // Everything is going great!
    return 0;
}

// Stops the GPIO UART operation by requesting the management threads to end
// and releasing related resources. Waits on the threads to die.
// The same structure may still be started again with gpioUartStart.
void gpioUartStop(GpioUart* uart)
{
    // Signal the threads to stop executing
    uart->shouldExecute = false;
    
    pthread_join(uart->rxThread, NULL);
    pthread_join(uart->txThread, NULL);
    
    sem_destroy(&uart->rxBufferLock);
    sem_destroy(&uart->txBufferLock);
}

// Adds a single byte to the transfer buffer to be sent out the UART
// If the buffer is full, the oldest value is removed.
void gpioUartSendByte(GpioUart* uart, unsigned char value)
{
    sem_wait(&uart->txBufferLock);
    
    if (uart->txBufferStart == uart->txBufferTail && uart->txBufferFull)
    {
        uart->txBuffer[uart->txBufferTail] = value;
        uart->txBufferTail = (uart->txBufferTail + 1) % UART_BUFFER_SIZE;
        uart->txBufferStart = uart->txBufferTail;
    }
    else
    {
        uart->txBuffer[uart->txBufferTail] = value;
        uart->txBufferTail = (uart->txBufferTail + 1) % UART_BUFFER_SIZE;
        uart->txBufferFull = (uart->txBufferStart == uart->txBufferTail);
    }
    sem_post(&uart->txBufferLock);
}

int min(int a, int b)
{
    return (a < b) ? a : b;
}

// Adds up to n bytes from the given buffer to the transfer buffer to be sent out the UART.
// Only adds up until the buffer is filled.
// Returns the actual number of bytes added.
int gpioUartSend(GpioUart* uart, unsigned char* buffer, size_t n)
{
    // Nothing to do if we are sending nothing...
    if (n == 0)
    {
        return 0;
    }
    
    sem_wait(&uart->txBufferLock);
    
    // Early opt-out if we are already full!
    if (uart->txBufferStart == uart->txBufferTail && uart->txBufferFull)
    {
        sem_post(&uart->txBufferLock);
        return 0;
    }
    
    if (uart->txBufferStart > uart->txBufferTail)
    {
        // We have one section of the buffer to copy to, between the tail and the start
        int bytesToCopy = min(n, uart->txBufferStart - uart->txBufferTail);
        memcpy(uart->txBuffer + uart->txBufferTail, buffer, bytesToCopy);
        
        // Update tail
        uart->txBufferTail += bytesToCopy;
        
        // And are we full now?
        uart->txBufferFull = (uart->txBufferStart == uart->txBufferTail);
        
        sem_post(&uart->txBufferLock);
        return bytesToCopy;
    }
    else
    {
        // We have two sections of the buffer to copy to, from the tail to the end
        // and from the beginning to the start
        
        // First copy over those bytes from the tail to the physical end of the buffer
        int bytesToCopy = min(n, UART_BUFFER_SIZE - uart->txBufferTail);
        memcpy(uart->txBuffer + uart->txBufferTail, buffer, bytesToCopy);
        
        // Then copy the remainder of the bytes from the beginning of the buffer to the start index
        int furtherBytesToCopy = min(n - bytesToCopy, uart->txBufferStart);
        memcpy(uart->txBuffer, buffer + bytesToCopy, furtherBytesToCopy);
        
        // Update tail
        uart->txBufferTail = (uart->txBufferTail + bytesToCopy + furtherBytesToCopy) % UART_BUFFER_SIZE;
        
        // And are we full now?
        uart->txBufferFull = (uart->txBufferStart == uart->txBufferTail);
        
        sem_post(&uart->txBufferLock);
        return bytesToCopy + furtherBytesToCopy;
    }
}

// Retrieves a single byte from the receive buffer, or returns -1 if it is currently empty
int gpioUartReceiveByte(GpioUart* uart)
{
    sem_wait(&uart->rxBufferLock);
    
    // Are we empty?
    if (uart->rxBufferStart == uart->rxBufferTail && !uart->rxBufferFull)
    {
        sem_post(&uart->rxBufferLock);
        return -1;
    }
    else
    {
        int value = uart->rxBuffer[uart->rxBufferStart];
        uart->rxBufferStart = (uart->rxBufferStart + 1) % UART_BUFFER_SIZE;
        uart->rxBufferFull = false;
        
        sem_post(&uart->rxBufferLock);
        return value;
    }
}

// Receives up to n bytes into the given buffer from the receive buffer. Returns the actual number
// of bytes placed into the given buffer.
int gpioUartReceive(GpioUart* uart, unsigned char* buffer, size_t n)
{
    // Nothing to do if we are receiving nothing...
    if (n == 0)
    {
        return 0;
    }
    
    sem_wait(&uart->rxBufferLock);
    
    // Are we empty?
    if (uart->rxBufferStart == uart->rxBufferTail && !uart->rxBufferFull)
    {
        sem_post(&uart->rxBufferLock);
        return 0;
    }
    else
    {
        // Is all the received data in the buffer in a straight run?
        if (uart->rxBufferStart < uart->rxBufferTail)
        {
            // If so, it is easy to copy...
            int bytesToCopy = min(n, uart->rxBufferTail - uart->rxBufferStart);
            memcpy(buffer, uart->rxBuffer + uart->rxBufferStart, bytesToCopy);
            
            // And advance the buffer start, which cannot now be full any longer
            uart->rxBufferStart = (uart->rxBufferStart + bytesToCopy) % UART_BUFFER_SIZE;
            uart->rxBufferFull = false;
            
            sem_post(&uart->rxBufferLock);
            return bytesToCopy;
        }
        else
        {
            // If not, we first copy until the physical end of the buffer
            int bytesToCopy = min(n, UART_BUFFER_SIZE - uart->rxBufferTail);
            memcpy(buffer, uart->rxBuffer + uart->rxBufferTail, bytesToCopy);
            
            // and then copy the remainder, from the physical beginning.
            int furtherBytesToCopy = min(n - bytesToCopy, uart->rxBufferStart);
            memcpy(buffer + bytesToCopy, uart->rxBuffer, furtherBytesToCopy);
            
            uart->rxBufferStart = (uart->rxBufferStart + bytesToCopy + furtherBytesToCopy) % UART_BUFFER_SIZE;
            uart->rxBufferFull = false;
            
            sem_post(&uart->rxBufferLock);
            return bytesToCopy + furtherBytesToCopy;
        }
    }
}

// Returns the number of bytes currently available in the receive buffer.
int gpioUartAvailable(GpioUart* uart)
{
    sem_wait(&uart->rxBufferLock);
    
    // Are we empty?
    if (uart->rxBufferStart == uart->rxBufferTail && !uart->rxBufferFull)
    {
        sem_post(&uart->rxBufferLock);
        return 0;
    }
    else
    {
        if (uart->rxBufferStart < uart->rxBufferTail)
        {
            sem_post(&uart->rxBufferLock);
            return uart->rxBufferTail - uart->rxBufferStart;
        }
        else
        {
            sem_post(&uart->rxBufferLock);
            return UART_BUFFER_SIZE - uart->rxBufferTail + uart->rxBufferStart;
        }
    }
}