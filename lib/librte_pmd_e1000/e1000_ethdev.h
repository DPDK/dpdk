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

#ifndef _E1000_ETHDEV_H_
#define _E1000_ETHDEV_H_

/* need update link, bit flag */
#define E1000_FLAG_NEED_LINK_UPDATE (uint32_t)(1 << 0)
#define E1000_FLAG_MAILBOX          (uint32_t)(1 << 1)

/*
 * Defines that were not part of e1000_hw.h as they are not used by the FreeBSD
 * driver.
 */
#define E1000_ADVTXD_POPTS_TXSM     0x00000200 /* L4 Checksum offload request */
#define E1000_ADVTXD_POPTS_IXSM     0x00000100 /* IP Checksum offload request */
#define E1000_ADVTXD_TUCMD_L4T_RSV  0x00001800 /* L4 Packet TYPE of Reserved */
#define E1000_RXD_STAT_TMST         0x10000    /* Timestamped Packet indication */
#define E1000_RXD_ERR_CKSUM_BIT     29
#define E1000_RXD_ERR_CKSUM_MSK     3
#define E1000_ADVTXD_MACLEN_SHIFT   9          /* Bit shift for l2_len */
#define E1000_CTRL_EXT_EXTEND_VLAN  (1<<26)    /* EXTENDED VLAN */
#define IGB_VFTA_SIZE 128

/* structure for interrupt relative data */
struct e1000_interrupt {
	uint32_t flags;
	uint32_t mask;
};

/* local vfta copy */
struct e1000_vfta {
	uint32_t vfta[IGB_VFTA_SIZE];
};

/*
 * VF data which used by PF host only
 */
#define E1000_MAX_VF_MC_ENTRIES         30
struct e1000_vf_info {
	uint8_t vf_mac_addresses[ETHER_ADDR_LEN];
	uint16_t vf_mc_hashes[E1000_MAX_VF_MC_ENTRIES];
	uint16_t num_vf_mc_hashes;
	uint16_t default_vf_vlan_id;
	uint16_t vlans_enabled;
	uint16_t pf_qos;
	uint16_t vlan_count;
	uint16_t tx_rate;
};

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct e1000_adapter {
	struct e1000_hw         hw;
	struct e1000_hw_stats   stats;
	struct e1000_interrupt  intr;
	struct e1000_vfta       shadow_vfta;
	struct e1000_vf_info    *vfdata;
};

#define E1000_DEV_PRIVATE_TO_HW(adapter) \
	(&((struct e1000_adapter *)adapter)->hw)

#define E1000_DEV_PRIVATE_TO_STATS(adapter) \
	(&((struct e1000_adapter *)adapter)->stats)

#define E1000_DEV_PRIVATE_TO_INTR(adapter) \
	(&((struct e1000_adapter *)adapter)->intr)

#define E1000_DEV_PRIVATE_TO_VFTA(adapter) \
	(&((struct e1000_adapter *)adapter)->shadow_vfta)

#define E1000_DEV_PRIVATE_TO_P_VFDATA(adapter) \
        (&((struct e1000_adapter *)adapter)->vfdata)

/*
 * RX/TX IGB function prototypes
 */
void eth_igb_tx_queue_release(void *txq);
void eth_igb_rx_queue_release(void *rxq);
void igb_dev_clear_queues(struct rte_eth_dev *dev);

int eth_igb_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);

uint32_t eth_igb_rx_queue_count(struct rte_eth_dev *dev, 
		uint16_t rx_queue_id);

int eth_igb_rx_descriptor_done(void *rx_queue, uint16_t offset);

int eth_igb_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		uint16_t nb_tx_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

int eth_igb_rx_init(struct rte_eth_dev *dev);

void eth_igb_tx_init(struct rte_eth_dev *dev);

uint16_t eth_igb_xmit_pkts(void *txq, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

uint16_t eth_igb_recv_pkts(void *rxq, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

uint16_t eth_igb_recv_scattered_pkts(void *rxq,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

int eth_igbvf_rx_init(struct rte_eth_dev *dev);

void eth_igbvf_tx_init(struct rte_eth_dev *dev);

/*
 * misc function prototypes
 */
void igb_pf_host_init(struct rte_eth_dev *eth_dev);
 
void igb_pf_mbx_process(struct rte_eth_dev *eth_dev);
 
int igb_pf_host_configure(struct rte_eth_dev *eth_dev);

/*
 * RX/TX EM function prototypes
 */
void eth_em_tx_queue_release(void *txq);
void eth_em_rx_queue_release(void *rxq);

void em_dev_clear_queues(struct rte_eth_dev *dev);

int eth_em_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);

uint32_t eth_em_rx_queue_count(struct rte_eth_dev *dev, 
		uint16_t rx_queue_id);

int eth_em_rx_descriptor_done(void *rx_queue, uint16_t offset);

int eth_em_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		uint16_t nb_tx_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

int eth_em_rx_init(struct rte_eth_dev *dev);

void eth_em_tx_init(struct rte_eth_dev *dev);

uint16_t eth_em_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

uint16_t eth_em_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

uint16_t eth_em_recv_scattered_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

#endif /* _E1000_ETHDEV_H_ */
