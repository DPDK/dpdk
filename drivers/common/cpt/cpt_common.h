/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _CPT_COMMON_H_
#define _CPT_COMMON_H_

/*
 * This file defines common macros and structs
 */

/*
 * Macros to determine CPT model. Driver makefile will define CPT_MODEL
 * accordingly
 */
#define CRYPTO_OCTEONTX		0x1

#define AE_TYPE 1
#define SE_TYPE 2

struct cptvf_meta_info {
	void *cptvf_meta_pool;
	int cptvf_op_mlen;
	int cptvf_op_sb_mlen;
};

struct rid {
	/** Request id of a crypto operation */
	uintptr_t rid;
};

/*
 * Pending queue structure
 *
 */
struct pending_queue {
	/** Tail of queue to be used for enqueue */
	uint16_t enq_tail;
	/** Head of queue to be used for dequeue */
	uint16_t deq_head;
	/** Array of pending requests */
	struct rid *rid_queue;
	/** Pending requests count */
	uint64_t pending_count;
};

#endif /* _CPT_COMMON_H_ */
