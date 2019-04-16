/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#include "opae_intel_max10.h"

static struct intel_max10_device *g_max10;

int max10_reg_read(unsigned int reg, unsigned int *val)
{
	if (!g_max10)
		return -ENODEV;

	return spi_transaction_read(g_max10->spi_tran_dev,
			reg, 4, (unsigned char *)val);
}

int max10_reg_write(unsigned int reg, unsigned int val)
{
	if (!g_max10)
		return -ENODEV;

	return spi_transaction_write(g_max10->spi_tran_dev,
			reg, 4, (unsigned char *)&val);
}

struct intel_max10_device *
intel_max10_device_probe(struct altera_spi_device *spi,
		int chipselect)
{
	struct intel_max10_device *dev;
	int ret;
	unsigned int val;

	dev = opae_malloc(sizeof(*dev));
	if (!dev)
		return NULL;

	dev->spi_master = spi;

	dev->spi_tran_dev = spi_transaction_init(spi, chipselect);
	if (!dev->spi_tran_dev) {
		dev_err(dev, "%s spi tran init fail\n", __func__);
		goto free_dev;
	}

	/* set the max10 device firstly */
	g_max10 = dev;

	/* read FPGA loading information */
	ret = max10_reg_read(FPGA_PAGE_INFO_OFF, &val);
	if (ret) {
		dev_err(dev, "fail to get FPGA loading info\n");
		goto spi_tran_fail;
	}
	dev_info(dev, "FPGA loaded from %s Image\n", val ? "User" : "Factory");

	/* set PKVL Polling manually in BBS */
	ret = max10_reg_write(PKVL_POLLING_CTRL, 0x3);
	if (ret) {
		dev_err(dev, "%s set PKVL polling fail\n", __func__);
		goto spi_tran_fail;
	}

	return dev;

spi_tran_fail:
	spi_transaction_remove(dev->spi_tran_dev);
free_dev:
	g_max10 = NULL;
	opae_free(dev);

	return NULL;
}

int intel_max10_device_remove(struct intel_max10_device *dev)
{
	if (!dev)
		return 0;

	if (dev->spi_tran_dev)
		spi_transaction_remove(dev->spi_tran_dev);

	g_max10 = NULL;
	opae_free(dev);

	return 0;
}
