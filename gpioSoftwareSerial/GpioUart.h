#include "GeneralPurposeIO.h"
#include <pthread.h>
#include <semaphore.h>

#ifndef GPIO_UART
#define GPIO_UART

#define UART_BUFFER_SIZE 4096

// Structure that contains all the state that governs how the GPIO UART works
typedef struct
{
    const char* rxPin;
    const char* txPin;
    
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
    int rxBufferTail;
    int txBufferTail;
    
    // Since if start == tail could either indicate an empty buffer or a full buffer,
    // This indicates whether the buffer is in fact full.
    // It is only valid if start == tail.
    bool rxBufferFull;
    bool txBufferFull;
    
    // Locks on accessing and modifying the buffers
    sem_t rxBufferLock;
    sem_t txBufferLock;
    
    pthread_t rxThread;
    pthread_t txThread;
    
    // A flag that tells the threads whether they should continue to execute
    bool shouldExecute;
    
} GpioUart;

// Starts the GPIO UART operation on the given pins at the given baud rate
// by starting up two threads to manage the receiving and transfering ends
// and setting up related resources.
// Returns 0 on success, non-zero on failure.
int gpioUartStart(GpioUart* uart, const char* rxPin, const char* txPin, int baudRate);

// Stops the GPIO UART operation by requesting the management threads to end
// and releasing related resources. Waits on the threads to die.
// The same structure may still be started again with gpioUartStart.
void gpioUartStop(GpioUart* uart);

// Adds a single byte to the transfer buffer to be sent out the UART
// If the buffer is full, the oldest value is removed.
void gpioUartSendByte(GpioUart* uart, unsigned char value);

// Adds n bytes from the given buffer to the transfer buffer to be sent out the UART
int gpioUartSend(GpioUart* uart, unsigned char* buffer, size_t n);

// Retrieves a single byte from the receive buffer, or returns -1 if it is currently empty
int gpioUartReceiveByte(GpioUart* uart);

// Receives up to n bytes into the given buffer from the receive buffer. Returns the actual number
// of bytes placed into the given buffer.
int gpioUartReceive(GpioUart* uart, unsigned char* buffer, size_t n);

// Returns the number of bytes currently available in the receive buffer.
int gpioUartAvailable(GpioUart* uart);

#endif