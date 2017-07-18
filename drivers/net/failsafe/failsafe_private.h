/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox.
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

#ifndef _RTE_ETH_FAILSAFE_PRIVATE_H_
#define _RTE_ETH_FAILSAFE_PRIVATE_H_

#include <sys/queue.h>

#include <rte_atomic.h>
#include <rte_dev.h>
#include <rte_ethdev.h>
#include <rte_devargs.h>

#define FAILSAFE_DRIVER_NAME "Fail-safe PMD"

#define PMD_FAILSAFE_MAC_KVARG "mac"
#define PMD_FAILSAFE_HOTPLUG_POLL_KVARG "hotplug_poll"
#define PMD_FAILSAFE_PARAM_STRING	\
	"dev(<ifc>),"			\
	"exec(<shell command>),"	\
	"mac=mac_addr,"			\
	"hotplug_poll=u64"		\
	""

#define FAILSAFE_HOTPLUG_DEFAULT_TIMEOUT_MS 2000

#define FAILSAFE_MAX_ETHPORTS 2
#define FAILSAFE_MAX_ETHADDR 128

/* TYPES */

struct rxq {
	struct fs_priv *priv;
	uint16_t qid;
	/* id of last sub_device polled */
	uint8_t last_polled;
	unsigned int socket_id;
	struct rte_eth_rxq_info info;
	rte_atomic64_t refcnt[];
};

struct txq {
	struct fs_priv *priv;
	uint16_t qid;
	unsigned int socket_id;
	struct rte_eth_txq_info info;
	rte_atomic64_t refcnt[];
};

struct rte_flow {
	TAILQ_ENTRY(rte_flow) next;
	/* sub_flows */
	struct rte_flow *flows[FAILSAFE_MAX_ETHPORTS];
	/* flow description for synchronization */
	struct rte_flow_desc *fd;
};

enum dev_state {
	DEV_UNDEFINED,
	DEV_PARSED,
	DEV_PROBED,
	DEV_ACTIVE,
	DEV_STARTED,
};

struct sub_device {
	/* Exhaustive DPDK device description */
	struct rte_devargs devargs;
	struct rte_bus *bus;
	struct rte_device *dev;
	struct rte_eth_dev *edev;
	uint8_t sid;
	/* Device state machine */
	enum dev_state state;
	/* Some device are defined as a command line */
	char *cmdline;
	/* fail-safe device backreference */
	struct rte_eth_dev *fs_dev;
	/* flag calling for recollection */
	volatile unsigned int remove:1;
	/* flow isolation state */
	int flow_isolated:1;
};

struct fs_priv {
	struct rte_eth_dev *dev;
	/*
	 * Set of sub_devices.
	 * subs[0] is the preferred device
	 * any other is just another slave
	 */
	struct sub_device *subs;
	uint8_t subs_head; /* if head == tail, no subs */
	uint8_t subs_tail; /* first invalid */
	uint8_t subs_tx; /* current emitting device */
	uint8_t current_probed;
	/* flow mapping */
	TAILQ_HEAD(sub_flows, rte_flow) flow_list;
	/* current number of mac_addr slots allocated. */
	uint32_t nb_mac_addr;
	struct ether_addr mac_addrs[FAILSAFE_MAX_ETHADDR];
	uint32_t mac_addr_pool[FAILSAFE_MAX_ETHADDR];
	/* current capabilities */
	struct rte_eth_dev_info infos;
	/*
	 * Fail-safe state machine.
	 * This level will be tracking state of the EAL and eth
	 * layer at large as defined by the user application.
	 * It will then steer the sub_devices toward the same
	 * synchronized state.
	 */
	enum dev_state state;
	unsigned int pending_alarm:1; /* An alarm is pending */
	/* flow isolation state */
	int flow_isolated:1;
};

/* MISC */

int failsafe_hotplug_alarm_install(struct rte_eth_dev *dev);
int failsafe_hotplug_alarm_cancel(struct rte_eth_dev *dev);

/* RX / TX */

void set_burst_fn(struct rte_eth_dev *dev, int force_safe);

