/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */
#include <rte_cryptodev.h>
#include <rte_malloc.h>

#include "test.h"
#include "test_cryptodev.h"

#define MAX_NB_SESSIONS 1
#define MAX_TEST_STRING_LEN 256

/*
 * The test suite will iterate through the capabilities of each probed cryptodev to identify the
 * common ones. Once the common capabilities are determined, the test suite will generate potential
 * valid inputs and crosscheck (compare) the output results from all cryptodevs.
 */
static struct rte_cryptodev_symmetric_capability *common_symm_capas;
static uint16_t nb_common_sym_caps;

/* Policies of capabilities selection */
enum capability_select_type {
	CAPABILITY_TYPE_MIN,
	CAPABILITY_TYPE_MAX,
	CAPABILITY_TYPE_LAST,
};

static const char * const capability_select_strings[] = {
	[CAPABILITY_TYPE_MIN] = "MIN",
	[CAPABILITY_TYPE_MAX] = "MAX",
};

/* Length of input text to be encrypted */
static size_t input_length[] = { 64, 256, 512 };

/* Calculate number of test cases(combinations) per algorithm */
#define NB_TEST_CASES_PER_ALGO (CAPABILITY_TYPE_LAST * RTE_DIM(input_length))

enum crypto_op_type {
	OP_ENCRYPT,
	OP_DECRYPT,
};

struct crosscheck_test_profile {
	char name[MAX_TEST_STRING_LEN];
	size_t input_buf_len;
	enum rte_crypto_sym_xform_type xform_type;
	int algo;
	uint16_t block_size;
	uint16_t key_size;
	uint16_t iv_size;
	uint16_t digest_size;
	uint16_t aad_size;
	uint32_t dataunit_set;
};

struct meta_test_suite {
	char suite_name[MAX_TEST_STRING_LEN];
	struct crosscheck_test_profile profile[NB_TEST_CASES_PER_ALGO];
};

struct memory_segment {
	uint8_t *mem;
	uint16_t len;
};

struct crosscheck_testsuite_params {
	struct rte_mempool *mbuf_pool;
	struct rte_mempool *op_mpool;
	struct rte_mempool *session_mpool;
	struct rte_cryptodev_config conf;
	struct rte_cryptodev_qp_conf qp_conf;

	uint8_t valid_devs[RTE_CRYPTO_MAX_DEVS];
	uint8_t valid_dev_count;

	struct memory_segment key;
	struct memory_segment digest;
	struct memory_segment aad;
	struct memory_segment iv;

	struct memory_segment expected_digest;
	struct memory_segment expected_aad;
};

static struct crosscheck_testsuite_params testsuite_params;

static const char*
algo_name_get(const struct rte_cryptodev_symmetric_capability *capa)
{
	switch (capa->xform_type) {
	case RTE_CRYPTO_SYM_XFORM_AUTH:
		return rte_cryptodev_get_auth_algo_string(capa->auth.algo);
	case RTE_CRYPTO_SYM_XFORM_CIPHER:
		return rte_cryptodev_get_cipher_algo_string(capa->cipher.algo);
	case RTE_CRYPTO_SYM_XFORM_AEAD:
		return rte_cryptodev_get_aead_algo_string(capa->aead.algo);
	default:
		return NULL;
	}
}

static void
incrementing_generate(uint8_t *dst, uint8_t start, uint16_t size)
{
	int i;

	for (i = 0; i < size; i++)
		dst[i] = start + i;
}

static void
pattern_fill(uint8_t *input, const char *pattern, uint16_t size)
{
	size_t pattern_len = strlen(pattern);
	size_t filled_len = 0, to_fill;

	while (filled_len < size) {
		to_fill = RTE_MIN(pattern_len, size - filled_len);
		rte_memcpy(input, pattern, to_fill);
		filled_len += to_fill;
		input += to_fill;
	}
}

static struct crosscheck_test_profile
profile_create(const struct rte_cryptodev_symmetric_capability *capa,
	       enum capability_select_type capability_type, size_t input_len)
{
	struct crosscheck_test_profile profile;

	memset(&profile, 0, sizeof(profile));
	profile.xform_type = capa->xform_type;

