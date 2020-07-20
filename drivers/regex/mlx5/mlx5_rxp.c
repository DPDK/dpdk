/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <rte_log.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_regexdev.h>
#include <rte_regexdev_core.h>
#include <rte_regexdev_driver.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_prm.h>
#include <mlx5_common_os.h>

#include "mlx5_regex.h"
#include "mlx5_regex_utils.h"
#include "mlx5_rxp_csrs.h"
#include "mlx5_rxp.h"

#define MLX5_REGEX_MAX_MATCHES MLX5_RXP_MAX_MATCHES
#define MLX5_REGEX_MAX_PAYLOAD_SIZE MLX5_RXP_MAX_JOB_LENGTH
#define MLX5_REGEX_MAX_RULES_PER_GROUP UINT32_MAX
#define MLX5_REGEX_MAX_GROUPS MLX5_RXP_MAX_SUBSETS

/* Private Declarations */
static int
rxp_poll_csr_for_value(struct ibv_context *ctx, uint32_t *value,
		       uint32_t address, uint32_t expected_value,
		       uint32_t expected_mask, uint32_t timeout_ms, uint8_t id);
static int
mlnx_set_database(struct mlx5_regex_priv *priv, uint8_t id, uint8_t db_to_use);
static int
mlnx_resume_database(struct mlx5_regex_priv *priv, uint8_t id);
static int
mlnx_update_database(struct mlx5_regex_priv *priv, uint8_t id);
static int
program_rxp_rules(struct mlx5_regex_priv *priv,
		  struct mlx5_rxp_ctl_rules_pgm *rules, uint8_t id);
static int
rxp_init_eng(struct mlx5_regex_priv *priv, uint8_t id);
static int
write_private_rules(struct mlx5_regex_priv *priv,
		    struct mlx5_rxp_ctl_rules_pgm *rules,
		    uint8_t id);
static int
write_shared_rules(struct mlx5_regex_priv *priv,
		   struct mlx5_rxp_ctl_rules_pgm *rules, uint32_t count,
		   uint8_t db_to_program);
static int
rxp_db_setup(struct mlx5_regex_priv *priv);
static void
rxp_dump_csrs(struct ibv_context *ctx, uint8_t id);
static int
rxp_write_rules_via_cp(struct ibv_context *ctx,
		       struct mlx5_rxp_rof_entry *rules,
		       int count, uint8_t id);
static int
rxp_flush_rules(struct ibv_context *ctx, struct mlx5_rxp_rof_entry *rules,
		int count, uint8_t id);
static int
rxp_start_engine(struct ibv_context *ctx, uint8_t id);
static int
rxp_stop_engine(struct ibv_context *ctx, uint8_t id);

static void __rte_unused
rxp_dump_csrs(struct ibv_context *ctx __rte_unused, uint8_t id __rte_unused)
{
	uint32_t reg, i;

	/* Main CSRs*/
	for (i = 0; i < MLX5_RXP_CSR_NUM_ENTRIES; i++) {
		if (mlx5_devx_regex_register_read(ctx, id,
						  (MLX5_RXP_CSR_WIDTH * i) +
						  MLX5_RXP_CSR_BASE_ADDRESS,
						  &reg)) {
			DRV_LOG(ERR, "Failed to read Main CSRs Engine %d!", id);
			return;
		}
		DRV_LOG(DEBUG, "RXP Main CSRs (Eng%d) register (%d): %08x",
			id, i, reg);
	}
	/* RTRU CSRs*/
	for (i = 0; i < MLX5_RXP_CSR_NUM_ENTRIES; i++) {
		if (mlx5_devx_regex_register_read(ctx, id,
						  (MLX5_RXP_CSR_WIDTH * i) +
						 MLX5_RXP_RTRU_CSR_BASE_ADDRESS,
						  &reg)) {
			DRV_LOG(ERR, "Failed to read RTRU CSRs Engine %d!", id);
			return;
		}
		DRV_LOG(DEBUG, "RXP RTRU CSRs (Eng%d) register (%d): %08x",
			id, i, reg);
	}
	/* STAT CSRs */
	for (i = 0; i < MLX5_RXP_CSR_NUM_ENTRIES; i++) {
		if (mlx5_devx_regex_register_read(ctx, id,
						  (MLX5_RXP_CSR_WIDTH * i) +
						MLX5_RXP_STATS_CSR_BASE_ADDRESS,
						  &reg)) {
			DRV_LOG(ERR, "Failed to read STAT CSRs Engine %d!", id);
			return;
		}
		DRV_LOG(DEBUG, "RXP STAT CSRs (Eng%d) register (%d): %08x",
			id, i, reg);
	}
}

int
mlx5_regex_info_get(struct rte_regexdev *dev __rte_unused,
		    struct rte_regexdev_info *info)
{
	info->max_matches = MLX5_REGEX_MAX_MATCHES;
	info->max_payload_size = MLX5_REGEX_MAX_PAYLOAD_SIZE;
	info->max_rules_per_group = MLX5_REGEX_MAX_RULES_PER_GROUP;
	info->max_groups = MLX5_REGEX_MAX_GROUPS;
	info->max_queue_pairs = 1;
	info->regexdev_capa = RTE_REGEXDEV_SUPP_PCRE_GREEDY_F;
	info->rule_flags = 0;
	info->max_queue_pairs = 10;
	return 0;
}

