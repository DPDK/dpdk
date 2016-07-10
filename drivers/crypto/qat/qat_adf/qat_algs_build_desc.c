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
 *	* Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *	* Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in
 *	  the documentation and/or other materials provided with the
 *	  distribution.
 *	* Neither the name of Intel Corporation nor the names of its
 *	  contributors may be used to endorse or promote products derived
 *	  from this software without specific prior written permission.
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

#include <rte_memcpy.h>
#include <rte_common.h>
#include <rte_spinlock.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_crypto_sym.h>

#include "../qat_logs.h"
#include "qat_algs.h"

#include <openssl/sha.h>	/* Needed to calculate pre-compute values */
#include <openssl/aes.h>	/* Needed to calculate pre-compute values */


/*
 * Returns size in bytes per hash algo for state1 size field in cd_ctrl
 * This is digest size rounded up to nearest quadword
 */
static int qat_hash_get_state1_size(enum icp_qat_hw_auth_algo qat_hash_alg)
{
	switch (qat_hash_alg) {
	case ICP_QAT_HW_AUTH_ALGO_SHA1:
		return QAT_HW_ROUND_UP(ICP_QAT_HW_SHA1_STATE1_SZ,
						QAT_HW_DEFAULT_ALIGNMENT);
	case ICP_QAT_HW_AUTH_ALGO_SHA256:
		return QAT_HW_ROUND_UP(ICP_QAT_HW_SHA256_STATE1_SZ,
						QAT_HW_DEFAULT_ALIGNMENT);
	case ICP_QAT_HW_AUTH_ALGO_SHA512:
		return QAT_HW_ROUND_UP(ICP_QAT_HW_SHA512_STATE1_SZ,
						QAT_HW_DEFAULT_ALIGNMENT);
	case ICP_QAT_HW_AUTH_ALGO_AES_XCBC_MAC:
		return QAT_HW_ROUND_UP(ICP_QAT_HW_AES_XCBC_MAC_STATE1_SZ,
						QAT_HW_DEFAULT_ALIGNMENT);
	case ICP_QAT_HW_AUTH_ALGO_GALOIS_128:
	case ICP_QAT_HW_AUTH_ALGO_GALOIS_64:
		return QAT_HW_ROUND_UP(ICP_QAT_HW_GALOIS_128_STATE1_SZ,
						QAT_HW_DEFAULT_ALIGNMENT);
	case ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2:
		return QAT_HW_ROUND_UP(ICP_QAT_HW_SNOW_3G_UIA2_STATE1_SZ,
						QAT_HW_DEFAULT_ALIGNMENT);
	case ICP_QAT_HW_AUTH_ALGO_DELIMITER:
		/* return maximum state1 size in this case */
		return QAT_HW_ROUND_UP(ICP_QAT_HW_SHA512_STATE1_SZ,
						QAT_HW_DEFAULT_ALIGNMENT);
	default:
		PMD_DRV_LOG(ERR, "invalid hash alg %u", qat_hash_alg);
		return -EFAULT;
	};
	return -EFAULT;
}

/* returns digest size in bytes  per hash algo */
static int qat_hash_get_digest_size(enum icp_qat_hw_auth_algo qat_hash_alg)
{
	switch (qat_hash_alg) {
	case ICP_QAT_HW_AUTH_ALGO_SHA1:
		return ICP_QAT_HW_SHA1_STATE1_SZ;
	case ICP_QAT_HW_AUTH_ALGO_SHA256:
		return ICP_QAT_HW_SHA256_STATE1_SZ;
	case ICP_QAT_HW_AUTH_ALGO_SHA512:
		return ICP_QAT_HW_SHA512_STATE1_SZ;
	case ICP_QAT_HW_AUTH_ALGO_DELIMITER:
		/* return maximum digest size in this case */
		return ICP_QAT_HW_SHA512_STATE1_SZ;
	default:
		PMD_DRV_LOG(ERR, "invalid hash alg %u", qat_hash_alg);
		return -EFAULT;
	};
	return -EFAULT;
}

/* returns block size in byes per hash algo */
static int qat_hash_get_block_size(enum icp_qat_hw_auth_algo qat_hash_alg)
{
	switch (qat_hash_alg) {
	case ICP_QAT_HW_AUTH_ALGO_SHA1:
		return SHA_CBLOCK;
	case ICP_QAT_HW_AUTH_ALGO_SHA256:
		return SHA256_CBLOCK;
	case ICP_QAT_HW_AUTH_ALGO_SHA512:
		return SHA512_CBLOCK;
	case ICP_QAT_HW_AUTH_ALGO_GALOIS_128:
		return 16;
	case ICP_QAT_HW_AUTH_ALGO_DELIMITER:
		/* return maximum block size in this case */
		return SHA512_CBLOCK;
	default:
		PMD_DRV_LOG(ERR, "invalid hash alg %u", qat_hash_alg);
		return -EFAULT;
	};
	return -EFAULT;
}

static int partial_hash_sha1(uint8_t *data_in, uint8_t *data_out)
{
	SHA_CTX ctx;

	if (!SHA1_Init(&ctx))
		return -EFAULT;
	SHA1_Transform(&ctx, data_in);
	rte_memcpy(data_out, &ctx, SHA_DIGEST_LENGTH);
	return 0;
}

