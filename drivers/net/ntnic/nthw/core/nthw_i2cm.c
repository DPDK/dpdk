/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "nt_util.h"
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_i2cm.h"

#define NT_I2C_CMD_START 0x80
#define NT_I2C_CMD_STOP 0x40
#define NT_I2C_CMD_RD 0x20
#define NT_I2C_CMD_WR 0x10
#define NT_I2C_CMD_NACK 0x08
#define NT_I2C_CMD_IRQ_ACK 0x01

#define NT_I2C_STATUS_NACK 0x80
#define NT_I2C_STATUS_TIP 0x02

#define NT_I2C_TRANSMIT_WR 0x00
#define NT_I2C_TRANSMIT_RD 0x01

#define NUM_RETRIES 50U
#define SLEEP_USECS 100U/* 0.1 ms */


static bool nthw_i2cm_ready(nthw_i2cm_t *p, bool wait_for_ack)
{
	uint32_t flags = NT_I2C_STATUS_TIP | (wait_for_ack ? NT_I2C_STATUS_NACK : 0U);

	for (uint32_t i = 0U; i < NUM_RETRIES; i++) {
		uint32_t status = nthw_field_get_updated(p->mp_fld_cmd_status_cmd_status);
		uint32_t ready = (status & flags) == 0U;
		/* MUST have a short break to avoid time-outs, even if ready == true */
		nt_os_wait_usec(SLEEP_USECS);

		if (ready)
			return true;
	}

	return false;
}

static int nthw_i2cm_write_internal(nthw_i2cm_t *p, uint8_t value)
{
	/* Write data to data register */
	nthw_field_set_val_flush32(p->mp_fld_data_data, value);
	nthw_field_set_val_flush32(p->mp_fld_cmd_status_cmd_status,
		NT_I2C_CMD_WR | NT_I2C_CMD_IRQ_ACK);

	if (!nthw_i2cm_ready(p, true)) {
		nthw_field_set_val_flush32(p->mp_fld_cmd_status_cmd_status,
			NT_I2C_CMD_STOP | NT_I2C_CMD_IRQ_ACK);
		NT_LOG(ERR, NTHW, "%s: Time-out writing data %u", __PRETTY_FUNCTION__, value);
		return 1;
	}

	/* Generate stop condition and clear interrupt */
	nthw_field_set_val_flush32(p->mp_fld_cmd_status_cmd_status,
		NT_I2C_CMD_STOP | NT_I2C_CMD_IRQ_ACK);

	if (!nthw_i2cm_ready(p, true)) {
		nthw_field_set_val_flush32(p->mp_fld_cmd_status_cmd_status,
			NT_I2C_CMD_STOP | NT_I2C_CMD_IRQ_ACK);
		NT_LOG(ERR, NTHW, "%s: Time-out sending stop condition", __PRETTY_FUNCTION__);
		return 1;
	}

	return 0;
}

static int nthw_i2cm_write_reg_addr_internal(nthw_i2cm_t *p, uint8_t dev_addr, uint8_t reg_addr,
	bool send_stop)
{
	/* Write device address to data register */
	nthw_field_set_val_flush32(p->mp_fld_data_data,
		(uint8_t)(dev_addr << 1 | NT_I2C_TRANSMIT_WR));

	/* #Set start condition along with secondary I2C dev_addr */
	nthw_field_set_val_flush32(p->mp_fld_cmd_status_cmd_status,
		NT_I2C_CMD_START | NT_I2C_CMD_WR | NT_I2C_CMD_IRQ_ACK);

	if (!nthw_i2cm_ready(p, true)) {
		nthw_field_set_val_flush32(p->mp_fld_cmd_status_cmd_status,
			NT_I2C_CMD_STOP | NT_I2C_CMD_IRQ_ACK);
		NT_LOG(ERR, NTHW, "%s: Time-out writing device address %u, reg_addr=%u",
			__PRETTY_FUNCTION__, dev_addr, reg_addr);
		return 1;
	}

	/* Writing I2C register address */
	nthw_field_set_val_flush32(p->mp_fld_data_data, reg_addr);

	if (send_stop) {
		nthw_field_set_val_flush32(p->mp_fld_cmd_status_cmd_status,
			NT_I2C_CMD_WR | NT_I2C_CMD_IRQ_ACK | NT_I2C_CMD_STOP);

	} else {
		nthw_field_set_val_flush32(p->mp_fld_cmd_status_cmd_status,
			NT_I2C_CMD_WR | NT_I2C_CMD_IRQ_ACK);
	}

	if (!nthw_i2cm_ready(p, true)) {
		nthw_field_set_val_flush32(p->mp_fld_cmd_status_cmd_status,
			NT_I2C_CMD_STOP | NT_I2C_CMD_IRQ_ACK);
		NT_LOG(ERR, NTHW, "%s: Time-out writing register address %u", __PRETTY_FUNCTION__,
			reg_addr);
		return 1;
	}

	return 0;
}

