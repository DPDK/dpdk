/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <net/if.h>
#include <linux/rtnetlink.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <fcntl.h>

#include <rte_malloc.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_kvargs.h>
#include <rte_rwlock.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>
#include <rte_alarm.h>
#include <rte_eal_paging.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_common.h>
#include <mlx5_common_mp.h>
#include <mlx5_common_mr.h>
#include <mlx5_malloc.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_common_os.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_mr.h"
#include "mlx5_flow.h"
#include "rte_pmd_mlx5.h"
#include "mlx5_verbs.h"

#define MLX5_TAGS_HLIST_ARRAY_SIZE 8192

#ifndef HAVE_IBV_MLX5_MOD_MPW
#define MLX5DV_CONTEXT_FLAGS_MPW_ALLOWED (1 << 2)
#define MLX5DV_CONTEXT_FLAGS_ENHANCED_MPW (1 << 3)
#endif

#ifndef HAVE_IBV_MLX5_MOD_CQE_128B_COMP
#define MLX5DV_CONTEXT_FLAGS_CQE_128B_COMP (1 << 4)
#endif

static const char *MZ_MLX5_PMD_SHARED_DATA = "mlx5_pmd_shared_data";

/* Spinlock for mlx5_shared_data allocation. */
static rte_spinlock_t mlx5_shared_data_lock = RTE_SPINLOCK_INITIALIZER;

/* Process local data for secondary processes. */
static struct mlx5_local_data mlx5_local_data;

/**
 * Set the completion channel file descriptor interrupt as non-blocking.
 *
 * @param[in] rxq_obj
 *   Pointer to RQ channel object, which includes the channel fd
 *
 * @param[out] fd
 *   The file descriptor (representing the intetrrupt) used in this channel.
 *
 * @return
 *   0 on successfully setting the fd to non-blocking, non-zero otherwise.
 */
int
mlx5_os_set_nonblock_channel_fd(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * Get mlx5 device attributes. The glue function query_device_ex() is called
 * with out parameter of type 'struct ibv_device_attr_ex *'. Then fill in mlx5
 * device attributes from the glue out parameter.
 *
 * @param dev
 *   Pointer to ibv context.
 *
 * @param device_attr
 *   Pointer to mlx5 device attributes.
 *
 * @return
 *   0 on success, non zero error number otherwise
 */
int
mlx5_os_get_dev_attr(void *ctx, struct mlx5_dev_attr *device_attr)
{
	int err;
	struct ibv_device_attr_ex attr_ex;
	memset(device_attr, 0, sizeof(*device_attr));
	err = mlx5_glue->query_device_ex(ctx, NULL, &attr_ex);
	if (err)
		return err;

	device_attr->device_cap_flags_ex = attr_ex.device_cap_flags_ex;
	device_attr->max_qp_wr = attr_ex.orig_attr.max_qp_wr;
	device_attr->max_sge = attr_ex.orig_attr.max_sge;
	device_attr->max_cq = attr_ex.orig_attr.max_cq;
	device_attr->max_qp = attr_ex.orig_attr.max_qp;
	device_attr->raw_packet_caps = attr_ex.raw_packet_caps;
	device_attr->max_rwq_indirection_table_size =
		attr_ex.rss_caps.max_rwq_indirection_table_size;
	device_attr->max_tso = attr_ex.tso_caps.max_tso;
	device_attr->tso_supported_qpts = attr_ex.tso_caps.supported_qpts;

	struct mlx5dv_context dv_attr = { .comp_mask = 0 };
	err = mlx5_glue->dv_query_device(ctx, &dv_attr);
	if (err)
		return err;

	device_attr->flags = dv_attr.flags;
	device_attr->comp_mask = dv_attr.comp_mask;
#ifdef HAVE_IBV_MLX5_MOD_SWP
	device_attr->sw_parsing_offloads =
		dv_attr.sw_parsing_caps.sw_parsing_offloads;
#endif
	device_attr->min_single_stride_log_num_of_bytes =
		dv_attr.striding_rq_caps.min_single_stride_log_num_of_bytes;
	device_attr->max_single_stride_log_num_of_bytes =
		dv_attr.striding_rq_caps.max_single_stride_log_num_of_bytes;
	device_attr->min_single_wqe_log_num_of_strides =
		dv_attr.striding_rq_caps.min_single_wqe_log_num_of_strides;
	device_attr->max_single_wqe_log_num_of_strides =
		dv_attr.striding_rq_caps.max_single_wqe_log_num_of_strides;
	device_attr->stride_supported_qpts =
		dv_attr.striding_rq_caps.supported_qpts;
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	device_attr->tunnel_offloads_caps = dv_attr.tunnel_offloads_caps;
#endif

	return err;
}

/**
 * Verbs callback to allocate a memory. This function should allocate the space
 * according to the size provided residing inside a huge page.
 * Please note that all allocation must respect the alignment from libmlx5
 * (i.e. currently rte_mem_page_size()).
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
	struct mlx5_priv *priv = data;
	void *ret;
	unsigned int socket = SOCKET_ID_ANY;
	size_t alignment = rte_mem_page_size();
	if (alignment == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get mem page size");
		rte_errno = ENOMEM;
		return NULL;
	}

	if (priv->verbs_alloc_ctx.type == MLX5_VERBS_ALLOC_TYPE_TX_QUEUE) {
		const struct mlx5_txq_ctrl *ctrl = priv->verbs_alloc_ctx.obj;

		socket = ctrl->socket;
	} else if (priv->verbs_alloc_ctx.type ==
		   MLX5_VERBS_ALLOC_TYPE_RX_QUEUE) {
		const struct mlx5_rxq_ctrl *ctrl = priv->verbs_alloc_ctx.obj;

		socket = ctrl->socket;
	}
	MLX5_ASSERT(data != NULL);
	ret = mlx5_malloc(0, size, alignment, socket);
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
	MLX5_ASSERT(data != NULL);
	mlx5_free(ptr);
}

/**
 * Initialize DR related data within private structure.
 * Routine checks the reference counter and does actual
 * resources creation/initialization only if counter is zero.
 *
 * @param[in] priv
 *   Pointer to the private device data structure.
 *
 * @return
 *   Zero on success, positive error code otherwise.
 */
static int
mlx5_alloc_shared_dr(struct mlx5_priv *priv)
{
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	char s[MLX5_HLIST_NAMESIZE];
	int err = 0;

	if (!sh->flow_tbls)
		err = mlx5_alloc_table_hash_list(priv);
	else
		DRV_LOG(DEBUG, "sh->flow_tbls[%p] already created, reuse\n",
			(void *)sh->flow_tbls);
	if (err)
		return err;
	/* Create tags hash list table. */
	snprintf(s, sizeof(s), "%s_tags", sh->ibdev_name);
	sh->tag_table = mlx5_hlist_create(s, MLX5_TAGS_HLIST_ARRAY_SIZE);
	if (!sh->tag_table) {
		DRV_LOG(ERR, "tags with hash creation failed.");
		err = ENOMEM;
		goto error;
	}
#ifdef HAVE_MLX5DV_DR
	void *domain;

	if (sh->dv_refcnt) {
		/* Shared DV/DR structures is already initialized. */
		sh->dv_refcnt++;
		priv->dr_shared = 1;
		return 0;
	}
	/* Reference counter is zero, we should initialize structures. */
	domain = mlx5_glue->dr_create_domain(sh->ctx,
					     MLX5DV_DR_DOMAIN_TYPE_NIC_RX);
	if (!domain) {
		DRV_LOG(ERR, "ingress mlx5dv_dr_create_domain failed");
		err = errno;
		goto error;
	}
	sh->rx_domain = domain;
	domain = mlx5_glue->dr_create_domain(sh->ctx,
					     MLX5DV_DR_DOMAIN_TYPE_NIC_TX);
	if (!domain) {
		DRV_LOG(ERR, "egress mlx5dv_dr_create_domain failed");
		err = errno;
		goto error;
	}
	pthread_mutex_init(&sh->dv_mutex, NULL);
	sh->tx_domain = domain;
#ifdef HAVE_MLX5DV_DR_ESWITCH
	if (priv->config.dv_esw_en) {
		domain  = mlx5_glue->dr_create_domain
			(sh->ctx, MLX5DV_DR_DOMAIN_TYPE_FDB);
		if (!domain) {
			DRV_LOG(ERR, "FDB mlx5dv_dr_create_domain failed");
			err = errno;
			goto error;
		}
		sh->fdb_domain = domain;
		sh->esw_drop_action = mlx5_glue->dr_create_flow_action_drop();
	}
#endif
	if (priv->config.reclaim_mode == MLX5_RCM_AGGR) {
		mlx5_glue->dr_reclaim_domain_memory(sh->rx_domain, 1);
		mlx5_glue->dr_reclaim_domain_memory(sh->tx_domain, 1);
		if (sh->fdb_domain)
			mlx5_glue->dr_reclaim_domain_memory(sh->fdb_domain, 1);
	}
	sh->pop_vlan_action = mlx5_glue->dr_create_flow_action_pop_vlan();
#endif /* HAVE_MLX5DV_DR */
	sh->dv_refcnt++;
	priv->dr_shared = 1;
	return 0;
error:
	/* Rollback the created objects. */
	if (sh->rx_domain) {
		mlx5_glue->dr_destroy_domain(sh->rx_domain);
		sh->rx_domain = NULL;
	}
	if (sh->tx_domain) {
		mlx5_glue->dr_destroy_domain(sh->tx_domain);
		sh->tx_domain = NULL;
	}
	if (sh->fdb_domain) {
		mlx5_glue->dr_destroy_domain(sh->fdb_domain);
		sh->fdb_domain = NULL;
	}
	if (sh->esw_drop_action) {
		mlx5_glue->destroy_flow_action(sh->esw_drop_action);
		sh->esw_drop_action = NULL;
	}
	if (sh->pop_vlan_action) {
		mlx5_glue->destroy_flow_action(sh->pop_vlan_action);
		sh->pop_vlan_action = NULL;
	}
	if (sh->tag_table) {
		/* tags should be destroyed with flow before. */
		mlx5_hlist_destroy(sh->tag_table, NULL, NULL);
		sh->tag_table = NULL;
	}
	mlx5_free_table_hash_list(priv);
	return err;
}

/**
 * Destroy DR related data within private structure.
 *
 * @param[in] priv
 *   Pointer to the private device data structure.
 */
void
mlx5_os_free_shared_dr(struct mlx5_priv *priv)
{
	struct mlx5_dev_ctx_shared *sh;

	if (!priv->dr_shared)
		return;
	priv->dr_shared = 0;
	sh = priv->sh;
	MLX5_ASSERT(sh);
#ifdef HAVE_MLX5DV_DR
	MLX5_ASSERT(sh->dv_refcnt);
	if (sh->dv_refcnt && --sh->dv_refcnt)
		return;
	if (sh->rx_domain) {
		mlx5_glue->dr_destroy_domain(sh->rx_domain);
		sh->rx_domain = NULL;
	}
	if (sh->tx_domain) {
		mlx5_glue->dr_destroy_domain(sh->tx_domain);
		sh->tx_domain = NULL;
	}
#ifdef HAVE_MLX5DV_DR_ESWITCH
	if (sh->fdb_domain) {
		mlx5_glue->dr_destroy_domain(sh->fdb_domain);
		sh->fdb_domain = NULL;
	}
	if (sh->esw_drop_action) {
		mlx5_glue->destroy_flow_action(sh->esw_drop_action);
		sh->esw_drop_action = NULL;
	}
#endif
	if (sh->pop_vlan_action) {
		mlx5_glue->destroy_flow_action(sh->pop_vlan_action);
		sh->pop_vlan_action = NULL;
	}
	pthread_mutex_destroy(&sh->dv_mutex);
#endif /* HAVE_MLX5DV_DR */
	if (sh->tag_table) {
		/* tags should be destroyed with flow before. */
		mlx5_hlist_destroy(sh->tag_table, NULL, NULL);
		sh->tag_table = NULL;
	}
	mlx5_free_table_hash_list(priv);
}

/**
 * Initialize shared data between primary and secondary process.
 *
 * A memzone is reserved by primary process and secondary processes attach to
 * the memzone.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_init_shared_data(void)
{
	const struct rte_memzone *mz;
	int ret = 0;

	rte_spinlock_lock(&mlx5_shared_data_lock);
	if (mlx5_shared_data == NULL) {
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			/* Allocate shared memory. */
			mz = rte_memzone_reserve(MZ_MLX5_PMD_SHARED_DATA,
						 sizeof(*mlx5_shared_data),
						 SOCKET_ID_ANY, 0);
			if (mz == NULL) {
				DRV_LOG(ERR,
					"Cannot allocate mlx5 shared data");
				ret = -rte_errno;
				goto error;
			}
			mlx5_shared_data = mz->addr;
			memset(mlx5_shared_data, 0, sizeof(*mlx5_shared_data));
			rte_spinlock_init(&mlx5_shared_data->lock);
		} else {
			/* Lookup allocated shared memory. */
			mz = rte_memzone_lookup(MZ_MLX5_PMD_SHARED_DATA);
			if (mz == NULL) {
				DRV_LOG(ERR,
					"Cannot attach mlx5 shared data");
				ret = -rte_errno;
				goto error;
			}
			mlx5_shared_data = mz->addr;
			memset(&mlx5_local_data, 0, sizeof(mlx5_local_data));
		}
	}
