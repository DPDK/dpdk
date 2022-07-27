/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2021 Intel Corporation
 */

#ifndef _PMD_AESNI_GCM_PRIV_H_
#define _PMD_AESNI_GCM_PRIV_H_

#include "ipsec_mb_private.h"

#define AESNI_GCM_IV_LENGTH 12

static const struct rte_cryptodev_capabilities aesni_gcm_capabilities[] = {
	{	/* AES GMAC (AUTH) */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
			{.auth = {
				.algo = RTE_CRYPTO_AUTH_AES_GMAC,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.digest_size = {
					.min = 1,
					.max = 16,
					.increment = 1
				},
				.iv_size = {
					.min = AESNI_GCM_IV_LENGTH,
					.max = AESNI_GCM_IV_LENGTH,
					.increment = 0
				}
			}, }
		}, }
	},
	{	/* AES GCM */
		.op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		{.sym = {
			.xform_type = RTE_CRYPTO_SYM_XFORM_AEAD,
			{.aead = {
				.algo = RTE_CRYPTO_AEAD_AES_GCM,
				.block_size = 16,
				.key_size = {
					.min = 16,
					.max = 32,
					.increment = 8
				},
				.digest_size = {
					.min = 1,
					.max = 16,
					.increment = 1
				},
				.aad_size = {
					.min = 0,
					.max = 65535,
					.increment = 1
				},
				.iv_size = {
					.min = AESNI_GCM_IV_LENGTH,
					.max = AESNI_GCM_IV_LENGTH,
					.increment = 0
				}
			}, }
		}, }
	},
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

uint8_t pmd_driver_id_aesni_gcm;

enum aesni_gcm_key_length {
	GCM_KEY_128 = 0,
	GCM_KEY_192,
	GCM_KEY_256,
	GCM_NUM_KEY_TYPES
};

typedef void (*aesni_gcm_t)(const struct gcm_key_data *gcm_key_data,
			    struct gcm_context_data *gcm_ctx_data,
			    uint8_t *out, const uint8_t *in,
			    uint64_t plaintext_len, const uint8_t *iv,
			    const uint8_t *aad, uint64_t aad_len,
			    uint8_t *auth_tag, uint64_t auth_tag_len);

typedef void (*aesni_gcm_pre_t)(const void *key,
				struct gcm_key_data *gcm_data);

typedef void (*aesni_gcm_init_t)(const struct gcm_key_data *gcm_key_data,
				 struct gcm_context_data *gcm_ctx_data,
				 const uint8_t *iv, uint8_t const *aad,
				 uint64_t aad_len);

typedef void (*aesni_gcm_update_t)(const struct gcm_key_data *gcm_key_data,
				   struct gcm_context_data *gcm_ctx_data,
				   uint8_t *out, const uint8_t *in,
				   uint64_t plaintext_len);

typedef void (*aesni_gcm_finalize_t)(const struct gcm_key_data *gcm_key_data,
				     struct gcm_context_data *gcm_ctx_data,
				     uint8_t *auth_tag, uint64_t auth_tag_len);

typedef void (*aesni_gmac_init_t)(const struct gcm_key_data *gcm_key_data,
				  struct gcm_context_data *gcm_ctx_data,
				  const uint8_t *iv, const uint64_t iv_len);

typedef void (*aesni_gmac_update_t)(const struct gcm_key_data *gcm_key_data,
				    struct gcm_context_data *gcm_ctx_data,
				    const uint8_t *in,
				    const uint64_t plaintext_len);

typedef void (*aesni_gmac_finalize_t)(const struct gcm_key_data *gcm_key_data,
				      struct gcm_context_data *gcm_ctx_data,
				      uint8_t *auth_tag,
				      const uint64_t auth_tag_len);

/** GCM operation handlers */
struct aesni_gcm_ops {
	aesni_gcm_t enc;
	aesni_gcm_t dec;
	aesni_gcm_pre_t pre;
	aesni_gcm_init_t init;
	aesni_gcm_update_t update_enc;
	aesni_gcm_update_t update_dec;
	aesni_gcm_finalize_t finalize_enc;
	aesni_gcm_finalize_t finalize_dec;
	aesni_gmac_init_t gmac_init;
	aesni_gmac_update_t gmac_update;
	aesni_gmac_finalize_t gmac_finalize;
};

RTE_DEFINE_PER_LCORE(struct aesni_gcm_ops[GCM_NUM_KEY_TYPES], gcm_ops);

struct aesni_gcm_qp_data {
	struct gcm_context_data gcm_ctx_data;
	uint8_t temp_digest[DIGEST_LENGTH_MAX];
	/* *< Buffers used to store the digest generated
	 * by the driver when verifying a digest provided
	 * by the user (using authentication verify operation)
	 */
	struct aesni_gcm_ops ops[GCM_NUM_KEY_TYPES];
	/**< Operation Handlers */
};

/** AESNI GCM private session structure */
struct aesni_gcm_session {
	struct {
		uint16_t length;
		uint16_t offset;
	} iv;
	/**< IV parameters */
	uint16_t aad_length;
	/**< AAD length */
	uint16_t req_digest_length;
	/**< Requested digest length */
	uint16_t gen_digest_length;
	/**< Generated digest length */
	enum ipsec_mb_operation op;
	/**< GCM operation type */
	struct gcm_key_data gdata_key;
	/**< GCM parameters */
	enum aesni_gcm_key_length key_length;
	/** Key Length */
};

#endif /* _PMD_AESNI_GCM_PRIV_H_ */
