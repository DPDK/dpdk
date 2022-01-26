/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include "test.h"

#include <stdio.h>
#include <rte_ip.h>
#include <rte_malloc.h>
#include <rte_ring.h>
#include <rte_mbuf.h>
#include <rte_cycles.h>

#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_libipsec_perf(void)
{
	printf("ipsec_perf not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <rte_ipsec.h>
#include <rte_random.h>

#include "test_cryptodev.h"

#define RING_SIZE	4096
#define BURST_SIZE	64
#define NUM_MBUF	4095
#define DEFAULT_SPI     7

struct ipsec_test_cfg {
	uint32_t replay_win_sz;
	uint32_t esn;
	uint64_t flags;
	enum rte_crypto_sym_xform_type type;
};

struct rte_mempool *mbuf_pool, *cop_pool;

struct stats_counter {
	uint64_t nb_prepare_call;
	uint64_t nb_prepare_pkt;
	uint64_t nb_process_call;
	uint64_t nb_process_pkt;
	uint64_t prepare_ticks_elapsed;
	uint64_t process_ticks_elapsed;
};

struct ipsec_sa {
	struct rte_ipsec_session ss[2];
	struct rte_ipsec_sa_prm sa_prm;
	struct rte_security_ipsec_xform ipsec_xform;
	struct rte_crypto_sym_xform cipher_xform;
	struct rte_crypto_sym_xform auth_xform;
	struct rte_crypto_sym_xform aead_xform;
	struct rte_crypto_sym_xform *crypto_xforms;
	struct rte_crypto_op *cop[BURST_SIZE];
	enum rte_crypto_sym_xform_type type;
	struct stats_counter cnt;
	uint32_t replay_win_sz;
	uint32_t sa_flags;
};

static const struct ipsec_test_cfg test_cfg[] = {
	{0, 0, 0, RTE_CRYPTO_SYM_XFORM_AEAD},
	{0, 0, 0, RTE_CRYPTO_SYM_XFORM_CIPHER},
	{128, 1, 0, RTE_CRYPTO_SYM_XFORM_AEAD},
	{128, 1, 0, RTE_CRYPTO_SYM_XFORM_CIPHER},

};

static struct rte_ipv4_hdr ipv4_outer  = {
	.version_ihl = IPVERSION << 4 |
		sizeof(ipv4_outer) / RTE_IPV4_IHL_MULTIPLIER,
	.time_to_live = IPDEFTTL,
	.next_proto_id = IPPROTO_ESP,
	.src_addr = RTE_IPV4(192, 168, 1, 100),
	.dst_addr = RTE_IPV4(192, 168, 2, 100),
};

static struct rte_ring *ring_inb_prepare;
static struct rte_ring *ring_inb_process;
static struct rte_ring *ring_outb_prepare;
static struct rte_ring *ring_outb_process;

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

struct supported_aead_algo {
	const char *keyword;
	enum rte_crypto_aead_algorithm algo;
	uint16_t iv_len;
	uint16_t block_size;
	uint16_t digest_len;
	uint16_t key_len;
	uint8_t aad_len;
};

const struct supported_cipher_algo cipher_algo[] = {
	{
		.keyword = "aes-128-cbc",
		.algo = RTE_CRYPTO_CIPHER_AES_CBC,
		.iv_len = 16,
		.block_size = 16,
		.key_len = 16
	}
};

const struct supported_auth_algo auth_algo[] = {
	{
		.keyword = "sha1-hmac",
		.algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
		.digest_len = 12,
		.key_len = 20
	}
};

const struct supported_aead_algo aead_algo[] = {
	{
		.keyword = "aes-128-gcm",
		.algo = RTE_CRYPTO_AEAD_AES_GCM,
		.iv_len = 8,
		.block_size = 4,
		.key_len = 20,
		.digest_len = 16,
		.aad_len = 8,
	}
};

static struct rte_mbuf *generate_mbuf_data(struct rte_mempool *mpool)
{
	struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mpool);

	if (mbuf) {
		mbuf->data_len = 64;
		mbuf->pkt_len  = 64;
	}

	return mbuf;
}

static int
fill_ipsec_param(struct ipsec_sa *sa)
{
	struct rte_ipsec_sa_prm *prm = &sa->sa_prm;

	memset(prm, 0, sizeof(*prm));

	prm->flags = sa->sa_flags;

	/* setup ipsec xform */
	prm->ipsec_xform = sa->ipsec_xform;
	prm->ipsec_xform.salt = (uint32_t)rte_rand();
	prm->ipsec_xform.replay_win_sz = sa->replay_win_sz;

	/* setup tunnel related fields */
	prm->tun.hdr_len = sizeof(ipv4_outer);
	prm->tun.next_proto = IPPROTO_IPIP;
	prm->tun.hdr = &ipv4_outer;

	if (sa->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		sa->aead_xform.type = sa->type;
		sa->aead_xform.aead.algo = aead_algo->algo;
		sa->aead_xform.next = NULL;
		sa->aead_xform.aead.digest_length = aead_algo->digest_len;
		sa->aead_xform.aead.iv.offset = IV_OFFSET;
		sa->aead_xform.aead.iv.length = 12;

		if (sa->ipsec_xform.direction ==
				RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
			sa->aead_xform.aead.op = RTE_CRYPTO_AEAD_OP_DECRYPT;
		} else {
			sa->aead_xform.aead.op = RTE_CRYPTO_AEAD_OP_ENCRYPT;
		}

		sa->crypto_xforms = &sa->aead_xform;
	} else {
		sa->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
		sa->cipher_xform.cipher.algo = cipher_algo->algo;
		sa->cipher_xform.cipher.iv.offset = IV_OFFSET;
		sa->cipher_xform.cipher.iv.length = 12;
		sa->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
		sa->auth_xform.auth.algo = auth_algo->algo;
		sa->auth_xform.auth.digest_length = auth_algo->digest_len;


		if (sa->ipsec_xform.direction ==
				RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
			sa->cipher_xform.cipher.op =
				RTE_CRYPTO_CIPHER_OP_DECRYPT;
			sa->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
			sa->cipher_xform.next = NULL;
			sa->auth_xform.next = &sa->cipher_xform;
			sa->crypto_xforms = &sa->auth_xform;
		} else {
			sa->cipher_xform.cipher.op =
				RTE_CRYPTO_CIPHER_OP_ENCRYPT;
			sa->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
			sa->auth_xform.next = NULL;
			sa->cipher_xform.next = &sa->auth_xform;
			sa->crypto_xforms = &sa->cipher_xform;
		}
	}

	prm->crypto_xform = sa->crypto_xforms;

	return TEST_SUCCESS;
}

static int
create_sa(enum rte_security_session_action_type action_type,
	  struct ipsec_sa *sa)
{
	static struct rte_cryptodev_sym_session dummy_ses;
	size_t sz;
	int rc;

	memset(&sa->ss[0], 0, sizeof(sa->ss[0]));

	rc = fill_ipsec_param(sa);
	if (rc != 0) {
		printf("failed to fill ipsec param\n");
		return TEST_FAILED;
	}

	sz = rte_ipsec_sa_size(&sa->sa_prm);
	TEST_ASSERT(sz > 0, "rte_ipsec_sa_size() failed\n");

	sa->ss[0].sa = rte_zmalloc(NULL, sz, RTE_CACHE_LINE_SIZE);
	TEST_ASSERT_NOT_NULL(sa->ss[0].sa,
		"failed to allocate memory for rte_ipsec_sa\n");

	sa->ss[0].type = action_type;
	sa->ss[0].crypto.ses = &dummy_ses;

	rc = rte_ipsec_sa_init(sa->ss[0].sa, &sa->sa_prm, sz);
	rc = (rc > 0 && (uint32_t)rc <= sz) ? 0 : -EINVAL;

	if (rc == 0)
		rc = rte_ipsec_session_prepare(&sa->ss[0]);
	else
		return TEST_FAILED;

	return TEST_SUCCESS;
}

static int
packet_prepare(struct rte_mbuf **buf, struct ipsec_sa *sa,
	       uint16_t num_pkts)
{
	uint64_t time_stamp;
	uint16_t k = 0, i;

	for (i = 0; i < num_pkts; i++) {

		sa->cop[i] = rte_crypto_op_alloc(cop_pool,
				RTE_CRYPTO_OP_TYPE_SYMMETRIC);

		if (sa->cop[i] == NULL) {

			RTE_LOG(ERR, USER1,
			"Failed to allocate symmetric crypto op\n");

			return k;
		}
	}

	time_stamp = rte_rdtsc_precise();

	k = rte_ipsec_pkt_crypto_prepare(&sa->ss[0], buf,
		sa->cop, num_pkts);

	time_stamp = rte_rdtsc_precise() - time_stamp;

	if (k != num_pkts) {
		RTE_LOG(ERR, USER1, "rte_ipsec_pkt_crypto_prepare fail\n");
		return k;
	}

	sa->cnt.prepare_ticks_elapsed += time_stamp;
	sa->cnt.nb_prepare_call++;
	sa->cnt.nb_prepare_pkt += k;

	for (i = 0; i < num_pkts; i++)
		rte_crypto_op_free(sa->cop[i]);

	return k;
}

static int
packet_process(struct rte_mbuf **buf, struct ipsec_sa *sa,
	       uint16_t num_pkts)
{
	uint64_t time_stamp;
	uint16_t k = 0;

	time_stamp = rte_rdtsc_precise();

	k = rte_ipsec_pkt_process(&sa->ss[0], buf, num_pkts);

	time_stamp = rte_rdtsc_precise() - time_stamp;

	if (k != num_pkts) {
		RTE_LOG(ERR, USER1, "rte_ipsec_pkt_process fail\n");
		return k;
	}

	sa->cnt.process_ticks_elapsed += time_stamp;
	sa->cnt.nb_process_call++;
	sa->cnt.nb_process_pkt += k;

	return k;
}

static int
create_traffic(struct ipsec_sa *sa, struct rte_ring *deq_ring,
	       struct rte_ring *enq_ring, struct rte_ring *ring)
{
	struct rte_mbuf *mbuf[BURST_SIZE];
	uint16_t num_pkts, n;

	while (rte_ring_empty(deq_ring) == 0) {

		num_pkts = rte_ring_sc_dequeue_burst(deq_ring, (void **)mbuf,
						     RTE_DIM(mbuf), NULL);

		if (num_pkts == 0)
			return TEST_FAILED;

		n = packet_prepare(mbuf, sa, num_pkts);
		if (n != num_pkts)
			return TEST_FAILED;

		num_pkts = rte_ring_sp_enqueue_burst(enq_ring, (void **)mbuf,
						     num_pkts, NULL);
		if (num_pkts == 0)
			return TEST_FAILED;
	}

	deq_ring = enq_ring;
	enq_ring = ring;

	while (rte_ring_empty(deq_ring) == 0) {

		num_pkts = rte_ring_sc_dequeue_burst(deq_ring, (void **)mbuf,
					       RTE_DIM(mbuf), NULL);
		if (num_pkts == 0)
			return TEST_FAILED;

		n = packet_process(mbuf, sa, num_pkts);
		if (n != num_pkts)
			return TEST_FAILED;

		num_pkts = rte_ring_sp_enqueue_burst(enq_ring, (void **)mbuf,
					       num_pkts, NULL);
		if (num_pkts == 0)
			return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static void
fill_ipsec_sa_out(const struct ipsec_test_cfg *test_cfg,
		  struct ipsec_sa *sa)
{
	sa->ipsec_xform.spi = DEFAULT_SPI;
	sa->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS;
	sa->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	sa->ipsec_xform.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	sa->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;
	sa->ipsec_xform.options.esn = test_cfg->esn;
	sa->type = test_cfg->type;
	sa->replay_win_sz = test_cfg->replay_win_sz;
	sa->sa_flags = test_cfg->flags;
	sa->cnt.nb_prepare_call = 0;
	sa->cnt.nb_prepare_pkt = 0;
	sa->cnt.nb_process_call = 0;
	sa->cnt.nb_process_pkt = 0;
	sa->cnt.process_ticks_elapsed = 0;
	sa->cnt.prepare_ticks_elapsed = 0;

}

static void
fill_ipsec_sa_in(const struct ipsec_test_cfg *test_cfg,
		  struct ipsec_sa *sa)
{
	sa->ipsec_xform.spi = DEFAULT_SPI;
	sa->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;
	sa->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	sa->ipsec_xform.mode = RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	sa->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;
	sa->ipsec_xform.options.esn = test_cfg->esn;
	sa->type = test_cfg->type;
	sa->replay_win_sz = test_cfg->replay_win_sz;
	sa->sa_flags = test_cfg->flags;
	sa->cnt.nb_prepare_call = 0;
	sa->cnt.nb_prepare_pkt = 0;
	sa->cnt.nb_process_call = 0;
	sa->cnt.nb_process_pkt = 0;
	sa->cnt.process_ticks_elapsed = 0;
	sa->cnt.prepare_ticks_elapsed = 0;
}

static int
init_sa_session(const struct ipsec_test_cfg *test_cfg,
		struct ipsec_sa *sa_out, struct ipsec_sa *sa_in)
{

	int rc;

	fill_ipsec_sa_in(test_cfg, sa_in);
	fill_ipsec_sa_out(test_cfg, sa_out);

	rc = create_sa(RTE_SECURITY_ACTION_TYPE_NONE, sa_out);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "out bound create_sa failed, cfg\n");
		return TEST_FAILED;
	}

	rc = create_sa(RTE_SECURITY_ACTION_TYPE_NONE, sa_in);
	if (rc != 0) {
		RTE_LOG(ERR, USER1, "out bound create_sa failed, cfg\n");
		return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static int
testsuite_setup(void)
{
	struct rte_mbuf *mbuf;
	int i;

	mbuf_pool = rte_pktmbuf_pool_create("IPSEC_PERF_MBUFPOOL",
			NUM_MBUFS, MBUF_CACHE_SIZE, 0, MBUF_SIZE,
			rte_socket_id());
	if (mbuf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Can't create MBUFPOOL\n");
		return TEST_FAILED;
	}

	cop_pool = rte_crypto_op_pool_create(
			"MBUF_CRYPTO_SYM_OP_POOL",
			RTE_CRYPTO_OP_TYPE_SYMMETRIC,
			NUM_MBUFS, MBUF_CACHE_SIZE,
			DEFAULT_NUM_XFORMS *
			sizeof(struct rte_crypto_sym_xform) +
			MAXIMUM_IV_LENGTH,
			rte_socket_id());
	if (cop_pool == NULL) {
		RTE_LOG(ERR, USER1, "Can't create CRYPTO_OP_POOL\n");
		return TEST_FAILED;
	}

	ring_inb_prepare = rte_ring_create("ipsec_test_ring_inb_prepare",
					   RING_SIZE, SOCKET_ID_ANY, 0);
	if (ring_inb_prepare == NULL)
		return TEST_FAILED;

	ring_inb_process = rte_ring_create("ipsec_test_ring_inb_process",
					   RING_SIZE, SOCKET_ID_ANY, 0);
	if (ring_inb_process == NULL)
		return TEST_FAILED;

	ring_outb_prepare = rte_ring_create("ipsec_test_ring_outb_prepare",
					    RING_SIZE, SOCKET_ID_ANY, 0);
	if (ring_outb_prepare == NULL)
		return TEST_FAILED;

	ring_outb_process = rte_ring_create("ipsec_test_ring_outb_process",
					    RING_SIZE, SOCKET_ID_ANY, 0);
	if (ring_outb_process == NULL)
		return TEST_FAILED;

	for (i = 0; i < NUM_MBUF; i++) {
		mbuf = generate_mbuf_data(mbuf_pool);

		if (mbuf && rte_ring_sp_enqueue_bulk(ring_inb_prepare,
			   (void **)&mbuf, 1, NULL))
			continue;
		else
			return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static int
measure_performance(struct ipsec_sa *sa_out, struct ipsec_sa *sa_in)
{
	uint64_t time_diff = 0;
	uint64_t begin = 0;
	uint64_t hz = rte_get_timer_hz();

	begin = rte_get_timer_cycles();

	do {
		if (create_traffic(sa_out, ring_inb_prepare, ring_inb_process,
				   ring_outb_prepare) < 0)
			return TEST_FAILED;

		if (create_traffic(sa_in, ring_outb_prepare, ring_outb_process,
				   ring_inb_prepare) < 0)
			return TEST_FAILED;

		time_diff = rte_get_timer_cycles() - begin;

	} while (time_diff < (hz * 10));

	return TEST_SUCCESS;
}

static void
print_metrics(const struct ipsec_test_cfg *test_cfg,
	      struct ipsec_sa *sa_out, struct ipsec_sa *sa_in)
{
	printf("\nMetrics of libipsec prepare/process api:\n");

	printf("replay window size = %u\n", test_cfg->replay_win_sz);
	if (test_cfg->esn)
		printf("replay esn is enabled\n");
	else
		printf("replay esn is disabled\n");
	if (test_cfg->type == RTE_CRYPTO_SYM_XFORM_AEAD)
		printf("AEAD algo is AES_GCM\n");
	else
		printf("CIPHER/AUTH algo is AES_CBC/SHA1\n");


	printf("avg cycles for a pkt prepare in outbound is = %.2Lf\n",
	(long double)sa_out->cnt.prepare_ticks_elapsed
		    / sa_out->cnt.nb_prepare_pkt);
	printf("avg cycles for a pkt process in outbound is = %.2Lf\n",
	(long double)sa_out->cnt.process_ticks_elapsed
		     / sa_out->cnt.nb_process_pkt);
	printf("avg cycles for a pkt prepare in inbound is = %.2Lf\n",
	(long double)sa_in->cnt.prepare_ticks_elapsed
		     / sa_in->cnt.nb_prepare_pkt);
	printf("avg cycles for a pkt process in inbound is = %.2Lf\n",
	(long double)sa_in->cnt.process_ticks_elapsed
		     / sa_in->cnt.nb_process_pkt);

}

static void
testsuite_teardown(void)
{
	if (mbuf_pool != NULL) {
		RTE_LOG(DEBUG, USER1, "MBUFPOOL count %u\n",
		rte_mempool_avail_count(mbuf_pool));
		rte_mempool_free(mbuf_pool);
		mbuf_pool = NULL;
	}

	if (cop_pool != NULL) {
		RTE_LOG(DEBUG, USER1, "CRYPTO_OP_POOL count %u\n",
		rte_mempool_avail_count(cop_pool));
		rte_mempool_free(cop_pool);
		cop_pool = NULL;
	}

	rte_ring_free(ring_inb_prepare);
	rte_ring_free(ring_inb_process);
	rte_ring_free(ring_outb_prepare);
	rte_ring_free(ring_outb_process);

	ring_inb_prepare = NULL;
	ring_inb_process = NULL;
	ring_outb_prepare = NULL;
	ring_outb_process = NULL;
}

static int
test_libipsec_perf(void)
{
	struct ipsec_sa sa_out;
	struct ipsec_sa sa_in;
	uint32_t i;
	int ret;

	if (testsuite_setup() < 0) {
		testsuite_teardown();
		return TEST_FAILED;
	}

	for (i = 0; i < RTE_DIM(test_cfg) ; i++) {

		ret = init_sa_session(&test_cfg[i], &sa_out, &sa_in);
		if (ret != 0) {
			testsuite_teardown();
			return TEST_FAILED;
		}

		if (measure_performance(&sa_out, &sa_in) < 0) {
			testsuite_teardown();
			return TEST_FAILED;
		}

		print_metrics(&test_cfg[i], &sa_out, &sa_in);
	}

	testsuite_teardown();

	return TEST_SUCCESS;
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_TEST_COMMAND(ipsec_perf_autotest, test_libipsec_perf);
