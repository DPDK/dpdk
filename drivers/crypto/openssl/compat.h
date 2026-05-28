/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium Networks
 */

#ifndef __RTA_COMPAT_H__
#define __RTA_COMPAT_H__

static __rte_always_inline void
free_hmac_ctx(EVP_MAC_CTX *ctx)
{
	EVP_MAC_CTX_free(ctx);
}

static __rte_always_inline void
free_cmac_ctx(EVP_MAC_CTX *ctx)
{
	EVP_MAC_CTX_free(ctx);
}

static __rte_always_inline void
set_dsa_sign(DSA_SIG *sign, BIGNUM *r, BIGNUM *s)
{
	DSA_SIG_set0(sign, r, s);
}

static __rte_always_inline void
get_dsa_sign(DSA_SIG *sign, const BIGNUM **r, const BIGNUM **s)
{
	DSA_SIG_get0(sign, r, s);
}

#endif /* __RTA_COMPAT_H__ */
