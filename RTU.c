#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <time.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>
#include <semaphore.h>
#include <pthread.h>
#include <wiringPi.h>
#include <errno.h>
#include <wiringPiSPI.h>
#include <sys/time.h>
#define NUM_BUTTONS 5
#define NUM_LEDS 4
#define NUM_SWITCH 2
#define SPI_CHANNEL	      0	// 0 or 1
#define SPI_SPEED 	2000000	// Max speed is 3.6 MHz when VDD = 5 V
#define ADC_CHANNEL       2	// Between 1 and 3
#define MY_PRIORITY 60
#define MSG_SIZE 50			// message size
#define CHAR_DEV "/dev/Project" // "/dev/YourDevName"
#define LED1 8
#define LED2 9
#define LED3 7
#define LED4 21
#define SWITCH1 32
#define SWITCH2 33


//function definitions
void* Historian_Handler(void* args);
void clearEvents();
void PrintEvent(int i);
uint16_t get_ADC(int channel);
int ADC_Average(double newVoltage);
void NoPowerEvent(double newADCValue);
void OverloadEvent(double newADCValue);
void buttonEvent();
void LED_Event();
void* ADC_handler(void* args);
void* ButtonRead(void* args);
void* LED_Handler(void* args);
void switch1_Event();
void switch2_Event();
void* switch1_Handler(void* args);
void* switch2_Handler(void* args);
void* Historian_Handler(void* args);

//global variables for measurements
double ADC_values[4] = {0};
int LEDSignals[4] = {0};
int buttonSignals[5] = {0};
int switchSignals[2] = {0};
char My_IP[13];
int IP_Digits;

//structure for
typedef struct Event {
   int eventId;
   int rtuId;
   double eventTime;
   int button1;
   int button2;
   int button3;
   int button4;
   int button5;
   int led1;
   int led2;
   int led3;
   int led4;
   int switch1;
   int switch2;
   double adcValue;
   char Message[25];
} Event;

Event RTU_Event[100]; //event buffer
int eventCounter;
int eventIdCounter;
sem_t my_semaphore;
int cdev_id;
struct timeval tv;
int updateEvents;
struct ifreq ifr;
int sock, length, n;
socklen_t fromlen;
struct sockaddr_in server;
struct sockaddr_in addr;
int boolval = 1;

//clears all the events stored in the event buffer
void clearEvents(){
  int i = 0;
  for(i=0;i<100;i++){
    RTU_Event[i].eventId = 0;
    RTU_Event[i].rtuId = 0;
    RTU_Event[i].eventTime = 0;
    RTU_Event[i].Message[0] = '\0';
    RTU_Event[i].adcValue = 0;
    RTU_Event[i].button1 = 0;
    RTU_Event[i].button2 = 0;
    RTU_Event[i].button3 = 0;
    RTU_Event[i].button4 = 0;
    RTU_Event[i].button5 = 0;
    RTU_Event[i].led1 = 0;
    RTU_Event[i].led2 = 0;
    RTU_Event[i].led3 = 0;
    RTU_Event[i].led4 = 0;
    RTU_Event[i].switch1 = 0;
    RTU_Event[i].switch2 = 0;
    RTU_Event[i].adcValue = 0;
  }
}

//just a utility function for testing
void PrintEvent(int i){
        printf("------------------\nevent id: %d\nevent time: %f\nVoltage: %f\n%s",RTU_Event[i].eventId,RTU_Event[i].eventTime,RTU_Event[i].adcValue,RTU_Event[i].Message);
        printf("buttons: |%d|%d|%d|%d|%d|\nLEDS: |%d|%d|%d|%d|\n",RTU_Event[i].button1,RTU_Event[i].button2,RTU_Event[i].button3,RTU_Event[i].button4,RTU_Event[i].button5,RTU_Event[i].led1,RTU_Event[i].led2,RTU_Event[i].led3,RTU_Event[i].led4);
        printf("Switches: |%d|%d|\n",RTU_Event[i].switch1,RTU_Event[i].switch2);
}

