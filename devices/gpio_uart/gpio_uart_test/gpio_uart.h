#include <linux/ioctl.h>

#ifndef GPIO_UART_H
#define GPIO_UART_H

#define GPIO_UART_IOC_MAGIC '-'

// Set the baud rate of the uart
#define GPIO_UART_IOC_SETBAUD _IO(GPIO_UART_IOC_MAGIC, 0)
// Get the baud rate of the uart
#define GPIO_UART_IOC_GETBAUD _IO(GPIO_UART_IOC_MAGIC, 1)

// To set the rx pin of the uart
// May only be set before uart has been started
#define GPIO_UART_IOC_SETRX _IO(GPIO_UART_IOC_MAGIC, 2)
// Get the rx pin of the uart
#define GPIO_UART_IOC_GETRX _IO(GPIO_UART_IOC_MAGIC, 3)

// To set the tx pin of the uart
// May only be set before uart has been started
#define GPIO_UART_IOC_SETTX _IO(GPIO_UART_IOC_MAGIC, 4)
// Get the tx pin of the uart
#define GPIO_UART_IOC_GETTX _IO(GPIO_UART_IOC_MAGIC, 5)

// Set the inverting logic setting of the uart (default false)
#define GPIO_UART_IOC_SETINVERTINGLOGIC _IO(GPIO_UART_IOC_MAGIC, 6)
// Get the inverting logic setting of the uart
#define GPIO_UART_IOC_GETINVERTINGLOGIC _IO(GPIO_UART_IOC_MAGIC, 7)

// Set the parity bit setting of the uart (default false)
#define GPIO_UART_IOC_SETPARITYBIT _IO(GPIO_UART_IOC_MAGIC, 8)
// Get the parity bit setting of the uart
#define GPIO_UART_IOC_GETPARITYBIT _IO(GPIO_UART_IOC_MAGIC, 9)

// Set the second stop bit setting of the uart (default false)
#define GPIO_UART_IOC_SETSECONDSTOPBIT _IO(GPIO_UART_IOC_MAGIC, 10)
// Get the second stop bit setting of the uart
#define GPIO_UART_IOC_GETSECONDSTOPBIT _IO(GPIO_UART_IOC_MAGIC, 11)

// To start the uart with previously set tx and rx pins
#define GPIO_UART_IOC_START _IO(GPIO_UART_IOC_MAGIC, 12)
// Stops the uart, allowing settings to be changed
#define GPIO_UART_IOC_STOP _IO(GPIO_UART_IOC_MAGIC, 13)


#endif