#include "Uart.h"
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

Uart::Uart(int uartNumber, int32_t baudRate)
{
    uartNumber = (uartNumber < 1) ? 1 : ((uartNumber > 5) ? 5 : uartNumber);
    switch (uartNumber)
    {
        case 1:
            uartHandle = open("/dev/ttyO1", O_RDWR);
            break;
        case 2:
            uartHandle = open("/dev/ttyO2", O_RDWR);
            break;
        case 3:
            uartHandle = open("/dev/ttyO3", O_RDWR);
            break;
        case 4:
            uartHandle = open("/dev/ttyO4", O_RDWR);
            break;
        case 5:
            uartHandle = open("/dev/ttyO5", O_RDWR);
            break;
    }
    
    if (uartHandle == -1)
    {
        isInitialized = false;
        return;
    }
    
    speed_t rate;
    switch (baudRate)
    {
        case 50:
            rate = B50;
            break;
        case 75:
            rate = B75;
            break;
        case 110:
            rate = B110;
            break;
        case 134:
            rate = B134;
            break;
        case 150:
            rate = B150;
            break;
        case 200:
            rate = B200;
            break;
        case 300:
            rate = B300;
            break;
        case 600:
            rate = B600;
            break;
        case 1200:
            rate = B1200;
            break;
        case 1800:
            rate = B1800;
            break;
        case 2400:
            rate = B2400;
            break;
        case 4800:
            rate = B4800;
            break;
        case 9600:
            rate = B9600;
            break;
        case 19200:
            rate = B19200;
            break;
        case 38400:
            rate = B38400;
            break;
        case 115200:
            rate = B115200;
            break;
        case 230400:
            rate = B230400;
            break;
        case 460800:
            rate = B460800;
            break;
        case 500000:
            rate = B500000;
            break;
        case 576000:
            rate = B576000;
            break;
        case 921600:
            rate = B921600;
            break;
        case 1000000:
            rate = B1000000;
            break;
        case 1500000:
            rate = B1500000;
            break;
        case 2000000:
            rate = B2000000;
            break;
        case 2500000:
            rate = B2500000;
            break;
        case 3000000:
            rate = B3000000;
            break;
        case 3500000:
            rate = B3500000;
            break;
        case 4000000:
            rate = B4000000;
            break;
        default:
            isInitialized = false;
            return;
    }
    
    struct termios terminalOptions;
    tcgetattr(uartHandle, &terminalOptions);
    cfsetispeed(&terminalOptions, rate);
    cfsetospeed(&terminalOptions, rate);
    int result = tcsetattr(uartHandle, TCSANOW, &terminalOptions);
    
    if (result == -1)
    {
        isInitialized = false;
        return;
    }
    
    isInitialized = true;
}

bool Uart::isReady() const
{
    return isInitialized;
}

int32_t Uart::readByte()
{
    uint8_t value = -1;
    read(uartHandle, &value, sizeof(uint8_t));
    return value;
}

void Uart::writeByte(uint8_t value)
{
    write(uartHandle, &value, sizeof(uint8_t));
}