/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <time.h>

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_cycles.h>
#include <rte_bus_vdev.h>
#include <rte_ip.h>

#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_lcore.h>
#include <rte_ipsec.h>
#include <rte_random.h>
#include <rte_esp.h>
#include <rte_security_driver.h>

#include "test.h"
#include "test_cryptodev.h"

#define VDEV_ARGS_SIZE	100
#define MAX_NB_SESSIONS	100
#define MAX_NB_SAS		2
#define REPLAY_WIN_0	0
#define REPLAY_WIN_32	32
#define REPLAY_WIN_64	64
#define REPLAY_WIN_128	128
#define REPLAY_WIN_256	256
#define DATA_64_BYTES	64
#define DATA_80_BYTES	80
#define DATA_100_BYTES	100
#define ESN_ENABLED		1
#define ESN_DISABLED	0
#define INBOUND_SPI		7
#define OUTBOUND_SPI	17
#define BURST_SIZE		32
#define REORDER_PKTS	1
#define DEQUEUE_COUNT	1000

struct user_params {
	enum rte_crypto_sym_xform_type auth;
	enum rte_crypto_sym_xform_type cipher;
	enum rte_crypto_sym_xform_type aead;

	char auth_algo[128];
	char cipher_algo[128];
	char aead_algo[128];
};

struct ipsec_testsuite_params {
	struct rte_mempool *mbuf_pool;
	struct rte_mempool *cop_mpool;
	struct rte_cryptodev_config conf;
	struct rte_cryptodev_qp_conf qp_conf;

	uint8_t valid_dev;
	uint8_t valid_dev_found;
};

struct ipsec_unitest_params {
	struct rte_crypto_sym_xform cipher_xform;
	struct rte_crypto_sym_xform auth_xform;
	struct rte_crypto_sym_xform aead_xform;
	struct rte_crypto_sym_xform *crypto_xforms;

	struct rte_security_ipsec_xform ipsec_xform;

	struct rte_ipsec_sa_prm sa_prm;
	struct rte_ipsec_session ss[MAX_NB_SAS];

	struct rte_crypto_op *cop[BURST_SIZE];

	struct rte_mbuf *obuf[BURST_SIZE], *ibuf[BURST_SIZE],
		*testbuf[BURST_SIZE];

	uint16_t pkt_index;
};

struct ipsec_test_cfg {
	uint32_t replay_win_sz;
	uint32_t esn;
	uint64_t flags;
	size_t pkt_sz;
	uint16_t num_pkts;
	uint32_t reorder_pkts;
};

static const struct ipsec_test_cfg test_cfg[] = {

	{REPLAY_WIN_0, ESN_DISABLED, 0, DATA_64_BYTES, 1, 0},
	{REPLAY_WIN_0, ESN_DISABLED, 0, DATA_80_BYTES, BURST_SIZE,
		REORDER_PKTS},
	{REPLAY_WIN_32, ESN_ENABLED, 0, DATA_100_BYTES, 1, 0},
	{REPLAY_WIN_32, ESN_ENABLED, 0, DATA_100_BYTES, BURST_SIZE,
		REORDER_PKTS},
	{REPLAY_WIN_64, ESN_ENABLED, 0, DATA_64_BYTES, 1, 0},
	{REPLAY_WIN_128, ESN_ENABLED, RTE_IPSEC_SAFLAG_SQN_ATOM,
		DATA_80_BYTES, 1, 0},
	{REPLAY_WIN_256, ESN_DISABLED, 0, DATA_100_BYTES, 1, 0},
};

static const int num_cfg = RTE_DIM(test_cfg);
static struct ipsec_testsuite_params testsuite_params = { NULL };
static struct ipsec_unitest_params unittest_params;
static struct user_params uparams;

struct supported_cipher_algo {
	const char *keyword;
	enum rte_crypto_cipher_algorithm algo;
	uint16_t iv_len;
	uint16_t block_size;
	uint16_t key_len;
};

struct supported_auth_algo {
	const char *keyword;
	enum rte_crypto_auth_algorithm algo;
	uint16_t digest_len;
	uint16_t key_len;
	uint8_t key_not_req;
};

const struct supported_cipher_algo cipher_algos[] = {
	{
		.keyword = "null",
		.algo = RTE_CRYPTO_CIPHER_NULL,
		.iv_len = 0,
		.block_size = 4,
		.key_len = 0
	},
};

const struct supported_auth_algo auth_algos[] = {
	{
		.keyword = "null",
		.algo = RTE_CRYPTO_AUTH_NULL,
		.digest_len = 0,
		.key_len = 0,
		.key_not_req = 1
	},
};

static int
dummy_sec_create(void *device, struct rte_security_session_conf *conf,
	struct rte_security_session *sess, struct rte_mempool *mp)
{
	RTE_SET_USED(device);
	RTE_SET_USED(conf);
	RTE_SET_USED(mp);

	sess->sess_private_data = NULL;
	return 0;
}

static int
dummy_sec_destroy(void *device, struct rte_security_session *sess)
{
	RTE_SET_USED(device);
	RTE_SET_USED(sess);
	return 0;
}

static const struct rte_security_ops dummy_sec_ops = {
	.session_create = dummy_sec_create,
	.session_destroy = dummy_sec_destroy,
};

static struct rte_security_ctx dummy_sec_ctx = {
	.ops = &dummy_sec_ops,
};

static const struct supported_cipher_algo *
find_match_cipher_algo(const char *cipher_keyword)
{
	size_t i;

	for (i = 0; i < RTE_DIM(cipher_algos); i++) {
		const struct supported_cipher_algo *algo =
			&cipher_algos[i];

		if (strcmp(cipher_keyword, algo->keyword) == 0)
			return algo;
	}

	return NULL;
}

static const struct supported_auth_algo *
find_match_auth_algo(const char *auth_keyword)
{
	size_t i;

	for (i = 0; i < RTE_DIM(auth_algos); i++) {
		const struct supported_auth_algo *algo =
			&auth_algos[i];

		if (strcmp(auth_keyword, algo->keyword) == 0)
			return algo;
	}

	return NULL;
}

static void
fill_crypto_xform(struct ipsec_unitest_params *ut_params,
	const struct supported_auth_algo *auth_algo,
	const struct supported_cipher_algo *cipher_algo)
{
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.cipher.algo = cipher_algo->algo;
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.auth.algo = auth_algo->algo;

	if (ut_params->ipsec_xform.direction ==
			RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
		ut_params->cipher_xform.cipher.op =
			RTE_CRYPTO_CIPHER_OP_DECRYPT;
		ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
		ut_params->cipher_xform.next = NULL;
		ut_params->auth_xform.next = &ut_params->cipher_xform;
		ut_params->crypto_xforms = &ut_params->auth_xform;
	} else {
		ut_params->cipher_xform.cipher.op =
			RTE_CRYPTO_CIPHER_OP_ENCRYPT;
		ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
		ut_params->auth_xform.next = NULL;
		ut_params->cipher_xform.next = &ut_params->auth_xform;
		ut_params->crypto_xforms = &ut_params->cipher_xform;
	}
}

static int
check_cryptodev_capablity(const struct ipsec_unitest_params *ut,
		uint8_t dev_id)
{
	struct rte_cryptodev_sym_capability_idx cap_idx;
	const struct rte_cryptodev_symmetric_capability *cap;
	int rc = -1;

	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = ut->auth_xform.auth.algo;
	cap = rte_cryptodev_sym_capability_get(dev_id, &cap_idx);

	if (cap != NULL) {
		rc = rte_cryptodev_sym_capability_check_auth(cap,
				ut->auth_xform.auth.key.length,
				ut->auth_xform.auth.digest_length, 0);
		if (rc == 0) {
			cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
			cap_idx.algo.cipher = ut->cipher_xform.cipher.algo;
			cap = rte_cryptodev_sym_capability_get(
					dev_id, &cap_idx);
			if (cap != NULL)
				rc = rte_cryptodev_sym_capability_check_cipher(
					cap,
					ut->cipher_xform.cipher.key.length,
					ut->cipher_xform.cipher.iv.length);
		}
	}

	return rc;
}

