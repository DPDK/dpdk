/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <netinet/in.h>

#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_interrupts.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_errno.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>
#include <rte_kvargs.h>
#include <rte_class.h>
#include <rte_ether.h>
#include <rte_telemetry.h>

#include "rte_ethdev_trace.h"
#include "rte_ethdev.h"
#include "rte_ethdev_driver.h"
#include "ethdev_profile.h"
#include "ethdev_private.h"

static const char *MZ_RTE_ETH_DEV_DATA = "rte_eth_dev_data";
struct rte_eth_dev rte_eth_devices[RTE_MAX_ETHPORTS];

/* spinlock for eth device callbacks */
static rte_spinlock_t eth_dev_cb_lock = RTE_SPINLOCK_INITIALIZER;

/* spinlock for add/remove rx callbacks */
static rte_spinlock_t eth_dev_rx_cb_lock = RTE_SPINLOCK_INITIALIZER;

/* spinlock for add/remove tx callbacks */
static rte_spinlock_t eth_dev_tx_cb_lock = RTE_SPINLOCK_INITIALIZER;

/* spinlock for shared data allocation */
static rte_spinlock_t eth_dev_shared_data_lock = RTE_SPINLOCK_INITIALIZER;

/* store statistics names and its offset in stats structure  */
struct rte_eth_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned offset;
};

/* Shared memory between primary and secondary processes. */
static struct {
	uint64_t next_owner_id;
	rte_spinlock_t ownership_lock;
	struct rte_eth_dev_data data[RTE_MAX_ETHPORTS];
} *eth_dev_shared_data;

static const struct rte_eth_xstats_name_off eth_dev_stats_strings[] = {
	{"rx_good_packets", offsetof(struct rte_eth_stats, ipackets)},
	{"tx_good_packets", offsetof(struct rte_eth_stats, opackets)},
	{"rx_good_bytes", offsetof(struct rte_eth_stats, ibytes)},
	{"tx_good_bytes", offsetof(struct rte_eth_stats, obytes)},
	{"rx_missed_errors", offsetof(struct rte_eth_stats, imissed)},
	{"rx_errors", offsetof(struct rte_eth_stats, ierrors)},
	{"tx_errors", offsetof(struct rte_eth_stats, oerrors)},
	{"rx_mbuf_allocation_errors", offsetof(struct rte_eth_stats,
		rx_nombuf)},
};

#define RTE_NB_STATS RTE_DIM(eth_dev_stats_strings)

static const struct rte_eth_xstats_name_off eth_dev_rxq_stats_strings[] = {
	{"packets", offsetof(struct rte_eth_stats, q_ipackets)},
	{"bytes", offsetof(struct rte_eth_stats, q_ibytes)},
	{"errors", offsetof(struct rte_eth_stats, q_errors)},
};

#define RTE_NB_RXQ_STATS RTE_DIM(eth_dev_rxq_stats_strings)

static const struct rte_eth_xstats_name_off eth_dev_txq_stats_strings[] = {
	{"packets", offsetof(struct rte_eth_stats, q_opackets)},
	{"bytes", offsetof(struct rte_eth_stats, q_obytes)},
};
#define RTE_NB_TXQ_STATS RTE_DIM(eth_dev_txq_stats_strings)

#define RTE_RX_OFFLOAD_BIT2STR(_name)	\
	{ DEV_RX_OFFLOAD_##_name, #_name }

#define RTE_ETH_RX_OFFLOAD_BIT2STR(_name)	\
	{ RTE_ETH_RX_OFFLOAD_##_name, #_name }

static const struct {
	uint64_t offload;
	const char *name;
} eth_dev_rx_offload_names[] = {
	RTE_RX_OFFLOAD_BIT2STR(VLAN_STRIP),
	RTE_RX_OFFLOAD_BIT2STR(IPV4_CKSUM),
	RTE_RX_OFFLOAD_BIT2STR(UDP_CKSUM),
	RTE_RX_OFFLOAD_BIT2STR(TCP_CKSUM),
	RTE_RX_OFFLOAD_BIT2STR(TCP_LRO),
	RTE_RX_OFFLOAD_BIT2STR(QINQ_STRIP),
	RTE_RX_OFFLOAD_BIT2STR(OUTER_IPV4_CKSUM),
	RTE_RX_OFFLOAD_BIT2STR(MACSEC_STRIP),
	RTE_RX_OFFLOAD_BIT2STR(HEADER_SPLIT),
	RTE_RX_OFFLOAD_BIT2STR(VLAN_FILTER),
	RTE_RX_OFFLOAD_BIT2STR(VLAN_EXTEND),
	RTE_RX_OFFLOAD_BIT2STR(JUMBO_FRAME),
	RTE_RX_OFFLOAD_BIT2STR(SCATTER),
	RTE_RX_OFFLOAD_BIT2STR(TIMESTAMP),
	RTE_RX_OFFLOAD_BIT2STR(SECURITY),
	RTE_RX_OFFLOAD_BIT2STR(KEEP_CRC),
	RTE_RX_OFFLOAD_BIT2STR(SCTP_CKSUM),
	RTE_RX_OFFLOAD_BIT2STR(OUTER_UDP_CKSUM),
	RTE_RX_OFFLOAD_BIT2STR(RSS_HASH),
	RTE_ETH_RX_OFFLOAD_BIT2STR(BUFFER_SPLIT),
};

#undef RTE_RX_OFFLOAD_BIT2STR
#undef RTE_ETH_RX_OFFLOAD_BIT2STR

#define RTE_TX_OFFLOAD_BIT2STR(_name)	\
	{ DEV_TX_OFFLOAD_##_name, #_name }

static const struct {
	uint64_t offload;
	const char *name;
} eth_dev_tx_offload_names[] = {
	RTE_TX_OFFLOAD_BIT2STR(VLAN_INSERT),
	RTE_TX_OFFLOAD_BIT2STR(IPV4_CKSUM),
	RTE_TX_OFFLOAD_BIT2STR(UDP_CKSUM),
	RTE_TX_OFFLOAD_BIT2STR(TCP_CKSUM),
	RTE_TX_OFFLOAD_BIT2STR(SCTP_CKSUM),
	RTE_TX_OFFLOAD_BIT2STR(TCP_TSO),
	RTE_TX_OFFLOAD_BIT2STR(UDP_TSO),
	RTE_TX_OFFLOAD_BIT2STR(OUTER_IPV4_CKSUM),
	RTE_TX_OFFLOAD_BIT2STR(QINQ_INSERT),
	RTE_TX_OFFLOAD_BIT2STR(VXLAN_TNL_TSO),
	RTE_TX_OFFLOAD_BIT2STR(GRE_TNL_TSO),
	RTE_TX_OFFLOAD_BIT2STR(IPIP_TNL_TSO),
	RTE_TX_OFFLOAD_BIT2STR(GENEVE_TNL_TSO),
	RTE_TX_OFFLOAD_BIT2STR(MACSEC_INSERT),
	RTE_TX_OFFLOAD_BIT2STR(MT_LOCKFREE),
	RTE_TX_OFFLOAD_BIT2STR(MULTI_SEGS),
	RTE_TX_OFFLOAD_BIT2STR(MBUF_FAST_FREE),
	RTE_TX_OFFLOAD_BIT2STR(SECURITY),
	RTE_TX_OFFLOAD_BIT2STR(UDP_TNL_TSO),
	RTE_TX_OFFLOAD_BIT2STR(IP_TNL_TSO),
	RTE_TX_OFFLOAD_BIT2STR(OUTER_UDP_CKSUM),
	RTE_TX_OFFLOAD_BIT2STR(SEND_ON_TIMESTAMP),
};

#undef RTE_TX_OFFLOAD_BIT2STR

/**
 * The user application callback description.
 *
 * It contains callback address to be registered by user application,
 * the pointer to the parameters for callback, and the event type.
 */
struct rte_eth_dev_callback {
	TAILQ_ENTRY(rte_eth_dev_callback) next; /**< Callbacks list */
	rte_eth_dev_cb_fn cb_fn;                /**< Callback address */
	void *cb_arg;                           /**< Parameter for callback */
	void *ret_param;                        /**< Return parameter */
	enum rte_eth_event_type event;          /**< Interrupt event type */
	uint32_t active;                        /**< Callback is executing */
};

enum {
	STAT_QMAP_TX = 0,
	STAT_QMAP_RX
};

int
rte_eth_iterator_init(struct rte_dev_iterator *iter, const char *devargs_str)
{
	int ret;
	struct rte_devargs devargs = {.args = NULL};
	const char *bus_param_key;
	char *bus_str = NULL;
	char *cls_str = NULL;
	int str_size;

	memset(iter, 0, sizeof(*iter));

	/*
	 * The devargs string may use various syntaxes:
	 *   - 0000:08:00.0,representor=[1-3]
	 *   - pci:0000:06:00.0,representor=[0,5]
	 *   - class=eth,mac=00:11:22:33:44:55
	 * A new syntax is in development (not yet supported):
	 *   - bus=X,paramX=x/class=Y,paramY=y/driver=Z,paramZ=z
	 */

	/*
	 * Handle pure class filter (i.e. without any bus-level argument),
	 * from future new syntax.
	 * rte_devargs_parse() is not yet supporting the new syntax,
	 * that's why this simple case is temporarily parsed here.
	 */
#define iter_anybus_str "class=eth,"
	if (strncmp(devargs_str, iter_anybus_str,
			strlen(iter_anybus_str)) == 0) {
		iter->cls_str = devargs_str + strlen(iter_anybus_str);
		goto end;
	}

	/* Split bus, device and parameters. */
	ret = rte_devargs_parse(&devargs, devargs_str);
	if (ret != 0)
		goto error;

	/*
	 * Assume parameters of old syntax can match only at ethdev level.
	 * Extra parameters will be ignored, thanks to "+" prefix.
	 */
	str_size = strlen(devargs.args) + 2;
	cls_str = malloc(str_size);
	if (cls_str == NULL) {
		ret = -ENOMEM;
		goto error;
	}
	ret = snprintf(cls_str, str_size, "+%s", devargs.args);
	if (ret != str_size - 1) {
		ret = -EINVAL;
		goto error;
	}
	iter->cls_str = cls_str;
	free(devargs.args); /* allocated by rte_devargs_parse() */
	devargs.args = NULL;

	iter->bus = devargs.bus;
	if (iter->bus->dev_iterate == NULL) {
		ret = -ENOTSUP;
		goto error;
	}

	/* Convert bus args to new syntax for use with new API dev_iterate. */
	if ((strcmp(iter->bus->name, "vdev") == 0) ||
		(strcmp(iter->bus->name, "fslmc") == 0) ||
		(strcmp(iter->bus->name, "dpaa_bus") == 0)) {
		bus_param_key = "name";
	} else if (strcmp(iter->bus->name, "pci") == 0) {
		bus_param_key = "addr";
	} else {
		ret = -ENOTSUP;
		goto error;
	}
	str_size = strlen(bus_param_key) + strlen(devargs.name) + 2;
	bus_str = malloc(str_size);
	if (bus_str == NULL) {
		ret = -ENOMEM;
		goto error;
	}
	ret = snprintf(bus_str, str_size, "%s=%s",
			bus_param_key, devargs.name);
	if (ret != str_size - 1) {
		ret = -EINVAL;
		goto error;
	}
	iter->bus_str = bus_str;

end:
	iter->cls = rte_class_find_by_name("eth");
	return 0;

error:
	if (ret == -ENOTSUP)
		RTE_ETHDEV_LOG(ERR, "Bus %s does not support iterating.\n",
				iter->bus->name);
	free(devargs.args);
	free(bus_str);
	free(cls_str);
	return ret;
}

uint16_t
rte_eth_iterator_next(struct rte_dev_iterator *iter)
{
	if (iter->cls == NULL) /* invalid ethdev iterator */
		return RTE_MAX_ETHPORTS;

	do { /* loop to try all matching rte_device */
		/* If not pure ethdev filter and */
		if (iter->bus != NULL &&
				/* not in middle of rte_eth_dev iteration, */
				iter->class_device == NULL) {
			/* get next rte_device to try. */
			iter->device = iter->bus->dev_iterate(
					iter->device, iter->bus_str, iter);
			if (iter->device == NULL)
				break; /* no more rte_device candidate */
		}
		/* A device is matching bus part, need to check ethdev part. */
		iter->class_device = iter->cls->dev_iterate(
				iter->class_device, iter->cls_str, iter);
		if (iter->class_device != NULL)
			return eth_dev_to_id(iter->class_device); /* match */
	} while (iter->bus != NULL); /* need to try next rte_device */

	/* No more ethdev port to iterate. */
	rte_eth_iterator_cleanup(iter);
	return RTE_MAX_ETHPORTS;
}

void
rte_eth_iterator_cleanup(struct rte_dev_iterator *iter)
{
	if (iter->bus_str == NULL)
		return; /* nothing to free in pure class filter */
	free(RTE_CAST_FIELD(iter, bus_str, char *)); /* workaround const */
	free(RTE_CAST_FIELD(iter, cls_str, char *)); /* workaround const */
	memset(iter, 0, sizeof(*iter));
}

uint16_t
rte_eth_find_next(uint16_t port_id)
{
	while (port_id < RTE_MAX_ETHPORTS &&
			rte_eth_devices[port_id].state == RTE_ETH_DEV_UNUSED)
		port_id++;

	if (port_id >= RTE_MAX_ETHPORTS)
		return RTE_MAX_ETHPORTS;

	return port_id;
}

/*
 * Macro to iterate over all valid ports for internal usage.
 * Note: RTE_ETH_FOREACH_DEV is different because filtering owned ports.
 */
#define RTE_ETH_FOREACH_VALID_DEV(port_id) \
	for (port_id = rte_eth_find_next(0); \
	     port_id < RTE_MAX_ETHPORTS; \
	     port_id = rte_eth_find_next(port_id + 1))

uint16_t
rte_eth_find_next_of(uint16_t port_id, const struct rte_device *parent)
{
	port_id = rte_eth_find_next(port_id);
	while (port_id < RTE_MAX_ETHPORTS &&
			rte_eth_devices[port_id].device != parent)
		port_id = rte_eth_find_next(port_id + 1);

	return port_id;
}

uint16_t
rte_eth_find_next_sibling(uint16_t port_id, uint16_t ref_port_id)
{
	RTE_ETH_VALID_PORTID_OR_ERR_RET(ref_port_id, RTE_MAX_ETHPORTS);
	return rte_eth_find_next_of(port_id,
			rte_eth_devices[ref_port_id].device);
}

static void
eth_dev_shared_data_prepare(void)
{
	const unsigned flags = 0;
	const struct rte_memzone *mz;

	rte_spinlock_lock(&eth_dev_shared_data_lock);

	if (eth_dev_shared_data == NULL) {
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			/* Allocate port data and ownership shared memory. */
			mz = rte_memzone_reserve(MZ_RTE_ETH_DEV_DATA,
					sizeof(*eth_dev_shared_data),
					rte_socket_id(), flags);
		} else
			mz = rte_memzone_lookup(MZ_RTE_ETH_DEV_DATA);
		if (mz == NULL)
			rte_panic("Cannot allocate ethdev shared data\n");

		eth_dev_shared_data = mz->addr;
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			eth_dev_shared_data->next_owner_id =
					RTE_ETH_DEV_NO_OWNER + 1;
			rte_spinlock_init(&eth_dev_shared_data->ownership_lock);
			memset(eth_dev_shared_data->data, 0,
			       sizeof(eth_dev_shared_data->data));
		}
	}

	rte_spinlock_unlock(&eth_dev_shared_data_lock);
}

static bool
eth_dev_is_allocated(const struct rte_eth_dev *ethdev)
{
	return ethdev->data->name[0] != '\0';
}

static struct rte_eth_dev *
eth_dev_allocated(const char *name)
{
	uint16_t i;

	RTE_BUILD_BUG_ON(RTE_MAX_ETHPORTS >= UINT16_MAX);

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (rte_eth_devices[i].data != NULL &&
		    strcmp(rte_eth_devices[i].data->name, name) == 0)
			return &rte_eth_devices[i];
	}
	return NULL;
}

struct rte_eth_dev *
rte_eth_dev_allocated(const char *name)
{
	struct rte_eth_dev *ethdev;

	eth_dev_shared_data_prepare();

	rte_spinlock_lock(&eth_dev_shared_data->ownership_lock);

	ethdev = eth_dev_allocated(name);

	rte_spinlock_unlock(&eth_dev_shared_data->ownership_lock);

	return ethdev;
}

static uint16_t
eth_dev_find_free_port(void)
{
	uint16_t i;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		/* Using shared name field to find a free port. */
		if (eth_dev_shared_data->data[i].name[0] == '\0') {
			RTE_ASSERT(rte_eth_devices[i].state ==
				   RTE_ETH_DEV_UNUSED);
			return i;
		}
	}
	return RTE_MAX_ETHPORTS;
}

static struct rte_eth_dev *
eth_dev_get(uint16_t port_id)
{
	struct rte_eth_dev *eth_dev = &rte_eth_devices[port_id];

	eth_dev->data = &eth_dev_shared_data->data[port_id];

	return eth_dev;
}

struct rte_eth_dev *
rte_eth_dev_allocate(const char *name)
{
	uint16_t port_id;
	struct rte_eth_dev *eth_dev = NULL;
	size_t name_len;

	name_len = strnlen(name, RTE_ETH_NAME_MAX_LEN);
	if (name_len == 0) {
		RTE_ETHDEV_LOG(ERR, "Zero length Ethernet device name\n");
		return NULL;
	}

	if (name_len >= RTE_ETH_NAME_MAX_LEN) {
		RTE_ETHDEV_LOG(ERR, "Ethernet device name is too long\n");
		return NULL;
	}

	eth_dev_shared_data_prepare();

	/* Synchronize port creation between primary and secondary threads. */
	rte_spinlock_lock(&eth_dev_shared_data->ownership_lock);

	if (eth_dev_allocated(name) != NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Ethernet device with name %s already allocated\n",
			name);
		goto unlock;
	}

	port_id = eth_dev_find_free_port();
	if (port_id == RTE_MAX_ETHPORTS) {
		RTE_ETHDEV_LOG(ERR,
			"Reached maximum number of Ethernet ports\n");
		goto unlock;
	}

	eth_dev = eth_dev_get(port_id);
	strlcpy(eth_dev->data->name, name, sizeof(eth_dev->data->name));
	eth_dev->data->port_id = port_id;
	eth_dev->data->mtu = RTE_ETHER_MTU;
	pthread_mutex_init(&eth_dev->data->flow_ops_mutex, NULL);

unlock:
	rte_spinlock_unlock(&eth_dev_shared_data->ownership_lock);

	return eth_dev;
}

/*
 * Attach to a port already registered by the primary process, which
 * makes sure that the same device would have the same port id both
 * in the primary and secondary process.
 */
struct rte_eth_dev *
rte_eth_dev_attach_secondary(const char *name)
{
	uint16_t i;
	struct rte_eth_dev *eth_dev = NULL;

	eth_dev_shared_data_prepare();

	/* Synchronize port attachment to primary port creation and release. */
	rte_spinlock_lock(&eth_dev_shared_data->ownership_lock);

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (strcmp(eth_dev_shared_data->data[i].name, name) == 0)
			break;
	}
	if (i == RTE_MAX_ETHPORTS) {
		RTE_ETHDEV_LOG(ERR,
			"Device %s is not driven by the primary process\n",
			name);
	} else {
		eth_dev = eth_dev_get(i);
		RTE_ASSERT(eth_dev->data->port_id == i);
	}

	rte_spinlock_unlock(&eth_dev_shared_data->ownership_lock);
	return eth_dev;
}

int
rte_eth_dev_release_port(struct rte_eth_dev *eth_dev)
{
	if (eth_dev == NULL)
		return -EINVAL;

	eth_dev_shared_data_prepare();

	if (eth_dev->state != RTE_ETH_DEV_UNUSED)
		rte_eth_dev_callback_process(eth_dev,
				RTE_ETH_EVENT_DESTROY, NULL);

	rte_spinlock_lock(&eth_dev_shared_data->ownership_lock);

	eth_dev->state = RTE_ETH_DEV_UNUSED;
	eth_dev->device = NULL;
	eth_dev->process_private = NULL;
	eth_dev->intr_handle = NULL;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;
	eth_dev->tx_pkt_prepare = NULL;
	eth_dev->rx_queue_count = NULL;
	eth_dev->rx_descriptor_done = NULL;
	eth_dev->rx_descriptor_status = NULL;
	eth_dev->tx_descriptor_status = NULL;
	eth_dev->dev_ops = NULL;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		rte_free(eth_dev->data->rx_queues);
		rte_free(eth_dev->data->tx_queues);
		rte_free(eth_dev->data->mac_addrs);
		rte_free(eth_dev->data->hash_mac_addrs);
		rte_free(eth_dev->data->dev_private);
		pthread_mutex_destroy(&eth_dev->data->flow_ops_mutex);
		memset(eth_dev->data, 0, sizeof(struct rte_eth_dev_data));
	}

	rte_spinlock_unlock(&eth_dev_shared_data->ownership_lock);

	return 0;
}

int
rte_eth_dev_is_valid_port(uint16_t port_id)
{
	if (port_id >= RTE_MAX_ETHPORTS ||
	    (rte_eth_devices[port_id].state == RTE_ETH_DEV_UNUSED))
		return 0;
	else
		return 1;
}

