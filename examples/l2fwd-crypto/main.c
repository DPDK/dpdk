/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
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

#include <rte_string_fns.h>
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
#include <rte_per_lcore.h>
#include <rte_prefetch.h>
#include <rte_random.h>
#include <rte_hexdump.h>
#ifdef RTE_LIBRTE_PMD_CRYPTO_SCHEDULER
#include <rte_cryptodev_scheduler.h>
#endif

enum cdev_type {
	CDEV_TYPE_ANY,
	CDEV_TYPE_HW,
	CDEV_TYPE_SW
};

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define NB_MBUF   8192

#define MAX_STR_LEN 32
#define MAX_KEY_SIZE 128
#define MAX_IV_SIZE 16
#define MAX_AAD_SIZE 65535
#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */
#define SESSION_POOL_CACHE_SIZE 0

#define MAXIMUM_IV_LENGTH	16
#define IV_OFFSET		(sizeof(struct rte_crypto_op) + \
				sizeof(struct rte_crypto_sym_op))

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 1024

static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct rte_ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
static uint64_t l2fwd_enabled_port_mask;
static uint64_t l2fwd_enabled_crypto_mask;

/* list of enabled ports */
static uint16_t l2fwd_dst_ports[RTE_MAX_ETHPORTS];


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
	L2FWD_CRYPTO_HASH_ONLY,
	L2FWD_CRYPTO_AEAD
};

struct l2fwd_key {
	uint8_t *data;
	uint32_t length;
	rte_iova_t phys_addr;
};

struct l2fwd_iv {
	uint8_t *data;
	uint16_t length;
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
	uint8_t cipher_key[MAX_KEY_SIZE];

	struct l2fwd_iv cipher_iv;
	unsigned int cipher_iv_param;
	int cipher_iv_random_size;

	struct rte_crypto_sym_xform auth_xform;
	uint8_t akey_param;
	int akey_random_size;
	uint8_t auth_key[MAX_KEY_SIZE];

	struct l2fwd_iv auth_iv;
	unsigned int auth_iv_param;
	int auth_iv_random_size;

	struct rte_crypto_sym_xform aead_xform;
	unsigned int aead_key_param;
	int aead_key_random_size;
	uint8_t aead_key[MAX_KEY_SIZE];

	struct l2fwd_iv aead_iv;
	unsigned int aead_iv_param;
	int aead_iv_random_size;

	struct l2fwd_key aad;
	unsigned aad_param;
	int aad_random_size;

	int digest_size;

	uint16_t block_size;
	char string_type[MAX_STR_LEN];

	uint64_t cryptodev_mask;

	unsigned int mac_updating;
};

/** l2fwd crypto lcore params */
struct l2fwd_crypto_params {
	uint8_t dev_id;
	uint8_t qp_id;

	unsigned digest_length;
	unsigned block_size;

	struct l2fwd_iv cipher_iv;
	struct l2fwd_iv auth_iv;
	struct l2fwd_iv aead_iv;
	struct l2fwd_key aad;
	struct rte_cryptodev_sym_session *session;

	uint8_t do_cipher;
	uint8_t do_hash;
	uint8_t do_aead;
	uint8_t hash_verify;

	enum rte_crypto_cipher_algorithm cipher_algo;
	enum rte_crypto_auth_algorithm auth_algo;
	enum rte_crypto_aead_algorithm aead_algo;
};

/** lcore configuration */
struct lcore_queue_conf {
	unsigned nb_rx_ports;
	uint16_t rx_port_list[MAX_RX_QUEUE_PER_LCORE];

	unsigned nb_crypto_devs;
	unsigned cryptodev_list[MAX_RX_QUEUE_PER_LCORE];

	struct op_buffer op_buf[RTE_CRYPTO_MAX_DEVS];
	struct pkt_buffer pkt_buf[RTE_MAX_ETHPORTS];
} __rte_cache_aligned;

struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = ETH_MQ_RX_NONE,
		.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
		.split_hdr_size = 0,
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

struct rte_mempool *l2fwd_pktmbuf_pool;
struct rte_mempool *l2fwd_crypto_op_pool;
static struct {
	struct rte_mempool *sess_mp;
	struct rte_mempool *priv_mp;
} session_pool_socket[RTE_MAX_NUMA_NODES];

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
	uint16_t portid;
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

	fflush(stdout);
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
	struct rte_ether_hdr *eth_hdr;
	struct rte_ipv4_hdr *ip_hdr;

	uint32_t ipdata_offset, data_len;
	uint32_t pad_len = 0;
	char *padding;

	eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	if (eth_hdr->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
		return -1;

	ipdata_offset = sizeof(struct rte_ether_hdr);

	ip_hdr = (struct rte_ipv4_hdr *)(rte_pktmbuf_mtod(m, char *) +
			ipdata_offset);

	ipdata_offset += (ip_hdr->version_ihl & RTE_IPV4_HDR_IHL_MASK)
			* RTE_IPV4_IHL_MULTIPLIER;


	/* Zero pad data to be crypto'd so it is block aligned */
	data_len  = rte_pktmbuf_data_len(m) - ipdata_offset;

	if ((cparams->do_hash || cparams->do_aead) && cparams->hash_verify)
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
		if (cparams->auth_iv.length) {
			uint8_t *iv_ptr = rte_crypto_op_ctod_offset(op,
						uint8_t *,
						IV_OFFSET +
						cparams->cipher_iv.length);
			/*
			 * Copy IV at the end of the crypto operation,
			 * after the cipher IV, if added
			 */
			rte_memcpy(iv_ptr, cparams->auth_iv.data,
					cparams->auth_iv.length);
		}
		if (!cparams->hash_verify) {
			/* Append space for digest to end of packet */
			op->sym->auth.digest.data = (uint8_t *)rte_pktmbuf_append(m,
				cparams->digest_length);
		} else {
			op->sym->auth.digest.data = rte_pktmbuf_mtod(m,
				uint8_t *) + ipdata_offset + data_len;
		}

		op->sym->auth.digest.phys_addr = rte_pktmbuf_iova_offset(m,
				rte_pktmbuf_pkt_len(m) - cparams->digest_length);

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
	}

	if (cparams->do_cipher) {
		uint8_t *iv_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
							IV_OFFSET);
		/* Copy IV at the end of the crypto operation */
		rte_memcpy(iv_ptr, cparams->cipher_iv.data,
				cparams->cipher_iv.length);

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

	if (cparams->do_aead) {
		uint8_t *iv_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
							IV_OFFSET);
		/* Copy IV at the end of the crypto operation */
		/*
		 * If doing AES-CCM, nonce is copied one byte
		 * after the start of IV field
		 */
		if (cparams->aead_algo == RTE_CRYPTO_AEAD_AES_CCM)
			rte_memcpy(iv_ptr + 1, cparams->aead_iv.data,
					cparams->aead_iv.length);
		else
			rte_memcpy(iv_ptr, cparams->aead_iv.data,
					cparams->aead_iv.length);

		op->sym->aead.data.offset = ipdata_offset;
		op->sym->aead.data.length = data_len;

		if (!cparams->hash_verify) {
			/* Append space for digest to end of packet */
			op->sym->aead.digest.data = (uint8_t *)rte_pktmbuf_append(m,
				cparams->digest_length);
		} else {
			op->sym->aead.digest.data = rte_pktmbuf_mtod(m,
				uint8_t *) + ipdata_offset + data_len;
		}

		op->sym->aead.digest.phys_addr = rte_pktmbuf_iova_offset(m,
				rte_pktmbuf_pkt_len(m) - cparams->digest_length);

		if (cparams->aad.length) {
			op->sym->aead.aad.data = cparams->aad.data;
			op->sym->aead.aad.phys_addr = cparams->aad.phys_addr;
		}
	}

	op->sym->m_src = m;

	return l2fwd_crypto_enqueue(op, cparams);
}