//gets the digital value from th adc chip
uint16_t get_ADC(int channel){
  uint8_t spiData[3];
	spiData[0] = 0b00000001; // Contains the Start Bit
	spiData[1] = 0b10000000 | (channel << 4);	// Mode and Channel: M0XX000
	spiData[2] = 0;	// "Don't care", this value doesn't matter.
	wiringPiSPIDataRW(SPI_CHANNEL, spiData, 3);
	return ((spiData[1] << 8) | spiData[2]);
}

//finds the average
int ADC_Average(double newVoltage){
  int i = 0;
  double avg = 0;
  ADC_values[3] = newVoltage;
  ADC_values[2] = ADC_values[3];
  ADC_values[1] = ADC_values[2];
  ADC_values[0] = ADC_values[1];
  for(i=0;i<4;i++){
    avg += ADC_values[i];
  }
  avg = avg/4.0;
  if(ADC_values[0] == ADC_values[1] == ADC_values[2] == ADC_values[3]){
    return -1;
  }
  else{
    return avg;
  }
}

//creates an event for a no power detetcion
void NoPowerEvent(double newADCValue){
  //get the current time and all measurements
  gettimeofday(&tv,NULL);
  RTU_Event[eventCounter].eventTime = (double)tv.tv_sec*(1000) + (double)(0.001)*tv.tv_usec;
  strcpy(RTU_Event[eventCounter].Message,"No Power Event");
  RTU_Event[eventCounter].eventId = 1;
  RTU_Event[eventCounter].rtuId = IP_Digits;
  RTU_Event[eventCounter].adcValue = newADCValue;
  RTU_Event[eventCounter].button1 = buttonSignals[0];
  RTU_Event[eventCounter].button2 = buttonSignals[1];
  RTU_Event[eventCounter].button3 = buttonSignals[2];
  RTU_Event[eventCounter].button4 = buttonSignals[3];
  RTU_Event[eventCounter].button5 = buttonSignals[4];
  RTU_Event[eventCounter].led1 = LEDSignals[0];
  RTU_Event[eventCounter].led2 = LEDSignals[1];
  RTU_Event[eventCounter].led3 = LEDSignals[2];
  RTU_Event[eventCounter].led4 = LEDSignals[3];
  RTU_Event[eventCounter].switch1 = switchSignals[0];
  RTU_Event[eventCounter].switch2 = switchSignals[1];
  updateEvents++;
  if(eventCounter++ == 100){
    eventCounter = 0;
  }
  return;
}

//creates an event for an overload detetcion
void OverloadEvent(double newADCValue){
  //get the current time and all measurements
  gettimeofday(&tv,NULL);
  RTU_Event[eventCounter].eventTime = (double)tv.tv_sec*(1000) + (double)(0.001)*tv.tv_usec;
  strcpy(RTU_Event[eventCounter].Message,"Overload Event");
  RTU_Event[eventCounter].eventId = 2;
  RTU_Event[eventCounter].rtuId = IP_Digits; //last 2 digits on IP for the Pi running the RTU
  RTU_Event[eventCounter].adcValue = newADCValue;
  RTU_Event[eventCounter].button1 = buttonSignals[0];
  RTU_Event[eventCounter].button2 = buttonSignals[1];
  RTU_Event[eventCounter].button3 = buttonSignals[2];
  RTU_Event[eventCounter].button4 = buttonSignals[3];
  RTU_Event[eventCounter].button5 = buttonSignals[4];
  RTU_Event[eventCounter].led1 = LEDSignals[0];
  RTU_Event[eventCounter].led2 = LEDSignals[1];
  RTU_Event[eventCounter].led3 = LEDSignals[2];
  RTU_Event[eventCounter].led4 = LEDSignals[3];
  RTU_Event[eventCounter].switch1 = switchSignals[0];
  RTU_Event[eventCounter].switch2 = switchSignals[1];
  updateEvents++;
  if(eventCounter++ == 100){
    eventCounter = 0;
  }
  return;
}

