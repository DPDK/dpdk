/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */
#include <netinet/in.h>

#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_common.h>

#include <mlx5_common.h>

#include "mlx5_vdpa_utils.h"
#include "mlx5_vdpa.h"

static void
mlx5_vdpa_rss_flows_destroy(struct mlx5_vdpa_priv *priv)
{
	unsigned i;

	for (i = 0; i < RTE_DIM(priv->steer.rss); ++i) {
		if (priv->steer.rss[i].flow) {
			claim_zero(mlx5_glue->dv_destroy_flow
						     (priv->steer.rss[i].flow));
			priv->steer.rss[i].flow = NULL;
		}
		if (priv->steer.rss[i].tir_action) {
			claim_zero(mlx5_glue->destroy_flow_action
					       (priv->steer.rss[i].tir_action));
			priv->steer.rss[i].tir_action = NULL;
		}
		if (priv->steer.rss[i].tir) {
			claim_zero(mlx5_devx_cmd_destroy
						      (priv->steer.rss[i].tir));
			priv->steer.rss[i].tir = NULL;
		}
		if (priv->steer.rss[i].matcher) {
			claim_zero(mlx5_glue->dv_destroy_flow_matcher
						  (priv->steer.rss[i].matcher));
			priv->steer.rss[i].matcher = NULL;
		}
	}
}

void
mlx5_vdpa_steer_unset(struct mlx5_vdpa_priv *priv)
{
	mlx5_vdpa_rss_flows_destroy(priv);
	if (priv->steer.rqt) {
		claim_zero(mlx5_devx_cmd_destroy(priv->steer.rqt));
		priv->steer.rqt = NULL;
	}
}

#define MLX5_VDPA_DEFAULT_RQT_SIZE 512
/*
 * Return the number of queues configured to the table on success, otherwise
 * -1 on error.
 */
static int
mlx5_vdpa_rqt_prepare(struct mlx5_vdpa_priv *priv, bool is_dummy)
{
	int i;
	uint32_t rqt_n = RTE_MIN(MLX5_VDPA_DEFAULT_RQT_SIZE,
				 1 << priv->log_max_rqt_size);
	struct mlx5_devx_rqt_attr *attr = rte_zmalloc(__func__, sizeof(*attr)
						      + rqt_n *
						      sizeof(uint32_t), 0);
	uint32_t k = 0, j;
	int ret = 0, num;
	uint16_t nr_vring = is_dummy ?
	(((priv->queues * 2) < priv->caps.max_num_virtio_queues) ?
	(priv->queues * 2) : priv->caps.max_num_virtio_queues) : priv->nr_virtqs;

	if (!attr) {
		DRV_LOG(ERR, "Failed to allocate RQT attributes memory.");
		rte_errno = ENOMEM;
		return -ENOMEM;
	}
	for (i = 0; i < nr_vring; i++) {
		if (is_virtq_recvq(i, priv->nr_virtqs) &&
			(is_dummy || (priv->virtqs[i].enable &&
			priv->virtqs[i].configured)) &&
			priv->virtqs[i].virtq) {
			attr->rq_list[k] = priv->virtqs[i].virtq->id;
			k++;
		}
	}
	if (k == 0)
		/* No enabled RQ to configure for RSS. */
		return 0;
	num = (int)k;
	for (j = 0; k != rqt_n; ++k, ++j)
		attr->rq_list[k] = attr->rq_list[j];
	attr->rq_type = MLX5_INLINE_Q_TYPE_VIRTQ;
	attr->rqt_max_size = rqt_n;
	attr->rqt_actual_size = rqt_n;
	if (!priv->steer.rqt) {
		priv->steer.rqt = mlx5_devx_cmd_create_rqt(priv->cdev->ctx,
							   attr);
		if (!priv->steer.rqt) {
			DRV_LOG(ERR, "Failed to create RQT.");
			ret = -rte_errno;
		}
	} else {
		ret = mlx5_devx_cmd_modify_rqt(priv->steer.rqt, attr);
		if (ret)
			DRV_LOG(ERR, "Failed to modify RQT.");
	}
	rte_free(attr);
	return ret ? -1 : num;
}

