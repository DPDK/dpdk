/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#include "opae_intel_max10.h"
#include <libfdt.h>

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
	unsigned int tmp = val;

	if (!g_max10)
		return -ENODEV;

	return spi_transaction_write(g_max10->spi_tran_dev,
			reg, 4, (unsigned char *)&tmp);
}

static struct max10_compatible_id max10_id_table[] = {
	{.compatible = MAX10_PAC,},
	{.compatible = MAX10_PAC_N3000,},
	{.compatible = MAX10_PAC_END,}
};

static struct max10_compatible_id *max10_match_compatible(const char *fdt_root)
{
	struct max10_compatible_id *id = max10_id_table;

	for (; strcmp(id->compatible, MAX10_PAC_END); id++) {
		if (fdt_node_check_compatible(fdt_root, 0, id->compatible))
			continue;

		return id;
	}

	return NULL;
}

static inline bool
is_max10_pac_n3000(struct intel_max10_device *max10)
{
	return max10->id && !strcmp(max10->id->compatible,
			MAX10_PAC_N3000);
}

static void max10_check_capability(struct intel_max10_device *max10)
{
	if (!max10->fdt_root)
		return;

	if (is_max10_pac_n3000(max10)) {
		max10->flags |= MAX10_FLAGS_NO_I2C2 |
				MAX10_FLAGS_NO_BMCIMG_FLASH;
		dev_info(max10, "found %s card\n", max10->id->compatible);
	}
}

static int altera_nor_flash_read(u32 offset,
		void *buffer, u32 len)
{
	int word_len;
	int i;
	unsigned int *buf = (unsigned int *)buffer;
	unsigned int value;
	int ret;

	if (!buffer || len <= 0)
		return -ENODEV;

	word_len = len/4;

	for (i = 0; i < word_len; i++) {
		ret = max10_reg_read(offset + i*4,
				&value);
		if (ret)
			return -EBUSY;

		*buf++ = value;
	}

	return 0;
}

static int enable_nor_flash(bool on)
{
	unsigned int val = 0;
	int ret;

	ret = max10_reg_read(RSU_REG_OFF, &val);
	if (ret) {
		dev_err(NULL "enabling flash error\n");
		return ret;
	}

	if (on)
		val |= RSU_ENABLE;
	else
		val &= ~RSU_ENABLE;

	return max10_reg_write(RSU_REG_OFF, val);
}

static int init_max10_device_table(struct intel_max10_device *max10)
{
	struct max10_compatible_id *id;
	struct fdt_header hdr;
	char *fdt_root = NULL;

	u32 dt_size, dt_addr, val;
	int ret;

	ret = max10_reg_read(DT_AVAIL_REG_OFF, &val);
	if (ret) {
		dev_err(max10 "cannot read DT_AVAIL_REG\n");
		return ret;
	}

	if (!(val & DT_AVAIL)) {
		dev_err(max10 "DT not available\n");
		return -EINVAL;
	}

	ret = max10_reg_read(DT_BASE_ADDR_REG_OFF, &dt_addr);
	if (ret) {
		dev_info(max10 "cannot get base addr of device table\n");
		return ret;
	}

	ret = enable_nor_flash(true);
	if (ret) {
		dev_err(max10 "fail to enable flash\n");
		return ret;
	}

	ret = altera_nor_flash_read(dt_addr, &hdr, sizeof(hdr));
	if (ret) {
		dev_err(max10 "read fdt header fail\n");
		goto done;
	}

	ret = fdt_check_header(&hdr);
	if (ret) {
		dev_err(max10 "check fdt header fail\n");
		goto done;
	}

	dt_size = fdt_totalsize(&hdr);
	if (dt_size > DFT_MAX_SIZE) {
		dev_err(max10 "invalid device table size\n");
		ret = -EINVAL;
		goto done;
	}

	fdt_root = opae_malloc(dt_size);
	if (!fdt_root) {
		ret = -ENOMEM;
		goto done;
	}

	ret = altera_nor_flash_read(dt_addr, fdt_root, dt_size);
	if (ret) {
		dev_err(max10 "cannot read device table\n");
		goto done;
	}

	id = max10_match_compatible(fdt_root);
	if (!id) {
		dev_err(max10 "max10 compatible not found\n");
		ret = -ENODEV;
		goto done;
	}

	max10->flags |= MAX10_FLAGS_DEVICE_TABLE;

	max10->id = id;
	max10->fdt_root = fdt_root;

done:
	ret = enable_nor_flash(false);

	if (ret && fdt_root)
		opae_free(fdt_root);

	return ret;
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

	/* init the MAX10 device table */
	ret = init_max10_device_table(dev);
	if (ret) {
		dev_err(dev, "init max10 device table fail\n");
		goto free_dev;
	}

	max10_check_capability(dev);

	/* read FPGA loading information */
	ret = max10_reg_read(FPGA_PAGE_INFO_OFF, &val);
	if (ret) {
		dev_err(dev, "fail to get FPGA loading info\n");
		goto spi_tran_fail;
	}
	dev_info(dev, "FPGA loaded from %s Image\n", val ? "User" : "Factory");

	return dev;

spi_tran_fail:
	if (dev->fdt_root)
		opae_free(dev->fdt_root);
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

	if (dev->fdt_root)
		opae_free(dev->fdt_root);

	g_max10 = NULL;
	opae_free(dev);

	return 0;
}
