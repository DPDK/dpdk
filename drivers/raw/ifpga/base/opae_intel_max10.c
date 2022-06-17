/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#include "opae_intel_max10.h"
#include <libfdt.h>
#include "opae_osdep.h"

int max10_sys_read(struct intel_max10_device *dev,
	unsigned int offset, unsigned int *val)
{
	if (!dev || !dev->ops->reg_read)
		return -ENODEV;

	return dev->ops->reg_read(dev, dev->csr->base + offset, val);
}

int max10_sys_write(struct intel_max10_device *dev,
	unsigned int offset, unsigned int val)
{
	if (!dev || !dev->ops->reg_write)
		return -ENODEV;

	return dev->ops->reg_write(dev, dev->csr->base + offset, val);
}

int max10_reg_read(struct intel_max10_device *dev,
	unsigned int offset, unsigned int *val)
{
	if (!dev || !dev->ops->reg_read)
		return -ENODEV;

	return dev->ops->reg_read(dev, offset, val);
}

int max10_reg_write(struct intel_max10_device *dev,
	unsigned int offset, unsigned int val)
{
	if (!dev || !dev->ops->reg_write)
		return -ENODEV;

	return dev->ops->reg_write(dev, offset, val);
}

int max10_sys_update_bits(struct intel_max10_device *dev, unsigned int offset,
					unsigned int msk, unsigned int val)
{
	int ret = 0;
	unsigned int temp = 0;

	ret = max10_sys_read(dev, offset, &temp);
	if (ret < 0)
		return ret;

	temp &= ~msk;
	temp |= val & msk;

	return max10_sys_write(dev, offset, temp);
}

static int n3000_bulk_raw_write(struct intel_max10_device *dev, uint32_t addr,
	void *buf, uint32_t len)
{
	uint32_t v = 0;
	uint32_t i = 0;
	char *p = buf;
	int ret = 0;

	len = IFPGA_ALIGN(len, 4);

	for (i = 0; i < len; i += 4) {
		v = *(uint32_t *)(p + i);
		ret = max10_reg_write(dev, addr + i, v);
		if (ret < 0) {
			dev_err(dev,
				"Failed to write to staging area 0x%08x [e:%d]\n",
				addr + i, ret);
			return ret;
		}
	}

	return 0;
}

static int n3000_bulk_raw_read(struct intel_max10_device *dev,
		uint32_t addr, void *buf, uint32_t len)
{
	u32 v, i;
	char *p = buf;
	int ret;

	len = IFPGA_ALIGN(len, 4);

	for (i = 0; i < len; i += 4) {
		ret = max10_reg_read(dev, addr + i, &v);
		if (ret < 0) {
			dev_err(dev,
				"Failed to write to staging area 0x%08x [e:%d]\n",
				addr + i, ret);
			return ret;
		}
		*(u32 *)(p + i) = v;
	}

	return 0;
}

static int n3000_flash_read(struct intel_max10_device *dev,
		u32 addr, void *buf, u32 size)
{
	if (!dev->raw_blk_ops.read_blk)
		return -ENODEV;

	return dev->raw_blk_ops.read_blk(dev, addr, buf, size);
}

static int n3000_flash_write(struct intel_max10_device *dev,
		u32 addr, void *buf, u32 size)
{
	if (!dev->raw_blk_ops.write_blk)
		return -ENODEV;

	return dev->raw_blk_ops.write_blk(dev, addr, buf, size);
}

static u32
pmci_get_write_space(struct intel_max10_device *dev, u32 size)
{
	u32 count, val;
	int ret;

	ret = opae_readl_poll_timeout(dev->mmio + PMCI_FLASH_CTRL, val,
				GET_FIELD(PMCI_FLASH_FIFO_SPACE, val) ==
				PMCI_FIFO_MAX_WORDS,
				PMCI_FLASH_INT_US, PMCI_FLASH_TIMEOUT_US);
	if (ret == -ETIMEDOUT)
		return 0;

	count = GET_FIELD(PMCI_FLASH_FIFO_SPACE, val) * 4;

	return (size > count) ? count : size;
}

static void pmci_write_fifo(void __iomem *base, char *buf, size_t count)
{
	size_t i;
	u32 val;

	for (i = 0; i < count/4 ; i++) {
		val = *(u32 *)(buf + i * 4);
		writel(val, base);
	}
}

static void pmci_read_fifo(void __iomem *base, char *buf, size_t count)
{
	size_t i;
	u32 val;

	for (i = 0; i < count/4; i++) {
		val = readl(base);
		*(u32 *)(buf + i * 4) = val;
	}
}

static int
__pmci_flash_bulk_write(struct intel_max10_device *dev, u32 addr,
		void *buf, u32 size)
{
	UNUSED(addr);
	u32 blk_size, n_offset = 0;

	while (size) {
		blk_size = pmci_get_write_space(dev, size);
		if (blk_size == 0) {
			dev_err(pmci->dev, "get FIFO available size fail\n");
			return -EIO;
		}
		size -= blk_size;
		pmci_write_fifo(dev->mmio + PMCI_FLASH_FIFO, (char *)buf + n_offset,
				blk_size);
		n_offset += blk_size;
	}

	return 0;
}

static int
pmci_flash_bulk_write(struct intel_max10_device *dev, u32 addr,
		void *buf, u32 size)
{
	int ret;

	pthread_mutex_lock(dev->bmc_ops.mutex);

	ret = __pmci_flash_bulk_write(dev, addr, buf, size);

	pthread_mutex_unlock(dev->bmc_ops.mutex);
	return ret;
}

static int
pmci_set_flash_host_mux(struct intel_max10_device *dev, bool request)
{
	u32 ctrl;
	int ret;

	ret = max10_sys_update_bits(dev,
			m10bmc_base(dev) + M10BMC_PMCI_FLASH_CTRL,
			FLASH_HOST_REQUEST,
			SET_FIELD(FLASH_HOST_REQUEST, request));
	if (ret)
		return ret;

	return opae_max10_read_poll_timeout(dev, m10bmc_base(dev) + M10BMC_PMCI_FLASH_CTRL,
			ctrl, request ? (get_flash_mux(ctrl) == FLASH_MUX_HOST) :
			(get_flash_mux(ctrl) != FLASH_MUX_HOST),
			PMCI_FLASH_INT_US, PMCI_FLASH_TIMEOUT_US);
}