static int
eth_is_valid_owner_id(uint64_t owner_id)
{
	if (owner_id == RTE_ETH_DEV_NO_OWNER ||
	    eth_dev_shared_data->next_owner_id <= owner_id)
		return 0;
	return 1;
}

uint64_t
rte_eth_find_next_owned_by(uint16_t port_id, const uint64_t owner_id)
{
	port_id = rte_eth_find_next(port_id);
	while (port_id < RTE_MAX_ETHPORTS &&
			rte_eth_devices[port_id].data->owner.id != owner_id)
		port_id = rte_eth_find_next(port_id + 1);

	return port_id;
}

int
rte_eth_dev_owner_new(uint64_t *owner_id)
{
	eth_dev_shared_data_prepare();

	rte_spinlock_lock(&eth_dev_shared_data->ownership_lock);

	*owner_id = eth_dev_shared_data->next_owner_id++;

	rte_spinlock_unlock(&eth_dev_shared_data->ownership_lock);
	return 0;
}

static int
eth_dev_owner_set(const uint16_t port_id, const uint64_t old_owner_id,
		       const struct rte_eth_dev_owner *new_owner)
{
	struct rte_eth_dev *ethdev = &rte_eth_devices[port_id];
	struct rte_eth_dev_owner *port_owner;

	if (port_id >= RTE_MAX_ETHPORTS || !eth_dev_is_allocated(ethdev)) {
		RTE_ETHDEV_LOG(ERR, "Port id %"PRIu16" is not allocated\n",
			port_id);
		return -ENODEV;
	}

	if (!eth_is_valid_owner_id(new_owner->id) &&
	    !eth_is_valid_owner_id(old_owner_id)) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid owner old_id=%016"PRIx64" new_id=%016"PRIx64"\n",
		       old_owner_id, new_owner->id);
		return -EINVAL;
	}

	port_owner = &rte_eth_devices[port_id].data->owner;
	if (port_owner->id != old_owner_id) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot set owner to port %u already owned by %s_%016"PRIX64"\n",
			port_id, port_owner->name, port_owner->id);
		return -EPERM;
	}

	/* can not truncate (same structure) */
	strlcpy(port_owner->name, new_owner->name, RTE_ETH_MAX_OWNER_NAME_LEN);

	port_owner->id = new_owner->id;

	RTE_ETHDEV_LOG(DEBUG, "Port %u owner is %s_%016"PRIx64"\n",
		port_id, new_owner->name, new_owner->id);

	return 0;
}

int
rte_eth_dev_owner_set(const uint16_t port_id,
		      const struct rte_eth_dev_owner *owner)
{
	int ret;

	eth_dev_shared_data_prepare();

	rte_spinlock_lock(&eth_dev_shared_data->ownership_lock);

	ret = eth_dev_owner_set(port_id, RTE_ETH_DEV_NO_OWNER, owner);

	rte_spinlock_unlock(&eth_dev_shared_data->ownership_lock);
	return ret;
}

int
rte_eth_dev_owner_unset(const uint16_t port_id, const uint64_t owner_id)
{
	const struct rte_eth_dev_owner new_owner = (struct rte_eth_dev_owner)
			{.id = RTE_ETH_DEV_NO_OWNER, .name = ""};
	int ret;

	eth_dev_shared_data_prepare();

	rte_spinlock_lock(&eth_dev_shared_data->ownership_lock);

	ret = eth_dev_owner_set(port_id, owner_id, &new_owner);

	rte_spinlock_unlock(&eth_dev_shared_data->ownership_lock);
	return ret;
}

int
rte_eth_dev_owner_delete(const uint64_t owner_id)
{
	uint16_t port_id;
	int ret = 0;

	eth_dev_shared_data_prepare();

	rte_spinlock_lock(&eth_dev_shared_data->ownership_lock);

	if (eth_is_valid_owner_id(owner_id)) {
		for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++)
			if (rte_eth_devices[port_id].data->owner.id == owner_id)
				memset(&rte_eth_devices[port_id].data->owner, 0,
				       sizeof(struct rte_eth_dev_owner));
		RTE_ETHDEV_LOG(NOTICE,
			"All port owners owned by %016"PRIx64" identifier have removed\n",
			owner_id);
	} else {
		RTE_ETHDEV_LOG(ERR,
			       "Invalid owner id=%016"PRIx64"\n",
			       owner_id);
		ret = -EINVAL;
	}

	rte_spinlock_unlock(&eth_dev_shared_data->ownership_lock);

	return ret;
}

int
rte_eth_dev_owner_get(const uint16_t port_id, struct rte_eth_dev_owner *owner)
{
	int ret = 0;
	struct rte_eth_dev *ethdev = &rte_eth_devices[port_id];

	eth_dev_shared_data_prepare();

	rte_spinlock_lock(&eth_dev_shared_data->ownership_lock);

	if (port_id >= RTE_MAX_ETHPORTS || !eth_dev_is_allocated(ethdev)) {
		RTE_ETHDEV_LOG(ERR, "Port id %"PRIu16" is not allocated\n",
			port_id);
		ret = -ENODEV;
	} else {
		rte_memcpy(owner, &ethdev->data->owner, sizeof(*owner));
	}

	rte_spinlock_unlock(&eth_dev_shared_data->ownership_lock);
	return ret;
}

int
rte_eth_dev_socket_id(uint16_t port_id)
{
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -1);
	return rte_eth_devices[port_id].data->numa_node;
}

void *
rte_eth_dev_get_sec_ctx(uint16_t port_id)
{
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, NULL);
	return rte_eth_devices[port_id].security_ctx;
}

uint16_t
rte_eth_dev_count_avail(void)
{
	uint16_t p;
	uint16_t count;

	count = 0;

	RTE_ETH_FOREACH_DEV(p)
		count++;

	return count;
}

uint16_t
rte_eth_dev_count_total(void)
{
	uint16_t port, count = 0;

	RTE_ETH_FOREACH_VALID_DEV(port)
		count++;

	return count;
}

int
rte_eth_dev_get_name_by_port(uint16_t port_id, char *name)
{
	char *tmp;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (name == NULL) {
		RTE_ETHDEV_LOG(ERR, "Null pointer is specified\n");
		return -EINVAL;
	}

	/* shouldn't check 'rte_eth_devices[i].data',
	 * because it might be overwritten by VDEV PMD */
	tmp = eth_dev_shared_data->data[port_id].name;
	strcpy(name, tmp);
	return 0;
}

int
rte_eth_dev_get_port_by_name(const char *name, uint16_t *port_id)
{
	uint16_t pid;

	if (name == NULL) {
		RTE_ETHDEV_LOG(ERR, "Null pointer is specified\n");
		return -EINVAL;
	}

	RTE_ETH_FOREACH_VALID_DEV(pid)
		if (!strcmp(name, eth_dev_shared_data->data[pid].name)) {
			*port_id = pid;
			return 0;
		}

	return -ENODEV;
}

static int
eth_err(uint16_t port_id, int ret)
{
	if (ret == 0)
		return 0;
	if (rte_eth_dev_is_removed(port_id))
		return -EIO;
	return ret;
}

static int
eth_dev_rx_queue_config(struct rte_eth_dev *dev, uint16_t nb_queues)
{
	uint16_t old_nb_queues = dev->data->nb_rx_queues;
	void **rxq;
	unsigned i;

	if (dev->data->rx_queues == NULL && nb_queues != 0) { /* first time configuration */
		dev->data->rx_queues = rte_zmalloc("ethdev->rx_queues",
				sizeof(dev->data->rx_queues[0]) * nb_queues,
				RTE_CACHE_LINE_SIZE);
		if (dev->data->rx_queues == NULL) {
			dev->data->nb_rx_queues = 0;
			return -(ENOMEM);
		}
	} else if (dev->data->rx_queues != NULL && nb_queues != 0) { /* re-configure */
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_release, -ENOTSUP);

		rxq = dev->data->rx_queues;

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->rx_queue_release)(rxq[i]);
		rxq = rte_realloc(rxq, sizeof(rxq[0]) * nb_queues,
				RTE_CACHE_LINE_SIZE);
		if (rxq == NULL)
			return -(ENOMEM);
		if (nb_queues > old_nb_queues) {
			uint16_t new_qs = nb_queues - old_nb_queues;

			memset(rxq + old_nb_queues, 0,
				sizeof(rxq[0]) * new_qs);
		}

		dev->data->rx_queues = rxq;

	} else if (dev->data->rx_queues != NULL && nb_queues == 0) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_release, -ENOTSUP);

		rxq = dev->data->rx_queues;

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->rx_queue_release)(rxq[i]);

		rte_free(dev->data->rx_queues);
		dev->data->rx_queues = NULL;
	}
	dev->data->nb_rx_queues = nb_queues;
	return 0;
}

static int
eth_dev_validate_rx_queue(const struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	uint16_t port_id;

	if (rx_queue_id >= dev->data->nb_rx_queues) {
		port_id = dev->data->port_id;
		RTE_ETHDEV_LOG(ERR,
			       "Invalid Rx queue_id=%u of device with port_id=%u\n",
			       rx_queue_id, port_id);
		return -EINVAL;
	}

	if (dev->data->rx_queues[rx_queue_id] == NULL) {
		port_id = dev->data->port_id;
		RTE_ETHDEV_LOG(ERR,
			       "Queue %u of device with port_id=%u has not been setup\n",
			       rx_queue_id, port_id);
		return -EINVAL;
	}

	return 0;
}

static int
eth_dev_validate_tx_queue(const struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	uint16_t port_id;

	if (tx_queue_id >= dev->data->nb_tx_queues) {
		port_id = dev->data->port_id;
		RTE_ETHDEV_LOG(ERR,
			       "Invalid Tx queue_id=%u of device with port_id=%u\n",
			       tx_queue_id, port_id);
		return -EINVAL;
	}

	if (dev->data->tx_queues[tx_queue_id] == NULL) {
		port_id = dev->data->port_id;
		RTE_ETHDEV_LOG(ERR,
			       "Queue %u of device with port_id=%u has not been setup\n",
			       tx_queue_id, port_id);
		return -EINVAL;
	}

	return 0;
}

int
rte_eth_dev_rx_queue_start(uint16_t port_id, uint16_t rx_queue_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!dev->data->dev_started) {
		RTE_ETHDEV_LOG(ERR,
			"Port %u must be started before start any queue\n",
			port_id);
		return -EINVAL;
	}

	ret = eth_dev_validate_rx_queue(dev, rx_queue_id);
	if (ret != 0)
		return ret;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_start, -ENOTSUP);

	if (rte_eth_dev_is_rx_hairpin_queue(dev, rx_queue_id)) {
		RTE_ETHDEV_LOG(INFO,
			"Can't start Rx hairpin queue %"PRIu16" of device with port_id=%"PRIu16"\n",
			rx_queue_id, port_id);
		return -EINVAL;
	}

	if (dev->data->rx_queue_state[rx_queue_id] != RTE_ETH_QUEUE_STATE_STOPPED) {
		RTE_ETHDEV_LOG(INFO,
			"Queue %"PRIu16" of device with port_id=%"PRIu16" already started\n",
			rx_queue_id, port_id);
		return 0;
	}

	return eth_err(port_id, dev->dev_ops->rx_queue_start(dev,
							     rx_queue_id));

}

int
rte_eth_dev_rx_queue_stop(uint16_t port_id, uint16_t rx_queue_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	ret = eth_dev_validate_rx_queue(dev, rx_queue_id);
	if (ret != 0)
		return ret;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_stop, -ENOTSUP);

	if (rte_eth_dev_is_rx_hairpin_queue(dev, rx_queue_id)) {
		RTE_ETHDEV_LOG(INFO,
			"Can't stop Rx hairpin queue %"PRIu16" of device with port_id=%"PRIu16"\n",
			rx_queue_id, port_id);
		return -EINVAL;
	}

	if (dev->data->rx_queue_state[rx_queue_id] == RTE_ETH_QUEUE_STATE_STOPPED) {
		RTE_ETHDEV_LOG(INFO,
			"Queue %"PRIu16" of device with port_id=%"PRIu16" already stopped\n",
			rx_queue_id, port_id);
		return 0;
	}

	return eth_err(port_id, dev->dev_ops->rx_queue_stop(dev, rx_queue_id));

}

int
rte_eth_dev_tx_queue_start(uint16_t port_id, uint16_t tx_queue_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!dev->data->dev_started) {
		RTE_ETHDEV_LOG(ERR,
			"Port %u must be started before start any queue\n",
			port_id);
		return -EINVAL;
	}

	ret = eth_dev_validate_tx_queue(dev, tx_queue_id);
	if (ret != 0)
		return ret;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_start, -ENOTSUP);

	if (rte_eth_dev_is_tx_hairpin_queue(dev, tx_queue_id)) {
		RTE_ETHDEV_LOG(INFO,
			"Can't start Tx hairpin queue %"PRIu16" of device with port_id=%"PRIu16"\n",
			tx_queue_id, port_id);
		return -EINVAL;
	}

	if (dev->data->tx_queue_state[tx_queue_id] != RTE_ETH_QUEUE_STATE_STOPPED) {
		RTE_ETHDEV_LOG(INFO,
			"Queue %"PRIu16" of device with port_id=%"PRIu16" already started\n",
			tx_queue_id, port_id);
		return 0;
	}

	return eth_err(port_id, dev->dev_ops->tx_queue_start(dev, tx_queue_id));
}

int
rte_eth_dev_tx_queue_stop(uint16_t port_id, uint16_t tx_queue_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	ret = eth_dev_validate_tx_queue(dev, tx_queue_id);
	if (ret != 0)
		return ret;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_stop, -ENOTSUP);

	if (rte_eth_dev_is_tx_hairpin_queue(dev, tx_queue_id)) {
		RTE_ETHDEV_LOG(INFO,
			"Can't stop Tx hairpin queue %"PRIu16" of device with port_id=%"PRIu16"\n",
			tx_queue_id, port_id);
		return -EINVAL;
	}

	if (dev->data->tx_queue_state[tx_queue_id] == RTE_ETH_QUEUE_STATE_STOPPED) {
		RTE_ETHDEV_LOG(INFO,
			"Queue %"PRIu16" of device with port_id=%"PRIu16" already stopped\n",
			tx_queue_id, port_id);
		return 0;
	}

	return eth_err(port_id, dev->dev_ops->tx_queue_stop(dev, tx_queue_id));

}

static int
eth_dev_tx_queue_config(struct rte_eth_dev *dev, uint16_t nb_queues)
{
	uint16_t old_nb_queues = dev->data->nb_tx_queues;
	void **txq;
	unsigned i;

	if (dev->data->tx_queues == NULL && nb_queues != 0) { /* first time configuration */
		dev->data->tx_queues = rte_zmalloc("ethdev->tx_queues",
						   sizeof(dev->data->tx_queues[0]) * nb_queues,
						   RTE_CACHE_LINE_SIZE);
		if (dev->data->tx_queues == NULL) {
			dev->data->nb_tx_queues = 0;
			return -(ENOMEM);
		}
	} else if (dev->data->tx_queues != NULL && nb_queues != 0) { /* re-configure */
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_release, -ENOTSUP);

		txq = dev->data->tx_queues;

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->tx_queue_release)(txq[i]);
		txq = rte_realloc(txq, sizeof(txq[0]) * nb_queues,
				  RTE_CACHE_LINE_SIZE);
		if (txq == NULL)
			return -ENOMEM;
		if (nb_queues > old_nb_queues) {
			uint16_t new_qs = nb_queues - old_nb_queues;

			memset(txq + old_nb_queues, 0,
			       sizeof(txq[0]) * new_qs);
		}

		dev->data->tx_queues = txq;

	} else if (dev->data->tx_queues != NULL && nb_queues == 0) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_release, -ENOTSUP);

		txq = dev->data->tx_queues;

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->tx_queue_release)(txq[i]);

		rte_free(dev->data->tx_queues);
		dev->data->tx_queues = NULL;
	}
	dev->data->nb_tx_queues = nb_queues;
	return 0;
}

uint32_t
rte_eth_speed_bitflag(uint32_t speed, int duplex)
{
	switch (speed) {
	case ETH_SPEED_NUM_10M:
		return duplex ? ETH_LINK_SPEED_10M : ETH_LINK_SPEED_10M_HD;
	case ETH_SPEED_NUM_100M:
		return duplex ? ETH_LINK_SPEED_100M : ETH_LINK_SPEED_100M_HD;
	case ETH_SPEED_NUM_1G:
		return ETH_LINK_SPEED_1G;
	case ETH_SPEED_NUM_2_5G:
		return ETH_LINK_SPEED_2_5G;
	case ETH_SPEED_NUM_5G:
		return ETH_LINK_SPEED_5G;
	case ETH_SPEED_NUM_10G:
		return ETH_LINK_SPEED_10G;
	case ETH_SPEED_NUM_20G:
		return ETH_LINK_SPEED_20G;
	case ETH_SPEED_NUM_25G:
		return ETH_LINK_SPEED_25G;
	case ETH_SPEED_NUM_40G:
		return ETH_LINK_SPEED_40G;
	case ETH_SPEED_NUM_50G:
		return ETH_LINK_SPEED_50G;
	case ETH_SPEED_NUM_56G:
		return ETH_LINK_SPEED_56G;
	case ETH_SPEED_NUM_100G:
		return ETH_LINK_SPEED_100G;
	case ETH_SPEED_NUM_200G:
		return ETH_LINK_SPEED_200G;
	default:
		return 0;
	}
}

const char *
rte_eth_dev_rx_offload_name(uint64_t offload)
{
	const char *name = "UNKNOWN";
	unsigned int i;

	for (i = 0; i < RTE_DIM(eth_dev_rx_offload_names); ++i) {
		if (offload == eth_dev_rx_offload_names[i].offload) {
			name = eth_dev_rx_offload_names[i].name;
			break;
		}
	}

	return name;
}

const char *
rte_eth_dev_tx_offload_name(uint64_t offload)
{
	const char *name = "UNKNOWN";
	unsigned int i;

	for (i = 0; i < RTE_DIM(eth_dev_tx_offload_names); ++i) {
		if (offload == eth_dev_tx_offload_names[i].offload) {
			name = eth_dev_tx_offload_names[i].name;
			break;
		}
	}

	return name;
}

static inline int
eth_dev_check_lro_pkt_size(uint16_t port_id, uint32_t config_size,
		   uint32_t max_rx_pkt_len, uint32_t dev_info_size)
{
	int ret = 0;

	if (dev_info_size == 0) {
		if (config_size != max_rx_pkt_len) {
			RTE_ETHDEV_LOG(ERR, "Ethdev port_id=%d max_lro_pkt_size"
				       " %u != %u is not allowed\n",
				       port_id, config_size, max_rx_pkt_len);
			ret = -EINVAL;
		}
	} else if (config_size > dev_info_size) {
		RTE_ETHDEV_LOG(ERR, "Ethdev port_id=%d max_lro_pkt_size %u "
			       "> max allowed value %u\n", port_id, config_size,
			       dev_info_size);
		ret = -EINVAL;
	} else if (config_size < RTE_ETHER_MIN_LEN) {
		RTE_ETHDEV_LOG(ERR, "Ethdev port_id=%d max_lro_pkt_size %u "
			       "< min allowed value %u\n", port_id, config_size,
			       (unsigned int)RTE_ETHER_MIN_LEN);
		ret = -EINVAL;
	}
	return ret;
}

/*
 * Validate offloads that are requested through rte_eth_dev_configure against
 * the offloads successfully set by the ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param req_offloads
 *   The offloads that have been requested through `rte_eth_dev_configure`.
 * @param set_offloads
 *   The offloads successfully set by the ethernet device.
 * @param offload_type
 *   The offload type i.e. Rx/Tx string.
 * @param offload_name
 *   The function that prints the offload name.
 * @return
 *   - (0) if validation successful.
 *   - (-EINVAL) if requested offload has been silently disabled.
 *
 */
static int
eth_dev_validate_offloads(uint16_t port_id, uint64_t req_offloads,
		  uint64_t set_offloads, const char *offload_type,
		  const char *(*offload_name)(uint64_t))
{
	uint64_t offloads_diff = req_offloads ^ set_offloads;
	uint64_t offload;
	int ret = 0;

	while (offloads_diff != 0) {
		/* Check if any offload is requested but not enabled. */
		offload = 1ULL << __builtin_ctzll(offloads_diff);
		if (offload & req_offloads) {
			RTE_ETHDEV_LOG(ERR,
				"Port %u failed to enable %s offload %s\n",
				port_id, offload_type, offload_name(offload));
			ret = -EINVAL;
		}

		/* Check if offload couldn't be disabled. */
		if (offload & set_offloads) {
			RTE_ETHDEV_LOG(DEBUG,
				"Port %u %s offload %s is not requested but enabled\n",
				port_id, offload_type, offload_name(offload));
		}

		offloads_diff &= ~offload;
	}

	return ret;
}

