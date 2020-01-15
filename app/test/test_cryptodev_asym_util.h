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
	if (memcmp(mod_inv, result_op->asym->modinv.result.data,
				result_op->asym->modinv.result.length))
		return -1;
	return 0;
}

static inline int verify_modexp(uint8_t *mod_exp,
		struct rte_crypto_op *result_op)
{
	if (memcmp(mod_exp, result_op->asym->modex.result.data,
				result_op->asym->modex.result.length))
		return -1;
	return 0;
}

static inline int verify_ecdsa_sign(uint8_t *sign_r,
		uint8_t *sign_s, struct rte_crypto_op *result_op)
{
	if (memcmp(sign_r, result_op->asym->ecdsa.r.data,
		   result_op->asym->ecdsa.r.length) ||
		   memcmp(sign_s, result_op->asym->ecdsa.s.data,
		   result_op->asym->ecdsa.s.length))
		return -1;
	return 0;
}

static inline int verify_ecpm(uint8_t *result_x, uint8_t *result_y,
			      struct rte_crypto_op *result_op)
{
	if (memcmp(result_x, result_op->asym->ecpm.r.x.data,
		   result_op->asym->ecpm.r.x.length) ||
		   memcmp(result_y, result_op->asym->ecpm.r.y.data,
		   result_op->asym->ecpm.r.y.length))
		return -1;

	return 0;
}
#endif /* TEST_CRYPTODEV_ASYM_TEST_UTIL_H__ */