/**
 * Actual writing of RXP instructions to RXP via CSRs.
 */
static int
rxp_write_rules_via_cp(struct ibv_context *ctx,
		       struct mlx5_rxp_rof_entry *rules,
		       int count, uint8_t id)
{
	int i, ret = 0;
	uint32_t tmp;

	for (i = 0; i < count; i++) {
		tmp = (uint32_t)rules[i].value;
		ret |= mlx5_devx_regex_register_write(ctx, id,
						      MLX5_RXP_RTRU_CSR_DATA_0,
						      tmp);
		tmp = (uint32_t)(rules[i].value >> 32);
		ret |= mlx5_devx_regex_register_write(ctx, id,
						      MLX5_RXP_RTRU_CSR_DATA_0 +
						      MLX5_RXP_CSR_WIDTH, tmp);
		tmp = rules[i].addr;
		ret |= mlx5_devx_regex_register_write(ctx, id,
						      MLX5_RXP_RTRU_CSR_ADDR,
						      tmp);
		if (ret) {
			DRV_LOG(ERR, "Failed to copy instructions to RXP.");
			return -1;
		}
	}
	DRV_LOG(DEBUG, "Written %d instructions", count);
	return 0;
}

static int
rxp_flush_rules(struct ibv_context *ctx, struct mlx5_rxp_rof_entry *rules,
		int count, uint8_t id)
{
	uint32_t val, fifo_depth;
	int ret;

	ret = rxp_write_rules_via_cp(ctx, rules, count, id);
	if (ret < 0) {
		DRV_LOG(ERR, "Failed to write rules via CSRs.");
		return -1;
	}
	ret = mlx5_devx_regex_register_read(ctx, id,
					    MLX5_RXP_RTRU_CSR_CAPABILITY,
					    &fifo_depth);
	if (ret) {
		DRV_LOG(ERR, "CSR read failed!");
		return -1;
	}
	ret = rxp_poll_csr_for_value(ctx, &val, MLX5_RXP_RTRU_CSR_FIFO_STAT,
				     count, ~0,
				     MLX5_RXP_POLL_CSR_FOR_VALUE_TIMEOUT, id);
	if (ret < 0) {
		DRV_LOG(ERR, "Rules not rx by RXP: credit: %d, depth: %d", val,
			fifo_depth);
		return ret;
	}
	DRV_LOG(DEBUG, "RTRU FIFO depth: 0x%x", fifo_depth);
	DRV_LOG(DEBUG, "Rules flush took %d cycles.", ret);
	ret = mlx5_devx_regex_register_read(ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
					    &val);
	if (ret) {
		DRV_LOG(ERR, "CSR read failed!");
		return -1;
	}
	val |= MLX5_RXP_RTRU_CSR_CTRL_GO;
	ret = mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
					     val);
	ret = rxp_poll_csr_for_value(ctx, &val, MLX5_RXP_RTRU_CSR_STATUS,
				     MLX5_RXP_RTRU_CSR_STATUS_UPDATE_DONE,
				     MLX5_RXP_RTRU_CSR_STATUS_UPDATE_DONE,
				     MLX5_RXP_POLL_CSR_FOR_VALUE_TIMEOUT, id);
	if (ret < 0) {
		DRV_LOG(ERR, "Rules update timeout: 0x%08X", val);
		return ret;
	}
	DRV_LOG(DEBUG, "Rules update took %d cycles", ret);
	if (mlx5_devx_regex_register_read(ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
					  &val)) {
		DRV_LOG(ERR, "CSR read failed!");
		return -1;
	}
	val &= ~(MLX5_RXP_RTRU_CSR_CTRL_GO);
	if (mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
					   val)) {
		DRV_LOG(ERR, "CSR write write failed!");
		return -1;
	}

	DRV_LOG(DEBUG, "RXP Flush rules finished.");
	return 0;
}

static int
rxp_poll_csr_for_value(struct ibv_context *ctx, uint32_t *value,
		       uint32_t address, uint32_t expected_value,
		       uint32_t expected_mask, uint32_t timeout_ms, uint8_t id)
{
	unsigned int i;
	int ret = 0;

	ret = -EBUSY;
	for (i = 0; i < timeout_ms; i++) {
		if (mlx5_devx_regex_register_read(ctx, id, address, value))
			return -1;
		if ((*value & expected_mask) == expected_value) {
			ret = 0;
			break;
		}
		rte_delay_us(1000);
	}
	return ret;
}

static int
rxp_start_engine(struct ibv_context *ctx, uint8_t id)
{
	uint32_t ctrl;
	int ret;

	ret = mlx5_devx_regex_register_read(ctx, id, MLX5_RXP_CSR_CTRL, &ctrl);
	if (ret)
		return ret;
	ctrl |= MLX5_RXP_CSR_CTRL_GO;
	ctrl |= MLX5_RXP_CSR_CTRL_DISABLE_L2C;
	ret = mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_CSR_CTRL, ctrl);
	return ret;
}