static int
pmci_get_mux(struct intel_max10_device *dev)
{
	pthread_mutex_lock(dev->bmc_ops.mutex);
	return pmci_set_flash_host_mux(dev, true);
}

static int
pmci_put_mux(struct intel_max10_device *dev)
{
	int ret;

	ret = pmci_set_flash_host_mux(dev, false);
	pthread_mutex_unlock(dev->bmc_ops.mutex);
	return ret;
}

static int
__pmci_flash_bulk_read(struct intel_max10_device *dev, u32 addr,
		     void *buf, u32 size)
{
	u32 blk_size, offset = 0, val;
	int ret;

	while (size) {
		blk_size = min_t(u32, size, PMCI_READ_BLOCK_SIZE);

		opae_writel(addr + offset, dev->mmio + PMCI_FLASH_ADDR);

		opae_writel(SET_FIELD(PMCI_FLASH_READ_COUNT, blk_size / 4)
				| PMCI_FLASH_RD_MODE,
			dev->mmio + PMCI_FLASH_CTRL);

		ret = opae_readl_poll_timeout((dev->mmio + PMCI_FLASH_CTRL),
				val, !(val & PMCI_FLASH_BUSY),
				PMCI_FLASH_INT_US,
				PMCI_FLASH_TIMEOUT_US);
		if (ret) {
			dev_err(dev, "%s timed out on reading flash 0x%xn",
				__func__, val);
			return ret;
		}

		pmci_read_fifo(dev->mmio + PMCI_FLASH_FIFO, (char *)buf + offset,
				blk_size);

		size -= blk_size;
		offset += blk_size;

		opae_writel(0, dev->mmio + PMCI_FLASH_CTRL);
	}

	return 0;
}

static int
pmci_flash_bulk_read(struct intel_max10_device *dev, u32 addr,
		     void *buf, u32 size)
{
	int ret;

	ret = pmci_get_mux(dev);
	if (ret)
		goto fail;

	ret = __pmci_flash_bulk_read(dev, addr, buf, size);
	if (ret)
		goto fail;

	return pmci_put_mux(dev);

fail:
	pmci_put_mux(dev);
	return ret;
}

static int pmci_check_flash_address(u32 start, u32 end)
{
	if (start < PMCI_FLASH_START || end > PMCI_FLASH_END)
		return -EINVAL;

	return 0;
}

int opae_read_flash(struct intel_max10_device *dev, u32 addr,
		u32 size, void *buf)
{
	int ret;

	if (!dev->bmc_ops.flash_read)
		return -ENODEV;

	if (!buf)
		return -EINVAL;

	if (dev->bmc_ops.check_flash_range) {
		ret = dev->bmc_ops.check_flash_range(addr, addr + size);
		if (ret)
			return ret;
	} else {
		u32 top_addr = dev->staging_area_base + dev->staging_area_size;
		if ((addr < dev->staging_area_base) ||
			((addr + size) >= top_addr))
			return -EINVAL;
	}

	ret = dev->bmc_ops.flash_read(dev, addr, buf, size);
	if (ret)
		return ret;

	return 0;
}

static int max10_spi_read(struct intel_max10_device *dev,
	unsigned int addr, unsigned int *val)
{
	if (!dev)
		return -ENODEV;

	dev_debug(dev, "%s: bus:0x%x, addr:0x%x\n", __func__, dev->bus, addr);

	return spi_transaction_read(dev->spi_tran_dev,
			addr, 4, (unsigned char *)val);
}

static int max10_spi_write(struct intel_max10_device *dev,
	unsigned int addr, unsigned int val)
{
	unsigned int tmp = val;

	if (!dev)
		return -ENODEV;

	dev_debug(dev, "%s: bus:0x%x, reg:0x%x, val:0x%x\n", __func__,
			dev->bus, addr, val);

	return spi_transaction_write(dev->spi_tran_dev,
			addr, 4, (unsigned char *)&tmp);
}

static int indirect_bus_clr_cmd(struct intel_max10_device *dev)
{
	unsigned int cmd;
	int ret;

	opae_writel(0, dev->mmio + INDIRECT_CMD_OFF);

	ret = opae_readl_poll_timeout((dev->mmio + INDIRECT_CMD_OFF), cmd,
				 (!cmd), INDIRECT_INT_US, INDIRECT_TIMEOUT_US);

	if (ret)
		dev_err(dev, "%s timed out on clearing cmd 0x%x\n",
				__func__, cmd);

	return ret;
}

static int max10_indirect_reg_read(struct intel_max10_device *dev,
	unsigned int addr, unsigned int *val)
{
	unsigned int cmd;
	int ret;

	if (!dev)
		return -ENODEV;

	pthread_mutex_lock(dev->bmc_ops.mutex);

	cmd = opae_readl(dev->mmio + INDIRECT_CMD_OFF);
	if (cmd)
		dev_warn(dev, "%s non-zero cmd 0x%x\n", __func__, cmd);

	opae_writel(addr, dev->mmio + INDIRECT_ADDR_OFF);

	opae_writel(INDIRECT_CMD_RD, dev->mmio + INDIRECT_CMD_OFF);

	ret = opae_readl_poll_timeout((dev->mmio + INDIRECT_CMD_OFF), cmd,
				 (cmd & INDIRECT_CMD_ACK), INDIRECT_INT_US,
				 INDIRECT_TIMEOUT_US);

	*val = opae_readl(dev->mmio + INDIRECT_RD_OFF);

	if (ret)
		dev_err(dev, "%s timed out on reg 0x%x cmd 0x%x\n",
				__func__, addr, cmd);

	if (indirect_bus_clr_cmd(dev))
		ret = -ETIME;

	pthread_mutex_unlock(dev->bmc_ops.mutex);

	return ret;
}

