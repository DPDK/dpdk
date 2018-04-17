/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */

#ifndef _VIRTIO_CRYPTO_ALGS_H_
#define _VIRTIO_CRYPTO_ALGS_H_

#include <rte_memory.h>

#include "virtio_crypto.h"

struct virtio_crypto_session {
	uint64_t session_id;

	struct {
		uint16_t offset;
		uint16_t length;
	} iv;

	struct {
		uint32_t length;
		phys_addr_t phys_addr;
	} aad;

	struct virtio_crypto_op_ctrl_req ctrl;
};

#endif /* _VIRTIO_CRYPTO_ALGS_H_ */
