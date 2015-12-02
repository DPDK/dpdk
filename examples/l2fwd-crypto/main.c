/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
#include <rte_mbuf_offload.h>
#include <rte_memcpy.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_memzone.h>
#include <rte_pci.h>
#include <rte_per_lcore.h>
#include <rte_prefetch.h>
#include <rte_random.h>
#include <rte_ring.h>

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define NB_MBUF   8192

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

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16

enum l2fwd_crypto_xform_chain {
	L2FWD_CRYPTO_CIPHER_HASH,
	L2FWD_CRYPTO_HASH_CIPHER
};

/** l2fwd crypto application command line options */
struct l2fwd_crypto_options {
	unsigned portmask;
	unsigned nb_ports_per_lcore;
	unsigned refresh_period;
	unsigned single_lcore:1;

	enum rte_cryptodev_type cdev_type;
	unsigned sessionless:1;

	enum l2fwd_crypto_xform_chain xform_chain;

	struct rte_crypto_xform cipher_xform;
	uint8_t ckey_data[32];

	struct rte_crypto_key iv_key;
	uint8_t ivkey_data[16];

	struct rte_crypto_xform auth_xform;
	uint8_t akey_data[128];
};

/** l2fwd crypto lcore params */
struct l2fwd_crypto_params {
	uint8_t dev_id;
	uint8_t qp_id;

	unsigned digest_length;
	unsigned block_size;

	struct rte_crypto_key iv_key;
	struct rte_cryptodev_session *session;
};

/** lcore configuration */
struct lcore_queue_conf {
	unsigned nb_rx_ports;
	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];

	unsigned nb_crypto_devs;
	unsigned cryptodev_list[MAX_RX_QUEUE_PER_LCORE];

	struct pkt_buffer crypto_pkt_buf[RTE_MAX_ETHPORTS];
	struct pkt_buffer tx_pkt_buf[RTE_MAX_ETHPORTS];
} __rte_cache_aligned;

struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

struct rte_mempool *l2fwd_pktmbuf_pool;
struct rte_mempool *l2fwd_mbuf_ol_pool;

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
struct l2fwd_crypto_statistics crypto_statistics[RTE_MAX_ETHPORTS];

/* A tsc-based timer responsible for triggering statistics printout */
#define TIMER_MILLISECOND 2000000ULL /* around 1ms at 2 Ghz */
#define MAX_TIMER_PERIOD 86400 /* 1 day max */

/* default period is 10 seconds */
static int64_t timer_period = 10 * TIMER_MILLISECOND * 1000;

uint64_t total_packets_dropped = 0, total_packets_tx = 0, total_packets_rx = 0,
	total_packets_enqueued = 0, total_packets_dequeued = 0,
	total_packets_errors = 0;

/* Print out statistics on packets dropped */
static void
print_stats(void)
{
	unsigned portid;
	uint64_t cdevid;


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
		if ((l2fwd_enabled_crypto_mask & (1lu << cdevid)) == 0)
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
	struct rte_mbuf **pkt_buffer;
	unsigned ret;

	pkt_buffer = (struct rte_mbuf **)
			qconf->crypto_pkt_buf[cparams->dev_id].buffer;

	ret = rte_cryptodev_enqueue_burst(cparams->dev_id, cparams->qp_id,
			pkt_buffer, (uint16_t) n);
	crypto_statistics[cparams->dev_id].enqueued += ret;
	if (unlikely(ret < n)) {
		crypto_statistics[cparams->dev_id].errors += (n - ret);
		do {
			rte_pktmbuf_offload_free(pkt_buffer[ret]->offload_ops);
			rte_pktmbuf_free(pkt_buffer[ret]);
		} while (++ret < n);
	}

	return 0;
}

