/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef	__OCTEONTX_ETHDEV_H__
#define	__OCTEONTX_ETHDEV_H__

#include <stdbool.h>

#include <rte_common.h>
#include <rte_ethdev_driver.h>
#include <rte_eventdev.h>
#include <rte_mempool.h>
#include <rte_memory.h>

#include <octeontx_fpavf.h>

#include "base/octeontx_bgx.h"
#include "base/octeontx_pki_var.h"
#include "base/octeontx_pkivf.h"
#include "base/octeontx_pkovf.h"
#include "base/octeontx_io.h"

#define OCTEONTX_PMD				net_octeontx
#define OCTEONTX_VDEV_DEFAULT_MAX_NR_PORT	12
#define OCTEONTX_VDEV_NR_PORT_ARG		("nr_port")
#define OCTEONTX_MAX_NAME_LEN			32

#define OCTEONTX_MAX_BGX_PORTS			4
#define OCTEONTX_MAX_LMAC_PER_BGX		4

#define OCCTX_RX_NB_SEG_MAX			6
#define OCCTX_INTR_POLL_INTERVAL_MS		1000
/* VLAN tag inserted by OCCTX_TX_VTAG_ACTION.
 * In Tx space is always reserved for this in FRS.
 */
#define OCCTX_MAX_VTAG_INS		2
#define OCCTX_MAX_VTAG_ACT_SIZE		(4 * OCCTX_MAX_VTAG_INS)

/* HW config of frame size doesn't include FCS */
#define OCCTX_MAX_HW_FRS		9212
#define OCCTX_MIN_HW_FRS		60

/* ETH_HLEN+ETH_FCS+2*VLAN_HLEN */
#define OCCTX_L2_OVERHEAD	(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + \
				 OCCTX_MAX_VTAG_ACT_SIZE)

/* Since HW FRS includes NPC VTAG insertion space, user has reduced FRS */
#define OCCTX_MAX_FRS	\
	(OCCTX_MAX_HW_FRS + RTE_ETHER_CRC_LEN - OCCTX_MAX_VTAG_ACT_SIZE)

#define OCCTX_MIN_FRS		(OCCTX_MIN_HW_FRS + RTE_ETHER_CRC_LEN)

#define OCCTX_MAX_MTU		(OCCTX_MAX_FRS - OCCTX_L2_OVERHEAD)

#define OCTEONTX_RX_OFFLOADS		(				   \
					 DEV_RX_OFFLOAD_CHECKSUM	 | \
					 DEV_RX_OFFLOAD_SCTP_CKSUM       | \
					 DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM | \
					 DEV_RX_OFFLOAD_SCATTER	         | \
					 DEV_RX_OFFLOAD_SCATTER		 | \
					 DEV_RX_OFFLOAD_JUMBO_FRAME	 | \
					 DEV_RX_OFFLOAD_VLAN_FILTER)

#define OCTEONTX_TX_OFFLOADS		(				   \
					 DEV_TX_OFFLOAD_MBUF_FAST_FREE	 | \
					 DEV_TX_OFFLOAD_MT_LOCKFREE	 | \
					 DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM | \
					 DEV_TX_OFFLOAD_OUTER_UDP_CKSUM	 | \
					 DEV_TX_OFFLOAD_IPV4_CKSUM	 | \
					 DEV_TX_OFFLOAD_TCP_CKSUM	 | \
					 DEV_TX_OFFLOAD_UDP_CKSUM	 | \
					 DEV_TX_OFFLOAD_SCTP_CKSUM	 | \
					 DEV_TX_OFFLOAD_MULTI_SEGS)

static inline struct octeontx_nic *
octeontx_pmd_priv(struct rte_eth_dev *dev)
{
	return dev->data->dev_private;
}

extern uint16_t
rte_octeontx_pchan_map[OCTEONTX_MAX_BGX_PORTS][OCTEONTX_MAX_LMAC_PER_BGX];

struct vlan_entry {
	TAILQ_ENTRY(vlan_entry) next;
	uint16_t vlan_id;
};

TAILQ_HEAD(octeontx_vlan_filter_tbl, vlan_entry);

struct octeontx_vlan_info {
	struct octeontx_vlan_filter_tbl fltr_tbl;
	uint8_t filter_on;
};

struct octeontx_fc_info {
	enum rte_eth_fc_mode mode;  /**< Link flow control mode */
	enum rte_eth_fc_mode def_mode;
	uint16_t high_water;
	uint16_t low_water;
	uint16_t def_highmark;
	uint16_t def_lowmark;
	uint32_t rx_fifosz;
};

/* Octeontx ethdev nic */
struct octeontx_nic {
	struct rte_eth_dev *dev;
	int node;
	int port_id;
	int port_ena;
	int base_ichan;
	int num_ichans;
	int base_ochan;
	int num_ochans;
	uint8_t evdev;
	uint8_t bpen;
	uint8_t fcs_strip;
	uint8_t bcast_mode;
	uint8_t mcast_mode;
	uint16_t num_tx_queues;
	uint64_t hwcap;
	uint8_t pko_vfid;
	uint8_t link_up;
	uint8_t	duplex;
	uint8_t speed;
	uint16_t bgx_mtu;
	uint16_t mtu;
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
	/* Rx port parameters */
	struct {
		bool classifier_enable;
		bool hash_enable;
		bool initialized;
	} pki;

	uint16_t ev_queues;
	uint16_t ev_ports;
	uint64_t rx_offloads;
	uint16_t rx_offload_flags;
	uint64_t tx_offloads;
	uint16_t tx_offload_flags;
	struct octeontx_vlan_info vlan_info;
	int print_flag;
	struct octeontx_fc_info fc;
} __rte_cache_aligned;

struct octeontx_txq {
	uint16_t queue_id;
	octeontx_dq_t dq;
	struct rte_eth_dev *eth_dev;
} __rte_cache_aligned;

struct octeontx_rxq {
	uint16_t queue_id;
	uint16_t port_id;
	uint8_t evdev;
	struct rte_eth_dev *eth_dev;
	uint16_t ev_queues;
	uint16_t ev_ports;
	struct rte_mempool *pool;
} __rte_cache_aligned;

void
octeontx_set_tx_function(struct rte_eth_dev *dev);

/* VLAN */
int octeontx_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t qidx);
int octeontx_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t qidx);
int octeontx_dev_vlan_offload_init(struct rte_eth_dev *dev);
int octeontx_dev_vlan_offload_fini(struct rte_eth_dev *eth_dev);
int octeontx_dev_vlan_offload_set(struct rte_eth_dev *dev, int mask);
int octeontx_dev_vlan_filter_set(struct rte_eth_dev *dev,
				 uint16_t vlan_id, int on);
int octeontx_dev_set_link_up(struct rte_eth_dev *eth_dev);
int octeontx_dev_set_link_down(struct rte_eth_dev *eth_dev);

/* Flow control */
int octeontx_dev_flow_ctrl_init(struct rte_eth_dev *dev);
int octeontx_dev_flow_ctrl_fini(struct rte_eth_dev *dev);
int octeontx_dev_flow_ctrl_get(struct rte_eth_dev *dev,
			       struct rte_eth_fc_conf *fc_conf);
int octeontx_dev_flow_ctrl_set(struct rte_eth_dev *dev,
			       struct rte_eth_fc_conf *fc_conf);

#endif /* __OCTEONTX_ETHDEV_H__ */