//create an event for a button press detetcion
void buttonEvent(){
  //get the current time and all measurements
  double volts = 0;
  gettimeofday(&tv,NULL);
  RTU_Event[eventCounter].eventTime = (double)tv.tv_sec*(1000) + (double)(0.001)*tv.tv_usec;
  strcpy(RTU_Event[eventCounter].Message,"Button Press Event");
  RTU_Event[eventCounter].eventId = 3;
  volts = (double)get_ADC(ADC_CHANNEL);
  volts = (volts/1023.00);
  volts *= 3.3; //calcuate current voltage
  RTU_Event[eventCounter].adcValue = volts;
  RTU_Event[eventCounter].rtuId = IP_Digits; //last 2 digits on IP for the Pi running the RTU
  RTU_Event[eventCounter].button1 = buttonSignals[0];
  RTU_Event[eventCounter].button2 = buttonSignals[1];
  RTU_Event[eventCounter].button3 = buttonSignals[2];
  RTU_Event[eventCounter].button4 = buttonSignals[3];
  RTU_Event[eventCounter].button5 = buttonSignals[4];
  RTU_Event[eventCounter].led1 = LEDSignals[0];
  RTU_Event[eventCounter].led2 = LEDSignals[1];
  RTU_Event[eventCounter].led3 = LEDSignals[2];
  RTU_Event[eventCounter].led4 = LEDSignals[3];
  RTU_Event[eventCounter].switch1 = switchSignals[0];
  RTU_Event[eventCounter].switch2 = switchSignals[1];
  updateEvents++;
  if(eventCounter++ == 100){
    eventCounter = 0;
  }
  return;
}

//creates an event for an LED event
void LED_Event(){
  //get the current time and all measurements
  double volts = 0;
  gettimeofday(&tv,NULL);
  RTU_Event[eventCounter].eventTime = (double)tv.tv_sec*(1000) + (double)(0.001)*tv.tv_usec;
  strcpy(RTU_Event[eventCounter].Message,"LED Event");
  RTU_Event[eventCounter].eventId = 4;
  volts = (double)get_ADC(ADC_CHANNEL);
  volts = (volts/1023.00);
  volts *= 3.3; //calcuate current voltage
  RTU_Event[eventCounter].adcValue = volts;
  RTU_Event[eventCounter].rtuId = IP_Digits; //last 2 digits on IP for the Pi running the RTU
  RTU_Event[eventCounter].button1 = buttonSignals[0];
  RTU_Event[eventCounter].button2 = buttonSignals[1];
  RTU_Event[eventCounter].button3 = buttonSignals[2];
  RTU_Event[eventCounter].button4 = buttonSignals[3];
  RTU_Event[eventCounter].button5 = buttonSignals[4];
  RTU_Event[eventCounter].led1 = LEDSignals[0];
  RTU_Event[eventCounter].led2 = LEDSignals[1];
  RTU_Event[eventCounter].led3 = LEDSignals[2];
  RTU_Event[eventCounter].led4 = LEDSignals[3];
  RTU_Event[eventCounter].switch1 = switchSignals[0];
  RTU_Event[eventCounter].switch2 = switchSignals[1];
  updateEvents++;

  if(eventCounter++ == 100){
    eventCounter = 0;
  }
  return;
}

