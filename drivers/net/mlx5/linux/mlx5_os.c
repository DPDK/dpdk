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
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_bus_auxiliary.h>
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
#include "mlx5_rx.h"
#include "mlx5_tx.h"
#include "mlx5_autoconf.h"
#include "mlx5_mr.h"
#include "mlx5_flow.h"
#include "rte_pmd_mlx5.h"
#include "mlx5_verbs.h"
#include "mlx5_nl.h"
#include "mlx5_devx.h"

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
	device_attr->max_cqe = attr_ex.orig_attr.max_cqe;
	device_attr->max_mr = attr_ex.orig_attr.max_mr;
	device_attr->max_pd = attr_ex.orig_attr.max_pd;
	device_attr->max_qp = attr_ex.orig_attr.max_qp;
	device_attr->max_srq = attr_ex.orig_attr.max_srq;
	device_attr->max_srq_wr = attr_ex.orig_attr.max_srq_wr;
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
	strlcpy(device_attr->fw_ver, attr_ex.orig_attr.fw_ver,
		sizeof(device_attr->fw_ver));

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
	struct mlx5_dev_ctx_shared *sh = data;
	void *ret;
	size_t alignment = rte_mem_page_size();
	if (alignment == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get mem page size");
		rte_errno = ENOMEM;
		return NULL;
	}

	MLX5_ASSERT(data != NULL);
	ret = mlx5_malloc(0, size, alignment, sh->numa_node);
	if (!ret && size)
		rte_errno = ENOMEM;
	return ret;
}

/**
 * Detect misc5 support or not
 *
 * @param[in] priv
 *   Device private data pointer
 */
#ifdef HAVE_MLX5DV_DR
static void
__mlx5_discovery_misc5_cap(struct mlx5_priv *priv)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	/* Dummy VxLAN matcher to detect rdma-core misc5 cap
	 * Case: IPv4--->UDP--->VxLAN--->vni
	 */
	void *tbl;
	struct mlx5_flow_dv_match_params matcher_mask;
	void *match_m;
	void *matcher;
	void *headers_m;
	void *misc5_m;
	uint32_t *tunnel_header_m;
	struct mlx5dv_flow_matcher_attr dv_attr;

	memset(&matcher_mask, 0, sizeof(matcher_mask));
	matcher_mask.size = sizeof(matcher_mask.buf);
	match_m = matcher_mask.buf;
	headers_m = MLX5_ADDR_OF(fte_match_param, match_m, outer_headers);
	misc5_m = MLX5_ADDR_OF(fte_match_param,
			       match_m, misc_parameters_5);
	tunnel_header_m = (uint32_t *)
				MLX5_ADDR_OF(fte_match_set_misc5,
				misc5_m, tunnel_header_1);
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_protocol, 0xff);
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_version, 4);
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, udp_dport, 0xffff);
	*tunnel_header_m = 0xffffff;

	tbl = mlx5_glue->dr_create_flow_tbl(priv->sh->rx_domain, 1);
	if (!tbl) {
		DRV_LOG(INFO, "No SW steering support");
		return;
	}
	dv_attr.type = IBV_FLOW_ATTR_NORMAL,
	dv_attr.match_mask = (void *)&matcher_mask,
	dv_attr.match_criteria_enable =
			(1 << MLX5_MATCH_CRITERIA_ENABLE_OUTER_BIT) |
			(1 << MLX5_MATCH_CRITERIA_ENABLE_MISC5_BIT);
	dv_attr.priority = 3;
#ifdef HAVE_MLX5DV_DR_ESWITCH
	void *misc2_m;
	if (priv->config.dv_esw_en) {
		/* FDB enabled reg_c_0 */
		dv_attr.match_criteria_enable |=
				(1 << MLX5_MATCH_CRITERIA_ENABLE_MISC2_BIT);
		misc2_m = MLX5_ADDR_OF(fte_match_param,
				       match_m, misc_parameters_2);
		MLX5_SET(fte_match_set_misc2, misc2_m,
			 metadata_reg_c_0, 0xffff);
	}
#endif
	matcher = mlx5_glue->dv_create_flow_matcher(priv->sh->ctx,
						    &dv_attr, tbl);
	if (matcher) {
		priv->sh->misc5_cap = 1;
		mlx5_glue->dv_destroy_flow_matcher(matcher);
	}
	mlx5_glue->dr_destroy_flow_tbl(tbl);
#else
	RTE_SET_USED(priv);
#endif
}
#endif

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
	char s[MLX5_NAME_SIZE] __rte_unused;
	int err;

	MLX5_ASSERT(sh && sh->refcnt);
	if (sh->refcnt > 1)
		return 0;
	err = mlx5_alloc_table_hash_list(priv);
	if (err)
		goto error;
	/* The resources below are only valid with DV support. */
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	/* Init port id action list. */
	snprintf(s, sizeof(s), "%s_port_id_action_list", sh->ibdev_name);
	sh->port_id_action_list = mlx5_list_create(s, sh, true,
						   flow_dv_port_id_create_cb,
						   flow_dv_port_id_match_cb,
						   flow_dv_port_id_remove_cb,
						   flow_dv_port_id_clone_cb,
						 flow_dv_port_id_clone_free_cb);
	if (!sh->port_id_action_list)
		goto error;
	/* Init push vlan action list. */
	snprintf(s, sizeof(s), "%s_push_vlan_action_list", sh->ibdev_name);
	sh->push_vlan_action_list = mlx5_list_create(s, sh, true,
						    flow_dv_push_vlan_create_cb,
						    flow_dv_push_vlan_match_cb,
						    flow_dv_push_vlan_remove_cb,
						    flow_dv_push_vlan_clone_cb,
					       flow_dv_push_vlan_clone_free_cb);
	if (!sh->push_vlan_action_list)
		goto error;
	/* Init sample action list. */
	snprintf(s, sizeof(s), "%s_sample_action_list", sh->ibdev_name);
	sh->sample_action_list = mlx5_list_create(s, sh, true,
						  flow_dv_sample_create_cb,
						  flow_dv_sample_match_cb,
						  flow_dv_sample_remove_cb,
						  flow_dv_sample_clone_cb,
						  flow_dv_sample_clone_free_cb);
	if (!sh->sample_action_list)
		goto error;
	/* Init dest array action list. */
	snprintf(s, sizeof(s), "%s_dest_array_list", sh->ibdev_name);
	sh->dest_array_list = mlx5_list_create(s, sh, true,
					       flow_dv_dest_array_create_cb,
					       flow_dv_dest_array_match_cb,
					       flow_dv_dest_array_remove_cb,
					       flow_dv_dest_array_clone_cb,
					      flow_dv_dest_array_clone_free_cb);
	if (!sh->dest_array_list)
		goto error;
