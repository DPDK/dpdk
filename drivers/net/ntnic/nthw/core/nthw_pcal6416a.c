/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */
#include <pthread.h>

#include "nt_util.h"
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_pcal6416a.h"

static const uint8_t read_port[2] = { 0x00, 0x01 };

/*
 * PCAL6416A I/O expander class
 */

void nthw_pcal6416a_read(nthw_pcal6416a_t *p, uint8_t pin, uint8_t *value)
{
	uint8_t port;
	uint8_t data;

	rte_spinlock_lock(&p->mp_nt_i2cm->i2cmmutex);
	nthw_pca9849_set_channel(p->mp_ca9849, p->m_mux_channel);

	if (pin < 8) {
		port = 0;

	} else {
		port = 1;
		pin = (uint8_t)(pin - 8);
	}

	nthw_i2cm_read(p->mp_nt_i2cm, p->m_dev_address, read_port[port], &data);

	*value = (data >> pin) & 0x1;
	rte_spinlock_unlock(&p->mp_nt_i2cm->i2cmmutex);
}
