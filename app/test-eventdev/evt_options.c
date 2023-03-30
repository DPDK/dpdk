/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>

#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_eventdev.h>
#include <rte_lcore.h>

#include "evt_options.h"
#include "evt_test.h"
#include "parser.h"

void
evt_options_default(struct evt_options *opt)
{
	memset(opt, 0, sizeof(*opt));
	opt->verbose_level = 1; /* Enable minimal prints */
	opt->dev_id = 0;
	strncpy(opt->test_name, "order_queue", EVT_TEST_NAME_MAX_LEN);
	opt->nb_flows = 1024;
	opt->socket_id = SOCKET_ID_ANY;
	opt->pool_sz = 16 * 1024;
	opt->prod_enq_burst_sz = 1;
	opt->wkr_deq_dep = 16;
	opt->nb_pkts = (1ULL << 26); /* do ~64M packets */
	opt->nb_timers = 1E8;
	opt->nb_timer_adptrs = 1;
	opt->timer_tick_nsec = 1E3; /* 1000ns ~ 1us */
	opt->max_tmo_nsec = 1E5;  /* 100000ns ~100us */
	opt->expiry_nsec = 1E4;   /* 10000ns ~10us */
	opt->prod_type = EVT_PROD_TYPE_SYNT;
	opt->eth_queues = 1;
	opt->vector_size = 64;
	opt->vector_tmo_nsec = 100E3;
	opt->crypto_op_type = RTE_CRYPTO_OP_TYPE_SYMMETRIC;
	opt->crypto_cipher_alg = RTE_CRYPTO_CIPHER_NULL;
	opt->crypto_cipher_key_sz = 0;
}

typedef int (*option_parser_t)(struct evt_options *opt,
		const char *arg);

struct long_opt_parser {
	const char *lgopt_name;
	option_parser_t parser_fn;
};

static int
evt_parse_nb_flows(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint32(&(opt->nb_flows), arg);

	return ret;
}

static int
evt_parse_dev_id(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint8(&(opt->dev_id), arg);

	return ret;
}

static int
evt_parse_verbose(struct evt_options *opt, const char *arg __rte_unused)
{
	opt->verbose_level = atoi(arg);
	return 0;
}

static int
evt_parse_fwd_latency(struct evt_options *opt, const char *arg __rte_unused)
{
	opt->fwd_latency = 1;
	return 0;
}

static int
evt_parse_queue_priority(struct evt_options *opt, const char *arg __rte_unused)
{
	opt->q_priority = 1;
	return 0;
}

static int
evt_parse_deq_tmo_nsec(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint32(&(opt->deq_tmo_nsec), arg);

	return ret;
}

static int
evt_parse_eth_prod_type(struct evt_options *opt, const char *arg __rte_unused)
{
	opt->prod_type = EVT_PROD_TYPE_ETH_RX_ADPTR;
	return 0;
}

static int
evt_parse_tx_first(struct evt_options *opt, const char *arg __rte_unused)
{
	int ret;

	ret = parser_read_uint32(&(opt->tx_first), arg);

	return ret;
}

static int
evt_parse_tx_pkt_sz(struct evt_options *opt, const char *arg __rte_unused)
{
	int ret;

	ret = parser_read_uint16(&(opt->tx_pkt_sz), arg);

	return ret;
}

static int
evt_parse_timer_prod_type(struct evt_options *opt, const char *arg __rte_unused)
{
	opt->prod_type = EVT_PROD_TYPE_EVENT_TIMER_ADPTR;
	return 0;
}

static int
evt_parse_timer_prod_type_burst(struct evt_options *opt,
		const char *arg __rte_unused)
{
	opt->prod_type = EVT_PROD_TYPE_EVENT_TIMER_ADPTR;
	opt->timdev_use_burst = 1;
	return 0;
}

static int
evt_parse_crypto_prod_type(struct evt_options *opt,
			   const char *arg __rte_unused)
{
	opt->prod_type = EVT_PROD_TYPE_EVENT_CRYPTO_ADPTR;
	return 0;
}

static int
evt_parse_crypto_adptr_mode(struct evt_options *opt, const char *arg)
{
	uint8_t mode;
	int ret;

	ret = parser_read_uint8(&mode, arg);
	opt->crypto_adptr_mode = mode ? RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD :
					RTE_EVENT_CRYPTO_ADAPTER_OP_NEW;
	return ret;
}