static int max10_indirect_reg_write(struct intel_max10_device *dev,
	unsigned int addr, unsigned int val)
{
	unsigned int cmd;
	int ret;

	if (!dev)
		return -ENODEV;

	pthread_mutex_lock(dev->bmc_ops.mutex);

	cmd = readl(dev->mmio + INDIRECT_CMD_OFF);

	if (cmd)
		dev_warn(dev, "%s non-zero cmd 0x%x\n", __func__, cmd);

	opae_writel(val, dev->mmio + INDIRECT_WR_OFF);

	opae_writel(addr, dev->mmio + INDIRECT_ADDR_OFF);

	writel(INDIRECT_CMD_WR, dev->mmio + INDIRECT_CMD_OFF);

	ret = opae_readl_poll_timeout((dev->mmio + INDIRECT_CMD_OFF), cmd,
				 (cmd & INDIRECT_CMD_ACK), INDIRECT_INT_US,
				 INDIRECT_TIMEOUT_US);

	if (ret)
		dev_err(dev, "%s timed out on reg 0x%x cmd 0x%x\n",
				__func__, addr, cmd);

	if (indirect_bus_clr_cmd(dev))
		ret = -ETIME;

	pthread_mutex_unlock(dev->bmc_ops.mutex);

	return ret;
}

const struct m10bmc_regmap m10bmc_pmci_regmap = {
	.reg_write = max10_indirect_reg_write,
	.reg_read = max10_indirect_reg_read,
};

const struct m10bmc_regmap m10bmc_n3000_regmap = {
	.reg_write = max10_spi_write,
	.reg_read = max10_spi_read,
};

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
	} else
		max10->flags |= MAX10_FLAGS_MAC_CACHE;
}

static int altera_nor_flash_read(struct intel_max10_device *dev,
	u32 offset, void *buffer, u32 len)
{
	int word_len;
	int i;
	unsigned int *buf = (unsigned int *)buffer;
	unsigned int value;
	int ret;

	if (!dev || !buffer || len <= 0)
		return -ENODEV;

	word_len = len/4;

	for (i = 0; i < word_len; i++) {
		ret = max10_reg_read(dev, offset + i*4,
				&value);
		if (ret)
			return -EBUSY;

		*buf++ = value;
	}

	return 0;
}

static int enable_nor_flash(struct intel_max10_device *dev, bool on)
{
	unsigned int val = 0;
	int ret;

	ret = max10_sys_read(dev, RSU_REG, &val);
	if (ret) {
		dev_err(NULL "enabling flash error\n");
		return ret;
	}

	if (on)
		val |= RSU_ENABLE;
	else
		val &= ~RSU_ENABLE;

	return max10_sys_write(dev, RSU_REG, val);
}

static int init_max10_device_table(struct intel_max10_device *max10)
{
	struct altera_spi_device *spi = NULL;
	struct max10_compatible_id *id;
	struct fdt_header hdr;
	char *fdt_root = NULL;
	u32 dtb_magic = 0;
	u32 dt_size, dt_addr, val;
	int ret = 0;

	spi = (struct altera_spi_device *)max10->spi_master;
	if (!spi) {
		dev_err(max10, "spi master is not set\n");
		return -EINVAL;
	}
	if (spi->dtb)
		dtb_magic = *(u32 *)spi->dtb;

	if (dtb_magic != 0xEDFE0DD0) {
		dev_info(max10, "read DTB from NOR flash\n");
		ret = max10_sys_read(max10, DT_AVAIL_REG, &val);
		if (ret) {
			dev_err(max10 "cannot read DT_AVAIL_REG\n");
			return ret;
		}

		if (!(val & DT_AVAIL)) {
			dev_err(max10 "DT not available\n");
			return -EINVAL;
		}

		ret = max10_sys_read(max10, DT_BASE_ADDR_REG, &dt_addr);
		if (ret) {
			dev_info(max10 "cannot get base addr of device table\n");
			return ret;
		}

		ret = enable_nor_flash(max10, true);
		if (ret) {
			dev_err(max10 "fail to enable flash\n");
			return ret;
		}

		ret = altera_nor_flash_read(max10, dt_addr, &hdr, sizeof(hdr));
		if (ret) {
			dev_err(max10 "read fdt header fail\n");
			goto disable_nor_flash;
		}

		ret = fdt_check_header(&hdr);
		if (ret) {
			dev_err(max10 "check fdt header fail\n");
			goto disable_nor_flash;
		}

		dt_size = fdt_totalsize(&hdr);
		if (dt_size > DFT_MAX_SIZE) {
			dev_err(max10 "invalid device table size\n");
			ret = -EINVAL;
			goto disable_nor_flash;
		}

		fdt_root = opae_malloc(dt_size);
		if (!fdt_root) {
			ret = -ENOMEM;
			goto disable_nor_flash;
		}

		ret = altera_nor_flash_read(max10, dt_addr, fdt_root, dt_size);
		if (ret) {
			opae_free(fdt_root);
			fdt_root = NULL;
			dev_err(max10 "cannot read device table\n");
			goto disable_nor_flash;
		}

		if (spi->dtb) {
			if (*spi->dtb_sz_ptr < dt_size) {
				dev_warn(max10,
						 "share memory for dtb is smaller than required %u\n",
						 dt_size);
			} else {
				*spi->dtb_sz_ptr = dt_size;
			}
			/* store dtb data into share memory  */
			memcpy(spi->dtb, fdt_root, *spi->dtb_sz_ptr);
		}

disable_nor_flash:
		enable_nor_flash(max10, false);
	} else {
		if (*spi->dtb_sz_ptr > 0) {
			dev_info(max10, "read DTB from shared memory\n");
			fdt_root = opae_malloc(*spi->dtb_sz_ptr);
			if (fdt_root)
				memcpy(fdt_root, spi->dtb, *spi->dtb_sz_ptr);
			else
				ret = -ENOMEM;
		}
	}

	if (fdt_root) {
		id = max10_match_compatible(fdt_root);
		if (!id) {
			dev_err(max10 "max10 compatible not found\n");
			ret = -ENODEV;
		} else {
			max10->flags |= MAX10_FLAGS_DEVICE_TABLE;
			max10->id = id;
			max10->fdt_root = fdt_root;
		}
	}

	return ret;
}

static u64 fdt_get_number(const fdt32_t *cell, int size)
{
	u64 r = 0;

	while (size--)
		r = (r << 32) | fdt32_to_cpu(*cell++);

	return r;
}

