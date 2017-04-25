/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>

#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_cryptodev.h>
#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_interrupts.h>
#include <rte_ip.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_memzone.h>
#include <rte_pci.h>
#include <rte_per_lcore.h>
#include <rte_prefetch.h>
#include <rte_random.h>
#include <rte_hexdump.h>

enum cdev_type {
	CDEV_TYPE_ANY,
	CDEV_TYPE_HW,
	CDEV_TYPE_SW
};

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define NB_MBUF   8192

#define MAX_STR_LEN 32
#define MAX_KEY_SIZE 128
#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512

static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
static uint64_t l2fwd_enabled_port_mask;
static uint64_t l2fwd_enabled_crypto_mask;

/* list of enabled ports */
static uint32_t l2fwd_dst_ports[RTE_MAX_ETHPORTS];


struct pkt_buffer {
	unsigned len;
	struct rte_mbuf *buffer[MAX_PKT_BURST];
};

struct op_buffer {
	unsigned len;
	struct rte_crypto_op *buffer[MAX_PKT_BURST];
};

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16

enum l2fwd_crypto_xform_chain {
	L2FWD_CRYPTO_CIPHER_HASH,
	L2FWD_CRYPTO_HASH_CIPHER,
	L2FWD_CRYPTO_CIPHER_ONLY,
	L2FWD_CRYPTO_HASH_ONLY
};

struct l2fwd_key {
	uint8_t *data;
	uint32_t length;
	phys_addr_t phys_addr;
};

/** l2fwd crypto application command line options */
struct l2fwd_crypto_options {
	unsigned portmask;
	unsigned nb_ports_per_lcore;
	unsigned refresh_period;
	unsigned single_lcore:1;

	enum cdev_type type;
	unsigned sessionless:1;

	enum l2fwd_crypto_xform_chain xform_chain;

	struct rte_crypto_sym_xform cipher_xform;
	unsigned ckey_param;
	int ckey_random_size;

	struct l2fwd_key iv;
	unsigned iv_param;
	int iv_random_size;

	struct rte_crypto_sym_xform auth_xform;
	uint8_t akey_param;
	int akey_random_size;

	struct l2fwd_key aad;
	unsigned aad_param;
	int aad_random_size;

	int digest_size;

	uint16_t block_size;
	char string_type[MAX_STR_LEN];

	uint64_t cryptodev_mask;
};

/** l2fwd crypto lcore params */
struct l2fwd_crypto_params {
	uint8_t dev_id;
	uint8_t qp_id;

	unsigned digest_length;
	unsigned block_size;

	struct l2fwd_key iv;
	struct l2fwd_key aad;
	struct rte_cryptodev_sym_session *session;

	uint8_t do_cipher;
	uint8_t do_hash;
	uint8_t hash_verify;

	enum rte_crypto_cipher_algorithm cipher_algo;
	enum rte_crypto_auth_algorithm auth_algo;
};

/** lcore configuration */
struct lcore_queue_conf {
	unsigned nb_rx_ports;
	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];

	unsigned nb_crypto_devs;
	unsigned cryptodev_list[MAX_RX_QUEUE_PER_LCORE];

	struct op_buffer op_buf[RTE_CRYPTO_MAX_DEVS];
	struct pkt_buffer pkt_buf[RTE_MAX_ETHPORTS];
} __rte_cache_aligned;

struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = ETH_MQ_RX_NONE,
		.max_rx_pkt_len = ETHER_MAX_LEN,
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 1, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

struct rte_mempool *l2fwd_pktmbuf_pool;
struct rte_mempool *l2fwd_crypto_op_pool;

/* Per-port statistics struct */
struct l2fwd_port_statistics {
	uint64_t tx;
	uint64_t rx;

	uint64_t crypto_enqueued;
	uint64_t crypto_dequeued;

	uint64_t dropped;
} __rte_cache_aligned;

struct l2fwd_crypto_statistics {
	uint64_t enqueued;
	uint64_t dequeued;

	uint64_t errors;
} __rte_cache_aligned;

struct l2fwd_port_statistics port_statistics[RTE_MAX_ETHPORTS];
struct l2fwd_crypto_statistics crypto_statistics[RTE_CRYPTO_MAX_DEVS];

/* A tsc-based timer responsible for triggering statistics printout */
#define TIMER_MILLISECOND 2000000ULL /* around 1ms at 2 Ghz */
#define MAX_TIMER_PERIOD 86400UL /* 1 day max */

/* default period is 10 seconds */
static int64_t timer_period = 10 * TIMER_MILLISECOND * 1000;

/* Print out statistics on packets dropped */
static void
print_stats(void)
{
	uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
	uint64_t total_packets_enqueued, total_packets_dequeued,
		total_packets_errors;
	unsigned portid;
	uint64_t cdevid;

	total_packets_dropped = 0;
	total_packets_tx = 0;
	total_packets_rx = 0;
	total_packets_enqueued = 0;
	total_packets_dequeued = 0;
	total_packets_errors = 0;

	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

		/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("\nPort statistics ====================================");

	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
		/* skip disabled ports */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;
		printf("\nStatistics for port %u ------------------------------"
			   "\nPackets sent: %32"PRIu64
			   "\nPackets received: %28"PRIu64
			   "\nPackets dropped: %29"PRIu64,
			   portid,
			   port_statistics[portid].tx,
			   port_statistics[portid].rx,
			   port_statistics[portid].dropped);

		total_packets_dropped += port_statistics[portid].dropped;
		total_packets_tx += port_statistics[portid].tx;
		total_packets_rx += port_statistics[portid].rx;
	}
	printf("\nCrypto statistics ==================================");

	for (cdevid = 0; cdevid < RTE_CRYPTO_MAX_DEVS; cdevid++) {
		/* skip disabled ports */
		if ((l2fwd_enabled_crypto_mask & (((uint64_t)1) << cdevid)) == 0)
			continue;
		printf("\nStatistics for cryptodev %"PRIu64
				" -------------------------"
			   "\nPackets enqueued: %28"PRIu64
			   "\nPackets dequeued: %28"PRIu64
			   "\nPackets errors: %30"PRIu64,
			   cdevid,
			   crypto_statistics[cdevid].enqueued,
			   crypto_statistics[cdevid].dequeued,
			   crypto_statistics[cdevid].errors);

		total_packets_enqueued += crypto_statistics[cdevid].enqueued;
		total_packets_dequeued += crypto_statistics[cdevid].dequeued;
		total_packets_errors += crypto_statistics[cdevid].errors;
	}
	printf("\nAggregate statistics ==============================="
		   "\nTotal packets received: %22"PRIu64
		   "\nTotal packets enqueued: %22"PRIu64
		   "\nTotal packets dequeued: %22"PRIu64
		   "\nTotal packets sent: %26"PRIu64
		   "\nTotal packets dropped: %23"PRIu64
		   "\nTotal packets crypto errors: %17"PRIu64,
		   total_packets_rx,
		   total_packets_enqueued,
		   total_packets_dequeued,
		   total_packets_tx,
		   total_packets_dropped,
		   total_packets_errors);
	printf("\n====================================================\n");
}

static int
l2fwd_crypto_send_burst(struct lcore_queue_conf *qconf, unsigned n,
		struct l2fwd_crypto_params *cparams)
{
	struct rte_crypto_op **op_buffer;
	unsigned ret;

	op_buffer = (struct rte_crypto_op **)
			qconf->op_buf[cparams->dev_id].buffer;

	ret = rte_cryptodev_enqueue_burst(cparams->dev_id,
			cparams->qp_id,	op_buffer, (uint16_t) n);