#endif
#ifdef HAVE_MLX5DV_DR
	void *domain;

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
	}
	/*
	 * The drop action is just some dummy placeholder in rdma-core. It
	 * does not belong to domains and has no any attributes, and, can be
	 * shared by the entire device.
	 */
	sh->dr_drop_action = mlx5_glue->dr_create_flow_action_drop();
	if (!sh->dr_drop_action) {
		DRV_LOG(ERR, "FDB mlx5dv_dr_create_flow_action_drop");
		err = errno;
		goto error;
	}
#endif
	if (!sh->tunnel_hub && priv->config.dv_miss_info)
		err = mlx5_alloc_tunnel_hub(sh);
	if (err) {
		DRV_LOG(ERR, "mlx5_alloc_tunnel_hub failed err=%d", err);
		goto error;
	}
	if (priv->config.reclaim_mode == MLX5_RCM_AGGR) {
		mlx5_glue->dr_reclaim_domain_memory(sh->rx_domain, 1);
		mlx5_glue->dr_reclaim_domain_memory(sh->tx_domain, 1);
		if (sh->fdb_domain)
			mlx5_glue->dr_reclaim_domain_memory(sh->fdb_domain, 1);
	}
	sh->pop_vlan_action = mlx5_glue->dr_create_flow_action_pop_vlan();
	if (!priv->config.allow_duplicate_pattern) {
#ifndef HAVE_MLX5_DR_ALLOW_DUPLICATE
		DRV_LOG(WARNING, "Disallow duplicate pattern is not supported - maybe old rdma-core version?");
#endif
		mlx5_glue->dr_allow_duplicate_rules(sh->rx_domain, 0);
		mlx5_glue->dr_allow_duplicate_rules(sh->tx_domain, 0);
		if (sh->fdb_domain)
			mlx5_glue->dr_allow_duplicate_rules(sh->fdb_domain, 0);
	}

	__mlx5_discovery_misc5_cap(priv);
#endif /* HAVE_MLX5DV_DR */
	sh->default_miss_action =
			mlx5_glue->dr_create_flow_action_default_miss();
	if (!sh->default_miss_action)
		DRV_LOG(WARNING, "Default miss action is not supported.");
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
	if (sh->dr_drop_action) {
		mlx5_glue->destroy_flow_action(sh->dr_drop_action);
		sh->dr_drop_action = NULL;
	}
	if (sh->pop_vlan_action) {
		mlx5_glue->destroy_flow_action(sh->pop_vlan_action);
		sh->pop_vlan_action = NULL;
	}
	if (sh->encaps_decaps) {
		mlx5_hlist_destroy(sh->encaps_decaps);
		sh->encaps_decaps = NULL;
	}
	if (sh->modify_cmds) {
		mlx5_hlist_destroy(sh->modify_cmds);
		sh->modify_cmds = NULL;
	}
	if (sh->tag_table) {
		/* tags should be destroyed with flow before. */
		mlx5_hlist_destroy(sh->tag_table);
		sh->tag_table = NULL;
	}
	if (sh->tunnel_hub) {
		mlx5_release_tunnel_hub(sh, priv->dev_port);
		sh->tunnel_hub = NULL;
	}
	mlx5_free_table_hash_list(priv);
	if (sh->port_id_action_list) {
		mlx5_list_destroy(sh->port_id_action_list);
		sh->port_id_action_list = NULL;
	}
	if (sh->push_vlan_action_list) {
		mlx5_list_destroy(sh->push_vlan_action_list);
		sh->push_vlan_action_list = NULL;
	}
	if (sh->sample_action_list) {
		mlx5_list_destroy(sh->sample_action_list);
		sh->sample_action_list = NULL;
	}
	if (sh->dest_array_list) {
		mlx5_list_destroy(sh->dest_array_list);
		sh->dest_array_list = NULL;
	}
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
	struct mlx5_dev_ctx_shared *sh = priv->sh;

	MLX5_ASSERT(sh && sh->refcnt);
	if (sh->refcnt > 1)
		return;
#ifdef HAVE_MLX5DV_DR
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
	if (sh->dr_drop_action) {
		mlx5_glue->destroy_flow_action(sh->dr_drop_action);
		sh->dr_drop_action = NULL;
	}
#endif
	if (sh->pop_vlan_action) {
		mlx5_glue->destroy_flow_action(sh->pop_vlan_action);
		sh->pop_vlan_action = NULL;
	}
#endif /* HAVE_MLX5DV_DR */
	if (sh->default_miss_action)
		mlx5_glue->destroy_flow_action
				(sh->default_miss_action);
	if (sh->encaps_decaps) {
		mlx5_hlist_destroy(sh->encaps_decaps);
		sh->encaps_decaps = NULL;
	}
	if (sh->modify_cmds) {
		mlx5_hlist_destroy(sh->modify_cmds);
		sh->modify_cmds = NULL;
	}
	if (sh->tag_table) {
		/* tags should be destroyed with flow before. */
		mlx5_hlist_destroy(sh->tag_table);
		sh->tag_table = NULL;
	}
	if (sh->tunnel_hub) {
		mlx5_release_tunnel_hub(sh, priv->dev_port);
		sh->tunnel_hub = NULL;
	}
	mlx5_free_table_hash_list(priv);
	if (sh->port_id_action_list) {
		mlx5_list_destroy(sh->port_id_action_list);
		sh->port_id_action_list = NULL;
	}
	if (sh->push_vlan_action_list) {
		mlx5_list_destroy(sh->push_vlan_action_list);
		sh->push_vlan_action_list = NULL;
	}
	if (sh->sample_action_list) {
		mlx5_list_destroy(sh->sample_action_list);
		sh->sample_action_list = NULL;
	}
	if (sh->dest_array_list) {
		mlx5_list_destroy(sh->dest_array_list);
		sh->dest_array_list = NULL;
	}
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
 * Create the Tx queue DevX/Verbs object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Tx queue array.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_os_txq_obj_new(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_data *txq_data = (*priv->txqs)[idx];
	struct mlx5_txq_ctrl *txq_ctrl =
			container_of(txq_data, struct mlx5_txq_ctrl, txq);

	if (txq_ctrl->type == MLX5_TXQ_TYPE_HAIRPIN)
		return mlx5_txq_devx_obj_new(dev, idx);
#ifdef HAVE_MLX5DV_DEVX_UAR_OFFSET
	if (!priv->config.dv_esw_en)
		return mlx5_txq_devx_obj_new(dev, idx);
#endif
	return mlx5_txq_ibv_obj_new(dev, idx);
}

/**
 * Release an Tx DevX/verbs queue object.
 *
 * @param txq_obj
 *   DevX/Verbs Tx queue object.
 */
