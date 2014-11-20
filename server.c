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
			UnixDomainPacket packet; bzero(&packet, sizeof(packet));
			readFromUnixDomainSocket(listeningSocket->fd, NULL, &packet);
			printf("The server recieved a message in it's Unix Domain Socket\n");
			printf("From: %s To: %s\nMessage: %s rediscover: %d\n", packet.from, packet.dest, packet.message, packet.rediscover);
			
			if(strcmp("time", packet.message) == 0) {
				//send time
				printf("SENDING TIME TO CLIENT\n");
			}
			else {
				//send echo
				printf("SENDING ECHO TO CLIENT\n");
				sendToUnixDomainSocket(listeningSocket->fd, ODR_SUN_PATH, packet.message, PacketTypeReply, 0, serverIP(), packet.from);
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
