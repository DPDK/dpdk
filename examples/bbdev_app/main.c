/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <assert.h>
#include <getopt.h>
#include <signal.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_cycles.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_log.h>
#include <rte_bbdev.h>
#include <rte_bbdev_op.h>

/* LLR values - negative value for '1' bit */
#define LLR_1_BIT 0x81
#define LLR_0_BIT 0x7F

#define MAX_PKT_BURST 32
#define NB_MBUF 8191
#define MEMPOOL_CACHE_SIZE 256

/* Hardcoded K value */
#define K 40
#define NCB (3 * RTE_ALIGN_CEIL(K + 4, 32))

#define CRC_24B_LEN 3

/* Configurable number of RX/TX ring descriptors */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512

#define BBDEV_ASSERT(a) do { \
	if (!(a)) { \
		usage(prgname); \
		return -1; \
	} \
} while (0)

static int input_dynfield_offset = -1;

static inline struct rte_mbuf **
mbuf_input(struct rte_mbuf *mbuf)
{
	return RTE_MBUF_DYNFIELD(mbuf,
			input_dynfield_offset, struct rte_mbuf **);
}

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_NONE,
		.split_hdr_size = 0,
	},
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
};

struct rte_bbdev_op_turbo_enc def_op_enc = {
	/* These values are arbitrarily put, and does not map to the real
	 * values for the data received from ethdev ports
	 */
	.rv_index = 0,
	.code_block_mode = 1,
	.cb_params = {
		.k = K,
	},
	.op_flags = RTE_BBDEV_TURBO_CRC_24A_ATTACH
};

struct rte_bbdev_op_turbo_dec def_op_dec = {
	/* These values are arbitrarily put, and does not map to the real
	 * values for the data received from ethdev ports
	 */
	.code_block_mode = 1,
	.cb_params = {
		.k = K,
	},
	.rv_index = 0,
	.iter_max = 8,
	.iter_min = 4,
	.ext_scale = 15,
	.num_maps = 0,
	.op_flags = RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN
};

struct app_config_params {
	/* Placeholders for app params */
	uint16_t port_id;
	uint16_t bbdev_id;
	uint64_t enc_core_mask;
	uint64_t dec_core_mask;

	/* Values filled during init time */
	uint16_t enc_queue_ids[RTE_MAX_LCORE];
	uint16_t dec_queue_ids[RTE_MAX_LCORE];
	uint16_t num_enc_cores;
	uint16_t num_dec_cores;
};

struct lcore_statistics {
	unsigned int enqueued;
	unsigned int dequeued;
	unsigned int rx_lost_packets;
	unsigned int enc_to_dec_lost_packets;
	unsigned int tx_lost_packets;
} __rte_cache_aligned;

/** each lcore configuration */
struct lcore_conf {
	uint64_t core_type;

	unsigned int port_id;
	unsigned int rx_queue_id;
	unsigned int tx_queue_id;

	unsigned int bbdev_id;
	unsigned int enc_queue_id;
	unsigned int dec_queue_id;

	uint8_t llr_temp_buf[NCB];

	struct rte_mempool *bbdev_dec_op_pool;
	struct rte_mempool *bbdev_enc_op_pool;
	struct rte_mempool *enc_out_pool;
	struct rte_ring *enc_to_dec_ring;

	struct lcore_statistics *lcore_stats;
} __rte_cache_aligned;

struct stats_lcore_params {
	struct lcore_conf *lconf;
	struct app_config_params *app_params;
};


static const struct app_config_params def_app_config = {
	.port_id = 0,
	.bbdev_id = 0,
	.enc_core_mask = 0x2,
	.dec_core_mask = 0x4,
	.num_enc_cores = 1,
	.num_dec_cores = 1,
};

static uint16_t global_exit_flag;

/* display usage */
static inline void
usage(const char *prgname)
{
	printf("%s [EAL options] "
			"  --\n"
			"  --enc_cores - number of encoding cores (default = 0x2)\n"
			"  --dec_cores - number of decoding cores (default = 0x4)\n"
			"  --port_id - Ethernet port ID (default = 0)\n"
			"  --bbdev_id - BBDev ID (default = 0)\n"
			"\n", prgname);
}