	switch (capa->xform_type) {
	case RTE_CRYPTO_SYM_XFORM_AUTH:
		profile.block_size = capa->auth.block_size;
		profile.algo = capa->auth.algo;

		switch (capability_type) {
		case CAPABILITY_TYPE_MIN:
			profile.key_size = capa->auth.key_size.min;
			profile.iv_size = capa->auth.iv_size.min;
			profile.digest_size = capa->auth.digest_size.min;
			profile.aad_size = capa->auth.aad_size.min;
			break;
		case CAPABILITY_TYPE_MAX:
			profile.key_size = capa->auth.key_size.max;
			profile.iv_size = capa->auth.iv_size.max;
			profile.digest_size = capa->auth.digest_size.max;
			profile.aad_size = capa->auth.aad_size.max;
			break;
		default:
			rte_panic("Wrong capability profile type: %i\n", capability_type);
			break;
		}
		break;
	case RTE_CRYPTO_SYM_XFORM_CIPHER:
		profile.block_size = capa->cipher.block_size;
		profile.algo = capa->cipher.algo;
		profile.dataunit_set = capa->cipher.dataunit_set;

		switch (capability_type) {
		case CAPABILITY_TYPE_MIN:
			profile.key_size = capa->cipher.key_size.min;
			profile.iv_size = capa->cipher.iv_size.min;
			break;
		case CAPABILITY_TYPE_MAX:
			profile.key_size = capa->cipher.key_size.max;
			profile.iv_size = capa->cipher.iv_size.max;
			break;
		default:
			rte_panic("Wrong capability profile type: %i\n", capability_type);
			break;
		}
		break;
	case RTE_CRYPTO_SYM_XFORM_AEAD:
		profile.block_size = capa->aead.block_size;
		profile.algo = capa->aead.algo;

		switch (capability_type) {
		case CAPABILITY_TYPE_MIN:
			profile.key_size = capa->aead.key_size.min;
			profile.iv_size = capa->aead.iv_size.min;
			profile.digest_size = capa->aead.digest_size.min;
			profile.aad_size = capa->aead.aad_size.min;
			break;
		case CAPABILITY_TYPE_MAX:
			profile.key_size = capa->aead.key_size.max;
			profile.iv_size = capa->aead.iv_size.max;
			profile.digest_size = capa->aead.digest_size.max;
			profile.aad_size = capa->aead.aad_size.max;
			break;
		default:
			rte_panic("Wrong capability profile type: %i\n", capability_type);
			break;
		}
		break;
	default:
		rte_panic("Wrong xform profile type: %i\n", capa->xform_type);
		break;
	}

	profile.input_buf_len = RTE_ALIGN_CEIL(input_len, profile.block_size);

	snprintf(profile.name, MAX_TEST_STRING_LEN,
			"'%s' - capabilities: '%s', input len: '%zu'",
			algo_name_get(capa), capability_select_strings[capability_type],
			input_len);

	return profile;
}

static inline int
common_range_set(struct rte_crypto_param_range *dst, const struct rte_crypto_param_range *src)
{
	/* Check if ranges overlaps */
	if ((dst->min > src->max) && (dst->max < src->min))
		return -1;
	dst->min = RTE_MAX(dst->min, src->min);
	dst->max = RTE_MIN(dst->max, src->max);

	return 0;
}

static uint16_t
nb_sym_capabilities_get(const struct rte_cryptodev_capabilities *cap)
{
	uint16_t nb_caps = 0;

	for (; cap->op != RTE_CRYPTO_OP_TYPE_UNDEFINED; cap++) {
		if (cap->op == RTE_CRYPTO_OP_TYPE_SYMMETRIC)
			nb_caps += 1;
	}

	return nb_caps;
}

static struct rte_cryptodev_sym_capability_idx
sym_capability_to_idx(const struct rte_cryptodev_symmetric_capability *cap)
{
	struct rte_cryptodev_sym_capability_idx cap_idx;

	cap_idx.type = cap->xform_type;
	switch (cap_idx.type) {
	case RTE_CRYPTO_SYM_XFORM_CIPHER:
		cap_idx.algo.auth = cap->auth.algo;
		break;
	case RTE_CRYPTO_SYM_XFORM_AUTH:
		cap_idx.algo.cipher = cap->cipher.algo;
		break;
	case RTE_CRYPTO_SYM_XFORM_AEAD:
		cap_idx.algo.aead = cap->aead.algo;
		break;
	default:
		rte_panic("Wrong capability profile type: %i\n", cap_idx.type);
		break;
	}

	return cap_idx;
}

/* Set the biggest common range for all capability fields */
static int
common_capability_set(struct rte_cryptodev_symmetric_capability *dst,
		       const struct rte_cryptodev_symmetric_capability *src)
{
	switch (src->xform_type) {
	case RTE_CRYPTO_SYM_XFORM_AUTH:
		if (dst->auth.algo != src->auth.algo)
			return -ENOENT;
		if (dst->auth.block_size != src->auth.block_size)
			return -ENOENT;
		if (common_range_set(&dst->auth.key_size, &src->auth.key_size))
			return -ENOENT;
		if (common_range_set(&dst->auth.digest_size, &src->auth.digest_size))
			return -ENOENT;
		if (common_range_set(&dst->auth.aad_size, &src->auth.aad_size))
			return -ENOENT;
		if (common_range_set(&dst->auth.iv_size, &src->auth.iv_size))
			return -ENOENT;
		break;
	case RTE_CRYPTO_SYM_XFORM_CIPHER:
		if (dst->cipher.algo != src->cipher.algo)
			return -ENOENT;
		if (dst->cipher.block_size != src->cipher.block_size)
			return -ENOENT;
		if (common_range_set(&dst->cipher.key_size, &src->cipher.key_size))
			return -ENOENT;
		if (common_range_set(&dst->cipher.iv_size, &src->cipher.iv_size))
			return -ENOENT;
		if (dst->cipher.dataunit_set != src->cipher.dataunit_set)
			return -ENOENT;
		break;
	case RTE_CRYPTO_SYM_XFORM_AEAD:
		if (dst->aead.algo != src->aead.algo)
			return -ENOENT;
		if (dst->aead.block_size != src->aead.block_size)
			return -ENOENT;
		if (common_range_set(&dst->aead.key_size, &src->aead.key_size))
			return -ENOENT;
		if (common_range_set(&dst->aead.digest_size, &src->aead.digest_size))
			return -ENOENT;
		if (common_range_set(&dst->aead.aad_size, &src->aead.aad_size))
			return -ENOENT;
		if (common_range_set(&dst->aead.iv_size, &src->aead.iv_size))
			return -ENOENT;
		break;
	default:
		RTE_LOG(ERR, USER1, "Unsupported xform_type!\n");
		return -ENOENT;
	}

	return 0;
}

