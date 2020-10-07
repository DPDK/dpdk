/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#ifndef _BCMFS_DEV_MSG_H_
#define _BCMFS_DEV_MSG_H_

#define MAX_SRC_ADDR_BUFFERS    8
#define MAX_DST_ADDR_BUFFERS    3

struct bcmfs_qp_message {
	/** Physical address of each source */
	uint64_t srcs_addr[MAX_SRC_ADDR_BUFFERS];
	/** Length of each sources */
	uint32_t srcs_len[MAX_SRC_ADDR_BUFFERS];
	/** Total number of sources */
	unsigned int srcs_count;
	/** Physical address of each destination */
	uint64_t dsts_addr[MAX_DST_ADDR_BUFFERS];
	/** Length of each destination */
	uint32_t dsts_len[MAX_DST_ADDR_BUFFERS];
	/** Total number of destinations */
	unsigned int dsts_count;

	void *ctx;
};

#endif /* _BCMFS_DEV_MSG_H_ */