uint16_t failsafe_rx_burst(void *rxq,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t failsafe_tx_burst(void *txq,
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

uint16_t failsafe_rx_burst_fast(void *rxq,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t failsafe_tx_burst_fast(void *txq,
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

/* ARGS */

int failsafe_args_parse(struct rte_eth_dev *dev, const char *params);
void failsafe_args_free(struct rte_eth_dev *dev);
int failsafe_args_count_subdevice(struct rte_eth_dev *dev, const char *params);
int failsafe_args_parse_subs(struct rte_eth_dev *dev);

/* EAL */

int failsafe_eal_init(struct rte_eth_dev *dev);
int failsafe_eal_uninit(struct rte_eth_dev *dev);

/* ETH_DEV */

int failsafe_eth_dev_state_sync(struct rte_eth_dev *dev);
void failsafe_dev_remove(struct rte_eth_dev *dev);
int failsafe_eth_rmv_event_callback(uint8_t port_id,
				    enum rte_eth_event_type type,
				    void *arg, void *out);
int failsafe_eth_lsc_event_callback(uint8_t port_id,
				    enum rte_eth_event_type event,
				    void *cb_arg, void *out);

/* GLOBALS */

extern const char pmd_failsafe_driver_name[];
extern const struct eth_dev_ops failsafe_ops;
extern const struct rte_flow_ops fs_flow_ops;
extern uint64_t hotplug_poll;
extern int mac_from_arg;

/* HELPERS */

/* dev: (struct rte_eth_dev *) fail-safe device */
#define PRIV(dev) \
	((struct fs_priv *)(dev)->data->dev_private)

/* sdev: (struct sub_device *) */
#define ETH(sdev) \
	((sdev)->edev)

/* sdev: (struct sub_device *) */
#define PORT_ID(sdev) \
	(ETH(sdev)->data->port_id)

/* sdev: (struct sub_device *) */
#define SUB_ID(sdev) \
	((sdev)->sid)

/**
 * Stateful iterator construct over fail-safe sub-devices:
 * s:     (struct sub_device *), iterator
 * i:     (uint8_t), increment
 * dev:   (struct rte_eth_dev *), fail-safe ethdev
 * state: (enum dev_state), minimum acceptable device state
 */
#define FOREACH_SUBDEV_STATE(s, i, dev, state)				\
	for (i = fs_find_next((dev), 0, state);				\
	     i < PRIV(dev)->subs_tail && (s = &PRIV(dev)->subs[i]);	\
	     i = fs_find_next((dev), i + 1, state))

/**
 * Iterator construct over fail-safe sub-devices:
 * s:   (struct sub_device *), iterator
 * i:   (uint8_t), increment
 * dev: (struct rte_eth_dev *), fail-safe ethdev
 */
#define FOREACH_SUBDEV(s, i, dev)			\
	FOREACH_SUBDEV_STATE(s, i, dev, DEV_UNDEFINED)

/* dev: (struct rte_eth_dev *) fail-safe device */
#define PREFERRED_SUBDEV(dev) \
	(&PRIV(dev)->subs[0])

/* dev: (struct rte_eth_dev *) fail-safe device */
#define TX_SUBDEV(dev)							  \
	(PRIV(dev)->subs_tx >= PRIV(dev)->subs_tail		   ? NULL \
	 : (PRIV(dev)->subs[PRIV(dev)->subs_tx].state < DEV_PROBED ? NULL \
	 : &PRIV(dev)->subs[PRIV(dev)->subs_tx]))

/**
 * s:   (struct sub_device *)
 * ops: (struct eth_dev_ops) member
 */
#define SUBOPS(s, ops) \
	(ETH(s)->dev_ops->ops)

/**
 * Atomic guard
 */

/**
 * a: (rte_atomic64_t)
 */
#define FS_ATOMIC_P(a) \
	rte_atomic64_add(&(a), 1)

/**
 * a: (rte_atomic64_t)
 */
#define FS_ATOMIC_V(a) \
	rte_atomic64_sub(&(a), 1)

/**
 * s: (struct sub_device *)
 * i: uint16_t qid
 */
#define FS_ATOMIC_RX(s, i) \
	rte_atomic64_read( \
	 &((struct rxq *)((s)->fs_dev->data->rx_queues[i]))->refcnt[(s)->sid] \
	)
/**
 * s: (struct sub_device *)
 * i: uint16_t qid
 */
#define FS_ATOMIC_TX(s, i) \
	rte_atomic64_read( \
	 &((struct txq *)((s)->fs_dev->data->tx_queues[i]))->refcnt[(s)->sid] \
	)

#define LOG__(level, m, ...) \
	RTE_LOG(level, PMD, "net_failsafe: " m "%c", __VA_ARGS__)
#define LOG_(level, ...) LOG__(level, __VA_ARGS__, '\n')
#define DEBUG(...) LOG_(DEBUG, __VA_ARGS__)
#define INFO(...) LOG_(INFO, __VA_ARGS__)
#define WARN(...) LOG_(WARNING, __VA_ARGS__)
#define ERROR(...) LOG_(ERR, __VA_ARGS__)

/* inlined functions */

static inline uint8_t
fs_find_next(struct rte_eth_dev *dev, uint8_t sid,
		enum dev_state min_state)
{
	while (sid < PRIV(dev)->subs_tail) {
		if (PRIV(dev)->subs[sid].state >= min_state)
			break;
		sid++;
	}
	if (sid >= PRIV(dev)->subs_tail)
		return PRIV(dev)->subs_tail;
	return sid;
}

/*
 * Switch emitting device.
 * If banned is set, banned must not be considered for
 * the role of emitting device.
 */
static inline void
fs_switch_dev(struct rte_eth_dev *dev,
	      struct sub_device *banned)
{
	struct sub_device *txd;
	enum dev_state req_state;

	req_state = PRIV(dev)->state;
	txd = TX_SUBDEV(dev);
	if (PREFERRED_SUBDEV(dev)->state >= req_state &&
	    PREFERRED_SUBDEV(dev) != banned) {
		if (txd != PREFERRED_SUBDEV(dev) &&
		    (txd == NULL ||
		     (req_state == DEV_STARTED) ||
		     (txd && txd->state < DEV_STARTED))) {
			DEBUG("Switching tx_dev to preferred sub_device");
			PRIV(dev)->subs_tx = 0;
		}
	} else if ((txd && txd->state < req_state) ||
		   txd == NULL ||
		   txd == banned) {
		struct sub_device *sdev;
		uint8_t i;

		/* Using acceptable device */
		FOREACH_SUBDEV_STATE(sdev, i, dev, req_state) {
			if (sdev == banned)
				continue;
			DEBUG("Switching tx_dev to sub_device %d",
			      i);
			PRIV(dev)->subs_tx = i;
			break;
		}
	} else if (txd && txd->state < req_state) {
		DEBUG("No device ready, deactivating tx_dev");
		PRIV(dev)->subs_tx = PRIV(dev)->subs_tail;
	} else {
		return;
	}
	set_burst_fn(dev, 0);
	rte_wmb();
}

#endif /* _RTE_ETH_FAILSAFE_PRIVATE_H_ */