static int partial_hash_sha256(uint8_t *data_in, uint8_t *data_out)
{
	SHA256_CTX ctx;

	if (!SHA256_Init(&ctx))
		return -EFAULT;
	SHA256_Transform(&ctx, data_in);
	rte_memcpy(data_out, &ctx, SHA256_DIGEST_LENGTH);
	return 0;
}

static int partial_hash_sha512(uint8_t *data_in, uint8_t *data_out)
{
	SHA512_CTX ctx;

	if (!SHA512_Init(&ctx))
		return -EFAULT;
	SHA512_Transform(&ctx, data_in);
	rte_memcpy(data_out, &ctx, SHA512_DIGEST_LENGTH);
	return 0;
}

static int partial_hash_compute(enum icp_qat_hw_auth_algo hash_alg,
			uint8_t *data_in,
			uint8_t *data_out)
{
	int digest_size;
	uint8_t digest[qat_hash_get_digest_size(
			ICP_QAT_HW_AUTH_ALGO_DELIMITER)];
	uint32_t *hash_state_out_be32;
	uint64_t *hash_state_out_be64;
	int i;

	PMD_INIT_FUNC_TRACE();
	digest_size = qat_hash_get_digest_size(hash_alg);
	if (digest_size <= 0)
		return -EFAULT;

	hash_state_out_be32 = (uint32_t *)data_out;
	hash_state_out_be64 = (uint64_t *)data_out;

	switch (hash_alg) {
	case ICP_QAT_HW_AUTH_ALGO_SHA1:
		if (partial_hash_sha1(data_in, digest))
			return -EFAULT;
		for (i = 0; i < digest_size >> 2; i++, hash_state_out_be32++)
			*hash_state_out_be32 =
				rte_bswap32(*(((uint32_t *)digest)+i));
		break;
	case ICP_QAT_HW_AUTH_ALGO_SHA256:
		if (partial_hash_sha256(data_in, digest))
			return -EFAULT;
		for (i = 0; i < digest_size >> 2; i++, hash_state_out_be32++)
			*hash_state_out_be32 =
				rte_bswap32(*(((uint32_t *)digest)+i));
		break;
	case ICP_QAT_HW_AUTH_ALGO_SHA512:
		if (partial_hash_sha512(data_in, digest))
			return -EFAULT;
		for (i = 0; i < digest_size >> 3; i++, hash_state_out_be64++)
			*hash_state_out_be64 =
				rte_bswap64(*(((uint64_t *)digest)+i));
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid hash alg %u", hash_alg);
		return -EFAULT;
	}

	return 0;
}
#define HMAC_IPAD_VALUE	0x36
#define HMAC_OPAD_VALUE	0x5c
#define HASH_XCBC_PRECOMP_KEY_NUM 3

