/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef _EVT_OPTIONS_
#define _EVT_OPTIONS_

#include <stdio.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_cryptodev.h>
#include <rte_ethdev.h>
#include <rte_eventdev.h>
#include <rte_lcore.h>

#include "evt_common.h"

#define EVT_BOOL_FMT(x)          ((x) ? "true" : "false")

#define EVT_VERBOSE              ("verbose")
#define EVT_DEVICE               ("dev")
#define EVT_TEST                 ("test")
#define EVT_PROD_LCORES          ("plcores")
#define EVT_WORK_LCORES          ("wlcores")
#define EVT_NB_FLOWS             ("nb_flows")
#define EVT_SOCKET_ID            ("socket_id")
#define EVT_POOL_SZ              ("pool_sz")
#define EVT_WKR_DEQ_DEP          ("worker_deq_depth")
#define EVT_NB_PKTS              ("nb_pkts")
#define EVT_NB_STAGES            ("nb_stages")
#define EVT_SCHED_TYPE_LIST      ("stlist")
#define EVT_FWD_LATENCY          ("fwd_latency")
#define EVT_QUEUE_PRIORITY       ("queue_priority")
#define EVT_DEQ_TMO_NSEC         ("deq_tmo_nsec")
#define EVT_PROD_ETHDEV          ("prod_type_ethdev")
#define EVT_PROD_CRYPTODEV	 ("prod_type_cryptodev")
#define EVT_PROD_TIMERDEV        ("prod_type_timerdev")
#define EVT_PROD_TIMERDEV_BURST  ("prod_type_timerdev_burst")
#define EVT_CRYPTO_ADPTR_MODE	 ("crypto_adptr_mode")
#define EVT_CRYPTO_OP_TYPE	 ("crypto_op_type")
#define EVT_CRYPTO_CIPHER_ALG	 ("crypto_cipher_alg")
#define EVT_CRYPTO_CIPHER_KEY	 ("crypto_cipher_key")
#define EVT_CRYPTO_CIPHER_IV_SZ  ("crypto_cipher_iv_sz")
#define EVT_NB_TIMERS            ("nb_timers")
#define EVT_NB_TIMER_ADPTRS      ("nb_timer_adptrs")
#define EVT_TIMER_TICK_NSEC      ("timer_tick_nsec")
#define EVT_MAX_TMO_NSEC         ("max_tmo_nsec")
#define EVT_EXPIRY_NSEC          ("expiry_nsec")
#define EVT_MBUF_SZ              ("mbuf_sz")
#define EVT_MAX_PKT_SZ           ("max_pkt_sz")
#define EVT_PROD_ENQ_BURST_SZ    ("prod_enq_burst_sz")
#define EVT_NB_ETH_QUEUES        ("nb_eth_queues")
#define EVT_ENA_VECTOR           ("enable_vector")
#define EVT_VECTOR_SZ            ("vector_size")
#define EVT_VECTOR_TMO           ("vector_tmo_ns")
#define EVT_PER_PORT_POOL	 ("per_port_pool")
#define EVT_TX_FIRST		 ("tx_first")
#define EVT_TX_PKT_SZ		 ("tx_pkt_sz")
#define EVT_HELP                 ("help")

void evt_options_default(struct evt_options *opt);
int evt_options_parse(struct evt_options *opt, int argc, char **argv);
void evt_options_dump(struct evt_options *opt);

/* options check helpers */
static inline bool
evt_lcores_has_overlap(bool lcores[], int lcore)
{
	if (lcores[lcore] == true) {
		evt_err("lcore overlaps at %d", lcore);
		return true;
	}

	return false;
}

static inline bool
evt_lcores_has_overlap_multi(bool lcoresx[], bool lcoresy[])
{
	int i;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (lcoresx[i] && lcoresy[i]) {
			evt_err("lcores overlaps at %d", i);
			return true;
		}
	}
	return false;
}

static inline bool
evt_has_active_lcore(bool lcores[])
{
	int i;

	for (i = 0; i < RTE_MAX_LCORE; i++)
		if (lcores[i])
			return true;
	return false;
}

static inline int
evt_nr_active_lcores(bool lcores[])
{
	int i;
	int c = 0;

	for (i = 0; i < RTE_MAX_LCORE; i++)
		if (lcores[i])
			c++;
	return c;
}

static inline int
evt_get_first_active_lcore(bool lcores[])
{
	int i;

	for (i = 0; i < RTE_MAX_LCORE; i++)
		if (lcores[i])
			return i;
	return -1;
}

static inline bool
evt_has_disabled_lcore(bool lcores[])
{
	int i;

	for (i = 0; i < RTE_MAX_LCORE; i++)
		if ((lcores[i] == true) && !(rte_lcore_is_enabled(i)))
			return true;
	return false;
}

static inline bool
evt_has_invalid_stage(struct evt_options *opt)
{
	if (!opt->nb_stages) {
		evt_err("need minimum one stage, check --stlist");
		return true;
	}
	if (opt->nb_stages > EVT_MAX_STAGES) {
		evt_err("requested changes are beyond EVT_MAX_STAGES=%d",
			EVT_MAX_STAGES);
		return true;
	}
	return false;
}

static inline bool
evt_has_invalid_sched_type(struct evt_options *opt)
{
	int i;

	for (i = 0; i < opt->nb_stages; i++) {
		if (opt->sched_type_list[i] > RTE_SCHED_TYPE_PARALLEL) {
			evt_err("invalid sched_type %d at %d",
				opt->sched_type_list[i], i);
			return true;
		}
	}
	return false;
}