//thread function that deals with the ADC
void* ADC_handler(void* args){
  uint16_t ADC_Value;
  //setup the SPI channel
  if(wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED) < 0) {
		printf("wiringPiSPISetup failed\n");
	}
  double period = 0;
  struct sched_param param;
  struct itimerspec itval;
  uint64_t num_periods = 0;
  int sched_dummy;
  double ADC_Voltage = 0;
  int powerLossFlag = 0;
  int lossFlag = 0;
  int overloadFlag = 0;
  int Period;

  param.sched_priority = 60;
  sched_dummy = sched_setscheduler (0, SCHED_FIFO, &param);
  int timer_fd = timerfd_create (CLOCK_MONOTONIC, 0);
  itval.it_interval.tv_sec = 0;	//no second intervals
  itval.it_value.tv_sec = 0;	//no starting seconds
  itval.it_interval.tv_nsec = 10000000;	//1millisecond for period
  itval.it_value.tv_nsec = 10000;	//starts after 10 microseconds
  sched_dummy = timerfd_settime(timer_fd, 0, &itval, NULL);
  read(timer_fd, &num_periods, sizeof(num_periods));
  while(read(timer_fd, &num_periods, sizeof(num_periods))){

    //get the current adc value
    ADC_Value = get_ADC(ADC_CHANNEL);
    ADC_Voltage = ((double)ADC_Value/1023.00);
    ADC_Voltage *= 3.3; //converts the digital value into voltage

    //checks for power loss
    powerLossFlag = ADC_Average(ADC_Voltage);

    //only allows one event until voltage returns to normal operation
    if((powerLossFlag == -1 || ADC_Voltage < 0.01)){
      if(lossFlag == 0){
        NoPowerEvent(ADC_Voltage); //create event with detected voltage
        lossFlag = 1;
      }
    }
    else{
      powerLossFlag = 0;
      lossFlag = 0;
    }
    //checks for overload
    if((ADC_Voltage > 2.0 || ADC_Voltage < 1.0)){
      if(overloadFlag == 0){
        OverloadEvent(ADC_Voltage);
        overloadFlag = 1;
      }
    }
    else{
      overloadFlag = 0;
    }
    usleep(100000);
  }
}

void* ButtonRead(void* args){
  char buffer[10];
  int dummy = 0;
  while(1){
    //clears button signal global array
    bzero(buffer,10);
    buttonSignals[0] = 0;
    buttonSignals[1] = 0;
    buttonSignals[2] = 0;
    buttonSignals[3] = 0;
    buttonSignals[4] = 0;
    dummy = read(cdev_id,buffer,sizeof(buffer)); //read from character device
    if(buffer[0] == '1'){
      buttonSignals[0] = 1;
      buttonEvent(); //create event
    }
    if(buffer[0] == '2'){
      buttonSignals[1] = 1;
      buttonEvent(); //create event
    }
    if(buffer[0] == '3'){
      buttonSignals[2] = 1;
      buttonEvent(); //create event
    }
    if(buffer[0] == '4'){
      buttonSignals[3] = 1;
      buttonEvent(); //create event
    }
    if(buffer[0] == '5'){
      buttonSignals[4] = 1;
      buttonEvent(); //create event
    }
    usleep(1000); //small sleep so the character device read does not crash
  }
}


//handles input from historian and LED events
void* LED_Handler(void* args){
  char buffer[40];

  wiringPiSetup();	// wiringPiSetupGpio() could be used. The numbers for the ports would
	pinMode(LED1, OUTPUT);	// Configure GPIO2, which is the one connected to the red LED.
	pinMode(LED2, OUTPUT);
	pinMode(LED3, OUTPUT);
	pinMode(LED4, OUTPUT);
	digitalWrite(LED1, LOW); //turns LED off initially
 	digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW); //turns LED off initially
 	digitalWrite(LED4, LOW);

  while(1){
    bzero(buffer,40); //clear buffer
    n = recvfrom(sock, buffer, 40, 0, (struct sockaddr *)&addr, &fromlen); //recieve from the Historian

    if(n>0){ //if a message was sent check against cases for the LED
      if(buffer[0] == '1'){
        if(buffer[1] == '1'){
          //message "11" will cause LED to turn on
          digitalWrite(LED1,HIGH); //turn on LED
          LEDSignals[0] = 1; //mark LED 1 as on

          LED_Event(); //create event
          bzero(buffer,40);

        }
        else if(buffer[1] == '0'){ // message "10" will cause LED 1 to be turned off
          digitalWrite(LED1,LOW); //turn off LED
          LEDSignals[0] = 0; //mark LED 1 as off

          LED_Event(); //create event
          bzero(buffer,40);

        }
      }
      if(buffer[0] == '2'){ //message "21" will cause LED to turn on
        if(buffer[1] == '1'){
          digitalWrite(LED2,HIGH); //turn on LED
          LEDSignals[1] = 1; //mark LED 2 as on

          LED_Event(); //create event
          bzero(buffer,40);

        }
        else if(buffer[1] == '0'){ // message "20" will cause LED 2 to be turned off
          digitalWrite(LED2,LOW); //turn off LED 2
          LEDSignals[1] = 0; //mark LED 2 as off

          LED_Event(); //create event
          bzero(buffer,40);

        }
      }
      if(buffer[0] == '3'){ //message "31" will cause LED to turn on
        if(buffer[1] == '1'){
          digitalWrite(LED3,HIGH); //turn on LED 3
          LEDSignals[2] = 1; //mark LED 3 as on

          LED_Event(); //create event
          bzero(buffer,40);

        }
        else if(buffer[1] == '0'){ // message "30" will cause LED 3 to be turned off
          digitalWrite(LED3,LOW); //tunr off LED 3
          LEDSignals[2] = 0; //mark LED 3 as off

          LED_Event(); //clear event
          bzero(buffer,40);

        }
      }
      if(buffer[0] == '4'){ //message "41" will cause LED to turn on
        if(buffer[1] == '1'){
          digitalWrite(LED4,HIGH); //turn on LED 4
          LEDSignals[3] = 1; //mark LED 4 as on

          LED_Event(); //create event
          bzero(buffer,40);

        }
        else if(buffer[1] == '0'){ // message "40" will cause LED 4 to be turned off
          digitalWrite(LED4,LOW); //turn off LED 4
          LEDSignals[3] = 0; //makr LED 4 as off

          LED_Event(); //crete event
          bzero(buffer,40);

        }
      }
    }
    bzero(buffer,40); //clear buffer
  }
  close(sock);
}

