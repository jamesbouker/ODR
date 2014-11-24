#ifndef _JBOUKER_ODR_
#define _JBOUKER_ODR_

void 	startODR();
void 	send_to_node(char* destIp, char *msg, int type, int rediscover, int port, int if_index, char *nextNodeHWAdress);
int 	send_to_node_helper(int fd, int if_index, void *msg, int len, char *nextNodeHWAdress);
void	send_to_odr_node(char* destIp, int type, int rediscover, int port, int if_index, char *hw_addr, int numHops);
void 	send_broadcast(char* destIp, char *sourceIp, char *msg, int broadcastId, int type, int rediscover, int port, int incoming_if_index, int numHops);
int 	send_broadcast_helper(int fd, int if_index, void *msg, int len);
int 	recieve_broadcast(int fd, char *addr, void *msg, int len);
void 	listenSelectLoop();
char  *	myIPAddress();

#endif

