/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <errno.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <rte_windows.h>
#include <ethdev_pci.h>

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
#include "mlx5_rx.h"
#include "mlx5_tx.h"
#include "mlx5_autoconf.h"
#include "mlx5_mr.h"
#include "mlx5_flow.h"
#include "mlx5_devx.h"

static const char *MZ_MLX5_PMD_SHARED_DATA = "mlx5_pmd_shared_data";

/* Spinlock for mlx5_shared_data allocation. */
static rte_spinlock_t mlx5_shared_data_lock = RTE_SPINLOCK_INITIALIZER;

/* rte flow indexed pool configuration. */
static struct mlx5_indexed_pool_config icfg[] = {
	{
		.size = sizeof(struct rte_flow),
		.trunk_size = 64,
		.need_lock = 1,
		.release_mem_en = 0,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.per_core_cache = 0,
		.type = "ctl_flow_ipool",
	},
	{
		.size = sizeof(struct rte_flow),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 1,
		.release_mem_en = 0,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.per_core_cache = 1 << 14,
		.type = "rte_flow_ipool",
	},
	{
		.size = sizeof(struct rte_flow),
		.trunk_size = 64,
		.grow_trunk = 3,
		.grow_shift = 2,
		.need_lock = 1,
		.release_mem_en = 0,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.per_core_cache = 0,
		.type = "mcp_flow_ipool",
	},
};

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
	if (mlx5_init_shared_data())
		return -rte_errno;
	return 0;
}

/**
 * Get mlx5 device attributes.
 *
 * @param ctx
 *   Pointer to device context.
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
	struct mlx5_context *mlx5_ctx;
	struct mlx5_hca_attr hca_attr;
	void *pv_iseg = NULL;
	u32 cb_iseg = 0;
	int err = 0;

	if (!ctx)
		return -EINVAL;
	mlx5_ctx = (struct mlx5_context *)ctx;
	memset(device_attr, 0, sizeof(*device_attr));
	err = mlx5_devx_cmd_query_hca_attr(mlx5_ctx, &hca_attr);
	if (err) {
		DRV_LOG(ERR, "Failed to get device hca_cap");
		return err;
	}
	device_attr->max_cq = 1 << hca_attr.log_max_cq;
	device_attr->max_qp = 1 << hca_attr.log_max_qp;
	device_attr->max_qp_wr = 1 << hca_attr.log_max_qp_sz;
	device_attr->max_cqe = 1 << hca_attr.log_max_cq_sz;
	device_attr->max_mr = 1 << hca_attr.log_max_mrw_sz;
	device_attr->max_pd = 1 << hca_attr.log_max_pd;
	device_attr->max_srq = 1 << hca_attr.log_max_srq;
	device_attr->max_srq_wr = 1 << hca_attr.log_max_srq_sz;
	if (hca_attr.rss_ind_tbl_cap) {
		device_attr->max_rwq_indirection_table_size =
			1 << hca_attr.rss_ind_tbl_cap;
	}
	pv_iseg = mlx5_glue->query_hca_iseg(mlx5_ctx, &cb_iseg);
	if (pv_iseg == NULL) {
		DRV_LOG(ERR, "Failed to get device hca_iseg");
		return errno;
	}
	if (!err) {
		snprintf(device_attr->fw_ver, 64, "%x.%x.%04x",
			MLX5_GET(initial_seg, pv_iseg, fw_rev_major),
			MLX5_GET(initial_seg, pv_iseg, fw_rev_minor),
			MLX5_GET(initial_seg, pv_iseg, fw_rev_subminor));
	}
	return err;
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
	int err = 0;

	if (!sh->flow_tbls)
		err = mlx5_alloc_table_hash_list(priv);
	else
		DRV_LOG(DEBUG, "sh->flow_tbls[%p] already created, reuse",
			(void *)sh->flow_tbls);
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
	mlx5_free_table_hash_list(priv);
}

/**
 * Set the completion channel file descriptor interrupt as non-blocking.
 * Currently it has no support under Windows.
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
	(void)fd;
	DRV_LOG(WARNING, "%s: is not supported", __func__);
	return -ENOTSUP;
}

/**
 * Function API open device under Windows
 *
 * This function calls the Windows glue APIs to open a device.
 *
 * @param[in] spawn
 *   Pointer to the device attributes (name, port, etc).
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
	RTE_SET_USED(config);
	int err = 0;
	struct mlx5_context *mlx5_ctx;

	pthread_mutex_init(&sh->txpp.mutex, NULL);
	/* Set numa node from pci probe */
	sh->numa_node = spawn->pci_dev->device.numa_node;

	/* Try to open device with DevX */
	rte_errno = 0;
	sh->ctx = mlx5_glue->open_device(spawn->phys_dev);
	if (!sh->ctx) {
		DRV_LOG(ERR, "open_device failed");
		err = errno;
		return err;
	}
	sh->devx = 1;
	mlx5_ctx = (struct mlx5_context *)sh->ctx;
	err = mlx5_glue->query_device(spawn->phys_dev, &mlx5_ctx->mlx5_dev);
	if (err)
		DRV_LOG(ERR, "Failed to query device context fields.");
	return err;
}

