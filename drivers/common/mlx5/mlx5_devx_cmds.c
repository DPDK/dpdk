/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <unistd.h>

#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_eal_paging.h>

#include "mlx5_prm.h"
#include "mlx5_devx_cmds.h"
#include "mlx5_common_log.h"
#include "mlx5_malloc.h"

static void *
mlx5_devx_get_hca_cap(void *ctx, uint32_t *in, uint32_t *out,
		      int *err, uint32_t flags)
{
	const size_t size_in = MLX5_ST_SZ_DW(query_hca_cap_in) * sizeof(int);
	const size_t size_out = MLX5_ST_SZ_DW(query_hca_cap_out) * sizeof(int);
	int status, syndrome, rc;

	if (err)
		*err = 0;
	memset(in, 0, size_in);
	memset(out, 0, size_out);
	MLX5_SET(query_hca_cap_in, in, opcode, MLX5_CMD_OP_QUERY_HCA_CAP);
	MLX5_SET(query_hca_cap_in, in, op_mod, flags);
	rc = mlx5_glue->devx_general_cmd(ctx, in, size_in, out, size_out);
	if (rc) {
		DRV_LOG(ERR,
			"Failed to query devx HCA capabilities func %#02x",
			flags >> 1);
		if (err)
			*err = rc > 0 ? -rc : rc;
		return NULL;
	}
	status = MLX5_GET(query_hca_cap_out, out, status);
	syndrome = MLX5_GET(query_hca_cap_out, out, syndrome);
	if (status) {
		DRV_LOG(ERR,
			"Failed to query devx HCA capabilities func %#02x status %x, syndrome = %x",
			flags >> 1, status, syndrome);
		if (err)
			*err = -1;
		return NULL;
	}
	return MLX5_ADDR_OF(query_hca_cap_out, out, capability);
}

/**
 * Perform read access to the registers. Reads data from register
 * and writes ones to the specified buffer.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param[in] reg_id
 *   Register identifier according to the PRM.
 * @param[in] arg
 *   Register access auxiliary parameter according to the PRM.
 * @param[out] data
 *   Pointer to the buffer to store read data.
 * @param[in] dw_cnt
 *   Buffer size in double words.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int
mlx5_devx_cmd_register_read(void *ctx, uint16_t reg_id, uint32_t arg,
			    uint32_t *data, uint32_t dw_cnt)
{
	uint32_t in[MLX5_ST_SZ_DW(access_register_in)]   = {0};
	uint32_t out[MLX5_ST_SZ_DW(access_register_out) +
		     MLX5_ACCESS_REGISTER_DATA_DWORD_MAX] = {0};
	int status, rc;

	MLX5_ASSERT(data && dw_cnt);
	MLX5_ASSERT(dw_cnt <= MLX5_ACCESS_REGISTER_DATA_DWORD_MAX);
	if (dw_cnt  > MLX5_ACCESS_REGISTER_DATA_DWORD_MAX) {
		DRV_LOG(ERR, "Not enough  buffer for register read data");
		return -1;
	}
	MLX5_SET(access_register_in, in, opcode,
		 MLX5_CMD_OP_ACCESS_REGISTER_USER);
	MLX5_SET(access_register_in, in, op_mod,
					MLX5_ACCESS_REGISTER_IN_OP_MOD_READ);
	MLX5_SET(access_register_in, in, register_id, reg_id);
	MLX5_SET(access_register_in, in, argument, arg);
	rc = mlx5_glue->devx_general_cmd(ctx, in, sizeof(in), out,
					 MLX5_ST_SZ_BYTES(access_register_out) +
					 sizeof(uint32_t) * dw_cnt);
	if (rc)
		goto error;
	status = MLX5_GET(access_register_out, out, status);
	if (status) {
		int syndrome = MLX5_GET(access_register_out, out, syndrome);

		DRV_LOG(DEBUG, "Failed to read access NIC register 0x%X, "
			       "status %x, syndrome = %x",
			       reg_id, status, syndrome);
		return -1;
	}
	memcpy(data, &out[MLX5_ST_SZ_DW(access_register_out)],
	       dw_cnt * sizeof(uint32_t));
	return 0;
error:
	rc = (rc > 0) ? -rc : rc;
	return rc;
}

/**
 * Perform write access to the registers.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param[in] reg_id
 *   Register identifier according to the PRM.
 * @param[in] arg
 *   Register access auxiliary parameter according to the PRM.
 * @param[out] data
 *   Pointer to the buffer containing data to write.
 * @param[in] dw_cnt
 *   Buffer size in double words (32bit units).
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int
mlx5_devx_cmd_register_write(void *ctx, uint16_t reg_id, uint32_t arg,
			     uint32_t *data, uint32_t dw_cnt)
{
	uint32_t in[MLX5_ST_SZ_DW(access_register_in) +
		    MLX5_ACCESS_REGISTER_DATA_DWORD_MAX] = {0};
	uint32_t out[MLX5_ST_SZ_DW(access_register_out)] = {0};
	int status, rc;
	void *ptr;

	MLX5_ASSERT(data && dw_cnt);
	MLX5_ASSERT(dw_cnt <= MLX5_ACCESS_REGISTER_DATA_DWORD_MAX);
	if (dw_cnt > MLX5_ACCESS_REGISTER_DATA_DWORD_MAX) {
		DRV_LOG(ERR, "Data to write exceeds max size");
		return -1;
	}
	MLX5_SET(access_register_in, in, opcode,
		 MLX5_CMD_OP_ACCESS_REGISTER_USER);
	MLX5_SET(access_register_in, in, op_mod,
		 MLX5_ACCESS_REGISTER_IN_OP_MOD_WRITE);
	MLX5_SET(access_register_in, in, register_id, reg_id);
	MLX5_SET(access_register_in, in, argument, arg);
	ptr = MLX5_ADDR_OF(access_register_in, in, register_data);
	memcpy(ptr, data, dw_cnt * sizeof(uint32_t));
	rc = mlx5_glue->devx_general_cmd(ctx, in, sizeof(in), out, sizeof(out));

	rc = mlx5_glue->devx_general_cmd(ctx, in,
					 MLX5_ST_SZ_BYTES(access_register_in) +
					 dw_cnt * sizeof(uint32_t),
					 out, sizeof(out));
	if (rc)
		goto error;
	status = MLX5_GET(access_register_out, out, status);
	if (status) {
		int syndrome = MLX5_GET(access_register_out, out, syndrome);

		DRV_LOG(DEBUG, "Failed to write access NIC register 0x%X, "
			       "status %x, syndrome = %x",
			       reg_id, status, syndrome);
		return -1;
	}
	return 0;
error:
	rc = (rc > 0) ? -rc : rc;
	return rc;
}

/**
 * Allocate flow counters via devx interface.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
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
mlx5_devx_cmd_flow_counter_alloc(void *ctx, uint32_t bulk_n_128)
{
	struct mlx5_devx_obj *dcs = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*dcs),
						0, SOCKET_ID_ANY);
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
		mlx5_free(dcs);
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
				 void *cmd_comp,
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
 *   Context returned from mlx5 open_device() glue function.
 * @param[in] attr
 *   Attributes of the requested mkey.
 *
 * @return
 *   Pointer to Devx mkey on success, a negative value otherwise and rte_errno
 *   is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_mkey_create(void *ctx,
			  struct mlx5_devx_mkey_attr *attr)
{
	struct mlx5_klm *klm_array = attr->klm_array;
	int klm_num = attr->klm_num;
	int in_size_dw = MLX5_ST_SZ_DW(create_mkey_in) +
		     (klm_num ? RTE_ALIGN(klm_num, 4) : 0) * MLX5_ST_SZ_DW(klm);
	uint32_t in[in_size_dw];
	uint32_t out[MLX5_ST_SZ_DW(create_mkey_out)] = {0};
	void *mkc;
	struct mlx5_devx_obj *mkey = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*mkey),
						 0, SOCKET_ID_ANY);
	size_t pgsize;
	uint32_t translation_size;

	if (!mkey) {
		rte_errno = ENOMEM;
		return NULL;
	}
	memset(in, 0, in_size_dw * 4);
	pgsize = rte_mem_page_size();
	if (pgsize == (size_t)-1) {
		mlx5_free(mkey);
		DRV_LOG(ERR, "Failed to get page size");
		rte_errno = ENOMEM;
		return NULL;
	}
	MLX5_SET(create_mkey_in, in, opcode, MLX5_CMD_OP_CREATE_MKEY);
	mkc = MLX5_ADDR_OF(create_mkey_in, in, memory_key_mkey_entry);
	if (klm_num > 0) {
		int i;
		uint8_t *klm = (uint8_t *)MLX5_ADDR_OF(create_mkey_in, in,
						       klm_pas_mtt);
		translation_size = RTE_ALIGN(klm_num, 4);
		for (i = 0; i < klm_num; i++) {
			MLX5_SET(klm, klm, byte_count, klm_array[i].byte_count);
			MLX5_SET(klm, klm, mkey, klm_array[i].mkey);
			MLX5_SET64(klm, klm, address, klm_array[i].address);
			klm += MLX5_ST_SZ_BYTES(klm);
		}
		for (; i < (int)translation_size; i++) {
			MLX5_SET(klm, klm, mkey, 0x0);
			MLX5_SET64(klm, klm, address, 0x0);
			klm += MLX5_ST_SZ_BYTES(klm);
		}
		MLX5_SET(mkc, mkc, access_mode_1_0, attr->log_entity_size ?
			 MLX5_MKC_ACCESS_MODE_KLM_FBS :
			 MLX5_MKC_ACCESS_MODE_KLM);
		MLX5_SET(mkc, mkc, log_page_size, attr->log_entity_size);
	} else {
		translation_size = (RTE_ALIGN(attr->size, pgsize) * 8) / 16;
		MLX5_SET(mkc, mkc, access_mode_1_0, MLX5_MKC_ACCESS_MODE_MTT);
		MLX5_SET(mkc, mkc, log_page_size, rte_log2_u32(pgsize));
	}
	MLX5_SET(create_mkey_in, in, translations_octword_actual_size,
		 translation_size);
	MLX5_SET(create_mkey_in, in, mkey_umem_id, attr->umem_id);
	MLX5_SET(create_mkey_in, in, pg_access, attr->pg_access);
	MLX5_SET(mkc, mkc, lw, 0x1);
	MLX5_SET(mkc, mkc, lr, 0x1);
	if (attr->set_remote_rw) {
		MLX5_SET(mkc, mkc, rw, 0x1);
		MLX5_SET(mkc, mkc, rr, 0x1);
	}
	MLX5_SET(mkc, mkc, qpn, 0xffffff);
	MLX5_SET(mkc, mkc, pd, attr->pd);
	MLX5_SET(mkc, mkc, mkey_7_0, attr->umem_id & 0xFF);
	MLX5_SET(mkc, mkc, umr_en, attr->umr_en);
	MLX5_SET(mkc, mkc, translations_octword_size, translation_size);
	MLX5_SET(mkc, mkc, relaxed_ordering_write,
		 attr->relaxed_ordering_write);
	MLX5_SET(mkc, mkc, relaxed_ordering_read, attr->relaxed_ordering_read);
	MLX5_SET64(mkc, mkc, start_addr, attr->addr);
	MLX5_SET64(mkc, mkc, len, attr->size);
	MLX5_SET(mkc, mkc, crypto_en, attr->crypto_en);
	if (attr->crypto_en) {
		MLX5_SET(mkc, mkc, bsf_en, attr->crypto_en);
		MLX5_SET(mkc, mkc, bsf_octword_size, 4);
	}
	mkey->obj = mlx5_glue->devx_obj_create(ctx, in, in_size_dw * 4, out,
					       sizeof(out));
	if (!mkey->obj) {
		DRV_LOG(ERR, "Can't create %sdirect mkey - error %d",
			klm_num ? "an in" : "a ", errno);
		rte_errno = errno;
		mlx5_free(mkey);
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

		DRV_LOG(ERR, "Bad DevX status %x, syndrome = %x", status,
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
	mlx5_free(obj);
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
mlx5_devx_cmd_query_nic_vport_context(void *ctx,
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
			"status %x, syndrome = %x", status, syndrome);
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
 * Query NIC vDPA attributes.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param[out] vdpa_attr
 *   vDPA Attributes structure to fill.
 */
static void
mlx5_devx_cmd_query_hca_vdpa_attr(void *ctx,
				  struct mlx5_hca_vdpa_attr *vdpa_attr)
{
	uint32_t in[MLX5_ST_SZ_DW(query_hca_cap_in)];
	uint32_t out[MLX5_ST_SZ_DW(query_hca_cap_out)];
	void *hcattr;