	crypto_statistics[cparams->dev_id].enqueued += ret;
	if (unlikely(ret < n)) {
		crypto_statistics[cparams->dev_id].errors += (n - ret);
		do {
			rte_pktmbuf_free(op_buffer[ret]->sym->m_src);
			rte_crypto_op_free(op_buffer[ret]);
		} while (++ret < n);
	}

	return 0;
}

static int
l2fwd_crypto_enqueue(struct rte_crypto_op *op,
		struct l2fwd_crypto_params *cparams)
{
	unsigned lcore_id, len;
	struct lcore_queue_conf *qconf;

	lcore_id = rte_lcore_id();

	qconf = &lcore_queue_conf[lcore_id];
	len = qconf->op_buf[cparams->dev_id].len;
	qconf->op_buf[cparams->dev_id].buffer[len] = op;
	len++;

	/* enough ops to be sent */
	if (len == MAX_PKT_BURST) {
		l2fwd_crypto_send_burst(qconf, MAX_PKT_BURST, cparams);
		len = 0;
	}

	qconf->op_buf[cparams->dev_id].len = len;
	return 0;
}

static int
l2fwd_simple_crypto_enqueue(struct rte_mbuf *m,
		struct rte_crypto_op *op,
		struct l2fwd_crypto_params *cparams)
{
	struct ether_hdr *eth_hdr;
	struct ipv4_hdr *ip_hdr;

	uint32_t ipdata_offset, data_len;
	uint32_t pad_len = 0;
	char *padding;

	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	if (eth_hdr->ether_type != rte_cpu_to_be_16(ETHER_TYPE_IPv4))
		return -1;

	ipdata_offset = sizeof(struct ether_hdr);

	ip_hdr = (struct ipv4_hdr *)(rte_pktmbuf_mtod(m, char *) +
			ipdata_offset);

	ipdata_offset += (ip_hdr->version_ihl & IPV4_HDR_IHL_MASK)
			* IPV4_IHL_MULTIPLIER;


	/* Zero pad data to be crypto'd so it is block aligned */
	data_len  = rte_pktmbuf_data_len(m) - ipdata_offset;

	if (cparams->do_hash && cparams->hash_verify)
		data_len -= cparams->digest_length;

	if (cparams->do_cipher) {
		/*
		 * Following algorithms are block cipher algorithms,
		 * and might need padding
		 */
		switch (cparams->cipher_algo) {
		case RTE_CRYPTO_CIPHER_AES_CBC:
		case RTE_CRYPTO_CIPHER_AES_ECB:
		case RTE_CRYPTO_CIPHER_DES_CBC:
		case RTE_CRYPTO_CIPHER_3DES_CBC:
		case RTE_CRYPTO_CIPHER_3DES_ECB:
			if (data_len % cparams->block_size)
				pad_len = cparams->block_size -
					(data_len % cparams->block_size);
			break;
		default:
			pad_len = 0;
		}

		if (pad_len) {
			padding = rte_pktmbuf_append(m, pad_len);
			if (unlikely(!padding))
				return -1;

			data_len += pad_len;
			memset(padding, 0, pad_len);
		}
	}

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(op, cparams->session);

	if (cparams->do_hash) {
		if (!cparams->hash_verify) {
			/* Append space for digest to end of packet */
			op->sym->auth.digest.data = (uint8_t *)rte_pktmbuf_append(m,
				cparams->digest_length);
		} else {
			op->sym->auth.digest.data = rte_pktmbuf_mtod(m,
				uint8_t *) + ipdata_offset + data_len;
		}

		op->sym->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(m,
				rte_pktmbuf_pkt_len(m) - cparams->digest_length);
		op->sym->auth.digest.length = cparams->digest_length;

		/* For wireless algorithms, offset/length must be in bits */
		if (cparams->auth_algo == RTE_CRYPTO_AUTH_SNOW3G_UIA2 ||
				cparams->auth_algo == RTE_CRYPTO_AUTH_KASUMI_F9 ||
				cparams->auth_algo == RTE_CRYPTO_AUTH_ZUC_EIA3) {
			op->sym->auth.data.offset = ipdata_offset << 3;
			op->sym->auth.data.length = data_len << 3;
		} else {
			op->sym->auth.data.offset = ipdata_offset;
			op->sym->auth.data.length = data_len;
		}

		if (cparams->aad.length) {
			op->sym->auth.aad.data = cparams->aad.data;
			op->sym->auth.aad.phys_addr = cparams->aad.phys_addr;
			op->sym->auth.aad.length = cparams->aad.length;
		} else {
			op->sym->auth.aad.data = NULL;
			op->sym->auth.aad.phys_addr = 0;
			op->sym->auth.aad.length = 0;
		}
	}

	if (cparams->do_cipher) {
		op->sym->cipher.iv.data = cparams->iv.data;
		op->sym->cipher.iv.phys_addr = cparams->iv.phys_addr;
		op->sym->cipher.iv.length = cparams->iv.length;

		/* For wireless algorithms, offset/length must be in bits */
		if (cparams->cipher_algo == RTE_CRYPTO_CIPHER_SNOW3G_UEA2 ||
				cparams->cipher_algo == RTE_CRYPTO_CIPHER_KASUMI_F8 ||
				cparams->cipher_algo == RTE_CRYPTO_CIPHER_ZUC_EEA3) {
			op->sym->cipher.data.offset = ipdata_offset << 3;
			op->sym->cipher.data.length = data_len << 3;
		} else {
			op->sym->cipher.data.offset = ipdata_offset;
			op->sym->cipher.data.length = data_len;
		}
	}

	op->sym->m_src = m;

	return l2fwd_crypto_enqueue(op, cparams);
}


/* Send the burst of packets on an output interface */
static int
l2fwd_send_burst(struct lcore_queue_conf *qconf, unsigned n,
		uint8_t port)
{
	struct rte_mbuf **pkt_buffer;
	unsigned ret;

	pkt_buffer = (struct rte_mbuf **)qconf->pkt_buf[port].buffer;

	ret = rte_eth_tx_burst(port, 0, pkt_buffer, (uint16_t)n);
	port_statistics[port].tx += ret;
	if (unlikely(ret < n)) {
		port_statistics[port].dropped += (n - ret);
		do {
			rte_pktmbuf_free(pkt_buffer[ret]);
		} while (++ret < n);
	}

	return 0;
}

/* Enqueue packets for TX and prepare them to be sent */
static int
l2fwd_send_packet(struct rte_mbuf *m, uint8_t port)
{
	unsigned lcore_id, len;
	struct lcore_queue_conf *qconf;

	lcore_id = rte_lcore_id();

	qconf = &lcore_queue_conf[lcore_id];
	len = qconf->pkt_buf[port].len;
	qconf->pkt_buf[port].buffer[len] = m;
	len++;

	/* enough pkts to be sent */
	if (unlikely(len == MAX_PKT_BURST)) {
		l2fwd_send_burst(qconf, MAX_PKT_BURST, port);
		len = 0;
	}

	qconf->pkt_buf[port].len = len;
	return 0;
}

static void
l2fwd_simple_forward(struct rte_mbuf *m, unsigned portid)
{
	struct ether_hdr *eth;
	void *tmp;
	unsigned dst_port;

	dst_port = l2fwd_dst_ports[portid];
	eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

	/* 02:00:00:00:00:xx */
	tmp = &eth->d_addr.addr_bytes[0];
	*((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)dst_port << 40);

	/* src addr */
	ether_addr_copy(&l2fwd_ports_eth_addr[dst_port], &eth->s_addr);

	l2fwd_send_packet(m, (uint8_t) dst_port);
}

/** Generate random key */
static void
generate_random_key(uint8_t *key, unsigned length)
{
	int fd;
	int ret;

	fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		rte_exit(EXIT_FAILURE, "Failed to generate random key\n");

	ret = read(fd, key, length);
	close(fd);

	if (ret != (signed)length)
		rte_exit(EXIT_FAILURE, "Failed to generate random key\n");
}