static void
mlx5_os_txq_obj_release(struct mlx5_txq_obj *txq_obj)
{
	if (txq_obj->txq_ctrl->type == MLX5_TXQ_TYPE_HAIRPIN) {
		mlx5_txq_devx_obj_release(txq_obj);
		return;
	}
#ifdef HAVE_MLX5DV_DEVX_UAR_OFFSET
	if (!txq_obj->txq_ctrl->priv->config.dv_esw_en) {
		mlx5_txq_devx_obj_release(txq_obj);
		return;
	}
#endif
	mlx5_txq_ibv_obj_release(txq_obj);
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
 * DR flow drop action support detect.
 *
 * @param dev
 *   Pointer to rte_eth_dev structure.
 *
 */
static void
mlx5_flow_drop_action_config(struct rte_eth_dev *dev __rte_unused)
{
#ifdef HAVE_MLX5DV_DR
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!priv->config.dv_flow_en || !priv->sh->dr_drop_action)
		return;
	/**
	 * DR supports drop action placeholder when it is supported;
	 * otherwise, use the queue drop action.
	 */
	if (mlx5_flow_discover_dr_action_support(dev))
		priv->root_drop_action = priv->drop_queue.hrxq->action;
	else
		priv->root_drop_action = priv->sh->dr_drop_action;
#endif
}

static void
mlx5_queue_counter_id_prepare(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	void *ctx = priv->sh->ctx;

	priv->q_counters = mlx5_devx_cmd_queue_counter_alloc(ctx);
	if (!priv->q_counters) {
		struct ibv_cq *cq = mlx5_glue->create_cq(ctx, 1, NULL, NULL, 0);
		struct ibv_wq *wq;

		DRV_LOG(DEBUG, "Port %d queue counter object cannot be created "
			"by DevX - fall-back to use the kernel driver global "
			"queue counter.", dev->data->port_id);
		/* Create WQ by kernel and query its queue counter ID. */
		if (cq) {
			wq = mlx5_glue->create_wq(ctx,
						  &(struct ibv_wq_init_attr){
						    .wq_type = IBV_WQT_RQ,
						    .max_wr = 1,
						    .max_sge = 1,
						    .pd = priv->sh->pd,
						    .cq = cq,
						});
			if (wq) {
				/* Counter is assigned only on RDY state. */
				int ret = mlx5_glue->modify_wq(wq,
						 &(struct ibv_wq_attr){
						 .attr_mask = IBV_WQ_ATTR_STATE,
						 .wq_state = IBV_WQS_RDY,
						});

				if (ret == 0)
					mlx5_devx_cmd_wq_query(wq,
							 &priv->counter_set_id);
				claim_zero(mlx5_glue->destroy_wq(wq));
			}
			claim_zero(mlx5_glue->destroy_cq(cq));
		}
	} else {
		priv->counter_set_id = priv->q_counters->id;
	}
	if (priv->counter_set_id == 0)
		DRV_LOG(INFO, "Part of the port %d statistics will not be "
			"available.", dev->data->port_id);
}

/**
 * Check if representor spawn info match devargs.
 *
 * @param spawn
 *   Verbs device parameters (name, port, switch_info) to spawn.
 * @param eth_da
 *   Device devargs to probe.
 *
 * @return
 *   Match result.
 */
static bool
mlx5_representor_match(struct mlx5_dev_spawn_data *spawn,
		       struct rte_eth_devargs *eth_da)
{
	struct mlx5_switch_info *switch_info = &spawn->info;
	unsigned int p, f;
	uint16_t id;
	uint16_t repr_id = mlx5_representor_id_encode(switch_info,
						      eth_da->type);