error:
	rte_spinlock_unlock(&mlx5_shared_data_lock);
	return ret;
}

/**
 * PMD global initialization.
 *
 * Independent from individual device, this function initializes global
 * per-PMD data structures distinguishing primary and secondary processes.
 * Hence, each initialization is called once per a process.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_init_once(void)
{
	struct mlx5_shared_data *sd;
	struct mlx5_local_data *ld = &mlx5_local_data;
	int ret = 0;

	if (mlx5_init_shared_data())
		return -rte_errno;
	sd = mlx5_shared_data;
	MLX5_ASSERT(sd);
	rte_spinlock_lock(&sd->lock);
	switch (rte_eal_process_type()) {
	case RTE_PROC_PRIMARY:
		if (sd->init_done)
			break;
		LIST_INIT(&sd->mem_event_cb_list);
		rte_rwlock_init(&sd->mem_event_rwlock);
		rte_mem_event_callback_register("MLX5_MEM_EVENT_CB",
						mlx5_mr_mem_event_cb, NULL);
		ret = mlx5_mp_init_primary(MLX5_MP_NAME,
					   mlx5_mp_os_primary_handle);
		if (ret)
			goto out;
		sd->init_done = true;
		break;
	case RTE_PROC_SECONDARY:
		if (ld->init_done)
			break;
		ret = mlx5_mp_init_secondary(MLX5_MP_NAME,
					     mlx5_mp_os_secondary_handle);
		if (ret)
			goto out;
		++sd->secondary_cnt;
		ld->init_done = true;
		break;
	default:
		break;
	}
out:
	rte_spinlock_unlock(&sd->lock);
	return ret;
}

/**
 * Spawn an Ethernet device from Verbs information.
 *
 * @param dpdk_dev
 *   Backing DPDK device.
 * @param spawn
 *   Verbs device parameters (name, port, switch_info) to spawn.
 * @param config
 *   Device configuration parameters.
 *
 * @return
 *   A valid Ethernet device object on success, NULL otherwise and rte_errno
 *   is set. The following errors are defined:
 *
 *   EBUSY: device is not supposed to be spawned.
 *   EEXIST: device is already spawned
 */
static struct rte_eth_dev *
mlx5_dev_spawn(struct rte_device *dpdk_dev,
	       struct mlx5_dev_spawn_data *spawn,
	       struct mlx5_dev_config *config)
{
	const struct mlx5_switch_info *switch_info = &spawn->info;
	struct mlx5_dev_ctx_shared *sh = NULL;
	struct ibv_port_attr port_attr;
	struct mlx5dv_context dv_attr = { .comp_mask = 0 };
	struct rte_eth_dev *eth_dev = NULL;
	struct mlx5_priv *priv = NULL;
	int err = 0;
	unsigned int hw_padding = 0;
	unsigned int mps;
	unsigned int cqe_comp;
	unsigned int cqe_pad = 0;
	unsigned int tunnel_en = 0;
	unsigned int mpls_en = 0;
	unsigned int swp = 0;
	unsigned int mprq = 0;
	unsigned int mprq_min_stride_size_n = 0;
	unsigned int mprq_max_stride_size_n = 0;
	unsigned int mprq_min_stride_num_n = 0;
	unsigned int mprq_max_stride_num_n = 0;
	struct rte_ether_addr mac;
	char name[RTE_ETH_NAME_MAX_LEN];
	int own_domain_id = 0;
	uint16_t port_id;
	unsigned int i;
#ifdef HAVE_MLX5DV_DR_DEVX_PORT
	struct mlx5dv_devx_port devx_port = { .comp_mask = 0 };
#endif

	/* Determine if this port representor is supposed to be spawned. */
	if (switch_info->representor && dpdk_dev->devargs) {
		struct rte_eth_devargs eth_da;

		err = rte_eth_devargs_parse(dpdk_dev->devargs->args, &eth_da);
		if (err) {
			rte_errno = -err;
			DRV_LOG(ERR, "failed to process device arguments: %s",
				strerror(rte_errno));
			return NULL;
		}
		for (i = 0; i < eth_da.nb_representor_ports; ++i)
			if (eth_da.representor_ports[i] ==
			    (uint16_t)switch_info->port_name)
				break;
		if (i == eth_da.nb_representor_ports) {
			rte_errno = EBUSY;
			return NULL;
		}
	}
	/* Build device name. */
	if (spawn->pf_bond <  0) {
		/* Single device. */
		if (!switch_info->representor)
			strlcpy(name, dpdk_dev->name, sizeof(name));
		else
			snprintf(name, sizeof(name), "%s_representor_%u",
				 dpdk_dev->name, switch_info->port_name);
	} else {
		/* Bonding device. */
		if (!switch_info->representor)
			snprintf(name, sizeof(name), "%s_%s",
				 dpdk_dev->name,
				 mlx5_os_get_dev_device_name(spawn->phys_dev));
		else
			snprintf(name, sizeof(name), "%s_%s_representor_%u",
				 dpdk_dev->name,
				 mlx5_os_get_dev_device_name(spawn->phys_dev),
				 switch_info->port_name);
	}
	/* check if the device is already spawned */
	if (rte_eth_dev_get_port_by_name(name, &port_id) == 0) {
		rte_errno = EEXIST;
		return NULL;
	}
	DRV_LOG(DEBUG, "naming Ethernet device \"%s\"", name);
	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		struct mlx5_mp_id mp_id;

		eth_dev = rte_eth_dev_attach_secondary(name);
		if (eth_dev == NULL) {
			DRV_LOG(ERR, "can not attach rte ethdev");
			rte_errno = ENOMEM;
			return NULL;
		}
		eth_dev->device = dpdk_dev;
		eth_dev->dev_ops = &mlx5_os_dev_sec_ops;
		err = mlx5_proc_priv_init(eth_dev);
		if (err)
			return NULL;
		mp_id.port_id = eth_dev->data->port_id;
		strlcpy(mp_id.name, MLX5_MP_NAME, RTE_MP_MAX_NAME_LEN);
		/* Receive command fd from primary process */
		err = mlx5_mp_req_verbs_cmd_fd(&mp_id);
		if (err < 0)
			goto err_secondary;
		/* Remap UAR for Tx queues. */
		err = mlx5_tx_uar_init_secondary(eth_dev, err);
		if (err)
			goto err_secondary;
		/*
		 * Ethdev pointer is still required as input since
		 * the primary device is not accessible from the
		 * secondary process.
		 */
		eth_dev->rx_pkt_burst = mlx5_select_rx_function(eth_dev);
		eth_dev->tx_pkt_burst = mlx5_select_tx_function(eth_dev);
		return eth_dev;
err_secondary:
		mlx5_dev_close(eth_dev);
		return NULL;
	}
	/*
	 * Some parameters ("tx_db_nc" in particularly) are needed in
	 * advance to create dv/verbs device context. We proceed the
	 * devargs here to get ones, and later proceed devargs again
	 * to override some hardware settings.
	 */
	err = mlx5_args(config, dpdk_dev->devargs);
	if (err) {
		err = rte_errno;
		DRV_LOG(ERR, "failed to process device arguments: %s",
			strerror(rte_errno));
		goto error;
	}
	mlx5_malloc_mem_select(config->sys_mem_en);
	sh = mlx5_alloc_shared_dev_ctx(spawn, config);
	if (!sh)
		return NULL;
	config->devx = sh->devx;
#ifdef HAVE_MLX5DV_DR_ACTION_DEST_DEVX_TIR
	config->dest_tir = 1;
#endif
#ifdef HAVE_IBV_MLX5_MOD_SWP
	dv_attr.comp_mask |= MLX5DV_CONTEXT_MASK_SWP;
#endif
	/*
	 * Multi-packet send is supported by ConnectX-4 Lx PF as well
	 * as all ConnectX-5 devices.
	 */
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	dv_attr.comp_mask |= MLX5DV_CONTEXT_MASK_TUNNEL_OFFLOADS;
#endif
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
	dv_attr.comp_mask |= MLX5DV_CONTEXT_MASK_STRIDING_RQ;