static int
testsuite_setup(void)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	const struct supported_auth_algo *auth_algo;
	const struct supported_cipher_algo *cipher_algo;
	struct rte_cryptodev_info info;
	uint32_t i, nb_devs, dev_id;
	size_t sess_sz;
	int rc;

	memset(ts_params, 0, sizeof(*ts_params));
	memset(ut_params, 0, sizeof(*ut_params));
	memset(&uparams, 0, sizeof(struct user_params));

	uparams.auth = RTE_CRYPTO_SYM_XFORM_AUTH;
	uparams.cipher = RTE_CRYPTO_SYM_XFORM_CIPHER;
	uparams.aead = RTE_CRYPTO_SYM_XFORM_NOT_SPECIFIED;
	strcpy(uparams.auth_algo, "null");
	strcpy(uparams.cipher_algo, "null");

	auth_algo = find_match_auth_algo(uparams.auth_algo);
	cipher_algo = find_match_cipher_algo(uparams.cipher_algo);
	fill_crypto_xform(ut_params, auth_algo, cipher_algo);

	nb_devs = rte_cryptodev_count();
	if (nb_devs < 1) {
		RTE_LOG(ERR, USER1, "No crypto devices found?\n");
		return TEST_FAILED;
	}

	/* Find first valid crypto device */
	for (i = 0; i < nb_devs; i++) {
		rc = check_cryptodev_capablity(ut_params, i);
		if (rc == 0) {
			ts_params->valid_dev = i;
			ts_params->valid_dev_found = 1;
			break;
		}
	}

	if (ts_params->valid_dev_found == 0)
		return TEST_FAILED;

	ts_params->mbuf_pool = rte_pktmbuf_pool_create(
			"CRYPTO_MBUFPOOL",
			NUM_MBUFS, MBUF_CACHE_SIZE, 0, MBUF_SIZE,
			rte_socket_id());
	if (ts_params->mbuf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Can't create CRYPTO_MBUFPOOL\n");
		return TEST_FAILED;
	}

	ts_params->cop_mpool = rte_crypto_op_pool_create(
			"MBUF_CRYPTO_SYM_OP_POOL",
			RTE_CRYPTO_OP_TYPE_SYMMETRIC,
			NUM_MBUFS, MBUF_CACHE_SIZE,
			DEFAULT_NUM_XFORMS *
			sizeof(struct rte_crypto_sym_xform) +
			MAXIMUM_IV_LENGTH,
			rte_socket_id());
	if (ts_params->cop_mpool == NULL) {
		RTE_LOG(ERR, USER1, "Can't create CRYPTO_OP_POOL\n");
		return TEST_FAILED;
	}

	/* Set up all the qps on the first of the valid devices found */
	dev_id = ts_params->valid_dev;

	rte_cryptodev_info_get(dev_id, &info);

	ts_params->conf.nb_queue_pairs = info.max_nb_queue_pairs;
	ts_params->conf.socket_id = SOCKET_ID_ANY;

	sess_sz = rte_cryptodev_sym_get_private_session_size(dev_id);
	sess_sz = RTE_MAX(sess_sz, sizeof(struct rte_security_session));

	/*
	 * Create mempools for sessions
	 */
	if (info.sym.max_nb_sessions != 0 &&
			info.sym.max_nb_sessions < MAX_NB_SESSIONS) {
		RTE_LOG(ERR, USER1, "Device does not support "
				"at least %u sessions\n",
				MAX_NB_SESSIONS);
		return TEST_FAILED;
	}

	ts_params->qp_conf.mp_session_private = rte_mempool_create(
				"test_priv_sess_mp",
				MAX_NB_SESSIONS,
				sess_sz,
				0, 0, NULL, NULL, NULL,
				NULL, SOCKET_ID_ANY,
				0);

	TEST_ASSERT_NOT_NULL(ts_params->qp_conf.mp_session_private,
			"private session mempool allocation failed");

	ts_params->qp_conf.mp_session =
		rte_cryptodev_sym_session_pool_create("test_sess_mp",
			MAX_NB_SESSIONS, 0, 0, 0, SOCKET_ID_ANY);

	TEST_ASSERT_NOT_NULL(ts_params->qp_conf.mp_session,
			"session mempool allocation failed");

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(dev_id,
			&ts_params->conf),
			"Failed to configure cryptodev %u with %u qps",
			dev_id, ts_params->conf.nb_queue_pairs);

	ts_params->qp_conf.nb_descriptors = DEFAULT_NUM_OPS_INFLIGHT;

	TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
		dev_id, 0, &ts_params->qp_conf,
		rte_cryptodev_socket_id(dev_id)),
		"Failed to setup queue pair %u on cryptodev %u",
		0, dev_id);

	return TEST_SUCCESS;
}

static void
testsuite_teardown(void)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;

	if (ts_params->mbuf_pool != NULL) {
		RTE_LOG(DEBUG, USER1, "CRYPTO_MBUFPOOL count %u\n",
		rte_mempool_avail_count(ts_params->mbuf_pool));
		rte_mempool_free(ts_params->mbuf_pool);
		ts_params->mbuf_pool = NULL;
	}

	if (ts_params->cop_mpool != NULL) {
		RTE_LOG(DEBUG, USER1, "CRYPTO_OP_POOL count %u\n",
		rte_mempool_avail_count(ts_params->cop_mpool));
		rte_mempool_free(ts_params->cop_mpool);
		ts_params->cop_mpool = NULL;
	}

	/* Free session mempools */
	if (ts_params->qp_conf.mp_session != NULL) {
		rte_mempool_free(ts_params->qp_conf.mp_session);
		ts_params->qp_conf.mp_session = NULL;
	}

	if (ts_params->qp_conf.mp_session_private != NULL) {
		rte_mempool_free(ts_params->qp_conf.mp_session_private);
		ts_params->qp_conf.mp_session_private = NULL;
	}
}

static int
ut_setup(void)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;

	/* Clear unit test parameters before running test */
	memset(ut_params, 0, sizeof(*ut_params));

	/* Reconfigure device to default parameters */
	ts_params->conf.socket_id = SOCKET_ID_ANY;

	/* Start the device */
	TEST_ASSERT_SUCCESS(rte_cryptodev_start(ts_params->valid_dev),
			"Failed to start cryptodev %u",
			ts_params->valid_dev);

	return TEST_SUCCESS;
}

static void
ut_teardown(void)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	int i;

	for (i = 0; i < BURST_SIZE; i++) {
		/* free crypto operation structure */
		if (ut_params->cop[i])
			rte_crypto_op_free(ut_params->cop[i]);

		/*
		 * free mbuf - both obuf and ibuf are usually the same,
		 * so check if they point at the same address is necessary,
		 * to avoid freeing the mbuf twice.
		 */
		if (ut_params->obuf[i]) {
			rte_pktmbuf_free(ut_params->obuf[i]);
			if (ut_params->ibuf[i] == ut_params->obuf[i])
				ut_params->ibuf[i] = 0;
			ut_params->obuf[i] = 0;
		}
		if (ut_params->ibuf[i]) {
			rte_pktmbuf_free(ut_params->ibuf[i]);
			ut_params->ibuf[i] = 0;
		}

		if (ut_params->testbuf[i]) {
			rte_pktmbuf_free(ut_params->testbuf[i]);
			ut_params->testbuf[i] = 0;
		}
	}

	if (ts_params->mbuf_pool != NULL)
		RTE_LOG(DEBUG, USER1, "CRYPTO_MBUFPOOL count %u\n",
			rte_mempool_avail_count(ts_params->mbuf_pool));

	/* Stop the device */
	rte_cryptodev_stop(ts_params->valid_dev);
}

#define IPSEC_MAX_PAD_SIZE	UINT8_MAX

static const uint8_t esp_pad_bytes[IPSEC_MAX_PAD_SIZE] = {
	1, 2, 3, 4, 5, 6, 7, 8,
	9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24,
	25, 26, 27, 28, 29, 30, 31, 32,
	33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48,
	49, 50, 51, 52, 53, 54, 55, 56,
	57, 58, 59, 60, 61, 62, 63, 64,
	65, 66, 67, 68, 69, 70, 71, 72,
	73, 74, 75, 76, 77, 78, 79, 80,
	81, 82, 83, 84, 85, 86, 87, 88,
	89, 90, 91, 92, 93, 94, 95, 96,
	97, 98, 99, 100, 101, 102, 103, 104,
	105, 106, 107, 108, 109, 110, 111, 112,
	113, 114, 115, 116, 117, 118, 119, 120,
	121, 122, 123, 124, 125, 126, 127, 128,
	129, 130, 131, 132, 133, 134, 135, 136,
	137, 138, 139, 140, 141, 142, 143, 144,
	145, 146, 147, 148, 149, 150, 151, 152,
	153, 154, 155, 156, 157, 158, 159, 160,
	161, 162, 163, 164, 165, 166, 167, 168,
	169, 170, 171, 172, 173, 174, 175, 176,
	177, 178, 179, 180, 181, 182, 183, 184,
	185, 186, 187, 188, 189, 190, 191, 192,
	193, 194, 195, 196, 197, 198, 199, 200,
	201, 202, 203, 204, 205, 206, 207, 208,
	209, 210, 211, 212, 213, 214, 215, 216,
	217, 218, 219, 220, 221, 222, 223, 224,
	225, 226, 227, 228, 229, 230, 231, 232,
	233, 234, 235, 236, 237, 238, 239, 240,
	241, 242, 243, 244, 245, 246, 247, 248,
	249, 250, 251, 252, 253, 254, 255,
};

/* ***** data for tests ***** */

const char null_plain_data[] =
	"Network Security People Have A Strange Sense Of Humor unlike Other "
	"People who have a normal sense of humour";

const char null_encrypted_data[] =
	"Network Security People Have A Strange Sense Of Humor unlike Other "
	"People who have a normal sense of humour";

struct ipv4_hdr ipv4_outer  = {
	.version_ihl = IPVERSION << 4 |
		sizeof(ipv4_outer) / IPV4_IHL_MULTIPLIER,
	.time_to_live = IPDEFTTL,
	.next_proto_id = IPPROTO_ESP,
	.src_addr = IPv4(192, 168, 1, 100),
	.dst_addr = IPv4(192, 168, 2, 100),
};

static struct rte_mbuf *
setup_test_string(struct rte_mempool *mpool,
		const char *string, size_t len, uint8_t blocksize)
{
	struct rte_mbuf *m = rte_pktmbuf_alloc(mpool);
	size_t t_len = len - (blocksize ? (len % blocksize) : 0);

	if (m) {
		memset(m->buf_addr, 0, m->buf_len);
		char *dst = rte_pktmbuf_append(m, t_len);

		if (!dst) {
			rte_pktmbuf_free(m);
			return NULL;
		}
		if (string != NULL)
			rte_memcpy(dst, string, t_len);
		else
			memset(dst, 0, t_len);
	}

	return m;
}

static struct rte_mbuf *
setup_test_string_tunneled(struct rte_mempool *mpool, const char *string,
	size_t len, uint32_t spi, uint32_t seq)
{
	struct rte_mbuf *m = rte_pktmbuf_alloc(mpool);
	uint32_t hdrlen = sizeof(struct ipv4_hdr) + sizeof(struct esp_hdr);
	uint32_t taillen = sizeof(struct esp_tail);
	uint32_t t_len = len + hdrlen + taillen;
	uint32_t padlen;

	struct esp_hdr esph  = {
		.spi = rte_cpu_to_be_32(spi),
		.seq = rte_cpu_to_be_32(seq)
	};

	padlen = RTE_ALIGN(t_len, 4) - t_len;
	t_len += padlen;

	struct esp_tail espt  = {
		.pad_len = padlen,
		.next_proto = IPPROTO_IPIP,
	};

	if (m == NULL)
		return NULL;

	memset(m->buf_addr, 0, m->buf_len);
	char *dst = rte_pktmbuf_append(m, t_len);

	if (!dst) {
		rte_pktmbuf_free(m);
		return NULL;
	}
	/* copy outer IP and ESP header */
	ipv4_outer.total_length = rte_cpu_to_be_16(t_len);
	ipv4_outer.packet_id = rte_cpu_to_be_16(seq);
	rte_memcpy(dst, &ipv4_outer, sizeof(ipv4_outer));
	dst += sizeof(ipv4_outer);
	m->l3_len = sizeof(ipv4_outer);
	rte_memcpy(dst, &esph, sizeof(esph));
	dst += sizeof(esph);

	if (string != NULL) {
		/* copy payload */
		rte_memcpy(dst, string, len);
		dst += len;
		/* copy pad bytes */
		rte_memcpy(dst, esp_pad_bytes, padlen);
		dst += padlen;
		/* copy ESP tail header */
		rte_memcpy(dst, &espt, sizeof(espt));
	} else
		memset(dst, 0, t_len);

	return m;
}