	hcattr = mlx5_devx_get_hca_cap(ctx, in, out, NULL,
			MLX5_GET_HCA_CAP_OP_MOD_VDPA_EMULATION |
			MLX5_HCA_CAP_OPMOD_GET_CUR);
	if (!hcattr) {
		RTE_LOG(DEBUG, PMD, "Failed to query devx VDPA capabilities");
		vdpa_attr->valid = 0;
	} else {
		vdpa_attr->valid = 1;
		vdpa_attr->desc_tunnel_offload_type =
			MLX5_GET(virtio_emulation_cap, hcattr,
				 desc_tunnel_offload_type);
		vdpa_attr->eth_frame_offload_type =
			MLX5_GET(virtio_emulation_cap, hcattr,
				 eth_frame_offload_type);
		vdpa_attr->virtio_version_1_0 =
			MLX5_GET(virtio_emulation_cap, hcattr,
				 virtio_version_1_0);
		vdpa_attr->tso_ipv4 = MLX5_GET(virtio_emulation_cap, hcattr,
					       tso_ipv4);
		vdpa_attr->tso_ipv6 = MLX5_GET(virtio_emulation_cap, hcattr,
					       tso_ipv6);
		vdpa_attr->tx_csum = MLX5_GET(virtio_emulation_cap, hcattr,
					      tx_csum);
		vdpa_attr->rx_csum = MLX5_GET(virtio_emulation_cap, hcattr,
					      rx_csum);
		vdpa_attr->event_mode = MLX5_GET(virtio_emulation_cap, hcattr,
						 event_mode);
		vdpa_attr->virtio_queue_type =
			MLX5_GET(virtio_emulation_cap, hcattr,
				 virtio_queue_type);
		vdpa_attr->log_doorbell_stride =
			MLX5_GET(virtio_emulation_cap, hcattr,
				 log_doorbell_stride);
		vdpa_attr->log_doorbell_bar_size =
			MLX5_GET(virtio_emulation_cap, hcattr,
				 log_doorbell_bar_size);
		vdpa_attr->doorbell_bar_offset =
			MLX5_GET64(virtio_emulation_cap, hcattr,
				   doorbell_bar_offset);
		vdpa_attr->max_num_virtio_queues =
			MLX5_GET(virtio_emulation_cap, hcattr,
				 max_num_virtio_queues);
		vdpa_attr->umems[0].a = MLX5_GET(virtio_emulation_cap, hcattr,
						 umem_1_buffer_param_a);
		vdpa_attr->umems[0].b = MLX5_GET(virtio_emulation_cap, hcattr,
						 umem_1_buffer_param_b);
		vdpa_attr->umems[1].a = MLX5_GET(virtio_emulation_cap, hcattr,
						 umem_2_buffer_param_a);
		vdpa_attr->umems[1].b = MLX5_GET(virtio_emulation_cap, hcattr,
						 umem_2_buffer_param_b);
		vdpa_attr->umems[2].a = MLX5_GET(virtio_emulation_cap, hcattr,
						 umem_3_buffer_param_a);
		vdpa_attr->umems[2].b = MLX5_GET(virtio_emulation_cap, hcattr,
						 umem_3_buffer_param_b);
	}
}

int
mlx5_devx_cmd_query_parse_samples(struct mlx5_devx_obj *flex_obj,
				  uint32_t ids[], uint32_t num)
{
	uint32_t in[MLX5_ST_SZ_DW(general_obj_in_cmd_hdr)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(create_flex_parser_out)] = {0};
	void *hdr = MLX5_ADDR_OF(create_flex_parser_out, in, hdr);
	void *flex = MLX5_ADDR_OF(create_flex_parser_out, out, flex);
	void *sample = MLX5_ADDR_OF(parse_graph_flex, flex, sample_table);
	int ret;
	uint32_t idx = 0;
	uint32_t i;

	if (num > MLX5_GRAPH_NODE_SAMPLE_NUM) {
		rte_errno = EINVAL;
		DRV_LOG(ERR, "Too many sample IDs to be fetched.");
		return -rte_errno;
	}
	MLX5_SET(general_obj_in_cmd_hdr, hdr, opcode,
		 MLX5_CMD_OP_QUERY_GENERAL_OBJECT);
	MLX5_SET(general_obj_in_cmd_hdr, hdr, obj_type,
		 MLX5_GENERAL_OBJ_TYPE_FLEX_PARSE_GRAPH);
	MLX5_SET(general_obj_in_cmd_hdr, hdr, obj_id, flex_obj->id);
	ret = mlx5_glue->devx_obj_query(flex_obj->obj, in, sizeof(in),
					out, sizeof(out));
	if (ret) {
		rte_errno = ret;
		DRV_LOG(ERR, "Failed to query sample IDs with object %p.",
			(void *)flex_obj);
		return -rte_errno;
	}
	for (i = 0; i < MLX5_GRAPH_NODE_SAMPLE_NUM; i++) {
		void *s_off = (void *)((char *)sample + i *
			      MLX5_ST_SZ_BYTES(parse_graph_flow_match_sample));
		uint32_t en;

		en = MLX5_GET(parse_graph_flow_match_sample, s_off,
			      flow_match_sample_en);
		if (!en)
			continue;
		ids[idx++] = MLX5_GET(parse_graph_flow_match_sample, s_off,
				  flow_match_sample_field_id);
	}
	if (num != idx) {
		rte_errno = EINVAL;
		DRV_LOG(ERR, "Number of sample IDs are not as expected.");
		return -rte_errno;
	}
	return ret;
}

struct mlx5_devx_obj *
mlx5_devx_cmd_create_flex_parser(void *ctx,
				 struct mlx5_devx_graph_node_attr *data)
{
	uint32_t in[MLX5_ST_SZ_DW(create_flex_parser_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(general_obj_out_cmd_hdr)] = {0};
	void *hdr = MLX5_ADDR_OF(create_flex_parser_in, in, hdr);
	void *flex = MLX5_ADDR_OF(create_flex_parser_in, in, flex);
	void *sample = MLX5_ADDR_OF(parse_graph_flex, flex, sample_table);
	void *in_arc = MLX5_ADDR_OF(parse_graph_flex, flex, input_arc);
	void *out_arc = MLX5_ADDR_OF(parse_graph_flex, flex, output_arc);
	struct mlx5_devx_obj *parse_flex_obj = mlx5_malloc
		     (MLX5_MEM_ZERO, sizeof(*parse_flex_obj), 0, SOCKET_ID_ANY);
	uint32_t i;

	if (!parse_flex_obj) {
		DRV_LOG(ERR, "Failed to allocate flex parser data.");
		rte_errno = ENOMEM;
		return NULL;
	}
	MLX5_SET(general_obj_in_cmd_hdr, hdr, opcode,
		 MLX5_CMD_OP_CREATE_GENERAL_OBJECT);
	MLX5_SET(general_obj_in_cmd_hdr, hdr, obj_type,
		 MLX5_GENERAL_OBJ_TYPE_FLEX_PARSE_GRAPH);
	MLX5_SET(parse_graph_flex, flex, header_length_mode,
		 data->header_length_mode);
	MLX5_SET64(parse_graph_flex, flex, modify_field_select,
		   data->modify_field_select);
	MLX5_SET(parse_graph_flex, flex, header_length_base_value,
		 data->header_length_base_value);
	MLX5_SET(parse_graph_flex, flex, header_length_field_offset,
		 data->header_length_field_offset);
	MLX5_SET(parse_graph_flex, flex, header_length_field_shift,
		 data->header_length_field_shift);
	MLX5_SET(parse_graph_flex, flex, next_header_field_offset,
		 data->next_header_field_offset);
	MLX5_SET(parse_graph_flex, flex, next_header_field_size,
		 data->next_header_field_size);
	MLX5_SET(parse_graph_flex, flex, header_length_field_mask,
		 data->header_length_field_mask);
	for (i = 0; i < MLX5_GRAPH_NODE_SAMPLE_NUM; i++) {
		struct mlx5_devx_match_sample_attr *s = &data->sample[i];
		void *s_off = (void *)((char *)sample + i *
			      MLX5_ST_SZ_BYTES(parse_graph_flow_match_sample));

		if (!s->flow_match_sample_en)
			continue;
		MLX5_SET(parse_graph_flow_match_sample, s_off,
			 flow_match_sample_en, !!s->flow_match_sample_en);
		MLX5_SET(parse_graph_flow_match_sample, s_off,
			 flow_match_sample_field_offset,
			 s->flow_match_sample_field_offset);
		MLX5_SET(parse_graph_flow_match_sample, s_off,
			 flow_match_sample_offset_mode,
			 s->flow_match_sample_offset_mode);
		MLX5_SET(parse_graph_flow_match_sample, s_off,
			 flow_match_sample_field_offset_mask,
			 s->flow_match_sample_field_offset_mask);
		MLX5_SET(parse_graph_flow_match_sample, s_off,
			 flow_match_sample_field_offset_shift,
			 s->flow_match_sample_field_offset_shift);
		MLX5_SET(parse_graph_flow_match_sample, s_off,
			 flow_match_sample_field_base_offset,
			 s->flow_match_sample_field_base_offset);
		MLX5_SET(parse_graph_flow_match_sample, s_off,
			 flow_match_sample_tunnel_mode,
			 s->flow_match_sample_tunnel_mode);
	}
	for (i = 0; i < MLX5_GRAPH_NODE_ARC_NUM; i++) {
		struct mlx5_devx_graph_arc_attr *ia = &data->in[i];
		struct mlx5_devx_graph_arc_attr *oa = &data->out[i];
		void *in_off = (void *)((char *)in_arc + i *
			      MLX5_ST_SZ_BYTES(parse_graph_arc));
		void *out_off = (void *)((char *)out_arc + i *
			      MLX5_ST_SZ_BYTES(parse_graph_arc));

		if (ia->arc_parse_graph_node != 0) {
			MLX5_SET(parse_graph_arc, in_off,
				 compare_condition_value,
				 ia->compare_condition_value);
			MLX5_SET(parse_graph_arc, in_off, start_inner_tunnel,
				 ia->start_inner_tunnel);
			MLX5_SET(parse_graph_arc, in_off, arc_parse_graph_node,
				 ia->arc_parse_graph_node);
			MLX5_SET(parse_graph_arc, in_off,
				 parse_graph_node_handle,
				 ia->parse_graph_node_handle);
		}
		if (oa->arc_parse_graph_node != 0) {
			MLX5_SET(parse_graph_arc, out_off,
				 compare_condition_value,
				 oa->compare_condition_value);
			MLX5_SET(parse_graph_arc, out_off, start_inner_tunnel,
				 oa->start_inner_tunnel);
			MLX5_SET(parse_graph_arc, out_off, arc_parse_graph_node,
				 oa->arc_parse_graph_node);
			MLX5_SET(parse_graph_arc, out_off,
				 parse_graph_node_handle,
				 oa->parse_graph_node_handle);
		}
	}
	parse_flex_obj->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in),
							 out, sizeof(out));
	if (!parse_flex_obj->obj) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create FLEX PARSE GRAPH object "
			"by using DevX.");
		mlx5_free(parse_flex_obj);
		return NULL;
	}
	parse_flex_obj->id = MLX5_GET(general_obj_out_cmd_hdr, out, obj_id);
	return parse_flex_obj;
}

static int
mlx5_devx_cmd_query_hca_parse_graph_node_cap
	(void *ctx, struct mlx5_hca_flex_attr *attr)
{
	uint32_t in[MLX5_ST_SZ_DW(query_hca_cap_in)];
	uint32_t out[MLX5_ST_SZ_DW(query_hca_cap_out)];
	void *hcattr;
	int rc;

	hcattr = mlx5_devx_get_hca_cap(ctx, in, out, &rc,
			MLX5_GET_HCA_CAP_OP_MOD_PARSE_GRAPH_NODE_CAP |
			MLX5_HCA_CAP_OPMOD_GET_CUR);
	if (!hcattr)
		return rc;
	attr->node_in = MLX5_GET(parse_graph_node_cap, hcattr, node_in);
	attr->node_out = MLX5_GET(parse_graph_node_cap, hcattr, node_out);
	attr->header_length_mode = MLX5_GET(parse_graph_node_cap, hcattr,
					    header_length_mode);
	attr->sample_offset_mode = MLX5_GET(parse_graph_node_cap, hcattr,
					    sample_offset_mode);
	attr->max_num_arc_in = MLX5_GET(parse_graph_node_cap, hcattr,
					max_num_arc_in);
	attr->max_num_arc_out = MLX5_GET(parse_graph_node_cap, hcattr,
					 max_num_arc_out);
	attr->max_num_sample = MLX5_GET(parse_graph_node_cap, hcattr,
					max_num_sample);
	attr->sample_id_in_out = MLX5_GET(parse_graph_node_cap, hcattr,
					  sample_id_in_out);
	attr->max_base_header_length = MLX5_GET(parse_graph_node_cap, hcattr,
						max_base_header_length);
	attr->max_sample_base_offset = MLX5_GET(parse_graph_node_cap, hcattr,
						max_sample_base_offset);
	attr->max_next_header_offset = MLX5_GET(parse_graph_node_cap, hcattr,
						max_next_header_offset);
	attr->header_length_mask_width = MLX5_GET(parse_graph_node_cap, hcattr,
						  header_length_mask_width);
	/* Get the max supported samples from HCA CAP 2 */
	hcattr = mlx5_devx_get_hca_cap(ctx, in, out, &rc,
			MLX5_GET_HCA_CAP_OP_MOD_GENERAL_DEVICE_2 |
			MLX5_HCA_CAP_OPMOD_GET_CUR);
	if (!hcattr)
		return rc;
	attr->max_num_prog_sample =
		MLX5_GET(cmd_hca_cap_2, hcattr,	max_num_prog_sample_field);
	return 0;
}

static int
mlx5_devx_query_pkt_integrity_match(void *hcattr)
{
	return MLX5_GET(flow_table_nic_cap, hcattr,
			ft_field_support_2_nic_receive.inner_l3_ok) &&
	       MLX5_GET(flow_table_nic_cap, hcattr,
			ft_field_support_2_nic_receive.inner_l4_ok) &&
	       MLX5_GET(flow_table_nic_cap, hcattr,
			ft_field_support_2_nic_receive.outer_l3_ok) &&
	       MLX5_GET(flow_table_nic_cap, hcattr,
			ft_field_support_2_nic_receive.outer_l4_ok) &&
	       MLX5_GET(flow_table_nic_cap, hcattr,
			ft_field_support_2_nic_receive
				.inner_ipv4_checksum_ok) &&
	       MLX5_GET(flow_table_nic_cap, hcattr,
			ft_field_support_2_nic_receive.inner_l4_checksum_ok) &&
	       MLX5_GET(flow_table_nic_cap, hcattr,
			ft_field_support_2_nic_receive
				.outer_ipv4_checksum_ok) &&
	       MLX5_GET(flow_table_nic_cap, hcattr,
			ft_field_support_2_nic_receive.outer_l4_checksum_ok);
}

