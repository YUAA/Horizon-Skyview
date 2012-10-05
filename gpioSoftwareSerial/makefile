gpioUartTest: GpioUartTest.c GpioUart.c GeneralPurposeIOMock.c
	gcc $^ -o $@ -std=c99 -pedantic -Wall -g -lpthread -lrt
clean:
	rm -f gpioUartTest