static int
capabilities_inspect(void)
{
	struct crosscheck_testsuite_params *ts_params = &testsuite_params;
	const struct rte_cryptodev_symmetric_capability *next_dev_cap;
	struct rte_cryptodev_symmetric_capability common_cap;
	struct rte_cryptodev_sym_capability_idx cap_idx;
	const struct rte_cryptodev_capabilities *cap;
	struct rte_cryptodev_info dev_info;
	uint16_t nb_caps, cap_i = 0;
	uint8_t cdev_id, i;

	/* Get list of capabilities of first device */
	cdev_id = ts_params->valid_devs[0];
	rte_cryptodev_info_get(cdev_id, &dev_info);
	cap = dev_info.capabilities;
	nb_caps = nb_sym_capabilities_get(cap);
	common_symm_capas = rte_calloc(NULL, nb_caps,
				       sizeof(struct rte_cryptodev_symmetric_capability), 0);
	if (common_symm_capas == NULL)
		return -ENOMEM;

	for (; cap->op != RTE_CRYPTO_OP_TYPE_UNDEFINED; cap++) {
		/* Skip non symmetric capabilities */
		if (cap->op != RTE_CRYPTO_OP_TYPE_SYMMETRIC)
			continue;
		/* AES_CCM requires special handling due to api requirements, skip now */
		if (cap->sym.xform_type == RTE_CRYPTO_SYM_XFORM_AEAD &&
				cap->sym.aead.algo == RTE_CRYPTO_AEAD_AES_CCM)
			continue;

		cap_idx = sym_capability_to_idx(&cap->sym);
		common_cap = cap->sym;
		for (i = 1; i < ts_params->valid_dev_count; i++) {
			cdev_id = ts_params->valid_devs[i];
			next_dev_cap = rte_cryptodev_sym_capability_get(cdev_id, &cap_idx);
			/* Capability not supported by one of devs, skip */
			if (next_dev_cap == NULL)
				goto skip;
			/* Check if capabilities have a common range of values */
			if (common_capability_set(&common_cap, next_dev_cap) != 0)
				goto skip;
		}

		/* If capability reach this point - it's support by all cryptodevs */
		common_symm_capas[cap_i++] = common_cap;
skip:;
	}
	nb_common_sym_caps = cap_i;

	return 0;
}