/**
 * Query HCA attributes.
 * Using those attributes we can check on run time if the device
 * is having the required capabilities.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param[out] attr
 *   Attributes device values.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int
mlx5_devx_cmd_query_hca_attr(void *ctx,
			     struct mlx5_hca_attr *attr)
{
	uint32_t in[MLX5_ST_SZ_DW(query_hca_cap_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(query_hca_cap_out)] = {0};
	bool hca_cap_2_sup;
	uint64_t general_obj_types_supported = 0;
	void *hcattr;
	int rc, i;

	hcattr = mlx5_devx_get_hca_cap(ctx, in, out, &rc,
			MLX5_GET_HCA_CAP_OP_MOD_GENERAL_DEVICE |
			MLX5_HCA_CAP_OPMOD_GET_CUR);
	if (!hcattr)
		return rc;
	hca_cap_2_sup = MLX5_GET(cmd_hca_cap, hcattr, hca_cap_2);
	attr->max_wqe_sz_sq = MLX5_GET(cmd_hca_cap, hcattr, max_wqe_sz_sq);
	attr->flow_counter_bulk_alloc_bitmap =
			MLX5_GET(cmd_hca_cap, hcattr, flow_counter_bulk_alloc);
	attr->flow_counters_dump = MLX5_GET(cmd_hca_cap, hcattr,
					    flow_counters_dump);
	attr->log_max_rmp = MLX5_GET(cmd_hca_cap, hcattr, log_max_rmp);
	attr->mem_rq_rmp = MLX5_GET(cmd_hca_cap, hcattr, mem_rq_rmp);
	attr->log_max_rqt_size = MLX5_GET(cmd_hca_cap, hcattr,
					  log_max_rqt_size);
	attr->eswitch_manager = MLX5_GET(cmd_hca_cap, hcattr, eswitch_manager);
	attr->hairpin = MLX5_GET(cmd_hca_cap, hcattr, hairpin);
	attr->log_max_hairpin_queues = MLX5_GET(cmd_hca_cap, hcattr,
						log_max_hairpin_queues);
	attr->log_max_hairpin_wq_data_sz = MLX5_GET(cmd_hca_cap, hcattr,
						    log_max_hairpin_wq_data_sz);
	attr->log_max_hairpin_num_packets = MLX5_GET
		(cmd_hca_cap, hcattr, log_min_hairpin_wq_data_sz);
	attr->vhca_id = MLX5_GET(cmd_hca_cap, hcattr, vhca_id);
	attr->relaxed_ordering_write = MLX5_GET(cmd_hca_cap, hcattr,
						relaxed_ordering_write);
	attr->relaxed_ordering_read = MLX5_GET(cmd_hca_cap, hcattr,
					       relaxed_ordering_read);
	attr->access_register_user = MLX5_GET(cmd_hca_cap, hcattr,
					      access_register_user);
	attr->eth_net_offloads = MLX5_GET(cmd_hca_cap, hcattr,
					  eth_net_offloads);
	attr->eth_virt = MLX5_GET(cmd_hca_cap, hcattr, eth_virt);
	attr->flex_parser_protocols = MLX5_GET(cmd_hca_cap, hcattr,
					       flex_parser_protocols);
	attr->max_geneve_tlv_options = MLX5_GET(cmd_hca_cap, hcattr,
			max_geneve_tlv_options);
	attr->max_geneve_tlv_option_data_len = MLX5_GET(cmd_hca_cap, hcattr,
			max_geneve_tlv_option_data_len);
	attr->qos.sup = MLX5_GET(cmd_hca_cap, hcattr, qos);
	attr->qos.flow_meter_aso_sup = !!(MLX5_GET64(cmd_hca_cap, hcattr,
					 general_obj_types) &
			      MLX5_GENERAL_OBJ_TYPES_CAP_FLOW_METER_ASO);
	attr->vdpa.valid = !!(MLX5_GET64(cmd_hca_cap, hcattr,
					 general_obj_types) &
			      MLX5_GENERAL_OBJ_TYPES_CAP_VIRTQ_NET_Q);
	attr->vdpa.queue_counters_valid = !!(MLX5_GET64(cmd_hca_cap, hcattr,
							general_obj_types) &
				  MLX5_GENERAL_OBJ_TYPES_CAP_VIRTIO_Q_COUNTERS);
	attr->parse_graph_flex_node = !!(MLX5_GET64(cmd_hca_cap, hcattr,
					 general_obj_types) &
			      MLX5_GENERAL_OBJ_TYPES_CAP_PARSE_GRAPH_FLEX_NODE);
	attr->wqe_index_ignore = MLX5_GET(cmd_hca_cap, hcattr,
					  wqe_index_ignore_cap);
	attr->cross_channel = MLX5_GET(cmd_hca_cap, hcattr, cd);
	attr->non_wire_sq = MLX5_GET(cmd_hca_cap, hcattr, non_wire_sq);
	attr->log_max_static_sq_wq = MLX5_GET(cmd_hca_cap, hcattr,
					      log_max_static_sq_wq);
	attr->num_lag_ports = MLX5_GET(cmd_hca_cap, hcattr, num_lag_ports);
	attr->dev_freq_khz = MLX5_GET(cmd_hca_cap, hcattr,
				      device_frequency_khz);
	attr->scatter_fcs_w_decap_disable =
		MLX5_GET(cmd_hca_cap, hcattr, scatter_fcs_w_decap_disable);
	attr->roce = MLX5_GET(cmd_hca_cap, hcattr, roce);
	attr->rq_ts_format = MLX5_GET(cmd_hca_cap, hcattr, rq_ts_format);
	attr->sq_ts_format = MLX5_GET(cmd_hca_cap, hcattr, sq_ts_format);
	attr->steering_format_version =
		MLX5_GET(cmd_hca_cap, hcattr, steering_format_version);
	attr->regexp_params = MLX5_GET(cmd_hca_cap, hcattr, regexp_params);
	attr->regexp_version = MLX5_GET(cmd_hca_cap, hcattr, regexp_version);
	attr->regexp_num_of_engines = MLX5_GET(cmd_hca_cap, hcattr,
					       regexp_num_of_engines);
	/* Read the general_obj_types bitmap and extract the relevant bits. */
	general_obj_types_supported = MLX5_GET64(cmd_hca_cap, hcattr,
						 general_obj_types);
	attr->vdpa.valid = !!(general_obj_types_supported &
			      MLX5_GENERAL_OBJ_TYPES_CAP_VIRTQ_NET_Q);
	attr->vdpa.queue_counters_valid =
			!!(general_obj_types_supported &
			   MLX5_GENERAL_OBJ_TYPES_CAP_VIRTIO_Q_COUNTERS);
	attr->parse_graph_flex_node =
			!!(general_obj_types_supported &
			   MLX5_GENERAL_OBJ_TYPES_CAP_PARSE_GRAPH_FLEX_NODE);
	attr->flow_hit_aso = !!(general_obj_types_supported &
				MLX5_GENERAL_OBJ_TYPES_CAP_FLOW_HIT_ASO);
	attr->geneve_tlv_opt = !!(general_obj_types_supported &
				  MLX5_GENERAL_OBJ_TYPES_CAP_GENEVE_TLV_OPT);
	attr->dek = !!(general_obj_types_supported &
		       MLX5_GENERAL_OBJ_TYPES_CAP_DEK);
	attr->import_kek = !!(general_obj_types_supported &
			      MLX5_GENERAL_OBJ_TYPES_CAP_IMPORT_KEK);
	attr->credential = !!(general_obj_types_supported &
			      MLX5_GENERAL_OBJ_TYPES_CAP_CREDENTIAL);
	attr->crypto_login = !!(general_obj_types_supported &
				MLX5_GENERAL_OBJ_TYPES_CAP_CRYPTO_LOGIN);
	/* Add reading of other GENERAL_OBJ_TYPES_CAP bits above this line. */
	attr->log_max_cq = MLX5_GET(cmd_hca_cap, hcattr, log_max_cq);
	attr->log_max_qp = MLX5_GET(cmd_hca_cap, hcattr, log_max_qp);
	attr->log_max_cq_sz = MLX5_GET(cmd_hca_cap, hcattr, log_max_cq_sz);
	attr->log_max_qp_sz = MLX5_GET(cmd_hca_cap, hcattr, log_max_qp_sz);
	attr->log_max_mrw_sz = MLX5_GET(cmd_hca_cap, hcattr, log_max_mrw_sz);
	attr->log_max_pd = MLX5_GET(cmd_hca_cap, hcattr, log_max_pd);
	attr->log_max_srq = MLX5_GET(cmd_hca_cap, hcattr, log_max_srq);
	attr->log_max_srq_sz = MLX5_GET(cmd_hca_cap, hcattr, log_max_srq_sz);
	attr->reg_c_preserve =
		MLX5_GET(cmd_hca_cap, hcattr, reg_c_preserve);
	attr->mmo_regex_qp_en = MLX5_GET(cmd_hca_cap, hcattr, regexp_mmo_qp);
	attr->mmo_regex_sq_en = MLX5_GET(cmd_hca_cap, hcattr, regexp_mmo_sq);
	attr->mmo_dma_sq_en = MLX5_GET(cmd_hca_cap, hcattr, dma_mmo_sq);
	attr->mmo_compress_sq_en = MLX5_GET(cmd_hca_cap, hcattr,
			compress_mmo_sq);
	attr->mmo_decompress_sq_en = MLX5_GET(cmd_hca_cap, hcattr,
			decompress_mmo_sq);
	attr->mmo_dma_qp_en = MLX5_GET(cmd_hca_cap, hcattr, dma_mmo_qp);
	attr->mmo_compress_qp_en = MLX5_GET(cmd_hca_cap, hcattr,
			compress_mmo_qp);
	attr->mmo_decompress_qp_en = MLX5_GET(cmd_hca_cap, hcattr,
			decompress_mmo_qp);
	attr->compress_min_block_size = MLX5_GET(cmd_hca_cap, hcattr,
						 compress_min_block_size);
	attr->log_max_mmo_dma = MLX5_GET(cmd_hca_cap, hcattr, log_dma_mmo_size);
	attr->log_max_mmo_compress = MLX5_GET(cmd_hca_cap, hcattr,
					      log_compress_mmo_size);
	attr->log_max_mmo_decompress = MLX5_GET(cmd_hca_cap, hcattr,
						log_decompress_mmo_size);
	attr->cqe_compression = MLX5_GET(cmd_hca_cap, hcattr, cqe_compression);
	attr->mini_cqe_resp_flow_tag = MLX5_GET(cmd_hca_cap, hcattr,
						mini_cqe_resp_flow_tag);
	attr->mini_cqe_resp_l3_l4_tag = MLX5_GET(cmd_hca_cap, hcattr,
						 mini_cqe_resp_l3_l4_tag);
	attr->umr_indirect_mkey_disabled =
		MLX5_GET(cmd_hca_cap, hcattr, umr_indirect_mkey_disabled);
	attr->umr_modify_entity_size_disabled =
		MLX5_GET(cmd_hca_cap, hcattr, umr_modify_entity_size_disabled);
	attr->crypto = MLX5_GET(cmd_hca_cap, hcattr, crypto);
	if (attr->crypto)
		attr->aes_xts = MLX5_GET(cmd_hca_cap, hcattr, aes_xts);
	attr->ct_offload = !!(MLX5_GET64(cmd_hca_cap, hcattr,
					 general_obj_types) &
			      MLX5_GENERAL_OBJ_TYPES_CAP_CONN_TRACK_OFFLOAD);
	attr->rq_delay_drop = MLX5_GET(cmd_hca_cap, hcattr, rq_delay_drop);
	if (hca_cap_2_sup) {
		hcattr = mlx5_devx_get_hca_cap(ctx, in, out, &rc,
				MLX5_GET_HCA_CAP_OP_MOD_GENERAL_DEVICE_2 |
				MLX5_HCA_CAP_OPMOD_GET_CUR);
		if (!hcattr) {
			DRV_LOG(DEBUG,
				"Failed to query DevX HCA capabilities 2.");
			return rc;
		}
		attr->log_min_stride_wqe_sz = MLX5_GET(cmd_hca_cap_2, hcattr,
						       log_min_stride_wqe_sz);
	}
	if (attr->log_min_stride_wqe_sz == 0)
		attr->log_min_stride_wqe_sz = MLX5_MPRQ_LOG_MIN_STRIDE_WQE_SIZE;
	if (attr->qos.sup) {
		hcattr = mlx5_devx_get_hca_cap(ctx, in, out, &rc,
				MLX5_GET_HCA_CAP_OP_MOD_QOS_CAP |
				MLX5_HCA_CAP_OPMOD_GET_CUR);
		if (!hcattr) {
			DRV_LOG(DEBUG, "Failed to query devx QOS capabilities");
			return rc;
		}
		attr->qos.flow_meter_old =
				MLX5_GET(qos_cap, hcattr, flow_meter_old);
		attr->qos.log_max_flow_meter =
				MLX5_GET(qos_cap, hcattr, log_max_flow_meter);
		attr->qos.flow_meter_reg_c_ids =
				MLX5_GET(qos_cap, hcattr, flow_meter_reg_id);
		attr->qos.flow_meter =
				MLX5_GET(qos_cap, hcattr, flow_meter);
		attr->qos.packet_pacing =
				MLX5_GET(qos_cap, hcattr, packet_pacing);
		attr->qos.wqe_rate_pp =
				MLX5_GET(qos_cap, hcattr, wqe_rate_pp);
		if (attr->qos.flow_meter_aso_sup) {
			attr->qos.log_meter_aso_granularity =
				MLX5_GET(qos_cap, hcattr,
					log_meter_aso_granularity);
			attr->qos.log_meter_aso_max_alloc =
				MLX5_GET(qos_cap, hcattr,
					log_meter_aso_max_alloc);
			attr->qos.log_max_num_meter_aso =
				MLX5_GET(qos_cap, hcattr,
					log_max_num_meter_aso);
		}
	}
	/*
	 * Flex item support needs max_num_prog_sample_field
	 * from the Capabilities 2 table for PARSE_GRAPH_NODE
	 */
	if (attr->parse_graph_flex_node) {
		rc = mlx5_devx_cmd_query_hca_parse_graph_node_cap
			(ctx, &attr->flex);
		if (rc)
			return -1;
	}
	if (attr->vdpa.valid)
		mlx5_devx_cmd_query_hca_vdpa_attr(ctx, &attr->vdpa);
	if (!attr->eth_net_offloads)
		return 0;
	/* Query Flow Sampler Capability From FLow Table Properties Layout. */
	hcattr = mlx5_devx_get_hca_cap(ctx, in, out, &rc,
			MLX5_GET_HCA_CAP_OP_MOD_NIC_FLOW_TABLE |
			MLX5_HCA_CAP_OPMOD_GET_CUR);
	if (!hcattr) {
		attr->log_max_ft_sampler_num = 0;
		return rc;
	}
	attr->log_max_ft_sampler_num = MLX5_GET
		(flow_table_nic_cap, hcattr,
		 flow_table_properties_nic_receive.log_max_ft_sampler_num);
	attr->flow.tunnel_header_0_1 = MLX5_GET
		(flow_table_nic_cap, hcattr,
		 ft_field_support_2_nic_receive.tunnel_header_0_1);
	attr->pkt_integrity_match = mlx5_devx_query_pkt_integrity_match(hcattr);
	attr->inner_ipv4_ihl = MLX5_GET
		(flow_table_nic_cap, hcattr,
		 ft_field_support_2_nic_receive.inner_ipv4_ihl);
	attr->outer_ipv4_ihl = MLX5_GET
		(flow_table_nic_cap, hcattr,
		 ft_field_support_2_nic_receive.outer_ipv4_ihl);
	/* Query HCA offloads for Ethernet protocol. */
	hcattr = mlx5_devx_get_hca_cap(ctx, in, out, &rc,
			MLX5_GET_HCA_CAP_OP_MOD_ETHERNET_OFFLOAD_CAPS |
			MLX5_HCA_CAP_OPMOD_GET_CUR);
	if (!hcattr) {
		attr->eth_net_offloads = 0;
		return rc;
	}
	attr->wqe_vlan_insert = MLX5_GET(per_protocol_networking_offload_caps,
					 hcattr, wqe_vlan_insert);
	attr->csum_cap = MLX5_GET(per_protocol_networking_offload_caps,
					 hcattr, csum_cap);
	attr->vlan_cap = MLX5_GET(per_protocol_networking_offload_caps,
					 hcattr, vlan_cap);
	attr->lro_cap = MLX5_GET(per_protocol_networking_offload_caps, hcattr,
				 lro_cap);
	attr->max_lso_cap = MLX5_GET(per_protocol_networking_offload_caps,
				 hcattr, max_lso_cap);
	attr->scatter_fcs = MLX5_GET(per_protocol_networking_offload_caps,
				 hcattr, scatter_fcs);
	attr->tunnel_lro_gre = MLX5_GET(per_protocol_networking_offload_caps,
					hcattr, tunnel_lro_gre);
	attr->tunnel_lro_vxlan = MLX5_GET(per_protocol_networking_offload_caps,
					  hcattr, tunnel_lro_vxlan);
	attr->swp = MLX5_GET(per_protocol_networking_offload_caps,
					  hcattr, swp);
	attr->tunnel_stateless_gre =
				MLX5_GET(per_protocol_networking_offload_caps,
					  hcattr, tunnel_stateless_gre);
	attr->tunnel_stateless_vxlan =
				MLX5_GET(per_protocol_networking_offload_caps,
					  hcattr, tunnel_stateless_vxlan);
	attr->swp_csum = MLX5_GET(per_protocol_networking_offload_caps,
					  hcattr, swp_csum);
	attr->swp_lso = MLX5_GET(per_protocol_networking_offload_caps,
					  hcattr, swp_lso);
	attr->lro_max_msg_sz_mode = MLX5_GET
					(per_protocol_networking_offload_caps,
					 hcattr, lro_max_msg_sz_mode);
	for (i = 0 ; i < MLX5_LRO_NUM_SUPP_PERIODS ; i++) {
		attr->lro_timer_supported_periods[i] =
			MLX5_GET(per_protocol_networking_offload_caps, hcattr,
				 lro_timer_supported_periods[i]);
	}
	attr->lro_min_mss_size = MLX5_GET(per_protocol_networking_offload_caps,
					  hcattr, lro_min_mss_size);
	attr->tunnel_stateless_geneve_rx =
			    MLX5_GET(per_protocol_networking_offload_caps,
				     hcattr, tunnel_stateless_geneve_rx);
	attr->geneve_max_opt_len =
		    MLX5_GET(per_protocol_networking_offload_caps,
			     hcattr, max_geneve_opt_len);
	attr->wqe_inline_mode = MLX5_GET(per_protocol_networking_offload_caps,
					 hcattr, wqe_inline_mode);
	attr->tunnel_stateless_gtp = MLX5_GET
					(per_protocol_networking_offload_caps,
					 hcattr, tunnel_stateless_gtp);
	attr->rss_ind_tbl_cap = MLX5_GET
					(per_protocol_networking_offload_caps,
					 hcattr, rss_ind_tbl_cap);
	/* Query HCA attribute for ROCE. */
	if (attr->roce) {
		hcattr = mlx5_devx_get_hca_cap(ctx, in, out, &rc,
				MLX5_GET_HCA_CAP_OP_MOD_ROCE |
				MLX5_HCA_CAP_OPMOD_GET_CUR);
		if (!hcattr) {
			DRV_LOG(DEBUG,
				"Failed to query devx HCA ROCE capabilities");
			return rc;
		}
		attr->qp_ts_format = MLX5_GET(roce_caps, hcattr, qp_ts_format);
	}
	if (attr->eth_virt &&
	    attr->wqe_inline_mode == MLX5_CAP_INLINE_MODE_VPORT_CONTEXT) {
		rc = mlx5_devx_cmd_query_nic_vport_context(ctx, 0, attr);
		if (rc) {
			attr->eth_virt = 0;
			goto error;
		}
	}
	if (attr->eswitch_manager) {
		hcattr = mlx5_devx_get_hca_cap(ctx, in, out, &rc,
				MLX5_SET_HCA_CAP_OP_MOD_ESW |
				MLX5_HCA_CAP_OPMOD_GET_CUR);
		if (!hcattr)
			return rc;
		attr->esw_mgr_vport_id_valid =
			MLX5_GET(esw_cap, hcattr,
				 esw_manager_vport_number_valid);
		attr->esw_mgr_vport_id =
			MLX5_GET(esw_cap, hcattr, esw_manager_vport_number);
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
mlx5_devx_cmd_qp_query_tis_td(void *qp, uint32_t tis_num,
			      uint32_t *tis_td)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
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
#else
	(void)qp;
	(void)tis_num;
	(void)tis_td;
	return -ENOTSUP;
#endif
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
	if (wq_attr->log_wq_pg_sz > MLX5_ADAPTER_PAGE_SHIFT)
		MLX5_SET(wq, wq_ctx, log_wq_pg_sz,
			 wq_attr->log_wq_pg_sz - MLX5_ADAPTER_PAGE_SHIFT);
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
 *   Context returned from mlx5 open_device() glue function.
 * @param [in] rq_attr
 *   Pointer to create RQ attributes structure.
 * @param [in] socket
 *   CPU socket ID for allocations.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_rq(void *ctx,
			struct mlx5_devx_create_rq_attr *rq_attr,
			int socket)
{
	uint32_t in[MLX5_ST_SZ_DW(create_rq_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(create_rq_out)] = {0};
	void *rq_ctx, *wq_ctx;
	struct mlx5_devx_wq_attr *wq_attr;
	struct mlx5_devx_obj *rq = NULL;

	rq = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*rq), 0, socket);
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
	MLX5_SET(sqc, rq_ctx, ts_format, rq_attr->ts_format);
	wq_ctx = MLX5_ADDR_OF(rqc, rq_ctx, wq);
	wq_attr = &rq_attr->wq_attr;
	devx_cmd_fill_wq_data(wq_ctx, wq_attr);
	rq->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in),
						  out, sizeof(out));
	if (!rq->obj) {
		DRV_LOG(ERR, "Failed to create RQ using DevX");
		rte_errno = errno;
		mlx5_free(rq);
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
 * Create RMP using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param [in] rmp_attr
 *   Pointer to create RMP attributes structure.
 * @param [in] socket
 *   CPU socket ID for allocations.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_rmp(void *ctx,
			 struct mlx5_devx_create_rmp_attr *rmp_attr,
			 int socket)
{
	uint32_t in[MLX5_ST_SZ_DW(create_rmp_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(create_rmp_out)] = {0};
	void *rmp_ctx, *wq_ctx;
	struct mlx5_devx_wq_attr *wq_attr;
	struct mlx5_devx_obj *rmp = NULL;

	rmp = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*rmp), 0, socket);
	if (!rmp) {
		DRV_LOG(ERR, "Failed to allocate RMP data");
		rte_errno = ENOMEM;
		return NULL;
	}
	MLX5_SET(create_rmp_in, in, opcode, MLX5_CMD_OP_CREATE_RMP);
	rmp_ctx = MLX5_ADDR_OF(create_rmp_in, in, ctx);
	MLX5_SET(rmpc, rmp_ctx, state, rmp_attr->state);
	MLX5_SET(rmpc, rmp_ctx, basic_cyclic_rcv_wqe,
		 rmp_attr->basic_cyclic_rcv_wqe);
	wq_ctx = MLX5_ADDR_OF(rmpc, rmp_ctx, wq);
	wq_attr = &rmp_attr->wq_attr;
	devx_cmd_fill_wq_data(wq_ctx, wq_attr);
	rmp->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in), out,
					      sizeof(out));
	if (!rmp->obj) {
		DRV_LOG(ERR, "Failed to create RMP using DevX");
		rte_errno = errno;
		mlx5_free(rmp);
		return NULL;
	}
	rmp->id = MLX5_GET(create_rmp_out, out, rmpn);
	return rmp;
}

