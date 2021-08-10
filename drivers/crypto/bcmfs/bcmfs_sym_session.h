/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#ifndef _BCMFS_SYM_SESSION_H_
#define _BCMFS_SYM_SESSION_H_

#include <stdbool.h>
#include <rte_crypto.h>
#include <cryptodev_pmd.h>

#include "bcmfs_sym_defs.h"
#include "bcmfs_sym_req.h"

/* BCMFS_SYM operation order mode enumerator */
enum bcmfs_sym_chain_order {
	BCMFS_SYM_CHAIN_ONLY_CIPHER,
	BCMFS_SYM_CHAIN_ONLY_AUTH,
	BCMFS_SYM_CHAIN_CIPHER_AUTH,
	BCMFS_SYM_CHAIN_AUTH_CIPHER,
	BCMFS_SYM_CHAIN_AEAD,
	BCMFS_SYM_CHAIN_NOT_SUPPORTED
};

/* BCMFS_SYM crypto private session structure */
struct bcmfs_sym_session {
	enum bcmfs_sym_chain_order chain_order;

	/* Cipher Parameters */
	struct {
		enum rte_crypto_cipher_operation op;
		/* Cipher operation */
		enum rte_crypto_cipher_algorithm algo;
		/* Cipher algorithm */
		struct {
			uint8_t data[BCMFS_MAX_KEY_SIZE];
			size_t length;
		} key;
		struct {
			uint16_t offset;
			uint16_t length;
		} iv;
	} cipher;

	/* Authentication Parameters */
	struct {
		enum rte_crypto_auth_operation op;
		/* Auth operation */
		enum rte_crypto_auth_algorithm algo;
		/* Auth algorithm */

		struct {
			uint8_t data[BCMFS_MAX_KEY_SIZE];
			size_t length;
		} key;
		struct {
			uint16_t offset;
			uint16_t length;
		} iv;

		uint16_t digest_length;
	} auth;

	/* Aead Parameters */
	struct {
		enum rte_crypto_aead_operation op;
		/* AEAD operation */
		enum rte_crypto_aead_algorithm algo;
		 /* AEAD algorithm */
		struct {
			uint8_t data[BCMFS_MAX_KEY_SIZE];
			size_t length;
		} key;
		struct {
			uint16_t offset;
			uint16_t length;
		} iv;

		uint16_t digest_length;

		uint16_t aad_length;
	} aead;

	bool cipher_first;
} __rte_cache_aligned;

int
bcmfs_process_crypto_op(struct rte_crypto_op *op,
			struct bcmfs_sym_session *sess,
			struct bcmfs_sym_request *req);

int
bcmfs_sym_session_configure(struct rte_cryptodev *dev,
			    struct rte_crypto_sym_xform *xform,
			    struct rte_cryptodev_sym_session *sess,
			    struct rte_mempool *mempool);

void
bcmfs_sym_session_clear(struct rte_cryptodev *dev,
			struct rte_cryptodev_sym_session  *sess);

unsigned int
bcmfs_sym_session_get_private_size(struct rte_cryptodev *dev __rte_unused);

struct bcmfs_sym_session *
bcmfs_sym_get_session(struct rte_crypto_op *op);

#endif /* _BCMFS_SYM_SESSION_H_ */
