/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef _AESNI_GCM_OPS_H_
#define _AESNI_GCM_OPS_H_

#ifndef LINUX
#define LINUX
#endif

#include <intel-ipsec-mb.h>

/** Supported vector modes */
enum aesni_gcm_vector_mode {
	RTE_AESNI_GCM_NOT_SUPPORTED = 0,
	RTE_AESNI_GCM_SSE,
	RTE_AESNI_GCM_AVX,
	RTE_AESNI_GCM_AVX2,
	RTE_AESNI_GCM_AVX512,
	RTE_AESNI_GCM_VECTOR_NUM
};

enum aesni_gcm_key {
	GCM_KEY_128 = 0,
	GCM_KEY_192,
	GCM_KEY_256,
	GCM_KEY_NUM
};

typedef void (*aesni_gcm_t)(const struct gcm_key_data *gcm_key_data,
		struct gcm_context_data *gcm_ctx_data, uint8_t *out,
		const uint8_t *in, uint64_t plaintext_len, const uint8_t *iv,
		const uint8_t *aad, uint64_t aad_len,
		uint8_t *auth_tag, uint64_t auth_tag_len);

typedef void (*aesni_gcm_pre_t)(const void *key, struct gcm_key_data *gcm_data);

typedef void (*aesni_gcm_init_t)(const struct gcm_key_data *gcm_key_data,
		struct gcm_context_data *gcm_ctx_data,
		const uint8_t *iv,
		uint8_t const *aad,
		uint64_t aad_len);

typedef void (*aesni_gcm_update_t)(const struct gcm_key_data *gcm_key_data,
		struct gcm_context_data *gcm_ctx_data,
		uint8_t *out,
		const uint8_t *in,
		uint64_t plaintext_len);

typedef void (*aesni_gcm_finalize_t)(const struct gcm_key_data *gcm_key_data,
		struct gcm_context_data *gcm_ctx_data,
		uint8_t *auth_tag,
		uint64_t auth_tag_len);

#if IMB_VERSION(0, 54, 0) < IMB_VERSION_NUM
typedef void (*aesni_gmac_init_t)(const struct gcm_key_data *gcm_key_data,
		struct gcm_context_data *gcm_ctx_data,
		const uint8_t *iv,
		const uint64_t iv_len);

typedef void (*aesni_gmac_update_t)(const struct gcm_key_data *gcm_key_data,
		struct gcm_context_data *gcm_ctx_data,
		const uint8_t *in,
		const uint64_t plaintext_len);

typedef void (*aesni_gmac_finalize_t)(const struct gcm_key_data *gcm_key_data,
		struct gcm_context_data *gcm_ctx_data,
		uint8_t *auth_tag,
		const uint64_t auth_tag_len);
#endif

/** GCM library function pointer table */
struct aesni_gcm_ops {
	aesni_gcm_t enc;        /**< GCM encode function pointer */
	aesni_gcm_t dec;        /**< GCM decode function pointer */
	aesni_gcm_pre_t pre;    /**< GCM pre-compute */
	aesni_gcm_init_t init;
	aesni_gcm_update_t update_enc;
	aesni_gcm_update_t update_dec;
	aesni_gcm_finalize_t finalize_enc;
	aesni_gcm_finalize_t finalize_dec;
#if IMB_VERSION(0, 54, 0) < IMB_VERSION_NUM
	aesni_gmac_init_t gmac_init;
	aesni_gmac_update_t gmac_update;
	aesni_gmac_finalize_t gmac_finalize;
#endif
};

/** GCM per-session operation handlers */
struct aesni_gcm_session_ops {
	aesni_gcm_t cipher;
	aesni_gcm_pre_t pre;
	aesni_gcm_init_t init;
	aesni_gcm_update_t update;
	aesni_gcm_finalize_t finalize;
#if IMB_VERSION(0, 54, 0) < IMB_VERSION_NUM
	aesni_gmac_init_t gmac_init;
	aesni_gmac_update_t gmac_update;
	aesni_gmac_finalize_t gmac_finalize;
#endif
};

#endif /* _AESNI_GCM_OPS_H_ */