static int
create_dummy_sec_session(struct ipsec_unitest_params *ut,
	struct rte_cryptodev_qp_conf *qp, uint32_t j)
{
	static struct rte_security_session_conf conf;

	ut->ss[j].security.ses = rte_security_session_create(&dummy_sec_ctx,
					&conf, qp->mp_session_private);

	if (ut->ss[j].security.ses == NULL)
		return -ENOMEM;

	ut->ss[j].security.ctx = &dummy_sec_ctx;
	ut->ss[j].security.ol_flags = 0;
	return 0;
}

static int
create_crypto_session(struct ipsec_unitest_params *ut,
	struct rte_cryptodev_qp_conf *qp, uint8_t dev_id, uint32_t j)
{
	int32_t rc;
	struct rte_cryptodev_sym_session *s;

	s = rte_cryptodev_sym_session_create(qp->mp_session);
	if (s == NULL)
		return -ENOMEM;

	/* initiliaze SA crypto session for device */
	rc = rte_cryptodev_sym_session_init(dev_id, s,
			ut->crypto_xforms, qp->mp_session_private);
	if (rc == 0) {
		ut->ss[j].crypto.ses = s;
		return 0;
	} else {
		/* failure, do cleanup */
		rte_cryptodev_sym_session_clear(dev_id, s);
		rte_cryptodev_sym_session_free(s);
		return rc;
	}
}

static int
create_session(struct ipsec_unitest_params *ut,
	struct rte_cryptodev_qp_conf *qp, uint8_t crypto_dev, uint32_t j)
{
	if (ut->ss[j].type == RTE_SECURITY_ACTION_TYPE_NONE)
		return create_crypto_session(ut, qp, crypto_dev, j);
	else
		return create_dummy_sec_session(ut, qp, j);
}

static int
fill_ipsec_param(uint32_t replay_win_sz, uint64_t flags)
{
	struct ipsec_unitest_params *ut_params = &unittest_params;
	struct rte_ipsec_sa_prm *prm = &ut_params->sa_prm;
	const struct supported_auth_algo *auth_algo;
	const struct supported_cipher_algo *cipher_algo;

	memset(prm, 0, sizeof(*prm));

	prm->userdata = 1;
	prm->flags = flags;
	prm->replay_win_sz = replay_win_sz;

	/* setup ipsec xform */
	prm->ipsec_xform = ut_params->ipsec_xform;
	prm->ipsec_xform.salt = (uint32_t)rte_rand();

	/* setup tunnel related fields */
	prm->tun.hdr_len = sizeof(ipv4_outer);
	prm->tun.next_proto = IPPROTO_IPIP;
	prm->tun.hdr = &ipv4_outer;

	/* setup crypto section */
	if (uparams.aead != 0) {
		/* TODO: will need to fill out with other test cases */
	} else {
		if (uparams.auth == 0 && uparams.cipher == 0)
			return TEST_FAILED;

		auth_algo = find_match_auth_algo(uparams.auth_algo);
		cipher_algo = find_match_cipher_algo(uparams.cipher_algo);

		fill_crypto_xform(ut_params, auth_algo, cipher_algo);
	}

	prm->crypto_xform = ut_params->crypto_xforms;
	return TEST_SUCCESS;
}

static int
create_sa(enum rte_security_session_action_type action_type,
		uint32_t replay_win_sz, uint64_t flags, uint32_t j)
{
	struct ipsec_testsuite_params *ts = &testsuite_params;
	struct ipsec_unitest_params *ut = &unittest_params;
	size_t sz;
	int rc;

	memset(&ut->ss[j], 0, sizeof(ut->ss[j]));

	rc = fill_ipsec_param(replay_win_sz, flags);
	if (rc != 0)
		return TEST_FAILED;

	/* create rte_ipsec_sa*/
	sz = rte_ipsec_sa_size(&ut->sa_prm);
	TEST_ASSERT(sz > 0, "rte_ipsec_sa_size() failed\n");

	ut->ss[j].sa = rte_zmalloc(NULL, sz, RTE_CACHE_LINE_SIZE);
	TEST_ASSERT_NOT_NULL(ut->ss[j].sa,
		"failed to allocate memory for rte_ipsec_sa\n");

	ut->ss[j].type = action_type;
	rc = create_session(ut, &ts->qp_conf, ts->valid_dev, j);
	if (rc != 0)
		return TEST_FAILED;

	rc = rte_ipsec_sa_init(ut->ss[j].sa, &ut->sa_prm, sz);
	rc = (rc > 0 && (uint32_t)rc <= sz) ? 0 : -EINVAL;
	if (rc == 0)
		rc = rte_ipsec_session_prepare(&ut->ss[j]);

	return rc;
}

static int
crypto_dequeue_burst(uint16_t num_pkts)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	uint32_t pkt_cnt, k;
	int i;

	for (i = 0, pkt_cnt = 0;
		i < DEQUEUE_COUNT && pkt_cnt != num_pkts; i++) {
		k = rte_cryptodev_dequeue_burst(ts_params->valid_dev, 0,
			&ut_params->cop[pkt_cnt], num_pkts - pkt_cnt);
		pkt_cnt += k;
		rte_delay_us(1);
	}

	if (pkt_cnt != num_pkts) {
		RTE_LOG(ERR, USER1, "rte_cryptodev_dequeue_burst fail\n");
		return TEST_FAILED;
	}
	return TEST_SUCCESS;
}