#endif
	mlx5_glue->dv_query_device(sh->ctx, &dv_attr);
	if (dv_attr.flags & MLX5DV_CONTEXT_FLAGS_MPW_ALLOWED) {
		if (dv_attr.flags & MLX5DV_CONTEXT_FLAGS_ENHANCED_MPW) {
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
	if (dv_attr.comp_mask & MLX5DV_CONTEXT_MASK_SWP)
		swp = dv_attr.sw_parsing_caps.sw_parsing_offloads;
	DRV_LOG(DEBUG, "SWP support: %u", swp);
#endif
	config->swp = !!swp;
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
	if (dv_attr.comp_mask & MLX5DV_CONTEXT_MASK_STRIDING_RQ) {
		struct mlx5dv_striding_rq_caps mprq_caps =
			dv_attr.striding_rq_caps;

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
	    !(dv_attr.flags & MLX5DV_CONTEXT_FLAGS_CQE_128B_COMP))
		cqe_comp = 0;
	else
		cqe_comp = 1;
	config->cqe_comp = cqe_comp;
#ifdef HAVE_IBV_MLX5_MOD_CQE_128B_PAD
	/* Whether device supports 128B Rx CQE padding. */
	cqe_pad = RTE_CACHE_LINE_SIZE == 128 &&
		  (dv_attr.flags & MLX5DV_CONTEXT_FLAGS_CQE_128B_PAD);
#endif
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	if (dv_attr.comp_mask & MLX5DV_CONTEXT_MASK_TUNNEL_OFFLOADS) {
		tunnel_en = ((dv_attr.tunnel_offloads_caps &
			      MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_VXLAN) &&
			     (dv_attr.tunnel_offloads_caps &
			      MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_GRE) &&
			     (dv_attr.tunnel_offloads_caps &
			      MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_GENEVE));
	}
	DRV_LOG(DEBUG, "tunnel offloading is %ssupported",
		tunnel_en ? "" : "not ");
#else
	DRV_LOG(WARNING,
		"tunnel offloading disabled due to old OFED/rdma-core version");
#endif
	config->tunnel_en = tunnel_en;
#ifdef HAVE_IBV_DEVICE_MPLS_SUPPORT
	mpls_en = ((dv_attr.tunnel_offloads_caps &
		    MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_CW_MPLS_OVER_GRE) &&
		   (dv_attr.tunnel_offloads_caps &
		    MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_CW_MPLS_OVER_UDP));
	DRV_LOG(DEBUG, "MPLS over GRE/UDP tunnel offloading is %ssupported",
		mpls_en ? "" : "not ");
#else
	DRV_LOG(WARNING, "MPLS over GRE/UDP tunnel offloading disabled due to"
		" old OFED/rdma-core version or firmware configuration");
#endif
	config->mpls_en = mpls_en;
	/* Check port status. */
	err = mlx5_glue->query_port(sh->ctx, spawn->phys_port, &port_attr);
	if (err) {
		DRV_LOG(ERR, "port query failed: %s", strerror(err));
		goto error;
	}
	if (port_attr.link_layer != IBV_LINK_LAYER_ETHERNET) {
		DRV_LOG(ERR, "port is not configured in Ethernet mode");
		err = EINVAL;
		goto error;
	}
	if (port_attr.state != IBV_PORT_ACTIVE)
		DRV_LOG(DEBUG, "port is not active: \"%s\" (%d)",
			mlx5_glue->port_state_str(port_attr.state),
			port_attr.state);
	/* Allocate private eth device data. */
	priv = mlx5_malloc(MLX5_MEM_ZERO | MLX5_MEM_RTE,
			   sizeof(*priv),
			   RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (priv == NULL) {
		DRV_LOG(ERR, "priv allocation failure");
		err = ENOMEM;
		goto error;
	}
	priv->sh = sh;
	priv->dev_port = spawn->phys_port;
	priv->pci_dev = spawn->pci_dev;
	priv->mtu = RTE_ETHER_MTU;
	priv->mp_id.port_id = port_id;
	strlcpy(priv->mp_id.name, MLX5_MP_NAME, RTE_MP_MAX_NAME_LEN);
	/* Some internal functions rely on Netlink sockets, open them now. */
	priv->nl_socket_rdma = mlx5_nl_init(NETLINK_RDMA);
	priv->nl_socket_route =	mlx5_nl_init(NETLINK_ROUTE);
	priv->representor = !!switch_info->representor;
	priv->master = !!switch_info->master;
	priv->domain_id = RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID;
	priv->vport_meta_tag = 0;
	priv->vport_meta_mask = 0;
	priv->pf_bond = spawn->pf_bond;
#ifdef HAVE_MLX5DV_DR_DEVX_PORT
	/*
	 * The DevX port query API is implemented. E-Switch may use
	 * either vport or reg_c[0] metadata register to match on
	 * vport index. The engaged part of metadata register is
	 * defined by mask.
	 */
	if (switch_info->representor || switch_info->master) {
		devx_port.comp_mask = MLX5DV_DEVX_PORT_VPORT |
				      MLX5DV_DEVX_PORT_MATCH_REG_C_0;
		err = mlx5_glue->devx_port_query(sh->ctx, spawn->phys_port,
						 &devx_port);
		if (err) {
			DRV_LOG(WARNING,
				"can't query devx port %d on device %s",
				spawn->phys_port,
				mlx5_os_get_dev_device_name(spawn->phys_dev));
			devx_port.comp_mask = 0;
		}
	}
	if (devx_port.comp_mask & MLX5DV_DEVX_PORT_MATCH_REG_C_0) {
		priv->vport_meta_tag = devx_port.reg_c_0.value;
		priv->vport_meta_mask = devx_port.reg_c_0.mask;
		if (!priv->vport_meta_mask) {
			DRV_LOG(ERR, "vport zero mask for port %d"
				     " on bonding device %s",
				     spawn->phys_port,
				     mlx5_os_get_dev_device_name
							(spawn->phys_dev));
			err = ENOTSUP;
			goto error;
		}
		if (priv->vport_meta_tag & ~priv->vport_meta_mask) {
			DRV_LOG(ERR, "invalid vport tag for port %d"
				     " on bonding device %s",
				     spawn->phys_port,
				     mlx5_os_get_dev_device_name
							(spawn->phys_dev));
			err = ENOTSUP;
			goto error;
		}
	}
	if (devx_port.comp_mask & MLX5DV_DEVX_PORT_VPORT) {
		priv->vport_id = devx_port.vport_num;
	} else if (spawn->pf_bond >= 0) {
		DRV_LOG(ERR, "can't deduce vport index for port %d"
			     " on bonding device %s",
			     spawn->phys_port,
			     mlx5_os_get_dev_device_name(spawn->phys_dev));
		err = ENOTSUP;
		goto error;
	} else {
		/* Suppose vport index in compatible way. */
		priv->vport_id = switch_info->representor ?
				 switch_info->port_name + 1 : -1;
	}
#else
	/*
	 * Kernel/rdma_core support single E-Switch per PF configurations
	 * only and vport_id field contains the vport index for
	 * associated VF, which is deduced from representor port name.
	 * For example, let's have the IB device port 10, it has
	 * attached network device eth0, which has port name attribute
	 * pf0vf2, we can deduce the VF number as 2, and set vport index
	 * as 3 (2+1). This assigning schema should be changed if the
	 * multiple E-Switch instances per PF configurations or/and PCI
	 * subfunctions are added.
	 */
	priv->vport_id = switch_info->representor ?
			 switch_info->port_name + 1 : -1;
#endif
	/* representor_id field keeps the unmodified VF index. */
	priv->representor_id = switch_info->representor ?
			       switch_info->port_name : -1;
	/*
	 * Look for sibling devices in order to reuse their switch domain
	 * if any, otherwise allocate one.
	 */
	MLX5_ETH_FOREACH_DEV(port_id, priv->pci_dev) {
		const struct mlx5_priv *opriv =
			rte_eth_devices[port_id].data->dev_private;

		if (!opriv ||
		    opriv->sh != priv->sh ||
			opriv->domain_id ==
			RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID)
			continue;
		priv->domain_id = opriv->domain_id;
		break;
	}
	if (priv->domain_id == RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID) {
		err = rte_eth_switch_domain_alloc(&priv->domain_id);
		if (err) {
			err = rte_errno;
			DRV_LOG(ERR, "unable to allocate switch domain: %s",
				strerror(rte_errno));
			goto error;
		}
		own_domain_id = 1;
	}
	/* Override some values set by hardware configuration. */
	mlx5_args(config, dpdk_dev->devargs);
	err = mlx5_dev_check_sibling_config(priv, config);
	if (err)
		goto error;
	config->hw_csum = !!(sh->device_attr.device_cap_flags_ex &
			    IBV_DEVICE_RAW_IP_CSUM);
	DRV_LOG(DEBUG, "checksum offloading is %ssupported",
		(config->hw_csum ? "" : "not "));
#if !defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42) && \
	!defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
	DRV_LOG(DEBUG, "counters are not supported");
#endif
#if !defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_MLX5DV_DR)
	if (config->dv_flow_en) {
		DRV_LOG(WARNING, "DV flow is not supported");
		config->dv_flow_en = 0;
	}
#endif
	config->ind_table_max_size =
		sh->device_attr.max_rwq_indirection_table_size;
	/*
	 * Remove this check once DPDK supports larger/variable
	 * indirection tables.
	 */
	if (config->ind_table_max_size > (unsigned int)ETH_RSS_RETA_SIZE_512)
		config->ind_table_max_size = ETH_RSS_RETA_SIZE_512;
	DRV_LOG(DEBUG, "maximum Rx indirection table size is %u",
		config->ind_table_max_size);
	config->hw_vlan_strip = !!(sh->device_attr.raw_packet_caps &
				  IBV_RAW_PACKET_CAP_CVLAN_STRIPPING);
	DRV_LOG(DEBUG, "VLAN stripping is %ssupported",
		(config->hw_vlan_strip ? "" : "not "));
	config->hw_fcs_strip = !!(sh->device_attr.raw_packet_caps &
				 IBV_RAW_PACKET_CAP_SCATTER_FCS);
#if defined(HAVE_IBV_WQ_FLAG_RX_END_PADDING)
	hw_padding = !!sh->device_attr.rx_pad_end_addr_align;
#elif defined(HAVE_IBV_WQ_FLAGS_PCI_WRITE_END_PADDING)
	hw_padding = !!(sh->device_attr.device_cap_flags_ex &
			IBV_DEVICE_PCI_WRITE_END_PADDING);
#endif
	if (config->hw_padding && !hw_padding) {
		DRV_LOG(DEBUG, "Rx end alignment padding isn't supported");
		config->hw_padding = 0;
	} else if (config->hw_padding) {
		DRV_LOG(DEBUG, "Rx end alignment padding is enabled");
	}
	config->tso = (sh->device_attr.max_tso > 0 &&
		      (sh->device_attr.tso_supported_qpts &
		       (1 << IBV_QPT_RAW_PACKET)));
	if (config->tso)
		config->tso_max_payload_sz = sh->device_attr.max_tso;
	/*
	 * MPW is disabled by default, while the Enhanced MPW is enabled
	 * by default.
	 */
	if (config->mps == MLX5_ARG_UNSET)
		config->mps = (mps == MLX5_MPW_ENHANCED) ? MLX5_MPW_ENHANCED :
							  MLX5_MPW_DISABLED;
	else
		config->mps = config->mps ? mps : MLX5_MPW_DISABLED;
	DRV_LOG(INFO, "%sMPS is %s",
		config->mps == MLX5_MPW_ENHANCED ? "enhanced " :
		config->mps == MLX5_MPW ? "legacy " : "",
		config->mps != MLX5_MPW_DISABLED ? "enabled" : "disabled");
	if (config->cqe_comp && !cqe_comp) {
		DRV_LOG(WARNING, "Rx CQE compression isn't supported");
		config->cqe_comp = 0;
	}
	if (config->cqe_pad && !cqe_pad) {
		DRV_LOG(WARNING, "Rx CQE padding isn't supported");
		config->cqe_pad = 0;
	} else if (config->cqe_pad) {
		DRV_LOG(INFO, "Rx CQE padding is enabled");
	}
	if (config->devx) {
		priv->counter_fallback = 0;
		err = mlx5_devx_cmd_query_hca_attr(sh->ctx, &config->hca_attr);
		if (err) {
			err = -err;
			goto error;
		}
		if (!config->hca_attr.flow_counters_dump)
			priv->counter_fallback = 1;
#ifndef HAVE_IBV_DEVX_ASYNC
		priv->counter_fallback = 1;
#endif
		if (priv->counter_fallback)
			DRV_LOG(INFO, "Use fall-back DV counter management");
		/* Check for LRO support. */
		if (config->dest_tir && config->hca_attr.lro_cap &&
		    config->dv_flow_en) {
			/* TBD check tunnel lro caps. */
			config->lro.supported = config->hca_attr.lro_cap;
			DRV_LOG(DEBUG, "Device supports LRO");
			/*
			 * If LRO timeout is not configured by application,
			 * use the minimal supported value.
			 */
			if (!config->lro.timeout)
				config->lro.timeout =
				config->hca_attr.lro_timer_supported_periods[0];
			DRV_LOG(DEBUG, "LRO session timeout set to %d usec",
				config->lro.timeout);
		}
#if defined(HAVE_MLX5DV_DR) && defined(HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER)
		if (config->hca_attr.qos.sup &&
		    config->hca_attr.qos.srtcm_sup &&
		    config->dv_flow_en) {
			uint8_t reg_c_mask =
				config->hca_attr.qos.flow_meter_reg_c_ids;
			/*
			 * Meter needs two REG_C's for color match and pre-sfx
			 * flow match. Here get the REG_C for color match.
			 * REG_C_0 and REG_C_1 is reserved for metadata feature.
			 */
			reg_c_mask &= 0xfc;
			if (__builtin_popcount(reg_c_mask) < 1) {
				priv->mtr_en = 0;
				DRV_LOG(WARNING, "No available register for"
					" meter.");
			} else {
				priv->mtr_color_reg = ffs(reg_c_mask) - 1 +
						      REG_C_0;
				priv->mtr_en = 1;
				priv->mtr_reg_share =
				      config->hca_attr.qos.flow_meter_reg_share;
				DRV_LOG(DEBUG, "The REG_C meter uses is %d",
					priv->mtr_color_reg);
			}
		}
#endif
	}
	if (config->tx_pp) {
		DRV_LOG(DEBUG, "Timestamp counter frequency %u kHz",
			config->hca_attr.dev_freq_khz);
		DRV_LOG(DEBUG, "Packet pacing is %ssupported",
			config->hca_attr.qos.packet_pacing ? "" : "not ");
		DRV_LOG(DEBUG, "Cross channel ops are %ssupported",
			config->hca_attr.cross_channel ? "" : "not ");
		DRV_LOG(DEBUG, "WQE index ignore is %ssupported",
			config->hca_attr.wqe_index_ignore ? "" : "not ");
		DRV_LOG(DEBUG, "Non-wire SQ feature is %ssupported",
			config->hca_attr.non_wire_sq ? "" : "not ");
		DRV_LOG(DEBUG, "Static WQE SQ feature is %ssupported (%d)",
			config->hca_attr.log_max_static_sq_wq ? "" : "not ",
			config->hca_attr.log_max_static_sq_wq);
		DRV_LOG(DEBUG, "WQE rate PP mode is %ssupported",
			config->hca_attr.qos.wqe_rate_pp ? "" : "not ");
		if (!config->devx) {
			DRV_LOG(ERR, "DevX is required for packet pacing");
			err = ENODEV;
			goto error;
		}
		if (!config->hca_attr.qos.packet_pacing) {
			DRV_LOG(ERR, "Packet pacing is not supported");
			err = ENODEV;
			goto error;
		}
		if (!config->hca_attr.cross_channel) {
			DRV_LOG(ERR, "Cross channel operations are"
				     " required for packet pacing");
			err = ENODEV;
			goto error;
		}
		if (!config->hca_attr.wqe_index_ignore) {
			DRV_LOG(ERR, "WQE index ignore feature is"
				     " required for packet pacing");
			err = ENODEV;
			goto error;
		}
		if (!config->hca_attr.non_wire_sq) {
			DRV_LOG(ERR, "Non-wire SQ feature is"
				     " required for packet pacing");
			err = ENODEV;
			goto error;
		}
		if (!config->hca_attr.log_max_static_sq_wq) {
			DRV_LOG(ERR, "Static WQE SQ feature is"
				     " required for packet pacing");
			err = ENODEV;
			goto error;
		}
		if (!config->hca_attr.qos.wqe_rate_pp) {
			DRV_LOG(ERR, "WQE rate mode is required"
				     " for packet pacing");
			err = ENODEV;
			goto error;
		}
#ifndef HAVE_MLX5DV_DEVX_UAR_OFFSET
		DRV_LOG(ERR, "DevX does not provide UAR offset,"
			     " can't create queues for packet pacing");
		err = ENODEV;
		goto error;
#endif
	}
	if (config->devx) {
		uint32_t reg[MLX5_ST_SZ_DW(register_mtutc)];

		err = config->hca_attr.access_register_user ?
			mlx5_devx_cmd_register_read
				(sh->ctx, MLX5_REGISTER_ID_MTUTC, 0,
				reg, MLX5_ST_SZ_DW(register_mtutc)) : ENOTSUP;
		if (!err) {
			uint32_t ts_mode;

			/* MTUTC register is read successfully. */
			ts_mode = MLX5_GET(register_mtutc, reg,
					   time_stamp_mode);
			if (ts_mode == MLX5_MTUTC_TIMESTAMP_MODE_REAL_TIME)
				config->rt_timestamp = 1;
		} else {
			/* Kernel does not support register reading. */
			if (config->hca_attr.dev_freq_khz ==
						 (NS_PER_S / MS_PER_S))
				config->rt_timestamp = 1;
		}
	}
	/*
	 * If HW has bug working with tunnel packet decapsulation and
	 * scatter FCS, and decapsulation is needed, clear the hw_fcs_strip
	 * bit. Then DEV_RX_OFFLOAD_KEEP_CRC bit will not be set anymore.
	 */
	if (config->hca_attr.scatter_fcs_w_decap_disable && config->decap_en)
		config->hw_fcs_strip = 0;
	DRV_LOG(DEBUG, "FCS stripping configuration is %ssupported",
		(config->hw_fcs_strip ? "" : "not "));
	if (config->mprq.enabled && mprq) {
		if (config->mprq.stride_num_n &&
		    (config->mprq.stride_num_n > mprq_max_stride_num_n ||
		     config->mprq.stride_num_n < mprq_min_stride_num_n)) {
			config->mprq.stride_num_n =
				RTE_MIN(RTE_MAX(MLX5_MPRQ_STRIDE_NUM_N,
						mprq_min_stride_num_n),
					mprq_max_stride_num_n);
			DRV_LOG(WARNING,
				"the number of strides"
				" for Multi-Packet RQ is out of range,"
				" setting default value (%u)",
				1 << config->mprq.stride_num_n);
		}
		if (config->mprq.stride_size_n &&
		    (config->mprq.stride_size_n > mprq_max_stride_size_n ||
		     config->mprq.stride_size_n < mprq_min_stride_size_n)) {
			config->mprq.stride_size_n =
				RTE_MIN(RTE_MAX(MLX5_MPRQ_STRIDE_SIZE_N,
						mprq_min_stride_size_n),
					mprq_max_stride_size_n);
			DRV_LOG(WARNING,
				"the size of a stride"
				" for Multi-Packet RQ is out of range,"
				" setting default value (%u)",
				1 << config->mprq.stride_size_n);
		}
		config->mprq.min_stride_size_n = mprq_min_stride_size_n;
		config->mprq.max_stride_size_n = mprq_max_stride_size_n;
	} else if (config->mprq.enabled && !mprq) {
		DRV_LOG(WARNING, "Multi-Packet RQ isn't supported");
		config->mprq.enabled = 0;
	}
	if (config->max_dump_files_num == 0)
		config->max_dump_files_num = 128;
	eth_dev = rte_eth_dev_allocate(name);
	if (eth_dev == NULL) {
		DRV_LOG(ERR, "can not allocate rte ethdev");
		err = ENOMEM;
		goto error;
	}
	/* Flag to call rte_eth_dev_release_port() in rte_eth_dev_close(). */
	eth_dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;
	if (priv->representor) {
		eth_dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR;
		eth_dev->data->representor_id = priv->representor_id;
	}
	/*
	 * Store associated network device interface index. This index
	 * is permanent throughout the lifetime of device. So, we may store
	 * the ifindex here and use the cached value further.
	 */
	MLX5_ASSERT(spawn->ifindex);
	priv->if_index = spawn->ifindex;
	eth_dev->data->dev_private = priv;
	priv->dev_data = eth_dev->data;
	eth_dev->data->mac_addrs = priv->mac;
	eth_dev->device = dpdk_dev;
	/* Configure the first MAC address by default. */
	if (mlx5_get_mac(eth_dev, &mac.addr_bytes)) {
		DRV_LOG(ERR,
			"port %u cannot get MAC address, is mlx5_en"
			" loaded? (errno: %s)",
			eth_dev->data->port_id, strerror(rte_errno));
		err = ENODEV;
		goto error;
	}
	DRV_LOG(INFO,
		"port %u MAC address is %02x:%02x:%02x:%02x:%02x:%02x",
		eth_dev->data->port_id,
		mac.addr_bytes[0], mac.addr_bytes[1],
		mac.addr_bytes[2], mac.addr_bytes[3],
		mac.addr_bytes[4], mac.addr_bytes[5]);
#ifdef RTE_LIBRTE_MLX5_DEBUG
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
		goto error;
	}
	DRV_LOG(DEBUG, "port %u MTU is %u", eth_dev->data->port_id,
		priv->mtu);
	/* Initialize burst functions to prevent crashes before link-up. */
	eth_dev->rx_pkt_burst = removed_rx_burst;
	eth_dev->tx_pkt_burst = removed_tx_burst;
	eth_dev->dev_ops = &mlx5_os_dev_ops;
	/* Register MAC address. */
	claim_zero(mlx5_mac_addr_add(eth_dev, &mac, 0, 0));
	if (config->vf && config->vf_nl_en)
		mlx5_nl_mac_addr_sync(priv->nl_socket_route,
				      mlx5_ifindex(eth_dev),
				      eth_dev->data->mac_addrs,
				      MLX5_MAX_MAC_ADDRESSES);
	priv->flows = 0;
	priv->ctrl_flows = 0;
	TAILQ_INIT(&priv->flow_meters);
	TAILQ_INIT(&priv->flow_meter_profiles);
	/* Hint libmlx5 to use PMD allocator for data plane resources */
	mlx5_glue->dv_set_context_attr(sh->ctx,
			MLX5DV_CTX_ATTR_BUF_ALLOCATORS,
			(void *)((uintptr_t)&(struct mlx5dv_ctx_allocators){
				.alloc = &mlx5_alloc_verbs_buf,
				.free = &mlx5_free_verbs_buf,
				.data = priv,
			}));
	/* Bring Ethernet device up. */
	DRV_LOG(DEBUG, "port %u forcing Ethernet interface up",
		eth_dev->data->port_id);
	mlx5_set_link_up(eth_dev);
	/*
	 * Even though the interrupt handler is not installed yet,
	 * interrupts will still trigger on the async_fd from
	 * Verbs context returned by ibv_open_device().
	 */
	mlx5_link_update(eth_dev, 0);
#ifdef HAVE_MLX5DV_DR_ESWITCH
	if (!(config->hca_attr.eswitch_manager && config->dv_flow_en &&
	      (switch_info->representor || switch_info->master)))
		config->dv_esw_en = 0;
#else
	config->dv_esw_en = 0;
#endif
	/* Detect minimal data bytes to inline. */
	mlx5_set_min_inline(spawn, config);
	/* Store device configuration on private structure. */
	priv->config = *config;
	/* Create context for virtual machine VLAN workaround. */
	priv->vmwa_context = mlx5_vlan_vmwa_init(eth_dev, spawn->ifindex);
	if (config->dv_flow_en) {
		err = mlx5_alloc_shared_dr(priv);
		if (err)
			goto error;
		/*
		 * RSS id is shared with meter flow id. Meter flow id can only
		 * use the 24 MSB of the register.
		 */
		priv->qrss_id_pool = mlx5_flow_id_pool_alloc(UINT32_MAX >>
				     MLX5_MTR_COLOR_BITS);
		if (!priv->qrss_id_pool) {
			DRV_LOG(ERR, "can't create flow id pool");
			err = ENOMEM;
			goto error;
		}
	}
	/* Supported Verbs flow priority number detection. */
	err = mlx5_flow_discover_priorities(eth_dev);
	if (err < 0) {
		err = -err;
		goto error;
	}
	priv->config.flow_prio = err;
	if (!priv->config.dv_esw_en &&
	    priv->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY) {
		DRV_LOG(WARNING, "metadata mode %u is not supported "
				 "(no E-Switch)", priv->config.dv_xmeta_en);
		priv->config.dv_xmeta_en = MLX5_XMETA_MODE_LEGACY;
	}
	mlx5_set_metadata_mask(eth_dev);
	if (priv->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY &&
	    !priv->sh->dv_regc0_mask) {
		DRV_LOG(ERR, "metadata mode %u is not supported "
			     "(no metadata reg_c[0] is available)",
			     priv->config.dv_xmeta_en);
			err = ENOTSUP;
			goto error;
	}
	/*
	 * Allocate the buffer for flow creating, just once.
	 * The allocation must be done before any flow creating.
	 */
	mlx5_flow_alloc_intermediate(eth_dev);
	/* Query availability of metadata reg_c's. */
	err = mlx5_flow_discover_mreg_c(eth_dev);
	if (err < 0) {
		err = -err;
		goto error;
	}
	if (!mlx5_flow_ext_mreg_supported(eth_dev)) {
		DRV_LOG(DEBUG,
			"port %u extensive metadata register is not supported",
			eth_dev->data->port_id);
		if (priv->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY) {
			DRV_LOG(ERR, "metadata mode %u is not supported "
				     "(no metadata registers available)",
				     priv->config.dv_xmeta_en);
			err = ENOTSUP;
			goto error;
		}
	}
	if (priv->config.dv_flow_en &&
	    priv->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY &&
	    mlx5_flow_ext_mreg_supported(eth_dev) &&
	    priv->sh->dv_regc0_mask) {
		priv->mreg_cp_tbl = mlx5_hlist_create(MLX5_FLOW_MREG_HNAME,
						      MLX5_FLOW_MREG_HTABLE_SZ);
		if (!priv->mreg_cp_tbl) {
			err = ENOMEM;
			goto error;
		}
	}
	return eth_dev;
error:
	if (priv) {
		if (priv->mreg_cp_tbl)
			mlx5_hlist_destroy(priv->mreg_cp_tbl, NULL, NULL);
		if (priv->sh)
			mlx5_os_free_shared_dr(priv);
		if (priv->nl_socket_route >= 0)
			close(priv->nl_socket_route);
		if (priv->nl_socket_rdma >= 0)
			close(priv->nl_socket_rdma);
		if (priv->vmwa_context)
			mlx5_vlan_vmwa_exit(priv->vmwa_context);
		if (priv->qrss_id_pool)
			mlx5_flow_id_pool_release(priv->qrss_id_pool);
		if (own_domain_id)
			claim_zero(rte_eth_switch_domain_free(priv->domain_id));
		mlx5_free(priv);
		if (eth_dev != NULL)
			eth_dev->data->dev_private = NULL;
	}
	if (eth_dev != NULL) {
		/* mac_addrs must not be freed alone because part of
		 * dev_private
		 **/
		eth_dev->data->mac_addrs = NULL;
		rte_eth_dev_release_port(eth_dev);
	}
	if (sh)
		mlx5_free_shared_dev_ctx(sh);
	MLX5_ASSERT(err > 0);
	rte_errno = err;
	return NULL;
}

/**
 * Comparison callback to sort device data.
 *
 * This is meant to be used with qsort().
 *
 * @param a[in]
 *   Pointer to pointer to first data object.
 * @param b[in]
 *   Pointer to pointer to second data object.
 *
 * @return
 *   0 if both objects are equal, less than 0 if the first argument is less
 *   than the second, greater than 0 otherwise.
 */
static int
mlx5_dev_spawn_data_cmp(const void *a, const void *b)
{
	const struct mlx5_switch_info *si_a =
		&((const struct mlx5_dev_spawn_data *)a)->info;
	const struct mlx5_switch_info *si_b =
		&((const struct mlx5_dev_spawn_data *)b)->info;
	int ret;

	/* Master device first. */
	ret = si_b->master - si_a->master;
	if (ret)
		return ret;
	/* Then representor devices. */
	ret = si_b->representor - si_a->representor;
	if (ret)
		return ret;
	/* Unidentified devices come last in no specific order. */
	if (!si_a->representor)
		return 0;
	/* Order representors by name. */
	return si_a->port_name - si_b->port_name;
}

/**
 * Match PCI information for possible slaves of bonding device.
 *
 * @param[in] ibv_dev
 *   Pointer to Infiniband device structure.
 * @param[in] pci_dev
 *   Pointer to PCI device structure to match PCI address.
 * @param[in] nl_rdma
 *   Netlink RDMA group socket handle.
 *
 * @return
 *   negative value if no bonding device found, otherwise
 *   positive index of slave PF in bonding.
 */
static int
mlx5_device_bond_pci_match(const struct ibv_device *ibv_dev,
			   const struct rte_pci_device *pci_dev,
			   int nl_rdma)
{
	char ifname[IF_NAMESIZE + 1];
	unsigned int ifindex;
	unsigned int np, i;
	FILE *file = NULL;
	int pf = -1;

	/*
	 * Try to get master device name. If something goes
	 * wrong suppose the lack of kernel support and no
	 * bonding devices.
	 */
	if (nl_rdma < 0)
		return -1;
	if (!strstr(ibv_dev->name, "bond"))
		return -1;
	np = mlx5_nl_portnum(nl_rdma, ibv_dev->name);
	if (!np)
		return -1;
	/*
	 * The Master device might not be on the predefined
	 * port (not on port index 1, it is not garanted),
	 * we have to scan all Infiniband device port and
	 * find master.
	 */
	for (i = 1; i <= np; ++i) {
		/* Check whether Infiniband port is populated. */
		ifindex = mlx5_nl_ifindex(nl_rdma, ibv_dev->name, i);
		if (!ifindex)
			continue;
		if (!if_indextoname(ifindex, ifname))
			continue;
		/* Try to read bonding slave names from sysfs. */
		MKSTR(slaves,
		      "/sys/class/net/%s/master/bonding/slaves", ifname);
		file = fopen(slaves, "r");
		if (file)
			break;
	}
	if (!file)
		return -1;
	/* Use safe format to check maximal buffer length. */
	MLX5_ASSERT(atol(RTE_STR(IF_NAMESIZE)) == IF_NAMESIZE);
	while (fscanf(file, "%" RTE_STR(IF_NAMESIZE) "s", ifname) == 1) {
		char tmp_str[IF_NAMESIZE + 32];
		struct rte_pci_addr pci_addr;
		struct mlx5_switch_info	info;

		/* Process slave interface names in the loop. */
		snprintf(tmp_str, sizeof(tmp_str),
			 "/sys/class/net/%s", ifname);
		if (mlx5_dev_to_pci_addr(tmp_str, &pci_addr)) {
			DRV_LOG(WARNING, "can not get PCI address"
					 " for netdev \"%s\"", ifname);
			continue;
		}
		if (pci_dev->addr.domain != pci_addr.domain ||
		    pci_dev->addr.bus != pci_addr.bus ||
		    pci_dev->addr.devid != pci_addr.devid ||
		    pci_dev->addr.function != pci_addr.function)
			continue;
		/* Slave interface PCI address match found. */
		fclose(file);
		snprintf(tmp_str, sizeof(tmp_str),
			 "/sys/class/net/%s/phys_port_name", ifname);
		file = fopen(tmp_str, "rb");
		if (!file)
			break;
		info.name_type = MLX5_PHYS_PORT_NAME_TYPE_NOTSET;
		if (fscanf(file, "%32s", tmp_str) == 1)
			mlx5_translate_port_name(tmp_str, &info);
		if (info.name_type == MLX5_PHYS_PORT_NAME_TYPE_LEGACY ||
		    info.name_type == MLX5_PHYS_PORT_NAME_TYPE_UPLINK)
			pf = info.port_name;
		break;
	}
	if (file)
		fclose(file);
	return pf;
}

/**
 * DPDK callback to register a PCI device.
 *
 * This function spawns Ethernet devices out of a given PCI device.
 *
 * @param[in] pci_drv
 *   PCI driver structure (mlx5_driver).
 * @param[in] pci_dev
 *   PCI device information.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_os_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		  struct rte_pci_device *pci_dev)
{
	struct ibv_device **ibv_list;
	/*
	 * Number of found IB Devices matching with requested PCI BDF.
	 * nd != 1 means there are multiple IB devices over the same
	 * PCI device and we have representors and master.
	 */
	unsigned int nd = 0;
	/*
	 * Number of found IB device Ports. nd = 1 and np = 1..n means
	 * we have the single multiport IB device, and there may be
	 * representors attached to some of found ports.
	 */
	unsigned int np = 0;
	/*
	 * Number of DPDK ethernet devices to Spawn - either over
	 * multiple IB devices or multiple ports of single IB device.
	 * Actually this is the number of iterations to spawn.
	 */
	unsigned int ns = 0;
	/*
	 * Bonding device
	 *   < 0 - no bonding device (single one)
	 *  >= 0 - bonding device (value is slave PF index)
	 */
	int bd = -1;
	struct mlx5_dev_spawn_data *list = NULL;
	struct mlx5_dev_config dev_config;
	unsigned int dev_config_vf;
	int ret;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		mlx5_pmd_socket_init();
	ret = mlx5_init_once();
	if (ret) {
		DRV_LOG(ERR, "unable to init PMD global data: %s",
			strerror(rte_errno));
		return -rte_errno;
	}
	errno = 0;
	ibv_list = mlx5_glue->get_device_list(&ret);
	if (!ibv_list) {
		rte_errno = errno ? errno : ENOSYS;
		DRV_LOG(ERR, "cannot list devices, is ib_uverbs loaded?");
		return -rte_errno;
	}
	/*
	 * First scan the list of all Infiniband devices to find
	 * matching ones, gathering into the list.
	 */
	struct ibv_device *ibv_match[ret + 1];
	int nl_route = mlx5_nl_init(NETLINK_ROUTE);
	int nl_rdma = mlx5_nl_init(NETLINK_RDMA);
	unsigned int i;

	while (ret-- > 0) {
		struct rte_pci_addr pci_addr;

		DRV_LOG(DEBUG, "checking device \"%s\"", ibv_list[ret]->name);
		bd = mlx5_device_bond_pci_match
				(ibv_list[ret], pci_dev, nl_rdma);
		if (bd >= 0) {
			/*
			 * Bonding device detected. Only one match is allowed,
			 * the bonding is supported over multi-port IB device,
			 * there should be no matches on representor PCI
			 * functions or non VF LAG bonding devices with
			 * specified address.
			 */
			if (nd) {
				DRV_LOG(ERR,
					"multiple PCI match on bonding device"
					"\"%s\" found", ibv_list[ret]->name);
				rte_errno = ENOENT;
				ret = -rte_errno;
				goto exit;
			}
			DRV_LOG(INFO, "PCI information matches for"
				      " slave %d bonding device \"%s\"",
				      bd, ibv_list[ret]->name);
			ibv_match[nd++] = ibv_list[ret];
			break;
		}
		if (mlx5_dev_to_pci_addr
			(ibv_list[ret]->ibdev_path, &pci_addr))
			continue;
		if (pci_dev->addr.domain != pci_addr.domain ||
		    pci_dev->addr.bus != pci_addr.bus ||
		    pci_dev->addr.devid != pci_addr.devid ||
		    pci_dev->addr.function != pci_addr.function)
			continue;
		DRV_LOG(INFO, "PCI information matches for device \"%s\"",
			ibv_list[ret]->name);
		ibv_match[nd++] = ibv_list[ret];
	}
	ibv_match[nd] = NULL;
	if (!nd) {
		/* No device matches, just complain and bail out. */
		DRV_LOG(WARNING,
			"no Verbs device matches PCI device " PCI_PRI_FMT ","
			" are kernel drivers loaded?",
			pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function);
		rte_errno = ENOENT;
		ret = -rte_errno;
		goto exit;
	}
	if (nd == 1) {
		/*
		 * Found single matching device may have multiple ports.
		 * Each port may be representor, we have to check the port
		 * number and check the representors existence.
		 */
		if (nl_rdma >= 0)
			np = mlx5_nl_portnum(nl_rdma, ibv_match[0]->name);
		if (!np)
			DRV_LOG(WARNING, "can not get IB device \"%s\""
					 " ports number", ibv_match[0]->name);
		if (bd >= 0 && !np) {
			DRV_LOG(ERR, "can not get ports"
				     " for bonding device");
			rte_errno = ENOENT;
			ret = -rte_errno;
			goto exit;
		}
	}
#ifndef HAVE_MLX5DV_DR_DEVX_PORT
	if (bd >= 0) {
		/*
		 * This may happen if there is VF LAG kernel support and
		 * application is compiled with older rdma_core library.
		 */
		DRV_LOG(ERR,
			"No kernel/verbs support for VF LAG bonding found.");
		rte_errno = ENOTSUP;
		ret = -rte_errno;
		goto exit;
	}
#endif
	/*
	 * Now we can determine the maximal
	 * amount of devices to be spawned.
	 */
	list = mlx5_malloc(MLX5_MEM_ZERO,
			   sizeof(struct mlx5_dev_spawn_data) *
			   (np ? np : nd),
			   RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (!list) {
		DRV_LOG(ERR, "spawn data array allocation failure");
		rte_errno = ENOMEM;
		ret = -rte_errno;
		goto exit;
	}
	if (bd >= 0 || np > 1) {
		/*
		 * Single IB device with multiple ports found,
		 * it may be E-Switch master device and representors.
		 * We have to perform identification through the ports.
		 */
		MLX5_ASSERT(nl_rdma >= 0);
		MLX5_ASSERT(ns == 0);
		MLX5_ASSERT(nd == 1);
		MLX5_ASSERT(np);
		for (i = 1; i <= np; ++i) {
			list[ns].max_port = np;
			list[ns].phys_port = i;
			list[ns].phys_dev = ibv_match[0];
			list[ns].eth_dev = NULL;
			list[ns].pci_dev = pci_dev;
			list[ns].pf_bond = bd;
			list[ns].ifindex = mlx5_nl_ifindex
				(nl_rdma,
				mlx5_os_get_dev_device_name
						(list[ns].phys_dev), i);
			if (!list[ns].ifindex) {
				/*
				 * No network interface index found for the
				 * specified port, it means there is no
				 * representor on this port. It's OK,
				 * there can be disabled ports, for example
				 * if sriov_numvfs < sriov_totalvfs.
				 */
				continue;
			}
			ret = -1;
			if (nl_route >= 0)
				ret = mlx5_nl_switch_info
					       (nl_route,
						list[ns].ifindex,
						&list[ns].info);
			if (ret || (!list[ns].info.representor &&
				    !list[ns].info.master)) {
				/*
				 * We failed to recognize representors with
				 * Netlink, let's try to perform the task
				 * with sysfs.
				 */
				ret =  mlx5_sysfs_switch_info
						(list[ns].ifindex,
						 &list[ns].info);
			}
			if (!ret && bd >= 0) {
				switch (list[ns].info.name_type) {
				case MLX5_PHYS_PORT_NAME_TYPE_UPLINK:
					if (list[ns].info.port_name == bd)
						ns++;
					break;
				case MLX5_PHYS_PORT_NAME_TYPE_PFHPF:
					/* Fallthrough */
				case MLX5_PHYS_PORT_NAME_TYPE_PFVF:
					if (list[ns].info.pf_num == bd)
						ns++;
					break;
				default:
					break;
				}
				continue;
			}
			if (!ret && (list[ns].info.representor ^
				     list[ns].info.master))
				ns++;
		}
		if (!ns) {
			DRV_LOG(ERR,
				"unable to recognize master/representors"
				" on the IB device with multiple ports");
			rte_errno = ENOENT;
			ret = -rte_errno;
			goto exit;
		}
	} else {
		/*
		 * The existence of several matching entries (nd > 1) means
		 * port representors have been instantiated. No existing Verbs
		 * call nor sysfs entries can tell them apart, this can only
		 * be done through Netlink calls assuming kernel drivers are
		 * recent enough to support them.
		 *
		 * In the event of identification failure through Netlink,
		 * try again through sysfs, then:
		 *
		 * 1. A single IB device matches (nd == 1) with single
		 *    port (np=0/1) and is not a representor, assume
		 *    no switch support.
		 *
		 * 2. Otherwise no safe assumptions can be made;
		 *    complain louder and bail out.
		 */
		for (i = 0; i != nd; ++i) {
			memset(&list[ns].info, 0, sizeof(list[ns].info));
			list[ns].max_port = 1;
			list[ns].phys_port = 1;
			list[ns].phys_dev = ibv_match[i];
			list[ns].eth_dev = NULL;
			list[ns].pci_dev = pci_dev;
			list[ns].pf_bond = -1;
			list[ns].ifindex = 0;
			if (nl_rdma >= 0)
				list[ns].ifindex = mlx5_nl_ifindex
				(nl_rdma,
				mlx5_os_get_dev_device_name
						(list[ns].phys_dev), 1);
			if (!list[ns].ifindex) {
				char ifname[IF_NAMESIZE];

				/*
				 * Netlink failed, it may happen with old
				 * ib_core kernel driver (before 4.16).
				 * We can assume there is old driver because
				 * here we are processing single ports IB
				 * devices. Let's try sysfs to retrieve
				 * the ifindex. The method works for
				 * master device only.
				 */
				if (nd > 1) {
					/*
					 * Multiple devices found, assume
					 * representors, can not distinguish
					 * master/representor and retrieve
					 * ifindex via sysfs.
					 */
					continue;
				}
				ret = mlx5_get_ifname_sysfs
					(ibv_match[i]->ibdev_path, ifname);
				if (!ret)
					list[ns].ifindex =
						if_nametoindex(ifname);
				if (!list[ns].ifindex) {
					/*
					 * No network interface index found
					 * for the specified device, it means
					 * there it is neither representor
					 * nor master.
					 */
					continue;
				}
			}
			ret = -1;
			if (nl_route >= 0)
				ret = mlx5_nl_switch_info
					       (nl_route,
						list[ns].ifindex,
						&list[ns].info);
			if (ret || (!list[ns].info.representor &&
				    !list[ns].info.master)) {
				/*
				 * We failed to recognize representors with
				 * Netlink, let's try to perform the task
				 * with sysfs.
				 */
				ret =  mlx5_sysfs_switch_info
						(list[ns].ifindex,
						 &list[ns].info);
			}
			if (!ret && (list[ns].info.representor ^
				     list[ns].info.master)) {
				ns++;
			} else if ((nd == 1) &&
				   !list[ns].info.representor &&
				   !list[ns].info.master) {
				/*
				 * Single IB device with
				 * one physical port and
				 * attached network device.
				 * May be SRIOV is not enabled
				 * or there is no representors.
				 */
				DRV_LOG(INFO, "no E-Switch support detected");
				ns++;
				break;
			}
		}
		if (!ns) {
			DRV_LOG(ERR,
				"unable to recognize master/representors"
				" on the multiple IB devices");
			rte_errno = ENOENT;
			ret = -rte_errno;
			goto exit;
		}
	}
	MLX5_ASSERT(ns);
	/*
	 * Sort list to probe devices in natural order for users convenience
	 * (i.e. master first, then representors from lowest to highest ID).
	 */
	qsort(list, ns, sizeof(*list), mlx5_dev_spawn_data_cmp);
	/* Device specific configuration. */
	switch (pci_dev->id.device_id) {
	case PCI_DEVICE_ID_MELLANOX_CONNECTX4VF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX4LXVF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5VF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5EXVF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5BFVF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX6VF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX6DXVF:
		dev_config_vf = 1;
		break;
	default:
		dev_config_vf = 0;
		break;
	}
	for (i = 0; i != ns; ++i) {
		uint32_t restore;

		/* Default configuration. */
		memset(&dev_config, 0, sizeof(struct mlx5_dev_config));
		dev_config.vf = dev_config_vf;
		dev_config.mps = MLX5_ARG_UNSET;
		dev_config.dbnc = MLX5_ARG_UNSET;
		dev_config.rx_vec_en = 1;
		dev_config.txq_inline_max = MLX5_ARG_UNSET;
		dev_config.txq_inline_min = MLX5_ARG_UNSET;
		dev_config.txq_inline_mpw = MLX5_ARG_UNSET;
		dev_config.txqs_inline = MLX5_ARG_UNSET;
		dev_config.vf_nl_en = 1;
		dev_config.mr_ext_memseg_en = 1;
		dev_config.mprq.max_memcpy_len = MLX5_MPRQ_MEMCPY_DEFAULT_LEN;
		dev_config.mprq.min_rxqs_num = MLX5_MPRQ_MIN_RXQS;
		dev_config.dv_esw_en = 1;
		dev_config.dv_flow_en = 1;
		dev_config.decap_en = 1;
		dev_config.log_hp_size = MLX5_ARG_UNSET;
		list[i].eth_dev = mlx5_dev_spawn(&pci_dev->device,
						 &list[i],
						 &dev_config);
		if (!list[i].eth_dev) {
			if (rte_errno != EBUSY && rte_errno != EEXIST)
				break;
			/* Device is disabled or already spawned. Ignore it. */
			continue;
		}
		restore = list[i].eth_dev->data->dev_flags;
		rte_eth_copy_pci_info(list[i].eth_dev, pci_dev);
		/* Restore non-PCI flags cleared by the above call. */
		list[i].eth_dev->data->dev_flags |= restore;
		rte_eth_dev_probing_finish(list[i].eth_dev);
	}
	if (i != ns) {
		DRV_LOG(ERR,
			"probe of PCI device " PCI_PRI_FMT " aborted after"
			" encountering an error: %s",
			pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function,
			strerror(rte_errno));
		ret = -rte_errno;
		/* Roll back. */
		while (i--) {
			if (!list[i].eth_dev)
				continue;
			mlx5_dev_close(list[i].eth_dev);
			/* mac_addrs must not be freed because in dev_private */
			list[i].eth_dev->data->mac_addrs = NULL;
			claim_zero(rte_eth_dev_release_port(list[i].eth_dev));
		}
		/* Restore original error. */
		rte_errno = -ret;
	} else {
		ret = 0;
	}
exit:
	/*
	 * Do the routine cleanup:
	 * - close opened Netlink sockets
	 * - free allocated spawn data array
	 * - free the Infiniband device list
	 */
	if (nl_rdma >= 0)
		close(nl_rdma);
	if (nl_route >= 0)
		close(nl_route);
	if (list)
		mlx5_free(list);
	MLX5_ASSERT(ibv_list);
	mlx5_glue->free_device_list(ibv_list);
	return ret;
}

static int
mlx5_config_doorbell_mapping_env(const struct mlx5_dev_config *config)
{
	char *env;
	int value;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	/* Get environment variable to store. */
	env = getenv(MLX5_SHUT_UP_BF);
	value = env ? !!strcmp(env, "0") : MLX5_ARG_UNSET;
	if (config->dbnc == MLX5_ARG_UNSET)
		setenv(MLX5_SHUT_UP_BF, MLX5_SHUT_UP_BF_DEFAULT, 1);
	else
		setenv(MLX5_SHUT_UP_BF,
		       config->dbnc == MLX5_TXDB_NCACHED ? "1" : "0", 1);
	return value;
}

static void
mlx5_restore_doorbell_mapping_env(int value)
{
	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	/* Restore the original environment variable state. */
	if (value == MLX5_ARG_UNSET)
		unsetenv(MLX5_SHUT_UP_BF);
	else
		setenv(MLX5_SHUT_UP_BF, value ? "1" : "0", 1);
}

/**
 * Extract pdn of PD object using DV API.
 *
 * @param[in] pd
 *   Pointer to the verbs PD object.
 * @param[out] pdn
 *   Pointer to the PD object number variable.
 *
 * @return
 *   0 on success, error value otherwise.
 */
int
mlx5_os_get_pdn(void *pd, uint32_t *pdn)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	struct mlx5dv_obj obj;
	struct mlx5dv_pd pd_info;
	int ret = 0;

	obj.pd.in = pd;
	obj.pd.out = &pd_info;
	ret = mlx5_glue->dv_init_obj(&obj, MLX5DV_OBJ_PD);
	if (ret) {
		DRV_LOG(DEBUG, "Fail to get PD object info");
		return ret;
	}
	*pdn = pd_info.pdn;
	return 0;
#else
	(void)pd;
	(void)pdn;
	return -ENOTSUP;
#endif /* HAVE_IBV_FLOW_DV_SUPPORT */
}

