/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_SI5332_SI5156_H__
#define __NTHW_SI5332_SI5156_H__

#include "nthw_i2cm.h"
/*
 * PCA9849 I2c mux class
 */

struct nthw_pca9849 {
	nthw_i2cm_t *mp_nt_i2cm;
	uint8_t m_current_channel;
	uint8_t m_dev_address;
};

typedef struct nthw_pca9849 nthw_pca9849_t;

int nthw_pca9849_set_channel(nthw_pca9849_t *p, uint8_t channel);

#endif	/* __NTHW_SI5332_SI5156_H__ */
