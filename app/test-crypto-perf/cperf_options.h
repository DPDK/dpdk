
#ifndef _CPERF_OPTIONS_
#define _CPERF_OPTIONS_

#include <rte_crypto.h>

#define CPERF_PTEST_TYPE	("ptest")
#define CPERF_SILENT		("silent")

#define CPERF_POOL_SIZE		("pool-sz")
#define CPERF_TOTAL_OPS		("total-ops")
#define CPERF_BURST_SIZE	("burst-sz")
#define CPERF_BUFFER_SIZE	("buffer-sz")
#define CPERF_SEGMENTS_NB	("segments-nb")

#define CPERF_DEVTYPE		("devtype")
#define CPERF_OPTYPE		("optype")
#define CPERF_SESSIONLESS	("sessionless")
#define CPERF_OUT_OF_PLACE	("out-of-place")
#define CPERF_VERIFY		("verify")
#define CPERF_TEST_FILE		("test-file")
#define CPERF_TEST_NAME		("test-name")

#define CPERF_CIPHER_ALGO	("cipher-algo")
#define CPERF_CIPHER_OP		("cipher-op")
#define CPERF_CIPHER_KEY_SZ	("cipher-key-sz")
#define CPERF_CIPHER_IV_SZ	("cipher-iv-sz")

#define CPERF_AUTH_ALGO		("auth-algo")
#define CPERF_AUTH_OP		("auth-op")
#define CPERF_AUTH_KEY_SZ	("auth-key-sz")
#define CPERF_AUTH_DIGEST_SZ	("auth-digest-sz")
#define CPERF_AUTH_AAD_SZ	("auth-aad-sz")
#define CPERF_CSV		("csv-friendly")


enum cperf_perf_test_type {
	CPERF_TEST_TYPE_THROUGHPUT,
	CPERF_TEST_TYPE_CYCLECOUNT,
	CPERF_TEST_TYPE_LATENCY
};


extern const char *cperf_test_type_strs[];

enum cperf_op_type {
	CPERF_CIPHER_ONLY = 1,
	CPERF_AUTH_ONLY,
	CPERF_CIPHER_THEN_AUTH,
	CPERF_AUTH_THEN_CIPHER,
	CPERF_AEAD
};

extern const char *cperf_op_type_strs[];

struct cperf_options {
	enum cperf_perf_test_type test;

	uint32_t pool_sz;
	uint32_t total_ops;
	uint32_t burst_sz;
	uint32_t buffer_sz;
	uint32_t segments_nb;

	char device_type[RTE_CRYPTODEV_NAME_LEN];
	enum cperf_op_type op_type;

	uint32_t sessionless:1;
	uint32_t out_of_place:1;
	uint32_t verify:1;
	uint32_t silent:1;
	uint32_t csv:1;

	char *test_file;
	char *test_name;

	enum rte_crypto_cipher_algorithm cipher_algo;
	enum rte_crypto_cipher_operation cipher_op;

	uint16_t cipher_key_sz;
	uint16_t cipher_iv_sz;

	enum rte_crypto_auth_algorithm auth_algo;
	enum rte_crypto_auth_operation auth_op;

	uint16_t auth_key_sz;
	uint16_t auth_digest_sz;
	uint16_t auth_aad_sz;
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
