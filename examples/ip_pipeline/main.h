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

#ifndef _MAIN_H_
#define _MAIN_H_

#include <stdint.h>

#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_ethdev.h>

#ifdef RTE_LIBRTE_ACL
#include <rte_table_acl.h>
#endif

struct app_flow_key {
	union {
		struct {
			uint8_t ttl; /* needs to be set to 0 */
			uint8_t proto;
			uint16_t header_checksum; /* needs to be set to 0 */
			uint32_t ip_src;
		};
		uint64_t slab0;
	};

	union {
		struct {
			uint32_t ip_dst;
			uint16_t port_src;
			uint16_t port_dst;
		};
		uint64_t slab1;
	};
} __attribute__((__packed__));

struct app_arp_key {
	uint32_t nh_ip;
	uint32_t nh_iface;
} __attribute__((__packed__));

struct app_pkt_metadata {
	uint32_t signature;
	uint8_t reserved1[28];

	struct app_flow_key flow_key;

	struct app_arp_key arp_key;
	struct ether_addr nh_arp;

	uint8_t reserved3[2];
} __attribute__((__packed__));

#ifndef APP_MBUF_ARRAY_SIZE
#define APP_MBUF_ARRAY_SIZE            256
#endif

struct app_mbuf_array {
	struct rte_mbuf *array[APP_MBUF_ARRAY_SIZE];
	uint32_t n_mbufs;
};

#ifndef APP_MAX_PORTS
#define APP_MAX_PORTS                  4
#endif

#ifndef APP_MAX_SWQ_PER_CORE
#define APP_MAX_SWQ_PER_CORE           8
#endif

#define APP_SWQ_INVALID                ((uint32_t)(-1))

#define APP_SWQ_IN_REQ                 (APP_MAX_SWQ_PER_CORE - 1)

#define APP_SWQ_OUT_RESP               (APP_MAX_SWQ_PER_CORE - 1)

enum app_core_type {
	APP_CORE_NONE = 0, /* Unused */
	APP_CORE_MASTER,   /* Management */
	APP_CORE_RX,       /* Reception */
	APP_CORE_TX,       /* Transmission */
	APP_CORE_PT,       /* Pass-through */
	APP_CORE_FC,       /* Flow Classification */
	APP_CORE_FW,       /* Firewall */
	APP_CORE_RT,       /* Routing */
	APP_CORE_TM,       /* Traffic Management */
	APP_CORE_IPV4_FRAG,/* IPv4 Fragmentation */
	APP_CORE_IPV4_RAS, /* IPv4 Reassembly */
};

struct app_core_params {
	uint32_t core_id;
	enum app_core_type core_type;

	/* SWQ map */
	uint32_t swq_in[APP_MAX_SWQ_PER_CORE];
	uint32_t swq_out[APP_MAX_SWQ_PER_CORE];
} __rte_cache_aligned;

struct app_params {
	/* CPU cores */
	struct app_core_params cores[RTE_MAX_LCORE];
	uint32_t n_cores;

	/* Ports*/
	uint32_t ports[APP_MAX_PORTS];
	uint32_t n_ports;
	uint32_t rsz_hwq_rx;
	uint32_t rsz_hwq_tx;
	uint32_t bsz_hwq_rd;
	uint32_t bsz_hwq_wr;
	struct rte_eth_conf port_conf;
	struct rte_eth_rxconf rx_conf;
	struct rte_eth_txconf tx_conf;

	/* SW Queues (SWQs) */
	struct rte_ring **rings;
	uint32_t rsz_swq;
	uint32_t bsz_swq_rd;
	uint32_t bsz_swq_wr;

	/* Buffer pool */
	struct rte_mempool *pool;
	struct rte_mempool *indirect_pool;
	uint32_t pool_buffer_size;
	uint32_t pool_size;
	uint32_t pool_cache_size;

	/* Message buffer pool */
	struct rte_mempool *msg_pool;
	uint32_t msg_pool_buffer_size;
	uint32_t msg_pool_size;
	uint32_t msg_pool_cache_size;

