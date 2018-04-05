/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_string_fns.h>
#include <rte_acl.h>

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
#define L3FWDACL_DEBUG
#endif
#define DO_RFC_1812_CHECKS

#define RTE_LOGTYPE_L3FWD RTE_LOGTYPE_USER1

#define MAX_JUMBO_PKT_LEN  9600

#define MEMPOOL_CACHE_SIZE 256

/*
 * This expression is used to calculate the number of mbufs needed
 * depending on user input, taking into account memory for rx and tx hardware
 * rings, cache per lcore and mtable per port per lcore.
 * RTE_MAX is used to ensure that NB_MBUF never goes below a
 * minimum value of 8192
 */

#define NB_MBUF	RTE_MAX(\
	(nb_ports * nb_rx_queue * nb_rxd +	\
	nb_ports * nb_lcores * MAX_PKT_BURST +	\
	nb_ports * n_tx_queue * nb_txd +	\
	nb_lcores * MEMPOOL_CACHE_SIZE),	\
	(unsigned)8192)

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

#define NB_SOCKETS 8

/* Configure how many packets ahead to prefetch, when reading packets */
#define PREFETCH_OFFSET	3

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 1024
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
static uint32_t enabled_port_mask;
static int promiscuous_on; /**< Ports set in promiscuous mode off by default. */
static int numa_on = 1; /**< NUMA is enabled by default. */

struct lcore_rx_queue {
	uint16_t port_id;
	uint8_t queue_id;
} __rte_cache_aligned;

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT RTE_MAX_ETHPORTS
#define MAX_RX_QUEUE_PER_PORT 128

#define MAX_LCORE_PARAMS 1024
struct lcore_params {
	uint16_t port_id;
	uint8_t queue_id;
	uint8_t lcore_id;
} __rte_cache_aligned;

static struct lcore_params lcore_params_array[MAX_LCORE_PARAMS];
static struct lcore_params lcore_params_array_default[] = {
	{0, 0, 2},
	{0, 1, 2},
	{0, 2, 2},
	{1, 0, 2},
	{1, 1, 2},
	{1, 2, 2},
	{2, 0, 2},
	{3, 0, 3},
	{3, 1, 3},
};

static struct lcore_params *lcore_params = lcore_params_array_default;
static uint16_t nb_lcore_params = sizeof(lcore_params_array_default) /
				sizeof(lcore_params_array_default[0]);

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode	= ETH_MQ_RX_RSS,
		.max_rx_pkt_len = ETHER_MAX_LEN,
		.split_hdr_size = 0,
		.ignore_offload_bitfield = 1,
		.offloads = (DEV_RX_OFFLOAD_CRC_STRIP |
			     DEV_RX_OFFLOAD_CHECKSUM),
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = ETH_RSS_IP | ETH_RSS_UDP |
				ETH_RSS_TCP | ETH_RSS_SCTP,
		},
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

static struct rte_mempool *pktmbuf_pool[NB_SOCKETS];

/***********************start of ACL part******************************/
#ifdef DO_RFC_1812_CHECKS
static inline int
is_valid_ipv4_pkt(struct ipv4_hdr *pkt, uint32_t link_len);
#endif
static inline void
send_single_packet(struct rte_mbuf *m, uint16_t port);

#define MAX_ACL_RULE_NUM	100000
#define DEFAULT_MAX_CATEGORIES	1
#define L3FWD_ACL_IPV4_NAME	"l3fwd-acl-ipv4"
#define L3FWD_ACL_IPV6_NAME	"l3fwd-acl-ipv6"
#define ACL_LEAD_CHAR		('@')
#define ROUTE_LEAD_CHAR		('R')
#define COMMENT_LEAD_CHAR	('#')
#define OPTION_CONFIG		"config"
#define OPTION_NONUMA		"no-numa"
#define OPTION_ENBJMO		"enable-jumbo"
#define OPTION_RULE_IPV4	"rule_ipv4"
#define OPTION_RULE_IPV6	"rule_ipv6"
#define OPTION_SCALAR		"scalar"
#define ACL_DENY_SIGNATURE	0xf0000000
#define RTE_LOGTYPE_L3FWDACL	RTE_LOGTYPE_USER3
#define acl_log(format, ...)	RTE_LOG(ERR, L3FWDACL, format, ##__VA_ARGS__)
#define uint32_t_to_char(ip, a, b, c, d) do {\
		*a = (unsigned char)(ip >> 24 & 0xff);\
		*b = (unsigned char)(ip >> 16 & 0xff);\
		*c = (unsigned char)(ip >> 8 & 0xff);\
		*d = (unsigned char)(ip & 0xff);\
	} while (0)
#define OFF_ETHHEAD	(sizeof(struct ether_hdr))
#define OFF_IPV42PROTO (offsetof(struct ipv4_hdr, next_proto_id))
#define OFF_IPV62PROTO (offsetof(struct ipv6_hdr, proto))
#define MBUF_IPV4_2PROTO(m)	\
	rte_pktmbuf_mtod_offset((m), uint8_t *, OFF_ETHHEAD + OFF_IPV42PROTO)
#define MBUF_IPV6_2PROTO(m)	\
	rte_pktmbuf_mtod_offset((m), uint8_t *, OFF_ETHHEAD + OFF_IPV62PROTO)

#define GET_CB_FIELD(in, fd, base, lim, dlm)	do {            \
	unsigned long val;                                      \
	char *end;                                              \
	errno = 0;                                              \
	val = strtoul((in), &end, (base));                      \
	if (errno != 0 || end[0] != (dlm) || val > (lim))       \
		return -EINVAL;                               \
	(fd) = (typeof(fd))val;                                 \
	(in) = end + 1;                                         \
} while (0)

/*
  * ACL rules should have higher priorities than route ones to ensure ACL rule
  * always be found when input packets have multi-matches in the database.
  * A exception case is performance measure, which can define route rules with
  * higher priority and route rules will always be returned in each lookup.
  * Reserve range from ACL_RULE_PRIORITY_MAX + 1 to
  * RTE_ACL_MAX_PRIORITY for route entries in performance measure
  */
#define ACL_RULE_PRIORITY_MAX 0x10000000

/*
  * Forward port info save in ACL lib starts from 1
  * since ACL assume 0 is invalid.
  * So, need add 1 when saving and minus 1 when forwarding packets.
  */
#define FWD_PORT_SHIFT 1

/*
 * Rule and trace formats definitions.
 */

enum {
	PROTO_FIELD_IPV4,
	SRC_FIELD_IPV4,
	DST_FIELD_IPV4,
	SRCP_FIELD_IPV4,
	DSTP_FIELD_IPV4,
	NUM_FIELDS_IPV4
};

/*
 * That effectively defines order of IPV4VLAN classifications:
 *  - PROTO
 *  - VLAN (TAG and DOMAIN)
 *  - SRC IP ADDRESS
 *  - DST IP ADDRESS
 *  - PORTS (SRC and DST)
 */
enum {
	RTE_ACL_IPV4VLAN_PROTO,
	RTE_ACL_IPV4VLAN_VLAN,
	RTE_ACL_IPV4VLAN_SRC,
	RTE_ACL_IPV4VLAN_DST,
	RTE_ACL_IPV4VLAN_PORTS,
	RTE_ACL_IPV4VLAN_NUM
};

struct rte_acl_field_def ipv4_defs[NUM_FIELDS_IPV4] = {
	{
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint8_t),
		.field_index = PROTO_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_PROTO,
		.offset = 0,
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_SRC,
		.offset = offsetof(struct ipv4_hdr, src_addr) -
			offsetof(struct ipv4_hdr, next_proto_id),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_DST,
		.offset = offsetof(struct ipv4_hdr, dst_addr) -
			offsetof(struct ipv4_hdr, next_proto_id),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = SRCP_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_PORTS,
		.offset = sizeof(struct ipv4_hdr) -
			offsetof(struct ipv4_hdr, next_proto_id),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = DSTP_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_PORTS,
		.offset = sizeof(struct ipv4_hdr) -
			offsetof(struct ipv4_hdr, next_proto_id) +
			sizeof(uint16_t),
	},
};