/**
 * Function API to open IB device.
 *
 * This function calls the Linux glue APIs to open a device.
 *
 * @param[in] spawn
 *   Pointer to the IB device attributes (name, port, etc).
 * @param[out] config
 *   Pointer to device configuration structure.
 * @param[out] sh
 *   Pointer to shared context structure.
 *
 * @return
 *   0 on success, a positive error value otherwise.
 */
int
mlx5_os_open_device(const struct mlx5_dev_spawn_data *spawn,
		     const struct mlx5_dev_config *config,
		     struct mlx5_dev_ctx_shared *sh)
{
	int dbmap_env;
	int err = 0;

	sh->numa_node = spawn->pci_dev->device.numa_node;
	pthread_mutex_init(&sh->txpp.mutex, NULL);
	/*
	 * Configure environment variable "MLX5_BF_SHUT_UP"
	 * before the device creation. The rdma_core library
	 * checks the variable at device creation and
	 * stores the result internally.
	 */
	dbmap_env = mlx5_config_doorbell_mapping_env(config);
	/* Try to open IB device with DV first, then usual Verbs. */
	errno = 0;
	sh->ctx = mlx5_glue->dv_open_device(spawn->phys_dev);
	if (sh->ctx) {
		sh->devx = 1;
		DRV_LOG(DEBUG, "DevX is supported");
		/* The device is created, no need for environment. */
		mlx5_restore_doorbell_mapping_env(dbmap_env);
	} else {
		/* The environment variable is still configured. */
		sh->ctx = mlx5_glue->open_device(spawn->phys_dev);
		err = errno ? errno : ENODEV;
		/*
		 * The environment variable is not needed anymore,
		 * all device creation attempts are completed.
		 */
		mlx5_restore_doorbell_mapping_env(dbmap_env);
		if (!sh->ctx)
			return err;
		DRV_LOG(DEBUG, "DevX is NOT supported");
		err = 0;
	}
	return err;
}

