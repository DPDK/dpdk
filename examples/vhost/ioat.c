/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2020 Intel Corporation
 */
#include <rte_rawdev.h>
#include <rte_ioat_rawdev.h>

#include "ioat.h"
#include "main.h"

struct dma_for_vhost dma_bind[MAX_VHOST_DEVICE];

int
open_ioat(const char *value)
{
	struct dma_for_vhost *dma_info = dma_bind;
	char *input = strndup(value, strlen(value) + 1);
	char *addrs = input;
	char *ptrs[2];
	char *start, *end, *substr;
	int64_t vid, vring_id;
	struct rte_ioat_rawdev_config config;
	struct rte_rawdev_info info = { .dev_private = &config };
	char name[32];
	int dev_id;
	int ret = 0;
	uint16_t i = 0;
	char *dma_arg[MAX_VHOST_DEVICE];
	uint8_t args_nr;

	while (isblank(*addrs))
		addrs++;
	if (*addrs == '\0') {
		ret = -1;
		goto out;
	}

	/* process DMA devices within bracket. */
	addrs++;
	substr = strtok(addrs, ";]");
	if (!substr) {
		ret = -1;
		goto out;
	}
	args_nr = rte_strsplit(substr, strlen(substr),
			dma_arg, MAX_VHOST_DEVICE, ',');
	do {
		char *arg_temp = dma_arg[i];
		rte_strsplit(arg_temp, strlen(arg_temp), ptrs, 2, '@');

		start = strstr(ptrs[0], "txd");
		if (start == NULL) {
			ret = -1;
			goto out;
		}

		start += 3;
		vid = strtol(start, &end, 0);
		if (end == start) {
			ret = -1;
			goto out;
		}

		vring_id = 0 + VIRTIO_RXQ;
		if (rte_pci_addr_parse(ptrs[1],
				&(dma_info + vid)->dmas[vring_id].addr) < 0) {
			ret = -1;
			goto out;
		}

		rte_pci_device_name(&(dma_info + vid)->dmas[vring_id].addr,
				name, sizeof(name));
		dev_id = rte_rawdev_get_dev_id(name);
		if (dev_id == (uint16_t)(-ENODEV) ||
		dev_id == (uint16_t)(-EINVAL)) {
			ret = -1;
			goto out;
		}

		if (rte_rawdev_info_get(dev_id, &info, sizeof(config)) < 0 ||
		strstr(info.driver_name, "ioat") == NULL) {
			ret = -1;
			goto out;
		}

		(dma_info + vid)->dmas[vring_id].dev_id = dev_id;
		(dma_info + vid)->dmas[vring_id].is_valid = true;
		config.ring_size = IOAT_RING_SIZE;
		config.hdls_disable = true;
		if (rte_rawdev_configure(dev_id, &info, sizeof(config)) < 0) {
			ret = -1;
			goto out;
		}
		rte_rawdev_start(dev_id);

		dma_info->nr++;
		i++;
	} while (i < args_nr);
out:
	free(input);
	return ret;
}
