#include "shared.h"
#include <stdio.h>

void printInterface(Interface inf) {
	printf("Interface: %s\n", inf.if_name);
	printf("FD: %d\n", inf.fd);
	printf("if_index: %d\n", inf.if_index);
	printf("ip_addr: %s\n", inf.ip_addr);
}