/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <pthread.h>

#include "nt_util.h"
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_pca9532.h"

static const uint8_t led_sel_reg[4] = { 6, 7, 8, 9 };

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
