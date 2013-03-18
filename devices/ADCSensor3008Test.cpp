#include "ADCSensor3008.h"
#include <stdio.h>

int main(int argc, char** argv)
{
    ADCSensor3008* adc1 = new ADCSensor3008(0);
    
    for (int i = 0; i < 10; i++)
    {
        int32_t value = adc1->readConversion();
        printf("Value read: %d\n", value);
    }
}