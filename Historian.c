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
#include <pthread.h>
#include <semaphore.h>


#include <sys/time.h>

//stores data received from RTU for each event
typedef struct rtuData {
	int event_num;
	int rtu_num;
	float adc;
	double Time;
	int BUTTON_1;
	int BUTTON_2;
	int BUTTON_3;
	int BUTTON_4;
	int BUTTON_5;
	int LED_1;
	int LED_2;
	int LED_3;
	int LED_4;
	int SWITCH_1;
	int SWITCH_2;
	char type[25];
} rtuData;

//holds a list of events received from the RTUs
typedef struct dataList {
	struct dataList * next;
	struct rtuData* data;
} dataList;

dataList* head = NULL;
dataList* tail = NULL;
int eventCount, sock;
char msg [3];
char tempData [200][200];
struct timeval timestamp;
sem_t storing; //don't want to display events at the same time new events are being logged
struct sockaddr_in broadcast, single; //broadcast sends to all RTUs, single sends to one

//prints every parameter's value for data
int printRtuData(rtuData* data) {
	printf("Event number: %d RTU number: %d\n", data->event_num, data->rtu_num);
	printf("adc Voltage: %f V, Event time: %lf ms\n", data->adc, data->Time);
	printf("Button 1: %d Button 2: %d Button 3: %d Button 4: %d Button 5: %d\n", data->BUTTON_1, data->BUTTON_2, data->BUTTON_3, data->BUTTON_4, data->BUTTON_5);
	printf("LED 1: %d LED 2: %d LED 3: %d LED 4: %d\n", data->LED_1, data->LED_2, data->LED_3, data->LED_4);
	printf("Switch 1: %d Switch 2: %d\n", data->SWITCH_1, data->SWITCH_2);
	printf("%s\n\n",data->type);
}

//converts a character array received form an RTU into an rtuData node, which is returned
rtuData* storeData(char tempData[200]) {
	char* token[100];
	int i = 0;
	rtuData* data = malloc(sizeof(rtuData));
	if(data == NULL){
		printf("Did not malloc\n");
	}
	token[i] = strtok(tempData, "|");//"|" is our format for separating each parameter
	data->Time = atof(token[i]);
	data->adc = atof(strtok(NULL, "|"));
	data->BUTTON_1 = atoi(strtok(NULL, "|"));
	data->BUTTON_2 = atoi(strtok(NULL, "|"));
	data->BUTTON_3 = atoi(strtok(NULL, "|"));
	data->BUTTON_4 = atoi(strtok(NULL, "|"));
	data->BUTTON_5 = atoi(strtok(NULL, "|"));
	data->LED_1 = atoi(strtok(NULL, "|"));
	data->LED_2 = atoi(strtok(NULL, "|"));
	data->LED_3 = atoi(strtok(NULL, "|"));
	data->LED_4 = atoi(strtok(NULL, "|"));
	data->SWITCH_1 = atoi(strtok(NULL, "|"));
	data->SWITCH_2 = atoi(strtok(NULL, "|"));
	strcpy(data->type, strtok(NULL, "|"));
	data->event_num = atoi(strtok(NULL, "|"));
	data->rtu_num = atoi(strtok(NULL, "|"));

	//printRtuData(data);
	return data;
}

//inserts a new node into the end of the linked list of events
void insertList(dataList** head, dataList** tail, rtuData* data){
	dataList* new = malloc(sizeof(dataList));

	new->data = data;
	new->next = NULL;

	if(*head == NULL){
		*head = new;
		*tail = new;
		return;
	}
	else{
		(*tail)->next = new;
		(*tail) = new;
		return;
	}
}

//traverses the linked list of events, printing the data contained for each
void printList(dataList** head){
	dataList* temp = *head;
	if(*head == NULL){//no events yet
		printf("List Empty\n");
		return;
	}
	else{
		while(temp != NULL){//loop to end of list
			printRtuData(temp->data);
			temp = temp->next;
		}
		return;
	}
}