static int
rxp_stop_engine(struct ibv_context *ctx, uint8_t id)
{
	uint32_t ctrl;
	int ret;

	ret = mlx5_devx_regex_register_read(ctx, id, MLX5_RXP_CSR_CTRL, &ctrl);
	if (ret)
		return ret;
	ctrl &= ~MLX5_RXP_CSR_CTRL_GO;
	ret = mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_CSR_CTRL, ctrl);
	return ret;
}

static int
rxp_init_rtru(struct ibv_context *ctx, uint8_t id, uint32_t init_bits)
{
	uint32_t ctrl_value;
	uint32_t poll_value;
	uint32_t expected_value;
	uint32_t expected_mask;
	int ret = 0;

	/* Read the rtru ctrl CSR. */
	ret = mlx5_devx_regex_register_read(ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
					    &ctrl_value);
	if (ret)
		return -1;
	/* Clear any previous init modes. */
	ctrl_value &= ~(MLX5_RXP_RTRU_CSR_CTRL_INIT_MODE_MASK);
	if (ctrl_value & MLX5_RXP_RTRU_CSR_CTRL_INIT) {
		ctrl_value &= ~(MLX5_RXP_RTRU_CSR_CTRL_INIT);
		mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
					       ctrl_value);
	}
	/* Set the init_mode bits in the rtru ctrl CSR. */
	ctrl_value |= init_bits;
	mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
				       ctrl_value);
	/* Need to sleep for a short period after pulsing the rtru init bit. */
	rte_delay_us(20000);
	/* Poll the rtru status CSR until all the init done bits are set. */
	DRV_LOG(DEBUG, "waiting for RXP rule memory to complete init");
	/* Set the init bit in the rtru ctrl CSR. */
	ctrl_value |= MLX5_RXP_RTRU_CSR_CTRL_INIT;
	mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
				       ctrl_value);
	/* Clear the init bit in the rtru ctrl CSR */
	ctrl_value &= ~MLX5_RXP_RTRU_CSR_CTRL_INIT;
	mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
				       ctrl_value);
	/* Check that the following bits are set in the RTRU_CSR. */
	if (init_bits == MLX5_RXP_RTRU_CSR_CTRL_INIT_MODE_L1_L2) {
		/* Must be incremental mode */
		expected_value = MLX5_RXP_RTRU_CSR_STATUS_L1C_INIT_DONE |
			MLX5_RXP_RTRU_CSR_STATUS_L2C_INIT_DONE;
	} else {
		expected_value = MLX5_RXP_RTRU_CSR_STATUS_IM_INIT_DONE |
			MLX5_RXP_RTRU_CSR_STATUS_L1C_INIT_DONE |
			MLX5_RXP_RTRU_CSR_STATUS_L2C_INIT_DONE;
	}
	expected_mask = expected_value;
	ret = rxp_poll_csr_for_value(ctx, &poll_value,
				     MLX5_RXP_RTRU_CSR_STATUS,
				     expected_value, expected_mask,
				     MLX5_RXP_CSR_STATUS_TRIAL_TIMEOUT, id);
	if (ret)
		return ret;
	DRV_LOG(DEBUG, "rule memory initialise: 0x%08X", poll_value);
	/* Clear the init bit in the rtru ctrl CSR */
	ctrl_value &= ~(MLX5_RXP_RTRU_CSR_CTRL_INIT);
	mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
				       ctrl_value);
	return 0;
}

static int
rxp_parse_rof(const char *buf, uint32_t len,
	      struct mlx5_rxp_ctl_rules_pgm **rules)
{
	static const char del[] = "\n\r";
	char *line;
	char *tmp;
	char *cur_pos;
	uint32_t lines = 0;
	uint32_t entries;
	struct mlx5_rxp_rof_entry *curentry;

	tmp = rte_malloc("", len, 0);
	if (!tmp)
		return -ENOMEM;
	memcpy(tmp, buf, len);
	line = strtok(tmp, del);
	while (line) {
		if (line[0] != '#' && line[0] != '\0')
			lines++;
		line = strtok(NULL, del);
	}
	*rules = rte_malloc("", lines * sizeof(*curentry) + sizeof(**rules), 0);
	if (!(*rules)) {
		rte_free(tmp);
		return -ENOMEM;
	}
	memset(*rules, 0, lines * sizeof(curentry) + sizeof(**rules));
	curentry = (*rules)->rules;
	(*rules)->hdr.cmd = MLX5_RXP_CTL_RULES_PGM;
	entries = 0;
	memcpy(tmp, buf, len);
	line = strtok(tmp, del);
	while (line) {
		if (line[0] == '#' || line[0] == '\0') {
			line = strtok(NULL, del);
			continue;
		}
		curentry->type = strtoul(line, &cur_pos, 10);
		if (cur_pos == line || cur_pos[0] != ',')
			goto parse_error;
		cur_pos++;
		curentry->addr = strtoul(cur_pos, &cur_pos, 16);
		if (cur_pos[0] != ',')
			goto parse_error;
		cur_pos++;
		curentry->value = strtoull(cur_pos, &cur_pos, 16);
		if (cur_pos[0] != '\0' && cur_pos[0] != '\n')
			goto parse_error;
		curentry++;
		entries++;
		if (entries > lines)
			goto parse_error;
		line = strtok(NULL, del);
	}
	(*rules)->count = entries;
	(*rules)->hdr.len = entries * sizeof(*curentry) + sizeof(**rules);
	rte_free(tmp);
	return 0;
parse_error:
	rte_free(tmp);
	if (*rules)
		rte_free(*rules);
	return -EINVAL;
}

