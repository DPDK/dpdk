/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in
 *	   the documentation and/or other materials provided with the
 *	   distribution.
 *	 * Neither the name of Intel Corporation nor the names of its
 *	   contributors may be used to endorse or promote products derived
 *	   from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_mbuf_offload.h>

#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>

#include "test.h"
#include "test_cryptodev.h"

static enum rte_cryptodev_type gbl_cryptodev_type;

struct crypto_testsuite_params {
	struct rte_mempool *mbuf_pool;
	struct rte_mempool *mbuf_ol_pool;
	struct rte_cryptodev_config conf;
	struct rte_cryptodev_qp_conf qp_conf;

	uint8_t valid_devs[RTE_CRYPTO_MAX_DEVS];
	uint8_t valid_dev_count;
};

struct crypto_unittest_params {
	struct rte_crypto_xform cipher_xform;
	struct rte_crypto_xform auth_xform;

	struct rte_cryptodev_session *sess;

	struct rte_mbuf_offload *ol;
	struct rte_crypto_op *op;

	struct rte_mbuf *obuf, *ibuf;

	uint8_t *digest;
};

/*
 * Forward declarations.
 */
static int
test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(
		struct crypto_unittest_params *ut_params);

static int
test_AES_CBC_HMAC_SHA512_decrypt_perform(struct rte_cryptodev_session *sess,
		struct crypto_unittest_params *ut_params,
		struct crypto_testsuite_params *ts_param);

static struct rte_mbuf *
setup_test_string(struct rte_mempool *mpool,
		const char *string, size_t len, uint8_t blocksize)
{
	struct rte_mbuf *m = rte_pktmbuf_alloc(mpool);
	size_t t_len = len - (blocksize ? (len % blocksize) : 0);

	memset(m->buf_addr, 0, m->buf_len);
	if (m) {
		char *dst = rte_pktmbuf_append(m, t_len);

		if (!dst) {
			rte_pktmbuf_free(m);
			return NULL;
		}

		rte_memcpy(dst, string, t_len);
	}

	return m;
}

#if HEX_DUMP
static void
hexdump_mbuf_data(FILE *f, const char *title, struct rte_mbuf *m)
{
	rte_hexdump(f, title, rte_pktmbuf_mtod(m, const void *), m->data_len);
}
#endif

static struct rte_mbuf *
process_crypto_request(uint8_t dev_id, struct rte_mbuf *ibuf)
{
	struct rte_mbuf *obuf = NULL;
#if HEX_DUMP
	hexdump_mbuf_data(stdout, "Enqueued Packet", ibuf);
#endif

	if (rte_cryptodev_enqueue_burst(dev_id, 0, &ibuf, 1) != 1) {
		printf("Error sending packet for encryption");
		return NULL;
	}
	while (rte_cryptodev_dequeue_burst(dev_id, 0, &obuf, 1) == 0)
		rte_pause();

#if HEX_DUMP
	if (obuf)
		hexdump_mbuf_data(stdout, "Dequeued Packet", obuf);
#endif

	return obuf;
}

static struct crypto_testsuite_params testsuite_params = { NULL };
static struct crypto_unittest_params unittest_params;

static int
testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_info info;
	unsigned i, nb_devs, dev_id = 0;
	uint16_t qp_id;

	memset(ts_params, 0, sizeof(*ts_params));

	ts_params->mbuf_pool = rte_mempool_lookup("CRYPTO_MBUFPOOL");
	if (ts_params->mbuf_pool == NULL) {
		/* Not already created so create */
		ts_params->mbuf_pool = rte_pktmbuf_pool_create(
				"CRYPTO_MBUFPOOL",
				NUM_MBUFS, MBUF_CACHE_SIZE, 0, MBUF_SIZE,
				rte_socket_id());
		if (ts_params->mbuf_pool == NULL) {
			RTE_LOG(ERR, USER1, "Can't create CRYPTO_MBUFPOOL\n");
			return TEST_FAILED;
		}
	}

	ts_params->mbuf_ol_pool = rte_pktmbuf_offload_pool_create(
			"MBUF_OFFLOAD_POOL",
			NUM_MBUFS, MBUF_CACHE_SIZE,
			DEFAULT_NUM_XFORMS * sizeof(struct rte_crypto_xform),
			rte_socket_id());
	if (ts_params->mbuf_ol_pool == NULL) {
		RTE_LOG(ERR, USER1, "Can't create CRYPTO_OP_POOL\n");
		return TEST_FAILED;
	}

	/* Create 2 AESNI MB devices if required */
	if (gbl_cryptodev_type == RTE_CRYPTODEV_AESNI_MB_PMD) {
		nb_devs = rte_cryptodev_count_devtype(
				RTE_CRYPTODEV_AESNI_MB_PMD);
		if (nb_devs < 2) {
			for (i = nb_devs; i < 2; i++) {
				int dev_id = rte_eal_vdev_init(
					CRYPTODEV_NAME_AESNI_MB_PMD, NULL);

				TEST_ASSERT(dev_id >= 0,
					"Failed to create instance %u of"
					" pmd : %s",
					i, CRYPTODEV_NAME_AESNI_MB_PMD);
			}
		}
	}

	nb_devs = rte_cryptodev_count();
	if (nb_devs < 1) {
		RTE_LOG(ERR, USER1, "No crypto devices found?");
		return TEST_FAILED;
	}

	/* Create list of valid crypto devs */
	for (i = 0; i < nb_devs; i++) {
		rte_cryptodev_info_get(i, &info);
		if (info.dev_type == gbl_cryptodev_type)
			ts_params->valid_devs[ts_params->valid_dev_count++] = i;
	}

	if (ts_params->valid_dev_count < 1)
		return TEST_FAILED;

	/* Set up all the qps on the first of the valid devices found */
	for (i = 0; i < 1; i++) {
		dev_id = ts_params->valid_devs[i];

		rte_cryptodev_info_get(dev_id, &info);

		/*
		 * Since we can't free and re-allocate queue memory always set
		 * the queues on this device up to max size first so enough
		 * memory is allocated for any later re-configures needed by
		 * other tests
		 */

		ts_params->conf.nb_queue_pairs = info.max_nb_queue_pairs;
		ts_params->conf.socket_id = SOCKET_ID_ANY;
		ts_params->conf.session_mp.nb_objs = info.max_nb_sessions;

		TEST_ASSERT_SUCCESS(rte_cryptodev_configure(dev_id,
				&ts_params->conf),
				"Failed to configure cryptodev %u with %u qps",
				dev_id, ts_params->conf.nb_queue_pairs);

		ts_params->qp_conf.nb_descriptors = MAX_NUM_OPS_INFLIGHT;

		for (qp_id = 0; qp_id < info.max_nb_queue_pairs; qp_id++) {
			TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
					dev_id, qp_id, &ts_params->qp_conf,
					rte_cryptodev_socket_id(dev_id)),
					"Failed to setup queue pair %u on "
					"cryptodev %u",
					qp_id, dev_id);
		}
	}

	return TEST_SUCCESS;
}

static void
testsuite_teardown(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;

	if (ts_params->mbuf_pool != NULL) {
		RTE_LOG(DEBUG, USER1, "CRYPTO_MBUFPOOL count %u\n",
		rte_mempool_count(ts_params->mbuf_pool));
	}


	if (ts_params->mbuf_ol_pool != NULL) {
		RTE_LOG(DEBUG, USER1, "CRYPTO_OP_POOL count %u\n",
		rte_mempool_count(ts_params->mbuf_ol_pool));
	}

}

static int
ut_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	uint16_t qp_id;

	/* Clear unit test parameters before running test */
	memset(ut_params, 0, sizeof(*ut_params));

	/* Reconfigure device to default parameters */
	ts_params->conf.nb_queue_pairs = DEFAULT_NUM_QPS_PER_QAT_DEVICE;
	ts_params->conf.socket_id = SOCKET_ID_ANY;
	ts_params->conf.session_mp.nb_objs =
			(gbl_cryptodev_type == RTE_CRYPTODEV_QAT_PMD) ?
					DEFAULT_NUM_OPS_INFLIGHT :
					DEFAULT_NUM_OPS_INFLIGHT;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed to configure cryptodev %u",
			ts_params->valid_devs[0]);

	/*
	 * Now reconfigure queues to size we actually want to use in this
	 * test suite.
	 */
	ts_params->qp_conf.nb_descriptors = DEFAULT_NUM_OPS_INFLIGHT;

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs ; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
			ts_params->valid_devs[0], qp_id,
			&ts_params->qp_conf,
			rte_cryptodev_socket_id(ts_params->valid_devs[0])),
			"Failed to setup queue pair %u on cryptodev %u",
			qp_id, ts_params->valid_devs[0]);
	}


	rte_cryptodev_stats_reset(ts_params->valid_devs[0]);

	/* Start the device */
	TEST_ASSERT_SUCCESS(rte_cryptodev_start(ts_params->valid_devs[0]),
			"Failed to start cryptodev %u",
			ts_params->valid_devs[0]);

	return TEST_SUCCESS;
}

