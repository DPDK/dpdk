/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#ifndef _ETH_FAILSAFE_PRIVATE_H_
#define _ETH_FAILSAFE_PRIVATE_H_

#include <stdint.h>
#include <sys/queue.h>
#include <pthread.h>

#include <rte_atomic.h>
#include <rte_dev.h>
#include <rte_ethdev_driver.h>
#include <rte_devargs.h>
#include <rte_flow.h>
#include <rte_interrupts.h>

#define FAILSAFE_DRIVER_NAME "Fail-safe PMD"
#define FAILSAFE_OWNER_NAME "Fail-safe"

#define PMD_FAILSAFE_MAC_KVARG "mac"
#define PMD_FAILSAFE_HOTPLUG_POLL_KVARG "hotplug_poll"
#define PMD_FAILSAFE_PARAM_STRING	\
	"dev(<ifc>),"			\
	"exec(<shell command>),"	\
	"fd(<fd number>),"		\
	"mac=mac_addr,"			\
	"hotplug_poll=u64"		\
	""

#define FAILSAFE_HOTPLUG_DEFAULT_TIMEOUT_MS 2000

#define FAILSAFE_MAX_ETHPORTS 2
#define FAILSAFE_MAX_ETHADDR 128

#define DEVARGS_MAXLEN 4096

enum rxp_service_state {
	SS_NO_SERVICE = 0,
	SS_REGISTERED,
	SS_READY,
	SS_RUNNING,
};

/* TYPES */

struct rx_proxy {
	/* epoll file descriptor */
	int efd;
	/* event vector to be used by epoll */
	struct rte_epoll_event *evec;
	/* rte service id */
	uint32_t sid;
	/* service core id */
	uint32_t scid;
	enum rxp_service_state sstate;
};

#define FS_RX_PROXY_INIT (struct rx_proxy){ \
	.efd = -1, \
	.evec = NULL, \
	.sid = 0, \
	.scid = 0, \
	.sstate = SS_NO_SERVICE, \
}

struct rxq {
	struct fs_priv *priv;
	uint16_t qid;
	/* next sub_device to poll */
	struct sub_device *sdev;
	unsigned int socket_id;
	int event_fd;
	unsigned int enable_events:1;
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
	struct rte_flow_conv_rule rule;
	uint8_t rule_data[];
};

enum dev_state {
	DEV_UNDEFINED,
	DEV_PARSED,
	DEV_PROBED,
	DEV_ACTIVE,
	DEV_STARTED,
};

struct fs_stats {
	struct rte_eth_stats stats;
	uint64_t timestamp;
};

/*
 * Allocated in shared memory.
 */
struct sub_device {
	/* Exhaustive DPDK device description */
	struct sub_device *next;
	struct rte_devargs devargs;
	struct rte_bus *bus; /* for primary process only. */
	struct rte_device *dev; /* for primary process only. */
	uint8_t sid;
	/* Device state machine */
	enum dev_state state;
	/* Last stats snapshot passed to user */
	struct fs_stats stats_snapshot;
	/* Some device are defined as a command line */
	char *cmdline;
	/* Others are retrieved through a file descriptor */
	char *fd_str;
	/* fail-safe device backreference */
	uint16_t fs_port_id; /* shared between processes */
	/* sub device port id*/
	uint16_t sdev_port_id; /* shared between processes */
	/* flag calling for recollection */
	volatile unsigned int remove:1;
	/* flow isolation state */
	int flow_isolated:1;
	/* RMV callback registration state */
	unsigned int rmv_callback:1;
	/* LSC callback registration state */
	unsigned int lsc_callback:1;
};

/*
 * This is referenced by eth_dev->data->dev_private
 * This is shared between processes.
 */
