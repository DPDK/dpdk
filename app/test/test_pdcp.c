/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_bitmap.h>
#include <rte_errno.h>
#ifdef RTE_LIB_EVENTDEV
#include <rte_eventdev.h>
#include <rte_event_timer_adapter.h>
#endif /* RTE_LIB_EVENTDEV */
#include <rte_malloc.h>
#include <rte_pdcp.h>
#include <rte_pdcp_hdr.h>
#include <rte_timer.h>

#include "test.h"
#include "test_cryptodev.h"
#include "test_cryptodev_security_pdcp_test_vectors.h"

#define NSECPERSEC 1E9
#define NB_DESC 1024
#define TIMER_ADAPTER_ID 0
#define TEST_EV_QUEUE_ID 0
#define TEST_EV_PORT_ID 0
#define CDEV_INVALID_ID UINT8_MAX
#define NB_TESTS RTE_DIM(pdcp_test_params)
#define PDCP_IV_LEN 16

/* Assert that condition is true, or goto the mark */
#define ASSERT_TRUE_OR_GOTO(cond, mark, ...) do {\
	if (!(cond)) { \
		RTE_LOG(ERR, USER1, "Error at: %s:%d\n", __func__, __LINE__); \
		RTE_LOG(ERR, USER1, __VA_ARGS__); \
		goto mark; \
	} \
} while (0)

/* According to formula(7.2.a Window_Size) */
#define PDCP_WINDOW_SIZE(sn_size) (1 << (sn_size - 1))

struct pdcp_testsuite_params {
	struct rte_mempool *mbuf_pool;
	struct rte_mempool *cop_pool;
	struct rte_mempool *sess_pool;
	bool cdevs_used[RTE_CRYPTO_MAX_DEVS];
	int evdev;
#ifdef RTE_LIB_EVENTDEV
	struct rte_event_timer_adapter *timdev;
#endif /* RTE_LIB_EVENTDEV */
	bool timer_is_running;
	uint64_t min_resolution_ns;
	struct rte_pdcp_up_ctrl_pdu_hdr *status_report;
	uint32_t status_report_bitmask_capacity;
	uint8_t *ctrl_pdu_buf;
};

static struct pdcp_testsuite_params testsuite_params;

struct test_rte_timer_args {
	int status;
	struct rte_pdcp_entity *pdcp_entity;
};

struct pdcp_test_conf {
	struct rte_pdcp_entity_conf entity;
	struct rte_crypto_sym_xform c_xfrm;
	struct rte_crypto_sym_xform a_xfrm;
	bool is_integrity_protected;
	uint8_t input[RTE_PDCP_CTRL_PDU_SIZE_MAX];
	uint32_t input_len;
	uint8_t output[RTE_PDCP_CTRL_PDU_SIZE_MAX];
	uint32_t output_len;
};

static int create_test_conf_from_index(const int index, struct pdcp_test_conf *conf);

typedef int (*test_with_conf_t)(struct pdcp_test_conf *conf);

static int
run_test_foreach_known_vec(test_with_conf_t test, bool stop_on_first_pass)
{
	struct pdcp_test_conf test_conf;
	bool all_tests_skipped = true;
	uint32_t i;
	int ret;

	for (i = 0; i < NB_TESTS; i++) {
		create_test_conf_from_index(i, &test_conf);
		ret = test(&test_conf);

		if (ret == TEST_FAILED) {
			printf("[%03i] - %s - failed\n", i, pdcp_test_params[i].name);
			return TEST_FAILED;
		}

		if ((ret == TEST_SKIPPED) || (ret == -ENOTSUP))
			continue;

		if (stop_on_first_pass)
			return TEST_SUCCESS;

		all_tests_skipped = false;
	}

	if (all_tests_skipped)
		return TEST_SKIPPED;

	return TEST_SUCCESS;
}

static int
run_test_with_all_known_vec(const void *args)
{
	test_with_conf_t test = args;

	return run_test_foreach_known_vec(test, false);
}

static int
run_test_with_all_known_vec_until_first_pass(const void *args)
{
	test_with_conf_t test = args;

	return run_test_foreach_known_vec(test, true);
}

static inline uint32_t
pdcp_sn_mask_get(enum rte_security_pdcp_sn_size sn_size)
{
	return (1 << sn_size) - 1;
}

static inline uint32_t
pdcp_sn_from_count_get(uint32_t count, enum rte_security_pdcp_sn_size sn_size)
{
	return (count & pdcp_sn_mask_get(sn_size));
}

static inline uint32_t
pdcp_hfn_mask_get(enum rte_security_pdcp_sn_size sn_size)
{
	return ~pdcp_sn_mask_get(sn_size);
}

static inline uint32_t
pdcp_hfn_from_count_get(uint32_t count, enum rte_security_pdcp_sn_size sn_size)
{
	return (count & pdcp_hfn_mask_get(sn_size)) >> sn_size;
}

static void
pdcp_timer_start_cb(void *timer, void *args)
{
	bool *is_timer_running = timer;

	RTE_SET_USED(args);
	*is_timer_running = true;
}

static void
pdcp_timer_stop_cb(void *timer, void *args)
{
	bool *is_timer_running = timer;

	RTE_SET_USED(args);
	*is_timer_running = false;
}

static struct rte_pdcp_t_reordering t_reorder_timer = {
	.timer = &testsuite_params.timer_is_running,
	.start = pdcp_timer_start_cb,
	.stop = pdcp_timer_stop_cb,
};

static inline void
bitmask_set_bit(uint8_t *mask, uint32_t bit)
{
	mask[bit / 8] |= (1 << bit % 8);
}

static inline bool
bitmask_is_bit_set(const uint8_t *mask, uint32_t bit)
{
	return mask[bit / 8] & (1 << (bit % 8));
}

static inline int
pdcp_hdr_size_get(enum rte_security_pdcp_sn_size sn_size)
{
	return RTE_ALIGN_MUL_CEIL(sn_size, 8) / 8;
}

static int
pktmbuf_read_into(const struct rte_mbuf *m, void *buf, size_t buf_len)
{
	if (m->pkt_len > buf_len)
		return -ENOMEM;

	const void *read = rte_pktmbuf_read(m, 0, m->pkt_len, buf);
	if (read != NULL && read != buf)
		memcpy(buf, read, m->pkt_len);

	return 0;
}

static int
cryptodev_init(int dev_id)
{
	struct pdcp_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_qp_conf qp_conf;
	struct rte_cryptodev_info dev_info;
	struct rte_cryptodev_config config;
	int ret, socket_id;

	/* Check if device was already initialized */
	if (ts_params->cdevs_used[dev_id])
		return 0;

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (dev_info.max_nb_queue_pairs < 1) {
		RTE_LOG(ERR, USER1, "Cryptodev doesn't have sufficient queue pairs available\n");
		return -ENODEV;
	}

	socket_id = rte_socket_id();

	memset(&config, 0, sizeof(config));
	config.nb_queue_pairs = 1;
	config.socket_id = socket_id;

	ret = rte_cryptodev_configure(dev_id, &config);
	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Could not configure cryptodev - %d\n", dev_id);
		return -ENODEV;
	}

	memset(&qp_conf, 0, sizeof(qp_conf));
	qp_conf.nb_descriptors = NB_DESC;

	ret = rte_cryptodev_queue_pair_setup(dev_id, 0, &qp_conf, socket_id);
	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Could not configure queue pair\n");
		return -ENODEV;
	}

	ret = rte_cryptodev_start(dev_id);
	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Could not start cryptodev\n");
		return -ENODEV;
	}

	/* Mark device as initialized */
	ts_params->cdevs_used[dev_id] = true;

	return 0;
}

static void
cryptodev_fini(int dev_id)
{
	rte_cryptodev_stop(dev_id);
}

static unsigned int
cryptodev_sess_priv_max_req_get(void)
{
	struct rte_cryptodev_info info;
	unsigned int sess_priv_sz;
	int i, nb_dev;
	void *sec_ctx;

	nb_dev = rte_cryptodev_count();

	sess_priv_sz = 0;

	for (i = 0; i < nb_dev; i++) {
		rte_cryptodev_info_get(i, &info);
		sess_priv_sz = RTE_MAX(sess_priv_sz, rte_cryptodev_sym_get_private_session_size(i));
		if (info.feature_flags & RTE_CRYPTODEV_FF_SECURITY) {
			sec_ctx = rte_cryptodev_get_sec_ctx(i);
			sess_priv_sz = RTE_MAX(sess_priv_sz,
					       rte_security_session_get_size(sec_ctx));
		}
	}

	return sess_priv_sz;
}

