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

#define MSG_SIZE 40

//returns -1 on error
int main(int argc, char *argv[]) {
	int sock, length;
	struct sockaddr_in broadcast, receive;
	socklen_t fromlen;
	int boolval = 1;
	int n = 0;
	char buffer[MSG_SIZE];

    if (argc < 2){                              //if no port given
		printf("usage: %s port\n", argv[0]);     
        return -1;
    }
    sock = socket(AF_INET, SOCK_DGRAM, 0); //create socket
    if (sock < 0){
    	error("Opening socket");
    }
     
   length = sizeof(broadcast);            //size of server
    bzero(&broadcast,length);            //reset values to zero
    broadcast.sin_family = AF_INET;        //symbol constant for Internet domain
    broadcast.sin_addr.s_addr = inet_addr("192.206.19.255");        //IP address of the machine
    broadcast.sin_port = htons(atoi(argv[1])); 
 
    if (bind(sock, (struct sockaddr *)&broadcast, length) < 0){ //couldn't bind
        error("binding");
    }
 
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &boolval, sizeof(boolval)) < 0){ //couldn't set socket options
        printf("error setting socket options\n");
        exit(-1);
    }

	do
  	{

		// bzero: to "clean up" the buffer. The messages aren't always the same length...
		bzero(buffer,MSG_SIZE);		// sets all values to zero. memset() could be used
		printf("Please enter the message (! to exit): ");
		fgets(buffer,MSG_SIZE-1,stdin); // MSG_SIZE-1 because a null character is added

		if (buffer[0] != '!'){// send message to anyone there...
			n = sendto(sock, buffer, strlen(buffer), 0,
			(const struct sockaddr *)&broadcast,length);
			if (n < 0)
		  		error("Sendto");

			// receive message
			n = recvfrom(sock, buffer, MSG_SIZE, 0, (struct sockaddr *)&receive, &length);
			if (n < 0)
		  		error("recvfrom");

			printf("Received something: %s\n", buffer);
		}
	} while (buffer[0] != '!');

	close(sock);// close socket.
	return 0;
}