static int
l2fwd_crypto_enqueue(struct rte_mbuf *m, struct l2fwd_crypto_params *cparams)
{
	unsigned lcore_id, len;
	struct lcore_queue_conf *qconf;

	lcore_id = rte_lcore_id();

	qconf = &lcore_queue_conf[lcore_id];
	len = qconf->crypto_pkt_buf[cparams->dev_id].len;
	qconf->crypto_pkt_buf[cparams->dev_id].buffer[len] = m;
	len++;

	/* enough pkts to be sent */
	if (len == MAX_PKT_BURST) {
		l2fwd_crypto_send_burst(qconf, MAX_PKT_BURST, cparams);
		len = 0;
	}

	qconf->crypto_pkt_buf[cparams->dev_id].len = len;
	return 0;
}

static int
l2fwd_simple_crypto_enqueue(struct rte_mbuf *m,
		struct rte_mbuf_offload *ol,
		struct l2fwd_crypto_params *cparams)
{
	struct ether_hdr *eth_hdr;
	struct ipv4_hdr *ip_hdr;

	unsigned ipdata_offset, pad_len, data_len;
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
	pad_len = data_len % cparams->block_size ? cparams->block_size -
			(data_len % cparams->block_size) : 0;

	if (pad_len) {
		padding = rte_pktmbuf_append(m, pad_len);
		if (unlikely(!padding))
			return -1;

		data_len += pad_len;
		memset(padding, 0, pad_len);
	}

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_session(&ol->op.crypto, cparams->session);

	/* Append space for digest to end of packet */
	ol->op.crypto.digest.data = (uint8_t *)rte_pktmbuf_append(m,
			cparams->digest_length);
	ol->op.crypto.digest.phys_addr = rte_pktmbuf_mtophys_offset(m,
			rte_pktmbuf_pkt_len(m) - cparams->digest_length);
	ol->op.crypto.digest.length = cparams->digest_length;

	ol->op.crypto.iv.data = cparams->iv_key.data;
	ol->op.crypto.iv.phys_addr = cparams->iv_key.phys_addr;
	ol->op.crypto.iv.length = cparams->iv_key.length;

	ol->op.crypto.data.to_cipher.offset = ipdata_offset;
	ol->op.crypto.data.to_cipher.length = data_len;

	ol->op.crypto.data.to_hash.offset = ipdata_offset;
	ol->op.crypto.data.to_hash.length = data_len;

	rte_pktmbuf_offload_attach(m, ol);

	return l2fwd_crypto_enqueue(m, cparams);
}


/* Send the burst of packets on an output interface */
static int
l2fwd_send_burst(struct lcore_queue_conf *qconf, unsigned n, uint8_t port)
{
	struct rte_mbuf **pkt_buffer;
	unsigned ret;
	unsigned queueid = 0;

	pkt_buffer = (struct rte_mbuf **)qconf->tx_pkt_buf[port].buffer;

	ret = rte_eth_tx_burst(port, (uint16_t) queueid, pkt_buffer,
			(uint16_t)n);
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
	len = qconf->tx_pkt_buf[port].len;
	qconf->tx_pkt_buf[port].buffer[len] = m;
	len++;

	/* enough pkts to be sent */
	if (unlikely(len == MAX_PKT_BURST)) {
		l2fwd_send_burst(qconf, MAX_PKT_BURST, port);
		len = 0;
	}

	qconf->tx_pkt_buf[port].len = len;
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
	unsigned i;

	for (i = 0; i < length; i++)
		key[i] = rand() % 0xff;
}

static struct rte_cryptodev_session *
initialize_crypto_session(struct l2fwd_crypto_options *options,
		uint8_t cdev_id)
{
	struct rte_crypto_xform *first_xform;

	if (options->xform_chain == L2FWD_CRYPTO_CIPHER_HASH) {
		first_xform = &options->cipher_xform;
		first_xform->next = &options->auth_xform;
	} else {
		first_xform = &options->auth_xform;
		first_xform->next = &options->cipher_xform;
	}

	/* Setup Cipher Parameters */
	return rte_cryptodev_session_create(cdev_id, first_xform);
}

static void
l2fwd_crypto_options_print(struct l2fwd_crypto_options *options);

