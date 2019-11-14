/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#include "opae_osdep.h"
#include "opae_spi.h"

static int nios_spi_indirect_read(struct altera_spi_device *dev, u32 reg,
		u32 *val)
{
	u64 ctrl = 0;
	u64 stat = 0;
	int loops = SPI_MAX_RETRY;

	ctrl = NIOS_SPI_RD | ((u64)reg << 32);
	opae_writeq(ctrl, dev->regs + NIOS_SPI_CTRL);

	stat = opae_readq(dev->regs + NIOS_SPI_STAT);
	while (!(stat & NIOS_SPI_VALID) && --loops)
		stat = opae_readq(dev->regs + NIOS_SPI_STAT);

	*val = stat & NIOS_SPI_READ_DATA;

	return loops ? 0 : -ETIMEDOUT;
}

static int nios_spi_indirect_write(struct altera_spi_device *dev, u32 reg,
		u32 value)
{

	u64 ctrl = 0;
	u64 stat = 0;
	int loops = SPI_MAX_RETRY;

	ctrl |= NIOS_SPI_WR | (u64)reg << 32;
	ctrl |= value & NIOS_SPI_WRITE_DATA;

	opae_writeq(ctrl, dev->regs + NIOS_SPI_CTRL);

	stat = opae_readq(dev->regs + NIOS_SPI_STAT);
	while (!(stat & NIOS_SPI_VALID) && --loops)
		stat = opae_readq(dev->regs + NIOS_SPI_STAT);

	return loops ? 0 : -ETIMEDOUT;
}

static int spi_indirect_write(struct altera_spi_device *dev, u32 reg,
		u32 value)
{
	u64 ctrl;

	opae_writeq(value & WRITE_DATA_MASK, dev->regs + SPI_WRITE);

	ctrl = CTRL_W | (reg >> 2);
	opae_writeq(ctrl, dev->regs + SPI_CTRL);

	return 0;
}

static int spi_indirect_read(struct altera_spi_device *dev, u32 reg,
		u32 *val)
{
	u64 tmp;
	u64 ctrl;

	ctrl = CTRL_R | (reg >> 2);
	opae_writeq(ctrl, dev->regs + SPI_CTRL);

	/**
	 *  FIXME: Read one more time to avoid HW timing issue. This is
	 *  a short term workaround solution, and must be removed once
	 *  hardware fixing is done.
	 */
	tmp = opae_readq(dev->regs + SPI_READ);

	*val = (u32)tmp;

	return 0;
}

int spi_reg_write(struct altera_spi_device *dev, u32 reg,
		u32 value)
{
	return dev->reg_write(dev, reg, value);
}

int spi_reg_read(struct altera_spi_device *dev, u32 reg,
		u32 *val)
{
	return dev->reg_read(dev, reg, val);
}

void spi_cs_activate(struct altera_spi_device *dev, unsigned int chip_select)
{
	spi_reg_write(dev, ALTERA_SPI_SLAVE_SEL, 1 << chip_select);
	spi_reg_write(dev, ALTERA_SPI_CONTROL, ALTERA_SPI_CONTROL_SSO_MSK);
}

void spi_cs_deactivate(struct altera_spi_device *dev)
{
	spi_reg_write(dev, ALTERA_SPI_CONTROL, 0);
}

static int spi_flush_rx(struct altera_spi_device *dev)
{
	u32 val = 0;
	int ret;

	ret = spi_reg_read(dev, ALTERA_SPI_STATUS, &val);
	if (ret)
		return ret;

	if (val & ALTERA_SPI_STATUS_RRDY_MSK) {
		ret = spi_reg_read(dev, ALTERA_SPI_RXDATA, &val);
		if (ret)
			return ret;
	}

	return 0;
}

static unsigned int spi_write_bytes(struct altera_spi_device *dev, int count)
{
	unsigned int val = 0;
	u16 *p16;
	u32 *p32;

	if (dev->txbuf) {
		switch (dev->data_width) {
		case 1:
			val = dev->txbuf[count];
			break;
		case 2:
			p16 = (u16 *)(dev->txbuf + 2*count);
			val = *p16;
			if (dev->endian == SPI_BIG_ENDIAN)
				val = cpu_to_be16(val);
			break;
		case 4:
			p32 = (u32 *)(dev->txbuf + 4*count);
			val = *p32;
			break;
		}
	}

	return val;
}

