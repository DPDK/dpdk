/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include <bus_driver.h>
#include <rte_log.h>
#include <rte_interrupts.h>
#include <rte_kvargs.h>
#include <rte_memcpy.h>
#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_errno.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>
#include <rte_class.h>
#include <rte_ether.h>
#include <rte_telemetry.h>

#include "rte_ethdev.h"
#include "rte_ethdev_trace_fp.h"
#include "ethdev_driver.h"
#include "rte_flow_driver.h"
#include "ethdev_profile.h"
#include "ethdev_private.h"
#include "ethdev_trace.h"
#include "sff_telemetry.h"

struct rte_eth_dev rte_eth_devices[RTE_MAX_ETHPORTS];

/* public fast-path API */
struct rte_eth_fp_ops rte_eth_fp_ops[RTE_MAX_ETHPORTS];

/* spinlock for add/remove Rx callbacks */
static rte_spinlock_t eth_dev_rx_cb_lock = RTE_SPINLOCK_INITIALIZER;

/* spinlock for add/remove Tx callbacks */
static rte_spinlock_t eth_dev_tx_cb_lock = RTE_SPINLOCK_INITIALIZER;

/* store statistics names and its offset in stats structure  */
struct rte_eth_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned offset;
};

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
	RTE_RX_OFFLOAD_BIT2STR(VLAN_FILTER),
	RTE_RX_OFFLOAD_BIT2STR(VLAN_EXTEND),
	RTE_RX_OFFLOAD_BIT2STR(SCATTER),
	RTE_RX_OFFLOAD_BIT2STR(TIMESTAMP),
	RTE_RX_OFFLOAD_BIT2STR(SECURITY),
	RTE_RX_OFFLOAD_BIT2STR(KEEP_CRC),
	RTE_RX_OFFLOAD_BIT2STR(SCTP_CKSUM),
	RTE_RX_OFFLOAD_BIT2STR(OUTER_UDP_CKSUM),
	RTE_RX_OFFLOAD_BIT2STR(RSS_HASH),
	RTE_RX_OFFLOAD_BIT2STR(BUFFER_SPLIT),
};

#undef RTE_RX_OFFLOAD_BIT2STR
#undef RTE_ETH_RX_OFFLOAD_BIT2STR

#define RTE_TX_OFFLOAD_BIT2STR(_name)	\
	{ RTE_ETH_TX_OFFLOAD_##_name, #_name }

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

static const struct {
	uint64_t offload;
	const char *name;
} rte_eth_dev_capa_names[] = {
	{RTE_ETH_DEV_CAPA_RUNTIME_RX_QUEUE_SETUP, "RUNTIME_RX_QUEUE_SETUP"},
	{RTE_ETH_DEV_CAPA_RUNTIME_TX_QUEUE_SETUP, "RUNTIME_TX_QUEUE_SETUP"},
	{RTE_ETH_DEV_CAPA_RXQ_SHARE, "RXQ_SHARE"},
	{RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP, "FLOW_RULE_KEEP"},
	{RTE_ETH_DEV_CAPA_FLOW_SHARED_OBJECT_KEEP, "FLOW_SHARED_OBJECT_KEEP"},
};

enum {
	STAT_QMAP_TX = 0,
	STAT_QMAP_RX
};

static const struct {
	enum rte_eth_hash_function algo;
	const char *name;
} rte_eth_dev_rss_algo_names[] = {
	{RTE_ETH_HASH_FUNCTION_DEFAULT, "default"},
	{RTE_ETH_HASH_FUNCTION_SIMPLE_XOR, "simple_xor"},
	{RTE_ETH_HASH_FUNCTION_TOEPLITZ, "toeplitz"},
	{RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ, "symmetric_toeplitz"},
	{RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ_SORT, "symmetric_toeplitz_sort"},
};

int
rte_eth_iterator_init(struct rte_dev_iterator *iter, const char *devargs_str)
{
	int ret;
	struct rte_devargs devargs;
	const char *bus_param_key;
	char *bus_str = NULL;
	char *cls_str = NULL;
	int str_size;

	if (iter == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot initialize NULL iterator\n");
		return -EINVAL;
	}

	if (devargs_str == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot initialize iterator from NULL device description string\n");
		return -EINVAL;
	}

	memset(iter, 0, sizeof(*iter));
	memset(&devargs, 0, sizeof(devargs));

	/*
	 * The devargs string may use various syntaxes:
	 *   - 0000:08:00.0,representor=[1-3]
	 *   - pci:0000:06:00.0,representor=[0,5]
	 *   - class=eth,mac=00:11:22:33:44:55
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
	rte_devargs_reset(&devargs);

	rte_eth_trace_iterator_init(devargs_str);

	return 0;

error:
	if (ret == -ENOTSUP)
		RTE_ETHDEV_LOG(ERR, "Bus %s does not support iterating.\n",
				iter->bus->name);
	rte_devargs_reset(&devargs);
	free(bus_str);
	free(cls_str);
	return ret;
}

uint16_t
rte_eth_iterator_next(struct rte_dev_iterator *iter)
{
	if (iter == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get next device from NULL iterator\n");
		return RTE_MAX_ETHPORTS;
	}

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
		if (iter->class_device != NULL) {
			uint16_t id = eth_dev_to_id(iter->class_device);

			rte_eth_trace_iterator_next(iter, id);

			return id; /* match */
		}
	} while (iter->bus != NULL); /* need to try next rte_device */

	/* No more ethdev port to iterate. */
	rte_eth_iterator_cleanup(iter);
	return RTE_MAX_ETHPORTS;
}

void
rte_eth_iterator_cleanup(struct rte_dev_iterator *iter)
{
	if (iter == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot do clean up from NULL iterator\n");
		return;
	}

	if (iter->bus_str == NULL)
		return; /* nothing to free in pure class filter */

	rte_eth_trace_iterator_cleanup(iter);

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

	rte_eth_trace_find_next(port_id);

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

	rte_eth_trace_find_next_of(port_id, parent);

	return port_id;
}

uint16_t
rte_eth_find_next_sibling(uint16_t port_id, uint16_t ref_port_id)
{
	uint16_t ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(ref_port_id, RTE_MAX_ETHPORTS);
	ret = rte_eth_find_next_of(port_id,
			rte_eth_devices[ref_port_id].device);

	rte_eth_trace_find_next_sibling(port_id, ref_port_id, ret);

	return ret;
}

static bool
eth_dev_is_allocated(const struct rte_eth_dev *ethdev)
{
	return ethdev->data != NULL && ethdev->data->name[0] != '\0';
}

int
rte_eth_dev_is_valid_port(uint16_t port_id)
{
	int is_valid;

	if (port_id >= RTE_MAX_ETHPORTS ||
	    (rte_eth_devices[port_id].state == RTE_ETH_DEV_UNUSED))
		is_valid = 0;
	else
		is_valid = 1;

	rte_ethdev_trace_is_valid_port(port_id, is_valid);

	return is_valid;
}

static int
eth_is_valid_owner_id(uint64_t owner_id)
	__rte_exclusive_locks_required(rte_mcfg_ethdev_get_lock())
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

	rte_eth_trace_find_next_owned_by(port_id, owner_id);

	return port_id;
}

int
rte_eth_dev_owner_new(uint64_t *owner_id)
{
	int ret;

	if (owner_id == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot get new owner ID to NULL\n");
		return -EINVAL;
	}

	rte_spinlock_lock(rte_mcfg_ethdev_get_lock());

	if (eth_dev_shared_data_prepare() != NULL) {
		*owner_id = eth_dev_shared_data->next_owner_id++;
		eth_dev_shared_data->allocated_owners++;
		ret = 0;
	} else {
		ret = -ENOMEM;
	}

	rte_spinlock_unlock(rte_mcfg_ethdev_get_lock());

	rte_ethdev_trace_owner_new(*owner_id, ret);

	return ret;
}

static int
eth_dev_owner_set(const uint16_t port_id, const uint64_t old_owner_id,
		       const struct rte_eth_dev_owner *new_owner)
	__rte_exclusive_locks_required(rte_mcfg_ethdev_get_lock())
{
	struct rte_eth_dev *ethdev = &rte_eth_devices[port_id];
	struct rte_eth_dev_owner *port_owner;

	if (port_id >= RTE_MAX_ETHPORTS || !eth_dev_is_allocated(ethdev)) {
		RTE_ETHDEV_LOG(ERR, "Port ID %"PRIu16" is not allocated\n",
			port_id);
		return -ENODEV;
	}

	if (new_owner == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot set ethdev port %u owner from NULL owner\n",
			port_id);
		return -EINVAL;
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

	rte_spinlock_lock(rte_mcfg_ethdev_get_lock());

	if (eth_dev_shared_data_prepare() != NULL)
		ret = eth_dev_owner_set(port_id, RTE_ETH_DEV_NO_OWNER, owner);
	else
		ret = -ENOMEM;

	rte_spinlock_unlock(rte_mcfg_ethdev_get_lock());

	rte_ethdev_trace_owner_set(port_id, owner, ret);

	return ret;
}

int
rte_eth_dev_owner_unset(const uint16_t port_id, const uint64_t owner_id)
{
	const struct rte_eth_dev_owner new_owner = (struct rte_eth_dev_owner)
			{.id = RTE_ETH_DEV_NO_OWNER, .name = ""};
	int ret;

	rte_spinlock_lock(rte_mcfg_ethdev_get_lock());

	if (eth_dev_shared_data_prepare() != NULL)
		ret = eth_dev_owner_set(port_id, owner_id, &new_owner);
	else
		ret = -ENOMEM;

	rte_spinlock_unlock(rte_mcfg_ethdev_get_lock());

	rte_ethdev_trace_owner_unset(port_id, owner_id, ret);

	return ret;
}

int
rte_eth_dev_owner_delete(const uint64_t owner_id)
{
	uint16_t port_id;
	int ret = 0;

	rte_spinlock_lock(rte_mcfg_ethdev_get_lock());

	if (eth_dev_shared_data_prepare() == NULL) {
		ret = -ENOMEM;
	} else if (eth_is_valid_owner_id(owner_id)) {
		for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++) {
			struct rte_eth_dev_data *data =
				rte_eth_devices[port_id].data;
			if (data != NULL && data->owner.id == owner_id)
				memset(&data->owner, 0,
				       sizeof(struct rte_eth_dev_owner));
		}
		RTE_ETHDEV_LOG(NOTICE,
			"All port owners owned by %016"PRIx64" identifier have removed\n",
			owner_id);
		eth_dev_shared_data->allocated_owners--;
		eth_dev_shared_data_release();
	} else {
		RTE_ETHDEV_LOG(ERR,
			       "Invalid owner ID=%016"PRIx64"\n",
			       owner_id);
		ret = -EINVAL;
	}

	rte_spinlock_unlock(rte_mcfg_ethdev_get_lock());

	rte_ethdev_trace_owner_delete(owner_id, ret);

	return ret;
}

int
rte_eth_dev_owner_get(const uint16_t port_id, struct rte_eth_dev_owner *owner)
{
	struct rte_eth_dev *ethdev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	ethdev = &rte_eth_devices[port_id];

	if (!eth_dev_is_allocated(ethdev)) {
		RTE_ETHDEV_LOG(ERR, "Port ID %"PRIu16" is not allocated\n",
			port_id);
		return -ENODEV;
	}

	if (owner == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot get ethdev port %u owner to NULL\n",
			port_id);
		return -EINVAL;
	}

	rte_spinlock_lock(rte_mcfg_ethdev_get_lock());

	if (eth_dev_shared_data_prepare() != NULL) {
		rte_memcpy(owner, &ethdev->data->owner, sizeof(*owner));
		ret = 0;
	} else {
		ret = -ENOMEM;
	}

	rte_spinlock_unlock(rte_mcfg_ethdev_get_lock());

	rte_ethdev_trace_owner_get(port_id, owner, ret);

	return ret;
}

int
rte_eth_dev_socket_id(uint16_t port_id)
{
	int socket_id = SOCKET_ID_ANY;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		rte_errno = EINVAL;
	} else {
		socket_id = rte_eth_devices[port_id].data->numa_node;
		if (socket_id == SOCKET_ID_ANY)
			rte_errno = 0;
	}

	rte_ethdev_trace_socket_id(port_id, socket_id);

	return socket_id;
}

void *
rte_eth_dev_get_sec_ctx(uint16_t port_id)
{
	void *ctx;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, NULL);
	ctx = rte_eth_devices[port_id].security_ctx;

	rte_ethdev_trace_get_sec_ctx(port_id, ctx);

	return ctx;
}

uint16_t
rte_eth_dev_count_avail(void)
{
	uint16_t p;
	uint16_t count;

	count = 0;

	RTE_ETH_FOREACH_DEV(p)
		count++;

	rte_ethdev_trace_count_avail(count);

	return count;
}

uint16_t
rte_eth_dev_count_total(void)
{
	uint16_t port, count = 0;

	RTE_ETH_FOREACH_VALID_DEV(port)
		count++;

	rte_ethdev_trace_count_total(count);

	return count;
}

int
rte_eth_dev_get_name_by_port(uint16_t port_id, char *name)
{
	char *tmp;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (name == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot get ethdev port %u name to NULL\n",
			port_id);
		return -EINVAL;
	}

	rte_spinlock_lock(rte_mcfg_ethdev_get_lock());
	/* shouldn't check 'rte_eth_devices[i].data',
	 * because it might be overwritten by VDEV PMD */
	tmp = eth_dev_shared_data->data[port_id].name;
	rte_spinlock_unlock(rte_mcfg_ethdev_get_lock());

	strcpy(name, tmp);

	rte_ethdev_trace_get_name_by_port(port_id, name);

	return 0;
}

int
rte_eth_dev_get_port_by_name(const char *name, uint16_t *port_id)
{
	int ret = -ENODEV;
	uint16_t pid;

	if (name == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot get port ID from NULL name\n");
		return -EINVAL;
	}

	if (port_id == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get port ID to NULL for %s\n", name);
		return -EINVAL;
	}

	rte_spinlock_lock(rte_mcfg_ethdev_get_lock());
	RTE_ETH_FOREACH_VALID_DEV(pid) {
		if (strcmp(name, eth_dev_shared_data->data[pid].name) != 0)
			continue;

		*port_id = pid;
		rte_ethdev_trace_get_port_by_name(name, *port_id);
		ret = 0;
		break;
	}
	rte_spinlock_unlock(rte_mcfg_ethdev_get_lock());

	return ret;
}