static int
evt_parse_crypto_op_type(struct evt_options *opt, const char *arg)
{
	uint8_t op_type;
	int ret;

	ret = parser_read_uint8(&op_type, arg);
	opt->crypto_op_type = op_type ? RTE_CRYPTO_OP_TYPE_ASYMMETRIC :
					RTE_CRYPTO_OP_TYPE_SYMMETRIC;
	return ret;
}

static bool
cipher_alg_is_bit_mode(enum rte_crypto_cipher_algorithm alg)
{
	return (alg == RTE_CRYPTO_CIPHER_SNOW3G_UEA2 ||
		alg == RTE_CRYPTO_CIPHER_ZUC_EEA3 ||
		alg == RTE_CRYPTO_CIPHER_KASUMI_F8);
}

static int
evt_parse_crypto_cipher_alg(struct evt_options *opt, const char *arg)
{
	enum rte_crypto_cipher_algorithm cipher_alg;

	if (rte_cryptodev_get_cipher_algo_enum(&cipher_alg, arg) < 0) {
		RTE_LOG(ERR, USER1, "Invalid cipher algorithm specified\n");
		return -1;
	}

	opt->crypto_cipher_alg = cipher_alg;
	opt->crypto_cipher_bit_mode = cipher_alg_is_bit_mode(cipher_alg);

	return 0;
}

static int
evt_parse_crypto_cipher_key(struct evt_options *opt, const char *arg)
{
	opt->crypto_cipher_key_sz = EVT_CRYPTO_MAX_KEY_SIZE;
	if (parse_hex_string(arg, opt->crypto_cipher_key,
			     (uint32_t *)&opt->crypto_cipher_key_sz)) {
		RTE_LOG(ERR, USER1, "Invalid cipher key specified\n");
		return -1;
	}

	return 0;
}

static int
evt_parse_crypto_cipher_iv_sz(struct evt_options *opt, const char *arg)
{
	uint16_t iv_sz;
	int ret;

	ret = parser_read_uint16(&(iv_sz), arg);
	if (iv_sz > EVT_CRYPTO_MAX_IV_SIZE) {
		RTE_LOG(ERR, USER1,
			"Unsupported cipher IV length [%d] specified\n",
			iv_sz);
		return -1;
	}

	opt->crypto_cipher_iv_sz = iv_sz;
	return ret;
}

static int
evt_parse_test_name(struct evt_options *opt, const char *arg)
{
	strlcpy(opt->test_name, arg, EVT_TEST_NAME_MAX_LEN);
	return 0;
}

static int
evt_parse_socket_id(struct evt_options *opt, const char *arg)
{
	opt->socket_id = atoi(arg);
	return 0;
}

static int
evt_parse_wkr_deq_dep(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint16(&(opt->wkr_deq_dep), arg);
	return ret;
}

static int
evt_parse_nb_pkts(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint64(&(opt->nb_pkts), arg);

	return ret;
}

static int
evt_parse_nb_timers(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint64(&(opt->nb_timers), arg);

	return ret;
}

static int
evt_parse_timer_tick_nsec(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint64(&(opt->timer_tick_nsec), arg);

	return ret;
}

static int
evt_parse_max_tmo_nsec(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint64(&(opt->max_tmo_nsec), arg);

	return ret;
}

static int
evt_parse_expiry_nsec(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint64(&(opt->expiry_nsec), arg);

	return ret;
}

static int
evt_parse_nb_timer_adptrs(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint8(&(opt->nb_timer_adptrs), arg);
	if (opt->nb_timer_adptrs <= 0) {
		evt_err("Number of timer adapters cannot be <= 0");
		return -EINVAL;
	}

	return ret;
}

static int
evt_parse_pool_sz(struct evt_options *opt, const char *arg)
{
	opt->pool_sz = atoi(arg);

	return 0;
}

static int
evt_parse_plcores(struct evt_options *opt, const char *corelist)
{
	int ret;

	ret = parse_lcores_list(opt->plcores, RTE_MAX_LCORE, corelist);
	if (ret == -E2BIG)
		evt_err("duplicate lcores in plcores");

	return ret;
}

static int
evt_parse_work_lcores(struct evt_options *opt, const char *corelist)
{
	int ret;

	ret = parse_lcores_list(opt->wlcores, RTE_MAX_LCORE, corelist);
	if (ret == -E2BIG)
		evt_err("duplicate lcores in wlcores");

	return ret;
}

static int
evt_parse_mbuf_sz(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint16(&(opt->mbuf_sz), arg);

	return ret;
}