static struct rte_cryptodev_sym_session *
initialize_crypto_session(struct l2fwd_crypto_options *options,
		uint8_t cdev_id)
{
	struct rte_crypto_sym_xform *first_xform;

	if (options->xform_chain == L2FWD_CRYPTO_CIPHER_HASH) {
		first_xform = &options->cipher_xform;
		first_xform->next = &options->auth_xform;
	} else if (options->xform_chain == L2FWD_CRYPTO_HASH_CIPHER) {
		first_xform = &options->auth_xform;
		first_xform->next = &options->cipher_xform;
	} else if (options->xform_chain == L2FWD_CRYPTO_CIPHER_ONLY) {
		first_xform = &options->cipher_xform;
	} else {
		first_xform = &options->auth_xform;
	}

	/* Setup Cipher Parameters */
	return rte_cryptodev_sym_session_create(cdev_id, first_xform);
}

static void
l2fwd_crypto_options_print(struct l2fwd_crypto_options *options);

/* main processing loop */
static void
l2fwd_main_loop(struct l2fwd_crypto_options *options)
{
	struct rte_mbuf *m, *pkts_burst[MAX_PKT_BURST];
	struct rte_crypto_op *ops_burst[MAX_PKT_BURST];

	unsigned lcore_id = rte_lcore_id();
	uint64_t prev_tsc = 0, diff_tsc, cur_tsc, timer_tsc = 0;
	unsigned i, j, portid, nb_rx, len;
	struct lcore_queue_conf *qconf = &lcore_queue_conf[lcore_id];
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) /
			US_PER_S * BURST_TX_DRAIN_US;
	struct l2fwd_crypto_params *cparams;
	struct l2fwd_crypto_params port_cparams[qconf->nb_crypto_devs];

	if (qconf->nb_rx_ports == 0) {
		RTE_LOG(INFO, L2FWD, "lcore %u has nothing to do\n", lcore_id);
		return;
	}

	RTE_LOG(INFO, L2FWD, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->nb_rx_ports; i++) {

		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
			portid);
	}

	for (i = 0; i < qconf->nb_crypto_devs; i++) {
		port_cparams[i].do_cipher = 0;
		port_cparams[i].do_hash = 0;

		switch (options->xform_chain) {
		case L2FWD_CRYPTO_CIPHER_HASH:
		case L2FWD_CRYPTO_HASH_CIPHER:
			port_cparams[i].do_cipher = 1;
			port_cparams[i].do_hash = 1;
			break;
		case L2FWD_CRYPTO_HASH_ONLY:
			port_cparams[i].do_hash = 1;
			break;
		case L2FWD_CRYPTO_CIPHER_ONLY:
			port_cparams[i].do_cipher = 1;
			break;
		}

		port_cparams[i].dev_id = qconf->cryptodev_list[i];
		port_cparams[i].qp_id = 0;

		port_cparams[i].block_size = options->block_size;

		if (port_cparams[i].do_hash) {
			port_cparams[i].digest_length =
					options->auth_xform.auth.digest_length;
			if (options->auth_xform.auth.add_auth_data_length) {
				port_cparams[i].aad.data = options->aad.data;
				port_cparams[i].aad.length =
					options->auth_xform.auth.add_auth_data_length;
				port_cparams[i].aad.phys_addr = options->aad.phys_addr;
				if (!options->aad_param)
					generate_random_key(port_cparams[i].aad.data,
						port_cparams[i].aad.length);

			} else
				port_cparams[i].aad.length = 0;

			if (options->auth_xform.auth.op == RTE_CRYPTO_AUTH_OP_VERIFY)
				port_cparams[i].hash_verify = 1;
			else
				port_cparams[i].hash_verify = 0;

			port_cparams[i].auth_algo = options->auth_xform.auth.algo;
		}

		if (port_cparams[i].do_cipher) {
			port_cparams[i].iv.data = options->iv.data;
			port_cparams[i].iv.length = options->iv.length;
			port_cparams[i].iv.phys_addr = options->iv.phys_addr;
			if (!options->iv_param)
				generate_random_key(port_cparams[i].iv.data,
						port_cparams[i].iv.length);

			port_cparams[i].cipher_algo = options->cipher_xform.cipher.algo;
		}

		port_cparams[i].session = initialize_crypto_session(options,
				port_cparams[i].dev_id);

		if (port_cparams[i].session == NULL)
			return;
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u cryptoid=%u\n", lcore_id,
				port_cparams[i].dev_id);
	}

	l2fwd_crypto_options_print(options);

	/*
	 * Initialize previous tsc timestamp before the loop,
	 * to avoid showing the port statistics immediately,
	 * so user can see the crypto information.
	 */
	prev_tsc = rte_rdtsc();
	while (1) {

		cur_tsc = rte_rdtsc();

		/*
		 * Crypto device/TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {
			/* Enqueue all crypto ops remaining in buffers */
			for (i = 0; i < qconf->nb_crypto_devs; i++) {
				cparams = &port_cparams[i];
				len = qconf->op_buf[cparams->dev_id].len;
				l2fwd_crypto_send_burst(qconf, len, cparams);
				qconf->op_buf[cparams->dev_id].len = 0;
			}
			/* Transmit all packets remaining in buffers */
			for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
				if (qconf->pkt_buf[portid].len == 0)
					continue;
				l2fwd_send_burst(&lcore_queue_conf[lcore_id],
						 qconf->pkt_buf[portid].len,
						 (uint8_t) portid);
				qconf->pkt_buf[portid].len = 0;
			}

			/* if timer is enabled */
			if (timer_period > 0) {

				/* advance the timer */
				timer_tsc += diff_tsc;

				/* if timer has reached its timeout */
				if (unlikely(timer_tsc >=
						(uint64_t)timer_period)) {

					/* do this only on master core */
					if (lcore_id == rte_get_master_lcore()
						&& options->refresh_period) {
						print_stats();
						timer_tsc = 0;
					}
				}
			}

			prev_tsc = cur_tsc;
		}

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->nb_rx_ports; i++) {
			portid = qconf->rx_port_list[i];

			cparams = &port_cparams[i];

			nb_rx = rte_eth_rx_burst((uint8_t) portid, 0,
						 pkts_burst, MAX_PKT_BURST);

			port_statistics[portid].rx += nb_rx;

			if (nb_rx) {
				/*
				 * If we can't allocate a crypto_ops, then drop
				 * the rest of the burst and dequeue and
				 * process the packets to free offload structs
				 */
				if (rte_crypto_op_bulk_alloc(
						l2fwd_crypto_op_pool,
						RTE_CRYPTO_OP_TYPE_SYMMETRIC,
						ops_burst, nb_rx) !=
								nb_rx) {
					for (j = 0; j < nb_rx; j++)
						rte_pktmbuf_free(pkts_burst[j]);

					nb_rx = 0;
				}

				/* Enqueue packets from Crypto device*/
				for (j = 0; j < nb_rx; j++) {
					m = pkts_burst[j];

					l2fwd_simple_crypto_enqueue(m,
							ops_burst[j], cparams);
				}
			}

			/* Dequeue packets from Crypto device */
			do {
				nb_rx = rte_cryptodev_dequeue_burst(
						cparams->dev_id, cparams->qp_id,
						ops_burst, MAX_PKT_BURST);

				crypto_statistics[cparams->dev_id].dequeued +=
						nb_rx;

				/* Forward crypto'd packets */
				for (j = 0; j < nb_rx; j++) {
					m = ops_burst[j]->sym->m_src;

					rte_crypto_op_free(ops_burst[j]);
					l2fwd_simple_forward(m, portid);
				}
			} while (nb_rx == MAX_PKT_BURST);
		}
	}
}

