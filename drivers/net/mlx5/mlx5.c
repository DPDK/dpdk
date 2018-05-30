/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <net/if.h>
#include <sys/mman.h>
#include <linux/rtnetlink.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_malloc.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_config.h>
#include <rte_eal_memconfig.h>
#include <rte_kvargs.h>
#include <rte_rwlock.h>
#include <rte_spinlock.h>

#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"
#include "mlx5_glue.h"
#include "mlx5_mr.h"

/* Device parameter to enable RX completion queue compression. */
#define MLX5_RXQ_CQE_COMP_EN "rxq_cqe_comp_en"

/* Device parameter to enable Multi-Packet Rx queue. */
#define MLX5_RX_MPRQ_EN "mprq_en"

/* Device parameter to configure log 2 of the number of strides for MPRQ. */
#define MLX5_RX_MPRQ_LOG_STRIDE_NUM "mprq_log_stride_num"

/* Device parameter to limit the size of memcpy'd packet for MPRQ. */
#define MLX5_RX_MPRQ_MAX_MEMCPY_LEN "mprq_max_memcpy_len"

/* Device parameter to set the minimum number of Rx queues to enable MPRQ. */
#define MLX5_RXQS_MIN_MPRQ "rxqs_min_mprq"

/* Device parameter to configure inline send. */
#define MLX5_TXQ_INLINE "txq_inline"

/*
 * Device parameter to configure the number of TX queues threshold for
 * enabling inline send.
 */
#define MLX5_TXQS_MIN_INLINE "txqs_min_inline"

/* Device parameter to enable multi-packet send WQEs. */
#define MLX5_TXQ_MPW_EN "txq_mpw_en"

/* Device parameter to include 2 dsegs in the title WQEBB. */
#define MLX5_TXQ_MPW_HDR_DSEG_EN "txq_mpw_hdr_dseg_en"

/* Device parameter to limit the size of inlining packet. */
#define MLX5_TXQ_MAX_INLINE_LEN "txq_max_inline_len"

/* Device parameter to enable hardware Tx vector. */
#define MLX5_TX_VEC_EN "tx_vec_en"

/* Device parameter to enable hardware Rx vector. */
#define MLX5_RX_VEC_EN "rx_vec_en"

/* Allow L3 VXLAN flow creation. */
#define MLX5_L3_VXLAN_EN "l3_vxlan_en"

/* Activate Netlink support in VF mode. */
#define MLX5_VF_NL_EN "vf_nl_en"

#ifndef HAVE_IBV_MLX5_MOD_MPW
#define MLX5DV_CONTEXT_FLAGS_MPW_ALLOWED (1 << 2)
#define MLX5DV_CONTEXT_FLAGS_ENHANCED_MPW (1 << 3)
#endif

#ifndef HAVE_IBV_MLX5_MOD_CQE_128B_COMP
#define MLX5DV_CONTEXT_FLAGS_CQE_128B_COMP (1 << 4)
#endif

static const char *MZ_MLX5_PMD_SHARED_DATA = "mlx5_pmd_shared_data";

/* Shared memory between primary and secondary processes. */
struct mlx5_shared_data *mlx5_shared_data;

/* Spinlock for mlx5_shared_data allocation. */
static rte_spinlock_t mlx5_shared_data_lock = RTE_SPINLOCK_INITIALIZER;

/** Driver-specific log messages type. */
int mlx5_logtype;

/**
 * Prepare shared data between primary and secondary process.
 */
static void
mlx5_prepare_shared_data(void)
{
	const struct rte_memzone *mz;

	rte_spinlock_lock(&mlx5_shared_data_lock);
	if (mlx5_shared_data == NULL) {
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			/* Allocate shared memory. */
			mz = rte_memzone_reserve(MZ_MLX5_PMD_SHARED_DATA,
						 sizeof(*mlx5_shared_data),
						 SOCKET_ID_ANY, 0);
		} else {
			/* Lookup allocated shared memory. */
			mz = rte_memzone_lookup(MZ_MLX5_PMD_SHARED_DATA);
		}
		if (mz == NULL)
			rte_panic("Cannot allocate mlx5 shared data\n");
		mlx5_shared_data = mz->addr;
		/* Initialize shared data. */
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			LIST_INIT(&mlx5_shared_data->mem_event_cb_list);
			rte_rwlock_init(&mlx5_shared_data->mem_event_rwlock);
		}
		rte_mem_event_callback_register("MLX5_MEM_EVENT_CB",
						mlx5_mr_mem_event_cb, NULL);
	}
	rte_spinlock_unlock(&mlx5_shared_data_lock);
}

/**
 * Retrieve integer value from environment variable.
 *
 * @param[in] name
 *   Environment variable name.
 *
 * @return
 *   Integer value, 0 if the variable is not set.
 */
int
mlx5_getenv_int(const char *name)
{
	const char *val = getenv(name);

	if (val == NULL)
		return 0;
	return atoi(val);
}

/**
 * Verbs callback to allocate a memory. This function should allocate the space
 * according to the size provided residing inside a huge page.
 * Please note that all allocation must respect the alignment from libmlx5
 * (i.e. currently sysconf(_SC_PAGESIZE)).
 *
 * @param[in] size
 *   The size in bytes of the memory to allocate.
 * @param[in] data
 *   A pointer to the callback data.
 *
 * @return
 *   Allocated buffer, NULL otherwise and rte_errno is set.
 */
static void *
mlx5_alloc_verbs_buf(size_t size, void *data)
{
	struct priv *priv = data;
	void *ret;
	size_t alignment = sysconf(_SC_PAGESIZE);
	unsigned int socket = SOCKET_ID_ANY;

	if (priv->verbs_alloc_ctx.type == MLX5_VERBS_ALLOC_TYPE_TX_QUEUE) {
		const struct mlx5_txq_ctrl *ctrl = priv->verbs_alloc_ctx.obj;

		socket = ctrl->socket;
	} else if (priv->verbs_alloc_ctx.type ==
		   MLX5_VERBS_ALLOC_TYPE_RX_QUEUE) {
		const struct mlx5_rxq_ctrl *ctrl = priv->verbs_alloc_ctx.obj;

		socket = ctrl->socket;
	}
	assert(data != NULL);
	ret = rte_malloc_socket(__func__, size, alignment, socket);
	if (!ret && size)
		rte_errno = ENOMEM;
	return ret;
}

/**
 * Verbs callback to free a memory.
 *
 * @param[in] ptr
 *   A pointer to the memory to free.
 * @param[in] data
 *   A pointer to the callback data.
 */
static void
mlx5_free_verbs_buf(void *ptr, void *data __rte_unused)
{
	assert(data != NULL);
	rte_free(ptr);
}

/**
 * DPDK callback to close the device.
 *
 * Destroy all queues and objects, free memory.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 */
