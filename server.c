#include "server.h"
#include "shared.h"

Interface eth0;
UnixDomainSocket *listeningSocket;

char * serverIP() {
	return eth0.ip_addr;
}

void listenLoop() {
	fd_set set;
	int ret;

	while (1) {
		printf("waiting on select\n");

		FD_ZERO(&set);
		FD_SET(listeningSocket->fd, &set);
		ret = select(listeningSocket->fd + 1, &set, NULL, NULL, NULL);
		printf("select returned: %d\n", ret);

		if (ret > 0) {
			//unix domain was written to!
			char message[MAXLINE];
			char fromAddr[MAXLINE];
			int port;
			msg_recv(listeningSocket->fd, message, fromAddr, &port);

			printf("The server recieved a message in it's Unix Domain Socket\n");
			printf("From: %s\nMessage: %s\n", fromAddr, message);
			
			if(strcmp("TIME", message) == 0) {
				//send time
				time_t ts = time(NULL);
    			char *buff = ctime(&ts);
				printf("SENDING TIME TO CLIENT\n");
				msg_send(listeningSocket->fd, fromAddr, port, buff, -1);
			}
			else {
				//send echo
				printf("SENDING ECHO TO CLIENT\n");
				msg_send(listeningSocket->fd, fromAddr, port, message, -1);
			}
		}
	}
}

int main(int argc, char **argv) {
	printf("sever running\n");

	eth0 = getEth0();
	printf("Server IP: %s\n", serverIP());

	listeningSocket = unixDomainSocketMake(UnixDomainSocketTypeServer, 1, NULL);

	listenLoop();

	return 0;
}