/**
 * DV flow counter mode detect and config.
 *
 * @param dev
 *   Pointer to rte_eth_dev structure.
 *
 */
static void
mlx5_flow_counter_mode_config(struct rte_eth_dev *dev __rte_unused)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	bool fallback;

#ifndef HAVE_IBV_DEVX_ASYNC
	fallback = true;
#else
	fallback = false;
	if (!priv->config.devx || !priv->config.dv_flow_en ||
	    !priv->config.hca_attr.flow_counters_dump ||
	    !(priv->config.hca_attr.flow_counter_bulk_alloc_bitmap & 0x4) ||
	    (mlx5_flow_dv_discover_counter_offset_support(dev) == -ENOTSUP))
		fallback = true;
#endif
	if (fallback)
		DRV_LOG(INFO, "Use fall-back DV counter management. Flow "
			"counter dump:%d, bulk_alloc_bitmap:0x%hhx.",
			priv->config.hca_attr.flow_counters_dump,
			priv->config.hca_attr.flow_counter_bulk_alloc_bitmap);
	/* Initialize fallback mode only on the port initializes sh. */
	if (sh->refcnt == 1)
		sh->cmng.counter_fallback = fallback;
	else if (fallback != sh->cmng.counter_fallback)
		DRV_LOG(WARNING, "Port %d in sh has different fallback mode "
			"with others:%d.", PORT_ID(priv), fallback);
#endif
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
 *   EEXIST: device is already spawned
 */
static struct rte_eth_dev *
mlx5_dev_spawn(struct rte_device *dpdk_dev,
	       struct mlx5_dev_spawn_data *spawn,
	       struct mlx5_dev_config *config)
{
	const struct mlx5_switch_info *switch_info = &spawn->info;
	struct mlx5_dev_ctx_shared *sh = NULL;
	struct mlx5_dev_attr device_attr;
	struct rte_eth_dev *eth_dev = NULL;
	struct mlx5_priv *priv = NULL;
	int err = 0;
	unsigned int cqe_comp;
	struct rte_ether_addr mac;
	char name[RTE_ETH_NAME_MAX_LEN];
	int own_domain_id = 0;
	uint16_t port_id;
	int i;