static int
crypto_ipsec(uint16_t num_pkts)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	uint32_t k, ng;
	struct rte_ipsec_group grp[1];

	/* call crypto prepare */
	k = rte_ipsec_pkt_crypto_prepare(&ut_params->ss[0], ut_params->ibuf,
		ut_params->cop, num_pkts);
	if (k != num_pkts) {
		RTE_LOG(ERR, USER1, "rte_ipsec_pkt_crypto_prepare fail\n");
		return TEST_FAILED;
	}

	k = rte_cryptodev_enqueue_burst(ts_params->valid_dev, 0,
		ut_params->cop, num_pkts);
	if (k != num_pkts) {
		RTE_LOG(ERR, USER1, "rte_cryptodev_enqueue_burst fail\n");
		return TEST_FAILED;
	}

	if (crypto_dequeue_burst(num_pkts) == TEST_FAILED)
		return TEST_FAILED;

	ng = rte_ipsec_pkt_crypto_group(
		(const struct rte_crypto_op **)(uintptr_t)ut_params->cop,
		ut_params->obuf, grp, num_pkts);
	if (ng != 1 ||
		grp[0].m[0] != ut_params->obuf[0] ||
		grp[0].cnt != num_pkts ||
		grp[0].id.ptr != &ut_params->ss[0]) {
		RTE_LOG(ERR, USER1, "rte_ipsec_pkt_crypto_group fail\n");
		return TEST_FAILED;
	}

	/* call crypto process */
	k = rte_ipsec_pkt_process(grp[0].id.ptr, grp[0].m, grp[0].cnt);
	if (k != num_pkts) {
		RTE_LOG(ERR, USER1, "rte_ipsec_pkt_process fail\n");
		return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static int
lksd_proto_ipsec(uint16_t num_pkts)
{
	struct ipsec_unitest_params *ut_params = &unittest_params;
	uint32_t i, k, ng;
	struct rte_ipsec_group grp[1];

	/* call crypto prepare */
	k = rte_ipsec_pkt_crypto_prepare(&ut_params->ss[0], ut_params->ibuf,
		ut_params->cop, num_pkts);
	if (k != num_pkts) {
		RTE_LOG(ERR, USER1, "rte_ipsec_pkt_crypto_prepare fail\n");
		return TEST_FAILED;
	}

	/* check crypto ops */
	for (i = 0; i != num_pkts; i++) {
		TEST_ASSERT_EQUAL(ut_params->cop[i]->type,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC,
			"%s: invalid crypto op type for %u-th packet\n",
			__func__, i);
		TEST_ASSERT_EQUAL(ut_params->cop[i]->status,
			RTE_CRYPTO_OP_STATUS_NOT_PROCESSED,
			"%s: invalid crypto op status for %u-th packet\n",
			__func__, i);
		TEST_ASSERT_EQUAL(ut_params->cop[i]->sess_type,
			RTE_CRYPTO_OP_SECURITY_SESSION,
			"%s: invalid crypto op sess_type for %u-th packet\n",
			__func__, i);
		TEST_ASSERT_EQUAL(ut_params->cop[i]->sym->m_src,
			ut_params->ibuf[i],
			"%s: invalid crypto op m_src for %u-th packet\n",
			__func__, i);
	}

	/* update crypto ops, pretend all finished ok */
	for (i = 0; i != num_pkts; i++)
		ut_params->cop[i]->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

	ng = rte_ipsec_pkt_crypto_group(
		(const struct rte_crypto_op **)(uintptr_t)ut_params->cop,
		ut_params->obuf, grp, num_pkts);
	if (ng != 1 ||
		grp[0].m[0] != ut_params->obuf[0] ||
		grp[0].cnt != num_pkts ||
		grp[0].id.ptr != &ut_params->ss[0]) {
		RTE_LOG(ERR, USER1, "rte_ipsec_pkt_crypto_group fail\n");
		return TEST_FAILED;
	}

	/* call crypto process */
	k = rte_ipsec_pkt_process(grp[0].id.ptr, grp[0].m, grp[0].cnt);
	if (k != num_pkts) {
		RTE_LOG(ERR, USER1, "rte_ipsec_pkt_process fail\n");
		return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static int
crypto_ipsec_2sa(void)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	struct rte_ipsec_group grp[BURST_SIZE];
	uint32_t k, ng, i, r;

	for (i = 0; i < BURST_SIZE; i++) {
		r = i % 2;
		/* call crypto prepare */
		k = rte_ipsec_pkt_crypto_prepare(&ut_params->ss[r],
				ut_params->ibuf + i, ut_params->cop + i, 1);
		if (k != 1) {
			RTE_LOG(ERR, USER1,
				"rte_ipsec_pkt_crypto_prepare fail\n");
			return TEST_FAILED;
		}
		k = rte_cryptodev_enqueue_burst(ts_params->valid_dev, 0,
				ut_params->cop + i, 1);
		if (k != 1) {
			RTE_LOG(ERR, USER1,
				"rte_cryptodev_enqueue_burst fail\n");
			return TEST_FAILED;
		}
	}

	if (crypto_dequeue_burst(BURST_SIZE) == TEST_FAILED)
		return TEST_FAILED;

	ng = rte_ipsec_pkt_crypto_group(
		(const struct rte_crypto_op **)(uintptr_t)ut_params->cop,
		ut_params->obuf, grp, BURST_SIZE);
	if (ng != BURST_SIZE) {
		RTE_LOG(ERR, USER1, "rte_ipsec_pkt_crypto_group fail ng=%d\n",
				ng);
		return TEST_FAILED;
	}

	/* call crypto process */
	for (i = 0; i < ng; i++) {
		k = rte_ipsec_pkt_process(grp[i].id.ptr, grp[i].m, grp[i].cnt);
		if (k != grp[i].cnt) {
			RTE_LOG(ERR, USER1, "rte_ipsec_pkt_process fail\n");
			return TEST_FAILED;
		}
	}
	return TEST_SUCCESS;
}

#define PKT_4	4
#define PKT_12	12
#define PKT_21	21

static uint32_t
crypto_ipsec_4grp(uint32_t pkt_num)
{
	uint32_t sa_ind;

	/* group packets in 4 different size groups groups, 2 per SA */
	if (pkt_num < PKT_4)
		sa_ind = 0;
	else if (pkt_num < PKT_12)
		sa_ind = 1;
	else if (pkt_num < PKT_21)
		sa_ind = 0;
	else
		sa_ind = 1;

	return sa_ind;
}

static uint32_t
crypto_ipsec_4grp_check_mbufs(uint32_t grp_ind, struct rte_ipsec_group *grp)
{
	struct ipsec_unitest_params *ut_params = &unittest_params;
	uint32_t i, j;
	uint32_t rc = 0;

	if (grp_ind == 0) {
		for (i = 0, j = 0; i < PKT_4; i++, j++)
			if (grp[grp_ind].m[i] != ut_params->obuf[j]) {
				rc = TEST_FAILED;
				break;
			}
	} else if (grp_ind == 1) {
		for (i = 0, j = PKT_4; i < (PKT_12 - PKT_4); i++, j++) {
			if (grp[grp_ind].m[i] != ut_params->obuf[j]) {
				rc = TEST_FAILED;
				break;
			}
		}
	} else if (grp_ind == 2) {
		for (i = 0, j =  PKT_12; i < (PKT_21 - PKT_12); i++, j++)
			if (grp[grp_ind].m[i] != ut_params->obuf[j]) {
				rc = TEST_FAILED;
				break;
			}
	} else if (grp_ind == 3) {
		for (i = 0, j = PKT_21; i < (BURST_SIZE - PKT_21); i++, j++)
			if (grp[grp_ind].m[i] != ut_params->obuf[j]) {
				rc = TEST_FAILED;
				break;
			}
	} else
		rc = TEST_FAILED;

	return rc;
}

static uint32_t
crypto_ipsec_4grp_check_cnt(uint32_t grp_ind, struct rte_ipsec_group *grp)
{
	uint32_t rc = 0;

	if (grp_ind == 0) {
		if (grp[grp_ind].cnt != PKT_4)
			rc = TEST_FAILED;
	} else if (grp_ind == 1) {
		if (grp[grp_ind].cnt != PKT_12 - PKT_4)
			rc = TEST_FAILED;
	} else if (grp_ind == 2) {
		if (grp[grp_ind].cnt != PKT_21 - PKT_12)
			rc = TEST_FAILED;
	} else if (grp_ind == 3) {
		if (grp[grp_ind].cnt != BURST_SIZE - PKT_21)
			rc = TEST_FAILED;
	} else
		rc = TEST_FAILED;

	return rc;
}

static int
crypto_ipsec_2sa_4grp(void)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	struct rte_ipsec_group grp[BURST_SIZE];
	uint32_t k, ng, i, j;
	uint32_t rc = 0;

	for (i = 0; i < BURST_SIZE; i++) {
		j = crypto_ipsec_4grp(i);

		/* call crypto prepare */
		k = rte_ipsec_pkt_crypto_prepare(&ut_params->ss[j],
				ut_params->ibuf + i, ut_params->cop + i, 1);
		if (k != 1) {
			RTE_LOG(ERR, USER1,
				"rte_ipsec_pkt_crypto_prepare fail\n");
			return TEST_FAILED;
		}
		k = rte_cryptodev_enqueue_burst(ts_params->valid_dev, 0,
				ut_params->cop + i, 1);
		if (k != 1) {
			RTE_LOG(ERR, USER1,
				"rte_cryptodev_enqueue_burst fail\n");
			return TEST_FAILED;
		}
	}

	if (crypto_dequeue_burst(BURST_SIZE) == TEST_FAILED)
		return TEST_FAILED;

	ng = rte_ipsec_pkt_crypto_group(
		(const struct rte_crypto_op **)(uintptr_t)ut_params->cop,
		ut_params->obuf, grp, BURST_SIZE);
	if (ng != 4) {
		RTE_LOG(ERR, USER1, "rte_ipsec_pkt_crypto_group fail ng=%d\n",
			ng);
		return TEST_FAILED;
	}

	/* call crypto process */
	for (i = 0; i < ng; i++) {
		k = rte_ipsec_pkt_process(grp[i].id.ptr, grp[i].m, grp[i].cnt);
		if (k != grp[i].cnt) {
			RTE_LOG(ERR, USER1, "rte_ipsec_pkt_process fail\n");
			return TEST_FAILED;
		}
		rc = crypto_ipsec_4grp_check_cnt(i, grp);
		if (rc != 0) {
			RTE_LOG(ERR, USER1,
				"crypto_ipsec_4grp_check_cnt fail\n");
			return TEST_FAILED;
		}
		rc = crypto_ipsec_4grp_check_mbufs(i, grp);
		if (rc != 0) {
			RTE_LOG(ERR, USER1,
				"crypto_ipsec_4grp_check_mbufs fail\n");
			return TEST_FAILED;
		}
	}
	return TEST_SUCCESS;
}

static void
test_ipsec_reorder_inb_pkt_burst(uint16_t num_pkts)
{
	struct ipsec_unitest_params *ut_params = &unittest_params;
	struct rte_mbuf *ibuf_tmp[BURST_SIZE];
	uint16_t j;

	/* reorder packets and create gaps in sequence numbers */
	static const uint32_t reorder[BURST_SIZE] = {
			24, 25, 26, 27, 28, 29, 30, 31,
			16, 17, 18, 19, 20, 21, 22, 23,
			8, 9, 10, 11, 12, 13, 14, 15,
			0, 1, 2, 3, 4, 5, 6, 7,
	};

	if (num_pkts != BURST_SIZE)
		return;

	for (j = 0; j != BURST_SIZE; j++)
		ibuf_tmp[j] = ut_params->ibuf[reorder[j]];

	memcpy(ut_params->ibuf, ibuf_tmp, sizeof(ut_params->ibuf));
}

static int
test_ipsec_crypto_op_alloc(uint16_t num_pkts)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	int rc = 0;
	uint16_t j;

	for (j = 0; j < num_pkts && rc == 0; j++) {
		ut_params->cop[j] = rte_crypto_op_alloc(ts_params->cop_mpool,
				RTE_CRYPTO_OP_TYPE_SYMMETRIC);
		if (ut_params->cop[j] == NULL) {
			RTE_LOG(ERR, USER1,
				"Failed to allocate symmetric crypto op\n");
			rc = TEST_FAILED;
		}
	}

	return rc;
}

static void
test_ipsec_dump_buffers(struct ipsec_unitest_params *ut_params, int i)
{
	uint16_t j = ut_params->pkt_index;

	printf("\ntest config: num %d\n", i);
	printf("	replay_win_sz %u\n", test_cfg[i].replay_win_sz);
	printf("	esn %u\n", test_cfg[i].esn);
	printf("	flags 0x%" PRIx64 "\n", test_cfg[i].flags);
	printf("	pkt_sz %zu\n", test_cfg[i].pkt_sz);
	printf("	num_pkts %u\n\n", test_cfg[i].num_pkts);

	if (ut_params->ibuf[j]) {
		printf("ibuf[%u] data:\n", j);
		rte_pktmbuf_dump(stdout, ut_params->ibuf[j],
			ut_params->ibuf[j]->data_len);
	}
	if (ut_params->obuf[j]) {
		printf("obuf[%u] data:\n", j);
		rte_pktmbuf_dump(stdout, ut_params->obuf[j],
			ut_params->obuf[j]->data_len);
	}
	if (ut_params->testbuf[j]) {
		printf("testbuf[%u] data:\n", j);
		rte_pktmbuf_dump(stdout, ut_params->testbuf[j],
			ut_params->testbuf[j]->data_len);
	}
}

static void
destroy_sa(uint32_t j)
{
	struct ipsec_unitest_params *ut = &unittest_params;

	rte_ipsec_sa_fini(ut->ss[j].sa);
	rte_free(ut->ss[j].sa);
	rte_cryptodev_sym_session_free(ut->ss[j].crypto.ses);
	memset(&ut->ss[j], 0, sizeof(ut->ss[j]));
}

static int
crypto_inb_burst_null_null_check(struct ipsec_unitest_params *ut_params, int i,
		uint16_t num_pkts)
{
	uint16_t j;

	for (j = 0; j < num_pkts && num_pkts <= BURST_SIZE; j++) {
		ut_params->pkt_index = j;

		/* compare the data buffers */
		TEST_ASSERT_BUFFERS_ARE_EQUAL(null_plain_data,
			rte_pktmbuf_mtod(ut_params->obuf[j], void *),
			test_cfg[i].pkt_sz,
			"input and output data does not match\n");
		TEST_ASSERT_EQUAL(ut_params->obuf[j]->data_len,
			ut_params->obuf[j]->pkt_len,
			"data_len is not equal to pkt_len");
		TEST_ASSERT_EQUAL(ut_params->obuf[j]->data_len,
			test_cfg[i].pkt_sz,
			"data_len is not equal to input data");
	}

	return 0;
}

