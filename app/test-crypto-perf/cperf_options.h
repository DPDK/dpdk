/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _CPERF_OPTIONS_
#define _CPERF_OPTIONS_

#include <rte_crypto.h>
#include <rte_cryptodev.h>
#ifdef RTE_LIB_SECURITY
#include <rte_security.h>
#endif

#define CPERF_PTEST_TYPE	("ptest")
#define CPERF_MODEX_LEN		("modex-len")
#define CPERF_RSA_PRIV_KEYTYPE	("rsa-priv-keytype")
#define CPERF_RSA_MODLEN	("rsa-modlen")
#define CPERF_SILENT		("silent")
#define CPERF_ENABLE_SDAP	("enable-sdap")

#define CPERF_POOL_SIZE		("pool-sz")
#define CPERF_TOTAL_OPS		("total-ops")
#define CPERF_BURST_SIZE	("burst-sz")
#define CPERF_BUFFER_SIZE	("buffer-sz")
#define CPERF_SEGMENT_SIZE	("segment-sz")
#define CPERF_DESC_NB		("desc-nb")
#define CPERF_IMIX		("imix")

#define CPERF_DEVTYPE		("devtype")
#define CPERF_OPTYPE		("optype")
#define CPERF_SESSIONLESS	("sessionless")
#define CPERF_SHARED_SESSION	("shared-session")
#define CPERF_OUT_OF_PLACE	("out-of-place")
#define CPERF_TEST_FILE		("test-file")
#define CPERF_TEST_NAME		("test-name")

#define CPERF_LOW_PRIO_QP_MASK	("low-prio-qp-mask")

#define CPERF_CIPHER_ALGO	("cipher-algo")
#define CPERF_CIPHER_OP		("cipher-op")
#define CPERF_CIPHER_KEY_SZ	("cipher-key-sz")
#define CPERF_CIPHER_IV_SZ	("cipher-iv-sz")

#define CPERF_AUTH_ALGO		("auth-algo")
#define CPERF_AUTH_OP		("auth-op")
#define CPERF_AUTH_KEY_SZ	("auth-key-sz")
#define CPERF_AUTH_IV_SZ	("auth-iv-sz")

#define CPERF_AEAD_ALGO		("aead-algo")
#define CPERF_AEAD_OP		("aead-op")
#define CPERF_AEAD_KEY_SZ	("aead-key-sz")
#define CPERF_AEAD_IV_SZ	("aead-iv-sz")
#define CPERF_AEAD_AAD_SZ	("aead-aad-sz")

#define CPERF_DIGEST_SZ		("digest-sz")

#define CPERF_ASYM_OP		("asym-op")

#ifdef RTE_LIB_SECURITY
#define CPERF_PDCP_SN_SZ	("pdcp-sn-sz")
#define CPERF_PDCP_DOMAIN	("pdcp-domain")
#define CPERF_PDCP_SES_HFN_EN	("pdcp-ses-hfn-en")
#define PDCP_DEFAULT_HFN	0x1
#define CPERF_DOCSIS_HDR_SZ	("docsis-hdr-sz")
#define CPERF_TLS_VERSION	("tls-version")
#endif

#define CPERF_CSV		("csv-friendly")

/* benchmark-specific options */
#define CPERF_PMDCC_DELAY_MS	("pmd-cyclecount-delay-ms")

#define MAX_LIST 32

enum cperf_perf_test_type {
	CPERF_TEST_TYPE_THROUGHPUT,
	CPERF_TEST_TYPE_LATENCY,
	CPERF_TEST_TYPE_VERIFY,
	CPERF_TEST_TYPE_PMDCC
};


extern const char *cperf_test_type_strs[];
extern const char *cperf_rsa_priv_keytype_strs[];

enum cperf_op_type {
	CPERF_CIPHER_ONLY = 1,
	CPERF_AUTH_ONLY,
	CPERF_CIPHER_THEN_AUTH,
	CPERF_AUTH_THEN_CIPHER,
	CPERF_AEAD,
	CPERF_PDCP,
	CPERF_DOCSIS,
	CPERF_IPSEC,
	CPERF_ASYM_MODEX,
	CPERF_ASYM_RSA,
	CPERF_ASYM_SECP256R1,
	CPERF_ASYM_SECP384R1,
	CPERF_ASYM_ED25519,
	CPERF_ASYM_SM2,
	CPERF_TLS,
};

extern const char *cperf_op_type_strs[];

struct cperf_options {
	enum cperf_perf_test_type test;

	uint32_t pool_sz;
	uint32_t total_ops;
	uint32_t headroom_sz;
	uint32_t tailroom_sz;
	uint32_t segment_sz;
	uint32_t test_buffer_size;
	uint32_t *imix_buffer_sizes;
	uint32_t nb_descriptors;
	uint16_t nb_qps;
	uint64_t low_prio_qp_mask;

	uint32_t sessionless:1;
	uint32_t shared_session:1;
	uint32_t out_of_place:1;
	uint32_t silent:1;
	uint32_t csv:1;
	uint32_t is_outbound:1;

	enum rte_crypto_cipher_algorithm cipher_algo;
	enum rte_crypto_cipher_operation cipher_op;

	uint16_t cipher_key_sz;
	uint16_t cipher_iv_sz;

	enum rte_crypto_auth_algorithm auth_algo;
	enum rte_crypto_auth_operation auth_op;

	uint16_t auth_key_sz;
	uint16_t auth_iv_sz;

	enum rte_crypto_aead_algorithm aead_algo;
	enum rte_crypto_aead_operation aead_op;

	uint16_t aead_key_sz;
	uint16_t aead_iv_sz;
	uint16_t aead_aad_sz;

	uint16_t digest_sz;

#ifdef RTE_LIB_SECURITY
	uint16_t pdcp_sn_sz;
	uint16_t pdcp_ses_hfn_en;
	uint16_t pdcp_sdap;
	enum rte_security_pdcp_domain pdcp_domain;
	uint16_t docsis_hdr_sz;
	enum rte_security_tls_version tls_version;
#endif
	char device_type[RTE_CRYPTODEV_NAME_MAX_LEN];
	enum cperf_op_type op_type;

	char *test_file;
	char *test_name;

	uint32_t buffer_size_list[MAX_LIST];
	uint8_t buffer_size_count;
	uint32_t max_buffer_size;
	uint32_t min_buffer_size;
	uint32_t inc_buffer_size;

	uint32_t burst_size_list[MAX_LIST];
	uint8_t burst_size_count;
	uint32_t max_burst_size;
	uint32_t min_burst_size;
	uint32_t inc_burst_size;

	/* pmd-cyclecount specific options */
	uint32_t pmdcc_delay;
	uint32_t imix_distribution_list[MAX_LIST];
	uint8_t imix_distribution_count;
	struct cperf_modex_test_data *modex_data;
	uint16_t modex_len;
	struct cperf_ecdsa_test_data *secp256r1_data;
	struct cperf_ecdsa_test_data *secp384r1_data;
	struct cperf_eddsa_test_data *eddsa_data;
	struct cperf_sm2_test_data *sm2_data;
	enum rte_crypto_asym_op_type asym_op_type;
	enum rte_crypto_auth_algorithm asym_hash_alg;
	struct cperf_rsa_test_data *rsa_data;
	uint16_t rsa_modlen;
	uint8_t rsa_keytype;
};

void
cperf_options_default(struct cperf_options *options);

int
cperf_options_parse(struct cperf_options *options,
		int argc, char **argv);

int
cperf_options_check(struct cperf_options *options);

void
cperf_options_dump(struct cperf_options *options);

#endif
