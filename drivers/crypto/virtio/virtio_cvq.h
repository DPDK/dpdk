/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Marvell
 */

#ifndef _VIRTIO_CVQ_H_
#define _VIRTIO_CVQ_H_

#include <rte_spinlock.h>
#include <virtio_crypto.h>

#include "virtio_cryptodev.h"

struct virtqueue;

struct virtcrypto_ctl {
	const struct rte_memzone *hdr_mz; /**< memzone to populate hdr. */
	rte_iova_t hdr_mem;               /**< hdr for each xmit packet */
	rte_spinlock_t lock;              /**< spinlock for control queue. */
	void (*notify_queue)(struct virtqueue *vq, void *cookie); /**< notify ops. */
	void *notify_cookie;              /**< cookie for notify ops */
};

struct virtio_pmd_ctrl {
	struct virtio_crypto_op_ctrl_req hdr;
	struct virtio_crypto_session_input input;
	uint8_t data[VIRTIO_CRYPTO_MAX_CTRL_DATA];
};

int
virtio_crypto_send_command(struct virtcrypto_ctl *cvq, struct virtio_pmd_ctrl *ctrl,
	int *dlen, int pkt_num);

#endif /* _VIRTIO_CVQ_H_ */