	/* Rule tables */
	uint32_t max_arp_rules;
	uint32_t max_routing_rules;
	uint32_t max_firewall_rules;
	uint32_t max_flow_rules;

	/* Processing */
	uint32_t ether_hdr_pop_push;
} __rte_cache_aligned;

extern struct app_params app;

const char *app_core_type_id_to_string(enum app_core_type id);
int app_core_type_string_to_id(const char *string, enum app_core_type *id);
void app_cores_config_print(void);

void app_check_core_params(void);
struct app_core_params *app_get_core_params(uint32_t core_id);
uint32_t app_get_first_core_id(enum app_core_type core_type);
struct rte_ring *app_get_ring_req(uint32_t core_id);
struct rte_ring *app_get_ring_resp(uint32_t core_id);

int app_parse_args(int argc, char **argv);
void app_print_usage(char *prgname);
void app_init(void);
void app_ping(void);
int app_lcore_main_loop(void *arg);

/* Hash functions */
uint64_t test_hash(void *key, uint32_t key_size, uint64_t seed);
uint32_t rte_jhash2_16(uint32_t *k, uint32_t initval);
#if defined(__x86_64__)
uint32_t rte_aeshash_16(uint64_t *k, uint64_t seed);
uint32_t rte_crchash_16(uint64_t *k, uint64_t seed);
#endif

/* I/O with no pipeline */
void app_main_loop_rx(void);
void app_main_loop_tx(void);
void app_main_loop_passthrough(void);

/* Pipeline */
void app_main_loop_pipeline_rx(void);
void app_main_loop_pipeline_rx_frag(void);
void app_main_loop_pipeline_tx(void);
void app_main_loop_pipeline_tx_ras(void);
void app_main_loop_pipeline_flow_classification(void);
void app_main_loop_pipeline_firewall(void);
void app_main_loop_pipeline_routing(void);
void app_main_loop_pipeline_passthrough(void);
void app_main_loop_pipeline_ipv4_frag(void);
void app_main_loop_pipeline_ipv4_ras(void);

/* Command Line Interface (CLI) */
void app_main_loop_cmdline(void);

/* Messages */
enum app_msg_req_type {
	APP_MSG_REQ_PING,
	APP_MSG_REQ_FC_ADD,
	APP_MSG_REQ_FC_DEL,
	APP_MSG_REQ_FC_ADD_ALL,
	APP_MSG_REQ_FW_ADD,
	APP_MSG_REQ_FW_DEL,
	APP_MSG_REQ_RT_ADD,
	APP_MSG_REQ_RT_DEL,
	APP_MSG_REQ_ARP_ADD,
	APP_MSG_REQ_ARP_DEL,
	APP_MSG_REQ_RX_PORT_ENABLE,
	APP_MSG_REQ_RX_PORT_DISABLE,
};

struct app_msg_req {
	enum app_msg_req_type type;
	union {
		struct {
			uint32_t ip;
			uint8_t depth;
			uint8_t port;
			uint32_t nh_ip;
		} routing_add;
		struct {
			uint32_t ip;
			uint8_t depth;
		} routing_del;
		struct {
			uint8_t out_iface;
			uint32_t nh_ip;
			struct ether_addr nh_arp;
		} arp_add;
		struct {
			uint8_t out_iface;
			uint32_t nh_ip;
		} arp_del;
		struct {
			union {
				uint8_t key_raw[16];
				struct app_flow_key key;
			};
			uint8_t port;
		} flow_classif_add;
		struct {
			union {
				uint8_t key_raw[16];
				struct app_flow_key key;
			};
		} flow_classif_del;
#ifdef RTE_LIBRTE_ACL
		struct {
			struct rte_table_acl_rule_add_params add_params;
			uint8_t port;
		} firewall_add;
		struct {
			struct rte_table_acl_rule_delete_params delete_params;
		} firewall_del;
#endif
		struct {
			uint8_t port;
		} rx_up;
		struct {
			uint8_t port;
		} rx_down;
	};
};

struct app_msg_resp {
	int result;
};

#define APP_FLUSH 0xFF

#endif /* _MAIN_H_ */
