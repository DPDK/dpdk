/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2022 Intel Corporation
 */
#ifndef _QAT_SYM_SESSION_H_
#define _QAT_SYM_SESSION_H_

#include <rte_crypto.h>
#include <cryptodev_pmd.h>
#include <rte_security.h>
#include <rte_net_crc.h>

#include "qat_common.h"
#include "icp_qat_hw.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_la.h"

#ifndef RTE_QAT_OPENSSL
#ifndef RTE_ARCH_ARM
#include <intel-ipsec-mb.h>
#endif
#endif

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

#define ICP_QAT_HW_GEN3_CRC_FLAGS_BUILD(ref_in, ref_out) \
	(((ref_in & QAT_GEN3_COMP_REFLECT_IN_MASK) << \
				QAT_GEN3_COMP_REFLECT_IN_BITPOS) | \
	((ref_out & QAT_GEN3_COMP_REFLECT_OUT_MASK) << \
				QAT_GEN3_COMP_REFLECT_OUT_BITPOS))

#define QAT_AES_CMAC_CONST_RB 0x87

#define QAT_CRYPTO_SLICE_SPC	1
#define QAT_CRYPTO_SLICE_UCS	2
#define QAT_CRYPTO_SLICE_WCP	4

#define QAT_PREFIX_SIZE		64
#define QAT_PREFIX_TBL_SIZE	((QAT_PREFIX_SIZE) * 2)

#define QAT_SESSION_IS_SLICE_SET(flags, flag)	\
	(!!((flags) & (flag)))

#define QAT_SM3_BLOCK_SIZE	64
#define QAT_SHA_CBLOCK 64
#define QAT_SHA512_CBLOCK 128
#define QAT_MD5_CBLOCK 64

enum qat_sym_proto_flag {
	QAT_CRYPTO_PROTO_FLAG_NONE = 0,
	QAT_CRYPTO_PROTO_FLAG_CCM = 1,
	QAT_CRYPTO_PROTO_FLAG_GCM = 2,
	QAT_CRYPTO_PROTO_FLAG_SNOW3G = 3,
	QAT_CRYPTO_PROTO_FLAG_ZUC = 4
};

struct qat_sym_session;

/*
 * typedef qat_op_build_request_t function pointer, passed in as argument
 * in enqueue op burst, where a build request assigned base on the type of
 * crypto op.
 */
typedef int (*qat_sym_build_request_t)(void *in_op, struct qat_sym_session *ctx,
		uint8_t *out_msg, void *op_cookie);

/* Common content descriptor */
struct __rte_cache_aligned __rte_packed_begin qat_sym_cd {
	struct icp_qat_hw_cipher_algo_blk cipher;
	union {
		struct icp_qat_hw_auth_algo_blk hash;
		struct icp_qat_hw_gen2_crc_cd crc_gen2;
		struct icp_qat_hw_gen3_crc_cd crc_gen3;
		struct icp_qat_hw_gen4_crc_cd crc_gen4;
	};
} __rte_packed_end;

struct qat_sym_session {
	enum icp_qat_fw_la_cmd_id qat_cmd;
	enum icp_qat_hw_cipher_algo qat_cipher_alg;
	enum icp_qat_hw_cipher_dir qat_dir;
	enum icp_qat_hw_cipher_mode qat_mode;
	enum icp_qat_hw_auth_algo qat_hash_alg;
	enum icp_qat_hw_auth_op auth_op;
	enum icp_qat_hw_auth_mode auth_mode;
	void *bpi_ctx;
	union {
		struct qat_sym_cd cd;
		uint8_t key_array[32];
	};
	alignas(RTE_CACHE_LINE_SIZE) uint8_t prefix_state[QAT_PREFIX_TBL_SIZE];
	uint8_t *cd_cur_ptr;
	union {
		phys_addr_t cd_paddr;
		phys_addr_t key_paddr;
	};
	phys_addr_t prefix_paddr;
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
	uint16_t auth_key_length;
	uint16_t digest_length;
	rte_spinlock_t lock;	/* protects this struct */
	uint16_t dev_id;
	uint8_t aes_cmac;
	uint8_t is_single_pass;
	uint8_t is_single_pass_gmac;
	uint8_t is_ucs;
	uint8_t is_iv12B;
	uint8_t is_gmac;
	uint8_t is_auth;
	uint8_t is_cnt_zero;
	/* Some generations need different setup of counter */
	uint8_t is_wireless;
	uint32_t slice_types;
	struct rte_net_crc *crc;
	enum qat_sym_proto_flag qat_proto_flag;
	qat_sym_build_request_t build_request[2];
#ifndef RTE_QAT_OPENSSL
	IMB_MGR *mb_mgr;
	alignas(16) uint64_t expkey[4 * 15];
	alignas(16) uint32_t dust[4 * 15];
	uint8_t docsis_key_len;
#endif
};

int
qat_sym_session_configure(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess);

int
qat_sym_session_set_parameters(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform, void *session_private,
		rte_iova_t session_private_iova);

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

void
qat_sym_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *session);

unsigned int
qat_sym_session_get_private_size(struct rte_cryptodev *dev);

int
qat_cipher_crc_cap_msg_sess_prepare(struct qat_sym_session *session,
					rte_iova_t session_paddr,
					const uint8_t *cipherkey,
					uint32_t cipherkeylen,
					enum qat_device_gen qat_dev_gen);

void
qat_sym_sesssion_init_common_hdr(struct qat_sym_session *session,
					struct icp_qat_fw_comn_req_hdr *header,
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

int
qat_security_session_create(void *dev, struct rte_security_session_conf *conf,
		struct rte_security_session *sess);
int
qat_security_session_destroy(void *dev, struct rte_security_session *sess);
unsigned int
qat_security_session_get_size(void *dev __rte_unused);

#endif /* _QAT_SYM_SESSION_H_ */