static void spi_fill_readbuffer(struct altera_spi_device *dev,
		unsigned int value, int count)
{
	u16 *p16;
	u32 *p32;

	if (dev->rxbuf) {
		switch (dev->data_width) {
		case 1:
			dev->rxbuf[count] = value;
			break;
		case 2:
			p16 = (u16 *)(dev->rxbuf + 2*count);
			if (dev->endian == SPI_BIG_ENDIAN)
				*p16 = cpu_to_be16((u16)value);
			else
				*p16 = (u16)value;
			break;
		case 4:
			p32 = (u32 *)(dev->rxbuf + 4*count);
			if (dev->endian == SPI_BIG_ENDIAN)
				*p32 = cpu_to_be32(value);
			else
				*p32 = value;
			break;
		}
	}
}

static int spi_txrx(struct altera_spi_device *dev)
{
	unsigned int count = 0;
	u32 rxd;
	unsigned int tx_data;
	u32 status;
	int ret;

	while (count < dev->len) {
		tx_data = spi_write_bytes(dev, count);
		spi_reg_write(dev, ALTERA_SPI_TXDATA, tx_data);

		while (1) {
			ret = spi_reg_read(dev, ALTERA_SPI_STATUS, &status);
			if (ret)
				return -EIO;
			if (status & ALTERA_SPI_STATUS_RRDY_MSK)
				break;
		}

		ret = spi_reg_read(dev, ALTERA_SPI_RXDATA, &rxd);
		if (ret)
			return -EIO;

		spi_fill_readbuffer(dev, rxd, count);

		count++;
	}

	return 0;
}

int spi_command(struct altera_spi_device *dev, unsigned int chip_select,
		unsigned int wlen, void *wdata,
		unsigned int rlen, void *rdata)
{
	if (((wlen > 0) && !wdata) || ((rlen > 0) && !rdata)) {
		dev_err(dev, "error on spi command checking\n");
		return -EINVAL;
	}

	wlen = wlen / dev->data_width;
	rlen = rlen / dev->data_width;

	/* flush rx buffer */
	spi_flush_rx(dev);

	spi_cs_activate(dev, chip_select);
	if (wlen) {
		dev->txbuf = wdata;
		dev->rxbuf = rdata;
		dev->len = wlen;
		spi_txrx(dev);
	}
	if (rlen) {
		dev->rxbuf = rdata;
		dev->txbuf = NULL;
		dev->len = rlen;
		spi_txrx(dev);
	}
	spi_cs_deactivate(dev);
	return 0;
}

struct altera_spi_device *altera_spi_alloc(void *base, int type)
{
	struct altera_spi_device *spi_dev =
		opae_malloc(sizeof(struct altera_spi_device));

	if (!spi_dev)
		return NULL;

	spi_dev->regs = base;

	switch (type) {
	case TYPE_SPI:
		spi_dev->reg_read = spi_indirect_read;
		spi_dev->reg_write = spi_indirect_write;
		break;
	case TYPE_NIOS_SPI:
		spi_dev->reg_read = nios_spi_indirect_read;
		spi_dev->reg_write = nios_spi_indirect_write;
		break;
	default:
		dev_err(dev, "%s: invalid SPI type\n", __func__);
		goto error;
	}

	return spi_dev;

error:
	altera_spi_release(spi_dev);
	return NULL;
}

void altera_spi_init(struct altera_spi_device *spi_dev)
{
	spi_dev->spi_param.info = opae_readq(spi_dev->regs + SPI_CORE_PARAM);

	spi_dev->data_width = spi_dev->spi_param.data_width / 8;
	spi_dev->endian = spi_dev->spi_param.endian;
	spi_dev->num_chipselect = spi_dev->spi_param.num_chipselect;
	dev_info(spi_dev, "spi param: type=%d, data width:%d, endian:%d, clock_polarity=%d, clock=%dMHz, chips=%d, cpha=%d\n",
			spi_dev->spi_param.type,
			spi_dev->data_width, spi_dev->endian,
			spi_dev->spi_param.clock_polarity,
			spi_dev->spi_param.clock,
			spi_dev->num_chipselect,
			spi_dev->spi_param.clock_phase);

	/* clear */
	spi_reg_write(spi_dev, ALTERA_SPI_CONTROL, 0);
	spi_reg_write(spi_dev, ALTERA_SPI_STATUS, 0);
	/* flush rxdata */
	spi_flush_rx(spi_dev);
}

void altera_spi_release(struct altera_spi_device *dev)
{
	if (dev)
		opae_free(dev);
}
