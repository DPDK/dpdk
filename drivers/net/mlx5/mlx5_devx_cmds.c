// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018 Mellanox Technologies, Ltd */

#include <rte_flow_driver.h>
#include <rte_malloc.h>
#include <unistd.h>

#include "mlx5.h"
#include "mlx5_glue.h"
#include "mlx5_prm.h"

/**
 * Allocate flow counters via devx interface.
 *
 * @param[in] ctx
 *   ibv contexts returned from mlx5dv_open_device.
 * @param dcs
 *   Pointer to counters properties structure to be filled by the routine.
 * @param bulk_n_128
 *   Bulk counter numbers in 128 counters units.
 *
 * @return
 *   Pointer to counter object on success, a negative value otherwise and
 *   rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_flow_counter_alloc(struct ibv_context *ctx, uint32_t bulk_n_128)
{
	struct mlx5_devx_obj *dcs = rte_zmalloc("dcs", sizeof(*dcs), 0);
	uint32_t in[MLX5_ST_SZ_DW(alloc_flow_counter_in)]   = {0};
	uint32_t out[MLX5_ST_SZ_DW(alloc_flow_counter_out)] = {0};

	if (!dcs) {
		rte_errno = ENOMEM;
		return NULL;
	}
	MLX5_SET(alloc_flow_counter_in, in, opcode,
		 MLX5_CMD_OP_ALLOC_FLOW_COUNTER);
	MLX5_SET(alloc_flow_counter_in, in, flow_counter_bulk, bulk_n_128);
	dcs->obj = mlx5_glue->devx_obj_create(ctx, in,
					      sizeof(in), out, sizeof(out));
	if (!dcs->obj) {
		DRV_LOG(ERR, "Can't allocate counters - error %d", errno);
		rte_errno = errno;
		rte_free(dcs);
		return NULL;
	}
	dcs->id = MLX5_GET(alloc_flow_counter_out, out, flow_counter_id);
	return dcs;
}

/**
 * Query flow counters values.
 *
 * @param[in] dcs
 *   devx object that was obtained from mlx5_devx_cmd_fc_alloc.
 * @param[in] clear
 *   Whether hardware should clear the counters after the query or not.
 * @param[in] n_counters
 *   0 in case of 1 counter to read, otherwise the counter number to read.
 *  @param pkts
 *   The number of packets that matched the flow.
 *  @param bytes
 *    The number of bytes that matched the flow.
 *  @param mkey
 *   The mkey key for batch query.
 *  @param addr
 *    The address in the mkey range for batch query.
 *  @param cmd_comp
 *   The completion object for asynchronous batch query.
 *  @param async_id
 *    The ID to be returned in the asynchronous batch query response.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int
mlx5_devx_cmd_flow_counter_query(struct mlx5_devx_obj *dcs,
				 int clear, uint32_t n_counters,
				 uint64_t *pkts, uint64_t *bytes,
				 uint32_t mkey, void *addr,
				 struct mlx5dv_devx_cmd_comp *cmd_comp,
				 uint64_t async_id)
{
	int out_len = MLX5_ST_SZ_BYTES(query_flow_counter_out) +
			MLX5_ST_SZ_BYTES(traffic_counter);
	uint32_t out[out_len];
	uint32_t in[MLX5_ST_SZ_DW(query_flow_counter_in)] = {0};
	void *stats;
	int rc;

	MLX5_SET(query_flow_counter_in, in, opcode,
		 MLX5_CMD_OP_QUERY_FLOW_COUNTER);
	MLX5_SET(query_flow_counter_in, in, op_mod, 0);
	MLX5_SET(query_flow_counter_in, in, flow_counter_id, dcs->id);
	MLX5_SET(query_flow_counter_in, in, clear, !!clear);

	if (n_counters) {
		MLX5_SET(query_flow_counter_in, in, num_of_counters,
			 n_counters);
		MLX5_SET(query_flow_counter_in, in, dump_to_memory, 1);
		MLX5_SET(query_flow_counter_in, in, mkey, mkey);
		MLX5_SET64(query_flow_counter_in, in, address,
			   (uint64_t)(uintptr_t)addr);
	}
	if (!cmd_comp)
		rc = mlx5_glue->devx_obj_query(dcs->obj, in, sizeof(in), out,
					       out_len);
	else
		rc = mlx5_glue->devx_obj_query_async(dcs->obj, in, sizeof(in),
						     out_len, async_id,
						     cmd_comp);
	if (rc) {
		DRV_LOG(ERR, "Failed to query devx counters with rc %d", rc);
		rte_errno = rc;
		return -rc;
	}
	if (!n_counters) {
		stats = MLX5_ADDR_OF(query_flow_counter_out,
				     out, flow_statistics);
		*pkts = MLX5_GET64(traffic_counter, stats, packets);
		*bytes = MLX5_GET64(traffic_counter, stats, octets);
	}
	return 0;
}

/**
 * Create a new mkey.
 *
 * @param[in] ctx
 *   ibv contexts returned from mlx5dv_open_device.
 * @param[in] attr
 *   Attributes of the requested mkey.
 *
 * @return
 *   Pointer to Devx mkey on success, a negative value otherwise and rte_errno
 *   is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_mkey_create(struct ibv_context *ctx,
			  struct mlx5_devx_mkey_attr *attr)
{
	uint32_t in[MLX5_ST_SZ_DW(create_mkey_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(create_mkey_out)] = {0};
	void *mkc;
	struct mlx5_devx_obj *mkey = rte_zmalloc("mkey", sizeof(*mkey), 0);
	size_t pgsize;
	uint32_t translation_size;

	if (!mkey) {
		rte_errno = ENOMEM;
		return NULL;
	}
	pgsize = sysconf(_SC_PAGESIZE);
	translation_size = (RTE_ALIGN(attr->size, pgsize) * 8) / 16;
	MLX5_SET(create_mkey_in, in, opcode, MLX5_CMD_OP_CREATE_MKEY);
	MLX5_SET(create_mkey_in, in, translations_octword_actual_size,
		 translation_size);
	MLX5_SET(create_mkey_in, in, mkey_umem_id, attr->umem_id);
	mkc = MLX5_ADDR_OF(create_mkey_in, in, memory_key_mkey_entry);
	MLX5_SET(mkc, mkc, lw, 0x1);
	MLX5_SET(mkc, mkc, lr, 0x1);
	MLX5_SET(mkc, mkc, access_mode_1_0, MLX5_MKC_ACCESS_MODE_MTT);
	MLX5_SET(mkc, mkc, qpn, 0xffffff);
	MLX5_SET(mkc, mkc, pd, attr->pd);
	MLX5_SET(mkc, mkc, mkey_7_0, attr->umem_id & 0xFF);
	MLX5_SET(mkc, mkc, translations_octword_size, translation_size);
	MLX5_SET64(mkc, mkc, start_addr, attr->addr);
	MLX5_SET64(mkc, mkc, len, attr->size);
	MLX5_SET(mkc, mkc, log_page_size, rte_log2_u32(pgsize));
	mkey->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in), out,
					       sizeof(out));
	if (!mkey->obj) {
		DRV_LOG(ERR, "Can't create mkey - error %d", errno);
		rte_errno = errno;
		rte_free(mkey);
		return NULL;
	}
	mkey->id = MLX5_GET(create_mkey_out, out, mkey_index);
	mkey->id = (mkey->id << 8) | (attr->umem_id & 0xFF);
	return mkey;
}

/**
 * Get status of devx command response.
 * Mainly used for asynchronous commands.
 *
 * @param[in] out
 *   The out response buffer.
 *
 * @return
 *   0 on success, non-zero value otherwise.
 */