/*
 * Create TIR using DevX API.
 *
 * @param[in] ctx
 *  Context returned from mlx5 open_device() glue function.
 * @param [in] tir_attr
 *   Pointer to TIR attributes structure.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_tir(void *ctx,
			 struct mlx5_devx_tir_attr *tir_attr)
{
	uint32_t in[MLX5_ST_SZ_DW(create_tir_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(create_tir_out)] = {0};
	void *tir_ctx, *outer, *inner, *rss_key;
	struct mlx5_devx_obj *tir = NULL;

	tir = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*tir), 0, SOCKET_ID_ANY);
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
	rss_key = MLX5_ADDR_OF(tirc, tir_ctx, rx_hash_toeplitz_key);
	memcpy(rss_key, tir_attr->rx_hash_toeplitz_key, MLX5_RSS_HASH_KEY_LEN);
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
		mlx5_free(tir);
		return NULL;
	}
	tir->id = MLX5_GET(create_tir_out, out, tirn);
	return tir;
}

/**
 * Modify TIR using DevX API.
 *
 * @param[in] tir
 *   Pointer to TIR DevX object structure.
 * @param [in] modify_tir_attr
 *   Pointer to TIR modification attributes structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_devx_cmd_modify_tir(struct mlx5_devx_obj *tir,
			 struct mlx5_devx_modify_tir_attr *modify_tir_attr)
{
	struct mlx5_devx_tir_attr *tir_attr = &modify_tir_attr->tir;
	uint32_t in[MLX5_ST_SZ_DW(modify_tir_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(modify_tir_out)] = {0};
	void *tir_ctx;
	int ret;

	MLX5_SET(modify_tir_in, in, opcode, MLX5_CMD_OP_MODIFY_TIR);
	MLX5_SET(modify_tir_in, in, tirn, modify_tir_attr->tirn);
	MLX5_SET64(modify_tir_in, in, modify_bitmask,
		   modify_tir_attr->modify_bitmask);
	tir_ctx = MLX5_ADDR_OF(modify_rq_in, in, ctx);
	if (modify_tir_attr->modify_bitmask &
			MLX5_MODIFY_TIR_IN_MODIFY_BITMASK_LRO) {
		MLX5_SET(tirc, tir_ctx, lro_timeout_period_usecs,
			 tir_attr->lro_timeout_period_usecs);
		MLX5_SET(tirc, tir_ctx, lro_enable_mask,
			 tir_attr->lro_enable_mask);
		MLX5_SET(tirc, tir_ctx, lro_max_msg_sz,
			 tir_attr->lro_max_msg_sz);
	}
	if (modify_tir_attr->modify_bitmask &
			MLX5_MODIFY_TIR_IN_MODIFY_BITMASK_INDIRECT_TABLE)
		MLX5_SET(tirc, tir_ctx, indirect_table,
			 tir_attr->indirect_table);
	if (modify_tir_attr->modify_bitmask &
			MLX5_MODIFY_TIR_IN_MODIFY_BITMASK_HASH) {
		int i;
		void *outer, *inner;

		MLX5_SET(tirc, tir_ctx, rx_hash_symmetric,
			 tir_attr->rx_hash_symmetric);
		MLX5_SET(tirc, tir_ctx, rx_hash_fn, tir_attr->rx_hash_fn);
		for (i = 0; i < 10; i++) {
			MLX5_SET(tirc, tir_ctx, rx_hash_toeplitz_key[i],
				 tir_attr->rx_hash_toeplitz_key[i]);
		}
		outer = MLX5_ADDR_OF(tirc, tir_ctx,
				     rx_hash_field_selector_outer);
		MLX5_SET(rx_hash_field_select, outer, l3_prot_type,
			 tir_attr->rx_hash_field_selector_outer.l3_prot_type);
		MLX5_SET(rx_hash_field_select, outer, l4_prot_type,
			 tir_attr->rx_hash_field_selector_outer.l4_prot_type);
		MLX5_SET
		(rx_hash_field_select, outer, selected_fields,
		 tir_attr->rx_hash_field_selector_outer.selected_fields);
		inner = MLX5_ADDR_OF(tirc, tir_ctx,
				     rx_hash_field_selector_inner);
		MLX5_SET(rx_hash_field_select, inner, l3_prot_type,
			 tir_attr->rx_hash_field_selector_inner.l3_prot_type);
		MLX5_SET(rx_hash_field_select, inner, l4_prot_type,
			 tir_attr->rx_hash_field_selector_inner.l4_prot_type);
		MLX5_SET
		(rx_hash_field_select, inner, selected_fields,
		 tir_attr->rx_hash_field_selector_inner.selected_fields);
	}
	if (modify_tir_attr->modify_bitmask &
	    MLX5_MODIFY_TIR_IN_MODIFY_BITMASK_SELF_LB_EN) {
		MLX5_SET(tirc, tir_ctx, self_lb_block, tir_attr->self_lb_block);
	}
	ret = mlx5_glue->devx_obj_modify(tir->obj, in, sizeof(in),
					 out, sizeof(out));
	if (ret) {
		DRV_LOG(ERR, "Failed to modify TIR using DevX");
		rte_errno = errno;
		return -errno;
	}
	return ret;
}

/**
 * Create RQT using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param [in] rqt_attr
 *   Pointer to RQT attributes structure.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_rqt(void *ctx,
			 struct mlx5_devx_rqt_attr *rqt_attr)
{
	uint32_t *in = NULL;
	uint32_t inlen = MLX5_ST_SZ_BYTES(create_rqt_in) +
			 rqt_attr->rqt_actual_size * sizeof(uint32_t);
	uint32_t out[MLX5_ST_SZ_DW(create_rqt_out)] = {0};
	void *rqt_ctx;
	struct mlx5_devx_obj *rqt = NULL;
	int i;

	in = mlx5_malloc(MLX5_MEM_ZERO, inlen, 0, SOCKET_ID_ANY);
	if (!in) {
		DRV_LOG(ERR, "Failed to allocate RQT IN data");
		rte_errno = ENOMEM;
		return NULL;
	}
	rqt = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*rqt), 0, SOCKET_ID_ANY);
	if (!rqt) {
		DRV_LOG(ERR, "Failed to allocate RQT data");
		rte_errno = ENOMEM;
		mlx5_free(in);
		return NULL;
	}
	MLX5_SET(create_rqt_in, in, opcode, MLX5_CMD_OP_CREATE_RQT);
	rqt_ctx = MLX5_ADDR_OF(create_rqt_in, in, rqt_context);
	MLX5_SET(rqtc, rqt_ctx, list_q_type, rqt_attr->rq_type);
	MLX5_SET(rqtc, rqt_ctx, rqt_max_size, rqt_attr->rqt_max_size);
	MLX5_SET(rqtc, rqt_ctx, rqt_actual_size, rqt_attr->rqt_actual_size);
	for (i = 0; i < rqt_attr->rqt_actual_size; i++)
		MLX5_SET(rqtc, rqt_ctx, rq_num[i], rqt_attr->rq_list[i]);
	rqt->obj = mlx5_glue->devx_obj_create(ctx, in, inlen, out, sizeof(out));
	mlx5_free(in);
	if (!rqt->obj) {
		DRV_LOG(ERR, "Failed to create RQT using DevX");
		rte_errno = errno;
		mlx5_free(rqt);
		return NULL;
	}
	rqt->id = MLX5_GET(create_rqt_out, out, rqtn);
	return rqt;
}

/**
 * Modify RQT using DevX API.
 *
 * @param[in] rqt
 *   Pointer to RQT DevX object structure.
 * @param [in] rqt_attr
 *   Pointer to RQT attributes structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_devx_cmd_modify_rqt(struct mlx5_devx_obj *rqt,
			 struct mlx5_devx_rqt_attr *rqt_attr)
{
	uint32_t inlen = MLX5_ST_SZ_BYTES(modify_rqt_in) +
			 rqt_attr->rqt_actual_size * sizeof(uint32_t);
	uint32_t out[MLX5_ST_SZ_DW(modify_rqt_out)] = {0};
	uint32_t *in = mlx5_malloc(MLX5_MEM_ZERO, inlen, 0, SOCKET_ID_ANY);
	void *rqt_ctx;
	int i;
	int ret;

	if (!in) {
		DRV_LOG(ERR, "Failed to allocate RQT modify IN data.");
		rte_errno = ENOMEM;
		return -ENOMEM;
	}
	MLX5_SET(modify_rqt_in, in, opcode, MLX5_CMD_OP_MODIFY_RQT);
	MLX5_SET(modify_rqt_in, in, rqtn, rqt->id);
	MLX5_SET64(modify_rqt_in, in, modify_bitmask, 0x1);
	rqt_ctx = MLX5_ADDR_OF(modify_rqt_in, in, rqt_context);
	MLX5_SET(rqtc, rqt_ctx, list_q_type, rqt_attr->rq_type);
	MLX5_SET(rqtc, rqt_ctx, rqt_max_size, rqt_attr->rqt_max_size);
	MLX5_SET(rqtc, rqt_ctx, rqt_actual_size, rqt_attr->rqt_actual_size);
	for (i = 0; i < rqt_attr->rqt_actual_size; i++)
		MLX5_SET(rqtc, rqt_ctx, rq_num[i], rqt_attr->rq_list[i]);
	ret = mlx5_glue->devx_obj_modify(rqt->obj, in, inlen, out, sizeof(out));
	mlx5_free(in);
	if (ret) {
		DRV_LOG(ERR, "Failed to modify RQT using DevX.");
		rte_errno = errno;
		return -rte_errno;
	}
	return ret;
}

/**
 * Create SQ using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param [in] sq_attr
 *   Pointer to SQ attributes structure.
 * @param [in] socket
 *   CPU socket ID for allocations.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 **/
