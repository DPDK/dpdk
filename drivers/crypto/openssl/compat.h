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

#else

#define set_rsa_params(rsa, p, q, ret) \
	(ret = !RSA_set0_factors(rsa, p, q))

#define set_rsa_crt_params(rsa, dmp1, dmq1, iqmp, ret) \
	(ret = !RSA_set0_crt_params(rsa, dmp1, dmq1, iqmp))

/* n, e must be non-null, d can be NULL */
#define set_rsa_keys(rsa, n, e, d, ret) \
	(ret = !RSA_set0_key(rsa, n, e, d))

#endif /* version < 10100000 */

#endif /* __RTA_COMPAT_H__ */