static void
ut_teardown(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct rte_cryptodev_stats stats;

	/* free crypto session structure */
	if (ut_params->sess) {
		rte_cryptodev_session_free(ts_params->valid_devs[0],
				ut_params->sess);
		ut_params->sess = NULL;
	}

	/* free crypto operation structure */
	if (ut_params->ol)
		rte_pktmbuf_offload_free(ut_params->ol);

	/*
	 * free mbuf - both obuf and ibuf are usually the same,
	 * but rte copes even if we call free twice
	 */
	if (ut_params->obuf) {
		rte_pktmbuf_free(ut_params->obuf);
		ut_params->obuf = 0;
	}
	if (ut_params->ibuf) {
		rte_pktmbuf_free(ut_params->ibuf);
		ut_params->ibuf = 0;
	}

	if (ts_params->mbuf_pool != NULL)
		RTE_LOG(DEBUG, USER1, "CRYPTO_MBUFPOOL count %u\n",
				rte_mempool_count(ts_params->mbuf_pool));

	rte_cryptodev_stats_get(ts_params->valid_devs[0], &stats);

	/* Stop the device */
	rte_cryptodev_stop(ts_params->valid_devs[0]);
}

static int
test_device_configure_invalid_dev_id(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint16_t dev_id, num_devs = 0;

	TEST_ASSERT((num_devs = rte_cryptodev_count()) >= 1,
			"Need at least %d devices for test", 1);

	/* valid dev_id values */
	dev_id = ts_params->valid_devs[ts_params->valid_dev_count - 1];

	/* Stop the device in case it's started so it can be configured */
	rte_cryptodev_stop(ts_params->valid_devs[dev_id]);

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(dev_id, &ts_params->conf),
			"Failed test for rte_cryptodev_configure: "
			"invalid dev_num %u", dev_id);

	/* invalid dev_id values */
	dev_id = num_devs;

	TEST_ASSERT_FAIL(rte_cryptodev_configure(dev_id, &ts_params->conf),
			"Failed test for rte_cryptodev_configure: "
			"invalid dev_num %u", dev_id);

	dev_id = 0xff;

	TEST_ASSERT_FAIL(rte_cryptodev_configure(dev_id, &ts_params->conf),
			"Failed test for rte_cryptodev_configure:"
			"invalid dev_num %u", dev_id);

	return TEST_SUCCESS;
}

static int
test_device_configure_invalid_queue_pair_ids(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;

	/* Stop the device in case it's started so it can be configured */
	rte_cryptodev_stop(ts_params->valid_devs[0]);

	/* valid - one queue pairs */
	ts_params->conf.nb_queue_pairs = 1;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed to configure cryptodev: dev_id %u, qp_id %u",
			ts_params->valid_devs[0], ts_params->conf.nb_queue_pairs);


	/* valid - max value queue pairs */
	ts_params->conf.nb_queue_pairs = MAX_NUM_QPS_PER_QAT_DEVICE;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed to configure cryptodev: dev_id %u, qp_id %u",
			ts_params->valid_devs[0], ts_params->conf.nb_queue_pairs);


	/* invalid - zero queue pairs */
	ts_params->conf.nb_queue_pairs = 0;

	TEST_ASSERT_FAIL(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed test for rte_cryptodev_configure, dev_id %u,"
			" invalid qps: %u",
			ts_params->valid_devs[0],
			ts_params->conf.nb_queue_pairs);


	/* invalid - max value supported by field queue pairs */
	ts_params->conf.nb_queue_pairs = UINT16_MAX;

	TEST_ASSERT_FAIL(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed test for rte_cryptodev_configure, dev_id %u,"
			" invalid qps: %u",
			ts_params->valid_devs[0],
			ts_params->conf.nb_queue_pairs);


	/* invalid - max value + 1 queue pairs */
	ts_params->conf.nb_queue_pairs = MAX_NUM_QPS_PER_QAT_DEVICE + 1;

	TEST_ASSERT_FAIL(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed test for rte_cryptodev_configure, dev_id %u,"
			" invalid qps: %u",
			ts_params->valid_devs[0],
			ts_params->conf.nb_queue_pairs);

	return TEST_SUCCESS;
}