static int
testsuite_setup(void)
{
	struct pdcp_testsuite_params *ts_params = &testsuite_params;
	int nb_cdev, sess_priv_size, nb_sess = 1024;

	RTE_SET_USED(pdcp_test_hfn_threshold);

	nb_cdev = rte_cryptodev_count();
	if (nb_cdev < 1) {
		RTE_LOG(ERR, USER1, "No crypto devices found.\n");
		return TEST_SKIPPED;
	}

	memset(ts_params, 0, sizeof(*ts_params));

	ts_params->mbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", NUM_MBUFS, MBUF_CACHE_SIZE, 0,
						       MBUF_SIZE, SOCKET_ID_ANY);
	if (ts_params->mbuf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Could not create mbuf pool\n");
		return TEST_FAILED;
	}

	ts_params->cop_pool = rte_crypto_op_pool_create("cop_pool", RTE_CRYPTO_OP_TYPE_SYMMETRIC,
							 NUM_MBUFS, MBUF_CACHE_SIZE,
							 2 * MAXIMUM_IV_LENGTH, SOCKET_ID_ANY);
	if (ts_params->cop_pool == NULL) {
		RTE_LOG(ERR, USER1, "Could not create crypto_op pool\n");
		goto mbuf_pool_free;
	}

	/* Get max session priv size required */
	sess_priv_size = cryptodev_sess_priv_max_req_get();

	ts_params->sess_pool = rte_cryptodev_sym_session_pool_create("sess_pool", nb_sess,
								     sess_priv_size,
								     RTE_MEMPOOL_CACHE_MAX_SIZE,
								     0, SOCKET_ID_ANY);
	if (ts_params->sess_pool == NULL) {
		RTE_LOG(ERR, USER1, "Could not create session pool\n");
		goto cop_pool_free;
	}

	/* Allocate memory for longest possible status report */
	ts_params->status_report_bitmask_capacity = RTE_PDCP_CTRL_PDU_SIZE_MAX -
		sizeof(struct rte_pdcp_up_ctrl_pdu_hdr);
	ts_params->status_report = rte_zmalloc(NULL, RTE_PDCP_CTRL_PDU_SIZE_MAX, 0);
	if (ts_params->status_report == NULL) {
		RTE_LOG(ERR, USER1, "Could not allocate status report\n");
		goto cop_pool_free;
	}

	ts_params->ctrl_pdu_buf = rte_zmalloc(NULL, RTE_PDCP_CTRL_PDU_SIZE_MAX, 0);
	if (ts_params->ctrl_pdu_buf == NULL) {
		RTE_LOG(ERR, USER1, "Could not allocate status report data\n");
		goto cop_pool_free;
	}

	return 0;

cop_pool_free:
	rte_mempool_free(ts_params->cop_pool);
	ts_params->cop_pool = NULL;
mbuf_pool_free:
	rte_mempool_free(ts_params->mbuf_pool);
	ts_params->mbuf_pool = NULL;
	rte_free(ts_params->status_report);
	rte_free(ts_params->ctrl_pdu_buf);
	return TEST_FAILED;
}

static void
testsuite_teardown(void)
{
	struct pdcp_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id;

	for (dev_id = 0; dev_id < RTE_CRYPTO_MAX_DEVS; dev_id++) {
		if (ts_params->cdevs_used[dev_id])
			cryptodev_fini(dev_id);
	}

	rte_mempool_free(ts_params->sess_pool);
	ts_params->sess_pool = NULL;

	rte_mempool_free(ts_params->cop_pool);
	ts_params->cop_pool = NULL;

	rte_mempool_free(ts_params->mbuf_pool);
	ts_params->mbuf_pool = NULL;

	rte_free(ts_params->status_report);
	rte_free(ts_params->ctrl_pdu_buf);
}

static int
ut_setup_pdcp(void)
{
	return 0;
}

static void
ut_teardown_pdcp(void)
{
}

static int
crypto_caps_cipher_verify(uint8_t dev_id, const struct rte_crypto_sym_xform *c_xfrm)
{
	const struct rte_cryptodev_symmetric_capability *cap;
	struct rte_cryptodev_sym_capability_idx cap_idx;
	int ret;

	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = c_xfrm->cipher.algo;

	cap = rte_cryptodev_sym_capability_get(dev_id, &cap_idx);
	if (cap == NULL)
		return -1;

	ret = rte_cryptodev_sym_capability_check_cipher(cap, c_xfrm->cipher.key.length,
							c_xfrm->cipher.iv.length);

	return ret;
}

static int
crypto_caps_auth_verify(uint8_t dev_id, const struct rte_crypto_sym_xform *a_xfrm)
{
	const struct rte_cryptodev_symmetric_capability *cap;
	struct rte_cryptodev_sym_capability_idx cap_idx;
	int ret;

	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = a_xfrm->auth.algo;

	cap = rte_cryptodev_sym_capability_get(dev_id, &cap_idx);
	if (cap == NULL)
		return -1;

	ret = rte_cryptodev_sym_capability_check_auth(cap, a_xfrm->auth.key.length,
						      a_xfrm->auth.digest_length,
						      a_xfrm->auth.iv.length);

	return ret;
}

static int
cryptodev_id_get(bool is_integrity_protected, const struct rte_crypto_sym_xform *c_xfrm,
		 const struct rte_crypto_sym_xform *a_xfrm)
{
	int i, nb_devs;

	nb_devs = rte_cryptodev_count();

	/* Check capabilities */

	for (i = 0; i < nb_devs; i++) {
		if ((crypto_caps_cipher_verify(i, c_xfrm) == 0) &&
		    (!is_integrity_protected || crypto_caps_auth_verify(i, a_xfrm) == 0))
			break;
	}

	if (i == nb_devs)
		return -1;

	return i;
}

static int
pdcp_known_vec_verify(struct rte_mbuf *m, const uint8_t *expected, uint32_t expected_pkt_len)
{
	uint8_t *actual = rte_pktmbuf_mtod(m, uint8_t *);
	uint32_t actual_pkt_len = rte_pktmbuf_pkt_len(m);

	debug_hexdump(stdout, "Received:", actual, actual_pkt_len);
	debug_hexdump(stdout, "Expected:", expected, expected_pkt_len);

	TEST_ASSERT_EQUAL(actual_pkt_len, expected_pkt_len,
			  "Mismatch in packet lengths [expected: %d, received: %d]",
			  expected_pkt_len, actual_pkt_len);

	TEST_ASSERT_BUFFERS_ARE_EQUAL(actual, expected, expected_pkt_len,
				     "Generated packet not as expected");

	return 0;
}

static struct rte_crypto_op *
process_crypto_request(uint8_t dev_id, struct rte_crypto_op *op)
{
	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1, "Error sending packet to cryptodev\n");
		return NULL;
	}

	op = NULL;

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &op, 1) == 0)
		rte_pause();

	return op;
}

static uint32_t
pdcp_sn_from_raw_get(const void *data, enum rte_security_pdcp_sn_size size)
{
	uint32_t sn = 0;

	if (size == RTE_SECURITY_PDCP_SN_SIZE_12) {
		sn = rte_cpu_to_be_16(*(const uint16_t *)data);
		sn = sn & 0xfff;
	} else if (size == RTE_SECURITY_PDCP_SN_SIZE_18) {
		sn = rte_cpu_to_be_32(*(const uint32_t *)data);
		sn = (sn & 0x3ffff00) >> 8;
	}

	return sn;
}

static void
pdcp_sn_to_raw_set(void *data, uint32_t sn, int size)
{
	if (size == RTE_SECURITY_PDCP_SN_SIZE_12) {
		struct rte_pdcp_up_data_pdu_sn_12_hdr *pdu_hdr = data;
		pdu_hdr->sn_11_8 = ((sn & 0xf00) >> 8);
		pdu_hdr->sn_7_0 = (sn & 0xff);
	} else if (size == RTE_SECURITY_PDCP_SN_SIZE_18) {
		struct rte_pdcp_up_data_pdu_sn_18_hdr *pdu_hdr = data;
		pdu_hdr->sn_17_16 = ((sn & 0x30000) >> 16);
		pdu_hdr->sn_15_8 = ((sn & 0xff00) >> 8);
		pdu_hdr->sn_7_0 = (sn & 0xff);
	}
}