static int
test_ipsec_crypto_inb_burst_null_null(int i)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	uint16_t num_pkts = test_cfg[i].num_pkts;
	uint16_t j;
	int rc;

	/* create rte_ipsec_sa */
	rc = create_sa(RTE_SECURITY_ACTION_TYPE_NONE,
			test_cfg[i].replay_win_sz, test_cfg[i].flags, 0);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "rte_ipsec_sa_init failed, cfg %d\n",
			i);
		return TEST_FAILED;
	}

	/* Generate test mbuf data */
	for (j = 0; j < num_pkts && rc == 0; j++) {
		/* packet with sequence number 0 is invalid */
		ut_params->ibuf[j] = setup_test_string_tunneled(
			ts_params->mbuf_pool, null_encrypted_data,
			test_cfg[i].pkt_sz, INBOUND_SPI, j + 1);
		if (ut_params->ibuf[j] == NULL)
			rc = TEST_FAILED;
	}

	if (rc == 0) {
		if (test_cfg[i].reorder_pkts)
			test_ipsec_reorder_inb_pkt_burst(num_pkts);
		rc = test_ipsec_crypto_op_alloc(num_pkts);
	}

	if (rc == 0) {
		/* call ipsec library api */
		rc = crypto_ipsec(num_pkts);
		if (rc == 0)
			rc = crypto_inb_burst_null_null_check(
					ut_params, i, num_pkts);
		else {
			RTE_LOG(ERR, USER1, "crypto_ipsec failed, cfg %d\n",
				i);
			rc = TEST_FAILED;
		}
	}

	if (rc == TEST_FAILED)
		test_ipsec_dump_buffers(ut_params, i);

	destroy_sa(0);
	return rc;
}

static int
test_ipsec_crypto_inb_burst_null_null_wrapper(void)
{
	int i;
	int rc = 0;
	struct ipsec_unitest_params *ut_params = &unittest_params;

	ut_params->ipsec_xform.spi = INBOUND_SPI;
	ut_params->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;
	ut_params->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	ut_params->ipsec_xform.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	ut_params->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;

	for (i = 0; i < num_cfg && rc == 0; i++) {
		ut_params->ipsec_xform.options.esn = test_cfg[i].esn;
		rc = test_ipsec_crypto_inb_burst_null_null(i);
	}

	return rc;
}

static int
crypto_outb_burst_null_null_check(struct ipsec_unitest_params *ut_params,
	uint16_t num_pkts)
{
	void *obuf_data;
	void *testbuf_data;
	uint16_t j;

	for (j = 0; j < num_pkts && num_pkts <= BURST_SIZE; j++) {
		ut_params->pkt_index = j;

		testbuf_data = rte_pktmbuf_mtod(ut_params->testbuf[j], void *);
		obuf_data = rte_pktmbuf_mtod(ut_params->obuf[j], void *);
		/* compare the buffer data */
		TEST_ASSERT_BUFFERS_ARE_EQUAL(testbuf_data, obuf_data,
			ut_params->obuf[j]->pkt_len,
			"test and output data does not match\n");
		TEST_ASSERT_EQUAL(ut_params->obuf[j]->data_len,
			ut_params->testbuf[j]->data_len,
			"obuf data_len is not equal to testbuf data_len");
		TEST_ASSERT_EQUAL(ut_params->obuf[j]->pkt_len,
			ut_params->testbuf[j]->pkt_len,
			"obuf pkt_len is not equal to testbuf pkt_len");
	}

	return 0;
}

static int
test_ipsec_crypto_outb_burst_null_null(int i)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	uint16_t num_pkts = test_cfg[i].num_pkts;
	uint16_t j;
	int32_t rc;

	/* create rte_ipsec_sa*/
	rc = create_sa(RTE_SECURITY_ACTION_TYPE_NONE,
			test_cfg[i].replay_win_sz, test_cfg[i].flags, 0);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "rte_ipsec_sa_init failed, cfg %d\n",
			i);
		return TEST_FAILED;
	}

	/* Generate input mbuf data */
	for (j = 0; j < num_pkts && rc == 0; j++) {
		ut_params->ibuf[j] = setup_test_string(ts_params->mbuf_pool,
			null_plain_data, test_cfg[i].pkt_sz, 0);
		if (ut_params->ibuf[j] == NULL)
			rc = TEST_FAILED;
		else {
			/* Generate test mbuf data */
			/* packet with sequence number 0 is invalid */
			ut_params->testbuf[j] = setup_test_string_tunneled(
					ts_params->mbuf_pool,
					null_plain_data, test_cfg[i].pkt_sz,
					OUTBOUND_SPI, j + 1);
			if (ut_params->testbuf[j] == NULL)
				rc = TEST_FAILED;
		}
	}

	if (rc == 0)
		rc = test_ipsec_crypto_op_alloc(num_pkts);

	if (rc == 0) {
		/* call ipsec library api */
		rc = crypto_ipsec(num_pkts);
		if (rc == 0)
			rc = crypto_outb_burst_null_null_check(ut_params,
					num_pkts);
		else
			RTE_LOG(ERR, USER1, "crypto_ipsec failed, cfg %d\n",
				i);
	}

	if (rc == TEST_FAILED)
		test_ipsec_dump_buffers(ut_params, i);

	destroy_sa(0);
	return rc;
}

static int
test_ipsec_crypto_outb_burst_null_null_wrapper(void)
{
	int i;
	int rc = 0;
	struct ipsec_unitest_params *ut_params = &unittest_params;

	ut_params->ipsec_xform.spi = OUTBOUND_SPI;
	ut_params->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS;
	ut_params->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	ut_params->ipsec_xform.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	ut_params->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;

	for (i = 0; i < num_cfg && rc == 0; i++) {
		ut_params->ipsec_xform.options.esn = test_cfg[i].esn;
		rc = test_ipsec_crypto_outb_burst_null_null(i);
	}

	return rc;
}

static int
inline_inb_burst_null_null_check(struct ipsec_unitest_params *ut_params, int i,
	uint16_t num_pkts)
{
	void *ibuf_data;
	void *obuf_data;
	uint16_t j;

	for (j = 0; j < num_pkts && num_pkts <= BURST_SIZE; j++) {
		ut_params->pkt_index = j;

		/* compare the buffer data */
		ibuf_data = rte_pktmbuf_mtod(ut_params->ibuf[j], void *);
		obuf_data = rte_pktmbuf_mtod(ut_params->obuf[j], void *);

		TEST_ASSERT_BUFFERS_ARE_EQUAL(ibuf_data, obuf_data,
			ut_params->ibuf[j]->data_len,
			"input and output data does not match\n");
		TEST_ASSERT_EQUAL(ut_params->ibuf[j]->data_len,
			ut_params->obuf[j]->data_len,
			"ibuf data_len is not equal to obuf data_len");
		TEST_ASSERT_EQUAL(ut_params->ibuf[j]->pkt_len,
			ut_params->obuf[j]->pkt_len,
			"ibuf pkt_len is not equal to obuf pkt_len");
		TEST_ASSERT_EQUAL(ut_params->ibuf[j]->data_len,
			test_cfg[i].pkt_sz,
			"data_len is not equal input data");
	}
	return 0;
}

static int
test_ipsec_inline_crypto_inb_burst_null_null(int i)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	uint16_t num_pkts = test_cfg[i].num_pkts;
	uint16_t j;
	int32_t rc;
	uint32_t n;

	/* create rte_ipsec_sa*/
	rc = create_sa(RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
			test_cfg[i].replay_win_sz, test_cfg[i].flags, 0);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "rte_ipsec_sa_init failed, cfg %d\n",
			i);
		return TEST_FAILED;
	}

	/* Generate inbound mbuf data */
	for (j = 0; j < num_pkts && rc == 0; j++) {
		ut_params->ibuf[j] = setup_test_string_tunneled(
			ts_params->mbuf_pool,
			null_plain_data, test_cfg[i].pkt_sz,
			INBOUND_SPI, j + 1);
		if (ut_params->ibuf[j] == NULL)
			rc = TEST_FAILED;
		else {
			/* Generate test mbuf data */
			ut_params->obuf[j] = setup_test_string(
				ts_params->mbuf_pool,
				null_plain_data, test_cfg[i].pkt_sz, 0);
			if (ut_params->obuf[j] == NULL)
				rc = TEST_FAILED;
		}
	}

	if (rc == 0) {
		n = rte_ipsec_pkt_process(&ut_params->ss[0], ut_params->ibuf,
				num_pkts);
		if (n == num_pkts)
			rc = inline_inb_burst_null_null_check(ut_params, i,
					num_pkts);
		else {
			RTE_LOG(ERR, USER1,
				"rte_ipsec_pkt_process failed, cfg %d\n",
				i);
			rc = TEST_FAILED;
		}
	}

	if (rc == TEST_FAILED)
		test_ipsec_dump_buffers(ut_params, i);

	destroy_sa(0);
	return rc;
}

static int
test_ipsec_inline_crypto_inb_burst_null_null_wrapper(void)
{
	int i;
	int rc = 0;
	struct ipsec_unitest_params *ut_params = &unittest_params;

	ut_params->ipsec_xform.spi = INBOUND_SPI;
	ut_params->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;
	ut_params->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	ut_params->ipsec_xform.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	ut_params->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;

	for (i = 0; i < num_cfg && rc == 0; i++) {
		ut_params->ipsec_xform.options.esn = test_cfg[i].esn;
		rc = test_ipsec_inline_crypto_inb_burst_null_null(i);
	}

	return rc;
}

static int
test_ipsec_inline_proto_inb_burst_null_null(int i)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	uint16_t num_pkts = test_cfg[i].num_pkts;
	uint16_t j;
	int32_t rc;
	uint32_t n;

	/* create rte_ipsec_sa*/
	rc = create_sa(RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
			test_cfg[i].replay_win_sz, test_cfg[i].flags, 0);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "rte_ipsec_sa_init failed, cfg %d\n",
			i);
		return TEST_FAILED;
	}

	/* Generate inbound mbuf data */
	for (j = 0; j < num_pkts && rc == 0; j++) {
		ut_params->ibuf[j] = setup_test_string(
			ts_params->mbuf_pool,
			null_plain_data, test_cfg[i].pkt_sz, 0);
		if (ut_params->ibuf[j] == NULL)
			rc = TEST_FAILED;
		else {
			/* Generate test mbuf data */
			ut_params->obuf[j] = setup_test_string(
				ts_params->mbuf_pool,
				null_plain_data, test_cfg[i].pkt_sz, 0);
			if (ut_params->obuf[j] == NULL)
				rc = TEST_FAILED;
		}
	}

	if (rc == 0) {
		n = rte_ipsec_pkt_process(&ut_params->ss[0], ut_params->ibuf,
				num_pkts);
		if (n == num_pkts)
			rc = inline_inb_burst_null_null_check(ut_params, i,
					num_pkts);
		else {
			RTE_LOG(ERR, USER1,
				"rte_ipsec_pkt_process failed, cfg %d\n",
				i);
			rc = TEST_FAILED;
		}
	}

	if (rc == TEST_FAILED)
		test_ipsec_dump_buffers(ut_params, i);

	destroy_sa(0);
	return rc;
}