	switch (eth_da->type) {
	case RTE_ETH_REPRESENTOR_SF:
		if (!(spawn->info.port_name == -1 &&
		      switch_info->name_type ==
				MLX5_PHYS_PORT_NAME_TYPE_PFHPF) &&
		    switch_info->name_type != MLX5_PHYS_PORT_NAME_TYPE_PFSF) {
			rte_errno = EBUSY;
			return false;
		}
		break;
	case RTE_ETH_REPRESENTOR_VF:
		/* Allows HPF representor index -1 as exception. */
		if (!(spawn->info.port_name == -1 &&
		      switch_info->name_type ==
				MLX5_PHYS_PORT_NAME_TYPE_PFHPF) &&
		    switch_info->name_type != MLX5_PHYS_PORT_NAME_TYPE_PFVF) {
			rte_errno = EBUSY;
			return false;
		}
		break;
	case RTE_ETH_REPRESENTOR_NONE:
		rte_errno = EBUSY;
		return false;
	default:
		rte_errno = ENOTSUP;
		DRV_LOG(ERR, "unsupported representor type");
		return false;
	}
	/* Check representor ID: */
	for (p = 0; p < eth_da->nb_ports; ++p) {
		if (spawn->pf_bond < 0) {
			/* For non-LAG mode, allow and ignore pf. */
			switch_info->pf_num = eth_da->ports[p];
			repr_id = mlx5_representor_id_encode(switch_info,
							     eth_da->type);
		}
		for (f = 0; f < eth_da->nb_representor_ports; ++f) {
			id = MLX5_REPRESENTOR_ID
				(eth_da->ports[p], eth_da->type,
				 eth_da->representor_ports[f]);
			if (repr_id == id)
				return true;
		}
	}
	rte_errno = EBUSY;
	return false;
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
 * @param config
 *   Device arguments.
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
	       struct mlx5_dev_config *config,
	       struct rte_eth_devargs *eth_da)
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
	struct mlx5_port_info vport_info = { .query_flags = 0 };
	int i;

	/* Determine if this port representor is supposed to be spawned. */
	if (switch_info->representor && dpdk_dev->devargs &&
	    !mlx5_representor_match(spawn, eth_da))
		return NULL;
	/* Build device name. */
	if (spawn->pf_bond < 0) {
		/* Single device. */
		if (!switch_info->representor)
			strlcpy(name, dpdk_dev->name, sizeof(name));
		else
			err = snprintf(name, sizeof(name), "%s_representor_%s%u",
				 dpdk_dev->name,
				 switch_info->name_type ==
				 MLX5_PHYS_PORT_NAME_TYPE_PFSF ? "sf" : "vf",
				 switch_info->port_name);
	} else {
		/* Bonding device. */
		if (!switch_info->representor) {
			err = snprintf(name, sizeof(name), "%s_%s",
				 dpdk_dev->name,
				 mlx5_os_get_dev_device_name(spawn->phys_dev));
		} else {
			err = snprintf(name, sizeof(name), "%s_%s_representor_c%dpf%d%s%u",
				dpdk_dev->name,
				mlx5_os_get_dev_device_name(spawn->phys_dev),
				switch_info->ctrl_num,
				switch_info->pf_num,
				switch_info->name_type ==
				MLX5_PHYS_PORT_NAME_TYPE_PFSF ? "sf" : "vf",
				switch_info->port_name);
		}
	}
	if (err >= (int)sizeof(name))
		DRV_LOG(WARNING, "device name overflow %s", name);
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
		eth_dev->dev_ops = &mlx5_dev_sec_ops;
		eth_dev->rx_descriptor_status = mlx5_rx_descriptor_status;
		eth_dev->tx_descriptor_status = mlx5_tx_descriptor_status;
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
	if (config->dv_miss_info) {
		if (switch_info->master || switch_info->representor)
			config->dv_xmeta_en = MLX5_XMETA_MODE_META16;
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
	/* Rx CQE compression is enabled by default. */
	config->cqe_comp = 1;
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
	/* Some internal functions rely on Netlink sockets, open them now. */
	priv->nl_socket_rdma = mlx5_nl_init(NETLINK_RDMA);
	priv->nl_socket_route =	mlx5_nl_init(NETLINK_ROUTE);
	priv->representor = !!switch_info->representor;
	priv->master = !!switch_info->master;
	priv->domain_id = RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID;
	priv->vport_meta_tag = 0;
	priv->vport_meta_mask = 0;
	priv->pf_bond = spawn->pf_bond;

	DRV_LOG(DEBUG,
		"dev_port=%u bus=%s pci=%s master=%d representor=%d pf_bond=%d\n",
		priv->dev_port, dpdk_dev->bus->name,
		priv->pci_dev ? priv->pci_dev->name : "NONE",
		priv->master, priv->representor, priv->pf_bond);

	/*
	 * If we have E-Switch we should determine the vport attributes.
	 * E-Switch may use either source vport field or reg_c[0] metadata
	 * register to match on vport index. The engaged part of metadata
	 * register is defined by mask.
	 */
	if (switch_info->representor || switch_info->master) {
		err = mlx5_glue->devx_port_query(sh->ctx,
						 spawn->phys_port,
						 &vport_info);
		if (err) {
			DRV_LOG(WARNING,
				"can't query devx port %d on device %s",
				spawn->phys_port,
				mlx5_os_get_dev_device_name(spawn->phys_dev));
			vport_info.query_flags = 0;
		}
	}
	if (vport_info.query_flags & MLX5_PORT_QUERY_REG_C0) {
		priv->vport_meta_tag = vport_info.vport_meta_tag;
		priv->vport_meta_mask = vport_info.vport_meta_mask;
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
	if (vport_info.query_flags & MLX5_PORT_QUERY_VPORT) {
		priv->vport_id = vport_info.vport_id;
	} else if (spawn->pf_bond >= 0 &&
		   (switch_info->representor || switch_info->master)) {
		DRV_LOG(ERR, "can't deduce vport index for port %d"
			     " on bonding device %s",
			     spawn->phys_port,
			     mlx5_os_get_dev_device_name(spawn->phys_dev));
		err = ENOTSUP;
		goto error;
	} else {
		/*
		 * Suppose vport index in compatible way. Kernel/rdma_core
		 * support single E-Switch per PF configurations only and
		 * vport_id field contains the vport index for associated VF,
		 * which is deduced from representor port name.
		 * For example, let's have the IB device port 10, it has
		 * attached network device eth0, which has port name attribute
		 * pf0vf2, we can deduce the VF number as 2, and set vport index
		 * as 3 (2+1). This assigning schema should be changed if the
		 * multiple E-Switch instances per PF configurations or/and PCI
		 * subfunctions are added.
		 */
		priv->vport_id = switch_info->representor ?
				 switch_info->port_name + 1 : -1;
	}
	priv->representor_id = mlx5_representor_id_encode(switch_info,
							  eth_da->type);
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
		DRV_LOG(DEBUG, "dev_port-%u inherit domain_id=%u\n",
			priv->dev_port, priv->domain_id);
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
		DRV_LOG(DEBUG, "dev_port-%u new domain_id=%u\n",
			priv->dev_port, priv->domain_id);
	}
	/* Override some values set by hardware configuration. */
	mlx5_args(config, dpdk_dev->devargs);
	err = mlx5_dev_check_sibling_config(priv, config, dpdk_dev);
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
	if (spawn->max_port > UINT8_MAX) {
		/* Verbs can't support ports larger than 255 by design. */
		DRV_LOG(ERR, "can't support IB ports > UINT8_MAX");
		err = EINVAL;
		goto error;
	}
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
	if (config->devx) {
		err = mlx5_devx_cmd_query_hca_attr(sh->ctx, &config->hca_attr);
		if (err) {
			err = -err;
			goto error;
		}
		/* Check relax ordering support. */
		if (!haswell_broadwell_cpu) {
			sh->cmng.relaxed_ordering_write =
				config->hca_attr.relaxed_ordering_write;
			sh->cmng.relaxed_ordering_read =
				config->hca_attr.relaxed_ordering_read;
		} else {
			sh->cmng.relaxed_ordering_read = 0;
			sh->cmng.relaxed_ordering_write = 0;
		}
		sh->rq_ts_format = config->hca_attr.rq_ts_format;
		sh->sq_ts_format = config->hca_attr.sq_ts_format;
		sh->steering_format_version =
			config->hca_attr.steering_format_version;
		sh->qp_ts_format = config->hca_attr.qp_ts_format;
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
			DRV_LOG(DEBUG, "LRO minimal size of TCP segment "
				"required for coalescing is %d bytes",
				config->hca_attr.lro_min_mss_size);
		}
#if defined(HAVE_MLX5DV_DR) && \
	(defined(HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER) || \
	 defined(HAVE_MLX5_DR_CREATE_ACTION_ASO))
		if (config->hca_attr.qos.sup &&
		    config->hca_attr.qos.flow_meter_old &&
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
				/*
				 * The meter color register is used by the
				 * flow-hit feature as well.
				 * The flow-hit feature must use REG_C_3
				 * Prefer REG_C_3 if it is available.
				 */
				if (reg_c_mask & (1 << (REG_C_3 - REG_C_0)))
					priv->mtr_color_reg = REG_C_3;
				else
					priv->mtr_color_reg = ffs(reg_c_mask)
							      - 1 + REG_C_0;
				priv->mtr_en = 1;
				priv->mtr_reg_share =
				      config->hca_attr.qos.flow_meter;
				DRV_LOG(DEBUG, "The REG_C meter uses is %d",
					priv->mtr_color_reg);
			}
		}
		if (config->hca_attr.qos.sup &&
			config->hca_attr.qos.flow_meter_aso_sup) {
			uint32_t log_obj_size =
				rte_log2_u32(MLX5_ASO_MTRS_PER_POOL >> 1);
			if (log_obj_size >=
			config->hca_attr.qos.log_meter_aso_granularity &&
			log_obj_size <=
			config->hca_attr.qos.log_meter_aso_max_alloc)
				sh->meter_aso_en = 1;
		}
		if (priv->mtr_en) {
			err = mlx5_aso_flow_mtrs_mng_init(priv->sh);
			if (err) {
				err = -err;
				goto error;
			}
		}
		if (config->hca_attr.flow.tunnel_header_0_1)
			sh->tunnel_header_0_1 = 1;