static int
create_test_conf_from_index(const int index, struct pdcp_test_conf *conf)
{
	const struct pdcp_testsuite_params *ts_params = &testsuite_params;
	struct rte_crypto_sym_xform c_xfrm, a_xfrm;
	uint32_t sn, expected_len;
	uint8_t *data, *expected;
	int pdcp_hdr_sz;

	memset(conf, 0, sizeof(*conf));
	memset(&c_xfrm, 0, sizeof(c_xfrm));
	memset(&a_xfrm, 0, sizeof(a_xfrm));

	conf->entity.sess_mpool = ts_params->sess_pool;
	conf->entity.cop_pool = ts_params->cop_pool;
	conf->entity.ctrl_pdu_pool = ts_params->mbuf_pool;
	conf->entity.pdcp_xfrm.bearer = pdcp_test_bearer[index];
	conf->entity.pdcp_xfrm.en_ordering = 0;
	conf->entity.pdcp_xfrm.remove_duplicates = 0;
	conf->entity.pdcp_xfrm.domain = pdcp_test_params[index].domain;
	conf->entity.t_reordering = t_reorder_timer;

	if (pdcp_test_packet_direction[index] == PDCP_DIR_UPLINK)
		conf->entity.pdcp_xfrm.pkt_dir = RTE_SECURITY_PDCP_UPLINK;
	else
		conf->entity.pdcp_xfrm.pkt_dir = RTE_SECURITY_PDCP_DOWNLINK;

	conf->entity.pdcp_xfrm.sn_size = pdcp_test_data_sn_size[index];

	/* Zero initialize unsupported flags */
	conf->entity.pdcp_xfrm.hfn_threshold = 0;
	conf->entity.pdcp_xfrm.hfn_ovrd = 0;
	conf->entity.pdcp_xfrm.sdap_enabled = 0;

	c_xfrm.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	c_xfrm.cipher.algo = pdcp_test_params[index].cipher_alg;
	c_xfrm.cipher.key.length = pdcp_test_params[index].cipher_key_len;
	c_xfrm.cipher.key.data = pdcp_test_crypto_key[index];

	a_xfrm.type = RTE_CRYPTO_SYM_XFORM_AUTH;

	if (pdcp_test_params[index].auth_alg == 0) {
		conf->is_integrity_protected = false;
	} else {
		a_xfrm.auth.algo = pdcp_test_params[index].auth_alg;
		a_xfrm.auth.key.data = pdcp_test_auth_key[index];
		a_xfrm.auth.key.length = pdcp_test_params[index].auth_key_len;
		conf->is_integrity_protected = true;
	}

	pdcp_hdr_sz = pdcp_hdr_size_get(pdcp_test_data_sn_size[index]);

	/*
	 * Uplink means PDCP entity is configured for transmit. Downlink means PDCP entity is
	 * configured for receive. When integrity protecting is enabled, PDCP always performs
	 * digest-encrypted or auth-gen-encrypt for uplink (and decrypt-auth-verify for downlink).
	 * So for uplink, crypto chain would be auth-cipher while for downlink it would be
	 * cipher-auth.
	 *
	 * When integrity protection is not required, xform would be cipher only.
	 */

	if (conf->is_integrity_protected) {
		if (conf->entity.pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_UPLINK) {
			conf->entity.crypto_xfrm = &conf->a_xfrm;

			a_xfrm.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
			a_xfrm.next = &conf->c_xfrm;

			c_xfrm.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
			c_xfrm.next = NULL;
		} else {
			conf->entity.crypto_xfrm = &conf->c_xfrm;

			c_xfrm.cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
			c_xfrm.next = &conf->a_xfrm;

			a_xfrm.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
			a_xfrm.next = NULL;
		}
	} else {
		conf->entity.crypto_xfrm = &conf->c_xfrm;
		c_xfrm.next = NULL;

		if (conf->entity.pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_UPLINK)
			c_xfrm.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
		else
			c_xfrm.cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
	}

	/* Update xforms to match PDCP requirements */

	if ((c_xfrm.cipher.algo == RTE_CRYPTO_CIPHER_AES_CTR) ||
	    (c_xfrm.cipher.algo == RTE_CRYPTO_CIPHER_ZUC_EEA3 ||
	    (c_xfrm.cipher.algo == RTE_CRYPTO_CIPHER_SNOW3G_UEA2)))
		c_xfrm.cipher.iv.length = PDCP_IV_LEN;
	else
		c_xfrm.cipher.iv.length = 0;

	if (conf->is_integrity_protected) {
		if (a_xfrm.auth.algo == RTE_CRYPTO_AUTH_NULL)
			a_xfrm.auth.digest_length = 0;
		else
			a_xfrm.auth.digest_length = RTE_PDCP_MAC_I_LEN;

		if ((a_xfrm.auth.algo == RTE_CRYPTO_AUTH_ZUC_EIA3) ||
		    (a_xfrm.auth.algo == RTE_CRYPTO_AUTH_SNOW3G_UIA2))
			a_xfrm.auth.iv.length = PDCP_IV_LEN;
		else
			a_xfrm.auth.iv.length = 0;
	}

	conf->c_xfrm = c_xfrm;
	conf->a_xfrm = a_xfrm;

	conf->entity.dev_id = (uint8_t)cryptodev_id_get(conf->is_integrity_protected,
			&conf->c_xfrm, &conf->a_xfrm);

	if (pdcp_test_params[index].domain == RTE_SECURITY_PDCP_MODE_CONTROL ||
	    pdcp_test_params[index].domain == RTE_SECURITY_PDCP_MODE_DATA) {
		data = pdcp_test_data_in[index];
		sn = pdcp_sn_from_raw_get(data, pdcp_test_data_sn_size[index]);
		conf->entity.pdcp_xfrm.hfn = pdcp_test_hfn[index];
		conf->entity.sn = sn;
	}

	if (conf->entity.pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_UPLINK) {
#ifdef VEC_DUMP
		debug_hexdump(stdout, "Original vector:", pdcp_test_data_in[index],
				pdcp_test_data_in_len[index]);
#endif
		/* Since the vectors available already have PDCP header, trim the same */
		conf->input_len = pdcp_test_data_in_len[index] - pdcp_hdr_sz;
		memcpy(conf->input, pdcp_test_data_in[index] + pdcp_hdr_sz, conf->input_len);
	} else {
		conf->input_len = pdcp_test_data_in_len[index];

		if (conf->is_integrity_protected)
			conf->input_len += RTE_PDCP_MAC_I_LEN;

		memcpy(conf->input, pdcp_test_data_out[index], conf->input_len);
#ifdef VEC_DUMP
		debug_hexdump(stdout, "Original vector:", conf->input, conf->input_len);
#endif
	}

	if (conf->entity.pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_UPLINK)
		expected = pdcp_test_data_out[index];
	else
		expected = pdcp_test_data_in[index];

	/* Calculate expected packet length */
	expected_len = pdcp_test_data_in_len[index];

	/* In DL processing, PDCP header would be stripped */
	if (conf->entity.pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_DOWNLINK) {
		expected += pdcp_hdr_sz;
		expected_len -= pdcp_hdr_sz;
	}

	/* In UL processing with integrity protection, MAC would be added */
	if (conf->is_integrity_protected &&
	    conf->entity.pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_UPLINK)
		expected_len += 4;

	memcpy(conf->output, expected, expected_len);
	conf->output_len = expected_len;

	return 0;
}

static struct rte_pdcp_entity*
test_entity_create(const struct pdcp_test_conf *t_conf, int *rc)
{
	struct rte_pdcp_entity *pdcp_entity;
	int ret;

	if (t_conf->entity.pdcp_xfrm.sn_size != RTE_SECURITY_PDCP_SN_SIZE_12 &&
	    t_conf->entity.pdcp_xfrm.sn_size != RTE_SECURITY_PDCP_SN_SIZE_18) {
		*rc = -ENOTSUP;
		return NULL;
	}

	if (t_conf->entity.dev_id == CDEV_INVALID_ID) {
		RTE_LOG(DEBUG, USER1, "Could not find device with required capabilities\n");
		*rc = -ENOTSUP;
		return NULL;
	}

	ret = cryptodev_init(t_conf->entity.dev_id);
	if (ret) {
		*rc = ret;
		RTE_LOG(DEBUG, USER1, "Could not initialize cryptodev\n");
		return NULL;
	}

	rte_errno = 0;

	pdcp_entity = rte_pdcp_entity_establish(&t_conf->entity);
	if (pdcp_entity == NULL) {
		*rc = -rte_errno;
		RTE_LOG(DEBUG, USER1, "Could not establish PDCP entity\n");
		return NULL;
	}

	return pdcp_entity;
}

static uint16_t
test_process_packets(const struct rte_pdcp_entity *pdcp_entity, uint8_t cdev_id,
		     struct rte_mbuf *in_mb[], uint16_t nb_in,
		     struct rte_mbuf *out_mb[], uint16_t *nb_err)
{
	struct rte_crypto_op *cop, *cop_out;
	struct rte_pdcp_group grp[1];
	uint16_t nb_success, nb_grp;
	struct rte_mbuf *mbuf, *mb;

	if (nb_in != 1)
		return -ENOTSUP;

	mbuf = in_mb[0];

	nb_success = rte_pdcp_pkt_pre_process(pdcp_entity, &mbuf, &cop_out, 1, nb_err);
	if (nb_success != 1 || *nb_err != 0) {
		RTE_LOG(ERR, USER1, "Could not pre process PDCP packet\n");
		return TEST_FAILED;
	}

#ifdef VEC_DUMP
	printf("Pre-processed vector:\n");
	rte_pktmbuf_dump(stdout, mbuf, rte_pktmbuf_pkt_len(mbuf));
#endif

	cop = process_crypto_request(cdev_id, cop_out);
	if (cop == NULL) {
		RTE_LOG(ERR, USER1, "Could not process crypto request\n");
		return -EIO;
	}

	grp[0].id.val = 0;

	nb_grp = rte_pdcp_pkt_crypto_group(&cop_out, &mb, grp, 1);
	if (nb_grp != 1 || grp[0].cnt != 1) {
		RTE_LOG(ERR, USER1, "Could not group PDCP crypto results\n");
		return -ENOTRECOVERABLE;
	}

	if ((uintptr_t)pdcp_entity != grp[0].id.val) {
		RTE_LOG(ERR, USER1, "PDCP entity not matching the one from crypto_op\n");
		return -ENOTRECOVERABLE;
	}

#ifdef VEC_DUMP
	printf("Crypto processed vector:\n");
	rte_pktmbuf_dump(stdout, cop->sym->m_dst, rte_pktmbuf_pkt_len(mbuf));
#endif

	return rte_pdcp_pkt_post_process(grp[0].id.ptr, grp[0].m, out_mb, grp[0].cnt, nb_err);
}

static struct rte_mbuf*
mbuf_from_data_create(uint8_t *data, uint16_t data_len)
{
	const struct pdcp_testsuite_params *ts_params = &testsuite_params;
	struct rte_mbuf *mbuf;
	uint8_t *input_text;

	mbuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	if (mbuf == NULL) {
		RTE_LOG(ERR, USER1, "Could not create mbuf\n");
		return NULL;
	}

	memset(rte_pktmbuf_mtod(mbuf, uint8_t *), 0, rte_pktmbuf_tailroom(mbuf));

	input_text = (uint8_t *)rte_pktmbuf_append(mbuf, data_len);
	memcpy(input_text, data, data_len);

	return mbuf;
}