static void
mlx5_dev_close(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int i;
	int ret;

	DRV_LOG(DEBUG, "port %u closing device \"%s\"",
		dev->data->port_id,
		((priv->ctx != NULL) ? priv->ctx->device->name : ""));
	/* In case mlx5_dev_stop() has not been called. */
	mlx5_dev_interrupt_handler_uninstall(dev);
	mlx5_traffic_disable(dev);
	/* Prevent crashes when queues are still in use. */
	dev->rx_pkt_burst = removed_rx_burst;
	dev->tx_pkt_burst = removed_tx_burst;
	if (priv->rxqs != NULL) {
		/* XXX race condition if mlx5_rx_burst() is still running. */
		usleep(1000);
		for (i = 0; (i != priv->rxqs_n); ++i)
			mlx5_rxq_release(dev, i);
		priv->rxqs_n = 0;
		priv->rxqs = NULL;
	}
	if (priv->txqs != NULL) {
		/* XXX race condition if mlx5_tx_burst() is still running. */
		usleep(1000);
		for (i = 0; (i != priv->txqs_n); ++i)
			mlx5_txq_release(dev, i);
		priv->txqs_n = 0;
		priv->txqs = NULL;
	}
	mlx5_flow_delete_drop_queue(dev);
	mlx5_mprq_free_mp(dev);
	mlx5_mr_release(dev);
	if (priv->pd != NULL) {
		assert(priv->ctx != NULL);
		claim_zero(mlx5_glue->dealloc_pd(priv->pd));
		claim_zero(mlx5_glue->close_device(priv->ctx));
	} else
		assert(priv->ctx == NULL);
	if (priv->rss_conf.rss_key != NULL)
		rte_free(priv->rss_conf.rss_key);
	if (priv->reta_idx != NULL)
		rte_free(priv->reta_idx);
	if (priv->primary_socket)
		mlx5_socket_uninit(dev);
	if (priv->config.vf)
		mlx5_nl_mac_addr_flush(dev);
	if (priv->nl_socket >= 0)
		close(priv->nl_socket);
	ret = mlx5_hrxq_ibv_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some hash Rx queue still remain",
			dev->data->port_id);
	ret = mlx5_ind_table_ibv_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some indirection table still remain",
			dev->data->port_id);
	ret = mlx5_rxq_ibv_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some Verbs Rx queue still remain",
			dev->data->port_id);
	ret = mlx5_rxq_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some Rx queues still remain",
			dev->data->port_id);
	ret = mlx5_txq_ibv_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some Verbs Tx queue still remain",
			dev->data->port_id);
	ret = mlx5_txq_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some Tx queues still remain",
			dev->data->port_id);
	ret = mlx5_flow_verify(dev);
	if (ret)
		DRV_LOG(WARNING, "port %u some flows still remain",
			dev->data->port_id);
	memset(priv, 0, sizeof(*priv));
}

const struct eth_dev_ops mlx5_dev_ops = {
	.dev_configure = mlx5_dev_configure,
	.dev_start = mlx5_dev_start,
	.dev_stop = mlx5_dev_stop,
	.dev_set_link_down = mlx5_set_link_down,
	.dev_set_link_up = mlx5_set_link_up,
	.dev_close = mlx5_dev_close,
	.promiscuous_enable = mlx5_promiscuous_enable,
	.promiscuous_disable = mlx5_promiscuous_disable,
	.allmulticast_enable = mlx5_allmulticast_enable,
	.allmulticast_disable = mlx5_allmulticast_disable,
	.link_update = mlx5_link_update,
	.stats_get = mlx5_stats_get,
	.stats_reset = mlx5_stats_reset,
	.xstats_get = mlx5_xstats_get,
	.xstats_reset = mlx5_xstats_reset,
	.xstats_get_names = mlx5_xstats_get_names,
	.dev_infos_get = mlx5_dev_infos_get,
	.dev_supported_ptypes_get = mlx5_dev_supported_ptypes_get,
	.vlan_filter_set = mlx5_vlan_filter_set,
	.rx_queue_setup = mlx5_rx_queue_setup,
	.tx_queue_setup = mlx5_tx_queue_setup,
	.rx_queue_release = mlx5_rx_queue_release,
	.tx_queue_release = mlx5_tx_queue_release,
	.flow_ctrl_get = mlx5_dev_get_flow_ctrl,
	.flow_ctrl_set = mlx5_dev_set_flow_ctrl,
	.mac_addr_remove = mlx5_mac_addr_remove,
	.mac_addr_add = mlx5_mac_addr_add,
	.mac_addr_set = mlx5_mac_addr_set,
	.set_mc_addr_list = mlx5_set_mc_addr_list,
	.mtu_set = mlx5_dev_set_mtu,
	.vlan_strip_queue_set = mlx5_vlan_strip_queue_set,
	.vlan_offload_set = mlx5_vlan_offload_set,
	.reta_update = mlx5_dev_rss_reta_update,
	.reta_query = mlx5_dev_rss_reta_query,
	.rss_hash_update = mlx5_rss_hash_update,
	.rss_hash_conf_get = mlx5_rss_hash_conf_get,
	.filter_ctrl = mlx5_dev_filter_ctrl,
	.rx_descriptor_status = mlx5_rx_descriptor_status,
	.tx_descriptor_status = mlx5_tx_descriptor_status,
	.rx_queue_intr_enable = mlx5_rx_intr_enable,
	.rx_queue_intr_disable = mlx5_rx_intr_disable,
	.is_removed = mlx5_is_removed,
};

static const struct eth_dev_ops mlx5_dev_sec_ops = {
	.stats_get = mlx5_stats_get,
	.stats_reset = mlx5_stats_reset,
	.xstats_get = mlx5_xstats_get,
	.xstats_reset = mlx5_xstats_reset,
	.xstats_get_names = mlx5_xstats_get_names,
	.dev_infos_get = mlx5_dev_infos_get,
	.rx_descriptor_status = mlx5_rx_descriptor_status,
	.tx_descriptor_status = mlx5_tx_descriptor_status,
};