#define	IPV6_ADDR_LEN	16
#define	IPV6_ADDR_U16	(IPV6_ADDR_LEN / sizeof(uint16_t))
#define	IPV6_ADDR_U32	(IPV6_ADDR_LEN / sizeof(uint32_t))

enum {
	PROTO_FIELD_IPV6,
	SRC1_FIELD_IPV6,
	SRC2_FIELD_IPV6,
	SRC3_FIELD_IPV6,
	SRC4_FIELD_IPV6,
	DST1_FIELD_IPV6,
	DST2_FIELD_IPV6,
	DST3_FIELD_IPV6,
	DST4_FIELD_IPV6,
	SRCP_FIELD_IPV6,
	DSTP_FIELD_IPV6,
	NUM_FIELDS_IPV6
};

struct rte_acl_field_def ipv6_defs[NUM_FIELDS_IPV6] = {
	{
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint8_t),
		.field_index = PROTO_FIELD_IPV6,
		.input_index = PROTO_FIELD_IPV6,
		.offset = 0,
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC1_FIELD_IPV6,
		.input_index = SRC1_FIELD_IPV6,
		.offset = offsetof(struct ipv6_hdr, src_addr) -
			offsetof(struct ipv6_hdr, proto),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC2_FIELD_IPV6,
		.input_index = SRC2_FIELD_IPV6,
		.offset = offsetof(struct ipv6_hdr, src_addr) -
			offsetof(struct ipv6_hdr, proto) + sizeof(uint32_t),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC3_FIELD_IPV6,
		.input_index = SRC3_FIELD_IPV6,
		.offset = offsetof(struct ipv6_hdr, src_addr) -
			offsetof(struct ipv6_hdr, proto) + 2 * sizeof(uint32_t),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC4_FIELD_IPV6,
		.input_index = SRC4_FIELD_IPV6,
		.offset = offsetof(struct ipv6_hdr, src_addr) -
			offsetof(struct ipv6_hdr, proto) + 3 * sizeof(uint32_t),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST1_FIELD_IPV6,
		.input_index = DST1_FIELD_IPV6,
		.offset = offsetof(struct ipv6_hdr, dst_addr)
				- offsetof(struct ipv6_hdr, proto),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST2_FIELD_IPV6,
		.input_index = DST2_FIELD_IPV6,
		.offset = offsetof(struct ipv6_hdr, dst_addr) -
			offsetof(struct ipv6_hdr, proto) + sizeof(uint32_t),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST3_FIELD_IPV6,
		.input_index = DST3_FIELD_IPV6,
		.offset = offsetof(struct ipv6_hdr, dst_addr) -
			offsetof(struct ipv6_hdr, proto) + 2 * sizeof(uint32_t),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST4_FIELD_IPV6,
		.input_index = DST4_FIELD_IPV6,
		.offset = offsetof(struct ipv6_hdr, dst_addr) -
			offsetof(struct ipv6_hdr, proto) + 3 * sizeof(uint32_t),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = SRCP_FIELD_IPV6,
		.input_index = SRCP_FIELD_IPV6,
		.offset = sizeof(struct ipv6_hdr) -
			offsetof(struct ipv6_hdr, proto),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = DSTP_FIELD_IPV6,
		.input_index = SRCP_FIELD_IPV6,
		.offset = sizeof(struct ipv6_hdr) -
			offsetof(struct ipv6_hdr, proto) + sizeof(uint16_t),
	},
};

enum {
	CB_FLD_SRC_ADDR,
	CB_FLD_DST_ADDR,
	CB_FLD_SRC_PORT_LOW,
	CB_FLD_SRC_PORT_DLM,
	CB_FLD_SRC_PORT_HIGH,
	CB_FLD_DST_PORT_LOW,
	CB_FLD_DST_PORT_DLM,
	CB_FLD_DST_PORT_HIGH,
	CB_FLD_PROTO,
	CB_FLD_USERDATA,
	CB_FLD_NUM,
};

RTE_ACL_RULE_DEF(acl4_rule, RTE_DIM(ipv4_defs));
RTE_ACL_RULE_DEF(acl6_rule, RTE_DIM(ipv6_defs));

struct acl_search_t {
	const uint8_t *data_ipv4[MAX_PKT_BURST];
	struct rte_mbuf *m_ipv4[MAX_PKT_BURST];
	uint32_t res_ipv4[MAX_PKT_BURST];
	int num_ipv4;

	const uint8_t *data_ipv6[MAX_PKT_BURST];
	struct rte_mbuf *m_ipv6[MAX_PKT_BURST];
	uint32_t res_ipv6[MAX_PKT_BURST];
	int num_ipv6;
};

static struct {
	char mapped[NB_SOCKETS];
	struct rte_acl_ctx *acx_ipv4[NB_SOCKETS];
	struct rte_acl_ctx *acx_ipv6[NB_SOCKETS];
#ifdef L3FWDACL_DEBUG
	struct acl4_rule *rule_ipv4;
	struct acl6_rule *rule_ipv6;
#endif
} acl_config;

static struct{
	const char *rule_ipv4_name;
	const char *rule_ipv6_name;
	int scalar;
} parm_config;

const char cb_port_delim[] = ":";

static inline void
print_one_ipv4_rule(struct acl4_rule *rule, int extra)
{
	unsigned char a, b, c, d;

	uint32_t_to_char(rule->field[SRC_FIELD_IPV4].value.u32,
			&a, &b, &c, &d);
	printf("%hhu.%hhu.%hhu.%hhu/%u ", a, b, c, d,
			rule->field[SRC_FIELD_IPV4].mask_range.u32);
	uint32_t_to_char(rule->field[DST_FIELD_IPV4].value.u32,
			&a, &b, &c, &d);
	printf("%hhu.%hhu.%hhu.%hhu/%u ", a, b, c, d,
			rule->field[DST_FIELD_IPV4].mask_range.u32);
	printf("%hu : %hu %hu : %hu 0x%hhx/0x%hhx ",
		rule->field[SRCP_FIELD_IPV4].value.u16,
		rule->field[SRCP_FIELD_IPV4].mask_range.u16,
		rule->field[DSTP_FIELD_IPV4].value.u16,
		rule->field[DSTP_FIELD_IPV4].mask_range.u16,
		rule->field[PROTO_FIELD_IPV4].value.u8,
		rule->field[PROTO_FIELD_IPV4].mask_range.u8);
	if (extra)
		printf("0x%x-0x%x-0x%x ",
			rule->data.category_mask,
			rule->data.priority,
			rule->data.userdata);
}

static inline void
print_one_ipv6_rule(struct acl6_rule *rule, int extra)
{
	unsigned char a, b, c, d;

	uint32_t_to_char(rule->field[SRC1_FIELD_IPV6].value.u32,
		&a, &b, &c, &d);
	printf("%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[SRC2_FIELD_IPV6].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[SRC3_FIELD_IPV6].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[SRC4_FIELD_IPV6].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x/%u ", a, b, c, d,
			rule->field[SRC1_FIELD_IPV6].mask_range.u32
			+ rule->field[SRC2_FIELD_IPV6].mask_range.u32
			+ rule->field[SRC3_FIELD_IPV6].mask_range.u32
			+ rule->field[SRC4_FIELD_IPV6].mask_range.u32);

	uint32_t_to_char(rule->field[DST1_FIELD_IPV6].value.u32,
		&a, &b, &c, &d);
	printf("%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[DST2_FIELD_IPV6].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[DST3_FIELD_IPV6].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[DST4_FIELD_IPV6].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x/%u ", a, b, c, d,
			rule->field[DST1_FIELD_IPV6].mask_range.u32
			+ rule->field[DST2_FIELD_IPV6].mask_range.u32
			+ rule->field[DST3_FIELD_IPV6].mask_range.u32
			+ rule->field[DST4_FIELD_IPV6].mask_range.u32);

	printf("%hu : %hu %hu : %hu 0x%hhx/0x%hhx ",
		rule->field[SRCP_FIELD_IPV6].value.u16,
		rule->field[SRCP_FIELD_IPV6].mask_range.u16,
		rule->field[DSTP_FIELD_IPV6].value.u16,
		rule->field[DSTP_FIELD_IPV6].mask_range.u16,
		rule->field[PROTO_FIELD_IPV6].value.u8,
		rule->field[PROTO_FIELD_IPV6].mask_range.u8);
	if (extra)
		printf("0x%x-0x%x-0x%x ",
			rule->data.category_mask,
			rule->data.priority,
			rule->data.userdata);
}