static int
test_attempt_single(struct pdcp_test_conf *t_conf)
{
	struct rte_mbuf *mbuf, **out_mb = NULL;
	struct rte_pdcp_entity *pdcp_entity;
	uint16_t nb_success, nb_err;
	int ret = 0, nb_max_out_mb;

	pdcp_entity = test_entity_create(t_conf, &ret);
	if (pdcp_entity == NULL)
		goto exit;

	/* Allocate buffer for holding mbufs returned */

	/* Max packets that can be cached in entity + burst size */
	nb_max_out_mb = pdcp_entity->max_pkt_cache + 1;
	out_mb = rte_malloc(NULL, nb_max_out_mb * sizeof(uintptr_t), 0);
	if (out_mb == NULL) {
		RTE_LOG(ERR, USER1, "Could not allocate buffer for holding out_mb buffers\n");
		ret = -ENOMEM;
		goto entity_release;
	}

	mbuf = mbuf_from_data_create(t_conf->input, t_conf->input_len);
	if (mbuf == NULL) {
		ret = -ENOMEM;
		goto entity_release;
	}

#ifdef VEC_DUMP
	printf("Adjusted vector:\n");
	rte_pktmbuf_dump(stdout, mbuf, t_conf->input_len);
#endif

	nb_success = test_process_packets(pdcp_entity, t_conf->entity.dev_id, &mbuf, 1, out_mb,
			&nb_err);
	if (nb_success != 1 || nb_err != 0) {
		RTE_LOG(ERR, USER1, "Could not process PDCP packet\n");
		ret = TEST_FAILED;
		goto mbuf_free;
	}

	/* If expected output provided - verify, else - store for future use */
	if (t_conf->output_len) {
		ret = pdcp_known_vec_verify(mbuf, t_conf->output, t_conf->output_len);
		if (ret)
			goto mbuf_free;
	} else {
		ret = pktmbuf_read_into(mbuf, t_conf->output, RTE_PDCP_CTRL_PDU_SIZE_MAX);
		if (ret)
			goto mbuf_free;
		t_conf->output_len = mbuf->pkt_len;
	}

	ret = rte_pdcp_entity_suspend(pdcp_entity, out_mb);
	if (ret) {
		RTE_LOG(DEBUG, USER1, "Could not suspend PDCP entity\n");
		goto mbuf_free;
	}

mbuf_free:
	rte_pktmbuf_free(mbuf);
entity_release:
	rte_pdcp_entity_release(pdcp_entity, out_mb);
	rte_free(out_mb);
exit:
	return ret;
}

static void
uplink_to_downlink_convert(const struct pdcp_test_conf *ul_cfg,
			   struct pdcp_test_conf *dl_cfg)
{
	assert(ul_cfg->entity.pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_UPLINK);

	memcpy(dl_cfg, ul_cfg, sizeof(*dl_cfg));
	dl_cfg->entity.pdcp_xfrm.pkt_dir = RTE_SECURITY_PDCP_DOWNLINK;
	dl_cfg->entity.reverse_iv_direction = false;

	if (dl_cfg->is_integrity_protected) {
		dl_cfg->entity.crypto_xfrm = &dl_cfg->c_xfrm;

		dl_cfg->c_xfrm.cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
		dl_cfg->c_xfrm.next = &dl_cfg->a_xfrm;

		dl_cfg->a_xfrm.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
		dl_cfg->a_xfrm.next = NULL;
	} else {
		dl_cfg->entity.crypto_xfrm = &dl_cfg->c_xfrm;
		dl_cfg->c_xfrm.next = NULL;
		dl_cfg->c_xfrm.cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
	}

	dl_cfg->entity.dev_id = (uint8_t)cryptodev_id_get(dl_cfg->is_integrity_protected,
			&dl_cfg->c_xfrm, &dl_cfg->a_xfrm);

	memcpy(dl_cfg->input, ul_cfg->output, ul_cfg->output_len);
	dl_cfg->input_len = ul_cfg->output_len;

	memcpy(dl_cfg->output, ul_cfg->input, ul_cfg->input_len);
	dl_cfg->output_len = ul_cfg->input_len;
}

/*
 * According to ETSI TS 138 323 V17.1.0, Section 5.2.2.1,
 * SN could be divided into following ranges,
 * relatively to current value of RX_DELIV state:
 * +-------------+-------------+-------------+-------------+
 * |  -Outside   |  -Window    |   +Window   |  +Outside   |
 * |   (valid)   |  (Invalid)  |   (Valid)   |  (Invalid)  |
 * +-------------+-------------^-------------+-------------+
 *                             |
 *                             v
 *                        SN(RX_DELIV)
 */
enum sn_range_type {
	SN_RANGE_MINUS_OUTSIDE,
	SN_RANGE_MINUS_WINDOW,
	SN_RANGE_PLUS_WINDOW,
	SN_RANGE_PLUS_OUTSIDE,
};

#define PDCP_SET_COUNT(hfn, sn, size) ((hfn << size) | (sn & ((1 << size) - 1)))

/*
 * Take uplink test case as base, modify RX_DELIV in state and SN in input
 */
static int
test_sn_range_type(enum sn_range_type type, struct pdcp_test_conf *conf)
{
	uint32_t rx_deliv_hfn, rx_deliv_sn, new_hfn, new_sn;
	const int domain = conf->entity.pdcp_xfrm.domain;
	struct pdcp_test_conf dl_conf;
	int ret, expected_ret;

	if (conf->entity.pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_DOWNLINK)
		return TEST_SKIPPED;

	if (domain != RTE_SECURITY_PDCP_MODE_CONTROL && domain != RTE_SECURITY_PDCP_MODE_DATA)
		return TEST_SKIPPED;

	const uint32_t sn_size = conf->entity.pdcp_xfrm.sn_size;
	const uint32_t window_size = PDCP_WINDOW_SIZE(sn_size);
	/* Max value of SN that could fit in `sn_size` bits */
	const uint32_t max_sn = (1 << sn_size) - 1;
	const uint32_t shift = (max_sn - window_size) / 2;
	/* Could be any number up to `shift` value */
	const uint32_t default_sn = RTE_MIN(2u, shift);

	/* Initialize HFN as non zero value, to be able check values before */
	rx_deliv_hfn = 0xa;

	switch (type) {
	case SN_RANGE_PLUS_WINDOW:
		/* Within window size, HFN stay same */
		new_hfn = rx_deliv_hfn;
		rx_deliv_sn = default_sn;
		new_sn = rx_deliv_sn + 1;
		expected_ret = TEST_SUCCESS;
		break;
	case SN_RANGE_MINUS_WINDOW:
		/* Within window size, HFN stay same */
		new_hfn = rx_deliv_hfn;
		rx_deliv_sn = default_sn;
		new_sn = rx_deliv_sn - 1;
		expected_ret = TEST_FAILED;
		break;
	case SN_RANGE_PLUS_OUTSIDE:
		/* RCVD_SN >= SN(RX_DELIV) + Window_Size */
		new_hfn = rx_deliv_hfn - 1;
		rx_deliv_sn = default_sn;
		new_sn = rx_deliv_sn + window_size;
		expected_ret = TEST_FAILED;
		break;
	case SN_RANGE_MINUS_OUTSIDE:
		/* RCVD_SN < SN(RX_DELIV) - Window_Size */
		new_hfn = rx_deliv_hfn + 1;
		rx_deliv_sn = window_size + default_sn;
		new_sn = rx_deliv_sn - window_size - 1;
		expected_ret = TEST_SUCCESS;
		break;
	default:
		return TEST_FAILED;
	}

	/* Configure Uplink to generate expected, encrypted packet */
	pdcp_sn_to_raw_set(conf->input, new_sn, conf->entity.pdcp_xfrm.sn_size);
	conf->entity.out_of_order_delivery = true;
	conf->entity.reverse_iv_direction = true;
	conf->entity.pdcp_xfrm.hfn = new_hfn;
	conf->entity.sn = new_sn;
	conf->output_len = 0;
	ret = test_attempt_single(conf);
	if (ret != TEST_SUCCESS)
		return ret;

	/* Flip configuration to downlink */
	uplink_to_downlink_convert(conf, &dl_conf);

	/* Modify the rx_deliv to verify the expected behaviour */
	dl_conf.entity.pdcp_xfrm.hfn = rx_deliv_hfn;
	dl_conf.entity.sn = rx_deliv_sn;
	ret = test_attempt_single(&dl_conf);
	if ((ret == TEST_SKIPPED) || (ret == -ENOTSUP))
		return ret;

	TEST_ASSERT_EQUAL(ret, expected_ret, "Unexpected result");

	return TEST_SUCCESS;
}

static int
test_sn_plus_window(struct pdcp_test_conf *t_conf)
{
	return test_sn_range_type(SN_RANGE_PLUS_WINDOW, t_conf);
}

static int
test_sn_minus_window(struct pdcp_test_conf *t_conf)
{
	return test_sn_range_type(SN_RANGE_MINUS_WINDOW, t_conf);
}

static int
test_sn_plus_outside(struct pdcp_test_conf *t_conf)
{
	return test_sn_range_type(SN_RANGE_PLUS_OUTSIDE, t_conf);
}

static int
test_sn_minus_outside(struct pdcp_test_conf *t_conf)
{
	return test_sn_range_type(SN_RANGE_MINUS_OUTSIDE, t_conf);
}

static struct rte_mbuf *
generate_packet_for_dl_with_sn(struct pdcp_test_conf ul_conf, uint32_t count)
{
	enum rte_security_pdcp_sn_size sn_size = ul_conf.entity.pdcp_xfrm.sn_size;
	int ret;

	ul_conf.entity.pdcp_xfrm.hfn = pdcp_hfn_from_count_get(count, sn_size);
	ul_conf.entity.sn = pdcp_sn_from_count_get(count, sn_size);
	ul_conf.entity.out_of_order_delivery = true;
	ul_conf.entity.reverse_iv_direction = true;
	ul_conf.output_len = 0;

	ret = test_attempt_single(&ul_conf);
	if (ret != TEST_SUCCESS)
		return NULL;

	return mbuf_from_data_create(ul_conf.output, ul_conf.output_len);
}

static bool
array_asc_sorted_check(struct rte_mbuf *m[], uint32_t len, enum rte_security_pdcp_sn_size sn_size)
{
	uint32_t i;

	if (len < 2)
		return true;

	for (i = 0; i < (len - 1); i++) {
		if (pdcp_sn_from_raw_get(rte_pktmbuf_mtod(m[i], void *), sn_size) >
		    pdcp_sn_from_raw_get(rte_pktmbuf_mtod(m[i + 1], void *), sn_size))
			return false;
	}

	return true;
}

