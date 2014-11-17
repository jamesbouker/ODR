#include "odr.h"
#include "prhwaddrs.h"
#include "shared.h"
#include <stdio.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>

int main(int argc, char **argv) {
	printf("ODR running\n");
	initOdr();
	return 0;
}

void listenLoop() {

}

void initOdr() {
 	struct hwa_info	*hwa, *hwahead;
	struct sockaddr	*sa;
	char   *ptr;
	int    i, prflag;

	printf("\n");

	for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {
		
		printf("%s :%s", hwa->if_name, ((hwa->ip_alias) == IP_ALIAS) ? " (alias)\n" : "\n");
		if(strcmp(hwa->if_name, "lo") != 0 && strcmp(hwa->if_name, "eth0"))
		if ( (sa = hwa->ip_addr) != NULL)
			printf("         IP addr = %s\n", sock_ntop_host(sa, sizeof(*sa)));
				
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

		printf("\n         interface index = %d\n\n", hwa->if_index);

		struct sockaddr_ll addr;
  		short proto = PROTO_NUM;
  		int fd = socket(AF_PACKET,SOCK_DGRAM,htons(proto));
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
	}

	free_hwa_info(hwahead);
}
