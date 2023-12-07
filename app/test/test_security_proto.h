/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef _TEST_SECURITY_PROTO_H_
#define _TEST_SECURITY_PROTO_H_

#include <rte_cryptodev.h>
#include <rte_security.h>

int test_sec_crypto_caps_aead_verify(const struct rte_security_capability *sec_cap,
		struct rte_crypto_sym_xform *aead);

int test_sec_crypto_caps_cipher_verify(const struct rte_security_capability *sec_cap,
		struct rte_crypto_sym_xform *cipher);

int test_sec_crypto_caps_auth_verify(const struct rte_security_capability *sec_cap,
		struct rte_crypto_sym_xform *auth);

#endif