#endif
#ifdef HAVE_MLX5_DR_CREATE_ACTION_ASO
		if (config->hca_attr.flow_hit_aso &&
		    priv->mtr_color_reg == REG_C_3) {
			sh->flow_hit_aso_en = 1;
			err = mlx5_flow_aso_age_mng_init(sh);
			if (err) {
				err = -err;
				goto error;
			}
			DRV_LOG(DEBUG, "Flow Hit ASO is supported.");
		}
#endif /* HAVE_MLX5_DR_CREATE_ACTION_ASO */
#if defined(HAVE_MLX5_DR_CREATE_ACTION_ASO) && \
	defined(HAVE_MLX5_DR_ACTION_ASO_CT)
		if (config->hca_attr.ct_offload &&
		    priv->mtr_color_reg == REG_C_3) {
			err = mlx5_flow_aso_ct_mng_init(sh);
			if (err) {
				err = -err;
				goto error;
			}
			DRV_LOG(DEBUG, "CT ASO is supported.");
			sh->ct_aso_en = 1;
		}
#endif /* HAVE_MLX5_DR_CREATE_ACTION_ASO && HAVE_MLX5_DR_ACTION_ASO_CT */
#if defined(HAVE_MLX5DV_DR) && defined(HAVE_MLX5_DR_CREATE_ACTION_FLOW_SAMPLE)
		if (config->hca_attr.log_max_ft_sampler_num > 0  &&
		    config->dv_flow_en) {
			priv->sampler_en = 1;
			DRV_LOG(DEBUG, "Sampler enabled!");
		} else {
			priv->sampler_en = 0;
			if (!config->hca_attr.log_max_ft_sampler_num)
				DRV_LOG(WARNING,
					"No available register for sampler.");
			else
				DRV_LOG(DEBUG, "DV flow is not supported!");
		}
