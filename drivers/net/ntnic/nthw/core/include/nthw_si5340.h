/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_SI5340_H__
#define __NTHW_SI5340_H__

#include "clock_profiles_structs.h"

#define SI5340_SUCCESS (0)
#define SI5340_FAILED (999)
#define SI5340_TIMEOUT (666)

struct nthw_si5340 {
	uint8_t mn_iic_addr;
	nthw_iic_t *mp_nthw_iic;
	int mn_clk_cfg;
	uint8_t m_si5340_page;
};

typedef struct nthw_si5340 nthw_si5340_t;

nthw_si5340_t *nthw_si5340_new(void);
int nthw_si5340_init(nthw_si5340_t *p, nthw_iic_t *p_nthw_iic, uint8_t n_iic_addr);
void nthw_si5340_delete(nthw_si5340_t *p);

int nthw_si5340_config_fmt2(nthw_si5340_t *p, const clk_profile_data_fmt2_t *p_data,
	const int data_cnt);

#endif	/* __NTHW_SI5338_H__ */
