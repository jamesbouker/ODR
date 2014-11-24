#include "odr.h"
#include "shared.h"
#include "prhwaddrs.h"
#include "table.h"

//#pragma mark - Data
UnixDomainSocket *unixDomainSocketODR;
UnixDomainSocket *unixDomainSocketServer; 
UnixDomainSocket unixDomainSocketClient;

int numberOfInterfaces;
int lastBroadcastIdSent = 0;
int lastBroadcastIdHeard = -1;
Interface *interfaces;
Interface eth0;

Table table;
UnixDomainPacket packetFromClient;

ODRNode reqresp[50];
int reqrespSize = 0;

char client_sun_path[100];

//#pragma mark - Main

int main(int argc, char **argv) {
	startODR();

	printf("ODR running\n");
	printf("My IP: %s\n", myIPAddress());

	listenSelectLoop();

	return 0;
}

//#pragma mark - Control Loop

void listenSelectLoop() {
	fd_set set;
	int max, ret, i;
	max = unixDomainSocketODR->fd;
	for (i = 0; i < numberOfInterfaces; i++)
		max = max(max, interfaces[i].fd);

	while (1) {
		// printf("waiting on select\n");

		FD_ZERO(&set);
		for (i = 0; i < numberOfInterfaces; i++)
			FD_SET(interfaces[i].fd, &set);
		FD_SET(unixDomainSocketODR->fd, &set);

		ret = select(max + 1, &set, NULL, NULL, NULL);
		// printf("select returned: %d\n\n\n", ret);
		if (ret > 0) {
			if(FD_ISSET(unixDomainSocketODR->fd, &set)) {
				//unix domain was written to!
				UnixDomainPacket packet; bzero(&packet, sizeof(packet));
				readFromUnixDomainSocket(unixDomainSocketODR->fd, client_sun_path, &packet);
				unixDomainSocketClient = unixDomainSocketODR[0];
				strcpy(unixDomainSocketClient.sun_path, client_sun_path);

				// printf("Recieved message in my Unix Domain Socket\n");
				// printf("sun_path: %s\n", client_sun_path);
				// printf("From: %s To: %s\nMessage: %s rediscover: %d type: %d\n\n", packet.from, packet.dest, packet.message, packet.rediscover, packet.type);

				TableEntry *entry = entryForIp(&table, packet.dest);
				if(packet.rediscover != -1) {
					//FROM CLIENT
					printf("Recieved message in my Unix Domain Socket: Fromt the client\n");
					if(entry == NULL) {
						printf("Entry doesn't exist: sending broadcast\n");
						packetFromClient = packet;
						send_broadcast(packet.dest, myIPAddress(), packet.message, lastBroadcastIdSent, PacketTypeRequest, packet.rediscover, packet.port, -1, 1);
						lastBroadcastIdSent++;
					}
					else {
						if(packet.rediscover) {
							//remove and start again
							printf("rediscover set - sending broadcast\n");
							removeTableEntry(&table, packet.dest);
							packetFromClient = packet;
							send_broadcast(packet.dest, myIPAddress(), packet.message, lastBroadcastIdSent, PacketTypeRequest, packet.rediscover, packet.port, -1, 1);
							lastBroadcastIdSent++;
						}
						else {
							//send packet of APP REQ to nextNode
							printf("Sending API level message to next hop\n");
							send_to_node(packet.dest, packet.from, packet.message, PacketTypeAPISend, packet.rediscover, packet.port, entry->if_index, entry->nextHopNode, 1);
						}
					}
				}
				else {
					//FROM SERVER
					printf("ODR recieved from the server!\n");
					if(entry != NULL) {
						printf("Sending API level Reply to next hop\n");
						send_to_node(packet.dest, packet.from, packet.message, PacketTypeAPIReply, packet.rediscover, packet.port, entry->if_index, entry->nextHopNode, -1);
					}
					else {
						printf("Reverse adress not in table for: %s\n", packet.dest);
						printTable(&table);
					}
				}
			}
			else {
				//find the incoming interface
				int iIndex;
				for(iIndex=0; iIndex<numberOfInterfaces; iIndex++)
					if(FD_ISSET(interfaces[iIndex].fd, &set))
						break;
				Interface inf = interfaces[iIndex];

				//broadcast recieved
				ODRPacket packet;
				char addr[6];
				bzero(&packet, sizeof(packet));
				recieve_broadcast(interfaces[iIndex].fd, addr, &packet, sizeof(ODRPacket));
				printf("\n\n%s ODR recieved a packet from node at %s for %s\n", myIPAddress(), packet.from, packet.dest);
				
				if(packet.type == PacketTypeAPIReply || packet.type == PacketTypeAPISend) {
					printf("It was an API packet\n");
					//APP LEVEL PACKETS

					//add the return address
					if(packet.type == PacketTypeAPISend) {
						TableEntry *retEntry = entryForIp(&table, packet.from);
						TableEntry newEntry = tableEntryMake(packet.from, packet.hw_addr, inf.if_index, packet.numHops, 1);
						if(retEntry == NULL) {
							printf("Added return adrress to the table\n");
							addTableEntry(&table, newEntry);
							printTable(&table);
						}
						else if(retEntry->numHops > newEntry.numHops) {
							//add if better
							printf("updating better return adress in table\n");
							removeTableEntry(&table, packet.from);
							addTableEntry(&table, newEntry);
							printTable(&table);
						}
					}

					if(strcmp(myIPAddress(), packet.dest) == 0) {
						if(packet.type == PacketTypeAPISend) {
							printf("API packet made it to it's destination, delivering to the server!\n");
							sendToUnixDomainSocket(unixDomainSocketServer->fd, unixDomainSocketServer->sun_path, packet.message, packet.type, packet.rediscover, packet.from, packet.dest, packet.port);
						}
						else if(packet.type == PacketTypeAPIReply) {
							printf("Recieved reply, send to client!\n");
							sendToUnixDomainSocket(unixDomainSocketClient.fd, unixDomainSocketClient.sun_path, packet.message, packet.type, packet.rediscover, packet.from, packet.dest, packet.port);
						}
					}
					else {
						//not the destination -> forward to the next guy
						TableEntry *entry = entryForIp(&table, packet.dest);
						TableEntry *retEntry = entryForIp(&table, packet.from);
						if(entry != NULL) {
							//in our table
							printf("This is not for us, forward to the next hop\n");
							send_to_node(packet.dest, packet.from, packet.message, packet.type, packet.rediscover, packet.port, entry->if_index, entry->nextHopNode, retEntry->numHops+1);
						}
						else {
							//not in our table - give up - edge case - should never happen
						}
					}
				}
				else {
					//ODR PACKETS
					printf("It was an ODR level packet\n");

					if(strcmp(myIPAddress(), packet.from) == 0) {
						printf("Ignoring packet: I initiated the broadcast\n");
					}
					else {
						TableEntry *entry = entryForIp(&table, packet.dest);
						if(packet.type == PacketTypeRequest) {
							if(packet.broadcastId >= lastBroadcastIdHeard) {

								if(packet.broadcastId > lastBroadcastIdHeard && packet.rediscover == 1 && packet.type == PacketTypeRequest) {
									printf("rediscover flag set - emptying table\n");
									table.size = 0;
									entry = NULL;
								}
								lastBroadcastIdHeard = packet.broadcastId;
								if(lastBroadcastIdSent < lastBroadcastIdHeard+1)
									lastBroadcastIdSent = lastBroadcastIdHeard+1;

								//add the return address
								TableEntry *retEntry = entryForIp(&table, packet.from);
								TableEntry newEntry = tableEntryMake(packet.from, packet.hw_addr, inf.if_index, packet.numHops, 1);
								int shouldForward = 0;
								if(retEntry == NULL) {
									printf("Added return adrress to the table\n");
									addTableEntry(&table, newEntry);
									reqresp[packet.broadcastId] = ODRNodeMake(inf.if_index, packet.hw_addr);
									reqrespSize++;
									shouldForward = 1;
									printTable(&table);
								}
								else if(retEntry->numHops > newEntry.numHops) {
									//add if better
									printf("updating better return adress in table\n");
									removeTableEntry(&table, packet.from);
									addTableEntry(&table, newEntry);
									reqresp[packet.broadcastId] = ODRNodeMake(inf.if_index, packet.hw_addr);
									reqrespSize++;
									shouldForward = 1;
									printTable(&table);
								}
								else if(reqrespSize <= packet.broadcastId) {
									reqresp[packet.broadcastId] = ODRNodeMake(inf.if_index, packet.hw_addr);
									reqrespSize++;
									shouldForward = 1;
									printf("Recording reqresp node\n");
								}
								entry = entryForIp(&table, packet.dest);

								if(packet.rediscover == 1 || packet.rediscover == 0) {
									if(strcmp(myIPAddress(), packet.dest) == 0) {
										printf("RREQ reached it's destination: Sending RREP!\n");
										send_to_odr_node(packet.from, myIPAddress(), PacketTypeReply, packet.rediscover, packet.port, inf.if_index, packet.hw_addr, 1, packet.broadcastId);
									}
									else if(entry != NULL) {
										//- send back ODR RREP with numHops+1 and my HW adress with type ODR RREP
										printf("We have %s in our table, sending back the RREP\n", packet.dest);
										send_to_odr_node(packet.from, packet.dest, PacketTypeReply, 0, packet.port, inf.if_index, packet.hw_addr, entry->numHops+1, packet.broadcastId);
									}
									else if(shouldForward) {
										// printf("Should forward: %d\n", shouldForward);
										//we should only do this if we just found a better path or did not have on earlier
										//BROADCAST OUT A RREQ
										send_broadcast(packet.dest, packet.from, packet.message, packet.broadcastId, packet.type, packet.rediscover, packet.port, inf.if_index, packet.numHops+1);
									}
								}
							}
						}
						else if(packet.type == PacketTypeReply) {
							printf("ODR Recieved RREP\n");
							//ODR RREP
							TableEntry *serverEntry = entryForIp(&table, packet.from);
							TableEntry newEntry = tableEntryMake(packet.from, packet.hw_addr, inf.if_index, packet.numHops, 1);
							if(serverEntry != NULL && serverEntry->numHops > packet.numHops) {
								removeTableEntry(&table, serverEntry->ipAdrress);
								addTableEntry(&table, newEntry);
								printTable(&table);
								printf("replaced existing table entry\n");
							}
							else if(serverEntry == NULL) {
								addTableEntry(&table, newEntry);
								printf("added new entry to table\n");
								printTable(&table);
							}


							if(strcmp(myIPAddress(), packet.dest) == 0) {
								//send APP PACKET to first node
								// printf("attempting to send packetFromClient: %s\n", packetFromClient.dest);
								TableEntry *nextHop = entryForIp(&table, packetFromClient.dest);
								// printf("retrieved table entry\n");
								if(nextHop != NULL && packetFromClient.type != -5) {
									printf("Recieved RREQ, sending API packet to destination: %s\n", packetFromClient.dest);
									send_to_node(packetFromClient.dest, packetFromClient.from, packetFromClient.message, PacketTypeAPISend, packetFromClient.rediscover, packetFromClient.port, nextHop->if_index, nextHop->nextHopNode, 1);
									packetFromClient.type = -5;
									// printf("sent to the node\n");
								}
								// else
									// printf("WTF NEXT NODE WAS NULL\n");
							}
							else {
								ODRNode sender = reqresp[packet.broadcastId];
								send_to_odr_node(packet.dest, packet.from, packet.type, packet.rediscover, packet.port, sender.if_index, sender.hw_addr, packet.numHops+1, packet.broadcastId);
							}
						}
					}
				}
			}
		}
	}
}