static int
test_reorder_gap_fill(struct pdcp_test_conf *ul_conf)
{
	const enum rte_security_pdcp_sn_size sn_size = ul_conf->entity.pdcp_xfrm.sn_size;
	struct rte_mbuf *m0 = NULL, *m1 = NULL, *out_mb[2] = {0};
	uint16_t nb_success = 0, nb_err = 0;
	struct rte_pdcp_entity *pdcp_entity;
	struct pdcp_test_conf dl_conf;
	int ret = TEST_FAILED, nb_out;
	uint8_t cdev_id;

	const int start_count = 0;

	if (ul_conf->entity.pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_DOWNLINK)
		return TEST_SKIPPED;

	/* Create configuration for actual testing */
	uplink_to_downlink_convert(ul_conf, &dl_conf);
	dl_conf.entity.pdcp_xfrm.hfn = pdcp_hfn_from_count_get(start_count, sn_size);
	dl_conf.entity.sn = pdcp_sn_from_count_get(start_count, sn_size);

	pdcp_entity = test_entity_create(&dl_conf, &ret);
	if (pdcp_entity == NULL)
		return ret;

	cdev_id = dl_conf.entity.dev_id;

	/* Send packet with SN > RX_DELIV to create a gap */
	m1 = generate_packet_for_dl_with_sn(*ul_conf, start_count + 1);
	ASSERT_TRUE_OR_GOTO(m1 != NULL, exit, "Could not allocate buffer for packet\n");

	/* Buffered packets after insert [NULL, m1] */
	nb_success = test_process_packets(pdcp_entity, cdev_id, &m1, 1, out_mb, &nb_err);
	ASSERT_TRUE_OR_GOTO(nb_err == 0, exit, "Error occurred during packet process\n");
	ASSERT_TRUE_OR_GOTO(nb_success == 0, exit, "Packet was not buffered as expected\n");
	m1 = NULL; /* Packet was moved to PDCP lib */

	/* Generate packet to fill the existing gap */
	m0 = generate_packet_for_dl_with_sn(*ul_conf, start_count);
	ASSERT_TRUE_OR_GOTO(m0 != NULL, exit, "Could not allocate buffer for packet\n");

	/*
	 * Buffered packets after insert [m0, m1]
	 * Gap filled, all packets should be returned
	 */
	nb_success = test_process_packets(pdcp_entity, cdev_id, &m0, 1, out_mb, &nb_err);
	ASSERT_TRUE_OR_GOTO(nb_err == 0, exit, "Error occurred during packet process\n");
	ASSERT_TRUE_OR_GOTO(nb_success == 2, exit,
			"Packet count mismatch (received: %i, expected: 2)\n", nb_success);
	m0 = NULL; /* Packet was moved to out_mb */

	/* Check that packets in correct order */
	ASSERT_TRUE_OR_GOTO(array_asc_sorted_check(out_mb, nb_success, sn_size), exit,
			"Error occurred during packet drain\n");
	ASSERT_TRUE_OR_GOTO(testsuite_params.timer_is_running == false, exit,
			"Timer should be stopped after full drain\n");

	ret = TEST_SUCCESS;
exit:
	rte_pktmbuf_free(m0);
	rte_pktmbuf_free(m1);
	rte_pktmbuf_free_bulk(out_mb, nb_success);
	nb_out = rte_pdcp_entity_release(pdcp_entity, out_mb);
	rte_pktmbuf_free_bulk(out_mb, nb_out);
	return ret;
}

static int
test_reorder_gap_in_reorder_buffer(const struct pdcp_test_conf *ul_conf)
{
	const enum rte_security_pdcp_sn_size sn_size = ul_conf->entity.pdcp_xfrm.sn_size;
	struct rte_mbuf *m = NULL, *out_mb[2] = {0};
	uint16_t nb_success = 0, nb_err = 0;
	struct rte_pdcp_entity *pdcp_entity;
	int ret = TEST_FAILED, nb_out, i;
	struct pdcp_test_conf dl_conf;
	uint8_t cdev_id;

	const int start_count = 0;

	if (ul_conf->entity.pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_DOWNLINK)
		return TEST_SKIPPED;

	/* Create configuration for actual testing */
	uplink_to_downlink_convert(ul_conf, &dl_conf);
	dl_conf.entity.pdcp_xfrm.hfn = pdcp_hfn_from_count_get(start_count, sn_size);
	dl_conf.entity.sn = pdcp_sn_from_count_get(start_count, sn_size);
	pdcp_entity = test_entity_create(&dl_conf, &ret);
	if (pdcp_entity == NULL)
		return ret;

	cdev_id = dl_conf.entity.dev_id;

	/* Create two gaps [NULL, m1, NULL, m3]*/
	for (i = 0; i < 2; i++) {
		m = generate_packet_for_dl_with_sn(*ul_conf, start_count + 2 * i + 1);
		ASSERT_TRUE_OR_GOTO(m != NULL, exit, "Could not allocate buffer for packet\n");
		nb_success = test_process_packets(pdcp_entity, cdev_id, &m, 1, out_mb, &nb_err);
		ASSERT_TRUE_OR_GOTO(nb_err == 0, exit, "Error occurred during packet process\n");
		ASSERT_TRUE_OR_GOTO(nb_success == 0, exit, "Packet was not buffered as expected\n");
		m = NULL; /* Packet was moved to PDCP lib */
	}

	/* Generate packet to fill the first gap */
	m = generate_packet_for_dl_with_sn(*ul_conf, start_count);
	ASSERT_TRUE_OR_GOTO(m != NULL, exit, "Could not allocate buffer for packet\n");

	/*
	 * Buffered packets after insert [m0, m1, NULL, m3]
	 * Only first gap should be filled, timer should be restarted for second gap
	 */
	nb_success = test_process_packets(pdcp_entity, cdev_id, &m, 1, out_mb, &nb_err);
	ASSERT_TRUE_OR_GOTO(nb_err == 0, exit, "Error occurred during packet process\n");
	ASSERT_TRUE_OR_GOTO(nb_success == 2, exit,
			"Packet count mismatch (received: %i, expected: 2)\n", nb_success);
	m = NULL;
	/* Check that packets in correct order */
	ASSERT_TRUE_OR_GOTO(array_asc_sorted_check(out_mb, nb_success, sn_size),
			exit, "Error occurred during packet drain\n");
	ASSERT_TRUE_OR_GOTO(testsuite_params.timer_is_running == true, exit,
			"Timer should be restarted after partial drain");


	ret = TEST_SUCCESS;
exit:
	rte_pktmbuf_free(m);
	rte_pktmbuf_free_bulk(out_mb, nb_success);
	nb_out = rte_pdcp_entity_release(pdcp_entity, out_mb);
	rte_pktmbuf_free_bulk(out_mb, nb_out);
	return ret;
}

static int
test_reorder_buffer_full_window_size_sn_12(const struct pdcp_test_conf *ul_conf)
{
	const enum rte_security_pdcp_sn_size sn_size = ul_conf->entity.pdcp_xfrm.sn_size;
	const uint32_t window_size = PDCP_WINDOW_SIZE(sn_size);
	struct rte_mbuf *m1 = NULL, **out_mb = NULL;
	uint16_t nb_success = 0, nb_err = 0;
	struct rte_pdcp_entity *pdcp_entity;
	struct pdcp_test_conf dl_conf;
	const int rx_deliv = 0;
	int ret = TEST_FAILED;
	size_t i, nb_out;
	uint8_t cdev_id;

	if (ul_conf->entity.pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_DOWNLINK ||
		sn_size != RTE_SECURITY_PDCP_SN_SIZE_12)
		return TEST_SKIPPED;

	/* Create configuration for actual testing */
	uplink_to_downlink_convert(ul_conf, &dl_conf);
	dl_conf.entity.pdcp_xfrm.hfn = pdcp_hfn_from_count_get(rx_deliv, sn_size);
	dl_conf.entity.sn = pdcp_sn_from_count_get(rx_deliv, sn_size);

	pdcp_entity = test_entity_create(&dl_conf, &ret);
	if (pdcp_entity == NULL)
		return ret;

	ASSERT_TRUE_OR_GOTO(pdcp_entity->max_pkt_cache >= window_size, exit,
			"PDCP max packet cache is too small");
	cdev_id = dl_conf.entity.dev_id;
	out_mb = rte_zmalloc(NULL, pdcp_entity->max_pkt_cache * sizeof(uintptr_t), 0);
	ASSERT_TRUE_OR_GOTO(out_mb != NULL, exit,
			"Could not allocate buffer for holding out_mb buffers\n");

	/* Send packets with SN > RX_DELIV to create a gap */
	for (i = rx_deliv + 1; i < window_size; i++) {
		m1 = generate_packet_for_dl_with_sn(*ul_conf, i);
		ASSERT_TRUE_OR_GOTO(m1 != NULL, exit, "Could not allocate buffer for packet\n");
		/* Buffered packets after insert [NULL, m1] */
		nb_success = test_process_packets(pdcp_entity, cdev_id, &m1, 1, out_mb, &nb_err);
		ASSERT_TRUE_OR_GOTO(nb_err == 0, exit, "Error occurred during packet buffering\n");
		ASSERT_TRUE_OR_GOTO(nb_success == 0, exit, "Packet was not buffered as expected\n");
	}

	m1 = generate_packet_for_dl_with_sn(*ul_conf, rx_deliv);
	ASSERT_TRUE_OR_GOTO(m1 != NULL, exit, "Could not allocate buffer for packet\n");
	/* Insert missing packet */
	nb_success = test_process_packets(pdcp_entity, cdev_id, &m1, 1, out_mb, &nb_err);
	ASSERT_TRUE_OR_GOTO(nb_err == 0, exit, "Error occurred during packet buffering\n");
	ASSERT_TRUE_OR_GOTO(nb_success == window_size, exit,
			"Packet count mismatch (received: %i, expected: %i)\n",
			nb_success, window_size);
	m1 = NULL;

	ret = TEST_SUCCESS;
exit:
	rte_pktmbuf_free(m1);
	rte_pktmbuf_free_bulk(out_mb, nb_success);
	nb_out = rte_pdcp_entity_release(pdcp_entity, out_mb);
	rte_pktmbuf_free_bulk(out_mb, nb_out);
	rte_free(out_mb);
	return ret;
}