static int
crosscheck_init(void)
{
	struct crosscheck_testsuite_params *ts_params = &testsuite_params;
	const struct rte_cryptodev_symmetric_capability *cap;
	const uint32_t nb_queue_pairs = 1;
	struct rte_cryptodev_info info;
	uint32_t session_priv_size = 0;
	uint32_t nb_devs, dev_id;
	uint8_t i;

	memset(ts_params, 0, sizeof(*ts_params));

	/* Create list of valid crypto devs */
	nb_devs = rte_cryptodev_count();
	for (dev_id = 0; dev_id < nb_devs; dev_id++) {
		rte_cryptodev_info_get(dev_id, &info);

		if (info.sym.max_nb_sessions != 0 && info.sym.max_nb_sessions < MAX_NB_SESSIONS)
			continue;
		if (info.max_nb_queue_pairs < nb_queue_pairs)
			continue;
		ts_params->valid_devs[ts_params->valid_dev_count++] = dev_id;
		/* Obtaining configuration parameters, that will satisfy all cryptodevs */
		session_priv_size = RTE_MAX(session_priv_size,
					    rte_cryptodev_sym_get_private_session_size(dev_id));
	}

	if (ts_params->valid_dev_count < 2) {
		RTE_LOG(WARNING, USER1, "Min number of cryptodevs for test is 2, found (%d)\n",
			ts_params->valid_dev_count);
		return TEST_SKIPPED;
	}

	/* Create pools for mbufs, crypto operations and sessions */
	ts_params->mbuf_pool = rte_pktmbuf_pool_create("CRYPTO_MBUFPOOL", NUM_MBUFS,
			MBUF_CACHE_SIZE, 0, MBUF_SIZE, rte_socket_id());
	if (ts_params->mbuf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Can't create CRYPTO_MBUFPOOL\n");
		return TEST_FAILED;
	}

	ts_params->op_mpool = rte_crypto_op_pool_create("MBUF_CRYPTO_SYM_OP_POOL",
			RTE_CRYPTO_OP_TYPE_SYMMETRIC, NUM_MBUFS, MBUF_CACHE_SIZE,
			DEFAULT_NUM_XFORMS * sizeof(struct rte_crypto_sym_xform) +
			MAXIMUM_IV_LENGTH, rte_socket_id());

	if (ts_params->op_mpool == NULL) {
		RTE_LOG(ERR, USER1, "Can't create CRYPTO_OP_POOL\n");
		return TEST_FAILED;
	}

	ts_params->session_mpool = rte_cryptodev_sym_session_pool_create("test_sess_mp",
			MAX_NB_SESSIONS, session_priv_size, 0, 0, SOCKET_ID_ANY);
	TEST_ASSERT_NOT_NULL(ts_params->session_mpool, "session mempool allocation failed");

	/* Setup queue pair conf params */
	ts_params->conf.nb_queue_pairs = nb_queue_pairs;
	ts_params->conf.socket_id = SOCKET_ID_ANY;
	ts_params->conf.ff_disable = RTE_CRYPTODEV_FF_SECURITY;
	ts_params->qp_conf.nb_descriptors = MAX_NUM_OPS_INFLIGHT;
	ts_params->qp_conf.mp_session = ts_params->session_mpool;

	if (capabilities_inspect() != 0)
		return TEST_FAILED;

	/* Allocate memory based on max supported capabilities */
	for (i = 0; i < nb_common_sym_caps; i++) {
		cap = &common_symm_capas[i];
		switch (cap->xform_type) {
		case RTE_CRYPTO_SYM_XFORM_AUTH:
			ts_params->key.len = RTE_MAX(ts_params->key.len, cap->auth.key_size.max);
			ts_params->digest.len = RTE_MAX(ts_params->digest.len,
							cap->auth.digest_size.max);
			ts_params->aad.len = RTE_MAX(ts_params->aad.len, cap->auth.aad_size.max);
			ts_params->iv.len = RTE_MAX(ts_params->iv.len, cap->auth.iv_size.max);
			break;
		case RTE_CRYPTO_SYM_XFORM_CIPHER:
			ts_params->key.len = RTE_MAX(ts_params->key.len, cap->cipher.key_size.max);
			ts_params->iv.len = RTE_MAX(ts_params->iv.len, cap->cipher.iv_size.max);
			break;
		case RTE_CRYPTO_SYM_XFORM_AEAD:
			ts_params->key.len = RTE_MAX(ts_params->key.len, cap->aead.key_size.max);
			ts_params->digest.len = RTE_MAX(ts_params->digest.len,
							cap->aead.digest_size.max);
			ts_params->aad.len = RTE_MAX(ts_params->aad.len, cap->aead.aad_size.max);
			ts_params->iv.len = RTE_MAX(ts_params->iv.len, cap->aead.iv_size.max);
			break;
		default:
			rte_panic("Wrong capability profile type: %i\n", cap->xform_type);
			break;
		}
	}

	if (ts_params->key.len) {
		ts_params->key.mem = rte_zmalloc(NULL, ts_params->key.len, 0);
		TEST_ASSERT_NOT_NULL(ts_params->key.mem, "Key mem allocation failed\n");
		pattern_fill(ts_params->key.mem, "*Secret key*", ts_params->key.len);
	}
	if (ts_params->digest.len) {
		ts_params->digest.mem = rte_zmalloc(NULL, ts_params->digest.len, 16);
		TEST_ASSERT_NOT_NULL(ts_params->digest.mem, "digest mem allocation failed\n");
		ts_params->expected_digest.len = ts_params->digest.len;
		ts_params->expected_digest.mem = rte_zmalloc(NULL, ts_params->digest.len, 0);
		TEST_ASSERT_NOT_NULL(ts_params->expected_digest.mem,
				     "Expected digest allocation failed\n");
	}
	if (ts_params->aad.len) {
		ts_params->aad.mem = rte_zmalloc(NULL, ts_params->aad.len, 16);
		TEST_ASSERT_NOT_NULL(ts_params->aad.mem, "aad mem allocation failed\n");
		ts_params->expected_aad.len = ts_params->aad.len;
		ts_params->expected_aad.mem = rte_zmalloc(NULL, ts_params->expected_aad.len, 0);
		TEST_ASSERT_NOT_NULL(ts_params->expected_aad.mem,
				     "Expected aad allocation failed\n");
	}
	if (ts_params->iv.len) {
		ts_params->iv.mem = rte_zmalloc(NULL, ts_params->iv.len, 0);
		TEST_ASSERT_NOT_NULL(ts_params->iv.mem, "iv mem allocation failed\n");
		pattern_fill(ts_params->iv.mem, "IV", ts_params->iv.len);
	}

	return TEST_SUCCESS;
}