static int qat_alg_do_precomputes(enum icp_qat_hw_auth_algo hash_alg,
				const uint8_t *auth_key,
				uint16_t auth_keylen,
				uint8_t *p_state_buf,
				uint16_t *p_state_len)
{
	int block_size;
	uint8_t ipad[qat_hash_get_block_size(ICP_QAT_HW_AUTH_ALGO_DELIMITER)];
	uint8_t opad[qat_hash_get_block_size(ICP_QAT_HW_AUTH_ALGO_DELIMITER)];
	int i;

	PMD_INIT_FUNC_TRACE();
	if (hash_alg == ICP_QAT_HW_AUTH_ALGO_AES_XCBC_MAC) {
		static uint8_t qat_aes_xcbc_key_seed[
					ICP_QAT_HW_AES_XCBC_MAC_STATE2_SZ] = {
			0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
			0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
			0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
			0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
			0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
			0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
		};

		uint8_t *in = NULL;
		uint8_t *out = p_state_buf;
		int x;
		AES_KEY enc_key;

		in = rte_zmalloc("working mem for key",
				ICP_QAT_HW_AES_XCBC_MAC_STATE2_SZ, 16);
		rte_memcpy(in, qat_aes_xcbc_key_seed,
				ICP_QAT_HW_AES_XCBC_MAC_STATE2_SZ);
		for (x = 0; x < HASH_XCBC_PRECOMP_KEY_NUM; x++) {
			if (AES_set_encrypt_key(auth_key, auth_keylen << 3,
				&enc_key) != 0) {
				rte_free(in -
					(x * ICP_QAT_HW_AES_XCBC_MAC_KEY_SZ));
				memset(out -
					(x * ICP_QAT_HW_AES_XCBC_MAC_KEY_SZ),
					0, ICP_QAT_HW_AES_XCBC_MAC_STATE2_SZ);
				return -EFAULT;
			}
			AES_encrypt(in, out, &enc_key);
			in += ICP_QAT_HW_AES_XCBC_MAC_KEY_SZ;
			out += ICP_QAT_HW_AES_XCBC_MAC_KEY_SZ;
		}
		*p_state_len = ICP_QAT_HW_AES_XCBC_MAC_STATE2_SZ;
		rte_free(in - x*ICP_QAT_HW_AES_XCBC_MAC_KEY_SZ);
		return 0;
	} else if ((hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_128) ||
		(hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_64)) {
		uint8_t *in = NULL;
		uint8_t *out = p_state_buf;
		AES_KEY enc_key;

		memset(p_state_buf, 0, ICP_QAT_HW_GALOIS_H_SZ +
				ICP_QAT_HW_GALOIS_LEN_A_SZ +
				ICP_QAT_HW_GALOIS_E_CTR0_SZ);
		in = rte_zmalloc("working mem for key",
				ICP_QAT_HW_GALOIS_H_SZ, 16);
		memset(in, 0, ICP_QAT_HW_GALOIS_H_SZ);
		if (AES_set_encrypt_key(auth_key, auth_keylen << 3,
			&enc_key) != 0) {
			return -EFAULT;
		}
		AES_encrypt(in, out, &enc_key);
		*p_state_len = ICP_QAT_HW_GALOIS_H_SZ +
				ICP_QAT_HW_GALOIS_LEN_A_SZ +
				ICP_QAT_HW_GALOIS_E_CTR0_SZ;
		rte_free(in);
		return 0;
	}

	block_size = qat_hash_get_block_size(hash_alg);
	if (block_size <= 0)
		return -EFAULT;
	/* init ipad and opad from key and xor with fixed values */
	memset(ipad, 0, block_size);
	memset(opad, 0, block_size);

	if (auth_keylen > (unsigned int)block_size) {
		PMD_DRV_LOG(ERR, "invalid keylen %u", auth_keylen);
		return -EFAULT;
	}
	rte_memcpy(ipad, auth_key, auth_keylen);
	rte_memcpy(opad, auth_key, auth_keylen);

	for (i = 0; i < block_size; i++) {
		uint8_t *ipad_ptr = ipad + i;
		uint8_t *opad_ptr = opad + i;
		*ipad_ptr ^= HMAC_IPAD_VALUE;
		*opad_ptr ^= HMAC_OPAD_VALUE;
	}

	/* do partial hash of ipad and copy to state1 */
	if (partial_hash_compute(hash_alg, ipad, p_state_buf)) {
		memset(ipad, 0, block_size);
		memset(opad, 0, block_size);
		PMD_DRV_LOG(ERR, "ipad precompute failed");
		return -EFAULT;
	}

	/*
	 * State len is a multiple of 8, so may be larger than the digest.
	 * Put the partial hash of opad state_len bytes after state1
	 */
	*p_state_len = qat_hash_get_state1_size(hash_alg);
	if (partial_hash_compute(hash_alg, opad, p_state_buf + *p_state_len)) {
		memset(ipad, 0, block_size);
		memset(opad, 0, block_size);
		PMD_DRV_LOG(ERR, "opad precompute failed");
		return -EFAULT;
	}

	/*  don't leave data lying around */
	memset(ipad, 0, block_size);
	memset(opad, 0, block_size);
	return 0;
}

void qat_alg_init_common_hdr(struct icp_qat_fw_comn_req_hdr *header)
{
	PMD_INIT_FUNC_TRACE();
	header->hdr_flags =
		ICP_QAT_FW_COMN_HDR_FLAGS_BUILD(ICP_QAT_FW_COMN_REQ_FLAG_SET);
	header->service_type = ICP_QAT_FW_COMN_REQ_CPM_FW_LA;
	header->comn_req_flags =
		ICP_QAT_FW_COMN_FLAGS_BUILD(QAT_COMN_CD_FLD_TYPE_64BIT_ADR,
					QAT_COMN_PTR_TYPE_FLAT);
	ICP_QAT_FW_LA_PARTIAL_SET(header->serv_specif_flags,
				  ICP_QAT_FW_LA_PARTIAL_NONE);
	ICP_QAT_FW_LA_CIPH_IV_FLD_FLAG_SET(header->serv_specif_flags,
					   ICP_QAT_FW_CIPH_IV_16BYTE_DATA);
	ICP_QAT_FW_LA_PROTO_SET(header->serv_specif_flags,
				ICP_QAT_FW_LA_NO_PROTO);
	ICP_QAT_FW_LA_UPDATE_STATE_SET(header->serv_specif_flags,
					   ICP_QAT_FW_LA_NO_UPDATE_STATE);
}