#ifdef RTE_LIB_EVENTDEV
static void
event_timer_start_cb(void *timer, void *args)
{
	struct rte_event_timer *evtims = args;
	int ret = 0;

	ret = rte_event_timer_arm_burst(timer, &evtims, 1);
	assert(ret == 1);
}
#endif /* RTE_LIB_EVENTDEV */

static int
test_expiry_with_event_timer(const struct pdcp_test_conf *ul_conf)
{
#ifdef RTE_LIB_EVENTDEV
	const enum rte_security_pdcp_sn_size sn_size = ul_conf->entity.pdcp_xfrm.sn_size;
	struct rte_mbuf *m1 = NULL, *out_mb[1] = {0};
	uint16_t n = 0, nb_err = 0, nb_try = 5;
	struct rte_pdcp_entity *pdcp_entity;
	struct pdcp_test_conf dl_conf;
	int ret = TEST_FAILED, nb_out;
	struct rte_event event;

	const int start_count = 0;
	struct rte_event_timer evtim = {
		.ev.op = RTE_EVENT_OP_NEW,
		.ev.queue_id = TEST_EV_QUEUE_ID,
		.ev.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.ev.event_type =  RTE_EVENT_TYPE_TIMER,
		.state = RTE_EVENT_TIMER_NOT_ARMED,
		.timeout_ticks = 1,
	};

	if (ul_conf->entity.pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_DOWNLINK)
		return TEST_SKIPPED;

	/* Create configuration for actual testing */
	uplink_to_downlink_convert(ul_conf, &dl_conf);
	dl_conf.entity.pdcp_xfrm.hfn = pdcp_hfn_from_count_get(start_count, sn_size);
	dl_conf.entity.sn = pdcp_sn_from_count_get(start_count, sn_size);
	dl_conf.entity.t_reordering.args = &evtim;
	dl_conf.entity.t_reordering.timer = testsuite_params.timdev;
	dl_conf.entity.t_reordering.start = event_timer_start_cb;

	pdcp_entity = test_entity_create(&dl_conf, &ret);
	if (pdcp_entity == NULL)
		return ret;

	evtim.ev.event_ptr = pdcp_entity;

	/* Send packet with SN > RX_DELIV to create a gap */
	m1 = generate_packet_for_dl_with_sn(*ul_conf, start_count + 1);
	ASSERT_TRUE_OR_GOTO(m1 != NULL, exit, "Could not allocate buffer for packet\n");

	/* Buffered packets after insert [NULL, m1] */
	n = test_process_packets(pdcp_entity, dl_conf.entity.dev_id, &m1, 1, out_mb, &nb_err);
	ASSERT_TRUE_OR_GOTO(nb_err == 0, exit, "Error occurred during packet buffering\n");
	ASSERT_TRUE_OR_GOTO(n == 0, exit, "Packet was not buffered as expected\n");

	m1 = NULL; /* Packet was moved to PDCP lib */

	n = rte_event_dequeue_burst(testsuite_params.evdev, TEST_EV_PORT_ID, &event, 1, 0);
	while (n != 1) {
		rte_delay_us(testsuite_params.min_resolution_ns / 1000);
		n = rte_event_dequeue_burst(testsuite_params.evdev, TEST_EV_PORT_ID, &event, 1, 0);
		ASSERT_TRUE_OR_GOTO(nb_try > 0, exit,
				"Dequeued unexpected timer expiry event: %i\n", n);
		nb_try--;
	}

	ASSERT_TRUE_OR_GOTO(event.event_type == RTE_EVENT_TYPE_TIMER, exit, "Unexpected event type\n");

	/* Handle expiry event */
	n = rte_pdcp_t_reordering_expiry_handle(event.event_ptr, out_mb);
	ASSERT_TRUE_OR_GOTO(n == 1, exit, "Unexpected number of expired packets :%i\n", n);

	ret = TEST_SUCCESS;
exit:
	rte_pktmbuf_free(m1);
	rte_pktmbuf_free_bulk(out_mb, n);
	nb_out = rte_pdcp_entity_release(pdcp_entity, out_mb);
	rte_pktmbuf_free_bulk(out_mb, nb_out);
	return ret;
#else
	RTE_SET_USED(ul_conf);
	return TEST_SKIPPED;
#endif /* RTE_LIB_EVENTDEV */
}

static void
test_rte_timer_expiry_handle(struct rte_timer *timer_handle, void *arg)
{
	struct test_rte_timer_args *timer_data = arg;
	struct rte_mbuf *out_mb[1] = {0};
	uint16_t n;

	RTE_SET_USED(timer_handle);

	n = rte_pdcp_t_reordering_expiry_handle(timer_data->pdcp_entity, out_mb);
	rte_pktmbuf_free_bulk(out_mb, n);

	timer_data->status =  n == 1 ? n : -1;
}

static void
test_rte_timer_start_cb(void *timer, void *args)
{
	rte_timer_reset_sync(timer, 1, SINGLE, rte_lcore_id(), test_rte_timer_expiry_handle, args);
}

static int
test_expiry_with_rte_timer(const struct pdcp_test_conf *ul_conf)
{
	const enum rte_security_pdcp_sn_size sn_size = ul_conf->entity.pdcp_xfrm.sn_size;
	struct rte_mbuf *m1 = NULL, *out_mb[1] = {0};
	uint16_t n = 0, nb_err = 0, nb_try = 5;
	struct test_rte_timer_args timer_args;
	struct rte_pdcp_entity *pdcp_entity;
	struct pdcp_test_conf dl_conf;
	int ret = TEST_FAILED, nb_out;
	struct rte_timer timer = {0};

	const int start_count = 0;

	if (ul_conf->entity.pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_DOWNLINK)
		return TEST_SKIPPED;

	/* Set up a timer */
	rte_timer_init(&timer);

	/* Create configuration for actual testing */
	uplink_to_downlink_convert(ul_conf, &dl_conf);
	dl_conf.entity.pdcp_xfrm.hfn = pdcp_hfn_from_count_get(start_count, sn_size);
	dl_conf.entity.sn = pdcp_sn_from_count_get(start_count, sn_size);
	dl_conf.entity.t_reordering.args = &timer_args;
	dl_conf.entity.t_reordering.timer = &timer;
	dl_conf.entity.t_reordering.start = test_rte_timer_start_cb;

	pdcp_entity = test_entity_create(&dl_conf, &ret);
	if (pdcp_entity == NULL)
		return ret;

	timer_args.status = 0;
	timer_args.pdcp_entity = pdcp_entity;

	/* Send packet with SN > RX_DELIV to create a gap */
	m1 = generate_packet_for_dl_with_sn(*ul_conf, start_count + 1);
	ASSERT_TRUE_OR_GOTO(m1 != NULL, exit, "Could not allocate buffer for packet\n");

	/* Buffered packets after insert [NULL, m1] */
	n = test_process_packets(pdcp_entity, dl_conf.entity.dev_id, &m1, 1, out_mb, &nb_err);
	ASSERT_TRUE_OR_GOTO(nb_err == 0, exit, "Error occurred during packet buffering\n");
	ASSERT_TRUE_OR_GOTO(n == 0, exit, "Packet was not buffered as expected\n");

	m1 = NULL; /* Packet was moved to PDCP lib */

	/* Verify that expire was handled correctly */
	rte_timer_manage();
	while (timer_args.status != 1) {
		rte_delay_us(1);
		rte_timer_manage();
		ASSERT_TRUE_OR_GOTO(nb_try > 0, exit, "Bad expire handle status %i\n",
			timer_args.status);
		nb_try--;
	}

	ret = TEST_SUCCESS;
exit:
	rte_pktmbuf_free(m1);
	rte_pktmbuf_free_bulk(out_mb, n);
	nb_out = rte_pdcp_entity_release(pdcp_entity, out_mb);
	rte_pktmbuf_free_bulk(out_mb, nb_out);
	return ret;
}

static struct rte_pdcp_up_ctrl_pdu_hdr *
pdcp_status_report_init(uint32_t fmc)
{
	struct rte_pdcp_up_ctrl_pdu_hdr *hdr = testsuite_params.status_report;

	hdr->d_c = RTE_PDCP_PDU_TYPE_CTRL;
	hdr->pdu_type = RTE_PDCP_CTRL_PDU_TYPE_STATUS_REPORT;
	hdr->fmc = rte_cpu_to_be_32(fmc);
	hdr->r = 0;
	memset(hdr->bitmap, 0, testsuite_params.status_report_bitmask_capacity);

	return hdr;
}

static uint32_t
pdcp_status_report_len(void)
{
	struct rte_pdcp_up_ctrl_pdu_hdr *hdr = testsuite_params.status_report;
	uint32_t i;

	for (i = testsuite_params.status_report_bitmask_capacity; i != 0; i--) {
		if (hdr->bitmap[i - 1])
			return i;
	}

	return 0;
}

static int
pdcp_status_report_verify(struct rte_mbuf *status_report,
			 const struct rte_pdcp_up_ctrl_pdu_hdr *expected_hdr, uint32_t expected_len)
{
	uint32_t received_len = rte_pktmbuf_pkt_len(status_report);
	uint8_t *received_buf = testsuite_params.ctrl_pdu_buf;
	int ret;

	ret = pktmbuf_read_into(status_report, received_buf, RTE_PDCP_CTRL_PDU_SIZE_MAX);
	TEST_ASSERT_SUCCESS(ret, "Failed to copy status report pkt into continuous buffer");

	debug_hexdump(stdout, "Received:", received_buf, received_len);
	debug_hexdump(stdout, "Expected:", expected_hdr, expected_len);

	TEST_ASSERT_EQUAL(expected_len, received_len,
			  "Mismatch in packet lengths [expected: %d, received: %d]",
			  expected_len, received_len);

	TEST_ASSERT_BUFFERS_ARE_EQUAL(received_buf, expected_hdr, expected_len,
				     "Generated packet not as expected");

	return 0;
}

