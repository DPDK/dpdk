/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <rte_log.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_regexdev.h>
#include <rte_regexdev_core.h>
#include <rte_regexdev_driver.h>
#include <sys/mman.h>

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

#define MLX5_REGEX_RXP_ROF2_LINE_LEN 34

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
program_rxp_rules(struct mlx5_regex_priv *priv, const char *buf, uint32_t len,
		  uint8_t id);
static int
rxp_init_eng(struct mlx5_regex_priv *priv, uint8_t id);
static int
rxp_db_setup(struct mlx5_regex_priv *priv);
static void
rxp_dump_csrs(struct ibv_context *ctx, uint8_t id);
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
	info->regexdev_capa = RTE_REGEXDEV_SUPP_PCRE_GREEDY_F |
			      RTE_REGEXDEV_CAPA_QUEUE_PAIR_OOS_F;
	info->rule_flags = 0;
	info->max_queue_pairs = UINT16_MAX;
	return 0;
}

static int
rxp_poll_csr_for_value(struct ibv_context *ctx, uint32_t *value,
		       uint32_t address, uint32_t expected_value,
		       uint32_t expected_mask, uint32_t timeout_ms, uint8_t id)
{
	unsigned int i;
	int ret;

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
rxp_init_rtru(struct mlx5_regex_priv *priv, uint8_t id, uint32_t init_bits)
{
	uint32_t ctrl_value;
	uint32_t poll_value;
	uint32_t expected_value;
	uint32_t expected_mask;
	struct ibv_context *ctx = priv->ctx;
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
		expected_value = MLX5_RXP_RTRU_CSR_STATUS_L1C_INIT_DONE;
	} else {
		expected_value = MLX5_RXP_RTRU_CSR_STATUS_IM_INIT_DONE |
			MLX5_RXP_RTRU_CSR_STATUS_L1C_INIT_DONE;
	}
	if (priv->is_bf2)
		expected_value |= MLX5_RXP_RTRU_CSR_STATUS_L2C_INIT_DONE;


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
rxp_parse_line(char *line, uint32_t *type, uint32_t *address, uint64_t *value)
{
	char *cur_pos;

	if (*line == '\0' || *line == '#')
		return  1;
	*type = strtoul(line, &cur_pos, 10);
	if (*cur_pos != ',' && *cur_pos != '\0')
		return -1;
	*address = strtoul(cur_pos+1, &cur_pos, 16);
	if (*cur_pos != ',' && *cur_pos != '\0')
		return -1;
	*value = strtoul(cur_pos+1, &cur_pos, 16);
	if (*cur_pos != ',' && *cur_pos != '\0')
		return -1;
	return 0;
}

static uint32_t
rxp_get_reg_address(uint32_t address)
{
	uint32_t block;
	uint32_t reg;

	block = (address >> 16) & 0xFFFF;
	if (block == 0)
		reg = MLX5_RXP_CSR_BASE_ADDRESS;
	else if (block == 1)
		reg = MLX5_RXP_RTRU_CSR_BASE_ADDRESS;
	else {
		DRV_LOG(ERR, "Invalid ROF register 0x%08X!", address);
			return UINT32_MAX;
	}
	reg += (address & 0xFFFF) * MLX5_RXP_CSR_WIDTH;
	return reg;
}

#define MLX5_RXP_NUM_LINES_PER_BLOCK 8

static int
rxp_program_rof(struct mlx5_regex_priv *priv, const char *buf, uint32_t len,
		uint8_t id)
{
	static const char del[] = "\n\r";
	char *line;
	char *tmp;
	uint32_t type = 0;
	uint32_t address;
	uint64_t val;
	uint32_t reg_val;
	int ret;
	int skip = -1;
	int last = 0;
	uint32_t temp;
	uint32_t tmp_addr;
	uint32_t rof_rule_addr;
	uint64_t tmp_write_swap[4];
	struct mlx5_rxp_rof_entry rules[8];
	int i;
	int db_free;
	int j;

	tmp = rte_malloc("", len, 0);
	if (!tmp)
		return -ENOMEM;
	memcpy(tmp, buf, len);
	db_free = mlnx_update_database(priv, id);
	if (db_free < 0) {
		DRV_LOG(ERR, "Failed to setup db memory!");
		rte_free(tmp);
		return db_free;
	}
	for (line = strtok(tmp, del), j = 0; line; line = strtok(NULL, del),
	     j++, last = type) {
		ret = rxp_parse_line(line, &type, &address, &val);
		if (ret != 0) {
			if (ret < 0)
				goto parse_error;
			continue;
		}
		switch (type) {
		case MLX5_RXP_ROF_ENTRY_EQ:
			if (skip == 0 && address == 0)
				skip = 1;
			tmp_addr = rxp_get_reg_address(address);
			if (tmp_addr == UINT32_MAX)
				goto parse_error;
			ret = mlx5_devx_regex_register_read(priv->ctx, id,
							    tmp_addr, &reg_val);
			if (ret)
				goto parse_error;
			if (skip == -1 && address == 0) {
				if (val == reg_val) {
					skip = 0;
					continue;
				}
			} else if (skip == 0) {
				if (val != reg_val) {
					DRV_LOG(ERR,
						"got %08X expected == %" PRIx64,
						reg_val, val);
					goto parse_error;
				}
			}
			break;
		case MLX5_RXP_ROF_ENTRY_GTE:
			if (skip == 0 && address == 0)
				skip = 1;
			tmp_addr = rxp_get_reg_address(address);
			if (tmp_addr == UINT32_MAX)
				goto parse_error;
			ret = mlx5_devx_regex_register_read(priv->ctx, id,
							    tmp_addr, &reg_val);
			if (ret)
				goto parse_error;
			if (skip == -1 && address == 0) {
				if (reg_val >= val) {
					skip = 0;
					continue;
				}
			} else if (skip == 0) {
				if (reg_val < val) {
					DRV_LOG(ERR,
						"got %08X expected >= %" PRIx64,
						reg_val, val);
					goto parse_error;
				}
			}
			break;
		case MLX5_RXP_ROF_ENTRY_LTE:
			tmp_addr = rxp_get_reg_address(address);
			if (tmp_addr == UINT32_MAX)
				goto parse_error;
			ret = mlx5_devx_regex_register_read(priv->ctx, id,
							    tmp_addr, &reg_val);
			if (ret)
				goto parse_error;
			if (skip == 0 && address == 0 &&
			    last != MLX5_RXP_ROF_ENTRY_GTE) {
				skip = 1;
			} else if (skip == 0 && address == 0 &&
				   last == MLX5_RXP_ROF_ENTRY_GTE) {
				if (reg_val > val)
					skip = -1;
				continue;
			}
			if (skip == -1 && address == 0) {
				if (reg_val <= val) {
					skip = 0;
					continue;
				}
			} else if (skip == 0) {
				if (reg_val > val) {
					DRV_LOG(ERR,
						"got %08X expected <= %" PRIx64,
						reg_val, val);
					goto parse_error;
				}
			}
			break;
		case MLX5_RXP_ROF_ENTRY_CHECKSUM:
			break;
		case MLX5_RXP_ROF_ENTRY_CHECKSUM_EX_EM:
			if (skip)
				continue;
			tmp_addr = rxp_get_reg_address(address);
			if (tmp_addr == UINT32_MAX)
				goto parse_error;

			ret = mlx5_devx_regex_register_read(priv->ctx, id,
							    tmp_addr, &reg_val);
			if (ret) {
				DRV_LOG(ERR, "RXP CSR read failed!");
				return ret;
			}
			if (reg_val != val) {
				DRV_LOG(ERR, "got %08X expected <= %" PRIx64,
					reg_val, val);
				goto parse_error;
			}
			break;
		case MLX5_RXP_ROF_ENTRY_IM:
			if (skip)
				continue;
			/*
			 * NOTE: All rules written to RXP must be carried out in
			 * triplets of: 2xData + 1xAddr.
			 * No optimisation is currently allowed in this
			 * sequence to perform less writes.
			 */
			temp = val;
			ret |= mlx5_devx_regex_register_write
					(priv->ctx, id,
					 MLX5_RXP_RTRU_CSR_DATA_0, temp);
			temp = (uint32_t)(val >> 32);
			ret |= mlx5_devx_regex_register_write
					(priv->ctx, id,
					 MLX5_RXP_RTRU_CSR_DATA_0 +
					 MLX5_RXP_CSR_WIDTH, temp);
			temp = address;
			ret |= mlx5_devx_regex_register_write
					(priv->ctx, id, MLX5_RXP_RTRU_CSR_ADDR,
					 temp);
			if (ret) {
				DRV_LOG(ERR,
					"Failed to copy instructions to RXP.");
				goto parse_error;
			}
			break;
		case MLX5_RXP_ROF_ENTRY_EM:
			if (skip)
				continue;
			for (i = 0; i < MLX5_RXP_NUM_LINES_PER_BLOCK; i++) {
				ret = rxp_parse_line(line, &type,
						     &rules[i].addr,
						     &rules[i].value);
				if (ret != 0)
					goto parse_error;
				if (i < (MLX5_RXP_NUM_LINES_PER_BLOCK - 1)) {
					line = strtok(NULL, del);
					if (!line)
						goto parse_error;
				}
			}
			if ((uint8_t *)((uint8_t *)
					priv->db[id].ptr +
					((rules[7].addr <<
					 MLX5_RXP_INST_OFFSET))) >=
					((uint8_t *)((uint8_t *)
					priv->db[id].ptr + MLX5_MAX_DB_SIZE))) {
				DRV_LOG(ERR, "DB exceeded memory!");
				goto parse_error;
			}
			/*
			 * Rule address Offset to align with RXP
			 * external instruction offset.
			 */
			rof_rule_addr = (rules[0].addr << MLX5_RXP_INST_OFFSET);
			/* 32 byte instruction swap (sw work around)! */
			tmp_write_swap[0] = le64toh(rules[4].value);
			tmp_write_swap[1] = le64toh(rules[5].value);
			tmp_write_swap[2] = le64toh(rules[6].value);
			tmp_write_swap[3] = le64toh(rules[7].value);
			/* Write only 4 of the 8 instructions. */
			memcpy((uint8_t *)((uint8_t *)
				priv->db[id].ptr + rof_rule_addr),
				&tmp_write_swap, (sizeof(uint64_t) * 4));
			/* Write 1st 4 rules of block after last 4. */
			rof_rule_addr = (rules[4].addr << MLX5_RXP_INST_OFFSET);
			tmp_write_swap[0] = le64toh(rules[0].value);
			tmp_write_swap[1] = le64toh(rules[1].value);
			tmp_write_swap[2] = le64toh(rules[2].value);
			tmp_write_swap[3] = le64toh(rules[3].value);
			memcpy((uint8_t *)((uint8_t *)
				priv->db[id].ptr + rof_rule_addr),
				&tmp_write_swap, (sizeof(uint64_t) * 4));
			break;
		default:
			break;
		}

	}
	ret = mlnx_set_database(priv, id, db_free);
	if (ret < 0) {
		DRV_LOG(ERR, "Failed to register db memory!");
		goto parse_error;
	}
	rte_free(tmp);
	return 0;
parse_error:
	rte_free(tmp);
	return ret;
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
program_rxp_rules(struct mlx5_regex_priv *priv, const char *buf, uint32_t len,
		  uint8_t id)
{
	int ret;
	uint32_t val;

	ret = rxp_init_eng(priv, id);
	if (ret < 0)
		return ret;
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
	ret = mlx5_devx_regex_register_read(priv->ctx, id,
					    MLX5_RXP_RTRU_CSR_CTRL, &val);
	if (ret) {
		DRV_LOG(ERR, "CSR read failed!");
		return -1;
	}
	val |= MLX5_RXP_RTRU_CSR_CTRL_GO;
	ret = mlx5_devx_regex_register_write(priv->ctx, id,
					     MLX5_RXP_RTRU_CSR_CTRL, val);
	if (ret) {
		DRV_LOG(ERR, "Can't program rof file!");
		return -1;
	}
	ret = rxp_program_rof(priv, buf, len, id);
	if (ret) {
		DRV_LOG(ERR, "Can't program rof file!");
		return -1;
	}
	if (priv->is_bf2) {
		ret = rxp_poll_csr_for_value
			(priv->ctx, &val, MLX5_RXP_RTRU_CSR_STATUS,
			 MLX5_RXP_RTRU_CSR_STATUS_UPDATE_DONE,
			 MLX5_RXP_RTRU_CSR_STATUS_UPDATE_DONE,
			 MLX5_RXP_POLL_CSR_FOR_VALUE_TIMEOUT, id);
		if (ret < 0) {
			DRV_LOG(ERR, "Rules update timeout: 0x%08X", val);
			return ret;
		}
		DRV_LOG(DEBUG, "Rules update took %d cycles", ret);
	}
	if (mlx5_devx_regex_register_read(priv->ctx, id, MLX5_RXP_RTRU_CSR_CTRL,
					  &val)) {
		DRV_LOG(ERR, "CSR read failed!");
		return -1;
	}
	val &= ~(MLX5_RXP_RTRU_CSR_CTRL_GO);
	if (mlx5_devx_regex_register_write(priv->ctx, id,
					   MLX5_RXP_RTRU_CSR_CTRL, val)) {
		DRV_LOG(ERR, "CSR write failed!");
		return -1;
	}
	ret = mlx5_devx_regex_register_read(priv->ctx, id, MLX5_RXP_CSR_CTRL,
					    &val);
	if (ret)
		return ret;
	val &= ~MLX5_RXP_CSR_CTRL_INIT;
	ret = mlx5_devx_regex_register_write(priv->ctx, id, MLX5_RXP_CSR_CTRL,
					     val);
	if (ret)
		return ret;
	rxp_init_rtru(priv, id, MLX5_RXP_RTRU_CSR_CTRL_INIT_MODE_L1_L2);
	if (priv->is_bf2) {
		ret = rxp_poll_csr_for_value(priv->ctx, &val,
					     MLX5_RXP_CSR_STATUS,
					     MLX5_RXP_CSR_STATUS_INIT_DONE,
					     MLX5_RXP_CSR_STATUS_INIT_DONE,
					     MLX5_RXP_CSR_STATUS_TRIAL_TIMEOUT,
					     id);
		if (ret) {
			DRV_LOG(ERR, "Device init failed!");
			return ret;
		}
	}
	ret = mlnx_resume_database(priv, id);
	if (ret < 0) {
		DRV_LOG(ERR, "Failed to resume engine!");
		return ret;
	}

	return ret;

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
	if (ret)
		return ret;
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
	ret = rxp_init_rtru(priv, id,
			    MLX5_RXP_RTRU_CSR_CTRL_INIT_MODE_IM_L1_L2);
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
rxp_db_setup(struct mlx5_regex_priv *priv)
{
	int ret;
	uint8_t i;

	/* Setup database memories for both RXP engines + reprogram memory. */
	for (i = 0; i < (priv->nb_engines + MLX5_RXP_EM_COUNT); i++) {
		priv->db[i].ptr = rte_malloc("", MLX5_MAX_DB_SIZE, 1 << 21);
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
	uint32_t id;
	int ret;
	uint32_t ver;

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
	if (mlx5_devx_regex_register_read(priv->ctx, 0,
					  MLX5_RXP_CSR_BASE_ADDRESS, &ver)) {
		DRV_LOG(ERR, "Failed to read Main CSRs Engine 0!");
		return -1;
	}
	/* Need to ensure RXP not busy before stop! */
	for (id = 0; id < priv->nb_engines; id++) {
		ret = rxp_stop_engine(priv->ctx, id);
		if (ret) {
			DRV_LOG(ERR, "Can't stop engine.");
			ret = -ENODEV;
			goto tidyup_error;
		}
		ret = program_rxp_rules(priv, rule_db, rule_db_len, id);
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
