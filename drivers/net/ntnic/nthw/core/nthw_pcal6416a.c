/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "nt_util.h"
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_pcal6416a.h"

static const uint8_t read_port[2] = { 0x00, 0x01 };
static const uint8_t write_port[2] = { 0x02, 0x03 };
static const uint8_t config_port[2] = { 0x06, 0x07 };

/*
 * PCAL6416A I/O expander class
 */

nthw_pcal6416a_t *nthw_pcal6416a_new(void)
{
	nthw_pcal6416a_t *p = malloc(sizeof(nthw_pcal6416a_t));

	if (p) {
		memset(p, 0, sizeof(nthw_pcal6416a_t));
		p->m_config_data[0] = 0xFF;
		p->m_config_data[1] = 0xFF;
	}

	return p;
}

int nthw_pcal6416a_init(nthw_pcal6416a_t *p, nthw_i2cm_t *p_nt_i2cm, uint8_t dev_address,
	nthw_pca9849_t *pca9849, uint8_t mux_channel)
{
	p->mp_nt_i2cm = p_nt_i2cm;
	p->m_dev_address = dev_address;
	p->mp_ca9849 = pca9849;
	p->m_mux_channel = mux_channel;
	return 0;
}

void nthw_pcal6416a_write(nthw_pcal6416a_t *p, uint8_t pin, uint8_t value)
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

	nthw_i2cm_read(p->mp_nt_i2cm, p->m_dev_address, write_port[port], &data);

	if (value == 0)
		data = (uint8_t)(data & (~(uint8_t)(1 << pin)));

	else
		data = (uint8_t)(data | (1 << pin));

	nthw_i2cm_write(p->mp_nt_i2cm, p->m_dev_address, write_port[port], data);

	/* Enable pin as output pin when writing to it first time */
	data = (uint8_t)(p->m_config_data[port] & (~(uint8_t)(1 << pin)));

	if (data != p->m_config_data[port]) {
		nthw_i2cm_write(p->mp_nt_i2cm, p->m_dev_address, config_port[port], data);
		p->m_config_data[port] = data;
	}

	rte_spinlock_unlock(&p->mp_nt_i2cm->i2cmmutex);
}

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
