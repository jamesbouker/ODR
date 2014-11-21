#include "shared.h"
#include "prhwaddrs.h"

//#pragma mark - Interface

void printInterface(Interface inf) {
	printf("Interface: %s\n", inf.if_name);
	printf("FD: %d\n", inf.fd);
	printf("if_index: %d\n", inf.if_index);
	printf("ip_addr: %s\n", inf.ip_addr);
}

Interface getEth0() {
	struct 	hwa_info *hwa;
	struct 	sockaddr *sa;
	Interface eth0;
	bzero(&eth0, sizeof(eth0));
	for (hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {
		sa = hwa->ip_addr;
		if(strcmp(hwa->if_name, "eth0") == 0) {
			char *ip_addr = sock_ntop_host(sa, sizeof(*sa));
			strcpy(eth0.ip_addr, ip_addr);
			strcpy(eth0.if_name, hwa->if_name);
			eth0.if_index = hwa->if_index;
			eth0.fd = -1;
		}
	}
	return eth0;
}

//#pragma mark - Unix Domain Sockets

UnixDomainSocket * unixDomainSocketMake(UnixDomainSocketType type, int shouldBind, char *init_sun_path) {
	UnixDomainSocket *unixSocket = malloc(sizeof(UnixDomainSocket));
	struct sockaddr_un actualSocket; 
	bzero(&actualSocket, sizeof(actualSocket));
	char buff[MAXLINE];
	int fd = socket(AF_LOCAL, SOCK_DGRAM, 0);
	char sun_path[MAXLINE];

	if(type == UnixDomainSocketTypeServer)
		strcpy(sun_path, SERVER_SUN_PATH);
	else if(type == UnixDomainSocketTypeODR)
		strcpy(sun_path, ODR_SUN_PATH);
	else if (type == UnixDomainSocketTypeClient && init_sun_path == NULL) {
		strcpy(buff, CLIENT_SUN_PATH);
		int newFd = mkstemp(buff);
		close(newFd);
		strcpy(sun_path, buff);
	}
	else if(type == UnixDomainSocketTypeClient)
		strcpy(sun_path, init_sun_path);

	if(fd < 0) {
		printf("error creating socket\n");
		exit(1);
	}
	else
		unixSocket->fd = fd;

	strcpy(unixSocket->sun_path, sun_path);
	printf("UnixDomainSocket sun_path: %s\n", unixSocket->sun_path);

	unixDomainSocketUnlink(unixSocket);
	actualSocket.sun_family = AF_LOCAL;
	strcpy(actualSocket.sun_path, unixSocket->sun_path);

	if(shouldBind) {
		if(bind(fd, (struct sockaddr *)&actualSocket, sizeof(actualSocket)) < 0) {
			printf("Bind failed in unixSocketMake\n");
			exit(1);
		}
	}

	//the client binds on the client but then needs the odr sun_path for later
	if(type == UnixDomainSocketTypeClient)
		strcpy(unixSocket->sun_path, ODR_SUN_PATH);

	printf("FD: %d\n", unixSocket->fd);

	return unixSocket;
}

void unixDomainSocketUnlink(UnixDomainSocket * unixSocket) {
	unlink(unixSocket->sun_path); 
}

int sendToUnixDomainSocket(int fd, char *sun_path, char *message, PacketType type, int rediscover, char *fromIp, char *toIp, int port) {
	UnixDomainPacket packet;
	bzero(&packet, sizeof(packet));

	strcpy(packet.from, fromIp);
	strcpy(packet.dest, toIp);
	strcpy(packet.message, message);
	packet.rediscover = rediscover;
	packet.type = type;
	packet.port = port;

	struct sockaddr_un actualSocket;
	bzero(&actualSocket, sizeof(actualSocket));
	actualSocket.sun_family = AF_LOCAL;
	strcpy(actualSocket.sun_path, sun_path);
	int sendRet = sendto(fd, &packet, sizeof(packet), 0, (struct sockaddr *)&actualSocket, sizeof(actualSocket));
	printf("sendToUnixDomainSocket: Ret: %d fd: %d\nFrom: %s Destination: %s \nmessage: %s sun_path: %s\ntype: %d\n\n",sendRet, fd, packet.from, packet.dest, packet.message, actualSocket.sun_path, packet.type);
	return sendRet;
}

int readFromUnixDomainSocket(int fd, char *sun_path, UnixDomainPacket *packet) {
	struct sockaddr_un actualSocket;
	socklen_t addrlen = sizeof(actualSocket);
	int ret = recvfrom(fd, packet, sizeof(packet[0]), 0, (struct sockaddr *)&actualSocket, &addrlen);
	if(sun_path != NULL)
		strcpy(sun_path, actualSocket.sun_path);

	return ret;
}

//#pragma - mark API

int msg_send(int fd, char *destAddr, int destPort, char *message, int rediscover) {
	Interface inf = getEth0();
	char *ip = inf.ip_addr;

	// A little hacky - Q: How would ODR know what sun_path to deliver to without this extra field?
	PacketType type = PacketTypeAPISend;
	if(rediscover == -1)
		type = PacketTypeAPIReply;

	int ret = sendToUnixDomainSocket(fd, ODR_SUN_PATH, message, type, rediscover, ip, destAddr, destPort);
	return ret;
}

int msg_recv(int fd, char *message, char *fromAddr, int *fromPort) {
	UnixDomainPacket packet; bzero(&packet, sizeof(packet));
	int ret = readFromUnixDomainSocket(fd, NULL, &packet);

	strcpy(message, packet.message);
	strcpy(fromAddr, packet.from);
	fromPort[0] = packet.port;
	return ret;
}
