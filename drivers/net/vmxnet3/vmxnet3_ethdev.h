/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _VMXNET3_ETHDEV_H_
#define _VMXNET3_ETHDEV_H_

#ifdef RTE_LIBRTE_VMXNET3_DEBUG_DRIVER
#define VMXNET3_ASSERT(x) do {			   \
	if (!(x)) rte_panic("VMXNET3: %s\n", #x); \
} while(0)
#else
#define VMXNET3_ASSERT(x) do { (void)(x); } while (0)
#endif

#define VMXNET3_MAX_MAC_ADDRS 1

/* UPT feature to negotiate */
#define VMXNET3_F_RXCSUM      0x0001
#define VMXNET3_F_RSS         0x0002
#define VMXNET3_F_RXVLAN      0x0004
#define VMXNET3_F_LRO         0x0008

/* Hash Types supported by device */
#define VMXNET3_RSS_HASH_TYPE_NONE      0x0
#define VMXNET3_RSS_HASH_TYPE_IPV4      0x01
#define VMXNET3_RSS_HASH_TYPE_TCP_IPV4  0x02
#define VMXNET3_RSS_HASH_TYPE_IPV6      0x04
#define VMXNET3_RSS_HASH_TYPE_TCP_IPV6  0x08

#define VMXNET3_RSS_HASH_FUNC_NONE      0x0
#define VMXNET3_RSS_HASH_FUNC_TOEPLITZ  0x01

#define VMXNET3_RSS_MAX_KEY_SIZE        40
#define VMXNET3_RSS_MAX_IND_TABLE_SIZE  128

#define VMXNET3_RSS_OFFLOAD_ALL ( \
	ETH_RSS_IPV4 | \
	ETH_RSS_NONFRAG_IPV4_TCP | \
	ETH_RSS_IPV6 | \
	ETH_RSS_NONFRAG_IPV6_TCP)

/* RSS configuration structure - shared with device through GPA */
typedef
struct VMXNET3_RSSConf {
	uint16_t   hashType;
	uint16_t   hashFunc;
	uint16_t   hashKeySize;
	uint16_t   indTableSize;
	uint8_t    hashKey[VMXNET3_RSS_MAX_KEY_SIZE];
	/*
	 * indTable is only element that can be changed without
	 * device quiesce-reset-update-activation cycle
	 */
	uint8_t    indTable[VMXNET3_RSS_MAX_IND_TABLE_SIZE];
} VMXNET3_RSSConf;

typedef
struct vmxnet3_mf_table {
	void          *mfTableBase; /* Multicast addresses list */
	uint64_t      mfTablePA;    /* Physical address of the list */
	uint16_t      num_addrs;    /* number of multicast addrs */
} vmxnet3_mf_table_t;

struct vmxnet3_hw {

	uint8_t *hw_addr0;	/* BAR0: PT-Passthrough Regs    */
	uint8_t *hw_addr1;	/* BAR1: VD-Virtual Device Regs */
	/* BAR2: MSI-X Regs */
	/* BAR3: Port IO    */
	void *back;

	uint16_t device_id;
	uint16_t vendor_id;
	uint16_t subsystem_device_id;
	uint16_t subsystem_vendor_id;
	bool adapter_stopped;

	uint8_t perm_addr[ETHER_ADDR_LEN];
	uint8_t num_tx_queues;
	uint8_t num_rx_queues;
	uint8_t bufs_per_pkt;

	Vmxnet3_TxQueueDesc   *tqd_start;	/* start address of all tx queue desc */
	Vmxnet3_RxQueueDesc   *rqd_start;	/* start address of all rx queue desc */

	Vmxnet3_DriverShared  *shared;
	uint64_t              sharedPA;

	uint64_t              queueDescPA;
	uint16_t              queue_desc_len;

	VMXNET3_RSSConf		 *rss_conf;
	uint64_t			 rss_confPA;
	vmxnet3_mf_table_t   *mf_table;
	uint32_t	      shadow_vfta[VMXNET3_VFT_SIZE];
#define VMXNET3_VFT_TABLE_SIZE     (VMXNET3_VFT_SIZE * sizeof(uint32_t))
};

#define VMXNET3_GET_ADDR_LO(reg)   ((uint32_t)(reg))
#define VMXNET3_GET_ADDR_HI(reg)   ((uint32_t)(((uint64_t)(reg)) >> 32))

/* Config space read/writes */

#define VMXNET3_PCI_REG(reg) (*((volatile uint32_t *)(reg)))

static inline uint32_t vmxnet3_read_addr(volatile void *addr)
{
	return VMXNET3_PCI_REG(addr);
}

#define VMXNET3_PCI_REG_WRITE(reg, value) do { \
	VMXNET3_PCI_REG((reg)) = (value); \
} while(0)

#define VMXNET3_PCI_BAR0_REG_ADDR(hw, reg) \
	((volatile uint32_t *)((char *)(hw)->hw_addr0 + (reg)))
#define VMXNET3_READ_BAR0_REG(hw, reg) \
	vmxnet3_read_addr(VMXNET3_PCI_BAR0_REG_ADDR((hw), (reg)))
#define VMXNET3_WRITE_BAR0_REG(hw, reg, value) \
	VMXNET3_PCI_REG_WRITE(VMXNET3_PCI_BAR0_REG_ADDR((hw), (reg)), (value))

#define VMXNET3_PCI_BAR1_REG_ADDR(hw, reg) \
	((volatile uint32_t *)((char *)(hw)->hw_addr1 + (reg)))
#define VMXNET3_READ_BAR1_REG(hw, reg) \
	vmxnet3_read_addr(VMXNET3_PCI_BAR1_REG_ADDR((hw), (reg)))
#define VMXNET3_WRITE_BAR1_REG(hw, reg, value) \
	VMXNET3_PCI_REG_WRITE(VMXNET3_PCI_BAR1_REG_ADDR((hw), (reg)), (value))

/*
 * RX/TX function prototypes
 */

void vmxnet3_dev_clear_queues(struct rte_eth_dev *dev);

void vmxnet3_dev_rx_queue_release(void *rxq);
void vmxnet3_dev_tx_queue_release(void *txq);

int  vmxnet3_dev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);
int  vmxnet3_dev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		uint16_t nb_tx_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

int vmxnet3_dev_rxtx_init(struct rte_eth_dev *dev);

int vmxnet3_rss_configure(struct rte_eth_dev *dev);

uint16_t vmxnet3_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);
uint16_t vmxnet3_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

#endif /* _VMXNET3_ETHDEV_H_ */