/* Available operators in flow isolated mode. */
const struct eth_dev_ops mlx5_dev_ops_isolate = {
	.dev_configure = mlx5_dev_configure,
	.dev_start = mlx5_dev_start,
	.dev_stop = mlx5_dev_stop,
	.dev_set_link_down = mlx5_set_link_down,
	.dev_set_link_up = mlx5_set_link_up,
	.dev_close = mlx5_dev_close,
	.link_update = mlx5_link_update,
	.stats_get = mlx5_stats_get,
	.stats_reset = mlx5_stats_reset,
	.xstats_get = mlx5_xstats_get,
	.xstats_reset = mlx5_xstats_reset,
	.xstats_get_names = mlx5_xstats_get_names,
	.dev_infos_get = mlx5_dev_infos_get,
	.dev_supported_ptypes_get = mlx5_dev_supported_ptypes_get,
	.vlan_filter_set = mlx5_vlan_filter_set,
	.rx_queue_setup = mlx5_rx_queue_setup,
	.tx_queue_setup = mlx5_tx_queue_setup,
	.rx_queue_release = mlx5_rx_queue_release,
	.tx_queue_release = mlx5_tx_queue_release,
	.flow_ctrl_get = mlx5_dev_get_flow_ctrl,
	.flow_ctrl_set = mlx5_dev_set_flow_ctrl,
	.mac_addr_remove = mlx5_mac_addr_remove,
	.mac_addr_add = mlx5_mac_addr_add,
	.mac_addr_set = mlx5_mac_addr_set,
	.set_mc_addr_list = mlx5_set_mc_addr_list,
	.mtu_set = mlx5_dev_set_mtu,
	.vlan_strip_queue_set = mlx5_vlan_strip_queue_set,
	.vlan_offload_set = mlx5_vlan_offload_set,
	.filter_ctrl = mlx5_dev_filter_ctrl,
	.rx_descriptor_status = mlx5_rx_descriptor_status,
	.tx_descriptor_status = mlx5_tx_descriptor_status,
	.rx_queue_intr_enable = mlx5_rx_intr_enable,
	.rx_queue_intr_disable = mlx5_rx_intr_disable,
	.is_removed = mlx5_is_removed,
};

static struct {
	struct rte_pci_addr pci_addr; /* associated PCI address */
	uint32_t ports; /* physical ports bitfield. */
} mlx5_dev[32];

/**
 * Get device index in mlx5_dev[] from PCI bus address.
 *
 * @param[in] pci_addr
 *   PCI bus address to look for.
 *
 * @return
 *   mlx5_dev[] index on success, -1 on failure.
 */
static int
mlx5_dev_idx(struct rte_pci_addr *pci_addr)
{
	unsigned int i;
	int ret = -1;

	assert(pci_addr != NULL);
	for (i = 0; (i != RTE_DIM(mlx5_dev)); ++i) {
		if ((mlx5_dev[i].pci_addr.domain == pci_addr->domain) &&
		    (mlx5_dev[i].pci_addr.bus == pci_addr->bus) &&
		    (mlx5_dev[i].pci_addr.devid == pci_addr->devid) &&
		    (mlx5_dev[i].pci_addr.function == pci_addr->function))
			return i;
		if ((mlx5_dev[i].ports == 0) && (ret == -1))
			ret = i;
	}
	return ret;
}

/**
 * Verify and store value for device argument.
 *
 * @param[in] key
 *   Key argument to verify.
 * @param[in] val
 *   Value associated with key.
 * @param opaque
 *   User data.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_args_check(const char *key, const char *val, void *opaque)
{
	struct mlx5_dev_config *config = opaque;
	unsigned long tmp;

	errno = 0;
	tmp = strtoul(val, NULL, 0);
	if (errno) {
		rte_errno = errno;
		DRV_LOG(WARNING, "%s: \"%s\" is not a valid integer", key, val);
		return -rte_errno;
	}
	if (strcmp(MLX5_RXQ_CQE_COMP_EN, key) == 0) {
		config->cqe_comp = !!tmp;
	} else if (strcmp(MLX5_RX_MPRQ_EN, key) == 0) {
		config->mprq.enabled = !!tmp;
	} else if (strcmp(MLX5_RX_MPRQ_LOG_STRIDE_NUM, key) == 0) {
		config->mprq.stride_num_n = tmp;
	} else if (strcmp(MLX5_RX_MPRQ_MAX_MEMCPY_LEN, key) == 0) {
		config->mprq.max_memcpy_len = tmp;
	} else if (strcmp(MLX5_RXQS_MIN_MPRQ, key) == 0) {
		config->mprq.min_rxqs_num = tmp;
	} else if (strcmp(MLX5_TXQ_INLINE, key) == 0) {
		config->txq_inline = tmp;
	} else if (strcmp(MLX5_TXQS_MIN_INLINE, key) == 0) {
		config->txqs_inline = tmp;
	} else if (strcmp(MLX5_TXQ_MPW_EN, key) == 0) {
		config->mps = !!tmp ? config->mps : 0;
	} else if (strcmp(MLX5_TXQ_MPW_HDR_DSEG_EN, key) == 0) {
		config->mpw_hdr_dseg = !!tmp;
	} else if (strcmp(MLX5_TXQ_MAX_INLINE_LEN, key) == 0) {
		config->inline_max_packet_sz = tmp;
	} else if (strcmp(MLX5_TX_VEC_EN, key) == 0) {
		config->tx_vec_en = !!tmp;
	} else if (strcmp(MLX5_RX_VEC_EN, key) == 0) {
		config->rx_vec_en = !!tmp;
	} else if (strcmp(MLX5_L3_VXLAN_EN, key) == 0) {
		config->l3_vxlan_en = !!tmp;
	} else if (strcmp(MLX5_VF_NL_EN, key) == 0) {
		config->vf_nl_en = !!tmp;
	} else {
		DRV_LOG(WARNING, "%s: unknown parameter", key);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	return 0;
}

/**
 * Parse device parameters.
 *
 * @param config
 *   Pointer to device configuration structure.
 * @param devargs
 *   Device arguments structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_args(struct mlx5_dev_config *config, struct rte_devargs *devargs)
{
	const char **params = (const char *[]){
		MLX5_RXQ_CQE_COMP_EN,
		MLX5_RX_MPRQ_EN,
		MLX5_RX_MPRQ_LOG_STRIDE_NUM,
		MLX5_RX_MPRQ_MAX_MEMCPY_LEN,
		MLX5_RXQS_MIN_MPRQ,
		MLX5_TXQ_INLINE,
		MLX5_TXQS_MIN_INLINE,
		MLX5_TXQ_MPW_EN,
		MLX5_TXQ_MPW_HDR_DSEG_EN,
		MLX5_TXQ_MAX_INLINE_LEN,
		MLX5_TX_VEC_EN,
		MLX5_RX_VEC_EN,
		MLX5_L3_VXLAN_EN,
		MLX5_VF_NL_EN,
		NULL,
	};
	struct rte_kvargs *kvlist;
	int ret = 0;
	int i;

	if (devargs == NULL)
		return 0;
	/* Following UGLY cast is done to pass checkpatch. */
	kvlist = rte_kvargs_parse(devargs->args, params);
	if (kvlist == NULL)
		return 0;
	/* Process parameters. */
	for (i = 0; (params[i] != NULL); ++i) {
		if (rte_kvargs_count(kvlist, params[i])) {
			ret = rte_kvargs_process(kvlist, params[i],
						 mlx5_args_check, config);
			if (ret) {
				rte_errno = EINVAL;
				rte_kvargs_free(kvlist);
				return -rte_errno;
			}
		}
	}
	rte_kvargs_free(kvlist);
	return 0;
}

static struct rte_pci_driver mlx5_driver;