int
eth_err(uint16_t port_id, int ret)
{
	if (ret == 0)
		return 0;
	if (rte_eth_dev_is_removed(port_id))
		return -EIO;
	return ret;
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
rte_eth_rx_queue_is_valid(uint16_t port_id, uint16_t queue_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	return eth_dev_validate_rx_queue(dev, queue_id);
}

int
rte_eth_tx_queue_is_valid(uint16_t port_id, uint16_t queue_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	return eth_dev_validate_tx_queue(dev, queue_id);
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

	if (*dev->dev_ops->rx_queue_start == NULL)
		return -ENOTSUP;

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

	ret = eth_err(port_id, dev->dev_ops->rx_queue_start(dev, rx_queue_id));

	rte_ethdev_trace_rx_queue_start(port_id, rx_queue_id, ret);

	return ret;
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

	if (*dev->dev_ops->rx_queue_stop == NULL)
		return -ENOTSUP;

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

	ret = eth_err(port_id, dev->dev_ops->rx_queue_stop(dev, rx_queue_id));

	rte_ethdev_trace_rx_queue_stop(port_id, rx_queue_id, ret);

	return ret;
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

	if (*dev->dev_ops->tx_queue_start == NULL)
		return -ENOTSUP;

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

	ret = eth_err(port_id, dev->dev_ops->tx_queue_start(dev, tx_queue_id));

	rte_ethdev_trace_tx_queue_start(port_id, tx_queue_id, ret);

	return ret;
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

	if (*dev->dev_ops->tx_queue_stop == NULL)
		return -ENOTSUP;

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

	ret = eth_err(port_id, dev->dev_ops->tx_queue_stop(dev, tx_queue_id));

	rte_ethdev_trace_tx_queue_stop(port_id, tx_queue_id, ret);

	return ret;
}

uint32_t
rte_eth_speed_bitflag(uint32_t speed, int duplex)
{
	uint32_t ret;

	switch (speed) {
	case RTE_ETH_SPEED_NUM_10M:
		ret = duplex ? RTE_ETH_LINK_SPEED_10M : RTE_ETH_LINK_SPEED_10M_HD;
		break;
	case RTE_ETH_SPEED_NUM_100M:
		ret = duplex ? RTE_ETH_LINK_SPEED_100M : RTE_ETH_LINK_SPEED_100M_HD;
		break;
	case RTE_ETH_SPEED_NUM_1G:
		ret = RTE_ETH_LINK_SPEED_1G;
		break;
	case RTE_ETH_SPEED_NUM_2_5G:
		ret = RTE_ETH_LINK_SPEED_2_5G;
		break;
	case RTE_ETH_SPEED_NUM_5G:
		ret = RTE_ETH_LINK_SPEED_5G;
		break;
	case RTE_ETH_SPEED_NUM_10G:
		ret = RTE_ETH_LINK_SPEED_10G;
		break;
	case RTE_ETH_SPEED_NUM_20G:
		ret = RTE_ETH_LINK_SPEED_20G;
		break;
	case RTE_ETH_SPEED_NUM_25G:
		ret = RTE_ETH_LINK_SPEED_25G;
		break;
	case RTE_ETH_SPEED_NUM_40G:
		ret = RTE_ETH_LINK_SPEED_40G;
		break;
	case RTE_ETH_SPEED_NUM_50G:
		ret = RTE_ETH_LINK_SPEED_50G;
		break;
	case RTE_ETH_SPEED_NUM_56G:
		ret = RTE_ETH_LINK_SPEED_56G;
		break;
	case RTE_ETH_SPEED_NUM_100G:
		ret = RTE_ETH_LINK_SPEED_100G;
		break;
	case RTE_ETH_SPEED_NUM_200G:
		ret = RTE_ETH_LINK_SPEED_200G;
		break;
	case RTE_ETH_SPEED_NUM_400G:
		ret = RTE_ETH_LINK_SPEED_400G;
		break;
	default:
		ret = 0;
	}

	rte_eth_trace_speed_bitflag(speed, duplex, ret);

	return ret;
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

	rte_ethdev_trace_rx_offload_name(offload, name);

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

	rte_ethdev_trace_tx_offload_name(offload, name);

	return name;
}

static char *
eth_dev_offload_names(uint64_t bitmask, char *buf, size_t size,
	const char *(*offload_name)(uint64_t))
{
	unsigned int pos = 0;
	int ret;

	/* There should be at least enough space to handle those cases */
	RTE_ASSERT(size >= sizeof("none") && size >= sizeof("..."));

	if (bitmask == 0) {
		ret = snprintf(&buf[pos], size - pos, "none");
		if (ret < 0 || pos + ret >= size)
			ret = 0;
		pos += ret;
		goto out;
	}

	while (bitmask != 0) {
		uint64_t offload = RTE_BIT64(rte_ctz64(bitmask));
		const char *name = offload_name(offload);

		ret = snprintf(&buf[pos], size - pos, "%s,", name);
		if (ret < 0 || pos + ret >= size) {
			if (pos + sizeof("...") >= size)
				pos = size - sizeof("...");
			ret = snprintf(&buf[pos], size - pos, "...");
			if (ret > 0 && pos + ret < size)
				pos += ret;
			goto out;
		}

		pos += ret;
		bitmask &= ~offload;
	}

	/* Eliminate trailing comma */
	pos--;
out:
	buf[pos] = '\0';
	return buf;
}

const char *
rte_eth_dev_capability_name(uint64_t capability)
{
	const char *name = "UNKNOWN";
	unsigned int i;

	for (i = 0; i < RTE_DIM(rte_eth_dev_capa_names); ++i) {
		if (capability == rte_eth_dev_capa_names[i].offload) {
			name = rte_eth_dev_capa_names[i].name;
			break;
		}
	}

	rte_ethdev_trace_capability_name(capability, name);

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
 * the offloads successfully set by the Ethernet device.
 *
 * @param port_id
 *   The port identifier of the Ethernet device.
 * @param req_offloads
 *   The offloads that have been requested through `rte_eth_dev_configure`.
 * @param set_offloads
 *   The offloads successfully set by the Ethernet device.
 * @param offload_type
 *   The offload type i.e. Rx/Tx string.
 * @param offload_name
 *   The function that prints the offload name.
 * @return
 *   - (0) if validation successful.
 *   - (-EINVAL) if requested offload has been silently disabled.
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
		offload = RTE_BIT64(rte_ctz64(offloads_diff));
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

static uint32_t
eth_dev_get_overhead_len(uint32_t max_rx_pktlen, uint16_t max_mtu)
{
	uint32_t overhead_len;

	if (max_mtu != UINT16_MAX && max_rx_pktlen > max_mtu)
		overhead_len = max_rx_pktlen - max_mtu;
	else
		overhead_len = RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN;

	return overhead_len;
}

/* rte_eth_dev_info_get() should be called prior to this function */
static int
eth_dev_validate_mtu(uint16_t port_id, struct rte_eth_dev_info *dev_info,
		uint16_t mtu)
{
	uint32_t overhead_len;
	uint32_t frame_size;

	if (mtu < dev_info->min_mtu) {
		RTE_ETHDEV_LOG(ERR,
			"MTU (%u) < device min MTU (%u) for port_id %u\n",
			mtu, dev_info->min_mtu, port_id);
		return -EINVAL;
	}
	if (mtu > dev_info->max_mtu) {
		RTE_ETHDEV_LOG(ERR,
			"MTU (%u) > device max MTU (%u) for port_id %u\n",
			mtu, dev_info->max_mtu, port_id);
		return -EINVAL;
	}

	overhead_len = eth_dev_get_overhead_len(dev_info->max_rx_pktlen,
			dev_info->max_mtu);
	frame_size = mtu + overhead_len;
	if (frame_size < RTE_ETHER_MIN_LEN) {
		RTE_ETHDEV_LOG(ERR,
			"Frame size (%u) < min frame size (%u) for port_id %u\n",
			frame_size, RTE_ETHER_MIN_LEN, port_id);
		return -EINVAL;
	}

	if (frame_size > dev_info->max_rx_pktlen) {
		RTE_ETHDEV_LOG(ERR,
			"Frame size (%u) > device max frame size (%u) for port_id %u\n",
			frame_size, dev_info->max_rx_pktlen, port_id);
		return -EINVAL;
	}

	return 0;
}

int
rte_eth_dev_configure(uint16_t port_id, uint16_t nb_rx_q, uint16_t nb_tx_q,
		      const struct rte_eth_conf *dev_conf)
{
	enum rte_eth_hash_function algorithm;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_conf orig_conf;
	int diag;
	int ret;
	uint16_t old_mtu;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (dev_conf == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot configure ethdev port %u from NULL config\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->dev_configure == NULL)
		return -ENOTSUP;

	if (dev->data->dev_started) {
		RTE_ETHDEV_LOG(ERR,
			"Port %u must be stopped to allow configuration\n",
			port_id);
		return -EBUSY;
	}

	/*
	 * Ensure that "dev_configured" is always 0 each time prepare to do
	 * dev_configure() to avoid any non-anticipated behaviour.
	 * And set to 1 when dev_configure() is executed successfully.
	 */
	dev->data->dev_configured = 0;

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

	/* fields must be zero to reserve them for future ABI changes */
	if (dev_conf->rxmode.reserved_64s[0] != 0 ||
	    dev_conf->rxmode.reserved_64s[1] != 0 ||
	    dev_conf->rxmode.reserved_ptrs[0] != NULL ||
	    dev_conf->rxmode.reserved_ptrs[1] != NULL) {
		RTE_ETHDEV_LOG(ERR, "Rxmode reserved fields not zero\n");
		ret = -EINVAL;
		goto rollback;
	}

	if (dev_conf->txmode.reserved_64s[0] != 0 ||
	    dev_conf->txmode.reserved_64s[1] != 0 ||
	    dev_conf->txmode.reserved_ptrs[0] != NULL ||
	    dev_conf->txmode.reserved_ptrs[1] != NULL) {
		RTE_ETHDEV_LOG(ERR, "txmode reserved fields not zero\n");
		ret = -EINVAL;
		goto rollback;
	}

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		goto rollback;

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
			"Number of Rx queues requested (%u) is greater than max supported(%d)\n",
			nb_rx_q, RTE_MAX_QUEUES_PER_PORT);
		ret = -EINVAL;
		goto rollback;
	}

	if (nb_tx_q > RTE_MAX_QUEUES_PER_PORT) {
		RTE_ETHDEV_LOG(ERR,
			"Number of Tx queues requested (%u) is greater than max supported(%d)\n",
			nb_tx_q, RTE_MAX_QUEUES_PER_PORT);
		ret = -EINVAL;
		goto rollback;
	}

	/*
	 * Check that the numbers of Rx and Tx queues are not greater
	 * than the maximum number of Rx and Tx queues supported by the
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

	if (dev_conf->rxmode.mtu == 0)
		dev->data->dev_conf.rxmode.mtu =
			(dev_info.max_mtu == 0) ? RTE_ETHER_MTU :
			RTE_MIN(dev_info.max_mtu, RTE_ETHER_MTU);

	ret = eth_dev_validate_mtu(port_id, &dev_info,
			dev->data->dev_conf.rxmode.mtu);
	if (ret != 0)
		goto rollback;

	dev->data->mtu = dev->data->dev_conf.rxmode.mtu;

	/*
	 * If LRO is enabled, check that the maximum aggregated packet
	 * size is supported by the configured device.
	 */
	if (dev_conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO) {
		uint32_t max_rx_pktlen;
		uint32_t overhead_len;

		overhead_len = eth_dev_get_overhead_len(dev_info.max_rx_pktlen,
				dev_info.max_mtu);
		max_rx_pktlen = dev->data->dev_conf.rxmode.mtu + overhead_len;
		if (dev_conf->rxmode.max_lro_pkt_size == 0)
			dev->data->dev_conf.rxmode.max_lro_pkt_size = max_rx_pktlen;
		ret = eth_dev_check_lro_pkt_size(port_id,
				dev->data->dev_conf.rxmode.max_lro_pkt_size,
				max_rx_pktlen,
				dev_info.max_lro_pkt_size);
		if (ret != 0)
			goto rollback;
	}

	/* Any requested offloading must be within its device capabilities */
	if ((dev_conf->rxmode.offloads & dev_info.rx_offload_capa) !=
	     dev_conf->rxmode.offloads) {
		char buffer[512];

		RTE_ETHDEV_LOG(ERR, "Ethdev port_id=%u does not support Rx offloads %s\n",
			port_id, eth_dev_offload_names(
			dev_conf->rxmode.offloads & ~dev_info.rx_offload_capa,
			buffer, sizeof(buffer), rte_eth_dev_rx_offload_name));
		RTE_ETHDEV_LOG(DEBUG, "Ethdev port_id=%u was requested Rx offloads %s\n",
			port_id, eth_dev_offload_names(dev_conf->rxmode.offloads,
			buffer, sizeof(buffer), rte_eth_dev_rx_offload_name));
		RTE_ETHDEV_LOG(DEBUG, "Ethdev port_id=%u supports Rx offloads %s\n",
			port_id, eth_dev_offload_names(dev_info.rx_offload_capa,
			buffer, sizeof(buffer), rte_eth_dev_rx_offload_name));

		ret = -EINVAL;
		goto rollback;
	}
	if ((dev_conf->txmode.offloads & dev_info.tx_offload_capa) !=
	     dev_conf->txmode.offloads) {
		char buffer[512];

		RTE_ETHDEV_LOG(ERR, "Ethdev port_id=%u does not support Tx offloads %s\n",
			port_id, eth_dev_offload_names(
			dev_conf->txmode.offloads & ~dev_info.tx_offload_capa,
			buffer, sizeof(buffer), rte_eth_dev_tx_offload_name));
		RTE_ETHDEV_LOG(DEBUG, "Ethdev port_id=%u was requested Tx offloads %s\n",
			port_id, eth_dev_offload_names(dev_conf->txmode.offloads,
			buffer, sizeof(buffer), rte_eth_dev_tx_offload_name));
		RTE_ETHDEV_LOG(DEBUG, "Ethdev port_id=%u supports Tx offloads %s\n",
			port_id, eth_dev_offload_names(dev_info.tx_offload_capa,
			buffer, sizeof(buffer), rte_eth_dev_tx_offload_name));
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
	if (((dev_conf->rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG) == 0) &&
	    (dev_conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_RSS_HASH)) {
		RTE_ETHDEV_LOG(ERR,
			"Ethdev port_id=%u config invalid Rx mq_mode without RSS but %s offload is requested\n",
			port_id,
			rte_eth_dev_rx_offload_name(RTE_ETH_RX_OFFLOAD_RSS_HASH));
		ret = -EINVAL;
		goto rollback;
	}

	if (dev_conf->rx_adv_conf.rss_conf.rss_key != NULL &&
	    dev_conf->rx_adv_conf.rss_conf.rss_key_len != dev_info.hash_key_size) {
		RTE_ETHDEV_LOG(ERR,
			"Ethdev port_id=%u invalid RSS key len: %u, valid value: %u\n",
			port_id, dev_conf->rx_adv_conf.rss_conf.rss_key_len,
			dev_info.hash_key_size);
		ret = -EINVAL;
		goto rollback;
	}

	algorithm = dev_conf->rx_adv_conf.rss_conf.algorithm;
	if ((size_t)algorithm >= CHAR_BIT * sizeof(dev_info.rss_algo_capa) ||
	    (dev_info.rss_algo_capa & RTE_ETH_HASH_ALGO_TO_CAPA(algorithm)) == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Ethdev port_id=%u configured RSS hash algorithm (%u)"
			"is not in the algorithm capability (0x%" PRIx32 ")\n",
			port_id, algorithm, dev_info.rss_algo_capa);
		ret = -EINVAL;
		goto rollback;
	}

	/*
	 * Setup new number of Rx/Tx queues and reconfigure device.
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

	dev->data->dev_configured = 1;
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
				if (pool_mask & UINT64_C(1))
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

	if (*dev->dev_ops->dev_start == NULL)
		return -ENOTSUP;

	if (dev->data->dev_configured == 0) {
		RTE_ETHDEV_LOG(INFO,
			"Device with port_id=%"PRIu16" is not configured.\n",
			port_id);
		return -EINVAL;
	}

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
		if (*dev->dev_ops->link_update == NULL)
			return -ENOTSUP;
		(*dev->dev_ops->link_update)(dev, 0);
	}

	/* expose selection of PMD fast-path functions */
	eth_dev_fp_ops_setup(rte_eth_fp_ops + port_id, dev);

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

	if (*dev->dev_ops->dev_stop == NULL)
		return -ENOTSUP;

	if (dev->data->dev_started == 0) {
		RTE_ETHDEV_LOG(INFO,
			"Device with port_id=%"PRIu16" already stopped\n",
			port_id);
		return 0;
	}

	/* point fast-path functions to dummy ones */
	eth_dev_fp_ops_reset(rte_eth_fp_ops + port_id);

	ret = (*dev->dev_ops->dev_stop)(dev);
	if (ret == 0)
		dev->data->dev_started = 0;
	rte_ethdev_trace_stop(port_id, ret);

	return ret;
}