void switch1_Event(){
  double volts = 0;
  //logic to hanlde switch signal
  if(switchSignals[0] == 1){ //if on and event is detected change to 0
    switchSignals[0] = 0;
  }
  else{
    switchSignals[0] = 1;
  }

  gettimeofday(&tv,NULL);
  //get the current time and all measurements
  RTU_Event[eventCounter].eventTime = (double)tv.tv_sec*(1000) + (double)(0.001)*tv.tv_usec;
  strcpy(RTU_Event[eventCounter].Message,"Switch Event");
  RTU_Event[eventCounter].eventId = 5;
  volts = (double)get_ADC(ADC_CHANNEL);
  volts = (volts/1023.00);
  volts *= 3.3; //calcuate current voltage
  RTU_Event[eventCounter].adcValue = volts;
  RTU_Event[eventCounter].rtuId = IP_Digits; //last 2 digits on IP for the Pi running the RTU
  RTU_Event[eventCounter].button1 = buttonSignals[0];
  RTU_Event[eventCounter].button2 = buttonSignals[1];
  RTU_Event[eventCounter].button3 = buttonSignals[2];
  RTU_Event[eventCounter].button4 = buttonSignals[3];
  RTU_Event[eventCounter].button5 = buttonSignals[4];
  RTU_Event[eventCounter].led1 = LEDSignals[0];
  RTU_Event[eventCounter].led2 = LEDSignals[1];
  RTU_Event[eventCounter].led3 = LEDSignals[2];
  RTU_Event[eventCounter].led4 = LEDSignals[3];
  RTU_Event[eventCounter].switch1 = switchSignals[0];
  RTU_Event[eventCounter].switch2 = switchSignals[1];
  updateEvents++;
  if(eventCounter++ == 100){
    eventCounter = 0;
  }

  return;
}

void switch2_Event(){
  double volts = 0;
  if(switchSignals[1] == 1){ //if on and event is detected change to 0
    switchSignals[1] = 0;
  }
  else{
    switchSignals[1] = 1;
  }
  //get the current time and all measurements
  gettimeofday(&tv,NULL);
  RTU_Event[eventCounter].eventTime = (double)tv.tv_sec*(1000) + (double)(0.001)*tv.tv_usec;
  strcpy(RTU_Event[eventCounter].Message,"Switch Event");
  RTU_Event[eventCounter].eventId = 5;
  volts = (double)get_ADC(ADC_CHANNEL);
  volts = (volts/1023.00);
  volts *= 3.3; //calcuate current voltage
  RTU_Event[eventCounter].adcValue = volts;
  RTU_Event[eventCounter].rtuId = IP_Digits; //last 2 digits on IP for the Pi running the RTU
  RTU_Event[eventCounter].button1 = buttonSignals[0];
  RTU_Event[eventCounter].button2 = buttonSignals[1];
  RTU_Event[eventCounter].button3 = buttonSignals[2];
  RTU_Event[eventCounter].button4 = buttonSignals[3];
  RTU_Event[eventCounter].button5 = buttonSignals[4];
  RTU_Event[eventCounter].led1 = LEDSignals[0];
  RTU_Event[eventCounter].led2 = LEDSignals[1];
  RTU_Event[eventCounter].led3 = LEDSignals[2];
  RTU_Event[eventCounter].led4 = LEDSignals[3];
  RTU_Event[eventCounter].switch1 = switchSignals[0];
  RTU_Event[eventCounter].switch2 = switchSignals[1];
  updateEvents++;
  if(eventCounter++ == 100){
    eventCounter = 0;
  }
  return;
}