static int
l2fwd_launch_one_lcore(void *arg)
{
	l2fwd_main_loop((struct l2fwd_crypto_options *)arg);
	return 0;
}

/* Display command line arguments usage */
static void
l2fwd_crypto_usage(const char *prgname)
{
	printf("%s [EAL options] --\n"
		"  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
		"  -q NQ: number of queue (=ports) per lcore (default is 1)\n"
		"  -s manage all ports from single lcore\n"
		"  -T PERIOD: statistics will be refreshed each PERIOD seconds"
		" (0 to disable, 10 default, 86400 maximum)\n"

		"  --cdev_type HW / SW / ANY\n"
		"  --chain HASH_CIPHER / CIPHER_HASH\n"

		"  --cipher_algo ALGO\n"
		"  --cipher_op ENCRYPT / DECRYPT\n"
		"  --cipher_key KEY (bytes separated with \":\")\n"
		"  --cipher_key_random_size SIZE: size of cipher key when generated randomly\n"
		"  --iv IV (bytes separated with \":\")\n"
		"  --iv_random_size SIZE: size of IV when generated randomly\n"

		"  --auth_algo ALGO\n"
		"  --auth_op GENERATE / VERIFY\n"
		"  --auth_key KEY (bytes separated with \":\")\n"
		"  --auth_key_random_size SIZE: size of auth key when generated randomly\n"
		"  --aad AAD (bytes separated with \":\")\n"
		"  --aad_random_size SIZE: size of AAD when generated randomly\n"
		"  --digest_size SIZE: size of digest to be generated/verified\n"

		"  --sessionless\n"
		"  --cryptodev_mask MASK: hexadecimal bitmask of crypto devices to configure\n",
	       prgname);
}

/** Parse crypto device type command line argument */
static int
parse_cryptodev_type(enum cdev_type *type, char *optarg)
{
	if (strcmp("HW", optarg) == 0) {
		*type = CDEV_TYPE_HW;
		return 0;
	} else if (strcmp("SW", optarg) == 0) {
		*type = CDEV_TYPE_SW;
		return 0;
	} else if (strcmp("ANY", optarg) == 0) {
		*type = CDEV_TYPE_ANY;
		return 0;
	}

	return -1;
}

/** Parse crypto chain xform command line argument */
static int
parse_crypto_opt_chain(struct l2fwd_crypto_options *options, char *optarg)
{
	if (strcmp("CIPHER_HASH", optarg) == 0) {
		options->xform_chain = L2FWD_CRYPTO_CIPHER_HASH;
		return 0;
	} else if (strcmp("HASH_CIPHER", optarg) == 0) {
		options->xform_chain = L2FWD_CRYPTO_HASH_CIPHER;
		return 0;
	} else if (strcmp("CIPHER_ONLY", optarg) == 0) {
		options->xform_chain = L2FWD_CRYPTO_CIPHER_ONLY;
		return 0;
	} else if (strcmp("HASH_ONLY", optarg) == 0) {
		options->xform_chain = L2FWD_CRYPTO_HASH_ONLY;
		return 0;
	}

	return -1;
}

/** Parse crypto cipher algo option command line argument */
static int
parse_cipher_algo(enum rte_crypto_cipher_algorithm *algo, char *optarg)
{

	if (rte_cryptodev_get_cipher_algo_enum(algo, optarg) < 0) {
		RTE_LOG(ERR, USER1, "Cipher algorithm specified "
				"not supported!\n");
		return -1;
	}

	return 0;
}

/** Parse crypto cipher operation command line argument */
static int
parse_cipher_op(enum rte_crypto_cipher_operation *op, char *optarg)
{
	if (strcmp("ENCRYPT", optarg) == 0) {
		*op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
		return 0;
	} else if (strcmp("DECRYPT", optarg) == 0) {
		*op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
		return 0;
	}

	printf("Cipher operation not supported!\n");
	return -1;
}

/** Parse crypto key command line argument */
static int
parse_key(uint8_t *data, char *input_arg)
{
	unsigned byte_count;
	char *token;

	for (byte_count = 0, token = strtok(input_arg, ":");
			(byte_count < MAX_KEY_SIZE) && (token != NULL);
			token = strtok(NULL, ":")) {

		int number = (int)strtol(token, NULL, 16);

		if (errno == EINVAL || errno == ERANGE || number > 0xFF)
			return -1;

		data[byte_count++] = (uint8_t)number;
	}

	return byte_count;
}

/** Parse size param*/
static int
parse_size(int *size, const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		n = 0;

	if (n == 0) {
		printf("invalid size\n");
		return -1;
	}

	*size = n;
	return 0;
}

/** Parse crypto cipher operation command line argument */
static int
parse_auth_algo(enum rte_crypto_auth_algorithm *algo, char *optarg)
{
	if (rte_cryptodev_get_auth_algo_enum(algo, optarg) < 0) {
		RTE_LOG(ERR, USER1, "Authentication algorithm specified "
				"not supported!\n");
		return -1;
	}

	return 0;
}

static int
parse_auth_op(enum rte_crypto_auth_operation *op, char *optarg)
{
	if (strcmp("VERIFY", optarg) == 0) {
		*op = RTE_CRYPTO_AUTH_OP_VERIFY;
		return 0;
	} else if (strcmp("GENERATE", optarg) == 0) {
		*op = RTE_CRYPTO_AUTH_OP_GENERATE;
		return 0;
	}

	printf("Authentication operation specified not supported!\n");
	return -1;
}

static int
parse_cryptodev_mask(struct l2fwd_crypto_options *options,
		const char *q_arg)
{
	char *end = NULL;
	uint64_t pm;

	/* parse hexadecimal string */
	pm = strtoul(q_arg, &end, 16);
	if ((pm == '\0') || (end == NULL) || (*end != '\0'))
		pm = 0;

	options->cryptodev_mask = pm;
	if (options->cryptodev_mask == 0) {
		printf("invalid cryptodev_mask specified\n");
		return -1;
	}

	return 0;
}

/** Parse long options */
static int
l2fwd_crypto_parse_args_long_options(struct l2fwd_crypto_options *options,
		struct option *lgopts, int option_index)
{
	int retval;

	if (strcmp(lgopts[option_index].name, "cdev_type") == 0) {
		retval = parse_cryptodev_type(&options->type, optarg);
		if (retval == 0)
			snprintf(options->string_type, MAX_STR_LEN,
				"%s", optarg);
		return retval;
	}

	else if (strcmp(lgopts[option_index].name, "chain") == 0)
		return parse_crypto_opt_chain(options, optarg);

	/* Cipher options */
	else if (strcmp(lgopts[option_index].name, "cipher_algo") == 0)
		return parse_cipher_algo(&options->cipher_xform.cipher.algo,
				optarg);

	else if (strcmp(lgopts[option_index].name, "cipher_op") == 0)
		return parse_cipher_op(&options->cipher_xform.cipher.op,
				optarg);

	else if (strcmp(lgopts[option_index].name, "cipher_key") == 0) {
		options->ckey_param = 1;
		options->cipher_xform.cipher.key.length =
			parse_key(options->cipher_xform.cipher.key.data, optarg);
		if (options->cipher_xform.cipher.key.length > 0)
			return 0;
		else
			return -1;
	}

	else if (strcmp(lgopts[option_index].name, "cipher_key_random_size") == 0)
		return parse_size(&options->ckey_random_size, optarg);

	else if (strcmp(lgopts[option_index].name, "iv") == 0) {
		options->iv_param = 1;
		options->iv.length =
			parse_key(options->iv.data, optarg);
		if (options->iv.length > 0)
			return 0;
		else
			return -1;
	}

	else if (strcmp(lgopts[option_index].name, "iv_random_size") == 0)
		return parse_size(&options->iv_random_size, optarg);

