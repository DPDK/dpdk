/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <ctype.h>
#include <stdalign.h>
#include <stdlib.h>
#include <pthread.h>

#include <eal_export.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>

#include "ethdev_driver.h"
#include "ethdev_private.h"
#include "rte_flow_driver.h"

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
	__rte_requires_capability(rte_mcfg_ethdev_get_lock())
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
	__rte_requires_capability(rte_mcfg_ethdev_get_lock())
{
	struct rte_eth_dev *eth_dev = &rte_eth_devices[port_id];

	eth_dev->data = &eth_dev_shared_data->data[port_id];

	return eth_dev;
}

static void
eth_dev_set_dummy_fops(struct rte_eth_dev *eth_dev)
{
	eth_dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
	eth_dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;
	eth_dev->tx_pkt_prepare = rte_eth_tx_pkt_prepare_dummy;
	eth_dev->rx_queue_count = rte_eth_queue_count_dummy;
	eth_dev->tx_queue_count = rte_eth_queue_count_dummy;
	eth_dev->rx_descriptor_status = rte_eth_descriptor_status_dummy;
	eth_dev->tx_descriptor_status = rte_eth_descriptor_status_dummy;
	eth_dev->recycle_tx_mbufs_reuse = rte_eth_recycle_tx_mbufs_reuse_dummy;
	eth_dev->recycle_rx_descriptors_refill = rte_eth_recycle_rx_descriptors_refill_dummy;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_dev_allocate)
struct rte_eth_dev *
rte_eth_dev_allocate(const char *name)
{
	uint16_t port_id;
	struct rte_eth_dev *eth_dev = NULL;
	size_t name_len;

	name_len = strnlen(name, RTE_ETH_NAME_MAX_LEN);
	if (name_len == 0) {
		RTE_ETHDEV_LOG_LINE(ERR, "Zero length Ethernet device name");
		return NULL;
	}

	if (name_len >= RTE_ETH_NAME_MAX_LEN) {
		RTE_ETHDEV_LOG_LINE(ERR, "Ethernet device name is too long");
		return NULL;
	}

	/* Synchronize port creation between primary and secondary processes. */
	rte_spinlock_lock(rte_mcfg_ethdev_get_lock());

	if (eth_dev_shared_data_prepare() == NULL)
		goto unlock;

	if (eth_dev_allocated(name) != NULL) {
		RTE_ETHDEV_LOG_LINE(ERR,
			"Ethernet device with name %s already allocated",
			name);
		goto unlock;
	}

	port_id = eth_dev_find_free_port();
	if (port_id == RTE_MAX_ETHPORTS) {
		RTE_ETHDEV_LOG_LINE(ERR,
			"Reached maximum number of Ethernet ports");
		goto unlock;
	}

	eth_dev = eth_dev_get(port_id);
	eth_dev_set_dummy_fops(eth_dev);
	eth_dev->flow_fp_ops = &rte_flow_fp_default_ops;
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

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_dev_allocated)
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
RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_dev_attach_secondary)
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
		RTE_ETHDEV_LOG_LINE(ERR,
			"Device %s is not driven by the primary process",
			name);
	} else {
		eth_dev = eth_dev_get(i);
		RTE_ASSERT(eth_dev->data->port_id == i);
	}

unlock:
	rte_spinlock_unlock(rte_mcfg_ethdev_get_lock());
	return eth_dev;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_dev_callback_process)
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

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_dev_probing_finish)
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

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_dev_release_port)
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

	eth_dev->flow_fp_ops = &rte_flow_fp_default_ops;

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

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_dev_create)
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
			/* try alloc private data on device-local node. */
			ethdev->data->dev_private = rte_zmalloc_socket(
				name, priv_data_size, RTE_CACHE_LINE_SIZE,
				device->numa_node);

			/* fall back to alloc on any socket on failure */
			if (ethdev->data->dev_private == NULL) {
				ethdev->data->dev_private = rte_zmalloc(name,
						priv_data_size, RTE_CACHE_LINE_SIZE);

				if (ethdev->data->dev_private == NULL) {
					RTE_ETHDEV_LOG_LINE(ERR, "failed to allocate private data");
					retval = -ENOMEM;
					goto probe_failed;
				}
				/* got memory, but not local, so issue warning */
				RTE_ETHDEV_LOG_LINE(WARNING,
						"Private data for ethdev '%s' not allocated on local NUMA node %d",
						device->name, device->numa_node);
			}
		}
	} else {
		ethdev = rte_eth_dev_attach_secondary(name);
		if (!ethdev) {
			RTE_ETHDEV_LOG_LINE(ERR,
				"secondary process attach failed, ethdev doesn't exist");
			return  -ENODEV;
		}
	}

	ethdev->device = device;

	if (ethdev_bus_specific_init) {
		retval = ethdev_bus_specific_init(ethdev, bus_init_params);
		if (retval) {
			RTE_ETHDEV_LOG_LINE(ERR,
				"ethdev bus specific initialisation failed");
			goto probe_failed;
		}
	}

	retval = ethdev_init(ethdev, init_params);
	if (retval) {
		RTE_ETHDEV_LOG_LINE(ERR, "ethdev initialisation failed");
		goto probe_failed;
	}

	rte_eth_dev_probing_finish(ethdev);

	return retval;

