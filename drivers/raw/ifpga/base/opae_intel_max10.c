/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#include "opae_intel_max10.h"
#include <libfdt.h>

int max10_reg_read(struct intel_max10_device *dev,
	unsigned int reg, unsigned int *val)
{
	if (!dev)
		return -ENODEV;

	dev_debug(dev, "%s: bus:0x%x, reg:0x%x\n", __func__, dev->bus, reg);

	return spi_transaction_read(dev->spi_tran_dev,
			reg, 4, (unsigned char *)val);
}

int max10_reg_write(struct intel_max10_device *dev,
	unsigned int reg, unsigned int val)
{
	unsigned int tmp = val;

	if (!dev)
		return -ENODEV;

	dev_debug(dev, "%s: bus:0x%x, reg:0x%x, val:0x%x\n", __func__,
			dev->bus, reg, val);

	return spi_transaction_write(dev->spi_tran_dev,
			reg, 4, (unsigned char *)&tmp);
}

int max10_sys_read(struct intel_max10_device *dev,
	unsigned int offset, unsigned int *val)
{
	if (!dev)
		return -ENODEV;


	return max10_reg_read(dev, dev->base + offset, val);
}

int max10_sys_write(struct intel_max10_device *dev,
	unsigned int offset, unsigned int val)
{
	if (!dev)
		return -ENODEV;

	return max10_reg_write(dev, dev->base + offset, val);
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
	struct max10_compatible_id *id;
	struct fdt_header hdr;
	char *fdt_root = NULL;

	u32 dt_size, dt_addr, val;
	int ret;

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

	ret = altera_nor_flash_read(max10, dt_addr, fdt_root, dt_size);
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
	ret = enable_nor_flash(max10, false);

	if (ret && fdt_root)
		opae_free(fdt_root);

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
			dev->base = MAX10_SEC_BASE_ADDR;
			dev->flags |= MAX10_FLAGS_SECURE;
		} else {
			dev_info(dev, "non-secure MAX10 detected\n");
			dev->base = MAX10_BASE_ADDR;
		}
		return 0;
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

	return 0;
}

static int
max10_non_secure_hw_init(struct intel_max10_device *dev)
{
	max10_check_capability(dev);

	max10_sensor_init(dev, 0);

	return 0;
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

	TAILQ_INIT(&dev->opae_sensor_list);

	dev->spi_master = spi;

	dev->spi_tran_dev = spi_transaction_init(spi, chipselect);
	if (!dev->spi_tran_dev) {
		dev_err(dev, "%s spi tran init fail\n", __func__);
		goto free_dev;
	}

	/* check the max10 version */
	ret = check_max10_version(dev);
	if (ret) {
		dev_err(dev, "Failed to find max10 hardware!\n");
		goto free_dev;
	}

	/* load the MAX10 device table */
	ret = init_max10_device_table(dev);
	if (ret) {
		dev_err(dev, "Init max10 device table fail\n");
		goto free_dev;
	}

	/* init max10 devices, like sensor*/
	if (dev->flags & MAX10_FLAGS_SECURE)
		ret = max10_secure_hw_init(dev);
	else
		ret = max10_non_secure_hw_init(dev);
	if (ret) {
		dev_err(dev, "Failed to init max10 hardware!\n");
		goto free_dtb;
	}

	/* read FPGA loading information */
	ret = max10_sys_read(dev, FPGA_PAGE_INFO, &val);
	if (ret) {
		dev_err(dev, "fail to get FPGA loading info\n");
		goto release_max10_hw;
	}
	dev_info(dev, "FPGA loaded from %s Image\n", val ? "User" : "Factory");

	return dev;

release_max10_hw:
	max10_sensor_uinit(dev);
free_dtb:
	if (dev->fdt_root)
		opae_free(dev->fdt_root);
	if (dev->spi_tran_dev)
		spi_transaction_remove(dev->spi_tran_dev);
free_dev:
	opae_free(dev);

	return NULL;
}

int intel_max10_device_remove(struct intel_max10_device *dev)
{
	if (!dev)
		return 0;

	max10_sensor_uinit(dev);

	if (dev->spi_tran_dev)
		spi_transaction_remove(dev->spi_tran_dev);

	if (dev->fdt_root)
		opae_free(dev->fdt_root);

	opae_free(dev);

	return 0;
}
