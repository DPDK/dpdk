/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */
#ifndef __NTHW_PCAL6416A_H__
#define __NTHW_PCAL6416A_H__

#include <stdint.h>

#include "nthw_i2cm.h"
#include "nthw_si5332_si5156.h"

/*
 * PCAL6416A I/O expander class
 */

struct nthw_pcal6416a {
	nthw_i2cm_t *mp_nt_i2cm;
	uint8_t m_dev_address;
	nthw_pca9849_t *mp_ca9849;
	uint8_t m_mux_channel;
	uint8_t m_config_data[2];
};

typedef struct nthw_pcal6416a nthw_pcal6416a_t;

nthw_pcal6416a_t *nthw_pcal6416a_new(void);
int nthw_pcal6416a_init(nthw_pcal6416a_t *p, nthw_i2cm_t *p_nt_i2cm, uint8_t dev_address,
	nthw_pca9849_t *pca9849, uint8_t mux_channel);
void nthw_pcal6416a_write(nthw_pcal6416a_t *p, uint8_t pin, uint8_t value);
void nthw_pcal6416a_read(nthw_pcal6416a_t *p, uint8_t pin, uint8_t *value);

#endif	/* __NTHW_PCAL6416A_H__ */