static int
mlnx_set_database(struct mlx5_regex_priv *priv, uint8_t id, uint8_t db_to_use)
{
	int ret;
	uint32_t umem_id;

	ret = mlx5_devx_regex_database_stop(priv->ctx, id);
	if (ret < 0) {
		DRV_LOG(ERR, "stop engine failed!");
		return ret;
	}
	umem_id = mlx5_os_get_umem_id(priv->db[db_to_use].umem.umem);
	ret = mlx5_devx_regex_database_program(priv->ctx, id, umem_id, 0);
	if (ret < 0) {
		DRV_LOG(ERR, "program db failed!");
		return ret;
	}
	return 0;
}

static int
mlnx_resume_database(struct mlx5_regex_priv *priv, uint8_t id)
{
	mlx5_devx_regex_database_resume(priv->ctx, id);
	return 0;
}

/*
 * Assign db memory for RXP programming.
 */
static int
mlnx_update_database(struct mlx5_regex_priv *priv, uint8_t id)
{
	unsigned int i;
	uint8_t db_free = MLX5_RXP_DB_NOT_ASSIGNED;
	uint8_t eng_assigned = MLX5_RXP_DB_NOT_ASSIGNED;

	/* Check which database rxp_eng is currently located if any? */
	for (i = 0; i < (priv->nb_engines + MLX5_RXP_EM_COUNT);
	     i++) {
		if (priv->db[i].db_assigned_to_eng_num == id) {
			eng_assigned = i;
			break;
		}
	}
	/*
	 * If private mode then, we can keep the same db ptr as RXP will be
	 * programming EM itself if necessary, however need to see if
	 * programmed yet.
	 */
	if ((priv->prog_mode == MLX5_RXP_PRIVATE_PROG_MODE) &&
	    (eng_assigned != MLX5_RXP_DB_NOT_ASSIGNED))
		return eng_assigned;
	/* Check for inactive db memory to use. */
	for (i = 0; i < (priv->nb_engines + MLX5_RXP_EM_COUNT);
	     i++) {
		if (priv->db[i].active == true)
			continue; /* Already in use, so skip db. */
		/* Set this db to active now as free to use. */
		priv->db[i].active = true;
		/* Now unassign last db index in use by RXP Eng. */
		if (eng_assigned != MLX5_RXP_DB_NOT_ASSIGNED) {
			priv->db[eng_assigned].active = false;
			priv->db[eng_assigned].db_assigned_to_eng_num =
				MLX5_RXP_DB_NOT_ASSIGNED;

			/* Set all DB memory to 0's before setting up DB. */
			memset(priv->db[i].ptr, 0x00, MLX5_MAX_DB_SIZE);
		}
		/* Now reassign new db index with RXP Engine. */
		priv->db[i].db_assigned_to_eng_num = id;
		db_free = i;
		break;
	}
	if (db_free == MLX5_RXP_DB_NOT_ASSIGNED)
		return -1;
	return db_free;
}

/*
 * Program RXP instruction db to RXP engine/s.
 */
static int
program_rxp_rules(struct mlx5_regex_priv *priv,
		  struct mlx5_rxp_ctl_rules_pgm *rules, uint8_t id)
{
	int ret, db_free;
	uint32_t rule_cnt;

	rule_cnt = rules->count;
	db_free = mlnx_update_database(priv, id);
	if (db_free < 0) {
		DRV_LOG(ERR, "Failed to setup db memory!");
		return db_free;
	}
	if (priv->prog_mode == MLX5_RXP_PRIVATE_PROG_MODE) {
		/* Register early to ensure RXP writes to EM use valid addr. */
		ret = mlnx_set_database(priv, id, db_free);
		if (ret < 0) {
			DRV_LOG(ERR, "Failed to register db memory!");
			return ret;
		}
	}
	ret = write_private_rules(priv, rules, id);
	if (ret < 0) {
		DRV_LOG(ERR, "Failed to write rules!");
		return ret;
	}
	if (priv->prog_mode == MLX5_RXP_SHARED_PROG_MODE) {
		/* Write external rules directly to EM. */
		rules->count = rule_cnt;
	       /* Now write external instructions to EM. */
		ret = write_shared_rules(priv, rules, rules->hdr.len, db_free);
		if (ret < 0) {
			DRV_LOG(ERR, "Failed to write EM rules!");
			return ret;
		}
		ret = mlnx_set_database(priv, id, db_free);
		if (ret < 0) {
			DRV_LOG(ERR, "Failed to register db memory!");
			return ret;
		}
	}
	ret = mlnx_resume_database(priv, id);
	if (ret < 0) {
		DRV_LOG(ERR, "Failed to resume engine!");
		return ret;
	}
	DRV_LOG(DEBUG, "Programmed RXP Engine %d\n", id);
	rules->count = rule_cnt;
	return 0;
}

