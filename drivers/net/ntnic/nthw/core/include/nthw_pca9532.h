/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_PCA9532_H__
#define __NTHW_PCA9532_H__

#include "nthw_si5332_si5156.h"

struct nthw_pca9532 {
	nthw_i2cm_t *mp_nt_i2cm;
	uint8_t m_dev_address;
	nthw_pca9849_t *mp_ca9849;
	uint8_t m_mux_channel;
};

typedef struct nthw_pca9532 nthw_pca9532_t;

nthw_pca9532_t *nthw_pca9532_new(void);
int nthw_pca9532_init(nthw_pca9532_t *p, nthw_i2cm_t *p_nt_i2cm, uint8_t dev_address,
	nthw_pca9849_t *pca9849, uint8_t mux_channel);
void nthw_pca9532_set_led_on(nthw_pca9532_t *p, uint8_t led_pos, bool state_on);

#endif	/* __NTHW_PCA9532_H__ */