static int __rte_unused
mlx5_vdpa_rss_flows_create(struct mlx5_vdpa_priv *priv)
{
#ifdef HAVE_MLX5DV_DR
	struct mlx5_devx_tir_attr tir_att = {
		.disp_type = MLX5_TIRC_DISP_TYPE_INDIRECT,
		.rx_hash_fn = MLX5_RX_HASH_FN_TOEPLITZ,
		.transport_domain = priv->td->id,
		.indirect_table = priv->steer.rqt->id,
		.rx_hash_symmetric = 1,
		.rx_hash_toeplitz_key = { 0x2c, 0xc6, 0x81, 0xd1,
					  0x5b, 0xdb, 0xf4, 0xf7,
					  0xfc, 0xa2, 0x83, 0x19,
					  0xdb, 0x1a, 0x3e, 0x94,
					  0x6b, 0x9e, 0x38, 0xd9,
					  0x2c, 0x9c, 0x03, 0xd1,
					  0xad, 0x99, 0x44, 0xa7,
					  0xd9, 0x56, 0x3d, 0x59,
					  0x06, 0x3c, 0x25, 0xf3,
					  0xfc, 0x1f, 0xdc, 0x2a },
	};
	struct {
		size_t size;
		/**< Size of match value. Do NOT split size and key! */
		uint32_t buf[MLX5_ST_SZ_DW(fte_match_param)];
		/**< Matcher value. This value is used as the mask or a key. */
	} matcher_mask = {
				.size = sizeof(matcher_mask.buf) -
					MLX5_ST_SZ_BYTES(fte_match_set_misc4) -
					MLX5_ST_SZ_BYTES(fte_match_set_misc5),
			},
	  matcher_value = {
				.size = sizeof(matcher_value.buf) -
					MLX5_ST_SZ_BYTES(fte_match_set_misc4) -
					MLX5_ST_SZ_BYTES(fte_match_set_misc5),
			};
	struct mlx5dv_flow_matcher_attr dv_attr = {
		.type = IBV_FLOW_ATTR_NORMAL,
		.match_mask = (void *)&matcher_mask,
	};
	void *match_m = matcher_mask.buf;
	void *match_v = matcher_value.buf;
	void *headers_m = MLX5_ADDR_OF(fte_match_param, match_m, outer_headers);
	void *headers_v = MLX5_ADDR_OF(fte_match_param, match_v, outer_headers);
	void *actions[1];
	const uint8_t l3_hash =
		(1 << MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_SRC_IP) |
		(1 << MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_DST_IP);
	const uint8_t l4_hash =
		(1 << MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_L4_SPORT) |
		(1 << MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_L4_DPORT);
	enum { PRIO, CRITERIA, IP_VER_M, IP_VER_V, IP_PROT_M, IP_PROT_V, L3_BIT,
	       L4_BIT, HASH, END};
	const uint8_t vars[RTE_DIM(priv->steer.rss)][END] = {
		{ 7, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ 6, 1 << MLX5_MATCH_CRITERIA_ENABLE_OUTER_BIT, 0xf, 4, 0, 0,
		 MLX5_L3_PROT_TYPE_IPV4, 0, l3_hash },
		{ 6, 1 << MLX5_MATCH_CRITERIA_ENABLE_OUTER_BIT, 0xf, 6, 0, 0,
		 MLX5_L3_PROT_TYPE_IPV6, 0, l3_hash },
		{ 5, 1 << MLX5_MATCH_CRITERIA_ENABLE_OUTER_BIT, 0xf, 4, 0xff,
		 IPPROTO_UDP, MLX5_L3_PROT_TYPE_IPV4, MLX5_L4_PROT_TYPE_UDP,
		 l3_hash | l4_hash },
		{ 5, 1 << MLX5_MATCH_CRITERIA_ENABLE_OUTER_BIT, 0xf, 4, 0xff,
		 IPPROTO_TCP, MLX5_L3_PROT_TYPE_IPV4, MLX5_L4_PROT_TYPE_TCP,
		 l3_hash | l4_hash },
		{ 5, 1 << MLX5_MATCH_CRITERIA_ENABLE_OUTER_BIT, 0xf, 6, 0xff,
		 IPPROTO_UDP, MLX5_L3_PROT_TYPE_IPV6, MLX5_L4_PROT_TYPE_UDP,
		 l3_hash | l4_hash },
		{ 5, 1 << MLX5_MATCH_CRITERIA_ENABLE_OUTER_BIT, 0xf, 6, 0xff,
		 IPPROTO_TCP, MLX5_L3_PROT_TYPE_IPV6, MLX5_L4_PROT_TYPE_TCP,
		 l3_hash | l4_hash },
	};
	unsigned i;

	for (i = 0; i < RTE_DIM(priv->steer.rss); ++i) {
		dv_attr.priority = vars[i][PRIO];
		dv_attr.match_criteria_enable = vars[i][CRITERIA];
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_version,
			 vars[i][IP_VER_M]);
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_version,
			 vars[i][IP_VER_V]);
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_protocol,
			 vars[i][IP_PROT_M]);
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol,
			 vars[i][IP_PROT_V]);
		tir_att.rx_hash_field_selector_outer.l3_prot_type =
								vars[i][L3_BIT];
		tir_att.rx_hash_field_selector_outer.l4_prot_type =
								vars[i][L4_BIT];
		tir_att.rx_hash_field_selector_outer.selected_fields =
								  vars[i][HASH];
		priv->steer.rss[i].matcher = mlx5_glue->dv_create_flow_matcher
				   (priv->cdev->ctx, &dv_attr, priv->steer.tbl);
		if (!priv->steer.rss[i].matcher) {
			DRV_LOG(ERR, "Failed to create matcher %d.", i);
			goto error;
		}
		priv->steer.rss[i].tir = mlx5_devx_cmd_create_tir
						    (priv->cdev->ctx, &tir_att);
		if (!priv->steer.rss[i].tir) {
			DRV_LOG(ERR, "Failed to create TIR %d.", i);
			goto error;
		}
		priv->steer.rss[i].tir_action =
				mlx5_glue->dv_create_flow_action_dest_devx_tir
						  (priv->steer.rss[i].tir->obj);
		if (!priv->steer.rss[i].tir_action) {
			DRV_LOG(ERR, "Failed to create TIR action %d.", i);
			goto error;
		}
		actions[0] = priv->steer.rss[i].tir_action;
		priv->steer.rss[i].flow = mlx5_glue->dv_create_flow
					(priv->steer.rss[i].matcher,
					 (void *)&matcher_value, 1, actions);
		if (!priv->steer.rss[i].flow) {
			DRV_LOG(ERR, "Failed to create flow %d.", i);
			goto error;
		}
	}
	return 0;
