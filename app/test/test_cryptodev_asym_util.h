/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium Networks
 */

#ifndef TEST_CRYPTODEV_ASYM_TEST_UTIL_H__
#define TEST_CRYPTODEV_ASYM_TEST_UTIL_H__

/* Below Apis compare resulted buffer to original test vector */

static inline int rsa_verify(struct rsa_test_data *rsa_param,
		struct rte_crypto_op *result_op)
{
	if (memcmp(rsa_param->data,
				result_op->asym->rsa.message.data,
				result_op->asym->rsa.message.length))
		return -1;
	return 0;
}

static inline int verify_modinv(uint8_t *mod_inv,
		struct rte_crypto_op *result_op)
{
	if (memcmp(mod_inv, result_op->asym->modinv.base.data,
				result_op->asym->modinv.base.length))
		return -1;
	return 0;
}

static inline int verify_modexp(uint8_t *mod_exp,
		struct rte_crypto_op *result_op)
{
	if (memcmp(mod_exp, result_op->asym->modex.base.data,
				result_op->asym->modex.base.length))
		return -1;
	return 0;
}

#endif /* TEST_CRYPTODEV_ASYM_TEST_UTIL_H__ */