int qat_alg_aead_session_create_content_desc_cipher(struct qat_session *cdesc,
						uint8_t *cipherkey,
						uint32_t cipherkeylen)
{
	struct icp_qat_hw_cipher_algo_blk *cipher;
	struct icp_qat_fw_la_bulk_req *req_tmpl = &cdesc->fw_req;
	struct icp_qat_fw_comn_req_hdr_cd_pars *cd_pars = &req_tmpl->cd_pars;
	struct icp_qat_fw_comn_req_hdr *header = &req_tmpl->comn_hdr;
	void *ptr = &req_tmpl->cd_ctrl;
	struct icp_qat_fw_cipher_cd_ctrl_hdr *cipher_cd_ctrl = ptr;
	struct icp_qat_fw_auth_cd_ctrl_hdr *hash_cd_ctrl = ptr;
	enum icp_qat_hw_cipher_convert key_convert;
	uint16_t proto = ICP_QAT_FW_LA_NO_PROTO;	/* no CCM/GCM/Snow3G */
	uint16_t cipher_offset = 0;

	PMD_INIT_FUNC_TRACE();

	if (cdesc->qat_cmd == ICP_QAT_FW_LA_CMD_HASH_CIPHER &&
		cdesc->qat_hash_alg != ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2) {
		cipher =
		    (struct icp_qat_hw_cipher_algo_blk *)((char *)&cdesc->cd +
				sizeof(struct icp_qat_hw_auth_algo_blk));
		cipher_offset = sizeof(struct icp_qat_hw_auth_algo_blk);
	} else {
		cipher = (struct icp_qat_hw_cipher_algo_blk *)&cdesc->cd;
		cipher_offset = 0;
	}
	/* CD setup */
	if (cdesc->qat_dir == ICP_QAT_HW_CIPHER_ENCRYPT) {
		ICP_QAT_FW_LA_RET_AUTH_SET(header->serv_specif_flags,
					ICP_QAT_FW_LA_RET_AUTH_RES);
		ICP_QAT_FW_LA_CMP_AUTH_SET(header->serv_specif_flags,
					ICP_QAT_FW_LA_NO_CMP_AUTH_RES);
	} else {
		ICP_QAT_FW_LA_RET_AUTH_SET(header->serv_specif_flags,
					ICP_QAT_FW_LA_NO_RET_AUTH_RES);
		ICP_QAT_FW_LA_CMP_AUTH_SET(header->serv_specif_flags,
					ICP_QAT_FW_LA_CMP_AUTH_RES);
	}

	if (cdesc->qat_mode == ICP_QAT_HW_CIPHER_CTR_MODE) {
		/* CTR Streaming ciphers are a special case. Decrypt = encrypt
		 * Overriding default values previously set
		 */
		cdesc->qat_dir = ICP_QAT_HW_CIPHER_ENCRYPT;
		key_convert = ICP_QAT_HW_CIPHER_NO_CONVERT;
	} else if (cdesc->qat_dir == ICP_QAT_HW_CIPHER_ENCRYPT)
		key_convert = ICP_QAT_HW_CIPHER_NO_CONVERT;
	else
		key_convert = ICP_QAT_HW_CIPHER_KEY_CONVERT;

	if (cdesc->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2)
		key_convert = ICP_QAT_HW_CIPHER_KEY_CONVERT;

	/* For Snow3G, set key convert and other bits */
	if (cdesc->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2) {
		key_convert = ICP_QAT_HW_CIPHER_KEY_CONVERT;
		ICP_QAT_FW_LA_RET_AUTH_SET(header->serv_specif_flags,
					ICP_QAT_FW_LA_NO_RET_AUTH_RES);
		if (cdesc->qat_cmd == ICP_QAT_FW_LA_CMD_HASH_CIPHER)  {
			ICP_QAT_FW_LA_RET_AUTH_SET(header->serv_specif_flags,
				ICP_QAT_FW_LA_RET_AUTH_RES);
			ICP_QAT_FW_LA_CMP_AUTH_SET(header->serv_specif_flags,
				ICP_QAT_FW_LA_NO_CMP_AUTH_RES);
		}
	}

	cipher->aes.cipher_config.val =
	    ICP_QAT_HW_CIPHER_CONFIG_BUILD(cdesc->qat_mode,
					cdesc->qat_cipher_alg, key_convert,
					cdesc->qat_dir);
	memcpy(cipher->aes.key, cipherkey, cipherkeylen);

	proto = ICP_QAT_FW_LA_PROTO_GET(header->serv_specif_flags);
	if (cdesc->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2)
		proto = ICP_QAT_FW_LA_SNOW_3G_PROTO;

	/* Request template setup */
	qat_alg_init_common_hdr(header);
	header->service_cmd_id = cdesc->qat_cmd;
	ICP_QAT_FW_LA_DIGEST_IN_BUFFER_SET(header->serv_specif_flags,
					ICP_QAT_FW_LA_NO_DIGEST_IN_BUFFER);
	/* Configure the common header protocol flags */
	ICP_QAT_FW_LA_PROTO_SET(header->serv_specif_flags, proto);
	cd_pars->u.s.content_desc_addr = cdesc->cd_paddr;
	cd_pars->u.s.content_desc_params_sz = sizeof(cdesc->cd) >> 3;

	/* Cipher CD config setup */
	if (cdesc->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2) {
		cipher_cd_ctrl->cipher_key_sz =
			(ICP_QAT_HW_SNOW_3G_UEA2_KEY_SZ +
			ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ) >> 3;
		cipher_cd_ctrl->cipher_state_sz =
			ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ >> 3;
		cipher_cd_ctrl->cipher_cfg_offset = cipher_offset >> 3;
		if (cdesc->qat_cmd == ICP_QAT_FW_LA_CMD_HASH_CIPHER)  {
		ICP_QAT_FW_LA_DIGEST_IN_BUFFER_SET(header->serv_specif_flags,
				ICP_QAT_FW_LA_DIGEST_IN_BUFFER);
		}
	} else {
		cipher_cd_ctrl->cipher_key_sz = cipherkeylen >> 3;
		cipher_cd_ctrl->cipher_state_sz = ICP_QAT_HW_AES_BLK_SZ >> 3;
		cipher_cd_ctrl->cipher_cfg_offset = cipher_offset >> 3;
	}

	if (cdesc->qat_cmd == ICP_QAT_FW_LA_CMD_CIPHER) {
		ICP_QAT_FW_COMN_CURR_ID_SET(cipher_cd_ctrl,
					ICP_QAT_FW_SLICE_CIPHER);
		ICP_QAT_FW_COMN_NEXT_ID_SET(cipher_cd_ctrl,
					ICP_QAT_FW_SLICE_DRAM_WR);
	} else if (cdesc->qat_cmd == ICP_QAT_FW_LA_CMD_CIPHER_HASH) {
		ICP_QAT_FW_COMN_CURR_ID_SET(cipher_cd_ctrl,
					ICP_QAT_FW_SLICE_CIPHER);
		ICP_QAT_FW_COMN_NEXT_ID_SET(cipher_cd_ctrl,
					ICP_QAT_FW_SLICE_AUTH);
		ICP_QAT_FW_COMN_CURR_ID_SET(hash_cd_ctrl,
					ICP_QAT_FW_SLICE_AUTH);
		ICP_QAT_FW_COMN_NEXT_ID_SET(hash_cd_ctrl,
					ICP_QAT_FW_SLICE_DRAM_WR);
	} else if (cdesc->qat_cmd == ICP_QAT_FW_LA_CMD_HASH_CIPHER) {
		ICP_QAT_FW_COMN_CURR_ID_SET(hash_cd_ctrl,
					ICP_QAT_FW_SLICE_AUTH);
		ICP_QAT_FW_COMN_NEXT_ID_SET(hash_cd_ctrl,
					ICP_QAT_FW_SLICE_CIPHER);
		ICP_QAT_FW_COMN_CURR_ID_SET(cipher_cd_ctrl,
					ICP_QAT_FW_SLICE_CIPHER);
		ICP_QAT_FW_COMN_NEXT_ID_SET(cipher_cd_ctrl,
					ICP_QAT_FW_SLICE_DRAM_WR);
	} else {
		PMD_DRV_LOG(ERR, "invalid param, only authenticated "
			    "encryption supported");
		return -EFAULT;
	}
	return 0;
}

