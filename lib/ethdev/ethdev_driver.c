/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <stdlib.h>
#include <pthread.h>

#include <rte_kvargs.h>
#include <rte_malloc.h>

#include "ethdev_driver.h"
#include "ethdev_private.h"

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

static uint16_t
eth_dev_find_free_port(void)
	__rte_exclusive_locks_required(rte_mcfg_ethdev_get_lock())
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
	__rte_exclusive_locks_required(rte_mcfg_ethdev_get_lock())
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

	/* Synchronize port creation between primary and secondary processes. */
	rte_spinlock_lock(rte_mcfg_ethdev_get_lock());

	if (eth_dev_shared_data_prepare() == NULL)
		goto unlock;

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
	eth_dev->data->backer_port_id = RTE_MAX_ETHPORTS;
	eth_dev->data->mtu = RTE_ETHER_MTU;
	pthread_mutex_init(&eth_dev->data->flow_ops_mutex, NULL);
	RTE_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	eth_dev_shared_data->allocated_ports++;

unlock:
	rte_spinlock_unlock(rte_mcfg_ethdev_get_lock());

	return eth_dev;
}

struct rte_eth_dev *
rte_eth_dev_allocated(const char *name)
{
	struct rte_eth_dev *ethdev;

	rte_spinlock_lock(rte_mcfg_ethdev_get_lock());

	if (eth_dev_shared_data_prepare() != NULL)
		ethdev = eth_dev_allocated(name);
	else
		ethdev = NULL;

	rte_spinlock_unlock(rte_mcfg_ethdev_get_lock());

	return ethdev;
}

/*
 * Attach to a port already registered by the primary process, which
 * makes sure that the same device would have the same port ID both
 * in the primary and secondary process.
 */
struct rte_eth_dev *
rte_eth_dev_attach_secondary(const char *name)
{
	uint16_t i;
	struct rte_eth_dev *eth_dev = NULL;

	/* Synchronize port attachment to primary port creation and release. */
	rte_spinlock_lock(rte_mcfg_ethdev_get_lock());

	if (eth_dev_shared_data_prepare() == NULL)
		goto unlock;

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

unlock:
	rte_spinlock_unlock(rte_mcfg_ethdev_get_lock());
	return eth_dev;
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

	/*
	 * for secondary process, at that point we expect device
	 * to be already 'usable', so shared data and all function pointers
	 * for fast-path devops have to be setup properly inside rte_eth_dev.
	 */
	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		eth_dev_fp_ops_setup(rte_eth_fp_ops + dev->data->port_id, dev);

	rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_NEW, NULL);

	dev->state = RTE_ETH_DEV_ATTACHED;
}

int
rte_eth_dev_release_port(struct rte_eth_dev *eth_dev)
{
	int ret;

	if (eth_dev == NULL)
		return -EINVAL;

	rte_spinlock_lock(rte_mcfg_ethdev_get_lock());
	if (eth_dev_shared_data_prepare() == NULL)
		ret = -EINVAL;
	else
		ret = 0;
	rte_spinlock_unlock(rte_mcfg_ethdev_get_lock());
	if (ret != 0)
		return ret;

	if (eth_dev->state != RTE_ETH_DEV_UNUSED)
		rte_eth_dev_callback_process(eth_dev,
				RTE_ETH_EVENT_DESTROY, NULL);

	eth_dev_fp_ops_reset(rte_eth_fp_ops + eth_dev->data->port_id);

	rte_spinlock_lock(rte_mcfg_ethdev_get_lock());

	eth_dev->state = RTE_ETH_DEV_UNUSED;
	eth_dev->device = NULL;
	eth_dev->process_private = NULL;
	eth_dev->intr_handle = NULL;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;
	eth_dev->tx_pkt_prepare = NULL;
	eth_dev->rx_queue_count = NULL;
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
		eth_dev->data = NULL;

		eth_dev_shared_data->allocated_ports--;
		eth_dev_shared_data_release();
	}

	rte_spinlock_unlock(rte_mcfg_ethdev_get_lock());

	return 0;
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

	if (*ethdev_init == NULL)
		return -EINVAL;

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

	if (*ethdev_uninit == NULL)
		return -EINVAL;

	ret = ethdev_uninit(ethdev);
	if (ret)
		return ret;

	return rte_eth_dev_release_port(ethdev);
}

struct rte_eth_dev *
rte_eth_dev_get_by_name(const char *name)
{
	uint16_t pid;

	if (rte_eth_dev_get_port_by_name(name, &pid))
		return NULL;

	return &rte_eth_devices[pid];
}

int
rte_eth_dev_is_rx_hairpin_queue(struct rte_eth_dev *dev, uint16_t queue_id)
{
	if (dev->data->rx_queue_state[queue_id] == RTE_ETH_QUEUE_STATE_HAIRPIN)
		return 1;
	return 0;
}

int
rte_eth_dev_is_tx_hairpin_queue(struct rte_eth_dev *dev, uint16_t queue_id)
{
	if (dev->data->tx_queue_state[queue_id] == RTE_ETH_QUEUE_STATE_HAIRPIN)
		return 1;
	return 0;
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
			/* fallthrough */

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
			if (eth_da->type != RTE_ETH_REPRESENTOR_NONE) {
				RTE_ETHDEV_LOG(ERR, "duplicated representor key: %s\n",
					dargs);
				result = -1;
				goto parse_cleanup;
			}
			result = rte_eth_devargs_parse_representor_ports(
					pair->value, eth_da);
			if (result < 0)
				goto parse_cleanup;
		}
	}

parse_cleanup:
	free(args.str);

	return result;
}

