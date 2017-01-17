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

#include <isa-l_crypto/aes_gcm.h>

typedef void (*aesni_gcm_init_t)(struct gcm_data *my_ctx_data,
		uint8_t *iv,
		uint8_t const *aad,
		uint64_t aad_len);

typedef void (*aesni_gcm_update_t)(struct gcm_data *my_ctx_data,
		uint8_t *out,
		const uint8_t *in,
		uint64_t plaintext_len);

typedef void (*aesni_gcm_finalize_t)(struct gcm_data *my_ctx_data,
		uint8_t *auth_tag,
		uint64_t auth_tag_len);

struct aesni_gcm_ops {
	aesni_gcm_init_t init;
	aesni_gcm_update_t update;
	aesni_gcm_finalize_t finalize;
};

#endif /* _AESNI_GCM_OPS_H_ */