/*
 * Reserved UAR address space for TXQ UAR(hw doorbell) mapping, process
 * local resource used by both primary and secondary to avoid duplicate
 * reservation.
 * The space has to be available on both primary and secondary process,
 * TXQ UAR maps to this area using fixed mmap w/o double check.
 */
static void *uar_base;

static int
find_lower_va_bound(const struct rte_memseg_list *msl __rte_unused,
		const struct rte_memseg *ms, void *arg)
{
	void **addr = arg;

	if (*addr == NULL)
		*addr = ms->addr;
	else
		*addr = RTE_MIN(*addr, ms->addr);

	return 0;
}

/**
 * Reserve UAR address space for primary process.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_uar_init_primary(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	void *addr = (void *)0;

	if (uar_base) { /* UAR address space mapped. */
		priv->uar_base = uar_base;
		return 0;
	}
	/* find out lower bound of hugepage segments */
	rte_memseg_walk(find_lower_va_bound, &addr);

	/* keep distance to hugepages to minimize potential conflicts. */
	addr = RTE_PTR_SUB(addr, MLX5_UAR_OFFSET + MLX5_UAR_SIZE);
	/* anonymous mmap, no real memory consumption. */
	addr = mmap(addr, MLX5_UAR_SIZE,
		    PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		DRV_LOG(ERR,
			"port %u failed to reserve UAR address space, please"
			" adjust MLX5_UAR_SIZE or try --base-virtaddr",
			dev->data->port_id);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	/* Accept either same addr or a new addr returned from mmap if target
	 * range occupied.
	 */
	DRV_LOG(INFO, "port %u reserved UAR address space: %p",
		dev->data->port_id, addr);
	priv->uar_base = addr; /* for primary and secondary UAR re-mmap. */
	uar_base = addr; /* process local, don't reserve again. */
	return 0;
}

/**
 * Reserve UAR address space for secondary process, align with
 * primary process.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_uar_init_secondary(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	void *addr;

	assert(priv->uar_base);
	if (uar_base) { /* already reserved. */
		assert(uar_base == priv->uar_base);
		return 0;
	}
	/* anonymous mmap, no real memory consumption. */
	addr = mmap(priv->uar_base, MLX5_UAR_SIZE,
		    PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		DRV_LOG(ERR, "port %u UAR mmap failed: %p size: %llu",
			dev->data->port_id, priv->uar_base, MLX5_UAR_SIZE);
		rte_errno = ENXIO;
		return -rte_errno;
	}
	if (priv->uar_base != addr) {
		DRV_LOG(ERR,
			"port %u UAR address %p size %llu occupied, please"
			" adjust MLX5_UAR_OFFSET or try EAL parameter"
			" --base-virtaddr",
			dev->data->port_id, priv->uar_base, MLX5_UAR_SIZE);
		rte_errno = ENXIO;
		return -rte_errno;
	}
	uar_base = addr; /* process local, don't reserve again */
	DRV_LOG(INFO, "port %u reserved UAR address space: %p",
		dev->data->port_id, addr);
	return 0;
}

/**
 * DPDK callback to register a PCI device.
 *
 * This function creates an Ethernet device for each port of a given
 * PCI device.
 *
 * @param[in] pci_drv
 *   PCI driver structure (mlx5_driver).
 * @param[in] pci_dev
 *   PCI device information.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	       struct rte_pci_device *pci_dev)
{
	struct ibv_device **list = NULL;
	struct ibv_device *ibv_dev;
	int err = 0;
	struct ibv_context *attr_ctx = NULL;
	struct ibv_device_attr_ex device_attr;
	unsigned int vf = 0;
	unsigned int mps;
	unsigned int cqe_comp;
	unsigned int tunnel_en = 0;
	unsigned int mpls_en = 0;
	unsigned int swp = 0;
	unsigned int verb_priorities = 0;
	unsigned int mprq = 0;
	unsigned int mprq_min_stride_size_n = 0;
	unsigned int mprq_max_stride_size_n = 0;
	unsigned int mprq_min_stride_num_n = 0;
	unsigned int mprq_max_stride_num_n = 0;
	int idx;
	int i;
	struct mlx5dv_context attrs_out = {0};
#ifdef HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT
	struct ibv_counter_set_description cs_desc;
#endif

	/* Prepare shared data between primary and secondary process. */
	mlx5_prepare_shared_data();
	assert(pci_drv == &mlx5_driver);
	/* Get mlx5_dev[] index. */
	idx = mlx5_dev_idx(&pci_dev->addr);
	if (idx == -1) {
		DRV_LOG(ERR, "this driver cannot support any more adapters");
		err = ENOMEM;
		goto error;
	}
	DRV_LOG(DEBUG, "using driver device index %d", idx);
	/* Save PCI address. */
	mlx5_dev[idx].pci_addr = pci_dev->addr;
	list = mlx5_glue->get_device_list(&i);
	if (list == NULL) {
		assert(errno);
		err = errno;
		if (errno == ENOSYS)
			DRV_LOG(ERR,
				"cannot list devices, is ib_uverbs loaded?");
		goto error;
	}
	assert(i >= 0);
	/*
	 * For each listed device, check related sysfs entry against
	 * the provided PCI ID.
	 */
	while (i != 0) {
		struct rte_pci_addr pci_addr;

		--i;
		DRV_LOG(DEBUG, "checking device \"%s\"", list[i]->name);
		if (mlx5_ibv_device_to_pci_addr(list[i], &pci_addr))
			continue;
		if ((pci_dev->addr.domain != pci_addr.domain) ||
		    (pci_dev->addr.bus != pci_addr.bus) ||
		    (pci_dev->addr.devid != pci_addr.devid) ||
		    (pci_dev->addr.function != pci_addr.function))
			continue;
		DRV_LOG(INFO, "PCI information matches, using device \"%s\"",
			list[i]->name);
		vf = ((pci_dev->id.device_id ==
		       PCI_DEVICE_ID_MELLANOX_CONNECTX4VF) ||
		      (pci_dev->id.device_id ==
		       PCI_DEVICE_ID_MELLANOX_CONNECTX4LXVF) ||
		      (pci_dev->id.device_id ==
		       PCI_DEVICE_ID_MELLANOX_CONNECTX5VF) ||
		      (pci_dev->id.device_id ==
		       PCI_DEVICE_ID_MELLANOX_CONNECTX5EXVF));
		attr_ctx = mlx5_glue->open_device(list[i]);
		rte_errno = errno;
		err = rte_errno;
		break;
	}
	if (attr_ctx == NULL) {
		switch (err) {
		case 0:
			DRV_LOG(ERR,
				"cannot access device, is mlx5_ib loaded?");
			err = ENODEV;
			break;
		case EINVAL:
			DRV_LOG(ERR,
				"cannot use device, are drivers up to date?");
			break;
		}
		goto error;
	}
	ibv_dev = list[i];
	DRV_LOG(DEBUG, "device opened");