int
mlx5_devx_get_out_command_status(void *out)
{
	int status;

	if (!out)
		return -EINVAL;
	status = MLX5_GET(query_flow_counter_out, out, status);
	if (status) {
		int syndrome = MLX5_GET(query_flow_counter_out, out, syndrome);

		DRV_LOG(ERR, "Bad devX status %x, syndrome = %x", status,
			syndrome);
	}
	return status;
}

/**
 * Destroy any object allocated by a Devx API.
 *
 * @param[in] obj
 *   Pointer to a general object.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int
mlx5_devx_cmd_destroy(struct mlx5_devx_obj *obj)
{
	int ret;

	if (!obj)
		return 0;
	ret =  mlx5_glue->devx_obj_destroy(obj->obj);
	rte_free(obj);
	return ret;
}

/**
 * Query NIC vport context.
 * Fills minimal inline attribute.
 *
 * @param[in] ctx
 *   ibv contexts returned from mlx5dv_open_device.
 * @param[in] vport
 *   vport index
 * @param[out] attr
 *   Attributes device values.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
static int
mlx5_devx_cmd_query_nic_vport_context(struct ibv_context *ctx,
				      unsigned int vport,
				      struct mlx5_hca_attr *attr)
{
	uint32_t in[MLX5_ST_SZ_DW(query_nic_vport_context_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(query_nic_vport_context_out)] = {0};
	void *vctx;
	int status, syndrome, rc;

	/* Query NIC vport context to determine inline mode. */
	MLX5_SET(query_nic_vport_context_in, in, opcode,
		 MLX5_CMD_OP_QUERY_NIC_VPORT_CONTEXT);
	MLX5_SET(query_nic_vport_context_in, in, vport_number, vport);
	if (vport)
		MLX5_SET(query_nic_vport_context_in, in, other_vport, 1);
	rc = mlx5_glue->devx_general_cmd(ctx,
					 in, sizeof(in),
					 out, sizeof(out));
	if (rc)
		goto error;
	status = MLX5_GET(query_nic_vport_context_out, out, status);
	syndrome = MLX5_GET(query_nic_vport_context_out, out, syndrome);
	if (status) {
		DRV_LOG(DEBUG, "Failed to query NIC vport context, "
			"status %x, syndrome = %x",
			status, syndrome);
		return -1;
	}
	vctx = MLX5_ADDR_OF(query_nic_vport_context_out, out,
			    nic_vport_context);
	attr->vport_inline_mode = MLX5_GET(nic_vport_context, vctx,
					   min_wqe_inline_mode);
	return 0;
error:
	rc = (rc > 0) ? -rc : rc;
	return rc;
}

