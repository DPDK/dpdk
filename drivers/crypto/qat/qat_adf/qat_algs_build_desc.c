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
#include <openssl/md5.h>	/* Needed to calculate pre-compute values */


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
	case ICP_QAT_HW_AUTH_ALGO_SHA224:
		return QAT_HW_ROUND_UP(ICP_QAT_HW_SHA224_STATE1_SZ,
						QAT_HW_DEFAULT_ALIGNMENT);
	case ICP_QAT_HW_AUTH_ALGO_SHA256:
		return QAT_HW_ROUND_UP(ICP_QAT_HW_SHA256_STATE1_SZ,
						QAT_HW_DEFAULT_ALIGNMENT);
	case ICP_QAT_HW_AUTH_ALGO_SHA384:
		return QAT_HW_ROUND_UP(ICP_QAT_HW_SHA384_STATE1_SZ,
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
	case ICP_QAT_HW_AUTH_ALGO_MD5:
		return QAT_HW_ROUND_UP(ICP_QAT_HW_MD5_STATE1_SZ,
						QAT_HW_DEFAULT_ALIGNMENT);
	case ICP_QAT_HW_AUTH_ALGO_KASUMI_F9:
		return QAT_HW_ROUND_UP(ICP_QAT_HW_KASUMI_F9_STATE1_SZ,
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
	case ICP_QAT_HW_AUTH_ALGO_SHA224:
		return ICP_QAT_HW_SHA224_STATE1_SZ;
	case ICP_QAT_HW_AUTH_ALGO_SHA256:
		return ICP_QAT_HW_SHA256_STATE1_SZ;
	case ICP_QAT_HW_AUTH_ALGO_SHA384:
		return ICP_QAT_HW_SHA384_STATE1_SZ;
	case ICP_QAT_HW_AUTH_ALGO_SHA512:
		return ICP_QAT_HW_SHA512_STATE1_SZ;
	case ICP_QAT_HW_AUTH_ALGO_MD5:
		return ICP_QAT_HW_MD5_STATE1_SZ;
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
	case ICP_QAT_HW_AUTH_ALGO_SHA224:
		return SHA256_CBLOCK;
	case ICP_QAT_HW_AUTH_ALGO_SHA256:
		return SHA256_CBLOCK;
	case ICP_QAT_HW_AUTH_ALGO_SHA384:
		return SHA512_CBLOCK;
	case ICP_QAT_HW_AUTH_ALGO_SHA512:
		return SHA512_CBLOCK;
	case ICP_QAT_HW_AUTH_ALGO_GALOIS_128:
		return 16;
	case ICP_QAT_HW_AUTH_ALGO_MD5:
		return MD5_CBLOCK;
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

static int partial_hash_sha224(uint8_t *data_in, uint8_t *data_out)
{
	SHA256_CTX ctx;

	if (!SHA224_Init(&ctx))
		return -EFAULT;
	SHA256_Transform(&ctx, data_in);
	rte_memcpy(data_out, &ctx, SHA256_DIGEST_LENGTH);
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

static int partial_hash_sha384(uint8_t *data_in, uint8_t *data_out)
{
	SHA512_CTX ctx;

	if (!SHA384_Init(&ctx))
		return -EFAULT;
	SHA512_Transform(&ctx, data_in);
	rte_memcpy(data_out, &ctx, SHA512_DIGEST_LENGTH);
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

static int partial_hash_md5(uint8_t *data_in, uint8_t *data_out)
{
	MD5_CTX ctx;

	if (!MD5_Init(&ctx))
		return -EFAULT;
	MD5_Transform(&ctx, data_in);
	rte_memcpy(data_out, &ctx, MD5_DIGEST_LENGTH);

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
	case ICP_QAT_HW_AUTH_ALGO_SHA224:
		if (partial_hash_sha224(data_in, digest))
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
	case ICP_QAT_HW_AUTH_ALGO_SHA384:
		if (partial_hash_sha384(data_in, digest))
			return -EFAULT;
		for (i = 0; i < digest_size >> 3; i++, hash_state_out_be64++)
			*hash_state_out_be64 =
				rte_bswap64(*(((uint64_t *)digest)+i));
		break;
	case ICP_QAT_HW_AUTH_ALGO_SHA512:
		if (partial_hash_sha512(data_in, digest))
			return -EFAULT;
		for (i = 0; i < digest_size >> 3; i++, hash_state_out_be64++)
			*hash_state_out_be64 =
				rte_bswap64(*(((uint64_t *)digest)+i));
		break;
	case ICP_QAT_HW_AUTH_ALGO_MD5:
		if (partial_hash_md5(data_in, data_out))
			return -EFAULT;
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

void qat_alg_init_common_hdr(struct icp_qat_fw_comn_req_hdr *header,
		uint16_t proto)
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
				proto);
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
	uint32_t total_key_size;
	uint16_t proto = ICP_QAT_FW_LA_NO_PROTO;	/* no CCM/GCM/SNOW 3G */
	uint16_t cipher_offset, cd_size;
	uint32_t wordIndex  = 0;
	uint32_t *temp_key = NULL;
	PMD_INIT_FUNC_TRACE();

	if (cdesc->qat_cmd == ICP_QAT_FW_LA_CMD_CIPHER) {
		cd_pars->u.s.content_desc_addr = cdesc->cd_paddr;
		ICP_QAT_FW_COMN_CURR_ID_SET(cipher_cd_ctrl,
					ICP_QAT_FW_SLICE_CIPHER);
		ICP_QAT_FW_COMN_NEXT_ID_SET(cipher_cd_ctrl,
					ICP_QAT_FW_SLICE_DRAM_WR);
		ICP_QAT_FW_LA_RET_AUTH_SET(header->serv_specif_flags,
					ICP_QAT_FW_LA_NO_RET_AUTH_RES);
		ICP_QAT_FW_LA_CMP_AUTH_SET(header->serv_specif_flags,
					ICP_QAT_FW_LA_NO_CMP_AUTH_RES);
		cdesc->cd_cur_ptr = (uint8_t *)&cdesc->cd;
	} else if (cdesc->qat_cmd == ICP_QAT_FW_LA_CMD_CIPHER_HASH) {
		cd_pars->u.s.content_desc_addr = cdesc->cd_paddr;
		ICP_QAT_FW_COMN_CURR_ID_SET(cipher_cd_ctrl,
					ICP_QAT_FW_SLICE_CIPHER);
		ICP_QAT_FW_COMN_NEXT_ID_SET(cipher_cd_ctrl,
					ICP_QAT_FW_SLICE_AUTH);
		ICP_QAT_FW_COMN_CURR_ID_SET(hash_cd_ctrl,
					ICP_QAT_FW_SLICE_AUTH);
		ICP_QAT_FW_COMN_NEXT_ID_SET(hash_cd_ctrl,
					ICP_QAT_FW_SLICE_DRAM_WR);
		cdesc->cd_cur_ptr = (uint8_t *)&cdesc->cd;
	} else if (cdesc->qat_cmd != ICP_QAT_FW_LA_CMD_HASH_CIPHER) {
		PMD_DRV_LOG(ERR, "Invalid param, must be a cipher command.");
		return -EFAULT;
	}

	if (cdesc->qat_mode == ICP_QAT_HW_CIPHER_CTR_MODE) {
		/*
		 * CTR Streaming ciphers are a special case. Decrypt = encrypt
		 * Overriding default values previously set
		 */
		cdesc->qat_dir = ICP_QAT_HW_CIPHER_ENCRYPT;
		key_convert = ICP_QAT_HW_CIPHER_NO_CONVERT;
	} else if (cdesc->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2)
		key_convert = ICP_QAT_HW_CIPHER_KEY_CONVERT;
	else if (cdesc->qat_dir == ICP_QAT_HW_CIPHER_ENCRYPT)
		key_convert = ICP_QAT_HW_CIPHER_NO_CONVERT;
	else
		key_convert = ICP_QAT_HW_CIPHER_KEY_CONVERT;

	if (cdesc->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2) {
		total_key_size = ICP_QAT_HW_SNOW_3G_UEA2_KEY_SZ +
			ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ;
		cipher_cd_ctrl->cipher_state_sz =
			ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ >> 3;
		proto = ICP_QAT_FW_LA_SNOW_3G_PROTO;
	} else if (cdesc->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_KASUMI) {
		total_key_size = ICP_QAT_HW_KASUMI_F8_KEY_SZ;
		cipher_cd_ctrl->cipher_state_sz = ICP_QAT_HW_KASUMI_BLK_SZ >> 3;
		cipher_cd_ctrl->cipher_padding_sz =
					(2 * ICP_QAT_HW_KASUMI_BLK_SZ) >> 3;
	} else if (cdesc->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_3DES) {
		total_key_size = ICP_QAT_HW_3DES_KEY_SZ;
		cipher_cd_ctrl->cipher_state_sz = ICP_QAT_HW_3DES_BLK_SZ >> 3;
		proto = ICP_QAT_FW_LA_PROTO_GET(header->serv_specif_flags);
	} else {
		total_key_size = cipherkeylen;
		cipher_cd_ctrl->cipher_state_sz = ICP_QAT_HW_AES_BLK_SZ >> 3;
		proto = ICP_QAT_FW_LA_PROTO_GET(header->serv_specif_flags);
	}
	cipher_cd_ctrl->cipher_key_sz = total_key_size >> 3;
	cipher_offset = cdesc->cd_cur_ptr-((uint8_t *)&cdesc->cd);
	cipher_cd_ctrl->cipher_cfg_offset = cipher_offset >> 3;

	header->service_cmd_id = cdesc->qat_cmd;
	qat_alg_init_common_hdr(header, proto);

	cipher = (struct icp_qat_hw_cipher_algo_blk *)cdesc->cd_cur_ptr;

	cipher->cipher_config.val =
	    ICP_QAT_HW_CIPHER_CONFIG_BUILD(cdesc->qat_mode,
					cdesc->qat_cipher_alg, key_convert,
					cdesc->qat_dir);

	if (cdesc->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_KASUMI) {
		temp_key = (uint32_t *)(cdesc->cd_cur_ptr +
					sizeof(struct icp_qat_hw_cipher_config)
					+ cipherkeylen);
		memcpy(cipher->key, cipherkey, cipherkeylen);
		memcpy(temp_key, cipherkey, cipherkeylen);

		/* XOR Key with KASUMI F8 key modifier at 4 bytes level */
		for (wordIndex = 0; wordIndex < (cipherkeylen >> 2);
								wordIndex++)
			temp_key[wordIndex] ^= KASUMI_F8_KEY_MODIFIER_4_BYTES;

		cdesc->cd_cur_ptr += sizeof(struct icp_qat_hw_cipher_config) +
					cipherkeylen + cipherkeylen;
	} else {
		memcpy(cipher->key, cipherkey, cipherkeylen);
		cdesc->cd_cur_ptr += sizeof(struct icp_qat_hw_cipher_config) +
					cipherkeylen;
	}

	if (total_key_size > cipherkeylen) {
		uint32_t padding_size =  total_key_size-cipherkeylen;
		if ((cdesc->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_3DES)
			&& (cipherkeylen == QAT_3DES_KEY_SZ_OPT2))
			/* K3 not provided so use K1 = K3*/
			memcpy(cdesc->cd_cur_ptr, cipherkey, padding_size);
		else
			memset(cdesc->cd_cur_ptr, 0, padding_size);
		cdesc->cd_cur_ptr += padding_size;
	}
	cd_size = cdesc->cd_cur_ptr-(uint8_t *)&cdesc->cd;
	cd_pars->u.s.content_desc_params_sz = RTE_ALIGN_CEIL(cd_size, 8) >> 3;

	return 0;
}

int qat_alg_aead_session_create_content_desc_auth(struct qat_session *cdesc,
						uint8_t *authkey,
						uint32_t authkeylen,
						uint32_t add_auth_data_length,
						uint32_t digestsize,
						unsigned int operation)
{
	struct icp_qat_hw_auth_setup *hash;
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
	uint16_t proto = ICP_QAT_FW_LA_NO_PROTO;	/* no CCM/GCM/SNOW 3G */
	uint16_t state1_size = 0, state2_size = 0;
	uint16_t hash_offset, cd_size;
	uint32_t *aad_len = NULL;
	uint32_t wordIndex  = 0;
	uint32_t *pTempKey;

	PMD_INIT_FUNC_TRACE();

	if (cdesc->qat_cmd == ICP_QAT_FW_LA_CMD_AUTH) {
		ICP_QAT_FW_COMN_CURR_ID_SET(hash_cd_ctrl,
					ICP_QAT_FW_SLICE_AUTH);
		ICP_QAT_FW_COMN_NEXT_ID_SET(hash_cd_ctrl,
					ICP_QAT_FW_SLICE_DRAM_WR);
		cdesc->cd_cur_ptr = (uint8_t *)&cdesc->cd;
	} else if (cdesc->qat_cmd == ICP_QAT_FW_LA_CMD_HASH_CIPHER) {
		ICP_QAT_FW_COMN_CURR_ID_SET(hash_cd_ctrl,
				ICP_QAT_FW_SLICE_AUTH);
		ICP_QAT_FW_COMN_NEXT_ID_SET(hash_cd_ctrl,
				ICP_QAT_FW_SLICE_CIPHER);
		ICP_QAT_FW_COMN_CURR_ID_SET(cipher_cd_ctrl,
				ICP_QAT_FW_SLICE_CIPHER);
		ICP_QAT_FW_COMN_NEXT_ID_SET(cipher_cd_ctrl,
				ICP_QAT_FW_SLICE_DRAM_WR);
		cdesc->cd_cur_ptr = (uint8_t *)&cdesc->cd;
	} else if (cdesc->qat_cmd != ICP_QAT_FW_LA_CMD_CIPHER_HASH) {
		PMD_DRV_LOG(ERR, "Invalid param, must be a hash command.");
		return -EFAULT;
	}

	if (operation == RTE_CRYPTO_AUTH_OP_VERIFY) {
		ICP_QAT_FW_LA_RET_AUTH_SET(header->serv_specif_flags,
				ICP_QAT_FW_LA_NO_RET_AUTH_RES);
		ICP_QAT_FW_LA_CMP_AUTH_SET(header->serv_specif_flags,
				ICP_QAT_FW_LA_CMP_AUTH_RES);
		cdesc->auth_op = ICP_QAT_HW_AUTH_VERIFY;
	} else {
		ICP_QAT_FW_LA_RET_AUTH_SET(header->serv_specif_flags,
					   ICP_QAT_FW_LA_RET_AUTH_RES);
		ICP_QAT_FW_LA_CMP_AUTH_SET(header->serv_specif_flags,
					   ICP_QAT_FW_LA_NO_CMP_AUTH_RES);
		cdesc->auth_op = ICP_QAT_HW_AUTH_GENERATE;
	}

	/*
	 * Setup the inner hash config
	 */
	hash_offset = cdesc->cd_cur_ptr-((uint8_t *)&cdesc->cd);
	hash = (struct icp_qat_hw_auth_setup *)cdesc->cd_cur_ptr;
	hash->auth_config.reserved = 0;
	hash->auth_config.config =
			ICP_QAT_HW_AUTH_CONFIG_BUILD(ICP_QAT_HW_AUTH_MODE1,
				cdesc->qat_hash_alg, digestsize);

	if (cdesc->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2
		|| cdesc->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_KASUMI_F9)
		hash->auth_counter.counter = 0;
	else
		hash->auth_counter.counter = rte_bswap32(
				qat_hash_get_block_size(cdesc->qat_hash_alg));

	cdesc->cd_cur_ptr += sizeof(struct icp_qat_hw_auth_setup);

	/*
	 * cd_cur_ptr now points at the state1 information.
	 */
	switch (cdesc->qat_hash_alg) {
	case ICP_QAT_HW_AUTH_ALGO_SHA1:
		if (qat_alg_do_precomputes(ICP_QAT_HW_AUTH_ALGO_SHA1,
			authkey, authkeylen, cdesc->cd_cur_ptr,	&state1_size)) {
			PMD_DRV_LOG(ERR, "(SHA)precompute failed");
			return -EFAULT;
		}
		state2_size = RTE_ALIGN_CEIL(ICP_QAT_HW_SHA1_STATE2_SZ, 8);
		break;
	case ICP_QAT_HW_AUTH_ALGO_SHA224:
		if (qat_alg_do_precomputes(ICP_QAT_HW_AUTH_ALGO_SHA224,
			authkey, authkeylen, cdesc->cd_cur_ptr, &state1_size)) {
			PMD_DRV_LOG(ERR, "(SHA)precompute failed");
			return -EFAULT;
		}
		state2_size = ICP_QAT_HW_SHA224_STATE2_SZ;
		break;
	case ICP_QAT_HW_AUTH_ALGO_SHA256:
		if (qat_alg_do_precomputes(ICP_QAT_HW_AUTH_ALGO_SHA256,
			authkey, authkeylen, cdesc->cd_cur_ptr,	&state1_size)) {
			PMD_DRV_LOG(ERR, "(SHA)precompute failed");
			return -EFAULT;
		}
		state2_size = ICP_QAT_HW_SHA256_STATE2_SZ;
		break;
	case ICP_QAT_HW_AUTH_ALGO_SHA384:
		if (qat_alg_do_precomputes(ICP_QAT_HW_AUTH_ALGO_SHA384,
			authkey, authkeylen, cdesc->cd_cur_ptr, &state1_size)) {
			PMD_DRV_LOG(ERR, "(SHA)precompute failed");
			return -EFAULT;
		}
		state2_size = ICP_QAT_HW_SHA384_STATE2_SZ;
		break;
	case ICP_QAT_HW_AUTH_ALGO_SHA512:
		if (qat_alg_do_precomputes(ICP_QAT_HW_AUTH_ALGO_SHA512,
			authkey, authkeylen, cdesc->cd_cur_ptr,	&state1_size)) {
			PMD_DRV_LOG(ERR, "(SHA)precompute failed");
			return -EFAULT;
		}
		state2_size = ICP_QAT_HW_SHA512_STATE2_SZ;
		break;
	case ICP_QAT_HW_AUTH_ALGO_AES_XCBC_MAC:
		state1_size = ICP_QAT_HW_AES_XCBC_MAC_STATE1_SZ;
		if (qat_alg_do_precomputes(ICP_QAT_HW_AUTH_ALGO_AES_XCBC_MAC,
			authkey, authkeylen, cdesc->cd_cur_ptr + state1_size,
			&state2_size)) {
			PMD_DRV_LOG(ERR, "(XCBC)precompute failed");
			return -EFAULT;
		}
		break;
	case ICP_QAT_HW_AUTH_ALGO_GALOIS_128:
	case ICP_QAT_HW_AUTH_ALGO_GALOIS_64:
		proto = ICP_QAT_FW_LA_GCM_PROTO;
		state1_size = ICP_QAT_HW_GALOIS_128_STATE1_SZ;
		if (qat_alg_do_precomputes(cdesc->qat_hash_alg,
			authkey, authkeylen, cdesc->cd_cur_ptr + state1_size,
			&state2_size)) {
			PMD_DRV_LOG(ERR, "(GCM)precompute failed");
			return -EFAULT;
		}
		/*
		 * Write (the length of AAD) into bytes 16-19 of state2
		 * in big-endian format. This field is 8 bytes
		 */
		auth_param->u2.aad_sz =
				RTE_ALIGN_CEIL(add_auth_data_length, 16);
		auth_param->hash_state_sz = (auth_param->u2.aad_sz) >> 3;

		aad_len = (uint32_t *)(cdesc->cd_cur_ptr +
					ICP_QAT_HW_GALOIS_128_STATE1_SZ +
					ICP_QAT_HW_GALOIS_H_SZ);
		*aad_len = rte_bswap32(add_auth_data_length);
		break;
	case ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2:
		proto = ICP_QAT_FW_LA_SNOW_3G_PROTO;
		state1_size = qat_hash_get_state1_size(
				ICP_QAT_HW_AUTH_ALGO_SNOW_3G_UIA2);
		state2_size = ICP_QAT_HW_SNOW_3G_UIA2_STATE2_SZ;
		memset(cdesc->cd_cur_ptr, 0, state1_size + state2_size);

		cipherconfig = (struct icp_qat_hw_cipher_algo_blk *)
				(cdesc->cd_cur_ptr + state1_size + state2_size);
		cipherconfig->cipher_config.val =
		ICP_QAT_HW_CIPHER_CONFIG_BUILD(ICP_QAT_HW_CIPHER_ECB_MODE,
			ICP_QAT_HW_CIPHER_ALGO_SNOW_3G_UEA2,
			ICP_QAT_HW_CIPHER_KEY_CONVERT,
			ICP_QAT_HW_CIPHER_ENCRYPT);
		memcpy(cipherconfig->key, authkey, authkeylen);
		memset(cipherconfig->key + authkeylen,
				0, ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ);
		cdesc->cd_cur_ptr += sizeof(struct icp_qat_hw_cipher_config) +
				authkeylen + ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ;
		auth_param->hash_state_sz =
				RTE_ALIGN_CEIL(add_auth_data_length, 16) >> 3;
		break;
	case ICP_QAT_HW_AUTH_ALGO_MD5:
		if (qat_alg_do_precomputes(ICP_QAT_HW_AUTH_ALGO_MD5,
			authkey, authkeylen, cdesc->cd_cur_ptr,
			&state1_size)) {
			PMD_DRV_LOG(ERR, "(MD5)precompute failed");
			return -EFAULT;
		}
		state2_size = ICP_QAT_HW_MD5_STATE2_SZ;
		break;
	case ICP_QAT_HW_AUTH_ALGO_NULL:
		break;
	case ICP_QAT_HW_AUTH_ALGO_KASUMI_F9:
		state1_size = qat_hash_get_state1_size(
				ICP_QAT_HW_AUTH_ALGO_KASUMI_F9);
		state2_size = ICP_QAT_HW_KASUMI_F9_STATE2_SZ;
		memset(cdesc->cd_cur_ptr, 0, state1_size + state2_size);
		pTempKey = (uint32_t *)(cdesc->cd_cur_ptr + state1_size
							+ authkeylen);
		/*
		* The Inner Hash Initial State2 block must contain IK
		* (Initialisation Key), followed by IK XOR-ed with KM
		* (Key Modifier): IK||(IK^KM).
		*/
		/* write the auth key */
		memcpy(cdesc->cd_cur_ptr + state1_size, authkey, authkeylen);
		/* initialise temp key with auth key */
		memcpy(pTempKey, authkey, authkeylen);
		/* XOR Key with KASUMI F9 key modifier at 4 bytes level */
		for (wordIndex = 0; wordIndex < (authkeylen >> 2); wordIndex++)
			pTempKey[wordIndex] ^= KASUMI_F9_KEY_MODIFIER_4_BYTES;
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid HASH alg %u", cdesc->qat_hash_alg);
		return -EFAULT;
	}

	/* Request template setup */
	qat_alg_init_common_hdr(header, proto);
	header->service_cmd_id = cdesc->qat_cmd;

	/* Auth CD config setup */
	hash_cd_ctrl->hash_cfg_offset = hash_offset >> 3;
	hash_cd_ctrl->hash_flags = ICP_QAT_FW_AUTH_HDR_FLAG_NO_NESTED;
	hash_cd_ctrl->inner_res_sz = digestsize;
	hash_cd_ctrl->final_sz = digestsize;
	hash_cd_ctrl->inner_state1_sz = state1_size;
	auth_param->auth_res_sz = digestsize;

	hash_cd_ctrl->inner_state2_sz  = state2_size;
	hash_cd_ctrl->inner_state2_offset = hash_cd_ctrl->hash_cfg_offset +
			((sizeof(struct icp_qat_hw_auth_setup) +
			 RTE_ALIGN_CEIL(hash_cd_ctrl->inner_state1_sz, 8))
					>> 3);

	cdesc->cd_cur_ptr += state1_size + state2_size;
	cd_size = cdesc->cd_cur_ptr-(uint8_t *)&cdesc->cd;

	cd_pars->u.s.content_desc_addr = cdesc->cd_paddr;
	cd_pars->u.s.content_desc_params_sz = RTE_ALIGN_CEIL(cd_size, 8) >> 3;

	return 0;
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

int qat_alg_validate_kasumi_key(int key_len, enum icp_qat_hw_cipher_algo *alg)
{
	switch (key_len) {
	case ICP_QAT_HW_KASUMI_KEY_SZ:
		*alg = ICP_QAT_HW_CIPHER_ALGO_KASUMI;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int qat_alg_validate_3des_key(int key_len, enum icp_qat_hw_cipher_algo *alg)
{
	switch (key_len) {
	case QAT_3DES_KEY_SZ_OPT1:
	case QAT_3DES_KEY_SZ_OPT2:
		*alg = ICP_QAT_HW_CIPHER_ALGO_3DES;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
