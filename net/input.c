#include "ns.h"
#include<inc/lib.h>
#include<inc/x86.h>

extern union Nsipc nsipcbuf;
void sleep(int);
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
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	int recv;
	memset(nsipcbuf.pkt.jp_data,0,256);
	while(1)
	{
		//cprintf("hello\n");
		//cprintf("in input.c jp_date=%x\n",nsipcbuf.pkt.jp_data);
					
		recv = sys_net_e1000_receive(nsipcbuf.pkt.jp_data);
		//hexdump("input.c: ", nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
		if(recv < 0)
		{
			sys_yield();	
			//cprintf("<0 loop");
			continue;
		}
		//memmove(nsipcbuf.pkt.jp_data,temp_buf,recv);
		
		nsipcbuf.pkt.jp_len = recv;
		//cprintf("length in input.c=%d\n",recv);
		ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P | PTE_U | PTE_W);
		
		sleep(20);
		//sys_yield();
	}
	//sleep(1);
	
}
void
sleep(int sec)
{
        unsigned now = sys_time_msec();
        unsigned end = now + sec * 1;

        if ((int)now < 0 && (int)now > -MAXERROR)
                panic("sys_time_msec: %e", (int)now);
        if (end < now)
                panic("sleep: wrap");

        while (sys_time_msec() < end)
                sys_yield();
}