/* Bypass comment and empty lines */
static inline int
is_bypass_line(char *buff)
{
	int i = 0;

	/* comment line */
	if (buff[0] == COMMENT_LEAD_CHAR)
		return 1;
	/* empty line */
	while (buff[i] != '\0') {
		if (!isspace(buff[i]))
			return 0;
		i++;
	}
	return 1;
}

#ifdef L3FWDACL_DEBUG
static inline void
dump_acl4_rule(struct rte_mbuf *m, uint32_t sig)
{
	uint32_t offset = sig & ~ACL_DENY_SIGNATURE;
	unsigned char a, b, c, d;
	struct ipv4_hdr *ipv4_hdr = rte_pktmbuf_mtod_offset(m,
							    struct ipv4_hdr *,
							    sizeof(struct ether_hdr));

	uint32_t_to_char(rte_bswap32(ipv4_hdr->src_addr), &a, &b, &c, &d);
	printf("Packet Src:%hhu.%hhu.%hhu.%hhu ", a, b, c, d);
	uint32_t_to_char(rte_bswap32(ipv4_hdr->dst_addr), &a, &b, &c, &d);
	printf("Dst:%hhu.%hhu.%hhu.%hhu ", a, b, c, d);

	printf("Src port:%hu,Dst port:%hu ",
			rte_bswap16(*(uint16_t *)(ipv4_hdr + 1)),
			rte_bswap16(*((uint16_t *)(ipv4_hdr + 1) + 1)));
	printf("hit ACL %d - ", offset);

	print_one_ipv4_rule(acl_config.rule_ipv4 + offset, 1);

	printf("\n\n");
}

static inline void
dump_acl6_rule(struct rte_mbuf *m, uint32_t sig)
{
	unsigned i;
	uint32_t offset = sig & ~ACL_DENY_SIGNATURE;
	struct ipv6_hdr *ipv6_hdr = rte_pktmbuf_mtod_offset(m,
							    struct ipv6_hdr *,
							    sizeof(struct ether_hdr));

	printf("Packet Src");
	for (i = 0; i < RTE_DIM(ipv6_hdr->src_addr); i += sizeof(uint16_t))
		printf(":%.2x%.2x",
			ipv6_hdr->src_addr[i], ipv6_hdr->src_addr[i + 1]);

	printf("\nDst");
	for (i = 0; i < RTE_DIM(ipv6_hdr->dst_addr); i += sizeof(uint16_t))
		printf(":%.2x%.2x",
			ipv6_hdr->dst_addr[i], ipv6_hdr->dst_addr[i + 1]);

	printf("\nSrc port:%hu,Dst port:%hu ",
			rte_bswap16(*(uint16_t *)(ipv6_hdr + 1)),
			rte_bswap16(*((uint16_t *)(ipv6_hdr + 1) + 1)));
	printf("hit ACL %d - ", offset);

	print_one_ipv6_rule(acl_config.rule_ipv6 + offset, 1);

	printf("\n\n");
}
#endif /* L3FWDACL_DEBUG */

static inline void
dump_ipv4_rules(struct acl4_rule *rule, int num, int extra)
{
	int i;

	for (i = 0; i < num; i++, rule++) {
		printf("\t%d:", i + 1);
		print_one_ipv4_rule(rule, extra);
		printf("\n");
	}
}

static inline void
dump_ipv6_rules(struct acl6_rule *rule, int num, int extra)
{
	int i;

	for (i = 0; i < num; i++, rule++) {
		printf("\t%d:", i + 1);
		print_one_ipv6_rule(rule, extra);
		printf("\n");
	}
}

#ifdef DO_RFC_1812_CHECKS
static inline void
prepare_one_packet(struct rte_mbuf **pkts_in, struct acl_search_t *acl,
	int index)
{
	struct ipv4_hdr *ipv4_hdr;
	struct rte_mbuf *pkt = pkts_in[index];

	if (RTE_ETH_IS_IPV4_HDR(pkt->packet_type)) {
		ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct ipv4_hdr *,
						   sizeof(struct ether_hdr));

		/* Check to make sure the packet is valid (RFC1812) */
		if (is_valid_ipv4_pkt(ipv4_hdr, pkt->pkt_len) >= 0) {

			/* Update time to live and header checksum */
			--(ipv4_hdr->time_to_live);
			++(ipv4_hdr->hdr_checksum);

			/* Fill acl structure */
			acl->data_ipv4[acl->num_ipv4] = MBUF_IPV4_2PROTO(pkt);
			acl->m_ipv4[(acl->num_ipv4)++] = pkt;

		} else {
			/* Not a valid IPv4 packet */
			rte_pktmbuf_free(pkt);
		}
	} else if (RTE_ETH_IS_IPV6_HDR(pkt->packet_type)) {
		/* Fill acl structure */
		acl->data_ipv6[acl->num_ipv6] = MBUF_IPV6_2PROTO(pkt);
		acl->m_ipv6[(acl->num_ipv6)++] = pkt;

	} else {
		/* Unknown type, drop the packet */
		rte_pktmbuf_free(pkt);
	}
}

#else
static inline void
prepare_one_packet(struct rte_mbuf **pkts_in, struct acl_search_t *acl,
	int index)
{
	struct rte_mbuf *pkt = pkts_in[index];

	if (RTE_ETH_IS_IPV4_HDR(pkt->packet_type)) {
		/* Fill acl structure */
		acl->data_ipv4[acl->num_ipv4] = MBUF_IPV4_2PROTO(pkt);
		acl->m_ipv4[(acl->num_ipv4)++] = pkt;

	} else if (RTE_ETH_IS_IPV6_HDR(pkt->packet_type)) {
		/* Fill acl structure */
		acl->data_ipv6[acl->num_ipv6] = MBUF_IPV6_2PROTO(pkt);
		acl->m_ipv6[(acl->num_ipv6)++] = pkt;
	} else {
		/* Unknown type, drop the packet */
		rte_pktmbuf_free(pkt);
	}
}
#endif /* DO_RFC_1812_CHECKS */

static inline void
prepare_acl_parameter(struct rte_mbuf **pkts_in, struct acl_search_t *acl,
	int nb_rx)
{
	int i;

	acl->num_ipv4 = 0;
	acl->num_ipv6 = 0;

	/* Prefetch first packets */
	for (i = 0; i < PREFETCH_OFFSET && i < nb_rx; i++) {
		rte_prefetch0(rte_pktmbuf_mtod(
				pkts_in[i], void *));
	}

	for (i = 0; i < (nb_rx - PREFETCH_OFFSET); i++) {
		rte_prefetch0(rte_pktmbuf_mtod(pkts_in[
				i + PREFETCH_OFFSET], void *));
		prepare_one_packet(pkts_in, acl, i);
	}

	/* Process left packets */
	for (; i < nb_rx; i++)
		prepare_one_packet(pkts_in, acl, i);
}

static inline void
send_one_packet(struct rte_mbuf *m, uint32_t res)
{
	if (likely((res & ACL_DENY_SIGNATURE) == 0 && res != 0)) {
		/* forward packets */
		send_single_packet(m,
			(uint8_t)(res - FWD_PORT_SHIFT));
	} else{
		/* in the ACL list, drop it */
#ifdef L3FWDACL_DEBUG
		if ((res & ACL_DENY_SIGNATURE) != 0) {
			if (RTE_ETH_IS_IPV4_HDR(m->packet_type))
				dump_acl4_rule(m, res);
			else if (RTE_ETH_IS_IPV6_HDR(m->packet_type))
				dump_acl6_rule(m, res);
		}
#endif
		rte_pktmbuf_free(m);
	}
}



static inline void
send_packets(struct rte_mbuf **m, uint32_t *res, int num)
{
	int i;

	/* Prefetch first packets */
	for (i = 0; i < PREFETCH_OFFSET && i < num; i++) {
		rte_prefetch0(rte_pktmbuf_mtod(
				m[i], void *));
	}

	for (i = 0; i < (num - PREFETCH_OFFSET); i++) {
		rte_prefetch0(rte_pktmbuf_mtod(m[
				i + PREFETCH_OFFSET], void *));
		send_one_packet(m[i], res[i]);
	}

	/* Process left packets */
	for (; i < num; i++)
		send_one_packet(m[i], res[i]);
}

