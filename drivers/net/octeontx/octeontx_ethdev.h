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

#define OCTEONTX_RX_OFFLOADS		(DEV_RX_OFFLOAD_CHECKSUM     | \
					 DEV_RX_OFFLOAD_SCATTER	     | \
					 DEV_RX_OFFLOAD_JUMBO_FRAME)

#define OCTEONTX_TX_OFFLOADS		(DEV_TX_OFFLOAD_MT_LOCKFREE    |  \
					 DEV_TX_OFFLOAD_MBUF_FAST_FREE |  \
					 DEV_TX_OFFLOAD_MULTI_SEGS)

static inline struct octeontx_nic *
octeontx_pmd_priv(struct rte_eth_dev *dev)
{
	return dev->data->dev_private;
}

extern uint16_t
rte_octeontx_pchan_map[OCTEONTX_MAX_BGX_PORTS][OCTEONTX_MAX_LMAC_PER_BGX];

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
#endif /* __OCTEONTX_ETHDEV_H__ */