static int nthw_i2cm_read_internal(nthw_i2cm_t *p, uint8_t dev_addr, uint8_t *value)
{
	/* Write I2C device address - with LSBit set to READ */

	nthw_field_set_val_flush32(p->mp_fld_data_data,
		(uint8_t)(dev_addr << 1 | NT_I2C_TRANSMIT_RD));
	/* #Send START condition along with secondary I2C dev_addr */
	nthw_field_set_val_flush32(p->mp_fld_cmd_status_cmd_status,
		NT_I2C_CMD_START | NT_I2C_CMD_WR | NT_I2C_CMD_IRQ_ACK);

	if (!nthw_i2cm_ready(p, true)) {
		nthw_field_set_val_flush32(p->mp_fld_cmd_status_cmd_status,
			NT_I2C_CMD_STOP | NT_I2C_CMD_IRQ_ACK);
		NT_LOG(ERR, NTHW, "%s: Time-out rewriting device address %u", __PRETTY_FUNCTION__,
			dev_addr);
		return 1;
	}

	nthw_field_set_val_flush32(p->mp_fld_cmd_status_cmd_status,
		NT_I2C_CMD_RD | NT_I2C_CMD_NACK | NT_I2C_CMD_IRQ_ACK);

	if (!nthw_i2cm_ready(p, false)) {
		nthw_field_set_val_flush32(p->mp_fld_cmd_status_cmd_status,
			NT_I2C_CMD_STOP | NT_I2C_CMD_IRQ_ACK);
		NT_LOG(ERR, NTHW, "%s: Time-out during read operation", __PRETTY_FUNCTION__);
		return 1;
	}

	*value = (uint8_t)nthw_field_get_updated(p->mp_fld_data_data);

	/* Generate stop condition and clear interrupt */
	nthw_field_set_val_flush32(p->mp_fld_cmd_status_cmd_status,
		NT_I2C_CMD_STOP | NT_I2C_CMD_IRQ_ACK);

	if (!nthw_i2cm_ready(p, false)) {
		nthw_field_set_val_flush32(p->mp_fld_cmd_status_cmd_status,
			NT_I2C_CMD_STOP | NT_I2C_CMD_IRQ_ACK);
		NT_LOG(ERR, NTHW, "%s: Time-out sending stop condition", __PRETTY_FUNCTION__);
		return 1;
	}

	return 0;
}

int nthw_i2cm_read(nthw_i2cm_t *p, uint8_t dev_addr, uint8_t reg_addr, uint8_t *value)
{
	int status;
	status = nthw_i2cm_write_reg_addr_internal(p, dev_addr, reg_addr, false);

	if (status != 0)
		return status;

	status = nthw_i2cm_read_internal(p, dev_addr, value);

	if (status != 0)
		return status;

	return 0;
}

int nthw_i2cm_write(nthw_i2cm_t *p, uint8_t dev_addr, uint8_t reg_addr, uint8_t value)
{
	int status;
	status = nthw_i2cm_write_reg_addr_internal(p, dev_addr, reg_addr, false);

	if (status != 0)
		return status;

	status = nthw_i2cm_write_internal(p, value);

	if (status != 0)
		return status;

	return 0;
}