int
rte_eth_dev_set_link_up(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->dev_set_link_up == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->dev_set_link_up)(dev));

	rte_ethdev_trace_set_link_up(port_id, ret);

	return ret;
}

int
rte_eth_dev_set_link_down(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->dev_set_link_down == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->dev_set_link_down)(dev));

	rte_ethdev_trace_set_link_down(port_id, ret);

	return ret;
}

int
rte_eth_dev_close(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int firsterr, binerr;
	int *lasterr = &firsterr;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	/*
	 * Secondary process needs to close device to release process private
	 * resources. But secondary process should not be obliged to wait
	 * for device stop before closing ethdev.
	 */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY &&
			dev->data->dev_started) {
		RTE_ETHDEV_LOG(ERR, "Cannot close started device (port %u)\n",
			       port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->dev_close == NULL)
		return -ENOTSUP;
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

	if (*dev->dev_ops->dev_reset == NULL)
		return -ENOTSUP;

	ret = rte_eth_dev_stop(port_id);
	if (ret != 0) {
		RTE_ETHDEV_LOG(ERR,
			"Failed to stop device (port %u) before reset: %s - ignore\n",
			port_id, rte_strerror(-ret));
	}
	ret = eth_err(port_id, dev->dev_ops->dev_reset(dev));

	rte_ethdev_trace_reset(port_id, ret);

	return ret;
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

	if (*dev->dev_ops->is_removed == NULL)
		return 0;

	ret = dev->dev_ops->is_removed(dev);
	if (ret != 0)
		/* Device is physically removed. */
		dev->state = RTE_ETH_DEV_REMOVED;

	rte_ethdev_trace_is_removed(port_id, ret);

	return ret;
}

static int
rte_eth_check_rx_mempool(struct rte_mempool *mp, uint16_t offset,
			 uint16_t min_length)
{
	uint16_t data_room_size;

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
	data_room_size = rte_pktmbuf_data_room_size(mp);
	if (data_room_size < offset + min_length) {
		RTE_ETHDEV_LOG(ERR,
			       "%s mbuf_data_room_size %u < %u (%u + %u)\n",
			       mp->name, data_room_size,
			       offset + min_length, offset, min_length);
		return -EINVAL;
	}
	return 0;
}

static int
eth_dev_buffer_split_get_supported_hdrs_helper(uint16_t port_id, uint32_t **ptypes)
{
	int cnt;

	cnt = rte_eth_buffer_split_get_supported_hdr_ptypes(port_id, NULL, 0);
	if (cnt <= 0)
		return cnt;

	*ptypes = malloc(sizeof(uint32_t) * cnt);
	if (*ptypes == NULL)
		return -ENOMEM;

	cnt = rte_eth_buffer_split_get_supported_hdr_ptypes(port_id, *ptypes, cnt);
	if (cnt <= 0) {
		free(*ptypes);
		*ptypes = NULL;
	}
	return cnt;
}

static int
rte_eth_rx_queue_check_split(uint16_t port_id,
			const struct rte_eth_rxseg_split *rx_seg,
			uint16_t n_seg, uint32_t *mbp_buf_size,
			const struct rte_eth_dev_info *dev_info)
{
	const struct rte_eth_rxseg_capa *seg_capa = &dev_info->rx_seg_capa;
	struct rte_mempool *mp_first;
	uint32_t offset_mask;
	uint16_t seg_idx;
	int ret = 0;
	int ptype_cnt;
	uint32_t *ptypes;
	uint32_t prev_proto_hdrs = RTE_PTYPE_UNKNOWN;
	int i;

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
	offset_mask = RTE_BIT32(seg_capa->offset_align_log2) - 1;

	ptypes = NULL;
	ptype_cnt = eth_dev_buffer_split_get_supported_hdrs_helper(port_id, &ptypes);

	for (seg_idx = 0; seg_idx < n_seg; seg_idx++) {
		struct rte_mempool *mpl = rx_seg[seg_idx].mp;
		uint32_t length = rx_seg[seg_idx].length;
		uint32_t offset = rx_seg[seg_idx].offset;
		uint32_t proto_hdr = rx_seg[seg_idx].proto_hdr;

		if (mpl == NULL) {
			RTE_ETHDEV_LOG(ERR, "null mempool pointer\n");
			ret = -EINVAL;
			goto out;
		}
		if (seg_idx != 0 && mp_first != mpl &&
		    seg_capa->multi_pools == 0) {
			RTE_ETHDEV_LOG(ERR, "Receiving to multiple pools is not supported\n");
			ret = -ENOTSUP;
			goto out;
		}
		if (offset != 0) {
			if (seg_capa->offset_allowed == 0) {
				RTE_ETHDEV_LOG(ERR, "Rx segmentation with offset is not supported\n");
				ret = -ENOTSUP;
				goto out;
			}
			if (offset & offset_mask) {
				RTE_ETHDEV_LOG(ERR, "Rx segmentation invalid offset alignment %u, %u\n",
					       offset,
					       seg_capa->offset_align_log2);
				ret = -EINVAL;
				goto out;
			}
		}

		offset += seg_idx != 0 ? 0 : RTE_PKTMBUF_HEADROOM;
		*mbp_buf_size = rte_pktmbuf_data_room_size(mpl);
		if (proto_hdr != 0) {
			/* Split based on protocol headers. */
			if (length != 0) {
				RTE_ETHDEV_LOG(ERR,
					"Do not set length split and protocol split within a segment\n"
					);
				ret = -EINVAL;
				goto out;
			}
			if ((proto_hdr & prev_proto_hdrs) != 0) {
				RTE_ETHDEV_LOG(ERR,
					"Repeat with previous protocol headers or proto-split after length-based split\n"
					);
				ret = -EINVAL;
				goto out;
			}
			if (ptype_cnt <= 0) {
				RTE_ETHDEV_LOG(ERR,
					"Port %u failed to get supported buffer split header protocols\n",
					port_id);
				ret = -ENOTSUP;
				goto out;
			}
			for (i = 0; i < ptype_cnt; i++) {
				if ((prev_proto_hdrs | proto_hdr) == ptypes[i])
					break;
			}
			if (i == ptype_cnt) {
				RTE_ETHDEV_LOG(ERR,
					"Requested Rx split header protocols 0x%x is not supported.\n",
					proto_hdr);
				ret = -EINVAL;
				goto out;
			}
			prev_proto_hdrs |= proto_hdr;
		} else {
			/* Split at fixed length. */
			length = length != 0 ? length : *mbp_buf_size;
			prev_proto_hdrs = RTE_PTYPE_ALL_MASK;
		}

		ret = rte_eth_check_rx_mempool(mpl, offset, length);
		if (ret != 0)
			goto out;
	}
out:
	free(ptypes);
	return ret;
}

static int
rte_eth_rx_queue_check_mempools(struct rte_mempool **rx_mempools,
			       uint16_t n_mempools, uint32_t *min_buf_size,
			       const struct rte_eth_dev_info *dev_info)
{
	uint16_t pool_idx;
	int ret;

	if (n_mempools > dev_info->max_rx_mempools) {
		RTE_ETHDEV_LOG(ERR,
			       "Too many Rx mempools %u vs maximum %u\n",
			       n_mempools, dev_info->max_rx_mempools);
		return -EINVAL;
	}

	for (pool_idx = 0; pool_idx < n_mempools; pool_idx++) {
		struct rte_mempool *mp = rx_mempools[pool_idx];

		if (mp == NULL) {
			RTE_ETHDEV_LOG(ERR, "null Rx mempool pointer\n");
			return -EINVAL;
		}

		ret = rte_eth_check_rx_mempool(mp, RTE_PKTMBUF_HEADROOM,
					       dev_info->min_rx_bufsize);
		if (ret != 0)
			return ret;

		*min_buf_size = RTE_MIN(*min_buf_size,
					rte_pktmbuf_data_room_size(mp));
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
	uint64_t rx_offloads;
	uint32_t mbp_buf_size = UINT32_MAX;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_rxconf local_conf;
	uint32_t buf_data_size;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (rx_queue_id >= dev->data->nb_rx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid Rx queue_id=%u\n", rx_queue_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->rx_queue_setup == NULL)
		return -ENOTSUP;

	if (rx_conf != NULL &&
	   (rx_conf->reserved_64s[0] != 0 ||
	    rx_conf->reserved_64s[1] != 0 ||
	    rx_conf->reserved_ptrs[0] != NULL ||
	    rx_conf->reserved_ptrs[1] != NULL)) {
		RTE_ETHDEV_LOG(ERR, "Rx conf reserved fields not zero\n");
		return -EINVAL;
	}

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	rx_offloads = dev->data->dev_conf.rxmode.offloads;
	if (rx_conf != NULL)
		rx_offloads |= rx_conf->offloads;

	/* Ensure that we have one and only one source of Rx buffers */
	if ((mp != NULL) +
	    (rx_conf != NULL && rx_conf->rx_nseg > 0) +
	    (rx_conf != NULL && rx_conf->rx_nmempool > 0) != 1) {
		RTE_ETHDEV_LOG(ERR,
			       "Ambiguous Rx mempools configuration\n");
		return -EINVAL;
	}

	if (mp != NULL) {
		/* Single pool configuration check. */
		ret = rte_eth_check_rx_mempool(mp, RTE_PKTMBUF_HEADROOM,
					       dev_info.min_rx_bufsize);
		if (ret != 0)
			return ret;

		mbp_buf_size = rte_pktmbuf_data_room_size(mp);
		buf_data_size = mbp_buf_size - RTE_PKTMBUF_HEADROOM;
		if (buf_data_size > dev_info.max_rx_bufsize)
			RTE_ETHDEV_LOG(DEBUG,
				"For port_id=%u, the mbuf data buffer size (%u) is bigger than "
				"max buffer size (%u) device can utilize, so mbuf size can be reduced.\n",
				port_id, buf_data_size, dev_info.max_rx_bufsize);
	} else if (rx_conf != NULL && rx_conf->rx_nseg > 0) {
		const struct rte_eth_rxseg_split *rx_seg;
		uint16_t n_seg;

		/* Extended multi-segment configuration check. */
		if (rx_conf->rx_seg == NULL) {
			RTE_ETHDEV_LOG(ERR,
				       "Memory pool is null and no multi-segment configuration provided\n");
			return -EINVAL;
		}

		rx_seg = (const struct rte_eth_rxseg_split *)rx_conf->rx_seg;
		n_seg = rx_conf->rx_nseg;

		if (rx_offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT) {
			ret = rte_eth_rx_queue_check_split(port_id, rx_seg, n_seg,
							   &mbp_buf_size,
							   &dev_info);
			if (ret != 0)
				return ret;
		} else {
			RTE_ETHDEV_LOG(ERR, "No Rx segmentation offload configured\n");
			return -EINVAL;
		}
	} else if (rx_conf != NULL && rx_conf->rx_nmempool > 0) {
		/* Extended multi-pool configuration check. */
		if (rx_conf->rx_mempools == NULL) {
			RTE_ETHDEV_LOG(ERR, "Memory pools array is null\n");
			return -EINVAL;
		}

		ret = rte_eth_rx_queue_check_mempools(rx_conf->rx_mempools,
						     rx_conf->rx_nmempool,
						     &mbp_buf_size,
						     &dev_info);
		if (ret != 0)
			return ret;
	} else {
		RTE_ETHDEV_LOG(ERR, "Missing Rx mempool configuration\n");
		return -EINVAL;
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

	eth_dev_rxq_release(dev, rx_queue_id);

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

	if (local_conf.share_group > 0 &&
	    (dev_info.dev_capa & RTE_ETH_DEV_CAPA_RXQ_SHARE) == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Ethdev port_id=%d rx_queue_id=%d, enabled share_group=%hu while device doesn't support Rx queue share\n",
			port_id, rx_queue_id, local_conf.share_group);
		return -EINVAL;
	}

	/*
	 * If LRO is enabled, check that the maximum aggregated packet
	 * size is supported by the configured device.
	 */
	/* Get the real Ethernet overhead length */
	if (local_conf.offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO) {
		uint32_t overhead_len;
		uint32_t max_rx_pktlen;
		int ret;

		overhead_len = eth_dev_get_overhead_len(dev_info.max_rx_pktlen,
				dev_info.max_mtu);
		max_rx_pktlen = dev->data->mtu + overhead_len;
		if (dev->data->dev_conf.rxmode.max_lro_pkt_size == 0)
			dev->data->dev_conf.rxmode.max_lro_pkt_size = max_rx_pktlen;
		ret = eth_dev_check_lro_pkt_size(port_id,
				dev->data->dev_conf.rxmode.max_lro_pkt_size,
				max_rx_pktlen,
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
	int i;
	int count;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (rx_queue_id >= dev->data->nb_rx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid Rx queue_id=%u\n", rx_queue_id);
		return -EINVAL;
	}

	if (conf == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot setup ethdev port %u Rx hairpin queue from NULL config\n",
			port_id);
		return -EINVAL;
	}

	if (conf->reserved != 0) {
		RTE_ETHDEV_LOG(ERR,
			       "Rx hairpin reserved field not zero\n");
		return -EINVAL;
	}

	ret = rte_eth_dev_hairpin_capability_get(port_id, &cap);
	if (ret != 0)
		return ret;
	if (*dev->dev_ops->rx_hairpin_queue_setup == NULL)
		return -ENOTSUP;
	/* if nb_rx_desc is zero use max number of desc from the driver. */
	if (nb_rx_desc == 0)
		nb_rx_desc = cap.max_nb_desc;
	if (nb_rx_desc > cap.max_nb_desc) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid value for nb_rx_desc(=%hu), should be: <= %hu\n",
			nb_rx_desc, cap.max_nb_desc);
		return -EINVAL;
	}
	if (conf->peer_count > cap.max_rx_2_tx) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid value for number of peers for Rx queue(=%u), should be: <= %hu\n",
			conf->peer_count, cap.max_rx_2_tx);
		return -EINVAL;
	}
	if (conf->use_locked_device_memory && !cap.rx_cap.locked_device_memory) {
		RTE_ETHDEV_LOG(ERR,
			"Attempt to use locked device memory for Rx queue, which is not supported\n");
		return -EINVAL;
	}
	if (conf->use_rte_memory && !cap.rx_cap.rte_memory) {
		RTE_ETHDEV_LOG(ERR,
			"Attempt to use DPDK memory for Rx queue, which is not supported\n");
		return -EINVAL;
	}
	if (conf->use_locked_device_memory && conf->use_rte_memory) {
		RTE_ETHDEV_LOG(ERR,
			"Attempt to use mutually exclusive memory settings for Rx queue\n");
		return -EINVAL;
	}
	if (conf->force_memory &&
	    !conf->use_locked_device_memory &&
	    !conf->use_rte_memory) {
		RTE_ETHDEV_LOG(ERR,
			"Attempt to force Rx queue memory settings, but none is set\n");
		return -EINVAL;
	}
	if (conf->peer_count == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid value for number of peers for Rx queue(=%u), should be: > 0\n",
			conf->peer_count);
		return -EINVAL;
	}
	for (i = 0, count = 0; i < dev->data->nb_rx_queues &&
	     cap.max_nb_queues != UINT16_MAX; i++) {
		if (i == rx_queue_id || rte_eth_dev_is_rx_hairpin_queue(dev, i))
			count++;
	}
	if (count > cap.max_nb_queues) {
		RTE_ETHDEV_LOG(ERR, "To many Rx hairpin queues max is %d\n",
		cap.max_nb_queues);
		return -EINVAL;
	}
	if (dev->data->dev_started)
		return -EBUSY;
	eth_dev_rxq_release(dev, rx_queue_id);
	ret = (*dev->dev_ops->rx_hairpin_queue_setup)(dev, rx_queue_id,
						      nb_rx_desc, conf);
	if (ret == 0)
		dev->data->rx_queue_state[rx_queue_id] =
			RTE_ETH_QUEUE_STATE_HAIRPIN;
	ret = eth_err(port_id, ret);

	rte_eth_trace_rx_hairpin_queue_setup(port_id, rx_queue_id, nb_rx_desc,
					     conf, ret);

	return ret;
}