int
rte_eth_dev_configure(uint16_t port_id, uint16_t nb_rx_q, uint16_t nb_tx_q,
		      const struct rte_eth_conf *dev_conf)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_conf orig_conf;
	uint16_t overhead_len;
	int diag;
	int ret;
	uint16_t old_mtu;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_configure, -ENOTSUP);

	if (dev->data->dev_started) {
		RTE_ETHDEV_LOG(ERR,
			"Port %u must be stopped to allow configuration\n",
			port_id);
		return -EBUSY;
	}

	 /* Store original config, as rollback required on failure */
	memcpy(&orig_conf, &dev->data->dev_conf, sizeof(dev->data->dev_conf));

	/*
	 * Copy the dev_conf parameter into the dev structure.
	 * rte_eth_dev_info_get() requires dev_conf, copy it before dev_info get
	 */
	if (dev_conf != &dev->data->dev_conf)
		memcpy(&dev->data->dev_conf, dev_conf,
		       sizeof(dev->data->dev_conf));

	/* Backup mtu for rollback */
	old_mtu = dev->data->mtu;

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		goto rollback;

	/* Get the real Ethernet overhead length */
	if (dev_info.max_mtu != UINT16_MAX &&
	    dev_info.max_rx_pktlen > dev_info.max_mtu)
		overhead_len = dev_info.max_rx_pktlen - dev_info.max_mtu;
	else
		overhead_len = RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN;

	/* If number of queues specified by application for both Rx and Tx is
	 * zero, use driver preferred values. This cannot be done individually
	 * as it is valid for either Tx or Rx (but not both) to be zero.
	 * If driver does not provide any preferred valued, fall back on
	 * EAL defaults.
	 */
	if (nb_rx_q == 0 && nb_tx_q == 0) {
		nb_rx_q = dev_info.default_rxportconf.nb_queues;
		if (nb_rx_q == 0)
			nb_rx_q = RTE_ETH_DEV_FALLBACK_RX_NBQUEUES;
		nb_tx_q = dev_info.default_txportconf.nb_queues;
		if (nb_tx_q == 0)
			nb_tx_q = RTE_ETH_DEV_FALLBACK_TX_NBQUEUES;
	}

	if (nb_rx_q > RTE_MAX_QUEUES_PER_PORT) {
		RTE_ETHDEV_LOG(ERR,
			"Number of RX queues requested (%u) is greater than max supported(%d)\n",
			nb_rx_q, RTE_MAX_QUEUES_PER_PORT);
		ret = -EINVAL;
		goto rollback;
	}

	if (nb_tx_q > RTE_MAX_QUEUES_PER_PORT) {
		RTE_ETHDEV_LOG(ERR,
			"Number of TX queues requested (%u) is greater than max supported(%d)\n",
			nb_tx_q, RTE_MAX_QUEUES_PER_PORT);
		ret = -EINVAL;
		goto rollback;
	}

	/*
	 * Check that the numbers of RX and TX queues are not greater
	 * than the maximum number of RX and TX queues supported by the
	 * configured device.
	 */
	if (nb_rx_q > dev_info.max_rx_queues) {
		RTE_ETHDEV_LOG(ERR, "Ethdev port_id=%u nb_rx_queues=%u > %u\n",
			port_id, nb_rx_q, dev_info.max_rx_queues);
		ret = -EINVAL;
		goto rollback;
	}

	if (nb_tx_q > dev_info.max_tx_queues) {
		RTE_ETHDEV_LOG(ERR, "Ethdev port_id=%u nb_tx_queues=%u > %u\n",
			port_id, nb_tx_q, dev_info.max_tx_queues);
		ret = -EINVAL;
		goto rollback;
	}

	/* Check that the device supports requested interrupts */
	if ((dev_conf->intr_conf.lsc == 1) &&
			(!(dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC))) {
		RTE_ETHDEV_LOG(ERR, "Driver %s does not support lsc\n",
			dev->device->driver->name);
		ret = -EINVAL;
		goto rollback;
	}
	if ((dev_conf->intr_conf.rmv == 1) &&
			(!(dev->data->dev_flags & RTE_ETH_DEV_INTR_RMV))) {
		RTE_ETHDEV_LOG(ERR, "Driver %s does not support rmv\n",
			dev->device->driver->name);
		ret = -EINVAL;
		goto rollback;
	}

	/*
	 * If jumbo frames are enabled, check that the maximum RX packet
	 * length is supported by the configured device.
	 */
	if (dev_conf->rxmode.offloads & DEV_RX_OFFLOAD_JUMBO_FRAME) {
		if (dev_conf->rxmode.max_rx_pkt_len > dev_info.max_rx_pktlen) {
			RTE_ETHDEV_LOG(ERR,
				"Ethdev port_id=%u max_rx_pkt_len %u > max valid value %u\n",
				port_id, dev_conf->rxmode.max_rx_pkt_len,
				dev_info.max_rx_pktlen);
			ret = -EINVAL;
			goto rollback;
		} else if (dev_conf->rxmode.max_rx_pkt_len < RTE_ETHER_MIN_LEN) {
			RTE_ETHDEV_LOG(ERR,
				"Ethdev port_id=%u max_rx_pkt_len %u < min valid value %u\n",
				port_id, dev_conf->rxmode.max_rx_pkt_len,
				(unsigned int)RTE_ETHER_MIN_LEN);
			ret = -EINVAL;
			goto rollback;
		}

		/* Scale the MTU size to adapt max_rx_pkt_len */
		dev->data->mtu = dev->data->dev_conf.rxmode.max_rx_pkt_len -
				overhead_len;
	} else {
		uint16_t pktlen = dev_conf->rxmode.max_rx_pkt_len;
		if (pktlen < RTE_ETHER_MIN_MTU + overhead_len ||
		    pktlen > RTE_ETHER_MTU + overhead_len)
			/* Use default value */
			dev->data->dev_conf.rxmode.max_rx_pkt_len =
						RTE_ETHER_MTU + overhead_len;
	}

	/*
	 * If LRO is enabled, check that the maximum aggregated packet
	 * size is supported by the configured device.
	 */
	if (dev_conf->rxmode.offloads & DEV_RX_OFFLOAD_TCP_LRO) {
		if (dev_conf->rxmode.max_lro_pkt_size == 0)
			dev->data->dev_conf.rxmode.max_lro_pkt_size =
				dev->data->dev_conf.rxmode.max_rx_pkt_len;
		ret = eth_dev_check_lro_pkt_size(port_id,
				dev->data->dev_conf.rxmode.max_lro_pkt_size,
				dev->data->dev_conf.rxmode.max_rx_pkt_len,
				dev_info.max_lro_pkt_size);
		if (ret != 0)
			goto rollback;
	}

	/* Any requested offloading must be within its device capabilities */
	if ((dev_conf->rxmode.offloads & dev_info.rx_offload_capa) !=
	     dev_conf->rxmode.offloads) {
		RTE_ETHDEV_LOG(ERR,
			"Ethdev port_id=%u requested Rx offloads 0x%"PRIx64" doesn't match Rx offloads "
			"capabilities 0x%"PRIx64" in %s()\n",
			port_id, dev_conf->rxmode.offloads,
			dev_info.rx_offload_capa,
			__func__);
		ret = -EINVAL;
		goto rollback;
	}
	if ((dev_conf->txmode.offloads & dev_info.tx_offload_capa) !=
	     dev_conf->txmode.offloads) {
		RTE_ETHDEV_LOG(ERR,
			"Ethdev port_id=%u requested Tx offloads 0x%"PRIx64" doesn't match Tx offloads "
			"capabilities 0x%"PRIx64" in %s()\n",
			port_id, dev_conf->txmode.offloads,
			dev_info.tx_offload_capa,
			__func__);
		ret = -EINVAL;
		goto rollback;
	}

	dev->data->dev_conf.rx_adv_conf.rss_conf.rss_hf =
		rte_eth_rss_hf_refine(dev_conf->rx_adv_conf.rss_conf.rss_hf);

	/* Check that device supports requested rss hash functions. */
	if ((dev_info.flow_type_rss_offloads |
	     dev_conf->rx_adv_conf.rss_conf.rss_hf) !=
	    dev_info.flow_type_rss_offloads) {
		RTE_ETHDEV_LOG(ERR,
			"Ethdev port_id=%u invalid rss_hf: 0x%"PRIx64", valid value: 0x%"PRIx64"\n",
			port_id, dev_conf->rx_adv_conf.rss_conf.rss_hf,
			dev_info.flow_type_rss_offloads);
		ret = -EINVAL;
		goto rollback;
	}

	/* Check if Rx RSS distribution is disabled but RSS hash is enabled. */
	if (((dev_conf->rxmode.mq_mode & ETH_MQ_RX_RSS_FLAG) == 0) &&
	    (dev_conf->rxmode.offloads & DEV_RX_OFFLOAD_RSS_HASH)) {
		RTE_ETHDEV_LOG(ERR,
			"Ethdev port_id=%u config invalid Rx mq_mode without RSS but %s offload is requested\n",
			port_id,
			rte_eth_dev_rx_offload_name(DEV_RX_OFFLOAD_RSS_HASH));
		ret = -EINVAL;
		goto rollback;
	}

	/*
	 * Setup new number of RX/TX queues and reconfigure device.
	 */
	diag = eth_dev_rx_queue_config(dev, nb_rx_q);
	if (diag != 0) {
		RTE_ETHDEV_LOG(ERR,
			"Port%u eth_dev_rx_queue_config = %d\n",
			port_id, diag);
		ret = diag;
		goto rollback;
	}

	diag = eth_dev_tx_queue_config(dev, nb_tx_q);
	if (diag != 0) {
		RTE_ETHDEV_LOG(ERR,
			"Port%u eth_dev_tx_queue_config = %d\n",
			port_id, diag);
		eth_dev_rx_queue_config(dev, 0);
		ret = diag;
		goto rollback;
	}

	diag = (*dev->dev_ops->dev_configure)(dev);
	if (diag != 0) {
		RTE_ETHDEV_LOG(ERR, "Port%u dev_configure = %d\n",
			port_id, diag);
		ret = eth_err(port_id, diag);
		goto reset_queues;
	}

	/* Initialize Rx profiling if enabled at compilation time. */
	diag = __rte_eth_dev_profile_init(port_id, dev);
	if (diag != 0) {
		RTE_ETHDEV_LOG(ERR, "Port%u __rte_eth_dev_profile_init = %d\n",
			port_id, diag);
		ret = eth_err(port_id, diag);
		goto reset_queues;
	}

	/* Validate Rx offloads. */
	diag = eth_dev_validate_offloads(port_id,
			dev_conf->rxmode.offloads,
			dev->data->dev_conf.rxmode.offloads, "Rx",
			rte_eth_dev_rx_offload_name);
	if (diag != 0) {
		ret = diag;
		goto reset_queues;
	}

	/* Validate Tx offloads. */
	diag = eth_dev_validate_offloads(port_id,
			dev_conf->txmode.offloads,
			dev->data->dev_conf.txmode.offloads, "Tx",
			rte_eth_dev_tx_offload_name);
	if (diag != 0) {
		ret = diag;
		goto reset_queues;
	}

	rte_ethdev_trace_configure(port_id, nb_rx_q, nb_tx_q, dev_conf, 0);
	return 0;
reset_queues:
	eth_dev_rx_queue_config(dev, 0);
	eth_dev_tx_queue_config(dev, 0);
rollback:
	memcpy(&dev->data->dev_conf, &orig_conf, sizeof(dev->data->dev_conf));
	if (old_mtu != dev->data->mtu)
		dev->data->mtu = old_mtu;

	rte_ethdev_trace_configure(port_id, nb_rx_q, nb_tx_q, dev_conf, ret);
	return ret;
}

void
rte_eth_dev_internal_reset(struct rte_eth_dev *dev)
{
	if (dev->data->dev_started) {
		RTE_ETHDEV_LOG(ERR, "Port %u must be stopped to allow reset\n",
			dev->data->port_id);
		return;
	}

	eth_dev_rx_queue_config(dev, 0);
	eth_dev_tx_queue_config(dev, 0);

	memset(&dev->data->dev_conf, 0, sizeof(dev->data->dev_conf));
}

static void
eth_dev_mac_restore(struct rte_eth_dev *dev,
			struct rte_eth_dev_info *dev_info)
{
	struct rte_ether_addr *addr;
	uint16_t i;
	uint32_t pool = 0;
	uint64_t pool_mask;

	/* replay MAC address configuration including default MAC */
	addr = &dev->data->mac_addrs[0];
	if (*dev->dev_ops->mac_addr_set != NULL)
		(*dev->dev_ops->mac_addr_set)(dev, addr);
	else if (*dev->dev_ops->mac_addr_add != NULL)
		(*dev->dev_ops->mac_addr_add)(dev, addr, 0, pool);

	if (*dev->dev_ops->mac_addr_add != NULL) {
		for (i = 1; i < dev_info->max_mac_addrs; i++) {
			addr = &dev->data->mac_addrs[i];

			/* skip zero address */
			if (rte_is_zero_ether_addr(addr))
				continue;

			pool = 0;
			pool_mask = dev->data->mac_pool_sel[i];

			do {
				if (pool_mask & 1ULL)
					(*dev->dev_ops->mac_addr_add)(dev,
						addr, i, pool);
				pool_mask >>= 1;
				pool++;
			} while (pool_mask);
		}
	}
}

static int
eth_dev_config_restore(struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info, uint16_t port_id)
{
	int ret;

	if (!(*dev_info->dev_flags & RTE_ETH_DEV_NOLIVE_MAC_ADDR))
		eth_dev_mac_restore(dev, dev_info);

	/* replay promiscuous configuration */
	/*
	 * use callbacks directly since we don't need port_id check and
	 * would like to bypass the same value set
	 */
	if (rte_eth_promiscuous_get(port_id) == 1 &&
	    *dev->dev_ops->promiscuous_enable != NULL) {
		ret = eth_err(port_id,
			      (*dev->dev_ops->promiscuous_enable)(dev));
		if (ret != 0 && ret != -ENOTSUP) {
			RTE_ETHDEV_LOG(ERR,
				"Failed to enable promiscuous mode for device (port %u): %s\n",
				port_id, rte_strerror(-ret));
			return ret;
		}
	} else if (rte_eth_promiscuous_get(port_id) == 0 &&
		   *dev->dev_ops->promiscuous_disable != NULL) {
		ret = eth_err(port_id,
			      (*dev->dev_ops->promiscuous_disable)(dev));
		if (ret != 0 && ret != -ENOTSUP) {
			RTE_ETHDEV_LOG(ERR,
				"Failed to disable promiscuous mode for device (port %u): %s\n",
				port_id, rte_strerror(-ret));
			return ret;
		}
	}

	/* replay all multicast configuration */
	/*
	 * use callbacks directly since we don't need port_id check and
	 * would like to bypass the same value set
	 */
	if (rte_eth_allmulticast_get(port_id) == 1 &&
	    *dev->dev_ops->allmulticast_enable != NULL) {
		ret = eth_err(port_id,
			      (*dev->dev_ops->allmulticast_enable)(dev));
		if (ret != 0 && ret != -ENOTSUP) {
			RTE_ETHDEV_LOG(ERR,
				"Failed to enable allmulticast mode for device (port %u): %s\n",
				port_id, rte_strerror(-ret));
			return ret;
		}
	} else if (rte_eth_allmulticast_get(port_id) == 0 &&
		   *dev->dev_ops->allmulticast_disable != NULL) {
		ret = eth_err(port_id,
			      (*dev->dev_ops->allmulticast_disable)(dev));
		if (ret != 0 && ret != -ENOTSUP) {
			RTE_ETHDEV_LOG(ERR,
				"Failed to disable allmulticast mode for device (port %u): %s\n",
				port_id, rte_strerror(-ret));
			return ret;
		}
	}

	return 0;
}

int
rte_eth_dev_start(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	int diag;
	int ret, ret_stop;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_start, -ENOTSUP);

	if (dev->data->dev_started != 0) {
		RTE_ETHDEV_LOG(INFO,
			"Device with port_id=%"PRIu16" already started\n",
			port_id);
		return 0;
	}

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	/* Lets restore MAC now if device does not support live change */
	if (*dev_info.dev_flags & RTE_ETH_DEV_NOLIVE_MAC_ADDR)
		eth_dev_mac_restore(dev, &dev_info);

	diag = (*dev->dev_ops->dev_start)(dev);
	if (diag == 0)
		dev->data->dev_started = 1;
	else
		return eth_err(port_id, diag);

	ret = eth_dev_config_restore(dev, &dev_info, port_id);
	if (ret != 0) {
		RTE_ETHDEV_LOG(ERR,
			"Error during restoring configuration for device (port %u): %s\n",
			port_id, rte_strerror(-ret));
		ret_stop = rte_eth_dev_stop(port_id);
		if (ret_stop != 0) {
			RTE_ETHDEV_LOG(ERR,
				"Failed to stop device (port %u): %s\n",
				port_id, rte_strerror(-ret_stop));
		}

		return ret;
	}

	if (dev->data->dev_conf.intr_conf.lsc == 0) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->link_update, -ENOTSUP);
		(*dev->dev_ops->link_update)(dev, 0);
	}

	rte_ethdev_trace_start(port_id);
	return 0;
}

int
rte_eth_dev_stop(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_stop, -ENOTSUP);

	if (dev->data->dev_started == 0) {
		RTE_ETHDEV_LOG(INFO,
			"Device with port_id=%"PRIu16" already stopped\n",
			port_id);
		return 0;
	}

	dev->data->dev_started = 0;
	ret = (*dev->dev_ops->dev_stop)(dev);
	rte_ethdev_trace_stop(port_id, ret);

	return ret;
}

int
rte_eth_dev_set_link_up(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_set_link_up, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->dev_set_link_up)(dev));
}

int
rte_eth_dev_set_link_down(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_set_link_down, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->dev_set_link_down)(dev));
}

int
rte_eth_dev_close(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int firsterr, binerr;
	int *lasterr = &firsterr;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_close, -ENOTSUP);
	*lasterr = (*dev->dev_ops->dev_close)(dev);
	if (*lasterr != 0)
		lasterr = &binerr;

	rte_ethdev_trace_close(port_id);
	*lasterr = rte_eth_dev_release_port(dev);

	return firsterr;
}

int
rte_eth_dev_reset(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_reset, -ENOTSUP);

	ret = rte_eth_dev_stop(port_id);
	if (ret != 0) {
		RTE_ETHDEV_LOG(ERR,
			"Failed to stop device (port %u) before reset: %s - ignore\n",
			port_id, rte_strerror(-ret));
	}
	ret = dev->dev_ops->dev_reset(dev);

	return eth_err(port_id, ret);
}

int
rte_eth_dev_is_removed(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, 0);

	dev = &rte_eth_devices[port_id];

	if (dev->state == RTE_ETH_DEV_REMOVED)
		return 1;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->is_removed, 0);

	ret = dev->dev_ops->is_removed(dev);
	if (ret != 0)
		/* Device is physically removed. */
		dev->state = RTE_ETH_DEV_REMOVED;

	return ret;
}

static int
rte_eth_rx_queue_check_split(const struct rte_eth_rxseg_split *rx_seg,
			     uint16_t n_seg, uint32_t *mbp_buf_size,
			     const struct rte_eth_dev_info *dev_info)
{
	const struct rte_eth_rxseg_capa *seg_capa = &dev_info->rx_seg_capa;
	struct rte_mempool *mp_first;
	uint32_t offset_mask;
	uint16_t seg_idx;

	if (n_seg > seg_capa->max_nseg) {
		RTE_ETHDEV_LOG(ERR,
			       "Requested Rx segments %u exceed supported %u\n",
			       n_seg, seg_capa->max_nseg);
		return -EINVAL;
	}
	/*
	 * Check the sizes and offsets against buffer sizes
	 * for each segment specified in extended configuration.
	 */
	mp_first = rx_seg[0].mp;
	offset_mask = (1u << seg_capa->offset_align_log2) - 1;
	for (seg_idx = 0; seg_idx < n_seg; seg_idx++) {
		struct rte_mempool *mpl = rx_seg[seg_idx].mp;
		uint32_t length = rx_seg[seg_idx].length;
		uint32_t offset = rx_seg[seg_idx].offset;

		if (mpl == NULL) {
			RTE_ETHDEV_LOG(ERR, "null mempool pointer\n");
			return -EINVAL;
		}
		if (seg_idx != 0 && mp_first != mpl &&
		    seg_capa->multi_pools == 0) {
			RTE_ETHDEV_LOG(ERR, "Receiving to multiple pools is not supported\n");
			return -ENOTSUP;
		}
		if (offset != 0) {
			if (seg_capa->offset_allowed == 0) {
				RTE_ETHDEV_LOG(ERR, "Rx segmentation with offset is not supported\n");
				return -ENOTSUP;
			}
			if (offset & offset_mask) {
				RTE_ETHDEV_LOG(ERR, "Rx segmentation invalid offset alignment %u, %u\n",
					       offset,
					       seg_capa->offset_align_log2);
				return -EINVAL;
			}
		}
		if (mpl->private_data_size <
			sizeof(struct rte_pktmbuf_pool_private)) {
			RTE_ETHDEV_LOG(ERR,
				       "%s private_data_size %u < %u\n",
				       mpl->name, mpl->private_data_size,
				       (unsigned int)sizeof
					(struct rte_pktmbuf_pool_private));
			return -ENOSPC;
		}
		offset += seg_idx != 0 ? 0 : RTE_PKTMBUF_HEADROOM;
		*mbp_buf_size = rte_pktmbuf_data_room_size(mpl);
		length = length != 0 ? length : *mbp_buf_size;
		if (*mbp_buf_size < length + offset) {
			RTE_ETHDEV_LOG(ERR,
				       "%s mbuf_data_room_size %u < %u (segment length=%u + segment offset=%u)\n",
				       mpl->name, *mbp_buf_size,
				       length + offset, length, offset);
			return -EINVAL;
		}
	}
	return 0;
}