#ifdef HAVE_IBV_MLX5_MOD_SWP
	attrs_out.comp_mask |= MLX5DV_CONTEXT_MASK_SWP;
#endif
	/*
	 * Multi-packet send is supported by ConnectX-4 Lx PF as well
	 * as all ConnectX-5 devices.
	 */
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	attrs_out.comp_mask |= MLX5DV_CONTEXT_MASK_TUNNEL_OFFLOADS;
#endif
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
	attrs_out.comp_mask |= MLX5DV_CONTEXT_MASK_STRIDING_RQ;
#endif
	mlx5_glue->dv_query_device(attr_ctx, &attrs_out);
	if (attrs_out.flags & MLX5DV_CONTEXT_FLAGS_MPW_ALLOWED) {
		if (attrs_out.flags & MLX5DV_CONTEXT_FLAGS_ENHANCED_MPW) {
			DRV_LOG(DEBUG, "enhanced MPW is supported");
			mps = MLX5_MPW_ENHANCED;
		} else {
			DRV_LOG(DEBUG, "MPW is supported");
			mps = MLX5_MPW;
		}
	} else {
		DRV_LOG(DEBUG, "MPW isn't supported");
		mps = MLX5_MPW_DISABLED;
	}
#ifdef HAVE_IBV_MLX5_MOD_SWP
	if (attrs_out.comp_mask & MLX5DV_CONTEXT_MASK_SWP)
		swp = attrs_out.sw_parsing_caps.sw_parsing_offloads;
	DRV_LOG(DEBUG, "SWP support: %u", swp);
#endif
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
	if (attrs_out.comp_mask & MLX5DV_CONTEXT_MASK_STRIDING_RQ) {
		struct mlx5dv_striding_rq_caps mprq_caps =
			attrs_out.striding_rq_caps;

		DRV_LOG(DEBUG, "\tmin_single_stride_log_num_of_bytes: %d",
			mprq_caps.min_single_stride_log_num_of_bytes);
		DRV_LOG(DEBUG, "\tmax_single_stride_log_num_of_bytes: %d",
			mprq_caps.max_single_stride_log_num_of_bytes);
		DRV_LOG(DEBUG, "\tmin_single_wqe_log_num_of_strides: %d",
			mprq_caps.min_single_wqe_log_num_of_strides);
		DRV_LOG(DEBUG, "\tmax_single_wqe_log_num_of_strides: %d",
			mprq_caps.max_single_wqe_log_num_of_strides);
		DRV_LOG(DEBUG, "\tsupported_qpts: %d",
			mprq_caps.supported_qpts);
		DRV_LOG(DEBUG, "device supports Multi-Packet RQ");
		mprq = 1;
		mprq_min_stride_size_n =
			mprq_caps.min_single_stride_log_num_of_bytes;
		mprq_max_stride_size_n =
			mprq_caps.max_single_stride_log_num_of_bytes;
		mprq_min_stride_num_n =
			mprq_caps.min_single_wqe_log_num_of_strides;
		mprq_max_stride_num_n =
			mprq_caps.max_single_wqe_log_num_of_strides;
	}
#endif
	if (RTE_CACHE_LINE_SIZE == 128 &&
	    !(attrs_out.flags & MLX5DV_CONTEXT_FLAGS_CQE_128B_COMP))
		cqe_comp = 0;
	else
		cqe_comp = 1;
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	if (attrs_out.comp_mask & MLX5DV_CONTEXT_MASK_TUNNEL_OFFLOADS) {
		tunnel_en = ((attrs_out.tunnel_offloads_caps &
			      MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_VXLAN) &&
			     (attrs_out.tunnel_offloads_caps &
			      MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_GRE));
	}
	DRV_LOG(DEBUG, "tunnel offloading is %ssupported",
		tunnel_en ? "" : "not ");
#else
	DRV_LOG(WARNING,
		"tunnel offloading disabled due to old OFED/rdma-core version");
#endif
#ifdef HAVE_IBV_DEVICE_MPLS_SUPPORT
	mpls_en = ((attrs_out.tunnel_offloads_caps &
		    MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_CW_MPLS_OVER_GRE) &&
		   (attrs_out.tunnel_offloads_caps &
		    MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_CW_MPLS_OVER_UDP));
	DRV_LOG(DEBUG, "MPLS over GRE/UDP tunnel offloading is %ssupported",
		mpls_en ? "" : "not ");
#else
	DRV_LOG(WARNING, "MPLS over GRE/UDP tunnel offloading disabled due to"
		" old OFED/rdma-core version or firmware configuration");
