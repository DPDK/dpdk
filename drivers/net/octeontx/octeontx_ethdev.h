/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium Inc. 2017. All rights reserved.
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
 *     * Neither the name of Cavium networks nor the names of its
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
#ifndef	__OCTEONTX_ETHDEV_H__
#define	__OCTEONTX_ETHDEV_H__

#include <stdbool.h>

#include <rte_common.h>
#include <rte_ethdev.h>
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

static inline struct octeontx_nic *
octeontx_pmd_priv(struct rte_eth_dev *dev)
{
	return dev->data->dev_private;
}

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