int
rte_eth_rx_queue_setup(uint16_t port_id, uint16_t rx_queue_id,
		       uint16_t nb_rx_desc, unsigned int socket_id,
		       const struct rte_eth_rxconf *rx_conf,
		       struct rte_mempool *mp)
{
	int ret;
	uint32_t mbp_buf_size;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_rxconf local_conf;
	void **rxq;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid RX queue_id=%u\n", rx_queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_setup, -ENOTSUP);

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	if (mp != NULL) {
		/* Single pool configuration check. */
		if (rx_conf != NULL && rx_conf->rx_nseg != 0) {
			RTE_ETHDEV_LOG(ERR,
				       "Ambiguous segment configuration\n");
			return -EINVAL;
		}
		/*
		 * Check the size of the mbuf data buffer, this value
		 * must be provided in the private data of the memory pool.
		 * First check that the memory pool(s) has a valid private data.
		 */
		if (mp->private_data_size <
				sizeof(struct rte_pktmbuf_pool_private)) {
			RTE_ETHDEV_LOG(ERR, "%s private_data_size %u < %u\n",
				mp->name, mp->private_data_size,
				(unsigned int)
				sizeof(struct rte_pktmbuf_pool_private));
			return -ENOSPC;
		}
		mbp_buf_size = rte_pktmbuf_data_room_size(mp);
		if (mbp_buf_size < dev_info.min_rx_bufsize +
				   RTE_PKTMBUF_HEADROOM) {
			RTE_ETHDEV_LOG(ERR,
				       "%s mbuf_data_room_size %u < %u (RTE_PKTMBUF_HEADROOM=%u + min_rx_bufsize(dev)=%u)\n",
				       mp->name, mbp_buf_size,
				       RTE_PKTMBUF_HEADROOM +
				       dev_info.min_rx_bufsize,
				       RTE_PKTMBUF_HEADROOM,
				       dev_info.min_rx_bufsize);
			return -EINVAL;
		}
	} else {
		const struct rte_eth_rxseg_split *rx_seg;
		uint16_t n_seg;

		/* Extended multi-segment configuration check. */
		if (rx_conf == NULL || rx_conf->rx_seg == NULL || rx_conf->rx_nseg == 0) {
			RTE_ETHDEV_LOG(ERR,
				       "Memory pool is null and no extended configuration provided\n");
			return -EINVAL;
		}

		rx_seg = (const struct rte_eth_rxseg_split *)rx_conf->rx_seg;
		n_seg = rx_conf->rx_nseg;

		if (rx_conf->offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT) {
			ret = rte_eth_rx_queue_check_split(rx_seg, n_seg,
							   &mbp_buf_size,
							   &dev_info);
			if (ret != 0)
				return ret;
		} else {
			RTE_ETHDEV_LOG(ERR, "No Rx segmentation offload configured\n");
			return -EINVAL;
		}
	}

	/* Use default specified by driver, if nb_rx_desc is zero */
	if (nb_rx_desc == 0) {
		nb_rx_desc = dev_info.default_rxportconf.ring_size;
		/* If driver default is also zero, fall back on EAL default */
		if (nb_rx_desc == 0)
			nb_rx_desc = RTE_ETH_DEV_FALLBACK_RX_RINGSIZE;
	}

	if (nb_rx_desc > dev_info.rx_desc_lim.nb_max ||
			nb_rx_desc < dev_info.rx_desc_lim.nb_min ||
			nb_rx_desc % dev_info.rx_desc_lim.nb_align != 0) {

		RTE_ETHDEV_LOG(ERR,
			"Invalid value for nb_rx_desc(=%hu), should be: <= %hu, >= %hu, and a product of %hu\n",
			nb_rx_desc, dev_info.rx_desc_lim.nb_max,
			dev_info.rx_desc_lim.nb_min,
			dev_info.rx_desc_lim.nb_align);
		return -EINVAL;
	}

	if (dev->data->dev_started &&
		!(dev_info.dev_capa &
			RTE_ETH_DEV_CAPA_RUNTIME_RX_QUEUE_SETUP))
		return -EBUSY;

	if (dev->data->dev_started &&
		(dev->data->rx_queue_state[rx_queue_id] !=
			RTE_ETH_QUEUE_STATE_STOPPED))
		return -EBUSY;

	rxq = dev->data->rx_queues;
	if (rxq[rx_queue_id]) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_release,
					-ENOTSUP);
		(*dev->dev_ops->rx_queue_release)(rxq[rx_queue_id]);
		rxq[rx_queue_id] = NULL;
	}

	if (rx_conf == NULL)
		rx_conf = &dev_info.default_rxconf;

	local_conf = *rx_conf;

	/*
	 * If an offloading has already been enabled in
	 * rte_eth_dev_configure(), it has been enabled on all queues,
	 * so there is no need to enable it in this queue again.
	 * The local_conf.offloads input to underlying PMD only carries
	 * those offloadings which are only enabled on this queue and
	 * not enabled on all queues.
	 */
	local_conf.offloads &= ~dev->data->dev_conf.rxmode.offloads;

	/*
	 * New added offloadings for this queue are those not enabled in
	 * rte_eth_dev_configure() and they must be per-queue type.
	 * A pure per-port offloading can't be enabled on a queue while
	 * disabled on another queue. A pure per-port offloading can't
	 * be enabled for any queue as new added one if it hasn't been
	 * enabled in rte_eth_dev_configure().
	 */
	if ((local_conf.offloads & dev_info.rx_queue_offload_capa) !=
	     local_conf.offloads) {
		RTE_ETHDEV_LOG(ERR,
			"Ethdev port_id=%d rx_queue_id=%d, new added offloads 0x%"PRIx64" must be "
			"within per-queue offload capabilities 0x%"PRIx64" in %s()\n",
			port_id, rx_queue_id, local_conf.offloads,
			dev_info.rx_queue_offload_capa,
			__func__);
		return -EINVAL;
	}

	/*
	 * If LRO is enabled, check that the maximum aggregated packet
	 * size is supported by the configured device.
	 */
	if (local_conf.offloads & DEV_RX_OFFLOAD_TCP_LRO) {
		if (dev->data->dev_conf.rxmode.max_lro_pkt_size == 0)
			dev->data->dev_conf.rxmode.max_lro_pkt_size =
				dev->data->dev_conf.rxmode.max_rx_pkt_len;
		int ret = eth_dev_check_lro_pkt_size(port_id,
				dev->data->dev_conf.rxmode.max_lro_pkt_size,
				dev->data->dev_conf.rxmode.max_rx_pkt_len,
				dev_info.max_lro_pkt_size);
		if (ret != 0)
			return ret;
	}

	ret = (*dev->dev_ops->rx_queue_setup)(dev, rx_queue_id, nb_rx_desc,
					      socket_id, &local_conf, mp);
	if (!ret) {
		if (!dev->data->min_rx_buf_size ||
		    dev->data->min_rx_buf_size > mbp_buf_size)
			dev->data->min_rx_buf_size = mbp_buf_size;
	}

	rte_ethdev_trace_rxq_setup(port_id, rx_queue_id, nb_rx_desc, mp,
		rx_conf, ret);
	return eth_err(port_id, ret);
}

int
rte_eth_rx_hairpin_queue_setup(uint16_t port_id, uint16_t rx_queue_id,
			       uint16_t nb_rx_desc,
			       const struct rte_eth_hairpin_conf *conf)
{
	int ret;
	struct rte_eth_dev *dev;
	struct rte_eth_hairpin_cap cap;
	void **rxq;
	int i;
	int count;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid RX queue_id=%u\n", rx_queue_id);
		return -EINVAL;
	}
	ret = rte_eth_dev_hairpin_capability_get(port_id, &cap);
	if (ret != 0)
		return ret;
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_hairpin_queue_setup,
				-ENOTSUP);
	/* if nb_rx_desc is zero use max number of desc from the driver. */
	if (nb_rx_desc == 0)
		nb_rx_desc = cap.max_nb_desc;
	if (nb_rx_desc > cap.max_nb_desc) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid value for nb_rx_desc(=%hu), should be: <= %hu",
			nb_rx_desc, cap.max_nb_desc);
		return -EINVAL;
	}
	if (conf->peer_count > cap.max_rx_2_tx) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid value for number of peers for Rx queue(=%u), should be: <= %hu",
			conf->peer_count, cap.max_rx_2_tx);
		return -EINVAL;
	}
	if (conf->peer_count == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid value for number of peers for Rx queue(=%u), should be: > 0",
			conf->peer_count);
		return -EINVAL;
	}
	for (i = 0, count = 0; i < dev->data->nb_rx_queues &&
	     cap.max_nb_queues != UINT16_MAX; i++) {
		if (i == rx_queue_id || rte_eth_dev_is_rx_hairpin_queue(dev, i))
			count++;
	}
	if (count > cap.max_nb_queues) {
		RTE_ETHDEV_LOG(ERR, "To many Rx hairpin queues max is %d",
		cap.max_nb_queues);
		return -EINVAL;
	}
	if (dev->data->dev_started)
		return -EBUSY;
	rxq = dev->data->rx_queues;
	if (rxq[rx_queue_id] != NULL) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_release,
					-ENOTSUP);
		(*dev->dev_ops->rx_queue_release)(rxq[rx_queue_id]);
		rxq[rx_queue_id] = NULL;
	}
	ret = (*dev->dev_ops->rx_hairpin_queue_setup)(dev, rx_queue_id,
						      nb_rx_desc, conf);
	if (ret == 0)
		dev->data->rx_queue_state[rx_queue_id] =
			RTE_ETH_QUEUE_STATE_HAIRPIN;
	return eth_err(port_id, ret);
}

int
rte_eth_tx_queue_setup(uint16_t port_id, uint16_t tx_queue_id,
		       uint16_t nb_tx_desc, unsigned int socket_id,
		       const struct rte_eth_txconf *tx_conf)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf local_conf;
	void **txq;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (tx_queue_id >= dev->data->nb_tx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid TX queue_id=%u\n", tx_queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_setup, -ENOTSUP);

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	/* Use default specified by driver, if nb_tx_desc is zero */
	if (nb_tx_desc == 0) {
		nb_tx_desc = dev_info.default_txportconf.ring_size;
		/* If driver default is zero, fall back on EAL default */
		if (nb_tx_desc == 0)
			nb_tx_desc = RTE_ETH_DEV_FALLBACK_TX_RINGSIZE;
	}
	if (nb_tx_desc > dev_info.tx_desc_lim.nb_max ||
	    nb_tx_desc < dev_info.tx_desc_lim.nb_min ||
	    nb_tx_desc % dev_info.tx_desc_lim.nb_align != 0) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid value for nb_tx_desc(=%hu), should be: <= %hu, >= %hu, and a product of %hu\n",
			nb_tx_desc, dev_info.tx_desc_lim.nb_max,
			dev_info.tx_desc_lim.nb_min,
			dev_info.tx_desc_lim.nb_align);
		return -EINVAL;
	}

	if (dev->data->dev_started &&
		!(dev_info.dev_capa &
			RTE_ETH_DEV_CAPA_RUNTIME_TX_QUEUE_SETUP))
		return -EBUSY;

	if (dev->data->dev_started &&
		(dev->data->tx_queue_state[tx_queue_id] !=
			RTE_ETH_QUEUE_STATE_STOPPED))
		return -EBUSY;

	txq = dev->data->tx_queues;
	if (txq[tx_queue_id]) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_release,
					-ENOTSUP);
		(*dev->dev_ops->tx_queue_release)(txq[tx_queue_id]);
		txq[tx_queue_id] = NULL;
	}

	if (tx_conf == NULL)
		tx_conf = &dev_info.default_txconf;

	local_conf = *tx_conf;

	/*
	 * If an offloading has already been enabled in
	 * rte_eth_dev_configure(), it has been enabled on all queues,
	 * so there is no need to enable it in this queue again.
	 * The local_conf.offloads input to underlying PMD only carries
	 * those offloadings which are only enabled on this queue and
	 * not enabled on all queues.
	 */
	local_conf.offloads &= ~dev->data->dev_conf.txmode.offloads;

	/*
	 * New added offloadings for this queue are those not enabled in
	 * rte_eth_dev_configure() and they must be per-queue type.
	 * A pure per-port offloading can't be enabled on a queue while
	 * disabled on another queue. A pure per-port offloading can't
	 * be enabled for any queue as new added one if it hasn't been
	 * enabled in rte_eth_dev_configure().
	 */
	if ((local_conf.offloads & dev_info.tx_queue_offload_capa) !=
	     local_conf.offloads) {
		RTE_ETHDEV_LOG(ERR,
			"Ethdev port_id=%d tx_queue_id=%d, new added offloads 0x%"PRIx64" must be "
			"within per-queue offload capabilities 0x%"PRIx64" in %s()\n",
			port_id, tx_queue_id, local_conf.offloads,
			dev_info.tx_queue_offload_capa,
			__func__);
		return -EINVAL;
	}

	rte_ethdev_trace_txq_setup(port_id, tx_queue_id, nb_tx_desc, tx_conf);
	return eth_err(port_id, (*dev->dev_ops->tx_queue_setup)(dev,
		       tx_queue_id, nb_tx_desc, socket_id, &local_conf));
}

int
rte_eth_tx_hairpin_queue_setup(uint16_t port_id, uint16_t tx_queue_id,
			       uint16_t nb_tx_desc,
			       const struct rte_eth_hairpin_conf *conf)
{
	struct rte_eth_dev *dev;
	struct rte_eth_hairpin_cap cap;
	void **txq;
	int i;
	int count;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	if (tx_queue_id >= dev->data->nb_tx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid TX queue_id=%u\n", tx_queue_id);
		return -EINVAL;
	}
	ret = rte_eth_dev_hairpin_capability_get(port_id, &cap);
	if (ret != 0)
		return ret;
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_hairpin_queue_setup,
				-ENOTSUP);
	/* if nb_rx_desc is zero use max number of desc from the driver. */
	if (nb_tx_desc == 0)
		nb_tx_desc = cap.max_nb_desc;
	if (nb_tx_desc > cap.max_nb_desc) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid value for nb_tx_desc(=%hu), should be: <= %hu",
			nb_tx_desc, cap.max_nb_desc);
		return -EINVAL;
	}
	if (conf->peer_count > cap.max_tx_2_rx) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid value for number of peers for Tx queue(=%u), should be: <= %hu",
			conf->peer_count, cap.max_tx_2_rx);
		return -EINVAL;
	}
	if (conf->peer_count == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid value for number of peers for Tx queue(=%u), should be: > 0",
			conf->peer_count);
		return -EINVAL;
	}
	for (i = 0, count = 0; i < dev->data->nb_tx_queues &&
	     cap.max_nb_queues != UINT16_MAX; i++) {
		if (i == tx_queue_id || rte_eth_dev_is_tx_hairpin_queue(dev, i))
			count++;
	}
	if (count > cap.max_nb_queues) {
		RTE_ETHDEV_LOG(ERR, "To many Tx hairpin queues max is %d",
		cap.max_nb_queues);
		return -EINVAL;
	}
	if (dev->data->dev_started)
		return -EBUSY;
	txq = dev->data->tx_queues;
	if (txq[tx_queue_id] != NULL) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_release,
					-ENOTSUP);
		(*dev->dev_ops->tx_queue_release)(txq[tx_queue_id]);
		txq[tx_queue_id] = NULL;
	}
	ret = (*dev->dev_ops->tx_hairpin_queue_setup)
		(dev, tx_queue_id, nb_tx_desc, conf);
	if (ret == 0)
		dev->data->tx_queue_state[tx_queue_id] =
			RTE_ETH_QUEUE_STATE_HAIRPIN;
	return eth_err(port_id, ret);
}

int
rte_eth_hairpin_bind(uint16_t tx_port, uint16_t rx_port)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(tx_port, -ENODEV);
	dev = &rte_eth_devices[tx_port];
	if (dev->data->dev_started == 0) {
		RTE_ETHDEV_LOG(ERR, "Tx port %d is not started\n", tx_port);
		return -EBUSY;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->hairpin_bind, -ENOTSUP);
	ret = (*dev->dev_ops->hairpin_bind)(dev, rx_port);
	if (ret != 0)
		RTE_ETHDEV_LOG(ERR, "Failed to bind hairpin Tx %d"
			       " to Rx %d (%d - all ports)\n",
			       tx_port, rx_port, RTE_MAX_ETHPORTS);

	return ret;
}

int
rte_eth_hairpin_unbind(uint16_t tx_port, uint16_t rx_port)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(tx_port, -ENODEV);
	dev = &rte_eth_devices[tx_port];
	if (dev->data->dev_started == 0) {
		RTE_ETHDEV_LOG(ERR, "Tx port %d is already stopped\n", tx_port);
		return -EBUSY;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->hairpin_unbind, -ENOTSUP);
	ret = (*dev->dev_ops->hairpin_unbind)(dev, rx_port);
	if (ret != 0)
		RTE_ETHDEV_LOG(ERR, "Failed to unbind hairpin Tx %d"
			       " from Rx %d (%d - all ports)\n",
			       tx_port, rx_port, RTE_MAX_ETHPORTS);

	return ret;
}

int
rte_eth_hairpin_get_peer_ports(uint16_t port_id, uint16_t *peer_ports,
			       size_t len, uint32_t direction)
{
	struct rte_eth_dev *dev;
	int ret;

	if (peer_ports == NULL || len == 0)
		return -EINVAL;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->hairpin_get_peer_ports,
				-ENOTSUP);

	ret = (*dev->dev_ops->hairpin_get_peer_ports)(dev, peer_ports,
						      len, direction);
	if (ret < 0)
		RTE_ETHDEV_LOG(ERR, "Failed to get %d hairpin peer %s ports\n",
			       port_id, direction ? "Rx" : "Tx");

	return ret;
}

void
rte_eth_tx_buffer_drop_callback(struct rte_mbuf **pkts, uint16_t unsent,
		void *userdata __rte_unused)
{
	rte_pktmbuf_free_bulk(pkts, unsent);
}

void
rte_eth_tx_buffer_count_callback(struct rte_mbuf **pkts, uint16_t unsent,
		void *userdata)
{
	uint64_t *count = userdata;

	rte_pktmbuf_free_bulk(pkts, unsent);
	*count += unsent;
}

int
rte_eth_tx_buffer_set_err_callback(struct rte_eth_dev_tx_buffer *buffer,
		buffer_tx_error_fn cbfn, void *userdata)
{
	buffer->error_callback = cbfn;
	buffer->error_userdata = userdata;
	return 0;
}

int
rte_eth_tx_buffer_init(struct rte_eth_dev_tx_buffer *buffer, uint16_t size)
{
	int ret = 0;

	if (buffer == NULL)
		return -EINVAL;

	buffer->size = size;
	if (buffer->error_callback == NULL) {
		ret = rte_eth_tx_buffer_set_err_callback(
			buffer, rte_eth_tx_buffer_drop_callback, NULL);
	}

	return ret;
}

int
rte_eth_tx_done_cleanup(uint16_t port_id, uint16_t queue_id, uint32_t free_cnt)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret;

	/* Validate Input Data. Bail if not valid or not supported. */
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_done_cleanup, -ENOTSUP);

	/* Call driver to free pending mbufs. */
	ret = (*dev->dev_ops->tx_done_cleanup)(dev->data->tx_queues[queue_id],
					       free_cnt);
	return eth_err(port_id, ret);
}

int
rte_eth_promiscuous_enable(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int diag = 0;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (dev->data->promiscuous == 1)
		return 0;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->promiscuous_enable, -ENOTSUP);

	diag = (*dev->dev_ops->promiscuous_enable)(dev);
	dev->data->promiscuous = (diag == 0) ? 1 : 0;

	return eth_err(port_id, diag);
}

int
rte_eth_promiscuous_disable(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int diag = 0;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (dev->data->promiscuous == 0)
		return 0;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->promiscuous_disable, -ENOTSUP);

	dev->data->promiscuous = 0;
	diag = (*dev->dev_ops->promiscuous_disable)(dev);
	if (diag != 0)
		dev->data->promiscuous = 1;

	return eth_err(port_id, diag);
}

int
rte_eth_promiscuous_get(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	return dev->data->promiscuous;
}

int
rte_eth_allmulticast_enable(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int diag;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (dev->data->all_multicast == 1)
		return 0;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->allmulticast_enable, -ENOTSUP);
	diag = (*dev->dev_ops->allmulticast_enable)(dev);
	dev->data->all_multicast = (diag == 0) ? 1 : 0;

	return eth_err(port_id, diag);
}

int
rte_eth_allmulticast_disable(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int diag;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (dev->data->all_multicast == 0)
		return 0;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->allmulticast_disable, -ENOTSUP);
	dev->data->all_multicast = 0;
	diag = (*dev->dev_ops->allmulticast_disable)(dev);
	if (diag != 0)
		dev->data->all_multicast = 1;

	return eth_err(port_id, diag);
}

int
rte_eth_allmulticast_get(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	return dev->data->all_multicast;
}

int
rte_eth_link_get(uint16_t port_id, struct rte_eth_link *eth_link)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.intr_conf.lsc &&
	    dev->data->dev_started)
		rte_eth_linkstatus_get(dev, eth_link);
	else {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->link_update, -ENOTSUP);
		(*dev->dev_ops->link_update)(dev, 1);
		*eth_link = dev->data->dev_link;
	}

	return 0;
}

