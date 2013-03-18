#include "ADCSensor3008.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

// Our static class members
int ADCSensor3008::spiAdcHandle;
    // A mutex to prevent problems with access to the shared spiAdcHandle
std::mutex ADCSensor3008::spiAdcMutex;

ADCSensor3008::ADCSensor3008(int adcNumber)
{
    this->adcNumber = (adcNumber < 0) ? 0 : ((adcNumber > 7) ? 7 : adcNumber);
    this->lastConvertedValue = -1;
}

int32_t ADCSensor3008::getConversion() const
{
    return this->lastConvertedValue;
}

// Open and configure the spi device.
// If already intialized, this method does nothing.
// The spiAdcMutex lock should already be taken.
// Returns -1 on error, 0 otherwise
int32_t ADCSensor3008::initializeSpi()
{
    if (ADCSensor3008::spiAdcHandle == 0)
    {
        int handle = open("/dev/spidev2.0", O_RDWR);
        if (handle == -1)
        {
            perror("ADCSensor3008: opening /dev/spidev2.0");
            return -1;
        }
        ADCSensor3008::spiAdcHandle = handle;
        
        // Set to SPI Mode 3, data in on falling edge, data out on rising edge
        uint8_t mode = 3;
        int result = ioctl(ADCSensor3008::spiAdcHandle, SPI_IOC_WR_MODE, &mode);
        if (result == -1)
        {
            perror("ADCSensor3008: setting spi to mode 0");
            return -1;
        }
        
        uint8_t bits = 8;
        result = ioctl(ADCSensor3008::spiAdcHandle, SPI_IOC_WR_BITS_PER_WORD, &bits);
        if (result == -1)
        {
            perror("ADCSensor3008: setting spi bits per word to 8");
            return -1;
        }
        
        uint32_t speed = 100000;
        result = ioctl(ADCSensor3008::spiAdcHandle, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
        if (result == -1)
        {
            perror("ADCSensor3008: setting spi speed to 100kHz");
            return -1;
        }
    }
    
    return 0;
}

int32_t ADCSensor3008::readConversion()
{
    // Lock us up in the garage
    this->spiAdcMutex.lock();
    
    // Open the spi device file if it has not been yet opened
    if (initializeSpi() == -1)
    {
        this->spiAdcMutex.unlock();
        return -1;
    }
    
    uint8_t tx[] = {(uint8_t)(0x18 + this->adcNumber), 0, 0};
    uint8_t rx[] = {0, 0, 0};
    
    struct spi_ioc_transfer spiTransfer;
    spiTransfer.tx_buf = (unsigned long)tx;
    spiTransfer.rx_buf = (unsigned long)rx;
    spiTransfer.len = 3;
    spiTransfer.delay_usecs = 0;
    spiTransfer.speed_hz = 0;
    spiTransfer.bits_per_word = 0;
    
    int result = ioctl(this->spiAdcHandle, SPI_IOC_MESSAGE(1), &spiTransfer);
    if (result == -1)
    {
        perror("ADCSensor3008: performing spi transfer");
        this->spiAdcMutex.unlock();
        return -1;
    }
    
    this->spiAdcMutex.unlock();
    
    // Byte 0 is nonsence 0xFF because our command hadn't been read yet,
    // first two bits of the byte 1 are no good either. (time it takes to do adc conversion).
    // Then we only want the top four bits of byte 2, as the 10-bit total value ends there.
    int32_t adcValue = ((rx[1] & 0x3F) << 4) | ((rx[2] & 0xF0) >> 4);
    this->lastConvertedValue = adcValue;
    
    return adcValue;
}
