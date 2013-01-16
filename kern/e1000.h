#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

// PCI config related data
#define e1000_vendor_id 0x8086
#define e1000_dev_id 0x100e

#define BUFFER_SIZE 64
#define RX_BUFFER_SIZE 64
#define PACKET_SIZE 1518
#define RX_PACKET_SIZE 2048

// Tx Desc Registers
#define e1000_tctl 0x00400
#define e1000_tipg 0x00410
#define e1000_tdbal 0x03800
#define e1000_tdbah 0x03804
#define e1000_tdlen 0x03808
#define e1000_tdh 0x03810
#define e1000_tdt 0x03818

//Rx Desc Registers
#define e1000_rctl 0x00100
#define e1000_rdbal 0x02800
#define e1000_rdbah 0x02804
#define e1000_rdlen 0x02808
#define e1000_rdh 0x02810
#define e1000_rdt 0x02818
#define e1000_mta 0x5200

// Transmit control
#define e1000_tctl_en 0x00000002
#define e1000_tctl_psp 0x00000008
#define e1000_tctl_ct 0x00000ff0
#define e1000_tctl_cold 0x1000000

#define e1000_tx_cmd_rs 0x00000008
#define e1000_tx_cmd_eop 0x00000001

#define e1000_tx_stat_dd 0x00000001

// Receive Control
#define e1000_rctl_en 0x00000002
#define e1000_rctl_lpe 0x00000020
#define e1000_rctl_bam 0x00008000
#define e1000_rctl_bsize 0x00030000
#define e1000_rctl_crc 0x04000000
#define e1000_rctl_mo 0x00000000
#define e1000_rctl_upe 0x00000008
#define e1000_rctl_rdmts_quad 0x00000100
#define e1000_rctl_lbm_no 0x00000000

#define e1000_rx_stat_dd 0x00000001
#define e1000_rx_stat_eop 0x00000002

#define e1000_rx_ral 0x05400
#define e1000_rx_rah 0x05404
#define e1000_rx_rah_av 0x80000000

// Tx Desc structure
struct tx_desc
{
	uint64_t buf_addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t css;
	uint8_t status;
	uint16_t special;
};

int e1000_attach(struct pci_func *);
void e1000_initialize();
int e1000_transmit(char *, uint32_t);
static void hexdump(const char *prefix, const void *data, int len);
int e1000_receive(char *);
struct rx_desc
{
	uint64_t buf_addr;
	uint16_t length;
	uint16_t checksum;
	uint8_t status;
	uint8_t errors;
	uint16_t special;
};
#endif	// JOS_KERN_E1000_H