int
rte_eth_link_get_nowait(uint16_t port_id, struct rte_eth_link *eth_link)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.intr_conf.lsc &&
	    dev->data->dev_started)
		rte_eth_linkstatus_get(dev, eth_link);
	else {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->link_update, -ENOTSUP);
		(*dev->dev_ops->link_update)(dev, 0);
		*eth_link = dev->data->dev_link;
	}

	return 0;
}

const char *
rte_eth_link_speed_to_str(uint32_t link_speed)
{
	switch (link_speed) {
	case ETH_SPEED_NUM_NONE: return "None";
	case ETH_SPEED_NUM_10M:  return "10 Mbps";
	case ETH_SPEED_NUM_100M: return "100 Mbps";
	case ETH_SPEED_NUM_1G:   return "1 Gbps";
	case ETH_SPEED_NUM_2_5G: return "2.5 Gbps";
	case ETH_SPEED_NUM_5G:   return "5 Gbps";
	case ETH_SPEED_NUM_10G:  return "10 Gbps";
	case ETH_SPEED_NUM_20G:  return "20 Gbps";
	case ETH_SPEED_NUM_25G:  return "25 Gbps";
	case ETH_SPEED_NUM_40G:  return "40 Gbps";
	case ETH_SPEED_NUM_50G:  return "50 Gbps";
	case ETH_SPEED_NUM_56G:  return "56 Gbps";
	case ETH_SPEED_NUM_100G: return "100 Gbps";
	case ETH_SPEED_NUM_200G: return "200 Gbps";
	case ETH_SPEED_NUM_UNKNOWN: return "Unknown";
	default: return "Invalid";
	}
}

int
rte_eth_link_to_str(char *str, size_t len, const struct rte_eth_link *eth_link)
{
	if (eth_link->link_status == ETH_LINK_DOWN)
		return snprintf(str, len, "Link down");
	else
		return snprintf(str, len, "Link up at %s %s %s",
			rte_eth_link_speed_to_str(eth_link->link_speed),
			(eth_link->link_duplex == ETH_LINK_FULL_DUPLEX) ?
			"FDX" : "HDX",
			(eth_link->link_autoneg == ETH_LINK_AUTONEG) ?
			"Autoneg" : "Fixed");
}

int
rte_eth_stats_get(uint16_t port_id, struct rte_eth_stats *stats)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	memset(stats, 0, sizeof(*stats));

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->stats_get, -ENOTSUP);
	stats->rx_nombuf = dev->data->rx_mbuf_alloc_failed;
	return eth_err(port_id, (*dev->dev_ops->stats_get)(dev, stats));
}

int
rte_eth_stats_reset(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->stats_reset, -ENOTSUP);
	ret = (*dev->dev_ops->stats_reset)(dev);
	if (ret != 0)
		return eth_err(port_id, ret);

	dev->data->rx_mbuf_alloc_failed = 0;

	return 0;
}

static inline int
eth_dev_get_xstats_basic_count(struct rte_eth_dev *dev)
{
	uint16_t nb_rxqs, nb_txqs;
	int count;

	nb_rxqs = RTE_MIN(dev->data->nb_rx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);
	nb_txqs = RTE_MIN(dev->data->nb_tx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);

	count = RTE_NB_STATS;
	if (dev->data->dev_flags & RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS) {
		count += nb_rxqs * RTE_NB_RXQ_STATS;
		count += nb_txqs * RTE_NB_TXQ_STATS;
	}

	return count;
}

static int
eth_dev_get_xstats_count(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int count;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	if (dev->dev_ops->xstats_get_names_by_id != NULL) {
		count = (*dev->dev_ops->xstats_get_names_by_id)(dev, NULL,
				NULL, 0);
		if (count < 0)
			return eth_err(port_id, count);
	}
	if (dev->dev_ops->xstats_get_names != NULL) {
		count = (*dev->dev_ops->xstats_get_names)(dev, NULL, 0);
		if (count < 0)
			return eth_err(port_id, count);
	} else
		count = 0;


	count += eth_dev_get_xstats_basic_count(dev);

	return count;
}

int
rte_eth_xstats_get_id_by_name(uint16_t port_id, const char *xstat_name,
		uint64_t *id)
{
	int cnt_xstats, idx_xstat;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (!id) {
		RTE_ETHDEV_LOG(ERR, "Id pointer is NULL\n");
		return -ENOMEM;
	}

	if (!xstat_name) {
		RTE_ETHDEV_LOG(ERR, "xstat_name pointer is NULL\n");
		return -ENOMEM;
	}

	/* Get count */
	cnt_xstats = rte_eth_xstats_get_names_by_id(port_id, NULL, 0, NULL);
	if (cnt_xstats  < 0) {
		RTE_ETHDEV_LOG(ERR, "Cannot get count of xstats\n");
		return -ENODEV;
	}

	/* Get id-name lookup table */
	struct rte_eth_xstat_name xstats_names[cnt_xstats];

	if (cnt_xstats != rte_eth_xstats_get_names_by_id(
			port_id, xstats_names, cnt_xstats, NULL)) {
		RTE_ETHDEV_LOG(ERR, "Cannot get xstats lookup\n");
		return -1;
	}

	for (idx_xstat = 0; idx_xstat < cnt_xstats; idx_xstat++) {
		if (!strcmp(xstats_names[idx_xstat].name, xstat_name)) {
			*id = idx_xstat;
			return 0;
		};
	}

	return -EINVAL;
}

/* retrieve basic stats names */
static int
eth_basic_stats_get_names(struct rte_eth_dev *dev,
	struct rte_eth_xstat_name *xstats_names)
{
	int cnt_used_entries = 0;
	uint32_t idx, id_queue;
	uint16_t num_q;

	for (idx = 0; idx < RTE_NB_STATS; idx++) {
		strlcpy(xstats_names[cnt_used_entries].name,
			eth_dev_stats_strings[idx].name,
			sizeof(xstats_names[0].name));
		cnt_used_entries++;
	}

	if ((dev->data->dev_flags & RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS) == 0)
		return cnt_used_entries;

	num_q = RTE_MIN(dev->data->nb_rx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);
	for (id_queue = 0; id_queue < num_q; id_queue++) {
		for (idx = 0; idx < RTE_NB_RXQ_STATS; idx++) {
			snprintf(xstats_names[cnt_used_entries].name,
				sizeof(xstats_names[0].name),
				"rx_q%u_%s",
				id_queue, eth_dev_rxq_stats_strings[idx].name);
			cnt_used_entries++;
		}

	}
	num_q = RTE_MIN(dev->data->nb_tx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);
	for (id_queue = 0; id_queue < num_q; id_queue++) {
		for (idx = 0; idx < RTE_NB_TXQ_STATS; idx++) {
			snprintf(xstats_names[cnt_used_entries].name,
				sizeof(xstats_names[0].name),
				"tx_q%u_%s",
				id_queue, eth_dev_txq_stats_strings[idx].name);
			cnt_used_entries++;
		}
	}
	return cnt_used_entries;
}

/* retrieve ethdev extended statistics names */
int
rte_eth_xstats_get_names_by_id(uint16_t port_id,
	struct rte_eth_xstat_name *xstats_names, unsigned int size,
	uint64_t *ids)
{
	struct rte_eth_xstat_name *xstats_names_copy;
	unsigned int no_basic_stat_requested = 1;
	unsigned int no_ext_stat_requested = 1;
	unsigned int expected_entries;
	unsigned int basic_count;
	struct rte_eth_dev *dev;
	unsigned int i;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	basic_count = eth_dev_get_xstats_basic_count(dev);
	ret = eth_dev_get_xstats_count(port_id);
	if (ret < 0)
		return ret;
	expected_entries = (unsigned int)ret;

	/* Return max number of stats if no ids given */
	if (!ids) {
		if (!xstats_names)
			return expected_entries;
		else if (xstats_names && size < expected_entries)
			return expected_entries;
	}

	if (ids && !xstats_names)
		return -EINVAL;

	if (ids && dev->dev_ops->xstats_get_names_by_id != NULL && size > 0) {
		uint64_t ids_copy[size];

		for (i = 0; i < size; i++) {
			if (ids[i] < basic_count) {
				no_basic_stat_requested = 0;
				break;
			}

			/*
			 * Convert ids to xstats ids that PMD knows.
			 * ids known by user are basic + extended stats.
			 */
			ids_copy[i] = ids[i] - basic_count;
		}

		if (no_basic_stat_requested)
			return (*dev->dev_ops->xstats_get_names_by_id)(dev,
					xstats_names, ids_copy, size);
	}

	/* Retrieve all stats */
	if (!ids) {
		int num_stats = rte_eth_xstats_get_names(port_id, xstats_names,
				expected_entries);
		if (num_stats < 0 || num_stats > (int)expected_entries)
			return num_stats;
		else
			return expected_entries;
	}

	xstats_names_copy = calloc(expected_entries,
		sizeof(struct rte_eth_xstat_name));

	if (!xstats_names_copy) {
		RTE_ETHDEV_LOG(ERR, "Can't allocate memory\n");
		return -ENOMEM;
	}

	if (ids) {
		for (i = 0; i < size; i++) {
			if (ids[i] >= basic_count) {
				no_ext_stat_requested = 0;
				break;
			}
		}
	}

	/* Fill xstats_names_copy structure */
	if (ids && no_ext_stat_requested) {
		eth_basic_stats_get_names(dev, xstats_names_copy);
	} else {
		ret = rte_eth_xstats_get_names(port_id, xstats_names_copy,
			expected_entries);
		if (ret < 0) {
			free(xstats_names_copy);
			return ret;
		}
	}

	/* Filter stats */
	for (i = 0; i < size; i++) {
		if (ids[i] >= expected_entries) {
			RTE_ETHDEV_LOG(ERR, "Id value isn't valid\n");
			free(xstats_names_copy);
			return -1;
		}
		xstats_names[i] = xstats_names_copy[ids[i]];
	}

	free(xstats_names_copy);
	return size;
}

int
rte_eth_xstats_get_names(uint16_t port_id,
	struct rte_eth_xstat_name *xstats_names,
	unsigned int size)
{
	struct rte_eth_dev *dev;
	int cnt_used_entries;
	int cnt_expected_entries;
	int cnt_driver_entries;

	cnt_expected_entries = eth_dev_get_xstats_count(port_id);
	if (xstats_names == NULL || cnt_expected_entries < 0 ||
			(int)size < cnt_expected_entries)
		return cnt_expected_entries;

	/* port_id checked in eth_dev_get_xstats_count() */
	dev = &rte_eth_devices[port_id];

	cnt_used_entries = eth_basic_stats_get_names(dev, xstats_names);

	if (dev->dev_ops->xstats_get_names != NULL) {
		/* If there are any driver-specific xstats, append them
		 * to end of list.
		 */
		cnt_driver_entries = (*dev->dev_ops->xstats_get_names)(
			dev,
			xstats_names + cnt_used_entries,
			size - cnt_used_entries);
		if (cnt_driver_entries < 0)
			return eth_err(port_id, cnt_driver_entries);
		cnt_used_entries += cnt_driver_entries;
	}

	return cnt_used_entries;
}


static int
eth_basic_stats_get(uint16_t port_id, struct rte_eth_xstat *xstats)
{
	struct rte_eth_dev *dev;
	struct rte_eth_stats eth_stats;
	unsigned int count = 0, i, q;
	uint64_t val, *stats_ptr;
	uint16_t nb_rxqs, nb_txqs;
	int ret;

	ret = rte_eth_stats_get(port_id, &eth_stats);
	if (ret < 0)
		return ret;

	dev = &rte_eth_devices[port_id];

	nb_rxqs = RTE_MIN(dev->data->nb_rx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);
	nb_txqs = RTE_MIN(dev->data->nb_tx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);

	/* global stats */
	for (i = 0; i < RTE_NB_STATS; i++) {
		stats_ptr = RTE_PTR_ADD(&eth_stats,
					eth_dev_stats_strings[i].offset);
		val = *stats_ptr;
		xstats[count++].value = val;
	}

	if ((dev->data->dev_flags & RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS) == 0)
		return count;

	/* per-rxq stats */
	for (q = 0; q < nb_rxqs; q++) {
		for (i = 0; i < RTE_NB_RXQ_STATS; i++) {
			stats_ptr = RTE_PTR_ADD(&eth_stats,
					eth_dev_rxq_stats_strings[i].offset +
					q * sizeof(uint64_t));
			val = *stats_ptr;
			xstats[count++].value = val;
		}
	}

	/* per-txq stats */
	for (q = 0; q < nb_txqs; q++) {
		for (i = 0; i < RTE_NB_TXQ_STATS; i++) {
			stats_ptr = RTE_PTR_ADD(&eth_stats,
					eth_dev_txq_stats_strings[i].offset +
					q * sizeof(uint64_t));
			val = *stats_ptr;
			xstats[count++].value = val;
		}
	}
	return count;
}

/* retrieve ethdev extended statistics */
int
rte_eth_xstats_get_by_id(uint16_t port_id, const uint64_t *ids,
			 uint64_t *values, unsigned int size)
{
	unsigned int no_basic_stat_requested = 1;
	unsigned int no_ext_stat_requested = 1;
	unsigned int num_xstats_filled;
	unsigned int basic_count;
	uint16_t expected_entries;
	struct rte_eth_dev *dev;
	unsigned int i;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	ret = eth_dev_get_xstats_count(port_id);
	if (ret < 0)
		return ret;
	expected_entries = (uint16_t)ret;
	struct rte_eth_xstat xstats[expected_entries];
	dev = &rte_eth_devices[port_id];
	basic_count = eth_dev_get_xstats_basic_count(dev);

	/* Return max number of stats if no ids given */
	if (!ids) {
		if (!values)
			return expected_entries;
		else if (values && size < expected_entries)
			return expected_entries;
	}

	if (ids && !values)
		return -EINVAL;

	if (ids && dev->dev_ops->xstats_get_by_id != NULL && size) {
		unsigned int basic_count = eth_dev_get_xstats_basic_count(dev);
		uint64_t ids_copy[size];

		for (i = 0; i < size; i++) {
			if (ids[i] < basic_count) {
				no_basic_stat_requested = 0;
				break;
			}

			/*
			 * Convert ids to xstats ids that PMD knows.
			 * ids known by user are basic + extended stats.
			 */
			ids_copy[i] = ids[i] - basic_count;
		}

		if (no_basic_stat_requested)
			return (*dev->dev_ops->xstats_get_by_id)(dev, ids_copy,
					values, size);
	}

	if (ids) {
		for (i = 0; i < size; i++) {
			if (ids[i] >= basic_count) {
				no_ext_stat_requested = 0;
				break;
			}
		}
	}

	/* Fill the xstats structure */
	if (ids && no_ext_stat_requested)
		ret = eth_basic_stats_get(port_id, xstats);
	else
		ret = rte_eth_xstats_get(port_id, xstats, expected_entries);

	if (ret < 0)
		return ret;
	num_xstats_filled = (unsigned int)ret;

	/* Return all stats */
	if (!ids) {
		for (i = 0; i < num_xstats_filled; i++)
			values[i] = xstats[i].value;
		return expected_entries;
	}

	/* Filter stats */
	for (i = 0; i < size; i++) {
		if (ids[i] >= expected_entries) {
			RTE_ETHDEV_LOG(ERR, "Id value isn't valid\n");
			return -1;
		}
		values[i] = xstats[ids[i]].value;
	}
	return size;
}

int
rte_eth_xstats_get(uint16_t port_id, struct rte_eth_xstat *xstats,
	unsigned int n)
{
	struct rte_eth_dev *dev;
	unsigned int count = 0, i;
	signed int xcount = 0;
	uint16_t nb_rxqs, nb_txqs;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	nb_rxqs = RTE_MIN(dev->data->nb_rx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);
	nb_txqs = RTE_MIN(dev->data->nb_tx_queues, RTE_ETHDEV_QUEUE_STAT_CNTRS);

	/* Return generic statistics */
	count = RTE_NB_STATS;
	if (dev->data->dev_flags & RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS)
		count += (nb_rxqs * RTE_NB_RXQ_STATS) + (nb_txqs * RTE_NB_TXQ_STATS);

	/* implemented by the driver */
	if (dev->dev_ops->xstats_get != NULL) {
		/* Retrieve the xstats from the driver at the end of the
		 * xstats struct.
		 */
		xcount = (*dev->dev_ops->xstats_get)(dev,
				     xstats ? xstats + count : NULL,
				     (n > count) ? n - count : 0);

		if (xcount < 0)
			return eth_err(port_id, xcount);
	}

	if (n < count + xcount || xstats == NULL)
		return count + xcount;

	/* now fill the xstats structure */
	ret = eth_basic_stats_get(port_id, xstats);
	if (ret < 0)
		return ret;
	count = ret;

	for (i = 0; i < count; i++)
		xstats[i].id = i;
	/* add an offset to driver-specific stats */
	for ( ; i < count + xcount; i++)
		xstats[i].id += count;

	return count + xcount;
}

/* reset ethdev extended statistics */
int
rte_eth_xstats_reset(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	/* implemented by the driver */
	if (dev->dev_ops->xstats_reset != NULL)
		return eth_err(port_id, (*dev->dev_ops->xstats_reset)(dev));

	/* fallback to default */
	return rte_eth_stats_reset(port_id);
}

static int
eth_dev_set_queue_stats_mapping(uint16_t port_id, uint16_t queue_id,
		uint8_t stat_idx, uint8_t is_rx)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_stats_mapping_set, -ENOTSUP);

	if (is_rx && (queue_id >= dev->data->nb_rx_queues))
		return -EINVAL;

	if (!is_rx && (queue_id >= dev->data->nb_tx_queues))
		return -EINVAL;

	if (stat_idx >= RTE_ETHDEV_QUEUE_STAT_CNTRS)
		return -EINVAL;

	return (*dev->dev_ops->queue_stats_mapping_set)
			(dev, queue_id, stat_idx, is_rx);
}


int
rte_eth_dev_set_tx_queue_stats_mapping(uint16_t port_id, uint16_t tx_queue_id,
		uint8_t stat_idx)
{
	return eth_err(port_id, eth_dev_set_queue_stats_mapping(port_id,
						tx_queue_id,
						stat_idx, STAT_QMAP_TX));
}


int
rte_eth_dev_set_rx_queue_stats_mapping(uint16_t port_id, uint16_t rx_queue_id,
		uint8_t stat_idx)
{
	return eth_err(port_id, eth_dev_set_queue_stats_mapping(port_id,
						rx_queue_id,
						stat_idx, STAT_QMAP_RX));
}

int
rte_eth_dev_fw_version_get(uint16_t port_id, char *fw_version, size_t fw_size)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fw_version_get, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->fw_version_get)(dev,
							fw_version, fw_size));
}

int
rte_eth_dev_info_get(uint16_t port_id, struct rte_eth_dev_info *dev_info)
{
	struct rte_eth_dev *dev;
	const struct rte_eth_desc_lim lim = {
		.nb_max = UINT16_MAX,
		.nb_min = 0,
		.nb_align = 1,
		.nb_seg_max = UINT16_MAX,
		.nb_mtu_seg_max = UINT16_MAX,
	};
	int diag;

	/*
	 * Init dev_info before port_id check since caller does not have
	 * return status and does not know if get is successful or not.
	 */
	memset(dev_info, 0, sizeof(struct rte_eth_dev_info));
	dev_info->switch_info.domain_id = RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	dev_info->rx_desc_lim = lim;
	dev_info->tx_desc_lim = lim;
	dev_info->device = dev->device;
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;
	dev_info->max_mtu = UINT16_MAX;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);
	diag = (*dev->dev_ops->dev_infos_get)(dev, dev_info);
	if (diag != 0) {
		/* Cleanup already filled in device information */
		memset(dev_info, 0, sizeof(struct rte_eth_dev_info));
		return eth_err(port_id, diag);
	}

	/* Maximum number of queues should be <= RTE_MAX_QUEUES_PER_PORT */
	dev_info->max_rx_queues = RTE_MIN(dev_info->max_rx_queues,
			RTE_MAX_QUEUES_PER_PORT);
	dev_info->max_tx_queues = RTE_MIN(dev_info->max_tx_queues,
			RTE_MAX_QUEUES_PER_PORT);

	dev_info->driver_name = dev->device->driver->name;
	dev_info->nb_rx_queues = dev->data->nb_rx_queues;
	dev_info->nb_tx_queues = dev->data->nb_tx_queues;

	dev_info->dev_flags = &dev->data->dev_flags;

	return 0;
}

int
rte_eth_dev_get_supported_ptypes(uint16_t port_id, uint32_t ptype_mask,
				 uint32_t *ptypes, int num)
{
	int i, j;
	struct rte_eth_dev *dev;
	const uint32_t *all_ptypes;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_supported_ptypes_get, 0);
	all_ptypes = (*dev->dev_ops->dev_supported_ptypes_get)(dev);

	if (!all_ptypes)
		return 0;

	for (i = 0, j = 0; all_ptypes[i] != RTE_PTYPE_UNKNOWN; ++i)
		if (all_ptypes[i] & ptype_mask) {
			if (j < num)
				ptypes[j] = all_ptypes[i];
			j++;
		}

	return j;
}

