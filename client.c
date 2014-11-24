#include "client.h"
#include <stdio.h>
#include "shared.h"

Interface eth0;

char destIp[MAXLINE];
char input[MAXLINE];
char message[MAXLINE];

char * clientIP() {
	return eth0.ip_addr;
}

int count=0;
void sendMessage(UnixDomainSocket *socket, char *destIp, char *message, int rediscover) {
	// count++;
	// if(count != 2)
	if(strcmp(clientIP(), destIp) == 0) {
		printf("Cannot send a message to localhost: Unsupported feature\n");
	}
	else
		msg_send(socket->fd, destIp, 7847, message, rediscover);
}

void awaitResponse(UnixDomainSocket *socket) {
	fd_set set;
	int ret;
	int timeoutCount = 0;

	//Do not modify - in class professor said 5 seocnd timeout no sense in performing more than one re-attemp
	while (1) {
		FD_ZERO(&set);
		FD_SET(socket->fd, &set);
		struct timeval tv = {5, 0};
		ret = select(socket->fd + 1, &set, NULL, NULL, &tv);

		if (ret > 0) {
			//unix domain was written to!
			char messageRcv[MAXLINE];
			char fromAddr[MAXLINE];
			int port;
			msg_recv(socket->fd, messageRcv, fromAddr, &port);

			time_t ts = time(NULL);
    		char *buff = ctime(&ts);
			printf("The client recieved a reply\n");
			printf("From: %s\nMessage: %s\n", fromAddr, messageRcv);
			printf("Arrival Time: %s\n", buff);
			break;
		}
		else if(ret == 0 && timeoutCount == 0) {
			//timeout - retry with the rediscover set to 1
			printf("Timeout ocurred, sending again w/ rediscover\n");
			timeoutCount++;
			sendMessage(socket, destIp, message, 1);
		}
		else if(ret == 0){
			//done
			printf("Timeout ocurred again, giving up\n");
			break;
		}
		else
			printf("select error: %d\n", ret);
	}
}

int main(int argc, char **argv) {
	eth0 = getEth0();

	printf("client running on %s\n", clientIP());

	UnixDomainSocket *socket = unixDomainSocketMake(UnixDomainSocketTypeClient, 1, NULL);

	while(1) {
		printf("Time or Echo service? (time/echo/quit)\n");
	  	int ret = scanf("%s", input);
	  	if(ret < 0) {
		  	printf("scanf() error %d\n", ret);
		  	continue;
		}
		else if(strcmp(input, "echo") == 0) {
			//echo
			printf("Message: \n");
			ret = Readline(0, input, MAXLINE);
		  	if(ret < 0) {
			  	printf("scanf() error %d\n", ret);
			  	continue;
			}
			else {
				strcpy(message, input);
				printf("Desination ip: ");
				ret = scanf("%s", input);
			  	if(ret < 0) {
				  	printf("scanf() error %d\n", ret);
				  	continue;
				}
				else {
					strcpy(destIp, input);
					sendMessage(socket, destIp, message, 0);
					awaitResponse(socket);
				}
			}
		}
		else if(strcmp(input, "time") == 0) {
			//time
			printf("Desination ip: ");
			ret = scanf("%s", destIp);
		  	if(ret < 0) {
			  	printf("scanf() error %d\n", ret);
			  	continue;
			}
			else {
				strcpy(message, "TIME");
				sendMessage(socket, destIp, message, 0);
				awaitResponse(socket);
			}
		}
		else if(strcmp(input, "quit") == 0) {
			printf("Terminating....\n");
			exit(1);
		}
		else {
			printf("Type either time, echo, or quit\n");
		}
	}

	unixDomainSocketUnlink(socket);
  	free(socket);

	return 0;
}