static int
rxp_init_eng(struct mlx5_regex_priv *priv, uint8_t id)
{
	uint32_t ctrl;
	uint32_t reg;
	struct ibv_context *ctx = priv->ctx;
	int ret;

	ret = mlx5_devx_regex_register_read(ctx, id, MLX5_RXP_CSR_CTRL, &ctrl);
	if (ret)
		return ret;
	if (ctrl & MLX5_RXP_CSR_CTRL_INIT) {
		ctrl &= ~MLX5_RXP_CSR_CTRL_INIT;
		ret = mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_CSR_CTRL,
						     ctrl);
		if (ret)
			return ret;
	}
	ctrl |= MLX5_RXP_CSR_CTRL_INIT;
	ret = mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_CSR_CTRL, ctrl);
	if (ret)
		return ret;
	ctrl &= ~MLX5_RXP_CSR_CTRL_INIT;
	ret = mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_CSR_CTRL, ctrl);
	rte_delay_us(20000);
	ret = rxp_poll_csr_for_value(ctx, &ctrl, MLX5_RXP_CSR_STATUS,
				     MLX5_RXP_CSR_STATUS_INIT_DONE,
				     MLX5_RXP_CSR_STATUS_INIT_DONE,
				     MLX5_RXP_CSR_STATUS_TRIAL_TIMEOUT, id);
	if (ret)
		return ret;
	ret = mlx5_devx_regex_register_read(ctx, id, MLX5_RXP_CSR_CTRL, &ctrl);
	if (ret)
		return ret;
	ctrl &= ~MLX5_RXP_CSR_CTRL_INIT;
	ret = mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_CSR_CTRL,
					     ctrl);
	if (ret)
		return ret;
	ret = rxp_init_rtru(ctx, id, MLX5_RXP_RTRU_CSR_CTRL_INIT_MODE_IM_L1_L2);
	if (ret)
		return ret;
	ret = mlx5_devx_regex_register_read(ctx, id, MLX5_RXP_CSR_CAPABILITY_5,
					    &reg);
	if (ret)
		return ret;
	DRV_LOG(DEBUG, "max matches: %d, DDOS threshold: %d", reg >> 16,
		reg & 0xffff);
	if ((reg >> 16) >= priv->nb_max_matches)
		ret = mlx5_devx_regex_register_write(ctx, id,
						     MLX5_RXP_CSR_MAX_MATCH,
						     priv->nb_max_matches);
	else
		ret = mlx5_devx_regex_register_write(ctx, id,
						     MLX5_RXP_CSR_MAX_MATCH,
						     (reg >> 16));
	ret |= mlx5_devx_regex_register_write(ctx, id, MLX5_RXP_CSR_MAX_PREFIX,
					 (reg & 0xFFFF));
	ret |= mlx5_devx_regex_register_write(ctx, id,
					      MLX5_RXP_CSR_MAX_LATENCY, 0);
	ret |= mlx5_devx_regex_register_write(ctx, id,
					      MLX5_RXP_CSR_MAX_PRI_THREAD, 0);
	return ret;
}