int
rte_eth_dev_set_ptypes(uint16_t port_id, uint32_t ptype_mask,
				 uint32_t *set_ptypes, unsigned int num)
{
	const uint32_t valid_ptype_masks[] = {
		RTE_PTYPE_L2_MASK,
		RTE_PTYPE_L3_MASK,
		RTE_PTYPE_L4_MASK,
		RTE_PTYPE_TUNNEL_MASK,
		RTE_PTYPE_INNER_L2_MASK,
		RTE_PTYPE_INNER_L3_MASK,
		RTE_PTYPE_INNER_L4_MASK,
	};
	const uint32_t *all_ptypes;
	struct rte_eth_dev *dev;
	uint32_t unused_mask;
	unsigned int i, j;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (num > 0 && set_ptypes == NULL)
		return -EINVAL;

	if (*dev->dev_ops->dev_supported_ptypes_get == NULL ||
			*dev->dev_ops->dev_ptypes_set == NULL) {
		ret = 0;
		goto ptype_unknown;
	}

	if (ptype_mask == 0) {
		ret = (*dev->dev_ops->dev_ptypes_set)(dev,
				ptype_mask);
		goto ptype_unknown;
	}

	unused_mask = ptype_mask;
	for (i = 0; i < RTE_DIM(valid_ptype_masks); i++) {
		uint32_t mask = ptype_mask & valid_ptype_masks[i];
		if (mask && mask != valid_ptype_masks[i]) {
			ret = -EINVAL;
			goto ptype_unknown;
		}
		unused_mask &= ~valid_ptype_masks[i];
	}

	if (unused_mask) {
		ret = -EINVAL;
		goto ptype_unknown;
	}

	all_ptypes = (*dev->dev_ops->dev_supported_ptypes_get)(dev);
	if (all_ptypes == NULL) {
		ret = 0;
		goto ptype_unknown;
	}

	/*
	 * Accommodate as many set_ptypes as possible. If the supplied
	 * set_ptypes array is insufficient fill it partially.
	 */
	for (i = 0, j = 0; set_ptypes != NULL &&
				(all_ptypes[i] != RTE_PTYPE_UNKNOWN); ++i) {
		if (ptype_mask & all_ptypes[i]) {
			if (j < num - 1) {
				set_ptypes[j] = all_ptypes[i];
				j++;
				continue;
			}
			break;
		}
	}

	if (set_ptypes != NULL && j < num)
		set_ptypes[j] = RTE_PTYPE_UNKNOWN;

	return (*dev->dev_ops->dev_ptypes_set)(dev, ptype_mask);

ptype_unknown:
	if (num > 0)
		set_ptypes[0] = RTE_PTYPE_UNKNOWN;

	return ret;
}

int
rte_eth_macaddr_get(uint16_t port_id, struct rte_ether_addr *mac_addr)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	rte_ether_addr_copy(&dev->data->mac_addrs[0], mac_addr);

	return 0;
}

int
rte_eth_dev_get_mtu(uint16_t port_id, uint16_t *mtu)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	*mtu = dev->data->mtu;
	return 0;
}

int
rte_eth_dev_set_mtu(uint16_t port_id, uint16_t mtu)
{
	int ret;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mtu_set, -ENOTSUP);

	/*
	 * Check if the device supports dev_infos_get, if it does not
	 * skip min_mtu/max_mtu validation here as this requires values
	 * that are populated within the call to rte_eth_dev_info_get()
	 * which relies on dev->dev_ops->dev_infos_get.
	 */
	if (*dev->dev_ops->dev_infos_get != NULL) {
		ret = rte_eth_dev_info_get(port_id, &dev_info);
		if (ret != 0)
			return ret;

		if (mtu < dev_info.min_mtu || mtu > dev_info.max_mtu)
			return -EINVAL;
	}

	ret = (*dev->dev_ops->mtu_set)(dev, mtu);
	if (!ret)
		dev->data->mtu = mtu;

	return eth_err(port_id, ret);
}

int
rte_eth_dev_vlan_filter(uint16_t port_id, uint16_t vlan_id, int on)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	if (!(dev->data->dev_conf.rxmode.offloads &
	      DEV_RX_OFFLOAD_VLAN_FILTER)) {
		RTE_ETHDEV_LOG(ERR, "Port %u: vlan-filtering disabled\n",
			port_id);
		return -ENOSYS;
	}

	if (vlan_id > 4095) {
		RTE_ETHDEV_LOG(ERR, "Port_id=%u invalid vlan_id=%u > 4095\n",
			port_id, vlan_id);
		return -EINVAL;
	}
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_filter_set, -ENOTSUP);

	ret = (*dev->dev_ops->vlan_filter_set)(dev, vlan_id, on);
	if (ret == 0) {
		struct rte_vlan_filter_conf *vfc;
		int vidx;
		int vbit;

		vfc = &dev->data->vlan_filter_conf;
		vidx = vlan_id / 64;
		vbit = vlan_id % 64;

		if (on)
			vfc->ids[vidx] |= UINT64_C(1) << vbit;
		else
			vfc->ids[vidx] &= ~(UINT64_C(1) << vbit);
	}

	return eth_err(port_id, ret);
}

int
rte_eth_dev_set_vlan_strip_on_queue(uint16_t port_id, uint16_t rx_queue_id,
				    int on)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid rx_queue_id=%u\n", rx_queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_strip_queue_set, -ENOTSUP);
	(*dev->dev_ops->vlan_strip_queue_set)(dev, rx_queue_id, on);

	return 0;
}

int
rte_eth_dev_set_vlan_ether_type(uint16_t port_id,
				enum rte_vlan_type vlan_type,
				uint16_t tpid)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_tpid_set, -ENOTSUP);

	return eth_err(port_id, (*dev->dev_ops->vlan_tpid_set)(dev, vlan_type,
							       tpid));
}

int
rte_eth_dev_set_vlan_offload(uint16_t port_id, int offload_mask)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_dev *dev;
	int ret = 0;
	int mask = 0;
	int cur, org = 0;
	uint64_t orig_offloads;
	uint64_t dev_offloads;
	uint64_t new_offloads;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	/* save original values in case of failure */
	orig_offloads = dev->data->dev_conf.rxmode.offloads;
	dev_offloads = orig_offloads;

	/* check which option changed by application */
	cur = !!(offload_mask & ETH_VLAN_STRIP_OFFLOAD);
	org = !!(dev_offloads & DEV_RX_OFFLOAD_VLAN_STRIP);
	if (cur != org) {
		if (cur)
			dev_offloads |= DEV_RX_OFFLOAD_VLAN_STRIP;
		else
			dev_offloads &= ~DEV_RX_OFFLOAD_VLAN_STRIP;
		mask |= ETH_VLAN_STRIP_MASK;
	}

	cur = !!(offload_mask & ETH_VLAN_FILTER_OFFLOAD);
	org = !!(dev_offloads & DEV_RX_OFFLOAD_VLAN_FILTER);
	if (cur != org) {
		if (cur)
			dev_offloads |= DEV_RX_OFFLOAD_VLAN_FILTER;
		else
			dev_offloads &= ~DEV_RX_OFFLOAD_VLAN_FILTER;
		mask |= ETH_VLAN_FILTER_MASK;
	}

	cur = !!(offload_mask & ETH_VLAN_EXTEND_OFFLOAD);
	org = !!(dev_offloads & DEV_RX_OFFLOAD_VLAN_EXTEND);
	if (cur != org) {
		if (cur)
			dev_offloads |= DEV_RX_OFFLOAD_VLAN_EXTEND;
		else
			dev_offloads &= ~DEV_RX_OFFLOAD_VLAN_EXTEND;
		mask |= ETH_VLAN_EXTEND_MASK;
	}

	cur = !!(offload_mask & ETH_QINQ_STRIP_OFFLOAD);
	org = !!(dev_offloads & DEV_RX_OFFLOAD_QINQ_STRIP);
	if (cur != org) {
		if (cur)
			dev_offloads |= DEV_RX_OFFLOAD_QINQ_STRIP;
		else
			dev_offloads &= ~DEV_RX_OFFLOAD_QINQ_STRIP;
		mask |= ETH_QINQ_STRIP_MASK;
	}

	/*no change*/
	if (mask == 0)
		return ret;

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	/* Rx VLAN offloading must be within its device capabilities */
	if ((dev_offloads & dev_info.rx_offload_capa) != dev_offloads) {
		new_offloads = dev_offloads & ~orig_offloads;
		RTE_ETHDEV_LOG(ERR,
			"Ethdev port_id=%u requested new added VLAN offloads "
			"0x%" PRIx64 " must be within Rx offloads capabilities "
			"0x%" PRIx64 " in %s()\n",
			port_id, new_offloads, dev_info.rx_offload_capa,
			__func__);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_offload_set, -ENOTSUP);
	dev->data->dev_conf.rxmode.offloads = dev_offloads;
	ret = (*dev->dev_ops->vlan_offload_set)(dev, mask);
	if (ret) {
		/* hit an error restore  original values */
		dev->data->dev_conf.rxmode.offloads = orig_offloads;
	}

	return eth_err(port_id, ret);
}

int
rte_eth_dev_get_vlan_offload(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	uint64_t *dev_offloads;
	int ret = 0;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	dev_offloads = &dev->data->dev_conf.rxmode.offloads;

	if (*dev_offloads & DEV_RX_OFFLOAD_VLAN_STRIP)
		ret |= ETH_VLAN_STRIP_OFFLOAD;

	if (*dev_offloads & DEV_RX_OFFLOAD_VLAN_FILTER)
		ret |= ETH_VLAN_FILTER_OFFLOAD;

	if (*dev_offloads & DEV_RX_OFFLOAD_VLAN_EXTEND)
		ret |= ETH_VLAN_EXTEND_OFFLOAD;

	if (*dev_offloads & DEV_RX_OFFLOAD_QINQ_STRIP)
		ret |= ETH_QINQ_STRIP_OFFLOAD;

	return ret;
}

int
rte_eth_dev_set_vlan_pvid(uint16_t port_id, uint16_t pvid, int on)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_pvid_set, -ENOTSUP);

	return eth_err(port_id, (*dev->dev_ops->vlan_pvid_set)(dev, pvid, on));
}

int
rte_eth_dev_flow_ctrl_get(uint16_t port_id, struct rte_eth_fc_conf *fc_conf)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->flow_ctrl_get, -ENOTSUP);
	memset(fc_conf, 0, sizeof(*fc_conf));
	return eth_err(port_id, (*dev->dev_ops->flow_ctrl_get)(dev, fc_conf));
}

int
rte_eth_dev_flow_ctrl_set(uint16_t port_id, struct rte_eth_fc_conf *fc_conf)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if ((fc_conf->send_xon != 0) && (fc_conf->send_xon != 1)) {
		RTE_ETHDEV_LOG(ERR, "Invalid send_xon, only 0/1 allowed\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->flow_ctrl_set, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->flow_ctrl_set)(dev, fc_conf));
}

int
rte_eth_dev_priority_flow_ctrl_set(uint16_t port_id,
				   struct rte_eth_pfc_conf *pfc_conf)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (pfc_conf->priority > (ETH_DCB_NUM_USER_PRIORITIES - 1)) {
		RTE_ETHDEV_LOG(ERR, "Invalid priority, only 0-7 allowed\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	/* High water, low water validation are device specific */
	if  (*dev->dev_ops->priority_flow_ctrl_set)
		return eth_err(port_id, (*dev->dev_ops->priority_flow_ctrl_set)
					(dev, pfc_conf));
	return -ENOTSUP;
}

static int
eth_check_reta_mask(struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	uint16_t i, num;

	if (!reta_conf)
		return -EINVAL;

	num = (reta_size + RTE_RETA_GROUP_SIZE - 1) / RTE_RETA_GROUP_SIZE;
	for (i = 0; i < num; i++) {
		if (reta_conf[i].mask)
			return 0;
	}

	return -EINVAL;
}

static int
eth_check_reta_entry(struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size,
			 uint16_t max_rxq)
{
	uint16_t i, idx, shift;

	if (!reta_conf)
		return -EINVAL;

	if (max_rxq == 0) {
		RTE_ETHDEV_LOG(ERR, "No receive queue is available\n");
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if ((reta_conf[idx].mask & (1ULL << shift)) &&
			(reta_conf[idx].reta[shift] >= max_rxq)) {
			RTE_ETHDEV_LOG(ERR,
				"reta_conf[%u]->reta[%u]: %u exceeds the maximum rxq index: %u\n",
				idx, shift,
				reta_conf[idx].reta[shift], max_rxq);
			return -EINVAL;
		}
	}

	return 0;
}

int
rte_eth_dev_rss_reta_update(uint16_t port_id,
			    struct rte_eth_rss_reta_entry64 *reta_conf,
			    uint16_t reta_size)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	/* Check mask bits */
	ret = eth_check_reta_mask(reta_conf, reta_size);
	if (ret < 0)
		return ret;

	dev = &rte_eth_devices[port_id];

	/* Check entry value */
	ret = eth_check_reta_entry(reta_conf, reta_size,
				dev->data->nb_rx_queues);
	if (ret < 0)
		return ret;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->reta_update, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->reta_update)(dev, reta_conf,
							     reta_size));
}

int
rte_eth_dev_rss_reta_query(uint16_t port_id,
			   struct rte_eth_rss_reta_entry64 *reta_conf,
			   uint16_t reta_size)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	/* Check mask bits */
	ret = eth_check_reta_mask(reta_conf, reta_size);
	if (ret < 0)
		return ret;

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->reta_query, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->reta_query)(dev, reta_conf,
							    reta_size));
}

int
rte_eth_dev_rss_hash_update(uint16_t port_id,
			    struct rte_eth_rss_conf *rss_conf)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info = { .flow_type_rss_offloads = 0, };
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	rss_conf->rss_hf = rte_eth_rss_hf_refine(rss_conf->rss_hf);

	dev = &rte_eth_devices[port_id];
	if ((dev_info.flow_type_rss_offloads | rss_conf->rss_hf) !=
	    dev_info.flow_type_rss_offloads) {
		RTE_ETHDEV_LOG(ERR,
			"Ethdev port_id=%u invalid rss_hf: 0x%"PRIx64", valid value: 0x%"PRIx64"\n",
			port_id, rss_conf->rss_hf,
			dev_info.flow_type_rss_offloads);
		return -EINVAL;
	}
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rss_hash_update, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->rss_hash_update)(dev,
								 rss_conf));
}

int
rte_eth_dev_rss_hash_conf_get(uint16_t port_id,
			      struct rte_eth_rss_conf *rss_conf)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rss_hash_conf_get, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->rss_hash_conf_get)(dev,
								   rss_conf));
}

int
rte_eth_dev_udp_tunnel_port_add(uint16_t port_id,
				struct rte_eth_udp_tunnel *udp_tunnel)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (udp_tunnel == NULL) {
		RTE_ETHDEV_LOG(ERR, "Invalid udp_tunnel parameter\n");
		return -EINVAL;
	}

	if (udp_tunnel->prot_type >= RTE_TUNNEL_TYPE_MAX) {
		RTE_ETHDEV_LOG(ERR, "Invalid tunnel type\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->udp_tunnel_port_add, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->udp_tunnel_port_add)(dev,
								udp_tunnel));
}

int
rte_eth_dev_udp_tunnel_port_delete(uint16_t port_id,
				   struct rte_eth_udp_tunnel *udp_tunnel)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (udp_tunnel == NULL) {
		RTE_ETHDEV_LOG(ERR, "Invalid udp_tunnel parameter\n");
		return -EINVAL;
	}

	if (udp_tunnel->prot_type >= RTE_TUNNEL_TYPE_MAX) {
		RTE_ETHDEV_LOG(ERR, "Invalid tunnel type\n");
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->udp_tunnel_port_del, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->udp_tunnel_port_del)(dev,
								udp_tunnel));
}

int
rte_eth_led_on(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_led_on, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->dev_led_on)(dev));
}

int
rte_eth_led_off(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_led_off, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->dev_led_off)(dev));
}

int
rte_eth_fec_get_capability(uint16_t port_id,
			   struct rte_eth_fec_capa *speed_fec_capa,
			   unsigned int num)
{
	struct rte_eth_dev *dev;
	int ret;

	if (speed_fec_capa == NULL && num > 0)
		return -EINVAL;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fec_get_capability, -ENOTSUP);
	ret = (*dev->dev_ops->fec_get_capability)(dev, speed_fec_capa, num);

	return ret;
}

int
rte_eth_fec_get(uint16_t port_id, uint32_t *fec_capa)
{
	struct rte_eth_dev *dev;

	if (fec_capa == NULL)
		return -EINVAL;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fec_get, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->fec_get)(dev, fec_capa));
}

int
rte_eth_fec_set(uint16_t port_id, uint32_t fec_capa)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fec_set, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->fec_set)(dev, fec_capa));
}

/*
 * Returns index into MAC address array of addr. Use 00:00:00:00:00:00 to find
 * an empty spot.
 */
static int
eth_dev_get_mac_addr_index(uint16_t port_id, const struct rte_ether_addr *addr)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	unsigned i;
	int ret;

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return -1;

	for (i = 0; i < dev_info.max_mac_addrs; i++)
		if (memcmp(addr, &dev->data->mac_addrs[i],
				RTE_ETHER_ADDR_LEN) == 0)
			return i;

	return -1;
}

static const struct rte_ether_addr null_mac_addr;

int
rte_eth_dev_mac_addr_add(uint16_t port_id, struct rte_ether_addr *addr,
			uint32_t pool)
{
	struct rte_eth_dev *dev;
	int index;
	uint64_t pool_mask;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mac_addr_add, -ENOTSUP);

	if (rte_is_zero_ether_addr(addr)) {
		RTE_ETHDEV_LOG(ERR, "Port %u: Cannot add NULL MAC address\n",
			port_id);
		return -EINVAL;
	}
	if (pool >= ETH_64_POOLS) {
		RTE_ETHDEV_LOG(ERR, "Pool id must be 0-%d\n", ETH_64_POOLS - 1);
		return -EINVAL;
	}

	index = eth_dev_get_mac_addr_index(port_id, addr);
	if (index < 0) {
		index = eth_dev_get_mac_addr_index(port_id, &null_mac_addr);
		if (index < 0) {
			RTE_ETHDEV_LOG(ERR, "Port %u: MAC address array full\n",
				port_id);
			return -ENOSPC;
		}
	} else {
		pool_mask = dev->data->mac_pool_sel[index];

		/* Check if both MAC address and pool is already there, and do nothing */
		if (pool_mask & (1ULL << pool))
			return 0;
	}

	/* Update NIC */
	ret = (*dev->dev_ops->mac_addr_add)(dev, addr, index, pool);

	if (ret == 0) {
		/* Update address in NIC data structure */
		rte_ether_addr_copy(addr, &dev->data->mac_addrs[index]);

		/* Update pool bitmap in NIC data structure */
		dev->data->mac_pool_sel[index] |= (1ULL << pool);
	}

	return eth_err(port_id, ret);
}

int
rte_eth_dev_mac_addr_remove(uint16_t port_id, struct rte_ether_addr *addr)
{
	struct rte_eth_dev *dev;
	int index;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mac_addr_remove, -ENOTSUP);

	index = eth_dev_get_mac_addr_index(port_id, addr);
	if (index == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Port %u: Cannot remove default MAC address\n",
			port_id);
		return -EADDRINUSE;
	} else if (index < 0)
		return 0;  /* Do nothing if address wasn't found */

	/* Update NIC */
	(*dev->dev_ops->mac_addr_remove)(dev, index);

	/* Update address in NIC data structure */
	rte_ether_addr_copy(&null_mac_addr, &dev->data->mac_addrs[index]);

	/* reset pool bitmap */
	dev->data->mac_pool_sel[index] = 0;

	return 0;
}

int
rte_eth_dev_default_mac_addr_set(uint16_t port_id, struct rte_ether_addr *addr)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (!rte_is_valid_assigned_ether_addr(addr))
		return -EINVAL;

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mac_addr_set, -ENOTSUP);

	ret = (*dev->dev_ops->mac_addr_set)(dev, addr);
	if (ret < 0)
		return ret;

	/* Update default address in NIC data structure */
	rte_ether_addr_copy(addr, &dev->data->mac_addrs[0]);

	return 0;
}


/*
 * Returns index into MAC address array of addr. Use 00:00:00:00:00:00 to find
 * an empty spot.
 */
static int
eth_dev_get_hash_mac_addr_index(uint16_t port_id,
		const struct rte_ether_addr *addr)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	unsigned i;
	int ret;

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return -1;

	if (!dev->data->hash_mac_addrs)
		return -1;

	for (i = 0; i < dev_info.max_hash_mac_addrs; i++)
		if (memcmp(addr, &dev->data->hash_mac_addrs[i],
			RTE_ETHER_ADDR_LEN) == 0)
			return i;

	return -1;
}

int
rte_eth_dev_uc_hash_table_set(uint16_t port_id, struct rte_ether_addr *addr,
				uint8_t on)
{
	int index;
	int ret;
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (rte_is_zero_ether_addr(addr)) {
		RTE_ETHDEV_LOG(ERR, "Port %u: Cannot add NULL MAC address\n",
			port_id);
		return -EINVAL;
	}

	index = eth_dev_get_hash_mac_addr_index(port_id, addr);
	/* Check if it's already there, and do nothing */
	if ((index >= 0) && on)
		return 0;

	if (index < 0) {
		if (!on) {
			RTE_ETHDEV_LOG(ERR,
				"Port %u: the MAC address was not set in UTA\n",
				port_id);
			return -EINVAL;
		}

		index = eth_dev_get_hash_mac_addr_index(port_id, &null_mac_addr);
		if (index < 0) {
			RTE_ETHDEV_LOG(ERR, "Port %u: MAC address array full\n",
				port_id);
			return -ENOSPC;
		}
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->uc_hash_table_set, -ENOTSUP);
	ret = (*dev->dev_ops->uc_hash_table_set)(dev, addr, on);
	if (ret == 0) {
		/* Update address in NIC data structure */
		if (on)
			rte_ether_addr_copy(addr,
					&dev->data->hash_mac_addrs[index]);
		else
			rte_ether_addr_copy(&null_mac_addr,
					&dev->data->hash_mac_addrs[index]);
	}

	return eth_err(port_id, ret);
}