/**
 * Query HCA attributes.
 * Using those attributes we can check on run time if the device
 * is having the required capabilities.
 *
 * @param[in] ctx
 *   ibv contexts returned from mlx5dv_open_device.
 * @param[out] attr
 *   Attributes device values.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int
mlx5_devx_cmd_query_hca_attr(struct ibv_context *ctx,
			     struct mlx5_hca_attr *attr)
{
	uint32_t in[MLX5_ST_SZ_DW(query_hca_cap_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(query_hca_cap_out)] = {0};
	void *hcattr;
	int status, syndrome, rc;

	MLX5_SET(query_hca_cap_in, in, opcode, MLX5_CMD_OP_QUERY_HCA_CAP);
	MLX5_SET(query_hca_cap_in, in, op_mod,
		 MLX5_GET_HCA_CAP_OP_MOD_GENERAL_DEVICE |
		 MLX5_HCA_CAP_OPMOD_GET_CUR);

	rc = mlx5_glue->devx_general_cmd(ctx,
					 in, sizeof(in), out, sizeof(out));
	if (rc)
		goto error;
	status = MLX5_GET(query_hca_cap_out, out, status);
	syndrome = MLX5_GET(query_hca_cap_out, out, syndrome);
	if (status) {
		DRV_LOG(DEBUG, "Failed to query devx HCA capabilities, "
			"status %x, syndrome = %x",
			status, syndrome);
		return -1;
	}
	hcattr = MLX5_ADDR_OF(query_hca_cap_out, out, capability);
	attr->flow_counter_bulk_alloc_bitmap =
			MLX5_GET(cmd_hca_cap, hcattr, flow_counter_bulk_alloc);
	attr->flow_counters_dump = MLX5_GET(cmd_hca_cap, hcattr,
					    flow_counters_dump);
	attr->eswitch_manager = MLX5_GET(cmd_hca_cap, hcattr, eswitch_manager);
	attr->hairpin = MLX5_GET(cmd_hca_cap, hcattr, hairpin);
	attr->log_max_hairpin_queues = MLX5_GET(cmd_hca_cap, hcattr,
						log_max_hairpin_queues);
	attr->log_max_hairpin_wq_data_sz = MLX5_GET(cmd_hca_cap, hcattr,
						    log_max_hairpin_wq_data_sz);
	attr->log_max_hairpin_num_packets = MLX5_GET
		(cmd_hca_cap, hcattr, log_min_hairpin_wq_data_sz);
	attr->vhca_id = MLX5_GET(cmd_hca_cap, hcattr, vhca_id);
	attr->qos.sup = MLX5_GET(cmd_hca_cap, hcattr, qos);
	if (attr->qos.sup) {
		MLX5_SET(query_hca_cap_in, in, op_mod,
			 MLX5_GET_HCA_CAP_OP_MOD_QOS_CAP |
			 MLX5_HCA_CAP_OPMOD_GET_CUR);
		rc = mlx5_glue->devx_general_cmd(ctx, in, sizeof(in),
						 out, sizeof(out));
		if (rc)
			goto error;
		if (status) {
			DRV_LOG(DEBUG, "Failed to query devx QOS capabilities,"
				" status %x, syndrome = %x",
				status, syndrome);
			return -1;
		}
		hcattr = MLX5_ADDR_OF(query_hca_cap_out, out, capability);
		attr->qos.srtcm_sup =
				MLX5_GET(qos_cap, hcattr, flow_meter_srtcm);
		attr->qos.log_max_flow_meter =
				MLX5_GET(qos_cap, hcattr, log_max_flow_meter);
		attr->qos.flow_meter_reg_c_ids =
			MLX5_GET(qos_cap, hcattr, flow_meter_reg_id);
	}
	attr->eth_net_offloads = MLX5_GET(cmd_hca_cap, hcattr,
					  eth_net_offloads);
	attr->eth_virt = MLX5_GET(cmd_hca_cap, hcattr, eth_virt);
	attr->flex_parser_protocols = MLX5_GET(cmd_hca_cap, hcattr,
					       flex_parser_protocols);
	if (!attr->eth_net_offloads)
		return 0;

	/* Query HCA offloads for Ethernet protocol. */
	memset(in, 0, sizeof(in));
	memset(out, 0, sizeof(out));
	MLX5_SET(query_hca_cap_in, in, opcode, MLX5_CMD_OP_QUERY_HCA_CAP);
	MLX5_SET(query_hca_cap_in, in, op_mod,
		 MLX5_GET_HCA_CAP_OP_MOD_ETHERNET_OFFLOAD_CAPS |
		 MLX5_HCA_CAP_OPMOD_GET_CUR);

	rc = mlx5_glue->devx_general_cmd(ctx,
					 in, sizeof(in),
					 out, sizeof(out));
	if (rc) {
		attr->eth_net_offloads = 0;
		goto error;
	}
	status = MLX5_GET(query_hca_cap_out, out, status);
	syndrome = MLX5_GET(query_hca_cap_out, out, syndrome);
	if (status) {
		DRV_LOG(DEBUG, "Failed to query devx HCA capabilities, "
			"status %x, syndrome = %x",
			status, syndrome);
		attr->eth_net_offloads = 0;
		return -1;
	}
	hcattr = MLX5_ADDR_OF(query_hca_cap_out, out, capability);
	attr->wqe_vlan_insert = MLX5_GET(per_protocol_networking_offload_caps,
					 hcattr, wqe_vlan_insert);
	attr->lro_cap = MLX5_GET(per_protocol_networking_offload_caps, hcattr,
				 lro_cap);
	attr->tunnel_lro_gre = MLX5_GET(per_protocol_networking_offload_caps,
					hcattr, tunnel_lro_gre);
	attr->tunnel_lro_vxlan = MLX5_GET(per_protocol_networking_offload_caps,
					  hcattr, tunnel_lro_vxlan);
	attr->lro_max_msg_sz_mode = MLX5_GET
					(per_protocol_networking_offload_caps,
					 hcattr, lro_max_msg_sz_mode);
	for (int i = 0 ; i < MLX5_LRO_NUM_SUPP_PERIODS ; i++) {
		attr->lro_timer_supported_periods[i] =
			MLX5_GET(per_protocol_networking_offload_caps, hcattr,
				 lro_timer_supported_periods[i]);
	}
	attr->tunnel_stateless_geneve_rx =
			    MLX5_GET(per_protocol_networking_offload_caps,
				     hcattr, tunnel_stateless_geneve_rx);
	attr->geneve_max_opt_len =
		    MLX5_GET(per_protocol_networking_offload_caps,
			     hcattr, max_geneve_opt_len);
	attr->wqe_inline_mode = MLX5_GET(per_protocol_networking_offload_caps,
					 hcattr, wqe_inline_mode);
	if (attr->wqe_inline_mode != MLX5_CAP_INLINE_MODE_VPORT_CONTEXT)
		return 0;
	if (attr->eth_virt) {
		rc = mlx5_devx_cmd_query_nic_vport_context(ctx, 0, attr);
		if (rc) {
			attr->eth_virt = 0;
			goto error;
		}
	}
	return 0;
