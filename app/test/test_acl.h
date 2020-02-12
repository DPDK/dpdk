/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef TEST_ACL_H_
#define TEST_ACL_H_

struct ipv4_7tuple {
	uint16_t vlan;
	uint16_t domain;
	uint8_t proto;
	uint32_t ip_src;
	uint32_t ip_dst;
	uint16_t port_src;
	uint16_t port_dst;
	uint32_t allow;
	uint32_t deny;
};

/**
 * Legacy support for 7-tuple IPv4 and VLAN rule.
 * This structure and corresponding API is deprecated.
 */
struct rte_acl_ipv4vlan_rule {
	struct rte_acl_rule_data data; /**< Miscellaneous data for the rule. */
	uint8_t proto;                 /**< IPv4 protocol ID. */
	uint8_t proto_mask;            /**< IPv4 protocol ID mask. */
	uint16_t vlan;                 /**< VLAN ID. */
	uint16_t vlan_mask;            /**< VLAN ID mask. */
	uint16_t domain;               /**< VLAN domain. */
	uint16_t domain_mask;          /**< VLAN domain mask. */
	uint32_t src_addr;             /**< IPv4 source address. */
	uint32_t src_mask_len;         /**< IPv4 source address mask. */
	uint32_t dst_addr;             /**< IPv4 destination address. */
	uint32_t dst_mask_len;         /**< IPv4 destination address mask. */
	uint16_t src_port_low;         /**< L4 source port low. */
	uint16_t src_port_high;        /**< L4 source port high. */
	uint16_t dst_port_low;         /**< L4 destination port low. */
	uint16_t dst_port_high;        /**< L4 destination port high. */
};

/**
 * Specifies fields layout inside rte_acl_rule for rte_acl_ipv4vlan_rule.
 */
enum {
	RTE_ACL_IPV4VLAN_PROTO_FIELD,
	RTE_ACL_IPV4VLAN_VLAN1_FIELD,
	RTE_ACL_IPV4VLAN_VLAN2_FIELD,
	RTE_ACL_IPV4VLAN_SRC_FIELD,
	RTE_ACL_IPV4VLAN_DST_FIELD,
	RTE_ACL_IPV4VLAN_SRCP_FIELD,
	RTE_ACL_IPV4VLAN_DSTP_FIELD,
	RTE_ACL_IPV4VLAN_NUM_FIELDS
};

/**
 * Macro to define rule size for rte_acl_ipv4vlan_rule.
 */
#define	RTE_ACL_IPV4VLAN_RULE_SZ	\
	RTE_ACL_RULE_SZ(RTE_ACL_IPV4VLAN_NUM_FIELDS)

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

/* rules for invalid layout test */
struct rte_acl_ipv4vlan_rule invalid_layout_rules[] = {
		/* test src and dst address */
		{
				.data = {.userdata = 1, .category_mask = 1,
					.priority = 1},
				.src_addr = RTE_IPV4(10,0,0,0),
				.src_mask_len = 24,
		},
		{
				.data = {.userdata = 2, .category_mask = 1,
					.priority = 1},
				.dst_addr = RTE_IPV4(10,0,0,0),
				.dst_mask_len = 24,
		},
		/* test src and dst ports */
		{
				.data = {.userdata = 3, .category_mask = 1,
					.priority = 1},
				.dst_port_low = 100,
				.dst_port_high = 100,
		},
		{
				.data = {.userdata = 4, .category_mask = 1,
					.priority = 1},
				.src_port_low = 100,
				.src_port_high = 100,
		},
		/* test proto */
		{
				.data = {.userdata = 5, .category_mask = 1,
					.priority = 1},
				.proto = 0xf,
				.proto_mask = 0xf
		},
		{
				.data = {.userdata = 6, .category_mask = 1,
					.priority = 1},
				.dst_port_low = 0xf,
				.dst_port_high = 0xf,
		}
};