struct mlx5_devx_obj *
mlx5_devx_cmd_create_sq(void *ctx,
			struct mlx5_devx_create_sq_attr *sq_attr)
{
	uint32_t in[MLX5_ST_SZ_DW(create_sq_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(create_sq_out)] = {0};
	void *sq_ctx;
	void *wq_ctx;
	struct mlx5_devx_wq_attr *wq_attr;
	struct mlx5_devx_obj *sq = NULL;

	sq = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*sq), 0, SOCKET_ID_ANY);
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
		 sq_attr->allow_multi_pkt_send_wqe);
	MLX5_SET(sqc, sq_ctx, min_wqe_inline_mode,
		 sq_attr->min_wqe_inline_mode);
	MLX5_SET(sqc, sq_ctx, state, sq_attr->state);
	MLX5_SET(sqc, sq_ctx, reg_umr, sq_attr->reg_umr);
	MLX5_SET(sqc, sq_ctx, allow_swp, sq_attr->allow_swp);
	MLX5_SET(sqc, sq_ctx, hairpin, sq_attr->hairpin);
	MLX5_SET(sqc, sq_ctx, non_wire, sq_attr->non_wire);
	MLX5_SET(sqc, sq_ctx, static_sq_wq, sq_attr->static_sq_wq);
	MLX5_SET(sqc, sq_ctx, user_index, sq_attr->user_index);
	MLX5_SET(sqc, sq_ctx, cqn, sq_attr->cqn);
	MLX5_SET(sqc, sq_ctx, packet_pacing_rate_limit_index,
		 sq_attr->packet_pacing_rate_limit_index);
	MLX5_SET(sqc, sq_ctx, tis_lst_sz, sq_attr->tis_lst_sz);
	MLX5_SET(sqc, sq_ctx, tis_num_0, sq_attr->tis_num);
	MLX5_SET(sqc, sq_ctx, ts_format, sq_attr->ts_format);
	wq_ctx = MLX5_ADDR_OF(sqc, sq_ctx, wq);
	wq_attr = &sq_attr->wq_attr;
	devx_cmd_fill_wq_data(wq_ctx, wq_attr);
	sq->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in),
					     out, sizeof(out));
	if (!sq->obj) {
		DRV_LOG(ERR, "Failed to create SQ using DevX");
		rte_errno = errno;
		mlx5_free(sq);
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
		return -rte_errno;
	}
	return ret;
}