/*
 * Parses IPV6 address, exepcts the following format:
 * XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX (where X - is a hexedecimal digit).
 */
static int
parse_ipv6_addr(const char *in, const char **end, uint32_t v[IPV6_ADDR_U32],
	char dlm)
{
	uint32_t addr[IPV6_ADDR_U16];

	GET_CB_FIELD(in, addr[0], 16, UINT16_MAX, ':');
	GET_CB_FIELD(in, addr[1], 16, UINT16_MAX, ':');
	GET_CB_FIELD(in, addr[2], 16, UINT16_MAX, ':');
	GET_CB_FIELD(in, addr[3], 16, UINT16_MAX, ':');
	GET_CB_FIELD(in, addr[4], 16, UINT16_MAX, ':');
	GET_CB_FIELD(in, addr[5], 16, UINT16_MAX, ':');
	GET_CB_FIELD(in, addr[6], 16, UINT16_MAX, ':');
	GET_CB_FIELD(in, addr[7], 16, UINT16_MAX, dlm);

	*end = in;

	v[0] = (addr[0] << 16) + addr[1];
	v[1] = (addr[2] << 16) + addr[3];
	v[2] = (addr[4] << 16) + addr[5];
	v[3] = (addr[6] << 16) + addr[7];

	return 0;
}

static int
parse_ipv6_net(const char *in, struct rte_acl_field field[4])
{
	int32_t rc;
	const char *mp;
	uint32_t i, m, v[4];
	const uint32_t nbu32 = sizeof(uint32_t) * CHAR_BIT;

	/* get address. */
	rc = parse_ipv6_addr(in, &mp, v, '/');
	if (rc != 0)
		return rc;

	/* get mask. */
	GET_CB_FIELD(mp, m, 0, CHAR_BIT * sizeof(v), 0);

	/* put all together. */
	for (i = 0; i != RTE_DIM(v); i++) {
		if (m >= (i + 1) * nbu32)
			field[i].mask_range.u32 = nbu32;
		else
			field[i].mask_range.u32 = m > (i * nbu32) ?
				m - (i * 32) : 0;

		field[i].value.u32 = v[i];
	}

	return 0;
}

static int
parse_cb_ipv6_rule(char *str, struct rte_acl_rule *v, int has_userdata)
{
	int i, rc;
	char *s, *sp, *in[CB_FLD_NUM];
	static const char *dlm = " \t\n";
	int dim = has_userdata ? CB_FLD_NUM : CB_FLD_USERDATA;
	s = str;

	for (i = 0; i != dim; i++, s = NULL) {
		in[i] = strtok_r(s, dlm, &sp);
		if (in[i] == NULL)
			return -EINVAL;
	}

	rc = parse_ipv6_net(in[CB_FLD_SRC_ADDR], v->field + SRC1_FIELD_IPV6);
	if (rc != 0) {
		acl_log("failed to read source address/mask: %s\n",
			in[CB_FLD_SRC_ADDR]);
		return rc;
	}

	rc = parse_ipv6_net(in[CB_FLD_DST_ADDR], v->field + DST1_FIELD_IPV6);
	if (rc != 0) {
		acl_log("failed to read destination address/mask: %s\n",
			in[CB_FLD_DST_ADDR]);
		return rc;
	}

	/* source port. */
	GET_CB_FIELD(in[CB_FLD_SRC_PORT_LOW],
		v->field[SRCP_FIELD_IPV6].value.u16,
		0, UINT16_MAX, 0);
	GET_CB_FIELD(in[CB_FLD_SRC_PORT_HIGH],
		v->field[SRCP_FIELD_IPV6].mask_range.u16,
		0, UINT16_MAX, 0);

	if (strncmp(in[CB_FLD_SRC_PORT_DLM], cb_port_delim,
			sizeof(cb_port_delim)) != 0)
		return -EINVAL;

	/* destination port. */
	GET_CB_FIELD(in[CB_FLD_DST_PORT_LOW],
		v->field[DSTP_FIELD_IPV6].value.u16,
		0, UINT16_MAX, 0);
	GET_CB_FIELD(in[CB_FLD_DST_PORT_HIGH],
		v->field[DSTP_FIELD_IPV6].mask_range.u16,
		0, UINT16_MAX, 0);

	if (strncmp(in[CB_FLD_DST_PORT_DLM], cb_port_delim,
			sizeof(cb_port_delim)) != 0)
		return -EINVAL;

	if (v->field[SRCP_FIELD_IPV6].mask_range.u16
			< v->field[SRCP_FIELD_IPV6].value.u16
			|| v->field[DSTP_FIELD_IPV6].mask_range.u16
			< v->field[DSTP_FIELD_IPV6].value.u16)
		return -EINVAL;

	GET_CB_FIELD(in[CB_FLD_PROTO], v->field[PROTO_FIELD_IPV6].value.u8,
		0, UINT8_MAX, '/');
	GET_CB_FIELD(in[CB_FLD_PROTO], v->field[PROTO_FIELD_IPV6].mask_range.u8,
		0, UINT8_MAX, 0);

	if (has_userdata)
		GET_CB_FIELD(in[CB_FLD_USERDATA], v->data.userdata,
			0, UINT32_MAX, 0);

	return 0;
}

/*
 * Parse ClassBench rules file.
 * Expected format:
 * '@'<src_ipv4_addr>'/'<masklen> <space> \
 * <dst_ipv4_addr>'/'<masklen> <space> \
 * <src_port_low> <space> ":" <src_port_high> <space> \
 * <dst_port_low> <space> ":" <dst_port_high> <space> \
 * <proto>'/'<mask>
 */
static int
parse_ipv4_net(const char *in, uint32_t *addr, uint32_t *mask_len)
{
	uint8_t a, b, c, d, m;

	GET_CB_FIELD(in, a, 0, UINT8_MAX, '.');
	GET_CB_FIELD(in, b, 0, UINT8_MAX, '.');
	GET_CB_FIELD(in, c, 0, UINT8_MAX, '.');
	GET_CB_FIELD(in, d, 0, UINT8_MAX, '/');
	GET_CB_FIELD(in, m, 0, sizeof(uint32_t) * CHAR_BIT, 0);

	addr[0] = IPv4(a, b, c, d);
	mask_len[0] = m;

	return 0;
}

static int
parse_cb_ipv4vlan_rule(char *str, struct rte_acl_rule *v, int has_userdata)
{
	int i, rc;
	char *s, *sp, *in[CB_FLD_NUM];
	static const char *dlm = " \t\n";
	int dim = has_userdata ? CB_FLD_NUM : CB_FLD_USERDATA;
	s = str;

	for (i = 0; i != dim; i++, s = NULL) {
		in[i] = strtok_r(s, dlm, &sp);
		if (in[i] == NULL)
			return -EINVAL;
	}

	rc = parse_ipv4_net(in[CB_FLD_SRC_ADDR],
			&v->field[SRC_FIELD_IPV4].value.u32,
			&v->field[SRC_FIELD_IPV4].mask_range.u32);
	if (rc != 0) {
			acl_log("failed to read source address/mask: %s\n",
			in[CB_FLD_SRC_ADDR]);
		return rc;
	}

	rc = parse_ipv4_net(in[CB_FLD_DST_ADDR],
			&v->field[DST_FIELD_IPV4].value.u32,
			&v->field[DST_FIELD_IPV4].mask_range.u32);
	if (rc != 0) {
		acl_log("failed to read destination address/mask: %s\n",
			in[CB_FLD_DST_ADDR]);
		return rc;
	}

	GET_CB_FIELD(in[CB_FLD_SRC_PORT_LOW],
		v->field[SRCP_FIELD_IPV4].value.u16,
		0, UINT16_MAX, 0);
	GET_CB_FIELD(in[CB_FLD_SRC_PORT_HIGH],
		v->field[SRCP_FIELD_IPV4].mask_range.u16,
		0, UINT16_MAX, 0);

	if (strncmp(in[CB_FLD_SRC_PORT_DLM], cb_port_delim,
			sizeof(cb_port_delim)) != 0)
		return -EINVAL;

	GET_CB_FIELD(in[CB_FLD_DST_PORT_LOW],
		v->field[DSTP_FIELD_IPV4].value.u16,
		0, UINT16_MAX, 0);
	GET_CB_FIELD(in[CB_FLD_DST_PORT_HIGH],
		v->field[DSTP_FIELD_IPV4].mask_range.u16,
		0, UINT16_MAX, 0);

	if (strncmp(in[CB_FLD_DST_PORT_DLM], cb_port_delim,
			sizeof(cb_port_delim)) != 0)
		return -EINVAL;

	if (v->field[SRCP_FIELD_IPV4].mask_range.u16
			< v->field[SRCP_FIELD_IPV4].value.u16
			|| v->field[DSTP_FIELD_IPV4].mask_range.u16
			< v->field[DSTP_FIELD_IPV4].value.u16)
		return -EINVAL;

	GET_CB_FIELD(in[CB_FLD_PROTO], v->field[PROTO_FIELD_IPV4].value.u8,
		0, UINT8_MAX, '/');
	GET_CB_FIELD(in[CB_FLD_PROTO], v->field[PROTO_FIELD_IPV4].mask_range.u8,
		0, UINT8_MAX, 0);

	if (has_userdata)
		GET_CB_FIELD(in[CB_FLD_USERDATA], v->data.userdata, 0,
			UINT32_MAX, 0);

	return 0;
}