struct fs_priv {
	struct rte_eth_dev_data *data; /* backreference to shared data. */
	/*
	 * Set of sub_devices.
	 * subs[0] is the preferred device
	 * any other is just another sub device
	 */
	struct sub_device *subs;  /* shared between processes */
	uint8_t subs_head; /* if head == tail, no subs */
	uint8_t subs_tail; /* first invalid */
	uint8_t subs_tx; /* current emitting device */
	uint8_t current_probed;
	/* flow mapping */
	TAILQ_HEAD(sub_flows, rte_flow) flow_list;
	/* current number of mac_addr slots allocated. */
	uint32_t nb_mac_addr;
	struct rte_ether_addr mac_addrs[FAILSAFE_MAX_ETHADDR];
	uint32_t mac_addr_pool[FAILSAFE_MAX_ETHADDR];
	uint32_t nb_mcast_addr;
	struct rte_ether_addr *mcast_addrs;
	/* current capabilities */
	struct rte_eth_dev_owner my_owner; /* Unique owner. */
	struct rte_intr_handle intr_handle; /* Port interrupt handle. */
	/*
	 * Fail-safe state machine.
	 * This level will be tracking state of the EAL and eth
	 * layer at large as defined by the user application.
	 * It will then steer the sub_devices toward the same
	 * synchronized state.
	 */
	enum dev_state state;
	struct rte_eth_stats stats_accumulator;
	/*
	 * Rx interrupts/events proxy.
	 * The PMD issues Rx events to the EAL on behalf of its subdevices,
	 * it does that by registering an event-fd for each of its queues with
	 * the EAL. A PMD service thread listens to all the Rx events from the
	 * subdevices, when an Rx event is issued by a subdevice it will be
	 * caught by this service with will trigger an Rx event in the
	 * appropriate failsafe Rx queue.
	 */
	struct rx_proxy rxp;
	pthread_mutex_t hotplug_mutex;
	/* Hot-plug mutex is locked by the alarm mechanism. */
	volatile unsigned int alarm_lock:1;
	unsigned int pending_alarm:1; /* An alarm is pending */
	/* flow isolation state */
	int flow_isolated:1;
};

/* FAILSAFE_INTR */

int failsafe_rx_intr_install(struct rte_eth_dev *dev);
void failsafe_rx_intr_uninstall(struct rte_eth_dev *dev);
int failsafe_rx_intr_install_subdevice(struct sub_device *sdev);
void failsafe_rx_intr_uninstall_subdevice(struct sub_device *sdev);

/* MISC */

int failsafe_hotplug_alarm_install(struct rte_eth_dev *dev);
int failsafe_hotplug_alarm_cancel(struct rte_eth_dev *dev);

/* RX / TX */

void failsafe_set_burst_fn(struct rte_eth_dev *dev, int force_safe);

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
void failsafe_eth_dev_unregister_callbacks(struct sub_device *sdev);
int failsafe_eth_dev_close(struct rte_eth_dev *dev);
void failsafe_dev_remove(struct rte_eth_dev *dev);
void failsafe_stats_increment(struct rte_eth_stats *to,
				struct rte_eth_stats *from);
int failsafe_eth_rmv_event_callback(uint16_t port_id,
				    enum rte_eth_event_type type,
				    void *arg, void *out);
int failsafe_eth_lsc_event_callback(uint16_t port_id,
				    enum rte_eth_event_type event,
				    void *cb_arg, void *out);
int failsafe_eth_new_event_callback(uint16_t port_id,
				    enum rte_eth_event_type event,
				    void *cb_arg, void *out);

/* GLOBALS */

extern const char pmd_failsafe_driver_name[];
extern const struct eth_dev_ops failsafe_ops;
extern const struct rte_flow_ops fs_flow_ops;
extern uint64_t failsafe_hotplug_poll;
extern int failsafe_mac_from_arg;

/* HELPERS */

/* dev: (struct rte_eth_dev *) fail-safe device */
#define PRIV(dev) \
	((struct fs_priv *)(dev)->data->dev_private)

/* sdev: (struct sub_device *) */
#define ETH(sdev) \
	((sdev)->sdev_port_id == RTE_MAX_ETHPORTS ? \
	NULL : &rte_eth_devices[(sdev)->sdev_port_id])

/* sdev: (struct sub_device *) */
#define PORT_ID(sdev) \
	((sdev)->sdev_port_id)

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
#define FOREACH_SUBDEV_STATE(s, i, dev, state)		\
	for (s = fs_find_next((dev), 0, state, &i);	\
	     s != NULL;					\
	     s = fs_find_next((dev), i + 1, state, &i))

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
	rte_atomic64_set(&(a), 1)

/**
 * a: (rte_atomic64_t)
 */
#define FS_ATOMIC_V(a) \
	rte_atomic64_set(&(a), 0)

/**
 * s: (struct sub_device *)
 * i: uint16_t qid
 */