static int fdt_get_reg(const void *fdt, int node, unsigned int idx,
		u64 *start, u64 *size)
{
	const fdt32_t *prop, *end;
	int na = 0, ns = 0, len = 0, parent;

	parent = fdt_parent_offset(fdt, node);
	if (parent < 0)
		return parent;

	prop = fdt_getprop(fdt, parent, "#address-cells", NULL);
	na = prop ? fdt32_to_cpu(*prop) : 2;

	prop = fdt_getprop(fdt, parent, "#size-cells", NULL);
	ns = prop ? fdt32_to_cpu(*prop) : 2;

	prop = fdt_getprop(fdt, node, "reg", &len);
	if (!prop)
		return -FDT_ERR_NOTFOUND;

	end = prop + len/sizeof(*prop);
	prop = prop + (na + ns) * idx;

	if (prop + na + ns > end)
		return -FDT_ERR_NOTFOUND;

	*start = fdt_get_number(prop, na);
	*size = fdt_get_number(prop + na, ns);

	return 0;
}

static int __fdt_stringlist_search(const void *fdt, int offset,
		const char *prop, const char *string)
{
	int length, len, index = 0;
	const char *list, *end;

	list = fdt_getprop(fdt, offset, prop, &length);
	if (!list)
		return length;

	len = strlen(string) + 1;
	end = list + length;

	while (list < end) {
		length = strnlen(list, end - list) + 1;

		if (list + length > end)
			return -FDT_ERR_BADVALUE;

		if (length == len && memcmp(list, string, length) == 0)
			return index;

		list += length;
		index++;
	}

	return -FDT_ERR_NOTFOUND;
}

static int fdt_get_named_reg(const void *fdt, int node, const char *name,
		u64 *start, u64 *size)
{
	int idx;

	idx = __fdt_stringlist_search(fdt, node, "reg-names", name);
	if (idx < 0)
		return idx;

	return fdt_get_reg(fdt, node, idx, start, size);
}

static void max10_sensor_uinit(struct intel_max10_device *dev)
{
	struct opae_sensor_info *info;

	TAILQ_FOREACH(info, &dev->opae_sensor_list, node) {
		TAILQ_REMOVE(&dev->opae_sensor_list, info, node);
		opae_free(info);
	}
}

static bool sensor_reg_valid(struct sensor_reg *reg)
{
	return !!reg->size;
}

static int max10_add_sensor(struct intel_max10_device *dev,
	struct raw_sensor_info *info, struct opae_sensor_info *sensor)
{
	int i;
	int ret = 0;
	unsigned int val;

	if (!info || !sensor)
		return -ENODEV;

	sensor->id = info->id;
	sensor->name = info->name;
	sensor->type = info->type;
	sensor->multiplier = info->multiplier;

	for (i = SENSOR_REG_VALUE; i < SENSOR_REG_MAX; i++) {
		if (!sensor_reg_valid(&info->regs[i]))
			continue;

		ret = max10_sys_read(dev, info->regs[i].regoff, &val);
		if (ret)
			break;

		if (val == 0xdeadbeef) {
			dev_debug(dev, "%s: sensor:%s invalid 0x%x at:%d\n",
				__func__, sensor->name, val, i);
			continue;
		}

		val *= info->multiplier;

		switch (i) {
		case SENSOR_REG_VALUE:
			sensor->value_reg = info->regs[i].regoff;
			sensor->flags |= OPAE_SENSOR_VALID;
			break;
		case SENSOR_REG_HIGH_WARN:
			sensor->high_warn = val;
			sensor->flags |= OPAE_SENSOR_HIGH_WARN_VALID;
			break;
		case SENSOR_REG_HIGH_FATAL:
			sensor->high_fatal = val;
			sensor->flags |= OPAE_SENSOR_HIGH_FATAL_VALID;
			break;
		case SENSOR_REG_LOW_WARN:
			sensor->low_warn = val;
			sensor->flags |= OPAE_SENSOR_LOW_WARN_VALID;
			break;
		case SENSOR_REG_LOW_FATAL:
			sensor->low_fatal = val;
			sensor->flags |= OPAE_SENSOR_LOW_FATAL_VALID;
			break;
		case SENSOR_REG_HYSTERESIS:
			sensor->hysteresis = val;
			sensor->flags |= OPAE_SENSOR_HYSTERESIS_VALID;
			break;
		}
	}

	return ret;
}