static int
add_rules(const char *rule_path,
		struct rte_acl_rule **proute_base,
		unsigned int *proute_num,
		struct rte_acl_rule **pacl_base,
		unsigned int *pacl_num, uint32_t rule_size,
		int (*parser)(char *, struct rte_acl_rule*, int))
{
	uint8_t *acl_rules, *route_rules;
	struct rte_acl_rule *next;
	unsigned int acl_num = 0, route_num = 0, total_num = 0;
	unsigned int acl_cnt = 0, route_cnt = 0;
	char buff[LINE_MAX];
	FILE *fh = fopen(rule_path, "rb");
	unsigned int i = 0;
	int val;

	if (fh == NULL)
		rte_exit(EXIT_FAILURE, "%s: Open %s failed\n", __func__,
			rule_path);

	while ((fgets(buff, LINE_MAX, fh) != NULL)) {
		if (buff[0] == ROUTE_LEAD_CHAR)
			route_num++;
		else if (buff[0] == ACL_LEAD_CHAR)
			acl_num++;
	}

	if (0 == route_num)
		rte_exit(EXIT_FAILURE, "Not find any route entries in %s!\n",
				rule_path);

	val = fseek(fh, 0, SEEK_SET);
	if (val < 0) {
		rte_exit(EXIT_FAILURE, "%s: File seek operation failed\n",
			__func__);
	}

	acl_rules = calloc(acl_num, rule_size);

	if (NULL == acl_rules)
		rte_exit(EXIT_FAILURE, "%s: failed to malloc memory\n",
			__func__);

	route_rules = calloc(route_num, rule_size);

	if (NULL == route_rules)
		rte_exit(EXIT_FAILURE, "%s: failed to malloc memory\n",
			__func__);

	i = 0;
	while (fgets(buff, LINE_MAX, fh) != NULL) {
		i++;

		if (is_bypass_line(buff))
			continue;

		char s = buff[0];

		/* Route entry */
		if (s == ROUTE_LEAD_CHAR)
			next = (struct rte_acl_rule *)(route_rules +
				route_cnt * rule_size);

		/* ACL entry */
		else if (s == ACL_LEAD_CHAR)
			next = (struct rte_acl_rule *)(acl_rules +
				acl_cnt * rule_size);

		/* Illegal line */
		else
			rte_exit(EXIT_FAILURE,
				"%s Line %u: should start with leading "
				"char %c or %c\n",
				rule_path, i, ROUTE_LEAD_CHAR, ACL_LEAD_CHAR);

		if (parser(buff + 1, next, s == ROUTE_LEAD_CHAR) != 0)
			rte_exit(EXIT_FAILURE,
				"%s Line %u: parse rules error\n",
				rule_path, i);

		if (s == ROUTE_LEAD_CHAR) {
			/* Check the forwarding port number */
			if ((enabled_port_mask & (1 << next->data.userdata)) ==
					0)
				rte_exit(EXIT_FAILURE,
					"%s Line %u: fwd number illegal:%u\n",
					rule_path, i, next->data.userdata);
			next->data.userdata += FWD_PORT_SHIFT;
			route_cnt++;
		} else {
			next->data.userdata = ACL_DENY_SIGNATURE + acl_cnt;
			acl_cnt++;
		}

		next->data.priority = RTE_ACL_MAX_PRIORITY - total_num;
		next->data.category_mask = -1;
		total_num++;
	}

	fclose(fh);

	*pacl_base = (struct rte_acl_rule *)acl_rules;
	*pacl_num = acl_num;
	*proute_base = (struct rte_acl_rule *)route_rules;
	*proute_num = route_cnt;

	return 0;
}

static void
dump_acl_config(void)
{
	printf("ACL option are:\n");
	printf(OPTION_RULE_IPV4": %s\n", parm_config.rule_ipv4_name);
	printf(OPTION_RULE_IPV6": %s\n", parm_config.rule_ipv6_name);
	printf(OPTION_SCALAR": %d\n", parm_config.scalar);
}

static int
check_acl_config(void)
{
	if (parm_config.rule_ipv4_name == NULL) {
		acl_log("ACL IPv4 rule file not specified\n");
		return -1;
	} else if (parm_config.rule_ipv6_name == NULL) {
		acl_log("ACL IPv6 rule file not specified\n");
		return -1;
	}

	return 0;
}

static struct rte_acl_ctx*
setup_acl(struct rte_acl_rule *route_base,
		struct rte_acl_rule *acl_base, unsigned int route_num,
		unsigned int acl_num, int ipv6, int socketid)
{
	char name[PATH_MAX];
	struct rte_acl_param acl_param;
	struct rte_acl_config acl_build_param;
	struct rte_acl_ctx *context;
	int dim = ipv6 ? RTE_DIM(ipv6_defs) : RTE_DIM(ipv4_defs);

	/* Create ACL contexts */
	snprintf(name, sizeof(name), "%s%d",
			ipv6 ? L3FWD_ACL_IPV6_NAME : L3FWD_ACL_IPV4_NAME,
			socketid);

	acl_param.name = name;
	acl_param.socket_id = socketid;
	acl_param.rule_size = RTE_ACL_RULE_SZ(dim);
	acl_param.max_rule_num = MAX_ACL_RULE_NUM;

	if ((context = rte_acl_create(&acl_param)) == NULL)
		rte_exit(EXIT_FAILURE, "Failed to create ACL context\n");

	if (parm_config.scalar && rte_acl_set_ctx_classify(context,
			RTE_ACL_CLASSIFY_SCALAR) != 0)
		rte_exit(EXIT_FAILURE,
			"Failed to setup classify method for  ACL context\n");

	if (rte_acl_add_rules(context, route_base, route_num) < 0)
			rte_exit(EXIT_FAILURE, "add rules failed\n");

	if (rte_acl_add_rules(context, acl_base, acl_num) < 0)
			rte_exit(EXIT_FAILURE, "add rules failed\n");

	/* Perform builds */
	memset(&acl_build_param, 0, sizeof(acl_build_param));

	acl_build_param.num_categories = DEFAULT_MAX_CATEGORIES;
	acl_build_param.num_fields = dim;
	memcpy(&acl_build_param.defs, ipv6 ? ipv6_defs : ipv4_defs,
		ipv6 ? sizeof(ipv6_defs) : sizeof(ipv4_defs));

	if (rte_acl_build(context, &acl_build_param) != 0)
		rte_exit(EXIT_FAILURE, "Failed to build ACL trie\n");

	rte_acl_dump(context);

	return context;
}