static int
test_queue_pair_descriptor_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_info dev_info;
	struct rte_cryptodev_qp_conf qp_conf = {
		.nb_descriptors = MAX_NUM_OPS_INFLIGHT
	};

	uint16_t qp_id;

	/* Stop the device in case it's started so it can be configured */
	rte_cryptodev_stop(ts_params->valid_devs[0]);


	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	ts_params->conf.session_mp.nb_objs = dev_info.max_nb_sessions;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf), "Failed to configure cryptodev %u",
			ts_params->valid_devs[0]);


	/*
	 * Test various ring sizes on this device. memzones can't be
	 * freed so are re-used if ring is released and re-created.
	 */
	qp_conf.nb_descriptors = MIN_NUM_OPS_INFLIGHT; /* min size*/

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Failed test for "
				"rte_cryptodev_queue_pair_setup: num_inflights "
				"%u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	qp_conf.nb_descriptors = (uint32_t)(MAX_NUM_OPS_INFLIGHT / 2);

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Failed test for"
				" rte_cryptodev_queue_pair_setup: num_inflights"
				" %u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	qp_conf.nb_descriptors = MAX_NUM_OPS_INFLIGHT; /* valid */

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Failed test for "
				"rte_cryptodev_queue_pair_setup: num_inflights"
				" %u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	/* invalid number of descriptors - max supported + 2 */
	qp_conf.nb_descriptors = MAX_NUM_OPS_INFLIGHT + 2;

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_FAIL(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Unexpectedly passed test for "
				"rte_cryptodev_queue_pair_setup:"
				"num_inflights %u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	/* invalid number of descriptors - max value of parameter */
	qp_conf.nb_descriptors = UINT32_MAX-1;

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_FAIL(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Unexpectedly passed test for "
				"rte_cryptodev_queue_pair_setup:"
				"num_inflights %u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	qp_conf.nb_descriptors = DEFAULT_NUM_OPS_INFLIGHT;

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Failed test for"
				" rte_cryptodev_queue_pair_setup:"
				"num_inflights %u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	/* invalid number of descriptors - max supported + 1 */
	qp_conf.nb_descriptors = DEFAULT_NUM_OPS_INFLIGHT + 1;

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_FAIL(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Unexpectedly passed test for "
				"rte_cryptodev_queue_pair_setup:"
				"num_inflights %u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	/* test invalid queue pair id */
	qp_conf.nb_descriptors = DEFAULT_NUM_OPS_INFLIGHT;	/*valid */

	qp_id = DEFAULT_NUM_QPS_PER_QAT_DEVICE;		/*invalid */

	TEST_ASSERT_FAIL(rte_cryptodev_queue_pair_setup(
			ts_params->valid_devs[0],
			qp_id, &qp_conf,
			rte_cryptodev_socket_id(ts_params->valid_devs[0])),
			"Failed test for rte_cryptodev_queue_pair_setup:"
			"invalid qp %u on cryptodev %u",
			qp_id, ts_params->valid_devs[0]);

	qp_id = 0xffff; /*invalid*/

	TEST_ASSERT_FAIL(rte_cryptodev_queue_pair_setup(
			ts_params->valid_devs[0],
			qp_id, &qp_conf,
			rte_cryptodev_socket_id(ts_params->valid_devs[0])),
			"Failed test for rte_cryptodev_queue_pair_setup:"
			"invalid qp %u on cryptodev %u",
			qp_id, ts_params->valid_devs[0]);

	return TEST_SUCCESS;
}

/* ***** Plaintext data for tests ***** */

const char catch_22_quote_1[] =
		"There was only one catch and that was Catch-22, which "
		"specified that a concern for one's safety in the face of "
		"dangers that were real and immediate was the process of a "
		"rational mind. Orr was crazy and could be grounded. All he "
		"had to do was ask; and as soon as he did, he would no longer "
		"be crazy and would have to fly more missions. Orr would be "
		"crazy to fly more missions and sane if he didn't, but if he "
		"was sane he had to fly them. If he flew them he was crazy "
		"and didn't have to; but if he didn't want to he was sane and "
		"had to. Yossarian was moved very deeply by the absolute "
		"simplicity of this clause of Catch-22 and let out a "
		"respectful whistle. \"That's some catch, that Catch-22\", he "
		"observed. \"It's the best there is,\" Doc Daneeka agreed.";

const char catch_22_quote[] =
		"What a lousy earth! He wondered how many people were "
		"destitute that same night even in his own prosperous country, "
		"how many homes were shanties, how many husbands were drunk "
		"and wives socked, and how many children were bullied, abused, "
		"or abandoned. How many families hungered for food they could "
		"not afford to buy? How many hearts were broken? How many "
		"suicides would take place that same night, how many people "
		"would go insane? How many cockroaches and landlords would "
		"triumph? How many winners were losers, successes failures, "
		"and rich men poor men? How many wise guys were stupid? How "
		"many happy endings were unhappy endings? How many honest men "
		"were liars, brave men cowards, loyal men traitors, how many "
		"sainted men were corrupt, how many people in positions of "
		"trust had sold their souls to bodyguards, how many had never "
		"had souls? How many straight-and-narrow paths were crooked "
		"paths? How many best families were worst families and how "
		"many good people were bad people? When you added them all up "
		"and then subtracted, you might be left with only the children, "
		"and perhaps with Albert Einstein and an old violinist or "
		"sculptor somewhere.";

#define QUOTE_480_BYTES		(480)
#define QUOTE_512_BYTES		(512)
#define QUOTE_768_BYTES		(768)
#define QUOTE_1024_BYTES	(1024)



/* ***** SHA1 Hash Tests ***** */

#define HMAC_KEY_LENGTH_SHA1	(DIGEST_BYTE_LENGTH_SHA1)

static uint8_t hmac_sha1_key[] = {
	0xF8, 0x2A, 0xC7, 0x54, 0xDB, 0x96, 0x18, 0xAA,
	0xC3, 0xA1, 0x53, 0xF6, 0x1F, 0x17, 0x60, 0xBD,
	0xDE, 0xF4, 0xDE, 0xAD };

/* ***** SHA224 Hash Tests ***** */

#define HMAC_KEY_LENGTH_SHA224	(DIGEST_BYTE_LENGTH_SHA224)


/* ***** AES-CBC Cipher Tests ***** */

#define CIPHER_KEY_LENGTH_AES_CBC	(16)
#define CIPHER_IV_LENGTH_AES_CBC	(CIPHER_KEY_LENGTH_AES_CBC)

static uint8_t aes_cbc_key[] = {
	0xE4, 0x23, 0x33, 0x8A, 0x35, 0x64, 0x61, 0xE2,
	0x49, 0x03, 0xDD, 0xC6, 0xB8, 0xCA, 0x55, 0x7A };

static uint8_t aes_cbc_iv[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };


/* ***** AES-CBC / HMAC-SHA1 Hash Tests ***** */

static const uint8_t catch_22_quote_2_512_bytes_AES_CBC_ciphertext[] = {
	0x8B, 0X4D, 0XDA, 0X1B, 0XCF, 0X04, 0XA0, 0X31,
	0XB4, 0XBF, 0XBD, 0X68, 0X43, 0X20, 0X7E, 0X76,
	0XB1, 0X96, 0X8B, 0XA2, 0X7C, 0XA2, 0X83, 0X9E,
	0X39, 0X5A, 0X2F, 0X7E, 0X92, 0XB4, 0X48, 0X1A,
	0X3F, 0X6B, 0X5D, 0XDF, 0X52, 0X85, 0X5F, 0X8E,
	0X42, 0X3C, 0XFB, 0XE9, 0X1A, 0X24, 0XD6, 0X08,
	0XDD, 0XFD, 0X16, 0XFB, 0XE9, 0X55, 0XEF, 0XF0,
	0XA0, 0X8D, 0X13, 0XAB, 0X81, 0XC6, 0X90, 0X01,
	0XB5, 0X18, 0X84, 0XB3, 0XF6, 0XE6, 0X11, 0X57,
	0XD6, 0X71, 0XC6, 0X3C, 0X3F, 0X2F, 0X33, 0XEE,
	0X24, 0X42, 0X6E, 0XAC, 0X0B, 0XCA, 0XEC, 0XF9,
	0X84, 0XF8, 0X22, 0XAA, 0X60, 0XF0, 0X32, 0XA9,
	0X75, 0X75, 0X3B, 0XCB, 0X70, 0X21, 0X0A, 0X8D,
	0X0F, 0XE0, 0XC4, 0X78, 0X2B, 0XF8, 0X97, 0XE3,
	0XE4, 0X26, 0X4B, 0X29, 0XDA, 0X88, 0XCD, 0X46,
	0XEC, 0XAA, 0XF9, 0X7F, 0XF1, 0X15, 0XEA, 0XC3,
	0X87, 0XE6, 0X31, 0XF2, 0XCF, 0XDE, 0X4D, 0X80,
	0X70, 0X91, 0X7E, 0X0C, 0XF7, 0X26, 0X3A, 0X92,
	0X4F, 0X18, 0X83, 0XC0, 0X8F, 0X59, 0X01, 0XA5,
	0X88, 0XD1, 0XDB, 0X26, 0X71, 0X27, 0X16, 0XF5,
	0XEE, 0X10, 0X82, 0XAC, 0X68, 0X26, 0X9B, 0XE2,
	0X6D, 0XD8, 0X9A, 0X80, 0XDF, 0X04, 0X31, 0XD5,
	0XF1, 0X35, 0X5C, 0X3B, 0XDD, 0X9A, 0X65, 0XBA,
	0X58, 0X34, 0X85, 0X61, 0X1C, 0X42, 0X10, 0X76,
	0X73, 0X02, 0X42, 0XC9, 0X23, 0X18, 0X8E, 0XB4,
	0X6F, 0XB4, 0XA3, 0X54, 0X6E, 0X88, 0X3B, 0X62,
	0X7C, 0X02, 0X8D, 0X4C, 0X9F, 0XC8, 0X45, 0XF4,
	0XC9, 0XDE, 0X4F, 0XEB, 0X22, 0X83, 0X1B, 0XE4,
	0X49, 0X37, 0XE4, 0XAD, 0XE7, 0XCD, 0X21, 0X54,
	0XBC, 0X1C, 0XC2, 0X04, 0X97, 0XB4, 0X10, 0X61,
	0XF0, 0XE4, 0XEF, 0X27, 0X63, 0X3A, 0XDA, 0X91,
	0X41, 0X25, 0X62, 0X1C, 0X5C, 0XB6, 0X38, 0X4A,
	0X88, 0X71, 0X59, 0X5A, 0X8D, 0XA0, 0X09, 0XAF,
	0X72, 0X94, 0XD7, 0X79, 0X5C, 0X60, 0X7C, 0X8F,
	0X4C, 0XF5, 0XD9, 0XA1, 0X39, 0X6D, 0X81, 0X28,
	0XEF, 0X13, 0X28, 0XDF, 0XF5, 0X3E, 0XF7, 0X8E,
	0X09, 0X9C, 0X78, 0X18, 0X79, 0XB8, 0X68, 0XD7,
	0XA8, 0X29, 0X62, 0XAD, 0XDE, 0XE1, 0X61, 0X76,
	0X1B, 0X05, 0X16, 0XCD, 0XBF, 0X02, 0X8E, 0XA6,
	0X43, 0X6E, 0X92, 0X55, 0X4F, 0X60, 0X9C, 0X03,
	0XB8, 0X4F, 0XA3, 0X02, 0XAC, 0XA8, 0XA7, 0X0C,
	0X1E, 0XB5, 0X6B, 0XF8, 0XC8, 0X4D, 0XDE, 0XD2,
	0XB0, 0X29, 0X6E, 0X40, 0XE6, 0XD6, 0XC9, 0XE6,
	0XB9, 0X0F, 0XB6, 0X63, 0XF5, 0XAA, 0X2B, 0X96,
	0XA7, 0X16, 0XAC, 0X4E, 0X0A, 0X33, 0X1C, 0XA6,
	0XE6, 0XBD, 0X8A, 0XCF, 0X40, 0XA9, 0XB2, 0XFA,
	0X63, 0X27, 0XFD, 0X9B, 0XD9, 0XFC, 0XD5, 0X87,
	0X8D, 0X4C, 0XB6, 0XA4, 0XCB, 0XE7, 0X74, 0X55,
	0XF4, 0XFB, 0X41, 0X25, 0XB5, 0X4B, 0X0A, 0X1B,
	0XB1, 0XD6, 0XB7, 0XD9, 0X47, 0X2A, 0XC3, 0X98,
	0X6A, 0XC4, 0X03, 0X73, 0X1F, 0X93, 0X6E, 0X53,
	0X19, 0X25, 0X64, 0X15, 0X83, 0XF9, 0X73, 0X2A,
	0X74, 0XB4, 0X93, 0X69, 0XC4, 0X72, 0XFC, 0X26,
	0XA2, 0X9F, 0X43, 0X45, 0XDD, 0XB9, 0XEF, 0X36,
	0XC8, 0X3A, 0XCD, 0X99, 0X9B, 0X54, 0X1A, 0X36,
	0XC1, 0X59, 0XF8, 0X98, 0XA8, 0XCC, 0X28, 0X0D,
	0X73, 0X4C, 0XEE, 0X98, 0XCB, 0X7C, 0X58, 0X7E,
	0X20, 0X75, 0X1E, 0XB7, 0XC9, 0XF8, 0XF2, 0X0E,
	0X63, 0X9E, 0X05, 0X78, 0X1A, 0XB6, 0XA8, 0X7A,
	0XF9, 0X98, 0X6A, 0XA6, 0X46, 0X84, 0X2E, 0XF6,
	0X4B, 0XDC, 0X9B, 0X8F, 0X9B, 0X8F, 0XEE, 0XB4,
	0XAA, 0X3F, 0XEE, 0XC0, 0X37, 0X27, 0X76, 0XC7,
	0X95, 0XBB, 0X26, 0X74, 0X69, 0X12, 0X7F, 0XF1,
	0XBB, 0XFF, 0XAE, 0XB5, 0X99, 0X6E, 0XCB, 0X0C
};

static const uint8_t catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA1_digest[] = {
	0x9a, 0X4f, 0X88, 0X1b, 0Xb6, 0X8f, 0Xd8, 0X60,
	0X42, 0X1a, 0X7d, 0X3d, 0Xf5, 0X82, 0X80, 0Xf1,
	0X18, 0X8c, 0X1d, 0X32 };


static int
test_AES_CBC_HMAC_SHA1_encrypt_digest(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Generate test mbuf data and space for digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			catch_22_quote,	QUOTE_512_BYTES, 0);

	ut_params->digest = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			DIGEST_BYTE_LENGTH_SHA1);
	TEST_ASSERT_NOT_NULL(ut_params->digest, "no room to append digest");

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_XFORM_CIPHER;
	ut_params->cipher_xform.next = &ut_params->auth_xform;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
	ut_params->cipher_xform.cipher.key.data = aes_cbc_key;
	ut_params->cipher_xform.cipher.key.length = CIPHER_KEY_LENGTH_AES_CBC;

	/* Setup HMAC Parameters */

	ut_params->auth_xform.type = RTE_CRYPTO_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA1_HMAC;
	ut_params->auth_xform.auth.key.length = HMAC_KEY_LENGTH_SHA1;
	ut_params->auth_xform.auth.key.data = hmac_sha1_key;
	ut_params->auth_xform.auth.digest_length = DIGEST_BYTE_LENGTH_SHA1;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_session_create(ts_params->valid_devs[0],
			&ut_params->cipher_xform);
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	/* Generate Crypto op data structure */
	ut_params->ol = rte_pktmbuf_offload_alloc(ts_params->mbuf_ol_pool,
				RTE_PKTMBUF_OL_CRYPTO);
	TEST_ASSERT_NOT_NULL(ut_params->ol,
			"Failed to allocate pktmbuf offload");

	ut_params->op = &ut_params->ol->op.crypto;

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_session(ut_params->op, ut_params->sess);

	ut_params->op->digest.data = ut_params->digest;
	ut_params->op->digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, QUOTE_512_BYTES);
	ut_params->op->digest.length = DIGEST_BYTE_LENGTH_SHA1;

	ut_params->op->iv.data = (uint8_t *)rte_pktmbuf_prepend(ut_params->ibuf,
			CIPHER_IV_LENGTH_AES_CBC);
	ut_params->op->iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	ut_params->op->iv.length = CIPHER_IV_LENGTH_AES_CBC;

	rte_memcpy(ut_params->op->iv.data, aes_cbc_iv,
			CIPHER_IV_LENGTH_AES_CBC);

	ut_params->op->data.to_cipher.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_cipher.length = QUOTE_512_BYTES;
	ut_params->op->data.to_hash.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_hash.length = QUOTE_512_BYTES;

	rte_pktmbuf_offload_attach(ut_params->ibuf, ut_params->ol);

	/* Process crypto operation */
	ut_params->obuf = process_crypto_request(ts_params->valid_devs[0],
			ut_params->ibuf);
	TEST_ASSERT_NOT_NULL(ut_params->obuf, "failed to retrieve obuf");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->obuf, uint8_t *) +
			CIPHER_IV_LENGTH_AES_CBC,
			catch_22_quote_2_512_bytes_AES_CBC_ciphertext,
			QUOTE_512_BYTES,
			"Ciphertext data not as expected");

	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->obuf, uint8_t *) +
			CIPHER_IV_LENGTH_AES_CBC + QUOTE_512_BYTES,
			catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA1_digest,
			gbl_cryptodev_type == RTE_CRYPTODEV_AESNI_MB_PMD ?
					TRUNCATED_DIGEST_BYTE_LENGTH_SHA1 :
					DIGEST_BYTE_LENGTH_SHA1,
			"Generated digest data not as expected");

	return TEST_SUCCESS;
}