int
rte_eth_dev_uc_all_hash_table_set(uint16_t port_id, uint8_t on)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->uc_all_hash_table_set, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->uc_all_hash_table_set)(dev,
								       on));
}

int rte_eth_set_queue_rate_limit(uint16_t port_id, uint16_t queue_idx,
					uint16_t tx_rate)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_link link;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	dev = &rte_eth_devices[port_id];
	link = dev->data->dev_link;

	if (queue_idx > dev_info.max_tx_queues) {
		RTE_ETHDEV_LOG(ERR,
			"Set queue rate limit:port %u: invalid queue id=%u\n",
			port_id, queue_idx);
		return -EINVAL;
	}

	if (tx_rate > link.link_speed) {
		RTE_ETHDEV_LOG(ERR,
			"Set queue rate limit:invalid tx_rate=%u, bigger than link speed= %d\n",
			tx_rate, link.link_speed);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_queue_rate_limit, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->set_queue_rate_limit)(dev,
							queue_idx, tx_rate));
}

int
rte_eth_mirror_rule_set(uint16_t port_id,
			struct rte_eth_mirror_conf *mirror_conf,
			uint8_t rule_id, uint8_t on)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (mirror_conf->rule_type == 0) {
		RTE_ETHDEV_LOG(ERR, "Mirror rule type can not be 0\n");
		return -EINVAL;
	}

	if (mirror_conf->dst_pool >= ETH_64_POOLS) {
		RTE_ETHDEV_LOG(ERR, "Invalid dst pool, pool id must be 0-%d\n",
			ETH_64_POOLS - 1);
		return -EINVAL;
	}

	if ((mirror_conf->rule_type & (ETH_MIRROR_VIRTUAL_POOL_UP |
	     ETH_MIRROR_VIRTUAL_POOL_DOWN)) &&
	    (mirror_conf->pool_mask == 0)) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid mirror pool, pool mask can not be 0\n");
		return -EINVAL;
	}

	if ((mirror_conf->rule_type & ETH_MIRROR_VLAN) &&
	    mirror_conf->vlan.vlan_mask == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid vlan mask, vlan mask can not be 0\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mirror_rule_set, -ENOTSUP);

	return eth_err(port_id, (*dev->dev_ops->mirror_rule_set)(dev,
						mirror_conf, rule_id, on));
}

int
rte_eth_mirror_rule_reset(uint16_t port_id, uint8_t rule_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mirror_rule_reset, -ENOTSUP);

	return eth_err(port_id, (*dev->dev_ops->mirror_rule_reset)(dev,
								   rule_id));
}

RTE_INIT(eth_dev_init_cb_lists)
{
	uint16_t i;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++)
		TAILQ_INIT(&rte_eth_devices[i].link_intr_cbs);
}

int
rte_eth_dev_callback_register(uint16_t port_id,
			enum rte_eth_event_type event,
			rte_eth_dev_cb_fn cb_fn, void *cb_arg)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_callback *user_cb;
	uint16_t next_port;
	uint16_t last_port;

	if (!cb_fn)
		return -EINVAL;

	if (!rte_eth_dev_is_valid_port(port_id) && port_id != RTE_ETH_ALL) {
		RTE_ETHDEV_LOG(ERR, "Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	if (port_id == RTE_ETH_ALL) {
		next_port = 0;
		last_port = RTE_MAX_ETHPORTS - 1;
	} else {
		next_port = last_port = port_id;
	}

	rte_spinlock_lock(&eth_dev_cb_lock);

	do {
		dev = &rte_eth_devices[next_port];

		TAILQ_FOREACH(user_cb, &(dev->link_intr_cbs), next) {
			if (user_cb->cb_fn == cb_fn &&
				user_cb->cb_arg == cb_arg &&
				user_cb->event == event) {
				break;
			}
		}

		/* create a new callback. */
		if (user_cb == NULL) {
			user_cb = rte_zmalloc("INTR_USER_CALLBACK",
				sizeof(struct rte_eth_dev_callback), 0);
			if (user_cb != NULL) {
				user_cb->cb_fn = cb_fn;
				user_cb->cb_arg = cb_arg;
				user_cb->event = event;
				TAILQ_INSERT_TAIL(&(dev->link_intr_cbs),
						  user_cb, next);
			} else {
				rte_spinlock_unlock(&eth_dev_cb_lock);
				rte_eth_dev_callback_unregister(port_id, event,
								cb_fn, cb_arg);
				return -ENOMEM;
			}

		}
	} while (++next_port <= last_port);

	rte_spinlock_unlock(&eth_dev_cb_lock);
	return 0;
}

int
rte_eth_dev_callback_unregister(uint16_t port_id,
			enum rte_eth_event_type event,
			rte_eth_dev_cb_fn cb_fn, void *cb_arg)
{
	int ret;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_callback *cb, *next;
	uint16_t next_port;
	uint16_t last_port;

	if (!cb_fn)
		return -EINVAL;

	if (!rte_eth_dev_is_valid_port(port_id) && port_id != RTE_ETH_ALL) {
		RTE_ETHDEV_LOG(ERR, "Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	if (port_id == RTE_ETH_ALL) {
		next_port = 0;
		last_port = RTE_MAX_ETHPORTS - 1;
	} else {
		next_port = last_port = port_id;
	}

	rte_spinlock_lock(&eth_dev_cb_lock);

	do {
		dev = &rte_eth_devices[next_port];
		ret = 0;
		for (cb = TAILQ_FIRST(&dev->link_intr_cbs); cb != NULL;
		     cb = next) {

			next = TAILQ_NEXT(cb, next);

			if (cb->cb_fn != cb_fn || cb->event != event ||
			    (cb_arg != (void *)-1 && cb->cb_arg != cb_arg))
				continue;

			/*
			 * if this callback is not executing right now,
			 * then remove it.
			 */
			if (cb->active == 0) {
				TAILQ_REMOVE(&(dev->link_intr_cbs), cb, next);
				rte_free(cb);
			} else {
				ret = -EAGAIN;
			}
		}
	} while (++next_port <= last_port);

	rte_spinlock_unlock(&eth_dev_cb_lock);
	return ret;
}

int
rte_eth_dev_callback_process(struct rte_eth_dev *dev,
	enum rte_eth_event_type event, void *ret_param)
{
	struct rte_eth_dev_callback *cb_lst;
	struct rte_eth_dev_callback dev_cb;
	int rc = 0;

	rte_spinlock_lock(&eth_dev_cb_lock);
	TAILQ_FOREACH(cb_lst, &(dev->link_intr_cbs), next) {
		if (cb_lst->cb_fn == NULL || cb_lst->event != event)
			continue;
		dev_cb = *cb_lst;
		cb_lst->active = 1;
		if (ret_param != NULL)
			dev_cb.ret_param = ret_param;

		rte_spinlock_unlock(&eth_dev_cb_lock);
		rc = dev_cb.cb_fn(dev->data->port_id, dev_cb.event,
				dev_cb.cb_arg, dev_cb.ret_param);
		rte_spinlock_lock(&eth_dev_cb_lock);
		cb_lst->active = 0;
	}
	rte_spinlock_unlock(&eth_dev_cb_lock);
	return rc;
}

void
rte_eth_dev_probing_finish(struct rte_eth_dev *dev)
{
	if (dev == NULL)
		return;

	rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_NEW, NULL);

	dev->state = RTE_ETH_DEV_ATTACHED;
}

int
rte_eth_dev_rx_intr_ctl(uint16_t port_id, int epfd, int op, void *data)
{
	uint32_t vec;
	struct rte_eth_dev *dev;
	struct rte_intr_handle *intr_handle;
	uint16_t qid;
	int rc;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	if (!dev->intr_handle) {
		RTE_ETHDEV_LOG(ERR, "RX Intr handle unset\n");
		return -ENOTSUP;
	}

	intr_handle = dev->intr_handle;
	if (!intr_handle->intr_vec) {
		RTE_ETHDEV_LOG(ERR, "RX Intr vector unset\n");
		return -EPERM;
	}

	for (qid = 0; qid < dev->data->nb_rx_queues; qid++) {
		vec = intr_handle->intr_vec[qid];
		rc = rte_intr_rx_ctl(intr_handle, epfd, op, vec, data);
		if (rc && rc != -EEXIST) {
			RTE_ETHDEV_LOG(ERR,
				"p %u q %u rx ctl error op %d epfd %d vec %u\n",
				port_id, qid, op, epfd, vec);
		}
	}

	return 0;
}

int
rte_eth_dev_rx_intr_ctl_q_get_fd(uint16_t port_id, uint16_t queue_id)
{
	struct rte_intr_handle *intr_handle;
	struct rte_eth_dev *dev;
	unsigned int efd_idx;
	uint32_t vec;
	int fd;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -1);

	dev = &rte_eth_devices[port_id];

	if (queue_id >= dev->data->nb_rx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid RX queue_id=%u\n", queue_id);
		return -1;
	}

	if (!dev->intr_handle) {
		RTE_ETHDEV_LOG(ERR, "RX Intr handle unset\n");
		return -1;
	}

	intr_handle = dev->intr_handle;
	if (!intr_handle->intr_vec) {
		RTE_ETHDEV_LOG(ERR, "RX Intr vector unset\n");
		return -1;
	}

	vec = intr_handle->intr_vec[queue_id];
	efd_idx = (vec >= RTE_INTR_VEC_RXTX_OFFSET) ?
		(vec - RTE_INTR_VEC_RXTX_OFFSET) : vec;
	fd = intr_handle->efds[efd_idx];

	return fd;
}

static inline int
eth_dev_dma_mzone_name(char *name, size_t len, uint16_t port_id, uint16_t queue_id,
		const char *ring_name)
{
	return snprintf(name, len, "eth_p%d_q%d_%s",
			port_id, queue_id, ring_name);
}

const struct rte_memzone *
rte_eth_dma_zone_reserve(const struct rte_eth_dev *dev, const char *ring_name,
			 uint16_t queue_id, size_t size, unsigned align,
			 int socket_id)
{
	char z_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	int rc;

	rc = eth_dev_dma_mzone_name(z_name, sizeof(z_name), dev->data->port_id,
			queue_id, ring_name);
	if (rc >= RTE_MEMZONE_NAMESIZE) {
		RTE_ETHDEV_LOG(ERR, "ring name too long\n");
		rte_errno = ENAMETOOLONG;
		return NULL;
	}

	mz = rte_memzone_lookup(z_name);
	if (mz) {
		if ((socket_id != SOCKET_ID_ANY && socket_id != mz->socket_id) ||
				size > mz->len ||
				((uintptr_t)mz->addr & (align - 1)) != 0) {
			RTE_ETHDEV_LOG(ERR,
				"memzone %s does not justify the requested attributes\n",
				mz->name);
			return NULL;
		}

		return mz;
	}

	return rte_memzone_reserve_aligned(z_name, size, socket_id,
			RTE_MEMZONE_IOVA_CONTIG, align);
}

int
rte_eth_dma_zone_free(const struct rte_eth_dev *dev, const char *ring_name,
		uint16_t queue_id)
{
	char z_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	int rc = 0;

	rc = eth_dev_dma_mzone_name(z_name, sizeof(z_name), dev->data->port_id,
			queue_id, ring_name);
	if (rc >= RTE_MEMZONE_NAMESIZE) {
		RTE_ETHDEV_LOG(ERR, "ring name too long\n");
		return -ENAMETOOLONG;
	}

	mz = rte_memzone_lookup(z_name);
	if (mz)
		rc = rte_memzone_free(mz);
	else
		rc = -ENOENT;

	return rc;
}

int
rte_eth_dev_create(struct rte_device *device, const char *name,
	size_t priv_data_size,
	ethdev_bus_specific_init ethdev_bus_specific_init,
	void *bus_init_params,
	ethdev_init_t ethdev_init, void *init_params)
{
	struct rte_eth_dev *ethdev;
	int retval;

	RTE_FUNC_PTR_OR_ERR_RET(*ethdev_init, -EINVAL);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		ethdev = rte_eth_dev_allocate(name);
		if (!ethdev)
			return -ENODEV;

		if (priv_data_size) {
			ethdev->data->dev_private = rte_zmalloc_socket(
				name, priv_data_size, RTE_CACHE_LINE_SIZE,
				device->numa_node);

			if (!ethdev->data->dev_private) {
				RTE_ETHDEV_LOG(ERR,
					"failed to allocate private data\n");
				retval = -ENOMEM;
				goto probe_failed;
			}
		}
	} else {
		ethdev = rte_eth_dev_attach_secondary(name);
		if (!ethdev) {
			RTE_ETHDEV_LOG(ERR,
				"secondary process attach failed, ethdev doesn't exist\n");
			return  -ENODEV;
		}
	}

	ethdev->device = device;

	if (ethdev_bus_specific_init) {
		retval = ethdev_bus_specific_init(ethdev, bus_init_params);
		if (retval) {
			RTE_ETHDEV_LOG(ERR,
				"ethdev bus specific initialisation failed\n");
			goto probe_failed;
		}
	}

	retval = ethdev_init(ethdev, init_params);
	if (retval) {
		RTE_ETHDEV_LOG(ERR, "ethdev initialisation failed\n");
		goto probe_failed;
	}

	rte_eth_dev_probing_finish(ethdev);

	return retval;

probe_failed:
	rte_eth_dev_release_port(ethdev);
	return retval;
}

int
rte_eth_dev_destroy(struct rte_eth_dev *ethdev,
	ethdev_uninit_t ethdev_uninit)
{
	int ret;

	ethdev = rte_eth_dev_allocated(ethdev->data->name);
	if (!ethdev)
		return -ENODEV;

	RTE_FUNC_PTR_OR_ERR_RET(*ethdev_uninit, -EINVAL);

	ret = ethdev_uninit(ethdev);
	if (ret)
		return ret;

	return rte_eth_dev_release_port(ethdev);
}

int
rte_eth_dev_rx_intr_ctl_q(uint16_t port_id, uint16_t queue_id,
			  int epfd, int op, void *data)
{
	uint32_t vec;
	struct rte_eth_dev *dev;
	struct rte_intr_handle *intr_handle;
	int rc;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (queue_id >= dev->data->nb_rx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid RX queue_id=%u\n", queue_id);
		return -EINVAL;
	}

	if (!dev->intr_handle) {
		RTE_ETHDEV_LOG(ERR, "RX Intr handle unset\n");
		return -ENOTSUP;
	}

	intr_handle = dev->intr_handle;
	if (!intr_handle->intr_vec) {
		RTE_ETHDEV_LOG(ERR, "RX Intr vector unset\n");
		return -EPERM;
	}

	vec = intr_handle->intr_vec[queue_id];
	rc = rte_intr_rx_ctl(intr_handle, epfd, op, vec, data);
	if (rc && rc != -EEXIST) {
		RTE_ETHDEV_LOG(ERR,
			"p %u q %u rx ctl error op %d epfd %d vec %u\n",
			port_id, queue_id, op, epfd, vec);
		return rc;
	}

	return 0;
}

int
rte_eth_dev_rx_intr_enable(uint16_t port_id,
			   uint16_t queue_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	ret = eth_dev_validate_rx_queue(dev, queue_id);
	if (ret != 0)
		return ret;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_intr_enable, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->rx_queue_intr_enable)(dev,
								queue_id));
}

int
rte_eth_dev_rx_intr_disable(uint16_t port_id,
			    uint16_t queue_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	ret = eth_dev_validate_rx_queue(dev, queue_id);
	if (ret != 0)
		return ret;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_intr_disable, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->rx_queue_intr_disable)(dev,
								queue_id));
}


const struct rte_eth_rxtx_callback *
rte_eth_add_rx_callback(uint16_t port_id, uint16_t queue_id,
		rte_rx_callback_fn fn, void *user_param)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	rte_errno = ENOTSUP;
	return NULL;
#endif
	struct rte_eth_dev *dev;

	/* check input parameters */
	if (!rte_eth_dev_is_valid_port(port_id) || fn == NULL ||
		    queue_id >= rte_eth_devices[port_id].data->nb_rx_queues) {
		rte_errno = EINVAL;
		return NULL;
	}
	dev = &rte_eth_devices[port_id];
	if (rte_eth_dev_is_rx_hairpin_queue(dev, queue_id)) {
		rte_errno = EINVAL;
		return NULL;
	}
	struct rte_eth_rxtx_callback *cb = rte_zmalloc(NULL, sizeof(*cb), 0);

	if (cb == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	cb->fn.rx = fn;
	cb->param = user_param;

	rte_spinlock_lock(&eth_dev_rx_cb_lock);
	/* Add the callbacks in fifo order. */
	struct rte_eth_rxtx_callback *tail =
		rte_eth_devices[port_id].post_rx_burst_cbs[queue_id];

	if (!tail) {
		/* Stores to cb->fn and cb->param should complete before
		 * cb is visible to data plane.
		 */
		__atomic_store_n(
			&rte_eth_devices[port_id].post_rx_burst_cbs[queue_id],
			cb, __ATOMIC_RELEASE);

	} else {
		while (tail->next)
			tail = tail->next;
		/* Stores to cb->fn and cb->param should complete before
		 * cb is visible to data plane.
		 */
		__atomic_store_n(&tail->next, cb, __ATOMIC_RELEASE);
	}
	rte_spinlock_unlock(&eth_dev_rx_cb_lock);

	return cb;
}

const struct rte_eth_rxtx_callback *
rte_eth_add_first_rx_callback(uint16_t port_id, uint16_t queue_id,
		rte_rx_callback_fn fn, void *user_param)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	rte_errno = ENOTSUP;
	return NULL;
#endif
	/* check input parameters */
	if (!rte_eth_dev_is_valid_port(port_id) || fn == NULL ||
		queue_id >= rte_eth_devices[port_id].data->nb_rx_queues) {
		rte_errno = EINVAL;
		return NULL;
	}

	struct rte_eth_rxtx_callback *cb = rte_zmalloc(NULL, sizeof(*cb), 0);

	if (cb == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	cb->fn.rx = fn;
	cb->param = user_param;

	rte_spinlock_lock(&eth_dev_rx_cb_lock);
	/* Add the callbacks at first position */
	cb->next = rte_eth_devices[port_id].post_rx_burst_cbs[queue_id];
	/* Stores to cb->fn, cb->param and cb->next should complete before
	 * cb is visible to data plane threads.
	 */
	__atomic_store_n(
		&rte_eth_devices[port_id].post_rx_burst_cbs[queue_id],
		cb, __ATOMIC_RELEASE);
	rte_spinlock_unlock(&eth_dev_rx_cb_lock);

	return cb;
}

const struct rte_eth_rxtx_callback *
rte_eth_add_tx_callback(uint16_t port_id, uint16_t queue_id,
		rte_tx_callback_fn fn, void *user_param)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	rte_errno = ENOTSUP;
	return NULL;
#endif
	struct rte_eth_dev *dev;

	/* check input parameters */
	if (!rte_eth_dev_is_valid_port(port_id) || fn == NULL ||
		    queue_id >= rte_eth_devices[port_id].data->nb_tx_queues) {
		rte_errno = EINVAL;
		return NULL;
	}

	dev = &rte_eth_devices[port_id];
	if (rte_eth_dev_is_tx_hairpin_queue(dev, queue_id)) {
		rte_errno = EINVAL;
		return NULL;
	}

	struct rte_eth_rxtx_callback *cb = rte_zmalloc(NULL, sizeof(*cb), 0);

	if (cb == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	cb->fn.tx = fn;
	cb->param = user_param;

	rte_spinlock_lock(&eth_dev_tx_cb_lock);
	/* Add the callbacks in fifo order. */
	struct rte_eth_rxtx_callback *tail =
		rte_eth_devices[port_id].pre_tx_burst_cbs[queue_id];

	if (!tail) {
		/* Stores to cb->fn and cb->param should complete before
		 * cb is visible to data plane.
		 */
		__atomic_store_n(
			&rte_eth_devices[port_id].pre_tx_burst_cbs[queue_id],
			cb, __ATOMIC_RELEASE);

	} else {
		while (tail->next)
			tail = tail->next;
		/* Stores to cb->fn and cb->param should complete before
		 * cb is visible to data plane.
		 */
		__atomic_store_n(&tail->next, cb, __ATOMIC_RELEASE);
	}
	rte_spinlock_unlock(&eth_dev_tx_cb_lock);

	return cb;
}

int
rte_eth_remove_rx_callback(uint16_t port_id, uint16_t queue_id,
		const struct rte_eth_rxtx_callback *user_cb)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	return -ENOTSUP;