static int
write_private_rules(struct mlx5_regex_priv *priv,
		    struct mlx5_rxp_ctl_rules_pgm *rules,
		    uint8_t id)
{
	unsigned int pending;
	uint32_t block, reg, val, rule_cnt, rule_offset, rtru_max_num_entries;
	int ret = 1;

	if (priv->prog_mode == MLX5_RXP_MODE_NOT_DEFINED)
		return -EINVAL;
	if (rules->hdr.len == 0 || rules->hdr.cmd < MLX5_RXP_CTL_RULES_PGM ||
				   rules->hdr.cmd > MLX5_RXP_CTL_RULES_PGM_INCR)
		return -EINVAL;
	/* For a non-incremental rules program, re-init the RXP. */
	if (rules->hdr.cmd == MLX5_RXP_CTL_RULES_PGM) {
		ret = rxp_init_eng(priv, id);
		if (ret < 0)
			return ret;
	} else if (rules->hdr.cmd == MLX5_RXP_CTL_RULES_PGM_INCR) {
		/* Flush RXP L1 and L2 cache by using MODE_L1_L2. */
		ret = rxp_init_rtru(priv->ctx, id,
				    MLX5_RXP_RTRU_CSR_CTRL_INIT_MODE_L1_L2);
		if (ret < 0)
			return ret;
	}
	if (rules->count == 0)
		return -EINVAL;
	/* Confirm the RXP is initialised. */
	if (mlx5_devx_regex_register_read(priv->ctx, id,
					    MLX5_RXP_CSR_STATUS, &val)) {
		DRV_LOG(ERR, "Failed to read from RXP!");
		return -ENODEV;
	}
	if (!(val & MLX5_RXP_CSR_STATUS_INIT_DONE)) {
		DRV_LOG(ERR, "RXP not initialised...");
		return -EBUSY;
	}
	/* Get the RTRU maximum number of entries allowed. */
	if (mlx5_devx_regex_register_read(priv->ctx, id,
			MLX5_RXP_RTRU_CSR_CAPABILITY, &rtru_max_num_entries)) {
		DRV_LOG(ERR, "Failed to read RTRU capability!");
		return -ENODEV;
	}
	rtru_max_num_entries = (rtru_max_num_entries & 0x00FF);
	rule_cnt = 0;
	pending = 0;
	while (rules->count > 0) {
		if ((rules->rules[rule_cnt].type == MLX5_RXP_ROF_ENTRY_INST) ||
		    (rules->rules[rule_cnt].type == MLX5_RXP_ROF_ENTRY_IM) ||
		    (rules->rules[rule_cnt].type == MLX5_RXP_ROF_ENTRY_EM)) {
			if ((rules->rules[rule_cnt].type ==
			     MLX5_RXP_ROF_ENTRY_EM) &&
			    (priv->prog_mode == MLX5_RXP_SHARED_PROG_MODE)) {
				/* Skip EM rules programming. */
				if (pending > 0) {
					/* Flush any rules that are pending. */
					rule_offset = (rule_cnt - pending);
					ret = rxp_flush_rules(priv->ctx,
						&rules->rules[rule_offset],
						pending, id);
					if (ret < 0) {
						DRV_LOG(ERR, "Flushing rules.");
						return -ENODEV;
					}
					pending = 0;
				}
				rule_cnt++;
			} else {
				pending++;
				rule_cnt++;
				/*
				 * If parsing the last rule, or if reached the
				 * maximum number of rules for this batch, then
				 * flush the rules batch to the RXP.
				 */
				if ((rules->count == 1) ||
				    (pending == rtru_max_num_entries)) {
					rule_offset = (rule_cnt - pending);
					ret = rxp_flush_rules(priv->ctx,
						&rules->rules[rule_offset],
						pending, id);
					if (ret < 0) {
						DRV_LOG(ERR, "Flushing rules.");
						return -ENODEV;
					}
					pending = 0;
				}
			}
		} else if ((rules->rules[rule_cnt].type ==
				MLX5_RXP_ROF_ENTRY_EQ) ||
			 (rules->rules[rule_cnt].type ==
				MLX5_RXP_ROF_ENTRY_GTE) ||
			 (rules->rules[rule_cnt].type ==
				MLX5_RXP_ROF_ENTRY_LTE) ||
			 (rules->rules[rule_cnt].type ==
				MLX5_RXP_ROF_ENTRY_CHECKSUM) ||
			 (rules->rules[rule_cnt].type ==
				MLX5_RXP_ROF_ENTRY_CHECKSUM_EX_EM)) {
			if (pending) {
				/* Flush rules before checking reg values. */
				rule_offset = (rule_cnt - pending);
				ret = rxp_flush_rules(priv->ctx,
					&rules->rules[rule_offset],
					pending, id);
				if (ret < 0) {
					DRV_LOG(ERR, "Failed to flush rules.");
					return -ENODEV;
				}
			}
			block = (rules->rules[rule_cnt].addr >> 16) & 0xFFFF;
			if (block == 0)
				reg = MLX5_RXP_CSR_BASE_ADDRESS;
			else if (block == 1)
				reg = MLX5_RXP_RTRU_CSR_BASE_ADDRESS;
			else {
				DRV_LOG(ERR, "Invalid ROF register 0x%08X!",
					rules->rules[rule_cnt].addr);
				return -EINVAL;
			}
			reg += (rules->rules[rule_cnt].addr & 0xFFFF) *
				MLX5_RXP_CSR_WIDTH;
			ret = mlx5_devx_regex_register_read(priv->ctx, id,
							    reg, &val);
			if (ret) {
				DRV_LOG(ERR, "RXP CSR read failed!");
				return ret;
			}
			if ((priv->prog_mode == MLX5_RXP_SHARED_PROG_MODE) &&
			    ((rules->rules[rule_cnt].type ==
			    MLX5_RXP_ROF_ENTRY_CHECKSUM_EX_EM) &&
			    (val != rules->rules[rule_cnt].value))) {
				DRV_LOG(ERR, "Unexpected value for register:");
				DRV_LOG(ERR, "reg %x" PRIu32 " got %x" PRIu32,
					rules->rules[rule_cnt].addr, val);
				DRV_LOG(ERR, "expected %" PRIx64 ".",
					rules->rules[rule_cnt].value);
					return -EINVAL;
			} else if ((priv->prog_mode ==
				 MLX5_RXP_PRIVATE_PROG_MODE) &&
				 (rules->rules[rule_cnt].type ==
				 MLX5_RXP_ROF_ENTRY_CHECKSUM) &&
				 (val != rules->rules[rule_cnt].value)) {
				DRV_LOG(ERR, "Unexpected value for register:");
				DRV_LOG(ERR, "reg %x" PRIu32 " got %x" PRIu32,
					rules->rules[rule_cnt].addr, val);
				DRV_LOG(ERR, "expected %" PRIx64 ".",
					rules->rules[rule_cnt].value);
				return -EINVAL;
			} else if ((rules->rules[rule_cnt].type ==
					MLX5_RXP_ROF_ENTRY_EQ) &&
				  (val != rules->rules[rule_cnt].value)) {
				DRV_LOG(ERR, "Unexpected value for register:");
				DRV_LOG(ERR, "reg %x" PRIu32 " got %x" PRIu32,
					rules->rules[rule_cnt].addr, val);
				DRV_LOG(ERR, "expected %" PRIx64 ".",
					rules->rules[rule_cnt].value);
					return -EINVAL;
			} else if ((rules->rules[rule_cnt].type ==
					MLX5_RXP_ROF_ENTRY_GTE) &&
				 (val < rules->rules[rule_cnt].value)) {
				DRV_LOG(ERR, "Unexpected value reg 0x%08X,",
					rules->rules[rule_cnt].addr);
				DRV_LOG(ERR, "got %X, expected >= %" PRIx64 ".",
					val, rules->rules[rule_cnt].value);
				return -EINVAL;
			} else if ((rules->rules[rule_cnt].type ==
					MLX5_RXP_ROF_ENTRY_LTE) &&
				 (val > rules->rules[rule_cnt].value)) {
				DRV_LOG(ERR, "Unexpected value reg 0x%08X,",
					rules->rules[rule_cnt].addr);
				DRV_LOG(ERR, "got %08X expected <= %" PRIx64,
					val, rules->rules[rule_cnt].value);
				return -EINVAL;
			}
			rule_cnt++;
			pending = 0;
		} else {
			DRV_LOG(ERR, "Error: Invalid rule type %d!",
				rules->rules[rule_cnt].type);
			return -EINVAL;
		}
		rules->count--;
	}
	return ret;
}