/**
 * Install shared asynchronous device events handler.
 * This function is implemented to support event sharing
 * between multiple ports of single IB device.
 *
 * @param sh
 *   Pointer to mlx5_dev_ctx_shared object.
 */
void
mlx5_os_dev_shared_handler_install(struct mlx5_dev_ctx_shared *sh)
{
	int ret;
	int flags;

	sh->intr_handle.fd = -1;
	flags = fcntl(((struct ibv_context *)sh->ctx)->async_fd, F_GETFL);
	ret = fcntl(((struct ibv_context *)sh->ctx)->async_fd,
		    F_SETFL, flags | O_NONBLOCK);
	if (ret) {
		DRV_LOG(INFO, "failed to change file descriptor async event"
			" queue");
	} else {
		sh->intr_handle.fd = ((struct ibv_context *)sh->ctx)->async_fd;
		sh->intr_handle.type = RTE_INTR_HANDLE_EXT;
		if (rte_intr_callback_register(&sh->intr_handle,
					mlx5_dev_interrupt_handler, sh)) {
			DRV_LOG(INFO, "Fail to install the shared interrupt.");
			sh->intr_handle.fd = -1;
		}
	}
	if (sh->devx) {
#ifdef HAVE_IBV_DEVX_ASYNC
		sh->intr_handle_devx.fd = -1;
		sh->devx_comp =
			(void *)mlx5_glue->devx_create_cmd_comp(sh->ctx);
		struct mlx5dv_devx_cmd_comp *devx_comp = sh->devx_comp;
		if (!devx_comp) {
			DRV_LOG(INFO, "failed to allocate devx_comp.");
			return;
		}
		flags = fcntl(devx_comp->fd, F_GETFL);
		ret = fcntl(devx_comp->fd, F_SETFL, flags | O_NONBLOCK);
		if (ret) {
			DRV_LOG(INFO, "failed to change file descriptor"
				" devx comp");
			return;
		}
		sh->intr_handle_devx.fd = devx_comp->fd;
		sh->intr_handle_devx.type = RTE_INTR_HANDLE_EXT;
		if (rte_intr_callback_register(&sh->intr_handle_devx,
					mlx5_dev_interrupt_handler_devx, sh)) {
			DRV_LOG(INFO, "Fail to install the devx shared"
				" interrupt.");
			sh->intr_handle_devx.fd = -1;
		}
#endif /* HAVE_IBV_DEVX_ASYNC */
	}
}