/**
 * Create TIS using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param [in] tis_attr
 *   Pointer to TIS attributes structure.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_tis(void *ctx,
			 struct mlx5_devx_tis_attr *tis_attr)
{
	uint32_t in[MLX5_ST_SZ_DW(create_tis_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(create_tis_out)] = {0};
	struct mlx5_devx_obj *tis = NULL;
	void *tis_ctx;

	tis = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*tis), 0, SOCKET_ID_ANY);
	if (!tis) {
		DRV_LOG(ERR, "Failed to allocate TIS object");
		rte_errno = ENOMEM;
		return NULL;
	}
	MLX5_SET(create_tis_in, in, opcode, MLX5_CMD_OP_CREATE_TIS);
	tis_ctx = MLX5_ADDR_OF(create_tis_in, in, ctx);
	MLX5_SET(tisc, tis_ctx, strict_lag_tx_port_affinity,
		 tis_attr->strict_lag_tx_port_affinity);
	MLX5_SET(tisc, tis_ctx, lag_tx_port_affinity,
		 tis_attr->lag_tx_port_affinity);
	MLX5_SET(tisc, tis_ctx, prio, tis_attr->prio);
	MLX5_SET(tisc, tis_ctx, transport_domain,
		 tis_attr->transport_domain);
	tis->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in),
					      out, sizeof(out));
	if (!tis->obj) {
		DRV_LOG(ERR, "Failed to create TIS using DevX");
		rte_errno = errno;
		mlx5_free(tis);
		return NULL;
	}
	tis->id = MLX5_GET(create_tis_out, out, tisn);
	return tis;
}

/**
 * Create transport domain using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_td(void *ctx)
{
	uint32_t in[MLX5_ST_SZ_DW(alloc_transport_domain_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(alloc_transport_domain_out)] = {0};
	struct mlx5_devx_obj *td = NULL;

	td = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*td), 0, SOCKET_ID_ANY);
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
		mlx5_free(td);
		return NULL;
	}
	td->id = MLX5_GET(alloc_transport_domain_out, out,
			   transport_domain);
	return td;
}

/**
 * Dump all flows to file.
 *
 * @param[in] fdb_domain
 *   FDB domain.
 * @param[in] rx_domain
 *   RX domain.
 * @param[in] tx_domain
 *   TX domain.
 * @param[out] file
 *   Pointer to file stream.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int
mlx5_devx_cmd_flow_dump(void *fdb_domain __rte_unused,
			void *rx_domain __rte_unused,
			void *tx_domain __rte_unused, FILE *file __rte_unused)
{
	int ret = 0;

#ifdef HAVE_MLX5_DR_FLOW_DUMP
	if (fdb_domain) {
		ret = mlx5_glue->dr_dump_domain(file, fdb_domain);
		if (ret)
			return ret;
	}
	MLX5_ASSERT(rx_domain);
	ret = mlx5_glue->dr_dump_domain(file, rx_domain);
	if (ret)
		return ret;
	MLX5_ASSERT(tx_domain);
	ret = mlx5_glue->dr_dump_domain(file, tx_domain);
#else
	ret = ENOTSUP;
#endif
	return -ret;
}

int
mlx5_devx_cmd_flow_single_dump(void *rule_info __rte_unused,
			FILE *file __rte_unused)
{
	int ret = 0;
#ifdef HAVE_MLX5_DR_FLOW_DUMP_RULE
	if (rule_info)
		ret = mlx5_glue->dr_dump_rule(file, rule_info);
#else
	ret = ENOTSUP;
#endif
	return -ret;
}

/*
 * Create CQ using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param [in] attr
 *   Pointer to CQ attributes structure.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_cq(void *ctx, struct mlx5_devx_cq_attr *attr)
{
	uint32_t in[MLX5_ST_SZ_DW(create_cq_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(create_cq_out)] = {0};
	struct mlx5_devx_obj *cq_obj = mlx5_malloc(MLX5_MEM_ZERO,
						   sizeof(*cq_obj),
						   0, SOCKET_ID_ANY);
	void *cqctx = MLX5_ADDR_OF(create_cq_in, in, cq_context);

	if (!cq_obj) {
		DRV_LOG(ERR, "Failed to allocate CQ object memory.");
		rte_errno = ENOMEM;
		return NULL;
	}
	MLX5_SET(create_cq_in, in, opcode, MLX5_CMD_OP_CREATE_CQ);
	if (attr->db_umem_valid) {
		MLX5_SET(cqc, cqctx, dbr_umem_valid, attr->db_umem_valid);
		MLX5_SET(cqc, cqctx, dbr_umem_id, attr->db_umem_id);
		MLX5_SET64(cqc, cqctx, dbr_addr, attr->db_umem_offset);
	} else {
		MLX5_SET64(cqc, cqctx, dbr_addr, attr->db_addr);
	}
	MLX5_SET(cqc, cqctx, cqe_sz, (RTE_CACHE_LINE_SIZE == 128) ?
				     MLX5_CQE_SIZE_128B : MLX5_CQE_SIZE_64B);
	MLX5_SET(cqc, cqctx, cc, attr->use_first_only);
	MLX5_SET(cqc, cqctx, oi, attr->overrun_ignore);
	MLX5_SET(cqc, cqctx, log_cq_size, attr->log_cq_size);
	if (attr->log_page_size > MLX5_ADAPTER_PAGE_SHIFT)
		MLX5_SET(cqc, cqctx, log_page_size,
			 attr->log_page_size - MLX5_ADAPTER_PAGE_SHIFT);
	MLX5_SET(cqc, cqctx, c_eqn, attr->eqn);
	MLX5_SET(cqc, cqctx, uar_page, attr->uar_page_id);
	MLX5_SET(cqc, cqctx, cqe_comp_en, !!attr->cqe_comp_en);
	MLX5_SET(cqc, cqctx, mini_cqe_res_format, attr->mini_cqe_res_format);
	MLX5_SET(cqc, cqctx, mini_cqe_res_format_ext,
		 attr->mini_cqe_res_format_ext);
	if (attr->q_umem_valid) {
		MLX5_SET(create_cq_in, in, cq_umem_valid, attr->q_umem_valid);
		MLX5_SET(create_cq_in, in, cq_umem_id, attr->q_umem_id);
		MLX5_SET64(create_cq_in, in, cq_umem_offset,
			   attr->q_umem_offset);
	}
	cq_obj->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in), out,
						 sizeof(out));
	if (!cq_obj->obj) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create CQ using DevX errno=%d.", errno);
		mlx5_free(cq_obj);
		return NULL;
	}
	cq_obj->id = MLX5_GET(create_cq_out, out, cqn);
	return cq_obj;
}

/**
 * Create VIRTQ using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param [in] attr
 *   Pointer to VIRTQ attributes structure.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_virtq(void *ctx,
			   struct mlx5_devx_virtq_attr *attr)
{
	uint32_t in[MLX5_ST_SZ_DW(create_virtq_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(general_obj_out_cmd_hdr)] = {0};
	struct mlx5_devx_obj *virtq_obj = mlx5_malloc(MLX5_MEM_ZERO,
						     sizeof(*virtq_obj),
						     0, SOCKET_ID_ANY);
	void *virtq = MLX5_ADDR_OF(create_virtq_in, in, virtq);
	void *hdr = MLX5_ADDR_OF(create_virtq_in, in, hdr);
	void *virtctx = MLX5_ADDR_OF(virtio_net_q, virtq, virtio_q_context);

	if (!virtq_obj) {
		DRV_LOG(ERR, "Failed to allocate virtq data.");
		rte_errno = ENOMEM;
		return NULL;
	}
	MLX5_SET(general_obj_in_cmd_hdr, hdr, opcode,
		 MLX5_CMD_OP_CREATE_GENERAL_OBJECT);
	MLX5_SET(general_obj_in_cmd_hdr, hdr, obj_type,
		 MLX5_GENERAL_OBJ_TYPE_VIRTQ);
	MLX5_SET16(virtio_net_q, virtq, hw_available_index,
		   attr->hw_available_index);
	MLX5_SET16(virtio_net_q, virtq, hw_used_index, attr->hw_used_index);
	MLX5_SET16(virtio_net_q, virtq, tso_ipv4, attr->tso_ipv4);
	MLX5_SET16(virtio_net_q, virtq, tso_ipv6, attr->tso_ipv6);
	MLX5_SET16(virtio_net_q, virtq, tx_csum, attr->tx_csum);
	MLX5_SET16(virtio_net_q, virtq, rx_csum, attr->rx_csum);
	MLX5_SET16(virtio_q, virtctx, virtio_version_1_0,
		   attr->virtio_version_1_0);
	MLX5_SET16(virtio_q, virtctx, event_mode, attr->event_mode);
	MLX5_SET(virtio_q, virtctx, event_qpn_or_msix, attr->qp_id);
	MLX5_SET64(virtio_q, virtctx, desc_addr, attr->desc_addr);
	MLX5_SET64(virtio_q, virtctx, used_addr, attr->used_addr);
	MLX5_SET64(virtio_q, virtctx, available_addr, attr->available_addr);
	MLX5_SET16(virtio_q, virtctx, queue_index, attr->queue_index);
	MLX5_SET16(virtio_q, virtctx, queue_size, attr->q_size);
	MLX5_SET(virtio_q, virtctx, virtio_q_mkey, attr->mkey);
	MLX5_SET(virtio_q, virtctx, umem_1_id, attr->umems[0].id);
	MLX5_SET(virtio_q, virtctx, umem_1_size, attr->umems[0].size);
	MLX5_SET64(virtio_q, virtctx, umem_1_offset, attr->umems[0].offset);
	MLX5_SET(virtio_q, virtctx, umem_2_id, attr->umems[1].id);
	MLX5_SET(virtio_q, virtctx, umem_2_size, attr->umems[1].size);
	MLX5_SET64(virtio_q, virtctx, umem_2_offset, attr->umems[1].offset);
	MLX5_SET(virtio_q, virtctx, umem_3_id, attr->umems[2].id);
	MLX5_SET(virtio_q, virtctx, umem_3_size, attr->umems[2].size);
	MLX5_SET64(virtio_q, virtctx, umem_3_offset, attr->umems[2].offset);
	MLX5_SET(virtio_q, virtctx, counter_set_id, attr->counters_obj_id);
	MLX5_SET(virtio_q, virtctx, pd, attr->pd);
	MLX5_SET(virtio_q, virtctx, queue_period_mode, attr->hw_latency_mode);
	MLX5_SET(virtio_q, virtctx, queue_period_us, attr->hw_max_latency_us);
	MLX5_SET(virtio_q, virtctx, queue_max_count, attr->hw_max_pending_comp);
	MLX5_SET(virtio_net_q, virtq, tisn_or_qpn, attr->tis_id);
	virtq_obj->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in), out,
						    sizeof(out));
	if (!virtq_obj->obj) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create VIRTQ Obj using DevX.");
		mlx5_free(virtq_obj);
		return NULL;
	}
	virtq_obj->id = MLX5_GET(general_obj_out_cmd_hdr, out, obj_id);
	return virtq_obj;
}

/**
 * Modify VIRTQ using DevX API.
 *
 * @param[in] virtq_obj
 *   Pointer to virtq object structure.
 * @param [in] attr
 *   Pointer to modify virtq attributes structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_devx_cmd_modify_virtq(struct mlx5_devx_obj *virtq_obj,
			   struct mlx5_devx_virtq_attr *attr)
{
	uint32_t in[MLX5_ST_SZ_DW(create_virtq_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(general_obj_out_cmd_hdr)] = {0};
	void *virtq = MLX5_ADDR_OF(create_virtq_in, in, virtq);
	void *hdr = MLX5_ADDR_OF(create_virtq_in, in, hdr);
	void *virtctx = MLX5_ADDR_OF(virtio_net_q, virtq, virtio_q_context);
	int ret;

	MLX5_SET(general_obj_in_cmd_hdr, hdr, opcode,
		 MLX5_CMD_OP_MODIFY_GENERAL_OBJECT);
	MLX5_SET(general_obj_in_cmd_hdr, hdr, obj_type,
		 MLX5_GENERAL_OBJ_TYPE_VIRTQ);
	MLX5_SET(general_obj_in_cmd_hdr, hdr, obj_id, virtq_obj->id);
	MLX5_SET64(virtio_net_q, virtq, modify_field_select, attr->type);
	MLX5_SET16(virtio_q, virtctx, queue_index, attr->queue_index);
	switch (attr->type) {
	case MLX5_VIRTQ_MODIFY_TYPE_STATE:
		MLX5_SET16(virtio_net_q, virtq, state, attr->state);
		break;
	case MLX5_VIRTQ_MODIFY_TYPE_DIRTY_BITMAP_PARAMS:
		MLX5_SET(virtio_net_q, virtq, dirty_bitmap_mkey,
			 attr->dirty_bitmap_mkey);
		MLX5_SET64(virtio_net_q, virtq, dirty_bitmap_addr,
			 attr->dirty_bitmap_addr);
		MLX5_SET(virtio_net_q, virtq, dirty_bitmap_size,
			 attr->dirty_bitmap_size);
		break;
	case MLX5_VIRTQ_MODIFY_TYPE_DIRTY_BITMAP_DUMP_ENABLE:
		MLX5_SET(virtio_net_q, virtq, dirty_bitmap_dump_enable,
			 attr->dirty_bitmap_dump_enable);
		break;
	default:
		rte_errno = EINVAL;
		return -rte_errno;
	}
	ret = mlx5_glue->devx_obj_modify(virtq_obj->obj, in, sizeof(in),
					 out, sizeof(out));
	if (ret) {
		DRV_LOG(ERR, "Failed to modify VIRTQ using DevX.");
		rte_errno = errno;
		return -rte_errno;
	}
	return ret;
}

/**
 * Query VIRTQ using DevX API.
 *
 * @param[in] virtq_obj
 *   Pointer to virtq object structure.
 * @param [in/out] attr
 *   Pointer to virtq attributes structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_devx_cmd_query_virtq(struct mlx5_devx_obj *virtq_obj,
			   struct mlx5_devx_virtq_attr *attr)
{
	uint32_t in[MLX5_ST_SZ_DW(general_obj_in_cmd_hdr)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(query_virtq_out)] = {0};
	void *hdr = MLX5_ADDR_OF(query_virtq_out, in, hdr);
	void *virtq = MLX5_ADDR_OF(query_virtq_out, out, virtq);
	int ret;

	MLX5_SET(general_obj_in_cmd_hdr, hdr, opcode,
		 MLX5_CMD_OP_QUERY_GENERAL_OBJECT);
	MLX5_SET(general_obj_in_cmd_hdr, hdr, obj_type,
		 MLX5_GENERAL_OBJ_TYPE_VIRTQ);
	MLX5_SET(general_obj_in_cmd_hdr, hdr, obj_id, virtq_obj->id);
	ret = mlx5_glue->devx_obj_query(virtq_obj->obj, in, sizeof(in),
					 out, sizeof(out));
	if (ret) {
		DRV_LOG(ERR, "Failed to modify VIRTQ using DevX.");
		rte_errno = errno;
		return -errno;
	}
	attr->hw_available_index = MLX5_GET16(virtio_net_q, virtq,
					      hw_available_index);
	attr->hw_used_index = MLX5_GET16(virtio_net_q, virtq, hw_used_index);
	attr->state = MLX5_GET16(virtio_net_q, virtq, state);
	attr->error_type = MLX5_GET16(virtio_net_q, virtq,
				      virtio_q_context.error_type);
	return ret;
}

/**
 * Create QP using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param [in] attr
 *   Pointer to QP attributes structure.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_qp(void *ctx,
			struct mlx5_devx_qp_attr *attr)
{
	uint32_t in[MLX5_ST_SZ_DW(create_qp_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(create_qp_out)] = {0};
	struct mlx5_devx_obj *qp_obj = mlx5_malloc(MLX5_MEM_ZERO,
						   sizeof(*qp_obj),
						   0, SOCKET_ID_ANY);
	void *qpc = MLX5_ADDR_OF(create_qp_in, in, qpc);

	if (!qp_obj) {
		DRV_LOG(ERR, "Failed to allocate QP data.");
		rte_errno = ENOMEM;
		return NULL;
	}
	MLX5_SET(create_qp_in, in, opcode, MLX5_CMD_OP_CREATE_QP);
	MLX5_SET(qpc, qpc, st, MLX5_QP_ST_RC);
	MLX5_SET(qpc, qpc, pd, attr->pd);
	MLX5_SET(qpc, qpc, ts_format, attr->ts_format);
	MLX5_SET(qpc, qpc, user_index, attr->user_index);
	if (attr->uar_index) {
		if (attr->mmo) {
			void *qpc_ext_and_pas_list = MLX5_ADDR_OF(create_qp_in,
				in, qpc_extension_and_pas_list);
			void *qpc_ext = MLX5_ADDR_OF(qpc_extension_and_pas_list,
				qpc_ext_and_pas_list, qpc_data_extension);

			MLX5_SET(create_qp_in, in, qpc_ext, 1);
			MLX5_SET(qpc_extension, qpc_ext, mmo, 1);
		}
		MLX5_SET(qpc, qpc, pm_state, MLX5_QP_PM_MIGRATED);
		MLX5_SET(qpc, qpc, uar_page, attr->uar_index);
		if (attr->log_page_size > MLX5_ADAPTER_PAGE_SHIFT)
			MLX5_SET(qpc, qpc, log_page_size,
				 attr->log_page_size - MLX5_ADAPTER_PAGE_SHIFT);
		if (attr->num_of_send_wqbbs) {
			MLX5_ASSERT(RTE_IS_POWER_OF_2(attr->num_of_send_wqbbs));
			MLX5_SET(qpc, qpc, cqn_snd, attr->cqn);
			MLX5_SET(qpc, qpc, log_sq_size,
				 rte_log2_u32(attr->num_of_send_wqbbs));
		} else {
			MLX5_SET(qpc, qpc, no_sq, 1);
		}
		if (attr->num_of_receive_wqes) {
			MLX5_ASSERT(RTE_IS_POWER_OF_2(
					attr->num_of_receive_wqes));
			MLX5_SET(qpc, qpc, cqn_rcv, attr->cqn);
			MLX5_SET(qpc, qpc, log_rq_stride, attr->log_rq_stride -
				 MLX5_LOG_RQ_STRIDE_SHIFT);
			MLX5_SET(qpc, qpc, log_rq_size,
				 rte_log2_u32(attr->num_of_receive_wqes));
			MLX5_SET(qpc, qpc, rq_type, MLX5_NON_ZERO_RQ);
		} else {
			MLX5_SET(qpc, qpc, rq_type, MLX5_ZERO_LEN_RQ);
		}
		if (attr->dbr_umem_valid) {
			MLX5_SET(qpc, qpc, dbr_umem_valid,
				 attr->dbr_umem_valid);
			MLX5_SET(qpc, qpc, dbr_umem_id, attr->dbr_umem_id);
		}
		MLX5_SET64(qpc, qpc, dbr_addr, attr->dbr_address);
		MLX5_SET64(create_qp_in, in, wq_umem_offset,
			   attr->wq_umem_offset);
		MLX5_SET(create_qp_in, in, wq_umem_id, attr->wq_umem_id);
		MLX5_SET(create_qp_in, in, wq_umem_valid, 1);
	} else {
		/* Special QP to be managed by FW - no SQ\RQ\CQ\UAR\DB rec. */
		MLX5_SET(qpc, qpc, rq_type, MLX5_ZERO_LEN_RQ);
		MLX5_SET(qpc, qpc, no_sq, 1);
	}
	qp_obj->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in), out,
						 sizeof(out));
	if (!qp_obj->obj) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create QP Obj using DevX.");
		mlx5_free(qp_obj);
		return NULL;
	}
	qp_obj->id = MLX5_GET(create_qp_out, out, qpn);
	return qp_obj;
}