error:
	rc = (rc > 0) ? -rc : rc;
	return rc;
}

/**
 * Query TIS transport domain from QP verbs object using DevX API.
 *
 * @param[in] qp
 *   Pointer to verbs QP returned by ibv_create_qp .
 * @param[in] tis_num
 *   TIS number of TIS to query.
 * @param[out] tis_td
 *   Pointer to TIS transport domain variable, to be set by the routine.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int
mlx5_devx_cmd_qp_query_tis_td(struct ibv_qp *qp, uint32_t tis_num,
			      uint32_t *tis_td)
{
	uint32_t in[MLX5_ST_SZ_DW(query_tis_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(query_tis_out)] = {0};
	int rc;
	void *tis_ctx;

	MLX5_SET(query_tis_in, in, opcode, MLX5_CMD_OP_QUERY_TIS);
	MLX5_SET(query_tis_in, in, tisn, tis_num);
	rc = mlx5_glue->devx_qp_query(qp, in, sizeof(in), out, sizeof(out));
	if (rc) {
		DRV_LOG(ERR, "Failed to query QP using DevX");
		return -rc;
	};
	tis_ctx = MLX5_ADDR_OF(query_tis_out, out, tis_context);
	*tis_td = MLX5_GET(tisc, tis_ctx, transport_domain);
	return 0;
}

/**
 * Fill WQ data for DevX API command.
 * Utility function for use when creating DevX objects containing a WQ.
 *
 * @param[in] wq_ctx
 *   Pointer to WQ context to fill with data.
 * @param [in] wq_attr
 *   Pointer to WQ attributes structure to fill in WQ context.
 */
static void
devx_cmd_fill_wq_data(void *wq_ctx, struct mlx5_devx_wq_attr *wq_attr)
{
	MLX5_SET(wq, wq_ctx, wq_type, wq_attr->wq_type);
	MLX5_SET(wq, wq_ctx, wq_signature, wq_attr->wq_signature);
	MLX5_SET(wq, wq_ctx, end_padding_mode, wq_attr->end_padding_mode);
	MLX5_SET(wq, wq_ctx, cd_slave, wq_attr->cd_slave);
	MLX5_SET(wq, wq_ctx, hds_skip_first_sge, wq_attr->hds_skip_first_sge);
	MLX5_SET(wq, wq_ctx, log2_hds_buf_size, wq_attr->log2_hds_buf_size);
	MLX5_SET(wq, wq_ctx, page_offset, wq_attr->page_offset);
	MLX5_SET(wq, wq_ctx, lwm, wq_attr->lwm);
	MLX5_SET(wq, wq_ctx, pd, wq_attr->pd);
	MLX5_SET(wq, wq_ctx, uar_page, wq_attr->uar_page);
	MLX5_SET64(wq, wq_ctx, dbr_addr, wq_attr->dbr_addr);
	MLX5_SET(wq, wq_ctx, hw_counter, wq_attr->hw_counter);
	MLX5_SET(wq, wq_ctx, sw_counter, wq_attr->sw_counter);
	MLX5_SET(wq, wq_ctx, log_wq_stride, wq_attr->log_wq_stride);
	MLX5_SET(wq, wq_ctx, log_wq_pg_sz, wq_attr->log_wq_pg_sz);
	MLX5_SET(wq, wq_ctx, log_wq_sz, wq_attr->log_wq_sz);
	MLX5_SET(wq, wq_ctx, dbr_umem_valid, wq_attr->dbr_umem_valid);
	MLX5_SET(wq, wq_ctx, wq_umem_valid, wq_attr->wq_umem_valid);
	MLX5_SET(wq, wq_ctx, log_hairpin_num_packets,
		 wq_attr->log_hairpin_num_packets);
	MLX5_SET(wq, wq_ctx, log_hairpin_data_sz, wq_attr->log_hairpin_data_sz);
	MLX5_SET(wq, wq_ctx, single_wqe_log_num_of_strides,
		 wq_attr->single_wqe_log_num_of_strides);
	MLX5_SET(wq, wq_ctx, two_byte_shift_en, wq_attr->two_byte_shift_en);
	MLX5_SET(wq, wq_ctx, single_stride_log_num_of_bytes,
		 wq_attr->single_stride_log_num_of_bytes);
	MLX5_SET(wq, wq_ctx, dbr_umem_id, wq_attr->dbr_umem_id);
	MLX5_SET(wq, wq_ctx, wq_umem_id, wq_attr->wq_umem_id);
	MLX5_SET64(wq, wq_ctx, wq_umem_offset, wq_attr->wq_umem_offset);
}