#endif
	/* Check input parameters. */
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (user_cb == NULL ||
			queue_id >= rte_eth_devices[port_id].data->nb_rx_queues)
		return -EINVAL;

	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct rte_eth_rxtx_callback *cb;
	struct rte_eth_rxtx_callback **prev_cb;
	int ret = -EINVAL;

	rte_spinlock_lock(&eth_dev_rx_cb_lock);
	prev_cb = &dev->post_rx_burst_cbs[queue_id];
	for (; *prev_cb != NULL; prev_cb = &cb->next) {
		cb = *prev_cb;
		if (cb == user_cb) {
			/* Remove the user cb from the callback list. */
			__atomic_store_n(prev_cb, cb->next, __ATOMIC_RELAXED);
			ret = 0;
			break;
		}
	}
	rte_spinlock_unlock(&eth_dev_rx_cb_lock);

	return ret;
}

int
rte_eth_remove_tx_callback(uint16_t port_id, uint16_t queue_id,
		const struct rte_eth_rxtx_callback *user_cb)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	return -ENOTSUP;
#endif
	/* Check input parameters. */
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (user_cb == NULL ||
			queue_id >= rte_eth_devices[port_id].data->nb_tx_queues)
		return -EINVAL;

	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret = -EINVAL;
	struct rte_eth_rxtx_callback *cb;
	struct rte_eth_rxtx_callback **prev_cb;

	rte_spinlock_lock(&eth_dev_tx_cb_lock);
	prev_cb = &dev->pre_tx_burst_cbs[queue_id];
	for (; *prev_cb != NULL; prev_cb = &cb->next) {
		cb = *prev_cb;
		if (cb == user_cb) {
			/* Remove the user cb from the callback list. */
			__atomic_store_n(prev_cb, cb->next, __ATOMIC_RELAXED);
			ret = 0;
			break;
		}
	}
	rte_spinlock_unlock(&eth_dev_tx_cb_lock);

	return ret;
}

int
rte_eth_rx_queue_info_get(uint16_t port_id, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (qinfo == NULL)
		return -EINVAL;

	dev = &rte_eth_devices[port_id];
	if (queue_id >= dev->data->nb_rx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid RX queue_id=%u\n", queue_id);
		return -EINVAL;
	}

	if (dev->data->rx_queues == NULL ||
			dev->data->rx_queues[queue_id] == NULL) {
		RTE_ETHDEV_LOG(ERR,
			       "Rx queue %"PRIu16" of device with port_id=%"
			       PRIu16" has not been setup\n",
			       queue_id, port_id);
		return -EINVAL;
	}

	if (rte_eth_dev_is_rx_hairpin_queue(dev, queue_id)) {
		RTE_ETHDEV_LOG(INFO,
			"Can't get hairpin Rx queue %"PRIu16" info of device with port_id=%"PRIu16"\n",
			queue_id, port_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rxq_info_get, -ENOTSUP);

	memset(qinfo, 0, sizeof(*qinfo));
	dev->dev_ops->rxq_info_get(dev, queue_id, qinfo);
	return 0;
}

int
rte_eth_tx_queue_info_get(uint16_t port_id, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (qinfo == NULL)
		return -EINVAL;

	dev = &rte_eth_devices[port_id];
	if (queue_id >= dev->data->nb_tx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid TX queue_id=%u\n", queue_id);
		return -EINVAL;
	}

	if (dev->data->tx_queues == NULL ||
			dev->data->tx_queues[queue_id] == NULL) {
		RTE_ETHDEV_LOG(ERR,
			       "Tx queue %"PRIu16" of device with port_id=%"
			       PRIu16" has not been setup\n",
			       queue_id, port_id);
		return -EINVAL;
	}

	if (rte_eth_dev_is_tx_hairpin_queue(dev, queue_id)) {
		RTE_ETHDEV_LOG(INFO,
			"Can't get hairpin Tx queue %"PRIu16" info of device with port_id=%"PRIu16"\n",
			queue_id, port_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->txq_info_get, -ENOTSUP);

	memset(qinfo, 0, sizeof(*qinfo));
	dev->dev_ops->txq_info_get(dev, queue_id, qinfo);

	return 0;
}

int
rte_eth_rx_burst_mode_get(uint16_t port_id, uint16_t queue_id,
			  struct rte_eth_burst_mode *mode)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (mode == NULL)
		return -EINVAL;

	dev = &rte_eth_devices[port_id];

	if (queue_id >= dev->data->nb_rx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid RX queue_id=%u\n", queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_burst_mode_get, -ENOTSUP);
	memset(mode, 0, sizeof(*mode));
	return eth_err(port_id,
		       dev->dev_ops->rx_burst_mode_get(dev, queue_id, mode));
}

int
rte_eth_tx_burst_mode_get(uint16_t port_id, uint16_t queue_id,
			  struct rte_eth_burst_mode *mode)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (mode == NULL)
		return -EINVAL;

	dev = &rte_eth_devices[port_id];

	if (queue_id >= dev->data->nb_tx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid TX queue_id=%u\n", queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_burst_mode_get, -ENOTSUP);
	memset(mode, 0, sizeof(*mode));
	return eth_err(port_id,
		       dev->dev_ops->tx_burst_mode_get(dev, queue_id, mode));
}

int
rte_eth_dev_set_mc_addr_list(uint16_t port_id,
			     struct rte_ether_addr *mc_addr_set,
			     uint32_t nb_mc_addr)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_mc_addr_list, -ENOTSUP);
	return eth_err(port_id, dev->dev_ops->set_mc_addr_list(dev,
						mc_addr_set, nb_mc_addr));
}

int
rte_eth_timesync_enable(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_enable, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->timesync_enable)(dev));
}

int
rte_eth_timesync_disable(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_disable, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->timesync_disable)(dev));
}

int
rte_eth_timesync_read_rx_timestamp(uint16_t port_id, struct timespec *timestamp,
				   uint32_t flags)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_read_rx_timestamp, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->timesync_read_rx_timestamp)
				(dev, timestamp, flags));
}

int
rte_eth_timesync_read_tx_timestamp(uint16_t port_id,
				   struct timespec *timestamp)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_read_tx_timestamp, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->timesync_read_tx_timestamp)
				(dev, timestamp));
}

int
rte_eth_timesync_adjust_time(uint16_t port_id, int64_t delta)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_adjust_time, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->timesync_adjust_time)(dev,
								      delta));
}

int
rte_eth_timesync_read_time(uint16_t port_id, struct timespec *timestamp)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_read_time, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->timesync_read_time)(dev,
								timestamp));
}

int
rte_eth_timesync_write_time(uint16_t port_id, const struct timespec *timestamp)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_write_time, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->timesync_write_time)(dev,
								timestamp));
}

int
rte_eth_read_clock(uint16_t port_id, uint64_t *clock)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->read_clock, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->read_clock)(dev, clock));
}

int
rte_eth_dev_get_reg_info(uint16_t port_id, struct rte_dev_reg_info *info)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (info == NULL)
		return -EINVAL;

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->get_reg, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->get_reg)(dev, info));
}

int
rte_eth_dev_get_eeprom_length(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->get_eeprom_length, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->get_eeprom_length)(dev));
}

int
rte_eth_dev_get_eeprom(uint16_t port_id, struct rte_dev_eeprom_info *info)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (info == NULL)
		return -EINVAL;

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->get_eeprom, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->get_eeprom)(dev, info));
}

int
rte_eth_dev_set_eeprom(uint16_t port_id, struct rte_dev_eeprom_info *info)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (info == NULL)
		return -EINVAL;

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_eeprom, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->set_eeprom)(dev, info));
}

int
rte_eth_dev_get_module_info(uint16_t port_id,
			    struct rte_eth_dev_module_info *modinfo)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (modinfo == NULL)
		return -EINVAL;

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->get_module_info, -ENOTSUP);
	return (*dev->dev_ops->get_module_info)(dev, modinfo);
}

int
rte_eth_dev_get_module_eeprom(uint16_t port_id,
			      struct rte_dev_eeprom_info *info)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (info == NULL || info->data == NULL || info->length == 0)
		return -EINVAL;

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->get_module_eeprom, -ENOTSUP);
	return (*dev->dev_ops->get_module_eeprom)(dev, info);
}

int
rte_eth_dev_get_dcb_info(uint16_t port_id,
			     struct rte_eth_dcb_info *dcb_info)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	memset(dcb_info, 0, sizeof(struct rte_eth_dcb_info));

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->get_dcb_info, -ENOTSUP);
	return eth_err(port_id, (*dev->dev_ops->get_dcb_info)(dev, dcb_info));
}

static void
eth_dev_adjust_nb_desc(uint16_t *nb_desc,
		const struct rte_eth_desc_lim *desc_lim)
{
	if (desc_lim->nb_align != 0)
		*nb_desc = RTE_ALIGN_CEIL(*nb_desc, desc_lim->nb_align);

	if (desc_lim->nb_max != 0)
		*nb_desc = RTE_MIN(*nb_desc, desc_lim->nb_max);

	*nb_desc = RTE_MAX(*nb_desc, desc_lim->nb_min);
}

int
rte_eth_dev_adjust_nb_rx_tx_desc(uint16_t port_id,
				 uint16_t *nb_rx_desc,
				 uint16_t *nb_tx_desc)
{
	struct rte_eth_dev_info dev_info;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	if (nb_rx_desc != NULL)
		eth_dev_adjust_nb_desc(nb_rx_desc, &dev_info.rx_desc_lim);

	if (nb_tx_desc != NULL)
		eth_dev_adjust_nb_desc(nb_tx_desc, &dev_info.tx_desc_lim);

	return 0;
}

int
rte_eth_dev_hairpin_capability_get(uint16_t port_id,
				   struct rte_eth_hairpin_cap *cap)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->hairpin_cap_get, -ENOTSUP);
	memset(cap, 0, sizeof(*cap));
	return eth_err(port_id, (*dev->dev_ops->hairpin_cap_get)(dev, cap));
}

int
rte_eth_dev_is_rx_hairpin_queue(struct rte_eth_dev *dev, uint16_t queue_id)
{
	if (dev->data->rx_queue_state[queue_id] ==
	    RTE_ETH_QUEUE_STATE_HAIRPIN)
		return 1;
	return 0;
}

int
rte_eth_dev_is_tx_hairpin_queue(struct rte_eth_dev *dev, uint16_t queue_id)
{
	if (dev->data->tx_queue_state[queue_id] ==
	    RTE_ETH_QUEUE_STATE_HAIRPIN)
		return 1;
	return 0;
}

int
rte_eth_dev_pool_ops_supported(uint16_t port_id, const char *pool)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (pool == NULL)
		return -EINVAL;

	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->pool_ops_supported == NULL)
		return 1; /* all pools are supported */

	return (*dev->dev_ops->pool_ops_supported)(dev, pool);
}

/**
 * A set of values to describe the possible states of a switch domain.
 */
enum rte_eth_switch_domain_state {
	RTE_ETH_SWITCH_DOMAIN_UNUSED = 0,
	RTE_ETH_SWITCH_DOMAIN_ALLOCATED
};

/**
 * Array of switch domains available for allocation. Array is sized to
 * RTE_MAX_ETHPORTS elements as there cannot be more active switch domains than
 * ethdev ports in a single process.
 */
static struct rte_eth_dev_switch {
	enum rte_eth_switch_domain_state state;
} eth_dev_switch_domains[RTE_MAX_ETHPORTS];

int
rte_eth_switch_domain_alloc(uint16_t *domain_id)
{
	uint16_t i;

	*domain_id = RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (eth_dev_switch_domains[i].state ==
			RTE_ETH_SWITCH_DOMAIN_UNUSED) {
			eth_dev_switch_domains[i].state =
				RTE_ETH_SWITCH_DOMAIN_ALLOCATED;
			*domain_id = i;
			return 0;
		}
	}

	return -ENOSPC;
}

int
rte_eth_switch_domain_free(uint16_t domain_id)
{
	if (domain_id == RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID ||
		domain_id >= RTE_MAX_ETHPORTS)
		return -EINVAL;

	if (eth_dev_switch_domains[domain_id].state !=
		RTE_ETH_SWITCH_DOMAIN_ALLOCATED)
		return -EINVAL;

	eth_dev_switch_domains[domain_id].state = RTE_ETH_SWITCH_DOMAIN_UNUSED;

	return 0;
}

static int
eth_dev_devargs_tokenise(struct rte_kvargs *arglist, const char *str_in)
{
	int state;
	struct rte_kvargs_pair *pair;
	char *letter;

	arglist->str = strdup(str_in);
	if (arglist->str == NULL)
		return -ENOMEM;

	letter = arglist->str;
	state = 0;
	arglist->count = 0;
	pair = &arglist->pairs[0];
	while (1) {
		switch (state) {
		case 0: /* Initial */
			if (*letter == '=')
				return -EINVAL;
			else if (*letter == '\0')
				return 0;

			state = 1;
			pair->key = letter;
			/* fall-thru */

		case 1: /* Parsing key */
			if (*letter == '=') {
				*letter = '\0';
				pair->value = letter + 1;
				state = 2;
			} else if (*letter == ',' || *letter == '\0')
				return -EINVAL;
			break;


		case 2: /* Parsing value */
			if (*letter == '[')
				state = 3;
			else if (*letter == ',') {
				*letter = '\0';
				arglist->count++;
				pair = &arglist->pairs[arglist->count];
				state = 0;
			} else if (*letter == '\0') {
				letter--;
				arglist->count++;
				pair = &arglist->pairs[arglist->count];
				state = 0;
			}
			break;

		case 3: /* Parsing list */
			if (*letter == ']')
				state = 2;
			else if (*letter == '\0')
				return -EINVAL;
			break;
		}
		letter++;
	}
}

int
rte_eth_devargs_parse(const char *dargs, struct rte_eth_devargs *eth_da)
{
	struct rte_kvargs args;
	struct rte_kvargs_pair *pair;
	unsigned int i;
	int result = 0;

	memset(eth_da, 0, sizeof(*eth_da));

	result = eth_dev_devargs_tokenise(&args, dargs);
	if (result < 0)
		goto parse_cleanup;

	for (i = 0; i < args.count; i++) {
		pair = &args.pairs[i];
		if (strcmp("representor", pair->key) == 0) {
			result = rte_eth_devargs_parse_list(pair->value,
				rte_eth_devargs_parse_representor_ports,
				eth_da);
			if (result < 0)
				goto parse_cleanup;
		}
	}

parse_cleanup:
	if (args.str)
		free(args.str);

	return result;
}

static int
eth_dev_handle_port_list(const char *cmd __rte_unused,
		const char *params __rte_unused,
		struct rte_tel_data *d)
{
	int port_id;

	rte_tel_data_start_array(d, RTE_TEL_INT_VAL);
	RTE_ETH_FOREACH_DEV(port_id)
		rte_tel_data_add_array_int(d, port_id);
	return 0;
}

static void
eth_dev_add_port_queue_stats(struct rte_tel_data *d, uint64_t *q_stats,
		const char *stat_name)
{
	int q;
	struct rte_tel_data *q_data = rte_tel_data_alloc();
	rte_tel_data_start_array(q_data, RTE_TEL_U64_VAL);
	for (q = 0; q < RTE_ETHDEV_QUEUE_STAT_CNTRS; q++)
		rte_tel_data_add_array_u64(q_data, q_stats[q]);
	rte_tel_data_add_dict_container(d, stat_name, q_data, 0);
}

#define ADD_DICT_STAT(stats, s) rte_tel_data_add_dict_u64(d, #s, stats.s)

static int
eth_dev_handle_port_stats(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	struct rte_eth_stats stats;
	int port_id, ret;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	port_id = atoi(params);
	if (!rte_eth_dev_is_valid_port(port_id))
		return -1;

	ret = rte_eth_stats_get(port_id, &stats);
	if (ret < 0)
		return -1;

	rte_tel_data_start_dict(d);
	ADD_DICT_STAT(stats, ipackets);
	ADD_DICT_STAT(stats, opackets);
	ADD_DICT_STAT(stats, ibytes);
	ADD_DICT_STAT(stats, obytes);
	ADD_DICT_STAT(stats, imissed);
	ADD_DICT_STAT(stats, ierrors);
	ADD_DICT_STAT(stats, oerrors);
	ADD_DICT_STAT(stats, rx_nombuf);
	eth_dev_add_port_queue_stats(d, stats.q_ipackets, "q_ipackets");
	eth_dev_add_port_queue_stats(d, stats.q_opackets, "q_opackets");
	eth_dev_add_port_queue_stats(d, stats.q_ibytes, "q_ibytes");
	eth_dev_add_port_queue_stats(d, stats.q_obytes, "q_obytes");
	eth_dev_add_port_queue_stats(d, stats.q_errors, "q_errors");

	return 0;
}

static int
eth_dev_handle_port_xstats(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	struct rte_eth_xstat *eth_xstats;
	struct rte_eth_xstat_name *xstat_names;
	int port_id, num_xstats;
	int i, ret;
	char *end_param;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	port_id = strtoul(params, &end_param, 0);
	if (*end_param != '\0')
		RTE_ETHDEV_LOG(NOTICE,
			"Extra parameters passed to ethdev telemetry command, ignoring");
	if (!rte_eth_dev_is_valid_port(port_id))
		return -1;

	num_xstats = rte_eth_xstats_get(port_id, NULL, 0);
	if (num_xstats < 0)
		return -1;

	/* use one malloc for both names and stats */
	eth_xstats = malloc((sizeof(struct rte_eth_xstat) +
			sizeof(struct rte_eth_xstat_name)) * num_xstats);
	if (eth_xstats == NULL)
		return -1;
	xstat_names = (void *)&eth_xstats[num_xstats];

	ret = rte_eth_xstats_get_names(port_id, xstat_names, num_xstats);
	if (ret < 0 || ret > num_xstats) {
		free(eth_xstats);
		return -1;
	}

	ret = rte_eth_xstats_get(port_id, eth_xstats, num_xstats);
	if (ret < 0 || ret > num_xstats) {
		free(eth_xstats);
		return -1;
	}

	rte_tel_data_start_dict(d);
	for (i = 0; i < num_xstats; i++)
		rte_tel_data_add_dict_u64(d, xstat_names[i].name,
				eth_xstats[i].value);
	return 0;
}

static int
eth_dev_handle_port_link_status(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *d)
{
	static const char *status_str = "status";
	int ret, port_id;
	struct rte_eth_link link;
	char *end_param;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	port_id = strtoul(params, &end_param, 0);
	if (*end_param != '\0')
		RTE_ETHDEV_LOG(NOTICE,
			"Extra parameters passed to ethdev telemetry command, ignoring");
	if (!rte_eth_dev_is_valid_port(port_id))
		return -1;

	ret = rte_eth_link_get_nowait(port_id, &link);
	if (ret < 0)
		return -1;

	rte_tel_data_start_dict(d);
	if (!link.link_status) {
		rte_tel_data_add_dict_string(d, status_str, "DOWN");
		return 0;
	}
	rte_tel_data_add_dict_string(d, status_str, "UP");
	rte_tel_data_add_dict_u64(d, "speed", link.link_speed);
	rte_tel_data_add_dict_string(d, "duplex",
			(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
				"full-duplex" : "half-duplex");
	return 0;
}

int
rte_eth_hairpin_queue_peer_update(uint16_t peer_port, uint16_t peer_queue,
				  struct rte_hairpin_peer_info *cur_info,
				  struct rte_hairpin_peer_info *peer_info,
				  uint32_t direction)
{
	struct rte_eth_dev *dev;

	/* Current queue information is not mandatory. */
	if (peer_info == NULL)
		return -EINVAL;

	/* No need to check the validity again. */
	dev = &rte_eth_devices[peer_port];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->hairpin_queue_peer_update,
				-ENOTSUP);

	return (*dev->dev_ops->hairpin_queue_peer_update)(dev, peer_queue,
					cur_info, peer_info, direction);
}

int
rte_eth_hairpin_queue_peer_bind(uint16_t cur_port, uint16_t cur_queue,
				struct rte_hairpin_peer_info *peer_info,
				uint32_t direction)
{
	struct rte_eth_dev *dev;

	if (peer_info == NULL)
		return -EINVAL;

	/* No need to check the validity again. */
	dev = &rte_eth_devices[cur_port];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->hairpin_queue_peer_bind,
				-ENOTSUP);

	return (*dev->dev_ops->hairpin_queue_peer_bind)(dev, cur_queue,
							peer_info, direction);
}

int
rte_eth_hairpin_queue_peer_unbind(uint16_t cur_port, uint16_t cur_queue,
				  uint32_t direction)
{
	struct rte_eth_dev *dev;

	/* No need to check the validity again. */
	dev = &rte_eth_devices[cur_port];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->hairpin_queue_peer_unbind,
				-ENOTSUP);

	return (*dev->dev_ops->hairpin_queue_peer_unbind)(dev, cur_queue,
							  direction);
}

RTE_LOG_REGISTER(rte_eth_dev_logtype, lib.ethdev, INFO);

RTE_INIT(ethdev_init_telemetry)
{
	rte_telemetry_register_cmd("/ethdev/list", eth_dev_handle_port_list,
			"Returns list of available ethdev ports. Takes no parameters");
	rte_telemetry_register_cmd("/ethdev/stats", eth_dev_handle_port_stats,
			"Returns the common stats for a port. Parameters: int port_id");
	rte_telemetry_register_cmd("/ethdev/xstats", eth_dev_handle_port_xstats,
			"Returns the extended stats for a port. Parameters: int port_id");
	rte_telemetry_register_cmd("/ethdev/link_status",
			eth_dev_handle_port_link_status,
			"Returns the link status for a port. Parameters: int port_id");
}