static int
app_acl_init(void)
{
	unsigned lcore_id;
	unsigned int i;
	int socketid;
	struct rte_acl_rule *acl_base_ipv4, *route_base_ipv4,
		*acl_base_ipv6, *route_base_ipv6;
	unsigned int acl_num_ipv4 = 0, route_num_ipv4 = 0,
		acl_num_ipv6 = 0, route_num_ipv6 = 0;

	if (check_acl_config() != 0)
		rte_exit(EXIT_FAILURE, "Failed to get valid ACL options\n");

	dump_acl_config();

	/* Load  rules from the input file */
	if (add_rules(parm_config.rule_ipv4_name, &route_base_ipv4,
			&route_num_ipv4, &acl_base_ipv4, &acl_num_ipv4,
			sizeof(struct acl4_rule), &parse_cb_ipv4vlan_rule) < 0)
		rte_exit(EXIT_FAILURE, "Failed to add rules\n");

	acl_log("IPv4 Route entries %u:\n", route_num_ipv4);
	dump_ipv4_rules((struct acl4_rule *)route_base_ipv4, route_num_ipv4, 1);

	acl_log("IPv4 ACL entries %u:\n", acl_num_ipv4);
	dump_ipv4_rules((struct acl4_rule *)acl_base_ipv4, acl_num_ipv4, 1);

	if (add_rules(parm_config.rule_ipv6_name, &route_base_ipv6,
			&route_num_ipv6,
			&acl_base_ipv6, &acl_num_ipv6,
			sizeof(struct acl6_rule), &parse_cb_ipv6_rule) < 0)
		rte_exit(EXIT_FAILURE, "Failed to add rules\n");

	acl_log("IPv6 Route entries %u:\n", route_num_ipv6);
	dump_ipv6_rules((struct acl6_rule *)route_base_ipv6, route_num_ipv6, 1);

	acl_log("IPv6 ACL entries %u:\n", acl_num_ipv6);
	dump_ipv6_rules((struct acl6_rule *)acl_base_ipv6, acl_num_ipv6, 1);

	memset(&acl_config, 0, sizeof(acl_config));

	/* Check sockets a context should be created on */
	if (!numa_on)
		acl_config.mapped[0] = 1;
	else {
		for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
			if (rte_lcore_is_enabled(lcore_id) == 0)
				continue;

			socketid = rte_lcore_to_socket_id(lcore_id);
			if (socketid >= NB_SOCKETS) {
				acl_log("Socket %d of lcore %u is out "
					"of range %d\n",
					socketid, lcore_id, NB_SOCKETS);
				free(route_base_ipv4);
				free(route_base_ipv6);
				free(acl_base_ipv4);
				free(acl_base_ipv6);
				return -1;
			}

			acl_config.mapped[socketid] = 1;
		}
	}

	for (i = 0; i < NB_SOCKETS; i++) {
		if (acl_config.mapped[i]) {
			acl_config.acx_ipv4[i] = setup_acl(route_base_ipv4,
				acl_base_ipv4, route_num_ipv4, acl_num_ipv4,
				0, i);

			acl_config.acx_ipv6[i] = setup_acl(route_base_ipv6,
				acl_base_ipv6, route_num_ipv6, acl_num_ipv6,
				1, i);
		}
	}

	free(route_base_ipv4);
	free(route_base_ipv6);

#ifdef L3FWDACL_DEBUG
	acl_config.rule_ipv4 = (struct acl4_rule *)acl_base_ipv4;
	acl_config.rule_ipv6 = (struct acl6_rule *)acl_base_ipv6;
#else
	free(acl_base_ipv4);
	free(acl_base_ipv6);
#endif

	return 0;
}

/***********************end of ACL part******************************/

struct lcore_conf {
	uint16_t n_rx_queue;
	struct lcore_rx_queue rx_queue_list[MAX_RX_QUEUE_PER_LCORE];
	uint16_t n_tx_port;
	uint16_t tx_port_id[RTE_MAX_ETHPORTS];
	uint16_t tx_queue_id[RTE_MAX_ETHPORTS];
	struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];
} __rte_cache_aligned;

static struct lcore_conf lcore_conf[RTE_MAX_LCORE];

/* Enqueue a single packet, and send burst if queue is filled */
static inline void
send_single_packet(struct rte_mbuf *m, uint16_t port)
{
	uint32_t lcore_id;
	struct lcore_conf *qconf;

	lcore_id = rte_lcore_id();

	qconf = &lcore_conf[lcore_id];
	rte_eth_tx_buffer(port, qconf->tx_queue_id[port],
			qconf->tx_buffer[port], m);
}

#ifdef DO_RFC_1812_CHECKS
static inline int
is_valid_ipv4_pkt(struct ipv4_hdr *pkt, uint32_t link_len)
{
	/* From http://www.rfc-editor.org/rfc/rfc1812.txt section 5.2.2 */
	/*
	 * 1. The packet length reported by the Link Layer must be large
	 * enough to hold the minimum length legal IP datagram (20 bytes).
	 */
	if (link_len < sizeof(struct ipv4_hdr))
		return -1;

	/* 2. The IP checksum must be correct. */
	/* this is checked in H/W */

	/*
	 * 3. The IP version number must be 4. If the version number is not 4
	 * then the packet may be another version of IP, such as IPng or
	 * ST-II.
	 */
	if (((pkt->version_ihl) >> 4) != 4)
		return -3;
	/*
	 * 4. The IP header length field must be large enough to hold the
	 * minimum length legal IP datagram (20 bytes = 5 words).
	 */
	if ((pkt->version_ihl & 0xf) < 5)
		return -4;

	/*
	 * 5. The IP total length field must be large enough to hold the IP
	 * datagram header, whose length is specified in the IP header length
	 * field.
	 */
	if (rte_cpu_to_be_16(pkt->total_length) < sizeof(struct ipv4_hdr))
		return -5;

	return 0;
}
#endif

/* main processing loop */
static int
main_loop(__attribute__((unused)) void *dummy)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc;
	int i, nb_rx;
	uint16_t portid;
	uint8_t queueid;
	struct lcore_conf *qconf;
	int socketid;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1)
			/ US_PER_S * BURST_TX_DRAIN_US;

	prev_tsc = 0;
	lcore_id = rte_lcore_id();
	qconf = &lcore_conf[lcore_id];
	socketid = rte_lcore_to_socket_id(lcore_id);

	if (qconf->n_rx_queue == 0) {
		RTE_LOG(INFO, L3FWD, "lcore %u has nothing to do\n", lcore_id);
		return 0;
	}

	RTE_LOG(INFO, L3FWD, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_queue; i++) {

		portid = qconf->rx_queue_list[i].port_id;
		queueid = qconf->rx_queue_list[i].queue_id;
		RTE_LOG(INFO, L3FWD,
			" -- lcoreid=%u portid=%u rxqueueid=%hhu\n",
			lcore_id, portid, queueid);
	}

	while (1) {

		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {
			for (i = 0; i < qconf->n_tx_port; ++i) {
				portid = qconf->tx_port_id[i];
				rte_eth_tx_buffer_flush(portid,
						qconf->tx_queue_id[portid],
						qconf->tx_buffer[portid]);
			}
			prev_tsc = cur_tsc;
		}

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->n_rx_queue; ++i) {

			portid = qconf->rx_queue_list[i].port_id;
			queueid = qconf->rx_queue_list[i].queue_id;
			nb_rx = rte_eth_rx_burst(portid, queueid,
				pkts_burst, MAX_PKT_BURST);

			if (nb_rx > 0) {
				struct acl_search_t acl_search;

				prepare_acl_parameter(pkts_burst, &acl_search,
					nb_rx);

				if (acl_search.num_ipv4) {
					rte_acl_classify(
						acl_config.acx_ipv4[socketid],
						acl_search.data_ipv4,
						acl_search.res_ipv4,
						acl_search.num_ipv4,
						DEFAULT_MAX_CATEGORIES);

					send_packets(acl_search.m_ipv4,
						acl_search.res_ipv4,
						acl_search.num_ipv4);
				}

				if (acl_search.num_ipv6) {
					rte_acl_classify(
						acl_config.acx_ipv6[socketid],
						acl_search.data_ipv6,
						acl_search.res_ipv6,
						acl_search.num_ipv6,
						DEFAULT_MAX_CATEGORIES);

					send_packets(acl_search.m_ipv6,
						acl_search.res_ipv6,
						acl_search.num_ipv6);
				}
			}
		}
	}
}