/**
 * Create RQ using DevX API.
 *
 * @param[in] ctx
 *   ibv_context returned from mlx5dv_open_device.
 * @param [in] rq_attr
 *   Pointer to create RQ attributes structure.
 * @param [in] socket
 *   CPU socket ID for allocations.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_rq(struct ibv_context *ctx,
			struct mlx5_devx_create_rq_attr *rq_attr,
			int socket)
{
	uint32_t in[MLX5_ST_SZ_DW(create_rq_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(create_rq_out)] = {0};
	void *rq_ctx, *wq_ctx;
	struct mlx5_devx_wq_attr *wq_attr;
	struct mlx5_devx_obj *rq = NULL;

	rq = rte_calloc_socket(__func__, 1, sizeof(*rq), 0, socket);
	if (!rq) {
		DRV_LOG(ERR, "Failed to allocate RQ data");
		rte_errno = ENOMEM;
		return NULL;
	}
	MLX5_SET(create_rq_in, in, opcode, MLX5_CMD_OP_CREATE_RQ);
	rq_ctx = MLX5_ADDR_OF(create_rq_in, in, ctx);
	MLX5_SET(rqc, rq_ctx, rlky, rq_attr->rlky);
	MLX5_SET(rqc, rq_ctx, delay_drop_en, rq_attr->delay_drop_en);
	MLX5_SET(rqc, rq_ctx, scatter_fcs, rq_attr->scatter_fcs);
	MLX5_SET(rqc, rq_ctx, vsd, rq_attr->vsd);
	MLX5_SET(rqc, rq_ctx, mem_rq_type, rq_attr->mem_rq_type);
	MLX5_SET(rqc, rq_ctx, state, rq_attr->state);
	MLX5_SET(rqc, rq_ctx, flush_in_error_en, rq_attr->flush_in_error_en);
	MLX5_SET(rqc, rq_ctx, hairpin, rq_attr->hairpin);
	MLX5_SET(rqc, rq_ctx, user_index, rq_attr->user_index);
	MLX5_SET(rqc, rq_ctx, cqn, rq_attr->cqn);
	MLX5_SET(rqc, rq_ctx, counter_set_id, rq_attr->counter_set_id);
	MLX5_SET(rqc, rq_ctx, rmpn, rq_attr->rmpn);
	wq_ctx = MLX5_ADDR_OF(rqc, rq_ctx, wq);
	wq_attr = &rq_attr->wq_attr;
	devx_cmd_fill_wq_data(wq_ctx, wq_attr);
	rq->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in),
						  out, sizeof(out));
	if (!rq->obj) {
		DRV_LOG(ERR, "Failed to create RQ using DevX");
		rte_errno = errno;
		rte_free(rq);
		return NULL;
	}
	rq->id = MLX5_GET(create_rq_out, out, rqn);
	return rq;
}

/**
 * Modify RQ using DevX API.
 *
 * @param[in] rq
 *   Pointer to RQ object structure.
 * @param [in] rq_attr
 *   Pointer to modify RQ attributes structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_devx_cmd_modify_rq(struct mlx5_devx_obj *rq,
			struct mlx5_devx_modify_rq_attr *rq_attr)
{
	uint32_t in[MLX5_ST_SZ_DW(modify_rq_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(modify_rq_out)] = {0};
	void *rq_ctx, *wq_ctx;
	int ret;

	MLX5_SET(modify_rq_in, in, opcode, MLX5_CMD_OP_MODIFY_RQ);
	MLX5_SET(modify_rq_in, in, rq_state, rq_attr->rq_state);
	MLX5_SET(modify_rq_in, in, rqn, rq->id);
	MLX5_SET64(modify_rq_in, in, modify_bitmask, rq_attr->modify_bitmask);
	rq_ctx = MLX5_ADDR_OF(modify_rq_in, in, ctx);
	MLX5_SET(rqc, rq_ctx, state, rq_attr->state);
	if (rq_attr->modify_bitmask &
			MLX5_MODIFY_RQ_IN_MODIFY_BITMASK_SCATTER_FCS)
		MLX5_SET(rqc, rq_ctx, scatter_fcs, rq_attr->scatter_fcs);
	if (rq_attr->modify_bitmask & MLX5_MODIFY_RQ_IN_MODIFY_BITMASK_VSD)
		MLX5_SET(rqc, rq_ctx, vsd, rq_attr->vsd);
	if (rq_attr->modify_bitmask &
			MLX5_MODIFY_RQ_IN_MODIFY_BITMASK_RQ_COUNTER_SET_ID)
		MLX5_SET(rqc, rq_ctx, counter_set_id, rq_attr->counter_set_id);
	MLX5_SET(rqc, rq_ctx, hairpin_peer_sq, rq_attr->hairpin_peer_sq);
	MLX5_SET(rqc, rq_ctx, hairpin_peer_vhca, rq_attr->hairpin_peer_vhca);
	if (rq_attr->modify_bitmask & MLX5_MODIFY_RQ_IN_MODIFY_BITMASK_WQ_LWM) {
		wq_ctx = MLX5_ADDR_OF(rqc, rq_ctx, wq);
		MLX5_SET(wq, wq_ctx, lwm, rq_attr->lwm);
	}
	ret = mlx5_glue->devx_obj_modify(rq->obj, in, sizeof(in),
					 out, sizeof(out));
	if (ret) {
		DRV_LOG(ERR, "Failed to modify RQ using DevX");
		rte_errno = errno;
		return -errno;
	}
	return ret;
}

/**
 * Create TIR using DevX API.
 *
 * @param[in] ctx
 *   ibv_context returned from mlx5dv_open_device.
 * @param [in] tir_attr
 *   Pointer to TIR attributes structure.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_tir(struct ibv_context *ctx,
			 struct mlx5_devx_tir_attr *tir_attr)
{
	uint32_t in[MLX5_ST_SZ_DW(create_tir_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(create_tir_out)] = {0};
	void *tir_ctx, *outer, *inner;
	struct mlx5_devx_obj *tir = NULL;
	int i;

	tir = rte_calloc(__func__, 1, sizeof(*tir), 0);
	if (!tir) {
		DRV_LOG(ERR, "Failed to allocate TIR data");
		rte_errno = ENOMEM;
		return NULL;
	}
	MLX5_SET(create_tir_in, in, opcode, MLX5_CMD_OP_CREATE_TIR);
	tir_ctx = MLX5_ADDR_OF(create_tir_in, in, ctx);
	MLX5_SET(tirc, tir_ctx, disp_type, tir_attr->disp_type);
	MLX5_SET(tirc, tir_ctx, lro_timeout_period_usecs,
		 tir_attr->lro_timeout_period_usecs);
	MLX5_SET(tirc, tir_ctx, lro_enable_mask, tir_attr->lro_enable_mask);
	MLX5_SET(tirc, tir_ctx, lro_max_msg_sz, tir_attr->lro_max_msg_sz);
	MLX5_SET(tirc, tir_ctx, inline_rqn, tir_attr->inline_rqn);
	MLX5_SET(tirc, tir_ctx, rx_hash_symmetric, tir_attr->rx_hash_symmetric);
	MLX5_SET(tirc, tir_ctx, tunneled_offload_en,
		 tir_attr->tunneled_offload_en);
	MLX5_SET(tirc, tir_ctx, indirect_table, tir_attr->indirect_table);
	MLX5_SET(tirc, tir_ctx, rx_hash_fn, tir_attr->rx_hash_fn);
	MLX5_SET(tirc, tir_ctx, self_lb_block, tir_attr->self_lb_block);
	MLX5_SET(tirc, tir_ctx, transport_domain, tir_attr->transport_domain);
	for (i = 0; i < 10; i++) {
		MLX5_SET(tirc, tir_ctx, rx_hash_toeplitz_key[i],
			 tir_attr->rx_hash_toeplitz_key[i]);
	}
	outer = MLX5_ADDR_OF(tirc, tir_ctx, rx_hash_field_selector_outer);
	MLX5_SET(rx_hash_field_select, outer, l3_prot_type,
		 tir_attr->rx_hash_field_selector_outer.l3_prot_type);
	MLX5_SET(rx_hash_field_select, outer, l4_prot_type,
		 tir_attr->rx_hash_field_selector_outer.l4_prot_type);
	MLX5_SET(rx_hash_field_select, outer, selected_fields,
		 tir_attr->rx_hash_field_selector_outer.selected_fields);
	inner = MLX5_ADDR_OF(tirc, tir_ctx, rx_hash_field_selector_inner);
	MLX5_SET(rx_hash_field_select, inner, l3_prot_type,
		 tir_attr->rx_hash_field_selector_inner.l3_prot_type);
	MLX5_SET(rx_hash_field_select, inner, l4_prot_type,
		 tir_attr->rx_hash_field_selector_inner.l4_prot_type);
	MLX5_SET(rx_hash_field_select, inner, selected_fields,
		 tir_attr->rx_hash_field_selector_inner.selected_fields);
	tir->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in),
						   out, sizeof(out));
	if (!tir->obj) {
		DRV_LOG(ERR, "Failed to create TIR using DevX");
		rte_errno = errno;
		rte_free(tir);
		return NULL;
	}
	tir->id = MLX5_GET(create_tir_out, out, tirn);
	return tir;
}

/**
 * Create RQT using DevX API.
 *
 * @param[in] ctx
 *   ibv_context returned from mlx5dv_open_device.
 * @param [in] rqt_attr
 *   Pointer to RQT attributes structure.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_rqt(struct ibv_context *ctx,
			 struct mlx5_devx_rqt_attr *rqt_attr)
{
	uint32_t *in = NULL;
	uint32_t inlen = MLX5_ST_SZ_BYTES(create_rqt_in) +
			 rqt_attr->rqt_actual_size * sizeof(uint32_t);
	uint32_t out[MLX5_ST_SZ_DW(create_rqt_out)] = {0};
	void *rqt_ctx;
	struct mlx5_devx_obj *rqt = NULL;
	int i;

	in = rte_calloc(__func__, 1, inlen, 0);
	if (!in) {
		DRV_LOG(ERR, "Failed to allocate RQT IN data");
		rte_errno = ENOMEM;
		return NULL;
	}
	rqt = rte_calloc(__func__, 1, sizeof(*rqt), 0);
	if (!rqt) {
		DRV_LOG(ERR, "Failed to allocate RQT data");
		rte_errno = ENOMEM;
		rte_free(in);
		return NULL;
	}
	MLX5_SET(create_rqt_in, in, opcode, MLX5_CMD_OP_CREATE_RQT);
	rqt_ctx = MLX5_ADDR_OF(create_rqt_in, in, rqt_context);
	MLX5_SET(rqtc, rqt_ctx, rqt_max_size, rqt_attr->rqt_max_size);
	MLX5_SET(rqtc, rqt_ctx, rqt_actual_size, rqt_attr->rqt_actual_size);
	for (i = 0; i < rqt_attr->rqt_actual_size; i++)
		MLX5_SET(rqtc, rqt_ctx, rq_num[i], rqt_attr->rq_list[i]);
	rqt->obj = mlx5_glue->devx_obj_create(ctx, in, inlen, out, sizeof(out));
	rte_free(in);
	if (!rqt->obj) {
		DRV_LOG(ERR, "Failed to create RQT using DevX");
		rte_errno = errno;
		rte_free(rqt);
		return NULL;
	}
	rqt->id = MLX5_GET(create_rqt_out, out, rqtn);
	return rqt;
}

/**
 * Create SQ using DevX API.
 *
 * @param[in] ctx
 *   ibv_context returned from mlx5dv_open_device.
 * @param [in] sq_attr
 *   Pointer to SQ attributes structure.
 * @param [in] socket
 *   CPU socket ID for allocations.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 **/