static void
crosscheck_fini(void)
{
	struct crosscheck_testsuite_params *ts_params = &testsuite_params;

	rte_mempool_free(ts_params->mbuf_pool);
	rte_mempool_free(ts_params->op_mpool);
	rte_mempool_free(ts_params->session_mpool);
	rte_free(ts_params->key.mem);
	rte_free(ts_params->digest.mem);
	rte_free(ts_params->aad.mem);
	rte_free(ts_params->iv.mem);
}

static int
dev_configure_and_start(uint64_t ff_disable)
{
	struct crosscheck_testsuite_params *ts_params = &testsuite_params;
	uint8_t i, dev_id;
	uint16_t qp_id;

	/* Reconfigure device to default parameters */
	ts_params->conf.ff_disable = ff_disable;

	/* Configure cryptodevs */
	for (i = 0; i < ts_params->valid_dev_count; i++) {
		dev_id = ts_params->valid_devs[i];
		TEST_ASSERT_SUCCESS(rte_cryptodev_configure(dev_id, &ts_params->conf),
				    "Failed to configure cryptodev %u with %u qps",
				    dev_id, ts_params->conf.nb_queue_pairs);

		for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
			TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
				dev_id, qp_id, &ts_params->qp_conf,
				rte_cryptodev_socket_id(dev_id)),
				"Failed to setup queue pair %u on cryptodev %u",
				qp_id, dev_id);
		}
		rte_cryptodev_stats_reset(dev_id);

		/* Start the device */
		TEST_ASSERT_SUCCESS(rte_cryptodev_start(dev_id), "Failed to start cryptodev %u",
				    dev_id);
	}

	return TEST_SUCCESS;
}

static int
crosscheck_suite_setup(void)
{
	dev_configure_and_start(RTE_CRYPTODEV_FF_SECURITY);

	return 0;
}

static void
crosscheck_suite_teardown(void)
{
	struct crosscheck_testsuite_params *ts_params = &testsuite_params;
	uint8_t i, dev_id;

	for (i = 0; i < ts_params->valid_dev_count; i++) {
		dev_id = ts_params->valid_devs[i];
		rte_cryptodev_stop(dev_id);
	}
}

static struct rte_crypto_op *
crypto_request_process(uint8_t dev_id, struct rte_crypto_op *op)
{
	struct rte_crypto_op *res = NULL;

	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1, "Error sending packet for encryption\n");
		return NULL;
	}

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &res, 1) == 0)
		rte_pause();

	if (res->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, USER1, "Operation status %d\n", res->status);
		return NULL;
	}

	if (res != op) {
		RTE_LOG(ERR, USER1, "Unexpected operation received!\n");
		rte_crypto_op_free(res);
		return NULL;
	}

	return res;
}

static struct rte_cryptodev_sym_session*
session_create(const struct crosscheck_test_profile *profile, uint8_t dev_id,
	       enum crypto_op_type op_type)
{
	struct crosscheck_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_sym_session *session;
	struct rte_crypto_sym_xform xform;

	memset(&xform, 0, sizeof(xform));

	switch (profile->xform_type) {
	case RTE_CRYPTO_SYM_XFORM_AUTH:
		xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
		xform.next = NULL;
		xform.auth.algo = profile->algo;
		xform.auth.op = op_type == OP_ENCRYPT ? RTE_CRYPTO_AUTH_OP_GENERATE :
			RTE_CRYPTO_AUTH_OP_VERIFY;
		xform.auth.digest_length = profile->digest_size;
		xform.auth.key.length = profile->key_size;
		xform.auth.key.data = ts_params->key.mem;
		xform.auth.iv.length = profile->iv_size;
		xform.auth.iv.offset = IV_OFFSET;
		break;
	case RTE_CRYPTO_SYM_XFORM_CIPHER:
		xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
		xform.next = NULL;
		xform.cipher.algo = profile->algo;
		xform.cipher.op = op_type == OP_ENCRYPT ? RTE_CRYPTO_CIPHER_OP_ENCRYPT :
			RTE_CRYPTO_CIPHER_OP_DECRYPT;
		xform.cipher.key.length = profile->key_size;
		xform.cipher.key.data = ts_params->key.mem;
		xform.cipher.iv.length = profile->iv_size;
		xform.cipher.iv.offset = IV_OFFSET;
		break;
	case RTE_CRYPTO_SYM_XFORM_AEAD:
		xform.type = RTE_CRYPTO_SYM_XFORM_AEAD;
		xform.next = NULL;
		xform.aead.algo = profile->algo;
		xform.aead.op = op_type == OP_ENCRYPT ? RTE_CRYPTO_AEAD_OP_ENCRYPT :
			RTE_CRYPTO_AEAD_OP_DECRYPT;
		xform.aead.digest_length = profile->digest_size;
		xform.aead.key.length = profile->key_size;
		xform.aead.key.data = ts_params->key.mem;
		xform.aead.iv.length = profile->iv_size;
		xform.aead.iv.offset = IV_OFFSET;
		xform.aead.aad_length = profile->aad_size;
		break;
	default:
		return NULL;
	}

	session = rte_cryptodev_sym_session_create(dev_id, &xform, testsuite_params.session_mpool);

	return session;
}

