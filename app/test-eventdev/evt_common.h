/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef _EVT_COMMON_
#define _EVT_COMMON_

#include <rte_common.h>
#include <rte_crypto.h>
#include <rte_debug.h>
#include <rte_event_crypto_adapter.h>
#include <rte_eventdev.h>
#include <rte_service.h>

#define CLNRM  "\x1b[0m"
#define CLRED  "\x1b[31m"
#define CLGRN  "\x1b[32m"
#define CLYEL  "\x1b[33m"

#define evt_err(fmt, args...) \
	fprintf(stderr, CLRED"error: %s() "fmt CLNRM "\n", __func__, ## args)

#define evt_info(fmt, args...) \
	fprintf(stdout, CLYEL""fmt CLNRM "\n", ## args)

#define EVT_STR_FMT 20

#define evt_dump(str, fmt, val...) \
	printf("\t%-*s : "fmt"\n", EVT_STR_FMT, str, ## val)

#define evt_dump_begin(str) printf("\t%-*s : {", EVT_STR_FMT, str)

#define evt_dump_end printf("\b}\n")

#define EVT_MAX_STAGES           64
#define EVT_MAX_PORTS            256
#define EVT_MAX_QUEUES           256

enum evt_prod_type {
	EVT_PROD_TYPE_NONE,
	EVT_PROD_TYPE_SYNT,          /* Producer type Synthetic i.e. CPU. */
	EVT_PROD_TYPE_ETH_RX_ADPTR,  /* Producer type Eth Rx Adapter. */
	EVT_PROD_TYPE_EVENT_TIMER_ADPTR,  /* Producer type Timer Adapter. */
	EVT_PROD_TYPE_EVENT_CRYPTO_ADPTR,  /* Producer type Crypto Adapter. */
	EVT_PROD_TYPE_MAX,
};

struct evt_options {
#define EVT_TEST_NAME_MAX_LEN     32
#define EVT_CRYPTO_MAX_KEY_SIZE   256
#define EVT_CRYPTO_MAX_IV_SIZE    16
	char test_name[EVT_TEST_NAME_MAX_LEN];
	bool plcores[RTE_MAX_LCORE];
	bool wlcores[RTE_MAX_LCORE];
	bool crypto_cipher_bit_mode;
	int pool_sz;
	int socket_id;
	int nb_stages;
	int verbose_level;
	uint8_t dev_id;
	uint8_t timdev_cnt;
	uint8_t nb_timer_adptrs;
	uint8_t timdev_use_burst;
	uint8_t per_port_pool;
	uint8_t sched_type_list[EVT_MAX_STAGES];
	uint16_t mbuf_sz;
	uint16_t wkr_deq_dep;
	uint16_t vector_size;
	uint16_t eth_queues;
	uint16_t crypto_cipher_iv_sz;
	uint32_t nb_flows;
	uint32_t tx_first;
	uint16_t tx_pkt_sz;
	uint32_t max_pkt_sz;
	uint32_t prod_enq_burst_sz;
	uint32_t deq_tmo_nsec;
	uint32_t crypto_cipher_key_sz;
	uint32_t q_priority:1;
	uint32_t fwd_latency:1;
	uint32_t ena_vector : 1;
	uint64_t nb_pkts;
	uint64_t nb_timers;
	uint64_t expiry_nsec;
	uint64_t max_tmo_nsec;
	uint64_t vector_tmo_nsec;
	uint64_t timer_tick_nsec;
	uint64_t optm_timer_tick_nsec;
	enum evt_prod_type prod_type;
	enum rte_event_crypto_adapter_mode crypto_adptr_mode;
	enum rte_crypto_op_type crypto_op_type;
	enum rte_crypto_cipher_algorithm crypto_cipher_alg;
	uint8_t crypto_cipher_key[EVT_CRYPTO_MAX_KEY_SIZE];
};

static inline bool
evt_has_distributed_sched(uint8_t dev_id)
{
	struct rte_event_dev_info dev_info;

	rte_event_dev_info_get(dev_id, &dev_info);
	return (dev_info.event_dev_cap & RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED) ?
			true : false;
}

static inline bool
evt_has_burst_mode(uint8_t dev_id)
{
	struct rte_event_dev_info dev_info;

	rte_event_dev_info_get(dev_id, &dev_info);
	return (dev_info.event_dev_cap & RTE_EVENT_DEV_CAP_BURST_MODE) ?
			true : false;
}


static inline bool
evt_has_all_types_queue(uint8_t dev_id)
{
	struct rte_event_dev_info dev_info;

	rte_event_dev_info_get(dev_id, &dev_info);
	return (dev_info.event_dev_cap & RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES) ?
			true : false;
}

static inline bool
evt_has_flow_id(uint8_t dev_id)
{
	struct rte_event_dev_info dev_info;

	rte_event_dev_info_get(dev_id, &dev_info);
	return (dev_info.event_dev_cap & RTE_EVENT_DEV_CAP_CARRY_FLOW_ID) ?
			true : false;
}

static inline int
evt_service_setup(uint32_t service_id)
{
	int32_t core_cnt;
	unsigned int lcore = 0;
	uint32_t core_array[RTE_MAX_LCORE];
	uint8_t cnt;
	uint8_t min_cnt = UINT8_MAX;

	if (!rte_service_lcore_count())
		return -ENOENT;

	core_cnt = rte_service_lcore_list(core_array,
			RTE_MAX_LCORE);
	if (core_cnt < 0)
		return -ENOENT;
	/* Get the core which has least number of services running. */
	while (core_cnt--) {
		/* Reset default mapping */
		rte_service_map_lcore_set(service_id,
				core_array[core_cnt], 0);
		cnt = rte_service_lcore_count_services(
				core_array[core_cnt]);
		if (cnt < min_cnt) {
			lcore = core_array[core_cnt];
			min_cnt = cnt;
		}
	}
	if (rte_service_map_lcore_set(service_id, lcore, 1))
		return -ENOENT;

	return 0;
}

static inline int
evt_configure_eventdev(struct evt_options *opt, uint8_t nb_queues,
		uint8_t nb_ports)
{
	struct rte_event_dev_info info;
	int ret;

	memset(&info, 0, sizeof(struct rte_event_dev_info));
	ret = rte_event_dev_info_get(opt->dev_id, &info);
	if (ret) {
		evt_err("failed to get eventdev info %d", opt->dev_id);
		return ret;
	}

	if (opt->deq_tmo_nsec) {
		if (opt->deq_tmo_nsec < info.min_dequeue_timeout_ns) {
			opt->deq_tmo_nsec = info.min_dequeue_timeout_ns;
			evt_info("dequeue_timeout_ns too low, using %d",
					opt->deq_tmo_nsec);
		}
		if (opt->deq_tmo_nsec > info.max_dequeue_timeout_ns) {
			opt->deq_tmo_nsec = info.max_dequeue_timeout_ns;
			evt_info("dequeue_timeout_ns too high, using %d",
					opt->deq_tmo_nsec);
		}
	}

	const struct rte_event_dev_config config = {
			.dequeue_timeout_ns = opt->deq_tmo_nsec,
			.nb_event_queues = nb_queues,
			.nb_event_ports = nb_ports,
			.nb_single_link_event_port_queues = 0,
			.nb_events_limit  = info.max_num_events,
			.nb_event_queue_flows = opt->nb_flows,
			.nb_event_port_dequeue_depth =
				info.max_event_port_dequeue_depth,
			.nb_event_port_enqueue_depth =
				info.max_event_port_enqueue_depth,
	};

	return rte_event_dev_configure(opt->dev_id, &config);
}

#endif /*  _EVT_COMMON_*/
