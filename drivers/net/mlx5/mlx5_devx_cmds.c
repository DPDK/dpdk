// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018 Mellanox Technologies, Ltd */

#include <rte_flow_driver.h>

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
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int mlx5_devx_cmd_flow_counter_alloc(struct ibv_context *ctx,
				     struct mlx5_devx_counter_set *dcs)
{
	uint32_t in[MLX5_ST_SZ_DW(alloc_flow_counter_in)]   = {0};
	uint32_t out[MLX5_ST_SZ_DW(alloc_flow_counter_out)] = {0};
	int status, syndrome;

	MLX5_SET(alloc_flow_counter_in, in, opcode,
		 MLX5_CMD_OP_ALLOC_FLOW_COUNTER);
	dcs->obj = mlx5_glue->devx_obj_create(ctx, in,
					      sizeof(in), out, sizeof(out));
	if (!dcs->obj)
		return -errno;
	status = MLX5_GET(query_flow_counter_out, out, status);
	syndrome = MLX5_GET(query_flow_counter_out, out, syndrome);
	if (status) {
		DRV_LOG(DEBUG, "Failed to create devx counters, "
			"status %x, syndrome %x", status, syndrome);
		return -1;
	}
	dcs->id = MLX5_GET(alloc_flow_counter_out,
			   out, flow_counter_id);
	return 0;
}

/**
 * Free flow counters obtained via devx interface.
 *
 * @param[in] obj
 *   devx object that was obtained from mlx5_devx_cmd_fc_alloc.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int mlx5_devx_cmd_flow_counter_free(struct mlx5dv_devx_obj *obj)
{
	return mlx5_glue->devx_obj_destroy(obj);
}

/**
 * Query flow counters values.
 *
 * @param[in] dcs
 *   devx object that was obtained from mlx5_devx_cmd_fc_alloc.
 * @param[in] clear
 *   Whether hardware should clear the counters after the query or not.
 *  @param pkts
 *   The number of packets that matched the flow.
 *  @param bytes
 *    The number of bytes that matched the flow.
 *
 * @return
 *   0 on success, a negative value otherwise.
 */
int
mlx5_devx_cmd_flow_counter_query(struct mlx5_devx_counter_set *dcs,
				 int clear __rte_unused,
				 uint64_t *pkts, uint64_t *bytes)
{
	uint32_t out[MLX5_ST_SZ_BYTES(query_flow_counter_out) +
		MLX5_ST_SZ_BYTES(traffic_counter)]   = {0};
	uint32_t in[MLX5_ST_SZ_DW(query_flow_counter_in)] = {0};
	void *stats;
	int status, syndrome, rc;

	MLX5_SET(query_flow_counter_in, in, opcode,
		 MLX5_CMD_OP_QUERY_FLOW_COUNTER);
	MLX5_SET(query_flow_counter_in, in, op_mod, 0);
	MLX5_SET(query_flow_counter_in, in, flow_counter_id, dcs->id);
	rc = mlx5_glue->devx_obj_query(dcs->obj,
				       in, sizeof(in), out, sizeof(out));
	if (rc)
		return rc;
	status = MLX5_GET(query_flow_counter_out, out, status);
	syndrome = MLX5_GET(query_flow_counter_out, out, syndrome);
	if (status) {
		DRV_LOG(DEBUG, "Failed to query devx counters, "
			"id %d, status %x, syndrome = %x",
			status, syndrome, dcs->id);
		return -1;
	}
	stats = MLX5_ADDR_OF(query_flow_counter_out,
			     out, flow_statistics);
	*pkts = MLX5_GET64(traffic_counter, stats, packets);
	*bytes = MLX5_GET64(traffic_counter, stats, octets);
	return 0;
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
		return rc;
	status = MLX5_GET(query_hca_cap_out, out, status);
	syndrome = MLX5_GET(query_hca_cap_out, out, syndrome);
	if (status) {
		DRV_LOG(DEBUG, "Failed to query devx HCA capabilities, "
			"status %x, syndrome = %x",
			status, syndrome);
		return -1;
	}
	hcattr = MLX5_ADDR_OF(query_hca_cap_out, out, capability);
	attr->eswitch_manager = MLX5_GET(cmd_hca_cap, hcattr, eswitch_manager);
	return 0;
}
