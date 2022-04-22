/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

/* Log file related character defs. */
#define COMMENT_LEAD_CHAR	('#')
#define ROUTE_LEAD_CHAR		('R')
#define ACL_LEAD_CHAR		('@')

#define	IPV6_ADDR_LEN	16
#define	IPV6_ADDR_U16	(IPV6_ADDR_LEN / sizeof(uint16_t))
#define	IPV6_ADDR_U32	(IPV6_ADDR_LEN / sizeof(uint32_t))

#define GET_CB_FIELD(in, fd, base, lim, dlm)	do {            \
	unsigned long val;                                      \
	char *end;                                              \
	errno = 0;                                              \
	val = strtoul((in), &end, (base));                      \
	if (errno != 0 || end[0] != (dlm) || val > (lim))       \
		return -EINVAL;                                 \
	(fd) = (typeof(fd))val;                                 \
	(in) = end + 1;                                         \
} while (0)

struct ipv4_l3fwd_route {
	uint32_t ip;
	uint8_t  depth;
	uint8_t  if_out;
};

struct ipv6_l3fwd_route {
	uint8_t ip[16];
	uint8_t depth;
	uint8_t if_out;
};

struct ipv4_5tuple {
	uint32_t ip_dst;
	uint32_t ip_src;
	uint16_t port_dst;
	uint16_t port_src;
	uint8_t  proto;
} __rte_packed;

struct ipv6_5tuple {
	uint8_t  ip_dst[IPV6_ADDR_LEN];
	uint8_t  ip_src[IPV6_ADDR_LEN];
	uint16_t port_dst;
	uint16_t port_src;
	uint8_t  proto;
} __rte_packed;

struct lpm_route_rule {
	union {
		uint32_t ip;
		union {
			uint32_t ip_32[IPV6_ADDR_U32];
			uint8_t ip_8[IPV6_ADDR_LEN];
		};
	};
	uint8_t depth;
	uint8_t if_out;
};

struct ipv4_l3fwd_em_route {
	struct ipv4_5tuple key;
	uint8_t if_out;
};

struct ipv6_l3fwd_em_route {
	struct ipv6_5tuple key;
	uint8_t if_out;
};

struct em_rule {
		union {
		struct ipv4_5tuple v4_key;
		struct ipv6_5tuple v6_key;
	};
	uint8_t if_out;
};

extern struct lpm_route_rule *route_base_v4;
extern struct lpm_route_rule *route_base_v6;
extern int route_num_v4;
extern int route_num_v6;

extern const struct ipv4_l3fwd_route ipv4_l3fwd_route_array[16];
extern const struct ipv6_l3fwd_route ipv6_l3fwd_route_array[16];

extern const struct ipv4_l3fwd_em_route ipv4_l3fwd_em_route_array[16];
extern const struct ipv6_l3fwd_em_route ipv6_l3fwd_em_route_array[16];

void
read_config_files_lpm(void);

void
read_config_files_em(void);

void
read_config_files_acl(void);

void
em_free_routes(void);

void
lpm_free_routes(void);

void
acl_free_routes(void);

void
l3fwd_set_alg(const char *optarg);

void
l3fwd_set_rule_ipv6_name(const char *optarg);

void
l3fwd_set_rule_ipv4_name(const char *optarg);

int
is_bypass_line(const char *buff);