int qat_alg_aead_session_create_content_desc_auth(struct qat_session *cdesc,
						uint8_t *authkey,
						uint32_t authkeylen,
						uint32_t add_auth_data_length,
						uint32_t digestsize,
						unsigned int operation)
{
	struct icp_qat_hw_cipher_algo_blk *cipher;
	struct icp_qat_hw_auth_algo_blk *hash;
	struct icp_qat_hw_cipher_algo_blk *cipherconfig;
	struct icp_qat_fw_la_bulk_req *req_tmpl = &cdesc->fw_req;
	struct icp_qat_fw_comn_req_hdr_cd_pars *cd_pars = &req_tmpl->cd_pars;
	struct icp_qat_fw_comn_req_hdr *header = &req_tmpl->comn_hdr;
	void *ptr = &req_tmpl->cd_ctrl;
	struct icp_qat_fw_cipher_cd_ctrl_hdr *cipher_cd_ctrl = ptr;
	struct icp_qat_fw_auth_cd_ctrl_hdr *hash_cd_ctrl = ptr;
	struct icp_qat_fw_la_auth_req_params *auth_param =
		(struct icp_qat_fw_la_auth_req_params *)
		((char *)&req_tmpl->serv_specif_rqpars +
		sizeof(struct icp_qat_fw_la_cipher_req_params));
	enum icp_qat_hw_cipher_convert key_convert;
	uint16_t proto = ICP_QAT_FW_LA_NO_PROTO;	/* no CCM/GCM/Snow3G */
	uint16_t state1_size = 0;
	uint16_t state2_size = 0;
	uint16_t cipher_offset = 0, hash_offset = 0;

	PMD_INIT_FUNC_TRACE();

	if (cdesc->qat_cmd == ICP_QAT_FW_LA_CMD_HASH_CIPHER &&
		cdesc->qat_hash_alg != ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2) {
		hash = (struct icp_qat_hw_auth_algo_blk *)&cdesc->cd;
		cipher =
		(struct icp_qat_hw_cipher_algo_blk *)((char *)&cdesc->cd +
				sizeof(struct icp_qat_hw_auth_algo_blk));
		hash_offset = 0;
		cipher_offset = ((char *)hash - (char *)cipher);
	} else {
		cipher = (struct icp_qat_hw_cipher_algo_blk *)&cdesc->cd;
		hash = (struct icp_qat_hw_auth_algo_blk *)((char *)&cdesc->cd +
				sizeof(struct icp_qat_hw_cipher_algo_blk));
		cipher_offset = 0;
		hash_offset = ((char *)hash - (char *)cipher);
	}

	/* CD setup */
	if (cdesc->qat_dir == ICP_QAT_HW_CIPHER_ENCRYPT) {
		ICP_QAT_FW_LA_RET_AUTH_SET(header->serv_specif_flags,
					   ICP_QAT_FW_LA_RET_AUTH_RES);
		ICP_QAT_FW_LA_CMP_AUTH_SET(header->serv_specif_flags,
					   ICP_QAT_FW_LA_NO_CMP_AUTH_RES);
	} else {
		ICP_QAT_FW_LA_RET_AUTH_SET(header->serv_specif_flags,
					   ICP_QAT_FW_LA_NO_RET_AUTH_RES);
		ICP_QAT_FW_LA_CMP_AUTH_SET(header->serv_specif_flags,
					   ICP_QAT_FW_LA_CMP_AUTH_RES);
	}

	if (cdesc->qat_mode == ICP_QAT_HW_CIPHER_CTR_MODE) {
		/* CTR Streaming ciphers are a special case. Decrypt = encrypt
		 * Overriding default values previously set
		 */
		cdesc->qat_dir = ICP_QAT_HW_CIPHER_ENCRYPT;
		key_convert = ICP_QAT_HW_CIPHER_NO_CONVERT;
	} else if (cdesc->qat_dir == ICP_QAT_HW_CIPHER_ENCRYPT)
		key_convert = ICP_QAT_HW_CIPHER_NO_CONVERT;
	else
		key_convert = ICP_QAT_HW_CIPHER_KEY_CONVERT;

	if (cdesc->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2)
		key_convert = ICP_QAT_HW_CIPHER_KEY_CONVERT;

	cipher->aes.cipher_config.val =
	    ICP_QAT_HW_CIPHER_CONFIG_BUILD(cdesc->qat_mode,
					cdesc->qat_cipher_alg, key_convert,
					cdesc->qat_dir);

	hash->sha.inner_setup.auth_config.reserved = 0;
	hash->sha.inner_setup.auth_config.config =
			ICP_QAT_HW_AUTH_CONFIG_BUILD(ICP_QAT_HW_AUTH_MODE1,
				cdesc->qat_hash_alg, digestsize);
	hash->sha.inner_setup.auth_counter.counter =
		rte_bswap32(qat_hash_get_block_size(cdesc->qat_hash_alg));
	if (cdesc->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2)  {
		hash->sha.inner_setup.auth_counter.counter = 0;
		hash->sha.outer_setup.auth_config.reserved = 0;
		cipherconfig = (struct icp_qat_hw_cipher_algo_blk *)
				((char *)&cdesc->cd +
				sizeof(struct icp_qat_hw_auth_algo_blk)
				+ 16);
		cipherconfig->aes.cipher_config.val =
		ICP_QAT_HW_CIPHER_CONFIG_BUILD(ICP_QAT_HW_CIPHER_ECB_MODE,
			ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2,
			ICP_QAT_HW_CIPHER_KEY_CONVERT,
			ICP_QAT_HW_CIPHER_ENCRYPT);
		memcpy(cipherconfig->aes.key, authkey, authkeylen);
		memset(cipherconfig->aes.key + authkeylen, 0,
			ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ);
	}

	/* Do precomputes */
	if (cdesc->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_AES_XCBC_MAC) {
		if (qat_alg_do_precomputes(cdesc->qat_hash_alg,
			authkey, authkeylen, (uint8_t *)(hash->sha.state1 +
			ICP_QAT_HW_AES_XCBC_MAC_STATE1_SZ), &state2_size)) {
			PMD_DRV_LOG(ERR, "(XCBC)precompute failed");
			return -EFAULT;
		}
	} else if ((cdesc->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_128) ||
		(cdesc->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_64)) {
		if (qat_alg_do_precomputes(cdesc->qat_hash_alg,
			authkey, authkeylen, (uint8_t *)(hash->sha.state1 +
			ICP_QAT_HW_GALOIS_128_STATE1_SZ), &state2_size)) {
			PMD_DRV_LOG(ERR, "(GCM)precompute failed");
			return -EFAULT;
		}
		/*
		 * Write (the length of AAD) into bytes 16-19 of state2
		 * in big-endian format. This field is 8 bytes
		 */
		uint32_t *aad_len = (uint32_t *)&hash->sha.state1[
					ICP_QAT_HW_GALOIS_128_STATE1_SZ +
					ICP_QAT_HW_GALOIS_H_SZ];
		*aad_len = rte_bswap32(add_auth_data_length);

		proto = ICP_QAT_FW_LA_GCM_PROTO;
	} else if (cdesc->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2)  {
		proto = ICP_QAT_FW_LA_SNOW_3G_PROTO;
		state1_size = qat_hash_get_state1_size(cdesc->qat_hash_alg);
	} else {
		if (qat_alg_do_precomputes(cdesc->qat_hash_alg,
			authkey, authkeylen, (uint8_t *)(hash->sha.state1),
			&state1_size)) {
			PMD_DRV_LOG(ERR, "(SHA)precompute failed");
			return -EFAULT;
		}
	}

	/* Request template setup */
	qat_alg_init_common_hdr(header);
	header->service_cmd_id = cdesc->qat_cmd;
	ICP_QAT_FW_LA_DIGEST_IN_BUFFER_SET(header->serv_specif_flags,
					   ICP_QAT_FW_LA_DIGEST_IN_BUFFER);
	/* Configure the common header protocol flags */
	ICP_QAT_FW_LA_PROTO_SET(header->serv_specif_flags, proto);
	cd_pars->u.s.content_desc_addr = cdesc->cd_paddr;
	cd_pars->u.s.content_desc_params_sz = sizeof(cdesc->cd) >> 3;

	if (cdesc->qat_cmd == ICP_QAT_FW_LA_CMD_AUTH)  {
		ICP_QAT_FW_LA_DIGEST_IN_BUFFER_SET(header->serv_specif_flags,
			ICP_QAT_FW_LA_NO_DIGEST_IN_BUFFER);
		ICP_QAT_FW_LA_CIPH_IV_FLD_FLAG_SET(header->serv_specif_flags,
			ICP_QAT_FW_CIPH_IV_64BIT_PTR);
		ICP_QAT_FW_LA_RET_AUTH_SET(header->serv_specif_flags,
			ICP_QAT_FW_LA_RET_AUTH_RES);
		ICP_QAT_FW_LA_CMP_AUTH_SET(header->serv_specif_flags,
			ICP_QAT_FW_LA_NO_CMP_AUTH_RES);
	}
	if (operation == RTE_CRYPTO_AUTH_OP_VERIFY) {
		ICP_QAT_FW_LA_RET_AUTH_SET(header->serv_specif_flags,
						ICP_QAT_FW_LA_NO_RET_AUTH_RES);
		ICP_QAT_FW_LA_CMP_AUTH_SET(header->serv_specif_flags,
						ICP_QAT_FW_LA_CMP_AUTH_RES);
	}

	/* Cipher CD config setup */
	cipher_cd_ctrl->cipher_state_sz = ICP_QAT_HW_AES_BLK_SZ >> 3;
	cipher_cd_ctrl->cipher_cfg_offset = cipher_offset >> 3;

	if (cdesc->qat_cmd != ICP_QAT_FW_LA_CMD_AUTH) {
		cipher_cd_ctrl->cipher_state_sz = ICP_QAT_HW_AES_BLK_SZ >> 3;
		cipher_cd_ctrl->cipher_cfg_offset = cipher_offset>>3;
		} else {
		cipher_cd_ctrl->cipher_state_sz = 0;
		cipher_cd_ctrl->cipher_cfg_offset = 0;
	}

	/* Auth CD config setup */
	hash_cd_ctrl->hash_cfg_offset = hash_offset >> 3;
	hash_cd_ctrl->hash_flags = ICP_QAT_FW_AUTH_HDR_FLAG_NO_NESTED;
	hash_cd_ctrl->inner_res_sz = digestsize;
	hash_cd_ctrl->final_sz = digestsize;
	hash_cd_ctrl->inner_state1_sz = state1_size;

	switch (cdesc->qat_hash_alg) {
	case ICP_QAT_HW_AUTH_ALGO_SHA1:
		hash_cd_ctrl->inner_state2_sz =
			RTE_ALIGN_CEIL(ICP_QAT_HW_SHA1_STATE2_SZ, 8);
		break;
	case ICP_QAT_HW_AUTH_ALGO_SHA256:
		hash_cd_ctrl->inner_state2_sz = ICP_QAT_HW_SHA256_STATE2_SZ;
		break;
	case ICP_QAT_HW_AUTH_ALGO_SHA512:
		hash_cd_ctrl->inner_state2_sz = ICP_QAT_HW_SHA512_STATE2_SZ;
		break;
	case ICP_QAT_HW_AUTH_ALGO_AES_XCBC_MAC:
		hash_cd_ctrl->inner_state2_sz =
				ICP_QAT_HW_AES_XCBC_MAC_STATE2_SZ;
		hash_cd_ctrl->inner_state1_sz =
				ICP_QAT_HW_AES_XCBC_MAC_STATE1_SZ;
		memset(hash->sha.state1, 0, ICP_QAT_HW_AES_XCBC_MAC_STATE1_SZ);
		break;
	case ICP_QAT_HW_AUTH_ALGO_GALOIS_128:
	case ICP_QAT_HW_AUTH_ALGO_GALOIS_64:
		hash_cd_ctrl->inner_state2_sz = ICP_QAT_HW_GALOIS_H_SZ +
						ICP_QAT_HW_GALOIS_LEN_A_SZ +
						ICP_QAT_HW_GALOIS_E_CTR0_SZ;
		hash_cd_ctrl->inner_state1_sz = ICP_QAT_HW_GALOIS_128_STATE1_SZ;
		memset(hash->sha.state1, 0, ICP_QAT_HW_GALOIS_128_STATE1_SZ);
		break;
	case ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2:
		hash_cd_ctrl->inner_state2_sz =
				ICP_QAT_HW_SNOW_3G_UIA2_STATE2_SZ;
		hash_cd_ctrl->inner_state1_sz =
				ICP_QAT_HW_SNOW_3G_UIA2_STATE1_SZ;
		memset(hash->sha.state1, 0, ICP_QAT_HW_SNOW_3G_UIA2_STATE1_SZ);
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid HASH alg %u", cdesc->qat_hash_alg);
		return -EFAULT;
	}

	hash_cd_ctrl->inner_state2_offset = hash_cd_ctrl->hash_cfg_offset +
			((sizeof(struct icp_qat_hw_auth_setup) +
			 RTE_ALIGN_CEIL(hash_cd_ctrl->inner_state1_sz, 8))
					>> 3);
	auth_param->auth_res_sz = digestsize;

	if (cdesc->qat_cmd == ICP_QAT_FW_LA_CMD_AUTH) {
		ICP_QAT_FW_COMN_CURR_ID_SET(hash_cd_ctrl,
					ICP_QAT_FW_SLICE_AUTH);
		ICP_QAT_FW_COMN_NEXT_ID_SET(hash_cd_ctrl,
					ICP_QAT_FW_SLICE_DRAM_WR);
	} else if (cdesc->qat_cmd == ICP_QAT_FW_LA_CMD_CIPHER_HASH) {
		ICP_QAT_FW_COMN_CURR_ID_SET(cipher_cd_ctrl,
				ICP_QAT_FW_SLICE_CIPHER);
		ICP_QAT_FW_COMN_NEXT_ID_SET(cipher_cd_ctrl,
				ICP_QAT_FW_SLICE_AUTH);
		ICP_QAT_FW_COMN_CURR_ID_SET(hash_cd_ctrl,
				ICP_QAT_FW_SLICE_AUTH);
		ICP_QAT_FW_COMN_NEXT_ID_SET(hash_cd_ctrl,
				ICP_QAT_FW_SLICE_DRAM_WR);
	} else if (cdesc->qat_cmd == ICP_QAT_FW_LA_CMD_HASH_CIPHER) {
		ICP_QAT_FW_COMN_CURR_ID_SET(hash_cd_ctrl,
				ICP_QAT_FW_SLICE_AUTH);
		ICP_QAT_FW_COMN_NEXT_ID_SET(hash_cd_ctrl,
				ICP_QAT_FW_SLICE_CIPHER);
		ICP_QAT_FW_COMN_CURR_ID_SET(cipher_cd_ctrl,
				ICP_QAT_FW_SLICE_CIPHER);
		ICP_QAT_FW_COMN_NEXT_ID_SET(cipher_cd_ctrl,
				ICP_QAT_FW_SLICE_DRAM_WR);
	} else {
		PMD_DRV_LOG(ERR, "invalid param, only authenticated "
				"encryption supported");
		return -EFAULT;
	}
	return 0;
}

