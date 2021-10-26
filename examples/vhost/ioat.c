/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2020 Intel Corporation
 */

#include <sys/uio.h>
#ifdef RTE_RAW_IOAT
#include <rte_rawdev.h>
#include <rte_ioat_rawdev.h>

#include "ioat.h"
#include "main.h"

struct dma_for_vhost dma_bind[MAX_VHOST_DEVICE];

struct packet_tracker {
	unsigned short size_track[MAX_ENQUEUED_SIZE];
	unsigned short next_read;
	unsigned short next_write;
	unsigned short last_remain;
	unsigned short ioat_space;
};

struct packet_tracker cb_tracker[MAX_VHOST_DEVICE];

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
	int args_nr;

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
	if (args_nr <= 0) {
		ret = -1;
		goto out;
	}
	while (i < args_nr) {
		char *arg_temp = dma_arg[i];
		uint8_t sub_nr;
		sub_nr = rte_strsplit(arg_temp, strlen(arg_temp), ptrs, 2, '@');
		if (sub_nr != 2) {
			ret = -1;
			goto out;
		}

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
		cb_tracker[dev_id].ioat_space = IOAT_RING_SIZE - 1;
		dma_info->nr++;
		i++;
	}
out:
	free(input);
	return ret;
}

int32_t
ioat_transfer_data_cb(int vid, uint16_t queue_id,
		struct rte_vhost_iov_iter *iov_iter,
		struct rte_vhost_async_status *opaque_data, uint16_t count)
{
	uint32_t i_iter;
	uint16_t dev_id = dma_bind[vid].dmas[queue_id * 2 + VIRTIO_RXQ].dev_id;
	struct rte_vhost_iov_iter *iter = NULL;
	unsigned long i_seg;
	unsigned short mask = MAX_ENQUEUED_SIZE - 1;
	unsigned short write = cb_tracker[dev_id].next_write;

	if (!opaque_data) {
		for (i_iter = 0; i_iter < count; i_iter++) {
			iter = iov_iter + i_iter;
			i_seg = 0;
			if (cb_tracker[dev_id].ioat_space < iter->nr_segs)
				break;
			while (i_seg < iter->nr_segs) {
				rte_ioat_enqueue_copy(dev_id,
					(uintptr_t)(iter->iov[i_seg].src_addr),
					(uintptr_t)(iter->iov[i_seg].dst_addr),
					iter->iov[i_seg].len,
					0,
					0);
				i_seg++;
			}
			write &= mask;
			cb_tracker[dev_id].size_track[write] = iter->nr_segs;
			cb_tracker[dev_id].ioat_space -= iter->nr_segs;
			write++;
		}
	} else {
		/* Opaque data is not supported */
		return -1;
	}
	/* ring the doorbell */
	rte_ioat_perform_ops(dev_id);
	cb_tracker[dev_id].next_write = write;
	return i_iter;
}

int32_t
ioat_check_completed_copies_cb(int vid, uint16_t queue_id,
		struct rte_vhost_async_status *opaque_data,
		uint16_t max_packets)
{
	if (!opaque_data) {
		uintptr_t dump[255];
		int n_seg;
		unsigned short read, write;
		unsigned short nb_packet = 0;
		unsigned short mask = MAX_ENQUEUED_SIZE - 1;
		unsigned short i;

		uint16_t dev_id = dma_bind[vid].dmas[queue_id * 2
				+ VIRTIO_RXQ].dev_id;
		n_seg = rte_ioat_completed_ops(dev_id, 255, NULL, NULL, dump, dump);
		if (n_seg < 0) {
			RTE_LOG(ERR,
				VHOST_DATA,
				"fail to poll completed buf on IOAT device %u",
				dev_id);
			return 0;
		}
		if (n_seg == 0)
			return 0;

		cb_tracker[dev_id].ioat_space += n_seg;
		n_seg += cb_tracker[dev_id].last_remain;

		read = cb_tracker[dev_id].next_read;
		write = cb_tracker[dev_id].next_write;
		for (i = 0; i < max_packets; i++) {
			read &= mask;
			if (read == write)
				break;
			if (n_seg >= cb_tracker[dev_id].size_track[read]) {
				n_seg -= cb_tracker[dev_id].size_track[read];
				read++;
				nb_packet++;
			} else {
				break;
			}
		}
		cb_tracker[dev_id].next_read = read;
		cb_tracker[dev_id].last_remain = n_seg;
		return nb_packet;
	}
	/* Opaque data is not supported */
	return -1;
}

#endif /* RTE_RAW_IOAT */
