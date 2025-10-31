/* SPDX-License-Identifier: BSD-3-Clause
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

nthw_pca9849_t *nthw_pca9849_new(void);
int nthw_pca9849_init(nthw_pca9849_t *p, nthw_i2cm_t *p_nt_i2cm, uint8_t dev_address);

int nthw_pca9849_set_channel(nthw_pca9849_t *p, uint8_t channel);

/*
 * Si5332 clock synthesizer
 */

struct nthw_si5332 {
	nthw_i2cm_t *mp_nt_i2cm;
	uint8_t m_dev_address;
	nthw_pca9849_t *mp_pca9849;
	uint8_t m_mux_channel;
};

typedef struct nthw_si5332 nthw_si5332_t;

nthw_si5332_t *nthw_si5332_new(void);
int nthw_si5332_init(nthw_si5332_t *p, nthw_i2cm_t *p_nt_i2cm, uint8_t dev_address,
	nthw_pca9849_t *pca9849, uint8_t mux_channel);
bool nthw_si5332_clock_active(nthw_si5332_t *p);
void nthw_si5332_write(nthw_si5332_t *p, uint8_t address, uint8_t value);

/*
 * Si5156 MEMS Super TCXO
 */
struct nthw_si5156 {
	nthw_i2cm_t *mp_nt_i2cm;
	uint8_t m_dev_address;
	nthw_pca9849_t *mp_pca9849;
	uint8_t m_mux_channel;
};

typedef struct nthw_si5156 nthw_si5156_t;

nthw_si5156_t *nthw_si5156_new(void);
int nthw_si5156_init(nthw_si5156_t *p, nthw_i2cm_t *p_nt_i2cm, uint8_t dev_address,
	nthw_pca9849_t *pca9849, uint8_t mux_channel);
int nthw_si5156_write16(nthw_si5156_t *p, uint8_t address, uint16_t value);

#endif	/* __NTHW_SI5332_SI5156_H__ */
