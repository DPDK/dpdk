/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_cryptodev.h>
#include <rte_security.h>

#include "test_security_proto.h"

int
test_sec_crypto_caps_aead_verify(const struct rte_security_capability *sec_cap,
		struct rte_crypto_sym_xform *aead)
{
	const struct rte_cryptodev_symmetric_capability *sym_cap;
	const struct rte_cryptodev_capabilities *crypto_cap;
	int j = 0;

	while ((crypto_cap = &sec_cap->crypto_capabilities[j++])->op !=
			RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		if (crypto_cap->op == RTE_CRYPTO_OP_TYPE_SYMMETRIC &&
				crypto_cap->sym.xform_type == aead->type &&
				crypto_cap->sym.aead.algo == aead->aead.algo) {
			sym_cap = &crypto_cap->sym;
			if (rte_cryptodev_sym_capability_check_aead(sym_cap,
					aead->aead.key.length,
					aead->aead.digest_length,
					aead->aead.aad_length,
					aead->aead.iv.length) == 0)
				return 0;
		}
	}

	return -ENOTSUP;
}

int
test_sec_crypto_caps_cipher_verify(const struct rte_security_capability *sec_cap,
		struct rte_crypto_sym_xform *cipher)
{
	const struct rte_cryptodev_symmetric_capability *sym_cap;
	const struct rte_cryptodev_capabilities *cap;
	int j = 0;

	while ((cap = &sec_cap->crypto_capabilities[j++])->op !=
			RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		if (cap->op == RTE_CRYPTO_OP_TYPE_SYMMETRIC &&
				cap->sym.xform_type == cipher->type &&
				cap->sym.cipher.algo == cipher->cipher.algo) {
			sym_cap = &cap->sym;
			if (rte_cryptodev_sym_capability_check_cipher(sym_cap,
					cipher->cipher.key.length,
					cipher->cipher.iv.length) == 0)
				return 0;
		}
	}

	return -ENOTSUP;
}

int
test_sec_crypto_caps_auth_verify(const struct rte_security_capability *sec_cap,
		struct rte_crypto_sym_xform *auth)
{
	const struct rte_cryptodev_symmetric_capability *sym_cap;
	const struct rte_cryptodev_capabilities *cap;
	int j = 0;

	while ((cap = &sec_cap->crypto_capabilities[j++])->op !=
			RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		if (cap->op == RTE_CRYPTO_OP_TYPE_SYMMETRIC &&
				cap->sym.xform_type == auth->type &&
				cap->sym.auth.algo == auth->auth.algo) {
			sym_cap = &cap->sym;
			if (rte_cryptodev_sym_capability_check_auth(sym_cap,
					auth->auth.key.length,
					auth->auth.digest_length,
					auth->auth.iv.length) == 0)
				return 0;
		}
	}

	return -ENOTSUP;
}
