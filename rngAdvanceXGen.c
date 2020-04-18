#include <stdio.h>
#include <stdlib.h>

void nextvalue(unsigned long *value);

int main( int argc, const char* argv[] )
{
    int timesToAdvance = strtol(argv[1], NULL, 10);
    if (timesToAdvance == 0) {
        printf("Please include the amount of times to advance the RNG as a program argument");
        return -1;
    }
    
    unsigned long fromZero = 0;
    for (int i=0; i < timesToAdvance; i++) {
        nextvalue(&fromZero);
    }
    printf("initialized 0 RNG after being advanced %d times: %lu\n", timesToAdvance, fromZero);
    
    unsigned long fromOne = 1;
    for (int i=0; i < timesToAdvance; i++) {
        nextvalue(&fromOne);
    }
    printf("initialized 1 RNG after being advanced %d times: %lu\n", timesToAdvance, fromOne);
    
    printf("\nTherefore to advance the RNG state %d times multiply the current RNG value by %lu, then add %lu\n", timesToAdvance, fromOne - fromZero, fromZero);
}

void nextvalue(unsigned long *value)
{
    *value = ((*value * 1103515245UL) + 12345UL) & 0x7FFFFFFFUL;
}