/* Send the burst of packets on an output interface */
static int
l2fwd_send_burst(struct lcore_queue_conf *qconf, unsigned n,
		uint16_t port)
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
l2fwd_send_packet(struct rte_mbuf *m, uint16_t port)
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
l2fwd_mac_updating(struct rte_mbuf *m, uint16_t dest_portid)
{
	struct rte_ether_hdr *eth;
	void *tmp;

	eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	/* 02:00:00:00:00:xx */
	tmp = &eth->d_addr.addr_bytes[0];
	*((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)dest_portid << 40);

	/* src addr */
	rte_ether_addr_copy(&l2fwd_ports_eth_addr[dest_portid], &eth->s_addr);
}

static void
l2fwd_simple_forward(struct rte_mbuf *m, uint16_t portid,
		struct l2fwd_crypto_options *options)
{
	uint16_t dst_port;

	dst_port = l2fwd_dst_ports[portid];

	if (options->mac_updating)
		l2fwd_mac_updating(m, dst_port);

	l2fwd_send_packet(m, dst_port);
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
initialize_crypto_session(struct l2fwd_crypto_options *options, uint8_t cdev_id)
{
	struct rte_crypto_sym_xform *first_xform;
	struct rte_cryptodev_sym_session *session;
	int retval = rte_cryptodev_socket_id(cdev_id);

	if (retval < 0)
		return NULL;

	uint8_t socket_id = (uint8_t) retval;

	if (options->xform_chain == L2FWD_CRYPTO_AEAD) {
		first_xform = &options->aead_xform;
	} else if (options->xform_chain == L2FWD_CRYPTO_CIPHER_HASH) {
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

	session = rte_cryptodev_sym_session_create(
			session_pool_socket[socket_id].sess_mp);
	if (session == NULL)
		return NULL;

	if (rte_cryptodev_sym_session_init(cdev_id, session,
				first_xform,
				session_pool_socket[socket_id].priv_mp) < 0)
		return NULL;

	return session;
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
	unsigned int i, j, nb_rx, len;
	uint16_t portid;
	struct lcore_queue_conf *qconf = &lcore_queue_conf[lcore_id];
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) /
			US_PER_S * BURST_TX_DRAIN_US;
	struct l2fwd_crypto_params *cparams;
	struct l2fwd_crypto_params port_cparams[qconf->nb_crypto_devs];
	struct rte_cryptodev_sym_session *session;

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
		port_cparams[i].do_aead = 0;

		switch (options->xform_chain) {
		case L2FWD_CRYPTO_AEAD:
			port_cparams[i].do_aead = 1;
			break;
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
			port_cparams[i].auth_iv.data = options->auth_iv.data;
			port_cparams[i].auth_iv.length = options->auth_iv.length;
			if (!options->auth_iv_param)
				generate_random_key(port_cparams[i].auth_iv.data,
						port_cparams[i].auth_iv.length);
			if (options->auth_xform.auth.op == RTE_CRYPTO_AUTH_OP_VERIFY)
				port_cparams[i].hash_verify = 1;
			else
				port_cparams[i].hash_verify = 0;

			port_cparams[i].auth_algo = options->auth_xform.auth.algo;
			port_cparams[i].digest_length =
					options->auth_xform.auth.digest_length;
			/* Set IV parameters */
			if (options->auth_iv.length) {
				options->auth_xform.auth.iv.offset =
					IV_OFFSET + options->cipher_iv.length;
				options->auth_xform.auth.iv.length =
					options->auth_iv.length;
			}
		}

		if (port_cparams[i].do_aead) {
			port_cparams[i].aead_iv.data = options->aead_iv.data;
			port_cparams[i].aead_iv.length = options->aead_iv.length;
			if (!options->aead_iv_param)
				generate_random_key(port_cparams[i].aead_iv.data,
						port_cparams[i].aead_iv.length);
			port_cparams[i].aead_algo = options->aead_xform.aead.algo;
			port_cparams[i].digest_length =
					options->aead_xform.aead.digest_length;
			if (options->aead_xform.aead.aad_length) {
				port_cparams[i].aad.data = options->aad.data;
				port_cparams[i].aad.phys_addr = options->aad.phys_addr;
				port_cparams[i].aad.length = options->aad.length;
				if (!options->aad_param)
					generate_random_key(port_cparams[i].aad.data,
						port_cparams[i].aad.length);
				/*
				 * If doing AES-CCM, first 18 bytes has to be reserved,
				 * and actual AAD should start from byte 18
				 */
				if (port_cparams[i].aead_algo == RTE_CRYPTO_AEAD_AES_CCM)
					memmove(port_cparams[i].aad.data + 18,
							port_cparams[i].aad.data,
							port_cparams[i].aad.length);

			} else
				port_cparams[i].aad.length = 0;

			if (options->aead_xform.aead.op == RTE_CRYPTO_AEAD_OP_DECRYPT)
				port_cparams[i].hash_verify = 1;
			else
				port_cparams[i].hash_verify = 0;

			/* Set IV parameters */
			options->aead_xform.aead.iv.offset = IV_OFFSET;
			options->aead_xform.aead.iv.length = options->aead_iv.length;
		}

		if (port_cparams[i].do_cipher) {
			port_cparams[i].cipher_iv.data = options->cipher_iv.data;
			port_cparams[i].cipher_iv.length = options->cipher_iv.length;
			if (!options->cipher_iv_param)
				generate_random_key(port_cparams[i].cipher_iv.data,
						port_cparams[i].cipher_iv.length);

			port_cparams[i].cipher_algo = options->cipher_xform.cipher.algo;
			/* Set IV parameters */
			options->cipher_xform.cipher.iv.offset = IV_OFFSET;
			options->cipher_xform.cipher.iv.length =
						options->cipher_iv.length;
		}

		session = initialize_crypto_session(options,
				port_cparams[i].dev_id);
		if (session == NULL)
			rte_exit(EXIT_FAILURE, "Failed to initialize crypto session\n");

		port_cparams[i].session = session;

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
						 portid);
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