//swaps positions of two nodes in the event linked list
void swap(dataList* p1, dataList* p2){
	rtuData* temp = p1->data;
	p1->data = p2->data;
	p2->data = temp;
}

//sorts nodes in the linked list by time received, from earliest to latest
void sortlist(dataList** head, dataList** tail){
	dataList* start = *head;
	dataList* traverse;
	dataList* min;
	while(start->next){
		min = start;
		traverse = start->next;
		while(traverse){
			if(min->data->Time > traverse->data->Time){
				min = traverse;
			}
			traverse = traverse->next;
		}
		swap(start, min);
		start = start->next;
	}
}

//receives messages sent by the RTUs
void *rtuReceiver(void *params) {
	struct sockaddr_in receive;
	socklen_t length;
	int n, i, eventId;
	int j = 0;
	length = sizeof(receive);
	i = 0;
	while(1){
		bzero(tempData[0],200);
		n = recvfrom(sock, tempData[0], 200, 0, (struct sockaddr *) &receive, &length);
		if(tempData[0][0] == '1'){
			insertList(&head,&tail,storeData(tempData[i]));
		}
	}
	fflush(stdout);
}

//allows the user to give commands to the RTUs or print event logs
int main(int argc, char *argv[]) {
	int length, option, led, change_led, cancel, sent;
	socklen_t fromlen;
	int boolval = 1;
	int n = 0;
	char buffer[200];
	sem_init(&storing, 0, 1);
    if (argc < 2){                              //if no port given
		printf("usage: %s port\n", argv[0]);
        exit(-1);
    }
    sock = socket(AF_INET, SOCK_DGRAM, 0); //create socket
    if (sock < 0){
    	printf("Error opening socket");
		exit(-1);
    }

   	length = sizeof(broadcast);            //size of server
    bzero(&broadcast,length);            //reset values to zero
    broadcast.sin_family = AF_INET;        //symbol constant for Internet domain
    broadcast.sin_addr.s_addr = INADDR_ANY;        //IP address of the machine
    broadcast.sin_port = htons(atoi(argv[1]));

    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &boolval, sizeof(boolval)) < 0){ //couldn't set socket options
        printf("error setting socket options\n");
        exit(-1);
    }
	 struct ifreq ifr;
    ifr.ifr_addr.sa_family = AF_INET; //internet domain
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
    ioctl(sock, SIOCGIFADDR, &ifr);
	broadcast.sin_addr.s_addr = inet_addr("128.206.19.255");
	strcpy(msg, "Connection established");
	n = sendto(sock, &msg, sizeof(msg), 0, (const struct sockaddr *) &broadcast, sizeof (struct sockaddr_in));
	pthread_t receiver;
	if (pthread_create(&receiver, NULL, (void *) &rtuReceiver, NULL) != 0){//creates a thread to handle receiving from the RTUs
		printf("Error creating pthread");
		exit(-1);
   	}

	while(1) { //this loop allows user input on the terminal running Historian.c to view past events and/or manipulate the LEDs
    	printf("\nAvailable functions: 1. Turn an LED on/off | 2. View system event log | 3. Exit program\n");
    	printf("Enter a command: ");
    	scanf("%d", &option);
		switch(option) {
			case 1: //LEDs
				led = 0;
				while(led < 1 || led > 4) {//loop until a valid LED is given
					printf("Select an LED (1-4): ");
					scanf("%d", &led);
				}
				change_led = -1;//default is to do nothing
				switch (led) {
					case 1:
							printf("Enter 1 to turn LED 1 on, 0 to turn LED 1 off: ");
							scanf("%d", &change_led);
							if(change_led == 1) {
								strcpy(msg, "11"); //msg is 2 characters long, the first is the number of the LED to change (1-4), the second is 1 to turn the LED on or 0 to turn the LED off
								printf("LED 1 will be turned on.\n");
							}
							else if(change_led == 0) {
								strcpy(msg, "10");
								printf("LED 1 will be turned off.\n");
							}
							else
								printf("Invalid value, LED 1 status unchanged.\n");
						break;
					case 2:
						printf("Enter 1 to turn LED 2 on, 0 to turn LED 2 off: ");
							scanf("%d", &change_led);
							if(change_led == 1) {
								strcpy(msg, "21");
								printf("LED 2 will be turned on.\n");
							}
							else if(change_led == 0) {
								strcpy(msg, "20");
								printf("LED 2 will be turned off.\n");
							}
							else
								printf("Invalid value, LED 2 status unchanged.\n");
						break;
					case 3:
						printf("Enter 1 to turn LED 3 on, 0 to turn LED 3 off: ");
							scanf("%d", &change_led);
							if(change_led == 1) {
								strcpy(msg, "31");
								printf("LED 3 will be turned on.\n");
							}
							else if(change_led == 0) {
								strcpy(msg, "30");
								printf("LED 3 will be turned off.\n");
							}
							else
								printf("Invalid value, LED 3 status unchanged.\n");
						break;
					case 4:
						printf("Enter 1 to turn LED 4 on, 0 to turn LED 4 off: ");
							scanf("%d", &change_led);
							if(change_led == 1) {
								strcpy(msg, "41");
								printf("LED 4 will be turned on.\n");
							}
							else if(change_led == 0) {
								strcpy(msg, "40");
								printf("LED 4 will be turned off.\n");
							}
							else
								printf("Invalid value, LED 4 status unchanged.\n");
						break;
					default:
						break;
				}
				char rtuIP[15];//IP of the RTU to change
				int rtuID=0;
				printf("Enter the last two digits of the IP of the RTU whose LED should change: ");
				scanf("%d", &rtuID);
				sprintf(rtuIP, "128.206.19.%d", rtuID);//use user input as last two digits of target RTU's IP
				single.sin_addr.s_addr = inet_addr(rtuIP);
				single.sin_family = AF_INET;
				single.sin_port = htons(atoi(argv[1]));
				n = sendto (sock, &msg, sizeof(msg), 0, (const struct sockaddr *) &single, sizeof (struct sockaddr_in));//send msg to selected RTU
				if (n < 0) {
		   	    	printf ("Error: could not send data to selected RTU.\n");
					cancel = 0; //so users can cancel the command
					sent = 0;
					printf("Enter the last two digits of a valid IP or enter -1 to cancel the command");
					scanf("%d", &rtuID);
					while(cancel != -1 ) {//loop until a valid IP is entered or the user cancels the command
						sprintf(rtuIP, "128.206.19.%d", rtuID);
						single.sin_addr.s_addr = inet_addr(rtuIP);
						single.sin_family = AF_INET;
						single.sin_port = htons(atoi(argv[1]));
						n = sendto (sock, &msg, sizeof(msg), 0, (const struct sockaddr *) &single, sizeof (struct sockaddr_in));
						if (n < 0) {
			   	    		printf ("Error: could not send data to selected RTU.\n");
							printf("Enter the last two digits of a valid IP or enter -1 to cancel the command");
							scanf("%d", &rtuID);
						} else { //send successful
							sent = 1;
							printf("Command sent successfully\n");
						}
					}
					if(cancel == -1) {//message if command cancelled
						printf("Command cancelled.\n");
					}
				}
				else {//message if command was successful
					printf("Command sent successfully.\n");
				}
				break;
			case 2: //log
				printf("Printing event log: \n\n");
				sem_wait(&storing); //if events are currently being stored, wait until done
				sortlist(&head,&tail);
				printf("______________________________\n");
				printList(&head);
				sem_post(&storing); //done printing events, release semaphore
				printf("______________________________\n\n");
				printf("Finished.\n");
				break;
			case 3: //exit
				printf("Program terminated\n");
				exit(0);
			default: //invalid input
				printf("Error: %d is not a valid command.\n", option);
		}
		sleep (1); //execute once per second
	}
	close(sock);// close socket.
	return 0;
}
