/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2015-2018 Intel Corporation
 */
#ifndef _ICP_QAT_ALGS_H_
#define _ICP_QAT_ALGS_H_
#include <rte_memory.h>
#include <rte_crypto.h>
#include "icp_qat_hw.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_la.h"
#include "../qat_crypto.h"

/*
 * Key Modifier (KM) value used in KASUMI algorithm in F9 mode to XOR
 * Integrity Key (IK)
 */
#define KASUMI_F9_KEY_MODIFIER_4_BYTES   0xAAAAAAAA

#define KASUMI_F8_KEY_MODIFIER_4_BYTES   0x55555555

/* 3DES key sizes */
#define QAT_3DES_KEY_SZ_OPT1 24 /* Keys are independent */
#define QAT_3DES_KEY_SZ_OPT2 16 /* K3=K1 */

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

enum qat_crypto_proto_flag {
	QAT_CRYPTO_PROTO_FLAG_NONE = 0,
	QAT_CRYPTO_PROTO_FLAG_CCM = 1,
	QAT_CRYPTO_PROTO_FLAG_GCM = 2,
	QAT_CRYPTO_PROTO_FLAG_SNOW3G = 3,
	QAT_CRYPTO_PROTO_FLAG_ZUC = 4
};

/*
 * Maximum number of SGL entries
 */
#define QAT_SGL_MAX_NUMBER	16

struct qat_alg_buf_list {
	uint64_t resrvd;
	uint32_t num_bufs;
	uint32_t num_mapped_bufs;
	struct qat_alg_buf bufers[QAT_SGL_MAX_NUMBER];
} __rte_packed __rte_cache_aligned;

struct qat_crypto_op_cookie {
	struct qat_alg_buf_list qat_sgl_list_src;
	struct qat_alg_buf_list qat_sgl_list_dst;
	rte_iova_t qat_sgl_src_phys_addr;
	rte_iova_t qat_sgl_dst_phys_addr;
};

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
	enum icp_qat_hw_auth_op auth_op;
	void *bpi_ctx;
	struct qat_alg_cd cd;
	uint8_t *cd_cur_ptr;
	rte_iova_t cd_paddr;
	struct icp_qat_fw_la_bulk_req fw_req;
	uint8_t aad_len;
	struct qat_crypto_instance *inst;
	struct {
		uint16_t offset;
		uint16_t length;
	} cipher_iv;
	struct {
		uint16_t offset;
		uint16_t length;
	} auth_iv;
	uint16_t digest_length;
	rte_spinlock_t lock;	/* protects this struct */
	enum qat_device_gen min_qat_dev_gen;
};

int qat_get_inter_state_size(enum icp_qat_hw_auth_algo qat_hash_alg);

int qat_alg_aead_session_create_content_desc_cipher(struct qat_session *cd,
						uint8_t *enckey,
						uint32_t enckeylen);

int qat_alg_aead_session_create_content_desc_auth(struct qat_session *cdesc,
						uint8_t *authkey,
						uint32_t authkeylen,
						uint32_t aad_length,
						uint32_t digestsize,
						unsigned int operation);

void qat_alg_init_common_hdr(struct icp_qat_fw_comn_req_hdr *header,
					enum qat_crypto_proto_flag proto_flags);

int qat_alg_validate_aes_key(int key_len, enum icp_qat_hw_cipher_algo *alg);
int qat_alg_validate_aes_docsisbpi_key(int key_len,
					enum icp_qat_hw_cipher_algo *alg);
int qat_alg_validate_snow3g_key(int key_len, enum icp_qat_hw_cipher_algo *alg);
int qat_alg_validate_kasumi_key(int key_len, enum icp_qat_hw_cipher_algo *alg);
int qat_alg_validate_3des_key(int key_len, enum icp_qat_hw_cipher_algo *alg);
int qat_alg_validate_des_key(int key_len, enum icp_qat_hw_cipher_algo *alg);
int qat_cipher_get_block_size(enum icp_qat_hw_cipher_algo qat_cipher_alg);
int qat_alg_validate_zuc_key(int key_len, enum icp_qat_hw_cipher_algo *alg);
#endif