/**
 * Uninstall shared asynchronous device events handler.
 * This function is implemented to support event sharing
 * between multiple ports of single IB device.
 *
 * @param dev
 *   Pointer to mlx5_dev_ctx_shared object.
 */
void
mlx5_os_dev_shared_handler_uninstall(struct mlx5_dev_ctx_shared *sh)
{
	if (sh->intr_handle.fd >= 0)
		mlx5_intr_callback_unregister(&sh->intr_handle,
					      mlx5_dev_interrupt_handler, sh);
#ifdef HAVE_IBV_DEVX_ASYNC
	if (sh->intr_handle_devx.fd >= 0)
		rte_intr_callback_unregister(&sh->intr_handle_devx,
				  mlx5_dev_interrupt_handler_devx, sh);
	if (sh->devx_comp)
		mlx5_glue->devx_destroy_cmd_comp(sh->devx_comp);
#endif
}

/**
 * Read statistics by a named counter.
 *
 * @param[in] priv
 *   Pointer to the private device data structure.
 * @param[in] ctr_name
 *   Pointer to the name of the statistic counter to read
 * @param[out] stat
 *   Pointer to read statistic value.
 * @return
 *   0 on success and stat is valud, 1 if failed to read the value
 *   rte_errno is set.
 *
 */
int
mlx5_os_read_dev_stat(struct mlx5_priv *priv, const char *ctr_name,
		      uint64_t *stat)
{
	int fd;

	if (priv->sh) {
		MKSTR(path, "%s/ports/%d/hw_counters/%s",
		      priv->sh->ibdev_path,
		      priv->dev_port,
		      ctr_name);
		fd = open(path, O_RDONLY);
		/*
		 * in switchdev the file location is not per port
		 * but rather in <ibdev_path>/hw_counters/<file_name>.
		 */
		if (fd == -1) {
			MKSTR(path1, "%s/hw_counters/%s",
			      priv->sh->ibdev_path,
			      ctr_name);
			fd = open(path1, O_RDONLY);
		}
		if (fd != -1) {
			char buf[21] = {'\0'};
			ssize_t n = read(fd, buf, sizeof(buf));

			close(fd);
			if (n != -1) {
				*stat = strtoull(buf, NULL, 10);
				return 0;
			}
		}
	}
	*stat = 0;
	return 1;
}