/**
 * Modify QP using DevX API.
 * Currently supports only force loop-back QP.
 *
 * @param[in] qp
 *   Pointer to QP object structure.
 * @param [in] qp_st_mod_op
 *   The QP state modification operation.
 * @param [in] remote_qp_id
 *   The remote QP ID for MLX5_CMD_OP_INIT2RTR_QP operation.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_devx_cmd_modify_qp_state(struct mlx5_devx_obj *qp, uint32_t qp_st_mod_op,
			      uint32_t remote_qp_id)
{
	union {
		uint32_t rst2init[MLX5_ST_SZ_DW(rst2init_qp_in)];
		uint32_t init2rtr[MLX5_ST_SZ_DW(init2rtr_qp_in)];
		uint32_t rtr2rts[MLX5_ST_SZ_DW(rtr2rts_qp_in)];
	} in;
	union {
		uint32_t rst2init[MLX5_ST_SZ_DW(rst2init_qp_out)];
		uint32_t init2rtr[MLX5_ST_SZ_DW(init2rtr_qp_out)];
		uint32_t rtr2rts[MLX5_ST_SZ_DW(rtr2rts_qp_out)];
	} out;
	void *qpc;
	int ret;
	unsigned int inlen;
	unsigned int outlen;

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));
	MLX5_SET(rst2init_qp_in, &in, opcode, qp_st_mod_op);
	switch (qp_st_mod_op) {
	case MLX5_CMD_OP_RST2INIT_QP:
		MLX5_SET(rst2init_qp_in, &in, qpn, qp->id);
		qpc = MLX5_ADDR_OF(rst2init_qp_in, &in, qpc);
		MLX5_SET(qpc, qpc, primary_address_path.vhca_port_num, 1);
		MLX5_SET(qpc, qpc, rre, 1);
		MLX5_SET(qpc, qpc, rwe, 1);
		MLX5_SET(qpc, qpc, pm_state, MLX5_QP_PM_MIGRATED);
		inlen = sizeof(in.rst2init);
		outlen = sizeof(out.rst2init);
		break;
	case MLX5_CMD_OP_INIT2RTR_QP:
		MLX5_SET(init2rtr_qp_in, &in, qpn, qp->id);
		qpc = MLX5_ADDR_OF(init2rtr_qp_in, &in, qpc);
		MLX5_SET(qpc, qpc, primary_address_path.fl, 1);
		MLX5_SET(qpc, qpc, primary_address_path.vhca_port_num, 1);
		MLX5_SET(qpc, qpc, mtu, 1);
		MLX5_SET(qpc, qpc, log_msg_max, 30);
		MLX5_SET(qpc, qpc, remote_qpn, remote_qp_id);
		MLX5_SET(qpc, qpc, min_rnr_nak, 0);
		inlen = sizeof(in.init2rtr);
		outlen = sizeof(out.init2rtr);
		break;
	case MLX5_CMD_OP_RTR2RTS_QP:
		qpc = MLX5_ADDR_OF(rtr2rts_qp_in, &in, qpc);
		MLX5_SET(rtr2rts_qp_in, &in, qpn, qp->id);
		MLX5_SET(qpc, qpc, primary_address_path.ack_timeout, 16);
		MLX5_SET(qpc, qpc, log_ack_req_freq, 0);
		MLX5_SET(qpc, qpc, retry_count, 7);
		MLX5_SET(qpc, qpc, rnr_retry, 7);
		inlen = sizeof(in.rtr2rts);
		outlen = sizeof(out.rtr2rts);
		break;
	default:
		DRV_LOG(ERR, "Invalid or unsupported QP modify op %u.",
			qp_st_mod_op);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	ret = mlx5_glue->devx_obj_modify(qp->obj, &in, inlen, &out, outlen);
	if (ret) {
		DRV_LOG(ERR, "Failed to modify QP using DevX.");
		rte_errno = errno;
		return -rte_errno;
	}
	return ret;
}

struct mlx5_devx_obj *
mlx5_devx_cmd_create_virtio_q_counters(void *ctx)
{
	uint32_t in[MLX5_ST_SZ_DW(create_virtio_q_counters_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(general_obj_out_cmd_hdr)] = {0};
	struct mlx5_devx_obj *couners_obj = mlx5_malloc(MLX5_MEM_ZERO,
						       sizeof(*couners_obj), 0,
						       SOCKET_ID_ANY);
	void *hdr = MLX5_ADDR_OF(create_virtio_q_counters_in, in, hdr);

	if (!couners_obj) {
		DRV_LOG(ERR, "Failed to allocate virtio queue counters data.");
		rte_errno = ENOMEM;
		return NULL;
	}
	MLX5_SET(general_obj_in_cmd_hdr, hdr, opcode,
		 MLX5_CMD_OP_CREATE_GENERAL_OBJECT);
	MLX5_SET(general_obj_in_cmd_hdr, hdr, obj_type,
		 MLX5_GENERAL_OBJ_TYPE_VIRTIO_Q_COUNTERS);
	couners_obj->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in), out,
						      sizeof(out));
	if (!couners_obj->obj) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create virtio queue counters Obj using"
			" DevX.");
		mlx5_free(couners_obj);
		return NULL;
	}
	couners_obj->id = MLX5_GET(general_obj_out_cmd_hdr, out, obj_id);
	return couners_obj;
}

int
mlx5_devx_cmd_query_virtio_q_counters(struct mlx5_devx_obj *couners_obj,
				   struct mlx5_devx_virtio_q_couners_attr *attr)
{
	uint32_t in[MLX5_ST_SZ_DW(general_obj_in_cmd_hdr)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(query_virtio_q_counters_out)] = {0};
	void *hdr = MLX5_ADDR_OF(query_virtio_q_counters_out, in, hdr);
	void *virtio_q_counters = MLX5_ADDR_OF(query_virtio_q_counters_out, out,
					       virtio_q_counters);
	int ret;

	MLX5_SET(general_obj_in_cmd_hdr, hdr, opcode,
		 MLX5_CMD_OP_QUERY_GENERAL_OBJECT);
	MLX5_SET(general_obj_in_cmd_hdr, hdr, obj_type,
		 MLX5_GENERAL_OBJ_TYPE_VIRTIO_Q_COUNTERS);
	MLX5_SET(general_obj_in_cmd_hdr, hdr, obj_id, couners_obj->id);
	ret = mlx5_glue->devx_obj_query(couners_obj->obj, in, sizeof(in), out,
					sizeof(out));
	if (ret) {
		DRV_LOG(ERR, "Failed to query virtio q counters using DevX.");
		rte_errno = errno;
		return -errno;
	}
	attr->received_desc = MLX5_GET64(virtio_q_counters, virtio_q_counters,
					 received_desc);
	attr->completed_desc = MLX5_GET64(virtio_q_counters, virtio_q_counters,
					  completed_desc);
	attr->error_cqes = MLX5_GET(virtio_q_counters, virtio_q_counters,
				    error_cqes);
	attr->bad_desc_errors = MLX5_GET(virtio_q_counters, virtio_q_counters,
					 bad_desc_errors);
	attr->exceed_max_chain = MLX5_GET(virtio_q_counters, virtio_q_counters,
					  exceed_max_chain);
	attr->invalid_buffer = MLX5_GET(virtio_q_counters, virtio_q_counters,
					invalid_buffer);
	return ret;
}

/**
 * Create general object of type FLOW_HIT_ASO using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param [in] pd
 *   PD value to associate the FLOW_HIT_ASO object with.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_flow_hit_aso_obj(void *ctx, uint32_t pd)
{
	uint32_t in[MLX5_ST_SZ_DW(create_flow_hit_aso_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(general_obj_out_cmd_hdr)] = {0};
	struct mlx5_devx_obj *flow_hit_aso_obj = NULL;
	void *ptr = NULL;

	flow_hit_aso_obj = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*flow_hit_aso_obj),
				       0, SOCKET_ID_ANY);
	if (!flow_hit_aso_obj) {
		DRV_LOG(ERR, "Failed to allocate FLOW_HIT_ASO object data");
		rte_errno = ENOMEM;
		return NULL;
	}
	ptr = MLX5_ADDR_OF(create_flow_hit_aso_in, in, hdr);
	MLX5_SET(general_obj_in_cmd_hdr, ptr, opcode,
		 MLX5_CMD_OP_CREATE_GENERAL_OBJECT);
	MLX5_SET(general_obj_in_cmd_hdr, ptr, obj_type,
		 MLX5_GENERAL_OBJ_TYPE_FLOW_HIT_ASO);
	ptr = MLX5_ADDR_OF(create_flow_hit_aso_in, in, flow_hit_aso);
	MLX5_SET(flow_hit_aso, ptr, access_pd, pd);
	flow_hit_aso_obj->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in),
							   out, sizeof(out));
	if (!flow_hit_aso_obj->obj) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create FLOW_HIT_ASO obj using DevX.");
		mlx5_free(flow_hit_aso_obj);
		return NULL;
	}
	flow_hit_aso_obj->id = MLX5_GET(general_obj_out_cmd_hdr, out, obj_id);
	return flow_hit_aso_obj;
}

/*
 * Create PD using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_alloc_pd(void *ctx)
{
	struct mlx5_devx_obj *ppd =
		mlx5_malloc(MLX5_MEM_ZERO, sizeof(*ppd), 0, SOCKET_ID_ANY);
	u32 in[MLX5_ST_SZ_DW(alloc_pd_in)] = {0};
	u32 out[MLX5_ST_SZ_DW(alloc_pd_out)] = {0};

	if (!ppd) {
		DRV_LOG(ERR, "Failed to allocate PD data.");
		rte_errno = ENOMEM;
		return NULL;
	}
	MLX5_SET(alloc_pd_in, in, opcode, MLX5_CMD_OP_ALLOC_PD);
	ppd->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in),
				out, sizeof(out));
	if (!ppd->obj) {
		mlx5_free(ppd);
		DRV_LOG(ERR, "Failed to allocate PD Obj using DevX.");
		rte_errno = errno;
		return NULL;
	}
	ppd->id = MLX5_GET(alloc_pd_out, out, pd);
	return ppd;
}

/**
 * Create general object of type FLOW_METER_ASO using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param [in] pd
 *   PD value to associate the FLOW_METER_ASO object with.
 * @param [in] log_obj_size
 *   log_obj_size define to allocate number of 2 * meters
 *   in one FLOW_METER_ASO object.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_flow_meter_aso_obj(void *ctx, uint32_t pd,
						uint32_t log_obj_size)
{
	uint32_t in[MLX5_ST_SZ_DW(create_flow_meter_aso_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(general_obj_out_cmd_hdr)];
	struct mlx5_devx_obj *flow_meter_aso_obj;
	void *ptr;

	flow_meter_aso_obj = mlx5_malloc(MLX5_MEM_ZERO,
						sizeof(*flow_meter_aso_obj),
						0, SOCKET_ID_ANY);
	if (!flow_meter_aso_obj) {
		DRV_LOG(ERR, "Failed to allocate FLOW_METER_ASO object data");
		rte_errno = ENOMEM;
		return NULL;
	}
	ptr = MLX5_ADDR_OF(create_flow_meter_aso_in, in, hdr);
	MLX5_SET(general_obj_in_cmd_hdr, ptr, opcode,
		MLX5_CMD_OP_CREATE_GENERAL_OBJECT);
	MLX5_SET(general_obj_in_cmd_hdr, ptr, obj_type,
		MLX5_GENERAL_OBJ_TYPE_FLOW_METER_ASO);
	MLX5_SET(general_obj_in_cmd_hdr, ptr, log_obj_range,
		log_obj_size);
	ptr = MLX5_ADDR_OF(create_flow_meter_aso_in, in, flow_meter_aso);
	MLX5_SET(flow_meter_aso, ptr, access_pd, pd);
	flow_meter_aso_obj->obj = mlx5_glue->devx_obj_create(
							ctx, in, sizeof(in),
							out, sizeof(out));
	if (!flow_meter_aso_obj->obj) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create FLOW_METER_ASO obj using DevX.");
		mlx5_free(flow_meter_aso_obj);
		return NULL;
	}
	flow_meter_aso_obj->id = MLX5_GET(general_obj_out_cmd_hdr,
								out, obj_id);
	return flow_meter_aso_obj;
}

/*
 * Create general object of type CONN_TRACK_OFFLOAD using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param [in] pd
 *   PD value to associate the CONN_TRACK_OFFLOAD ASO object with.
 * @param [in] log_obj_size
 *   log_obj_size to allocate its power of 2 * objects
 *   in one CONN_TRACK_OFFLOAD bulk allocation.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_conn_track_offload_obj(void *ctx, uint32_t pd,
					    uint32_t log_obj_size)
{
	uint32_t in[MLX5_ST_SZ_DW(create_conn_track_aso_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(general_obj_out_cmd_hdr)];
	struct mlx5_devx_obj *ct_aso_obj;
	void *ptr;

	ct_aso_obj = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*ct_aso_obj),
				 0, SOCKET_ID_ANY);
	if (!ct_aso_obj) {
		DRV_LOG(ERR, "Failed to allocate CONN_TRACK_OFFLOAD object.");
		rte_errno = ENOMEM;
		return NULL;
	}
	ptr = MLX5_ADDR_OF(create_conn_track_aso_in, in, hdr);
	MLX5_SET(general_obj_in_cmd_hdr, ptr, opcode,
		 MLX5_CMD_OP_CREATE_GENERAL_OBJECT);
	MLX5_SET(general_obj_in_cmd_hdr, ptr, obj_type,
		 MLX5_GENERAL_OBJ_TYPE_CONN_TRACK_OFFLOAD);
	MLX5_SET(general_obj_in_cmd_hdr, ptr, log_obj_range, log_obj_size);
	ptr = MLX5_ADDR_OF(create_conn_track_aso_in, in, conn_track_offload);
	MLX5_SET(conn_track_offload, ptr, conn_track_aso_access_pd, pd);
	ct_aso_obj->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in),
						     out, sizeof(out));
	if (!ct_aso_obj->obj) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create CONN_TRACK_OFFLOAD obj by using DevX.");
		mlx5_free(ct_aso_obj);
		return NULL;
	}
	ct_aso_obj->id = MLX5_GET(general_obj_out_cmd_hdr, out, obj_id);
	return ct_aso_obj;
}

/**
 * Create general object of type GENEVE TLV option using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param [in] class
 *   TLV option variable value of class
 * @param [in] type
 *   TLV option variable value of type
 * @param [in] len
 *   TLV option variable value of len
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_geneve_tlv_option(void *ctx,
		uint16_t class, uint8_t type, uint8_t len)
{
	uint32_t in[MLX5_ST_SZ_DW(create_geneve_tlv_option_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(general_obj_out_cmd_hdr)] = {0};
	struct mlx5_devx_obj *geneve_tlv_opt_obj = mlx5_malloc(MLX5_MEM_ZERO,
						   sizeof(*geneve_tlv_opt_obj),
						   0, SOCKET_ID_ANY);

	if (!geneve_tlv_opt_obj) {
		DRV_LOG(ERR, "Failed to allocate geneve tlv option object.");
		rte_errno = ENOMEM;
		return NULL;
	}
	void *hdr = MLX5_ADDR_OF(create_geneve_tlv_option_in, in, hdr);
	void *opt = MLX5_ADDR_OF(create_geneve_tlv_option_in, in,
			geneve_tlv_opt);
	MLX5_SET(general_obj_in_cmd_hdr, hdr, opcode,
			MLX5_CMD_OP_CREATE_GENERAL_OBJECT);
	MLX5_SET(general_obj_in_cmd_hdr, hdr, obj_type,
		 MLX5_GENERAL_OBJ_TYPE_GENEVE_TLV_OPT);
	MLX5_SET(geneve_tlv_option, opt, option_class,
			rte_be_to_cpu_16(class));
	MLX5_SET(geneve_tlv_option, opt, option_type, type);
	MLX5_SET(geneve_tlv_option, opt, option_data_length, len);
	geneve_tlv_opt_obj->obj = mlx5_glue->devx_obj_create(ctx, in,
					sizeof(in), out, sizeof(out));
	if (!geneve_tlv_opt_obj->obj) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create Geneve tlv option "
				"Obj using DevX.");
		mlx5_free(geneve_tlv_opt_obj);
		return NULL;
	}
	geneve_tlv_opt_obj->id = MLX5_GET(general_obj_out_cmd_hdr, out, obj_id);
	return geneve_tlv_opt_obj;
}

int
mlx5_devx_cmd_wq_query(void *wq, uint32_t *counter_set_id)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	uint32_t in[MLX5_ST_SZ_DW(query_rq_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(query_rq_out)] = {0};
	int rc;
	void *rq_ctx;

	MLX5_SET(query_rq_in, in, opcode, MLX5_CMD_OP_QUERY_RQ);
	MLX5_SET(query_rq_in, in, rqn, ((struct ibv_wq *)wq)->wq_num);
	rc = mlx5_glue->devx_wq_query(wq, in, sizeof(in), out, sizeof(out));
	if (rc) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to query WQ counter set ID using DevX - "
			"rc = %d, errno = %d.", rc, errno);
		return -rc;
	};
	rq_ctx = MLX5_ADDR_OF(query_rq_out, out, rq_context);
	*counter_set_id = MLX5_GET(rqc, rq_ctx, counter_set_id);
	return 0;
#else
	(void)wq;
	(void)counter_set_id;
	return -ENOTSUP;
#endif
}

/*
 * Allocate queue counters via devx interface.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 *
 * @return
 *   Pointer to counter object on success, a NULL value otherwise and
 *   rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_queue_counter_alloc(void *ctx)
{
	struct mlx5_devx_obj *dcs = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*dcs), 0,
						SOCKET_ID_ANY);
	uint32_t in[MLX5_ST_SZ_DW(alloc_q_counter_in)]   = {0};
	uint32_t out[MLX5_ST_SZ_DW(alloc_q_counter_out)] = {0};

	if (!dcs) {
		rte_errno = ENOMEM;
		return NULL;
	}
	MLX5_SET(alloc_q_counter_in, in, opcode, MLX5_CMD_OP_ALLOC_Q_COUNTER);
	dcs->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in), out,
					      sizeof(out));
	if (!dcs->obj) {
		DRV_LOG(DEBUG, "Can't allocate q counter set by DevX - error "
			"%d.", errno);
		rte_errno = errno;
		mlx5_free(dcs);
		return NULL;
	}
	dcs->id = MLX5_GET(alloc_q_counter_out, out, counter_set_id);
	return dcs;
}

/**
 * Query queue counters values.
 *
 * @param[in] dcs
 *   devx object of the queue counter set.
 * @param[in] clear
 *   Whether hardware should clear the counters after the query or not.
 *  @param[out] out_of_buffers
 *   Number of dropped occurred due to lack of WQE for the associated QPs/RQs.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int
mlx5_devx_cmd_queue_counter_query(struct mlx5_devx_obj *dcs, int clear,
				  uint32_t *out_of_buffers)
{
	uint32_t out[MLX5_ST_SZ_BYTES(query_q_counter_out)] = {0};
	uint32_t in[MLX5_ST_SZ_DW(query_q_counter_in)] = {0};
	int rc;

	MLX5_SET(query_q_counter_in, in, opcode,
		 MLX5_CMD_OP_QUERY_Q_COUNTER);
	MLX5_SET(query_q_counter_in, in, op_mod, 0);
	MLX5_SET(query_q_counter_in, in, counter_set_id, dcs->id);
	MLX5_SET(query_q_counter_in, in, clear, !!clear);
	rc = mlx5_glue->devx_obj_query(dcs->obj, in, sizeof(in), out,
				       sizeof(out));
	if (rc) {
		DRV_LOG(ERR, "Failed to query devx q counter set - rc %d", rc);
		rte_errno = rc;
		return -rc;
	}
	*out_of_buffers = MLX5_GET(query_q_counter_out, out, out_of_buffer);
	return 0;
}

/**
 * Create general object of type DEK using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param [in] attr
 *   Pointer to DEK attributes structure.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_dek_obj(void *ctx, struct mlx5_devx_dek_attr *attr)
{
	uint32_t in[MLX5_ST_SZ_DW(create_dek_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(general_obj_out_cmd_hdr)] = {0};
	struct mlx5_devx_obj *dek_obj = NULL;
	void *ptr = NULL, *key_addr = NULL;

	dek_obj = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*dek_obj),
			      0, SOCKET_ID_ANY);
	if (dek_obj == NULL) {
		DRV_LOG(ERR, "Failed to allocate DEK object data");
		rte_errno = ENOMEM;
		return NULL;
	}
	ptr = MLX5_ADDR_OF(create_dek_in, in, hdr);
	MLX5_SET(general_obj_in_cmd_hdr, ptr, opcode,
		 MLX5_CMD_OP_CREATE_GENERAL_OBJECT);
	MLX5_SET(general_obj_in_cmd_hdr, ptr, obj_type,
		 MLX5_GENERAL_OBJ_TYPE_DEK);
	ptr = MLX5_ADDR_OF(create_dek_in, in, dek);
	MLX5_SET(dek, ptr, key_size, attr->key_size);
	MLX5_SET(dek, ptr, has_keytag, attr->has_keytag);
	MLX5_SET(dek, ptr, key_purpose, attr->key_purpose);
	MLX5_SET(dek, ptr, pd, attr->pd);
	MLX5_SET64(dek, ptr, opaque, attr->opaque);
	key_addr = MLX5_ADDR_OF(dek, ptr, key);
	memcpy(key_addr, (void *)(attr->key), MLX5_CRYPTO_KEY_MAX_SIZE);
	dek_obj->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in),
						  out, sizeof(out));
	if (dek_obj->obj == NULL) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create DEK obj using DevX.");
		mlx5_free(dek_obj);
		return NULL;
	}
	dek_obj->id = MLX5_GET(general_obj_out_cmd_hdr, out, obj_id);
	return dek_obj;
}

/**
 * Create general object of type IMPORT_KEK using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param [in] attr
 *   Pointer to IMPORT_KEK attributes structure.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_import_kek_obj(void *ctx,
				    struct mlx5_devx_import_kek_attr *attr)
{
	uint32_t in[MLX5_ST_SZ_DW(create_import_kek_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(general_obj_out_cmd_hdr)] = {0};
	struct mlx5_devx_obj *import_kek_obj = NULL;
	void *ptr = NULL, *key_addr = NULL;

	import_kek_obj = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*import_kek_obj),
				     0, SOCKET_ID_ANY);
	if (import_kek_obj == NULL) {
		DRV_LOG(ERR, "Failed to allocate IMPORT_KEK object data");
		rte_errno = ENOMEM;
		return NULL;
	}
	ptr = MLX5_ADDR_OF(create_import_kek_in, in, hdr);
	MLX5_SET(general_obj_in_cmd_hdr, ptr, opcode,
		 MLX5_CMD_OP_CREATE_GENERAL_OBJECT);
	MLX5_SET(general_obj_in_cmd_hdr, ptr, obj_type,
		 MLX5_GENERAL_OBJ_TYPE_IMPORT_KEK);
	ptr = MLX5_ADDR_OF(create_import_kek_in, in, import_kek);
	MLX5_SET(import_kek, ptr, key_size, attr->key_size);
	key_addr = MLX5_ADDR_OF(import_kek, ptr, key);
	memcpy(key_addr, (void *)(attr->key), MLX5_CRYPTO_KEY_MAX_SIZE);
	import_kek_obj->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in),
							 out, sizeof(out));
	if (import_kek_obj->obj == NULL) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create IMPORT_KEK object using DevX.");
		mlx5_free(import_kek_obj);
		return NULL;
	}
	import_kek_obj->id = MLX5_GET(general_obj_out_cmd_hdr, out, obj_id);
	return import_kek_obj;
}

/**
 * Create general object of type CREDENTIAL using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param [in] attr
 *   Pointer to CREDENTIAL attributes structure.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_credential_obj(void *ctx,
				    struct mlx5_devx_credential_attr *attr)
{
	uint32_t in[MLX5_ST_SZ_DW(create_credential_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(general_obj_out_cmd_hdr)] = {0};
	struct mlx5_devx_obj *credential_obj = NULL;
	void *ptr = NULL, *credential_addr = NULL;

	credential_obj = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*credential_obj),
				     0, SOCKET_ID_ANY);
	if (credential_obj == NULL) {
		DRV_LOG(ERR, "Failed to allocate CREDENTIAL object data");
		rte_errno = ENOMEM;
		return NULL;
	}
	ptr = MLX5_ADDR_OF(create_credential_in, in, hdr);
	MLX5_SET(general_obj_in_cmd_hdr, ptr, opcode,
		 MLX5_CMD_OP_CREATE_GENERAL_OBJECT);
	MLX5_SET(general_obj_in_cmd_hdr, ptr, obj_type,
		 MLX5_GENERAL_OBJ_TYPE_CREDENTIAL);
	ptr = MLX5_ADDR_OF(create_credential_in, in, credential);
	MLX5_SET(credential, ptr, credential_role, attr->credential_role);
	credential_addr = MLX5_ADDR_OF(credential, ptr, credential);
	memcpy(credential_addr, (void *)(attr->credential),
	       MLX5_CRYPTO_CREDENTIAL_SIZE);
	credential_obj->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in),
							 out, sizeof(out));
	if (credential_obj->obj == NULL) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create CREDENTIAL object using DevX.");
		mlx5_free(credential_obj);
		return NULL;
	}
	credential_obj->id = MLX5_GET(general_obj_out_cmd_hdr, out, obj_id);
	return credential_obj;
}

/**
 * Create general object of type CRYPTO_LOGIN using DevX API.
 *
 * @param[in] ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param [in] attr
 *   Pointer to CRYPTO_LOGIN attributes structure.
 *
 * @return
 *   The DevX object created, NULL otherwise and rte_errno is set.
 */