static int
test_ipsec_inline_proto_inb_burst_null_null_wrapper(void)
{
	int i;
	int rc = 0;
	struct ipsec_unitest_params *ut_params = &unittest_params;

	ut_params->ipsec_xform.spi = INBOUND_SPI;
	ut_params->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;
	ut_params->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	ut_params->ipsec_xform.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	ut_params->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;

	for (i = 0; i < num_cfg && rc == 0; i++) {
		ut_params->ipsec_xform.options.esn = test_cfg[i].esn;
		rc = test_ipsec_inline_proto_inb_burst_null_null(i);
	}

	return rc;
}

static int
inline_outb_burst_null_null_check(struct ipsec_unitest_params *ut_params,
	uint16_t num_pkts)
{
	void *obuf_data;
	void *ibuf_data;
	uint16_t j;

	for (j = 0; j < num_pkts && num_pkts <= BURST_SIZE; j++) {
		ut_params->pkt_index = j;

		/* compare the buffer data */
		ibuf_data = rte_pktmbuf_mtod(ut_params->ibuf[j], void *);
		obuf_data = rte_pktmbuf_mtod(ut_params->obuf[j], void *);
		TEST_ASSERT_BUFFERS_ARE_EQUAL(ibuf_data, obuf_data,
			ut_params->ibuf[j]->data_len,
			"input and output data does not match\n");
		TEST_ASSERT_EQUAL(ut_params->ibuf[j]->data_len,
			ut_params->obuf[j]->data_len,
			"ibuf data_len is not equal to obuf data_len");
		TEST_ASSERT_EQUAL(ut_params->ibuf[j]->pkt_len,
			ut_params->obuf[j]->pkt_len,
			"ibuf pkt_len is not equal to obuf pkt_len");

		/* check mbuf ol_flags */
		TEST_ASSERT(ut_params->ibuf[j]->ol_flags & PKT_TX_SEC_OFFLOAD,
			"ibuf PKT_TX_SEC_OFFLOAD is not set");
	}
	return 0;
}

static int
test_ipsec_inline_crypto_outb_burst_null_null(int i)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	uint16_t num_pkts = test_cfg[i].num_pkts;
	uint16_t j;
	int32_t rc;
	uint32_t n;

	/* create rte_ipsec_sa */
	rc = create_sa(RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
			test_cfg[i].replay_win_sz, test_cfg[i].flags, 0);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "rte_ipsec_sa_init failed, cfg %d\n",
			i);
		return TEST_FAILED;
	}

	/* Generate test mbuf data */
	for (j = 0; j < num_pkts && rc == 0; j++) {
		ut_params->ibuf[j] = setup_test_string(ts_params->mbuf_pool,
			null_plain_data, test_cfg[i].pkt_sz, 0);
		if (ut_params->ibuf[0] == NULL)
			rc = TEST_FAILED;

		if (rc == 0) {
			/* Generate test tunneled mbuf data for comparison */
			ut_params->obuf[j] = setup_test_string_tunneled(
					ts_params->mbuf_pool,
					null_plain_data, test_cfg[i].pkt_sz,
					OUTBOUND_SPI, j + 1);
			if (ut_params->obuf[j] == NULL)
				rc = TEST_FAILED;
		}
	}

	if (rc == 0) {
		n = rte_ipsec_pkt_process(&ut_params->ss[0], ut_params->ibuf,
				num_pkts);
		if (n == num_pkts)
			rc = inline_outb_burst_null_null_check(ut_params,
					num_pkts);
		else {
			RTE_LOG(ERR, USER1,
				"rte_ipsec_pkt_process failed, cfg %d\n",
				i);
			rc = TEST_FAILED;
		}
	}

	if (rc == TEST_FAILED)
		test_ipsec_dump_buffers(ut_params, i);

	destroy_sa(0);
	return rc;
}

static int
test_ipsec_inline_crypto_outb_burst_null_null_wrapper(void)
{
	int i;
	int rc = 0;
	struct ipsec_unitest_params *ut_params = &unittest_params;

	ut_params->ipsec_xform.spi = OUTBOUND_SPI;
	ut_params->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS;
	ut_params->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	ut_params->ipsec_xform.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	ut_params->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;

	for (i = 0; i < num_cfg && rc == 0; i++) {
		ut_params->ipsec_xform.options.esn = test_cfg[i].esn;
		rc = test_ipsec_inline_crypto_outb_burst_null_null(i);
	}

	return rc;
}

static int
test_ipsec_inline_proto_outb_burst_null_null(int i)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	uint16_t num_pkts = test_cfg[i].num_pkts;
	uint16_t j;
	int32_t rc;
	uint32_t n;

	/* create rte_ipsec_sa */
	rc = create_sa(RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
			test_cfg[i].replay_win_sz, test_cfg[i].flags, 0);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "rte_ipsec_sa_init failed, cfg %d\n",
			i);
		return TEST_FAILED;
	}

	/* Generate test mbuf data */
	for (j = 0; j < num_pkts && rc == 0; j++) {
		ut_params->ibuf[j] = setup_test_string(ts_params->mbuf_pool,
			null_plain_data, test_cfg[i].pkt_sz, 0);
		if (ut_params->ibuf[0] == NULL)
			rc = TEST_FAILED;

		if (rc == 0) {
			/* Generate test tunneled mbuf data for comparison */
			ut_params->obuf[j] = setup_test_string(
					ts_params->mbuf_pool,
					null_plain_data, test_cfg[i].pkt_sz, 0);
			if (ut_params->obuf[j] == NULL)
				rc = TEST_FAILED;
		}
	}

	if (rc == 0) {
		n = rte_ipsec_pkt_process(&ut_params->ss[0], ut_params->ibuf,
				num_pkts);
		if (n == num_pkts)
			rc = inline_outb_burst_null_null_check(ut_params,
					num_pkts);
		else {
			RTE_LOG(ERR, USER1,
				"rte_ipsec_pkt_process failed, cfg %d\n",
				i);
			rc = TEST_FAILED;
		}
	}

	if (rc == TEST_FAILED)
		test_ipsec_dump_buffers(ut_params, i);

	destroy_sa(0);
	return rc;
}

static int
test_ipsec_inline_proto_outb_burst_null_null_wrapper(void)
{
	int i;
	int rc = 0;
	struct ipsec_unitest_params *ut_params = &unittest_params;

	ut_params->ipsec_xform.spi = OUTBOUND_SPI;
	ut_params->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS;
	ut_params->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	ut_params->ipsec_xform.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	ut_params->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;

	for (i = 0; i < num_cfg && rc == 0; i++) {
		ut_params->ipsec_xform.options.esn = test_cfg[i].esn;
		rc = test_ipsec_inline_proto_outb_burst_null_null(i);
	}

	return rc;
}

static int
test_ipsec_lksd_proto_inb_burst_null_null(int i)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	uint16_t num_pkts = test_cfg[i].num_pkts;
	uint16_t j;
	int rc;

	/* create rte_ipsec_sa */
	rc = create_sa(RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
			test_cfg[i].replay_win_sz, test_cfg[i].flags, 0);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "rte_ipsec_sa_init failed, cfg %d\n",
			i);
		return TEST_FAILED;
	}

	/* Generate test mbuf data */
	for (j = 0; j < num_pkts && rc == 0; j++) {
		/* packet with sequence number 0 is invalid */
		ut_params->ibuf[j] = setup_test_string(ts_params->mbuf_pool,
			null_encrypted_data, test_cfg[i].pkt_sz, 0);
		if (ut_params->ibuf[j] == NULL)
			rc = TEST_FAILED;
	}

	if (rc == 0) {
		if (test_cfg[i].reorder_pkts)
			test_ipsec_reorder_inb_pkt_burst(num_pkts);
		rc = test_ipsec_crypto_op_alloc(num_pkts);
	}

	if (rc == 0) {
		/* call ipsec library api */
		rc = lksd_proto_ipsec(num_pkts);
		if (rc == 0)
			rc = crypto_inb_burst_null_null_check(ut_params, i,
					num_pkts);
		else {
			RTE_LOG(ERR, USER1, "%s failed, cfg %d\n",
				__func__, i);
			rc = TEST_FAILED;
		}
	}

	if (rc == TEST_FAILED)
		test_ipsec_dump_buffers(ut_params, i);

	destroy_sa(0);
	return rc;
}

static int
test_ipsec_lksd_proto_inb_burst_null_null_wrapper(void)
{
	int i;
	int rc = 0;
	struct ipsec_unitest_params *ut_params = &unittest_params;

	ut_params->ipsec_xform.spi = INBOUND_SPI;
	ut_params->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;
	ut_params->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	ut_params->ipsec_xform.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	ut_params->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;

	for (i = 0; i < num_cfg && rc == 0; i++) {
		ut_params->ipsec_xform.options.esn = test_cfg[i].esn;
		rc = test_ipsec_lksd_proto_inb_burst_null_null(i);
	}

	return rc;
}

static int
test_ipsec_lksd_proto_outb_burst_null_null_wrapper(void)
{
	int i;
	int rc = 0;
	struct ipsec_unitest_params *ut_params = &unittest_params;

	ut_params->ipsec_xform.spi = INBOUND_SPI;
	ut_params->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS;
	ut_params->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	ut_params->ipsec_xform.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	ut_params->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;

	for (i = 0; i < num_cfg && rc == 0; i++) {
		ut_params->ipsec_xform.options.esn = test_cfg[i].esn;
		rc = test_ipsec_lksd_proto_inb_burst_null_null(i);
	}

	return rc;
}