int
rte_eth_tx_queue_setup(uint16_t port_id, uint16_t tx_queue_id,
		       uint16_t nb_tx_desc, unsigned int socket_id,
		       const struct rte_eth_txconf *tx_conf)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf local_conf;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (tx_queue_id >= dev->data->nb_tx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid Tx queue_id=%u\n", tx_queue_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->tx_queue_setup == NULL)
		return -ENOTSUP;

	if (tx_conf != NULL &&
	   (tx_conf->reserved_64s[0] != 0 ||
	    tx_conf->reserved_64s[1] != 0 ||
	    tx_conf->reserved_ptrs[0] != NULL ||
	    tx_conf->reserved_ptrs[1] != NULL)) {
		RTE_ETHDEV_LOG(ERR, "Tx conf reserved fields not zero\n");
		return -EINVAL;
	}

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

	eth_dev_txq_release(dev, tx_queue_id);

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
	int i;
	int count;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (tx_queue_id >= dev->data->nb_tx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid Tx queue_id=%u\n", tx_queue_id);
		return -EINVAL;
	}

	if (conf == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot setup ethdev port %u Tx hairpin queue from NULL config\n",
			port_id);
		return -EINVAL;
	}

	ret = rte_eth_dev_hairpin_capability_get(port_id, &cap);
	if (ret != 0)
		return ret;
	if (*dev->dev_ops->tx_hairpin_queue_setup == NULL)
		return -ENOTSUP;
	/* if nb_rx_desc is zero use max number of desc from the driver. */
	if (nb_tx_desc == 0)
		nb_tx_desc = cap.max_nb_desc;
	if (nb_tx_desc > cap.max_nb_desc) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid value for nb_tx_desc(=%hu), should be: <= %hu\n",
			nb_tx_desc, cap.max_nb_desc);
		return -EINVAL;
	}
	if (conf->peer_count > cap.max_tx_2_rx) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid value for number of peers for Tx queue(=%u), should be: <= %hu\n",
			conf->peer_count, cap.max_tx_2_rx);
		return -EINVAL;
	}
	if (conf->use_locked_device_memory && !cap.tx_cap.locked_device_memory) {
		RTE_ETHDEV_LOG(ERR,
			"Attempt to use locked device memory for Tx queue, which is not supported\n");
		return -EINVAL;
	}
	if (conf->use_rte_memory && !cap.tx_cap.rte_memory) {
		RTE_ETHDEV_LOG(ERR,
			"Attempt to use DPDK memory for Tx queue, which is not supported\n");
		return -EINVAL;
	}
	if (conf->use_locked_device_memory && conf->use_rte_memory) {
		RTE_ETHDEV_LOG(ERR,
			"Attempt to use mutually exclusive memory settings for Tx queue\n");
		return -EINVAL;
	}
	if (conf->force_memory &&
	    !conf->use_locked_device_memory &&
	    !conf->use_rte_memory) {
		RTE_ETHDEV_LOG(ERR,
			"Attempt to force Tx queue memory settings, but none is set\n");
		return -EINVAL;
	}
	if (conf->peer_count == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Invalid value for number of peers for Tx queue(=%u), should be: > 0\n",
			conf->peer_count);
		return -EINVAL;
	}
	for (i = 0, count = 0; i < dev->data->nb_tx_queues &&
	     cap.max_nb_queues != UINT16_MAX; i++) {
		if (i == tx_queue_id || rte_eth_dev_is_tx_hairpin_queue(dev, i))
			count++;
	}
	if (count > cap.max_nb_queues) {
		RTE_ETHDEV_LOG(ERR, "To many Tx hairpin queues max is %d\n",
		cap.max_nb_queues);
		return -EINVAL;
	}
	if (dev->data->dev_started)
		return -EBUSY;
	eth_dev_txq_release(dev, tx_queue_id);
	ret = (*dev->dev_ops->tx_hairpin_queue_setup)
		(dev, tx_queue_id, nb_tx_desc, conf);
	if (ret == 0)
		dev->data->tx_queue_state[tx_queue_id] =
			RTE_ETH_QUEUE_STATE_HAIRPIN;
	ret = eth_err(port_id, ret);

	rte_eth_trace_tx_hairpin_queue_setup(port_id, tx_queue_id, nb_tx_desc,
					     conf, ret);

	return ret;
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

	if (*dev->dev_ops->hairpin_bind == NULL)
		return -ENOTSUP;
	ret = (*dev->dev_ops->hairpin_bind)(dev, rx_port);
	if (ret != 0)
		RTE_ETHDEV_LOG(ERR, "Failed to bind hairpin Tx %d"
			       " to Rx %d (%d - all ports)\n",
			       tx_port, rx_port, RTE_MAX_ETHPORTS);

	rte_eth_trace_hairpin_bind(tx_port, rx_port, ret);

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

	if (*dev->dev_ops->hairpin_unbind == NULL)
		return -ENOTSUP;
	ret = (*dev->dev_ops->hairpin_unbind)(dev, rx_port);
	if (ret != 0)
		RTE_ETHDEV_LOG(ERR, "Failed to unbind hairpin Tx %d"
			       " from Rx %d (%d - all ports)\n",
			       tx_port, rx_port, RTE_MAX_ETHPORTS);

	rte_eth_trace_hairpin_unbind(tx_port, rx_port, ret);

	return ret;
}

int
rte_eth_hairpin_get_peer_ports(uint16_t port_id, uint16_t *peer_ports,
			       size_t len, uint32_t direction)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (peer_ports == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u hairpin peer ports to NULL\n",
			port_id);
		return -EINVAL;
	}

	if (len == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u hairpin peer ports to array with zero size\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->hairpin_get_peer_ports == NULL)
		return -ENOTSUP;

	ret = (*dev->dev_ops->hairpin_get_peer_ports)(dev, peer_ports,
						      len, direction);
	if (ret < 0)
		RTE_ETHDEV_LOG(ERR, "Failed to get %d hairpin peer %s ports\n",
			       port_id, direction ? "Rx" : "Tx");

	rte_eth_trace_hairpin_get_peer_ports(port_id, peer_ports, len,
					     direction, ret);

	return ret;
}

void
rte_eth_tx_buffer_drop_callback(struct rte_mbuf **pkts, uint16_t unsent,
		void *userdata __rte_unused)
{
	rte_pktmbuf_free_bulk(pkts, unsent);

	rte_eth_trace_tx_buffer_drop_callback((void **)pkts, unsent);
}

void
rte_eth_tx_buffer_count_callback(struct rte_mbuf **pkts, uint16_t unsent,
		void *userdata)
{
	uint64_t *count = userdata;

	rte_pktmbuf_free_bulk(pkts, unsent);
	*count += unsent;

	rte_eth_trace_tx_buffer_count_callback((void **)pkts, unsent, *count);
}

int
rte_eth_tx_buffer_set_err_callback(struct rte_eth_dev_tx_buffer *buffer,
		buffer_tx_error_fn cbfn, void *userdata)
{
	if (buffer == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot set Tx buffer error callback to NULL buffer\n");
		return -EINVAL;
	}

	buffer->error_callback = cbfn;
	buffer->error_userdata = userdata;

	rte_eth_trace_tx_buffer_set_err_callback(buffer);

	return 0;
}

int
rte_eth_tx_buffer_init(struct rte_eth_dev_tx_buffer *buffer, uint16_t size)
{
	int ret = 0;

	if (buffer == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot initialize NULL buffer\n");
		return -EINVAL;
	}

	buffer->size = size;
	if (buffer->error_callback == NULL) {
		ret = rte_eth_tx_buffer_set_err_callback(
			buffer, rte_eth_tx_buffer_drop_callback, NULL);
	}

	rte_eth_trace_tx_buffer_init(buffer, size, ret);

	return ret;
}

int
rte_eth_tx_done_cleanup(uint16_t port_id, uint16_t queue_id, uint32_t free_cnt)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->tx_done_cleanup == NULL)
		return -ENOTSUP;

	/* Call driver to free pending mbufs. */
	ret = (*dev->dev_ops->tx_done_cleanup)(dev->data->tx_queues[queue_id],
					       free_cnt);
	ret = eth_err(port_id, ret);

	rte_eth_trace_tx_done_cleanup(port_id, queue_id, free_cnt, ret);

	return ret;
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

	if (*dev->dev_ops->promiscuous_enable == NULL)
		return -ENOTSUP;

	diag = (*dev->dev_ops->promiscuous_enable)(dev);
	dev->data->promiscuous = (diag == 0) ? 1 : 0;

	diag = eth_err(port_id, diag);

	rte_eth_trace_promiscuous_enable(port_id, dev->data->promiscuous,
					 diag);

	return diag;
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

	if (*dev->dev_ops->promiscuous_disable == NULL)
		return -ENOTSUP;

	dev->data->promiscuous = 0;
	diag = (*dev->dev_ops->promiscuous_disable)(dev);
	if (diag != 0)
		dev->data->promiscuous = 1;

	diag = eth_err(port_id, diag);

	rte_eth_trace_promiscuous_disable(port_id, dev->data->promiscuous,
					  diag);

	return diag;
}

int
rte_eth_promiscuous_get(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	rte_eth_trace_promiscuous_get(port_id, dev->data->promiscuous);

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

	if (*dev->dev_ops->allmulticast_enable == NULL)
		return -ENOTSUP;
	diag = (*dev->dev_ops->allmulticast_enable)(dev);
	dev->data->all_multicast = (diag == 0) ? 1 : 0;

	diag = eth_err(port_id, diag);

	rte_eth_trace_allmulticast_enable(port_id, dev->data->all_multicast,
					  diag);

	return diag;
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

	if (*dev->dev_ops->allmulticast_disable == NULL)
		return -ENOTSUP;
	dev->data->all_multicast = 0;
	diag = (*dev->dev_ops->allmulticast_disable)(dev);
	if (diag != 0)
		dev->data->all_multicast = 1;

	diag = eth_err(port_id, diag);

	rte_eth_trace_allmulticast_disable(port_id, dev->data->all_multicast,
					   diag);

	return diag;
}

int
rte_eth_allmulticast_get(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	rte_eth_trace_allmulticast_get(port_id, dev->data->all_multicast);

	return dev->data->all_multicast;
}

int
rte_eth_link_get(uint16_t port_id, struct rte_eth_link *eth_link)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (eth_link == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot get ethdev port %u link to NULL\n",
			port_id);
		return -EINVAL;
	}

	if (dev->data->dev_conf.intr_conf.lsc && dev->data->dev_started)
		rte_eth_linkstatus_get(dev, eth_link);
	else {
		if (*dev->dev_ops->link_update == NULL)
			return -ENOTSUP;
		(*dev->dev_ops->link_update)(dev, 1);
		*eth_link = dev->data->dev_link;
	}

	rte_eth_trace_link_get(port_id, eth_link);

	return 0;
}

int
rte_eth_link_get_nowait(uint16_t port_id, struct rte_eth_link *eth_link)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (eth_link == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot get ethdev port %u link to NULL\n",
			port_id);
		return -EINVAL;
	}

	if (dev->data->dev_conf.intr_conf.lsc && dev->data->dev_started)
		rte_eth_linkstatus_get(dev, eth_link);
	else {
		if (*dev->dev_ops->link_update == NULL)
			return -ENOTSUP;
		(*dev->dev_ops->link_update)(dev, 0);
		*eth_link = dev->data->dev_link;
	}

	rte_eth_trace_link_get_nowait(port_id, eth_link);

	return 0;
}

const char *
rte_eth_link_speed_to_str(uint32_t link_speed)
{
	const char *ret;

	switch (link_speed) {
	case RTE_ETH_SPEED_NUM_NONE:
		ret = "None";
		break;
	case RTE_ETH_SPEED_NUM_10M:
		ret = "10 Mbps";
		break;
	case RTE_ETH_SPEED_NUM_100M:
		ret = "100 Mbps";
		break;
	case RTE_ETH_SPEED_NUM_1G:
		ret = "1 Gbps";
		break;
	case RTE_ETH_SPEED_NUM_2_5G:
		ret = "2.5 Gbps";
		break;
	case RTE_ETH_SPEED_NUM_5G:
		ret = "5 Gbps";
		break;
	case RTE_ETH_SPEED_NUM_10G:
		ret = "10 Gbps";
		break;
	case RTE_ETH_SPEED_NUM_20G:
		ret = "20 Gbps";
		break;
	case RTE_ETH_SPEED_NUM_25G:
		ret = "25 Gbps";
		break;
	case RTE_ETH_SPEED_NUM_40G:
		ret = "40 Gbps";
		break;
	case RTE_ETH_SPEED_NUM_50G:
		ret = "50 Gbps";
		break;
	case RTE_ETH_SPEED_NUM_56G:
		ret = "56 Gbps";
		break;
	case RTE_ETH_SPEED_NUM_100G:
		ret = "100 Gbps";
		break;
	case RTE_ETH_SPEED_NUM_200G:
		ret = "200 Gbps";
		break;
	case RTE_ETH_SPEED_NUM_400G:
		ret = "400 Gbps";
		break;
	case RTE_ETH_SPEED_NUM_UNKNOWN:
		ret = "Unknown";
		break;
	default:
		ret = "Invalid";
	}

	rte_eth_trace_link_speed_to_str(link_speed, ret);

	return ret;
}

int
rte_eth_link_to_str(char *str, size_t len, const struct rte_eth_link *eth_link)
{
	int ret;

	if (str == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot convert link to NULL string\n");
		return -EINVAL;
	}

	if (len == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot convert link to string with zero size\n");
		return -EINVAL;
	}

	if (eth_link == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot convert to string from NULL link\n");
		return -EINVAL;
	}

	if (eth_link->link_status == RTE_ETH_LINK_DOWN)
		ret = snprintf(str, len, "Link down");
	else
		ret = snprintf(str, len, "Link up at %s %s %s",
			rte_eth_link_speed_to_str(eth_link->link_speed),
			(eth_link->link_duplex == RTE_ETH_LINK_FULL_DUPLEX) ?
			"FDX" : "HDX",
			(eth_link->link_autoneg == RTE_ETH_LINK_AUTONEG) ?
			"Autoneg" : "Fixed");

	rte_eth_trace_link_to_str(len, eth_link, str, ret);

	return ret;
}