			nb_rx = rte_eth_rx_burst(portid, 0,
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
					l2fwd_simple_forward(m, portid,
							options);
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
		"  --chain HASH_CIPHER / CIPHER_HASH / CIPHER_ONLY /"
		" HASH_ONLY / AEAD\n"

		"  --cipher_algo ALGO\n"
		"  --cipher_op ENCRYPT / DECRYPT\n"
		"  --cipher_key KEY (bytes separated with \":\")\n"
		"  --cipher_key_random_size SIZE: size of cipher key when generated randomly\n"
		"  --cipher_iv IV (bytes separated with \":\")\n"
		"  --cipher_iv_random_size SIZE: size of cipher IV when generated randomly\n"

		"  --auth_algo ALGO\n"
		"  --auth_op GENERATE / VERIFY\n"
		"  --auth_key KEY (bytes separated with \":\")\n"
		"  --auth_key_random_size SIZE: size of auth key when generated randomly\n"
		"  --auth_iv IV (bytes separated with \":\")\n"
		"  --auth_iv_random_size SIZE: size of auth IV when generated randomly\n"

		"  --aead_algo ALGO\n"
		"  --aead_op ENCRYPT / DECRYPT\n"
		"  --aead_key KEY (bytes separated with \":\")\n"
		"  --aead_key_random_size SIZE: size of AEAD key when generated randomly\n"
		"  --aead_iv IV (bytes separated with \":\")\n"
		"  --aead_iv_random_size SIZE: size of AEAD IV when generated randomly\n"
		"  --aad AAD (bytes separated with \":\")\n"
		"  --aad_random_size SIZE: size of AAD when generated randomly\n"

		"  --digest_size SIZE: size of digest to be generated/verified\n"

		"  --sessionless\n"
		"  --cryptodev_mask MASK: hexadecimal bitmask of crypto devices to configure\n"

		"  --[no-]mac-updating: Enable or disable MAC addresses updating (enabled by default)\n"
		"      When enabled:\n"
		"       - The source MAC address is replaced by the TX port MAC address\n"
		"       - The destination MAC address is replaced by 02:00:00:00:00:TX_PORT_ID\n",
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
	} else if (strcmp("AEAD", optarg) == 0) {
		options->xform_chain = L2FWD_CRYPTO_AEAD;
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

/** Parse bytes from command line argument */
static int
parse_bytes(uint8_t *data, char *input_arg, uint16_t max_size)
{
	unsigned byte_count;
	char *token;

	errno = 0;
	for (byte_count = 0, token = strtok(input_arg, ":");
			(byte_count < max_size) && (token != NULL);
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
parse_aead_algo(enum rte_crypto_aead_algorithm *algo, char *optarg)
{
	if (rte_cryptodev_get_aead_algo_enum(algo, optarg) < 0) {
		RTE_LOG(ERR, USER1, "AEAD algorithm specified "
				"not supported!\n");
		return -1;
	}

	return 0;
}

static int
parse_aead_op(enum rte_crypto_aead_operation *op, char *optarg)
{
	if (strcmp("ENCRYPT", optarg) == 0) {
		*op = RTE_CRYPTO_AEAD_OP_ENCRYPT;
		return 0;
	} else if (strcmp("DECRYPT", optarg) == 0) {
		*op = RTE_CRYPTO_AEAD_OP_DECRYPT;
		return 0;
	}

	printf("AEAD operation specified not supported!\n");
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
			strlcpy(options->string_type, optarg, MAX_STR_LEN);
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
			parse_bytes(options->cipher_key, optarg, MAX_KEY_SIZE);
		if (options->cipher_xform.cipher.key.length > 0)
			return 0;
		else
			return -1;
	}

	else if (strcmp(lgopts[option_index].name, "cipher_key_random_size") == 0)
		return parse_size(&options->ckey_random_size, optarg);

	else if (strcmp(lgopts[option_index].name, "cipher_iv") == 0) {
		options->cipher_iv_param = 1;
		options->cipher_iv.length =
			parse_bytes(options->cipher_iv.data, optarg, MAX_IV_SIZE);
		if (options->cipher_iv.length > 0)
			return 0;
		else
			return -1;
	}

	else if (strcmp(lgopts[option_index].name, "cipher_iv_random_size") == 0)
		return parse_size(&options->cipher_iv_random_size, optarg);

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
			parse_bytes(options->auth_key, optarg, MAX_KEY_SIZE);
		if (options->auth_xform.auth.key.length > 0)
			return 0;
		else
			return -1;
	}

	else if (strcmp(lgopts[option_index].name, "auth_key_random_size") == 0) {
		return parse_size(&options->akey_random_size, optarg);
	}

	else if (strcmp(lgopts[option_index].name, "auth_iv") == 0) {
		options->auth_iv_param = 1;
		options->auth_iv.length =
			parse_bytes(options->auth_iv.data, optarg, MAX_IV_SIZE);
		if (options->auth_iv.length > 0)
			return 0;
		else
			return -1;
	}

	else if (strcmp(lgopts[option_index].name, "auth_iv_random_size") == 0)
		return parse_size(&options->auth_iv_random_size, optarg);

	/* AEAD options */
	else if (strcmp(lgopts[option_index].name, "aead_algo") == 0) {
		return parse_aead_algo(&options->aead_xform.aead.algo,
				optarg);
	}

	else if (strcmp(lgopts[option_index].name, "aead_op") == 0)
		return parse_aead_op(&options->aead_xform.aead.op,
				optarg);

	else if (strcmp(lgopts[option_index].name, "aead_key") == 0) {
		options->aead_key_param = 1;
		options->aead_xform.aead.key.length =
			parse_bytes(options->aead_key, optarg, MAX_KEY_SIZE);
		if (options->aead_xform.aead.key.length > 0)
			return 0;
		else
			return -1;
	}

	else if (strcmp(lgopts[option_index].name, "aead_key_random_size") == 0)
		return parse_size(&options->aead_key_random_size, optarg);


	else if (strcmp(lgopts[option_index].name, "aead_iv") == 0) {
		options->aead_iv_param = 1;
		options->aead_iv.length =
			parse_bytes(options->aead_iv.data, optarg, MAX_IV_SIZE);
		if (options->aead_iv.length > 0)
			return 0;
		else
			return -1;
	}

	else if (strcmp(lgopts[option_index].name, "aead_iv_random_size") == 0)
		return parse_size(&options->aead_iv_random_size, optarg);

	else if (strcmp(lgopts[option_index].name, "aad") == 0) {
		options->aad_param = 1;
		options->aad.length =
			parse_bytes(options->aad.data, optarg, MAX_AAD_SIZE);
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

	else if (strcmp(lgopts[option_index].name, "mac-updating") == 0) {
		options->mac_updating = 1;
		return 0;
	}

	else if (strcmp(lgopts[option_index].name, "no-mac-updating") == 0) {
		options->mac_updating = 0;
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
	options->cipher_iv_param = 0;
	options->cipher_iv_random_size = -1;
	options->cipher_iv.length = 0;

	options->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	options->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;

	/* Authentication Data */
	options->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	options->auth_xform.next = NULL;
	options->akey_param = 0;
	options->akey_random_size = -1;
	options->auth_xform.auth.key.length = 0;
	options->auth_iv_param = 0;
	options->auth_iv_random_size = -1;
	options->auth_iv.length = 0;

	options->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA1_HMAC;
	options->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;

	/* AEAD Data */
	options->aead_xform.type = RTE_CRYPTO_SYM_XFORM_AEAD;
	options->aead_xform.next = NULL;
	options->aead_key_param = 0;
	options->aead_key_random_size = -1;
	options->aead_xform.aead.key.length = 0;
	options->aead_iv_param = 0;
	options->aead_iv_random_size = -1;
	options->aead_iv.length = 0;

	options->aead_xform.aead.algo = RTE_CRYPTO_AEAD_AES_GCM;
	options->aead_xform.aead.op = RTE_CRYPTO_AEAD_OP_ENCRYPT;

	options->aad_param = 0;
	options->aad_random_size = -1;
	options->aad.length = 0;

	options->digest_size = -1;

	options->type = CDEV_TYPE_ANY;
	options->cryptodev_mask = UINT64_MAX;

	options->mac_updating = 1;
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
	rte_hexdump(stdout, "IV:", options->cipher_iv.data, options->cipher_iv.length);
}

static void
display_auth_info(struct l2fwd_crypto_options *options)
{
	printf("\n---- Authentication information ---\n");
	printf("Algorithm: %s\n",
		rte_crypto_auth_algorithm_strings[options->auth_xform.auth.algo]);
	rte_hexdump(stdout, "Auth key:",
			options->auth_xform.auth.key.data,
			options->auth_xform.auth.key.length);
	rte_hexdump(stdout, "IV:", options->auth_iv.data, options->auth_iv.length);
}

static void
display_aead_info(struct l2fwd_crypto_options *options)
{
	printf("\n---- AEAD information ---\n");
	printf("Algorithm: %s\n",
		rte_crypto_aead_algorithm_strings[options->aead_xform.aead.algo]);
	rte_hexdump(stdout, "AEAD key:",
			options->aead_xform.aead.key.data,
			options->aead_xform.aead.key.length);
	rte_hexdump(stdout, "IV:", options->aead_iv.data, options->aead_iv.length);
	rte_hexdump(stdout, "AAD:", options->aad.data, options->aad.length);
}

static void
l2fwd_crypto_options_print(struct l2fwd_crypto_options *options)
{
	char string_cipher_op[MAX_STR_LEN];
	char string_auth_op[MAX_STR_LEN];
	char string_aead_op[MAX_STR_LEN];

	if (options->cipher_xform.cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
		strcpy(string_cipher_op, "Encrypt");
	else
		strcpy(string_cipher_op, "Decrypt");

	if (options->auth_xform.auth.op == RTE_CRYPTO_AUTH_OP_GENERATE)
		strcpy(string_auth_op, "Auth generate");
	else
		strcpy(string_auth_op, "Auth verify");

	if (options->aead_xform.aead.op == RTE_CRYPTO_AEAD_OP_ENCRYPT)
		strcpy(string_aead_op, "Authenticated encryption");
	else
		strcpy(string_aead_op, "Authenticated decryption");


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

	if (options->cipher_iv_param && (options->cipher_iv_random_size != -1))
		printf("Cipher IV already parsed, ignoring size of random IV\n");

	if (options->auth_iv_param && (options->auth_iv_random_size != -1))
		printf("Auth IV already parsed, ignoring size of random IV\n");

	if (options->aad_param && (options->aad_random_size != -1))
		printf("AAD already parsed, ignoring size of random AAD\n");

	printf("\nCrypto chain: ");
	switch (options->xform_chain) {
	case L2FWD_CRYPTO_AEAD:
		printf("Input --> %s --> Output\n", string_aead_op);
		display_aead_info(options);
		break;
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
			{ "cipher_iv", required_argument, 0, 0 },
			{ "cipher_iv_random_size", required_argument, 0, 0 },

			{ "auth_algo", required_argument, 0, 0 },
			{ "auth_op", required_argument, 0, 0 },
			{ "auth_key", required_argument, 0, 0 },
			{ "auth_key_random_size", required_argument, 0, 0 },
			{ "auth_iv", required_argument, 0, 0 },
			{ "auth_iv_random_size", required_argument, 0, 0 },

			{ "aead_algo", required_argument, 0, 0 },
			{ "aead_op", required_argument, 0, 0 },
			{ "aead_key", required_argument, 0, 0 },
			{ "aead_key_random_size", required_argument, 0, 0 },
			{ "aead_iv", required_argument, 0, 0 },
			{ "aead_iv_random_size", required_argument, 0, 0 },

			{ "aad", required_argument, 0, 0 },
			{ "aad_random_size", required_argument, 0, 0 },

			{ "digest_size", required_argument, 0, 0 },

			{ "sessionless", no_argument, 0, 0 },
			{ "cryptodev_mask", required_argument, 0, 0},

			{ "mac-updating", no_argument, 0, 0},
			{ "no-mac-updating", no_argument, 0, 0},

			{ NULL, 0, 0, 0 }
	};

	l2fwd_crypto_default_options(options);

	while ((opt = getopt_long(argc, argvopt, "p:q:sT:", lgopts,
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
check_all_ports_link_status(uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint16_t portid;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	int ret;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		RTE_ETH_FOREACH_DEV(portid) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			ret = rte_eth_link_get_nowait(portid, &link);
			if (ret < 0) {
				all_ports_up = 0;
				if (print_flag == 1)
					printf("Port %u link get failed: %s\n",
						portid, rte_strerror(-ret));
				continue;
			}
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf(
					"Port%d Link Up. Speed %u Mbps - %s\n",
						portid, link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex"));
				else
					printf("Port %d Link Down\n", portid);
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
check_type(const struct l2fwd_crypto_options *options,
		const struct rte_cryptodev_info *dev_info)
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

static const struct rte_cryptodev_capabilities *
check_device_support_cipher_algo(const struct l2fwd_crypto_options *options,
		const struct rte_cryptodev_info *dev_info,
		uint8_t cdev_id)
{
	unsigned int i = 0;
	const struct rte_cryptodev_capabilities *cap = &dev_info->capabilities[0];
	enum rte_crypto_cipher_algorithm cap_cipher_algo;
	enum rte_crypto_cipher_algorithm opt_cipher_algo =
					options->cipher_xform.cipher.algo;

	while (cap->op != RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		cap_cipher_algo = cap->sym.cipher.algo;
		if (cap->sym.xform_type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
			if (cap_cipher_algo == opt_cipher_algo) {
				if (check_type(options, dev_info) == 0)
					break;
			}
		}
		cap = &dev_info->capabilities[++i];
	}

	if (cap->op == RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		printf("Algorithm %s not supported by cryptodev %u"
			" or device not of preferred type (%s)\n",
			rte_crypto_cipher_algorithm_strings[opt_cipher_algo],
			cdev_id,
			options->string_type);
		return NULL;
	}

	return cap;
}

static const struct rte_cryptodev_capabilities *
check_device_support_auth_algo(const struct l2fwd_crypto_options *options,
		const struct rte_cryptodev_info *dev_info,
		uint8_t cdev_id)
{
	unsigned int i = 0;
	const struct rte_cryptodev_capabilities *cap = &dev_info->capabilities[0];
	enum rte_crypto_auth_algorithm cap_auth_algo;
	enum rte_crypto_auth_algorithm opt_auth_algo =
					options->auth_xform.auth.algo;

	while (cap->op != RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		cap_auth_algo = cap->sym.auth.algo;
		if (cap->sym.xform_type == RTE_CRYPTO_SYM_XFORM_AUTH) {
			if (cap_auth_algo == opt_auth_algo) {
				if (check_type(options, dev_info) == 0)
					break;
			}
		}
		cap = &dev_info->capabilities[++i];
	}

	if (cap->op == RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		printf("Algorithm %s not supported by cryptodev %u"
			" or device not of preferred type (%s)\n",
			rte_crypto_auth_algorithm_strings[opt_auth_algo],
			cdev_id,
			options->string_type);
		return NULL;
	}

	return cap;
}

static const struct rte_cryptodev_capabilities *
check_device_support_aead_algo(const struct l2fwd_crypto_options *options,
		const struct rte_cryptodev_info *dev_info,
		uint8_t cdev_id)
{
	unsigned int i = 0;
	const struct rte_cryptodev_capabilities *cap = &dev_info->capabilities[0];
	enum rte_crypto_aead_algorithm cap_aead_algo;
	enum rte_crypto_aead_algorithm opt_aead_algo =
					options->aead_xform.aead.algo;

	while (cap->op != RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		cap_aead_algo = cap->sym.aead.algo;
		if (cap->sym.xform_type == RTE_CRYPTO_SYM_XFORM_AEAD) {
			if (cap_aead_algo == opt_aead_algo) {
				if (check_type(options, dev_info) == 0)
					break;
			}
		}
		cap = &dev_info->capabilities[++i];
	}

	if (cap->op == RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		printf("Algorithm %s not supported by cryptodev %u"
			" or device not of preferred type (%s)\n",
			rte_crypto_aead_algorithm_strings[opt_aead_algo],
			cdev_id,
			options->string_type);
		return NULL;
	}

	return cap;
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
check_iv_param(const struct rte_crypto_param_range *iv_range_size,
		unsigned int iv_param, int iv_random_size,
		uint16_t iv_length)
{
	/*
	 * Check if length of provided IV is supported
	 * by the algorithm chosen.
	 */
	if (iv_param) {
		if (check_supported_size(iv_length,
				iv_range_size->min,
				iv_range_size->max,
				iv_range_size->increment)
					!= 0)
			return -1;
	/*
	 * Check if length of IV to be randomly generated
	 * is supported by the algorithm chosen.
	 */
	} else if (iv_random_size != -1) {
		if (check_supported_size(iv_random_size,
				iv_range_size->min,
				iv_range_size->max,
				iv_range_size->increment)
					!= 0)
			return -1;
	}

	return 0;
}

static int
check_capabilities(struct l2fwd_crypto_options *options, uint8_t cdev_id)
{
	struct rte_cryptodev_info dev_info;
	const struct rte_cryptodev_capabilities *cap;

	rte_cryptodev_info_get(cdev_id, &dev_info);

	/* Set AEAD parameters */
	if (options->xform_chain == L2FWD_CRYPTO_AEAD) {
		/* Check if device supports AEAD algo */
		cap = check_device_support_aead_algo(options, &dev_info,
						cdev_id);
		if (cap == NULL)
			return -1;

		if (check_iv_param(&cap->sym.aead.iv_size,
				options->aead_iv_param,
				options->aead_iv_random_size,
				options->aead_iv.length) != 0) {
			RTE_LOG(DEBUG, USER1,
				"Device %u does not support IV length\n",
				cdev_id);
			return -1;
		}

		/*
		 * Check if length of provided AEAD key is supported
		 * by the algorithm chosen.
		 */
		if (options->aead_key_param) {
			if (check_supported_size(
					options->aead_xform.aead.key.length,
					cap->sym.aead.key_size.min,
					cap->sym.aead.key_size.max,
					cap->sym.aead.key_size.increment)
						!= 0) {
				RTE_LOG(DEBUG, USER1,
					"Device %u does not support "
					"AEAD key length\n",
					cdev_id);
				return -1;
			}
		/*
		 * Check if length of the aead key to be randomly generated
		 * is supported by the algorithm chosen.
		 */
		} else if (options->aead_key_random_size != -1) {
			if (check_supported_size(options->aead_key_random_size,
					cap->sym.aead.key_size.min,
					cap->sym.aead.key_size.max,
					cap->sym.aead.key_size.increment)
						!= 0) {
				RTE_LOG(DEBUG, USER1,
					"Device %u does not support "
					"AEAD key length\n",
					cdev_id);
				return -1;
			}
		}


		/*
		 * Check if length of provided AAD is supported
		 * by the algorithm chosen.
		 */
		if (options->aad_param) {
			if (check_supported_size(options->aad.length,
					cap->sym.aead.aad_size.min,
					cap->sym.aead.aad_size.max,
					cap->sym.aead.aad_size.increment)
						!= 0) {
				RTE_LOG(DEBUG, USER1,
					"Device %u does not support "
					"AAD length\n",
					cdev_id);
				return -1;
			}
		/*
		 * Check if length of AAD to be randomly generated
		 * is supported by the algorithm chosen.
		 */
		} else if (options->aad_random_size != -1) {
			if (check_supported_size(options->aad_random_size,
					cap->sym.aead.aad_size.min,
					cap->sym.aead.aad_size.max,
					cap->sym.aead.aad_size.increment)
						!= 0) {
				RTE_LOG(DEBUG, USER1,
					"Device %u does not support "
					"AAD length\n",
					cdev_id);
				return -1;
			}
		}

		/* Check if digest size is supported by the algorithm. */
		if (options->digest_size != -1) {
			if (check_supported_size(options->digest_size,
					cap->sym.aead.digest_size.min,
					cap->sym.aead.digest_size.max,
					cap->sym.aead.digest_size.increment)
						!= 0) {
				RTE_LOG(DEBUG, USER1,
					"Device %u does not support "
					"digest length\n",
					cdev_id);
				return -1;
			}
		}
	}

	/* Set cipher parameters */
	if (options->xform_chain == L2FWD_CRYPTO_CIPHER_HASH ||
			options->xform_chain == L2FWD_CRYPTO_HASH_CIPHER ||
			options->xform_chain == L2FWD_CRYPTO_CIPHER_ONLY) {
		/* Check if device supports cipher algo */
		cap = check_device_support_cipher_algo(options, &dev_info,
						cdev_id);
		if (cap == NULL)
			return -1;

		if (check_iv_param(&cap->sym.cipher.iv_size,
				options->cipher_iv_param,
				options->cipher_iv_random_size,
				options->cipher_iv.length) != 0) {
			RTE_LOG(DEBUG, USER1,
				"Device %u does not support IV length\n",
				cdev_id);
			return -1;
		}

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
				RTE_LOG(DEBUG, USER1,
					"Device %u does not support cipher "
					"key length\n",
					cdev_id);
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
				RTE_LOG(DEBUG, USER1,
					"Device %u does not support cipher "
					"key length\n",
					cdev_id);
				return -1;
			}
		}
	}

	/* Set auth parameters */
	if (options->xform_chain == L2FWD_CRYPTO_CIPHER_HASH ||
			options->xform_chain == L2FWD_CRYPTO_HASH_CIPHER ||
			options->xform_chain == L2FWD_CRYPTO_HASH_ONLY) {
		/* Check if device supports auth algo */
		cap = check_device_support_auth_algo(options, &dev_info,
						cdev_id);
		if (cap == NULL)
			return -1;

		if (check_iv_param(&cap->sym.auth.iv_size,
				options->auth_iv_param,
				options->auth_iv_random_size,
				options->auth_iv.length) != 0) {
			RTE_LOG(DEBUG, USER1,
				"Device %u does not support IV length\n",
				cdev_id);
			return -1;
		}
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
				RTE_LOG(DEBUG, USER1,
					"Device %u does not support auth "
					"key length\n",
					cdev_id);
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
				RTE_LOG(DEBUG, USER1,
					"Device %u does not support auth "
					"key length\n",
					cdev_id);
				return -1;
			}
		}

		/* Check if digest size is supported by the algorithm. */
		if (options->digest_size != -1) {
			if (check_supported_size(options->digest_size,
					cap->sym.auth.digest_size.min,
					cap->sym.auth.digest_size.max,
					cap->sym.auth.digest_size.increment)
						!= 0) {
				RTE_LOG(DEBUG, USER1,
					"Device %u does not support "
					"digest length\n",
					cdev_id);
				return -1;
			}
		}
	}

	return 0;
}

static int
initialize_cryptodevs(struct l2fwd_crypto_options *options, unsigned nb_ports,
		uint8_t *enabled_cdevs)
{
	uint8_t cdev_id, cdev_count, enabled_cdev_count = 0;
	const struct rte_cryptodev_capabilities *cap;
	unsigned int sess_sz, max_sess_sz = 0;
	uint32_t sessions_needed = 0;
	int retval;

	cdev_count = rte_cryptodev_count();
	if (cdev_count == 0) {
		printf("No crypto devices available\n");
		return -1;
	}

	for (cdev_id = 0; cdev_id < cdev_count && enabled_cdev_count < nb_ports;
			cdev_id++) {
		if (check_cryptodev_mask(options, cdev_id) < 0)
			continue;

		if (check_capabilities(options, cdev_id) < 0)
			continue;

		sess_sz = rte_cryptodev_sym_get_private_session_size(cdev_id);
		if (sess_sz > max_sess_sz)
			max_sess_sz = sess_sz;

		l2fwd_enabled_crypto_mask |= (((uint64_t)1) << cdev_id);

		enabled_cdevs[cdev_id] = 1;
		enabled_cdev_count++;
	}

	for (cdev_id = 0; cdev_id < cdev_count; cdev_id++) {
		struct rte_cryptodev_qp_conf qp_conf;
		struct rte_cryptodev_info dev_info;

		if (enabled_cdevs[cdev_id] == 0)
			continue;

		retval = rte_cryptodev_socket_id(cdev_id);

		if (retval < 0) {
			printf("Invalid crypto device id used\n");
			return -1;
		}

		uint8_t socket_id = (uint8_t) retval;

		struct rte_cryptodev_config conf = {
			.nb_queue_pairs = 1,
			.socket_id = socket_id,
			.ff_disable = RTE_CRYPTODEV_FF_SECURITY,
		};

		rte_cryptodev_info_get(cdev_id, &dev_info);

		/*
		 * Two sessions objects are required for each session
		 * (one for the header, one for the private data)
		 */
		if (!strcmp(dev_info.driver_name, "crypto_scheduler")) {
#ifdef RTE_LIBRTE_PMD_CRYPTO_SCHEDULER
			uint32_t nb_slaves =
				rte_cryptodev_scheduler_slaves_get(cdev_id,
								NULL);

			sessions_needed = enabled_cdev_count * nb_slaves;
#endif
		} else
			sessions_needed = enabled_cdev_count;

		if (session_pool_socket[socket_id].priv_mp == NULL) {
			char mp_name[RTE_MEMPOOL_NAMESIZE];

			snprintf(mp_name, RTE_MEMPOOL_NAMESIZE,
				"priv_sess_mp_%u", socket_id);

			session_pool_socket[socket_id].priv_mp =
					rte_mempool_create(mp_name,
						sessions_needed,
						max_sess_sz,
						0, 0, NULL, NULL, NULL,
						NULL, socket_id,
						0);

			if (session_pool_socket[socket_id].priv_mp == NULL) {
				printf("Cannot create pool on socket %d\n",
					socket_id);
				return -ENOMEM;
			}

			printf("Allocated pool \"%s\" on socket %d\n",
				mp_name, socket_id);
		}

		if (session_pool_socket[socket_id].sess_mp == NULL) {
			char mp_name[RTE_MEMPOOL_NAMESIZE];
			snprintf(mp_name, RTE_MEMPOOL_NAMESIZE,
				"sess_mp_%u", socket_id);

			session_pool_socket[socket_id].sess_mp =
					rte_cryptodev_sym_session_pool_create(
							mp_name,
							sessions_needed,
							0, 0, 0, socket_id);

			if (session_pool_socket[socket_id].sess_mp == NULL) {
				printf("Cannot create pool on socket %d\n",
					socket_id);
				return -ENOMEM;
			}

			printf("Allocated pool \"%s\" on socket %d\n",
				mp_name, socket_id);
		}

		/* Set AEAD parameters */
		if (options->xform_chain == L2FWD_CRYPTO_AEAD) {
			cap = check_device_support_aead_algo(options, &dev_info,
							cdev_id);

			options->block_size = cap->sym.aead.block_size;

			/* Set IV if not provided from command line */
			if (options->aead_iv_param == 0) {
				if (options->aead_iv_random_size != -1)
					options->aead_iv.length =
						options->aead_iv_random_size;
				/* No size provided, use minimum size. */
				else
					options->aead_iv.length =
						cap->sym.aead.iv_size.min;
			}

			/* Set key if not provided from command line */
			if (options->aead_key_param == 0) {
				if (options->aead_key_random_size != -1)
					options->aead_xform.aead.key.length =
						options->aead_key_random_size;
				/* No size provided, use minimum size. */
				else
					options->aead_xform.aead.key.length =
						cap->sym.aead.key_size.min;

				generate_random_key(options->aead_key,
					options->aead_xform.aead.key.length);
			}

			/* Set AAD if not provided from command line */
			if (options->aad_param == 0) {
				if (options->aad_random_size != -1)
					options->aad.length =
						options->aad_random_size;
				/* No size provided, use minimum size. */
				else
					options->aad.length =
						cap->sym.auth.aad_size.min;
			}

			options->aead_xform.aead.aad_length =
						options->aad.length;

			/* Set digest size if not provided from command line */
			if (options->digest_size != -1)
				options->aead_xform.aead.digest_length =
							options->digest_size;
				/* No size provided, use minimum size. */
			else
				options->aead_xform.aead.digest_length =
						cap->sym.aead.digest_size.min;
		}

		/* Set cipher parameters */
		if (options->xform_chain == L2FWD_CRYPTO_CIPHER_HASH ||
				options->xform_chain == L2FWD_CRYPTO_HASH_CIPHER ||
				options->xform_chain == L2FWD_CRYPTO_CIPHER_ONLY) {
			cap = check_device_support_cipher_algo(options, &dev_info,
							cdev_id);
			options->block_size = cap->sym.cipher.block_size;

			/* Set IV if not provided from command line */
			if (options->cipher_iv_param == 0) {
				if (options->cipher_iv_random_size != -1)
					options->cipher_iv.length =
						options->cipher_iv_random_size;
				/* No size provided, use minimum size. */
				else
					options->cipher_iv.length =
						cap->sym.cipher.iv_size.min;
			}

			/* Set key if not provided from command line */
			if (options->ckey_param == 0) {
				if (options->ckey_random_size != -1)
					options->cipher_xform.cipher.key.length =
						options->ckey_random_size;
				/* No size provided, use minimum size. */
				else
					options->cipher_xform.cipher.key.length =
						cap->sym.cipher.key_size.min;

				generate_random_key(options->cipher_key,
					options->cipher_xform.cipher.key.length);
			}
		}

		/* Set auth parameters */
		if (options->xform_chain == L2FWD_CRYPTO_CIPHER_HASH ||
				options->xform_chain == L2FWD_CRYPTO_HASH_CIPHER ||
				options->xform_chain == L2FWD_CRYPTO_HASH_ONLY) {
			cap = check_device_support_auth_algo(options, &dev_info,
							cdev_id);

			/* Set IV if not provided from command line */
			if (options->auth_iv_param == 0) {
				if (options->auth_iv_random_size != -1)
					options->auth_iv.length =
						options->auth_iv_random_size;
				/* No size provided, use minimum size. */
				else
					options->auth_iv.length =
						cap->sym.auth.iv_size.min;
			}

			/* Set key if not provided from command line */
			if (options->akey_param == 0) {
				if (options->akey_random_size != -1)
					options->auth_xform.auth.key.length =
						options->akey_random_size;
				/* No size provided, use minimum size. */
				else
					options->auth_xform.auth.key.length =
						cap->sym.auth.key_size.min;

				generate_random_key(options->auth_key,
					options->auth_xform.auth.key.length);
			}

			/* Set digest size if not provided from command line */
			if (options->digest_size != -1)
				options->auth_xform.auth.digest_length =
							options->digest_size;
				/* No size provided, use minimum size. */
			else
				options->auth_xform.auth.digest_length =
						cap->sym.auth.digest_size.min;
		}

		retval = rte_cryptodev_configure(cdev_id, &conf);
		if (retval < 0) {
			printf("Failed to configure cryptodev %u", cdev_id);
			return -1;
		}

		qp_conf.nb_descriptors = 2048;
		qp_conf.mp_session = session_pool_socket[socket_id].sess_mp;
		qp_conf.mp_session_private =
				session_pool_socket[socket_id].priv_mp;

		retval = rte_cryptodev_queue_pair_setup(cdev_id, 0, &qp_conf,
				socket_id);
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
	}

	return enabled_cdev_count;
}

static int
initialize_ports(struct l2fwd_crypto_options *options)
{
	uint16_t last_portid = 0, portid;
	unsigned enabled_portcount = 0;
	unsigned nb_ports = rte_eth_dev_count_avail();

	if (nb_ports == 0) {
		printf("No Ethernet ports - bye\n");
		return -1;
	}

	/* Reset l2fwd_dst_ports */
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
		l2fwd_dst_ports[portid] = 0;

	RTE_ETH_FOREACH_DEV(portid) {
		int retval;
		struct rte_eth_dev_info dev_info;
		struct rte_eth_rxconf rxq_conf;
		struct rte_eth_txconf txq_conf;
		struct rte_eth_conf local_port_conf = port_conf;

		/* Skip ports that are not enabled */
		if ((options->portmask & (1 << portid)) == 0)
			continue;

		/* init port */
		printf("Initializing port %u... ", portid);
		fflush(stdout);

		retval = rte_eth_dev_info_get(portid, &dev_info);
		if (retval != 0) {
			printf("Error during getting device (port %u) info: %s\n",
					portid, strerror(-retval));
			return retval;
		}

		if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
			local_port_conf.txmode.offloads |=
				DEV_TX_OFFLOAD_MBUF_FAST_FREE;
		retval = rte_eth_dev_configure(portid, 1, 1, &local_port_conf);
		if (retval < 0) {
			printf("Cannot configure device: err=%d, port=%u\n",
				  retval, portid);
			return -1;
		}

		retval = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
							  &nb_txd);
		if (retval < 0) {
			printf("Cannot adjust number of descriptors: err=%d, port=%u\n",
				retval, portid);
			return -1;
		}

		/* init one RX queue */
		fflush(stdout);
		rxq_conf = dev_info.default_rxconf;
		rxq_conf.offloads = local_port_conf.rxmode.offloads;
		retval = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
					     rte_eth_dev_socket_id(portid),
					     &rxq_conf, l2fwd_pktmbuf_pool);
		if (retval < 0) {
			printf("rte_eth_rx_queue_setup:err=%d, port=%u\n",
					retval, portid);
			return -1;
		}

		/* init one TX queue on each port */
		fflush(stdout);
		txq_conf = dev_info.default_txconf;
		txq_conf.offloads = local_port_conf.txmode.offloads;
		retval = rte_eth_tx_queue_setup(portid, 0, nb_txd,
				rte_eth_dev_socket_id(portid),
				&txq_conf);
		if (retval < 0) {
			printf("rte_eth_tx_queue_setup:err=%d, port=%u\n",
				retval, portid);

			return -1;
		}

		/* Start device */
		retval = rte_eth_dev_start(portid);
		if (retval < 0) {
			printf("rte_eth_dev_start:err=%d, port=%u\n",
					retval, portid);
			return -1;
		}

		retval = rte_eth_promiscuous_enable(portid);
		if (retval != 0) {
			printf("rte_eth_promiscuous_enable:err=%s, port=%u\n",
				rte_strerror(-retval), portid);
			return -1;
		}

		retval = rte_eth_macaddr_get(portid,
					     &l2fwd_ports_eth_addr[portid]);
		if (retval < 0) {
			printf("rte_eth_macaddr_get :err=%d, port=%u\n",
					retval, portid);
			return -1;
		}

		printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
				portid,
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

	check_all_ports_link_status(l2fwd_enabled_port_mask);

	return enabled_portcount;
}

static void
reserve_key_memory(struct l2fwd_crypto_options *options)
{
	options->cipher_xform.cipher.key.data = options->cipher_key;

	options->auth_xform.auth.key.data = options->auth_key;

	options->aead_xform.aead.key.data = options->aead_key;

	options->cipher_iv.data = rte_malloc("cipher iv", MAX_KEY_SIZE, 0);
	if (options->cipher_iv.data == NULL)
		rte_exit(EXIT_FAILURE, "Failed to allocate memory for cipher IV");

	options->auth_iv.data = rte_malloc("auth iv", MAX_KEY_SIZE, 0);
	if (options->auth_iv.data == NULL)
		rte_exit(EXIT_FAILURE, "Failed to allocate memory for auth IV");

	options->aead_iv.data = rte_malloc("aead_iv", MAX_KEY_SIZE, 0);
	if (options->aead_iv.data == NULL)
		rte_exit(EXIT_FAILURE, "Failed to allocate memory for AEAD iv");

	options->aad.data = rte_malloc("aad", MAX_KEY_SIZE, 0);
	if (options->aad.data == NULL)
		rte_exit(EXIT_FAILURE, "Failed to allocate memory for AAD");
	options->aad.phys_addr = rte_malloc_virt2iova(options->aad.data);
}

int
main(int argc, char **argv)
{
	struct lcore_queue_conf *qconf = NULL;
	struct l2fwd_crypto_options options;

	uint8_t nb_cryptodevs, cdev_id;
	uint16_t portid;
	unsigned lcore_id, rx_lcore_id = 0;
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

	printf("MAC updating %s\n",
			options.mac_updating ? "enabled" : "disabled");

	/* create the mbuf pool */
	l2fwd_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF, 512,
			sizeof(struct rte_crypto_op),
			RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (l2fwd_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* create crypto op pool */
	l2fwd_crypto_op_pool = rte_crypto_op_pool_create("crypto_op_pool",
			RTE_CRYPTO_OP_TYPE_SYMMETRIC, NB_MBUF, 128, MAXIMUM_IV_LENGTH,
			rte_socket_id());
	if (l2fwd_crypto_op_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create crypto op pool\n");

	/* Enable Ethernet ports */
	enabled_portcount = initialize_ports(&options);
	if (enabled_portcount < 1)
		rte_exit(EXIT_FAILURE, "Failed to initial Ethernet ports\n");

	/* Initialize the port/queue configuration of each logical core */
	RTE_ETH_FOREACH_DEV(portid) {

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

		printf("Lcore %u: RX port %u\n", rx_lcore_id, portid);
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