	/* Authentication options */
	else if (strcmp(lgopts[option_index].name, "auth_algo") == 0) {
		return parse_auth_algo(&options->auth_xform.auth.algo,
				optarg);
	}

	else if (strcmp(lgopts[option_index].name, "auth_op") == 0)
		return parse_auth_op(&options->auth_xform.auth.op,
				optarg);

	else if (strcmp(lgopts[option_index].name, "auth_key") == 0) {
		options->akey_param = 1;
		options->auth_xform.auth.key.length =
			parse_key(options->auth_xform.auth.key.data, optarg);
		if (options->auth_xform.auth.key.length > 0)
			return 0;
		else
			return -1;
	}

	else if (strcmp(lgopts[option_index].name, "auth_key_random_size") == 0) {
		return parse_size(&options->akey_random_size, optarg);
	}

	else if (strcmp(lgopts[option_index].name, "aad") == 0) {
		options->aad_param = 1;
		options->aad.length =
			parse_key(options->aad.data, optarg);
		if (options->aad.length > 0)
			return 0;
		else
			return -1;
	}

	else if (strcmp(lgopts[option_index].name, "aad_random_size") == 0) {
		return parse_size(&options->aad_random_size, optarg);
	}

	else if (strcmp(lgopts[option_index].name, "digest_size") == 0) {
		return parse_size(&options->digest_size, optarg);
	}

	else if (strcmp(lgopts[option_index].name, "sessionless") == 0) {
		options->sessionless = 1;
		return 0;
	}

	else if (strcmp(lgopts[option_index].name, "cryptodev_mask") == 0)
		return parse_cryptodev_mask(options, optarg);

	return -1;
}

/** Parse port mask */
static int
l2fwd_crypto_parse_portmask(struct l2fwd_crypto_options *options,
		const char *q_arg)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(q_arg, &end, 16);
	if ((pm == '\0') || (end == NULL) || (*end != '\0'))
		pm = 0;

	options->portmask = pm;
	if (options->portmask == 0) {
		printf("invalid portmask specified\n");
		return -1;
	}

	return pm;
}

/** Parse number of queues */
static int
l2fwd_crypto_parse_nqueue(struct l2fwd_crypto_options *options,
		const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		n = 0;
	else if (n >= MAX_RX_QUEUE_PER_LCORE)
		n = 0;

	options->nb_ports_per_lcore = n;
	if (options->nb_ports_per_lcore == 0) {
		printf("invalid number of ports selected\n");
		return -1;
	}

	return 0;
}

