/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_pdcp.h>
#include <rte_pdcp_hdr.h>

#include "test.h"
#include "test_cryptodev.h"
#include "test_cryptodev_security_pdcp_test_vectors.h"

#define NB_DESC 1024
#define CDEV_INVALID_ID UINT8_MAX
#define NB_TESTS RTE_DIM(pdcp_test_params)
#define PDCP_IV_LEN 16

struct pdcp_testsuite_params {
	struct rte_mempool *mbuf_pool;
	struct rte_mempool *cop_pool;
	struct rte_mempool *sess_pool;
	bool cdevs_used[RTE_CRYPTO_MAX_DEVS];
};

static struct pdcp_testsuite_params testsuite_params;

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

static inline int
pdcp_hdr_size_get(enum rte_security_pdcp_sn_size sn_size)
{
	return RTE_ALIGN_MUL_CEIL(sn_size, 8) / 8;
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

	return 0;

cop_pool_free:
	rte_mempool_free(ts_params->cop_pool);
	ts_params->cop_pool = NULL;
mbuf_pool_free:
	rte_mempool_free(ts_params->mbuf_pool);
	ts_params->mbuf_pool = NULL;
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
	conf->entity.pdcp_xfrm.bearer = pdcp_test_bearer[index];
	conf->entity.pdcp_xfrm.en_ordering = 0;
	conf->entity.pdcp_xfrm.remove_duplicates = 0;
	conf->entity.pdcp_xfrm.domain = pdcp_test_params[index].domain;

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

	ret = pdcp_known_vec_verify(mbuf, t_conf->output, t_conf->output_len);
	if (ret)
		goto mbuf_free;

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

static int
run_test_for_one_known_vec(const void *arg)
{
	struct pdcp_test_conf test_conf;
	int i = *(const uint32_t *)arg;

	create_test_conf_from_index(i, &test_conf);
	return test_attempt_single(&test_conf);
}

struct unit_test_suite *test_suites[] = {
	NULL, /* Place holder for known_vector_cases */
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