#endif
	}
	if (config->cqe_comp && RTE_CACHE_LINE_SIZE == 128 &&
	    !(dv_attr.flags & MLX5DV_CONTEXT_FLAGS_CQE_128B_COMP)) {
		DRV_LOG(WARNING, "Rx CQE 128B compression is not supported");
		config->cqe_comp = 0;
	}
	if (config->cqe_comp_fmt == MLX5_CQE_RESP_FORMAT_FTAG_STRIDX &&
	    (!config->devx || !config->hca_attr.mini_cqe_resp_flow_tag)) {
		DRV_LOG(WARNING, "Flow Tag CQE compression"
				 " format isn't supported.");
		config->cqe_comp = 0;
	}
	if (config->cqe_comp_fmt == MLX5_CQE_RESP_FORMAT_L34H_STRIDX &&
	    (!config->devx || !config->hca_attr.mini_cqe_resp_l3_l4_tag)) {
		DRV_LOG(WARNING, "L3/L4 Header CQE compression"
				 " format isn't supported.");
		config->cqe_comp = 0;
	}
	DRV_LOG(DEBUG, "Rx CQE compression is %ssupported",
			config->cqe_comp ? "" : "not ");
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
	if (priv->representor) {
		eth_dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR;
		eth_dev->data->representor_id = priv->representor_id;
	}
	priv->mp_id.port_id = eth_dev->data->port_id;
	strlcpy(priv->mp_id.name, MLX5_MP_NAME, RTE_MP_MAX_NAME_LEN);
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
			" loaded? (errno: %s)",
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
	eth_dev->dev_ops = &mlx5_dev_ops;
	eth_dev->rx_descriptor_status = mlx5_rx_descriptor_status;
	eth_dev->tx_descriptor_status = mlx5_tx_descriptor_status;
	eth_dev->rx_queue_count = mlx5_rx_queue_count;
	/* Register MAC address. */
	claim_zero(mlx5_mac_addr_add(eth_dev, &mac, 0, 0));
	if (config->vf && config->vf_nl_en)
		mlx5_nl_mac_addr_sync(priv->nl_socket_route,
				      mlx5_ifindex(eth_dev),
				      eth_dev->data->mac_addrs,
				      MLX5_MAX_MAC_ADDRESSES);
	priv->ctrl_flows = 0;
	rte_spinlock_init(&priv->flow_list_lock);
	TAILQ_INIT(&priv->flow_meters);
	priv->mtr_profile_tbl = mlx5_l3t_create(MLX5_L3T_TYPE_PTR);
	if (!priv->mtr_profile_tbl)
		goto error;
	/* Hint libmlx5 to use PMD allocator for data plane resources */
	mlx5_glue->dv_set_context_attr(sh->ctx,
			MLX5DV_CTX_ATTR_BUF_ALLOCATORS,
			(void *)((uintptr_t)&(struct mlx5dv_ctx_allocators){
				.alloc = &mlx5_alloc_verbs_buf,
				.free = &mlx5_free_verbs_buf,
				.data = sh,
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
	for (i = 0; i < MLX5_FLOW_TYPE_MAXI; i++) {
		icfg[i].release_mem_en = !!config->reclaim_mode;
		if (config->reclaim_mode)
			icfg[i].per_core_cache = 0;
		priv->flows[i] = mlx5_ipool_create(&icfg[i]);
		if (!priv->flows[i])
			goto error;
	}
	/* Create context for virtual machine VLAN workaround. */
	priv->vmwa_context = mlx5_vlan_vmwa_init(eth_dev, spawn->ifindex);
	if (config->dv_flow_en) {
		err = mlx5_alloc_shared_dr(priv);
		if (err)
			goto error;
	}
	if (config->devx && config->dv_flow_en && config->dest_tir) {
		priv->obj_ops = devx_obj_ops;
		priv->obj_ops.drop_action_create =
						ibv_obj_ops.drop_action_create;
		priv->obj_ops.drop_action_destroy =
						ibv_obj_ops.drop_action_destroy;
#ifndef HAVE_MLX5DV_DEVX_UAR_OFFSET
		priv->obj_ops.txq_obj_modify = ibv_obj_ops.txq_obj_modify;
#else
		if (config->dv_esw_en)
			priv->obj_ops.txq_obj_modify =
						ibv_obj_ops.txq_obj_modify;
#endif
		/* Use specific wrappers for Tx object. */
		priv->obj_ops.txq_obj_new = mlx5_os_txq_obj_new;
		priv->obj_ops.txq_obj_release = mlx5_os_txq_obj_release;
		mlx5_queue_counter_id_prepare(eth_dev);
		priv->obj_ops.lb_dummy_queue_create =
					mlx5_rxq_ibv_obj_dummy_lb_create;
		priv->obj_ops.lb_dummy_queue_release =
					mlx5_rxq_ibv_obj_dummy_lb_release;
	} else {
		priv->obj_ops = ibv_obj_ops;
	}
	if (config->tx_pp &&
	    (priv->config.dv_esw_en ||
	     priv->obj_ops.txq_obj_new != mlx5_os_txq_obj_new)) {
		/*
		 * HAVE_MLX5DV_DEVX_UAR_OFFSET is required to support
		 * packet pacing and already checked above.
		 * Hence, we should only make sure the SQs will be created
		 * with DevX, not with Verbs.
		 * Verbs allocates the SQ UAR on its own and it can't be shared
		 * with Clock Queue UAR as required for Tx scheduling.
		 */
		DRV_LOG(ERR, "Verbs SQs, UAR can't be shared as required for packet pacing");
		err = ENODEV;
		goto error;
	}
	priv->drop_queue.hrxq = mlx5_drop_action_create(eth_dev);
	if (!priv->drop_queue.hrxq)
		goto error;
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
	priv->hrxqs = mlx5_list_create("hrxq", eth_dev, true,
				       mlx5_hrxq_create_cb,
				       mlx5_hrxq_match_cb,
				       mlx5_hrxq_remove_cb,
				       mlx5_hrxq_clone_cb,
				       mlx5_hrxq_clone_free_cb);
	if (!priv->hrxqs)
		goto error;
	rte_rwlock_init(&priv->ind_tbls_lock);
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
						      MLX5_FLOW_MREG_HTABLE_SZ,
						      false, true, eth_dev,
						      flow_dv_mreg_create_cb,
						      flow_dv_mreg_match_cb,
						      flow_dv_mreg_remove_cb,
						      flow_dv_mreg_clone_cb,
						    flow_dv_mreg_clone_free_cb);
		if (!priv->mreg_cp_tbl) {
			err = ENOMEM;
			goto error;
		}
	}
	rte_spinlock_init(&priv->shared_act_sl);
	mlx5_flow_counter_mode_config(eth_dev);
	mlx5_flow_drop_action_config(eth_dev);
	if (priv->config.dv_flow_en)
		eth_dev->data->dev_flags |= RTE_ETH_DEV_FLOW_OPS_THREAD_SAFE;
	return eth_dev;
error:
	if (priv) {
		if (priv->mreg_cp_tbl)
			mlx5_hlist_destroy(priv->mreg_cp_tbl);
		if (priv->sh)
			mlx5_os_free_shared_dr(priv);
		if (priv->nl_socket_route >= 0)
			close(priv->nl_socket_route);
		if (priv->nl_socket_rdma >= 0)
			close(priv->nl_socket_rdma);
		if (priv->vmwa_context)
			mlx5_vlan_vmwa_exit(priv->vmwa_context);
		if (eth_dev && priv->drop_queue.hrxq)
			mlx5_drop_action_destroy(eth_dev);
		if (priv->mtr_profile_tbl)
			mlx5_l3t_destroy(priv->mtr_profile_tbl);
		if (own_domain_id)
			claim_zero(rte_eth_switch_domain_free(priv->domain_id));
		if (priv->hrxqs)
			mlx5_list_destroy(priv->hrxqs);
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
 *   Pointer to primary PCI address structure to match.
 * @param[in] nl_rdma
 *   Netlink RDMA group socket handle.
 * @param[in] owner
 *   Rerepsentor owner PF index.
 * @param[out] bond_info
 *   Pointer to bonding information.
 *
 * @return
 *   negative value if no bonding device found, otherwise
 *   positive index of slave PF in bonding.
 */
static int
mlx5_device_bond_pci_match(const struct ibv_device *ibv_dev,
			   const struct rte_pci_addr *pci_dev,
			   int nl_rdma, uint16_t owner,
			   struct mlx5_bond_info *bond_info)
{
	char ifname[IF_NAMESIZE + 1];
	unsigned int ifindex;
	unsigned int np, i;
	FILE *bond_file = NULL, *file;
	int pf = -1;
	int ret;

	/*
	 * Try to get master device name. If something goes
	 * wrong suppose the lack of kernel support and no
	 * bonding devices.
	 */
	memset(bond_info, 0, sizeof(*bond_info));
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
		bond_file = fopen(slaves, "r");
		if (bond_file)
			break;
	}
	if (!bond_file)
		return -1;
	/* Use safe format to check maximal buffer length. */
	MLX5_ASSERT(atol(RTE_STR(IF_NAMESIZE)) == IF_NAMESIZE);
	while (fscanf(bond_file, "%" RTE_STR(IF_NAMESIZE) "s", ifname) == 1) {
		char tmp_str[IF_NAMESIZE + 32];
		struct rte_pci_addr pci_addr;
		struct mlx5_switch_info	info;

		/* Process slave interface names in the loop. */
		snprintf(tmp_str, sizeof(tmp_str),
			 "/sys/class/net/%s", ifname);
		if (mlx5_get_pci_addr(tmp_str, &pci_addr)) {
			DRV_LOG(WARNING, "can not get PCI address"
					 " for netdev \"%s\"", ifname);
			continue;
		}
		/* Slave interface PCI address match found. */
		snprintf(tmp_str, sizeof(tmp_str),
			 "/sys/class/net/%s/phys_port_name", ifname);
		file = fopen(tmp_str, "rb");
		if (!file)
			break;
		info.name_type = MLX5_PHYS_PORT_NAME_TYPE_NOTSET;
		if (fscanf(file, "%32s", tmp_str) == 1)
			mlx5_translate_port_name(tmp_str, &info);
		fclose(file);
		/* Only process PF ports. */
		if (info.name_type != MLX5_PHYS_PORT_NAME_TYPE_LEGACY &&
		    info.name_type != MLX5_PHYS_PORT_NAME_TYPE_UPLINK)
			continue;
		/* Check max bonding member. */
		if (info.port_name >= MLX5_BOND_MAX_PORTS) {
			DRV_LOG(WARNING, "bonding index out of range, "
				"please increase MLX5_BOND_MAX_PORTS: %s",
				tmp_str);
			break;
		}
		/* Match PCI address, allows BDF0+pfx or BDFx+pfx. */
		if (pci_dev->domain == pci_addr.domain &&
		    pci_dev->bus == pci_addr.bus &&
		    pci_dev->devid == pci_addr.devid &&
		    ((pci_dev->function == 0 &&
		      pci_dev->function + owner == pci_addr.function) ||
		     (pci_dev->function == owner &&
		      pci_addr.function == owner)))
			pf = info.port_name;
		/* Get ifindex. */
		snprintf(tmp_str, sizeof(tmp_str),
			 "/sys/class/net/%s/ifindex", ifname);
		file = fopen(tmp_str, "rb");
		if (!file)
			break;
		ret = fscanf(file, "%u", &ifindex);
		fclose(file);
		if (ret != 1)
			break;
		/* Save bonding info. */
		strncpy(bond_info->ports[info.port_name].ifname, ifname,
			sizeof(bond_info->ports[0].ifname));
		bond_info->ports[info.port_name].pci_addr = pci_addr;
		bond_info->ports[info.port_name].ifindex = ifindex;
		bond_info->n_port++;
	}
	if (pf >= 0) {
		/* Get bond interface info */
		ret = mlx5_sysfs_bond_info(ifindex, &bond_info->ifindex,
					   bond_info->ifname);
		if (ret)
			DRV_LOG(ERR, "unable to get bond info: %s",
				strerror(rte_errno));
		else
			DRV_LOG(INFO, "PF device %u, bond device %u(%s)",
				ifindex, bond_info->ifindex, bond_info->ifname);
	}
	return pf;
}

static void
mlx5_os_config_default(struct mlx5_dev_config *config)
{
	memset(config, 0, sizeof(*config));
	config->mps = MLX5_ARG_UNSET;
	config->dbnc = MLX5_ARG_UNSET;
	config->rx_vec_en = 1;
	config->txq_inline_max = MLX5_ARG_UNSET;
	config->txq_inline_min = MLX5_ARG_UNSET;
	config->txq_inline_mpw = MLX5_ARG_UNSET;
	config->txqs_inline = MLX5_ARG_UNSET;
	config->vf_nl_en = 1;
	config->mr_ext_memseg_en = 1;
	config->mprq.max_memcpy_len = MLX5_MPRQ_MEMCPY_DEFAULT_LEN;
	config->mprq.min_rxqs_num = MLX5_MPRQ_MIN_RXQS;
	config->dv_esw_en = 1;
	config->dv_flow_en = 1;
	config->decap_en = 1;
	config->log_hp_size = MLX5_ARG_UNSET;
}

/**
 * Register a PCI device within bonding.
 *
 * This function spawns Ethernet devices out of a given PCI device and
 * bonding owner PF index.
 *
 * @param[in] pci_dev
 *   PCI device information.
 * @param[in] req_eth_da
 *   Requested ethdev device argument.
 * @param[in] owner_id
 *   Requested owner PF port ID within bonding device, default to 0.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_os_pci_probe_pf(struct rte_pci_device *pci_dev,
		     struct rte_eth_devargs *req_eth_da,
		     uint16_t owner_id)
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
	struct rte_eth_devargs eth_da = *req_eth_da;
	struct rte_pci_addr owner_pci = pci_dev->addr; /* Owner PF. */
	struct mlx5_bond_info bond_info;
	int ret = -1;

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
				(ibv_list[ret], &owner_pci, nl_rdma, owner_id,
				 &bond_info);
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
			/* Amend owner pci address if owner PF ID specified. */
			if (eth_da.nb_representor_ports)
				owner_pci.function += owner_id;
			DRV_LOG(INFO, "PCI information matches for"
				      " slave %d bonding device \"%s\"",
				      bd, ibv_list[ret]->name);
			ibv_match[nd++] = ibv_list[ret];
			break;
		} else {
			/* Bonding device not found. */
			if (mlx5_get_pci_addr(ibv_list[ret]->ibdev_path,
					      &pci_addr))
				continue;
			if (owner_pci.domain != pci_addr.domain ||
			    owner_pci.bus != pci_addr.bus ||
			    owner_pci.devid != pci_addr.devid ||
			    owner_pci.function != pci_addr.function)
				continue;
			DRV_LOG(INFO, "PCI information matches for device \"%s\"",
				ibv_list[ret]->name);
			ibv_match[nd++] = ibv_list[ret];
		}
	}
	ibv_match[nd] = NULL;
	if (!nd) {
		/* No device matches, just complain and bail out. */
		DRV_LOG(WARNING,
			"no Verbs device matches PCI device " PCI_PRI_FMT ","
			" are kernel drivers loaded?",
			owner_pci.domain, owner_pci.bus,
			owner_pci.devid, owner_pci.function);
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
			list[ns].bond_info = &bond_info;
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
					if (np == 1) {
						/*
						 * Force standalone bonding
						 * device for ROCE LAG
						 * confgiurations.
						 */
						list[ns].info.master = 0;
						list[ns].info.representor = 0;
					}
					if (list[ns].info.port_name == bd)
						ns++;
					break;
				case MLX5_PHYS_PORT_NAME_TYPE_PFHPF:
					/* Fallthrough */
				case MLX5_PHYS_PORT_NAME_TYPE_PFVF:
					/* Fallthrough */
				case MLX5_PHYS_PORT_NAME_TYPE_PFSF:
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
			list[ns].bond_info = NULL;
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
		/*
		 * New kernels may add the switch_id attribute for the case
		 * there is no E-Switch and we wrongly recognized the
		 * only device as master. Override this if there is the
		 * single device with single port and new device name
		 * format present.
		 */
		if (nd == 1 &&
		    list[0].info.name_type == MLX5_PHYS_PORT_NAME_TYPE_UPLINK) {
			list[0].info.master = 0;
			list[0].info.representor = 0;
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
	case PCI_DEVICE_ID_MELLANOX_CONNECTXVF:
		dev_config_vf = 1;
		break;
	default:
		dev_config_vf = 0;
		break;
	}
	if (eth_da.type != RTE_ETH_REPRESENTOR_NONE) {
		/* Set devargs default values. */
		if (eth_da.nb_mh_controllers == 0) {
			eth_da.nb_mh_controllers = 1;
			eth_da.mh_controllers[0] = 0;
		}
		if (eth_da.nb_ports == 0 && ns > 0) {
			if (list[0].pf_bond >= 0 && list[0].info.representor)
				DRV_LOG(WARNING, "Representor on Bonding device should use pf#vf# syntax: %s",
					pci_dev->device.devargs->args);
			eth_da.nb_ports = 1;
			eth_da.ports[0] = list[0].info.pf_num;
		}
		if (eth_da.nb_representor_ports == 0) {
			eth_da.nb_representor_ports = 1;
			eth_da.representor_ports[0] = 0;
		}
	}
	for (i = 0; i != ns; ++i) {
		uint32_t restore;

		/* Default configuration. */
		mlx5_os_config_default(&dev_config);
		dev_config.vf = dev_config_vf;
		dev_config.allow_duplicate_pattern = 1;
		list[i].numa_node = pci_dev->device.numa_node;
		list[i].eth_dev = mlx5_dev_spawn(&pci_dev->device,
						 &list[i],
						 &dev_config,
						 &eth_da);
		if (!list[i].eth_dev) {
			if (rte_errno != EBUSY && rte_errno != EEXIST)
				break;
			/* Device is disabled or already spawned. Ignore it. */
			continue;
		}
		restore = list[i].eth_dev->data->dev_flags;
		rte_eth_copy_pci_info(list[i].eth_dev, pci_dev);
		/**
		 * Each representor has a dedicated interrupts vector.
		 * rte_eth_copy_pci_info() assigns PF interrupts handle to
		 * representor eth_dev object because representor and PF
		 * share the same PCI address.
		 * Override representor device with a dedicated
		 * interrupts handle here.
		 * Representor interrupts handle is released in mlx5_dev_stop().
		 */
		if (list[i].info.representor) {
			struct rte_intr_handle *intr_handle;
			intr_handle = mlx5_malloc(MLX5_MEM_SYS | MLX5_MEM_ZERO,
						  sizeof(*intr_handle), 0,
						  SOCKET_ID_ANY);
			if (!intr_handle) {
				DRV_LOG(ERR,
					"port %u failed to allocate memory for interrupt handler "
					"Rx interrupts will not be supported",
					i);
				rte_errno = ENOMEM;
				ret = -rte_errno;
				goto exit;
			}
			list[i].eth_dev->intr_handle = intr_handle;
		}
		/* Restore non-PCI flags cleared by the above call. */
		list[i].eth_dev->data->dev_flags |= restore;
		rte_eth_dev_probing_finish(list[i].eth_dev);
	}
	if (i != ns) {
		DRV_LOG(ERR,
			"probe of PCI device " PCI_PRI_FMT " aborted after"
			" encountering an error: %s",
			owner_pci.domain, owner_pci.bus,
			owner_pci.devid, owner_pci.function,
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
mlx5_os_parse_eth_devargs(struct rte_device *dev,
			  struct rte_eth_devargs *eth_da)
{
	int ret = 0;

	if (dev->devargs == NULL)
		return 0;
	memset(eth_da, 0, sizeof(*eth_da));
	/* Parse representor information first from class argument. */
	if (dev->devargs->cls_str)
		ret = rte_eth_devargs_parse(dev->devargs->cls_str, eth_da);
	if (ret != 0) {
		DRV_LOG(ERR, "failed to parse device arguments: %s",
			dev->devargs->cls_str);
		return -rte_errno;
	}
	if (eth_da->type == RTE_ETH_REPRESENTOR_NONE) {
		/* Parse legacy device argument */
		ret = rte_eth_devargs_parse(dev->devargs->args, eth_da);
		if (ret) {
			DRV_LOG(ERR, "failed to parse device arguments: %s",
				dev->devargs->args);
			return -rte_errno;
		}
	}
	return 0;
}

/**
 * Callback to register a PCI device.
 *
 * This function spawns Ethernet devices out of a given PCI device.
 *
 * @param[in] pci_dev
 *   PCI device information.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_os_pci_probe(struct rte_pci_device *pci_dev)
{
	struct rte_eth_devargs eth_da = { .nb_ports = 0 };
	int ret = 0;
	uint16_t p;

	ret = mlx5_os_parse_eth_devargs(&pci_dev->device, &eth_da);
	if (ret != 0)
		return ret;

	if (eth_da.nb_ports > 0) {
		/* Iterate all port if devargs pf is range: "pf[0-1]vf[...]". */
		for (p = 0; p < eth_da.nb_ports; p++)
			ret = mlx5_os_pci_probe_pf(pci_dev, &eth_da,
						   eth_da.ports[p]);
	} else {
		ret = mlx5_os_pci_probe_pf(pci_dev, &eth_da, 0);
	}
	return ret;
}

/* Probe a single SF device on auxiliary bus, no representor support. */
static int
mlx5_os_auxiliary_probe(struct rte_device *dev)
{
	struct rte_eth_devargs eth_da = { .nb_ports = 0 };
	struct mlx5_dev_config config;
	struct mlx5_dev_spawn_data spawn = { .pf_bond = -1 };
	struct rte_auxiliary_device *adev = RTE_DEV_TO_AUXILIARY(dev);
	struct rte_eth_dev *eth_dev;
	int ret = 0;

	/* Parse ethdev devargs. */
	ret = mlx5_os_parse_eth_devargs(dev, &eth_da);
	if (ret != 0)
		return ret;
	/* Set default config data. */
	mlx5_os_config_default(&config);
	config.sf = 1;
	/* Init spawn data. */
	spawn.max_port = 1;
	spawn.phys_port = 1;
	spawn.phys_dev = mlx5_os_get_ibv_dev(dev);
	if (spawn.phys_dev == NULL)
		return -rte_errno;
	ret = mlx5_auxiliary_get_ifindex(dev->name);
	if (ret < 0) {
		DRV_LOG(ERR, "failed to get ethdev ifindex: %s", dev->name);
		return ret;
	}
	spawn.ifindex = ret;
	spawn.numa_node = dev->numa_node;
	/* Spawn device. */
	eth_dev = mlx5_dev_spawn(dev, &spawn, &config, &eth_da);
	if (eth_dev == NULL)
		return -rte_errno;
	/* Post create. */
	eth_dev->intr_handle = &adev->intr_handle;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		eth_dev->data->dev_flags |= RTE_ETH_DEV_INTR_LSC;
		eth_dev->data->dev_flags |= RTE_ETH_DEV_INTR_RMV;
		eth_dev->data->numa_node = dev->numa_node;
	}
	rte_eth_dev_probing_finish(eth_dev);
	return 0;
}

