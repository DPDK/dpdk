/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium Networks
 */

#ifndef __RTA_COMPAT_H__
#define __RTA_COMPAT_H__

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)

#define set_rsa_params(rsa, p, q, ret) \
	do {rsa->p = p; rsa->q = q; ret = 0; } while (0)

#define set_rsa_crt_params(rsa, dmp1, dmq1, iqmp, ret) \
	do { \
		rsa->dmp1 = dmp1; \
		rsa->dmq1 = dmq1; \
		rsa->iqmp = iqmp; \
		ret = 0; \
	} while (0)

#define set_rsa_keys(rsa, n, e, d, ret) \
	do { \
		rsa->n = n; rsa->e = e; rsa->d = d; ret = 0; \
	} while (0)

#define set_dh_params(dh, p, g, ret) \
	do { \
		dh->p = p; \
		dh->q = NULL; \
		dh->g = g; \
		ret = 0; \
	} while (0)

#define set_dh_priv_key(dh, priv_key, ret) \
	do { dh->priv_key = priv_key; ret = 0; } while (0)

#define set_dsa_params(dsa, p, q, g, ret) \
	do { dsa->p = p; dsa->q = q; dsa->g = g; ret = 0; } while (0)

#define get_dh_pub_key(dh, pub_key) \
	(pub_key = dh->pub_key)

#define get_dh_priv_key(dh, priv_key) \
	(priv_key = dh->priv_key)

#define set_dsa_sign(sign, r, s) \
	do { sign->r = r; sign->s = s; } while (0)

#define get_dsa_sign(sign, r, s) \
	do { r = sign->r; s = sign->s; } while (0)

#define set_dsa_keys(dsa, pub, priv, ret) \
	do { dsa->pub_key = pub; dsa->priv_key = priv; ret = 0; } while (0)

#define set_dsa_pub_key(dsa, pub_key) \
	(dsa->pub_key = pub_key)

#define get_dsa_priv_key(dsa, priv_key) \
	(priv_key = dsa->priv_key)

#else

#define set_rsa_params(rsa, p, q, ret) \
	(ret = !RSA_set0_factors(rsa, p, q))

#define set_rsa_crt_params(rsa, dmp1, dmq1, iqmp, ret) \
	(ret = !RSA_set0_crt_params(rsa, dmp1, dmq1, iqmp))

/* n, e must be non-null, d can be NULL */
#define set_rsa_keys(rsa, n, e, d, ret) \
	(ret = !RSA_set0_key(rsa, n, e, d))

#define set_dh_params(dh, p, g, ret) \
	(ret = !DH_set0_pqg(dh, p, NULL, g))

#define set_dh_priv_key(dh, priv_key, ret) \
	(ret = !DH_set0_key(dh, NULL, priv_key))

#define get_dh_pub_key(dh, pub_key) \
	(DH_get0_key(dh_key, &pub_key, NULL))

#define get_dh_priv_key(dh, priv_key) \
	(DH_get0_key(dh_key, NULL, &priv_key))

#define set_dsa_params(dsa, p, q, g, ret) \
	(ret = !DSA_set0_pqg(dsa, p, q, g))

#define set_dsa_priv_key(dsa, priv_key) \
	(DSA_set0_key(dsa, NULL, priv_key))

#define set_dsa_sign(sign, r, s) \
	(DSA_SIG_set0(sign, r, s))

#define get_dsa_sign(sign, r, s) \
	(DSA_SIG_get0(sign, &r, &s))

#define set_dsa_keys(dsa, pub, priv, ret) \
	(ret = !DSA_set0_key(dsa, pub, priv))

#define set_dsa_pub_key(dsa, pub_key) \
	(DSA_set0_key(dsa, pub_key, NULL))

#define get_dsa_priv_key(dsa, priv_key) \
	(DSA_get0_key(dsa, NULL, &priv_key))

#endif /* version < 10100000 */

#endif /* __RTA_COMPAT_H__ */