#define FS_ATOMIC_RX(s, i) \
	rte_atomic64_read( \
	 &((struct rxq *) \
	 (fs_dev(s)->data->rx_queues[i]))->refcnt[(s)->sid])
/**
 * s: (struct sub_device *)
 * i: uint16_t qid
 */
#define FS_ATOMIC_TX(s, i) \
	rte_atomic64_read( \
	 &((struct txq *) \
	 (fs_dev(s)->data->tx_queues[i]))->refcnt[(s)->sid])

#ifdef RTE_EXEC_ENV_FREEBSD
#define FS_THREADID_TYPE void*
#define FS_THREADID_FMT  "p"
#else
#define FS_THREADID_TYPE unsigned long
#define FS_THREADID_FMT  "lu"
#endif

extern int failsafe_logtype;

#define LOG__(l, m, ...) \
	rte_log(RTE_LOG_ ## l, failsafe_logtype, \
		"net_failsafe: " m "%c", __VA_ARGS__)

#define LOG_(level, ...) LOG__(level, __VA_ARGS__, '\n')
#define DEBUG(...) LOG_(DEBUG, __VA_ARGS__)
#define INFO(...) LOG_(INFO, __VA_ARGS__)
#define WARN(...) LOG_(WARNING, __VA_ARGS__)
#define ERROR(...) LOG_(ERR, __VA_ARGS__)

/* inlined functions */

static inline struct sub_device *
fs_find_next(struct rte_eth_dev *dev,
	     uint8_t sid,
	     enum dev_state min_state,
	     uint8_t *sid_out)
{
	struct sub_device *subs;
	uint8_t tail;

	subs = PRIV(dev)->subs;
	tail = PRIV(dev)->subs_tail;
	while (sid < tail) {
		if (subs[sid].state >= min_state)
			break;
		sid++;
	}
	*sid_out = sid;
	if (sid >= tail)
		return NULL;
	return &subs[sid];
}

static inline struct rte_eth_dev *
fs_dev(struct sub_device *sdev) {
	return &rte_eth_devices[sdev->fs_port_id];
}

/*
 * Lock hot-plug mutex.
 * is_alarm means that the caller is, for sure, the hot-plug alarm mechanism.
 */
static inline int
fs_lock(struct rte_eth_dev *dev, unsigned int is_alarm)
{
	int ret;

	if (is_alarm) {
		ret = pthread_mutex_trylock(&PRIV(dev)->hotplug_mutex);
		if (ret) {
			DEBUG("Hot-plug mutex lock trying failed(%s), will try"
			      " again later...", strerror(ret));
			return ret;
		}
		PRIV(dev)->alarm_lock = 1;
	} else {
		ret = pthread_mutex_lock(&PRIV(dev)->hotplug_mutex);
		if (ret) {
			ERROR("Cannot lock mutex(%s)", strerror(ret));
			return ret;
		}
	}
	return ret;
}

/*
 * Unlock hot-plug mutex.
 * is_alarm means that the caller is, for sure, the hot-plug alarm mechanism.
 */
static inline void
fs_unlock(struct rte_eth_dev *dev, unsigned int is_alarm)
{
	int ret;

	if (is_alarm) {
		RTE_ASSERT(PRIV(dev)->alarm_lock == 1);
		PRIV(dev)->alarm_lock = 0;
	}
	ret = pthread_mutex_unlock(&PRIV(dev)->hotplug_mutex);
	if (ret)
		ERROR("Cannot unlock hot-plug mutex(%s)", strerror(ret));
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
		struct sub_device *sdev = NULL;
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
		if (i >= PRIV(dev)->subs_tail || sdev == NULL) {
			DEBUG("No device ready, deactivating tx_dev");
			PRIV(dev)->subs_tx = PRIV(dev)->subs_tail;
		}
	} else {
		return;
	}
	failsafe_set_burst_fn(dev, 0);
	rte_wmb();
}

/*
 * Adjust error value and rte_errno to the fail-safe actual error value.
 */
static inline int
fs_err(struct sub_device *sdev, int err)
{
	/* A device removal shouldn't be reported as an error. */
	if (sdev->remove == 1 || err == -EIO)
		return rte_errno = 0;
	return err;
}
#endif /* _ETH_FAILSAFE_PRIVATE_H_ */
