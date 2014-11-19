#include "odr.h"
#include "prhwaddrs.h"
#include "shared.h"
#include <stdio.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <unistd.h>

//#pragma mark - Data

int numberOfInterfaces;
int lastBroadcastIdSent;
Interface *interfaces;
Interface eth0;

//#pragma mark - Main

int main(int argc, char **argv) {
	printf("ODR running\n");
	initOdr();
	printf("My IP: %s\n", myIPAddress());

	if(strcmp(myIPAddress(), "130.245.156.21") == 0)
		send_broadcast("130.245.156.25", "Hey!", 1, 2, 3);

	listenSelectLoop();

	return 0;
}

//#pragma mark - Control Loop

void listenSelectLoop() {
	fd_set set;
	int max, ret, i;
	max = 0;
	for (i = 0; i < numberOfInterfaces; i++)
		max = max(max, interfaces[i].fd);

	while (1) {
		printf("Entered select loop\n");

		FD_ZERO(&set);
		for (i = 0; i < numberOfInterfaces; i++)
			FD_SET(interfaces[i].fd, &set);

		ret = select(max + 1, &set, NULL, NULL, NULL);
		printf("select returned: %d\n", ret);
		if (ret > 0) {
			//code!
			ODRPacket packet;
			char addr[6];
			bzero(&packet, sizeof(packet));
			recieve_broadcast(interfaces[0].fd, addr, &packet, sizeof(ODRPacket));
			printf("%s Recieved broadcast: %s\nFrom %s for %s\n", myIPAddress(), packet.message, packet.from, packet.dest);
			if(strcmp(myIPAddress(), packet.dest) == 0) {
				printf("Packet made it to destination!!!!\n");
				//inform the server here!
			}
			else {
				if(lastBroadcastIdSent != packet.broadcastId) {
					//never saw this guy before - re broadcast
					lastBroadcastIdSent = packet.broadcastId;
					printf("Forwarding broadcast with id: %d\n", packet.broadcastId);
					send_broadcast(packet.dest, packet.message, packet.broadcastId, packet.type, packet.rediscover);
				}
				else {
					printf("Ignoring broadcast with id: %d\n", packet.broadcastId);
				}
			}
		}
	}
}

//#pragma mark - Initialization

void initOdr() {
	struct 	hwa_info *hwa, *hwahead;
	struct 	sockaddr *sa;
	char   	*ptr;
	int    	i, prflag;
	int 	index;

	printf("\n");

	index = 0;
	numberOfInterfaces = 0;
	for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {
		if(strcmp(hwa->if_name, "lo") != 0 && strcmp(hwa->if_name, "eth0") != 0)
			numberOfInterfaces++;
	}

	printf("Number of Interfaces: %d\n", numberOfInterfaces);
	interfaces = malloc(sizeof(Interface) * numberOfInterfaces);
	for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {
		sa = hwa->ip_addr;
		if(strcmp(hwa->if_name, "eth0") == 0) {
			char *ip_addr = sock_ntop_host(sa, sizeof(*sa));
			strcpy(eth0.ip_addr, ip_addr);
			strcpy(eth0.if_name, hwa->if_name);
			eth0.if_index = hwa->if_index;
			eth0.fd = -1;
		}
		else if(strcmp(hwa->if_name, "lo") != 0) {
			strcpy(interfaces[index].if_name, hwa->if_name);
			if ( (sa = hwa->ip_addr) != NULL) {
				char *ip_addr = sock_ntop_host(sa, sizeof(*sa));
				strcpy(interfaces[index].ip_addr, ip_addr);
			}
			prflag = 0;
			i = 0;
			do {
				if (hwa->if_haddr[i] != '\0') {
					prflag = 1;
					break;
				}
			} while (++i < IF_HADDR);
			if (prflag) {
				ptr = hwa->if_haddr;
				i = IF_HADDR;
				do {
					char buf[3];
					snprintf(buf, 3, "%.2x", *ptr & 0xff);
					ptr++;
				} while (--i > 0);
			}

			int x;
			for(x = 0; x < IF_HADDR; x++)
				interfaces[i].hw_addr[x] = hwa->if_haddr[x];

			//create socket and bind
			interfaces[index].if_index = hwa->if_index;
			struct sockaddr_ll addr;
			short proto = PROTO_NUM;
			int fd = socket(AF_PACKET, SOCK_DGRAM, htons(proto));
			interfaces[index].fd = fd;
			if(fd < 0) {
				printf("socket() error - errno %d\n",errno);
				exit(errno);
			}
			bzero(&addr, sizeof(addr));
			addr.sll_family = AF_PACKET;
			addr.sll_protocol = htons(proto);
			addr.sll_ifindex = hwa->if_index;
			if(bind(fd, (SA*)&addr, sizeof(addr)) < 0) {
				printf("Bind error\n");
				exit(1);
			}

			printInterface(interfaces[index]);
			index++;
		}
	}

	free_hwa_info(hwahead);
}

void send_broadcast(char* destIp, char *msg, int broadcastId, int type, int rediscover) {
	lastBroadcastIdSent = broadcastId;
	printf("Sent broadcast from myip: %s!\n", myIPAddress());
	ODRPacket packet;
	strcpy(packet.from, myIPAddress());
	strcpy(packet.dest, destIp);
	strcpy(packet.message, msg);
	packet.broadcastId = broadcastId;
	packet.type = type;
	packet.rediscover = rediscover;
	int i;
	for(i=0; i<numberOfInterfaces; i++)
		send_broadcast_helper(interfaces[i].fd, interfaces[i].if_index, &packet, sizeof(ODRPacket));
}

int send_broadcast_helper(int fd, int if_index, void *msg, int len) {
	char boradcast_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	struct sockaddr_ll addr;
	bzero(&addr, sizeof(addr));
	
	addr.sll_halen = 6;
	addr.sll_hatype = 1;
	addr.sll_pkttype=PACKET_BROADCAST;
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(PROTO_NUM);
	addr.sll_ifindex = if_index;
	int i;
	for(i=0; i<6; i++) 
		addr.sll_addr[i] = boradcast_addr[i];
	for(i=6; i<8; i++)
		addr.sll_addr[i] = 0x00;
	
	int sentSize = sendto(fd, msg, len, 0, (struct sockaddr *)&addr, sizeof(addr));
	if(sentSize <= 0) printf("sendto() error: %d", errno);

	return sentSize;
}

int recieve_broadcast(int fd, char *addr, void *msg, int len) {
	void *tempBuffer = malloc(len);
	struct sockaddr_ll tempAddr;

	bzero(&tempAddr, sizeof(tempAddr));
	socklen_t size = sizeof(struct sockaddr_ll);
	int ret = recvfrom(fd, tempBuffer, len, 0, (struct sockaddr*)&tempAddr, &size);
	if(ret <= 0) printf("recvfrom() error:  %d\n",errno);

	if((void*)&tempAddr == NULL)
		printf("FUCK\n");
	else {
		memcpy(addr,tempAddr.sll_addr,6);
		memcpy(msg,tempBuffer,len);
	}

	free(tempBuffer);

	return ret;
}

char * myIPAddress() {
	return eth0.ip_addr;
}