static int
max10_sensor_init(struct intel_max10_device *dev, int parent)
{
	int i, ret = 0, offset = 0;
	const fdt32_t *num;
	const char *ptr;
	u64 start, size;
	struct raw_sensor_info *raw;
	struct opae_sensor_info *sensor;
	char *fdt_root = dev->fdt_root;

	if (!fdt_root) {
		dev_debug(dev, "skip sensor init as not find Device Tree\n");
		return 0;
	}

	fdt_for_each_subnode(offset, fdt_root, parent) {
		ptr = fdt_get_name(fdt_root, offset, NULL);
		if (!ptr) {
			dev_err(dev, "failed to fdt get name\n");
			continue;
		}

		if (!strstr(ptr, "sensor")) {
			dev_debug(dev, "%s is not a sensor node\n", ptr);
			continue;
		}

		dev_debug(dev, "found sensor node %s\n", ptr);

		raw = (struct raw_sensor_info *)opae_zmalloc(sizeof(*raw));
		if (!raw) {
			ret = -ENOMEM;
			goto free_sensor;
		}

		raw->name = fdt_getprop(fdt_root, offset, "sensor_name", NULL);
		if (!raw->name) {
			ret = -EINVAL;
			goto free_sensor;
		}

		raw->type = fdt_getprop(fdt_root, offset, "type", NULL);
		if (!raw->type) {
			ret = -EINVAL;
			goto free_sensor;
		}

		for (i = SENSOR_REG_VALUE; i < SENSOR_REG_MAX; i++) {
			ret = fdt_get_named_reg(fdt_root, offset,
					sensor_reg_name[i], &start,
					&size);
			if (ret) {
				dev_debug(dev, "no found %d: sensor node %s, %s\n",
						ret, ptr, sensor_reg_name[i]);
				if (i == SENSOR_REG_VALUE) {
					ret = -EINVAL;
					goto free_sensor;
				}

				continue;
			}

			/* This is a hack to compatible with non-secure
			 * solution. If sensors are included in root node,
			 * then it's non-secure dtb, which use absolute addr
			 * of non-secure solution.
			 */
			if (parent)
				raw->regs[i].regoff = start;
			else
				raw->regs[i].regoff = start -
					MAX10_BASE_ADDR;
			raw->regs[i].size = size;
		}

		num = fdt_getprop(fdt_root, offset, "id", NULL);
		if (!num) {
			ret = -EINVAL;
			goto free_sensor;
		}

		raw->id = fdt32_to_cpu(*num);
		num = fdt_getprop(fdt_root, offset, "multiplier", NULL);
		raw->multiplier = num ? fdt32_to_cpu(*num) : 1;

		dev_debug(dev, "found sensor from DTB: %s: %s: %u: %u\n",
				raw->name, raw->type,
				raw->id, raw->multiplier);

		for (i = SENSOR_REG_VALUE; i < SENSOR_REG_MAX; i++)
			dev_debug(dev, "sensor reg[%d]: %x: %zu\n",
					i, raw->regs[i].regoff,
					raw->regs[i].size);

		sensor = opae_zmalloc(sizeof(*sensor));
		if (!sensor) {
			ret = -EINVAL;
			goto free_sensor;
		}

		if (max10_add_sensor(dev, raw, sensor)) {
			ret = -EINVAL;
			opae_free(sensor);
			goto free_sensor;
		}

		if (sensor->flags & OPAE_SENSOR_VALID) {
			TAILQ_INSERT_TAIL(&dev->opae_sensor_list, sensor, node);
			dev_info(dev, "found valid sensor: %s\n", sensor->name);
		} else
			opae_free(sensor);

		opae_free(raw);
	}

	return 0;

free_sensor:
	if (raw)
		opae_free(raw);
	max10_sensor_uinit(dev);
	return ret;
}

static int check_max10_version(struct intel_max10_device *dev)
{
	unsigned int v;

	if (!max10_reg_read(dev, MAX10_SEC_BASE_ADDR + MAX10_BUILD_VER,
				&v)) {
		if (v != 0xffffffff) {
			dev_info(dev, "secure MAX10 detected\n");
			dev->flags |= MAX10_FLAGS_SECURE;
		} else {
			dev_info(dev, "non-secure MAX10 detected\n");
		}
		return 0;
	}

	return -ENODEV;
}

static int max10_staging_area_init(struct intel_max10_device *dev)
{
	char *fdt_root = dev->fdt_root;
	int ret, offset = 0;
	u64 start, size;

	if (!fdt_root) {
		dev_debug(dev,
			"skip staging area init as not find Device Tree\n");
		return -ENODEV;
	}

	dev->staging_area_size = 0;

	fdt_for_each_subnode(offset, fdt_root, 0) {
		if (fdt_node_check_compatible(fdt_root, offset,
					      "ifpga-sec-mgr,staging-area"))
			continue;

		ret = fdt_get_reg(fdt_root, offset, 0, &start, &size);
		if (ret)
			return ret;

		if ((start & 0x3) || (start > MAX_STAGING_AREA_BASE) ||
			(size > MAX_STAGING_AREA_SIZE))
			return -EINVAL;

		dev->staging_area_base = start;
		dev->staging_area_size = size;

		return ret;
	}

	return -ENODEV;
}

static int
max10_secure_hw_init(struct intel_max10_device *dev)
{
	int offset, sysmgr_offset = 0;
	char *fdt_root;

	fdt_root = dev->fdt_root;
	if (!fdt_root) {
		dev_debug(dev, "skip init as not find Device Tree\n");
		return 0;
	}

	fdt_for_each_subnode(offset, fdt_root, 0) {
		if (!fdt_node_check_compatible(fdt_root, offset,
					"intel-max10,system-manager")) {
			sysmgr_offset = offset;
			break;
		}
	}

	max10_check_capability(dev);

	max10_sensor_init(dev, sysmgr_offset);

	max10_staging_area_init(dev);

	return 0;
}

static int
max10_non_secure_hw_init(struct intel_max10_device *dev)
{
	max10_check_capability(dev);

	max10_sensor_init(dev, 0);

	return 0;
}

int max10_get_fpga_load_info(struct intel_max10_device *dev, unsigned int *val)
{
	int ret;
	unsigned int value;

	/* read FPGA loading information */
	ret = max10_sys_read(dev, dev->csr->fpga_page_info, &value);
	if (ret) {
		dev_err(dev, "fail to get FPGA loading info\n");
		return ret;
	}

	if (dev->type == M10_N3000)
		*val = value & 0x7;
	else if (dev->type == M10_N6000) {
		if (!GET_FIELD(PMCI_FPGA_CONFIGURED, value))
			return -EINVAL;
		*val = GET_FIELD(PMCI_FPGA_BOOT_PAGE, value);
	}

	return 0;
}

int max10_get_bmc_version(struct intel_max10_device *dev, unsigned int *val)
{
	int ret;

	ret = max10_sys_read(dev, dev->csr->build_version, val);
	if (ret)
		return ret;

	return 0;
}

int max10_get_bmcfw_version(struct intel_max10_device *dev, unsigned int *val)
{
	int ret;

	ret = max10_sys_read(dev, dev->csr->fw_version, val);
	if (ret)
		return ret;

	return 0;
}

static const struct m10bmc_csr m10bmc_spi_csr = {
	.base = MAX10_SEC_BASE_ADDR,
	.build_version = MAX10_BUILD_VER,
	.fw_version = NIOS2_FW_VERSION,
	.fpga_page_info = FPGA_PAGE_INFO,
	.doorbell = MAX10_DOORBELL,
	.auth_result = MAX10_AUTH_RESULT,
};

static const struct m10bmc_csr m10bmc_pmci_csr = {
	.base = M10BMC_PMCI_SYS_BASE,
	.build_version = M10BMC_PMCI_BUILD_VER,
	.fw_version = NIOS2_PMCI_FW_VERSION,
	.fpga_page_info = M10BMC_PMCI_FPGA_CONF_STS,
	.doorbell = M10BMC_PMCI_DOORBELL,
	.auth_result = M10BMC_PMCI_AUTH_RESULT,
};

