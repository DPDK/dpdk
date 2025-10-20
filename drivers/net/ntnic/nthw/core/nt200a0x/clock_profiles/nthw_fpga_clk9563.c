/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntnic_mod_reg.h"

/*
 * Clock profile for NT200A02 FPGA 9563
 */
#define si5340_revd_register_t type_9563_si5340_nt200a02_u23_v5
#define si5340_revd_registers data_9563_si5340_nt200a02_u23_v5
#include "nt200a02_u23_si5340_adr0_v5_registers.h"
static_assert(sizeof(type_9563_si5340_nt200a02_u23_v5) == sizeof(clk_profile_data_fmt2_t),
	clk_profile_size_error_msg);
static const int n_data_9563_si5340_nt200a02_u23_v5 = SI5340_REVD_REG_CONFIG_NUM_REGS;
static const clk_profile_data_fmt2_t *p_data_9563_si5340_nt200a02_u23_v5 =
	(const clk_profile_data_fmt2_t *)&data_9563_si5340_nt200a02_u23_v5[0];

static const int *get_n_data_9563_si5340_nt200a02_u23_v5(void)
{
	return &n_data_9563_si5340_nt200a02_u23_v5;
}

static const clk_profile_data_fmt2_t *get_p_data_9563_si5340_nt200a02_u23_v5(void)
{
	return p_data_9563_si5340_nt200a02_u23_v5;
}

static struct clk9563_ops ops = { .get_n_data_9563_si5340_nt200a02_u23_v5 =
		get_n_data_9563_si5340_nt200a02_u23_v5,
		.get_p_data_9563_si5340_nt200a02_u23_v5 =
			get_p_data_9563_si5340_nt200a02_u23_v5
};

void clk9563_ops_init(void)
{
	nthw_reg_clk9563_ops(&ops);
}

#undef si5340_revd_registers
#undef si5340_revd_register_t
#undef SI5340_REVD_REG_CONFIG_HEADER	/* Disable the include once protection */

#undef code