static int
evt_parse_max_pkt_sz(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint32(&(opt->max_pkt_sz), arg);

	return ret;
}

static int
evt_parse_ena_vector(struct evt_options *opt, const char *arg __rte_unused)
{
	opt->ena_vector = 1;
	return 0;
}

static int
evt_parse_vector_size(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint16(&(opt->vector_size), arg);

	return ret;
}

static int
evt_parse_vector_tmo_ns(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint64(&(opt->vector_tmo_nsec), arg);

	return ret;
}

static int
evt_parse_eth_queues(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint16(&(opt->eth_queues), arg);

	return ret;
}

static int
evt_parse_per_port_pool(struct evt_options *opt, const char *arg __rte_unused)
{
	opt->per_port_pool = 1;
	return 0;
}

static int
evt_parse_prod_enq_burst_sz(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint32(&(opt->prod_enq_burst_sz), arg);

	return ret;
}

static void
usage(char *program)
{
	printf("usage : %s [EAL options] -- [application options]\n", program);
	printf("application options:\n");
	printf("\t--verbose          : verbose level\n"
		"\t--dev              : device id of the event device\n"
		"\t--test             : name of the test application to run\n"
		"\t--socket_id        : socket_id of application resources\n"
		"\t--pool_sz          : pool size of the mempool\n"
		"\t--plcores          : list of lcore ids for producers\n"
		"\t--wlcores          : list of lcore ids for workers\n"
		"\t--stlist           : list of scheduled types of the stages\n"
		"\t--nb_flows         : number of flows to produce\n"
		"\t--nb_pkts          : number of packets to produce\n"
		"\t--worker_deq_depth : dequeue depth of the worker\n"
		"\t--fwd_latency      : perform fwd_latency measurement\n"
		"\t--queue_priority   : enable queue priority\n"
		"\t--deq_tmo_nsec     : global dequeue timeout\n"
		"\t--prod_type_ethdev : use ethernet device as producer.\n"
		"\t--prod_type_cryptodev : use crypto device as producer.\n"
		"\t--prod_type_timerdev : use event timer device as producer.\n"
		"\t                     expiry_nsec would be the timeout\n"
		"\t                     in ns.\n"
		"\t--prod_type_timerdev_burst : use timer device as producer\n"
		"\t                             burst mode.\n"
		"\t--nb_timers        : number of timers to arm.\n"
		"\t--nb_timer_adptrs  : number of timer adapters to use.\n"
		"\t--timer_tick_nsec  : timer tick interval in ns.\n"
		"\t--max_tmo_nsec     : max timeout interval in ns.\n"
		"\t--expiry_nsec      : event timer expiry ns.\n"
		"\t--crypto_adptr_mode : 0 for OP_NEW mode (default) and\n"
		"\t                      1 for OP_FORWARD mode.\n"
		"\t--crypto_op_type   : 0 for SYM ops (default) and\n"
		"\t                     1 for ASYM ops.\n"
		"\t--crypto_cipher_alg : cipher algorithm to be used\n"
		"\t                      default algorithm is NULL.\n"
		"\t--crypto_cipher_key : key for the cipher algorithm selected\n"
		"\t--crypto_cipher_iv_sz : IV size for the cipher algorithm\n"
		"\t                        selected\n"
		"\t--mbuf_sz          : packet mbuf size.\n"
		"\t--max_pkt_sz       : max packet size.\n"
		"\t--prod_enq_burst_sz : producer enqueue burst size.\n"
		"\t--nb_eth_queues    : number of ethernet Rx queues.\n"
		"\t--enable_vector    : enable event vectorization.\n"
		"\t--vector_size      : Max vector size.\n"
		"\t--vector_tmo_ns    : Max vector timeout in nanoseconds\n"
		"\t--per_port_pool    : Configure unique pool per ethdev port\n"
		"\t--tx_first         : Transmit given number of packets\n"
		"                       across all the ethernet devices before\n"
		"                       event workers start.\n"
		"\t--tx_pkt_sz        : Packet size to use with Tx first."
		);
	printf("available tests:\n");
	evt_test_dump_names();
}