//thread funtion to setup ISR for switch 1
void* switch1_Handler(void* args){
  wiringPiSetup();
  //both risign and falling edge detection
  wiringPiISR(26,INT_EDGE_BOTH,&switch1_Event);
}

//thread funtion to setup ISR for switch 1
void* switch2_Handler(void* args){
  wiringPiSetup();
  //both risign and falling edge detection
  wiringPiISR(23,INT_EDGE_BOTH,&switch2_Event);
}


//thread function to send updates to historian
void* Historian_Handler(void* args){
  char buffer[200];
  char buff1[50];
  char buff2[50];
  char buff3[50];
  char buff4[50];
  char num_buffer[100];
  double volts = 0;
  double sysTime = 0;
  struct sched_param param;
  struct itimerspec itval;
  uint64_t num_periods = 0;
  int sched_dummy;
  int tempHolder = 0;
  int i = 0;

  //real time task setup
  param.sched_priority = 60;
  sched_dummy = sched_setscheduler (0, SCHED_FIFO, &param);
  int timer_fd = timerfd_create (CLOCK_MONOTONIC, 0);
  itval.it_interval.tv_sec = 1;	//1 second period
  itval.it_value.tv_sec = 1;	//wait 1 second to start
  itval.it_interval.tv_nsec = 0;
  itval.it_value.tv_nsec = 0;
  sched_dummy = timerfd_settime(timer_fd, 0, &itval, NULL);
  read(timer_fd, &num_periods, sizeof(num_periods));

  while(read(timer_fd, &num_periods, sizeof(num_periods))){
    //gets measuremnts for current system status
    volts = (int)get_ADC(ADC_CHANNEL);
    volts = ((double)volts/1023.00);
    volts *= 3.3; //calculates voltage
    gettimeofday(&tv, NULL);
    sysTime = (double)tv.tv_sec*(1000) + (double)(0.001)*tv.tv_usec; //system time
    //printf("\n");
    //stores all current signal data into a message
    sprintf(buff1,"%.03f|%.03f|",sysTime,volts);
    sprintf(buff2,"%d|%d|%d|%d|%d|",buttonSignals[0],buttonSignals[1],buttonSignals[2],buttonSignals[3],buttonSignals[4]);
    sprintf(buff3, "%d|%d|%d|%d|",LEDSignals[0],LEDSignals[1],LEDSignals[2],LEDSignals[3]);
    sprintf(buff4, "%d|%d|---Normal Operations---|0|%d|",switchSignals[0],switchSignals[1],IP_Digits);
    strcat(buffer,buff1);
    strcat(buffer,buff2);
    strcat(buffer,buff3);
    strcat(buffer,buff4);
    //printf("%s\n",buffer); //used for testing, can be commented out
    addr.sin_addr.s_addr = inet_addr("128.206.19.255");
    //send the message to the Historian via a broadcast
    n = sendto(sock, buffer, 200, 0,(struct sockaddr *)&addr, fromlen);
    //clear buffers used
    bzero(buffer,200);
    bzero(buff1,50);
    bzero(buff2,50);
    bzero(buff3,50);
    bzero(buff4,50);
    tempHolder = eventCounter;
    if(tempHolder > 0){ //stores number of events that occured
      for(i=0;i<tempHolder;i++){
        //stores event data into strings
        sprintf(buff1,"%.03f|%.03f|",RTU_Event[i].eventTime,RTU_Event[i].adcValue);
        sprintf(buff2,"%d|%d|%d|%d|%d|",RTU_Event[i].button1,RTU_Event[i].button2,RTU_Event[i].button3,RTU_Event[i].button4,RTU_Event[i].button5);
        sprintf(buff3, "%d|%d|%d|%d|",RTU_Event[i].led1,RTU_Event[i].led2,RTU_Event[i].led3,RTU_Event[i].led4);
        sprintf(buff4, "%d|%d|%s|%d|%d|",RTU_Event[i].switch1,RTU_Event[i].switch2,RTU_Event[i].Message,RTU_Event[i].eventId,RTU_Event[i].rtuId);
        strcat(buffer,buff1);
        strcat(buffer,buff2);
        strcat(buffer,buff3);
        strcat(buffer,buff4);
        printf("%s\n",buffer);
        addr.sin_addr.s_addr = inet_addr("128.206.19.255");
        //broadcast event
        n = sendto(sock, buffer, 200, 0,(struct sockaddr *)&addr, fromlen);

        bzero(buffer,200);
        bzero(buff1,50);
        bzero(buff2,50);
        bzero(buff3,50);
        bzero(buff4,50);
      }
      eventCounter = 0;
      tempHolder = 0;
      clearEvents();
    }
    clearEvents(); //clear all events from the event buffer
  }
}