static const struct max10_sensor_raw_data n6000bmc_temp_tbl[] = {
	{ 0x444, 0x448, 0x44c, 0x0, 0x0, 500,
		"FPGA E-TILE Temperature #1" },
	{ 0x450, 0x454, 0x458, 0x0, 0x0, 500,
		"FPGA E-TILE Temperature #2" },
	{ 0x45c, 0x460, 0x464, 0x0, 0x0, 500,
		"FPGA E-TILE Temperature #3" },
	{ 0x468, 0x46c, 0x470, 0x0, 0x0, 500,
		"FPGA E-TILE Temperature #4" },
	{ 0x474, 0x478, 0x47c, 0x0, 0x0, 500,
		"FPGA P-TILE Temperature" },
	{ 0x484, 0x488, 0x48c, 0x0, 0x0, 500,
		"FPGA FABRIC Digital Temperature#1" },
	{ 0x490, 0x494, 0x498, 0x0, 0x0, 500,
		"FPGA FABRIC Digital Temperature#2" },
	{ 0x49c, 0x4a0, 0x4a4, 0x0, 0x0, 500,
		"FPGA FABRIC Digital Temperature#3" },
	{ 0x4a8, 0x4ac, 0x4b0, 0x0, 0x0, 500,
		"FPGA FABRIC Digital Temperature#4" },
	{ 0x4b4, 0x4b8, 0x4bc, 0x0, 0x0, 500,
		"FPGA FABRIC Digital Temperature#5" },
	{ 0x4c0, 0x4c4, 0x4c8, 0x0, 0x0, 500,
		"FPGA FABRIC Remote Digital Temperature#1" },
	{ 0x4cc, 0x4d0, 0x4d4, 0x0, 0x0, 500,
		"FPGA FABRIC Remote Digital Temperature#2" },
	{ 0x4d8, 0x4dc, 0x4e0, 0x0, 0x0, 500,
		"FPGA FABRIC Remote Digital Temperature#3" },
	{ 0x4e4, 0x4e8, 0x4ec, 0x0, 0x0, 500,
		"FPGA FABRIC Remote Digital Temperature#4" },
	{ 0x4f0, 0x4f4, 0x4f8, 0x0, 0x0, 500,
		"Board Top Near FPGA Temperature" },
	{ 0x4fc, 0x500, 0x504, 0x52c, 0x0, 500,
		"Board Bottom Near CVL Temperature" },
	{ 0x508, 0x50c, 0x510, 0x52c, 0x0, 500,
		"Board Top East Near VRs Temperature" },
	{ 0x514, 0x518, 0x51c, 0x52c, 0x0, 500,
		"Columbiaville Die Temperature" },
	{ 0x520, 0x524, 0x528, 0x52c, 0x0, 500,
		"Board Rear Side Temperature" },
	{ 0x530, 0x534, 0x538, 0x52c, 0x0, 500,
		"Board Front Side Temperature" },
	{ 0x53c, 0x540, 0x544, 0x0, 0x0, 500,
		"QSFP1 Temperature" },
	{ 0x548, 0x54c, 0x550, 0x0, 0x0, 500,
		"QSFP2 Temperature" },
	{ 0x554, 0x0, 0x0, 0x0, 0x0, 500,
		"FPGA Core Voltage Phase 0 VR Temperature" },
	{ 0x560, 0x0, 0x0, 0x0, 0x0, 500,
		"FPGA Core Voltage Phase 1 VR Temperature" },
	{ 0x56c, 0x0, 0x0, 0x0, 0x0, 500,
		"FPGA Core Voltage Phase 2 VR Temperature" },
	{ 0x578, 0x0, 0x0, 0x0, 0x0, 500,
		"FPGA Core Voltage VR Controller Temperature" },
	{ 0x584, 0x0, 0x0, 0x0, 0x0, 500,
		"FPGA VCCH VR Temperature" },
	{ 0x590, 0x0, 0x0, 0x0, 0x0, 500,
		"FPGA VCC_1V2 VR Temperature" },
	{ 0x59c, 0x0, 0x0, 0x0, 0x0, 500,
		"FPGA VCCH, VCC_1V2 VR Controller Temperature" },
	{ 0x5a8, 0x0, 0x0, 0x0, 0x0, 500,
		"3V3 VR Temperature" },
	{ 0x5b4, 0x5b8, 0x5bc, 0x0, 0x0, 500,
		"CVL Core Voltage VR Temperature" },
	{ 0x5c4, 0x5c8, 0x5cc, 0x5c0, 0x0, 500,
		"FPGA P-Tile Temperature [Remote]" },
	{ 0x5d0, 0x5d4, 0x5d8, 0x5c0, 0x0, 500,
		"FPGA E-Tile Temperature [Remote]" },
	{ 0x5dc, 0x5e0, 0x5e4, 0x5c0, 0x0, 500,
		"FPGA SDM Temperature [Remote]" },
	{ 0x5e8, 0x5ec, 0x5f0, 0x5c0, 0x0, 500,
		"FPGA Corner Temperature [Remote]" },
};

static const struct max10_sensor_data n6000bmc_tmp_data = {
	.type = SENSOR_TMP_NAME,
	.number = ARRAY_SIZE(n6000bmc_temp_tbl),
	.table = n6000bmc_temp_tbl,
};

