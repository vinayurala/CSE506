#include <kern/e1000.h>
#include <kern/pci.h>
#include <kern/pmap.h>
#include <inc/types.h>
#include <inc/string.h>
#include <inc/error.h>

// LAB 6: Your driver code here

volatile uint32_t *e1000_mmio_addr;
struct tx_desc tx_desc_list[BUFFER_SIZE];
struct rx_desc rx_desc_list[RX_BUFFER_SIZE];
char packet_buf[BUFFER_SIZE][PACKET_SIZE];
char rx_packet_buf[BUFFER_SIZE][RX_PACKET_SIZE];

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


int e1000_attach(struct pci_func *pcif)
{
	uint32_t dev_stat_reg;
	
	pci_func_enable(pcif);
	e1000_mmio_addr = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
	// check if we're mappped correctly
	dev_stat_reg = *(e1000_mmio_addr + 2);
	assert(dev_stat_reg == 0x80080783);

	e1000_initialize();	
	// Remove the set of statements after finishing 8th question, before running, make grade
	/*char test_pkt_buf[10][6] = {"Test1",
				   "Test2", 
				   "Test3", 
				   "Test4", 
				    "Test5",
				    "Test6", 
				    "test7",
				    "test8",
				    "test9",
				    "test0"};
	int i;
	for(i = 0; i < 10; i++)
	{
		while(1)
		{
			if(!(e1000_transmit(test_pkt_buf[i], strlen(test_pkt_buf[i]))))
				break;
		}
	}
	assert(0);   */
	return 1;
}

void e1000_initialize()
{

	//Set the register values
	*(e1000_mmio_addr + e1000_tdbal/4) = PADDR(tx_desc_list);
	// Not initializing TDBAH because 64-bit JOS address space is as good as 32-bit address space

	*(e1000_mmio_addr + e1000_tdlen/4) = sizeof(tx_desc_list);
	*(e1000_mmio_addr + e1000_tdh/4) = 0x0;
	*(e1000_mmio_addr + e1000_tdt/4) = 0x0;

	*(e1000_mmio_addr + e1000_tctl/4) = e1000_tctl_en | e1000_tctl_psp | e1000_tctl_ct | e1000_tctl_cold;

	// Using 8ns for IPGT, IPGR1 and IPGR2
	// Getting an invalid MMIO write at this position, not sure why
	*(e1000_mmio_addr + e1000_tipg/4) = 0x00802008;

	int i;
	for(i = 0; i < BUFFER_SIZE; i++)
	{	
		tx_desc_list[i].status |= e1000_tx_stat_dd;
		tx_desc_list[i].buf_addr = PADDR(packet_buf[i]);
	}

	//Receive Initialize
	// *(e1000_mmio_addr + e1000_mta/4) = 0;
        // *(e1000_mmio_addr + (e1000_mta/4) +1) = 0;

	for(i = 0;i < RX_BUFFER_SIZE; i++)
	 {
		 rx_desc_list[i].buf_addr = PADDR(rx_packet_buf[i]);
		 //rx_desc_list[i].status |= 0x4 | e1000_rx_stat_dd | e1000_rx_stat_eop;
	 }

	 *(e1000_mmio_addr + e1000_rx_ral/4) = 0x12005452;
	 *(e1000_mmio_addr + e1000_rx_rah/4) = 0x5634 | e1000_rx_rah_av;
	 *(e1000_mmio_addr + e1000_mta/4) = 0;

	 *(e1000_mmio_addr + e1000_rdbal/4) = PADDR(rx_desc_list);
	 *(e1000_mmio_addr + e1000_rdlen/4) = sizeof(rx_desc_list);
	 *(e1000_mmio_addr + e1000_rdh/4) = 0x0;
	 *(e1000_mmio_addr + e1000_rdt/4) = 0x0;

	 *(e1000_mmio_addr + e1000_rctl/4) = e1000_rctl_en | e1000_rctl_bam | e1000_rctl_crc;

	 //cprintf("MMIO Addr = %x\n", e1000_mmio_addr);
	 //cprintf("\nRAL = %x\n", e1000_mmio_addr + e1000_rx_ral/4);
	 //cprintf("\nRAH = %x\n", e1000_mmio_addr + e1000_rx_rah/4);
	 //cprintf("\nRDBAL = %x\n",*(e1000_mmio_addr + e1000_rdbal/4));

	// cprintf("\nTDBAL = %x\n", *(e1000_mmio_addr + e1000_tdbal/4));
	//cprintf("\nRDLEN = %d\n", *(e1000_mmio_addr + e1000_rdlen/4)/8);
	// cprintf("\nEthernet card initialized!\n");
	return;
}

int e1000_transmit(char *pkt_buf, uint32_t pkt_length)
{

	uint32_t tdt, next_tdt;
	//cprintf("pkt_length=%d\n",pkt_length);
	tdt = *(e1000_mmio_addr + e1000_tdt/4);
	next_tdt = (tdt + 1)%BUFFER_SIZE;

	int status = tx_desc_list[tdt].status & e1000_tx_stat_dd;

	if(!status)
		return -E_TX_RING_FULL;

	int i;
	/*for(i=0;i<pkt_length;i++)
	{
		cprintf("transmit pkt=%s\n",pkt_buf[i]);	
	}*/
	memmove(packet_buf[tdt], pkt_buf, pkt_length);
	tx_desc_list[tdt].length = pkt_length;

	tx_desc_list[tdt].cmd |= e1000_tx_cmd_rs;
	tx_desc_list[tdt].cmd |= e1000_tx_cmd_eop;
	//tx_desc_list[tdt].status &= ~e1000_tx_stat_dd;
	*(e1000_mmio_addr + e1000_tdt/4) = next_tdt;

	return 0;
}
int e1000_receive(char *pkt_buf)
{
	uint32_t rdt, next_rdt;
	rdt = *(e1000_mmio_addr + e1000_rdt/4);
	next_rdt = (rdt+1)%RX_BUFFER_SIZE;
	int status = rx_desc_list[rdt].status & e1000_rx_stat_dd;

//	cprintf("\nstatus = %d\n", status);
	if(status != 0x1)
		return -E_NO_PCKT;
	int len = rx_desc_list[rdt].length;
	//cprintf("length :%d", rdt);
	//cprintf("receive packet buf=%x\n",rx_packet_buf[rdt]);
	int i;
	/*for(i=0;i<len;i++){
		pkt_buf[i];
		rx_packet_buf[rdt][i];
		cprintf("the I %d\n",i);
	}*/
	//cprintf("\nRDT = %d\n", rdt);
	memmove(pkt_buf,rx_packet_buf[rdt],len);
	rx_desc_list[rdt].status = 0x0;
	//cprintf("in e1000.c after setting status in recv\n");
	 *(e1000_mmio_addr + e1000_rdt/4) = next_rdt;
	return len; 
}