int
rte_eth_stats_get(uint16_t port_id, struct rte_eth_stats *stats)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (stats == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot get ethdev port %u stats to NULL\n",
			port_id);
		return -EINVAL;
	}

	memset(stats, 0, sizeof(*stats));

	if (*dev->dev_ops->stats_get == NULL)
		return -ENOTSUP;
	stats->rx_nombuf = dev->data->rx_mbuf_alloc_failed;
	ret = eth_err(port_id, (*dev->dev_ops->stats_get)(dev, stats));

	rte_eth_trace_stats_get(port_id, stats, ret);

	return ret;
}

int
rte_eth_stats_reset(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->stats_reset == NULL)
		return -ENOTSUP;
	ret = (*dev->dev_ops->stats_reset)(dev);
	if (ret != 0)
		return eth_err(port_id, ret);

	dev->data->rx_mbuf_alloc_failed = 0;

	rte_eth_trace_stats_reset(port_id);

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

	if (xstat_name == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u xstats ID from NULL xstat name\n",
			port_id);
		return -ENOMEM;
	}

	if (id == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u xstats ID to NULL\n",
			port_id);
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

			rte_eth_trace_xstats_get_id_by_name(port_id,
							    xstat_name, *id);

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
					ids_copy, xstats_names, size);
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

		rte_eth_trace_xstats_get_names_by_id(port_id, &xstats_names[i],
						     ids[i]);
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
	int i;

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

	for (i = 0; i < cnt_used_entries; i++)
		rte_eth_trace_xstats_get_names(port_id, i, &xstats_names[i],
					       size, cnt_used_entries);

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
	dev = &rte_eth_devices[port_id];

	ret = eth_dev_get_xstats_count(port_id);
	if (ret < 0)
		return ret;
	expected_entries = (uint16_t)ret;
	struct rte_eth_xstat xstats[expected_entries];
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

	rte_eth_trace_xstats_get_by_id(port_id, ids, values, size);

	return size;
}

int
rte_eth_xstats_get(uint16_t port_id, struct rte_eth_xstat *xstats,
	unsigned int n)
{
	struct rte_eth_dev *dev;
	unsigned int count, i;
	signed int xcount = 0;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (xstats == NULL && n > 0)
		return -EINVAL;
	dev = &rte_eth_devices[port_id];

	count = eth_dev_get_xstats_basic_count(dev);

	/* implemented by the driver */
	if (dev->dev_ops->xstats_get != NULL) {
		/* Retrieve the xstats from the driver at the end of the
		 * xstats struct.
		 */
		xcount = (*dev->dev_ops->xstats_get)(dev,
				     (n > count) ? xstats + count : NULL,
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

	for (i = 0; i < n; i++)
		rte_eth_trace_xstats_get(port_id, xstats[i]);

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
	if (dev->dev_ops->xstats_reset != NULL) {
		int ret = eth_err(port_id, (*dev->dev_ops->xstats_reset)(dev));

		rte_eth_trace_xstats_reset(port_id, ret);

		return ret;
	}

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

	if (is_rx && (queue_id >= dev->data->nb_rx_queues))
		return -EINVAL;

	if (!is_rx && (queue_id >= dev->data->nb_tx_queues))
		return -EINVAL;

	if (stat_idx >= RTE_ETHDEV_QUEUE_STAT_CNTRS)
		return -EINVAL;

	if (*dev->dev_ops->queue_stats_mapping_set == NULL)
		return -ENOTSUP;
	return (*dev->dev_ops->queue_stats_mapping_set) (dev, queue_id, stat_idx, is_rx);
}

int
rte_eth_dev_set_tx_queue_stats_mapping(uint16_t port_id, uint16_t tx_queue_id,
		uint8_t stat_idx)
{
	int ret;

	ret = eth_err(port_id, eth_dev_set_queue_stats_mapping(port_id,
						tx_queue_id,
						stat_idx, STAT_QMAP_TX));

	rte_ethdev_trace_set_tx_queue_stats_mapping(port_id, tx_queue_id,
						    stat_idx, ret);

	return ret;
}

int
rte_eth_dev_set_rx_queue_stats_mapping(uint16_t port_id, uint16_t rx_queue_id,
		uint8_t stat_idx)
{
	int ret;

	ret = eth_err(port_id, eth_dev_set_queue_stats_mapping(port_id,
						rx_queue_id,
						stat_idx, STAT_QMAP_RX));

	rte_ethdev_trace_set_rx_queue_stats_mapping(port_id, rx_queue_id,
						    stat_idx, ret);

	return ret;
}

int
rte_eth_dev_fw_version_get(uint16_t port_id, char *fw_version, size_t fw_size)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (fw_version == NULL && fw_size > 0) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u FW version to NULL when string size is non zero\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->fw_version_get == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->fw_version_get)(dev,
							fw_version, fw_size));

	rte_ethdev_trace_fw_version_get(port_id, fw_version, fw_size, ret);

	return ret;
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

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (dev_info == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot get ethdev port %u info to NULL\n",
			port_id);
		return -EINVAL;
	}

	/*
	 * Init dev_info before port_id check since caller does not have
	 * return status and does not know if get is successful or not.
	 */
	memset(dev_info, 0, sizeof(struct rte_eth_dev_info));
	dev_info->switch_info.domain_id = RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID;

	dev_info->rx_desc_lim = lim;
	dev_info->tx_desc_lim = lim;
	dev_info->device = dev->device;
	dev_info->min_mtu = RTE_ETHER_MIN_LEN - RTE_ETHER_HDR_LEN -
		RTE_ETHER_CRC_LEN;
	dev_info->max_mtu = UINT16_MAX;
	dev_info->rss_algo_capa = RTE_ETH_HASH_ALGO_CAPA_MASK(DEFAULT);
	dev_info->max_rx_bufsize = UINT32_MAX;

	if (*dev->dev_ops->dev_infos_get == NULL)
		return -ENOTSUP;
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

	rte_ethdev_trace_info_get(port_id, dev_info);

	return 0;
}

int
rte_eth_dev_conf_get(uint16_t port_id, struct rte_eth_conf *dev_conf)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (dev_conf == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u configuration to NULL\n",
			port_id);
		return -EINVAL;
	}

	memcpy(dev_conf, &dev->data->dev_conf, sizeof(struct rte_eth_conf));

	rte_ethdev_trace_conf_get(port_id, dev_conf);

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

	if (ptypes == NULL && num > 0) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u supported packet types to NULL when array size is non zero\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->dev_supported_ptypes_get == NULL)
		return 0;
	all_ptypes = (*dev->dev_ops->dev_supported_ptypes_get)(dev);

	if (!all_ptypes)
		return 0;

	for (i = 0, j = 0; all_ptypes[i] != RTE_PTYPE_UNKNOWN; ++i)
		if (all_ptypes[i] & ptype_mask) {
			if (j < num) {
				ptypes[j] = all_ptypes[i];

				rte_ethdev_trace_get_supported_ptypes(port_id,
						j, num, ptypes[j]);
			}
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

	if (num > 0 && set_ptypes == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u set packet types to NULL when array size is non zero\n",
			port_id);
		return -EINVAL;
	}

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

				rte_ethdev_trace_set_ptypes(port_id, j, num,
						set_ptypes[j]);

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
rte_eth_macaddrs_get(uint16_t port_id, struct rte_ether_addr *ma,
	unsigned int num)
{
	int32_t ret;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;

	if (ma == NULL) {
		RTE_ETHDEV_LOG(ERR, "%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	/* will check for us that port_id is a valid one */
	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	dev = &rte_eth_devices[port_id];
	num = RTE_MIN(dev_info.max_mac_addrs, num);
	memcpy(ma, dev->data->mac_addrs, num * sizeof(ma[0]));

	rte_eth_trace_macaddrs_get(port_id, num);

	return num;
}

int
rte_eth_macaddr_get(uint16_t port_id, struct rte_ether_addr *mac_addr)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (mac_addr == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u MAC address to NULL\n",
			port_id);
		return -EINVAL;
	}

	rte_ether_addr_copy(&dev->data->mac_addrs[0], mac_addr);

	rte_eth_trace_macaddr_get(port_id, mac_addr);

	return 0;
}

int
rte_eth_dev_get_mtu(uint16_t port_id, uint16_t *mtu)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (mtu == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot get ethdev port %u MTU to NULL\n",
			port_id);
		return -EINVAL;
	}

	*mtu = dev->data->mtu;

	rte_ethdev_trace_get_mtu(port_id, *mtu);

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
	if (*dev->dev_ops->mtu_set == NULL)
		return -ENOTSUP;

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

		ret = eth_dev_validate_mtu(port_id, &dev_info, mtu);
		if (ret != 0)
			return ret;
	}

	if (dev->data->dev_configured == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Port %u must be configured before MTU set\n",
			port_id);
		return -EINVAL;
	}

	ret = (*dev->dev_ops->mtu_set)(dev, mtu);
	if (ret == 0)
		dev->data->mtu = mtu;

	ret = eth_err(port_id, ret);

	rte_ethdev_trace_set_mtu(port_id, mtu, ret);

	return ret;
}

int
rte_eth_dev_vlan_filter(uint16_t port_id, uint16_t vlan_id, int on)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (!(dev->data->dev_conf.rxmode.offloads &
	      RTE_ETH_RX_OFFLOAD_VLAN_FILTER)) {
		RTE_ETHDEV_LOG(ERR, "Port %u: VLAN-filtering disabled\n",
			port_id);
		return -ENOSYS;
	}

	if (vlan_id > 4095) {
		RTE_ETHDEV_LOG(ERR, "Port_id=%u invalid vlan_id=%u > 4095\n",
			port_id, vlan_id);
		return -EINVAL;
	}
	if (*dev->dev_ops->vlan_filter_set == NULL)
		return -ENOTSUP;

	ret = (*dev->dev_ops->vlan_filter_set)(dev, vlan_id, on);
	if (ret == 0) {
		struct rte_vlan_filter_conf *vfc;
		int vidx;
		int vbit;

		vfc = &dev->data->vlan_filter_conf;
		vidx = vlan_id / 64;
		vbit = vlan_id % 64;

		if (on)
			vfc->ids[vidx] |= RTE_BIT64(vbit);
		else
			vfc->ids[vidx] &= ~RTE_BIT64(vbit);
	}

	ret = eth_err(port_id, ret);

	rte_ethdev_trace_vlan_filter(port_id, vlan_id, on, ret);

	return ret;
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

	if (*dev->dev_ops->vlan_strip_queue_set == NULL)
		return -ENOTSUP;
	(*dev->dev_ops->vlan_strip_queue_set)(dev, rx_queue_id, on);

	rte_ethdev_trace_set_vlan_strip_on_queue(port_id, rx_queue_id, on);

	return 0;
}

int
rte_eth_dev_set_vlan_ether_type(uint16_t port_id,
				enum rte_vlan_type vlan_type,
				uint16_t tpid)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->vlan_tpid_set == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->vlan_tpid_set)(dev, vlan_type,
							      tpid));

	rte_ethdev_trace_set_vlan_ether_type(port_id, vlan_type, tpid, ret);

	return ret;
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
	cur = !!(offload_mask & RTE_ETH_VLAN_STRIP_OFFLOAD);
	org = !!(dev_offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP);
	if (cur != org) {
		if (cur)
			dev_offloads |= RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
		else
			dev_offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
		mask |= RTE_ETH_VLAN_STRIP_MASK;
	}

	cur = !!(offload_mask & RTE_ETH_VLAN_FILTER_OFFLOAD);
	org = !!(dev_offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER);
	if (cur != org) {
		if (cur)
			dev_offloads |= RTE_ETH_RX_OFFLOAD_VLAN_FILTER;
		else
			dev_offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_FILTER;
		mask |= RTE_ETH_VLAN_FILTER_MASK;
	}

	cur = !!(offload_mask & RTE_ETH_VLAN_EXTEND_OFFLOAD);
	org = !!(dev_offloads & RTE_ETH_RX_OFFLOAD_VLAN_EXTEND);
	if (cur != org) {
		if (cur)
			dev_offloads |= RTE_ETH_RX_OFFLOAD_VLAN_EXTEND;
		else
			dev_offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_EXTEND;
		mask |= RTE_ETH_VLAN_EXTEND_MASK;
	}

	cur = !!(offload_mask & RTE_ETH_QINQ_STRIP_OFFLOAD);
	org = !!(dev_offloads & RTE_ETH_RX_OFFLOAD_QINQ_STRIP);
	if (cur != org) {
		if (cur)
			dev_offloads |= RTE_ETH_RX_OFFLOAD_QINQ_STRIP;
		else
			dev_offloads &= ~RTE_ETH_RX_OFFLOAD_QINQ_STRIP;
		mask |= RTE_ETH_QINQ_STRIP_MASK;
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

	if (*dev->dev_ops->vlan_offload_set == NULL)
		return -ENOTSUP;
	dev->data->dev_conf.rxmode.offloads = dev_offloads;
	ret = (*dev->dev_ops->vlan_offload_set)(dev, mask);
	if (ret) {
		/* hit an error restore  original values */
		dev->data->dev_conf.rxmode.offloads = orig_offloads;
	}

	ret = eth_err(port_id, ret);

	rte_ethdev_trace_set_vlan_offload(port_id, offload_mask, ret);

	return ret;
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

	if (*dev_offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
		ret |= RTE_ETH_VLAN_STRIP_OFFLOAD;

	if (*dev_offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER)
		ret |= RTE_ETH_VLAN_FILTER_OFFLOAD;

	if (*dev_offloads & RTE_ETH_RX_OFFLOAD_VLAN_EXTEND)
		ret |= RTE_ETH_VLAN_EXTEND_OFFLOAD;

	if (*dev_offloads & RTE_ETH_RX_OFFLOAD_QINQ_STRIP)
		ret |= RTE_ETH_QINQ_STRIP_OFFLOAD;

	rte_ethdev_trace_get_vlan_offload(port_id, ret);

	return ret;
}

int
rte_eth_dev_set_vlan_pvid(uint16_t port_id, uint16_t pvid, int on)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->vlan_pvid_set == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->vlan_pvid_set)(dev, pvid, on));

	rte_ethdev_trace_set_vlan_pvid(port_id, pvid, on, ret);

	return ret;
}

int
rte_eth_dev_flow_ctrl_get(uint16_t port_id, struct rte_eth_fc_conf *fc_conf)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (fc_conf == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u flow control config to NULL\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->flow_ctrl_get == NULL)
		return -ENOTSUP;
	memset(fc_conf, 0, sizeof(*fc_conf));
	ret = eth_err(port_id, (*dev->dev_ops->flow_ctrl_get)(dev, fc_conf));

	rte_ethdev_trace_flow_ctrl_get(port_id, fc_conf, ret);

	return ret;
}

int
rte_eth_dev_flow_ctrl_set(uint16_t port_id, struct rte_eth_fc_conf *fc_conf)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (fc_conf == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot set ethdev port %u flow control from NULL config\n",
			port_id);
		return -EINVAL;
	}

	if ((fc_conf->send_xon != 0) && (fc_conf->send_xon != 1)) {
		RTE_ETHDEV_LOG(ERR, "Invalid send_xon, only 0/1 allowed\n");
		return -EINVAL;
	}

	if (*dev->dev_ops->flow_ctrl_set == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->flow_ctrl_set)(dev, fc_conf));

	rte_ethdev_trace_flow_ctrl_set(port_id, fc_conf, ret);

	return ret;
}