probe_failed:
	rte_eth_dev_release_port(ethdev);
	return retval;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_dev_destroy)
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

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_dev_get_by_name)
struct rte_eth_dev *
rte_eth_dev_get_by_name(const char *name)
{
	uint16_t pid;

	if (rte_eth_dev_get_port_by_name(name, &pid))
		return NULL;

	return &rte_eth_devices[pid];
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_dev_is_rx_hairpin_queue)
int
rte_eth_dev_is_rx_hairpin_queue(struct rte_eth_dev *dev, uint16_t queue_id)
{
	if (dev->data->rx_queue_state[queue_id] == RTE_ETH_QUEUE_STATE_HAIRPIN)
		return 1;
	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_dev_is_tx_hairpin_queue)
int
rte_eth_dev_is_tx_hairpin_queue(struct rte_eth_dev *dev, uint16_t queue_id)
{
	if (dev->data->tx_queue_state[queue_id] == RTE_ETH_QUEUE_STATE_HAIRPIN)
		return 1;
	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_dev_internal_reset)
void
rte_eth_dev_internal_reset(struct rte_eth_dev *dev)
{
	if (dev->data->dev_started) {
		RTE_ETHDEV_LOG_LINE(ERR, "Port %u must be stopped to allow reset",
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
			if (*letter == ']') {
				/* For devargs having singles lists move to state 2 once letter
				 * becomes ']' so each can be considered as different pair key
				 * value. But in nested lists case e.g. multiple representors
				 * case i.e. [pf[0-3],pfvf[3,4-6]], complete nested list should
				 * be considered as one pair value, hence checking if end of outer
				 * list ']' is reached else stay on state 3.
				 */
				if ((strcmp("representor", pair->key) == 0) &&
				    (*(letter + 1) != '\0' && *(letter + 2) != '\0' &&
				     *(letter + 3) != '\0')			    &&
				    ((*(letter + 2) == 'p' && *(letter + 3) == 'f')   ||
				     (*(letter + 2) == 'v' && *(letter + 3) == 'f')   ||
				     (*(letter + 2) == 's' && *(letter + 3) == 'f')   ||
				     (*(letter + 2) == 'c' && isdigit(*(letter + 3))) ||
				     (*(letter + 2) == '[' && isdigit(*(letter + 3))) ||
				     (isdigit(*(letter + 2)))))
					state = 3;
				else
					state = 2;
			} else if (*letter == '\0') {
				return -EINVAL;
			}
			break;
		}
		letter++;
	}
}

static int
devargs_parse_representor_ports(struct rte_eth_devargs *eth_devargs, char
				*da_val, unsigned int da_idx, unsigned int nb_da)
{
	struct rte_eth_devargs *eth_da;
	int result = 0;

	if (da_idx + 1 > nb_da) {
		RTE_ETHDEV_LOG_LINE(ERR, "Devargs parsed %d > max array size %d",
			       da_idx + 1, nb_da);
		result = -1;
		goto parse_cleanup;
	}
	eth_da = &eth_devargs[da_idx];
	memset(eth_da, 0, sizeof(*eth_da));
	RTE_ETHDEV_LOG_LINE(DEBUG, "	  Devargs idx %d value %s", da_idx, da_val);
	result = rte_eth_devargs_parse_representor_ports(da_val, eth_da);

parse_cleanup:
	return result;
}

