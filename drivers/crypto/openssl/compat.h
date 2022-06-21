/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium Networks
 */

#ifndef __RTA_COMPAT_H__
#define __RTA_COMPAT_H__

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)

static __rte_always_inline int
set_rsa_params(RSA *rsa, BIGNUM *p, BIGNUM *q)
{
	rsa->p = p;
	rsa->q = q;
	return 0;
}

static __rte_always_inline int
set_rsa_crt_params(RSA *rsa, BIGNUM *dmp1, BIGNUM *dmq1, BIGNUM *iqmp)
{
	rsa->dmp1 = dmp1;
	rsa->dmq1 = dmq1;
	rsa->iqmp = iqmp;
	return 0;
}

static __rte_always_inline int
set_rsa_keys(RSA *rsa, BIGNUM *n, BIGNUM *e, BIGNUM *d)
{
	rsa->n = n;
	rsa->e = e;
	rsa->d = d;
	return 0;
}

static __rte_always_inline int
set_dh_params(DH *dh, BIGNUM *p, BIGNUM *g)
{
	dh->p = p;
	dh->q = NULL;
	dh->g = g;
	return 0;
}

static __rte_always_inline int
set_dh_priv_key(DH *dh, BIGNUM *priv_key)
{
	dh->priv_key = priv_key;
	return 0;
}

static __rte_always_inline int
set_dsa_params(DSA *dsa, BIGNUM *p, BIGNUM *q, BIGNUM *g)
{
	dsa->p = p;
	dsa->q = q;
	dsa->g = g;
	return 0;
}

static __rte_always_inline void
get_dh_pub_key(DH *dh, const BIGNUM **pub_key)
{
	*pub_key = dh->pub_key;
}

static __rte_always_inline void
get_dh_priv_key(DH *dh, const BIGNUM **priv_key)
{
	*priv_key = dh->priv_key;
}

static __rte_always_inline void
set_dsa_sign(DSA_SIG *sign, BIGNUM *r, BIGNUM *s)
{
	sign->r = r;
	sign->s = s;
}

static __rte_always_inline void
get_dsa_sign(DSA_SIG *sign, const BIGNUM **r, const BIGNUM **s)
{
	*r = sign->r;
	*s = sign->s;
}

static __rte_always_inline int
set_dsa_keys(DSA *dsa, BIGNUM *pub, BIGNUM *priv)
{
	dsa->pub_key = pub;
	dsa->priv_key = priv;
	return 0;
}

static __rte_always_inline void
set_dsa_pub_key(DSA *dsa, BIGNUM *pub)
{
	dsa->pub_key = pub;
}

static __rte_always_inline void
get_dsa_priv_key(DSA *dsa, BIGNUM **priv_key)
{
	*priv_key = dsa->priv_key;
}

#elif (OPENSSL_VERSION_NUMBER >= 0x30000000L)
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
#else

static __rte_always_inline int
set_rsa_params(RSA *rsa, BIGNUM *p, BIGNUM *q)
{
	return !(RSA_set0_factors(rsa, p, q));
}

static __rte_always_inline int
set_rsa_crt_params(RSA *rsa, BIGNUM *dmp1, BIGNUM *dmq1, BIGNUM *iqmp)
{
	return !(RSA_set0_crt_params(rsa, dmp1, dmq1, iqmp));
}

/* n, e must be non-null, d can be NULL */

static __rte_always_inline  int
set_rsa_keys(RSA *rsa, BIGNUM *n, BIGNUM *e, BIGNUM *d)
{
	return !(RSA_set0_key(rsa, n, e, d));
}

static __rte_always_inline int
set_dh_params(DH *dh, BIGNUM *p, BIGNUM *g)
{
	return !(DH_set0_pqg(dh, p, NULL, g));
}

static __rte_always_inline int
set_dh_priv_key(DH *dh, BIGNUM *priv_key)
{
	return !(DH_set0_key(dh, NULL, priv_key));
}

static __rte_always_inline void
get_dh_pub_key(DH *dh_key, const BIGNUM **pub_key)
{
	DH_get0_key(dh_key, pub_key, NULL);
}

static __rte_always_inline void
get_dh_priv_key(DH *dh_key, const BIGNUM **priv_key)
{
	DH_get0_key(dh_key, NULL, priv_key);
}

static __rte_always_inline int
set_dsa_params(DSA *dsa, BIGNUM *p, BIGNUM *q, BIGNUM *g)
{
	return !(DSA_set0_pqg(dsa, p, q, g));
}

static __rte_always_inline void
set_dsa_priv_key(DSA *dsa, BIGNUM *priv_key)
{
	DSA_set0_key(dsa, NULL, priv_key);
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

static __rte_always_inline int
set_dsa_keys(DSA *dsa, BIGNUM *pub, BIGNUM *priv)
{
	return !(DSA_set0_key(dsa, pub, priv));
}

static __rte_always_inline void
set_dsa_pub_key(DSA *dsa, BIGNUM *pub_key)
{
	DSA_set0_key(dsa, pub_key, NULL);
}

static __rte_always_inline void
get_dsa_priv_key(DSA *dsa, const BIGNUM **priv_key)
{
	DSA_get0_key(dsa, NULL, priv_key);
}

#endif /* version < 10100000 */

#endif /* __RTA_COMPAT_H__ */