/* option dump helpers */
static inline void
evt_dump_worker_lcores(struct evt_options *opt)
{
	int c;

	evt_dump_begin("worker lcores");
	for  (c = 0; c < RTE_MAX_LCORE; c++) {
		if (opt->wlcores[c])
			printf("%d ", c);
	}
	evt_dump_end;
}

static inline void
evt_dump_producer_lcores(struct evt_options *opt)
{
	int c;

	evt_dump_begin("producer lcores");
	for  (c = 0; c < RTE_MAX_LCORE; c++) {
		if (opt->plcores[c])
			printf("%d ", c);
	}
	evt_dump_end;
}

static inline void
evt_dump_nb_flows(struct evt_options *opt)
{
	evt_dump("nb_flows", "%d", opt->nb_flows);
}

static inline void
evt_dump_worker_dequeue_depth(struct evt_options *opt)
{
	evt_dump("worker deq depth", "%d", opt->wkr_deq_dep);
}

static inline void
evt_dump_nb_stages(struct evt_options *opt)
{
	evt_dump("nb_stages", "%d", opt->nb_stages);
}

static inline void
evt_dump_fwd_latency(struct evt_options *opt)
{
	evt_dump("fwd_latency", "%s", EVT_BOOL_FMT(opt->fwd_latency));
}

static inline void
evt_dump_queue_priority(struct evt_options *opt)
{
	evt_dump("queue_priority", "%s", EVT_BOOL_FMT(opt->q_priority));
}

static inline const char*
evt_sched_type_2_str(uint8_t sched_type)
{

	if (sched_type == RTE_SCHED_TYPE_ORDERED)
		return "O";
	else if (sched_type == RTE_SCHED_TYPE_ATOMIC)
		return "A";
	else if (sched_type == RTE_SCHED_TYPE_PARALLEL)
		return "P";
	else
		return "I";
}

static inline void
evt_dump_sched_type_list(struct evt_options *opt)
{
	int i;

	evt_dump_begin("sched_type_list");
	for (i = 0; i < opt->nb_stages; i++)
		printf("%s ", evt_sched_type_2_str(opt->sched_type_list[i]));

	evt_dump_end;
}

static inline const char *
evt_prod_id_to_name(enum evt_prod_type prod_type)
{
	switch (prod_type) {
	default:
	case EVT_PROD_TYPE_SYNT:
		return "Synthetic producer lcores";
	case EVT_PROD_TYPE_ETH_RX_ADPTR:
		return "Ethdev Rx Adapter";
	case EVT_PROD_TYPE_EVENT_TIMER_ADPTR:
		return "Event timer adapter";
	case EVT_PROD_TYPE_EVENT_CRYPTO_ADPTR:
		return "Event crypto adapter";
	}

	return "";
}

#define EVT_PROD_MAX_NAME_LEN 50
static inline void
evt_dump_producer_type(struct evt_options *opt)
{
	char name[EVT_PROD_MAX_NAME_LEN];

	switch (opt->prod_type) {
	default:
	case EVT_PROD_TYPE_SYNT:
		snprintf(name, EVT_PROD_MAX_NAME_LEN,
				"Synthetic producer lcores");
		break;
	case EVT_PROD_TYPE_ETH_RX_ADPTR:
		snprintf(name, EVT_PROD_MAX_NAME_LEN,
				"Ethdev Rx Adapter producers");
		evt_dump("nb_ethdev", "%d", rte_eth_dev_count_avail());
		break;
	case EVT_PROD_TYPE_EVENT_TIMER_ADPTR:
		if (opt->timdev_use_burst)
			snprintf(name, EVT_PROD_MAX_NAME_LEN,
				"Event timer adapter burst mode producer");
		else
			snprintf(name, EVT_PROD_MAX_NAME_LEN,
				"Event timer adapter producer");
		evt_dump("nb_timer_adapters", "%d", opt->nb_timer_adptrs);
		evt_dump("max_tmo_nsec", "%"PRIu64"", opt->max_tmo_nsec);
		evt_dump("expiry_nsec", "%"PRIu64"", opt->expiry_nsec);
		if (opt->optm_timer_tick_nsec)
			evt_dump("optm_timer_tick_nsec", "%"PRIu64"",
					opt->optm_timer_tick_nsec);
		else
			evt_dump("timer_tick_nsec", "%"PRIu64"",
					opt->timer_tick_nsec);
		break;
	case EVT_PROD_TYPE_EVENT_CRYPTO_ADPTR:
		snprintf(name, EVT_PROD_MAX_NAME_LEN,
			 "Event crypto adapter producers");
		evt_dump("crypto adapter mode", "%s",
			 opt->crypto_adptr_mode ? "OP_FORWARD" : "OP_NEW");
		evt_dump("crypto op type", "%s",
			 (opt->crypto_op_type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) ?
			 "SYMMETRIC" : "ASYMMETRIC");
		evt_dump("nb_cryptodev", "%u", rte_cryptodev_count());
		if (opt->crypto_op_type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
			evt_dump("cipher algo", "%s",
				 rte_cryptodev_get_cipher_algo_string(opt->crypto_cipher_alg));
			evt_dump("cipher key sz", "%u",
				 opt->crypto_cipher_key_sz);
			evt_dump("cipher iv sz", "%u", opt->crypto_cipher_iv_sz);
		}
		break;
	}
	evt_dump("prod_type", "%s", name);
}

#endif /* _EVT_OPTIONS_ */