/**
 * Net class driver callback to probe a device.
 *
 * This function probe PCI bus device(s) or a single SF on auxiliary bus.
 *
 * @param[in] dev
 *   Pointer to the generic device.
 *
 * @return
 *   0 on success, the function cannot fail.
 */
int
mlx5_os_net_probe(struct rte_device *dev)
{
	int ret;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		mlx5_pmd_socket_init();
	ret = mlx5_init_once();
	if (ret) {
		DRV_LOG(ERR, "unable to init PMD global data: %s",
			strerror(rte_errno));
		return -rte_errno;
	}
	if (mlx5_dev_is_pci(dev))
		return mlx5_os_pci_probe(RTE_DEV_TO_PCI(dev));
	else
		return mlx5_os_auxiliary_probe(dev);
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
	if (!err && sh->ctx) {
		/* Hint libmlx5 to use PMD allocator for data plane resources */
		mlx5_glue->dv_set_context_attr(sh->ctx,
			MLX5DV_CTX_ATTR_BUF_ALLOCATORS,
			(void *)((uintptr_t)&(struct mlx5dv_ctx_allocators){
				.alloc = &mlx5_alloc_verbs_buf,
				.free = &mlx5_free_verbs_buf,
				.data = sh,
			}));
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
		if (priv->q_counters != NULL &&
		    strcmp(ctr_name, "out_of_buffer") == 0)
			return mlx5_devx_cmd_queue_counter_query
					(priv->q_counters, 0, (uint32_t *)stat);
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
	*reg_mr_cb = mlx5_mr_verbs_ops.reg_mr;
	*dereg_mr_cb = mlx5_mr_verbs_ops.dereg_mr;
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

/**
 * Flush device MAC addresses
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 */
void
mlx5_os_mac_addr_flush(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	mlx5_nl_mac_addr_flush(priv->nl_socket_route, mlx5_ifindex(dev),
			       dev->data->mac_addrs,
			       MLX5_MAX_MAC_ADDRESSES, priv->mac_own);
}
