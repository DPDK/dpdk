/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2019 Intel Corporation
 */
#ifndef _QAT_SYM_SESSION_H_
#define _QAT_SYM_SESSION_H_

#include <rte_crypto.h>
#include <rte_cryptodev_pmd.h>
#ifdef RTE_LIB_SECURITY
#include <rte_security.h>
#endif

#include "qat_common.h"
#include "icp_qat_hw.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_la.h"

/*
 * Key Modifier (KM) value used in KASUMI algorithm in F9 mode to XOR
 * Integrity Key (IK)
 */
#define KASUMI_F9_KEY_MODIFIER_4_BYTES   0xAAAAAAAA

#define KASUMI_F8_KEY_MODIFIER_4_BYTES   0x55555555

/*
 * AES-GCM J0 length
 */
#define AES_GCM_J0_LEN 16

/* 3DES key sizes */
#define QAT_3DES_KEY_SZ_OPT1 24 /* Keys are independent */
#define QAT_3DES_KEY_SZ_OPT2 16 /* K3=K1 */
#define QAT_3DES_KEY_SZ_OPT3 8 /* K1=K2=K3 */

/* 96-bit case of IV for CCP/GCM single pass algorithm */
#define QAT_AES_GCM_SPC_IV_SIZE 12


#define QAT_AES_HW_CONFIG_CBC_ENC(alg) \
	ICP_QAT_HW_CIPHER_CONFIG_BUILD(ICP_QAT_HW_CIPHER_CBC_MODE, alg, \
					ICP_QAT_HW_CIPHER_NO_CONVERT, \
					ICP_QAT_HW_CIPHER_ENCRYPT)

#define QAT_AES_HW_CONFIG_CBC_DEC(alg) \
	ICP_QAT_HW_CIPHER_CONFIG_BUILD(ICP_QAT_HW_CIPHER_CBC_MODE, alg, \
					ICP_QAT_HW_CIPHER_KEY_CONVERT, \
					ICP_QAT_HW_CIPHER_DECRYPT)

#define QAT_AES_CMAC_CONST_RB 0x87

enum qat_sym_proto_flag {
	QAT_CRYPTO_PROTO_FLAG_NONE = 0,
	QAT_CRYPTO_PROTO_FLAG_CCM = 1,
	QAT_CRYPTO_PROTO_FLAG_GCM = 2,
	QAT_CRYPTO_PROTO_FLAG_SNOW3G = 3,
	QAT_CRYPTO_PROTO_FLAG_ZUC = 4
};

/* Common content descriptor */
struct qat_sym_cd {
	struct icp_qat_hw_cipher_algo_blk cipher;
	struct icp_qat_hw_auth_algo_blk hash;
} __rte_packed __rte_cache_aligned;

struct qat_sym_session {
	enum icp_qat_fw_la_cmd_id qat_cmd;
	enum icp_qat_hw_cipher_algo qat_cipher_alg;
	enum icp_qat_hw_cipher_dir qat_dir;
	enum icp_qat_hw_cipher_mode qat_mode;
	enum icp_qat_hw_auth_algo qat_hash_alg;
	enum icp_qat_hw_auth_op auth_op;
	enum icp_qat_hw_auth_mode auth_mode;
	void *bpi_ctx;
	struct qat_sym_cd cd;
	uint8_t *cd_cur_ptr;
	phys_addr_t cd_paddr;
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
	uint8_t aes_cmac;
	uint8_t is_single_pass;
};

int
qat_sym_session_configure(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess,
		struct rte_mempool *mempool);

int
qat_sym_session_set_parameters(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform, void *session_private);

int
qat_sym_session_configure_aead(struct rte_cryptodev *dev,
				struct rte_crypto_sym_xform *xform,
				struct qat_sym_session *session);

int
qat_sym_session_configure_cipher(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,
		struct qat_sym_session *session);

int
qat_sym_session_configure_auth(struct rte_cryptodev *dev,
				struct rte_crypto_sym_xform *xform,
				struct qat_sym_session *session);

int
qat_sym_session_aead_create_cd_cipher(struct qat_sym_session *cd,
						const uint8_t *enckey,
						uint32_t enckeylen);

int
qat_sym_session_aead_create_cd_auth(struct qat_sym_session *cdesc,
						const uint8_t *authkey,
						uint32_t authkeylen,
						uint32_t aad_length,
						uint32_t digestsize,
						unsigned int operation);

void
qat_sym_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *session);

unsigned int
qat_sym_session_get_private_size(struct rte_cryptodev *dev);

void
qat_sym_sesssion_init_common_hdr(struct icp_qat_fw_comn_req_hdr *header,
					enum qat_sym_proto_flag proto_flags);
int
qat_sym_validate_aes_key(int key_len, enum icp_qat_hw_cipher_algo *alg);
int
qat_sym_validate_aes_docsisbpi_key(int key_len,
					enum icp_qat_hw_cipher_algo *alg);
int
qat_sym_validate_snow3g_key(int key_len, enum icp_qat_hw_cipher_algo *alg);
int
qat_sym_validate_kasumi_key(int key_len, enum icp_qat_hw_cipher_algo *alg);
int
qat_sym_validate_3des_key(int key_len, enum icp_qat_hw_cipher_algo *alg);
int
qat_sym_validate_des_key(int key_len, enum icp_qat_hw_cipher_algo *alg);
int
qat_cipher_get_block_size(enum icp_qat_hw_cipher_algo qat_cipher_alg);
int
qat_sym_validate_zuc_key(int key_len, enum icp_qat_hw_cipher_algo *alg);

#ifdef RTE_LIB_SECURITY
int
qat_security_session_create(void *dev, struct rte_security_session_conf *conf,
		struct rte_security_session *sess, struct rte_mempool *mempool);
int
qat_security_session_destroy(void *dev, struct rte_security_session *sess);
#endif

#endif /* _QAT_SYM_SESSION_H_ */