static int
test_AES_CBC_HMAC_SHA1_encrypt_digest_sessionless(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Generate test mbuf data and space for digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			catch_22_quote, QUOTE_512_BYTES, 0);

	ut_params->digest = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			DIGEST_BYTE_LENGTH_SHA1);
	TEST_ASSERT_NOT_NULL(ut_params->digest, "no room to append digest");

	/* Generate Crypto op data structure */
	ut_params->ol = rte_pktmbuf_offload_alloc(ts_params->mbuf_ol_pool,
				RTE_PKTMBUF_OL_CRYPTO);
	TEST_ASSERT_NOT_NULL(ut_params->ol,
			"Failed to allocate pktmbuf offload");

	ut_params->op = &ut_params->ol->op.crypto;

	TEST_ASSERT_NOT_NULL(rte_pktmbuf_offload_alloc_crypto_xforms(
			ut_params->ol, 2),
			"failed to allocate space for crypto transforms");

	/* Set crypto operation data parameters */
	ut_params->op->xform->type = RTE_CRYPTO_XFORM_CIPHER;

	/* cipher parameters */
	ut_params->op->xform->cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
	ut_params->op->xform->cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	ut_params->op->xform->cipher.key.data = aes_cbc_key;
	ut_params->op->xform->cipher.key.length = CIPHER_KEY_LENGTH_AES_CBC;

	/* hash parameters */
	ut_params->op->xform->next->type = RTE_CRYPTO_XFORM_AUTH;

	ut_params->op->xform->next->auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
	ut_params->op->xform->next->auth.algo = RTE_CRYPTO_AUTH_SHA1_HMAC;
	ut_params->op->xform->next->auth.key.length = HMAC_KEY_LENGTH_SHA1;
	ut_params->op->xform->next->auth.key.data = hmac_sha1_key;
	ut_params->op->xform->next->auth.digest_length =
			DIGEST_BYTE_LENGTH_SHA1;

	ut_params->op->digest.data = ut_params->digest;
	ut_params->op->digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, QUOTE_512_BYTES);
	ut_params->op->digest.length = DIGEST_BYTE_LENGTH_SHA1;

	ut_params->op->iv.data = (uint8_t *)rte_pktmbuf_prepend(ut_params->ibuf,
			CIPHER_IV_LENGTH_AES_CBC);
	ut_params->op->iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	ut_params->op->iv.length = CIPHER_IV_LENGTH_AES_CBC;

	rte_memcpy(ut_params->op->iv.data, aes_cbc_iv,
			CIPHER_IV_LENGTH_AES_CBC);

	ut_params->op->data.to_cipher.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_cipher.length = QUOTE_512_BYTES;
	ut_params->op->data.to_hash.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_hash.length = QUOTE_512_BYTES;

	rte_pktmbuf_offload_attach(ut_params->ibuf, ut_params->ol);

	/* Process crypto operation */
	ut_params->obuf = process_crypto_request(ts_params->valid_devs[0],
			ut_params->ibuf);
	TEST_ASSERT_NOT_NULL(ut_params->obuf, "failed to retrieve obuf");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->obuf, uint8_t *) +
			CIPHER_IV_LENGTH_AES_CBC,
			catch_22_quote_2_512_bytes_AES_CBC_ciphertext,
			QUOTE_512_BYTES,
			"Ciphertext data not as expected");

	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->obuf, uint8_t *) +
			CIPHER_IV_LENGTH_AES_CBC + QUOTE_512_BYTES,
			catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA1_digest,
			gbl_cryptodev_type == RTE_CRYPTODEV_AESNI_MB_PMD ?
					TRUNCATED_DIGEST_BYTE_LENGTH_SHA1 :
					DIGEST_BYTE_LENGTH_SHA1,
			"Generated digest data not as expected");


	return TEST_SUCCESS;
}

