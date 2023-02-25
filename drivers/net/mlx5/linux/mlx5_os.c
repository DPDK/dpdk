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
#include <bus_driver.h>
#include <bus_pci_driver.h>
#include <bus_auxiliary_driver.h>
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
 *   The file descriptor (representing the interrupt) used in this channel.
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
 * @param sh
 *   Pointer to shared device context.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_os_capabilities_prepare(struct mlx5_dev_ctx_shared *sh)
{
	int err;
	struct mlx5_common_device *cdev = sh->cdev;
	struct mlx5_hca_attr *hca_attr = &cdev->config.hca_attr;
	struct ibv_device_attr_ex attr_ex = { .comp_mask = 0 };
	struct mlx5dv_context dv_attr = { .comp_mask = 0 };

	err = mlx5_glue->query_device_ex(cdev->ctx, NULL, &attr_ex);
	if (err) {
		rte_errno = errno;
		return -rte_errno;
	}
#ifdef HAVE_IBV_MLX5_MOD_SWP
	dv_attr.comp_mask |= MLX5DV_CONTEXT_MASK_SWP;
#endif
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	dv_attr.comp_mask |= MLX5DV_CONTEXT_MASK_TUNNEL_OFFLOADS;
#endif
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
	dv_attr.comp_mask |= MLX5DV_CONTEXT_MASK_STRIDING_RQ;
#endif
	err = mlx5_glue->dv_query_device(cdev->ctx, &dv_attr);
	if (err) {
		rte_errno = errno;
		return -rte_errno;
	}
	memset(&sh->dev_cap, 0, sizeof(struct mlx5_dev_cap));
	if (mlx5_dev_is_pci(cdev->dev))
		sh->dev_cap.vf = mlx5_dev_is_vf_pci(RTE_DEV_TO_PCI(cdev->dev));
	else
		sh->dev_cap.sf = 1;
	sh->dev_cap.max_qp_wr = attr_ex.orig_attr.max_qp_wr;
	sh->dev_cap.max_sge = attr_ex.orig_attr.max_sge;
	sh->dev_cap.max_cq = attr_ex.orig_attr.max_cq;
	sh->dev_cap.max_qp = attr_ex.orig_attr.max_qp;
#ifdef HAVE_MLX5DV_DR_ACTION_DEST_DEVX_TIR
	sh->dev_cap.dest_tir = 1;
#endif
#if defined(HAVE_IBV_FLOW_DV_SUPPORT) && defined(HAVE_MLX5DV_DR)
	DRV_LOG(DEBUG, "DV flow is supported.");
	sh->dev_cap.dv_flow_en = 1;
#endif
#ifdef HAVE_MLX5DV_DR_ESWITCH
	if (hca_attr->eswitch_manager && sh->dev_cap.dv_flow_en && sh->esw_mode)
		sh->dev_cap.dv_esw_en = 1;
#endif
	/*
	 * Multi-packet send is supported by ConnectX-4 Lx PF as well
	 * as all ConnectX-5 devices.
	 */
	if (dv_attr.flags & MLX5DV_CONTEXT_FLAGS_MPW_ALLOWED) {
		if (dv_attr.flags & MLX5DV_CONTEXT_FLAGS_ENHANCED_MPW) {
			DRV_LOG(DEBUG, "Enhanced MPW is supported.");
			sh->dev_cap.mps = MLX5_MPW_ENHANCED;
		} else {
			DRV_LOG(DEBUG, "MPW is supported.");
			sh->dev_cap.mps = MLX5_MPW;
		}
	} else {
		DRV_LOG(DEBUG, "MPW isn't supported.");
		sh->dev_cap.mps = MLX5_MPW_DISABLED;
	}
#if (RTE_CACHE_LINE_SIZE == 128)
	if (dv_attr.flags & MLX5DV_CONTEXT_FLAGS_CQE_128B_COMP)
		sh->dev_cap.cqe_comp = 1;
	DRV_LOG(DEBUG, "Rx CQE 128B compression is %ssupported.",
		sh->dev_cap.cqe_comp ? "" : "not ");
#else
	sh->dev_cap.cqe_comp = 1;
#endif
#ifdef HAVE_IBV_DEVICE_MPLS_SUPPORT
	sh->dev_cap.mpls_en =
		((dv_attr.tunnel_offloads_caps &
		  MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_CW_MPLS_OVER_GRE) &&
		 (dv_attr.tunnel_offloads_caps &
		  MLX5DV_RAW_PACKET_CAP_TUNNELED_OFFLOAD_CW_MPLS_OVER_UDP));
	DRV_LOG(DEBUG, "MPLS over GRE/UDP tunnel offloading is %ssupported.",
		sh->dev_cap.mpls_en ? "" : "not ");
#else
	DRV_LOG(WARNING,
		"MPLS over GRE/UDP tunnel offloading disabled due to old OFED/rdma-core version or firmware configuration");
#endif
#if defined(HAVE_IBV_WQ_FLAG_RX_END_PADDING)
	sh->dev_cap.hw_padding = !!attr_ex.rx_pad_end_addr_align;
#elif defined(HAVE_IBV_WQ_FLAGS_PCI_WRITE_END_PADDING)
	sh->dev_cap.hw_padding = !!(attr_ex.device_cap_flags_ex &
				    IBV_DEVICE_PCI_WRITE_END_PADDING);
#endif
	sh->dev_cap.hw_csum =
		!!(attr_ex.device_cap_flags_ex & IBV_DEVICE_RAW_IP_CSUM);
	DRV_LOG(DEBUG, "Checksum offloading is %ssupported.",
		sh->dev_cap.hw_csum ? "" : "not ");
	sh->dev_cap.hw_vlan_strip = !!(attr_ex.raw_packet_caps &
				       IBV_RAW_PACKET_CAP_CVLAN_STRIPPING);
	DRV_LOG(DEBUG, "VLAN stripping is %ssupported.",
		(sh->dev_cap.hw_vlan_strip ? "" : "not "));
	sh->dev_cap.hw_fcs_strip = !!(attr_ex.raw_packet_caps &
				      IBV_RAW_PACKET_CAP_SCATTER_FCS);
#if !defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42) && \
	!defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
	DRV_LOG(DEBUG, "Counters are not supported.");
#endif
	/*
	 * DPDK doesn't support larger/variable indirection tables.
	 * Once DPDK supports it, take max size from device attr.
	 */
	sh->dev_cap.ind_table_max_size =
			RTE_MIN(attr_ex.rss_caps.max_rwq_indirection_table_size,
				(unsigned int)RTE_ETH_RSS_RETA_SIZE_512);
	DRV_LOG(DEBUG, "Maximum Rx indirection table size is %u",
		sh->dev_cap.ind_table_max_size);
	sh->dev_cap.tso = (attr_ex.tso_caps.max_tso > 0 &&
			   (attr_ex.tso_caps.supported_qpts &
			    (1 << IBV_QPT_RAW_PACKET)));
	if (sh->dev_cap.tso)
		sh->dev_cap.tso_max_payload_sz = attr_ex.tso_caps.max_tso;
	strlcpy(sh->dev_cap.fw_ver, attr_ex.orig_attr.fw_ver,
		sizeof(sh->dev_cap.fw_ver));
#ifdef HAVE_IBV_MLX5_MOD_SWP
	if (dv_attr.comp_mask & MLX5DV_CONTEXT_MASK_SWP)
		sh->dev_cap.swp = dv_attr.sw_parsing_caps.sw_parsing_offloads &
				  (MLX5_SW_PARSING_CAP |
				   MLX5_SW_PARSING_CSUM_CAP |
				   MLX5_SW_PARSING_TSO_CAP);
	DRV_LOG(DEBUG, "SWP support: %u", sh->dev_cap.swp);
#endif
#ifdef HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT
	if (dv_attr.comp_mask & MLX5DV_CONTEXT_MASK_STRIDING_RQ) {
		struct mlx5dv_striding_rq_caps *strd_rq_caps =
				&dv_attr.striding_rq_caps;

		sh->dev_cap.mprq.enabled = 1;
		sh->dev_cap.mprq.log_min_stride_size =
			strd_rq_caps->min_single_stride_log_num_of_bytes;
		sh->dev_cap.mprq.log_max_stride_size =
			strd_rq_caps->max_single_stride_log_num_of_bytes;
		sh->dev_cap.mprq.log_min_stride_num =
			strd_rq_caps->min_single_wqe_log_num_of_strides;
		sh->dev_cap.mprq.log_max_stride_num =
			strd_rq_caps->max_single_wqe_log_num_of_strides;
		sh->dev_cap.mprq.log_min_stride_wqe_size =
					cdev->config.devx ?
					hca_attr->log_min_stride_wqe_sz :
					MLX5_MPRQ_LOG_MIN_STRIDE_WQE_SIZE;
		DRV_LOG(DEBUG, "\tmin_single_stride_log_num_of_bytes: %u",
			sh->dev_cap.mprq.log_min_stride_size);
		DRV_LOG(DEBUG, "\tmax_single_stride_log_num_of_bytes: %u",
			sh->dev_cap.mprq.log_max_stride_size);
		DRV_LOG(DEBUG, "\tmin_single_wqe_log_num_of_strides: %u",
			sh->dev_cap.mprq.log_min_stride_num);
		DRV_LOG(DEBUG, "\tmax_single_wqe_log_num_of_strides: %u",
			sh->dev_cap.mprq.log_max_stride_num);
		DRV_LOG(DEBUG, "\tmin_stride_wqe_log_size: %u",
			sh->dev_cap.mprq.log_min_stride_wqe_size);
		DRV_LOG(DEBUG, "\tsupported_qpts: %d",
			strd_rq_caps->supported_qpts);
		DRV_LOG(DEBUG, "Device supports Multi-Packet RQ.");
	}
#endif
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	if (dv_attr.comp_mask & MLX5DV_CONTEXT_MASK_TUNNEL_OFFLOADS) {
		sh->dev_cap.tunnel_en = dv_attr.tunnel_offloads_caps &
					(MLX5_TUNNELED_OFFLOADS_VXLAN_CAP |
					 MLX5_TUNNELED_OFFLOADS_GRE_CAP |
					 MLX5_TUNNELED_OFFLOADS_GENEVE_CAP);
	}
	if (sh->dev_cap.tunnel_en) {
		DRV_LOG(DEBUG, "Tunnel offloading is supported for %s%s%s",
			sh->dev_cap.tunnel_en &
			MLX5_TUNNELED_OFFLOADS_VXLAN_CAP ? "[VXLAN]" : "",
			sh->dev_cap.tunnel_en &
			MLX5_TUNNELED_OFFLOADS_GRE_CAP ? "[GRE]" : "",
			sh->dev_cap.tunnel_en &
			MLX5_TUNNELED_OFFLOADS_GENEVE_CAP ? "[GENEVE]" : "");
	} else {
		DRV_LOG(DEBUG, "Tunnel offloading is not supported.");
	}
#else
	DRV_LOG(WARNING,
		"Tunnel offloading disabled due to old OFED/rdma-core version");
#endif
	if (!sh->cdev->config.devx)
		return 0;
	/* Check capabilities for Packet Pacing. */
	DRV_LOG(DEBUG, "Timestamp counter frequency %u kHz.",
		hca_attr->dev_freq_khz);
	DRV_LOG(DEBUG, "Packet pacing is %ssupported.",
		hca_attr->qos.packet_pacing ? "" : "not ");
	DRV_LOG(DEBUG, "Cross channel ops are %ssupported.",
		hca_attr->cross_channel ? "" : "not ");
	DRV_LOG(DEBUG, "WQE index ignore is %ssupported.",
		hca_attr->wqe_index_ignore ? "" : "not ");
	DRV_LOG(DEBUG, "Non-wire SQ feature is %ssupported.",
		hca_attr->non_wire_sq ? "" : "not ");
	DRV_LOG(DEBUG, "Static WQE SQ feature is %ssupported (%d)",
		hca_attr->log_max_static_sq_wq ? "" : "not ",
		hca_attr->log_max_static_sq_wq);
	DRV_LOG(DEBUG, "WQE rate PP mode is %ssupported.",
		hca_attr->qos.wqe_rate_pp ? "" : "not ");
	sh->dev_cap.txpp_en = hca_attr->qos.packet_pacing;
	if (!hca_attr->cross_channel) {
		DRV_LOG(DEBUG,
			"Cross channel operations are required for packet pacing.");
		sh->dev_cap.txpp_en = 0;
	}
	if (!hca_attr->wqe_index_ignore) {
		DRV_LOG(DEBUG,
			"WQE index ignore feature is required for packet pacing.");
		sh->dev_cap.txpp_en = 0;
	}
	if (!hca_attr->non_wire_sq) {
		DRV_LOG(DEBUG,
			"Non-wire SQ feature is required for packet pacing.");
		sh->dev_cap.txpp_en = 0;
	}
	if (!hca_attr->log_max_static_sq_wq) {
		DRV_LOG(DEBUG,
			"Static WQE SQ feature is required for packet pacing.");
		sh->dev_cap.txpp_en = 0;
	}
	if (!hca_attr->qos.wqe_rate_pp) {
		DRV_LOG(DEBUG,
			"WQE rate mode is required for packet pacing.");
		sh->dev_cap.txpp_en = 0;
	}
#ifndef HAVE_MLX5DV_DEVX_UAR_OFFSET
	DRV_LOG(DEBUG,
		"DevX does not provide UAR offset, can't create queues for packet pacing.");
	sh->dev_cap.txpp_en = 0;
#endif
	sh->dev_cap.scatter_fcs_w_decap_disable =
					hca_attr->scatter_fcs_w_decap_disable;
	sh->dev_cap.rq_delay_drop_en = hca_attr->rq_delay_drop;
	mlx5_rt_timestamp_config(sh, hca_attr);
	return 0;
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
	if (priv->sh->config.dv_esw_en) {
		/* FDB enabled reg_c_0 */
		dv_attr.match_criteria_enable |=
				(1 << MLX5_MATCH_CRITERIA_ENABLE_MISC2_BIT);
		misc2_m = MLX5_ADDR_OF(fte_match_param,
				       match_m, misc_parameters_2);
		MLX5_SET(fte_match_set_misc2, misc2_m,
			 metadata_reg_c_0, 0xffff);
	}
#endif
	matcher = mlx5_glue->dv_create_flow_matcher(priv->sh->cdev->ctx,
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
	/* Init shared flex parsers list, no need lcore_share */
	snprintf(s, sizeof(s), "%s_flex_parsers_list", sh->ibdev_name);
	sh->flex_parsers_dv = mlx5_list_create(s, sh, false,
					       mlx5_flex_parser_create_cb,
					       mlx5_flex_parser_match_cb,
					       mlx5_flex_parser_remove_cb,
					       mlx5_flex_parser_clone_cb,
					       mlx5_flex_parser_clone_free_cb);
	if (!sh->flex_parsers_dv)
		goto error;
	if (priv->sh->config.dv_flow_en == 2)
		return 0;
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
#else
	if (priv->sh->config.dv_flow_en == 2)
		return 0;
#endif
#ifdef HAVE_MLX5DV_DR
	void *domain;

	/* Reference counter is zero, we should initialize structures. */
	domain = mlx5_glue->dr_create_domain(sh->cdev->ctx,
					     MLX5DV_DR_DOMAIN_TYPE_NIC_RX);
	if (!domain) {
		DRV_LOG(ERR, "ingress mlx5dv_dr_create_domain failed");
		err = errno;
		goto error;
	}
	sh->rx_domain = domain;
	domain = mlx5_glue->dr_create_domain(sh->cdev->ctx,
					     MLX5DV_DR_DOMAIN_TYPE_NIC_TX);
	if (!domain) {
		DRV_LOG(ERR, "egress mlx5dv_dr_create_domain failed");
		err = errno;
		goto error;
	}
	sh->tx_domain = domain;
#ifdef HAVE_MLX5DV_DR_ESWITCH
	if (sh->config.dv_esw_en) {
		domain = mlx5_glue->dr_create_domain(sh->cdev->ctx,
						     MLX5DV_DR_DOMAIN_TYPE_FDB);
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
	if (!sh->tunnel_hub && sh->config.dv_miss_info)
		err = mlx5_alloc_tunnel_hub(sh);
	if (err) {
		DRV_LOG(ERR, "mlx5_alloc_tunnel_hub failed err=%d", err);
		goto error;
	}
	if (sh->config.reclaim_mode == MLX5_RCM_AGGR) {
		mlx5_glue->dr_reclaim_domain_memory(sh->rx_domain, 1);
		mlx5_glue->dr_reclaim_domain_memory(sh->tx_domain, 1);
		if (sh->fdb_domain)
			mlx5_glue->dr_reclaim_domain_memory(sh->fdb_domain, 1);
	}
	sh->pop_vlan_action = mlx5_glue->dr_create_flow_action_pop_vlan();
	if (!sh->config.allow_duplicate_pattern) {
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
	LIST_INIT(&sh->shared_rxqs);
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
	MLX5_ASSERT(LIST_EMPTY(&sh->shared_rxqs));
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
	if (sh->send_to_kernel_action.action) {
		void *action = sh->send_to_kernel_action.action;

		mlx5_glue->destroy_flow_action(action);
		sh->send_to_kernel_action.action = NULL;
	}
	if (sh->send_to_kernel_action.tbl) {
		struct mlx5_flow_tbl_resource *tbl =
				sh->send_to_kernel_action.tbl;

		flow_dv_tbl_resource_release(sh, tbl);
		sh->send_to_kernel_action.tbl = NULL;
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

	if (!priv->sh->config.dv_flow_en || !priv->sh->dr_drop_action)
		return;
	/**
	 * DR supports drop action placeholder when it is supported;
	 * otherwise, use the queue drop action.
	 */
	if (!priv->sh->drop_action_check_flag) {
		if (!mlx5_flow_discover_dr_action_support(dev))
			priv->sh->dr_drop_action_en = 1;
		priv->sh->drop_action_check_flag = 1;
	}
	if (priv->sh->dr_drop_action_en)
		priv->root_drop_action = priv->sh->dr_drop_action;
	else
		priv->root_drop_action = priv->drop_queue.hrxq->action;
#endif
}

static void
mlx5_queue_counter_id_prepare(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	void *ctx = priv->sh->cdev->ctx;

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
						    .pd = priv->sh->cdev->pd,
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
 * @param eth_da
 *   Device arguments.
 * @param mkvlist
 *   Pointer to mlx5 kvargs control, can be NULL if there is no devargs.
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
	       struct rte_eth_devargs *eth_da,
	       struct mlx5_kvargs_ctrl *mkvlist)
{
	const struct mlx5_switch_info *switch_info = &spawn->info;
	struct mlx5_dev_ctx_shared *sh = NULL;
	struct ibv_port_attr port_attr = { .state = IBV_PORT_NOP };
	struct rte_eth_dev *eth_dev = NULL;
	struct mlx5_priv *priv = NULL;
	int err = 0;
	struct rte_ether_addr mac;
	char name[RTE_ETH_NAME_MAX_LEN];
	int own_domain_id = 0;
	uint16_t port_id;
	struct mlx5_port_info vport_info = { .query_flags = 0 };
	int nl_rdma;
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
				       dpdk_dev->name, spawn->phys_dev_name);
		} else {
			err = snprintf(name, sizeof(name), "%s_%s_representor_c%dpf%d%s%u",
				dpdk_dev->name, spawn->phys_dev_name,
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
		/*
		 * When device is already spawned, its devargs should be set
		 * as used. otherwise, mlx5_kvargs_validate() will fail.
		 */
		if (mkvlist)
			mlx5_port_args_set_used(name, port_id, mkvlist);
		rte_errno = EEXIST;
		return NULL;
	}
	DRV_LOG(DEBUG, "naming Ethernet device \"%s\"", name);
	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		struct mlx5_mp_id mp_id;
		int fd;

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
		mlx5_mp_id_init(&mp_id, eth_dev->data->port_id);
		/* Receive command fd from primary process */
		fd = mlx5_mp_req_verbs_cmd_fd(&mp_id);
		if (fd < 0)
			goto err_secondary;
		/* Remap UAR for Tx queues. */
		err = mlx5_tx_uar_init_secondary(eth_dev, fd);
		close(fd);
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
	sh = mlx5_alloc_shared_dev_ctx(spawn, mkvlist);
	if (!sh)
		return NULL;
	nl_rdma = mlx5_nl_init(NETLINK_RDMA, 0);
	/* Check port status. */
	if (spawn->phys_port <= UINT8_MAX) {
		/* Legacy Verbs api only support u8 port number. */
		err = mlx5_glue->query_port(sh->cdev->ctx, spawn->phys_port,
					    &port_attr);
		if (err) {
			DRV_LOG(ERR, "port query failed: %s", strerror(err));
			goto error;
		}
		if (port_attr.link_layer != IBV_LINK_LAYER_ETHERNET) {
			DRV_LOG(ERR, "port is not configured in Ethernet mode");
			err = EINVAL;
			goto error;
		}
	} else if (nl_rdma >= 0) {
		/* IB doesn't allow more than 255 ports, must be Ethernet. */
		err = mlx5_nl_port_state(nl_rdma,
			spawn->phys_dev_name,
			spawn->phys_port);
		if (err < 0) {
			DRV_LOG(INFO, "Failed to get netlink port state: %s",
				strerror(rte_errno));
			err = -rte_errno;
			goto error;
		}
		port_attr.state = (enum ibv_port_state)err;
	}
	if (port_attr.state != IBV_PORT_ACTIVE)
		DRV_LOG(INFO, "port is not active: \"%s\" (%d)",
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
	/*
	 * When user configures remote PD and CTX and device creates RxQ by
	 * DevX, external RxQ is both supported and requested.
	 */
	if (mlx5_imported_pd_and_ctx(sh->cdev) && mlx5_devx_obj_ops_en(sh)) {
		priv->ext_rxqs = mlx5_malloc(MLX5_MEM_ZERO | MLX5_MEM_RTE,
					     sizeof(struct mlx5_external_rxq) *
					     MLX5_MAX_EXT_RX_QUEUES, 0,
					     SOCKET_ID_ANY);
		if (priv->ext_rxqs == NULL) {
			DRV_LOG(ERR, "Fail to allocate external RxQ array.");
			err = ENOMEM;
			goto error;
		}
		DRV_LOG(DEBUG, "External RxQ is supported.");
	}
	priv->sh = sh;
	priv->dev_port = spawn->phys_port;
	priv->pci_dev = spawn->pci_dev;
	priv->mtu = RTE_ETHER_MTU;
	/* Some internal functions rely on Netlink sockets, open them now. */
	priv->nl_socket_rdma = nl_rdma;
	priv->nl_socket_route =	mlx5_nl_init(NETLINK_ROUTE, 0);
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
	if (sh->esw_mode) {
		err = mlx5_glue->devx_port_query(sh->cdev->ctx,
						 spawn->phys_port,
						 &vport_info);
		if (err) {
			DRV_LOG(WARNING,
				"Cannot query devx port %d on device %s",
				spawn->phys_port, spawn->phys_dev_name);
			vport_info.query_flags = 0;
		}
	}
	if (vport_info.query_flags & MLX5_PORT_QUERY_REG_C0) {
		priv->vport_meta_tag = vport_info.vport_meta_tag;
		priv->vport_meta_mask = vport_info.vport_meta_mask;
		if (!priv->vport_meta_mask) {
			DRV_LOG(ERR,
				"vport zero mask for port %d on bonding device %s",
				spawn->phys_port, spawn->phys_dev_name);
			err = ENOTSUP;
			goto error;
		}
		if (priv->vport_meta_tag & ~priv->vport_meta_mask) {
			DRV_LOG(ERR,
				"Invalid vport tag for port %d on bonding device %s",
				spawn->phys_port, spawn->phys_dev_name);
			err = ENOTSUP;
			goto error;
		}
	}
	if (vport_info.query_flags & MLX5_PORT_QUERY_VPORT) {
		priv->vport_id = vport_info.vport_id;
	} else if (spawn->pf_bond >= 0 && sh->esw_mode) {
		DRV_LOG(ERR,
			"Cannot deduce vport index for port %d on bonding device %s",
			spawn->phys_port, spawn->phys_dev_name);
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
	if (sh->cdev->config.devx) {
		struct mlx5_hca_attr *hca_attr = &sh->cdev->config.hca_attr;

		sh->steering_format_version = hca_attr->steering_format_version;
#if defined(HAVE_MLX5DV_DR) && \
	(defined(HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER) || \
	 defined(HAVE_MLX5_DR_CREATE_ACTION_ASO))
		if (hca_attr->qos.sup && hca_attr->qos.flow_meter_old &&
		    sh->config.dv_flow_en) {
			uint8_t reg_c_mask = hca_attr->qos.flow_meter_reg_c_ids;
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
				priv->mtr_reg_share = hca_attr->qos.flow_meter;
				DRV_LOG(DEBUG, "The REG_C meter uses is %d",
					priv->mtr_color_reg);
			}
		}
		if (hca_attr->qos.sup && hca_attr->qos.flow_meter_aso_sup) {
			uint32_t log_obj_size =
				rte_log2_u32(MLX5_ASO_MTRS_PER_POOL >> 1);
			if (log_obj_size >=
			    hca_attr->qos.log_meter_aso_granularity &&
			    log_obj_size <=
			    hca_attr->qos.log_meter_aso_max_alloc)
				sh->meter_aso_en = 1;
		}
		if (priv->mtr_en) {
			err = mlx5_aso_flow_mtrs_mng_init(priv->sh);
			if (err) {
				err = -err;
				goto error;
			}
		}
		if (hca_attr->flow.tunnel_header_0_1)
			sh->tunnel_header_0_1 = 1;
		if (hca_attr->flow.tunnel_header_2_3)
			sh->tunnel_header_2_3 = 1;
#endif
#ifdef HAVE_MLX5_DR_CREATE_ACTION_ASO
		if (hca_attr->flow_hit_aso && priv->mtr_color_reg == REG_C_3) {
			sh->flow_hit_aso_en = 1;
			err = mlx5_flow_aso_age_mng_init(sh);
			if (err) {
				err = -err;
				goto error;
			}
			DRV_LOG(DEBUG, "Flow Hit ASO is supported.");
		}
#endif /* HAVE_MLX5_DR_CREATE_ACTION_ASO */
#if defined (HAVE_MLX5_DR_CREATE_ACTION_ASO) && \
    defined (HAVE_MLX5_DR_ACTION_ASO_CT)
		/* HWS create CT ASO SQ based on HWS configure queue number. */
		if (sh->config.dv_flow_en != 2 &&
		    hca_attr->ct_offload && priv->mtr_color_reg == REG_C_3) {
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
		if (hca_attr->log_max_ft_sampler_num > 0  &&
		    sh->config.dv_flow_en) {
			priv->sampler_en = 1;
			DRV_LOG(DEBUG, "Sampler enabled!");
		} else {
			priv->sampler_en = 0;
			if (!hca_attr->log_max_ft_sampler_num)
				DRV_LOG(WARNING,
					"No available register for sampler.");
			else
				DRV_LOG(DEBUG, "DV flow is not supported!");
		}
#endif
		if (hca_attr->lag_rx_port_affinity) {
			sh->lag_rx_port_affinity_en = 1;
			DRV_LOG(DEBUG, "LAG Rx Port Affinity enabled");
		}
		priv->num_lag_ports = hca_attr->num_lag_ports;
		DRV_LOG(DEBUG, "The number of lag ports is %d", priv->num_lag_ports);
	}
	/* Process parameters and store port configuration on priv structure. */
	err = mlx5_port_args_config(priv, mkvlist, &priv->config);
	if (err) {
		err = rte_errno;
		DRV_LOG(ERR, "Failed to process port configure: %s",
			strerror(rte_errno));
		goto error;
	}
	eth_dev = rte_eth_dev_allocate(name);
	if (eth_dev == NULL) {
		DRV_LOG(ERR, "can not allocate rte ethdev");
		err = ENOMEM;
		goto error;
	}
	if (priv->representor) {
		eth_dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR;
		eth_dev->data->representor_id = priv->representor_id;
		MLX5_ETH_FOREACH_DEV(port_id, dpdk_dev) {
			struct mlx5_priv *opriv =
				rte_eth_devices[port_id].data->dev_private;
			if (opriv &&
			    opriv->master &&
			    opriv->domain_id == priv->domain_id &&
			    opriv->sh == priv->sh) {
				eth_dev->data->backer_port_id = port_id;
				break;
			}
		}
		if (port_id >= RTE_MAX_ETHPORTS)
			eth_dev->data->backer_port_id = eth_dev->data->port_id;
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
	priv->lag_affinity_idx = sh->refcnt - 1;
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
	eth_dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
	eth_dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;
	eth_dev->dev_ops = &mlx5_dev_ops;
	eth_dev->rx_descriptor_status = mlx5_rx_descriptor_status;
	eth_dev->tx_descriptor_status = mlx5_tx_descriptor_status;
	eth_dev->rx_queue_count = mlx5_rx_queue_count;
	/* Register MAC address. */
	claim_zero(mlx5_mac_addr_add(eth_dev, &mac, 0, 0));
	if (sh->dev_cap.vf && sh->config.vf_nl_en)
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
	/* Bring Ethernet device up. */
	DRV_LOG(DEBUG, "port %u forcing Ethernet interface up",
		eth_dev->data->port_id);
	/* Read link status in case it is up and there will be no event. */
	mlx5_link_update(eth_dev, 0);
	/* Watch LSC interrupts between port probe and port start. */
	priv->sh->port[priv->dev_port - 1].nl_ih_port_id =
							eth_dev->data->port_id;
	mlx5_set_link_up(eth_dev);
	for (i = 0; i < MLX5_FLOW_TYPE_MAXI; i++) {
		icfg[i].release_mem_en = !!sh->config.reclaim_mode;
		if (sh->config.reclaim_mode)
			icfg[i].per_core_cache = 0;
		priv->flows[i] = mlx5_ipool_create(&icfg[i]);
		if (!priv->flows[i])
			goto error;
	}
	/* Create context for virtual machine VLAN workaround. */
	priv->vmwa_context = mlx5_vlan_vmwa_init(eth_dev, spawn->ifindex);
	if (sh->config.dv_flow_en) {
		err = mlx5_alloc_shared_dr(priv);
		if (err)
			goto error;
		if (mlx5_flex_item_port_init(eth_dev) < 0)
			goto error;
	}
	if (mlx5_devx_obj_ops_en(sh)) {
		priv->obj_ops = devx_obj_ops;
		mlx5_queue_counter_id_prepare(eth_dev);
		priv->obj_ops.lb_dummy_queue_create =
					mlx5_rxq_ibv_obj_dummy_lb_create;
		priv->obj_ops.lb_dummy_queue_release =
					mlx5_rxq_ibv_obj_dummy_lb_release;
	} else if (spawn->max_port > UINT8_MAX) {
		/* Verbs can't support ports larger than 255 by design. */
		DRV_LOG(ERR, "must enable DV and ESW when RDMA link ports > 255");
		err = ENOTSUP;
		goto error;
	} else {
		priv->obj_ops = ibv_obj_ops;
	}
	if (sh->config.tx_pp &&
	    priv->obj_ops.txq_obj_new != mlx5_txq_devx_obj_new) {
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
	priv->hrxqs = mlx5_list_create("hrxq", eth_dev, true,
				       mlx5_hrxq_create_cb,
				       mlx5_hrxq_match_cb,
				       mlx5_hrxq_remove_cb,
				       mlx5_hrxq_clone_cb,
				       mlx5_hrxq_clone_free_cb);
	if (!priv->hrxqs)
		goto error;
	mlx5_set_metadata_mask(eth_dev);
	if (sh->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY &&
	    !priv->sh->dv_regc0_mask) {
		DRV_LOG(ERR, "metadata mode %u is not supported "
			     "(no metadata reg_c[0] is available)",
			     sh->config.dv_xmeta_en);
			err = ENOTSUP;
			goto error;
	}
	rte_rwlock_init(&priv->ind_tbls_lock);
	if (priv->sh->config.dv_flow_en == 2) {
#ifdef HAVE_MLX5_HWS_SUPPORT
		if (priv->sh->config.dv_esw_en) {
			uint32_t usable_bits;
			uint32_t required_bits;

			if (priv->sh->dv_regc0_mask == UINT32_MAX) {
				DRV_LOG(ERR, "E-Switch port metadata is required when using HWS "
					     "but it is disabled (configure it through devlink)");
				err = ENOTSUP;
				goto error;
			}
			if (priv->sh->dv_regc0_mask == 0) {
				DRV_LOG(ERR, "E-Switch with HWS is not supported "
					     "(no available bits in reg_c[0])");
				err = ENOTSUP;
				goto error;
			}
			usable_bits = __builtin_popcount(priv->sh->dv_regc0_mask);
			required_bits = __builtin_popcount(priv->vport_meta_mask);
			if (usable_bits < required_bits) {
				DRV_LOG(ERR, "Not enough bits available in reg_c[0] to provide "
					     "representor matching.");
				err = ENOTSUP;
				goto error;
			}
		}
		if (priv->vport_meta_mask)
			flow_hw_set_port_info(eth_dev);
		if (priv->sh->config.dv_esw_en &&
		    priv->sh->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY &&
		    priv->sh->config.dv_xmeta_en != MLX5_XMETA_MODE_META32_HWS) {
			DRV_LOG(ERR,
				"metadata mode %u is not supported in HWS eswitch mode",
				priv->sh->config.dv_xmeta_en);
				err = ENOTSUP;
				goto error;
		}
		/* Only HWS requires this information. */
		flow_hw_init_tags_set(eth_dev);
		flow_hw_init_flow_metadata_config(eth_dev);
		if (priv->sh->config.dv_esw_en &&
		    flow_hw_create_vport_action(eth_dev)) {
			DRV_LOG(ERR, "port %u failed to create vport action",
				eth_dev->data->port_id);
			err = EINVAL;
			goto error;
		}
		/*
		 * If representor matching is disabled, PMD cannot create default flow rules
		 * to receive traffic for all ports, since implicit source port match is not added.
		 * Isolated mode is forced.
		 */
		if (priv->sh->config.dv_esw_en && !priv->sh->config.repr_matching) {
			err = mlx5_flow_isolate(eth_dev, 1, NULL);
			if (err < 0) {
				err = -err;
				goto error;
			}
			DRV_LOG(WARNING, "port %u ingress traffic is restricted to defined "
					 "flow rules (isolated mode) since representor "
					 "matching is disabled",
				eth_dev->data->port_id);
		}
		return eth_dev;
#else
		DRV_LOG(ERR, "DV support is missing for HWS.");
		goto error;
#endif
	}
	if (!priv->sh->flow_priority_check_flag) {
		/* Supported Verbs flow priority number detection. */
		err = mlx5_flow_discover_priorities(eth_dev);
		priv->sh->flow_max_priority = err;
		priv->sh->flow_priority_check_flag = 1;
	} else {
		err = priv->sh->flow_max_priority;
	}
	if (err < 0) {
		err = -err;
		goto error;
	}
	/* Query availability of metadata reg_c's. */
	if (!priv->sh->metadata_regc_check_flag) {
		err = mlx5_flow_discover_mreg_c(eth_dev);
		if (err < 0) {
			err = -err;
			goto error;
		}
	}
	if (!mlx5_flow_ext_mreg_supported(eth_dev)) {
		DRV_LOG(DEBUG,
			"port %u extensive metadata register is not supported",
			eth_dev->data->port_id);
		if (sh->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY) {
			DRV_LOG(ERR, "metadata mode %u is not supported "
				     "(no metadata registers available)",
				     sh->config.dv_xmeta_en);
			err = ENOTSUP;
			goto error;
		}
	}
	if (sh->config.dv_flow_en &&
	    sh->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY &&
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
	if (sh->config.dv_flow_en)
		eth_dev->data->dev_flags |= RTE_ETH_DEV_FLOW_OPS_THREAD_SAFE;
	return eth_dev;
error:
	if (priv) {
		priv->sh->port[priv->dev_port - 1].nl_ih_port_id =
							       RTE_MAX_ETHPORTS;
		rte_io_wmb();
#ifdef HAVE_MLX5_HWS_SUPPORT
		if (eth_dev &&
		    priv->sh &&
		    priv->sh->config.dv_flow_en == 2 &&
		    priv->sh->config.dv_esw_en)
			flow_hw_destroy_vport_action(eth_dev);
#endif
		if (priv->mreg_cp_tbl)
			mlx5_hlist_destroy(priv->mreg_cp_tbl);
		if (priv->sh)
			mlx5_os_free_shared_dr(priv);
		if (priv->nl_socket_route >= 0)
			close(priv->nl_socket_route);
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
		if (eth_dev && priv->flex_item_map)
			mlx5_flex_item_port_cleanup(eth_dev);
		mlx5_free(priv->ext_rxqs);
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
	if (nl_rdma >= 0)
		close(nl_rdma);
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
 * @param[in] ibdev_name
 *   Name of Infiniband device.
 * @param[in] pci_dev
 *   Pointer to primary PCI address structure to match.
 * @param[in] nl_rdma
 *   Netlink RDMA group socket handle.
 * @param[in] owner
 *   Representor owner PF index.
 * @param[out] bond_info
 *   Pointer to bonding information.
 *
 * @return
 *   negative value if no bonding device found, otherwise
 *   positive index of slave PF in bonding.
 */
static int
mlx5_device_bond_pci_match(const char *ibdev_name,
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
	uint8_t cur_guid[32] = {0};
	uint8_t guid[32] = {0};

	/*
	 * Try to get master device name. If something goes wrong suppose
	 * the lack of kernel support and no bonding devices.
	 */
	memset(bond_info, 0, sizeof(*bond_info));
	if (nl_rdma < 0)
		return -1;
	if (!strstr(ibdev_name, "bond"))
		return -1;
	np = mlx5_nl_portnum(nl_rdma, ibdev_name);
	if (!np)
		return -1;
	if (mlx5_get_device_guid(pci_dev, cur_guid, sizeof(cur_guid)) < 0)
		return -1;
	/*
	 * The master device might not be on the predefined port(not on port
	 * index 1, it is not guaranteed), we have to scan all Infiniband
	 * device ports and find master.
	 */
	for (i = 1; i <= np; ++i) {
		/* Check whether Infiniband port is populated. */
		ifindex = mlx5_nl_ifindex(nl_rdma, ibdev_name, i);
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
		int ret;

		/* Process slave interface names in the loop. */
		snprintf(tmp_str, sizeof(tmp_str),
			 "/sys/class/net/%s", ifname);
		if (mlx5_get_pci_addr(tmp_str, &pci_addr)) {
			DRV_LOG(WARNING,
				"Cannot get PCI address for netdev \"%s\".",
				ifname);
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
		/*
		 * Under socket direct mode, bonding will use
		 * system_image_guid as identification.
		 * After OFED 5.4, guid is readable (ret >= 0) under sysfs.
		 * All bonding members should have the same guid even if driver
		 * is using PCIe BDF.
		 */
		ret = mlx5_get_device_guid(&pci_addr, guid, sizeof(guid));
		if (ret < 0)
			break;
		else if (ret > 0) {
			if (!memcmp(guid, cur_guid, sizeof(guid)) &&
			    owner == info.port_name &&
			    (owner != 0 || (owner == 0 &&
			    !rte_pci_addr_cmp(pci_dev, &pci_addr))))
				pf = info.port_name;
		} else if (pci_dev->domain == pci_addr.domain &&
		    pci_dev->bus == pci_addr.bus &&
		    pci_dev->devid == pci_addr.devid &&
		    ((pci_dev->function == 0 &&
		      pci_dev->function + owner == pci_addr.function) ||
		     (pci_dev->function == owner &&
		      pci_addr.function == owner)))
			pf = info.port_name;
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
	if (owner == 0 && pf != 0) {
		DRV_LOG(INFO, "PCIe instance %04x:%02x:%02x.%x isn't bonding owner",
				pci_dev->domain, pci_dev->bus, pci_dev->devid,
				pci_dev->function);
	}
	return pf;
}

/**
 * Register a PCI device within bonding.
 *
 * This function spawns Ethernet devices out of a given PCI device and
 * bonding owner PF index.
 *
 * @param[in] cdev
 *   Pointer to common mlx5 device structure.
 * @param[in] req_eth_da
 *   Requested ethdev device argument.
 * @param[in] owner_id
 *   Requested owner PF port ID within bonding device, default to 0.
 * @param[in, out] mkvlist
 *   Pointer to mlx5 kvargs control, can be NULL if there is no devargs.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_os_pci_probe_pf(struct mlx5_common_device *cdev,
		     struct rte_eth_devargs *req_eth_da,
		     uint16_t owner_id, struct mlx5_kvargs_ctrl *mkvlist)
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
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(cdev->dev);
	struct mlx5_dev_spawn_data *list = NULL;
	struct rte_eth_devargs eth_da = *req_eth_da;
	struct rte_pci_addr owner_pci = pci_dev->addr; /* Owner PF. */
	struct mlx5_bond_info bond_info;
	int ret = -1;

	errno = 0;
	ibv_list = mlx5_glue->get_device_list(&ret);
	if (!ibv_list) {
		rte_errno = errno ? errno : ENOSYS;
		DRV_LOG(ERR, "Cannot list devices, is ib_uverbs loaded?");
		return -rte_errno;
	}
	/*
	 * First scan the list of all Infiniband devices to find
	 * matching ones, gathering into the list.
	 */
	struct ibv_device *ibv_match[ret + 1];
	int nl_route = mlx5_nl_init(NETLINK_ROUTE, 0);
	int nl_rdma = mlx5_nl_init(NETLINK_RDMA, 0);
	unsigned int i;

	while (ret-- > 0) {
		struct rte_pci_addr pci_addr;

		DRV_LOG(DEBUG, "Checking device \"%s\"", ibv_list[ret]->name);
		bd = mlx5_device_bond_pci_match(ibv_list[ret]->name, &owner_pci,
						nl_rdma, owner_id, &bond_info);
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
			DRV_LOG(INFO,
				"PCI information matches for slave %d bonding device \"%s\"",
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
			"PF %u doesn't have Verbs device matches PCI device " PCI_PRI_FMT ","
			" are kernel drivers loaded?",
			owner_id, owner_pci.domain, owner_pci.bus,
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
			DRV_LOG(WARNING,
				"Cannot get IB device \"%s\" ports number.",
				ibv_match[0]->name);
		if (bd >= 0 && !np) {
			DRV_LOG(ERR, "Cannot get ports for bonding device.");
			rte_errno = ENOENT;
			ret = -rte_errno;
			goto exit;
		}
	}
	/* Now we can determine the maximal amount of devices to be spawned. */
	list = mlx5_malloc(MLX5_MEM_ZERO,
			   sizeof(struct mlx5_dev_spawn_data) * (np ? np : nd),
			   RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (!list) {
		DRV_LOG(ERR, "Spawn data array allocation failure.");
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
			list[ns].phys_dev_name = ibv_match[0]->name;
			list[ns].eth_dev = NULL;
			list[ns].pci_dev = pci_dev;
			list[ns].cdev = cdev;
			list[ns].pf_bond = bd;
			list[ns].ifindex = mlx5_nl_ifindex(nl_rdma,
							   ibv_match[0]->name,
							   i);
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
				ret = mlx5_nl_switch_info(nl_route,
							  list[ns].ifindex,
							  &list[ns].info);
			if (ret || (!list[ns].info.representor &&
				    !list[ns].info.master)) {
				/*
				 * We failed to recognize representors with
				 * Netlink, let's try to perform the task
				 * with sysfs.
				 */
				ret = mlx5_sysfs_switch_info(list[ns].ifindex,
							     &list[ns].info);
			}
			if (!ret && bd >= 0) {
				switch (list[ns].info.name_type) {
				case MLX5_PHYS_PORT_NAME_TYPE_UPLINK:
					if (np == 1) {
						/*
						 * Force standalone bonding
						 * device for ROCE LAG
						 * configurations.
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
				"Unable to recognize master/representors on the IB device with multiple ports.");
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
			list[ns].phys_dev_name = ibv_match[i]->name;
			list[ns].eth_dev = NULL;
			list[ns].pci_dev = pci_dev;
			list[ns].cdev = cdev;
			list[ns].pf_bond = -1;
			list[ns].ifindex = 0;
			if (nl_rdma >= 0)
				list[ns].ifindex = mlx5_nl_ifindex
							    (nl_rdma,
							     ibv_match[i]->name,
							     1);
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
				ret = mlx5_nl_switch_info(nl_route,
							  list[ns].ifindex,
							  &list[ns].info);
			if (ret || (!list[ns].info.representor &&
				    !list[ns].info.master)) {
				/*
				 * We failed to recognize representors with
				 * Netlink, let's try to perform the task
				 * with sysfs.
				 */
				ret = mlx5_sysfs_switch_info(list[ns].ifindex,
							     &list[ns].info);
			}
			if (!ret && (list[ns].info.representor ^
				     list[ns].info.master)) {
				ns++;
			} else if ((nd == 1) &&
				   !list[ns].info.representor &&
				   !list[ns].info.master) {
				/*
				 * Single IB device with one physical port and
				 * attached network device.
				 * May be SRIOV is not enabled or there is no
				 * representors.
				 */
				DRV_LOG(INFO, "No E-Switch support detected.");
				ns++;
				break;
			}
		}
		if (!ns) {
			DRV_LOG(ERR,
				"Unable to recognize master/representors on the multiple IB devices.");
			rte_errno = ENOENT;
			ret = -rte_errno;
			goto exit;
		}
		/*
		 * New kernels may add the switch_id attribute for the case
		 * there is no E-Switch and we wrongly recognized the only
		 * device as master. Override this if there is the single
		 * device with single port and new device name format present.
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

		list[i].eth_dev = mlx5_dev_spawn(cdev->dev, &list[i], &eth_da,
						 mkvlist);
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
			struct rte_intr_handle *intr_handle =
				rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_SHARED);
			if (intr_handle == NULL) {
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
	if (eth_da->type == RTE_ETH_REPRESENTOR_NONE && dev->devargs->args) {
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
 * @param[in] cdev
 *   Pointer to common mlx5 device structure.
 * @param[in, out] mkvlist
 *   Pointer to mlx5 kvargs control, can be NULL if there is no devargs.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_os_pci_probe(struct mlx5_common_device *cdev,
		  struct mlx5_kvargs_ctrl *mkvlist)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(cdev->dev);
	struct rte_eth_devargs eth_da = { .nb_ports = 0 };
	int ret = 0;
	uint16_t p;

	ret = mlx5_os_parse_eth_devargs(cdev->dev, &eth_da);
	if (ret != 0)
		return ret;

	if (eth_da.nb_ports > 0) {
		/* Iterate all port if devargs pf is range: "pf[0-1]vf[...]". */
		for (p = 0; p < eth_da.nb_ports; p++) {
			ret = mlx5_os_pci_probe_pf(cdev, &eth_da,
						   eth_da.ports[p], mkvlist);
			if (ret) {
				DRV_LOG(INFO, "Probe of PCI device " PCI_PRI_FMT " "
					"aborted due to proding failure of PF %u",
					pci_dev->addr.domain, pci_dev->addr.bus,
					pci_dev->addr.devid, pci_dev->addr.function,
					eth_da.ports[p]);
				mlx5_net_remove(cdev);
				if (p != 0)
					break;
			}
		}
	} else {
		ret = mlx5_os_pci_probe_pf(cdev, &eth_da, 0, mkvlist);
	}
	return ret;
}

/* Probe a single SF device on auxiliary bus, no representor support. */
static int
mlx5_os_auxiliary_probe(struct mlx5_common_device *cdev,
			struct mlx5_kvargs_ctrl *mkvlist)
{
	struct rte_eth_devargs eth_da = { .nb_ports = 0 };
	struct mlx5_dev_spawn_data spawn = { .pf_bond = -1 };
	struct rte_device *dev = cdev->dev;
	struct rte_auxiliary_device *adev = RTE_DEV_TO_AUXILIARY(dev);
	struct rte_eth_dev *eth_dev;
	int ret = 0;

	/* Parse ethdev devargs. */
	ret = mlx5_os_parse_eth_devargs(dev, &eth_da);
	if (ret != 0)
		return ret;
	/* Init spawn data. */
	spawn.max_port = 1;
	spawn.phys_port = 1;
	spawn.phys_dev_name = mlx5_os_get_ctx_device_name(cdev->ctx);
	ret = mlx5_auxiliary_get_ifindex(dev->name);
	if (ret < 0) {
		DRV_LOG(ERR, "failed to get ethdev ifindex: %s", dev->name);
		return ret;
	}
	spawn.ifindex = ret;
	spawn.cdev = cdev;
	/* Spawn device. */
	eth_dev = mlx5_dev_spawn(dev, &spawn, &eth_da, mkvlist);
	if (eth_dev == NULL)
		return -rte_errno;
	/* Post create. */
	eth_dev->intr_handle = adev->intr_handle;
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
 * @param[in] cdev
 *   Pointer to the common mlx5 device.
 * @param[in, out] mkvlist
 *   Pointer to mlx5 kvargs control, can be NULL if there is no devargs.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_os_net_probe(struct mlx5_common_device *cdev,
		  struct mlx5_kvargs_ctrl *mkvlist)
{
	int ret;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		mlx5_pmd_socket_init();
	ret = mlx5_init_once();
	if (ret) {
		DRV_LOG(ERR, "Unable to init PMD global data: %s",
			strerror(rte_errno));
		return -rte_errno;
	}
	ret = mlx5_probe_again_args_validate(cdev, mkvlist);
	if (ret) {
		DRV_LOG(ERR, "Probe again parameters are not compatible : %s",
			strerror(rte_errno));
		return -rte_errno;
	}
	if (mlx5_dev_is_pci(cdev->dev))
		return mlx5_os_pci_probe(cdev, mkvlist);
	else
		return mlx5_os_auxiliary_probe(cdev, mkvlist);
}

/**
 * Cleanup resources when the last device is closed.
 */
void
mlx5_os_net_cleanup(void)
{
	mlx5_pmd_socket_uninit();
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
	struct ibv_context *ctx = sh->cdev->ctx;
	int nlsk_fd;

	sh->intr_handle = mlx5_os_interrupt_handler_create
		(RTE_INTR_INSTANCE_F_SHARED, true,
		 ctx->async_fd, mlx5_dev_interrupt_handler, sh);
	if (!sh->intr_handle) {
		DRV_LOG(ERR, "Failed to allocate intr_handle.");
		return;
	}
	nlsk_fd = mlx5_nl_init(NETLINK_ROUTE, RTMGRP_LINK);
	if (nlsk_fd < 0) {
		DRV_LOG(ERR, "Failed to create a socket for Netlink events: %s",
			rte_strerror(rte_errno));
		return;
	}
	sh->intr_handle_nl = mlx5_os_interrupt_handler_create
		(RTE_INTR_INSTANCE_F_SHARED, true,
		 nlsk_fd, mlx5_dev_interrupt_handler_nl, sh);
	if (sh->intr_handle_nl == NULL) {
		DRV_LOG(ERR, "Fail to allocate intr_handle");
		return;
	}
	if (sh->cdev->config.devx) {
#ifdef HAVE_IBV_DEVX_ASYNC
		struct mlx5dv_devx_cmd_comp *devx_comp;

		sh->devx_comp = (void *)mlx5_glue->devx_create_cmd_comp(ctx);
		devx_comp = sh->devx_comp;
		if (!devx_comp) {
			DRV_LOG(INFO, "failed to allocate devx_comp.");
			return;
		}
		sh->intr_handle_devx = mlx5_os_interrupt_handler_create
			(RTE_INTR_INSTANCE_F_SHARED, true,
			 devx_comp->fd,
			 mlx5_dev_interrupt_handler_devx, sh);
		if (!sh->intr_handle_devx) {
			DRV_LOG(ERR, "Failed to allocate intr_handle.");
			return;
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
	mlx5_os_interrupt_handler_destroy(sh->intr_handle,
					  mlx5_dev_interrupt_handler, sh);
	mlx5_os_interrupt_handler_destroy(sh->intr_handle_nl,
					  mlx5_dev_interrupt_handler_nl, sh);
#ifdef HAVE_IBV_DEVX_ASYNC
	mlx5_os_interrupt_handler_destroy(sh->intr_handle_devx,
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
	const int vf = priv->sh->dev_cap.vf;

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
	const int vf = priv->sh->dev_cap.vf;
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