	/* Build device name. */
	strlcpy(name, dpdk_dev->name, sizeof(name));
	/* check if the device is already spawned */
	if (rte_eth_dev_get_port_by_name(name, &port_id) == 0) {
		rte_errno = EEXIST;
		return NULL;
	}
	DRV_LOG(DEBUG, "naming Ethernet device \"%s\"", name);
	/*
	 * Some parameters are needed in advance to create device context. We
	 * process the devargs here to get ones, and later process devargs
	 * again to override some hardware settings.
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
	/* Initialize the shutdown event in mlx5_dev_spawn to
	 * support mlx5_is_removed for Windows.
	 */
	err = mlx5_glue->devx_init_showdown_event(sh->ctx);
	if (err) {
		DRV_LOG(ERR, "failed to init showdown event: %s",
			strerror(errno));
		goto error;
	}
	DRV_LOG(DEBUG, "MPW isn't supported");
	mlx5_os_get_dev_attr(sh->ctx, &device_attr);
	config->swp = 0;
	config->ind_table_max_size =
		sh->device_attr.max_rwq_indirection_table_size;
	cqe_comp = 0;
	config->cqe_comp = cqe_comp;
	DRV_LOG(DEBUG, "tunnel offloading is not supported");
	config->tunnel_en = 0;
	DRV_LOG(DEBUG, "MPLS over GRE/UDP tunnel offloading is no supported");
	config->mpls_en = 0;
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
	priv->representor = !!switch_info->representor;
	priv->master = !!switch_info->master;
	priv->domain_id = RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID;
	priv->vport_meta_tag = 0;
	priv->vport_meta_mask = 0;
	priv->pf_bond = spawn->pf_bond;
	priv->vport_id = -1;
	/* representor_id field keeps the unmodified VF index. */
	priv->representor_id = -1;
	/*
	 * Look for sibling devices in order to reuse their switch domain
	 * if any, otherwise allocate one.
	 */
	MLX5_ETH_FOREACH_DEV(port_id, dpdk_dev) {
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
	err = mlx5_dev_check_sibling_config(priv, config, dpdk_dev);
	if (err)
		goto error;
	DRV_LOG(DEBUG, "counters are not supported");
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
	DRV_LOG(DEBUG, "VLAN stripping is %ssupported",
		(config->hw_vlan_strip ? "" : "not "));
	if (config->hw_padding) {
		DRV_LOG(DEBUG, "Rx end alignment padding isn't supported");
		config->hw_padding = 0;
	}
	if (config->tso)
		config->tso_max_payload_sz = sh->device_attr.max_tso;
	DRV_LOG(DEBUG, "%sMPS is %s.",
		config->mps == MLX5_MPW_ENHANCED ? "enhanced " :
		config->mps == MLX5_MPW ? "legacy " : "",
		config->mps != MLX5_MPW_DISABLED ? "enabled" : "disabled");
	if (config->cqe_comp && !cqe_comp) {
		DRV_LOG(WARNING, "Rx CQE compression isn't supported.");
		config->cqe_comp = 0;
	}
	if (config->devx) {
		err = mlx5_devx_cmd_query_hca_attr(sh->ctx, &config->hca_attr);
		if (err) {
			err = -err;
			goto error;
		}
		/* Check relax ordering support. */
		sh->cmng.relaxed_ordering_read = 0;
		sh->cmng.relaxed_ordering_write = 0;
		if (!haswell_broadwell_cpu) {
			sh->cmng.relaxed_ordering_write =
				config->hca_attr.relaxed_ordering_write;
			sh->cmng.relaxed_ordering_read =
				config->hca_attr.relaxed_ordering_read;
		}
		config->hw_csum = config->hca_attr.csum_cap;
		DRV_LOG(DEBUG, "checksum offloading is %ssupported",
		    (config->hw_csum ? "" : "not "));
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
		sh->rq_ts_format = config->hca_attr.rq_ts_format;
		sh->sq_ts_format = config->hca_attr.sq_ts_format;
		sh->qp_ts_format = config->hca_attr.qp_ts_format;
	}
	if (config->mprq.enabled) {
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
	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;
	/* Configure the first MAC address by default. */
	if (mlx5_get_mac(eth_dev, &mac.addr_bytes)) {
		DRV_LOG(ERR,
			"port %u cannot get MAC address, is mlx5_en"
			" loaded? (errno: %s).",
			eth_dev->data->port_id, strerror(rte_errno));
		err = ENODEV;
		goto error;
	}
	DRV_LOG(INFO,
		"port %u MAC address is " RTE_ETHER_ADDR_PRT_FMT,
		eth_dev->data->port_id, RTE_ETHER_ADDR_BYTES(&mac));
#ifdef RTE_LIBRTE_MLX5_DEBUG
	{
		char ifname[MLX5_NAMESIZE];

		if (mlx5_get_ifname(eth_dev, &ifname) == 0)
			DRV_LOG(DEBUG, "port %u ifname is \"%s\"",
				eth_dev->data->port_id, ifname);
		else
			DRV_LOG(DEBUG, "port %u ifname is unknown.",
				eth_dev->data->port_id);
	}
#endif
	/* Get actual MTU if possible. */
	err = mlx5_get_mtu(eth_dev, &priv->mtu);
	if (err) {
		err = rte_errno;
		goto error;
	}
	DRV_LOG(DEBUG, "port %u MTU is %u.", eth_dev->data->port_id,
		priv->mtu);
	/* Initialize burst functions to prevent crashes before link-up. */
	eth_dev->rx_pkt_burst = removed_rx_burst;
	eth_dev->tx_pkt_burst = removed_tx_burst;
	eth_dev->dev_ops = &mlx5_dev_ops;
	eth_dev->rx_descriptor_status = mlx5_rx_descriptor_status;
	eth_dev->tx_descriptor_status = mlx5_tx_descriptor_status;
	eth_dev->rx_queue_count = mlx5_rx_queue_count;
	/* Register MAC address. */
	claim_zero(mlx5_mac_addr_add(eth_dev, &mac, 0, 0));
	priv->ctrl_flows = 0;
	TAILQ_INIT(&priv->flow_meters);
	priv->mtr_profile_tbl = mlx5_l3t_create(MLX5_L3T_TYPE_PTR);
	if (!priv->mtr_profile_tbl)
		goto error;
	/* Bring Ethernet device up. */
	DRV_LOG(DEBUG, "port %u forcing Ethernet interface up.",
		eth_dev->data->port_id);
	/* nl calls are unsupported - set to -1 not to fail on release */
	priv->nl_socket_rdma = -1;
	priv->nl_socket_route = -1;
	mlx5_set_link_up(eth_dev);
	/*
	 * Even though the interrupt handler is not installed yet,
	 * interrupts will still trigger on the async_fd from
	 * Verbs context returned by ibv_open_device().
	 */
	mlx5_link_update(eth_dev, 0);
	config->dv_esw_en = 0;
	/* Detect minimal data bytes to inline. */
	mlx5_set_min_inline(spawn, config);
	/* Store device configuration on private structure. */
	priv->config = *config;
	for (i = 0; i < MLX5_FLOW_TYPE_MAXI; i++) {
		icfg[i].release_mem_en = !!config->reclaim_mode;
		if (config->reclaim_mode)
			icfg[i].per_core_cache = 0;
		priv->flows[i] = mlx5_ipool_create(&icfg[i]);
		if (!priv->flows[i])
			goto error;
	}
	/* Create context for virtual machine VLAN workaround. */
	priv->vmwa_context = NULL;
	if (config->dv_flow_en) {
		err = mlx5_alloc_shared_dr(priv);
		if (err)
			goto error;
	}
	/* No supported flow priority number detection. */
	priv->config.flow_prio = -1;
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
			     "(no metadata reg_c[0] is available).",
			     priv->config.dv_xmeta_en);
			err = ENOTSUP;
			goto error;
	}
	priv->hrxqs = mlx5_list_create("hrxq", eth_dev, true,
		mlx5_hrxq_create_cb, mlx5_hrxq_match_cb,
		mlx5_hrxq_remove_cb, mlx5_hrxq_clone_cb,
		mlx5_hrxq_clone_free_cb);
	/* Query availability of metadata reg_c's. */
	err = mlx5_flow_discover_mreg_c(eth_dev);
	if (err < 0) {
		err = -err;
		goto error;
	}
	if (!mlx5_flow_ext_mreg_supported(eth_dev)) {
		DRV_LOG(DEBUG,
			"port %u extensive metadata register is not supported.",
			eth_dev->data->port_id);
		if (priv->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY) {
			DRV_LOG(ERR, "metadata mode %u is not supported "
				     "(no metadata registers available).",
				     priv->config.dv_xmeta_en);
			err = ENOTSUP;
			goto error;
		}
	}
	if (config->devx && config->dv_flow_en) {
		priv->obj_ops = devx_obj_ops;
	} else {
		DRV_LOG(ERR, "Flow mode %u is not supported "
				"(Windows flow must be DevX with DV flow enabled).",
				priv->config.dv_flow_en);
		err = ENOTSUP;
		goto error;
	}
	mlx5_flow_counter_mode_config(eth_dev);
	return eth_dev;
error:
	if (priv) {
		if (priv->mtr_profile_tbl)
			mlx5_l3t_destroy(priv->mtr_profile_tbl);
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
 * This function should share events between multiple ports of single IB
 * device.  Currently it has no support under Windows.
 *
 * @param sh
 *   Pointer to mlx5_dev_ctx_shared object.
 */
void
mlx5_os_dev_shared_handler_install(struct mlx5_dev_ctx_shared *sh)
{
	(void)sh;
	DRV_LOG(WARNING, "%s: is not supported", __func__);
}

/**
 * This function should share events between multiple ports of single IB
 * device.  Currently it has no support under Windows.
 *
 * @param dev
 *   Pointer to mlx5_dev_ctx_shared object.
 */
void
mlx5_os_dev_shared_handler_uninstall(struct mlx5_dev_ctx_shared *sh)
{
	(void)sh;
	DRV_LOG(WARNING, "%s: is not supported", __func__);
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
	RTE_SET_USED(priv);
	RTE_SET_USED(ctr_name);
	RTE_SET_USED(stat);
	DRV_LOG(WARNING, "%s: is not supported", __func__);
	return -ENOTSUP;
}

/**
 * Flush device MAC addresses
 * Currently it has no support under Windows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 */
void
mlx5_os_mac_addr_flush(struct rte_eth_dev *dev)
{
	(void)dev;
	DRV_LOG(WARNING, "%s: is not supported", __func__);
}

/**
 * Remove a MAC address from device
 * Currently it has no support under Windows.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param index
 *   MAC address index.
 */
void
mlx5_os_mac_addr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	(void)dev;
	(void)(index);
	DRV_LOG(WARNING, "%s: is not supported", __func__);
}

/**
 * Adds a MAC address to the device
 * Currently it has no support under Windows.
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
	(void)index;
	struct rte_ether_addr lmac;

	if (mlx5_get_mac(dev, &lmac.addr_bytes)) {
		DRV_LOG(ERR,
			"port %u cannot get MAC address, is mlx5_en"
			" loaded? (errno: %s)",
			dev->data->port_id, strerror(rte_errno));
		return rte_errno;
	}
	if (!rte_is_same_ether_addr(&lmac, mac)) {
		DRV_LOG(ERR,
			"adding new mac address to device is unsupported");
		return -ENOTSUP;
	}
	return 0;
}

/**
 * Modify a VF MAC address
 * Currently it has no support under Windows.
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
	(void)priv;
	(void)iface_idx;
	(void)mac_addr;
	(void)vf_index;
	DRV_LOG(WARNING, "%s: is not supported", __func__);
	return -ENOTSUP;
}

/**
 * Set device promiscuous mode
 * Currently it has no support under Windows.
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
	(void)dev;
	(void)enable;
	DRV_LOG(WARNING, "%s: is not supported", __func__);
	return -ENOTSUP;
}

/**
 * Set device allmulti mode
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
	(void)dev;
	(void)enable;
	DRV_LOG(WARNING, "%s: is not supported", __func__);
	return -ENOTSUP;
}

/**
 * Detect if a devx_device_bdf object has identical DBDF values to the
 * rte_pci_addr found in bus/pci probing
 *
 * @param[in] devx_bdf
 *   Pointer to the devx_device_bdf structure.
 * @param[in] addr
 *   Pointer to the rte_pci_addr structure.
 *
 * @return
 *   1 on Device match, 0 on mismatch.
 */
static int
mlx5_match_devx_bdf_to_addr(struct devx_device_bdf *devx_bdf,
			    struct rte_pci_addr *addr)
{
	if (addr->domain != (devx_bdf->bus_id >> 8) ||
	    addr->bus != (devx_bdf->bus_id & 0xff) ||
	    addr->devid != devx_bdf->dev_id ||
	    addr->function != devx_bdf->fnc_id) {
		return 0;
	}
	return 1;
}

/**
 * Detect if a devx_device_bdf object matches the rte_pci_addr
 * found in bus/pci probing
 * Compare both the Native/PF BDF and the raw_bdf representing a VF BDF.
 *
 * @param[in] devx_bdf
 *   Pointer to the devx_device_bdf structure.
 * @param[in] addr
 *   Pointer to the rte_pci_addr structure.
 *
 * @return
 *   1 on Device match, 0 on mismatch, rte_errno code on failure.
 */
static int
mlx5_match_devx_devices_to_addr(struct devx_device_bdf *devx_bdf,
				struct rte_pci_addr *addr)
{
	int err;
	struct devx_device mlx5_dev;

