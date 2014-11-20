#ifndef _JBOUKER_ODR_
#define _JBOUKER_ODR_

void 	startODR();
// void listen();
void 	send_broadcast(char* destIp, char *msg, int broadcastId, int type, int rediscover);
int 	send_broadcast_helper(int fd, int if_index, void *msg, int len);
int 	recieve_broadcast(int fd, char *addr, void *msg, int len);
void 	listenSelectLoop();
char  *	myIPAddress();

#endif