static void qat_alg_ablkcipher_init_com(struct icp_qat_fw_la_bulk_req *req,
					struct icp_qat_hw_cipher_algo_blk *cd,
					const uint8_t *key, unsigned int keylen)
{
	struct icp_qat_fw_comn_req_hdr_cd_pars *cd_pars = &req->cd_pars;
	struct icp_qat_fw_comn_req_hdr *header = &req->comn_hdr;
	struct icp_qat_fw_cipher_cd_ctrl_hdr *cd_ctrl = (void *)&req->cd_ctrl;

	PMD_INIT_FUNC_TRACE();
	rte_memcpy(cd->aes.key, key, keylen);
	qat_alg_init_common_hdr(header);
	header->service_cmd_id = ICP_QAT_FW_LA_CMD_CIPHER;
	cd_pars->u.s.content_desc_params_sz =
				sizeof(struct icp_qat_hw_cipher_algo_blk) >> 3;
	/* Cipher CD config setup */
	cd_ctrl->cipher_key_sz = keylen >> 3;
	cd_ctrl->cipher_state_sz = ICP_QAT_HW_AES_BLK_SZ >> 3;
	cd_ctrl->cipher_cfg_offset = 0;
	ICP_QAT_FW_COMN_CURR_ID_SET(cd_ctrl, ICP_QAT_FW_SLICE_CIPHER);
	ICP_QAT_FW_COMN_NEXT_ID_SET(cd_ctrl, ICP_QAT_FW_SLICE_DRAM_WR);
}

