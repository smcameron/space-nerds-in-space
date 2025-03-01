#include <stdint.h>
#include <string.h>

#include "net_utils.h"

static const struct network_address {
	unsigned char bytes;
	union address {
		unsigned char c[4];
		uint32_t in_addr4;
	} addr;
} private_network_addr[] = {
	{ .bytes = 1, .addr = { .c = {10, 255, 255, 255 }, } },
	{ .bytes = 2, .addr = { .c = {192, 168, 255, 255 }, } },
	{ .bytes = 2, .addr = { .c = {172, 16, 255, 255 }, } },
	{ .bytes = 2, .addr = { .c = {172, 16, 255, 255 }, } },
	{ .bytes = 0, .addr = { .c = {  0, 0, 0, 0 }, } },
};

/* Returns true if the address (in network byte order) is a private network */
int is_private_ip4_address(uint32_t address)
{
	unsigned char *x = (unsigned char *) &address;

	for (int i = 0; private_network_addr[i].bytes != 0; i++)
		if (memcmp(x, &private_network_addr[i].addr.c, private_network_addr[i].bytes) == 0)
			return 1;
	return 0;
}