#endif
	err = mlx5_glue->query_device_ex(attr_ctx, NULL, &device_attr);
	if (err) {
		DEBUG("ibv_query_device_ex() failed");
		goto error;
	}
	DRV_LOG(INFO, "%u port(s) detected",
		device_attr.orig_attr.phys_port_cnt);
	for (i = 0; i < device_attr.orig_attr.phys_port_cnt; i++) {
		char name[RTE_ETH_NAME_MAX_LEN];
		int len;
		uint32_t port = i + 1; /* ports are indexed from one */
		uint32_t test = (1 << i);
		struct ibv_context *ctx = NULL;
		struct ibv_port_attr port_attr;
		struct ibv_pd *pd = NULL;
		struct priv *priv = NULL;
		struct rte_eth_dev *eth_dev = NULL;
		struct ibv_device_attr_ex device_attr_ex;
		struct ether_addr mac;
		struct mlx5_dev_config config = {
			.cqe_comp = cqe_comp,
			.mps = mps,
			.tunnel_en = tunnel_en,
			.mpls_en = mpls_en,
			.tx_vec_en = 1,
			.rx_vec_en = 1,
			.mpw_hdr_dseg = 0,
			.txq_inline = MLX5_ARG_UNSET,
			.txqs_inline = MLX5_ARG_UNSET,
			.inline_max_packet_sz = MLX5_ARG_UNSET,
			.vf_nl_en = 1,
			.swp = !!swp,
			.mprq = {
				.enabled = 0, /* Disabled by default. */
				.stride_num_n = RTE_MAX(MLX5_MPRQ_STRIDE_NUM_N,
							mprq_min_stride_num_n),
				.max_memcpy_len = MLX5_MPRQ_MEMCPY_DEFAULT_LEN,
				.min_rxqs_num = MLX5_MPRQ_MIN_RXQS,
			},
		};

		len = snprintf(name, sizeof(name), PCI_PRI_FMT,
			 pci_dev->addr.domain, pci_dev->addr.bus,
			 pci_dev->addr.devid, pci_dev->addr.function);
		if (device_attr.orig_attr.phys_port_cnt > 1)
			snprintf(name + len, sizeof(name), " port %u", i);
		mlx5_dev[idx].ports |= test;
		if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
			eth_dev = rte_eth_dev_attach_secondary(name);
			if (eth_dev == NULL) {
				DRV_LOG(ERR, "can not attach rte ethdev");
				rte_errno = ENOMEM;
				err = rte_errno;
				goto error;
			}
			eth_dev->device = &pci_dev->device;
			eth_dev->dev_ops = &mlx5_dev_sec_ops;
			err = mlx5_uar_init_secondary(eth_dev);
			if (err) {
				err = rte_errno;
				goto error;
			}
			/* Receive command fd from primary process */
			err = mlx5_socket_connect(eth_dev);
			if (err < 0) {
				err = rte_errno;
				goto error;
			}
			/* Remap UAR for Tx queues. */
			err = mlx5_tx_uar_remap(eth_dev, err);
			if (err) {
				err = rte_errno;
				goto error;
			}
			/*
			 * Ethdev pointer is still required as input since
			 * the primary device is not accessible from the
			 * secondary process.
			 */
			eth_dev->rx_pkt_burst =
				mlx5_select_rx_function(eth_dev);
			eth_dev->tx_pkt_burst =
				mlx5_select_tx_function(eth_dev);
			rte_eth_dev_probing_finish(eth_dev);
			continue;
		}
		DRV_LOG(DEBUG, "using port %u (%08" PRIx32 ")", port, test);
		ctx = mlx5_glue->open_device(ibv_dev);
		if (ctx == NULL) {
			err = ENODEV;
			goto port_error;
		}
		/* Check port status. */
		err = mlx5_glue->query_port(ctx, port, &port_attr);
		if (err) {
			DRV_LOG(ERR, "port query failed: %s", strerror(err));
			goto port_error;
		}
		if (port_attr.link_layer != IBV_LINK_LAYER_ETHERNET) {
			DRV_LOG(ERR,
				"port %d is not configured in Ethernet mode",
				port);
			err = EINVAL;
			goto port_error;
		}
		if (port_attr.state != IBV_PORT_ACTIVE)
			DRV_LOG(DEBUG, "port %d is not active: \"%s\" (%d)",
				port,
				mlx5_glue->port_state_str(port_attr.state),
				port_attr.state);
		/* Allocate protection domain. */
		pd = mlx5_glue->alloc_pd(ctx);
		if (pd == NULL) {
			DRV_LOG(ERR, "PD allocation failure");
			err = ENOMEM;
			goto port_error;
		}
		mlx5_dev[idx].ports |= test;
		/* from rte_ethdev.c */
		priv = rte_zmalloc("ethdev private structure",
				   sizeof(*priv),
				   RTE_CACHE_LINE_SIZE);
		if (priv == NULL) {
			DRV_LOG(ERR, "priv allocation failure");
			err = ENOMEM;
			goto port_error;
		}
		priv->ctx = ctx;
		strncpy(priv->ibdev_path, priv->ctx->device->ibdev_path,
			sizeof(priv->ibdev_path));
		priv->device_attr = device_attr;
		priv->port = port;
		priv->pd = pd;
		priv->mtu = ETHER_MTU;
		err = mlx5_args(&config, pci_dev->device.devargs);
		if (err) {
			DRV_LOG(ERR, "failed to process device arguments: %s",
				strerror(err));
			err = rte_errno;
			goto port_error;
		}
		err = mlx5_glue->query_device_ex(ctx, NULL, &device_attr_ex);
		if (err) {
			DRV_LOG(ERR, "ibv_query_device_ex() failed");
			goto port_error;
		}
		config.hw_csum = !!(device_attr_ex.device_cap_flags_ex &
				    IBV_DEVICE_RAW_IP_CSUM);
		DRV_LOG(DEBUG, "checksum offloading is %ssupported",
			(config.hw_csum ? "" : "not "));
#ifdef HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT
		config.flow_counter_en = !!(device_attr.max_counter_sets);
		mlx5_glue->describe_counter_set(ctx, 0, &cs_desc);
		DRV_LOG(DEBUG,
			"counter type = %d, num of cs = %ld, attributes = %d",
			cs_desc.counter_type, cs_desc.num_of_cs,
			cs_desc.attributes);
#endif
		config.ind_table_max_size =
			device_attr_ex.rss_caps.max_rwq_indirection_table_size;
		/* Remove this check once DPDK supports larger/variable
		 * indirection tables. */
		if (config.ind_table_max_size >
				(unsigned int)ETH_RSS_RETA_SIZE_512)
			config.ind_table_max_size = ETH_RSS_RETA_SIZE_512;
		DRV_LOG(DEBUG, "maximum Rx indirection table size is %u",
			config.ind_table_max_size);
		config.hw_vlan_strip = !!(device_attr_ex.raw_packet_caps &
					 IBV_RAW_PACKET_CAP_CVLAN_STRIPPING);
		DRV_LOG(DEBUG, "VLAN stripping is %ssupported",
			(config.hw_vlan_strip ? "" : "not "));

		config.hw_fcs_strip = !!(device_attr_ex.raw_packet_caps &
					 IBV_RAW_PACKET_CAP_SCATTER_FCS);
		DRV_LOG(DEBUG, "FCS stripping configuration is %ssupported",
			(config.hw_fcs_strip ? "" : "not "));

#ifdef HAVE_IBV_WQ_FLAG_RX_END_PADDING
		config.hw_padding = !!device_attr_ex.rx_pad_end_addr_align;