static int
check_lcore_params(void)
{
	uint8_t queue, lcore;
	uint16_t i;
	int socketid;

	for (i = 0; i < nb_lcore_params; ++i) {
		queue = lcore_params[i].queue_id;
		if (queue >= MAX_RX_QUEUE_PER_PORT) {
			printf("invalid queue number: %hhu\n", queue);
			return -1;
		}
		lcore = lcore_params[i].lcore_id;
		if (!rte_lcore_is_enabled(lcore)) {
			printf("error: lcore %hhu is not enabled in "
				"lcore mask\n", lcore);
			return -1;
		}
		socketid = rte_lcore_to_socket_id(lcore);
		if (socketid != 0 && numa_on == 0) {
			printf("warning: lcore %hhu is on socket %d "
				"with numa off\n",
				lcore, socketid);
		}
	}
	return 0;
}

static int
check_port_config(void)
{
	unsigned portid;
	uint16_t i;

	for (i = 0; i < nb_lcore_params; ++i) {
		portid = lcore_params[i].port_id;

		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("port %u is not enabled in port mask\n", portid);
			return -1;
		}
		if (!rte_eth_dev_is_valid_port(portid)) {
			printf("port %u is not present on the board\n", portid);
			return -1;
		}
	}
	return 0;
}

static uint8_t
get_port_n_rx_queues(const uint16_t port)
{
	int queue = -1;
	uint16_t i;

	for (i = 0; i < nb_lcore_params; ++i) {
		if (lcore_params[i].port_id == port &&
				lcore_params[i].queue_id > queue)
			queue = lcore_params[i].queue_id;
	}
	return (uint8_t)(++queue);
}

static int
init_lcore_rx_queues(void)
{
	uint16_t i, nb_rx_queue;
	uint8_t lcore;

	for (i = 0; i < nb_lcore_params; ++i) {
		lcore = lcore_params[i].lcore_id;
		nb_rx_queue = lcore_conf[lcore].n_rx_queue;
		if (nb_rx_queue >= MAX_RX_QUEUE_PER_LCORE) {
			printf("error: too many queues (%u) for lcore: %u\n",
				(unsigned)nb_rx_queue + 1, (unsigned)lcore);
			return -1;
		} else {
			lcore_conf[lcore].rx_queue_list[nb_rx_queue].port_id =
				lcore_params[i].port_id;
			lcore_conf[lcore].rx_queue_list[nb_rx_queue].queue_id =
				lcore_params[i].queue_id;
			lcore_conf[lcore].n_rx_queue++;
		}
	}
	return 0;
}

/* display usage */
static void
print_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK -P"
		"--"OPTION_RULE_IPV4"=FILE"
		"--"OPTION_RULE_IPV6"=FILE"
		"  [--"OPTION_CONFIG" (port,queue,lcore)[,(port,queue,lcore]]"
		"  [--"OPTION_ENBJMO" [--max-pkt-len PKTLEN]]\n"
		"  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
		"  -P : enable promiscuous mode\n"
		"  --"OPTION_CONFIG": (port,queue,lcore): "
		"rx queues configuration\n"
		"  --"OPTION_NONUMA": optional, disable numa awareness\n"
		"  --"OPTION_ENBJMO": enable jumbo frame"
		" which max packet len is PKTLEN in decimal (64-9600)\n"
		"  --"OPTION_RULE_IPV4"=FILE: specify the ipv4 rules entries "
		"file. "
		"Each rule occupy one line. "
		"2 kinds of rules are supported. "
		"One is ACL entry at while line leads with character '%c', "
		"another is route entry at while line leads with "
		"character '%c'.\n"
		"  --"OPTION_RULE_IPV6"=FILE: specify the ipv6 rules "
		"entries file.\n"
		"  --"OPTION_SCALAR": Use scalar function to do lookup\n",
		prgname, ACL_LEAD_CHAR, ROUTE_LEAD_CHAR);
}