static int
eth_dev_tokenise_representor_list(char *p_val, struct rte_eth_devargs *eth_devargs,
				  unsigned int nb_da)
{
	char da_val[BUFSIZ], str[BUFSIZ];
	bool is_rep_portid_list = true;
	unsigned int devargs = 0;
	int result = 0, len = 0;
	int i = 0, j = 0;
	char *pos;

	pos = p_val;
	/* Length of consolidated list */
	while (*pos++ != '\0') {
		len++;
		if (isalpha(*pos))
			is_rep_portid_list = false;
	}

	/* List of representor portIDs i.e.[1,2,3] should be considered as single representor case*/
	if (is_rep_portid_list) {
		result = devargs_parse_representor_ports(eth_devargs, p_val, 0, 1);
		if (result < 0)
			return result;

		devargs++;
		return devargs;
	}

	memset(str, 0, BUFSIZ);
	memset(da_val, 0, BUFSIZ);
	/* Remove the exterior [] of the consolidated list */
	strncpy(str, &p_val[1], len - 2);
	while (1) {
		if (str[i] == '\0') {
			if (da_val[0] != '\0') {
				result = devargs_parse_representor_ports(eth_devargs, da_val,
									 devargs, nb_da);
				if (result < 0)
					goto parse_cleanup;

				devargs++;
			}
			break;
		}
		if (str[i] == ',' || str[i] == '[') {
			if (str[i] == ',') {
				if (da_val[0] != '\0') {
					da_val[j + 1] = '\0';
					result = devargs_parse_representor_ports(eth_devargs,
										 da_val, devargs,
										 nb_da);
					if (result < 0)
						goto parse_cleanup;

					devargs++;
					j = 0;
					memset(da_val, 0, BUFSIZ);
				}
			}

			if (str[i] == '[') {
				while (str[i] != ']' || isalpha(str[i + 1])) {
					da_val[j] = str[i];
					j++;
					i++;
				}
				da_val[j] = ']';
				da_val[j + 1] = '\0';
				result = devargs_parse_representor_ports(eth_devargs, da_val,
									 devargs, nb_da);
				if (result < 0)
					goto parse_cleanup;

				devargs++;
				j = 0;
				memset(da_val, 0, BUFSIZ);
			}
		} else {
			da_val[j] = str[i];
			j++;
		}
		i++;
	}
	result = devargs;

parse_cleanup:
	return result;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_devargs_parse)
int
rte_eth_devargs_parse(const char *dargs, struct rte_eth_devargs *eth_devargs,
		      unsigned int nb_da)
{
	struct rte_kvargs_pair *pair;
	struct rte_kvargs args;
	bool dup_rep = false;
	int devargs = 0;
	unsigned int i;
	int result = 0;

	memset(eth_devargs, 0, nb_da * sizeof(*eth_devargs));

	result = eth_dev_devargs_tokenise(&args, dargs);
	if (result < 0)
		goto parse_cleanup;

	for (i = 0; i < args.count; i++) {
		pair = &args.pairs[i];
		if (strcmp("representor", pair->key) == 0) {
			if (dup_rep) {
				RTE_ETHDEV_LOG_LINE(ERR, "Duplicated representor key: %s",
						    pair->value);
				result = -1;
				goto parse_cleanup;
			}

			RTE_ETHDEV_LOG_LINE(DEBUG, "Devarg pattern: %s", pair->value);
			if (pair->value[0] == '[') {
				/* Multiple representor list case */
				devargs = eth_dev_tokenise_representor_list(pair->value,
									    eth_devargs, nb_da);
				if (devargs < 0)
					goto parse_cleanup;
			} else {
				/* Single representor case */
				devargs = devargs_parse_representor_ports(eth_devargs, pair->value,
									  0, 1);
				if (devargs < 0)
					goto parse_cleanup;
				devargs++;
			}
			dup_rep = true;
		}
	}
	RTE_ETHDEV_LOG_LINE(DEBUG, "Total devargs parsed %d", devargs);
	result = devargs;

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

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_dma_zone_free)
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
		RTE_ETHDEV_LOG_LINE(ERR, "ring name too long");
		return -ENAMETOOLONG;
	}

	mz = rte_memzone_lookup(z_name);
	if (mz)
		rc = rte_memzone_free(mz);
	else
		rc = -ENOENT;

	return rc;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_dma_zone_reserve)
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
		RTE_ETHDEV_LOG_LINE(ERR, "ring name too long");
		rte_errno = ENAMETOOLONG;
		return NULL;
	}

	mz = rte_memzone_lookup(z_name);
	if (mz) {
		if ((socket_id != SOCKET_ID_ANY && socket_id != mz->socket_id) ||
				size > mz->len ||
				((uintptr_t)mz->addr & (align - 1)) != 0) {
			RTE_ETHDEV_LOG_LINE(ERR,
				"memzone %s does not justify the requested attributes",
				mz->name);
			return NULL;
		}

		return mz;
	}

	return rte_memzone_reserve_aligned(z_name, size, socket_id,
			RTE_MEMZONE_IOVA_CONTIG, align);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_hairpin_queue_peer_bind)
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
	if (dev->dev_ops->hairpin_queue_peer_bind == NULL)
		return -ENOTSUP;

	return dev->dev_ops->hairpin_queue_peer_bind(dev, cur_queue, peer_info, direction);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_hairpin_queue_peer_unbind)