struct mlx5_devx_obj *
mlx5_devx_cmd_create_crypto_login_obj(void *ctx,
				      struct mlx5_devx_crypto_login_attr *attr)
{
	uint32_t in[MLX5_ST_SZ_DW(create_crypto_login_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(general_obj_out_cmd_hdr)] = {0};
	struct mlx5_devx_obj *crypto_login_obj = NULL;
	void *ptr = NULL, *credential_addr = NULL;

	crypto_login_obj = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*crypto_login_obj),
				       0, SOCKET_ID_ANY);
	if (crypto_login_obj == NULL) {
		DRV_LOG(ERR, "Failed to allocate CRYPTO_LOGIN object data");
		rte_errno = ENOMEM;
		return NULL;
	}
	ptr = MLX5_ADDR_OF(create_crypto_login_in, in, hdr);
	MLX5_SET(general_obj_in_cmd_hdr, ptr, opcode,
		 MLX5_CMD_OP_CREATE_GENERAL_OBJECT);
	MLX5_SET(general_obj_in_cmd_hdr, ptr, obj_type,
		 MLX5_GENERAL_OBJ_TYPE_CRYPTO_LOGIN);
	ptr = MLX5_ADDR_OF(create_crypto_login_in, in, crypto_login);
	MLX5_SET(crypto_login, ptr, credential_pointer,
		 attr->credential_pointer);
	MLX5_SET(crypto_login, ptr, session_import_kek_ptr,
		 attr->session_import_kek_ptr);
	credential_addr = MLX5_ADDR_OF(crypto_login, ptr, credential);
	memcpy(credential_addr, (void *)(attr->credential),
	       MLX5_CRYPTO_CREDENTIAL_SIZE);
	crypto_login_obj->obj = mlx5_glue->devx_obj_create(ctx, in, sizeof(in),
							   out, sizeof(out));
	if (crypto_login_obj->obj == NULL) {
		rte_errno = errno;
		DRV_LOG(ERR, "Failed to create CRYPTO_LOGIN obj using DevX.");
		mlx5_free(crypto_login_obj);
		return NULL;
	}
	crypto_login_obj->id = MLX5_GET(general_obj_out_cmd_hdr, out, obj_id);
	return crypto_login_obj;
}

/**
 * Query LAG context.
 *
 * @param[in] ctx
 *   Pointer to ibv_context, returned from mlx5dv_open_device.
 * @param[out] lag_ctx
 *   Pointer to struct mlx5_devx_lag_context, to be set by the routine.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int
mlx5_devx_cmd_query_lag(void *ctx,
			struct mlx5_devx_lag_context *lag_ctx)
{
	uint32_t in[MLX5_ST_SZ_DW(query_lag_in)] = {0};
	uint32_t out[MLX5_ST_SZ_DW(query_lag_out)] = {0};
	void *lctx;
	int rc;

	MLX5_SET(query_lag_in, in, opcode, MLX5_CMD_OP_QUERY_LAG);
	rc = mlx5_glue->devx_general_cmd(ctx, in, sizeof(in), out, sizeof(out));
	if (rc)
		goto error;
	lctx = MLX5_ADDR_OF(query_lag_out, out, context);
	lag_ctx->fdb_selection_mode = MLX5_GET(lag_context, lctx,
					       fdb_selection_mode);
	lag_ctx->port_select_mode = MLX5_GET(lag_context, lctx,
					       port_select_mode);
	lag_ctx->lag_state = MLX5_GET(lag_context, lctx, lag_state);
	lag_ctx->tx_remap_affinity_2 = MLX5_GET(lag_context, lctx,
						tx_remap_affinity_2);
	lag_ctx->tx_remap_affinity_1 = MLX5_GET(lag_context, lctx,
						tx_remap_affinity_1);
	return 0;
error:
	rc = (rc > 0) ? -rc : rc;
	return rc;
}