static int
test_AES_CBC_HMAC_SHA1_decrypt_digest_verify(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Generate test mbuf data and digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			(const char *)
			catch_22_quote_2_512_bytes_AES_CBC_ciphertext,
			QUOTE_512_BYTES, 0);

	ut_params->digest = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			DIGEST_BYTE_LENGTH_SHA1);
	TEST_ASSERT_NOT_NULL(ut_params->digest,	"no room to append digest");

	rte_memcpy(ut_params->digest,
			catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA1_digest,
			DIGEST_BYTE_LENGTH_SHA1);

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
	ut_params->cipher_xform.cipher.key.data = aes_cbc_key;
	ut_params->cipher_xform.cipher.key.length = CIPHER_KEY_LENGTH_AES_CBC;

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_XFORM_AUTH;
	ut_params->auth_xform.next = &ut_params->cipher_xform;

	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA1_HMAC;
	ut_params->auth_xform.auth.key.length = HMAC_KEY_LENGTH_SHA1;
	ut_params->auth_xform.auth.key.data = hmac_sha1_key;
	ut_params->auth_xform.auth.digest_length = DIGEST_BYTE_LENGTH_SHA1;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_session_create(ts_params->valid_devs[0],
			&ut_params->auth_xform);
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	/* Generate Crypto op data structure */
	ut_params->ol = rte_pktmbuf_offload_alloc(ts_params->mbuf_ol_pool,
				RTE_PKTMBUF_OL_CRYPTO);
	TEST_ASSERT_NOT_NULL(ut_params->ol,
			"Failed to allocate pktmbuf offload");

	ut_params->op = &ut_params->ol->op.crypto;


	/* Set crypto operation data parameters */
	rte_crypto_op_attach_session(ut_params->op, ut_params->sess);

	ut_params->op->digest.data = ut_params->digest;
	ut_params->op->digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, QUOTE_512_BYTES);
	ut_params->op->digest.length = DIGEST_BYTE_LENGTH_SHA1;

	ut_params->op->iv.data = (uint8_t *)rte_pktmbuf_prepend(ut_params->ibuf,
			CIPHER_IV_LENGTH_AES_CBC);
	ut_params->op->iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	ut_params->op->iv.length = CIPHER_IV_LENGTH_AES_CBC;

	rte_memcpy(ut_params->op->iv.data, aes_cbc_iv,
			CIPHER_IV_LENGTH_AES_CBC);

	ut_params->op->data.to_cipher.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_cipher.length = QUOTE_512_BYTES;

	ut_params->op->data.to_hash.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_hash.length = QUOTE_512_BYTES;

	rte_pktmbuf_offload_attach(ut_params->ibuf, ut_params->ol);

	/* Process crypto operation */
	ut_params->obuf = process_crypto_request(ts_params->valid_devs[0],
			ut_params->ibuf);
	TEST_ASSERT_NOT_NULL(ut_params->obuf, "failed to retrieve obuf");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->obuf, uint8_t *) +
			CIPHER_IV_LENGTH_AES_CBC,
			catch_22_quote,
			QUOTE_512_BYTES,
			"Ciphertext data not as expected");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"Digest verification failed");


	return TEST_SUCCESS;
}


/* ***** AES-CBC / HMAC-SHA256 Hash Tests ***** */

#define HMAC_KEY_LENGTH_SHA256	(DIGEST_BYTE_LENGTH_SHA256)

static uint8_t hmac_sha256_key[] = {
	0x42, 0x1a, 0x7d, 0x3d, 0xf5, 0x82, 0x80, 0xf1,
	0xF1, 0x35, 0x5C, 0x3B, 0xDD, 0x9A, 0x65, 0xBA,
	0x58, 0x34, 0x85, 0x61, 0x1C, 0x42, 0x10, 0x76,
	0x9a, 0x4f, 0x88, 0x1b, 0xb6, 0x8f, 0xd8, 0x60 };

static const uint8_t catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA256_digest[] = {
	0xc8, 0x57, 0x57, 0x31, 0x03, 0xe0, 0x03, 0x55,
	0x07, 0xc8, 0x9e, 0x7f, 0x48, 0x9a, 0x61, 0x9a,
	0x68, 0xee, 0x03, 0x0e, 0x71, 0x75, 0xc7, 0xf4,
	0x2e, 0x45, 0x26, 0x32, 0x7c, 0x12, 0x15, 0x15 };

static int
test_AES_CBC_HMAC_SHA256_encrypt_digest(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Generate test mbuf data and space for digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			catch_22_quote,	QUOTE_512_BYTES, 0);

	ut_params->digest = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			DIGEST_BYTE_LENGTH_SHA256);
	TEST_ASSERT_NOT_NULL(ut_params->digest, "no room to append digest");

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_XFORM_CIPHER;
	ut_params->cipher_xform.next = &ut_params->auth_xform;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
	ut_params->cipher_xform.cipher.key.data = aes_cbc_key;
	ut_params->cipher_xform.cipher.key.length = CIPHER_KEY_LENGTH_AES_CBC;

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA256_HMAC;
	ut_params->auth_xform.auth.key.length = HMAC_KEY_LENGTH_SHA256;
	ut_params->auth_xform.auth.key.data = hmac_sha256_key;
	ut_params->auth_xform.auth.digest_length = DIGEST_BYTE_LENGTH_SHA256;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_session_create(ts_params->valid_devs[0],
			&ut_params->cipher_xform);
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	/* Generate Crypto op data structure */
	ut_params->ol = rte_pktmbuf_offload_alloc(ts_params->mbuf_ol_pool,
				RTE_PKTMBUF_OL_CRYPTO);
	TEST_ASSERT_NOT_NULL(ut_params->ol,
			"Failed to allocate pktmbuf offload");

	ut_params->op = &ut_params->ol->op.crypto;


	/* Set crypto operation data parameters */
	rte_crypto_op_attach_session(ut_params->op, ut_params->sess);

	ut_params->op->digest.data = ut_params->digest;
	ut_params->op->digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, QUOTE_512_BYTES);
	ut_params->op->digest.length = DIGEST_BYTE_LENGTH_SHA256;

	ut_params->op->iv.data = (uint8_t *)rte_pktmbuf_prepend(ut_params->ibuf,
			CIPHER_IV_LENGTH_AES_CBC);
	ut_params->op->iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	ut_params->op->iv.length = CIPHER_IV_LENGTH_AES_CBC;

	rte_memcpy(ut_params->op->iv.data, aes_cbc_iv,
			CIPHER_IV_LENGTH_AES_CBC);

	ut_params->op->data.to_cipher.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_cipher.length = QUOTE_512_BYTES;
	ut_params->op->data.to_hash.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_hash.length = QUOTE_512_BYTES;

	rte_pktmbuf_offload_attach(ut_params->ibuf, ut_params->ol);

	/* Process crypto operation */
	ut_params->obuf = process_crypto_request(ts_params->valid_devs[0],
			ut_params->ibuf);
	TEST_ASSERT_NOT_NULL(ut_params->obuf, "failed to retrieve obuf");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->obuf, uint8_t *) +
			CIPHER_IV_LENGTH_AES_CBC,
			catch_22_quote_2_512_bytes_AES_CBC_ciphertext,
			QUOTE_512_BYTES,
			"Ciphertext data not as expected");

	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->obuf, uint8_t *) +
			CIPHER_IV_LENGTH_AES_CBC + QUOTE_512_BYTES,
			catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA256_digest,
			gbl_cryptodev_type == RTE_CRYPTODEV_AESNI_MB_PMD ?
					TRUNCATED_DIGEST_BYTE_LENGTH_SHA256 :
					DIGEST_BYTE_LENGTH_SHA256,
			"Generated digest data not as expected");


	return TEST_SUCCESS;
}

static int
test_AES_CBC_HMAC_SHA256_decrypt_digest_verify(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Generate test mbuf data and digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			(const char *)
			catch_22_quote_2_512_bytes_AES_CBC_ciphertext,
			QUOTE_512_BYTES, 0);

	ut_params->digest = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			DIGEST_BYTE_LENGTH_SHA256);
	TEST_ASSERT_NOT_NULL(ut_params->digest,	"no room to append digest");

	rte_memcpy(ut_params->digest,
			catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA256_digest,
			DIGEST_BYTE_LENGTH_SHA256);

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
	ut_params->cipher_xform.cipher.key.data = aes_cbc_key;
	ut_params->cipher_xform.cipher.key.length = CIPHER_KEY_LENGTH_AES_CBC;

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_XFORM_AUTH;
	ut_params->auth_xform.next = &ut_params->cipher_xform;

	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA256_HMAC;
	ut_params->auth_xform.auth.key.data = hmac_sha256_key;
	ut_params->auth_xform.auth.key.length = HMAC_KEY_LENGTH_SHA256;
	ut_params->auth_xform.auth.digest_length = DIGEST_BYTE_LENGTH_SHA256;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_session_create(ts_params->valid_devs[0],
			&ut_params->auth_xform);
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	/* Generate Crypto op data structure */
	ut_params->ol = rte_pktmbuf_offload_alloc(ts_params->mbuf_ol_pool,
				RTE_PKTMBUF_OL_CRYPTO);
	TEST_ASSERT_NOT_NULL(ut_params->ol,
			"Failed to allocate pktmbuf offload");

	ut_params->op = &ut_params->ol->op.crypto;


	/* Set crypto operation data parameters */
	rte_crypto_op_attach_session(ut_params->op, ut_params->sess);

	ut_params->op->digest.data = ut_params->digest;
	ut_params->op->digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, QUOTE_512_BYTES);
	ut_params->op->digest.length = DIGEST_BYTE_LENGTH_SHA256;

	ut_params->op->iv.data = (uint8_t *)rte_pktmbuf_prepend(
			ut_params->ibuf, CIPHER_IV_LENGTH_AES_CBC);
	ut_params->op->iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	ut_params->op->iv.length = CIPHER_IV_LENGTH_AES_CBC;

	rte_memcpy(ut_params->op->iv.data, aes_cbc_iv,
			CIPHER_IV_LENGTH_AES_CBC);

	ut_params->op->data.to_cipher.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_cipher.length = QUOTE_512_BYTES;

	ut_params->op->data.to_hash.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_hash.length = QUOTE_512_BYTES;

	rte_pktmbuf_offload_attach(ut_params->ibuf, ut_params->ol);

	/* Process crypto operation */
	ut_params->obuf = process_crypto_request(ts_params->valid_devs[0],
			ut_params->ibuf);
	TEST_ASSERT_NOT_NULL(ut_params->obuf, "failed to retrieve obuf");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->obuf, uint8_t *) +
			CIPHER_IV_LENGTH_AES_CBC, catch_22_quote,
			QUOTE_512_BYTES,
			"Plaintext data not as expected");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"Digest verification failed");


	return TEST_SUCCESS;
}