int
rte_eth_hairpin_queue_peer_unbind(uint16_t cur_port, uint16_t cur_queue,
				  uint32_t direction)
{
	struct rte_eth_dev *dev;

	/* No need to check the validity again. */
	dev = &rte_eth_devices[cur_port];
	if (dev->dev_ops->hairpin_queue_peer_unbind == NULL)
		return -ENOTSUP;

	return dev->dev_ops->hairpin_queue_peer_unbind(dev, cur_queue, direction);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_hairpin_queue_peer_update)
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
	if (dev->dev_ops->hairpin_queue_peer_update == NULL)
		return -ENOTSUP;

	return dev->dev_ops->hairpin_queue_peer_update(dev, peer_queue,
						       cur_info, peer_info, direction);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_ip_reassembly_dynfield_register)
int
rte_eth_ip_reassembly_dynfield_register(int *field_offset, int *flag_offset)
{
	static const struct rte_mbuf_dynfield field_desc = {
		.name = RTE_MBUF_DYNFIELD_IP_REASSEMBLY_NAME,
		.size = sizeof(rte_eth_ip_reassembly_dynfield_t),
		.align = alignof(rte_eth_ip_reassembly_dynfield_t),
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

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_pkt_burst_dummy)
uint16_t
rte_eth_pkt_burst_dummy(void *queue __rte_unused,
		struct rte_mbuf **pkts __rte_unused,
		uint16_t nb_pkts __rte_unused)
{
	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_tx_pkt_prepare_dummy)
uint16_t
rte_eth_tx_pkt_prepare_dummy(void *queue __rte_unused,
		struct rte_mbuf **pkts __rte_unused,
		uint16_t nb_pkts)
{
	return nb_pkts;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_queue_count_dummy)
int
rte_eth_queue_count_dummy(void *queue __rte_unused)
{
	return -ENOTSUP;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_descriptor_status_dummy)
int
rte_eth_descriptor_status_dummy(void *queue __rte_unused,
		uint16_t offset __rte_unused)
{
	return -ENOTSUP;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_recycle_tx_mbufs_reuse_dummy)
uint16_t
rte_eth_recycle_tx_mbufs_reuse_dummy(void *queue __rte_unused,
		struct rte_eth_recycle_rxq_info *recycle_rxq_info __rte_unused)
{
	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_recycle_rx_descriptors_refill_dummy)
void
rte_eth_recycle_rx_descriptors_refill_dummy(void *queue __rte_unused,
		uint16_t nb __rte_unused)
{
	/* No action. */
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_representor_id_get)
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
			RTE_ETHDEV_LOG_LINE(WARNING, "Port %hu invalid representor ID Range %u - %u, entry %d",
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

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_switch_domain_alloc)
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

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_switch_domain_free)
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

RTE_EXPORT_INTERNAL_SYMBOL(rte_eth_get_restore_flags)
uint64_t
rte_eth_get_restore_flags(struct rte_eth_dev *dev, enum rte_eth_dev_operation op)
{
	if (dev->dev_ops->get_restore_flags != NULL)
		return dev->dev_ops->get_restore_flags(dev, op);
	else
		return RTE_ETH_RESTORE_ALL;
}
