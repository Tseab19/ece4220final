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

#define MSG_SIZE 50			// message size
#define CHAR_DEV "/dev/Project" // "/dev/YourDevName"
int cdev_id, dummy;
struct ifreq ifr;
int sock, length, n;
socklen_t fromlen;
struct sockaddr_in server;
struct sockaddr_in addr;
char buffer[50];


void establishConnection(){
  sock = socket(AF_INET, SOCK_DGRAM, 0); // Creates socket. Connectionless.
  	if (sock < 0){
		error("Opening socket");
		}
 	length = sizeof(server);			// length of structure
  bzero(&server,length);			// sets all values to zero. memset() could be used
  server.sin_family = AF_INET;		// symbol constant for Internet domain
  server.sin_addr.s_addr = INADDR_ANY;		// IP address of the machine on which
											// the server is running
 	server.sin_port = htons(atoi(argv[1]));	// port number

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

  n = recvfrom(sock, buffer, MSG_SIZE, 0, (struct sockaddr *)&addr, &fromlen);
  printf("%s\n",buffer);
  bzero(buffer,MSG_SIZE);
  return;
}



int main(void){
  if((cdev_id = open(CHAR_DEV, O_RDWR)) == -1) {
		printf("Cannot open device %s\n", CHAR_DEV);
		exit(1);
	}
  establishConnection();


}