/**
 * Set the reg_mr and dereg_mr call backs
 *
 * @param reg_mr_cb[out]
 *   Pointer to reg_mr func
 * @param dereg_mr_cb[out]
 *   Pointer to dereg_mr func
 *
 */
void
mlx5_os_set_reg_mr_cb(mlx5_reg_mr_t *reg_mr_cb,
		      mlx5_dereg_mr_t *dereg_mr_cb)
{
	*reg_mr_cb = mlx5_verbs_ops.reg_mr;
	*dereg_mr_cb = mlx5_verbs_ops.dereg_mr;
}

/**
 * Remove a MAC address from device
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param index
 *   MAC address index.
 */
void
mlx5_os_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const int vf = priv->config.vf;

	if (vf)
		mlx5_nl_mac_addr_remove(priv->nl_socket_route,
					mlx5_ifindex(dev), priv->mac_own,
					&dev->data->mac_addrs[index], index);
}

/**
 * Adds a MAC address to the device
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param mac_addr
 *   MAC address to register.
 * @param index
 *   MAC address index.
 *
 * @return
 *   0 on success, a negative errno value otherwise
 */
int
mlx5_os_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac,
		     uint32_t index)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const int vf = priv->config.vf;
	int ret = 0;

	if (vf)
		ret = mlx5_nl_mac_addr_add(priv->nl_socket_route,
					   mlx5_ifindex(dev), priv->mac_own,
					   mac, index);
	return ret;
}

