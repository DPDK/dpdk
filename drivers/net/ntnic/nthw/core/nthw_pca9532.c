/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "nt_util.h"
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_pca9532.h"

static const uint8_t led_sel_reg[4] = { 6, 7, 8, 9 };

nthw_pca9532_t *nthw_pca9532_new(void)
{
	nthw_pca9532_t *p = malloc(sizeof(nthw_pca9532_t));

	if (p)
		memset(p, 0, sizeof(nthw_pca9532_t));

	return p;
}

int nthw_pca9532_init(nthw_pca9532_t *p, nthw_i2cm_t *p_nt_i2cm, uint8_t dev_address,
	nthw_pca9849_t *pca9849, uint8_t mux_channel)
{
	p->mp_nt_i2cm = p_nt_i2cm;
	p->m_dev_address = dev_address;
	p->mp_ca9849 = pca9849;
	p->m_mux_channel = mux_channel;
	return 0;
}

void nthw_pca9532_set_led_on(nthw_pca9532_t *p, uint8_t led_pos, bool state_on)
{
	if (led_pos >= 16) {
		NT_LOG(ERR, NTHW, "Led pos (%u) out of range", led_pos);
		return;
	}

	rte_spinlock_lock(&p->mp_nt_i2cm->i2cmmutex);
	nthw_pca9849_set_channel(p->mp_ca9849, p->m_mux_channel);

	uint8_t reg_addr = led_sel_reg[led_pos / 4];
	uint8_t bit_pos = (uint8_t)((led_pos % 4) * 2);
	uint8_t data = 0U;

	nthw_i2cm_read(p->mp_nt_i2cm, p->m_dev_address, reg_addr, &data);
	data = data & (uint8_t)(~(0b11 << bit_pos));	/* Set bits to "00" aka off */

	if (state_on)
		data = (uint8_t)(data | (0b01 << bit_pos));

	nthw_i2cm_write(p->mp_nt_i2cm, p->m_dev_address, reg_addr, data);
	rte_spinlock_unlock(&p->mp_nt_i2cm->i2cmmutex);
}