int main(int argc, char* argv[]){
  eventIdCounter = 1;
  eventCounter = 0;
  char IP[2];
  char garbage[30];
  updateEvents = 0;
  sem_init(&my_semaphore, 0, 1);
  clearEvents();
  //open charcter device to be read from
  if((cdev_id = open(CHAR_DEV, O_RDWR)) == -1) {
    printf("Cannot open device %s\n", CHAR_DEV);
    exit(1);
  }
  sock = socket(AF_INET, SOCK_DGRAM, 0); // Creates socket. Connectionless.
  	if (sock < 0){
		error("Opening socket");
		}
 	length = sizeof(server);			// length of structure
  bzero(&server,length);			// sets all values to zero. memset() could be used
  server.sin_family = AF_INET;		// symbol constant for Internet domain
  server.sin_addr.s_addr = INADDR_ANY;		// IP address of the machine on which
	// the server is running
 	server.sin_port = htons(2100);	// port number
  // binds the socket to the address of the host and the port number
  if (bind(sock, (struct sockaddr *)&server, length) < 0)
   error("binding");
  // change socket permissions to allow broadcast
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &boolval, sizeof(boolval)) < 0){
   		printf("error setting socket options\n");
   		exit(-1);
  }
  fromlen = sizeof(struct sockaddr_in);	// size of structure
	ifr.ifr_addr.sa_family = AF_INET;
 	//gets the IP of the PI dynamically
 	strncpy(ifr.ifr_name, "wlan0", IFNAMSIZ-1);
 	ioctl(sock, SIOCGIFADDR, &ifr);
  strcpy(My_IP,inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
  IP[0] = My_IP[11];
  IP[1] = My_IP[12];
  printf("IP: %s",IP);
  IP_Digits = atoi(IP);
  pthread_t adcThread, LEDThread, buttonThread, switch1Thread, switch2Thread, histUpdate;
  //create thread for each task
  pthread_create(&adcThread,NULL, (void*)ADC_handler,NULL);
  usleep(1000);
  pthread_create(&LEDThread,NULL, (void*)LED_Handler,NULL);
  usleep(1000);
  pthread_create(&buttonThread,NULL, (void*)ButtonRead,NULL);
  usleep(1000);
  pthread_create(&switch1Thread,NULL, (void*)switch1_Handler,NULL);
  usleep(1000);
  pthread_create(&switch2Thread,NULL, (void*)switch2_Handler,NULL);
  usleep(1000);
  pthread_create(&histUpdate,NULL, (void*)Historian_Handler,NULL);

  usleep(1000);
  while(1){ //loop to keep main from finishing
    sleep(1);
  }

  return 0;
}
