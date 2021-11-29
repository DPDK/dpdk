/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#ifndef _BCMFS_SYM_DEFS_H_
#define _BCMFS_SYM_DEFS_H_

/*
 * Max block size of hash algorithm
 * currently SHA3 supports max block size
 * of 144 bytes
 */
#define BCMFS_MAX_KEY_SIZE	144
#define BCMFS_MAX_IV_SIZE	16
#define BCMFS_MAX_DIGEST_SIZE	64

struct bcmfs_sym_session;
struct bcmfs_sym_request;

/** Crypto Request processing successful. */
#define BCMFS_SYM_RESPONSE_SUCCESS               (0)
/** Crypto Request processing protocol failure. */
#define BCMFS_SYM_RESPONSE_PROTO_FAILURE         (1)
/** Crypto Request processing completion failure. */
#define BCMFS_SYM_RESPONSE_COMPL_ERROR           (2)
/** Crypto Request processing hash tag check error. */
#define BCMFS_SYM_RESPONSE_HASH_TAG_ERROR        (3)

/** Maximum threshold length to adjust AAD in continuation
 *  with source BD of (FMD + OMD)
 */
#define BCMFS_AAD_THRESH_LEN	64

int
bcmfs_process_sym_crypto_op(struct rte_crypto_op *op,
			    struct bcmfs_sym_session *sess,
			    struct bcmfs_sym_request *req);
#endif /* _BCMFS_SYM_DEFS_H_ */