/** Parse timer period */
static int
l2fwd_crypto_parse_timer_period(struct l2fwd_crypto_options *options,
		const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse number string */
	n = (unsigned)strtol(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		n = 0;

	if (n >= MAX_TIMER_PERIOD) {
		printf("Warning refresh period specified %lu is greater than "
				"max value %lu! using max value",
				n, MAX_TIMER_PERIOD);
		n = MAX_TIMER_PERIOD;
	}

	options->refresh_period = n * 1000 * TIMER_MILLISECOND;

	return 0;
}

/** Generate default options for application */
static void
l2fwd_crypto_default_options(struct l2fwd_crypto_options *options)
{
	options->portmask = 0xffffffff;
	options->nb_ports_per_lcore = 1;
	options->refresh_period = 10000;
	options->single_lcore = 0;
	options->sessionless = 0;

	options->xform_chain = L2FWD_CRYPTO_CIPHER_HASH;

	/* Cipher Data */
	options->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	options->cipher_xform.next = NULL;
	options->ckey_param = 0;
	options->ckey_random_size = -1;
	options->cipher_xform.cipher.key.length = 0;
	options->iv_param = 0;
	options->iv_random_size = -1;
	options->iv.length = 0;

	options->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	options->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;

	/* Authentication Data */
	options->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	options->auth_xform.next = NULL;
	options->akey_param = 0;
	options->akey_random_size = -1;
	options->auth_xform.auth.key.length = 0;
	options->aad_param = 0;
	options->aad_random_size = -1;
	options->aad.length = 0;
	options->digest_size = -1;

	options->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA1_HMAC;
	options->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;

	options->type = CDEV_TYPE_ANY;
	options->cryptodev_mask = UINT64_MAX;
}

static void
display_cipher_info(struct l2fwd_crypto_options *options)
{
	printf("\n---- Cipher information ---\n");
	printf("Algorithm: %s\n",
		rte_crypto_cipher_algorithm_strings[options->cipher_xform.cipher.algo]);
	rte_hexdump(stdout, "Cipher key:",
			options->cipher_xform.cipher.key.data,
			options->cipher_xform.cipher.key.length);
	rte_hexdump(stdout, "IV:", options->iv.data, options->iv.length);
}

static void
display_auth_info(struct l2fwd_crypto_options *options)
{
	printf("\n---- Authentication information ---\n");
	printf("Algorithm: %s\n",
		rte_crypto_auth_algorithm_strings[options->auth_xform.cipher.algo]);
	rte_hexdump(stdout, "Auth key:",
			options->auth_xform.auth.key.data,
			options->auth_xform.auth.key.length);
	rte_hexdump(stdout, "AAD:", options->aad.data, options->aad.length);
}

static void
l2fwd_crypto_options_print(struct l2fwd_crypto_options *options)
{
	char string_cipher_op[MAX_STR_LEN];
	char string_auth_op[MAX_STR_LEN];

	if (options->cipher_xform.cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
		strcpy(string_cipher_op, "Encrypt");
	else
		strcpy(string_cipher_op, "Decrypt");

	if (options->auth_xform.auth.op == RTE_CRYPTO_AUTH_OP_GENERATE)
		strcpy(string_auth_op, "Auth generate");
	else
		strcpy(string_auth_op, "Auth verify");

	printf("Options:-\nn");
	printf("portmask: %x\n", options->portmask);
	printf("ports per lcore: %u\n", options->nb_ports_per_lcore);
	printf("refresh period : %u\n", options->refresh_period);
	printf("single lcore mode: %s\n",
			options->single_lcore ? "enabled" : "disabled");
	printf("stats_printing: %s\n",
			options->refresh_period == 0 ? "disabled" : "enabled");

	printf("sessionless crypto: %s\n",
			options->sessionless ? "enabled" : "disabled");

	if (options->ckey_param && (options->ckey_random_size != -1))
		printf("Cipher key already parsed, ignoring size of random key\n");

	if (options->akey_param && (options->akey_random_size != -1))
		printf("Auth key already parsed, ignoring size of random key\n");

	if (options->iv_param && (options->iv_random_size != -1))
		printf("IV already parsed, ignoring size of random IV\n");

	if (options->aad_param && (options->aad_random_size != -1))
		printf("AAD already parsed, ignoring size of random AAD\n");

	printf("\nCrypto chain: ");
	switch (options->xform_chain) {
	case L2FWD_CRYPTO_CIPHER_HASH:
		printf("Input --> %s --> %s --> Output\n",
			string_cipher_op, string_auth_op);
		display_cipher_info(options);
		display_auth_info(options);
		break;
	case L2FWD_CRYPTO_HASH_CIPHER:
		printf("Input --> %s --> %s --> Output\n",
			string_auth_op, string_cipher_op);
		display_cipher_info(options);
		display_auth_info(options);
		break;
	case L2FWD_CRYPTO_HASH_ONLY:
		printf("Input --> %s --> Output\n", string_auth_op);
		display_auth_info(options);
		break;
	case L2FWD_CRYPTO_CIPHER_ONLY:
		printf("Input --> %s --> Output\n", string_cipher_op);
		display_cipher_info(options);
		break;
	}
}

/* Parse the argument given in the command line of the application */
static int
l2fwd_crypto_parse_args(struct l2fwd_crypto_options *options,
		int argc, char **argv)
{
	int opt, retval, option_index;
	char **argvopt = argv, *prgname = argv[0];

	static struct option lgopts[] = {
			{ "sessionless", no_argument, 0, 0 },

			{ "cdev_type", required_argument, 0, 0 },
			{ "chain", required_argument, 0, 0 },

			{ "cipher_algo", required_argument, 0, 0 },
			{ "cipher_op", required_argument, 0, 0 },
			{ "cipher_key", required_argument, 0, 0 },
			{ "cipher_key_random_size", required_argument, 0, 0 },

			{ "auth_algo", required_argument, 0, 0 },
			{ "auth_op", required_argument, 0, 0 },
			{ "auth_key", required_argument, 0, 0 },
			{ "auth_key_random_size", required_argument, 0, 0 },

			{ "iv", required_argument, 0, 0 },
			{ "iv_random_size", required_argument, 0, 0 },
			{ "aad", required_argument, 0, 0 },
			{ "aad_random_size", required_argument, 0, 0 },
			{ "digest_size", required_argument, 0, 0 },

			{ "sessionless", no_argument, 0, 0 },
			{ "cryptodev_mask", required_argument, 0, 0},

			{ NULL, 0, 0, 0 }
	};

	l2fwd_crypto_default_options(options);

	while ((opt = getopt_long(argc, argvopt, "p:q:st:", lgopts,
			&option_index)) != EOF) {
		switch (opt) {
		/* long options */
		case 0:
			retval = l2fwd_crypto_parse_args_long_options(options,
					lgopts, option_index);
			if (retval < 0) {
				l2fwd_crypto_usage(prgname);
				return -1;
			}
			break;

		/* portmask */
		case 'p':
			retval = l2fwd_crypto_parse_portmask(options, optarg);
			if (retval < 0) {
				l2fwd_crypto_usage(prgname);
				return -1;
			}
			break;

		/* nqueue */
		case 'q':
			retval = l2fwd_crypto_parse_nqueue(options, optarg);
			if (retval < 0) {
				l2fwd_crypto_usage(prgname);
				return -1;
			}
			break;

		/* single  */
		case 's':
			options->single_lcore = 1;

			break;

		/* timer period */
		case 'T':
			retval = l2fwd_crypto_parse_timer_period(options,
					optarg);
			if (retval < 0) {
				l2fwd_crypto_usage(prgname);
				return -1;
			}
			break;

		default:
			l2fwd_crypto_usage(prgname);
			return -1;
		}
	}


	if (optind >= 0)
		argv[optind-1] = prgname;

	retval = optind-1;
	optind = 1; /* reset getopt lib */

	return retval;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint8_t)portid,
						(unsigned)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
						(uint8_t)portid);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

/* Check if device has to be HW/SW or any */
static int
check_type(struct l2fwd_crypto_options *options, struct rte_cryptodev_info *dev_info)
{
	if (options->type == CDEV_TYPE_HW &&
			(dev_info->feature_flags & RTE_CRYPTODEV_FF_HW_ACCELERATED))
		return 0;
	if (options->type == CDEV_TYPE_SW &&
			!(dev_info->feature_flags & RTE_CRYPTODEV_FF_HW_ACCELERATED))
		return 0;
	if (options->type == CDEV_TYPE_ANY)
		return 0;

	return -1;
}

/* Check if the device is enabled by cryptodev_mask */
static int
check_cryptodev_mask(struct l2fwd_crypto_options *options,
		uint8_t cdev_id)
{
	if (options->cryptodev_mask & (1 << cdev_id))
		return 0;

	return -1;
}

static inline int
check_supported_size(uint16_t length, uint16_t min, uint16_t max,
		uint16_t increment)
{
	uint16_t supp_size;

	/* Single value */
	if (increment == 0) {
		if (length == min)
			return 0;
		else
			return -1;
	}

	/* Range of values */
	for (supp_size = min; supp_size <= max; supp_size += increment) {
		if (length == supp_size)
			return 0;
	}

	return -1;
}
static int
initialize_cryptodevs(struct l2fwd_crypto_options *options, unsigned nb_ports,
		uint8_t *enabled_cdevs)
{
	unsigned i, cdev_id, cdev_count, enabled_cdev_count = 0;
	const struct rte_cryptodev_capabilities *cap;
	enum rte_crypto_auth_algorithm cap_auth_algo;
	enum rte_crypto_auth_algorithm opt_auth_algo;
	enum rte_crypto_cipher_algorithm cap_cipher_algo;
	enum rte_crypto_cipher_algorithm opt_cipher_algo;
	int retval;

	cdev_count = rte_cryptodev_count();
	if (cdev_count == 0) {
		printf("No crypto devices available\n");
		return -1;
	}

	for (cdev_id = 0; cdev_id < cdev_count && enabled_cdev_count < nb_ports;
			cdev_id++) {
		struct rte_cryptodev_qp_conf qp_conf;
		struct rte_cryptodev_info dev_info;

		struct rte_cryptodev_config conf = {
			.nb_queue_pairs = 1,
			.socket_id = SOCKET_ID_ANY,
			.session_mp = {
				.nb_objs = 2048,
				.cache_size = 64
			}
		};

		if (check_cryptodev_mask(options, (uint8_t)cdev_id))
			continue;

		rte_cryptodev_info_get(cdev_id, &dev_info);

		/* Set cipher parameters */
		if (options->xform_chain == L2FWD_CRYPTO_CIPHER_HASH ||
				options->xform_chain == L2FWD_CRYPTO_HASH_CIPHER ||
				options->xform_chain == L2FWD_CRYPTO_CIPHER_ONLY) {
			/* Check if device supports cipher algo */
			i = 0;
			opt_cipher_algo = options->cipher_xform.cipher.algo;
			cap = &dev_info.capabilities[i];
			while (cap->op != RTE_CRYPTO_OP_TYPE_UNDEFINED) {
				cap_cipher_algo = cap->sym.cipher.algo;
				if (cap->sym.xform_type ==
						RTE_CRYPTO_SYM_XFORM_CIPHER) {
					if (cap_cipher_algo == opt_cipher_algo) {
						if (check_type(options, &dev_info) == 0)
							break;
					}
				}
				cap = &dev_info.capabilities[++i];
			}

			if (cap->op == RTE_CRYPTO_OP_TYPE_UNDEFINED) {
				printf("Algorithm %s not supported by cryptodev %u"
					" or device not of preferred type (%s)\n",
					rte_crypto_cipher_algorithm_strings[opt_cipher_algo],
					cdev_id,
					options->string_type);
				continue;
			}

			options->block_size = cap->sym.cipher.block_size;
			/*
			 * Check if length of provided IV is supported
			 * by the algorithm chosen.
			 */
			if (options->iv_param) {
				if (check_supported_size(options->iv.length,
						cap->sym.cipher.iv_size.min,
						cap->sym.cipher.iv_size.max,
						cap->sym.cipher.iv_size.increment)
							!= 0) {
					printf("Unsupported IV length\n");
					return -1;
				}
			/*
			 * Check if length of IV to be randomly generated
			 * is supported by the algorithm chosen.
			 */
			} else if (options->iv_random_size != -1) {
				if (check_supported_size(options->iv_random_size,
						cap->sym.cipher.iv_size.min,
						cap->sym.cipher.iv_size.max,
						cap->sym.cipher.iv_size.increment)
							!= 0) {
					printf("Unsupported IV length\n");
					return -1;
				}
				options->iv.length = options->iv_random_size;
			/* No size provided, use minimum size. */
			} else
				options->iv.length = cap->sym.cipher.iv_size.min;

			/*
			 * Check if length of provided cipher key is supported
			 * by the algorithm chosen.
			 */
			if (options->ckey_param) {
				if (check_supported_size(
						options->cipher_xform.cipher.key.length,
						cap->sym.cipher.key_size.min,
						cap->sym.cipher.key_size.max,
						cap->sym.cipher.key_size.increment)
							!= 0) {
					printf("Unsupported cipher key length\n");
					return -1;
				}
			/*
			 * Check if length of the cipher key to be randomly generated
			 * is supported by the algorithm chosen.
			 */
			} else if (options->ckey_random_size != -1) {
				if (check_supported_size(options->ckey_random_size,
						cap->sym.cipher.key_size.min,
						cap->sym.cipher.key_size.max,
						cap->sym.cipher.key_size.increment)
							!= 0) {
					printf("Unsupported cipher key length\n");
					return -1;
				}
				options->cipher_xform.cipher.key.length =
							options->ckey_random_size;
			/* No size provided, use minimum size. */
			} else
				options->cipher_xform.cipher.key.length =
						cap->sym.cipher.key_size.min;

			if (!options->ckey_param)
				generate_random_key(
					options->cipher_xform.cipher.key.data,
					options->cipher_xform.cipher.key.length);

		}

		/* Set auth parameters */
		if (options->xform_chain == L2FWD_CRYPTO_CIPHER_HASH ||
				options->xform_chain == L2FWD_CRYPTO_HASH_CIPHER ||
				options->xform_chain == L2FWD_CRYPTO_HASH_ONLY) {
			/* Check if device supports auth algo */
			i = 0;
			opt_auth_algo = options->auth_xform.auth.algo;
			cap = &dev_info.capabilities[i];
			while (cap->op != RTE_CRYPTO_OP_TYPE_UNDEFINED) {
				cap_auth_algo = cap->sym.auth.algo;
				if ((cap->sym.xform_type == RTE_CRYPTO_SYM_XFORM_AUTH) &&
						(cap_auth_algo == opt_auth_algo) &&
						(check_type(options, &dev_info) == 0)) {
					break;
				}
				cap = &dev_info.capabilities[++i];
			}

			if (cap->op == RTE_CRYPTO_OP_TYPE_UNDEFINED) {
				printf("Algorithm %s not supported by cryptodev %u"
					" or device not of preferred type (%s)\n",
					rte_crypto_auth_algorithm_strings[opt_auth_algo],
					cdev_id,
					options->string_type);
				continue;
			}

			/*
			 * Check if length of provided AAD is supported
			 * by the algorithm chosen.
			 */
			if (options->aad_param) {
				if (check_supported_size(options->aad.length,
						cap->sym.auth.aad_size.min,
						cap->sym.auth.aad_size.max,
						cap->sym.auth.aad_size.increment)
							!= 0) {
					printf("Unsupported AAD length\n");
					return -1;
				}
			/*
			 * Check if length of AAD to be randomly generated
			 * is supported by the algorithm chosen.
			 */
			} else if (options->aad_random_size != -1) {
				if (check_supported_size(options->aad_random_size,
						cap->sym.auth.aad_size.min,
						cap->sym.auth.aad_size.max,
						cap->sym.auth.aad_size.increment)
							!= 0) {
					printf("Unsupported AAD length\n");
					return -1;
				}
				options->aad.length = options->aad_random_size;
			/* No size provided, use minimum size. */
			} else
				options->aad.length = cap->sym.auth.aad_size.min;

			options->auth_xform.auth.add_auth_data_length =
						options->aad.length;

			/*
			 * Check if length of provided auth key is supported
			 * by the algorithm chosen.
			 */
			if (options->akey_param) {
				if (check_supported_size(
						options->auth_xform.auth.key.length,
						cap->sym.auth.key_size.min,
						cap->sym.auth.key_size.max,
						cap->sym.auth.key_size.increment)
							!= 0) {
					printf("Unsupported auth key length\n");
					return -1;
				}
			/*
			 * Check if length of the auth key to be randomly generated
			 * is supported by the algorithm chosen.
			 */
			} else if (options->akey_random_size != -1) {
				if (check_supported_size(options->akey_random_size,
						cap->sym.auth.key_size.min,
						cap->sym.auth.key_size.max,
						cap->sym.auth.key_size.increment)
							!= 0) {
					printf("Unsupported auth key length\n");
					return -1;
				}
				options->auth_xform.auth.key.length =
							options->akey_random_size;
			/* No size provided, use minimum size. */
			} else
				options->auth_xform.auth.key.length =
						cap->sym.auth.key_size.min;

			if (!options->akey_param)
				generate_random_key(
					options->auth_xform.auth.key.data,
					options->auth_xform.auth.key.length);

			/* Check if digest size is supported by the algorithm. */
			if (options->digest_size != -1) {
				if (check_supported_size(options->digest_size,
						cap->sym.auth.digest_size.min,
						cap->sym.auth.digest_size.max,
						cap->sym.auth.digest_size.increment)
							!= 0) {
					printf("Unsupported digest length\n");
					return -1;
				}
				options->auth_xform.auth.digest_length =
							options->digest_size;
			/* No size provided, use minimum size. */
			} else
				options->auth_xform.auth.digest_length =
						cap->sym.auth.digest_size.min;
		}

		retval = rte_cryptodev_configure(cdev_id, &conf);
		if (retval < 0) {
			printf("Failed to configure cryptodev %u", cdev_id);
			return -1;
		}

		qp_conf.nb_descriptors = 2048;

		retval = rte_cryptodev_queue_pair_setup(cdev_id, 0, &qp_conf,
				SOCKET_ID_ANY);
		if (retval < 0) {
			printf("Failed to setup queue pair %u on cryptodev %u",
					0, cdev_id);
			return -1;
		}

		retval = rte_cryptodev_start(cdev_id);
		if (retval < 0) {
			printf("Failed to start device %u: error %d\n",
					cdev_id, retval);
			return -1;
		}

		l2fwd_enabled_crypto_mask |= (((uint64_t)1) << cdev_id);

		enabled_cdevs[cdev_id] = 1;
		enabled_cdev_count++;
	}

	return enabled_cdev_count;
}

static int
initialize_ports(struct l2fwd_crypto_options *options)
{
	uint8_t last_portid, portid;
	unsigned enabled_portcount = 0;
	unsigned nb_ports = rte_eth_dev_count();

	if (nb_ports == 0) {
		printf("No Ethernet ports - bye\n");
		return -1;
	}

	/* Reset l2fwd_dst_ports */
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
		l2fwd_dst_ports[portid] = 0;

	for (last_portid = 0, portid = 0; portid < nb_ports; portid++) {
		int retval;

		/* Skip ports that are not enabled */
		if ((options->portmask & (1 << portid)) == 0)
			continue;

		/* init port */
		printf("Initializing port %u... ", (unsigned) portid);
		fflush(stdout);
		retval = rte_eth_dev_configure(portid, 1, 1, &port_conf);
		if (retval < 0) {
			printf("Cannot configure device: err=%d, port=%u\n",
				  retval, (unsigned) portid);
			return -1;
		}

		/* init one RX queue */
		fflush(stdout);
		retval = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
					     rte_eth_dev_socket_id(portid),
					     NULL, l2fwd_pktmbuf_pool);
		if (retval < 0) {
			printf("rte_eth_rx_queue_setup:err=%d, port=%u\n",
					retval, (unsigned) portid);
			return -1;
		}

		/* init one TX queue on each port */
		fflush(stdout);
		retval = rte_eth_tx_queue_setup(portid, 0, nb_txd,
				rte_eth_dev_socket_id(portid),
				NULL);
		if (retval < 0) {
			printf("rte_eth_tx_queue_setup:err=%d, port=%u\n",
				retval, (unsigned) portid);

			return -1;
		}

		/* Start device */
		retval = rte_eth_dev_start(portid);
		if (retval < 0) {
			printf("rte_eth_dev_start:err=%d, port=%u\n",
					retval, (unsigned) portid);
			return -1;
		}

		rte_eth_promiscuous_enable(portid);

		rte_eth_macaddr_get(portid, &l2fwd_ports_eth_addr[portid]);

		printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
				(unsigned) portid,
				l2fwd_ports_eth_addr[portid].addr_bytes[0],
				l2fwd_ports_eth_addr[portid].addr_bytes[1],
				l2fwd_ports_eth_addr[portid].addr_bytes[2],
				l2fwd_ports_eth_addr[portid].addr_bytes[3],
				l2fwd_ports_eth_addr[portid].addr_bytes[4],
				l2fwd_ports_eth_addr[portid].addr_bytes[5]);

		/* initialize port stats */
		memset(&port_statistics, 0, sizeof(port_statistics));

		/* Setup port forwarding table */
		if (enabled_portcount % 2) {
			l2fwd_dst_ports[portid] = last_portid;
			l2fwd_dst_ports[last_portid] = portid;
		} else {
			last_portid = portid;
		}

		l2fwd_enabled_port_mask |= (1 << portid);
		enabled_portcount++;
	}

	if (enabled_portcount == 1) {
		l2fwd_dst_ports[last_portid] = last_portid;
	} else if (enabled_portcount % 2) {
		printf("odd number of ports in portmask- bye\n");
		return -1;
	}

	check_all_ports_link_status(nb_ports, l2fwd_enabled_port_mask);

	return enabled_portcount;
}