/* these might look odd because they don't match up the rules. This is
 * intentional, as the invalid layout test presumes returning the correct
 * results using the wrong data layout.
 */
struct ipv4_7tuple invalid_layout_data[] = {
		{.ip_src = RTE_IPV4(10,0,1,0)},             /* should not match */
		{.ip_src = RTE_IPV4(10,0,0,1), .allow = 2}, /* should match 2 */
		{.port_src = 100, .allow = 4},          /* should match 4 */
		{.port_dst = 0xf, .allow = 6},          /* should match 6 */
};

#define ACL_ALLOW 0
#define ACL_DENY 1
#define ACL_ALLOW_MASK 0x1
#define ACL_DENY_MASK  0x2

/* ruleset for ACL unit test */
struct rte_acl_ipv4vlan_rule acl_test_rules[] = {
/* destination IP addresses */
		/* matches all packets traveling to 192.168.0.0/16 */
		{
				.data = {.userdata = 1, .category_mask = ACL_ALLOW_MASK,
						.priority = 230},
				.dst_addr = RTE_IPV4(192,168,0,0),
				.dst_mask_len = 16,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches all packets traveling to 192.168.1.0/24 */
		{
				.data = {.userdata = 2, .category_mask = ACL_ALLOW_MASK,
						.priority = 330},
				.dst_addr = RTE_IPV4(192,168,1,0),
				.dst_mask_len = 24,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches all packets traveling to 192.168.1.50 */
		{
				.data = {.userdata = 3, .category_mask = ACL_DENY_MASK,
						.priority = 230},
				.dst_addr = RTE_IPV4(192,168,1,50),
				.dst_mask_len = 32,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},

/* source IP addresses */
		/* matches all packets traveling from 10.0.0.0/8 */
		{
				.data = {.userdata = 4, .category_mask = ACL_ALLOW_MASK,
						.priority = 240},
				.src_addr = RTE_IPV4(10,0,0,0),
				.src_mask_len = 8,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches all packets traveling from 10.1.1.0/24 */
		{
				.data = {.userdata = 5, .category_mask = ACL_ALLOW_MASK,
						.priority = 340},
				.src_addr = RTE_IPV4(10,1,1,0),
				.src_mask_len = 24,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches all packets traveling from 10.1.1.1 */
		{
				.data = {.userdata = 6, .category_mask = ACL_DENY_MASK,
						.priority = 240},
				.src_addr = RTE_IPV4(10,1,1,1),
				.src_mask_len = 32,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},

/* VLAN tag */
		/* matches all packets with lower 7 bytes of VLAN tag equal to 0x64  */
		{
				.data = {.userdata = 7, .category_mask = ACL_ALLOW_MASK,
						.priority = 260},
				.vlan = 0x64,
				.vlan_mask = 0x7f,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches all packets with VLAN tags that have 0x5 in them */
		{
				.data = {.userdata = 8, .category_mask = ACL_ALLOW_MASK,
						.priority = 260},
				.vlan = 0x5,
				.vlan_mask = 0x5,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches all packets with VLAN tag 5 */
		{
				.data = {.userdata = 9, .category_mask = ACL_DENY_MASK,
						.priority = 360},
				.vlan = 0x5,
				.vlan_mask = 0xffff,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},

/* VLAN domain */
		/* matches all packets with lower 7 bytes of domain equal to 0x64  */
		{
				.data = {.userdata = 10, .category_mask = ACL_ALLOW_MASK,
						.priority = 250},
				.domain = 0x64,
				.domain_mask = 0x7f,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches all packets with domains that have 0x5 in them */
		{
				.data = {.userdata = 11, .category_mask = ACL_ALLOW_MASK,
						.priority = 350},
				.domain = 0x5,
				.domain_mask = 0x5,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches all packets with domain 5 */
		{
				.data = {.userdata = 12, .category_mask = ACL_DENY_MASK,
						.priority = 350},
				.domain = 0x5,
				.domain_mask = 0xffff,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},

/* destination port */
		/* matches everything with dst port 80 */
		{
				.data = {.userdata = 13, .category_mask = ACL_ALLOW_MASK,
						.priority = 310},
				.dst_port_low = 80,
				.dst_port_high = 80,
				.src_port_low = 0,
				.src_port_high = 0xffff,
		},
		/* matches everything with dst port 22-1023 */
		{
				.data = {.userdata = 14, .category_mask = ACL_ALLOW_MASK,
						.priority = 210},
				.dst_port_low = 22,
				.dst_port_high = 1023,
				.src_port_low = 0,
				.src_port_high = 0xffff,
		},
		/* matches everything with dst port 1020 */
		{
				.data = {.userdata = 15, .category_mask = ACL_DENY_MASK,
						.priority = 310},
				.dst_port_low = 1020,
				.dst_port_high = 1020,
				.src_port_low = 0,
				.src_port_high = 0xffff,
		},
		/* matches everything with dst portrange  1000-2000 */
		{
				.data = {.userdata = 16, .category_mask = ACL_DENY_MASK,
						.priority = 210},
				.dst_port_low = 1000,
				.dst_port_high = 2000,
				.src_port_low = 0,
				.src_port_high = 0xffff,
		},

/* source port */
		/* matches everything with src port 80 */
		{
				.data = {.userdata = 17, .category_mask = ACL_ALLOW_MASK,
						.priority = 320},
				.src_port_low = 80,
				.src_port_high = 80,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches everything with src port 22-1023 */
		{
				.data = {.userdata = 18, .category_mask = ACL_ALLOW_MASK,
						.priority = 220},
				.src_port_low = 22,
				.src_port_high = 1023,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches everything with src port 1020 */
		{
				.data = {.userdata = 19, .category_mask = ACL_DENY_MASK,
						.priority = 320},
				.src_port_low = 1020,
				.src_port_high = 1020,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches everything with src portrange  1000-2000 */
		{
				.data = {.userdata = 20, .category_mask = ACL_DENY_MASK,
						.priority = 220},
				.src_port_low = 1000,
				.src_port_high = 2000,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},

/* protocol number */
		/* matches all packets with protocol number either 0x64 or 0xE4 */
		{
				.data = {.userdata = 21, .category_mask = ACL_ALLOW_MASK,
						.priority = 270},
				.proto = 0x64,
				.proto_mask = 0x7f,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches all packets with protocol that have 0x5 in them */
		{
				.data = {.userdata = 22, .category_mask = ACL_ALLOW_MASK,
						.priority = 1},
				.proto = 0x5,
				.proto_mask = 0x5,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches all packets with protocol 5 */
		{
				.data = {.userdata = 23, .category_mask = ACL_DENY_MASK,
						.priority = 370},
				.proto = 0x5,
				.proto_mask = 0xff,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},

/* rules combining various fields */
		{
				.data = {.userdata = 24, .category_mask = ACL_ALLOW_MASK,
						.priority = 400},
				/** make sure that unmasked bytes don't fail! */
				.dst_addr = RTE_IPV4(1,2,3,4),
				.dst_mask_len = 16,
				.src_addr = RTE_IPV4(5,6,7,8),
				.src_mask_len = 24,
				.proto = 0x5,
				.proto_mask = 0xff,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 22,
				.dst_port_high = 1024,
				.vlan = 0x8100,
				.vlan_mask = 0xffff,
				.domain = 0x64,
				.domain_mask = 0xffff,
		},
		{
				.data = {.userdata = 25, .category_mask = ACL_DENY_MASK,
						.priority = 400},
				.dst_addr = RTE_IPV4(5,6,7,8),
				.dst_mask_len = 24,
				.src_addr = RTE_IPV4(1,2,3,4),
				.src_mask_len = 16,
				.proto = 0x5,
				.proto_mask = 0xff,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 22,
				.dst_port_high = 1024,
				.vlan = 0x8100,
				.vlan_mask = 0xffff,
				.domain = 0x64,
				.domain_mask = 0xffff,
		},
		{
				.data = {.userdata = 26, .category_mask = ACL_ALLOW_MASK,
						.priority = 500},
				.dst_addr = RTE_IPV4(1,2,3,4),
				.dst_mask_len = 8,
				.src_addr = RTE_IPV4(5,6,7,8),
				.src_mask_len = 32,
				.proto = 0x5,
				.proto_mask = 0xff,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 22,
				.dst_port_high = 1024,
				.vlan = 0x64,
				.vlan_mask = 0xffff,
		},
		{
				.data = {.userdata = 27, .category_mask = ACL_DENY_MASK,
						.priority = 500},
				.dst_addr = RTE_IPV4(5,6,7,8),
				.dst_mask_len = 32,
				.src_addr = RTE_IPV4(1,2,3,4),
				.src_mask_len = 8,
				.proto = 0x5,
				.proto_mask = 0xff,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 22,
				.dst_port_high = 1024,
				.vlan = 0x64,
				.vlan_mask = 0xffff,
		},
};

/* data for ACL unit test */
struct ipv4_7tuple acl_test_data[] = {
/* testing single rule aspects */
		{.ip_src = RTE_IPV4(10,0,0,0), .allow = 4}, /* should match 4 */
		{.ip_src = RTE_IPV4(10,1,1,2), .allow = 5}, /* should match 5 */
		{.ip_src = RTE_IPV4(10,1,1,1), .allow = 5,
				.deny = 6},                     /* should match 5, 6 */
		{.ip_dst = RTE_IPV4(10,0,0,0)},             /* should not match */
		{.ip_dst = RTE_IPV4(10,1,1,2)},             /* should not match */
		{.ip_dst = RTE_IPV4(10,1,1,1)},             /* should not match */

		{.ip_src = RTE_IPV4(192,168,2,50)},             /* should not match */
		{.ip_src = RTE_IPV4(192,168,1,2)},              /* should not match */
		{.ip_src = RTE_IPV4(192,168,1,50)},             /* should not match */
		{.ip_dst = RTE_IPV4(192,168,2,50), .allow = 1}, /* should match 1 */
		{.ip_dst = RTE_IPV4(192,168,1,49), .allow = 2}, /* should match 2 */
		{.ip_dst = RTE_IPV4(192,168,1,50), .allow = 2,
				.deny = 3},                         /* should match 2, 3 */

		{.vlan = 0x64, .allow = 7},            /* should match 7 */
		{.vlan = 0xfE4, .allow = 7},           /* should match 7 */
		{.vlan = 0xE2},                        /* should not match */
		{.vlan = 0xD, .allow = 8},             /* should match 8 */
		{.vlan = 0x6},                         /* should not match */
		{.vlan = 0x5, .allow = 8, .deny = 9},  /* should match 8, 9 */

		{.domain = 0x64, .allow = 10},             /* should match 10 */
		{.domain = 0xfE4, .allow = 10},            /* should match 10 */
		{.domain = 0xE2},                          /* should not match */
		{.domain = 0xD, .allow = 11},              /* should match 11 */
		{.domain = 0x6},                           /* should not match */
		{.domain = 0x5, .allow = 11, .deny = 12},  /* should match 11, 12 */

		{.port_dst = 80, .allow = 13},                /* should match 13 */
		{.port_dst = 79, .allow = 14},                /* should match 14 */
		{.port_dst = 81, .allow = 14},                /* should match 14 */
		{.port_dst = 21},                             /* should not match */
		{.port_dst = 1024, .deny = 16},               /* should match 16 */
		{.port_dst = 1020, .allow = 14, .deny = 15},  /* should match 14, 15 */

		{.port_src = 80, .allow = 17},                /* should match 17 */
		{.port_src = 79, .allow = 18},                /* should match 18 */
		{.port_src = 81, .allow = 18},                /* should match 18 */
		{.port_src = 21},                             /* should not match */
		{.port_src = 1024, .deny = 20},               /* should match 20 */
		{.port_src = 1020, .allow = 18, .deny = 19},  /* should match 18, 19 */

		{.proto = 0x64, .allow = 21},             /* should match 21 */
		{.proto = 0xE4, .allow = 21},             /* should match 21 */
		{.proto = 0xE2},                          /* should not match */
		{.proto = 0xD, .allow = 22},              /* should match 22 */
		{.proto = 0x6},                           /* should not match */
		{.proto = 0x5, .allow = 22, .deny = 23},  /* should match 22, 23 */

/* testing matching multiple rules at once */
		{.vlan = 0x5, .ip_src = RTE_IPV4(10,1,1,1),
				.allow = 5, .deny = 9},               /* should match 5, 9 */
		{.vlan = 0x5, .ip_src = RTE_IPV4(192,168,2,50),
				.allow = 8, .deny = 9},               /* should match 8, 9 */
		{.vlan = 0x55, .ip_src = RTE_IPV4(192,168,1,49),
				.allow = 8},                          /* should match 8 */
		{.port_dst = 80, .port_src = 1024,
				.allow = 13, .deny = 20},             /* should match 13,20 */
		{.port_dst = 79, .port_src = 1024,
				.allow = 14, .deny = 20},             /* should match 14,20 */
		{.proto = 0x5, .ip_dst = RTE_IPV4(192,168,2,50),
				.allow = 1, .deny = 23},               /* should match 1, 23 */

		{.proto = 0x5, .ip_dst = RTE_IPV4(192,168,1,50),
				.allow = 2, .deny = 23},              /* should match 2, 23 */
		{.vlan = 0x64, .domain = 0x5,
				.allow = 11, .deny = 12},             /* should match 11, 12 */
		{.proto = 0x5, .port_src = 80,
				.allow = 17, .deny = 23},             /* should match 17, 23 */
		{.proto = 0x5, .port_dst = 80,
				.allow = 13, .deny = 23},             /* should match 13, 23 */
		{.proto = 0x51, .port_src = 5000},            /* should not match */
		{.ip_src = RTE_IPV4(192,168,1,50),
				.ip_dst = RTE_IPV4(10,0,0,0),
				.proto = 0x51,
				.port_src = 5000,
				.port_dst = 5000},                    /* should not match */

/* test full packet rules */
		{
				.ip_dst = RTE_IPV4(1,2,100,200),
				.ip_src = RTE_IPV4(5,6,7,254),
				.proto = 0x5,
				.vlan = 0x8100,
				.domain = 0x64,
				.port_src = 12345,
				.port_dst = 80,
				.allow = 24,
				.deny = 23
		}, /* should match 23, 24 */
		{
				.ip_dst = RTE_IPV4(5,6,7,254),
				.ip_src = RTE_IPV4(1,2,100,200),
				.proto = 0x5,
				.vlan = 0x8100,
				.domain = 0x64,
				.port_src = 12345,
				.port_dst = 80,
				.allow = 13,
				.deny = 25
		}, /* should match 13, 25 */
		{
				.ip_dst = RTE_IPV4(1,10,20,30),
				.ip_src = RTE_IPV4(5,6,7,8),
				.proto = 0x5,
				.vlan = 0x64,
				.port_src = 12345,
				.port_dst = 80,
				.allow = 26,
				.deny = 23
		}, /* should match 23, 26 */
		{
				.ip_dst = RTE_IPV4(5,6,7,8),
				.ip_src = RTE_IPV4(1,10,20,30),
				.proto = 0x5,
				.vlan = 0x64,
				.port_src = 12345,
				.port_dst = 80,
				.allow = 13,
				.deny = 27
		}, /* should match 13, 27 */
		{
				.ip_dst = RTE_IPV4(2,2,3,4),
				.ip_src = RTE_IPV4(4,6,7,8),
				.proto = 0x5,
				.vlan = 0x64,
				.port_src = 12345,
				.port_dst = 80,
				.allow = 13,
				.deny = 23
		}, /* should match 13, 23 */
		{
				.ip_dst = RTE_IPV4(1,2,3,4),
				.ip_src = RTE_IPV4(4,6,7,8),
				.proto = 0x5,
				.vlan = 0x64,
				.port_src = 12345,
				.port_dst = 80,
				.allow = 13,
				.deny = 23
		}, /* should match 13, 23 */


/* visual separator! */
		{
				.ip_dst = RTE_IPV4(1,2,100,200),
				.ip_src = RTE_IPV4(5,6,7,254),
				.proto = 0x55,
				.vlan = 0x8000,
				.domain = 0x6464,
				.port_src = 12345,
				.port_dst = 8080,
				.allow = 10
		}, /* should match 10 */
		{
				.ip_dst = RTE_IPV4(5,6,7,254),
				.ip_src = RTE_IPV4(1,2,100,200),
				.proto = 0x55,
				.vlan = 0x8100,
				.domain = 0x6464,
				.port_src = 12345,
				.port_dst = 180,
				.allow = 10
		}, /* should match 10 */
		{
				.ip_dst = RTE_IPV4(1,10,20,30),
				.ip_src = RTE_IPV4(5,6,7,8),
				.proto = 0x55,
				.vlan = 0x64,
				.port_src = 12345,
				.port_dst = 180,
				.allow = 7
		}, /* should match 7 */
		{
				.ip_dst = RTE_IPV4(5,6,7,8),
				.ip_src = RTE_IPV4(1,10,20,30),
				.proto = 0x55,
				.vlan = 0x64,
				.port_src = 12345,
				.port_dst = 180,
				.allow = 7
		}, /* should match 7 */
		{
				.ip_dst = RTE_IPV4(2,2,3,4),
				.ip_src = RTE_IPV4(4,6,7,8),
				.proto = 0x55,
				.vlan = 0x64,
				.port_src = 12345,
				.port_dst = 180,
				.allow = 7
		}, /* should match 7 */
		{
				.ip_dst = RTE_IPV4(1,2,3,4),
				.ip_src = RTE_IPV4(4,6,7,8),
				.proto = 0x50,
				.vlan = 0x6466,
				.port_src = 12345,
				.port_dst = 12345,
		}, /* should not match */
};

/*
 * ruleset for ACL 32 bit range (by src addr) unit test
 * keep them ordered by priority in descending order.
 */
struct rte_acl_ipv4vlan_rule acl_u32_range_test_rules[] = {
		{
			.data = {
				.userdata = 500,
				.category_mask = ACL_ALLOW_MASK,
				.priority = 500
			},
			.src_addr = RTE_IPV4(0, 0, 0, 1),
			.src_mask_len = RTE_IPV4(0, 0, 2, 58),
		},
		{
			.data = {
				.userdata = 400,
				.category_mask = ACL_ALLOW_MASK,
				.priority = 400
			},
			.src_addr = RTE_IPV4(0, 4, 3, 2),
			.src_mask_len = RTE_IPV4(0, 4, 7, 255),
		},
		{
			.data = {
				.userdata = 300,
				.category_mask = ACL_ALLOW_MASK,
				.priority = 300
			},
			.src_addr = RTE_IPV4(0, 1, 12, 14),
			.src_mask_len = RTE_IPV4(0, 3, 11, 13),
		},
		{
			.data = {
				.userdata = 200,
				.category_mask = ACL_ALLOW_MASK,
				.priority = 200
			},
			.src_addr = RTE_IPV4(0, 0, 1, 40),
			.src_mask_len = RTE_IPV4(0, 4, 5, 6),
		},
};

#endif /* TEST_ACL_H_ */