/* main processing loop */
static void
l2fwd_main_loop(struct l2fwd_crypto_options *options)
{
	struct rte_mbuf *m, *pkts_burst[MAX_PKT_BURST];
	unsigned lcore_id = rte_lcore_id();
	uint64_t prev_tsc = 0, diff_tsc, cur_tsc, timer_tsc = 0;
	unsigned i, j, portid, nb_rx;
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

	l2fwd_crypto_options_print(options);

	for (i = 0; i < qconf->nb_rx_ports; i++) {

		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
			portid);
	}

	for (i = 0; i < qconf->nb_crypto_devs; i++) {
		port_cparams[i].dev_id = qconf->cryptodev_list[i];
		port_cparams[i].qp_id = 0;

		port_cparams[i].block_size = 64;
		port_cparams[i].digest_length = 20;

		port_cparams[i].iv_key.data =
				(uint8_t *)rte_malloc(NULL, 16, 8);
		port_cparams[i].iv_key.length = 16;
		port_cparams[i].iv_key.phys_addr = rte_malloc_virt2phy(
				(void *)port_cparams[i].iv_key.data);
		generate_random_key(port_cparams[i].iv_key.data,
				sizeof(cparams[i].iv_key.length));

		port_cparams[i].session = initialize_crypto_session(options,
				port_cparams[i].dev_id);

		if (port_cparams[i].session == NULL)
			return;
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u cryptoid=%u\n", lcore_id,
				port_cparams[i].dev_id);
	}

	while (1) {

		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
				if (qconf->tx_pkt_buf[portid].len == 0)
					continue;
				l2fwd_send_burst(&lcore_queue_conf[lcore_id],
						 qconf->tx_pkt_buf[portid].len,
						 (uint8_t) portid);
				qconf->tx_pkt_buf[portid].len = 0;
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
			struct rte_mbuf_offload *ol;

			portid = qconf->rx_port_list[i];

			cparams = &port_cparams[i];

			nb_rx = rte_eth_rx_burst((uint8_t) portid, 0,
						 pkts_burst, MAX_PKT_BURST);

			port_statistics[portid].rx += nb_rx;

			/* Enqueue packets from Crypto device*/
			for (j = 0; j < nb_rx; j++) {
				m = pkts_burst[j];
				ol = rte_pktmbuf_offload_alloc(
						l2fwd_mbuf_ol_pool,
						RTE_PKTMBUF_OL_CRYPTO);
				/*
				 * If we can't allocate a offload, then drop
				 * the rest of the burst and dequeue and
				 * process the packets to free offload structs
				 */
				if (unlikely(ol == NULL)) {
					for (; j < nb_rx; j++) {
						rte_pktmbuf_free(pkts_burst[j]);
						port_statistics[portid].dropped++;
					}
					break;
				}

				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
				rte_prefetch0((void *)ol);

				l2fwd_simple_crypto_enqueue(m, ol, cparams);
			}

			/* Dequeue packets from Crypto device */
			nb_rx = rte_cryptodev_dequeue_burst(
					cparams->dev_id, cparams->qp_id,
					pkts_burst, MAX_PKT_BURST);
			crypto_statistics[cparams->dev_id].dequeued += nb_rx;

			/* Forward crypto'd packets */
			for (j = 0; j < nb_rx; j++) {
				m = pkts_burst[j];
				rte_pktmbuf_offload_free(m->offload_ops);
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
				l2fwd_simple_forward(m, portid);
			}
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
	printf("%s [EAL options] -- --cdev TYPE [optional parameters]\n"
		"  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
		"  -q NQ: number of queue (=ports) per lcore (default is 1)\n"
		"  -s manage all ports from single lcore"
		"  -t PERIOD: statistics will be refreshed each PERIOD seconds"
		" (0 to disable, 10 default, 86400 maximum)\n"

		"  --cdev AESNI_MB / QAT\n"
		"  --chain HASH_CIPHER / CIPHER_HASH\n"

		"  --cipher_algo ALGO\n"
		"  --cipher_op ENCRYPT / DECRYPT\n"
		"  --cipher_key KEY\n"

		"  --auth ALGO\n"
		"  --auth_op GENERATE / VERIFY\n"
		"  --auth_key KEY\n"

		"  --sessionless\n",
	       prgname);
}