error:
	/* Resources will be freed by the caller. */
	return -1;
#else
	(void)priv;
	return -ENOTSUP;
#endif /* HAVE_MLX5DV_DR */
}

int
mlx5_vdpa_steer_update(struct mlx5_vdpa_priv *priv, bool is_dummy)
{
	int ret;

	pthread_mutex_lock(&priv->steer_update_lock);
	ret = mlx5_vdpa_rqt_prepare(priv, is_dummy);
	if (ret == 0) {
		mlx5_vdpa_steer_unset(priv);
	} else if (ret < 0) {
		pthread_mutex_unlock(&priv->steer_update_lock);
		return ret;
	} else if (!priv->steer.rss[0].flow) {
		ret = mlx5_vdpa_rss_flows_create(priv);
		if (ret) {
			DRV_LOG(ERR, "Cannot create RSS flows.");
			pthread_mutex_unlock(&priv->steer_update_lock);
			return -1;
		}
	}
	pthread_mutex_unlock(&priv->steer_update_lock);
	return 0;
}

int
mlx5_vdpa_steer_setup(struct mlx5_vdpa_priv *priv)
{
	if (mlx5_vdpa_steer_update(priv, false))
		goto error;
	return 0;
error:
	mlx5_vdpa_steer_unset(priv);
	return -1;
}