int
rte_eth_dev_priority_flow_ctrl_set(uint16_t port_id,
				   struct rte_eth_pfc_conf *pfc_conf)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (pfc_conf == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot set ethdev port %u priority flow control from NULL config\n",
			port_id);
		return -EINVAL;
	}

	if (pfc_conf->priority > (RTE_ETH_DCB_NUM_USER_PRIORITIES - 1)) {
		RTE_ETHDEV_LOG(ERR, "Invalid priority, only 0-7 allowed\n");
		return -EINVAL;
	}

	/* High water, low water validation are device specific */
	if  (*dev->dev_ops->priority_flow_ctrl_set == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->priority_flow_ctrl_set)
			       (dev, pfc_conf));

	rte_ethdev_trace_priority_flow_ctrl_set(port_id, pfc_conf, ret);

	return ret;
}

static int
validate_rx_pause_config(struct rte_eth_dev_info *dev_info, uint8_t tc_max,
		struct rte_eth_pfc_queue_conf *pfc_queue_conf)
{
	if ((pfc_queue_conf->mode == RTE_ETH_FC_RX_PAUSE) ||
			(pfc_queue_conf->mode == RTE_ETH_FC_FULL)) {
		if (pfc_queue_conf->rx_pause.tx_qid >= dev_info->nb_tx_queues) {
			RTE_ETHDEV_LOG(ERR,
				"PFC Tx queue not in range for Rx pause requested:%d configured:%d\n",
				pfc_queue_conf->rx_pause.tx_qid,
				dev_info->nb_tx_queues);
			return -EINVAL;
		}

		if (pfc_queue_conf->rx_pause.tc >= tc_max) {
			RTE_ETHDEV_LOG(ERR,
				"PFC TC not in range for Rx pause requested:%d max:%d\n",
				pfc_queue_conf->rx_pause.tc, tc_max);
			return -EINVAL;
		}
	}

	return 0;
}

static int
validate_tx_pause_config(struct rte_eth_dev_info *dev_info, uint8_t tc_max,
		struct rte_eth_pfc_queue_conf *pfc_queue_conf)
{
	if ((pfc_queue_conf->mode == RTE_ETH_FC_TX_PAUSE) ||
			(pfc_queue_conf->mode == RTE_ETH_FC_FULL)) {
		if (pfc_queue_conf->tx_pause.rx_qid >= dev_info->nb_rx_queues) {
			RTE_ETHDEV_LOG(ERR,
				"PFC Rx queue not in range for Tx pause requested:%d configured:%d\n",
				pfc_queue_conf->tx_pause.rx_qid,
				dev_info->nb_rx_queues);
			return -EINVAL;
		}

		if (pfc_queue_conf->tx_pause.tc >= tc_max) {
			RTE_ETHDEV_LOG(ERR,
				"PFC TC not in range for Tx pause requested:%d max:%d\n",
				pfc_queue_conf->tx_pause.tc, tc_max);
			return -EINVAL;
		}
	}

	return 0;
}

int
rte_eth_dev_priority_flow_ctrl_queue_info_get(uint16_t port_id,
		struct rte_eth_pfc_queue_info *pfc_queue_info)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (pfc_queue_info == NULL) {
		RTE_ETHDEV_LOG(ERR, "PFC info param is NULL for port (%u)\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->priority_flow_ctrl_queue_info_get == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->priority_flow_ctrl_queue_info_get)
			(dev, pfc_queue_info));

	rte_ethdev_trace_priority_flow_ctrl_queue_info_get(port_id,
						pfc_queue_info, ret);

	return ret;
}

int
rte_eth_dev_priority_flow_ctrl_queue_configure(uint16_t port_id,
		struct rte_eth_pfc_queue_conf *pfc_queue_conf)
{
	struct rte_eth_pfc_queue_info pfc_info;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (pfc_queue_conf == NULL) {
		RTE_ETHDEV_LOG(ERR, "PFC parameters are NULL for port (%u)\n",
			port_id);
		return -EINVAL;
	}

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	ret = rte_eth_dev_priority_flow_ctrl_queue_info_get(port_id, &pfc_info);
	if (ret != 0)
		return ret;

	if (pfc_info.tc_max == 0) {
		RTE_ETHDEV_LOG(ERR, "Ethdev port %u does not support PFC TC values\n",
			port_id);
		return -ENOTSUP;
	}

	/* Check requested mode supported or not */
	if (pfc_info.mode_capa == RTE_ETH_FC_RX_PAUSE &&
			pfc_queue_conf->mode == RTE_ETH_FC_TX_PAUSE) {
		RTE_ETHDEV_LOG(ERR, "PFC Tx pause unsupported for port (%d)\n",
			port_id);
		return -EINVAL;
	}

	if (pfc_info.mode_capa == RTE_ETH_FC_TX_PAUSE &&
			pfc_queue_conf->mode == RTE_ETH_FC_RX_PAUSE) {
		RTE_ETHDEV_LOG(ERR, "PFC Rx pause unsupported for port (%d)\n",
			port_id);
		return -EINVAL;
	}

	/* Validate Rx pause parameters */
	if (pfc_info.mode_capa == RTE_ETH_FC_FULL ||
			pfc_info.mode_capa == RTE_ETH_FC_RX_PAUSE) {
		ret = validate_rx_pause_config(&dev_info, pfc_info.tc_max,
				pfc_queue_conf);
		if (ret != 0)
			return ret;
	}

	/* Validate Tx pause parameters */
	if (pfc_info.mode_capa == RTE_ETH_FC_FULL ||
			pfc_info.mode_capa == RTE_ETH_FC_TX_PAUSE) {
		ret = validate_tx_pause_config(&dev_info, pfc_info.tc_max,
				pfc_queue_conf);
		if (ret != 0)
			return ret;
	}

	if (*dev->dev_ops->priority_flow_ctrl_queue_config == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->priority_flow_ctrl_queue_config)
			(dev, pfc_queue_conf));

	rte_ethdev_trace_priority_flow_ctrl_queue_configure(port_id,
						pfc_queue_conf, ret);

	return ret;
}

static int
eth_check_reta_mask(struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	uint16_t i, num;

	num = (reta_size + RTE_ETH_RETA_GROUP_SIZE - 1) / RTE_ETH_RETA_GROUP_SIZE;
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

	if (max_rxq == 0) {
		RTE_ETHDEV_LOG(ERR, "No receive queue is available\n");
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		if ((reta_conf[idx].mask & RTE_BIT64(shift)) &&
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
	enum rte_eth_rx_mq_mode mq_mode;
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (reta_conf == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot update ethdev port %u RSS RETA to NULL\n",
			port_id);
		return -EINVAL;
	}

	if (reta_size == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot update ethdev port %u RSS RETA with zero size\n",
			port_id);
		return -EINVAL;
	}

	/* Check mask bits */
	ret = eth_check_reta_mask(reta_conf, reta_size);
	if (ret < 0)
		return ret;

	/* Check entry value */
	ret = eth_check_reta_entry(reta_conf, reta_size,
				dev->data->nb_rx_queues);
	if (ret < 0)
		return ret;

	mq_mode = dev->data->dev_conf.rxmode.mq_mode;
	if (!(mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)) {
		RTE_ETHDEV_LOG(ERR, "Multi-queue RSS mode isn't enabled.\n");
		return -ENOTSUP;
	}

	if (*dev->dev_ops->reta_update == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->reta_update)(dev, reta_conf,
							    reta_size));

	rte_ethdev_trace_rss_reta_update(port_id, reta_conf, reta_size, ret);

	return ret;
}

int
rte_eth_dev_rss_reta_query(uint16_t port_id,
			   struct rte_eth_rss_reta_entry64 *reta_conf,
			   uint16_t reta_size)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (reta_conf == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot query ethdev port %u RSS RETA from NULL config\n",
			port_id);
		return -EINVAL;
	}

	/* Check mask bits */
	ret = eth_check_reta_mask(reta_conf, reta_size);
	if (ret < 0)
		return ret;

	if (*dev->dev_ops->reta_query == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->reta_query)(dev, reta_conf,
							   reta_size));

	rte_ethdev_trace_rss_reta_query(port_id, reta_conf, reta_size, ret);

	return ret;
}

int
rte_eth_dev_rss_hash_update(uint16_t port_id,
			    struct rte_eth_rss_conf *rss_conf)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info = { .flow_type_rss_offloads = 0, };
	enum rte_eth_rx_mq_mode mq_mode;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (rss_conf == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot update ethdev port %u RSS hash from NULL config\n",
			port_id);
		return -EINVAL;
	}

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	rss_conf->rss_hf = rte_eth_rss_hf_refine(rss_conf->rss_hf);
	if ((dev_info.flow_type_rss_offloads | rss_conf->rss_hf) !=
	    dev_info.flow_type_rss_offloads) {
		RTE_ETHDEV_LOG(ERR,
			"Ethdev port_id=%u invalid rss_hf: 0x%"PRIx64", valid value: 0x%"PRIx64"\n",
			port_id, rss_conf->rss_hf,
			dev_info.flow_type_rss_offloads);
		return -EINVAL;
	}

	mq_mode = dev->data->dev_conf.rxmode.mq_mode;
	if (!(mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)) {
		RTE_ETHDEV_LOG(ERR, "Multi-queue RSS mode isn't enabled.\n");
		return -ENOTSUP;
	}

	if (rss_conf->rss_key != NULL &&
	    rss_conf->rss_key_len != dev_info.hash_key_size) {
		RTE_ETHDEV_LOG(ERR,
			"Ethdev port_id=%u invalid RSS key len: %u, valid value: %u\n",
			port_id, rss_conf->rss_key_len, dev_info.hash_key_size);
		return -EINVAL;
	}

	if ((size_t)rss_conf->algorithm >= CHAR_BIT * sizeof(dev_info.rss_algo_capa) ||
	    (dev_info.rss_algo_capa &
	     RTE_ETH_HASH_ALGO_TO_CAPA(rss_conf->algorithm)) == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Ethdev port_id=%u configured RSS hash algorithm (%u)"
			"is not in the algorithm capability (0x%" PRIx32 ")\n",
			port_id, rss_conf->algorithm, dev_info.rss_algo_capa);
		return -EINVAL;
	}

	if (*dev->dev_ops->rss_hash_update == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->rss_hash_update)(dev,
								rss_conf));

	rte_ethdev_trace_rss_hash_update(port_id, rss_conf, ret);

	return ret;
}

int
rte_eth_dev_rss_hash_conf_get(uint16_t port_id,
			      struct rte_eth_rss_conf *rss_conf)
{
	struct rte_eth_dev_info dev_info = { 0 };
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (rss_conf == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u RSS hash config to NULL\n",
			port_id);
		return -EINVAL;
	}

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	if (rss_conf->rss_key != NULL &&
	    rss_conf->rss_key_len < dev_info.hash_key_size) {
		RTE_ETHDEV_LOG(ERR,
			"Ethdev port_id=%u invalid RSS key len: %u, should not be less than: %u\n",
			port_id, rss_conf->rss_key_len, dev_info.hash_key_size);
		return -EINVAL;
	}

	rss_conf->algorithm = RTE_ETH_HASH_FUNCTION_DEFAULT;

	if (*dev->dev_ops->rss_hash_conf_get == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->rss_hash_conf_get)(dev,
								  rss_conf));

	rte_ethdev_trace_rss_hash_conf_get(port_id, rss_conf, ret);

	return ret;
}

const char *
rte_eth_dev_rss_algo_name(enum rte_eth_hash_function rss_algo)
{
	const char *name = "Unknown function";
	unsigned int i;

	for (i = 0; i < RTE_DIM(rte_eth_dev_rss_algo_names); i++) {
		if (rss_algo == rte_eth_dev_rss_algo_names[i].algo)
			return rte_eth_dev_rss_algo_names[i].name;
	}

	return name;
}

int
rte_eth_dev_udp_tunnel_port_add(uint16_t port_id,
				struct rte_eth_udp_tunnel *udp_tunnel)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (udp_tunnel == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot add ethdev port %u UDP tunnel port from NULL UDP tunnel\n",
			port_id);
		return -EINVAL;
	}

	if (udp_tunnel->prot_type >= RTE_ETH_TUNNEL_TYPE_MAX) {
		RTE_ETHDEV_LOG(ERR, "Invalid tunnel type\n");
		return -EINVAL;
	}

	if (*dev->dev_ops->udp_tunnel_port_add == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->udp_tunnel_port_add)(dev,
								udp_tunnel));

	rte_ethdev_trace_udp_tunnel_port_add(port_id, udp_tunnel, ret);

	return ret;
}

int
rte_eth_dev_udp_tunnel_port_delete(uint16_t port_id,
				   struct rte_eth_udp_tunnel *udp_tunnel)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (udp_tunnel == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot delete ethdev port %u UDP tunnel port from NULL UDP tunnel\n",
			port_id);
		return -EINVAL;
	}

	if (udp_tunnel->prot_type >= RTE_ETH_TUNNEL_TYPE_MAX) {
		RTE_ETHDEV_LOG(ERR, "Invalid tunnel type\n");
		return -EINVAL;
	}

	if (*dev->dev_ops->udp_tunnel_port_del == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->udp_tunnel_port_del)(dev,
								udp_tunnel));

	rte_ethdev_trace_udp_tunnel_port_delete(port_id, udp_tunnel, ret);

	return ret;
}

int
rte_eth_led_on(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->dev_led_on == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->dev_led_on)(dev));

	rte_eth_trace_led_on(port_id, ret);

	return ret;
}

int
rte_eth_led_off(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->dev_led_off == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->dev_led_off)(dev));

	rte_eth_trace_led_off(port_id, ret);

	return ret;
}

int
rte_eth_fec_get_capability(uint16_t port_id,
			   struct rte_eth_fec_capa *speed_fec_capa,
			   unsigned int num)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (speed_fec_capa == NULL && num > 0) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u FEC capability to NULL when array size is non zero\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->fec_get_capability == NULL)
		return -ENOTSUP;
	ret = (*dev->dev_ops->fec_get_capability)(dev, speed_fec_capa, num);

	rte_eth_trace_fec_get_capability(port_id, speed_fec_capa, num, ret);

	return ret;
}

int
rte_eth_fec_get(uint16_t port_id, uint32_t *fec_capa)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (fec_capa == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u current FEC mode to NULL\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->fec_get == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->fec_get)(dev, fec_capa));

	rte_eth_trace_fec_get(port_id, fec_capa, ret);

	return ret;
}

int
rte_eth_fec_set(uint16_t port_id, uint32_t fec_capa)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (fec_capa == 0) {
		RTE_ETHDEV_LOG(ERR, "At least one FEC mode should be specified\n");
		return -EINVAL;
	}

	if (*dev->dev_ops->fec_set == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->fec_set)(dev, fec_capa));

	rte_eth_trace_fec_set(port_id, fec_capa, ret);

	return ret;
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

	if (addr == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot add ethdev port %u MAC address from NULL address\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->mac_addr_add == NULL)
		return -ENOTSUP;

	if (rte_is_zero_ether_addr(addr)) {
		RTE_ETHDEV_LOG(ERR, "Port %u: Cannot add NULL MAC address\n",
			port_id);
		return -EINVAL;
	}
	if (pool >= RTE_ETH_64_POOLS) {
		RTE_ETHDEV_LOG(ERR, "Pool ID must be 0-%d\n", RTE_ETH_64_POOLS - 1);
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
		if (pool_mask & RTE_BIT64(pool))
			return 0;
	}

	/* Update NIC */
	ret = (*dev->dev_ops->mac_addr_add)(dev, addr, index, pool);

	if (ret == 0) {
		/* Update address in NIC data structure */
		rte_ether_addr_copy(addr, &dev->data->mac_addrs[index]);

		/* Update pool bitmap in NIC data structure */
		dev->data->mac_pool_sel[index] |= RTE_BIT64(pool);
	}

	ret = eth_err(port_id, ret);

	rte_ethdev_trace_mac_addr_add(port_id, addr, pool, ret);

	return ret;
}