static int
replay_inb_null_null_check(struct ipsec_unitest_params *ut_params, int i,
	int num_pkts)
{
	uint16_t j;

	for (j = 0; j < num_pkts; j++) {
		/* compare the buffer data */
		TEST_ASSERT_BUFFERS_ARE_EQUAL(null_plain_data,
			rte_pktmbuf_mtod(ut_params->obuf[j], void *),
			test_cfg[i].pkt_sz,
			"input and output data does not match\n");

		TEST_ASSERT_EQUAL(ut_params->obuf[j]->data_len,
			ut_params->obuf[j]->pkt_len,
			"data_len is not equal to pkt_len");
	}

	return 0;
}

static int
test_ipsec_replay_inb_inside_null_null(int i)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	int rc;

	/* create rte_ipsec_sa*/
	rc = create_sa(RTE_SECURITY_ACTION_TYPE_NONE,
			test_cfg[i].replay_win_sz, test_cfg[i].flags, 0);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "rte_ipsec_sa_init failed, cfg %d\n",
			i);
		return TEST_FAILED;
	}

	/* Generate inbound mbuf data */
	ut_params->ibuf[0] = setup_test_string_tunneled(ts_params->mbuf_pool,
		null_encrypted_data, test_cfg[i].pkt_sz, INBOUND_SPI, 1);
	if (ut_params->ibuf[0] == NULL)
		rc = TEST_FAILED;
	else
		rc = test_ipsec_crypto_op_alloc(1);

	if (rc == 0) {
		/* call ipsec library api */
		rc = crypto_ipsec(1);
		if (rc == 0)
			rc = replay_inb_null_null_check(ut_params, i, 1);
		else {
			RTE_LOG(ERR, USER1, "crypto_ipsec failed, cfg %d\n",
					i);
			rc = TEST_FAILED;
		}
	}

	if ((rc == 0) && (test_cfg[i].replay_win_sz != 0)) {
		/* generate packet with seq number inside the replay window */
		if (ut_params->ibuf[0]) {
			rte_pktmbuf_free(ut_params->ibuf[0]);
			ut_params->ibuf[0] = 0;
		}

		ut_params->ibuf[0] = setup_test_string_tunneled(
			ts_params->mbuf_pool, null_encrypted_data,
			test_cfg[i].pkt_sz, INBOUND_SPI,
			test_cfg[i].replay_win_sz);
		if (ut_params->ibuf[0] == NULL)
			rc = TEST_FAILED;
		else
			rc = test_ipsec_crypto_op_alloc(1);

		if (rc == 0) {
			/* call ipsec library api */
			rc = crypto_ipsec(1);
			if (rc == 0)
				rc = replay_inb_null_null_check(
						ut_params, i, 1);
			else {
				RTE_LOG(ERR, USER1, "crypto_ipsec failed\n");
				rc = TEST_FAILED;
			}
		}
	}

	if (rc == TEST_FAILED)
		test_ipsec_dump_buffers(ut_params, i);

	destroy_sa(0);

	return rc;
}

static int
test_ipsec_replay_inb_inside_null_null_wrapper(void)
{
	int i;
	int rc = 0;
	struct ipsec_unitest_params *ut_params = &unittest_params;

	ut_params->ipsec_xform.spi = INBOUND_SPI;
	ut_params->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;
	ut_params->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	ut_params->ipsec_xform.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	ut_params->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;

	for (i = 0; i < num_cfg && rc == 0; i++) {
		ut_params->ipsec_xform.options.esn = test_cfg[i].esn;
		rc = test_ipsec_replay_inb_inside_null_null(i);
	}

	return rc;
}

static int
test_ipsec_replay_inb_outside_null_null(int i)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	int rc;

	/* create rte_ipsec_sa */
	rc = create_sa(RTE_SECURITY_ACTION_TYPE_NONE,
			test_cfg[i].replay_win_sz, test_cfg[i].flags, 0);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "rte_ipsec_sa_init failed, cfg %d\n",
			i);
		return TEST_FAILED;
	}

	/* Generate test mbuf data */
	ut_params->ibuf[0] = setup_test_string_tunneled(ts_params->mbuf_pool,
		null_encrypted_data, test_cfg[i].pkt_sz, INBOUND_SPI,
		test_cfg[i].replay_win_sz + 2);
	if (ut_params->ibuf[0] == NULL)
		rc = TEST_FAILED;
	else
		rc = test_ipsec_crypto_op_alloc(1);

	if (rc == 0) {
		/* call ipsec library api */
		rc = crypto_ipsec(1);
		if (rc == 0)
			rc = replay_inb_null_null_check(ut_params, i, 1);
		else {
			RTE_LOG(ERR, USER1, "crypto_ipsec failed, cfg %d\n",
					i);
			rc = TEST_FAILED;
		}
	}

	if ((rc == 0) && (test_cfg[i].replay_win_sz != 0)) {
		/* generate packet with seq number outside the replay window */
		if (ut_params->ibuf[0]) {
			rte_pktmbuf_free(ut_params->ibuf[0]);
			ut_params->ibuf[0] = 0;
		}
		ut_params->ibuf[0] = setup_test_string_tunneled(
			ts_params->mbuf_pool, null_encrypted_data,
			test_cfg[i].pkt_sz, INBOUND_SPI, 1);
		if (ut_params->ibuf[0] == NULL)
			rc = TEST_FAILED;
		else
			rc = test_ipsec_crypto_op_alloc(1);

		if (rc == 0) {
			/* call ipsec library api */
			rc = crypto_ipsec(1);
			if (rc == 0) {
				if (test_cfg[i].esn == 0) {
					RTE_LOG(ERR, USER1,
						"packet is not outside the replay window, cfg %d pkt0_seq %u pkt1_seq %u\n",
						i,
						test_cfg[i].replay_win_sz + 2,
						1);
					rc = TEST_FAILED;
				}
			} else {
				RTE_LOG(ERR, USER1,
					"packet is outside the replay window, cfg %d pkt0_seq %u pkt1_seq %u\n",
					i, test_cfg[i].replay_win_sz + 2, 1);
				rc = 0;
			}
		}
	}

	if (rc == TEST_FAILED)
		test_ipsec_dump_buffers(ut_params, i);

	destroy_sa(0);

	return rc;
}

static int
test_ipsec_replay_inb_outside_null_null_wrapper(void)
{
	int i;
	int rc = 0;
	struct ipsec_unitest_params *ut_params = &unittest_params;

	ut_params->ipsec_xform.spi = INBOUND_SPI;
	ut_params->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;
	ut_params->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	ut_params->ipsec_xform.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	ut_params->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;

	for (i = 0; i < num_cfg && rc == 0; i++) {
		ut_params->ipsec_xform.options.esn = test_cfg[i].esn;
		rc = test_ipsec_replay_inb_outside_null_null(i);
	}

	return rc;
}

static int
test_ipsec_replay_inb_repeat_null_null(int i)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	int rc;

	/* create rte_ipsec_sa */
	rc = create_sa(RTE_SECURITY_ACTION_TYPE_NONE,
			test_cfg[i].replay_win_sz, test_cfg[i].flags, 0);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "rte_ipsec_sa_init failed, cfg %d\n", i);
		return TEST_FAILED;
	}

	/* Generate test mbuf data */
	ut_params->ibuf[0] = setup_test_string_tunneled(ts_params->mbuf_pool,
		null_encrypted_data, test_cfg[i].pkt_sz, INBOUND_SPI, 1);
	if (ut_params->ibuf[0] == NULL)
		rc = TEST_FAILED;
	else
		rc = test_ipsec_crypto_op_alloc(1);

	if (rc == 0) {
		/* call ipsec library api */
		rc = crypto_ipsec(1);
		if (rc == 0)
			rc = replay_inb_null_null_check(ut_params, i, 1);
		else {
			RTE_LOG(ERR, USER1, "crypto_ipsec failed, cfg %d\n",
					i);
			rc = TEST_FAILED;
		}
	}

	if ((rc == 0) && (test_cfg[i].replay_win_sz != 0)) {
		/*
		 * generate packet with repeat seq number in the replay
		 * window
		 */
		if (ut_params->ibuf[0]) {
			rte_pktmbuf_free(ut_params->ibuf[0]);
			ut_params->ibuf[0] = 0;
		}

		ut_params->ibuf[0] = setup_test_string_tunneled(
			ts_params->mbuf_pool, null_encrypted_data,
			test_cfg[i].pkt_sz, INBOUND_SPI, 1);
		if (ut_params->ibuf[0] == NULL)
			rc = TEST_FAILED;
		else
			rc = test_ipsec_crypto_op_alloc(1);

		if (rc == 0) {
			/* call ipsec library api */
			rc = crypto_ipsec(1);
			if (rc == 0) {
				RTE_LOG(ERR, USER1,
					"packet is not repeated in the replay window, cfg %d seq %u\n",
					i, 1);
				rc = TEST_FAILED;
			} else {
				RTE_LOG(ERR, USER1,
					"packet is repeated in the replay window, cfg %d seq %u\n",
					i, 1);
				rc = 0;
			}
		}
	}

	if (rc == TEST_FAILED)
		test_ipsec_dump_buffers(ut_params, i);

	destroy_sa(0);

	return rc;
}

static int
test_ipsec_replay_inb_repeat_null_null_wrapper(void)
{
	int i;
	int rc = 0;
	struct ipsec_unitest_params *ut_params = &unittest_params;

	ut_params->ipsec_xform.spi = INBOUND_SPI;
	ut_params->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;
	ut_params->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	ut_params->ipsec_xform.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	ut_params->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;

	for (i = 0; i < num_cfg && rc == 0; i++) {
		ut_params->ipsec_xform.options.esn = test_cfg[i].esn;
		rc = test_ipsec_replay_inb_repeat_null_null(i);
	}

	return rc;
}

