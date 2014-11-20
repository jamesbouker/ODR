#ifndef _JBOUKER_SHARED_
#define _JBOUKER_SHARED_

#include "unp.h"
#include <stdio.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <unistd.h>

#define PROTO_NUM	5578

//where is the propper location to store these?
///tmp/unix.dg?
#define SERVER_SUN_PATH 	("/tmp/1895_server_sun_path.dg")
#define ODR_SUN_PATH 		("/tmp/1895_odr_sun_path.dg")
#define CLIENT_SUN_PATH 	("/tmp/-XXXXXX")

//Packet Structures

//TODO: Struct sizes! In class Professor went over header formats. 
//We are not following any of it...

typedef enum {
	PacketTypeRequest = 0,
	PacketTypeReply
} PacketType;

typedef struct {
	char from[20];
	char dest[20];
	char message[300];
	int broadcastId;
	int rediscover;
	PacketType type;
} ODRPacket;

typedef struct {
	char from[20];
	char dest[20];
	char message[300];
	int rediscover;
	PacketType type;
} UnixDomainPacket;

//Interaces

typedef struct {
	int fd;
	int if_index;
	char hw_addr[6];
  	char ip_addr[MAXLINE];
  	char if_name[MAXLINE];

} Interface;

Interface getEth0();
void printInterface(Interface inf);

//Unix Domain Sockets

typedef enum {
	UnixDomainSocketTypeClient = 0,
	UnixDomainSocketTypeServer,
	UnixDomainSocketTypeODR	
} UnixDomainSocketType;

typedef struct {
  	char sun_path[MAXLINE];
  	UnixDomainSocketType type;
  	int fd;
} UnixDomainSocket;

//returns a UnixDomainSocket ptr, caller is responsible for freeing! 
UnixDomainSocket * unixDomainSocketMake(UnixDomainSocketType type, int shouldBind, char *sun_path);
void unixDomainSocketUnlink(UnixDomainSocket *socket);
void sendToUnixDomainSocket(int fd, char *sun_path, char *message, PacketType type, int rediscover, char *fromIp, char *toIp);
void readFromUnixDomainSocket(int fd, char *sun_path, UnixDomainPacket *packet);

//API

int msg_send(int fd, char *destAddr, int destPort, char *message, int rediscover);
int msg_recv(int fd, char *message, char *fromAddr, int *fromPort);

#endif