	if (mlx5_match_devx_bdf_to_addr(devx_bdf, addr))
		return 1;
	/**
	 * Didn't match on Native/PF BDF, could still
	 * Match a VF BDF, check it next
	 */
	err = mlx5_glue->query_device(devx_bdf, &mlx5_dev);
	if (err) {
		DRV_LOG(ERR, "query_device failed");
		rte_errno = err;
		return rte_errno;
	}
	if (mlx5_match_devx_bdf_to_addr(&mlx5_dev.raw_bdf, addr))
		return 1;
	return 0;
}

/**
 * DPDK callback to register a PCI device.
 *
 * This function spawns Ethernet devices out of a given device.
 *
 * @param[in] dev
 *   Pointer to the generic device.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_os_net_probe(struct rte_device *dev)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev);
	struct devx_device_bdf *devx_bdf_devs, *orig_devx_bdf_devs;
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
	 * Currently not supported.
	 * unsigned int np = 0;
	 */

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
	int ret, err;
	uint32_t restore;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		DRV_LOG(ERR, "Secondary process is not supported on Windows.");
		return -ENOTSUP;
	}
	ret = mlx5_init_once();
	if (ret) {
		DRV_LOG(ERR, "unable to init PMD global data: %s",
			strerror(rte_errno));
		return -rte_errno;
	}
	errno = 0;
	devx_bdf_devs = mlx5_glue->get_device_list(&ret);
	orig_devx_bdf_devs = devx_bdf_devs;
	if (!devx_bdf_devs) {
		rte_errno = errno ? errno : ENOSYS;
		DRV_LOG(ERR, "cannot list devices, is ib_uverbs loaded?");
		return -rte_errno;
	}
	/*
	 * First scan the list of all Infiniband devices to find
	 * matching ones, gathering into the list.
	 */
	struct devx_device_bdf *devx_bdf_match[ret + 1];

	while (ret-- > 0) {
		err = mlx5_match_devx_devices_to_addr(devx_bdf_devs,
		    &pci_dev->addr);
		if (!err) {
			devx_bdf_devs++;
			continue;
		}
		if (err != 1) {
			ret = -err;
			goto exit;
		}
		devx_bdf_match[nd++] = devx_bdf_devs;
	}
	devx_bdf_match[nd] = NULL;
	if (!nd) {
		/* No device matches, just complain and bail out. */
		DRV_LOG(WARNING,
			"no DevX device matches PCI device " PCI_PRI_FMT ","
			" is DevX Configured?",
			pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function);
		rte_errno = ENOENT;
		ret = -rte_errno;
		goto exit;
	}
	/*
	 * Now we can determine the maximal
	 * amount of devices to be spawned.
	 */
	list = mlx5_malloc(MLX5_MEM_ZERO,
			   sizeof(struct mlx5_dev_spawn_data),
			   RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (!list) {
		DRV_LOG(ERR, "spawn data array allocation failure");
		rte_errno = ENOMEM;
		ret = -rte_errno;
		goto exit;
	}
	memset(&list[ns].info, 0, sizeof(list[ns].info));
	list[ns].max_port = 1;
	list[ns].phys_port = 1;
	list[ns].phys_dev = devx_bdf_match[ns];
	list[ns].eth_dev = NULL;
	list[ns].pci_dev = pci_dev;
	list[ns].pf_bond = bd;
	list[ns].ifindex = -1; /* Spawn will assign */
	list[ns].info =
		(struct mlx5_switch_info){
			.master = 0,
			.representor = 0,
			.name_type = MLX5_PHYS_PORT_NAME_TYPE_UPLINK,
			.port_name = 0,
			.switch_id = 0,
		};
	/* Device specific configuration. */
	switch (pci_dev->id.device_id) {
	case PCI_DEVICE_ID_MELLANOX_CONNECTX4VF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX4LXVF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5VF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5EXVF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5BFVF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX6VF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTXVF:
		dev_config_vf = 1;
		break;
	default:
		dev_config_vf = 0;
		break;
	}
	/* Default configuration. */
	memset(&dev_config, 0, sizeof(struct mlx5_dev_config));
	dev_config.vf = dev_config_vf;
	dev_config.mps = 0;
	dev_config.dbnc = MLX5_ARG_UNSET;
	dev_config.rx_vec_en = 1;
	dev_config.txq_inline_max = MLX5_ARG_UNSET;
	dev_config.txq_inline_min = MLX5_ARG_UNSET;
	dev_config.txq_inline_mpw = MLX5_ARG_UNSET;
	dev_config.txqs_inline = MLX5_ARG_UNSET;
	dev_config.vf_nl_en = 0;
	dev_config.mr_ext_memseg_en = 1;
	dev_config.mprq.max_memcpy_len = MLX5_MPRQ_MEMCPY_DEFAULT_LEN;
	dev_config.mprq.min_rxqs_num = MLX5_MPRQ_MIN_RXQS;
	dev_config.dv_esw_en = 0;
	dev_config.dv_flow_en = 1;
	dev_config.decap_en = 0;
	dev_config.log_hp_size = MLX5_ARG_UNSET;
	list[ns].numa_node = pci_dev->device.numa_node;
	list[ns].eth_dev = mlx5_dev_spawn(&pci_dev->device,
					  &list[ns],
					  &dev_config);
	if (!list[ns].eth_dev)
		goto exit;
	restore = list[ns].eth_dev->data->dev_flags;
	rte_eth_copy_pci_info(list[ns].eth_dev, pci_dev);
	/* Restore non-PCI flags cleared by the above call. */
	list[ns].eth_dev->data->dev_flags |= restore;
	rte_eth_dev_probing_finish(list[ns].eth_dev);
	ret = 0;
exit:
	/*
	 * Do the routine cleanup:
	 * - free allocated spawn data array
	 * - free the device list
	 */
	if (list)
		mlx5_free(list);
	MLX5_ASSERT(orig_devx_bdf_devs);
	mlx5_glue->free_device_list(orig_devx_bdf_devs);
	return ret;
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
	*reg_mr_cb = mlx5_os_reg_mr;
	*dereg_mr_cb = mlx5_os_dereg_mr;
}

/**
 * Extract pdn of PD object using DevX
 *
 * @param[in] pd
 *   Pointer to the DevX PD object.
 * @param[out] pdn
 *   Pointer to the PD object number variable.
 *
 * @return
 *   0 on success, error value otherwise.
 */
int
mlx5_os_get_pdn(void *pd, uint32_t *pdn)
{
	if (!pd)
		return -EINVAL;

	*pdn = ((struct mlx5_pd *)pd)->pdn;
	return 0;
}

const struct mlx5_flow_driver_ops mlx5_flow_verbs_drv_ops = {0};