static int
test_status_report_gen(const struct pdcp_test_conf *ul_conf,
		       const struct rte_pdcp_up_ctrl_pdu_hdr *hdr,
		       uint32_t bitmap_len)
{
	const enum rte_security_pdcp_sn_size sn_size = ul_conf->entity.pdcp_xfrm.sn_size;
	struct rte_mbuf *status_report = NULL, **out_mb, *m;
	uint16_t nb_success = 0, nb_err = 0;
	struct rte_pdcp_entity *pdcp_entity;
	struct pdcp_test_conf dl_conf;
	int ret = TEST_FAILED, nb_out;
	uint32_t nb_pkts = 0, i;
	uint8_t cdev_id;

	const uint32_t start_count = rte_be_to_cpu_32(hdr->fmc);

	if (ul_conf->entity.pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_DOWNLINK)
		return TEST_SKIPPED;

	/* Create configuration for actual testing */
	uplink_to_downlink_convert(ul_conf, &dl_conf);
	dl_conf.entity.pdcp_xfrm.hfn = pdcp_hfn_from_count_get(start_count, sn_size);
	dl_conf.entity.sn = pdcp_sn_from_count_get(start_count, sn_size);
	dl_conf.entity.status_report_required = true;

	pdcp_entity = test_entity_create(&dl_conf, &ret);
	if (pdcp_entity == NULL)
		return ret;

	cdev_id = dl_conf.entity.dev_id;
	out_mb = calloc(pdcp_entity->max_pkt_cache, sizeof(uintptr_t));

	for (i = 0; i < bitmap_len * 8; i++) {
		if (!bitmask_is_bit_set(hdr->bitmap, i))
			continue;

		m = generate_packet_for_dl_with_sn(*ul_conf, start_count + i + 1);
		ASSERT_TRUE_OR_GOTO(m != NULL, exit, "Could not allocate buffer for packet\n");

		nb_success = test_process_packets(pdcp_entity, cdev_id, &m, 1, out_mb, &nb_err);
		ASSERT_TRUE_OR_GOTO(nb_err == 0, exit, "Error occurred during packet buffering\n");
		ASSERT_TRUE_OR_GOTO(nb_success == 0, exit, "Packet was not buffered as expected\n");

	}

	m = NULL;

	/* Check status report */
	status_report = rte_pdcp_control_pdu_create(pdcp_entity,
			RTE_PDCP_CTRL_PDU_TYPE_STATUS_REPORT);
	ASSERT_TRUE_OR_GOTO(status_report != NULL, exit, "Could not generate status report\n");

	const uint32_t expected_len = sizeof(struct rte_pdcp_up_ctrl_pdu_hdr) + bitmap_len;

	ASSERT_TRUE_OR_GOTO(pdcp_status_report_verify(status_report, hdr, expected_len) == 0, exit,
			   "Report verification failure\n");

	ret = TEST_SUCCESS;
exit:
	rte_free(m);
	rte_pktmbuf_free(status_report);
	rte_pktmbuf_free_bulk(out_mb, nb_pkts);
	nb_out = rte_pdcp_entity_release(pdcp_entity, out_mb);
	rte_pktmbuf_free_bulk(out_mb, nb_out);
	free(out_mb);
	return ret;
}

static void
ctrl_pdu_hdr_packet_set(struct rte_pdcp_up_ctrl_pdu_hdr *hdr, uint32_t pkt_count)
{
	bitmask_set_bit(hdr->bitmap, pkt_count - rte_be_to_cpu_32(hdr->fmc) - 1);
}

static int
test_status_report_fmc_only(const struct pdcp_test_conf *ul_conf)
{
	struct rte_pdcp_up_ctrl_pdu_hdr *hdr = pdcp_status_report_init(42);

	return test_status_report_gen(ul_conf, hdr, pdcp_status_report_len());
}

static int
test_status_report_one_pkt_first_slab(const struct pdcp_test_conf *ul_conf)
{
	struct rte_pdcp_up_ctrl_pdu_hdr *hdr = pdcp_status_report_init(0);

	ctrl_pdu_hdr_packet_set(hdr, RTE_BITMAP_SLAB_BIT_SIZE / 2 + 1);

	return test_status_report_gen(ul_conf, hdr, pdcp_status_report_len());
}

static int
test_status_report_one_pkt_second_slab(const struct pdcp_test_conf *ul_conf)
{
	struct rte_pdcp_up_ctrl_pdu_hdr *hdr = pdcp_status_report_init(1);

	ctrl_pdu_hdr_packet_set(hdr, RTE_BITMAP_SLAB_BIT_SIZE + 1);

	return test_status_report_gen(ul_conf, hdr, pdcp_status_report_len());
}

static int
test_status_report_full_slab(const struct pdcp_test_conf *ul_conf)
{
	struct rte_pdcp_up_ctrl_pdu_hdr *hdr = pdcp_status_report_init(1);
	const uint32_t start_offset = RTE_BITMAP_SLAB_BIT_SIZE + 1;
	int i;

	for (i = 0; i < RTE_BITMAP_SLAB_BIT_SIZE; i++)
		ctrl_pdu_hdr_packet_set(hdr, start_offset + i);

	return test_status_report_gen(ul_conf, hdr, pdcp_status_report_len());
}

static int
test_status_report_two_sequential_slabs(const struct pdcp_test_conf *ul_conf)
{
	struct rte_pdcp_up_ctrl_pdu_hdr *hdr = pdcp_status_report_init(0);
	const uint32_t start_offset = RTE_BITMAP_SLAB_BIT_SIZE / 2 + 1;

	ctrl_pdu_hdr_packet_set(hdr, start_offset);
	ctrl_pdu_hdr_packet_set(hdr, start_offset + RTE_BITMAP_SLAB_BIT_SIZE);

	return test_status_report_gen(ul_conf, hdr, pdcp_status_report_len());
}

static int
test_status_report_two_non_sequential_slabs(const struct pdcp_test_conf *ul_conf)
{
	struct rte_pdcp_up_ctrl_pdu_hdr *hdr = pdcp_status_report_init(0);
	const uint32_t start_offset = RTE_BITMAP_SLAB_BIT_SIZE / 2 + 1;

	ctrl_pdu_hdr_packet_set(hdr, start_offset);
	ctrl_pdu_hdr_packet_set(hdr, start_offset + RTE_BITMAP_SLAB_BIT_SIZE);
	ctrl_pdu_hdr_packet_set(hdr, 3 * RTE_BITMAP_SLAB_BIT_SIZE);

	return test_status_report_gen(ul_conf, hdr, pdcp_status_report_len());
}

static int
test_status_report_max_length_sn_12(const struct pdcp_test_conf *ul_conf)
{
	struct rte_pdcp_up_ctrl_pdu_hdr *hdr;
	const uint32_t fmc = 0;
	uint32_t i;

	if (ul_conf->entity.pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_DOWNLINK ||
		ul_conf->entity.pdcp_xfrm.sn_size != RTE_SECURITY_PDCP_SN_SIZE_12)
		return TEST_SKIPPED;

	hdr = pdcp_status_report_init(fmc);

	const uint32_t max_count = RTE_MIN((RTE_PDCP_CTRL_PDU_SIZE_MAX - sizeof(hdr)) * 8,
			(uint32_t)PDCP_WINDOW_SIZE(RTE_SECURITY_PDCP_SN_SIZE_12));

	i = fmc + 2; /* set first count to have a gap, to enable packet buffering */

	for (; i < max_count; i++)
		ctrl_pdu_hdr_packet_set(hdr, i);

	return test_status_report_gen(ul_conf, hdr, pdcp_status_report_len());
}

static int
test_status_report_overlap_different_slabs(const struct pdcp_test_conf *ul_conf)
{
	struct rte_pdcp_up_ctrl_pdu_hdr *hdr = pdcp_status_report_init(63);
	const uint32_t sn_size = 12;

	ctrl_pdu_hdr_packet_set(hdr, 64 + 1);
	ctrl_pdu_hdr_packet_set(hdr, PDCP_WINDOW_SIZE(sn_size) + 1);

	return test_status_report_gen(ul_conf, hdr, pdcp_status_report_len());
}

static int
test_status_report_overlap_same_slab(const struct pdcp_test_conf *ul_conf)
{
	struct rte_pdcp_up_ctrl_pdu_hdr *hdr = pdcp_status_report_init(2);
	const uint32_t sn_size = 12;

	ctrl_pdu_hdr_packet_set(hdr, 4);
	ctrl_pdu_hdr_packet_set(hdr, PDCP_WINDOW_SIZE(sn_size) + 1);

	return test_status_report_gen(ul_conf, hdr, pdcp_status_report_len());
}

static int
test_combined(struct pdcp_test_conf *ul_conf)
{
	struct pdcp_test_conf dl_conf;
	int ret;

	if (ul_conf->entity.pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_DOWNLINK)
		return TEST_SKIPPED;

	ul_conf->entity.reverse_iv_direction = true;
	ul_conf->output_len = 0;

	ret = test_attempt_single(ul_conf);
	if (ret != TEST_SUCCESS)
		return ret;

	uplink_to_downlink_convert(ul_conf, &dl_conf);
	ret = test_attempt_single(&dl_conf);

	return ret;
}

