/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "generic/rte_spinlock.h"
#include "nt_util.h"
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_si5332_si5156.h"

nthw_pca9849_t *nthw_pca9849_new(void)
{
	nthw_pca9849_t *p = malloc(sizeof(nthw_pca9849_t));

	if (p)
		memset(p, 0, sizeof(nthw_pca9849_t));

	return p;
}

int nthw_pca9849_init(nthw_pca9849_t *p, nthw_i2cm_t *p_nt_i2cm, uint8_t dev_address)
{
	p->mp_nt_i2cm = p_nt_i2cm;
	p->m_dev_address = dev_address;
	p->m_current_channel = 0;
	return 0;
}

int nthw_pca9849_set_channel(nthw_pca9849_t *p, uint8_t channel)
{
	int res;

	if (p->m_current_channel != channel) {
		uint8_t data = channel + 4;
		res = nthw_i2cm_write(p->mp_nt_i2cm, p->m_dev_address, 0, data);

		if (res)
			return res;

		p->m_current_channel = channel;
		nthw_os_wait_usec(10000);
	}

	return 0;
}

/*
 * Si5332 clock synthesizer
 */

nthw_si5332_t *nthw_si5332_new(void)
{
	nthw_si5332_t *p = malloc(sizeof(nthw_si5332_t));

	if (p)
		memset(p, 0, sizeof(nthw_si5332_t));

	return p;
}

int nthw_si5332_init(nthw_si5332_t *p, nthw_i2cm_t *p_nt_i2cm, uint8_t dev_address,
	nthw_pca9849_t *pca9849, uint8_t mux_channel)
{
	p->mp_nt_i2cm = p_nt_i2cm;
	p->m_dev_address = dev_address;
	p->m_mux_channel = mux_channel;
	p->mp_pca9849 = pca9849;
	return 0;
}

bool nthw_si5332_clock_active(nthw_si5332_t *p)
{
	uint8_t ena1, ena2;

	rte_spinlock_lock(&p->mp_nt_i2cm->i2cmmutex);
	nthw_pca9849_set_channel(p->mp_pca9849, p->m_mux_channel);

	nthw_i2cm_read(p->mp_nt_i2cm, p->m_dev_address, 0xB6, &ena1);
	NT_LOG(DBG, NTHW, "Read %x from i2c dev 0x6A, reg 0xB6", ena1);

	nthw_i2cm_read(p->mp_nt_i2cm, p->m_dev_address, 0xB7, &ena2);
	NT_LOG(DBG, NTHW, "Read %x from i2c dev 0x6A, reg 0xB7", ena2);
	rte_spinlock_unlock(&p->mp_nt_i2cm->i2cmmutex);

	return ((ena1 & 0xDB) != 0) || ((ena2 & 0x06) != 0);
}

void nthw_si5332_write(nthw_si5332_t *p, uint8_t address, uint8_t value)
{
	rte_spinlock_lock(&p->mp_nt_i2cm->i2cmmutex);
	nthw_pca9849_set_channel(p->mp_pca9849, p->m_mux_channel);
	nthw_i2cm_write(p->mp_nt_i2cm, p->m_dev_address, address, value);
	rte_spinlock_unlock(&p->mp_nt_i2cm->i2cmmutex);
}

/*
 * Si5156 MEMS Super TCXO
 */

nthw_si5156_t *nthw_si5156_new(void)
{
	nthw_si5156_t *p = malloc(sizeof(nthw_si5156_t));

	if (p)
		memset(p, 0, sizeof(nthw_si5156_t));

	return p;
}

int nthw_si5156_init(nthw_si5156_t *p, nthw_i2cm_t *p_nt_i2cm, uint8_t dev_address,
	nthw_pca9849_t *pca9849, uint8_t mux_channel)
{
	p->mp_nt_i2cm = p_nt_i2cm;
	p->m_dev_address = dev_address;
	p->m_mux_channel = mux_channel;
	p->mp_pca9849 = pca9849;
	return 0;
}

int nthw_si5156_write16(nthw_si5156_t *p, uint8_t address, uint16_t value)
{
	int res = 0;
	rte_spinlock_lock(&p->mp_nt_i2cm->i2cmmutex);
	res = nthw_pca9849_set_channel(p->mp_pca9849, p->m_mux_channel);

	if (res)
		goto ERROR;

	res = nthw_i2cm_write16(p->mp_nt_i2cm, p->m_dev_address, address, value);

	if (res)
		goto ERROR;

ERROR:
	rte_spinlock_unlock(&p->mp_nt_i2cm->i2cmmutex);
	return res;
}