/*
 * Shared memory programming mode, here all external db instructions are written
 * to EM via the host.
 */
static int
write_shared_rules(struct mlx5_regex_priv *priv,
		   struct mlx5_rxp_ctl_rules_pgm *rules, uint32_t count,
		   uint8_t db_to_program)
{
	uint32_t rule_cnt, rof_rule_addr;
	uint64_t tmp_write_swap[4];

	if (priv->prog_mode == MLX5_RXP_MODE_NOT_DEFINED)
		return -EINVAL;
	if ((rules->count == 0) || (count == 0))
		return -EINVAL;
	rule_cnt = 0;
	/*
	 * Note the following section of code carries out a 32byte swap of
	 * instruction to coincide with HW 32byte swap. This may need removed
	 * in new variants of this programming function!
	 */
	while (rule_cnt < rules->count) {
		if ((rules->rules[rule_cnt].type == MLX5_RXP_ROF_ENTRY_EM) &&
		    (priv->prog_mode == MLX5_RXP_SHARED_PROG_MODE)) {
			/*
			 * Note there are always blocks of 8 instructions for
			 * 7's written sequentially. However there is no
			 * guarantee that all blocks are sequential!
			 */
			if (count >= (rule_cnt + MLX5_RXP_INST_BLOCK_SIZE)) {
				/*
				 * Ensure memory write not exceeding boundary
				 * Check essential to ensure 0x10000 offset
				 * accounted for!
				 */
				if ((uint8_t *)((uint8_t *)
				    priv->db[db_to_program].ptr +
				    ((rules->rules[rule_cnt + 7].addr <<
				    MLX5_RXP_INST_OFFSET))) >=
				    ((uint8_t *)((uint8_t *)
				    priv->db[db_to_program].ptr +
				    MLX5_MAX_DB_SIZE))) {
					DRV_LOG(ERR, "DB exceeded memory!");
					return -ENODEV;
				}
				/*
				 * Rule address Offset to align with RXP
				 * external instruction offset.
				 */
				rof_rule_addr = (rules->rules[rule_cnt].addr <<
						 MLX5_RXP_INST_OFFSET);
				/* 32 byte instruction swap (sw work around)! */
				tmp_write_swap[0] = le64toh(
					rules->rules[(rule_cnt + 4)].value);
				tmp_write_swap[1] = le64toh(
					rules->rules[(rule_cnt + 5)].value);
				tmp_write_swap[2] = le64toh(
					rules->rules[(rule_cnt + 6)].value);
				tmp_write_swap[3] = le64toh(
					rules->rules[(rule_cnt + 7)].value);
				/* Write only 4 of the 8 instructions. */
				memcpy((uint8_t *)((uint8_t *)
				       priv->db[db_to_program].ptr +
				       rof_rule_addr), &tmp_write_swap,
				       (sizeof(uint64_t) * 4));
				/* Write 1st 4 rules of block after last 4. */
				rof_rule_addr = (rules->rules[
						 (rule_cnt + 4)].addr <<
						 MLX5_RXP_INST_OFFSET);
				tmp_write_swap[0] = le64toh(
					rules->rules[(rule_cnt + 0)].value);
				tmp_write_swap[1] = le64toh(
					rules->rules[(rule_cnt + 1)].value);
				tmp_write_swap[2] = le64toh(
					rules->rules[(rule_cnt + 2)].value);
				tmp_write_swap[3] = le64toh(
					rules->rules[(rule_cnt + 3)].value);
				memcpy((uint8_t *)((uint8_t *)
				       priv->db[db_to_program].ptr +
				       rof_rule_addr), &tmp_write_swap,
				       (sizeof(uint64_t) * 4));
			} else
				return -1;
			/* Fast forward as already handled block of 8. */
			rule_cnt += MLX5_RXP_INST_BLOCK_SIZE;
		} else
			rule_cnt++; /* Must be something other than EM rule. */
	}
	return 0;
}