static void
reserve_key_memory(struct l2fwd_crypto_options *options)
{
	options->cipher_xform.cipher.key.data = rte_malloc("crypto key",
						MAX_KEY_SIZE, 0);
	if (options->cipher_xform.cipher.key.data == NULL)
		rte_exit(EXIT_FAILURE, "Failed to allocate memory for cipher key");


	options->auth_xform.auth.key.data = rte_malloc("auth key",
						MAX_KEY_SIZE, 0);
	if (options->auth_xform.auth.key.data == NULL)
		rte_exit(EXIT_FAILURE, "Failed to allocate memory for auth key");

	options->iv.data = rte_malloc("iv", MAX_KEY_SIZE, 0);
	if (options->iv.data == NULL)
		rte_exit(EXIT_FAILURE, "Failed to allocate memory for IV");
	options->iv.phys_addr = rte_malloc_virt2phy(options->iv.data);

	options->aad.data = rte_malloc("aad", MAX_KEY_SIZE, 0);
	if (options->aad.data == NULL)
		rte_exit(EXIT_FAILURE, "Failed to allocate memory for AAD");
	options->aad.phys_addr = rte_malloc_virt2phy(options->aad.data);
}

int
main(int argc, char **argv)
{
	struct lcore_queue_conf *qconf;
	struct l2fwd_crypto_options options;

	uint8_t nb_ports, nb_cryptodevs, portid, cdev_id;
	unsigned lcore_id, rx_lcore_id;
	int ret, enabled_cdevcount, enabled_portcount;
	uint8_t enabled_cdevs[RTE_CRYPTO_MAX_DEVS] = {0};

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	/* reserve memory for Cipher/Auth key and IV */
	reserve_key_memory(&options);

	/* parse application arguments (after the EAL ones) */
	ret = l2fwd_crypto_parse_args(&options, argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L2FWD-CRYPTO arguments\n");

	/* create the mbuf pool */
	l2fwd_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF, 512,
			sizeof(struct rte_crypto_op),
			RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (l2fwd_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* create crypto op pool */
	l2fwd_crypto_op_pool = rte_crypto_op_pool_create("crypto_op_pool",
			RTE_CRYPTO_OP_TYPE_SYMMETRIC, NB_MBUF, 128, 0,
			rte_socket_id());
	if (l2fwd_crypto_op_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create crypto op pool\n");

	/* Enable Ethernet ports */
	enabled_portcount = initialize_ports(&options);
	if (enabled_portcount < 1)
		rte_exit(EXIT_FAILURE, "Failed to initial Ethernet ports\n");

	nb_ports = rte_eth_dev_count();
	/* Initialize the port/queue configuration of each logical core */
	for (rx_lcore_id = 0, qconf = NULL, portid = 0;
			portid < nb_ports; portid++) {

		/* skip ports that are not enabled */
		if ((options.portmask & (1 << portid)) == 0)
			continue;

		if (options.single_lcore && qconf == NULL) {
			while (rte_lcore_is_enabled(rx_lcore_id) == 0) {
				rx_lcore_id++;
				if (rx_lcore_id >= RTE_MAX_LCORE)
					rte_exit(EXIT_FAILURE,
							"Not enough cores\n");
			}
		} else if (!options.single_lcore) {
			/* get the lcore_id for this port */
			while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
			       lcore_queue_conf[rx_lcore_id].nb_rx_ports ==
			       options.nb_ports_per_lcore) {
				rx_lcore_id++;
				if (rx_lcore_id >= RTE_MAX_LCORE)
					rte_exit(EXIT_FAILURE,
							"Not enough cores\n");
			}
		}

		/* Assigned a new logical core in the loop above. */
		if (qconf != &lcore_queue_conf[rx_lcore_id])
			qconf = &lcore_queue_conf[rx_lcore_id];

		qconf->rx_port_list[qconf->nb_rx_ports] = portid;
		qconf->nb_rx_ports++;

		printf("Lcore %u: RX port %u\n", rx_lcore_id, (unsigned)portid);
	}

	/* Enable Crypto devices */
	enabled_cdevcount = initialize_cryptodevs(&options, enabled_portcount,
			enabled_cdevs);
	if (enabled_cdevcount < 0)
		rte_exit(EXIT_FAILURE, "Failed to initialize crypto devices\n");

	if (enabled_cdevcount < enabled_portcount)
		rte_exit(EXIT_FAILURE, "Number of capable crypto devices (%d) "
				"has to be more or equal to number of ports (%d)\n",
				enabled_cdevcount, enabled_portcount);

	nb_cryptodevs = rte_cryptodev_count();

	/* Initialize the port/cryptodev configuration of each logical core */
	for (rx_lcore_id = 0, qconf = NULL, cdev_id = 0;
			cdev_id < nb_cryptodevs && enabled_cdevcount;
			cdev_id++) {
		/* Crypto op not supported by crypto device */
		if (!enabled_cdevs[cdev_id])
			continue;

		if (options.single_lcore && qconf == NULL) {
			while (rte_lcore_is_enabled(rx_lcore_id) == 0) {
				rx_lcore_id++;
				if (rx_lcore_id >= RTE_MAX_LCORE)
					rte_exit(EXIT_FAILURE,
							"Not enough cores\n");
			}
		} else if (!options.single_lcore) {
			/* get the lcore_id for this port */
			while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
			       lcore_queue_conf[rx_lcore_id].nb_crypto_devs ==
			       options.nb_ports_per_lcore) {
				rx_lcore_id++;
				if (rx_lcore_id >= RTE_MAX_LCORE)
					rte_exit(EXIT_FAILURE,
							"Not enough cores\n");
			}
		}

		/* Assigned a new logical core in the loop above. */
		if (qconf != &lcore_queue_conf[rx_lcore_id])
			qconf = &lcore_queue_conf[rx_lcore_id];

		qconf->cryptodev_list[qconf->nb_crypto_devs] = cdev_id;
		qconf->nb_crypto_devs++;

		enabled_cdevcount--;

		printf("Lcore %u: cryptodev %u\n", rx_lcore_id,
				(unsigned)cdev_id);
	}

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(l2fwd_launch_one_lcore, (void *)&options,
			CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	return 0;
}