#endif
		DRV_LOG(DEBUG,
			"hardware Rx end alignment padding is %ssupported",
			(config.hw_padding ? "" : "not "));
		config.vf = vf;
		config.tso = ((device_attr_ex.tso_caps.max_tso > 0) &&
			      (device_attr_ex.tso_caps.supported_qpts &
			      (1 << IBV_QPT_RAW_PACKET)));
		if (config.tso)
			config.tso_max_payload_sz =
					device_attr_ex.tso_caps.max_tso;
		if (config.mps && !mps) {
			DRV_LOG(ERR,
				"multi-packet send not supported on this device"
				" (" MLX5_TXQ_MPW_EN ")");
			err = ENOTSUP;
			goto port_error;
		}
		DRV_LOG(INFO, "%s MPS is %s",
			config.mps == MLX5_MPW_ENHANCED ? "enhanced " : "",
			config.mps != MLX5_MPW_DISABLED ? "enabled" :
			"disabled");
		if (config.cqe_comp && !cqe_comp) {
			DRV_LOG(WARNING, "Rx CQE compression isn't supported");
			config.cqe_comp = 0;
		}
		config.mprq.enabled = config.mprq.enabled && mprq;
		if (config.mprq.enabled) {
			if (config.mprq.stride_num_n > mprq_max_stride_num_n ||
			    config.mprq.stride_num_n < mprq_min_stride_num_n) {
				config.mprq.stride_num_n =
					RTE_MAX(MLX5_MPRQ_STRIDE_NUM_N,
						mprq_min_stride_num_n);
				DRV_LOG(WARNING,
					"the number of strides"
					" for Multi-Packet RQ is out of range,"
					" setting default value (%u)",
					1 << config.mprq.stride_num_n);
			}
			config.mprq.min_stride_size_n = mprq_min_stride_size_n;
			config.mprq.max_stride_size_n = mprq_max_stride_size_n;
		}
		eth_dev = rte_eth_dev_allocate(name);
		if (eth_dev == NULL) {
			DRV_LOG(ERR, "can not allocate rte ethdev");
			err = ENOMEM;
			goto port_error;
		}
		eth_dev->data->dev_private = priv;
		priv->dev_data = eth_dev->data;
		eth_dev->data->mac_addrs = priv->mac;
		eth_dev->device = &pci_dev->device;
		rte_eth_copy_pci_info(eth_dev, pci_dev);
		eth_dev->device->driver = &mlx5_driver.driver;
		err = mlx5_uar_init_primary(eth_dev);
		if (err) {
			err = rte_errno;
			goto port_error;
		}
		/* Configure the first MAC address by default. */
		if (mlx5_get_mac(eth_dev, &mac.addr_bytes)) {
			DRV_LOG(ERR,
				"port %u cannot get MAC address, is mlx5_en"
				" loaded? (errno: %s)",
				eth_dev->data->port_id, strerror(errno));
			err = ENODEV;
			goto port_error;
		}
		DRV_LOG(INFO,
			"port %u MAC address is %02x:%02x:%02x:%02x:%02x:%02x",
			eth_dev->data->port_id,
			mac.addr_bytes[0], mac.addr_bytes[1],
			mac.addr_bytes[2], mac.addr_bytes[3],
			mac.addr_bytes[4], mac.addr_bytes[5]);
#ifndef NDEBUG
		{
			char ifname[IF_NAMESIZE];

			if (mlx5_get_ifname(eth_dev, &ifname) == 0)
				DRV_LOG(DEBUG, "port %u ifname is \"%s\"",
					eth_dev->data->port_id, ifname);
			else
				DRV_LOG(DEBUG, "port %u ifname is unknown",
					eth_dev->data->port_id);
		}
#endif
		/* Get actual MTU if possible. */
		err = mlx5_get_mtu(eth_dev, &priv->mtu);
		if (err) {
			err = rte_errno;
			goto port_error;
		}
		DRV_LOG(DEBUG, "port %u MTU is %u", eth_dev->data->port_id,
			priv->mtu);
		/*
		 * Initialize burst functions to prevent crashes before link-up.
		 */
		eth_dev->rx_pkt_burst = removed_rx_burst;
		eth_dev->tx_pkt_burst = removed_tx_burst;
		eth_dev->dev_ops = &mlx5_dev_ops;
		/* Register MAC address. */
		claim_zero(mlx5_mac_addr_add(eth_dev, &mac, 0, 0));
		priv->nl_socket = -1;
		priv->nl_sn = 0;
		if (vf && config.vf_nl_en) {
			priv->nl_socket = mlx5_nl_init(RTMGRP_LINK);
			if (priv->nl_socket < 0)
				priv->nl_socket = -1;
			mlx5_nl_mac_addr_sync(eth_dev);
		}
		TAILQ_INIT(&priv->flows);
		TAILQ_INIT(&priv->ctrl_flows);
		/* Hint libmlx5 to use PMD allocator for data plane resources */
		struct mlx5dv_ctx_allocators alctr = {
			.alloc = &mlx5_alloc_verbs_buf,
			.free = &mlx5_free_verbs_buf,
			.data = priv,
		};
		mlx5_glue->dv_set_context_attr(ctx,
					       MLX5DV_CTX_ATTR_BUF_ALLOCATORS,
					       (void *)((uintptr_t)&alctr));
		/* Bring Ethernet device up. */
		DRV_LOG(DEBUG, "port %u forcing Ethernet interface up",
			eth_dev->data->port_id);
		mlx5_set_link_up(eth_dev);
		/*
		 * Even though the interrupt handler is not installed yet,
		 * interrupts will still trigger on the asyn_fd from
		 * Verbs context returned by ibv_open_device().
		 */
		mlx5_link_update(eth_dev, 0);
		/* Store device configuration on private structure. */
		priv->config = config;
		/* Create drop queue. */
		err = mlx5_flow_create_drop_queue(eth_dev);
		if (err) {
			DRV_LOG(ERR, "port %u drop queue allocation failed: %s",
				eth_dev->data->port_id, strerror(rte_errno));
			err = rte_errno;
			goto port_error;
		}
		/* Supported Verbs flow priority number detection. */
		if (verb_priorities == 0)
			verb_priorities = mlx5_get_max_verbs_prio(eth_dev);
		if (verb_priorities < MLX5_VERBS_FLOW_PRIO_8) {
			DRV_LOG(ERR, "port %u wrong Verbs flow priorities: %u",
				eth_dev->data->port_id, verb_priorities);
			goto port_error;
		}
		priv->config.max_verbs_prio = verb_priorities;
		/*
		 * Once the device is added to the list of memory event
		 * callback, its global MR cache table cannot be expanded
		 * on the fly because of deadlock. If it overflows, lookup
		 * should be done by searching MR list linearly, which is slow.
		 */
		err = mlx5_mr_btree_init(&priv->mr.cache,
					 MLX5_MR_BTREE_CACHE_N * 2,
					 eth_dev->device->numa_node);
		if (err) {
			err = rte_errno;
			goto port_error;
		}
		/* Add device to memory callback list. */
		rte_rwlock_write_lock(&mlx5_shared_data->mem_event_rwlock);
		LIST_INSERT_HEAD(&mlx5_shared_data->mem_event_cb_list,
				 priv, mem_event_cb);
		rte_rwlock_write_unlock(&mlx5_shared_data->mem_event_rwlock);
		rte_eth_dev_probing_finish(eth_dev);
		continue;
port_error:
		if (priv)
			rte_free(priv);
		if (pd)
			claim_zero(mlx5_glue->dealloc_pd(pd));
		if (ctx)
			claim_zero(mlx5_glue->close_device(ctx));
		if (eth_dev && rte_eal_process_type() == RTE_PROC_PRIMARY)
			rte_eth_dev_release_port(eth_dev);
		break;
	}
	/*
	 * XXX if something went wrong in the loop above, there is a resource
	 * leak (ctx, pd, priv, dpdk ethdev) but we can do nothing about it as
	 * long as the dpdk does not provide a way to deallocate a ethdev and a
	 * way to enumerate the registered ethdevs to free the previous ones.
	 */
	/* no port found, complain */
	if (!mlx5_dev[idx].ports) {
		rte_errno = ENODEV;
		err = rte_errno;
	}
error:
	if (attr_ctx)
		claim_zero(mlx5_glue->close_device(attr_ctx));
	if (list)
		mlx5_glue->free_device_list(list);
	if (err) {
		rte_errno = err;
		return -rte_errno;
	}
	return 0;
}