int
rte_eth_dev_mac_addr_remove(uint16_t port_id, struct rte_ether_addr *addr)
{
	struct rte_eth_dev *dev;
	int index;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (addr == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot remove ethdev port %u MAC address from NULL address\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->mac_addr_remove == NULL)
		return -ENOTSUP;

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

	rte_ethdev_trace_mac_addr_remove(port_id, addr);

	return 0;
}

int
rte_eth_dev_default_mac_addr_set(uint16_t port_id, struct rte_ether_addr *addr)
{
	struct rte_eth_dev *dev;
	int index;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (addr == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot set ethdev port %u default MAC address from NULL address\n",
			port_id);
		return -EINVAL;
	}

	if (!rte_is_valid_assigned_ether_addr(addr))
		return -EINVAL;

	if (*dev->dev_ops->mac_addr_set == NULL)
		return -ENOTSUP;

	/* Keep address unique in dev->data->mac_addrs[]. */
	index = eth_dev_get_mac_addr_index(port_id, addr);
	if (index > 0) {
		RTE_ETHDEV_LOG(ERR,
			"New default address for port %u was already in the address list. Please remove it first.\n",
			port_id);
		return -EEXIST;
	}

	ret = (*dev->dev_ops->mac_addr_set)(dev, addr);
	if (ret < 0)
		return ret;

	/* Update default address in NIC data structure */
	rte_ether_addr_copy(addr, &dev->data->mac_addrs[0]);

	rte_ethdev_trace_default_mac_addr_set(port_id, addr);

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

	if (addr == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot set ethdev port %u unicast hash table from NULL address\n",
			port_id);
		return -EINVAL;
	}

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

	if (*dev->dev_ops->uc_hash_table_set == NULL)
		return -ENOTSUP;
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

	ret = eth_err(port_id, ret);

	rte_ethdev_trace_uc_hash_table_set(port_id, on, ret);

	return ret;
}

int
rte_eth_dev_uc_all_hash_table_set(uint16_t port_id, uint8_t on)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->uc_all_hash_table_set == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->uc_all_hash_table_set)(dev, on));

	rte_ethdev_trace_uc_all_hash_table_set(port_id, on, ret);

	return ret;
}

int rte_eth_set_queue_rate_limit(uint16_t port_id, uint16_t queue_idx,
					uint32_t tx_rate)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_link link;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		return ret;

	link = dev->data->dev_link;

	if (queue_idx > dev_info.max_tx_queues) {
		RTE_ETHDEV_LOG(ERR,
			"Set queue rate limit:port %u: invalid queue ID=%u\n",
			port_id, queue_idx);
		return -EINVAL;
	}

	if (tx_rate > link.link_speed) {
		RTE_ETHDEV_LOG(ERR,
			"Set queue rate limit:invalid tx_rate=%u, bigger than link speed= %d\n",
			tx_rate, link.link_speed);
		return -EINVAL;
	}

	if (*dev->dev_ops->set_queue_rate_limit == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->set_queue_rate_limit)(dev,
							queue_idx, tx_rate));

	rte_eth_trace_set_queue_rate_limit(port_id, queue_idx, tx_rate, ret);

	return ret;
}

int rte_eth_rx_avail_thresh_set(uint16_t port_id, uint16_t queue_id,
			       uint8_t avail_thresh)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (queue_id > dev->data->nb_rx_queues) {
		RTE_ETHDEV_LOG(ERR,
			"Set queue avail thresh: port %u: invalid queue ID=%u.\n",
			port_id, queue_id);
		return -EINVAL;
	}

	if (avail_thresh > 99) {
		RTE_ETHDEV_LOG(ERR,
			"Set queue avail thresh: port %u: threshold should be <= 99.\n",
			port_id);
		return -EINVAL;
	}
	if (*dev->dev_ops->rx_queue_avail_thresh_set == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->rx_queue_avail_thresh_set)(dev,
							     queue_id, avail_thresh));

	rte_eth_trace_rx_avail_thresh_set(port_id, queue_id, avail_thresh, ret);

	return ret;
}

int rte_eth_rx_avail_thresh_query(uint16_t port_id, uint16_t *queue_id,
				 uint8_t *avail_thresh)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (queue_id == NULL)
		return -EINVAL;
	if (*queue_id >= dev->data->nb_rx_queues)
		*queue_id = 0;

	if (*dev->dev_ops->rx_queue_avail_thresh_query == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->rx_queue_avail_thresh_query)(dev,
							     queue_id, avail_thresh));

	rte_eth_trace_rx_avail_thresh_query(port_id, *queue_id, ret);

	return ret;
}

RTE_INIT(eth_dev_init_fp_ops)
{
	uint32_t i;

	for (i = 0; i != RTE_DIM(rte_eth_fp_ops); i++)
		eth_dev_fp_ops_reset(rte_eth_fp_ops + i);
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

	if (cb_fn == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot register ethdev port %u callback from NULL\n",
			port_id);
		return -EINVAL;
	}

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

	rte_ethdev_trace_callback_register(port_id, event, cb_fn, cb_arg);

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

	if (cb_fn == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot unregister ethdev port %u callback from NULL\n",
			port_id);
		return -EINVAL;
	}

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

	rte_ethdev_trace_callback_unregister(port_id, event, cb_fn, cb_arg,
					     ret);

	return ret;
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
		RTE_ETHDEV_LOG(ERR, "Rx Intr handle unset\n");
		return -ENOTSUP;
	}

	intr_handle = dev->intr_handle;
	if (rte_intr_vec_list_index_get(intr_handle, 0) < 0) {
		RTE_ETHDEV_LOG(ERR, "Rx Intr vector unset\n");
		return -EPERM;
	}

	for (qid = 0; qid < dev->data->nb_rx_queues; qid++) {
		vec = rte_intr_vec_list_index_get(intr_handle, qid);
		rc = rte_intr_rx_ctl(intr_handle, epfd, op, vec, data);

		rte_ethdev_trace_rx_intr_ctl(port_id, qid, epfd, op, data, rc);

		if (rc && rc != -EEXIST) {
			RTE_ETHDEV_LOG(ERR,
				"p %u q %u Rx ctl error op %d epfd %d vec %u\n",
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
		RTE_ETHDEV_LOG(ERR, "Invalid Rx queue_id=%u\n", queue_id);
		return -1;
	}

	if (!dev->intr_handle) {
		RTE_ETHDEV_LOG(ERR, "Rx Intr handle unset\n");
		return -1;
	}

	intr_handle = dev->intr_handle;
	if (rte_intr_vec_list_index_get(intr_handle, 0) < 0) {
		RTE_ETHDEV_LOG(ERR, "Rx Intr vector unset\n");
		return -1;
	}

	vec = rte_intr_vec_list_index_get(intr_handle, queue_id);
	efd_idx = (vec >= RTE_INTR_VEC_RXTX_OFFSET) ?
		(vec - RTE_INTR_VEC_RXTX_OFFSET) : vec;
	fd = rte_intr_efds_index_get(intr_handle, efd_idx);

	rte_ethdev_trace_rx_intr_ctl_q_get_fd(port_id, queue_id, fd);

	return fd;
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
		RTE_ETHDEV_LOG(ERR, "Invalid Rx queue_id=%u\n", queue_id);
		return -EINVAL;
	}

	if (!dev->intr_handle) {
		RTE_ETHDEV_LOG(ERR, "Rx Intr handle unset\n");
		return -ENOTSUP;
	}

	intr_handle = dev->intr_handle;
	if (rte_intr_vec_list_index_get(intr_handle, 0) < 0) {
		RTE_ETHDEV_LOG(ERR, "Rx Intr vector unset\n");
		return -EPERM;
	}

	vec = rte_intr_vec_list_index_get(intr_handle, queue_id);
	rc = rte_intr_rx_ctl(intr_handle, epfd, op, vec, data);

	rte_ethdev_trace_rx_intr_ctl_q(port_id, queue_id, epfd, op, data, rc);

	if (rc && rc != -EEXIST) {
		RTE_ETHDEV_LOG(ERR,
			"p %u q %u Rx ctl error op %d epfd %d vec %u\n",
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

	if (*dev->dev_ops->rx_queue_intr_enable == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->rx_queue_intr_enable)(dev, queue_id));

	rte_ethdev_trace_rx_intr_enable(port_id, queue_id, ret);

	return ret;
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

	if (*dev->dev_ops->rx_queue_intr_disable == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->rx_queue_intr_disable)(dev, queue_id));

	rte_ethdev_trace_rx_intr_disable(port_id, queue_id, ret);

	return ret;
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
		rte_atomic_store_explicit(
			&rte_eth_devices[port_id].post_rx_burst_cbs[queue_id],
			cb, rte_memory_order_release);

	} else {
		while (tail->next)
			tail = tail->next;
		/* Stores to cb->fn and cb->param should complete before
		 * cb is visible to data plane.
		 */
		rte_atomic_store_explicit(&tail->next, cb, rte_memory_order_release);
	}
	rte_spinlock_unlock(&eth_dev_rx_cb_lock);

	rte_eth_trace_add_rx_callback(port_id, queue_id, fn, user_param, cb);

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
	rte_atomic_store_explicit(
		&rte_eth_devices[port_id].post_rx_burst_cbs[queue_id],
		cb, rte_memory_order_release);
	rte_spinlock_unlock(&eth_dev_rx_cb_lock);

	rte_eth_trace_add_first_rx_callback(port_id, queue_id, fn, user_param,
					    cb);

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
		rte_atomic_store_explicit(
			&rte_eth_devices[port_id].pre_tx_burst_cbs[queue_id],
			cb, rte_memory_order_release);

	} else {
		while (tail->next)
			tail = tail->next;
		/* Stores to cb->fn and cb->param should complete before
		 * cb is visible to data plane.
		 */
		rte_atomic_store_explicit(&tail->next, cb, rte_memory_order_release);
	}
	rte_spinlock_unlock(&eth_dev_tx_cb_lock);

	rte_eth_trace_add_tx_callback(port_id, queue_id, fn, user_param, cb);

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
	RTE_ATOMIC(struct rte_eth_rxtx_callback *) *prev_cb;
	int ret = -EINVAL;

	rte_spinlock_lock(&eth_dev_rx_cb_lock);
	prev_cb = &dev->post_rx_burst_cbs[queue_id];
	for (; *prev_cb != NULL; prev_cb = &cb->next) {
		cb = *prev_cb;
		if (cb == user_cb) {
			/* Remove the user cb from the callback list. */
			rte_atomic_store_explicit(prev_cb, cb->next, rte_memory_order_relaxed);
			ret = 0;
			break;
		}
	}
	rte_spinlock_unlock(&eth_dev_rx_cb_lock);

	rte_eth_trace_remove_rx_callback(port_id, queue_id, user_cb, ret);

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
	RTE_ATOMIC(struct rte_eth_rxtx_callback *) *prev_cb;

	rte_spinlock_lock(&eth_dev_tx_cb_lock);
	prev_cb = &dev->pre_tx_burst_cbs[queue_id];
	for (; *prev_cb != NULL; prev_cb = &cb->next) {
		cb = *prev_cb;
		if (cb == user_cb) {
			/* Remove the user cb from the callback list. */
			rte_atomic_store_explicit(prev_cb, cb->next, rte_memory_order_relaxed);
			ret = 0;
			break;
		}
	}
	rte_spinlock_unlock(&eth_dev_tx_cb_lock);

	rte_eth_trace_remove_tx_callback(port_id, queue_id, user_cb, ret);

	return ret;
}

int
rte_eth_rx_queue_info_get(uint16_t port_id, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (queue_id >= dev->data->nb_rx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid Rx queue_id=%u\n", queue_id);
		return -EINVAL;
	}

	if (qinfo == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot get ethdev port %u Rx queue %u info to NULL\n",
			port_id, queue_id);
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

	if (*dev->dev_ops->rxq_info_get == NULL)
		return -ENOTSUP;

	memset(qinfo, 0, sizeof(*qinfo));
	dev->dev_ops->rxq_info_get(dev, queue_id, qinfo);
	qinfo->queue_state = dev->data->rx_queue_state[queue_id];

	rte_eth_trace_rx_queue_info_get(port_id, queue_id, qinfo);

	return 0;
}

int
rte_eth_tx_queue_info_get(uint16_t port_id, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (queue_id >= dev->data->nb_tx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid Tx queue_id=%u\n", queue_id);
		return -EINVAL;
	}

	if (qinfo == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot get ethdev port %u Tx queue %u info to NULL\n",
			port_id, queue_id);
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

	if (*dev->dev_ops->txq_info_get == NULL)
		return -ENOTSUP;

	memset(qinfo, 0, sizeof(*qinfo));
	dev->dev_ops->txq_info_get(dev, queue_id, qinfo);
	qinfo->queue_state = dev->data->tx_queue_state[queue_id];

	rte_eth_trace_tx_queue_info_get(port_id, queue_id, qinfo);

	return 0;
}

int
rte_eth_recycle_rx_queue_info_get(uint16_t port_id, uint16_t queue_id,
		struct rte_eth_recycle_rxq_info *recycle_rxq_info)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	ret = eth_dev_validate_rx_queue(dev, queue_id);
	if (unlikely(ret != 0))
		return ret;

	if (*dev->dev_ops->recycle_rxq_info_get == NULL)
		return -ENOTSUP;

	dev->dev_ops->recycle_rxq_info_get(dev, queue_id, recycle_rxq_info);

	return 0;
}

int
rte_eth_rx_burst_mode_get(uint16_t port_id, uint16_t queue_id,
			  struct rte_eth_burst_mode *mode)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (queue_id >= dev->data->nb_rx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid Rx queue_id=%u\n", queue_id);
		return -EINVAL;
	}

	if (mode == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u Rx queue %u burst mode to NULL\n",
			port_id, queue_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->rx_burst_mode_get == NULL)
		return -ENOTSUP;
	memset(mode, 0, sizeof(*mode));
	ret = eth_err(port_id,
		      dev->dev_ops->rx_burst_mode_get(dev, queue_id, mode));

	rte_eth_trace_rx_burst_mode_get(port_id, queue_id, mode, ret);

	return ret;
}

int
rte_eth_tx_burst_mode_get(uint16_t port_id, uint16_t queue_id,
			  struct rte_eth_burst_mode *mode)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (queue_id >= dev->data->nb_tx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid Tx queue_id=%u\n", queue_id);
		return -EINVAL;
	}

	if (mode == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u Tx queue %u burst mode to NULL\n",
			port_id, queue_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->tx_burst_mode_get == NULL)
		return -ENOTSUP;
	memset(mode, 0, sizeof(*mode));
	ret = eth_err(port_id,
		      dev->dev_ops->tx_burst_mode_get(dev, queue_id, mode));

	rte_eth_trace_tx_burst_mode_get(port_id, queue_id, mode, ret);

	return ret;
}

int
rte_eth_get_monitor_addr(uint16_t port_id, uint16_t queue_id,
		struct rte_power_monitor_cond *pmc)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (queue_id >= dev->data->nb_rx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid Rx queue_id=%u\n", queue_id);
		return -EINVAL;
	}

	if (pmc == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u Rx queue %u power monitor condition to NULL\n",
			port_id, queue_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->get_monitor_addr == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id,
		dev->dev_ops->get_monitor_addr(dev->data->rx_queues[queue_id], pmc));

	rte_eth_trace_get_monitor_addr(port_id, queue_id, pmc, ret);

	return ret;
}