/**
 * Modify a VF MAC address
 *
 * @param priv
 *   Pointer to device private data.
 * @param mac_addr
 *   MAC address to modify into.
 * @param iface_idx
 *   Net device interface index
 * @param vf_index
 *   VF index
 *
 * @return
 *   0 on success, a negative errno value otherwise
 */
int
mlx5_os_vf_mac_addr_modify(struct mlx5_priv *priv,
			   unsigned int iface_idx,
			   struct rte_ether_addr *mac_addr,
			   int vf_index)
{
	return mlx5_nl_vf_mac_addr_modify
		(priv->nl_socket_route, iface_idx, mac_addr, vf_index);
}

/**
 * Set device promiscuous mode
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param enable
 *   0 - promiscuous is disabled, otherwise - enabled
 *
 * @return
 *   0 on success, a negative error value otherwise
 */
int
mlx5_os_set_promisc(struct rte_eth_dev *dev, int enable)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	return mlx5_nl_promisc(priv->nl_socket_route,
			       mlx5_ifindex(dev), !!enable);
}

/**
 * Set device promiscuous mode
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param enable
 *   0 - all multicase is disabled, otherwise - enabled
 *
 * @return
 *   0 on success, a negative error value otherwise
 */
int
mlx5_os_set_allmulti(struct rte_eth_dev *dev, int enable)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	return mlx5_nl_allmulti(priv->nl_socket_route,
				mlx5_ifindex(dev), !!enable);
}

const struct eth_dev_ops mlx5_os_dev_ops = {
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
	.fw_version_get = mlx5_fw_version_get,
	.dev_infos_get = mlx5_dev_infos_get,
	.read_clock = mlx5_txpp_read_clock,
	.dev_supported_ptypes_get = mlx5_dev_supported_ptypes_get,
	.vlan_filter_set = mlx5_vlan_filter_set,
	.rx_queue_setup = mlx5_rx_queue_setup,
	.rx_hairpin_queue_setup = mlx5_rx_hairpin_queue_setup,
	.tx_queue_setup = mlx5_tx_queue_setup,
	.tx_hairpin_queue_setup = mlx5_tx_hairpin_queue_setup,
	.rx_queue_release = mlx5_rx_queue_release,
	.tx_queue_release = mlx5_tx_queue_release,
	.rx_queue_start = mlx5_rx_queue_start,
	.rx_queue_stop = mlx5_rx_queue_stop,
	.tx_queue_start = mlx5_tx_queue_start,
	.tx_queue_stop = mlx5_tx_queue_stop,
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
	.rxq_info_get = mlx5_rxq_info_get,
	.txq_info_get = mlx5_txq_info_get,
	.rx_burst_mode_get = mlx5_rx_burst_mode_get,
	.tx_burst_mode_get = mlx5_tx_burst_mode_get,
	.rx_queue_count = mlx5_rx_queue_count,
	.rx_queue_intr_enable = mlx5_rx_intr_enable,
	.rx_queue_intr_disable = mlx5_rx_intr_disable,
	.is_removed = mlx5_is_removed,
	.udp_tunnel_port_add  = mlx5_udp_tunnel_port_add,
	.get_module_info = mlx5_get_module_info,
	.get_module_eeprom = mlx5_get_module_eeprom,
	.hairpin_cap_get = mlx5_hairpin_cap_get,
	.mtr_ops_get = mlx5_flow_meter_ops_get,
};

/* Available operations from secondary process. */
const struct eth_dev_ops mlx5_os_dev_sec_ops = {
	.stats_get = mlx5_stats_get,
	.stats_reset = mlx5_stats_reset,
	.xstats_get = mlx5_xstats_get,
	.xstats_reset = mlx5_xstats_reset,
	.xstats_get_names = mlx5_xstats_get_names,
	.fw_version_get = mlx5_fw_version_get,
	.dev_infos_get = mlx5_dev_infos_get,
	.read_clock = mlx5_txpp_read_clock,
	.rx_queue_start = mlx5_rx_queue_start,
	.rx_queue_stop = mlx5_rx_queue_stop,
	.tx_queue_start = mlx5_tx_queue_start,
	.tx_queue_stop = mlx5_tx_queue_stop,
	.rx_descriptor_status = mlx5_rx_descriptor_status,
	.tx_descriptor_status = mlx5_tx_descriptor_status,
	.rxq_info_get = mlx5_rxq_info_get,
	.txq_info_get = mlx5_txq_info_get,
	.rx_burst_mode_get = mlx5_rx_burst_mode_get,
	.tx_burst_mode_get = mlx5_tx_burst_mode_get,
	.get_module_info = mlx5_get_module_info,
	.get_module_eeprom = mlx5_get_module_eeprom,
};

/* Available operations in flow isolated mode. */
const struct eth_dev_ops mlx5_os_dev_ops_isolate = {
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
	.fw_version_get = mlx5_fw_version_get,
	.dev_infos_get = mlx5_dev_infos_get,
	.read_clock = mlx5_txpp_read_clock,
	.dev_supported_ptypes_get = mlx5_dev_supported_ptypes_get,
	.vlan_filter_set = mlx5_vlan_filter_set,
	.rx_queue_setup = mlx5_rx_queue_setup,
	.rx_hairpin_queue_setup = mlx5_rx_hairpin_queue_setup,
	.tx_queue_setup = mlx5_tx_queue_setup,
	.tx_hairpin_queue_setup = mlx5_tx_hairpin_queue_setup,
	.rx_queue_release = mlx5_rx_queue_release,
	.tx_queue_release = mlx5_tx_queue_release,
	.rx_queue_start = mlx5_rx_queue_start,
	.rx_queue_stop = mlx5_rx_queue_stop,
	.tx_queue_start = mlx5_tx_queue_start,
	.tx_queue_stop = mlx5_tx_queue_stop,
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
	.rxq_info_get = mlx5_rxq_info_get,
	.txq_info_get = mlx5_txq_info_get,
	.rx_burst_mode_get = mlx5_rx_burst_mode_get,
	.tx_burst_mode_get = mlx5_tx_burst_mode_get,
	.rx_queue_intr_enable = mlx5_rx_intr_enable,
	.rx_queue_intr_disable = mlx5_rx_intr_disable,
	.is_removed = mlx5_is_removed,
	.get_module_info = mlx5_get_module_info,
	.get_module_eeprom = mlx5_get_module_eeprom,
	.hairpin_cap_get = mlx5_hairpin_cap_get,
	.mtr_ops_get = mlx5_flow_meter_ops_get,
};
