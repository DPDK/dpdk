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

#define OCTEONTX_VDEV_DEFAULT_MAX_NR_PORT	12
#define OCTEONTX_VDEV_NR_PORT_ARG		("nr_port")
#define OCTEONTX_MAX_NAME_LEN			32

#define OCTEONTX_MAX_BGX_PORTS			4
#define OCTEONTX_MAX_LMAC_PER_BGX		4

#define OCTEONTX_RX_OFFLOADS			DEV_RX_OFFLOAD_CRC_STRIP
#define OCTEONTX_TX_OFFLOADS			DEV_TX_OFFLOAD_MT_LOCKFREE

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
	uint8_t link_up;
	uint8_t	duplex;
	uint8_t speed;
	uint16_t mtu;
	uint8_t mac_addr[ETHER_ADDR_LEN];
	/* Rx port parameters */
	struct {
		bool classifier_enable;
		bool hash_enable;
		bool initialized;
	} pki;

	uint16_t ev_queues;
	uint16_t ev_ports;
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
} __rte_cache_aligned;

#endif /* __OCTEONTX_ETHDEV_H__ */