static inline int
eth_dev_dma_mzone_name(char *name, size_t len, uint16_t port_id, uint16_t queue_id,
		const char *ring_name)
{
	return snprintf(name, len, "eth_p%d_q%d_%s",
			port_id, queue_id, ring_name);
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

const struct rte_memzone *
rte_eth_dma_zone_reserve(const struct rte_eth_dev *dev, const char *ring_name,
			 uint16_t queue_id, size_t size, unsigned int align,
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
rte_eth_hairpin_queue_peer_bind(uint16_t cur_port, uint16_t cur_queue,
				struct rte_hairpin_peer_info *peer_info,
				uint32_t direction)
{
	struct rte_eth_dev *dev;

	if (peer_info == NULL)
		return -EINVAL;

	/* No need to check the validity again. */
	dev = &rte_eth_devices[cur_port];
	if (*dev->dev_ops->hairpin_queue_peer_bind == NULL)
		return -ENOTSUP;

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
	if (*dev->dev_ops->hairpin_queue_peer_unbind == NULL)
		return -ENOTSUP;

	return (*dev->dev_ops->hairpin_queue_peer_unbind)(dev, cur_queue,
							  direction);
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
	if (*dev->dev_ops->hairpin_queue_peer_update == NULL)
		return -ENOTSUP;

	return (*dev->dev_ops->hairpin_queue_peer_update)(dev, peer_queue,
					cur_info, peer_info, direction);
}

int
rte_eth_ip_reassembly_dynfield_register(int *field_offset, int *flag_offset)
{
	static const struct rte_mbuf_dynfield field_desc = {
		.name = RTE_MBUF_DYNFIELD_IP_REASSEMBLY_NAME,
		.size = sizeof(rte_eth_ip_reassembly_dynfield_t),
		.align = __alignof__(rte_eth_ip_reassembly_dynfield_t),
	};
	static const struct rte_mbuf_dynflag ip_reassembly_dynflag = {
		.name = RTE_MBUF_DYNFLAG_IP_REASSEMBLY_INCOMPLETE_NAME,
	};
	int offset;

	offset = rte_mbuf_dynfield_register(&field_desc);
	if (offset < 0)
		return -1;
	if (field_offset != NULL)
		*field_offset = offset;

	offset = rte_mbuf_dynflag_register(&ip_reassembly_dynflag);
	if (offset < 0)
		return -1;
	if (flag_offset != NULL)
		*flag_offset = offset;

	return 0;
}

uint16_t
rte_eth_pkt_burst_dummy(void *queue __rte_unused,
		struct rte_mbuf **pkts __rte_unused,
		uint16_t nb_pkts __rte_unused)
{
	return 0;
}

int
rte_eth_representor_id_get(uint16_t port_id,
			   enum rte_eth_representor_type type,
			   int controller, int pf, int representor_port,
			   uint16_t *repr_id)
{
	int ret, n, count;
	uint32_t i;
	struct rte_eth_representor_info *info = NULL;
	size_t size;

	if (type == RTE_ETH_REPRESENTOR_NONE)
		return 0;
	if (repr_id == NULL)
		return -EINVAL;

	/* Get PMD representor range info. */
	ret = rte_eth_representor_info_get(port_id, NULL);
	if (ret == -ENOTSUP && type == RTE_ETH_REPRESENTOR_VF &&
	    controller == -1 && pf == -1) {
		/* Direct mapping for legacy VF representor. */
		*repr_id = representor_port;
		return 0;
	} else if (ret < 0) {
		return ret;
	}
	n = ret;
	size = sizeof(*info) + n * sizeof(info->ranges[0]);
	info = calloc(1, size);
	if (info == NULL)
		return -ENOMEM;
	info->nb_ranges_alloc = n;
	ret = rte_eth_representor_info_get(port_id, info);
	if (ret < 0)
		goto out;

	/* Default controller and pf to caller. */
	if (controller == -1)
		controller = info->controller;
	if (pf == -1)
		pf = info->pf;

	/* Locate representor ID. */
	ret = -ENOENT;
	for (i = 0; i < info->nb_ranges; ++i) {
		if (info->ranges[i].type != type)
			continue;
		if (info->ranges[i].controller != controller)
			continue;
		if (info->ranges[i].id_end < info->ranges[i].id_base) {
			RTE_ETHDEV_LOG(WARNING, "Port %hu invalid representor ID Range %u - %u, entry %d\n",
				port_id, info->ranges[i].id_base,
				info->ranges[i].id_end, i);
			continue;

		}
		count = info->ranges[i].id_end - info->ranges[i].id_base + 1;
		switch (info->ranges[i].type) {
		case RTE_ETH_REPRESENTOR_PF:
			if (pf < info->ranges[i].pf ||
			    pf >= info->ranges[i].pf + count)
				continue;
			*repr_id = info->ranges[i].id_base +
				   (pf - info->ranges[i].pf);
			ret = 0;
			goto out;
		case RTE_ETH_REPRESENTOR_VF:
			if (info->ranges[i].pf != pf)
				continue;
			if (representor_port < info->ranges[i].vf ||
			    representor_port >= info->ranges[i].vf + count)
				continue;
			*repr_id = info->ranges[i].id_base +
				   (representor_port - info->ranges[i].vf);
			ret = 0;
			goto out;
		case RTE_ETH_REPRESENTOR_SF:
			if (info->ranges[i].pf != pf)
				continue;
			if (representor_port < info->ranges[i].sf ||
			    representor_port >= info->ranges[i].sf + count)
				continue;
			*repr_id = info->ranges[i].id_base +
			      (representor_port - info->ranges[i].sf);
			ret = 0;
			goto out;
		default:
			break;
		}
	}
out:
	free(info);
	return ret;
}

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