static struct rte_mbuf*
mbuf_create(const uint8_t *input_buf, uint16_t input_len)
{
	struct rte_mbuf *pkt;
	uint8_t *pkt_data;

	pkt = rte_pktmbuf_alloc(testsuite_params.mbuf_pool);
	if (pkt == NULL) {
		RTE_LOG(ERR, USER1,  "Failed to allocate input buffer in mempool");
		return NULL;
	}

	/* zeroing tailroom */
	memset(rte_pktmbuf_mtod(pkt, uint8_t *), 0, rte_pktmbuf_tailroom(pkt));

	pkt_data = (uint8_t *)rte_pktmbuf_append(pkt, input_len);
	if (pkt_data == NULL) {
		RTE_LOG(ERR, USER1, "no room to append data, len: %d", input_len);
		goto error;
	}
	rte_memcpy(pkt_data, input_buf, input_len);

	return pkt;
error:
	rte_pktmbuf_free(pkt);
	return NULL;
}

static struct rte_crypto_op*
operation_create(const struct crosscheck_test_profile *profile,
		 struct rte_mbuf *ibuf, enum crypto_op_type op_type)
{
	struct crosscheck_testsuite_params *ts_params = &testsuite_params;
	uint8_t *digest_data = NULL, *aad_data = NULL, *iv_ptr = NULL;
	uint16_t aad_size, digest_size, plaintext_len;
	struct rte_crypto_sym_op *sym_op;
	struct rte_crypto_op *op;