static int
test_ipsec_replay_inb_inside_burst_null_null(int i)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	uint16_t num_pkts = test_cfg[i].num_pkts;
	int rc;
	int j;

	/* create rte_ipsec_sa*/
	rc = create_sa(RTE_SECURITY_ACTION_TYPE_NONE,
			test_cfg[i].replay_win_sz, test_cfg[i].flags, 0);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "rte_ipsec_sa_init failed, cfg %d\n",
			i);
		return TEST_FAILED;
	}

	/* Generate inbound mbuf data */
	ut_params->ibuf[0] = setup_test_string_tunneled(ts_params->mbuf_pool,
		null_encrypted_data, test_cfg[i].pkt_sz, INBOUND_SPI, 1);
	if (ut_params->ibuf[0] == NULL)
		rc = TEST_FAILED;
	else
		rc = test_ipsec_crypto_op_alloc(1);

	if (rc == 0) {
		/* call ipsec library api */
		rc = crypto_ipsec(1);
		if (rc == 0)
			rc = replay_inb_null_null_check(ut_params, i, 1);
		else {
			RTE_LOG(ERR, USER1, "crypto_ipsec failed, cfg %d\n",
					i);
			rc = TEST_FAILED;
		}
	}

	if ((rc == 0) && (test_cfg[i].replay_win_sz != 0)) {
		/*
		 *  generate packet(s) with seq number(s) inside the
		 *  replay window
		 */
		if (ut_params->ibuf[0]) {
			rte_pktmbuf_free(ut_params->ibuf[0]);
			ut_params->ibuf[0] = 0;
		}

		for (j = 0; j < num_pkts && rc == 0; j++) {
			/* packet with sequence number 1 already processed */
			ut_params->ibuf[j] = setup_test_string_tunneled(
				ts_params->mbuf_pool, null_encrypted_data,
				test_cfg[i].pkt_sz, INBOUND_SPI, j + 2);
			if (ut_params->ibuf[j] == NULL)
				rc = TEST_FAILED;
		}

		if (rc == 0) {
			if (test_cfg[i].reorder_pkts)
				test_ipsec_reorder_inb_pkt_burst(num_pkts);
			rc = test_ipsec_crypto_op_alloc(num_pkts);
		}

		if (rc == 0) {
			/* call ipsec library api */
			rc = crypto_ipsec(num_pkts);
			if (rc == 0)
				rc = replay_inb_null_null_check(
						ut_params, i, num_pkts);
			else {
				RTE_LOG(ERR, USER1, "crypto_ipsec failed\n");
				rc = TEST_FAILED;
			}
		}
	}

	if (rc == TEST_FAILED)
		test_ipsec_dump_buffers(ut_params, i);

	destroy_sa(0);

	return rc;
}

static int
test_ipsec_replay_inb_inside_burst_null_null_wrapper(void)
{
	int i;
	int rc = 0;
	struct ipsec_unitest_params *ut_params = &unittest_params;

	ut_params->ipsec_xform.spi = INBOUND_SPI;
	ut_params->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;
	ut_params->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	ut_params->ipsec_xform.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	ut_params->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;

	for (i = 0; i < num_cfg && rc == 0; i++) {
		ut_params->ipsec_xform.options.esn = test_cfg[i].esn;
		rc = test_ipsec_replay_inb_inside_burst_null_null(i);
	}

	return rc;
}


static int
crypto_inb_burst_2sa_null_null_check(struct ipsec_unitest_params *ut_params,
		int i)
{
	uint16_t j;

	for (j = 0; j < BURST_SIZE; j++) {
		ut_params->pkt_index = j;

		/* compare the data buffers */
		TEST_ASSERT_BUFFERS_ARE_EQUAL(null_plain_data,
			rte_pktmbuf_mtod(ut_params->obuf[j], void *),
			test_cfg[i].pkt_sz,
			"input and output data does not match\n");
		TEST_ASSERT_EQUAL(ut_params->obuf[j]->data_len,
			ut_params->obuf[j]->pkt_len,
			"data_len is not equal to pkt_len");
		TEST_ASSERT_EQUAL(ut_params->obuf[j]->data_len,
			test_cfg[i].pkt_sz,
			"data_len is not equal to input data");
	}

	return 0;
}

static int
test_ipsec_crypto_inb_burst_2sa_null_null(int i)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	uint16_t num_pkts = test_cfg[i].num_pkts;
	uint16_t j, r;
	int rc = 0;

	if (num_pkts != BURST_SIZE)
		return rc;

	/* create rte_ipsec_sa */
	rc = create_sa(RTE_SECURITY_ACTION_TYPE_NONE,
			test_cfg[i].replay_win_sz, test_cfg[i].flags, 0);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "rte_ipsec_sa_init failed, cfg %d\n",
			i);
		return TEST_FAILED;
	}

	/* create second rte_ipsec_sa */
	ut_params->ipsec_xform.spi = INBOUND_SPI + 1;
	rc = create_sa(RTE_SECURITY_ACTION_TYPE_NONE,
			test_cfg[i].replay_win_sz, test_cfg[i].flags, 1);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "rte_ipsec_sa_init failed, cfg %d\n",
			i);
		destroy_sa(0);
		return TEST_FAILED;
	}

	/* Generate test mbuf data */
	for (j = 0; j < num_pkts && rc == 0; j++) {
		r = j % 2;
		/* packet with sequence number 0 is invalid */
		ut_params->ibuf[j] = setup_test_string_tunneled(
			ts_params->mbuf_pool, null_encrypted_data,
			test_cfg[i].pkt_sz, INBOUND_SPI + r, j + 1);
		if (ut_params->ibuf[j] == NULL)
			rc = TEST_FAILED;
	}

	if (rc == 0)
		rc = test_ipsec_crypto_op_alloc(num_pkts);

	if (rc == 0) {
		/* call ipsec library api */
		rc = crypto_ipsec_2sa();
		if (rc == 0)
			rc = crypto_inb_burst_2sa_null_null_check(
					ut_params, i);
		else {
			RTE_LOG(ERR, USER1, "crypto_ipsec failed, cfg %d\n",
				i);
			rc = TEST_FAILED;
		}
	}

	if (rc == TEST_FAILED)
		test_ipsec_dump_buffers(ut_params, i);

	destroy_sa(0);
	destroy_sa(1);
	return rc;
}

static int
test_ipsec_crypto_inb_burst_2sa_null_null_wrapper(void)
{
	int i;
	int rc = 0;
	struct ipsec_unitest_params *ut_params = &unittest_params;

	ut_params->ipsec_xform.spi = INBOUND_SPI;
	ut_params->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;
	ut_params->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	ut_params->ipsec_xform.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	ut_params->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;

	for (i = 0; i < num_cfg && rc == 0; i++) {
		ut_params->ipsec_xform.options.esn = test_cfg[i].esn;
		rc = test_ipsec_crypto_inb_burst_2sa_null_null(i);
	}

	return rc;
}

static int
test_ipsec_crypto_inb_burst_2sa_4grp_null_null(int i)
{
	struct ipsec_testsuite_params *ts_params = &testsuite_params;
	struct ipsec_unitest_params *ut_params = &unittest_params;
	uint16_t num_pkts = test_cfg[i].num_pkts;
	uint16_t j, k;
	int rc = 0;

	if (num_pkts != BURST_SIZE)
		return rc;

	/* create rte_ipsec_sa */
	rc = create_sa(RTE_SECURITY_ACTION_TYPE_NONE,
			test_cfg[i].replay_win_sz, test_cfg[i].flags, 0);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "rte_ipsec_sa_init failed, cfg %d\n",
			i);
		return TEST_FAILED;
	}

	/* create second rte_ipsec_sa */
	ut_params->ipsec_xform.spi = INBOUND_SPI + 1;
	rc = create_sa(RTE_SECURITY_ACTION_TYPE_NONE,
			test_cfg[i].replay_win_sz, test_cfg[i].flags, 1);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "rte_ipsec_sa_init failed, cfg %d\n",
			i);
		destroy_sa(0);
		return TEST_FAILED;
	}

	/* Generate test mbuf data */
	for (j = 0; j < num_pkts && rc == 0; j++) {
		k = crypto_ipsec_4grp(j);

		/* packet with sequence number 0 is invalid */
		ut_params->ibuf[j] = setup_test_string_tunneled(
			ts_params->mbuf_pool, null_encrypted_data,
			test_cfg[i].pkt_sz, INBOUND_SPI + k, j + 1);
		if (ut_params->ibuf[j] == NULL)
			rc = TEST_FAILED;
	}

	if (rc == 0)
		rc = test_ipsec_crypto_op_alloc(num_pkts);

	if (rc == 0) {
		/* call ipsec library api */
		rc = crypto_ipsec_2sa_4grp();
		if (rc == 0)
			rc = crypto_inb_burst_2sa_null_null_check(
					ut_params, i);
		else {
			RTE_LOG(ERR, USER1, "crypto_ipsec failed, cfg %d\n",
				i);
			rc = TEST_FAILED;
		}
	}

	if (rc == TEST_FAILED)
		test_ipsec_dump_buffers(ut_params, i);

	destroy_sa(0);
	destroy_sa(1);
	return rc;
}

static int
test_ipsec_crypto_inb_burst_2sa_4grp_null_null_wrapper(void)
{
	int i;
	int rc = 0;
	struct ipsec_unitest_params *ut_params = &unittest_params;

	ut_params->ipsec_xform.spi = INBOUND_SPI;
	ut_params->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;
	ut_params->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	ut_params->ipsec_xform.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	ut_params->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;

	for (i = 0; i < num_cfg && rc == 0; i++) {
		ut_params->ipsec_xform.options.esn = test_cfg[i].esn;
		rc = test_ipsec_crypto_inb_burst_2sa_4grp_null_null(i);
	}

	return rc;
}

static struct unit_test_suite ipsec_testsuite  = {
	.suite_name = "IPsec NULL Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_ipsec_crypto_inb_burst_null_null_wrapper),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_ipsec_crypto_outb_burst_null_null_wrapper),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_ipsec_inline_crypto_inb_burst_null_null_wrapper),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_ipsec_inline_crypto_outb_burst_null_null_wrapper),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_ipsec_inline_proto_inb_burst_null_null_wrapper),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_ipsec_inline_proto_outb_burst_null_null_wrapper),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_ipsec_lksd_proto_inb_burst_null_null_wrapper),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_ipsec_lksd_proto_outb_burst_null_null_wrapper),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_ipsec_replay_inb_inside_null_null_wrapper),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_ipsec_replay_inb_outside_null_null_wrapper),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_ipsec_replay_inb_repeat_null_null_wrapper),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_ipsec_replay_inb_inside_burst_null_null_wrapper),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_ipsec_crypto_inb_burst_2sa_null_null_wrapper),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_ipsec_crypto_inb_burst_2sa_4grp_null_null_wrapper),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_ipsec(void)
{
	return unit_test_suite_runner(&ipsec_testsuite);
}

REGISTER_TEST_COMMAND(ipsec_autotest, test_ipsec);
