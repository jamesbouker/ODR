#include "client.h"
#include <stdio.h>
#include "shared.h"

Interface eth0;

char * clientIP() {
	return eth0.ip_addr;
}

void sendMessage(UnixDomainSocket *socket, char *destIp) {
	sendToUnixDomainSocket(socket->fd, socket->sun_path, "Repeat after me!", PacketTypeRequest ,0, clientIP(), destIp);
}

void awaitResponse(UnixDomainSocket *socket) {
	while(1) sleep(1);
}

int main(int argc, char **argv) {
	char destIp[MAXLINE];
	eth0 = getEth0();

	printf("client running on %s\n", clientIP());

	UnixDomainSocket *socket = unixDomainSocketMake(UnixDomainSocketTypeClient, 1, NULL);

	printf("Destination IP: ");
  	int ret = scanf("%s", destIp);
  	if(ret < 0)
	  	printf("scanf() error %d\n", ret);
	else
		printf("Sending to IP: %s\n", destIp);

	sendMessage(socket, destIp);
	awaitResponse(socket);

  	free(socket);

	return 0;
}
