/* adc_wiringPi.c
 * Based on code from: https://projects.drogon.net/raspberry-pi/wiringpi/
 * Adapted and commented by: Luis Alberto Rivera
 * ECE4220/7220

 * Remember to compile using -lwiringPi
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>		// for the integer types
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <time.h>
#include <sys/time.h>


#define SPI_CHANNEL	      0	// 0 or 1
#define SPI_SPEED 	2000000	// Max speed is 3.6 MHz when VDD = 5 V
#define ADC_CHANNEL       2	// Between 0 and 3



// Note 1: The Raspberry Pi has two SPI channels. Channel 0 is used for communication
// with the MCP3004.

// Note 2: The MCP3004 chip on the auxiliary board has 4 ADC channels. Channels 0 and 1
// are connected to the light sensor and potentiometer on the board, respectively.
// Channels 2 and 3 are free.
// Be careful with the input voltages. They should be between 0 and VDD. The MCP3004
// chip on the auxiliary board is already connected to VDD = 3.3 V.
// Remember to connect your input circuit's ground to the board's ground, so they have
// the same reference.
// You can find the ADC chip and pins on the top right corner of the aux. board.

uint16_t get_ADC(int channel);	// prototype

int main(void)
{
    uint16_t ADCvalue;
    printf("Start\n");
    double timeStamp = 0;

	// Configure the SPI
	if(wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED) < 0) {
		printf("wiringPiSPISetup failed\n");
		return -1 ;
	}

  struct timeval tv;
  gettimeofday(&tv,NULL);
  FILE* fp;
  fp = fopen("adc_values.txt","w+");

	// Loop that constantly reads the converted value from the selected channel, and
	// prints it to the screen.
	// This is a simple test, with a sampling frequency of ~1 Hz. Remember that sleep()
	// is not the most accurate function...
	while(1){
		ADCvalue = get_ADC(ADC_CHANNEL);
    gettimeofday(&tv,NULL);
		timeStamp = (double)tv.tv_sec + (double)(0.000001)*tv.tv_usec;
    //printf("time: %f\n",timeStamp);
    fprintf(fp,"adc value: %d time in usec: %f\n",ADCvalue,timeStamp);
		usleep(100000);
	}

  return 0;
}

// As described in sections 5 and 6 of the MCP3004 manual, we need to send three bytes
// to initiate the conversion, and select the ADC mode and channel. As those three bytes
// are sent from the RPi to the MCP3004, three bytes are returned, containing the
// converted value. You can read about SPI communication for further details.
// The communication can be done "by hand": mapping registers, configuring the appropriate
// GPIO ports, sending and receiving data using functions like ioctl(), etc. However, the
// wiringPi SPI utility does the work for us.
// You can implement the SPI communication "by hand", if you feel adventurous...

// Input: ADC_chan -- 0, 1, 2 or 3
// Output: a 16 bit unsigned integer with the ADC conversion value. Since the ADC
//         resolution is 10 bits, the returned values should be between 0 and 1023.
// Assumes Single Channel Mode (not "Differential Mode").
uint16_t get_ADC(int ADC_chan)
{
	uint8_t spiData[3];
	spiData[0] = 0b00000001; // Contains the Start Bit
	spiData[1] = 0b10000000 | (ADC_chan << 4);	// Mode and Channel: M0XX0000
												// M = 1 ==> single ended
									// XX: channel selection: 00, 01, 10 or 11
	spiData[2] = 0;	// "Don't care", this value doesn't matter.

	// The next function performs a simultaneous write/read transaction over the selected
	// SPI bus. Data that was in the spiData buffer is overwritten by data returned from
	// the SPI bus.
	wiringPiSPIDataRW(SPI_CHANNEL, spiData, 3);

	// spiData[1] and spiData[2] now have the result (2 bits and 8 bits, respectively)

	return ((spiData[1] << 8) | spiData[2]);
}
