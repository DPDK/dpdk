/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Marvell.
 */

#ifndef _VIRTIO_RXTX_H_
#define _VIRTIO_RXTX_H_

struct virtcrypto_data {
	const struct rte_memzone *hdr_mz; /**< memzone to populate hdr. */
	rte_iova_t hdr_mem;               /**< hdr for each xmit packet */
};

#endif /* _VIRTIO_RXTX_H_ */