#ifdef RTE_LIB_EVENTDEV
static inline void
eventdev_conf_default_set(struct rte_event_dev_config *dev_conf, struct rte_event_dev_info *info)
{
	memset(dev_conf, 0, sizeof(struct rte_event_dev_config));
	dev_conf->dequeue_timeout_ns = info->min_dequeue_timeout_ns;
	dev_conf->nb_event_ports = 1;
	dev_conf->nb_event_queues = 1;
	dev_conf->nb_event_queue_flows = info->max_event_queue_flows;
	dev_conf->nb_event_port_dequeue_depth = info->max_event_port_dequeue_depth;
	dev_conf->nb_event_port_enqueue_depth = info->max_event_port_enqueue_depth;
	dev_conf->nb_event_port_enqueue_depth = info->max_event_port_enqueue_depth;
	dev_conf->nb_events_limit = info->max_num_events;
}

static inline int
eventdev_setup(void)
{
	struct rte_event_dev_config dev_conf;
	struct rte_event_dev_info info;
	int ret, evdev = 0;

	if (!rte_event_dev_count())
		return TEST_SKIPPED;

	ret = rte_event_dev_info_get(evdev, &info);
	TEST_ASSERT_SUCCESS(ret, "Failed to get event dev info");
	TEST_ASSERT(info.max_num_events < 0 || info.max_num_events >= 1,
			"ERROR max_num_events=%d < max_events=%d", info.max_num_events, 1);

	eventdev_conf_default_set(&dev_conf, &info);
	ret = rte_event_dev_configure(evdev, &dev_conf);
	TEST_ASSERT_SUCCESS(ret, "Failed to configure eventdev");

	ret = rte_event_queue_setup(evdev, TEST_EV_QUEUE_ID, NULL);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup queue=%d", TEST_EV_QUEUE_ID);

	/* Configure event port */
	ret = rte_event_port_setup(evdev, TEST_EV_PORT_ID, NULL);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup port=%d", TEST_EV_PORT_ID);
	ret = rte_event_port_link(evdev, TEST_EV_PORT_ID, NULL, NULL, 0);
	TEST_ASSERT(ret >= 0, "Failed to link all queues port=%d", TEST_EV_PORT_ID);

	ret = rte_event_dev_start(evdev);
	TEST_ASSERT_SUCCESS(ret, "Failed to start device");

	testsuite_params.evdev = evdev;

	return TEST_SUCCESS;
}

static int
event_timer_setup(void)
{
	struct rte_event_timer_adapter_info info;
	struct rte_event_timer_adapter *timdev;
	uint32_t caps = 0;

	struct rte_event_timer_adapter_conf config = {
		.event_dev_id = testsuite_params.evdev,
		.timer_adapter_id = TIMER_ADAPTER_ID,
		.timer_tick_ns = NSECPERSEC,
		.max_tmo_ns = 10 * NSECPERSEC,
		.nb_timers = 10,
		.flags = 0,
	};

	TEST_ASSERT_SUCCESS(rte_event_timer_adapter_caps_get(testsuite_params.evdev, &caps),
				"Failed to get adapter capabilities");

	if (!(caps & RTE_EVENT_TIMER_ADAPTER_CAP_INTERNAL_PORT))
		return TEST_SKIPPED;

	timdev = rte_event_timer_adapter_create(&config);

	TEST_ASSERT_NOT_NULL(timdev, "Failed to create event timer ring");

	testsuite_params.timdev = timdev;

	TEST_ASSERT_EQUAL(rte_event_timer_adapter_start(timdev), 0,
			"Failed to start event timer adapter");

	rte_event_timer_adapter_get_info(timdev, &info);
	testsuite_params.min_resolution_ns = info.min_resolution_ns;

	return TEST_SUCCESS;
}
#endif /* RTE_LIB_EVENTDEV */

static int
ut_setup_pdcp_event_timer(void)
{
#ifdef RTE_LIB_EVENTDEV
	int ret;

	ret = eventdev_setup();
	if (ret)
		return ret;

	return event_timer_setup();
#else
	return TEST_SKIPPED;
#endif /* RTE_LIB_EVENTDEV */
}

static void
ut_teardown_pdcp_event_timer(void)
{
#ifdef RTE_LIB_EVENTDEV
	struct rte_event_timer_adapter *timdev = testsuite_params.timdev;
	int evdev = testsuite_params.evdev;

	rte_event_dev_stop(evdev);
	rte_event_dev_close(evdev);

	rte_event_timer_adapter_stop(timdev);
	rte_event_timer_adapter_free(timdev);
#endif /* RTE_LIB_EVENTDEV */
}

static int
run_test_for_one_known_vec(const void *arg)
{
	struct pdcp_test_conf test_conf;
	int i = *(const uint32_t *)arg;

	create_test_conf_from_index(i, &test_conf);
	return test_attempt_single(&test_conf);
}

static struct unit_test_suite combined_mode_cases  = {
	.suite_name = "PDCP combined mode",
	.unit_test_cases = {
		TEST_CASE_NAMED_WITH_DATA("combined mode", ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec, test_combined),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite hfn_sn_test_cases  = {
	.suite_name = "PDCP HFN/SN",
	.unit_test_cases = {
		TEST_CASE_NAMED_WITH_DATA("SN plus window", ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec, test_sn_plus_window),
		TEST_CASE_NAMED_WITH_DATA("SN minus window", ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec, test_sn_minus_window),
		TEST_CASE_NAMED_WITH_DATA("SN plus outside", ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec, test_sn_plus_outside),
		TEST_CASE_NAMED_WITH_DATA("SN minus outside", ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec, test_sn_minus_outside),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite reorder_test_cases  = {
	.suite_name = "PDCP reorder",
	.unit_test_cases = {
		TEST_CASE_NAMED_WITH_DATA("test_reorder_gap_fill",
			ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec, test_reorder_gap_fill),
		TEST_CASE_NAMED_WITH_DATA("test_reorder_gap_in_reorder_buffer",
			ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec, test_reorder_gap_in_reorder_buffer),
		TEST_CASE_NAMED_WITH_DATA("test_reorder_buffer_full_window_size_sn_12",
			ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec_until_first_pass,
			test_reorder_buffer_full_window_size_sn_12),
		TEST_CASE_NAMED_WITH_DATA("test_expire_with_event_timer",
			ut_setup_pdcp_event_timer, ut_teardown_pdcp_event_timer,
			run_test_with_all_known_vec_until_first_pass,
			test_expiry_with_event_timer),
		TEST_CASE_NAMED_WITH_DATA("test_expire_with_rte_timer",
			ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec_until_first_pass,
			test_expiry_with_rte_timer),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite status_report_test_cases  = {
	.suite_name = "PDCP status report",
	.unit_test_cases = {
		TEST_CASE_NAMED_WITH_DATA("test_status_report_fmc_only",
			ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec, test_status_report_fmc_only),
		TEST_CASE_NAMED_WITH_DATA("test_status_report_one_pkt_first_slab",
			ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec, test_status_report_one_pkt_first_slab),
		TEST_CASE_NAMED_WITH_DATA("test_status_report_one_pkt_second_slab",
			ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec, test_status_report_one_pkt_second_slab),
		TEST_CASE_NAMED_WITH_DATA("test_status_report_full_slab",
			ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec, test_status_report_full_slab),
		TEST_CASE_NAMED_WITH_DATA("test_status_report_two_sequential_slabs",
			ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec, test_status_report_two_sequential_slabs),
		TEST_CASE_NAMED_WITH_DATA("test_status_report_two_non_sequential_slabs",
			ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec, test_status_report_two_non_sequential_slabs),
		TEST_CASE_NAMED_WITH_DATA("test_status_report_max_length_sn_12",
			ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec_until_first_pass,
			test_status_report_max_length_sn_12),
		TEST_CASE_NAMED_WITH_DATA("test_status_report_overlap_different_slabs",
			ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec, test_status_report_overlap_different_slabs),
		TEST_CASE_NAMED_WITH_DATA("test_status_report_overlap_same_slab",
			ut_setup_pdcp, ut_teardown_pdcp,
			run_test_with_all_known_vec, test_status_report_overlap_same_slab),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

struct unit_test_suite *test_suites[] = {
	NULL, /* Place holder for known_vector_cases */
	&combined_mode_cases,
	&hfn_sn_test_cases,
	&reorder_test_cases,
	&status_report_test_cases,
	NULL /* End of suites list */
};

static struct unit_test_suite pdcp_testsuite  = {
	.suite_name = "PDCP Unit Test Suite",
	.unit_test_cases = {TEST_CASES_END()},
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_suites = test_suites,
};

static int
test_pdcp(void)
{
	struct unit_test_suite *known_vector_cases;
	int ret, index[NB_TESTS];
	uint32_t i, size;

	size = sizeof(struct unit_test_suite);
	size += (NB_TESTS + 1) * sizeof(struct unit_test_case);

	known_vector_cases = rte_zmalloc(NULL, size, 0);
	if (known_vector_cases == NULL)
		return TEST_FAILED;

	known_vector_cases->suite_name = "Known vector cases";

	for (i = 0; i < NB_TESTS; i++) {
		index[i] = i;
		known_vector_cases->unit_test_cases[i].name = pdcp_test_params[i].name;
		known_vector_cases->unit_test_cases[i].data = (void *)&index[i];
		known_vector_cases->unit_test_cases[i].enabled = 1;
		known_vector_cases->unit_test_cases[i].setup = ut_setup_pdcp;
		known_vector_cases->unit_test_cases[i].teardown = ut_teardown_pdcp;
		known_vector_cases->unit_test_cases[i].testcase = NULL;
		known_vector_cases->unit_test_cases[i].testcase_with_data
				= run_test_for_one_known_vec;
	}

	known_vector_cases->unit_test_cases[i].testcase = NULL;
	known_vector_cases->unit_test_cases[i].testcase_with_data = NULL;

	test_suites[0] = known_vector_cases;

	ret = unit_test_suite_runner(&pdcp_testsuite);

	rte_free(known_vector_cases);
	return ret;
}

REGISTER_TEST_COMMAND(pdcp_autotest, test_pdcp);