//#pragma mark - Initialization

void startODR() {
	struct 	hwa_info *hwa, *hwahead;
	struct 	sockaddr *sa;
	char   	*ptr;
	int    	i, prflag;
	int 	index;

	// tableTest();

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
				printf("         HW addr = ");
				ptr = hwa->if_haddr;
				i = IF_HADDR;
				do {
					printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
				} while (--i > 0);
			}

			// printf("hw_addr: ");
			int x;
			for(x = 0; x < 6; x++) {
				interfaces[index].hw_addr[x] = hwa->if_haddr[x];
				// printf("%c\n", hwa->if_haddr[x]);
			}
			// printf("\n");

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

	unixDomainSocketODR = unixDomainSocketMake(UnixDomainSocketTypeODR, 1, NULL);
	unixDomainSocketServer = unixDomainSocketMake(UnixDomainSocketTypeServer, 0, NULL);
}

void send_to_odr_node(char* destIp, char *sourceIp, int type, int rediscover, int port, int if_index, char *hw_addr, int numHops, int broadcastId) {
	// printf("Sending to odr node from myip: %s!\n", myIPAddress());
	// printf("Sending to hw_addr: %s\n", hw_addr);

	ODRPacket packet;
	strcpy(packet.from, sourceIp);
	strcpy(packet.dest, destIp);
	packet.broadcastId = -1;
	packet.type = type;
	packet.rediscover = rediscover;
	packet.port = port;
	packet.numHops = numHops;
	packet.broadcastId = broadcastId;
	// int ret = -1;
	int i;
	for(i=0; i<numberOfInterfaces; i++) {
		if(interfaces[i].if_index == if_index) {
			int j;
			for(j=0; j<6; j++)
				packet.hw_addr[j] = interfaces[i].hw_addr[j];
			// ret = send_to_node_helper(interfaces[i].fd, if_index, &packet, sizeof(ODRPacket), hw_addr);
			send_to_node_helper(interfaces[i].fd, if_index, &packet, sizeof(ODRPacket), hw_addr);
		}
	}
	// printf("send_to_node ret: %d\n", ret);
}