struct mlx5_devx_obj *
mlx5_devx_cmd_create_sq(struct ibv_context *ctx,
			struct mlx5_devx_create_sq_attr *sq_attr)
{
	uint32_t in[MLX5_ST_SZ_DW(create_sq_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(create_sq_out)] = {0};
	void *sq_ctx;
	void *wq_ctx;
	struct mlx5_devx_wq_attr *wq_attr;
	struct mlx5_devx_obj *sq = NULL;

	sq = rte_calloc(__func__, 1, sizeof(*sq), 0);
	if (!sq) {
		DRV_LOG(ERR, "Failed to allocate SQ data");
		rte_errno = ENOMEM;
		return NULL;
	}
	MLX5_SET(create_sq_in, in, opcode, MLX5_CMD_OP_CREATE_SQ);
	sq_ctx = MLX5_ADDR_OF(create_sq_in, in, ctx);
	MLX5_SET(sqc, sq_ctx, rlky, sq_attr->rlky);
	MLX5_SET(sqc, sq_ctx, cd_master, sq_attr->cd_master);
	MLX5_SET(sqc, sq_ctx, fre, sq_attr->fre);
	MLX5_SET(sqc, sq_ctx, flush_in_error_en, sq_attr->flush_in_error_en);
	MLX5_SET(sqc, sq_ctx, allow_multi_pkt_send_wqe,
		 sq_attr->flush_in_error_en);
	MLX5_SET(sqc, sq_ctx, min_wqe_inline_mode,
		 sq_attr->min_wqe_inline_mode);
	MLX5_SET(sqc, sq_ctx, state, sq_attr->state);
	MLX5_SET(sqc, sq_ctx, reg_umr, sq_attr->reg_umr);
	MLX5_SET(sqc, sq_ctx, allow_swp, sq_attr->allow_swp);
	MLX5_SET(sqc, sq_ctx, hairpin, sq_attr->hairpin);
	MLX5_SET(sqc, sq_ctx, user_index, sq_attr->user_index);
	MLX5_SET(sqc, sq_ctx, cqn, sq_attr->cqn);
	MLX5_SET(sqc, sq_ctx, packet_pacing_rate_limit_index,
		 sq_attr->packet_pacing_rate_limit_index);
	MLX5_SET(sqc, sq_ctx, tis_lst_sz, sq_attr->tis_lst_sz);
	MLX5_SET(sqc, sq_ctx, tis_num_0, sq_attr->tis_num);
	wq_ctx = MLX5_ADDR_OF(sqc, sq_ctx, wq);
	wq_attr = &sq_attr->wq_attr;
	devx_cmd_fill_wq_data(wq_ctx, wq_attr);
	sq->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in),
					     out, sizeof(out));
	if (!sq->obj) {
		DRV_LOG(ERR, "Failed to create SQ using DevX");
		rte_errno = errno;
		rte_free(sq);
		return NULL;
	}
	sq->id = MLX5_GET(create_sq_out, out, sqn);
	return sq;
}

