/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _AESNI_GCM_OPS_H_
#define _AESNI_GCM_OPS_H_

#ifndef LINUX
#define LINUX
#endif

#include <gcm_defines.h>
#include <aux_funcs.h>

/** Supported vector modes */
enum aesni_gcm_vector_mode {
	RTE_AESNI_GCM_NOT_SUPPORTED = 0,
	RTE_AESNI_GCM_SSE,
	RTE_AESNI_GCM_AVX,
	RTE_AESNI_GCM_AVX2
};

typedef void (*aes_keyexp_128_enc_t)(void *key, void *enc_exp_keys);

typedef void (*aesni_gcm_t)(gcm_data *my_ctx_data, u8 *out, const u8 *in,
		u64 plaintext_len, u8 *iv, const u8 *aad, u64 aad_len,
		u8 *auth_tag, u64 auth_tag_len);

typedef void (*aesni_gcm_precomp_t)(gcm_data *my_ctx_data, u8 *hash_subkey);

/** GCM library function pointer table */
struct aesni_gcm_ops {
	struct {
		struct {
			aes_keyexp_128_enc_t aes128_enc;
			/**< AES128 enc key expansion */
		} keyexp;
		/**< Key expansion functions */
	} aux; /**< Auxiliary functions */

	struct {
		aesni_gcm_t enc;	/**< GCM encode function pointer */
		aesni_gcm_t dec;	/**< GCM decode function pointer */
		aesni_gcm_precomp_t precomp;	/**< GCM pre-compute */
	} gcm; /**< GCM functions */
};


static const struct aesni_gcm_ops gcm_ops[] = {
	[RTE_AESNI_GCM_NOT_SUPPORTED] = {
		.aux = {
			.keyexp = {
				NULL
			}
		},
		.gcm = {
			NULL
		}
	},
	[RTE_AESNI_GCM_SSE] = {
		.aux = {
			.keyexp = {
				aes_keyexp_128_enc_sse
			}
		},
		.gcm = {
			aesni_gcm_enc_sse,
			aesni_gcm_dec_sse,
			aesni_gcm_precomp_sse
		}
	},
	[RTE_AESNI_GCM_AVX] = {
		.aux = {
			.keyexp = {
				aes_keyexp_128_enc_avx,
			}
		},
		.gcm = {
			aesni_gcm_enc_avx_gen2,
			aesni_gcm_dec_avx_gen2,
			aesni_gcm_precomp_avx_gen2
		}
	},
	[RTE_AESNI_GCM_AVX2] = {
		.aux = {
			.keyexp = {
				aes_keyexp_128_enc_avx2,
			}
		},
		.gcm = {
			aesni_gcm_enc_avx_gen4,
			aesni_gcm_dec_avx_gen4,
			aesni_gcm_precomp_avx_gen4
		}
	}
};


#endif /* _AESNI_GCM_OPS_H_ */