static int
evt_parse_sched_type_list(struct evt_options *opt, const char *arg)
{
	char c;
	int i = 0, j = -1;

	for (i = 0; i < EVT_MAX_STAGES; i++)
		opt->sched_type_list[i] = (uint8_t)-1;

	i = 0;

	do {
		c = arg[++j];

		switch (c) {
		case 'o':
		case 'O':
			opt->sched_type_list[i++] = RTE_SCHED_TYPE_ORDERED;
			break;
		case 'a':
		case 'A':
			opt->sched_type_list[i++] = RTE_SCHED_TYPE_ATOMIC;
			break;
		case 'p':
		case 'P':
			opt->sched_type_list[i++] = RTE_SCHED_TYPE_PARALLEL;
			break;
		case ',':
			break;
		default:
			if (c != '\0') {
				evt_err("invalid sched_type %c", c);
				return -EINVAL;
			}
		}
	} while (c != '\0');

	opt->nb_stages = i;
	return 0;
}

static struct option lgopts[] = {
	{ EVT_NB_FLOWS,            1, 0, 0 },
	{ EVT_DEVICE,              1, 0, 0 },
	{ EVT_VERBOSE,             1, 0, 0 },
	{ EVT_TEST,                1, 0, 0 },
	{ EVT_PROD_LCORES,         1, 0, 0 },
	{ EVT_WORK_LCORES,         1, 0, 0 },
	{ EVT_SOCKET_ID,           1, 0, 0 },
	{ EVT_POOL_SZ,             1, 0, 0 },
	{ EVT_NB_PKTS,             1, 0, 0 },
	{ EVT_WKR_DEQ_DEP,         1, 0, 0 },
	{ EVT_SCHED_TYPE_LIST,     1, 0, 0 },
	{ EVT_FWD_LATENCY,         0, 0, 0 },
	{ EVT_QUEUE_PRIORITY,      0, 0, 0 },
	{ EVT_DEQ_TMO_NSEC,        1, 0, 0 },
	{ EVT_PROD_ETHDEV,         0, 0, 0 },
	{ EVT_PROD_CRYPTODEV,      0, 0, 0 },
	{ EVT_PROD_TIMERDEV,       0, 0, 0 },
	{ EVT_PROD_TIMERDEV_BURST, 0, 0, 0 },
	{ EVT_CRYPTO_ADPTR_MODE,   1, 0, 0 },
	{ EVT_CRYPTO_OP_TYPE,	   1, 0, 0 },
	{ EVT_CRYPTO_CIPHER_ALG,   1, 0, 0 },
	{ EVT_CRYPTO_CIPHER_KEY,   1, 0, 0 },
	{ EVT_CRYPTO_CIPHER_IV_SZ, 1, 0, 0 },
	{ EVT_NB_TIMERS,           1, 0, 0 },
	{ EVT_NB_TIMER_ADPTRS,     1, 0, 0 },
	{ EVT_TIMER_TICK_NSEC,     1, 0, 0 },
	{ EVT_MAX_TMO_NSEC,        1, 0, 0 },
	{ EVT_EXPIRY_NSEC,         1, 0, 0 },
	{ EVT_MBUF_SZ,             1, 0, 0 },
	{ EVT_MAX_PKT_SZ,          1, 0, 0 },
	{ EVT_PROD_ENQ_BURST_SZ,   1, 0, 0 },
	{ EVT_NB_ETH_QUEUES,       1, 0, 0 },
	{ EVT_ENA_VECTOR,          0, 0, 0 },
	{ EVT_VECTOR_SZ,           1, 0, 0 },
	{ EVT_VECTOR_TMO,          1, 0, 0 },
	{ EVT_PER_PORT_POOL,       0, 0, 0 },
	{ EVT_HELP,                0, 0, 0 },
	{ EVT_TX_FIRST,            1, 0, 0 },
	{ EVT_TX_PKT_SZ,           1, 0, 0 },
	{ NULL,                    0, 0, 0 }
};

