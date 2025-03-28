/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Stephen Hemminger <stephen@networkplumber.org>
 */

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <rte_config.h>
#include <rte_bitops.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_spinlock.h>
#include <rte_stdatomic.h>
#include <eal_export.h>

#include "rte_ethdev.h"
#include "rte_mirror.h"
#include "ethdev_driver.h"
#include "ethdev_private.h"
#include "ethdev_trace.h"

/* Upper bound of packet bursts redirected */
#define RTE_MIRROR_BURST_SIZE 64

/* spinlock for setting up mirror ports */
static rte_spinlock_t mirror_port_lock = RTE_SPINLOCK_INITIALIZER;

/* dynamically assigned offload flag to indicate ingress vs egress */
static uint64_t mirror_origin_flag;
static uint64_t mirror_ingress_flag;
static uint64_t mirror_egress_flag;
static int mirror_origin_offset = -1;

/* register dynamic mbuf fields, done on first mirror creation */
static int
ethdev_dyn_mirror_register(void)
{
	const struct rte_mbuf_dynfield field_desc = {
		.name = RTE_MBUF_DYNFIELD_MIRROR_ORIGIN,
		.size = sizeof(rte_mbuf_origin_t),
		.align = sizeof(rte_mbuf_origin_t),
	};
	struct rte_mbuf_dynflag flag_desc = {
		.name = RTE_MBUF_DYNFLAG_MIRROR_ORIGIN,
	};
	int offset;

	offset = rte_mbuf_dynfield_register(&field_desc);
	if (offset < 0) {
		RTE_ETHDEV_LOG_LINE(ERR,
				    "Failed to register mbuf origin field");
		return -1;
	}
	mirror_origin_offset = offset;

	offset = rte_mbuf_dynflag_register(&flag_desc);
	if (offset < 0) {
		RTE_ETHDEV_LOG_LINE(WARNING,
				    "Failed to register mbuf origin flag");
		return -1;
	}
	mirror_origin_flag = RTE_BIT64(offset);

	strlcpy(flag_desc.name, RTE_MBUF_DYNFLAG_MIRROR_INGRESS,
		sizeof(flag_desc.name));
	offset = rte_mbuf_dynflag_register(&flag_desc);
	if (offset < 0) {
		RTE_ETHDEV_LOG_LINE(WARNING,
				    "Failed to register mbuf ingress flag");
		return -1;
	}
	mirror_ingress_flag = RTE_BIT64(offset);

	strlcpy(flag_desc.name, RTE_MBUF_DYNFLAG_MIRROR_EGRESS,
		sizeof(flag_desc.name));
	offset = rte_mbuf_dynflag_register(&flag_desc);
	if (offset < 0) {
		RTE_ETHDEV_LOG_LINE(WARNING,
				    "Failed to register mbuf egress flag");
		return -1;
	}
	mirror_egress_flag = RTE_BIT64(offset);

	fprintf(stderr,
		"%s() origin offset=%d flag=%"PRIx64" ingress=%"PRIx64" egress=%"PRIx64"\n", __func__,
		mirror_origin_offset, mirror_origin_flag, mirror_ingress_flag, mirror_egress_flag);
	return 0;
}


/**
 * Structure used to hold information mirror port mirrors for a
 * queue on Rx and Tx.
 */
struct rte_eth_mirror {
	RTE_ATOMIC(struct rte_eth_mirror *) next;
	struct rte_eth_mirror_conf conf;
};

/* Add a new mirror entry to the list. */
static int
ethdev_insert_mirror(struct rte_eth_mirror **top,
		   const struct rte_eth_mirror_conf *conf)
{
	struct rte_eth_mirror *mirror = rte_zmalloc(NULL, sizeof(*mirror), 0);
	if (mirror == NULL)
		return -ENOMEM;

	mirror->conf = *conf;
	mirror->next = *top;

	rte_atomic_store_explicit(top, mirror, rte_memory_order_relaxed);
	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_eth_add_mirror, 25.11)