/* parse core mask */
static inline
uint16_t bbdev_parse_mask(const char *mask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(mask, &end, 16);
	if ((mask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return pm;
}

/* parse core mask */
static inline
uint16_t bbdev_parse_number(const char *mask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(mask, &end, 10);
	if ((mask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return pm;
}

static int
bbdev_parse_args(int argc, char **argv,
		struct app_config_params *app_params)
{
	int optind = 0;
	int opt;
	int opt_indx = 0;
	char *prgname = argv[0];

	static struct option lgopts[] = {
		{ "enc_core_mask", required_argument, 0, 'e' },
		{ "dec_core_mask", required_argument, 0, 'd' },
		{ "port_id", required_argument, 0, 'p' },
		{ "bbdev_id", required_argument, 0, 'b' },
		{ NULL, 0, 0, 0 }
	};

	BBDEV_ASSERT(argc != 0);
	BBDEV_ASSERT(argv != NULL);
	BBDEV_ASSERT(app_params != NULL);

	while ((opt = getopt_long(argc, argv, "e:d:p:b:", lgopts, &opt_indx)) !=
		EOF) {
		switch (opt) {
		case 'e':
			app_params->enc_core_mask =
				bbdev_parse_mask(optarg);
			if (app_params->enc_core_mask == 0) {
				usage(prgname);
				return -1;
			}
			app_params->num_enc_cores =
				__builtin_popcount(app_params->enc_core_mask);
			break;

		case 'd':
			app_params->dec_core_mask =
				bbdev_parse_mask(optarg);
			if (app_params->dec_core_mask == 0) {
				usage(prgname);
				return -1;
			}
			app_params->num_dec_cores =
				__builtin_popcount(app_params->dec_core_mask);
			break;

		case 'p':
			app_params->port_id = bbdev_parse_number(optarg);
			break;

		case 'b':
			app_params->bbdev_id = bbdev_parse_number(optarg);
			break;

		default:
			usage(prgname);
			return -1;
		}
	}
	optind = 0;
	return optind;
}

static void
signal_handler(int signum)
{
	printf("\nSignal %d received\n", signum);
	__atomic_store_n(&global_exit_flag, 1, __ATOMIC_RELAXED);
}

static void
print_mac(unsigned int portid, struct rte_ether_addr *bbdev_ports_eth_address)
{
	printf("Port %u, MAC address: " RTE_ETHER_ADDR_PRT_FMT "\n\n",
			(unsigned int) portid,
			RTE_ETHER_ADDR_BYTES(bbdev_ports_eth_address));
}

static inline void
pktmbuf_free_bulk(struct rte_mbuf **mbufs, unsigned int nb_to_free)
{
	unsigned int i;
	for (i = 0; i < nb_to_free; ++i)
		rte_pktmbuf_free(mbufs[i]);
}

static inline void
pktmbuf_input_free_bulk(struct rte_mbuf **mbufs, unsigned int nb_to_free)
{
	unsigned int i;
	for (i = 0; i < nb_to_free; ++i) {
		struct rte_mbuf *rx_pkt = *mbuf_input(mbufs[i]);
		rte_pktmbuf_free(rx_pkt);
		rte_pktmbuf_free(mbufs[i]);
	}
}

/* Check the link status of all ports in up to 9s, and print them finally */
static int
check_port_link_status(uint16_t port_id)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t count;
	struct rte_eth_link link;
	int link_get_err = -EINVAL;

	printf("\nChecking link status.");
	fflush(stdout);

	for (count = 0; count <= MAX_CHECK_TIME &&
			!__atomic_load_n(&global_exit_flag, __ATOMIC_RELAXED); count++) {
		memset(&link, 0, sizeof(link));
		link_get_err = rte_eth_link_get_nowait(port_id, &link);

		if (link_get_err >= 0 && link.link_status) {
			const char *dp = (link.link_duplex ==
				RTE_ETH_LINK_FULL_DUPLEX) ?
				"full-duplex" : "half-duplex";
			printf("\nPort %u Link Up - speed %s - %s\n",
				port_id,
				rte_eth_link_speed_to_str(link.link_speed),
				dp);
			return 0;
		}
		printf(".");
		fflush(stdout);
		rte_delay_ms(CHECK_INTERVAL);
	}

	if (link_get_err >= 0)
		printf("\nPort %d Link Down\n", port_id);
	else
		printf("\nGet link failed (port %d): %s\n", port_id,
		       rte_strerror(-link_get_err));

	return 0;
}

static inline void
add_ether_hdr(struct rte_mbuf *pkt_src, struct rte_mbuf *pkt_dst)
{
	struct rte_ether_hdr *eth_from;
	struct rte_ether_hdr *eth_to;

	eth_from = rte_pktmbuf_mtod(pkt_src, struct rte_ether_hdr *);
	eth_to = rte_pktmbuf_mtod(pkt_dst, struct rte_ether_hdr *);

	/* copy header */
	rte_memcpy(eth_to, eth_from, sizeof(struct rte_ether_hdr));
}

static inline void
add_awgn(struct rte_mbuf **mbufs, uint16_t num_pkts)
{
	RTE_SET_USED(mbufs);
	RTE_SET_USED(num_pkts);
}

/* Encoder output to Decoder input adapter. The Decoder accepts only soft input
 * so each bit of the encoder output must be translated into one byte of LLR. If
 * Sub-block Deinterleaver is bypassed, which is the case, the padding bytes
 * must additionally be inserted at the end of each sub-block.
 */
static inline void
transform_enc_out_dec_in(struct rte_mbuf **mbufs, uint8_t *temp_buf,
		uint16_t num_pkts, uint16_t k)
{
	uint16_t i, l, j;
	uint16_t start_bit_idx;
	uint16_t out_idx;
	uint16_t d = k + 4;
	uint16_t kpi = RTE_ALIGN_CEIL(d, 32);
	uint16_t nd = kpi - d;
	uint16_t ncb = 3 * kpi;

	for (i = 0; i < num_pkts; ++i) {
		uint16_t pkt_data_len = rte_pktmbuf_data_len(mbufs[i]) -
				sizeof(struct rte_ether_hdr);

		/* Resize the packet if needed */
		if (pkt_data_len < ncb) {
			char *data = rte_pktmbuf_append(mbufs[i],
					ncb - pkt_data_len);
			if (data == NULL)
				printf(
					"Not enough space in decoder input packet");
		}

		/* Translate each bit into 1 LLR byte. */
		start_bit_idx = 0;
		out_idx = 0;
		for (j = 0; j < 3; ++j) {
			for (l = start_bit_idx; l < start_bit_idx + d; ++l) {
				uint8_t *data = rte_pktmbuf_mtod_offset(
					mbufs[i], uint8_t *,
					sizeof(struct rte_ether_hdr) +
					(l >> 3));
				if (*data & (0x80 >> (l & 7)))
					temp_buf[out_idx] = LLR_1_BIT;
				else
					temp_buf[out_idx] = LLR_0_BIT;
				++out_idx;
			}
			/* Padding bytes should be at the end of the sub-block.
			 */
			memset(&temp_buf[out_idx], 0, nd);
			out_idx += nd;
			start_bit_idx += d;
		}

		rte_memcpy(rte_pktmbuf_mtod_offset(mbufs[i], uint8_t *,
				sizeof(struct rte_ether_hdr)), temp_buf, ncb);
	}
}

static inline void
verify_data(struct rte_mbuf **mbufs, uint16_t num_pkts)
{
	uint16_t i;
	for (i = 0; i < num_pkts; ++i) {
		struct rte_mbuf *out = mbufs[i];
		struct rte_mbuf *in = *mbuf_input(out);

		if (memcmp(rte_pktmbuf_mtod_offset(in, uint8_t *,
				sizeof(struct rte_ether_hdr)),
				rte_pktmbuf_mtod_offset(out, uint8_t *,
				sizeof(struct rte_ether_hdr)),
				K / 8 - CRC_24B_LEN))
			printf("Input and output buffers are not equal!\n");
	}
}

static int
initialize_ports(struct app_config_params *app_params,
		struct rte_mempool *ethdev_mbuf_mempool)
{
	int ret;
	uint16_t port_id = app_params->port_id;
	uint16_t q;
	/* ethernet addresses of ports */
	struct rte_ether_addr bbdev_port_eth_addr;

	/* initialize ports */
	printf("\nInitializing port %u...\n", app_params->port_id);
	ret = rte_eth_dev_configure(port_id, app_params->num_enc_cores,
		app_params->num_dec_cores, &port_conf);

	if (ret < 0) {
		printf("Cannot configure device: err=%d, port=%u\n",
			ret, port_id);
		return -1;
	}

	/* initialize RX queues for encoder */
	for (q = 0; q < app_params->num_enc_cores; q++) {
		ret = rte_eth_rx_queue_setup(port_id, q,
			RTE_TEST_RX_DESC_DEFAULT,
			rte_eth_dev_socket_id(port_id),
			NULL, ethdev_mbuf_mempool);
		if (ret < 0) {
			printf("rte_eth_rx_queue_setup: err=%d, queue=%u\n",
				ret, q);
			return -1;
		}
	}
	/* initialize TX queues for decoder */
	for (q = 0; q < app_params->num_dec_cores; q++) {
		ret = rte_eth_tx_queue_setup(port_id, q,
			RTE_TEST_TX_DESC_DEFAULT,
			rte_eth_dev_socket_id(port_id), NULL);
		if (ret < 0) {
			printf("rte_eth_tx_queue_setup: err=%d, queue=%u\n",
				ret, q);
			return -1;
		}
	}

	ret = rte_eth_promiscuous_enable(port_id);
	if (ret != 0) {
		printf("Cannot enable promiscuous mode: err=%s, port=%u\n",
			rte_strerror(-ret), port_id);
		return ret;
	}

	ret = rte_eth_macaddr_get(port_id, &bbdev_port_eth_addr);
	if (ret < 0) {
		printf("rte_eth_macaddr_get: err=%d, queue=%u\n",
			ret, q);
		return -1;
	}

	print_mac(port_id, &bbdev_port_eth_addr);

	return 0;
}

static void
lcore_conf_init(struct app_config_params *app_params,
		struct lcore_conf *lcore_conf,
		struct rte_mempool **bbdev_op_pools,
		struct rte_mempool *bbdev_mbuf_mempool,
		struct rte_ring *enc_to_dec_ring,
		struct lcore_statistics *lcore_stats)
{
	unsigned int lcore_id;
	struct lcore_conf *lconf;
	uint16_t rx_queue_id = 0;
	uint16_t tx_queue_id = 0;
	uint16_t enc_q_id = 0;
	uint16_t dec_q_id = 0;

	/* Configure lcores */
	for (lcore_id = 0; lcore_id < 8 * sizeof(uint64_t); ++lcore_id) {
		lconf = &lcore_conf[lcore_id];
		lconf->core_type = 0;

		if ((1ULL << lcore_id) & app_params->enc_core_mask) {
			lconf->core_type |= (1 << RTE_BBDEV_OP_TURBO_ENC);
			lconf->rx_queue_id = rx_queue_id++;
			lconf->enc_queue_id =
					app_params->enc_queue_ids[enc_q_id++];
		}

		if ((1ULL << lcore_id) & app_params->dec_core_mask) {
			lconf->core_type |= (1 << RTE_BBDEV_OP_TURBO_DEC);
			lconf->tx_queue_id = tx_queue_id++;
			lconf->dec_queue_id =
					app_params->dec_queue_ids[dec_q_id++];
		}

		lconf->bbdev_enc_op_pool =
				bbdev_op_pools[RTE_BBDEV_OP_TURBO_ENC];
		lconf->bbdev_dec_op_pool =
				bbdev_op_pools[RTE_BBDEV_OP_TURBO_DEC];
		lconf->bbdev_id = app_params->bbdev_id;
		lconf->port_id = app_params->port_id;
		lconf->enc_out_pool = bbdev_mbuf_mempool;
		lconf->enc_to_dec_ring = enc_to_dec_ring;
		lconf->lcore_stats = &lcore_stats[lcore_id];
	}
}

static void
print_lcore_stats(struct lcore_statistics *lstats, unsigned int lcore_id)
{
	static const char *stats_border = "_______";

	printf("\nLcore %d: %s enqueued count:\t\t%u\n",
			lcore_id, stats_border, lstats->enqueued);
	printf("Lcore %d: %s dequeued count:\t\t%u\n",
			lcore_id, stats_border, lstats->dequeued);
	printf("Lcore %d: %s RX lost packets count:\t\t%u\n",
			lcore_id, stats_border, lstats->rx_lost_packets);
	printf("Lcore %d: %s encoder-to-decoder lost count:\t%u\n",
			lcore_id, stats_border,
			lstats->enc_to_dec_lost_packets);
	printf("Lcore %d: %s TX lost packets count:\t\t%u\n",
			lcore_id, stats_border, lstats->tx_lost_packets);
}

static void
print_stats(struct stats_lcore_params *stats_lcore)
{
	unsigned int l_id;
	unsigned int bbdev_id = stats_lcore->app_params->bbdev_id;
	unsigned int port_id = stats_lcore->app_params->port_id;
	int len, ret, i;

	struct rte_eth_xstat *xstats;
	struct rte_eth_xstat_name *xstats_names;
	struct rte_bbdev_stats bbstats;
	static const char *stats_border = "_______";

	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

	/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("PORT STATISTICS:\n================\n");
	len = rte_eth_xstats_get(port_id, NULL, 0);
	if (len < 0)
		rte_exit(EXIT_FAILURE,
				"rte_eth_xstats_get(%u) failed: %d", port_id,
				len);

	xstats = calloc(len, sizeof(*xstats));
	if (xstats == NULL)
		rte_exit(EXIT_FAILURE,
				"Failed to calloc memory for xstats");

	ret = rte_eth_xstats_get(port_id, xstats, len);
	if (ret < 0 || ret > len) {
		free(xstats);
		rte_exit(EXIT_FAILURE,
				"rte_eth_xstats_get(%u) len%i failed: %d",
				port_id, len, ret);
	}

	xstats_names = calloc(len, sizeof(*xstats_names));
	if (xstats_names == NULL) {
		free(xstats);
		rte_exit(EXIT_FAILURE,
				"Failed to calloc memory for xstats_names");
	}

	ret = rte_eth_xstats_get_names(port_id, xstats_names, len);
	if (ret < 0 || ret > len) {
		free(xstats);
		free(xstats_names);
		rte_exit(EXIT_FAILURE,
				"rte_eth_xstats_get_names(%u) len%i failed: %d",
				port_id, len, ret);
	}

	for (i = 0; i < len; i++) {
		if (xstats[i].value > 0)
			printf("Port %u: %s %s:\t\t%"PRIu64"\n",
					port_id, stats_border,
					xstats_names[i].name,
					xstats[i].value);
	}

	ret = rte_bbdev_stats_get(bbdev_id, &bbstats);
	if (ret < 0) {
		free(xstats);
		free(xstats_names);
		rte_exit(EXIT_FAILURE,
				"ERROR(%d): Failure to get BBDEV %u statistics\n",
				ret, bbdev_id);
	}

	printf("\nBBDEV STATISTICS:\n=================\n");
	printf("BBDEV %u: %s enqueue count:\t\t%"PRIu64"\n",
			bbdev_id, stats_border,
			bbstats.enqueued_count);
	printf("BBDEV %u: %s dequeue count:\t\t%"PRIu64"\n",
			bbdev_id, stats_border,
			bbstats.dequeued_count);
	printf("BBDEV %u: %s enqueue error count:\t\t%"PRIu64"\n",
			bbdev_id, stats_border,
			bbstats.enqueue_err_count);
	printf("BBDEV %u: %s dequeue error count:\t\t%"PRIu64"\n\n",
			bbdev_id, stats_border,
			bbstats.dequeue_err_count);

	printf("LCORE STATISTICS:\n=================\n");
	for (l_id = 0; l_id < RTE_MAX_LCORE; ++l_id) {
		if (stats_lcore->lconf[l_id].core_type == 0)
			continue;
		print_lcore_stats(stats_lcore->lconf[l_id].lcore_stats, l_id);
	}

	fflush(stdout);

	free(xstats);
	free(xstats_names);
}

static int
stats_loop(void *arg)
{
	struct stats_lcore_params *stats_lcore = arg;

	while (!__atomic_load_n(&global_exit_flag, __ATOMIC_RELAXED)) {
		print_stats(stats_lcore);
		rte_delay_ms(500);
	}

	return 0;
}

static inline void
run_encoding(struct lcore_conf *lcore_conf)
{
	uint16_t i;
	uint16_t port_id, rx_queue_id;
	uint16_t bbdev_id, enc_queue_id;
	uint16_t nb_rx, nb_enq, nb_deq, nb_sent;
	struct rte_mbuf *rx_pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *enc_out_pkts[MAX_PKT_BURST];
	struct rte_bbdev_enc_op *bbdev_ops_burst[MAX_PKT_BURST];
	struct lcore_statistics *lcore_stats;
	struct rte_mempool *bbdev_op_pool, *enc_out_pool;
	struct rte_ring *enc_to_dec_ring;
	const int in_data_len = (def_op_enc.cb_params.k / 8) - CRC_24B_LEN;

	lcore_stats = lcore_conf->lcore_stats;
	port_id = lcore_conf->port_id;
	rx_queue_id = lcore_conf->rx_queue_id;
	bbdev_id = lcore_conf->bbdev_id;
	enc_queue_id = lcore_conf->enc_queue_id;
	bbdev_op_pool = lcore_conf->bbdev_enc_op_pool;
	enc_out_pool = lcore_conf->enc_out_pool;
	enc_to_dec_ring = lcore_conf->enc_to_dec_ring;

	/* Read packet from RX queues*/
	nb_rx = rte_eth_rx_burst(port_id, rx_queue_id, rx_pkts_burst,
			MAX_PKT_BURST);
	if (!nb_rx)
		return;

	if (unlikely(rte_mempool_get_bulk(enc_out_pool, (void **)enc_out_pkts,
			nb_rx) != 0)) {
		pktmbuf_free_bulk(rx_pkts_burst, nb_rx);
		lcore_stats->rx_lost_packets += nb_rx;
		return;
	}

	if (unlikely(rte_bbdev_enc_op_alloc_bulk(bbdev_op_pool, bbdev_ops_burst,
			nb_rx) != 0)) {
		pktmbuf_free_bulk(enc_out_pkts, nb_rx);
		pktmbuf_free_bulk(rx_pkts_burst, nb_rx);
		lcore_stats->rx_lost_packets += nb_rx;
		return;
	}

	for (i = 0; i < nb_rx; i++) {
		char *data;
		const uint16_t pkt_data_len =
				rte_pktmbuf_data_len(rx_pkts_burst[i]) -
				sizeof(struct rte_ether_hdr);
		/* save input mbuf pointer for later comparison */
		*mbuf_input(enc_out_pkts[i]) = rx_pkts_burst[i];

		/* copy ethernet header */
		rte_pktmbuf_reset(enc_out_pkts[i]);
		data = rte_pktmbuf_append(enc_out_pkts[i],
				sizeof(struct rte_ether_hdr));
		if (data == NULL) {
			printf(
				"Not enough space for ethernet header in encoder output mbuf\n");
			continue;
		}
		add_ether_hdr(rx_pkts_burst[i], enc_out_pkts[i]);

		/* set op */
		bbdev_ops_burst[i]->turbo_enc = def_op_enc;

		bbdev_ops_burst[i]->turbo_enc.input.data =
				rx_pkts_burst[i];
		bbdev_ops_burst[i]->turbo_enc.input.offset =
				sizeof(struct rte_ether_hdr);
		/* Encoder will attach the CRC24B, adjust the length */
		bbdev_ops_burst[i]->turbo_enc.input.length = in_data_len;

		if (in_data_len < pkt_data_len)
			rte_pktmbuf_trim(rx_pkts_burst[i], pkt_data_len -
					in_data_len);
		else if (in_data_len > pkt_data_len) {
			data = rte_pktmbuf_append(rx_pkts_burst[i],
					in_data_len - pkt_data_len);
			if (data == NULL)
				printf(
					"Not enough storage in mbuf to perform the encoding\n");
		}

		bbdev_ops_burst[i]->turbo_enc.output.data =
				enc_out_pkts[i];
		bbdev_ops_burst[i]->turbo_enc.output.offset =
				sizeof(struct rte_ether_hdr);
	}

	/* Enqueue packets on BBDevice */
	nb_enq = rte_bbdev_enqueue_enc_ops(bbdev_id, enc_queue_id,
			bbdev_ops_burst, nb_rx);
	if (unlikely(nb_enq < nb_rx)) {
		pktmbuf_input_free_bulk(&enc_out_pkts[nb_enq],
				nb_rx - nb_enq);
		rte_bbdev_enc_op_free_bulk(&bbdev_ops_burst[nb_enq],
				nb_rx - nb_enq);
		lcore_stats->rx_lost_packets += nb_rx - nb_enq;

		if (!nb_enq)
			return;
	}

	lcore_stats->enqueued += nb_enq;

	/* Dequeue packets from bbdev device*/
	nb_deq = 0;
	do {
		nb_deq += rte_bbdev_dequeue_enc_ops(bbdev_id, enc_queue_id,
				&bbdev_ops_burst[nb_deq], nb_enq - nb_deq);
	} while (unlikely(nb_deq < nb_enq));

	lcore_stats->dequeued += nb_deq;

	/* Generate and add AWGN */
	add_awgn(enc_out_pkts, nb_deq);

	rte_bbdev_enc_op_free_bulk(bbdev_ops_burst, nb_deq);

	/* Enqueue packets to encoder-to-decoder ring */
	nb_sent = rte_ring_enqueue_burst(enc_to_dec_ring, (void **)enc_out_pkts,
			nb_deq, NULL);
	if (unlikely(nb_sent < nb_deq)) {
		pktmbuf_input_free_bulk(&enc_out_pkts[nb_sent],
				nb_deq - nb_sent);
		lcore_stats->enc_to_dec_lost_packets += nb_deq - nb_sent;
	}
}

static void
run_decoding(struct lcore_conf *lcore_conf)
{
	uint16_t i;
	uint16_t port_id, tx_queue_id;
	uint16_t bbdev_id, bbdev_queue_id;
	uint16_t nb_recv, nb_enq, nb_deq, nb_tx;
	uint8_t *llr_temp_buf;
	struct rte_mbuf *recv_pkts_burst[MAX_PKT_BURST];
	struct rte_bbdev_dec_op *bbdev_ops_burst[MAX_PKT_BURST];
	struct lcore_statistics *lcore_stats;
	struct rte_mempool *bbdev_op_pool;
	struct rte_ring *enc_to_dec_ring;

	lcore_stats = lcore_conf->lcore_stats;
	port_id = lcore_conf->port_id;
	tx_queue_id = lcore_conf->tx_queue_id;
	bbdev_id = lcore_conf->bbdev_id;
	bbdev_queue_id = lcore_conf->dec_queue_id;
	bbdev_op_pool = lcore_conf->bbdev_dec_op_pool;
	enc_to_dec_ring = lcore_conf->enc_to_dec_ring;
	llr_temp_buf = lcore_conf->llr_temp_buf;

	/* Dequeue packets from the ring */
	nb_recv = rte_ring_dequeue_burst(enc_to_dec_ring,
			(void **)recv_pkts_burst, MAX_PKT_BURST, NULL);
	if (!nb_recv)
		return;

	if (unlikely(rte_bbdev_dec_op_alloc_bulk(bbdev_op_pool, bbdev_ops_burst,
			nb_recv) != 0)) {
		pktmbuf_input_free_bulk(recv_pkts_burst, nb_recv);
		lcore_stats->rx_lost_packets += nb_recv;
		return;
	}

	transform_enc_out_dec_in(recv_pkts_burst, llr_temp_buf, nb_recv,
			def_op_dec.cb_params.k);

	for (i = 0; i < nb_recv; i++) {
		/* set op */
		bbdev_ops_burst[i]->turbo_dec = def_op_dec;

		bbdev_ops_burst[i]->turbo_dec.input.data = recv_pkts_burst[i];
		bbdev_ops_burst[i]->turbo_dec.input.offset =
				sizeof(struct rte_ether_hdr);
		bbdev_ops_burst[i]->turbo_dec.input.length =
				rte_pktmbuf_data_len(recv_pkts_burst[i])
				- sizeof(struct rte_ether_hdr);

		bbdev_ops_burst[i]->turbo_dec.hard_output.data =
				recv_pkts_burst[i];
		bbdev_ops_burst[i]->turbo_dec.hard_output.offset =
				sizeof(struct rte_ether_hdr);
	}

	/* Enqueue packets on BBDevice */
	nb_enq = rte_bbdev_enqueue_dec_ops(bbdev_id, bbdev_queue_id,
			bbdev_ops_burst, nb_recv);
	if (unlikely(nb_enq < nb_recv)) {
		pktmbuf_input_free_bulk(&recv_pkts_burst[nb_enq],
				nb_recv - nb_enq);
		rte_bbdev_dec_op_free_bulk(&bbdev_ops_burst[nb_enq],
				nb_recv - nb_enq);
		lcore_stats->rx_lost_packets += nb_recv - nb_enq;

		if (!nb_enq)
			return;
	}

	lcore_stats->enqueued += nb_enq;

	/* Dequeue packets from BBDevice */
	nb_deq = 0;
	do {
		nb_deq += rte_bbdev_dequeue_dec_ops(bbdev_id, bbdev_queue_id,
				&bbdev_ops_burst[nb_deq], nb_enq - nb_deq);
	} while (unlikely(nb_deq < nb_enq));

	lcore_stats->dequeued += nb_deq;

	rte_bbdev_dec_op_free_bulk(bbdev_ops_burst, nb_deq);

	verify_data(recv_pkts_burst, nb_deq);

	/* Free the RX mbufs after verification */
	for (i = 0; i < nb_deq; ++i)
		rte_pktmbuf_free(*mbuf_input(recv_pkts_burst[i]));

	/* Transmit the packets */
	nb_tx = rte_eth_tx_burst(port_id, tx_queue_id, recv_pkts_burst, nb_deq);
	if (unlikely(nb_tx < nb_deq)) {
		pktmbuf_input_free_bulk(&recv_pkts_burst[nb_tx],
				nb_deq - nb_tx);
		lcore_stats->tx_lost_packets += nb_deq - nb_tx;
	}
}

static int
processing_loop(void *arg)
{
	struct lcore_conf *lcore_conf = arg;
	const bool run_encoder = (lcore_conf->core_type &
			(1 << RTE_BBDEV_OP_TURBO_ENC));
	const bool run_decoder = (lcore_conf->core_type &
			(1 << RTE_BBDEV_OP_TURBO_DEC));

	while (!__atomic_load_n(&global_exit_flag, __ATOMIC_RELAXED)) {
		if (run_encoder)
			run_encoding(lcore_conf);
		if (run_decoder)
			run_decoding(lcore_conf);
	}

	return 0;
}

static int
prepare_bbdev_device(unsigned int dev_id, struct rte_bbdev_info *info,
		struct app_config_params *app_params)
{
	int ret;
	unsigned int q_id, dec_q_id, enc_q_id;
	struct rte_bbdev_queue_conf qconf = {0};
	uint16_t dec_qs_nb = app_params->num_dec_cores;
	uint16_t enc_qs_nb = app_params->num_enc_cores;
	uint16_t tot_qs = dec_qs_nb + enc_qs_nb;

	ret = rte_bbdev_setup_queues(dev_id, tot_qs, info->socket_id);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
				"ERROR(%d): BBDEV %u not configured properly\n",
				ret, dev_id);

	/* setup device DEC queues */
	qconf.socket = info->socket_id;
	qconf.queue_size = info->drv.queue_size_lim;
	qconf.op_type = RTE_BBDEV_OP_TURBO_DEC;

	for (q_id = 0, dec_q_id = 0; q_id < dec_qs_nb; q_id++) {
		ret = rte_bbdev_queue_configure(dev_id, q_id, &qconf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
					"ERROR(%d): BBDEV %u DEC queue %u not configured properly\n",
					ret, dev_id, q_id);
		app_params->dec_queue_ids[dec_q_id++] = q_id;
	}

	/* setup device ENC queues */
	qconf.op_type = RTE_BBDEV_OP_TURBO_ENC;

	for (q_id = dec_qs_nb, enc_q_id = 0; q_id < tot_qs; q_id++) {
		ret = rte_bbdev_queue_configure(dev_id, q_id, &qconf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
					"ERROR(%d): BBDEV %u ENC queue %u not configured properly\n",
					ret, dev_id, q_id);
		app_params->enc_queue_ids[enc_q_id++] = q_id;
	}

	ret = rte_bbdev_start(dev_id);

	if (ret != 0)
		rte_exit(EXIT_FAILURE, "ERROR(%d): BBDEV %u not started\n",
			ret, dev_id);

	printf("BBdev %u started\n", dev_id);

	return 0;
}

static inline bool
check_matching_capabilities(uint64_t mask, uint64_t required_mask)
{
	return (mask & required_mask) == required_mask;
}

static void
enable_bbdev(struct app_config_params *app_params)
{
	struct rte_bbdev_info dev_info;
	const struct rte_bbdev_op_cap *op_cap;
	uint16_t bbdev_id = app_params->bbdev_id;
	bool encoder_capable = false;
	bool decoder_capable = false;

	rte_bbdev_info_get(bbdev_id, &dev_info);
	op_cap = dev_info.drv.capabilities;

	while (op_cap->type != RTE_BBDEV_OP_NONE) {
		if (op_cap->type == RTE_BBDEV_OP_TURBO_ENC) {
			if (check_matching_capabilities(
					op_cap->cap.turbo_enc.capability_flags,
					def_op_enc.op_flags))
				encoder_capable = true;
		}

		if (op_cap->type == RTE_BBDEV_OP_TURBO_DEC) {
			if (check_matching_capabilities(
					op_cap->cap.turbo_dec.capability_flags,
					def_op_dec.op_flags))
				decoder_capable = true;
		}

		op_cap++;
	}

	if (encoder_capable == false)
		rte_exit(EXIT_FAILURE,
			"The specified BBDev %u doesn't have required encoder capabilities!\n",
			bbdev_id);
	if (decoder_capable == false)
		rte_exit(EXIT_FAILURE,
			"The specified BBDev %u doesn't have required decoder capabilities!\n",
			bbdev_id);

	prepare_bbdev_device(bbdev_id, &dev_info, app_params);
}

int
main(int argc, char **argv)
{
	int ret;
	unsigned int nb_bbdevs, flags, lcore_id;
	void *sigret;
	struct app_config_params app_params = def_app_config;
	struct rte_mempool *ethdev_mbuf_mempool, *bbdev_mbuf_mempool;
	struct rte_mempool *bbdev_op_pools[RTE_BBDEV_OP_TYPE_COUNT];
	struct lcore_conf lcore_conf[RTE_MAX_LCORE] = { {0} };
	struct lcore_statistics lcore_stats[RTE_MAX_LCORE] = { {0} };
	struct stats_lcore_params stats_lcore;
	struct rte_ring *enc_to_dec_ring;
	bool stats_thread_started = false;
	unsigned int main_lcore_id = rte_get_main_lcore();

	static const struct rte_mbuf_dynfield input_dynfield_desc = {
		.name = "example_bbdev_dynfield_input",
		.size = sizeof(struct rte_mbuf *),
		.align = __alignof__(struct rte_mbuf *),
	};

	__atomic_store_n(&global_exit_flag, 0, __ATOMIC_RELAXED);

	sigret = signal(SIGTERM, signal_handler);
	if (sigret == SIG_ERR)
		rte_exit(EXIT_FAILURE, "signal(%d, ...) failed", SIGTERM);

	sigret = signal(SIGINT, signal_handler);
	if (sigret == SIG_ERR)
		rte_exit(EXIT_FAILURE, "signal(%d, ...) failed", SIGINT);

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");

	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = bbdev_parse_args(argc, argv, &app_params);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid BBDEV arguments\n");

	/*create bbdev op pools*/
	bbdev_op_pools[RTE_BBDEV_OP_TURBO_DEC] =
			rte_bbdev_op_pool_create("bbdev_op_pool_dec",
			RTE_BBDEV_OP_TURBO_DEC, NB_MBUF, 128, rte_socket_id());
	bbdev_op_pools[RTE_BBDEV_OP_TURBO_ENC] =
			rte_bbdev_op_pool_create("bbdev_op_pool_enc",
			RTE_BBDEV_OP_TURBO_ENC, NB_MBUF, 128, rte_socket_id());

	if ((bbdev_op_pools[RTE_BBDEV_OP_TURBO_DEC] == NULL) ||
			(bbdev_op_pools[RTE_BBDEV_OP_TURBO_ENC] == NULL))
		rte_exit(EXIT_FAILURE, "Cannot create bbdev op pools\n");

	/* Create encoder to decoder ring */
	flags = (app_params.num_enc_cores == 1) ? RING_F_SP_ENQ : 0;
	if (app_params.num_dec_cores == 1)
		flags |= RING_F_SC_DEQ;

	enc_to_dec_ring = rte_ring_create("enc_to_dec_ring",
		rte_align32pow2(NB_MBUF), rte_socket_id(), flags);

	/* Get the number of available bbdev devices */
	nb_bbdevs = rte_bbdev_count();
	if (nb_bbdevs <= app_params.bbdev_id)
		rte_exit(EXIT_FAILURE,
				"%u BBDevs detected, cannot use BBDev with ID %u!\n",
				nb_bbdevs, app_params.bbdev_id);
	printf("Number of bbdevs detected: %d\n", nb_bbdevs);

	if (!rte_eth_dev_is_valid_port(app_params.port_id))
		rte_exit(EXIT_FAILURE,
				"cannot use port with ID %u!\n",
				app_params.port_id);

	/* create the mbuf mempool for ethdev pkts */
	ethdev_mbuf_mempool = rte_pktmbuf_pool_create("ethdev_mbuf_pool",
			NB_MBUF, MEMPOOL_CACHE_SIZE, 0,
			RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (ethdev_mbuf_mempool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create ethdev mbuf mempool\n");

	/* create the mbuf mempool for encoder output */
	bbdev_mbuf_mempool = rte_pktmbuf_pool_create("bbdev_mbuf_pool",
			NB_MBUF, MEMPOOL_CACHE_SIZE, 0,
			RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (bbdev_mbuf_mempool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create ethdev mbuf mempool\n");

	/* register mbuf field to store input pointer */
	input_dynfield_offset =
		rte_mbuf_dynfield_register(&input_dynfield_desc);
	if (input_dynfield_offset < 0)
		rte_exit(EXIT_FAILURE, "Cannot register mbuf field\n");

	/* initialize ports */
	ret = initialize_ports(&app_params, ethdev_mbuf_mempool);

	/* Check if all requested lcores are available */
	for (lcore_id = 0; lcore_id < 8 * sizeof(uint64_t); ++lcore_id)
		if (((1ULL << lcore_id) & app_params.enc_core_mask) ||
				((1ULL << lcore_id) & app_params.dec_core_mask))
			if (!rte_lcore_is_enabled(lcore_id))
				rte_exit(EXIT_FAILURE,
						"Requested lcore_id %u is not enabled!\n",
						lcore_id);

	/* Start ethernet port */
	ret = rte_eth_dev_start(app_params.port_id);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
				ret, app_params.port_id);

	ret = check_port_link_status(app_params.port_id);
	if (ret < 0)
		exit(EXIT_FAILURE);

	/* start BBDevice and save BBDev queue IDs */
	enable_bbdev(&app_params);

	/* Initialize the port/queue configuration of each logical core */
	lcore_conf_init(&app_params, lcore_conf, bbdev_op_pools,
			bbdev_mbuf_mempool, enc_to_dec_ring, lcore_stats);

	stats_lcore.app_params = &app_params;
	stats_lcore.lconf = lcore_conf;

	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (lcore_conf[lcore_id].core_type != 0)
			/* launch per-lcore processing loop on worker lcores */
			rte_eal_remote_launch(processing_loop,
					&lcore_conf[lcore_id], lcore_id);
		else if (!stats_thread_started) {
			/* launch statistics printing loop */
			rte_eal_remote_launch(stats_loop, &stats_lcore,
					lcore_id);
			stats_thread_started = true;
		}
	}

	if (!stats_thread_started &&
			lcore_conf[main_lcore_id].core_type != 0)
		rte_exit(EXIT_FAILURE,
				"Not enough lcores to run the statistics printing loop!");
	else if (lcore_conf[main_lcore_id].core_type != 0)
		processing_loop(&lcore_conf[main_lcore_id]);
	else if (!stats_thread_started)
		stats_loop(&stats_lcore);

	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		ret |= rte_eal_wait_lcore(lcore_id);
	}

	/* clean up the EAL */
	rte_eal_cleanup();

	return ret;
}