void send_to_node(char* destIp, char * sourceIp, char *msg, int type, int rediscover, int port, int if_index, char *nextNodeHWAdress, int numHops) {
	// printf("Sending to node from myip: %s!\n", myIPAddress());
	printf("Forwarding to next hop, final destination: %s original source: %s\n", destIp, sourceIp);
	ODRPacket packet;
	strcpy(packet.from, sourceIp);
	strcpy(packet.dest, destIp);
	strcpy(packet.message, msg);
	packet.broadcastId = -1;
	packet.type = type;
	packet.rediscover = rediscover;
	packet.numHops = numHops;
	packet.port = port;
	int i,j;
	// int ret = -1;
	for(i=0; i<numberOfInterfaces; i++) {
		if(interfaces[i].if_index == if_index) {

			for(j=0; j<6; j++)
				packet.hw_addr[j] = interfaces[i].hw_addr[j];
			send_to_node_helper(interfaces[i].fd, if_index, &packet, sizeof(ODRPacket), nextNodeHWAdress);
			// ret = send_to_node_helper(interfaces[i].fd, if_index, &packet, sizeof(ODRPacket), nextNodeHWAdress);
		}
	}
	// printf("send_to_node ret: %d\n", ret);
}

void send_broadcast(char* destIp, char *fromIp, char *msg, int broadcastId, int type, int rediscover, int port, int incoming_if_index, int numHops) {
	printf("Sent broadcast from my ip: %s!\n", myIPAddress());
	ODRPacket packet;
	strcpy(packet.from, fromIp);
	strcpy(packet.dest, destIp);
	strcpy(packet.message, msg);
	packet.broadcastId = broadcastId;
	packet.type = type;
	packet.rediscover = rediscover;
	packet.port = port;
	packet.numHops = numHops;
	int i;
	for(i=0; i<numberOfInterfaces; i++) {
		if(incoming_if_index != interfaces[i].if_index) {
			int j;
			for(j=0; j<6; j++)
				packet.hw_addr[j] = interfaces[i].hw_addr[j];
			printf("Sending broadcast on if_index: %d\n", interfaces[i].if_index);
			send_broadcast_helper(interfaces[i].fd, interfaces[i].if_index, &packet, sizeof(ODRPacket));
		}
	}
}

int send_to_node_helper(int fd, int if_index, void *msg, int len, char *nextNodeHWAdress) {
	struct sockaddr_ll addr;
	bzero(&addr, sizeof(addr));
	
	addr.sll_halen = 6;
	addr.sll_hatype=1;
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(PROTO_NUM);
	addr.sll_ifindex = if_index;
	int i;
	for(i=0; i<6; i++)
		addr.sll_addr[i] = nextNodeHWAdress[i];
	for(i=6; i<8; i++)
		addr.sll_addr[i] = 0x00;
	
	int sentSize = sendto(fd, msg, len, 0, (struct sockaddr *)&addr, sizeof(addr));
	if(sentSize <= 0) printf("sendto() error: %d", errno);

	return sentSize;
}

int send_broadcast_helper(int fd, int if_index, void *msg, int len) {
	struct sockaddr_ll addr;
	bzero(&addr, sizeof(addr));
	
	addr.sll_halen = 6;
	addr.sll_hatype = 1;
	addr.sll_pkttype = PACKET_BROADCAST;
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(PROTO_NUM);
	addr.sll_ifindex = if_index;
	int i;
	for(i=0; i<6; i++) 
		addr.sll_addr[i] = 0xFF;
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
		printf("Critical Error\n");
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
