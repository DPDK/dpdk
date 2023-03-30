/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef MLX5_REGEX_H
#define MLX5_REGEX_H

#include <rte_regexdev.h>

#include <infiniband/verbs.h>
#include <infiniband/mlx5dv.h>

#include <mlx5_common.h>
#include <mlx5_common_mr.h>
#include <mlx5_common_devx.h>

#include "mlx5_rxp.h"
#include "mlx5_regex_utils.h"

struct mlx5_regex_hw_qp {
	uint16_t log_nb_desc; /* Log 2 number of desc for this object. */
	struct mlx5_devx_qp qp_obj; /* The QP DevX object. */
	size_t pi, db_pi;
	size_t ci;
	uint32_t qpn;
};

struct mlx5_regex_cq {
	uint32_t log_nb_desc; /* Log 2 number of desc for this object. */
	struct mlx5_devx_cq cq_obj; /* The CQ DevX object. */
	size_t ci;
};

struct mlx5_regex_qp {
	uint32_t flags; /* QP user flags. */
	uint32_t nb_desc; /* Total number of desc for this qp. */
	struct mlx5_regex_hw_qp *qps; /* Pointer to qp array. */
	uint16_t nb_obj; /* Number of qp objects. */
	struct mlx5_regex_cq cq; /* CQ struct. */
	uint64_t free_qps;
	struct mlx5_regex_job *jobs;
	struct ibv_mr *metadata;
	struct ibv_mr *outputs;
	struct ibv_mr *imkey_addr; /* Indirect mkey array region. */
	size_t ci, pi;
	struct mlx5_mr_ctrl mr_ctrl;
};

struct mlx5_regex_priv {
	TAILQ_ENTRY(mlx5_regex_priv) next;
	struct mlx5_common_device *cdev; /* Backend mlx5 device. */
	struct rte_regexdev *regexdev; /* Pointer to the RegEx dev. */
	uint16_t nb_queues; /* Number of queues. */
	struct mlx5_regex_qp *qps; /* Pointer to the qp array. */
	uint16_t nb_max_matches; /* Max number of matches. */
	enum mlx5_rxp_program_mode prog_mode;
	uint32_t nb_engines; /* Number of RegEx engines. */
	struct mlx5_uar uar; /* UAR object. */
	uint8_t is_bf2; /* The device is BF2 device. */
	uint8_t has_umr; /* The device supports UMR. */
	uint32_t mmo_regex_qp_cap:1;
	uint32_t mmo_regex_sq_cap:1;
};

/* mlx5_regex.c */
int mlx5_regex_start(struct rte_regexdev *dev);
int mlx5_regex_stop(struct rte_regexdev *dev);
int mlx5_regex_close(struct rte_regexdev *dev);

/* mlx5_rxp.c */
int mlx5_regex_info_get(struct rte_regexdev *dev,
			struct rte_regexdev_info *info);
int mlx5_regex_configure(struct rte_regexdev *dev,
			 const struct rte_regexdev_config *cfg);
int mlx5_regex_rules_db_import(struct rte_regexdev *dev,
			       const char *rule_db, uint32_t rule_db_len);
int mlx5_regex_check_rof_version(uint32_t combined_rof_version);
int mlx5_regex_parse_rules_db(struct mlx5_regex_priv *priv,
			       const char **rule_db, uint32_t *rule_db_len);
int mlx5_regex_get_rxp_vers(uint32_t regexp_version, uint32_t *target_rxp_vers);

/* mlx5_regex_devx.c */
int mlx5_devx_regex_rules_program(void *ctx, uint8_t engine, uint32_t rof_mkey,
				uint32_t rof_size, uint64_t db_mkey_offset);

/* mlx5_regex_control.c */
int mlx5_regex_qp_setup(struct rte_regexdev *dev, uint16_t qp_ind,
			const struct rte_regexdev_qp_conf *cfg);
void mlx5_regex_clean_ctrl(struct rte_regexdev *dev);

/* mlx5_regex_fastpath.c */
int mlx5_regexdev_setup_fastpath(struct mlx5_regex_priv *priv, uint32_t qp_id);
void mlx5_regexdev_teardown_fastpath(struct mlx5_regex_priv *priv,
				     uint32_t qp_id);
uint16_t mlx5_regexdev_enqueue(struct rte_regexdev *dev, uint16_t qp_id,
		       struct rte_regex_ops **ops, uint16_t nb_ops);
uint16_t mlx5_regexdev_dequeue(struct rte_regexdev *dev, uint16_t qp_id,
		       struct rte_regex_ops **ops, uint16_t nb_ops);
uint16_t mlx5_regexdev_enqueue_gga(struct rte_regexdev *dev, uint16_t qp_id,
		       struct rte_regex_ops **ops, uint16_t nb_ops);
uint16_t mlx5_regexdev_max_segs_get(void);

#endif /* MLX5_REGEX_H */
