/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2020 Intel Corporation
 */

#ifndef _IOAT_H_
#define _IOAT_H_

#include <rte_vhost.h>
#include <rte_pci.h>
#include <rte_vhost_async.h>

#define MAX_VHOST_DEVICE 1024
#define IOAT_RING_SIZE 4096
#define MAX_ENQUEUED_SIZE 4096

struct dma_info {
	struct rte_pci_addr addr;
	uint16_t dev_id;
	bool is_valid;
};

struct dma_for_vhost {
	struct dma_info dmas[RTE_MAX_QUEUES_PER_PORT * 2];
	uint16_t nr;
};

#ifdef RTE_RAW_IOAT
int open_ioat(const char *value);

int32_t
ioat_transfer_data_cb(int vid, uint16_t queue_id,
		struct rte_vhost_iov_iter *iov_iter,
		struct rte_vhost_async_status *opaque_data, uint16_t count);

int32_t
ioat_check_completed_copies_cb(int vid, uint16_t queue_id,
		struct rte_vhost_async_status *opaque_data,
		uint16_t max_packets);
#else
static int open_ioat(const char *value __rte_unused)
{
	return -1;
}

static int32_t
ioat_transfer_data_cb(int vid __rte_unused, uint16_t queue_id __rte_unused,
		struct rte_vhost_iov_iter *iov_iter __rte_unused,
		struct rte_vhost_async_status *opaque_data __rte_unused,
		uint16_t count __rte_unused)
{
	return -1;
}

static int32_t
ioat_check_completed_copies_cb(int vid __rte_unused,
		uint16_t queue_id __rte_unused,
		struct rte_vhost_async_status *opaque_data __rte_unused,
		uint16_t max_packets __rte_unused)
{
	return -1;
}
#endif
#endif /* _IOAT_H_ */