/** Parse crypto device type command line argument */
static int
parse_cryptodev_type(enum rte_cryptodev_type *type, char *optarg)
{
	if (strcmp("AESNI_MB", optarg) == 0) {
		*type = RTE_CRYPTODEV_AESNI_MB_PMD;
		return 0;
	} else if (strcmp("QAT", optarg) == 0) {
		*type = RTE_CRYPTODEV_QAT_PMD;
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
	}

	return -1;
}

/** Parse crypto cipher algo option command line argument */
static int
parse_cipher_algo(enum rte_crypto_cipher_algorithm *algo, char *optarg)
{
	if (strcmp("AES_CBC", optarg) == 0) {
		*algo = RTE_CRYPTO_CIPHER_AES_CBC;
		return 0;
	} else if (strcmp("AES_GCM", optarg) == 0) {
		*algo = RTE_CRYPTO_CIPHER_AES_GCM;
		return 0;
	}

	printf("Cipher algorithm  not supported!\n");
	return -1;
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
parse_key(struct rte_crypto_key *key __rte_unused,
		unsigned length __rte_unused, char *arg __rte_unused)
{
	printf("Currently an unsupported argument!\n");
	return -1;
}

/** Parse crypto cipher operation command line argument */
static int
parse_auth_algo(enum rte_crypto_auth_algorithm *algo, char *optarg)
{
	if (strcmp("SHA1", optarg) == 0) {
		*algo = RTE_CRYPTO_AUTH_SHA1;
		return 0;
	} else if (strcmp("SHA1_HMAC", optarg) == 0) {
		*algo = RTE_CRYPTO_AUTH_SHA1_HMAC;
		return 0;
	} else if (strcmp("SHA224", optarg) == 0) {
		*algo = RTE_CRYPTO_AUTH_SHA224;
		return 0;
	} else if (strcmp("SHA224_HMAC", optarg) == 0) {
		*algo = RTE_CRYPTO_AUTH_SHA224_HMAC;
		return 0;
	} else if (strcmp("SHA256", optarg) == 0) {
		*algo = RTE_CRYPTO_AUTH_SHA256;
		return 0;
	} else if (strcmp("SHA256_HMAC", optarg) == 0) {
		*algo = RTE_CRYPTO_AUTH_SHA256_HMAC;
		return 0;
	} else if (strcmp("SHA512", optarg) == 0) {
		*algo = RTE_CRYPTO_AUTH_SHA256;
		return 0;
	} else if (strcmp("SHA512_HMAC", optarg) == 0) {
		*algo = RTE_CRYPTO_AUTH_SHA256_HMAC;
		return 0;
	}

	printf("Authentication algorithm specified not supported!\n");
	return -1;
}

static int
parse_auth_op(enum rte_crypto_auth_operation *op, char *optarg)
{
	if (strcmp("VERIFY", optarg) == 0) {
		*op = RTE_CRYPTO_AUTH_OP_VERIFY;
		return 0;
	} else if (strcmp("GENERATE", optarg) == 0) {
		*op = RTE_CRYPTO_AUTH_OP_VERIFY;
		return 0;
	}

	printf("Authentication operation specified not supported!\n");
	return -1;
}

/** Parse long options */
static int
l2fwd_crypto_parse_args_long_options(struct l2fwd_crypto_options *options,
		struct option *lgopts, int option_index)
{
	if (strcmp(lgopts[option_index].name, "cdev_type") == 0)
		return parse_cryptodev_type(&options->cdev_type, optarg);

	else if (strcmp(lgopts[option_index].name, "chain") == 0)
		return parse_crypto_opt_chain(options, optarg);

	/* Cipher options */
	else if (strcmp(lgopts[option_index].name, "cipher_algo") == 0)
		return parse_cipher_algo(&options->cipher_xform.cipher.algo,
				optarg);

	else if (strcmp(lgopts[option_index].name, "cipher_op") == 0)
		return parse_cipher_op(&options->cipher_xform.cipher.op,
				optarg);

	else if (strcmp(lgopts[option_index].name, "cipher_key") == 0)
		return parse_key(&options->cipher_xform.cipher.key,
				sizeof(options->ckey_data), optarg);

	else if (strcmp(lgopts[option_index].name, "iv") == 0)
		return parse_key(&options->iv_key, sizeof(options->ivkey_data),
				optarg);

	/* Authentication options */
	else if (strcmp(lgopts[option_index].name, "auth_algo") == 0)
		return parse_auth_algo(&options->cipher_xform.auth.algo,
				optarg);

	else if (strcmp(lgopts[option_index].name, "auth_op") == 0)
		return parse_auth_op(&options->cipher_xform.auth.op,
				optarg);

	else if (strcmp(lgopts[option_index].name, "auth_key") == 0)
		return parse_key(&options->auth_xform.auth.key,
				sizeof(options->akey_data), optarg);

	else if (strcmp(lgopts[option_index].name, "sessionless") == 0) {
		options->sessionless = 1;
		return 0;
	}

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
	long int n;

	/* parse number string */
	n = strtol(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		n = 0;

	if (n >= MAX_TIMER_PERIOD) {
		printf("Warning refresh period specified %ld is greater than "
				"max value %d! using max value",
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
	srand(time(NULL));

	options->portmask = 0xffffffff;
	options->nb_ports_per_lcore = 1;
	options->refresh_period = 10000;
	options->single_lcore = 0;

	options->cdev_type = RTE_CRYPTODEV_AESNI_MB_PMD;
	options->sessionless = 0;
	options->xform_chain = L2FWD_CRYPTO_CIPHER_HASH;

	/* Cipher Data */
	options->cipher_xform.type = RTE_CRYPTO_XFORM_CIPHER;
	options->cipher_xform.next = NULL;

	options->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	options->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;

	generate_random_key(options->ckey_data, sizeof(options->ckey_data));

	options->cipher_xform.cipher.key.data = options->ckey_data;
	options->cipher_xform.cipher.key.phys_addr = 0;
	options->cipher_xform.cipher.key.length = 16;


	/* Authentication Data */
	options->auth_xform.type = RTE_CRYPTO_XFORM_AUTH;
	options->auth_xform.next = NULL;

	options->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA1_HMAC;
	options->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;

	options->auth_xform.auth.add_auth_data_length = 0;
	options->auth_xform.auth.digest_length = 20;

	generate_random_key(options->akey_data, sizeof(options->akey_data));

	options->auth_xform.auth.key.data = options->akey_data;
	options->auth_xform.auth.key.phys_addr = 0;
	options->auth_xform.auth.key.length = 20;
}

static void
l2fwd_crypto_options_print(struct l2fwd_crypto_options *options)
{
	printf("Options:-\nn");
	printf("portmask: %x\n", options->portmask);
	printf("ports per lcore: %u\n", options->nb_ports_per_lcore);
	printf("refresh period : %u\n", options->refresh_period);
	printf("single lcore mode: %s\n",
			options->single_lcore ? "enabled" : "disabled");
	printf("stats_printing: %s\n",
			options->refresh_period == 0 ? "disabled" : "enabled");

	switch (options->cdev_type) {
	case RTE_CRYPTODEV_AESNI_MB_PMD:
		printf("crytpodev type: AES-NI MB PMD\n"); break;
	case RTE_CRYPTODEV_QAT_PMD:
		printf("crytpodev type: QAT PMD\n"); break;
	default:
		break;
	}

	printf("sessionless crypto: %s\n",
			options->sessionless ? "enabled" : "disabled");
#if 0
	options->xform_chain = L2FWD_CRYPTO_CIPHER_HASH;

	/* Cipher Data */
	options->cipher_xform.type = RTE_CRYPTO_XFORM_CIPHER;
	options->cipher_xform.next = NULL;

	options->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	options->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;

	generate_random_key(options->ckey_data, sizeof(options->ckey_data));

	options->cipher_xform.cipher.key.data = options->ckey_data;
	options->cipher_xform.cipher.key.phys_addr = 0;
	options->cipher_xform.cipher.key.length = 16;


	/* Authentication Data */
	options->auth_xform.type = RTE_CRYPTO_XFORM_AUTH;
	options->auth_xform.next = NULL;

	options->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA1_HMAC;
	options->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;

	options->auth_xform.auth.add_auth_data_length = 0;
	options->auth_xform.auth.digest_length = 20;

	generate_random_key(options->akey_data, sizeof(options->akey_data));

	options->auth_xform.auth.key.data = options->akey_data;
	options->auth_xform.auth.key.phys_addr = 0;
	options->auth_xform.auth.key.length = 20;
#endif
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

			{ "auth_algo", required_argument, 0, 0 },
			{ "auth_op", required_argument, 0, 0 },
			{ "auth_key", required_argument, 0, 0 },

			{ "iv", required_argument, 0, 0 },

			{ "sessionless", no_argument, 0, 0 },
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
		case 't':
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
	optind = 0; /* reset getopt lib */

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
			if (link.link_status == 0) {
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

static int
initialize_cryptodevs(struct l2fwd_crypto_options *options, unsigned nb_ports)
{
	unsigned i, cdev_id, cdev_count, enabled_cdev_count = 0;
	int retval;

	if (options->cdev_type == RTE_CRYPTODEV_QAT_PMD) {
		if (rte_cryptodev_count() < nb_ports)
			return -1;
	} else if (options->cdev_type == RTE_CRYPTODEV_AESNI_MB_PMD) {
		for (i = 0; i < nb_ports; i++) {
			int id = rte_eal_vdev_init(CRYPTODEV_NAME_AESNI_MB_PMD,
					NULL);
			if (id < 0)
				return -1;
		}
	}

	cdev_count = rte_cryptodev_count();
	for (cdev_id = 0;
			cdev_id < cdev_count && enabled_cdev_count < nb_ports;
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

		rte_cryptodev_info_get(cdev_id, &dev_info);

		if (dev_info.dev_type != options->cdev_type)
			continue;


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

		l2fwd_enabled_crypto_mask |= (1 << cdev_id);

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

	if (nb_ports > RTE_MAX_ETHPORTS)
		nb_ports = RTE_MAX_ETHPORTS;

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

int
main(int argc, char **argv)
{
	struct lcore_queue_conf *qconf;
	struct l2fwd_crypto_options options;

	uint8_t nb_ports, nb_cryptodevs, portid, cdev_id;
	unsigned lcore_id, rx_lcore_id;
	int ret, enabled_cdevcount, enabled_portcount;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = l2fwd_crypto_parse_args(&options, argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L2FWD-CRYPTO arguments\n");

	/* create the mbuf pool */
	l2fwd_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF, 128,
		0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (l2fwd_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* create crypto op pool */
	l2fwd_mbuf_ol_pool = rte_pktmbuf_offload_pool_create(
			"mbuf_offload_pool", NB_MBUF, 128, 0, rte_socket_id());
	if (l2fwd_mbuf_ol_pool == NULL)
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
	enabled_cdevcount = initialize_cryptodevs(&options, enabled_portcount);
	if (enabled_cdevcount < 1)
		rte_exit(EXIT_FAILURE, "Failed to initial crypto devices\n");

	nb_cryptodevs = rte_cryptodev_count();
	/* Initialize the port/queue configuration of each logical core */
	for (rx_lcore_id = 0, qconf = NULL, cdev_id = 0;
			cdev_id < nb_cryptodevs && enabled_cdevcount;
			cdev_id++) {
		struct rte_cryptodev_info info;

		rte_cryptodev_info_get(cdev_id, &info);

		/* skip devices of the wrong type */
		if (options.cdev_type != info.dev_type)
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
