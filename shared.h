#ifndef _JBOUKER_SHARED_
#define _JBOUKER_SHARED_

#include "unp.h"

#define PROTO_NUM	5578

typedef struct {
	char from[20];
	char dest[20];
	char message[12];
	int broadcastId;
	int type;
	int rediscover;
} ODRPacket;

typedef struct {
	int fd;
	int if_index;
	char hw_addr[6];
  	char ip_addr[MAXLINE];
  	char if_name[MAXLINE];

} Interface;

void printInterface(Interface inf);

#endif