static int
parse_max_pkt_len(const char *pktlen)
{
	char *end = NULL;
	unsigned long len;

	/* parse decimal string */
	len = strtoul(pktlen, &end, 10);
	if ((pktlen[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (len == 0)
		return -1;

	return len;
}

static int
parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (pm == 0)
		return -1;

	return pm;
}

static int
parse_config(const char *q_arg)
{
	char s[256];
	const char *p, *p0 = q_arg;
	char *end;
	enum fieldnames {
		FLD_PORT = 0,
		FLD_QUEUE,
		FLD_LCORE,
		_NUM_FLD
	};
	unsigned long int_fld[_NUM_FLD];
	char *str_fld[_NUM_FLD];
	int i;
	unsigned size;

	nb_lcore_params = 0;

	while ((p = strchr(p0, '(')) != NULL) {
		++p;
		if ((p0 = strchr(p, ')')) == NULL)
			return -1;

		size = p0 - p;
		if (size >= sizeof(s))
			return -1;

		snprintf(s, sizeof(s), "%.*s", size, p);
		if (rte_strsplit(s, sizeof(s), str_fld, _NUM_FLD, ',') !=
				_NUM_FLD)
			return -1;
		for (i = 0; i < _NUM_FLD; i++) {
			errno = 0;
			int_fld[i] = strtoul(str_fld[i], &end, 0);
			if (errno != 0 || end == str_fld[i] || int_fld[i] > 255)
				return -1;
		}
		if (nb_lcore_params >= MAX_LCORE_PARAMS) {
			printf("exceeded max number of lcore params: %hu\n",
				nb_lcore_params);
			return -1;
		}
		lcore_params_array[nb_lcore_params].port_id =
			(uint8_t)int_fld[FLD_PORT];
		lcore_params_array[nb_lcore_params].queue_id =
			(uint8_t)int_fld[FLD_QUEUE];
		lcore_params_array[nb_lcore_params].lcore_id =
			(uint8_t)int_fld[FLD_LCORE];
		++nb_lcore_params;
	}
	lcore_params = lcore_params_array;
	return 0;
}

/* Parse the argument given in the command line of the application */
static int
parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = {
		{OPTION_CONFIG, 1, 0, 0},
		{OPTION_NONUMA, 0, 0, 0},
		{OPTION_ENBJMO, 0, 0, 0},
		{OPTION_RULE_IPV4, 1, 0, 0},
		{OPTION_RULE_IPV6, 1, 0, 0},
		{OPTION_SCALAR, 0, 0, 0},
		{NULL, 0, 0, 0}
	};

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "p:P",
				lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			enabled_port_mask = parse_portmask(optarg);
			if (enabled_port_mask == 0) {
				printf("invalid portmask\n");
				print_usage(prgname);
				return -1;
			}
			break;
		case 'P':
			printf("Promiscuous mode selected\n");
			promiscuous_on = 1;
			break;

		/* long options */
		case 0:
			if (!strncmp(lgopts[option_index].name,
					OPTION_CONFIG,
					sizeof(OPTION_CONFIG))) {
				ret = parse_config(optarg);
				if (ret) {
					printf("invalid config\n");
					print_usage(prgname);
					return -1;
				}
			}

			if (!strncmp(lgopts[option_index].name,
					OPTION_NONUMA,
					sizeof(OPTION_NONUMA))) {
				printf("numa is disabled\n");
				numa_on = 0;
			}

			if (!strncmp(lgopts[option_index].name,
					OPTION_ENBJMO, sizeof(OPTION_ENBJMO))) {
				struct option lenopts = {
					"max-pkt-len",
					required_argument,
					0,
					0
				};

				printf("jumbo frame is enabled\n");
				port_conf.rxmode.offloads |=
						DEV_RX_OFFLOAD_JUMBO_FRAME;
				port_conf.txmode.offloads |=
						DEV_TX_OFFLOAD_MULTI_SEGS;

				/*
				 * if no max-pkt-len set, then use the
				 * default value ETHER_MAX_LEN
				 */
				if (0 == getopt_long(argc, argvopt, "",
						&lenopts, &option_index)) {
					ret = parse_max_pkt_len(optarg);
					if ((ret < 64) ||
						(ret > MAX_JUMBO_PKT_LEN)) {
						printf("invalid packet "
							"length\n");
						print_usage(prgname);
						return -1;
					}
					port_conf.rxmode.max_rx_pkt_len = ret;
				}
				printf("set jumbo frame max packet length "
					"to %u\n",
					(unsigned int)
					port_conf.rxmode.max_rx_pkt_len);
			}

			if (!strncmp(lgopts[option_index].name,
					OPTION_RULE_IPV4,
					sizeof(OPTION_RULE_IPV4)))
				parm_config.rule_ipv4_name = optarg;

			if (!strncmp(lgopts[option_index].name,
					OPTION_RULE_IPV6,
					sizeof(OPTION_RULE_IPV6))) {
				parm_config.rule_ipv6_name = optarg;
			}

			if (!strncmp(lgopts[option_index].name,
					OPTION_SCALAR, sizeof(OPTION_SCALAR)))
				parm_config.scalar = 1;


			break;

		default:
			print_usage(prgname);
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 1; /* reset getopt lib */
	return ret;
}

static void
print_ethaddr(const char *name, const struct ether_addr *eth_addr)
{
	char buf[ETHER_ADDR_FMT_SIZE];
	ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", name, buf);
}

static int
init_mem(unsigned nb_mbuf)
{
	int socketid;
	unsigned lcore_id;
	char s[64];

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		if (numa_on)
			socketid = rte_lcore_to_socket_id(lcore_id);
		else
			socketid = 0;

		if (socketid >= NB_SOCKETS) {
			rte_exit(EXIT_FAILURE,
				"Socket %d of lcore %u is out of range %d\n",
				socketid, lcore_id, NB_SOCKETS);
		}
		if (pktmbuf_pool[socketid] == NULL) {
			snprintf(s, sizeof(s), "mbuf_pool_%d", socketid);
			pktmbuf_pool[socketid] =
				rte_pktmbuf_pool_create(s, nb_mbuf,
					MEMPOOL_CACHE_SIZE, 0,
					RTE_MBUF_DEFAULT_BUF_SIZE,
					socketid);
			if (pktmbuf_pool[socketid] == NULL)
				rte_exit(EXIT_FAILURE,
					"Cannot init mbuf pool on socket %d\n",
					socketid);
			else
				printf("Allocated mbuf pool on socket %d\n",
					socketid);
		}
	}
	return 0;
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

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		RTE_ETH_FOREACH_DEV(portid) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf(
					"Port%d Link Up. Speed %u Mbps %s\n",
						portid, link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
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

int
main(int argc, char **argv)
{
	struct lcore_conf *qconf;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf *txconf;
	int ret;
	unsigned nb_ports;
	uint16_t queueid;
	unsigned lcore_id;
	uint32_t n_tx_queue, nb_lcores;
	uint16_t portid;
	uint8_t nb_rx_queue, queue, socketid;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL parameters\n");
	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L3FWD parameters\n");

	if (check_lcore_params() < 0)
		rte_exit(EXIT_FAILURE, "check_lcore_params failed\n");

	ret = init_lcore_rx_queues();
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "init_lcore_rx_queues failed\n");

	nb_ports = rte_eth_dev_count_avail();

	if (check_port_config() < 0)
		rte_exit(EXIT_FAILURE, "check_port_config failed\n");

	/* Add ACL rules and route entries, build trie */
	if (app_acl_init() < 0)
		rte_exit(EXIT_FAILURE, "app_acl_init failed\n");

	nb_lcores = rte_lcore_count();

	/* initialize all ports */
	RTE_ETH_FOREACH_DEV(portid) {
		struct rte_eth_conf local_port_conf = port_conf;

		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("\nSkipping disabled port %d\n", portid);
			continue;
		}

		/* init port */
		printf("Initializing port %d ... ", portid);
		fflush(stdout);

		nb_rx_queue = get_port_n_rx_queues(portid);
		n_tx_queue = nb_lcores;
		if (n_tx_queue > MAX_TX_QUEUE_PER_PORT)
			n_tx_queue = MAX_TX_QUEUE_PER_PORT;
		printf("Creating queues: nb_rxq=%d nb_txq=%u... ",
			nb_rx_queue, (unsigned)n_tx_queue);
		rte_eth_dev_info_get(portid, &dev_info);
		if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
			local_port_conf.txmode.offloads |=
				DEV_TX_OFFLOAD_MBUF_FAST_FREE;
		ret = rte_eth_dev_configure(portid, nb_rx_queue,
					(uint16_t)n_tx_queue, &local_port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				"Cannot configure device: err=%d, port=%d\n",
				ret, portid);

		ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
						       &nb_txd);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				"rte_eth_dev_adjust_nb_rx_tx_desc: err=%d, port=%d\n",
				ret, portid);

		rte_eth_macaddr_get(portid, &ports_eth_addr[portid]);
		print_ethaddr(" Address:", &ports_eth_addr[portid]);
		printf(", ");

		/* init memory */
		ret = init_mem(NB_MBUF);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "init_mem failed\n");

		for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
			if (rte_lcore_is_enabled(lcore_id) == 0)
				continue;

			/* Initialize TX buffers */
			qconf = &lcore_conf[lcore_id];
			qconf->tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
					RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
					rte_eth_dev_socket_id(portid));
			if (qconf->tx_buffer[portid] == NULL)
				rte_exit(EXIT_FAILURE, "Can't allocate tx buffer for port %u\n",
						(unsigned) portid);

			rte_eth_tx_buffer_init(qconf->tx_buffer[portid], MAX_PKT_BURST);
		}

		/* init one TX queue per couple (lcore,port) */
		queueid = 0;
		for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
			if (rte_lcore_is_enabled(lcore_id) == 0)
				continue;

			if (numa_on)
				socketid = (uint8_t)
					rte_lcore_to_socket_id(lcore_id);
			else
				socketid = 0;

			printf("txq=%u,%d,%d ", lcore_id, queueid, socketid);
			fflush(stdout);

			rte_eth_dev_info_get(portid, &dev_info);
			txconf = &dev_info.default_txconf;
			txconf->txq_flags = ETH_TXQ_FLAGS_IGNORE;
			txconf->offloads = local_port_conf.txmode.offloads;
			ret = rte_eth_tx_queue_setup(portid, queueid, nb_txd,
						     socketid, txconf);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
					"rte_eth_tx_queue_setup: err=%d, "
					"port=%d\n", ret, portid);

			qconf = &lcore_conf[lcore_id];
			qconf->tx_queue_id[portid] = queueid;
			queueid++;

			qconf->tx_port_id[qconf->n_tx_port] = portid;
			qconf->n_tx_port++;
		}
		printf("\n");
	}

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;
		qconf = &lcore_conf[lcore_id];
		printf("\nInitializing rx queues on lcore %u ... ", lcore_id);
		fflush(stdout);
		/* init RX queues */
		for (queue = 0; queue < qconf->n_rx_queue; ++queue) {
			struct rte_eth_dev *dev;
			struct rte_eth_conf *conf;
			struct rte_eth_rxconf rxq_conf;

			portid = qconf->rx_queue_list[queue].port_id;
			queueid = qconf->rx_queue_list[queue].queue_id;
			dev = &rte_eth_devices[portid];
			conf = &dev->data->dev_conf;

			if (numa_on)
				socketid = (uint8_t)
					rte_lcore_to_socket_id(lcore_id);
			else
				socketid = 0;

			printf("rxq=%d,%d,%d ", portid, queueid, socketid);
			fflush(stdout);

			rte_eth_dev_info_get(portid, &dev_info);
			rxq_conf = dev_info.default_rxconf;
			rxq_conf.offloads = conf->rxmode.offloads;
			ret = rte_eth_rx_queue_setup(portid, queueid, nb_rxd,
					socketid, &rxq_conf,
					pktmbuf_pool[socketid]);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
					"rte_eth_rx_queue_setup: err=%d,"
					"port=%d\n", ret, portid);
		}
	}

	printf("\n");

	/* start ports */
	RTE_ETH_FOREACH_DEV(portid) {
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;

		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				"rte_eth_dev_start: err=%d, port=%d\n",
				ret, portid);

		/*
		 * If enabled, put device in promiscuous mode.
		 * This allows IO forwarding mode to forward packets
		 * to itself through 2 cross-connected  ports of the
		 * target machine.
		 */
		if (promiscuous_on)
			rte_eth_promiscuous_enable(portid);
	}

	check_all_ports_link_status(enabled_port_mask);

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(main_loop, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	return 0;
}