/**
 * Modify SQ using DevX API.
 *
 * @param[in] sq
 *   Pointer to SQ object structure.
 * @param [in] sq_attr
 *   Pointer to SQ attributes structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_devx_cmd_modify_sq(struct mlx5_devx_obj *sq,
			struct mlx5_devx_modify_sq_attr *sq_attr)
{
	uint32_t in[MLX5_ST_SZ_DW(modify_sq_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(modify_sq_out)] = {0};
	void *sq_ctx;
	int ret;

	MLX5_SET(modify_sq_in, in, opcode, MLX5_CMD_OP_MODIFY_SQ);
	MLX5_SET(modify_sq_in, in, sq_state, sq_attr->sq_state);
	MLX5_SET(modify_sq_in, in, sqn, sq->id);
	sq_ctx = MLX5_ADDR_OF(modify_sq_in, in, ctx);
	MLX5_SET(sqc, sq_ctx, state, sq_attr->state);
	MLX5_SET(sqc, sq_ctx, hairpin_peer_rq, sq_attr->hairpin_peer_rq);
	MLX5_SET(sqc, sq_ctx, hairpin_peer_vhca, sq_attr->hairpin_peer_vhca);
	ret = mlx5_glue->devx_obj_modify(sq->obj, in, sizeof(in),
					 out, sizeof(out));
	if (ret) {
		DRV_LOG(ERR, "Failed to modify SQ using DevX");
		rte_errno = errno;
		return -errno;
	}
	return ret;
}

/**
 * Create TIS using DevX API.
 *
 * @param[in] ctx
 *   ibv_context returned from mlx5dv_open_device.
 * @param [in] tis_attr
 *   Pointer to TIS attributes structure.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_tis(struct ibv_context *ctx,
			 struct mlx5_devx_tis_attr *tis_attr)
{
	uint32_t in[MLX5_ST_SZ_DW(create_tis_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(create_tis_out)] = {0};
	struct mlx5_devx_obj *tis = NULL;
	void *tis_ctx;

	tis = rte_calloc(__func__, 1, sizeof(*tis), 0);
	if (!tis) {
		DRV_LOG(ERR, "Failed to allocate TIS object");
		rte_errno = ENOMEM;
		return NULL;
	}
	MLX5_SET(create_tis_in, in, opcode, MLX5_CMD_OP_CREATE_TIS);
	tis_ctx = MLX5_ADDR_OF(create_tis_in, in, ctx);
	MLX5_SET(tisc, tis_ctx, strict_lag_tx_port_affinity,
		 tis_attr->strict_lag_tx_port_affinity);
	MLX5_SET(tisc, tis_ctx, strict_lag_tx_port_affinity,
		 tis_attr->strict_lag_tx_port_affinity);
	MLX5_SET(tisc, tis_ctx, prio, tis_attr->prio);
	MLX5_SET(tisc, tis_ctx, transport_domain,
		 tis_attr->transport_domain);
	tis->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in),
					      out, sizeof(out));
	if (!tis->obj) {
		DRV_LOG(ERR, "Failed to create TIS using DevX");
		rte_errno = errno;
		rte_free(tis);
		return NULL;
	}
	tis->id = MLX5_GET(create_tis_out, out, tisn);
	return tis;
}

/**
 * Create transport domain using DevX API.
 *
 * @param[in] ctx
 *   ibv_context returned from mlx5dv_open_device.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_td(struct ibv_context *ctx)
{
	uint32_t in[MLX5_ST_SZ_DW(alloc_transport_domain_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(alloc_transport_domain_out)] = {0};
	struct mlx5_devx_obj *td = NULL;

	td = rte_calloc(__func__, 1, sizeof(*td), 0);
	if (!td) {
		DRV_LOG(ERR, "Failed to allocate TD object");
		rte_errno = ENOMEM;
		return NULL;
	}
	MLX5_SET(alloc_transport_domain_in, in, opcode,
		 MLX5_CMD_OP_ALLOC_TRANSPORT_DOMAIN);
	td->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in),
					     out, sizeof(out));
	if (!td->obj) {
		DRV_LOG(ERR, "Failed to create TIS using DevX");
		rte_errno = errno;
		rte_free(td);
		return NULL;
	}
	td->id = MLX5_GET(alloc_transport_domain_out, out,
			   transport_domain);
	return td;
}