void qat_alg_ablkcipher_init_enc(struct qat_alg_ablkcipher_cd *cdesc,
					int alg, const uint8_t *key,
					unsigned int keylen)
{
	struct icp_qat_hw_cipher_algo_blk *enc_cd = cdesc->cd;
	struct icp_qat_fw_la_bulk_req *req = &cdesc->fw_req;
	struct icp_qat_fw_comn_req_hdr_cd_pars *cd_pars = &req->cd_pars;

	PMD_INIT_FUNC_TRACE();
	qat_alg_ablkcipher_init_com(req, enc_cd, key, keylen);
	cd_pars->u.s.content_desc_addr = cdesc->cd_paddr;
	enc_cd->aes.cipher_config.val = QAT_AES_HW_CONFIG_CBC_ENC(alg);
}

void qat_alg_ablkcipher_init_dec(struct qat_alg_ablkcipher_cd *cdesc,
					int alg, const uint8_t *key,
					unsigned int keylen)
{
	struct icp_qat_hw_cipher_algo_blk *dec_cd = cdesc->cd;
	struct icp_qat_fw_la_bulk_req *req = &cdesc->fw_req;
	struct icp_qat_fw_comn_req_hdr_cd_pars *cd_pars = &req->cd_pars;

	PMD_INIT_FUNC_TRACE();
	qat_alg_ablkcipher_init_com(req, dec_cd, key, keylen);
	cd_pars->u.s.content_desc_addr = cdesc->cd_paddr;
	dec_cd->aes.cipher_config.val = QAT_AES_HW_CONFIG_CBC_DEC(alg);
}

int qat_alg_validate_aes_key(int key_len, enum icp_qat_hw_cipher_algo *alg)
{
	switch (key_len) {
	case ICP_QAT_HW_AES_128_KEY_SZ:
		*alg = ICP_QAT_HW_CIPHER_ALGO_AES128;
		break;
	case ICP_QAT_HW_AES_192_KEY_SZ:
		*alg = ICP_QAT_HW_CIPHER_ALGO_AES192;
		break;
	case ICP_QAT_HW_AES_256_KEY_SZ:
		*alg = ICP_QAT_HW_CIPHER_ALGO_AES256;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int qat_alg_validate_snow3g_key(int key_len, enum icp_qat_hw_cipher_algo *alg)
{
	switch (key_len) {
	case ICP_QAT_HW_SNOW_3G_UEA2_KEY_SZ:
		*alg = ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
