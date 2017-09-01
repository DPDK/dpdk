/*-
 *   BSD LICENSE
 *
 *   Copyright 2012 6WIND S.A.
 *   Copyright 2012 Mellanox
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#ifndef RTE_PMD_MLX4_H_
#define RTE_PMD_MLX4_H_

#include <stdint.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_interrupts.h>

/* Request send completion once in every 64 sends, might be less. */
#define MLX4_PMD_TX_PER_COMP_REQ 64

/* Maximum size for inline data. */
#define MLX4_PMD_MAX_INLINE 0

/*
 * Maximum number of cached Memory Pools (MPs) per TX queue. Each RTE MP
 * from which buffers are to be transmitted will have to be mapped by this
 * driver to their own Memory Region (MR). This is a slow operation.
 *
 * This value is always 1 for RX queues.
 */
#ifndef MLX4_PMD_TX_MP_CACHE
#define MLX4_PMD_TX_MP_CACHE 8
#endif

/* Interrupt alarm timeout value in microseconds. */
#define MLX4_INTR_ALARM_TIMEOUT 100000

/* Port parameter. */
#define MLX4_PMD_PORT_KVARG "port"

enum {
	PCI_VENDOR_ID_MELLANOX = 0x15b3,
};

enum {
	PCI_DEVICE_ID_MELLANOX_CONNECTX3 = 0x1003,
	PCI_DEVICE_ID_MELLANOX_CONNECTX3VF = 0x1004,
	PCI_DEVICE_ID_MELLANOX_CONNECTX3PRO = 0x1007,
};

#define MLX4_DRIVER_NAME "net_mlx4"

struct rxq;
struct txq;
struct rte_flow;

struct priv {
	struct rte_eth_dev *dev; /* Ethernet device. */
	struct ibv_context *ctx; /* Verbs context. */
	struct ibv_device_attr device_attr; /* Device properties. */
	struct ibv_pd *pd; /* Protection Domain. */
	struct ether_addr mac; /* MAC address. */
	struct ibv_flow *mac_flow; /* Flow associated with MAC address. */
	/* Device properties. */
	uint16_t mtu; /* Configured MTU. */
	uint8_t port; /* Physical port number. */
	unsigned int started:1; /* Device started, flows enabled. */
	unsigned int vf:1; /* This is a VF device. */
	unsigned int intr_alarm:1; /* An interrupt alarm is scheduled. */
	unsigned int isolated:1; /* Toggle isolated mode. */
	/* RX/TX queues. */
	unsigned int rxqs_n; /* RX queues array size. */
	unsigned int txqs_n; /* TX queues array size. */
	struct rxq *(*rxqs)[]; /* RX queues. */
	struct txq *(*txqs)[]; /* TX queues. */
	struct rte_intr_handle intr_handle; /* Port interrupt handle. */
	struct rte_flow_drop *flow_drop_queue; /* Flow drop queue. */
	LIST_HEAD(mlx4_flows, rte_flow) flows;
};

/* mlx4.c */

int mlx4_link_update(struct rte_eth_dev *dev, int wait_to_complete);

/* mlx4_intr.c */

int mlx4_intr_uninstall(struct priv *priv);
int mlx4_intr_install(struct priv *priv);
int mlx4_rx_intr_disable(struct rte_eth_dev *dev, uint16_t idx);
int mlx4_rx_intr_enable(struct rte_eth_dev *dev, uint16_t idx);

#endif /* RTE_PMD_MLX4_H_ */
