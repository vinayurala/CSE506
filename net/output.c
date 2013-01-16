#include "ns.h"
#include <inc/lib.h>
#include <inc/x86.h>

extern union Nsipc nsipcbuf;

static void
hexdump(const char *prefix, const void *data, int len)
{
        int i;
        char buf[80];
        char *end = buf + sizeof(buf);
        char *out = NULL;
        for (i = 0; i < len; i++) {
                if (i % 16 == 0)
                        out = buf + snprintf(buf, end - buf,
                                             "%s%04x   ", prefix, i);
                out += snprintf(out, end - out, "%02x", ((uint8_t*)data)[i]);
                if (i % 16 == 15 || i == len - 1)
                        cprintf("%.*s\n", out - buf, buf);
                if (i % 2 == 1)
                        *(out++) = ' ';
                if (i % 16 == 7)
                        *(out++) = ' ';
        }
}

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	envid_t target_envid;
	int r, perm_store;

	while(1)
	{
		if((r = ipc_recv(NULL, &nsipcbuf, NULL)) < 0)
		{
			cprintf("\nIPC receive failure\n");
			return;
		}
		hexdump("output.c: ", nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
		while((r = sys_net_e1000_transmit(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len)) < 0)
			sys_yield();
	}
	return;
}
