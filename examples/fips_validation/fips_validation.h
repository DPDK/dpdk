/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _FIPS_VALIDATION_H_
#define _FIPS_VALIDATION_H_

#define FIPS_PARSE_ERR(fmt, args)					\
	RTE_LOG(ERR, USER1, "FIPS parse error" ## fmt ## "\n", ## args)

#define ERR_MSG_SIZE		128
#define MAX_CASE_LINE		15
#define MAX_LINE_CHAR		204800 /*< max number of characters per line */
#define MAX_NB_TESTS		10240
#define DEF_MBUF_SEG_SIZE	(UINT16_MAX - sizeof(struct rte_mbuf) - \
				RTE_PKTMBUF_HEADROOM)
#define MAX_STRING_SIZE		64
#define MAX_FILE_NAME_SIZE	256
#define MAX_DIGEST_SIZE		64

#define POSITIVE_TEST		0
#define NEGATIVE_TEST		-1

#define REQ_FILE_PERFIX		"req"
#define RSP_FILE_PERFIX		"rsp"
#define FAX_FILE_PERFIX		"fax"

enum fips_test_algorithms {
		FIPS_TEST_ALGO_AES = 0,
		FIPS_TEST_ALGO_AES_GCM,
		FIPS_TEST_ALGO_AES_CMAC,
		FIPS_TEST_ALGO_AES_CCM,
		FIPS_TEST_ALGO_HMAC,
		FIPS_TEST_ALGO_TDES,
		FIPS_TEST_ALGO_SHA,
		FIPS_TEST_ALGO_AES_XTS,
		FIPS_TEST_ALGO_MAX
};

enum file_types {
	FIPS_TYPE_REQ = 1,
	FIPS_TYPE_FAX,
	FIPS_TYPE_RSP
};

enum fips_test_op {
	FIPS_TEST_ENC_AUTH_GEN = 1,
	FIPS_TEST_DEC_AUTH_VERIF,
};

#define MAX_LINE_PER_VECTOR            16

struct fips_val {
	uint8_t *val;
	uint32_t len;
};

struct fips_test_vector {
	union {
		struct {
			struct fips_val key;
			struct fips_val digest;
			struct fips_val auth_aad;
			struct fips_val aad;
		} cipher_auth;
		struct {
			struct fips_val key;
			struct fips_val digest;
			struct fips_val aad;
		} aead;
	};

	struct fips_val pt;
	struct fips_val ct;
	struct fips_val iv;

	enum rte_crypto_op_status status;
};

typedef int (*post_prcess_t)(struct fips_val *val);

typedef int (*parse_callback_t)(const char *key, char *text,
		struct fips_val *val);

struct fips_test_callback {
	const char *key;
	parse_callback_t cb;
	struct fips_val *val;
};

enum fips_aesavs_test_types {
	AESAVS_TYPE_GFXBOX = 1,
	AESAVS_TYPE_KEYSBOX,
	AESAVS_TYPE_VARKEY,
	AESAVS_TYPE_VARTXT,
	AESAVS_TYPE_MMT,
	AESAVS_TYPE_MCT,
};

enum fips_tdes_test_types {
	TDES_INVERSE_PERMUTATION = 0,
	TDES_PERMUTATION,
	TDES_SUBSTITUTION_TABLE,
	TDES_VARIABLE_KEY,
	TDES_VARIABLE_TEXT,
	TDES_KAT,
	TDES_MCT, /* Monte Carlo (Modes) Test */
	TDES_MMT /* Multi block Message Test */
};

enum fips_tdes_test_mode {
	TDES_MODE_CBC = 0,
	TDES_MODE_ECB
};

enum fips_ccm_test_types {
	CCM_VADT	= 1, /* Variable Associated Data Test */
	CCM_VPT,		 /* Variable Payload Test */
	CCM_VNT,		 /* Variable Nonce Test */
	CCM_VTT,		 /* Variable Tag Test */
	CCM_DVPT,	 /*  Decryption-Verification Process Test */
};

