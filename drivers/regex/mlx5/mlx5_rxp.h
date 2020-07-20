/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_REGEX_RXP_H_
#define RTE_PMD_MLX5_REGEX_RXP_H_

#define MLX5_RXP_MAX_JOB_LENGTH	16384
#define MLX5_RXP_MAX_SUBSETS 4095
#define MLX5_RXP_CSR_NUM_ENTRIES 31

#define MLX5_RXP_CTRL_TYPE_MASK	7
#define MLX5_RXP_CTRL_TYPE_JOB_DESCRIPTOR 0
#define MLX5_RXP_CTRL_TYPE_RESPONSE_DESCRIPTOR 1
#define MLX5_RXP_CTRL_TYPE_MEMORY_WRITE	4
#define MLX5_RXP_CSR_CTRL_DISABLE_L2C (1 << 7)

#define MLX5_RXP_CTRL_JOB_DESC_SOF 0x0010
#define MLX5_RXP_CTRL_JOB_DESC_EOF 0x0020
#define MLX5_RXP_CTRL_JOB_DESC_HPM_ENABLE 0x0100
#define MLX5_RXP_CTRL_JOB_DESC_ANYMATCH_ENABLE 0x0200
#define MLX5_RXP_CTRL_JOB_DESC_FLAGS (MLX5_RXP_CTRL_JOB_DESC_SOF | \
				      MLX5_RXP_CTRL_JOB_DESC_EOF | \
				      MLX5_RXP_CTRL_JOB_DESC_HPM_ENABLE | \
				      MLX5_RXP_CTRL_JOB_DESC_ANYMATCH_ENABLE)

#define MLX5_RXP_CTRL_VALID 0x8000

#define MLX5_RXP_RESP_STATUS_MAX_PRI_THREADS (1 << 3)
#define MLX5_RXP_RESP_STATUS_MAX_SEC_THREADS (1 << 4)
#define MLX5_RXP_RESP_STATUS_MAX_LATENCY (1 << 5)
#define MLX5_RXP_RESP_STATUS_MAX_MATCH (1 << 6)
#define MLX5_RXP_RESP_STATUS_MAX_PREFIX	(1 << 7)
#define MLX5_RXP_RESP_STATUS_HPM (1 << 8)
#define MLX5_RXP_RESP_STATUS_ANYMATCH (1 << 9)
#define MLX5_RXP_RESP_STATUS_PMI_SOJ (1 << 13)
#define MLX5_RXP_RESP_STATUS_PMI_EOJ (1 << 14)

/* This describes the header the RXP expects for any search data. */
struct mlx5_rxp_job_desc {
	uint32_t job_id;
	uint16_t ctrl;
	uint16_t len;
	uint16_t subset[4];
} __rte_packed;

struct mlx5_rxp_response_desc {
	uint32_t job_id;
	uint16_t status;
	uint8_t	detected_match_count;
	uint8_t	match_count;
	uint16_t primary_thread_count;
	uint16_t instruction_count;
	uint16_t latency_count;
	uint16_t pmi_min_byte_ptr;
} __rte_packed;

struct mlx5_rxp_match_tuple {
	uint32_t rule_id;
	uint16_t start_ptr;
	uint16_t length;
} __rte_packed;

struct mlx5_rxp_response {
	struct mlx5_rxp_response_desc header;
	struct mlx5_rxp_match_tuple matches[0];
};

#define MLX5_RXP_MAX_MATCHES 254

#define MLX5_RXP_CTL_RULES_PGM 1
#define MLX5_RXP_CTL_RULES_PGM_INCR 2

#define MLX5_RXP_ROF_ENTRY_INST 0
#define MLX5_RXP_ROF_ENTRY_EQ 1
#define MLX5_RXP_ROF_ENTRY_GTE 2
#define MLX5_RXP_ROF_ENTRY_LTE 3
#define MLX5_RXP_ROF_ENTRY_CHECKSUM 4
#define MLX5_RXP_ROF_ENTRY_CHECKSUM_EX_EM 5
#define MLX5_RXP_ROF_ENTRY_IM 6
#define MLX5_RXP_ROF_ENTRY_EM 7
#define MLX5_RXP_ROF_ENTRY_TYPE_MAX 7

#define MLX5_RXP_INST_OFFSET 3
#define	MLX5_RXP_INST_BLOCK_SIZE 8
#define MLX5_MAX_SIZE_RES_DES (sizeof(struct mlx5_rxp_response_desc))
#define MLX5_MAX_DB_SIZE (1u << 27u)
#define MLX5_MAX_SIZE_MATCH_RESP (254 * sizeof(struct mlx5_rxp_match_tuple))
#define MLX5_RXP_SQ_NOT_BUSY false
#define MLX5_RXP_SQ_BUSY true


struct mlx5_rxp_ctl_hdr {
	uint16_t cmd;
	uint32_t len;
};

struct mlx5_rxp_rof_entry {
	uint8_t	type;
	uint32_t addr;
	uint64_t value;
};

struct mlx5_rxp_rof {
	uint32_t rof_version;
	char *timestamp;
	char *rxp_compiler_version;
	uint32_t rof_revision;
	uint32_t number_of_entries;
	struct mlx5_rxp_rof_entry *rof_entries;
};

struct mlx5_rxp_ctl_rules_pgm {
	struct mlx5_rxp_ctl_hdr hdr;
	uint32_t count;
	struct mlx5_rxp_rof_entry rules[0];
} __rte_packed;

/* RXP programming mode setting. */
enum mlx5_rxp_program_mode {
	MLX5_RXP_MODE_NOT_DEFINED = 0,
	MLX5_RXP_SHARED_PROG_MODE,
	MLX5_RXP_PRIVATE_PROG_MODE,
};

#define MLX5_RXP_POLL_CSR_FOR_VALUE_TIMEOUT 3000 /* Poll timeout in ms. */
#define MLX5_RXP_INITIALIZATION_TIMEOUT 60000 /* Initialize timeout in ms. */
#define MLX5_RXP_MAX_ENGINES 2u /* Number of RXP engines. */
#define MLX5_RXP_EM_COUNT 1u /* Extra External Memories to use. */
#define MLX5_RXP_DB_NOT_ASSIGNED 0xFF

struct mlx5_regex_umem {
	struct mlx5dv_devx_umem *umem;
	uint32_t id;
	uint64_t offset;
};

#endif /* RTE_PMD_MLX5_REGEX_RXP_H_ */