	op = rte_crypto_op_alloc(ts_params->op_mpool, RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	if (op == NULL) {
		RTE_LOG(ERR, USER1, "Failed to allocate symmetric crypto operation struct");
		return NULL;
	}

	plaintext_len = profile->input_buf_len;
	aad_size = profile->aad_size;
	digest_size = profile->digest_size;

	if (aad_size) {
		aad_data = ts_params->aad.mem;
		if (op_type == OP_ENCRYPT)
			pattern_fill(aad_data, "This is an aad.", aad_size);
	}

	if (digest_size) {
		digest_data = ts_params->digest.mem;
		if (op_type == OP_ENCRYPT)
			memset(digest_data, 0, sizeof(digest_size));
	}

	sym_op = op->sym;
	memset(sym_op, 0, sizeof(*sym_op));

	iv_ptr = rte_crypto_op_ctod_offset(op, uint8_t *, IV_OFFSET);
	rte_memcpy(iv_ptr, ts_params->iv.mem, profile->iv_size);

	switch (profile->xform_type) {
	case RTE_CRYPTO_SYM_XFORM_AUTH:
		sym_op->auth.digest.data = digest_data;
		sym_op->auth.digest.phys_addr = rte_malloc_virt2iova(sym_op->auth.digest.data);
		sym_op->auth.data.length = plaintext_len;
		break;
	case RTE_CRYPTO_SYM_XFORM_CIPHER:
		sym_op->cipher.data.length = plaintext_len;
		break;
	case RTE_CRYPTO_SYM_XFORM_AEAD:
		sym_op->aead.aad.data = aad_data;
		sym_op->aead.aad.phys_addr = rte_malloc_virt2iova(sym_op->aead.aad.data);
		sym_op->aead.digest.data = digest_data;
		sym_op->aead.digest.phys_addr = rte_malloc_virt2iova(sym_op->aead.digest.data);
		sym_op->aead.data.offset = 0;
		sym_op->aead.data.length = plaintext_len;
		break;
	default:
		goto error;
	}

	sym_op->m_src = ibuf;

	return op;

error:
	rte_crypto_op_free(op);
	return NULL;
}

static void
mbuf_to_buf_copy(const struct rte_mbuf *m, uint8_t *res_buf, uint16_t *len)
{
	const uint8_t *out;

	*len = m->pkt_len;
	out = rte_pktmbuf_read(m, 0, *len, res_buf);
	/* Single segment buffer */
	if (out != res_buf)
		memcpy(res_buf, out, *len);
}

static int
single_dev_process(const struct crosscheck_test_profile *profile, uint16_t dev_id, enum
		   crypto_op_type op_type, const uint8_t *input_buf, uint16_t input_len,
		   uint8_t *output_buf, uint16_t *output_len)
{
	struct rte_cryptodev_sym_session *session = NULL;
	struct rte_mbuf *ibuf = NULL, *obuf = NULL;
	struct rte_crypto_op *op = NULL;
	int ret = -1;

	session = session_create(profile, dev_id, op_type);
	if (session == NULL)
		goto error;

	ibuf = mbuf_create(input_buf, input_len);
	if (ibuf == NULL)
		goto error;

	op = operation_create(profile, ibuf, op_type);
	if (op == NULL)
		goto error;

	debug_hexdump(stdout, "Input:", rte_pktmbuf_mtod(ibuf, uint8_t*), ibuf->pkt_len);

	rte_crypto_op_attach_sym_session(op, session);

	struct rte_crypto_op *res = crypto_request_process(dev_id, op);
	if (res == NULL)
		goto error;

	obuf = op->sym->m_src;
	if (obuf == NULL) {
		RTE_LOG(ERR, USER1, "Invalid packet received\n");
		goto error;
	}
	mbuf_to_buf_copy(obuf, output_buf, output_len);

	ret = 0;

error:
	if (session != NULL) {
		int sret;
		sret = rte_cryptodev_sym_session_free(dev_id, session);
		RTE_VERIFY(sret == 0);
	}
	rte_pktmbuf_free(ibuf);
	rte_crypto_op_free(op);
	return ret;
}

static int
buffers_compare(const uint8_t *expected, uint16_t expected_len,
		const uint8_t *received, uint16_t received_len)
{
	TEST_ASSERT_EQUAL(expected_len, received_len, "Length mismatch %d != %d !\n",
			  expected_len, received_len);

	if (memcmp(expected, received, expected_len)) {
		rte_hexdump(rte_log_get_stream(), "expected", expected, expected_len);
		rte_hexdump(rte_log_get_stream(), "received", received, expected_len);
		return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static int
crosscheck_all_devices(const struct crosscheck_test_profile *profile, enum crypto_op_type op_type,
		       const uint8_t *input_text, uint16_t input_len, uint8_t *output_text,
		       uint16_t *output_len)
{
	struct crosscheck_testsuite_params *ts_params = &testsuite_params;
	uint16_t len = 0, expected_len = 0;
	uint8_t expected_text[MBUF_SIZE];
	uint8_t i, dev_id;
	int status;


	for (i = 0; i < ts_params->valid_dev_count; i++) {
		dev_id = ts_params->valid_devs[i];
		status = single_dev_process(profile, dev_id, op_type, input_text, input_len,
					    output_text, &len);
		TEST_ASSERT_SUCCESS(status, "Error occurred during processing");

		if (i == 0) {
			/* First device, copy data for future comparisons */
			memcpy(expected_text, output_text, len);
			memcpy(ts_params->expected_digest.mem, ts_params->digest.mem,
			       profile->digest_size);
			memcpy(ts_params->expected_aad.mem, ts_params->aad.mem, profile->aad_size);
			expected_len = len;
		} else {
			/* Compare output against expected(first) output */
			TEST_ASSERT_SUCCESS(buffers_compare(expected_text, expected_len,
					output_text, len),
					"Text mismatch occurred on dev %i\n", dev_id);
			TEST_ASSERT_SUCCESS(buffers_compare(ts_params->expected_digest.mem,
					profile->digest_size, ts_params->digest.mem,
					profile->digest_size),
					"Digest mismatch occurred on dev %i\n", dev_id);
			TEST_ASSERT_SUCCESS(buffers_compare(ts_params->expected_aad.mem,
					profile->aad_size, ts_params->aad.mem, profile->aad_size),
					"AAD mismatch occurred on dev %i\n", dev_id);
		}

		RTE_LOG(DEBUG, USER1, "DEV ID: %u finished processing\n", dev_id);
		debug_hexdump(stdout, "Output: ", output_text, len);
		if (profile->digest_size)
			debug_hexdump(stdout, "Digest: ", ts_params->digest.mem,
				      profile->digest_size);
	}

	*output_len = len;

	return TEST_SUCCESS;
}

static int
check_negative_all_devices(const struct crosscheck_test_profile *profile,
			   enum crypto_op_type op_type, const uint8_t *input_text,
			   uint16_t input_len)
{
	struct crosscheck_testsuite_params *ts_params = &testsuite_params;

	uint8_t output_text[MBUF_SIZE];
	uint8_t i, dev_id;
	uint16_t len;
	int status;

	for (i = 0; i < ts_params->valid_dev_count; i++) {
		dev_id = ts_params->valid_devs[i];
		status = single_dev_process(profile, dev_id, op_type, input_text, input_len,
					    output_text, &len);
		TEST_ASSERT_FAIL(status, "Error occurred during processing negative case");

	}

	return TEST_SUCCESS;
}

static int
crosscheck_with_profile_run(const struct crosscheck_test_profile *profile)
{
	struct crosscheck_testsuite_params *ts_params = &testsuite_params;
	uint8_t input_text[profile->input_buf_len];
	uint16_t output_len, encrypted_len;
	uint8_t encrypted_text[MBUF_SIZE];
	uint8_t output_text[MBUF_SIZE];
	int status;

	memset(ts_params->digest.mem, 0, ts_params->digest.len);
	memset(ts_params->aad.mem, 0, ts_params->aad.len);

	/* Encrypt Stage */
	RTE_LOG(DEBUG, USER1, "Executing encrypt stage\n");
	/* Fill input with incrementing pattern */
	incrementing_generate(input_text, 'a', profile->input_buf_len);
	status = crosscheck_all_devices(profile, OP_ENCRYPT, input_text, profile->input_buf_len,
					output_text, &output_len);
	TEST_ASSERT_SUCCESS(status, "Error occurred during encryption");

	/* Decrypt Stage */
	RTE_LOG(DEBUG, USER1, "Executing decrypt stage\n");
	/* Set up encrypted data as input */
	encrypted_len = output_len;
	memcpy(encrypted_text, output_text, output_len);
	status = crosscheck_all_devices(profile, OP_DECRYPT, encrypted_text, encrypted_len,
					output_text, &output_len);
	TEST_ASSERT_SUCCESS(status, "Error occurred during decryption");

	/* Negative Stage */
	RTE_LOG(DEBUG, USER1, "Executing negative stage\n");
	if (profile->digest_size) {
		/* Corrupting one byte of digest */
		ts_params->digest.mem[profile->digest_size - 1] += 1;
		status = check_negative_all_devices(profile, OP_DECRYPT, encrypted_text,
						    encrypted_len);
		TEST_ASSERT_SUCCESS(status, "Error occurred during decryption");
	}


	return TEST_SUCCESS;
}

static int
test_crosscheck_unit(const void *ptr)
{
	const struct crosscheck_test_profile *profile = ptr;

	if (profile->xform_type == RTE_CRYPTO_SYM_XFORM_NOT_SPECIFIED)
		return TEST_SKIPPED;

	return crosscheck_with_profile_run(profile);
}

static struct unit_test_suite*
sym_unit_test_suite_create(const struct rte_cryptodev_symmetric_capability *capa)
{
	size_t uts_size, total_size, input_sz;
	struct meta_test_suite *meta_ts;
	const char *suite_prefix = NULL;
	const char *algo_name = NULL;
	struct unit_test_suite *uts;
	uint64_t test_case_idx = 0;
	struct unit_test_case *utc;
	int cap_type;
	char *mem;

	const char * const suite_prefix_strings[] = {
		[RTE_CRYPTO_SYM_XFORM_AUTH] = "Algo AUTH ",
		[RTE_CRYPTO_SYM_XFORM_CIPHER] = "Algo CIPHER ",
		[RTE_CRYPTO_SYM_XFORM_AEAD] = "Algo AEAD ",
	};

	suite_prefix = suite_prefix_strings[capa->xform_type];
	algo_name = algo_name_get(capa);

	/* Calculate size for test suite with all test cases +1 NULL case */
	uts_size = sizeof(struct unit_test_suite) +
		(NB_TEST_CASES_PER_ALGO + 1) * sizeof(struct unit_test_case);

	/* Also allocate memory for suite meta data */
	total_size = uts_size + sizeof(struct meta_test_suite);
	mem = rte_zmalloc(NULL, total_size, 0);
	if (mem == NULL)
		return NULL;
	uts = (struct unit_test_suite *) mem;
	meta_ts = (struct meta_test_suite *) (mem + uts_size);

	/* Initialize test suite */
	snprintf(meta_ts->suite_name, MAX_TEST_STRING_LEN, "%s '%s'", suite_prefix, algo_name);
	uts->suite_name = meta_ts->suite_name;

	/* Initialize test cases */
	for (cap_type = 0; cap_type < CAPABILITY_TYPE_LAST; cap_type++) {
		for (input_sz = 0; input_sz < RTE_DIM(input_length); input_sz++) {
			meta_ts->profile[test_case_idx] = profile_create(
					capa, cap_type, input_length[input_sz]);
			utc = &uts->unit_test_cases[test_case_idx];
			utc->name = meta_ts->profile[test_case_idx].name;
			utc->data = (const void *) &meta_ts->profile[test_case_idx];
			utc->testcase_with_data = test_crosscheck_unit;
			utc->enabled = true;

			test_case_idx += 1;
			RTE_VERIFY(test_case_idx <= NB_TEST_CASES_PER_ALGO);
		}
	}

	return uts;
}

static int
test_crosscheck(void)
{
	struct unit_test_suite **test_suites = NULL;
	int ret, i;

	static struct unit_test_suite ts = {
		.suite_name = "Crosscheck Unit Test Suite",
		.setup = crosscheck_suite_setup,
		.teardown = crosscheck_suite_teardown,
		.unit_test_cases = {TEST_CASES_END()}
	};

	ret = crosscheck_init();
	if (ret)
		goto exit;

	if (nb_common_sym_caps == 0) {
		RTE_LOG(WARNING, USER1, "Cryptodevs don't have common capabilities\n");
		ret = TEST_SKIPPED;
		goto exit;
	}

	/* + 1 for NULL-end suite */
	test_suites = rte_calloc(NULL, nb_common_sym_caps + 1, sizeof(struct unit_test_suite *), 0);
	TEST_ASSERT_NOT_NULL(test_suites, "test_suites allocation failed");

	/* Create test suite for each supported algorithm */
	ts.unit_test_suites = test_suites;
	for (i = 0; i < nb_common_sym_caps; i++)
		ts.unit_test_suites[i] = sym_unit_test_suite_create(&common_symm_capas[i]);

	ret = unit_test_suite_runner(&ts);

	for (i = 0; i < nb_common_sym_caps; i++)
		rte_free(ts.unit_test_suites[i]);

	rte_free(test_suites);

exit:
	crosscheck_fini();

	return ret;
}

REGISTER_TEST_COMMAND(cryptodev_crosscheck, test_crosscheck);