enum fips_sha_test_types {
	SHA_KAT = 0,
	SHA_MCT
};

struct aesavs_interim_data {
	enum fips_aesavs_test_types test_type;
	uint32_t cipher_algo;
	uint32_t key_len;
};

struct hmac_interim_data {
	enum rte_crypto_auth_algorithm algo;
};

struct tdes_interim_data {
	enum fips_tdes_test_types test_type;
	enum fips_tdes_test_mode test_mode;
	uint32_t nb_keys;
};

struct ccm_interim_data {
	enum fips_ccm_test_types test_type;
	uint32_t aad_len;
	uint32_t pt_len;
	uint32_t digest_len;
	uint32_t key_len;
	uint32_t iv_len;
};

struct sha_interim_data {
	enum fips_sha_test_types test_type;
	enum rte_crypto_auth_algorithm algo;
};

struct gcm_interim_data {
	uint8_t is_gmac;
	uint8_t gen_iv;
};

struct fips_test_interim_info {
	FILE *fp_rd;
	FILE *fp_wr;
	enum file_types file_type;
	enum fips_test_algorithms algo;
	char *one_line_text;
	char *vec[MAX_LINE_PER_VECTOR];
	uint32_t vec_start_off;
	uint32_t nb_vec_lines;
	char device_name[MAX_STRING_SIZE];
	char file_name[MAX_FILE_NAME_SIZE];
	float version;

	union {
		struct aesavs_interim_data aes_data;
		struct hmac_interim_data hmac_data;
		struct tdes_interim_data tdes_data;
		struct ccm_interim_data ccm_data;
		struct sha_interim_data sha_data;
		struct gcm_interim_data gcm_data;
	} interim_info;

	enum fips_test_op op;

	const struct fips_test_callback *callbacks;
	const struct fips_test_callback *interim_callbacks;
	const struct fips_test_callback *writeback_callbacks;

	post_prcess_t parse_writeback;
	post_prcess_t kat_check;
};

extern struct fips_test_vector vec;
extern struct fips_test_interim_info info;

int
fips_test_init(const char *req_file_path, const char *rsp_file_path,
		const char *device_name);

void
fips_test_clear(void);

int
fips_test_fetch_one_block(void);

int
fips_test_parse_one_case(void);

void
fips_test_write_one_case(void);

int
parse_test_aes_init(void);

int
parse_test_tdes_init(void);

int
parse_test_hmac_init(void);

int
parse_test_gcm_init(void);

int
parse_test_cmac_init(void);

int
parse_test_ccm_init(void);

int
parse_test_sha_init(void);

int
parse_test_xts_init(void);

int
parser_read_uint8_hex(uint8_t *value, const char *p);

int
parse_uint8_hex_str(const char *key, char *src, struct fips_val *val);

int
parse_uint8_known_len_hex_str(const char *key, char *src, struct fips_val *val);

int
parser_read_uint32_val(const char *key, char *src, struct fips_val *val);

int
parser_read_uint32_bit_val(const char *key, char *src, struct fips_val *val);

int
parser_read_uint32(uint32_t *value, char *p);

int
parser_read_uint32_val(const char *key, char *src, struct fips_val *val);

int
writeback_hex_str(const char *key, char *dst, struct fips_val *val);

void
parse_write_hex_str(struct fips_val *src);

int
update_info_vec(uint32_t count);

typedef int (*fips_test_one_case_t)(void);
typedef int (*fips_prepare_op_t)(void);
typedef int (*fips_prepare_xform_t)(struct rte_crypto_sym_xform *);

struct fips_test_ops {
	fips_prepare_xform_t prepare_xform;
	fips_prepare_op_t prepare_op;
	fips_test_one_case_t test;
};

extern struct fips_test_ops test_ops;

int prepare_aead_op(void);

int prepare_auth_op(void);

int prepare_gcm_xform(struct rte_crypto_sym_xform *xform);

int prepare_gmac_xform(struct rte_crypto_sym_xform *xform);

#endif