/* ***** AES-CBC / HMAC-SHA512 Hash Tests ***** */

#define HMAC_KEY_LENGTH_SHA512  (DIGEST_BYTE_LENGTH_SHA512)

static uint8_t hmac_sha512_key[] = {
	0x42, 0x1a, 0x7d, 0x3d, 0xf5, 0x82, 0x80, 0xf1,
	0xF1, 0x35, 0x5C, 0x3B, 0xDD, 0x9A, 0x65, 0xBA,
	0x58, 0x34, 0x85, 0x65, 0x1C, 0x42, 0x50, 0x76,
	0x9a, 0xaf, 0x88, 0x1b, 0xb6, 0x8f, 0xf8, 0x60,
	0xa2, 0x5a, 0x7f, 0x3f, 0xf4, 0x72, 0x70, 0xf1,
	0xF5, 0x35, 0x4C, 0x3B, 0xDD, 0x90, 0x65, 0xB0,
	0x47, 0x3a, 0x75, 0x61, 0x5C, 0xa2, 0x10, 0x76,
	0x9a, 0xaf, 0x77, 0x5b, 0xb6, 0x7f, 0xf7, 0x60 };

static const uint8_t catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA512_digest[] = {
	0x5D, 0x54, 0x66, 0xC1, 0x6E, 0xBC, 0x04, 0xB8,
	0x46, 0xB8, 0x08, 0x6E, 0xE0, 0xF0, 0x43, 0x48,
	0x37, 0x96, 0x9C, 0xC6, 0x9C, 0xC2, 0x1E, 0xE8,
	0xF2, 0x0C, 0x0B, 0xEF, 0x86, 0xA2, 0xE3, 0x70,
	0x95, 0xC8, 0xB3, 0x06, 0x47, 0xA9, 0x90, 0xE8,
	0xA0, 0xC6, 0x72, 0x69, 0x05, 0xC0, 0x0D, 0x0E,
	0x21, 0x96, 0x65, 0x93, 0x74, 0x43, 0x2A, 0x1D,
	0x2E, 0xBF, 0xC2, 0xC2, 0xEE, 0xCC, 0x2F, 0x0A };

static int
test_AES_CBC_HMAC_SHA512_encrypt_digest(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Generate test mbuf data and space for digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			catch_22_quote,	QUOTE_512_BYTES, 0);

	ut_params->digest = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			DIGEST_BYTE_LENGTH_SHA512);
	TEST_ASSERT_NOT_NULL(ut_params->digest, "no room to append digest");

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_XFORM_CIPHER;
	ut_params->cipher_xform.next = &ut_params->auth_xform;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
	ut_params->cipher_xform.cipher.key.data = aes_cbc_key;
	ut_params->cipher_xform.cipher.key.length = CIPHER_KEY_LENGTH_AES_CBC;

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA512_HMAC;
	ut_params->auth_xform.auth.key.length = HMAC_KEY_LENGTH_SHA512;
	ut_params->auth_xform.auth.key.data = hmac_sha512_key;
	ut_params->auth_xform.auth.digest_length = DIGEST_BYTE_LENGTH_SHA512;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_session_create(ts_params->valid_devs[0],
			&ut_params->cipher_xform);

	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");


	/* Generate Crypto op data structure */
	ut_params->ol = rte_pktmbuf_offload_alloc(ts_params->mbuf_ol_pool,
				RTE_PKTMBUF_OL_CRYPTO);
	TEST_ASSERT_NOT_NULL(ut_params->ol,
			"Failed to allocate pktmbuf offload");

	ut_params->op = &ut_params->ol->op.crypto;


	/* Set crypto operation data parameters */
	rte_crypto_op_attach_session(ut_params->op, ut_params->sess);

	ut_params->op->digest.data = ut_params->digest;
	ut_params->op->digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, QUOTE_512_BYTES);
	ut_params->op->digest.length = DIGEST_BYTE_LENGTH_SHA512;

	ut_params->op->iv.data = (uint8_t *)rte_pktmbuf_prepend(ut_params->ibuf,
			CIPHER_IV_LENGTH_AES_CBC);
	ut_params->op->iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	ut_params->op->iv.length = CIPHER_IV_LENGTH_AES_CBC;

	rte_memcpy(ut_params->op->iv.data, aes_cbc_iv,
			CIPHER_IV_LENGTH_AES_CBC);

	ut_params->op->data.to_cipher.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_cipher.length = QUOTE_512_BYTES;
	ut_params->op->data.to_hash.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_hash.length = QUOTE_512_BYTES;

	rte_pktmbuf_offload_attach(ut_params->ibuf, ut_params->ol);

	/* Process crypto operation */
	ut_params->obuf = process_crypto_request(ts_params->valid_devs[0],
			ut_params->ibuf);
	TEST_ASSERT_NOT_NULL(ut_params->obuf, "failed to retrieve obuf");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->obuf, uint8_t *) +
			CIPHER_IV_LENGTH_AES_CBC,
			catch_22_quote_2_512_bytes_AES_CBC_ciphertext,
			QUOTE_512_BYTES,
			"Ciphertext data not as expected");

	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->obuf, uint8_t *) +
			CIPHER_IV_LENGTH_AES_CBC + QUOTE_512_BYTES,
			catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA512_digest,
			gbl_cryptodev_type == RTE_CRYPTODEV_AESNI_MB_PMD ?
					TRUNCATED_DIGEST_BYTE_LENGTH_SHA512 :
					DIGEST_BYTE_LENGTH_SHA512,
			"Generated digest data not as expected");


	return TEST_SUCCESS;
}


static int
test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(
		struct crypto_unittest_params *ut_params);

static int
test_AES_CBC_HMAC_SHA512_decrypt_perform(struct rte_cryptodev_session *sess,
		struct crypto_unittest_params *ut_params,
		struct crypto_testsuite_params *ts_params);

static int
test_AES_CBC_HMAC_SHA512_decrypt_digest_verify(void)
{
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct crypto_testsuite_params *ts_params = &testsuite_params;

	TEST_ASSERT(test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(
			ut_params) == TEST_SUCCESS,
			"Failed to create session params");

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_session_create(ts_params->valid_devs[0],
			&ut_params->auth_xform);
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	return test_AES_CBC_HMAC_SHA512_decrypt_perform(ut_params->sess,
			ut_params, ts_params);
}

static int
test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(
		struct crypto_unittest_params *ut_params)
{

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
	ut_params->cipher_xform.cipher.key.data = aes_cbc_key;
	ut_params->cipher_xform.cipher.key.length = CIPHER_KEY_LENGTH_AES_CBC;

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_XFORM_AUTH;
	ut_params->auth_xform.next = &ut_params->cipher_xform;

	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA512_HMAC;
	ut_params->auth_xform.auth.key.data = hmac_sha512_key;
	ut_params->auth_xform.auth.key.length = HMAC_KEY_LENGTH_SHA512;
	ut_params->auth_xform.auth.digest_length = DIGEST_BYTE_LENGTH_SHA512;
	return TEST_SUCCESS;
}


static int
test_AES_CBC_HMAC_SHA512_decrypt_perform(struct rte_cryptodev_session *sess,
		struct crypto_unittest_params *ut_params,
		struct crypto_testsuite_params *ts_params)
{
	/* Generate test mbuf data and digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			(const char *)
			catch_22_quote_2_512_bytes_AES_CBC_ciphertext,
			QUOTE_512_BYTES, 0);

	ut_params->digest = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			DIGEST_BYTE_LENGTH_SHA512);
	TEST_ASSERT_NOT_NULL(ut_params->digest, "no room to append digest");

	rte_memcpy(ut_params->digest,
			catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA512_digest,
			DIGEST_BYTE_LENGTH_SHA512);

	/* Generate Crypto op data structure */
	ut_params->ol = rte_pktmbuf_offload_alloc(ts_params->mbuf_ol_pool,
				RTE_PKTMBUF_OL_CRYPTO);
	TEST_ASSERT_NOT_NULL(ut_params->ol,
			"Failed to allocate pktmbuf offload");

	ut_params->op = &ut_params->ol->op.crypto;


	/* Set crypto operation data parameters */
	rte_crypto_op_attach_session(ut_params->op, sess);