static int
rxp_db_setup(struct mlx5_regex_priv *priv)
{
	int ret;
	uint8_t i;

	/* Setup database memories for both RXP engines + reprogram memory. */
	for (i = 0; i < (priv->nb_engines + MLX5_RXP_EM_COUNT); i++) {
		priv->db[i].ptr = rte_malloc("", MLX5_MAX_DB_SIZE, 0);
		if (!priv->db[i].ptr) {
			DRV_LOG(ERR, "Failed to alloc db memory!");
			ret = ENODEV;
			goto tidyup_error;
		}
		/* Register the memory. */
		priv->db[i].umem.umem = mlx5_glue->devx_umem_reg(priv->ctx,
							priv->db[i].ptr,
							MLX5_MAX_DB_SIZE, 7);
		if (!priv->db[i].umem.umem) {
			DRV_LOG(ERR, "Failed to register memory!");
			ret = ENODEV;
			goto tidyup_error;
		}
		/* Ensure set all DB memory to 0's before setting up DB. */
		memset(priv->db[i].ptr, 0x00, MLX5_MAX_DB_SIZE);
		/* No data currently in database. */
		priv->db[i].len = 0;
		priv->db[i].active = false;
		priv->db[i].db_assigned_to_eng_num = MLX5_RXP_DB_NOT_ASSIGNED;
	}
	return 0;
tidyup_error:
	for (i = 0; i < (priv->nb_engines + MLX5_RXP_EM_COUNT); i++) {
		if (priv->db[i].ptr)
			rte_free(priv->db[i].ptr);
		if (priv->db[i].umem.umem)
			mlx5_glue->devx_umem_dereg(priv->db[i].umem.umem);
	}
	return -ret;
}

int
mlx5_regex_rules_db_import(struct rte_regexdev *dev,
		     const char *rule_db, uint32_t rule_db_len)
{
	struct mlx5_regex_priv *priv = dev->data->dev_private;
	struct mlx5_rxp_ctl_rules_pgm *rules = NULL;
	uint8_t id;
	int ret;

	if (priv->prog_mode == MLX5_RXP_MODE_NOT_DEFINED) {
		DRV_LOG(ERR, "RXP programming mode not set!");
		return -1;
	}
	if (rule_db == NULL) {
		DRV_LOG(ERR, "Database empty!");
		return -ENODEV;
	}
	if (rule_db_len == 0)
		return -EINVAL;
	ret = rxp_parse_rof(rule_db, rule_db_len, &rules);
	if (ret) {
		DRV_LOG(ERR, "Can't parse ROF file.");
		return ret;
	}
	/* Need to ensure RXP not busy before stop! */
	for (id = 0; id < priv->nb_engines; id++) {
		ret = rxp_stop_engine(priv->ctx, id);
		if (ret) {
			DRV_LOG(ERR, "Can't stop engine.");
			ret = -ENODEV;
			goto tidyup_error;
		}
		ret = program_rxp_rules(priv, rules, id);
		if (ret < 0) {
			DRV_LOG(ERR, "Failed to program rxp rules.");
			ret = -ENODEV;
			goto tidyup_error;
		}
		ret = rxp_start_engine(priv->ctx, id);
		if (ret) {
			DRV_LOG(ERR, "Can't start engine.");
			ret = -ENODEV;
			goto tidyup_error;
		}
	}
	rte_free(rules);
	return 0;
tidyup_error:
	rte_free(rules);
	return ret;
}

int
mlx5_regex_configure(struct rte_regexdev *dev,
		     const struct rte_regexdev_config *cfg)
{
	struct mlx5_regex_priv *priv = dev->data->dev_private;
	int ret;

	if (priv->prog_mode == MLX5_RXP_MODE_NOT_DEFINED)
		return -1;
	priv->nb_queues = cfg->nb_queue_pairs;
	dev->data->dev_conf.nb_queue_pairs = priv->nb_queues;
	priv->qps = rte_zmalloc(NULL, sizeof(struct mlx5_regex_qp) *
				priv->nb_queues, 0);
	if (!priv->nb_queues) {
		DRV_LOG(ERR, "can't allocate qps memory");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	priv->nb_max_matches = cfg->nb_max_matches;
	/* Setup rxp db memories. */
	if (rxp_db_setup(priv)) {
		DRV_LOG(ERR, "Failed to setup RXP db memory");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	if (cfg->rule_db != NULL) {
		ret = mlx5_regex_rules_db_import(dev, cfg->rule_db,
						 cfg->rule_db_len);
		if (ret < 0) {
			DRV_LOG(ERR, "Failed to program rxp rules.");
			rte_errno = ENODEV;
			goto configure_error;
		}
	} else
		DRV_LOG(DEBUG, "Regex config without rules programming!");
	return 0;
configure_error:
	if (priv->qps)
		rte_free(priv->qps);
	return -rte_errno;
}
