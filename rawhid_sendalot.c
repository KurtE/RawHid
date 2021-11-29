#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#if defined(OS_LINUX) || defined(OS_MACOSX)
#include <sys/ioctl.h>
#include <termios.h>
#elif defined(OS_WINDOWS)
#include <conio.h>
#endif

#include "hid.h"

#define MAX_PACKET_SIZE 512
#define BULK_PACKETS_PER_TRANSFER 32

#define FILE_SIZE 22400000ul
int packet_size;
int tx_attr;
uint32_t count_packets;
char buf[MAX_PACKET_SIZE*BULK_PACKETS_PER_TRANSFER];

int main()
{
	int r;

	// C-based example is 16C0:0480:FFAB:0200
	r = rawhid_open(1, 0x16C0, 0x0480, 0xFFAB, 0x0200);
	if (r <= 0) {
		// Arduino-based example is 16C0:0486:FFAB:0200
		r = rawhid_open(1, 0x16C0, 0x0486, 0xFFAB, 0x0200);
		if (r <= 0) {
			printf("no rawhid device found\n");
			return -1;
		}
	}
	printf("found rawhid device\n");

	printf("Starting output(%u)\n", count_packets);
	printf("Rx Size:%d Tx Size:%d\n", rawhid_rxSize(0), rawhid_txSize(0));
	tx_attr = rawhid_txAttr(0);
	packet_size = rawhid_txSize(0);
	if (packet_size <= 0) {
		printf("invalid size field");
		return -1;
	}
	count_packets = FILE_SIZE / packet_size;

	if (tx_attr == 2) {
		for (uint32_t bulk_pn_start = 0; bulk_pn_start < count_packets; bulk_pn_start+=BULK_PACKETS_PER_TRANSFER){
			char *pb = buf;
			for (uint32_t i=0; i <  BULK_PACKETS_PER_TRANSFER; i++) {
				uint32_t packet_num = bulk_pn_start + i;
				memset(pb, 'A' + (packet_num & 0xf), packet_size) ;
				snprintf(pb, sizeof(buf),"%07u", packet_num);
				pb[7] = (packet_num == (count_packets-1))? '$' : ' ';
				pb[packet_size-1] = '\n';
				pb += packet_size;
			}
			rawhid_send(0, buf, packet_size*BULK_PACKETS_PER_TRANSFER, 100*BULK_PACKETS_PER_TRANSFER);
			if ((bulk_pn_start & 0x1ff) == 0) printf(".");
			if ((bulk_pn_start & 0xffff) == 0) printf("\n");
		}

	} else {
		for (uint32_t packet_num = 0; packet_num < count_packets; packet_num++){
			memset(buf, 'A' + (packet_num & 0xf), packet_size) ;
			snprintf(buf, sizeof(buf),"%07u", packet_num);
			buf[7] = (packet_num == (count_packets-1))? '$' : ' ';
			buf[packet_size-1] = '\n';
			rawhid_send(0, buf, packet_size, 100);
			if ((packet_num & 0x1ff) == 0) printf(".");
			if ((packet_num & 0xffff) == 0) printf("\n");
		}
	}

	printf("\nDone...\n");
	return 0;
}