static int
evt_opts_parse_long(int opt_idx, struct evt_options *opt)
{
	unsigned int i;

	struct long_opt_parser parsermap[] = {
		{ EVT_NB_FLOWS, evt_parse_nb_flows},
		{ EVT_DEVICE, evt_parse_dev_id},
		{ EVT_VERBOSE, evt_parse_verbose},
		{ EVT_TEST, evt_parse_test_name},
		{ EVT_PROD_LCORES, evt_parse_plcores},
		{ EVT_WORK_LCORES, evt_parse_work_lcores},
		{ EVT_SOCKET_ID, evt_parse_socket_id},
		{ EVT_POOL_SZ, evt_parse_pool_sz},
		{ EVT_NB_PKTS, evt_parse_nb_pkts},
		{ EVT_WKR_DEQ_DEP, evt_parse_wkr_deq_dep},
		{ EVT_SCHED_TYPE_LIST, evt_parse_sched_type_list},
		{ EVT_FWD_LATENCY, evt_parse_fwd_latency},
		{ EVT_QUEUE_PRIORITY, evt_parse_queue_priority},
		{ EVT_DEQ_TMO_NSEC, evt_parse_deq_tmo_nsec},
		{ EVT_PROD_ETHDEV, evt_parse_eth_prod_type},
		{ EVT_PROD_CRYPTODEV, evt_parse_crypto_prod_type},
		{ EVT_PROD_TIMERDEV, evt_parse_timer_prod_type},
		{ EVT_PROD_TIMERDEV_BURST, evt_parse_timer_prod_type_burst},
		{ EVT_CRYPTO_ADPTR_MODE, evt_parse_crypto_adptr_mode},
		{ EVT_CRYPTO_OP_TYPE, evt_parse_crypto_op_type},
		{ EVT_CRYPTO_CIPHER_ALG, evt_parse_crypto_cipher_alg},
		{ EVT_CRYPTO_CIPHER_KEY, evt_parse_crypto_cipher_key},
		{ EVT_CRYPTO_CIPHER_IV_SZ, evt_parse_crypto_cipher_iv_sz},
		{ EVT_NB_TIMERS, evt_parse_nb_timers},
		{ EVT_NB_TIMER_ADPTRS, evt_parse_nb_timer_adptrs},
		{ EVT_TIMER_TICK_NSEC, evt_parse_timer_tick_nsec},
		{ EVT_MAX_TMO_NSEC, evt_parse_max_tmo_nsec},
		{ EVT_EXPIRY_NSEC, evt_parse_expiry_nsec},
		{ EVT_MBUF_SZ, evt_parse_mbuf_sz},
		{ EVT_MAX_PKT_SZ, evt_parse_max_pkt_sz},
		{ EVT_PROD_ENQ_BURST_SZ, evt_parse_prod_enq_burst_sz},
		{ EVT_NB_ETH_QUEUES, evt_parse_eth_queues},
		{ EVT_ENA_VECTOR, evt_parse_ena_vector},
		{ EVT_VECTOR_SZ, evt_parse_vector_size},
		{ EVT_VECTOR_TMO, evt_parse_vector_tmo_ns},
		{ EVT_PER_PORT_POOL, evt_parse_per_port_pool},
		{ EVT_TX_FIRST, evt_parse_tx_first},
		{ EVT_TX_PKT_SZ, evt_parse_tx_pkt_sz},
	};

	for (i = 0; i < RTE_DIM(parsermap); i++) {
		if (strncmp(lgopts[opt_idx].name, parsermap[i].lgopt_name,
				strlen(lgopts[opt_idx].name)) == 0)
			return parsermap[i].parser_fn(opt, optarg);
	}

	return -EINVAL;
}

int
evt_options_parse(struct evt_options *opt, int argc, char **argv)
{
	int opts, retval, opt_idx;

	while ((opts = getopt_long(argc, argv, "", lgopts, &opt_idx)) != EOF) {
		switch (opts) {
		case 0: /* long options */
			if (!strcmp(lgopts[opt_idx].name, "help")) {
				usage(argv[0]);
				exit(EXIT_SUCCESS);
			}

			retval = evt_opts_parse_long(opt_idx, opt);
			if (retval != 0)
				return retval;
			break;
		default:
			return -EINVAL;
		}
	}
	return 0;
}

void
evt_options_dump(struct evt_options *opt)
{
	int lcore_id;
	struct rte_event_dev_info dev_info;

	rte_event_dev_info_get(opt->dev_id, &dev_info);
	evt_dump("driver", "%s", dev_info.driver_name);
	evt_dump("test", "%s", opt->test_name);
	evt_dump("dev", "%d", opt->dev_id);
	evt_dump("verbose_level", "%d", opt->verbose_level);
	evt_dump("socket_id", "%d", opt->socket_id);
	evt_dump("pool_sz", "%d", opt->pool_sz);
	evt_dump("main lcore", "%d", rte_get_main_lcore());
	evt_dump("nb_pkts", "%"PRIu64, opt->nb_pkts);
	evt_dump("nb_timers", "%"PRIu64, opt->nb_timers);
	evt_dump_begin("available lcores");
	RTE_LCORE_FOREACH(lcore_id)
		printf("%d ", lcore_id);
	evt_dump_end;
	evt_dump_nb_flows(opt);
	evt_dump_worker_dequeue_depth(opt);
}