int
rte_eth_add_mirror(uint16_t port_id, const struct rte_eth_mirror_conf *conf)
{
#ifndef RTE_ETHDEV_MIRROR
	return -ENOTSUP;
#endif
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct rte_eth_dev_info dev_info;

	if (conf == NULL) {
		RTE_ETHDEV_LOG_LINE(ERR, "Missing configuration information");
		return -EINVAL;
	}

	if (conf->mp == NULL) {
		RTE_ETHDEV_LOG_LINE(ERR, "not a valid mempool");
		return -EINVAL;
	}

	if (conf->direction == 0 ||
	    conf->direction > (RTE_ETH_MIRROR_DIRECTION_INGRESS | RTE_ETH_MIRROR_DIRECTION_EGRESS)) {
		RTE_ETHDEV_LOG_LINE(ERR, "Invalid direction %#x", conf->direction);
		return -EINVAL;
	}

	if (conf->snaplen < RTE_ETHER_HDR_LEN) {
		RTE_ETHDEV_LOG_LINE(ERR, "invalid snap len");
		return -EINVAL;
	}

	/* Checks that target exists */
	int ret = rte_eth_dev_info_get(conf->target, &dev_info);
	if (ret != 0)
		return ret;

	/* Loopback mirror could create packet storm */
	if (conf->target == port_id) {
		RTE_ETHDEV_LOG_LINE(ERR, "Cannot mirror port to self");
		return -EINVAL;
	}

	/* Because multiple directions and multiple queues will all going to the mirror port
	 * need the transmit path to be lockfree.
	 */
	if (!(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MT_LOCKFREE)) {
		RTE_ETHDEV_LOG_LINE(ERR, "Mirror needs lockfree transmit");
		return -ENOTSUP;
	}

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		/* Register dynamic fields once */
		if (mirror_origin_offset < 0) {
			ret = ethdev_dyn_mirror_register();
			if (ret < 0)
				return ret;
		}

		rte_spinlock_lock(&mirror_port_lock);
		ret = 0;
		if (conf->direction & RTE_ETH_MIRROR_DIRECTION_INGRESS)
			ret = ethdev_insert_mirror(&dev->rx_mirror, conf);
		if (ret == 0 && (conf->direction & RTE_ETH_MIRROR_DIRECTION_EGRESS))
			ret = ethdev_insert_mirror(&dev->tx_mirror, conf);
		rte_spinlock_unlock(&mirror_port_lock);
	} else {
		/* in secondary, proxy to primary */
		ret = ethdev_request(ETH_REQ_ADD_MIRROR, port_id, conf, sizeof(*conf));
		if (ret != 0)
			return ret;
	}

	rte_eth_trace_add_mirror(port_id, conf, ret);
	return ret;
}

static bool
ethdev_delete_mirror(struct rte_eth_mirror **top, uint16_t target_id)
{
	struct rte_eth_mirror *mirror;

	while ((mirror = *top) != NULL) {
		if (mirror->conf.target == target_id)
			goto found;
	}
	/* not found in list */
	return false;

found:
	/* unlink from list */
	rte_atomic_store_explicit(top, mirror->next, rte_memory_order_relaxed);

	/* Defer freeing the mirror until after one second
	 * to allow for active threads that are using it.
	 * Assumes no PMD takes more than one second to transmit a burst.
	 * Alternative would be RCU, but RCU in DPDK is optional.
	 */
	rte_eal_alarm_set(US_PER_S, rte_free, mirror);
	return true;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_eth_remove_mirror, 25.11)
int
rte_eth_remove_mirror(uint16_t port_id, uint16_t target_id)
{
#ifndef RTE_ETHDEV_MIRROR
	return -ENOTSUP;
#endif
	bool found;
	int ret = 0;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];

	RTE_ETH_VALID_PORTID_OR_ERR_RET(target_id, -ENODEV);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		rte_spinlock_lock(&mirror_port_lock);
		found = ethdev_delete_mirror(&dev->rx_mirror, target_id);
		found |= ethdev_delete_mirror(&dev->tx_mirror, target_id);
		rte_spinlock_unlock(&mirror_port_lock);
		if (!found)
			ret = -ENOENT; /* no mirror present */
	} else {
		ret = ethdev_request(port_id, ETH_REQ_REMOVE_MIRROR,
				     &target_id, sizeof(target_id));
	}

	rte_eth_trace_remove_mirror(port_id, target_id, ret);
	return ret;
}

static inline void
eth_dev_mirror(uint16_t port_id, uint8_t direction,
	     struct rte_mbuf **pkts, uint16_t nb_pkts,
	     const struct rte_eth_mirror_conf *conf)
{
	struct rte_mbuf *tosend[RTE_MIRROR_BURST_SIZE];
	unsigned int count = 0;
	unsigned int i;

	for (i = 0; i < nb_pkts; i++) {
		const struct rte_mbuf *m = pkts[i];

		struct rte_mbuf *mc = rte_pktmbuf_copy(m, conf->mp, 0, conf->snaplen);
		/* TODO: dropped stats? */
		if (unlikely(mc == NULL))
			continue;

		mc->port = port_id;

		mc->ol_flags &= ~(mirror_ingress_flag | mirror_egress_flag);
		if (direction & RTE_ETH_MIRROR_DIRECTION_INGRESS)
			mc->ol_flags |= mirror_ingress_flag;
		else if (direction & RTE_ETH_MIRROR_DIRECTION_EGRESS)
			mc->ol_flags |= mirror_egress_flag;

		tosend[count++] = mc;
	}

	uint16_t nsent = rte_eth_tx_burst(conf->target, 0, tosend, count);
	if (unlikely(nsent < count)) {
		uint16_t drop = count - nsent;

		/* TODO: need some stats here? */
		rte_pktmbuf_free_bulk(pkts + nsent, drop);
	}
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_eth_mirror_burst, 25.11)
void
rte_eth_mirror_burst(uint16_t port_id, uint8_t direction,
		     struct rte_mbuf **pkts, uint16_t nb_pkts,
		     const struct rte_eth_mirror *mirror)
{
	unsigned int i;

	for (i = 0; i < nb_pkts; i += RTE_MIRROR_BURST_SIZE) {
		uint16_t left  = nb_pkts - i;
		uint16_t burst = RTE_MIN(left, RTE_MIRROR_BURST_SIZE);

		eth_dev_mirror(port_id, direction,
			     pkts + i, burst, &mirror->conf);
	}
}
