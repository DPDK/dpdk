/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#ifndef _BCMFS_SYM_REQ_H_
#define _BCMFS_SYM_REQ_H_

#include <rte_cryptodev.h>

#include "bcmfs_dev_msg.h"
#include "bcmfs_sym_defs.h"

/* Fixed SPU2 Metadata */
struct spu2_fmd {
	uint64_t ctrl0;
	uint64_t ctrl1;
	uint64_t ctrl2;
	uint64_t ctrl3;
};

/*
 * This structure hold the supportive data required to process a
 * rte_crypto_op
 */
struct bcmfs_sym_request {
	/* spu2 engine related data */
	struct spu2_fmd fmd;
	/* cipher key */
	uint8_t cipher_key[BCMFS_MAX_KEY_SIZE];
	/* auth key */
	uint8_t auth_key[BCMFS_MAX_KEY_SIZE];
	/* iv key */
	uint8_t iv[BCMFS_MAX_IV_SIZE];
	/* digest data output from crypto h/w */
	uint8_t digest[BCMFS_MAX_DIGEST_SIZE];
	/* 2-Bytes response from crypto h/w */
	uint8_t resp[2];
	/*
	 * Below are all iovas for above members
	 * from top
	 */
	/* iova for fmd */
	rte_iova_t fptr;
	/* iova for cipher key */
	rte_iova_t cptr;
	/* iova for auth key */
	rte_iova_t aptr;
	/* iova for iv key */
	rte_iova_t iptr;
	/* iova for digest */
	rte_iova_t dptr;
	/* iova for response */
	rte_iova_t rptr;

	/* bcmfs qp message for h/w queues to process */
	struct bcmfs_qp_message msgs;
	/* crypto op */
	struct rte_crypto_op *op;
};

#endif /* _BCMFS_SYM_REQ_H_ */