static const struct max10_sensor_raw_data n6000bmc_in_tbl[] = {
	{ 0x5f4, 0x0, 0x0, 0x0, 0x0, 1,
		"Inlet 12V PCIe Rail Voltage" },
	{ 0x60c, 0x0, 0x0, 0x0, 0x0, 1,
		"Inlet 12V Aux Rail Voltage" },
	{ 0x624, 0x0, 0x0, 0x0, 0x0, 1,
		"Inlet 3V3 PCIe Rail Voltage" },
	{ 0x63c, 0x0, 0x0, 0x0, 0x0, 1,
		"FPGA Core Voltage Rail Voltage" },
	{ 0x644, 0x0, 0x0, 0x0, 0x0, 1,
		"FPGA VCCH Rail Voltage" },
	{ 0x64c, 0x0, 0x0, 0x0, 0x0, 1,
		"FPGA VCC_1V2 Rail Voltage" },
	{ 0x654, 0x0, 0x0, 0x0, 0x0, 1,
		"FPGA VCCH_GXER_1V1, VCCA_1V8 Voltage" },
	{ 0x664, 0x0, 0x0, 0x0, 0x0, 1,
		"FPGA VCCIO_1V2 Voltage" },
	{ 0x674, 0x0, 0x0, 0x0, 0x0, 1,
		"CVL Non Core Rails Inlet Voltage" },
	{ 0x684, 0x0, 0x0, 0x0, 0x0, 1,
		"MAX10 & Board CLK PWR 3V3 Inlet Voltage" },
	{ 0x694, 0x0, 0x0, 0x0, 0x0, 1,
		"CVL Core Voltage Rail Voltage" },
	{ 0x6ac, 0x0, 0x0, 0x0, 0x0, 1,
		"Board 3V3 VR Voltage" },
	{ 0x6b4, 0x0, 0x0, 0x0, 0x0, 1,
		"QSFP 3V3 Rail Voltage" },
	{ 0x6c4, 0x0, 0x0, 0x0, 0x0, 1,
		"QSFP (Primary) Supply Rail Voltage" },
	{ 0x6c8, 0x0, 0x0, 0x0, 0x0, 1,
		"QSFP (Secondary) Supply Rail Voltage" },
	{ 0x6cc, 0x0, 0x0, 0x0, 0x0, 1,
		"VCCCLK_GXER_2V5 Voltage" },
	{ 0x6d0, 0x0, 0x0, 0x0, 0x0, 1,
		"AVDDH_1V1_CVL Voltage" },
	{ 0x6d4, 0x0, 0x0, 0x0, 0x0, 1,
		"VDDH_1V8_CVL Voltage" },
	{ 0x6d8, 0x0, 0x0, 0x0, 0x0, 1,
		"VCCA_PLL Voltage" },
	{ 0x6e0, 0x0, 0x0, 0x0, 0x0, 1,
		"VCCRT_GXER_0V9 Voltage" },
	{ 0x6e8, 0x0, 0x0, 0x0, 0x0, 1,
		"VCCRT_GXEL_0V9 Voltage" },
	{ 0x6f0, 0x0, 0x0, 0x0, 0x0, 1,
		"VCCH_GXPL_1V8 Voltage" },
	{ 0x6f4, 0x0, 0x0, 0x0, 0x0, 1,
		"VCCPT_1V8 Voltage" },
	{ 0x6fc, 0x0, 0x0, 0x0, 0x0, 1,
		"VCC_3V3_M10 Voltage" },
	{ 0x700, 0x0, 0x0, 0x0, 0x0, 1,
		"VCC_1V8_M10 Voltage" },
	{ 0x704, 0x0, 0x0, 0x0, 0x0, 1,
		"VCC_1V2_EMIF1_2_3 Voltage" },
	{ 0x70c, 0x0, 0x0, 0x0, 0x0, 1,
		"VCC_1V2_EMIF4_5 Voltage" },
	{ 0x714, 0x0, 0x0, 0x0, 0x0, 1,
		"VCCA_1V8 Voltage" },
	{ 0x718, 0x0, 0x0, 0x0, 0x0, 1,
		"VCCH_GXER_1V1 Voltage" },
	{ 0x71c, 0x0, 0x0, 0x0, 0x0, 1,
		"AVDD_ETH_0V9_CVL Voltage" },
	{ 0x720, 0x0, 0x0, 0x0, 0x0, 1,
		"AVDD_PCIE_0V9_CVL Voltage" },
};

static const struct max10_sensor_data n6000bmc_in_data = {
	.type = SENSOR_IN_NAME,
	.number = ARRAY_SIZE(n6000bmc_in_tbl),
	.table = n6000bmc_in_tbl,
};

static const struct max10_sensor_raw_data n6000bmc_curr_tbl[] = {
	{ 0x600, 0x604, 0x608, 0x0, 0x0, 1,
		"Inlet 12V PCIe Rail Current" },
	{ 0x618, 0x61c, 0x620, 0x0, 0x0, 1,
		"Inlet 12V Aux Rail Current" },
	{ 0x630, 0x634, 0x638, 0x0, 0x0, 1,
		"Inlet 3V3 PCIe Rail Current" },
	{ 0x640, 0x0, 0x0, 0x0, 0x0, 1,
		"FPGA Core Voltage Rail Current" },
	{ 0x648, 0x0, 0x0, 0x0, 0x0, 1,
		"FPGA VCCH Rail Current" },
	{ 0x650, 0x0, 0x0, 0x0, 0x0, 1,
		"FPGA VCC_1V2 Rail Current" },
	{ 0x658, 0x65c, 0x660, 0x0, 0x0, 1,
		"FPGA VCCH_GXER_1V1, VCCA_1V8 Current" },
	{ 0x668, 0x66c, 0x670, 0x0, 0x0, 1,
		"FPGA VCCIO_1V2 Current" },
	{ 0x678, 0x67c, 0x680, 0x0, 0x0, 1,
		"CVL Non Core Rails Inlet Current" },
	{ 0x688, 0x68c, 0x680, 0x0, 0x0, 1,
		"MAX10 & Board CLK PWR 3V3 Inlet Current" },
	{ 0x690, 0x0, 0x0, 0x0, 0x0, 1,
		"CVL Core Voltage Rail Current" },
	{ 0x6b0, 0x0, 0x0, 0x0, 0x0, 1,
		"Board 3V3 VR Current" },
	{ 0x6b8, 0x6bc, 0x670, 0x0, 0x0, 1,
		"QSFP 3V3 Rail Current" },
};

static const struct max10_sensor_data n6000bmc_curr_data = {
	.type = SENSOR_CURR_NAME,
	.number = ARRAY_SIZE(n6000bmc_curr_tbl),
	.table = n6000bmc_curr_tbl,
};

static const struct max10_sensor_raw_data n6000bmc_power_tbl[] = {
	{ 0x724, 0x0, 0x0, 0x0, 0x0, 1000, "Board Power" },
};