	ut_params->op->digest.data = ut_params->digest;
	ut_params->op->digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, QUOTE_512_BYTES);
	ut_params->op->digest.length = DIGEST_BYTE_LENGTH_SHA512;

	ut_params->op->iv.data = (uint8_t *)rte_pktmbuf_prepend(
			ut_params->ibuf, CIPHER_IV_LENGTH_AES_CBC);
	ut_params->op->iv.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, 0);
	ut_params->op->iv.length = CIPHER_IV_LENGTH_AES_CBC;

	rte_memcpy(ut_params->op->iv.data, aes_cbc_iv,
			CIPHER_IV_LENGTH_AES_CBC);

	ut_params->op->data.to_cipher.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_cipher.length = QUOTE_512_BYTES;

	ut_params->op->data.to_hash.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_hash.length = QUOTE_512_BYTES;

	rte_pktmbuf_offload_attach(ut_params->ibuf, ut_params->ol);

	/* Process crypto operation */
	ut_params->obuf = process_crypto_request(ts_params->valid_devs[0],
			ut_params->ibuf);
	TEST_ASSERT_NOT_NULL(ut_params->obuf, "failed to retrieve obuf");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->obuf, uint8_t *) +
			CIPHER_IV_LENGTH_AES_CBC, catch_22_quote,
			QUOTE_512_BYTES,
			"Plaintext data not as expected");

	/* Validate obuf */
	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"Digest verification failed");



	return TEST_SUCCESS;
}

/* ***** AES-CBC / HMAC-AES_XCBC Chain Tests ***** */

static uint8_t aes_cbc_hmac_aes_xcbc_key[] = {
	0x87, 0x61, 0x54, 0x53, 0xC4, 0x6D, 0xDD, 0x51,
	0xE1, 0x9F, 0x86, 0x64, 0x39, 0x0A, 0xE6, 0x59
	};

static const uint8_t  catch_22_quote_2_512_bytes_HMAC_AES_XCBC_digest[] = {
	0xE0, 0xAC, 0x9A, 0xC4, 0x22, 0x64, 0x35, 0x89,
	0x77, 0x1D, 0x8B, 0x75
	};

static int
test_AES_CBC_HMAC_AES_XCBC_encrypt_digest(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Generate test mbuf data and space for digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			catch_22_quote, QUOTE_512_BYTES, 0);

	ut_params->digest = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			DIGEST_BYTE_LENGTH_AES_XCBC);
	TEST_ASSERT_NOT_NULL(ut_params->digest, "no room to append digest");

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_XFORM_CIPHER;
	ut_params->cipher_xform.next = &ut_params->auth_xform;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
	ut_params->cipher_xform.cipher.key.data = aes_cbc_key;
	ut_params->cipher_xform.cipher.key.length = CIPHER_KEY_LENGTH_AES_CBC;

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_AES_XCBC_MAC;
	ut_params->auth_xform.auth.key.length = AES_XCBC_MAC_KEY_SZ;
	ut_params->auth_xform.auth.key.data = aes_cbc_hmac_aes_xcbc_key;
	ut_params->auth_xform.auth.digest_length = DIGEST_BYTE_LENGTH_AES_XCBC;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_session_create(ts_params->valid_devs[0],
			&ut_params->cipher_xform);
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	/* Generate Crypto op data structure */
	ut_params->ol = rte_pktmbuf_offload_alloc(ts_params->mbuf_ol_pool,
				RTE_PKTMBUF_OL_CRYPTO);
	TEST_ASSERT_NOT_NULL(ut_params->ol,
			"Failed to allocate pktmbuf offload");

	ut_params->op = &ut_params->ol->op.crypto;


	/* Set crypto operation data parameters */
	rte_crypto_op_attach_session(ut_params->op, ut_params->sess);

	ut_params->op->iv.data = (uint8_t *)
		rte_pktmbuf_prepend(ut_params->ibuf,
				CIPHER_IV_LENGTH_AES_CBC);
	ut_params->op->iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	ut_params->op->iv.length = CIPHER_IV_LENGTH_AES_CBC;

	rte_memcpy(ut_params->op->iv.data, aes_cbc_iv,
			CIPHER_IV_LENGTH_AES_CBC);

	ut_params->op->data.to_cipher.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_cipher.length = QUOTE_512_BYTES;
	ut_params->op->data.to_hash.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_hash.length = QUOTE_512_BYTES;

	rte_pktmbuf_offload_attach(ut_params->ibuf, ut_params->ol);

	/* Process crypto operation */
	ut_params->obuf = process_crypto_request(ts_params->valid_devs[0],
			ut_params->ibuf);
	TEST_ASSERT_NOT_NULL(ut_params->obuf, "failed to retrieve obuf");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->obuf, uint8_t *) +
			CIPHER_IV_LENGTH_AES_CBC,
			catch_22_quote_2_512_bytes_AES_CBC_ciphertext,
			QUOTE_512_BYTES,
			"Ciphertext data not as expected");
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->obuf, uint8_t *) +
			CIPHER_IV_LENGTH_AES_CBC + QUOTE_512_BYTES,
			catch_22_quote_2_512_bytes_HMAC_AES_XCBC_digest,
			DIGEST_BYTE_LENGTH_AES_XCBC,
			"Generated digest data not as expected");

	return TEST_SUCCESS;
}

static int
test_AES_CBC_HMAC_AES_XCBC_decrypt_digest_verify(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Generate test mbuf data and space for digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
		(const char *)catch_22_quote_2_512_bytes_AES_CBC_ciphertext,
		QUOTE_512_BYTES, 0);

	ut_params->digest = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			DIGEST_BYTE_LENGTH_AES_XCBC);
	TEST_ASSERT_NOT_NULL(ut_params->digest, "no room to append digest");

	rte_memcpy(ut_params->digest,
			catch_22_quote_2_512_bytes_HMAC_AES_XCBC_digest,
			DIGEST_BYTE_LENGTH_AES_XCBC);

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
	ut_params->cipher_xform.cipher.key.data = aes_cbc_key;
	ut_params->cipher_xform.cipher.key.length = CIPHER_KEY_LENGTH_AES_CBC;

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_XFORM_AUTH;
	ut_params->auth_xform.next = &ut_params->cipher_xform;

	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_AES_XCBC_MAC;
	ut_params->auth_xform.auth.key.length = AES_XCBC_MAC_KEY_SZ;
	ut_params->auth_xform.auth.key.data = aes_cbc_hmac_aes_xcbc_key;
	ut_params->auth_xform.auth.digest_length = DIGEST_BYTE_LENGTH_AES_XCBC;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_session_create(ts_params->valid_devs[0],
			&ut_params->auth_xform);
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	/* Generate Crypto op data structure */
	ut_params->ol = rte_pktmbuf_offload_alloc(ts_params->mbuf_ol_pool,
				RTE_PKTMBUF_OL_CRYPTO);
	TEST_ASSERT_NOT_NULL(ut_params->ol,
			"Failed to allocate pktmbuf offload");

	ut_params->op = &ut_params->ol->op.crypto;


	/* Set crypto operation data parameters */
	rte_crypto_op_attach_session(ut_params->op, ut_params->sess);

	ut_params->op->iv.data = (uint8_t *)rte_pktmbuf_prepend(ut_params->ibuf,
			CIPHER_IV_LENGTH_AES_CBC);
	ut_params->op->iv.phys_addr = rte_pktmbuf_mtophys(ut_params->ibuf);
	ut_params->op->iv.length = CIPHER_IV_LENGTH_AES_CBC;

	rte_memcpy(ut_params->op->iv.data, aes_cbc_iv,
			CIPHER_IV_LENGTH_AES_CBC);

	ut_params->op->data.to_cipher.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_cipher.length = QUOTE_512_BYTES;
	ut_params->op->data.to_hash.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_hash.length = QUOTE_512_BYTES;
	rte_pktmbuf_offload_attach(ut_params->ibuf, ut_params->ol);

	/* Process crypto operation */
	ut_params->obuf = process_crypto_request(ts_params->valid_devs[0],
			ut_params->ibuf);
	TEST_ASSERT_NOT_NULL(ut_params->obuf, "failed to retrieve obuf");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->obuf, uint8_t *) +
			CIPHER_IV_LENGTH_AES_CBC, catch_22_quote,
			QUOTE_512_BYTES,
			"Ciphertext data not as expected");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"Digest verification failed");

	return TEST_SUCCESS;
}


/* ***** AES-GCM Tests ***** */

