/*
 *  This file is provided under a dual BSD/GPLv2 license.  When using or
 *  redistributing this file, you may do so under either license.
 *
 *  GPL LICENSE SUMMARY
 *  Copyright(c) 2015-2016 Intel Corporation.
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  Contact Information:
 *  qat-linux@intel.com
 *
 *  BSD LICENSE
 *  Copyright(c) 2015-2016 Intel Corporation.
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *    * Neither the name of Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _ICP_QAT_ALGS_H_
#define _ICP_QAT_ALGS_H_
#include <rte_memory.h>
#include "icp_qat_hw.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_la.h"

#define QAT_AES_HW_CONFIG_CBC_ENC(alg) \
	ICP_QAT_HW_CIPHER_CONFIG_BUILD(ICP_QAT_HW_CIPHER_CBC_MODE, alg, \
					ICP_QAT_HW_CIPHER_NO_CONVERT, \
					ICP_QAT_HW_CIPHER_ENCRYPT)

#define QAT_AES_HW_CONFIG_CBC_DEC(alg) \
	ICP_QAT_HW_CIPHER_CONFIG_BUILD(ICP_QAT_HW_CIPHER_CBC_MODE, alg, \
					ICP_QAT_HW_CIPHER_KEY_CONVERT, \
					ICP_QAT_HW_CIPHER_DECRYPT)

struct qat_alg_buf {
	uint32_t len;
	uint32_t resrvd;
	uint64_t addr;
} __rte_packed;

struct qat_alg_buf_list {
	uint64_t resrvd;
	uint32_t num_bufs;
	uint32_t num_mapped_bufs;
	struct qat_alg_buf bufers[];
} __rte_packed __rte_cache_aligned;

/* Common content descriptor */
struct qat_alg_cd {
	struct icp_qat_hw_cipher_algo_blk cipher;
	struct icp_qat_hw_auth_algo_blk hash;
} __rte_packed __rte_cache_aligned;

struct qat_session {
	enum icp_qat_fw_la_cmd_id qat_cmd;
	enum icp_qat_hw_cipher_algo qat_cipher_alg;
	enum icp_qat_hw_cipher_dir qat_dir;
	enum icp_qat_hw_cipher_mode qat_mode;
	enum icp_qat_hw_auth_algo qat_hash_alg;
	struct qat_alg_cd cd;
	phys_addr_t cd_paddr;
	struct icp_qat_fw_la_bulk_req fw_req;
	struct qat_crypto_instance *inst;
	uint8_t salt[ICP_QAT_HW_AES_BLK_SZ];
	rte_spinlock_t lock;	/* protects this struct */
};

struct qat_alg_ablkcipher_cd {
	struct icp_qat_hw_cipher_algo_blk *cd;
	phys_addr_t cd_paddr;
	struct icp_qat_fw_la_bulk_req fw_req;
	struct qat_crypto_instance *inst;
	rte_spinlock_t lock;	/* protects this struct */
};

int qat_get_inter_state_size(enum icp_qat_hw_auth_algo qat_hash_alg);

int qat_alg_aead_session_create_content_desc_cipher(struct qat_session *cd,
						uint8_t *enckey,
						uint32_t enckeylen);

int qat_alg_aead_session_create_content_desc_auth(struct qat_session *cdesc,
						uint8_t *authkey,
						uint32_t authkeylen,
						uint32_t add_auth_data_length,
						uint32_t digestsize,
						unsigned int operation);

void qat_alg_init_common_hdr(struct icp_qat_fw_comn_req_hdr *header);

void qat_alg_ablkcipher_init_enc(struct qat_alg_ablkcipher_cd *cd,
					int alg, const uint8_t *key,
					unsigned int keylen);

void qat_alg_ablkcipher_init_dec(struct qat_alg_ablkcipher_cd *cd,
					int alg, const uint8_t *key,
					unsigned int keylen);

int qat_alg_validate_aes_key(int key_len, enum icp_qat_hw_cipher_algo *alg);
int qat_alg_validate_snow3g_key(int key_len, enum icp_qat_hw_cipher_algo *alg);

#endif