int
rte_eth_dev_set_mc_addr_list(uint16_t port_id,
			     struct rte_ether_addr *mc_addr_set,
			     uint32_t nb_mc_addr)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->set_mc_addr_list == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, dev->dev_ops->set_mc_addr_list(dev,
						mc_addr_set, nb_mc_addr));

	rte_ethdev_trace_set_mc_addr_list(port_id, mc_addr_set, nb_mc_addr,
					  ret);

	return ret;
}

int
rte_eth_timesync_enable(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->timesync_enable == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->timesync_enable)(dev));

	rte_eth_trace_timesync_enable(port_id, ret);

	return ret;
}

int
rte_eth_timesync_disable(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->timesync_disable == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->timesync_disable)(dev));

	rte_eth_trace_timesync_disable(port_id, ret);

	return ret;
}

int
rte_eth_timesync_read_rx_timestamp(uint16_t port_id, struct timespec *timestamp,
				   uint32_t flags)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (timestamp == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot read ethdev port %u Rx timestamp to NULL\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->timesync_read_rx_timestamp == NULL)
		return -ENOTSUP;

	ret = eth_err(port_id, (*dev->dev_ops->timesync_read_rx_timestamp)
			       (dev, timestamp, flags));

	rte_eth_trace_timesync_read_rx_timestamp(port_id, timestamp, flags,
						 ret);

	return ret;
}

int
rte_eth_timesync_read_tx_timestamp(uint16_t port_id,
				   struct timespec *timestamp)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (timestamp == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot read ethdev port %u Tx timestamp to NULL\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->timesync_read_tx_timestamp == NULL)
		return -ENOTSUP;

	ret = eth_err(port_id, (*dev->dev_ops->timesync_read_tx_timestamp)
			       (dev, timestamp));

	rte_eth_trace_timesync_read_tx_timestamp(port_id, timestamp, ret);

	return ret;

}

int
rte_eth_timesync_adjust_time(uint16_t port_id, int64_t delta)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->timesync_adjust_time == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->timesync_adjust_time)(dev, delta));

	rte_eth_trace_timesync_adjust_time(port_id, delta, ret);

	return ret;
}

int
rte_eth_timesync_read_time(uint16_t port_id, struct timespec *timestamp)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (timestamp == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot read ethdev port %u timesync time to NULL\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->timesync_read_time == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->timesync_read_time)(dev,
								timestamp));

	rte_eth_trace_timesync_read_time(port_id, timestamp, ret);

	return ret;
}

int
rte_eth_timesync_write_time(uint16_t port_id, const struct timespec *timestamp)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (timestamp == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot write ethdev port %u timesync from NULL time\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->timesync_write_time == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->timesync_write_time)(dev,
								timestamp));

	rte_eth_trace_timesync_write_time(port_id, timestamp, ret);

	return ret;
}

int
rte_eth_read_clock(uint16_t port_id, uint64_t *clock)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (clock == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot read ethdev port %u clock to NULL\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->read_clock == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->read_clock)(dev, clock));

	rte_eth_trace_read_clock(port_id, clock, ret);

	return ret;
}

int
rte_eth_dev_get_reg_info(uint16_t port_id, struct rte_dev_reg_info *info)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (info == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u register info to NULL\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->get_reg == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->get_reg)(dev, info));

	rte_ethdev_trace_get_reg_info(port_id, info, ret);

	return ret;
}

int
rte_eth_dev_get_eeprom_length(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->get_eeprom_length == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->get_eeprom_length)(dev));

	rte_ethdev_trace_get_eeprom_length(port_id, ret);

	return ret;
}

int
rte_eth_dev_get_eeprom(uint16_t port_id, struct rte_dev_eeprom_info *info)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (info == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u EEPROM info to NULL\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->get_eeprom == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->get_eeprom)(dev, info));

	rte_ethdev_trace_get_eeprom(port_id, info, ret);

	return ret;
}

int
rte_eth_dev_set_eeprom(uint16_t port_id, struct rte_dev_eeprom_info *info)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (info == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot set ethdev port %u EEPROM from NULL info\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->set_eeprom == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->set_eeprom)(dev, info));

	rte_ethdev_trace_set_eeprom(port_id, info, ret);

	return ret;
}

int
rte_eth_dev_get_module_info(uint16_t port_id,
			    struct rte_eth_dev_module_info *modinfo)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (modinfo == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u EEPROM module info to NULL\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->get_module_info == NULL)
		return -ENOTSUP;
	ret = (*dev->dev_ops->get_module_info)(dev, modinfo);

	rte_ethdev_trace_get_module_info(port_id, modinfo, ret);

	return ret;
}

int
rte_eth_dev_get_module_eeprom(uint16_t port_id,
			      struct rte_dev_eeprom_info *info)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (info == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u module EEPROM info to NULL\n",
			port_id);
		return -EINVAL;
	}

	if (info->data == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u module EEPROM data to NULL\n",
			port_id);
		return -EINVAL;
	}

	if (info->length == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u module EEPROM to data with zero size\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->get_module_eeprom == NULL)
		return -ENOTSUP;
	ret = (*dev->dev_ops->get_module_eeprom)(dev, info);

	rte_ethdev_trace_get_module_eeprom(port_id, info, ret);

	return ret;
}

int
rte_eth_dev_get_dcb_info(uint16_t port_id,
			     struct rte_eth_dcb_info *dcb_info)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (dcb_info == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u DCB info to NULL\n",
			port_id);
		return -EINVAL;
	}

	memset(dcb_info, 0, sizeof(struct rte_eth_dcb_info));

	if (*dev->dev_ops->get_dcb_info == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->get_dcb_info)(dev, dcb_info));

	rte_ethdev_trace_get_dcb_info(port_id, dcb_info, ret);

	return ret;
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

	rte_ethdev_trace_adjust_nb_rx_tx_desc(port_id);

	return 0;
}

int
rte_eth_dev_hairpin_capability_get(uint16_t port_id,
				   struct rte_eth_hairpin_cap *cap)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (cap == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u hairpin capability to NULL\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->hairpin_cap_get == NULL)
		return -ENOTSUP;
	memset(cap, 0, sizeof(*cap));
	ret = eth_err(port_id, (*dev->dev_ops->hairpin_cap_get)(dev, cap));

	rte_ethdev_trace_hairpin_capability_get(port_id, cap, ret);

	return ret;
}

int
rte_eth_dev_pool_ops_supported(uint16_t port_id, const char *pool)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (pool == NULL) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot test ethdev port %u mempool operation from NULL pool\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->pool_ops_supported == NULL)
		return 1; /* all pools are supported */

	ret = (*dev->dev_ops->pool_ops_supported)(dev, pool);

	rte_ethdev_trace_pool_ops_supported(port_id, pool, ret);

	return ret;
}

int
rte_eth_representor_info_get(uint16_t port_id,
			     struct rte_eth_representor_info *info)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->representor_info_get == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id, (*dev->dev_ops->representor_info_get)(dev, info));

	rte_eth_trace_representor_info_get(port_id, info, ret);

	return ret;
}

int
rte_eth_rx_metadata_negotiate(uint16_t port_id, uint64_t *features)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_configured != 0) {
		RTE_ETHDEV_LOG(ERR,
			"The port (ID=%"PRIu16") is already configured\n",
			port_id);
		return -EBUSY;
	}

	if (features == NULL) {
		RTE_ETHDEV_LOG(ERR, "Invalid features (NULL)\n");
		return -EINVAL;
	}

	if ((*features & RTE_ETH_RX_METADATA_TUNNEL_ID) != 0 &&
			rte_flow_restore_info_dynflag_register() < 0)
		*features &= ~RTE_ETH_RX_METADATA_TUNNEL_ID;

	if (*dev->dev_ops->rx_metadata_negotiate == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id,
		      (*dev->dev_ops->rx_metadata_negotiate)(dev, features));

	rte_eth_trace_rx_metadata_negotiate(port_id, *features, ret);

	return ret;
}

int
rte_eth_ip_reassembly_capability_get(uint16_t port_id,
		struct rte_eth_ip_reassembly_params *reassembly_capa)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_configured == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Device with port_id=%u is not configured.\n"
			"Cannot get IP reassembly capability\n",
			port_id);
		return -EINVAL;
	}

	if (reassembly_capa == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot get reassembly capability to NULL\n");
		return -EINVAL;
	}

	if (*dev->dev_ops->ip_reassembly_capability_get == NULL)
		return -ENOTSUP;
	memset(reassembly_capa, 0, sizeof(struct rte_eth_ip_reassembly_params));

	ret = eth_err(port_id, (*dev->dev_ops->ip_reassembly_capability_get)
					(dev, reassembly_capa));

	rte_eth_trace_ip_reassembly_capability_get(port_id, reassembly_capa,
						   ret);

	return ret;
}

int
rte_eth_ip_reassembly_conf_get(uint16_t port_id,
		struct rte_eth_ip_reassembly_params *conf)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_configured == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Device with port_id=%u is not configured.\n"
			"Cannot get IP reassembly configuration\n",
			port_id);
		return -EINVAL;
	}

	if (conf == NULL) {
		RTE_ETHDEV_LOG(ERR, "Cannot get reassembly info to NULL\n");
		return -EINVAL;
	}

	if (*dev->dev_ops->ip_reassembly_conf_get == NULL)
		return -ENOTSUP;
	memset(conf, 0, sizeof(struct rte_eth_ip_reassembly_params));
	ret = eth_err(port_id,
		      (*dev->dev_ops->ip_reassembly_conf_get)(dev, conf));

	rte_eth_trace_ip_reassembly_conf_get(port_id, conf, ret);

	return ret;
}

int
rte_eth_ip_reassembly_conf_set(uint16_t port_id,
		const struct rte_eth_ip_reassembly_params *conf)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_configured == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Device with port_id=%u is not configured.\n"
			"Cannot set IP reassembly configuration\n",
			port_id);
		return -EINVAL;
	}

	if (dev->data->dev_started != 0) {
		RTE_ETHDEV_LOG(ERR,
			"Device with port_id=%u started,\n"
			"cannot configure IP reassembly params.\n",
			port_id);
		return -EINVAL;
	}

	if (conf == NULL) {
		RTE_ETHDEV_LOG(ERR,
				"Invalid IP reassembly configuration (NULL)\n");
		return -EINVAL;
	}

	if (*dev->dev_ops->ip_reassembly_conf_set == NULL)
		return -ENOTSUP;
	ret = eth_err(port_id,
		      (*dev->dev_ops->ip_reassembly_conf_set)(dev, conf));

	rte_eth_trace_ip_reassembly_conf_set(port_id, conf, ret);

	return ret;
}

int
rte_eth_dev_priv_dump(uint16_t port_id, FILE *file)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (file == NULL) {
		RTE_ETHDEV_LOG(ERR, "Invalid file (NULL)\n");
		return -EINVAL;
	}

	if (*dev->dev_ops->eth_dev_priv_dump == NULL)
		return -ENOTSUP;
	return eth_err(port_id, (*dev->dev_ops->eth_dev_priv_dump)(dev, file));
}

int
rte_eth_rx_descriptor_dump(uint16_t port_id, uint16_t queue_id,
			   uint16_t offset, uint16_t num, FILE *file)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (queue_id >= dev->data->nb_rx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid Rx queue_id=%u\n", queue_id);
		return -EINVAL;
	}

	if (file == NULL) {
		RTE_ETHDEV_LOG(ERR, "Invalid file (NULL)\n");
		return -EINVAL;
	}

	if (*dev->dev_ops->eth_rx_descriptor_dump == NULL)
		return -ENOTSUP;

	return eth_err(port_id, (*dev->dev_ops->eth_rx_descriptor_dump)(dev,
						queue_id, offset, num, file));
}

int
rte_eth_tx_descriptor_dump(uint16_t port_id, uint16_t queue_id,
			   uint16_t offset, uint16_t num, FILE *file)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (queue_id >= dev->data->nb_tx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid Tx queue_id=%u\n", queue_id);
		return -EINVAL;
	}

	if (file == NULL) {
		RTE_ETHDEV_LOG(ERR, "Invalid file (NULL)\n");
		return -EINVAL;
	}

	if (*dev->dev_ops->eth_tx_descriptor_dump == NULL)
		return -ENOTSUP;

	return eth_err(port_id, (*dev->dev_ops->eth_tx_descriptor_dump)(dev,
						queue_id, offset, num, file));
}

int
rte_eth_buffer_split_get_supported_hdr_ptypes(uint16_t port_id, uint32_t *ptypes, int num)
{
	int i, j;
	struct rte_eth_dev *dev;
	const uint32_t *all_types;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (ptypes == NULL && num > 0) {
		RTE_ETHDEV_LOG(ERR,
			"Cannot get ethdev port %u supported header protocol types to NULL when array size is non zero\n",
			port_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->buffer_split_supported_hdr_ptypes_get == NULL)
		return -ENOTSUP;
	all_types = (*dev->dev_ops->buffer_split_supported_hdr_ptypes_get)(dev);

	if (all_types == NULL)
		return 0;

	for (i = 0, j = 0; all_types[i] != RTE_PTYPE_UNKNOWN; ++i) {
		if (j < num) {
			ptypes[j] = all_types[i];

			rte_eth_trace_buffer_split_get_supported_hdr_ptypes(
							port_id, j, ptypes[j]);
		}
		j++;
	}

	return j;
}

int rte_eth_dev_count_aggr_ports(uint16_t port_id)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (*dev->dev_ops->count_aggr_ports == NULL)
		return 0;
	ret = eth_err(port_id, (*dev->dev_ops->count_aggr_ports)(dev));

	rte_eth_trace_count_aggr_ports(port_id, ret);

	return ret;
}

int rte_eth_dev_map_aggr_tx_affinity(uint16_t port_id, uint16_t tx_queue_id,
				     uint8_t affinity)
{
	struct rte_eth_dev *dev;
	int aggr_ports;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (tx_queue_id >= dev->data->nb_tx_queues) {
		RTE_ETHDEV_LOG(ERR, "Invalid Tx queue_id=%u\n", tx_queue_id);
		return -EINVAL;
	}

	if (*dev->dev_ops->map_aggr_tx_affinity == NULL)
		return -ENOTSUP;

	if (dev->data->dev_configured == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Port %u must be configured before Tx affinity mapping\n",
			port_id);
		return -EINVAL;
	}

	if (dev->data->dev_started) {
		RTE_ETHDEV_LOG(ERR,
			"Port %u must be stopped to allow configuration\n",
			port_id);
		return -EBUSY;
	}

	aggr_ports = rte_eth_dev_count_aggr_ports(port_id);
	if (aggr_ports == 0) {
		RTE_ETHDEV_LOG(ERR,
			"Port %u has no aggregated port\n",
			port_id);
		return -ENOTSUP;
	}

	if (affinity > aggr_ports) {
		RTE_ETHDEV_LOG(ERR,
			"Port %u map invalid affinity %u exceeds the maximum number %u\n",
			port_id, affinity, aggr_ports);
		return -EINVAL;
	}

	ret = eth_err(port_id, (*dev->dev_ops->map_aggr_tx_affinity)(dev,
				tx_queue_id, affinity));

	rte_eth_trace_map_aggr_tx_affinity(port_id, tx_queue_id, affinity, ret);

	return ret;
}

RTE_LOG_REGISTER_DEFAULT(rte_eth_dev_logtype, INFO);