static int
test_stats(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_stats stats;
	struct rte_cryptodev *dev;
	cryptodev_stats_get_t temp_pfn;

	rte_cryptodev_stats_reset(ts_params->valid_devs[0]);
	TEST_ASSERT((rte_cryptodev_stats_get(ts_params->valid_devs[0] + 600,
			&stats) == -ENODEV),
		"rte_cryptodev_stats_get invalid dev failed");
	TEST_ASSERT((rte_cryptodev_stats_get(ts_params->valid_devs[0], 0) != 0),
		"rte_cryptodev_stats_get invalid Param failed");
	dev = &rte_cryptodevs[ts_params->valid_devs[0]];
	temp_pfn = dev->dev_ops->stats_get;
	dev->dev_ops->stats_get = (cryptodev_stats_get_t)0;
	TEST_ASSERT((rte_cryptodev_stats_get(ts_params->valid_devs[0], &stats)
			== -ENOTSUP),
		"rte_cryptodev_stats_get invalid Param failed");
	dev->dev_ops->stats_get = temp_pfn;

	/* Test expected values */
	ut_setup();
	test_AES_CBC_HMAC_SHA1_encrypt_digest();
	ut_teardown();
	TEST_ASSERT_SUCCESS(rte_cryptodev_stats_get(ts_params->valid_devs[0],
			&stats),
		"rte_cryptodev_stats_get failed");
	TEST_ASSERT((stats.enqueued_count == 1),
		"rte_cryptodev_stats_get returned unexpected enqueued stat");
	TEST_ASSERT((stats.dequeued_count == 1),
		"rte_cryptodev_stats_get returned unexpected enqueued stat");
	TEST_ASSERT((stats.enqueue_err_count == 0),
		"rte_cryptodev_stats_get returned unexpected enqueued stat");
	TEST_ASSERT((stats.dequeue_err_count == 0),
		"rte_cryptodev_stats_get returned unexpected enqueued stat");

	/* invalid device but should ignore and not reset device stats*/
	rte_cryptodev_stats_reset(ts_params->valid_devs[0] + 300);
	TEST_ASSERT_SUCCESS(rte_cryptodev_stats_get(ts_params->valid_devs[0],
			&stats),
		"rte_cryptodev_stats_get failed");
	TEST_ASSERT((stats.enqueued_count == 1),
		"rte_cryptodev_stats_get returned unexpected enqueued stat");

	/* check that a valid reset clears stats */
	rte_cryptodev_stats_reset(ts_params->valid_devs[0]);
	TEST_ASSERT_SUCCESS(rte_cryptodev_stats_get(ts_params->valid_devs[0],
			&stats),
					  "rte_cryptodev_stats_get failed");
	TEST_ASSERT((stats.enqueued_count == 0),
		"rte_cryptodev_stats_get returned unexpected enqueued stat");
	TEST_ASSERT((stats.dequeued_count == 0),
		"rte_cryptodev_stats_get returned unexpected enqueued stat");

	return TEST_SUCCESS;
}


static int
test_multi_session(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	struct rte_cryptodev_info dev_info;
	struct rte_cryptodev_session **sessions;

	uint16_t i;

	test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(ut_params);


	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	sessions = rte_malloc(NULL, (sizeof(struct rte_cryptodev_session *) *
			dev_info.max_nb_sessions) + 1, 0);

	/* Create multiple crypto sessions*/
	for (i = 0; i < dev_info.max_nb_sessions; i++) {
		sessions[i] = rte_cryptodev_session_create(
				ts_params->valid_devs[0],
			&ut_params->auth_xform);
		TEST_ASSERT_NOT_NULL(sessions[i],
				"Session creation failed at session number %u",
				i);

		/* Attempt to send a request on each session */
		TEST_ASSERT_SUCCESS(test_AES_CBC_HMAC_SHA512_decrypt_perform(
				sessions[i], ut_params, ts_params),
				"Failed to perform decrypt on request "
				"number %u.", i);
	}

	/* Next session create should fail */
	sessions[i] = rte_cryptodev_session_create(ts_params->valid_devs[0],
			&ut_params->auth_xform);
	TEST_ASSERT_NULL(sessions[i],
			"Session creation succeeded unexpectedly!");

	for (i = 0; i < dev_info.max_nb_sessions; i++)
		rte_cryptodev_session_free(ts_params->valid_devs[0],
				sessions[i]);

	rte_free(sessions);

	return TEST_SUCCESS;
}

static int
test_not_in_place_crypto(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct rte_mbuf *dst_m = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(ut_params);

	/* Create multiple crypto sessions*/

	ut_params->sess = rte_cryptodev_session_create(
			ts_params->valid_devs[0], &ut_params->auth_xform);

	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");


	/* Generate test mbuf data and digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			(const char *)
			catch_22_quote_2_512_bytes_AES_CBC_ciphertext,
			QUOTE_512_BYTES, 0);

	ut_params->digest = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			DIGEST_BYTE_LENGTH_SHA512);
	TEST_ASSERT_NOT_NULL(ut_params->digest, "no room to append digest");

	rte_memcpy(ut_params->digest,
			catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA512_digest,
			DIGEST_BYTE_LENGTH_SHA512);

	/* Generate Crypto op data structure */
	ut_params->ol = rte_pktmbuf_offload_alloc(ts_params->mbuf_ol_pool,
				RTE_PKTMBUF_OL_CRYPTO);
	TEST_ASSERT_NOT_NULL(ut_params->ol,
			"Failed to allocate pktmbuf offload");

	ut_params->op = &ut_params->ol->op.crypto;


	/* Set crypto operation data parameters */
	rte_crypto_op_attach_session(ut_params->op, ut_params->sess);

	ut_params->op->digest.data = ut_params->digest;
	ut_params->op->digest.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, QUOTE_512_BYTES);
	ut_params->op->digest.length = DIGEST_BYTE_LENGTH_SHA512;

	ut_params->op->iv.data = (uint8_t *)rte_pktmbuf_prepend(
			ut_params->ibuf, CIPHER_IV_LENGTH_AES_CBC);
	ut_params->op->iv.phys_addr = rte_pktmbuf_mtophys_offset(
			ut_params->ibuf, 0);
	ut_params->op->iv.length = CIPHER_IV_LENGTH_AES_CBC;

	rte_memcpy(ut_params->op->iv.data, aes_cbc_iv,
			CIPHER_IV_LENGTH_AES_CBC);

	ut_params->op->data.to_cipher.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_cipher.length = QUOTE_512_BYTES;

	ut_params->op->data.to_hash.offset = CIPHER_IV_LENGTH_AES_CBC;
	ut_params->op->data.to_hash.length = QUOTE_512_BYTES;

	ut_params->op->dst.m = dst_m;
	ut_params->op->dst.offset = 0;

	rte_pktmbuf_offload_attach(ut_params->ibuf, ut_params->ol);

	/* Process crypto operation */
	ut_params->obuf = process_crypto_request(ts_params->valid_devs[0],
			ut_params->ibuf);
	TEST_ASSERT_NOT_NULL(ut_params->obuf, "failed to retrieve obuf");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->op->dst.m, char *),
			catch_22_quote,
			QUOTE_512_BYTES,
			"Plaintext data not as expected");

	/* Validate obuf */

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"Digest verification failed");

	return TEST_SUCCESS;
}


static struct unit_test_suite cryptodev_qat_testsuite  = {
	.suite_name = "Crypto QAT Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_device_configure_invalid_dev_id),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_device_configure_invalid_queue_pair_ids),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_queue_pair_descriptor_setup),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_multi_session),

		TEST_CASE_ST(ut_setup, ut_teardown,
				test_AES_CBC_HMAC_SHA1_encrypt_digest),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_AES_CBC_HMAC_SHA1_decrypt_digest_verify),

		TEST_CASE_ST(ut_setup, ut_teardown,
				test_AES_CBC_HMAC_SHA256_encrypt_digest),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_AES_CBC_HMAC_SHA256_decrypt_digest_verify),

		TEST_CASE_ST(ut_setup, ut_teardown,
				test_AES_CBC_HMAC_SHA512_encrypt_digest),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_AES_CBC_HMAC_SHA512_decrypt_digest_verify),

		TEST_CASE_ST(ut_setup, ut_teardown,
				test_AES_CBC_HMAC_AES_XCBC_encrypt_digest),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_AES_CBC_HMAC_AES_XCBC_decrypt_digest_verify),

		TEST_CASE_ST(ut_setup, ut_teardown, test_stats),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite cryptodev_aesni_mb_testsuite  = {
	.suite_name = "Crypto Device AESNI MB Unit Test Suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CBC_HMAC_SHA1_encrypt_digest),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CBC_HMAC_SHA1_decrypt_digest_verify),

		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CBC_HMAC_SHA256_encrypt_digest),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CBC_HMAC_SHA256_decrypt_digest_verify),

		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CBC_HMAC_SHA512_encrypt_digest),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CBC_HMAC_SHA512_decrypt_digest_verify),

		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CBC_HMAC_AES_XCBC_encrypt_digest),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CBC_HMAC_AES_XCBC_decrypt_digest_verify),

		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CBC_HMAC_SHA1_encrypt_digest_sessionless),

		TEST_CASE_ST(ut_setup, ut_teardown,
			test_not_in_place_crypto),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_cryptodev_qat(void /*argv __rte_unused, int argc __rte_unused*/)
{
	gbl_cryptodev_type = RTE_CRYPTODEV_QAT_PMD;
	return unit_test_suite_runner(&cryptodev_qat_testsuite);
}
static struct test_command cryptodev_qat_cmd = {
	.command = "cryptodev_qat_autotest",
	.callback = test_cryptodev_qat,
};

static int
test_cryptodev_aesni_mb(void /*argv __rte_unused, int argc __rte_unused*/)
{
	gbl_cryptodev_type = RTE_CRYPTODEV_AESNI_MB_PMD;

	return unit_test_suite_runner(&cryptodev_aesni_mb_testsuite);
}

static struct test_command cryptodev_aesni_mb_cmd = {
	.command = "cryptodev_aesni_mb_autotest",
	.callback = test_cryptodev_aesni_mb,
};

REGISTER_TEST_COMMAND(cryptodev_qat_cmd);
REGISTER_TEST_COMMAND(cryptodev_aesni_mb_cmd);