static const struct rte_pci_id mlx5_pci_id_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX4)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX4VF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX4LX)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX4LXVF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5VF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5EX)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5EXVF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5BF)
	},
	{
		.vendor_id = 0
	}
};

static struct rte_pci_driver mlx5_driver = {
	.driver = {
		.name = MLX5_DRIVER_NAME
	},
	.id_table = mlx5_pci_id_map,
	.probe = mlx5_pci_probe,
	.drv_flags = RTE_PCI_DRV_INTR_LSC | RTE_PCI_DRV_INTR_RMV,
};

#ifdef RTE_LIBRTE_MLX5_DLOPEN_DEPS

/**
 * Suffix RTE_EAL_PMD_PATH with "-glue".
 *
 * This function performs a sanity check on RTE_EAL_PMD_PATH before
 * suffixing its last component.
 *
 * @param buf[out]
 *   Output buffer, should be large enough otherwise NULL is returned.
 * @param size
 *   Size of @p out.
 *
 * @return
 *   Pointer to @p buf or @p NULL in case suffix cannot be appended.
 */
static char *
mlx5_glue_path(char *buf, size_t size)
{
	static const char *const bad[] = { "/", ".", "..", NULL };
	const char *path = RTE_EAL_PMD_PATH;
	size_t len = strlen(path);
	size_t off;
	int i;

	while (len && path[len - 1] == '/')
		--len;
	for (off = len; off && path[off - 1] != '/'; --off)
		;
	for (i = 0; bad[i]; ++i)
		if (!strncmp(path + off, bad[i], (int)(len - off)))
			goto error;
	i = snprintf(buf, size, "%.*s-glue", (int)len, path);
	if (i == -1 || (size_t)i >= size)
		goto error;
	return buf;
error:
	DRV_LOG(ERR,
		"unable to append \"-glue\" to last component of"
		" RTE_EAL_PMD_PATH (\"" RTE_EAL_PMD_PATH "\"),"
		" please re-configure DPDK");
	return NULL;
}

/**
 * Initialization routine for run-time dependency on rdma-core.
 */
static int
mlx5_glue_init(void)
{
	char glue_path[sizeof(RTE_EAL_PMD_PATH) - 1 + sizeof("-glue")];
	const char *path[] = {
		/*
		 * A basic security check is necessary before trusting
		 * MLX5_GLUE_PATH, which may override RTE_EAL_PMD_PATH.
		 */
		(geteuid() == getuid() && getegid() == getgid() ?
		 getenv("MLX5_GLUE_PATH") : NULL),
		/*
		 * When RTE_EAL_PMD_PATH is set, use its glue-suffixed
		 * variant, otherwise let dlopen() look up libraries on its
		 * own.
		 */
		(*RTE_EAL_PMD_PATH ?
		 mlx5_glue_path(glue_path, sizeof(glue_path)) : ""),
	};
	unsigned int i = 0;
	void *handle = NULL;
	void **sym;
	const char *dlmsg;

	while (!handle && i != RTE_DIM(path)) {
		const char *end;
		size_t len;
		int ret;

		if (!path[i]) {
			++i;
			continue;
		}
		end = strpbrk(path[i], ":;");
		if (!end)
			end = path[i] + strlen(path[i]);
		len = end - path[i];
		ret = 0;
		do {
			char name[ret + 1];

			ret = snprintf(name, sizeof(name), "%.*s%s" MLX5_GLUE,
				       (int)len, path[i],
				       (!len || *(end - 1) == '/') ? "" : "/");
			if (ret == -1)
				break;
			if (sizeof(name) != (size_t)ret + 1)
				continue;
			DRV_LOG(DEBUG, "looking for rdma-core glue as \"%s\"",
				name);
			handle = dlopen(name, RTLD_LAZY);
			break;
		} while (1);
		path[i] = end + 1;
		if (!*end)
			++i;
	}
	if (!handle) {
		rte_errno = EINVAL;
		dlmsg = dlerror();
		if (dlmsg)
			DRV_LOG(WARNING, "cannot load glue library: %s", dlmsg);
		goto glue_error;
	}
	sym = dlsym(handle, "mlx5_glue");
	if (!sym || !*sym) {
		rte_errno = EINVAL;
		dlmsg = dlerror();
		if (dlmsg)
			DRV_LOG(ERR, "cannot resolve glue symbol: %s", dlmsg);
		goto glue_error;
	}
	mlx5_glue = *sym;
	return 0;
glue_error:
	if (handle)
		dlclose(handle);
	DRV_LOG(WARNING,
		"cannot initialize PMD due to missing run-time dependency on"
		" rdma-core libraries (libibverbs, libmlx5)");
	return -rte_errno;
}

#endif

/**
 * Driver initialization routine.
 */
RTE_INIT(rte_mlx5_pmd_init);
static void
rte_mlx5_pmd_init(void)
{
	/* Build the static tables for Verbs conversion. */
	mlx5_set_ptype_table();
	mlx5_set_cksum_table();
	mlx5_set_swp_types_table();
	/*
	 * RDMAV_HUGEPAGES_SAFE tells ibv_fork_init() we intend to use
	 * huge pages. Calling ibv_fork_init() during init allows
	 * applications to use fork() safely for purposes other than
	 * using this PMD, which is not supported in forked processes.
	 */
	setenv("RDMAV_HUGEPAGES_SAFE", "1", 1);
	/* Match the size of Rx completion entry to the size of a cacheline. */
	if (RTE_CACHE_LINE_SIZE == 128)
		setenv("MLX5_CQE_SIZE", "128", 0);
#ifdef RTE_LIBRTE_MLX5_DLOPEN_DEPS
	if (mlx5_glue_init())
		return;
	assert(mlx5_glue);
#endif
#ifndef NDEBUG
	/* Glue structure must not contain any NULL pointers. */
	{
		unsigned int i;

		for (i = 0; i != sizeof(*mlx5_glue) / sizeof(void *); ++i)
			assert(((const void *const *)mlx5_glue)[i]);
	}
#endif
	if (strcmp(mlx5_glue->version, MLX5_GLUE_VERSION)) {
		DRV_LOG(ERR,
			"rdma-core glue \"%s\" mismatch: \"%s\" is required",
			mlx5_glue->version, MLX5_GLUE_VERSION);
		return;
	}
	mlx5_glue->fork_init();
	rte_pci_register(&mlx5_driver);
}

RTE_PMD_EXPORT_NAME(net_mlx5, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(net_mlx5, mlx5_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(net_mlx5, "* ib_uverbs & mlx5_core & mlx5_ib");

/** Initialize driver log type. */
RTE_INIT(vdev_netvsc_init_log)
{
	mlx5_logtype = rte_log_register("pmd.net.mlx5");
	if (mlx5_logtype >= 0)
		rte_log_set_level(mlx5_logtype, RTE_LOG_NOTICE);
}