static const struct max10_sensor_data n6000bmc_power_data = {
	.type = SENSOR_POWER_NAME,
	.number = ARRAY_SIZE(n6000bmc_power_tbl),
	.table = n6000bmc_power_tbl,
};

static const struct max10_sensor_board_data n6000bmc_sensor_board_data = {
	.tables = {
		[sensor_temp] = &n6000bmc_tmp_data,
		[sensor_in] = &n6000bmc_in_data,
		[sensor_curr] = &n6000bmc_curr_data,
		[sensor_power] = &n6000bmc_power_data,
	},
};

static int get_sensor_data(struct intel_max10_device *dev,
		struct opae_sensor_info *sensor,
		unsigned int *value,
		unsigned int reg,
		unsigned int flags)
{
	int ret;
	unsigned int data;

	if (!reg)
		return 0;

	ret = max10_sys_read(dev, reg, &data);
	if (ret)
		return ret;

	if (data == SENSOR_INVALID) {
		dev_debug(dev, "%s: sensor:%s invalid 0x%x at:%d\n",
				__func__, sensor->name, data, reg);
		return ret;
	}

	*value = data * sensor->multiplier;
	sensor->flags |= flags;

	return 0;
}

static int max10_parse_sensor_data(struct intel_max10_device *dev,
		const struct max10_sensor_data *sdata)
{
	struct opae_sensor_info *sensor;
	const struct max10_sensor_raw_data *raw;
	const struct max10_sensor_raw_data *table =
		(const struct max10_sensor_raw_data *)sdata->table;
	unsigned int i;
	static unsigned int sensor_id;
	int ret = 0;

	for (i = 0; i < sdata->number; i++) {
		raw = &table[i];

		sensor = opae_zmalloc(sizeof(*sensor));
		if (!sensor) {
			ret = -EINVAL;
			goto free_sensor;
		}

		sensor->type = sdata->type;
		sensor->id = sensor_id++;

		if (!raw->reg_input)
			continue;

		sensor->value_reg = raw->reg_input;
		sensor->multiplier = raw->multiplier;
		sensor->name = raw->label;

		ret = get_sensor_data(dev, sensor,
				&sensor->high_warn,
				raw->reg_high_warn,
				OPAE_SENSOR_HIGH_WARN_VALID);
		if (ret)
			break;

		ret = get_sensor_data(dev, sensor,
				&sensor->high_fatal,
				raw->reg_high_fatal,
				OPAE_SENSOR_HIGH_FATAL_VALID);
		if (ret)
			break;

		ret = get_sensor_data(dev, sensor,
				&sensor->hysteresis,
				raw->reg_hyst,
				OPAE_SENSOR_HYSTERESIS_VALID);
		if (ret)
			break;

		ret = get_sensor_data(dev, sensor,
				&sensor->low_warn,
				raw->reg_low_warn,
				OPAE_SENSOR_LOW_WARN_VALID);
		if (ret)
			break;

		sensor->flags |= OPAE_SENSOR_VALID;

		TAILQ_INSERT_TAIL(&dev->opae_sensor_list, sensor, node);
		dev_info(dev, "found valid sensor: %s\n", sensor->name);
	}

	return ret;

free_sensor:
	max10_sensor_uinit(dev);
	return ret;
}

static int max10_sensor_init_table(struct intel_max10_device *dev,
		const struct max10_sensor_board_data *data)
{
	int ret = 0;
	unsigned int i;
	const struct max10_sensor_data *sdata;

	for (i = 0; i < ARRAY_SIZE(data->tables); i++) {
		sdata = data->tables[i];
		if (!sdata)
			continue;
		ret = max10_parse_sensor_data(dev, sdata);
		if (ret)
			break;
	}

	return ret;
}

int
intel_max10_device_init(struct intel_max10_device *dev)
{
	int ret = 0;

	TAILQ_INIT(&dev->opae_sensor_list);


	if (dev->type == M10_N3000) {
		dev->ops = &m10bmc_n3000_regmap;
		dev->csr = &m10bmc_spi_csr;

		dev->raw_blk_ops.write_blk = n3000_bulk_raw_write;
		dev->raw_blk_ops.read_blk = n3000_bulk_raw_read;
		dev->bmc_ops.flash_read = n3000_flash_read;
		dev->bmc_ops.flash_write = n3000_flash_write;

		/* check the max10 version */
		ret = check_max10_version(dev);
		if (ret) {
			dev_err(dev, "Failed to find max10 hardware!\n");
			return ret;
		}

		/* load the MAX10 device table */
		ret = init_max10_device_table(dev);
		if (ret) {
			dev_err(dev, "Init max10 device table fail\n");
			return ret;
		}

		/* init max10 devices, like sensor*/
		if (dev->flags & MAX10_FLAGS_SECURE)
			ret = max10_secure_hw_init(dev);
		else
			ret = max10_non_secure_hw_init(dev);
		if (ret) {
			dev_err(dev, "Failed to init max10 hardware!\n");
			opae_free(dev->fdt_root);
			return ret;
		}
	} else if (dev->type == M10_N6000) {
		dev->ops = &m10bmc_pmci_regmap;
		dev->csr = &m10bmc_pmci_csr;
		dev->staging_area_size = MAX_STAGING_AREA_SIZE;
		dev->flags |= MAX10_FLAGS_SECURE;

		dev->bmc_ops.flash_read = pmci_flash_bulk_read;
		dev->bmc_ops.flash_write = pmci_flash_bulk_write;
		dev->bmc_ops.check_flash_range = pmci_check_flash_address;

		ret = max10_sensor_init_table(dev, &n6000bmc_sensor_board_data);
		if (ret)
			return ret;

		ret = pthread_mutex_init(&dev->bmc_ops.lock, NULL);
		if (ret)
			return ret;

		if (!dev->bmc_ops.mutex)
			dev->bmc_ops.mutex = &dev->bmc_ops.lock;
	}

	return ret;
}

int intel_max10_device_remove(struct intel_max10_device *dev)
{
	if (!dev)
		return 0;

	pthread_mutex_destroy(&dev->bmc_ops.lock);

	if (dev->type == M10_N3000) {
		max10_sensor_uinit(dev);

		if (dev->fdt_root)
			opae_free(dev->fdt_root);
	}

	return 0;
}
