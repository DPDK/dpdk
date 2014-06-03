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

/* rules for invalid layout test */
struct rte_acl_ipv4vlan_rule invalid_layout_rules[] = {
		/* test src and dst address */
		{
				.data = {.userdata = 1, .category_mask = 1},
				.src_addr = IPv4(10,0,0,0),
				.src_mask_len = 24,
		},
		{
				.data = {.userdata = 2, .category_mask = 1},
				.dst_addr = IPv4(10,0,0,0),
				.dst_mask_len = 24,
		},
		/* test src and dst ports */
		{
				.data = {.userdata = 3, .category_mask = 1},
				.dst_port_low = 100,
				.dst_port_high = 100,
		},
		{
				.data = {.userdata = 4, .category_mask = 1},
				.src_port_low = 100,
				.src_port_high = 100,
		},
		/* test proto */
		{
				.data = {.userdata = 5, .category_mask = 1},
				.proto = 0xf,
				.proto_mask = 0xf
		},
		{
				.data = {.userdata = 6, .category_mask = 1},
				.dst_port_low = 0xf,
				.dst_port_high = 0xf,
		}
};

/* these might look odd because they don't match up the rules. This is
 * intentional, as the invalid layout test presumes returning the correct
 * results using the wrong data layout.
 */
struct ipv4_7tuple invalid_layout_data[] = {
		{.ip_src = IPv4(10,0,1,0)},             /* should not match */
		{.ip_src = IPv4(10,0,0,1), .allow = 2}, /* should match 2 */
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
						.priority = 2},
				.dst_addr = IPv4(192,168,0,0),
				.dst_mask_len = 16,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches all packets traveling to 192.168.1.0/24 */
		{
				.data = {.userdata = 2, .category_mask = ACL_ALLOW_MASK,
						.priority = 3},
				.dst_addr = IPv4(192,168,1,0),
				.dst_mask_len = 24,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches all packets traveling to 192.168.1.50 */
		{
				.data = {.userdata = 3, .category_mask = ACL_DENY_MASK,
						.priority = 2},
				.dst_addr = IPv4(192,168,1,50),
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
						.priority = 2},
				.src_addr = IPv4(10,0,0,0),
				.src_mask_len = 8,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches all packets traveling from 10.1.1.0/24 */
		{
				.data = {.userdata = 5, .category_mask = ACL_ALLOW_MASK,
						.priority = 3},
				.src_addr = IPv4(10,1,1,0),
				.src_mask_len = 24,
				.src_port_low = 0,
				.src_port_high = 0xffff,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches all packets traveling from 10.1.1.1 */
		{
				.data = {.userdata = 6, .category_mask = ACL_DENY_MASK,
						.priority = 2},
				.src_addr = IPv4(10,1,1,1),
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
						.priority = 2},
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
						.priority = 2},
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
						.priority = 3},
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
						.priority = 2},
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
						.priority = 3},
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
						.priority = 3},
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
						.priority = 3},
				.dst_port_low = 80,
				.dst_port_high = 80,
				.src_port_low = 0,
				.src_port_high = 0xffff,
		},
		/* matches everything with dst port 22-1023 */
		{
				.data = {.userdata = 14, .category_mask = ACL_ALLOW_MASK,
						.priority = 2},
				.dst_port_low = 22,
				.dst_port_high = 1023,
				.src_port_low = 0,
				.src_port_high = 0xffff,
		},
		/* matches everything with dst port 1020 */
		{
				.data = {.userdata = 15, .category_mask = ACL_DENY_MASK,
						.priority = 3},
				.dst_port_low = 1020,
				.dst_port_high = 1020,
				.src_port_low = 0,
				.src_port_high = 0xffff,
		},
		/* matches everything with dst portrange  1000-2000 */
		{
				.data = {.userdata = 16, .category_mask = ACL_DENY_MASK,
						.priority = 2},
				.dst_port_low = 1000,
				.dst_port_high = 2000,
				.src_port_low = 0,
				.src_port_high = 0xffff,
		},

/* source port */
		/* matches everything with src port 80 */
		{
				.data = {.userdata = 17, .category_mask = ACL_ALLOW_MASK,
						.priority = 3},
				.src_port_low = 80,
				.src_port_high = 80,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches everything with src port 22-1023 */
		{
				.data = {.userdata = 18, .category_mask = ACL_ALLOW_MASK,
						.priority = 2},
				.src_port_low = 22,
				.src_port_high = 1023,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches everything with src port 1020 */
		{
				.data = {.userdata = 19, .category_mask = ACL_DENY_MASK,
						.priority = 3},
				.src_port_low = 1020,
				.src_port_high = 1020,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},
		/* matches everything with src portrange  1000-2000 */
		{
				.data = {.userdata = 20, .category_mask = ACL_DENY_MASK,
						.priority = 2},
				.src_port_low = 1000,
				.src_port_high = 2000,
				.dst_port_low = 0,
				.dst_port_high = 0xffff,
		},

/* protocol number */
		/* matches all packets with protocol number either 0x64 or 0xE4 */
		{
				.data = {.userdata = 21, .category_mask = ACL_ALLOW_MASK,
						.priority = 2},
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
						.priority = 3},
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
						.priority = 4},
				/** make sure that unmasked bytes don't fail! */
				.dst_addr = IPv4(1,2,3,4),
				.dst_mask_len = 16,
				.src_addr = IPv4(5,6,7,8),
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
						.priority = 4},
				.dst_addr = IPv4(5,6,7,8),
				.dst_mask_len = 24,
				.src_addr = IPv4(1,2,3,4),
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
						.priority = 5},
				.dst_addr = IPv4(1,2,3,4),
				.dst_mask_len = 8,
				.src_addr = IPv4(5,6,7,8),
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
						.priority = 5},
				.dst_addr = IPv4(5,6,7,8),
				.dst_mask_len = 32,
				.src_addr = IPv4(1,2,3,4),
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
		{.ip_src = IPv4(10,0,0,0), .allow = 4}, /* should match 4 */
		{.ip_src = IPv4(10,1,1,2), .allow = 5}, /* should match 5 */
		{.ip_src = IPv4(10,1,1,1), .allow = 5,
				.deny = 6},                     /* should match 5, 6 */
		{.ip_dst = IPv4(10,0,0,0)},             /* should not match */
		{.ip_dst = IPv4(10,1,1,2)},             /* should not match */
		{.ip_dst = IPv4(10,1,1,1)},             /* should not match */

		{.ip_src = IPv4(192,168,2,50)},             /* should not match */
		{.ip_src = IPv4(192,168,1,2)},              /* should not match */
		{.ip_src = IPv4(192,168,1,50)},             /* should not match */
		{.ip_dst = IPv4(192,168,2,50), .allow = 1}, /* should match 1 */
		{.ip_dst = IPv4(192,168,1,49), .allow = 2}, /* should match 2 */
		{.ip_dst = IPv4(192,168,1,50), .allow = 2,
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
		{.vlan = 0x5, .ip_src = IPv4(10,1,1,1),
				.allow = 5, .deny = 9},               /* should match 5, 9 */
		{.vlan = 0x5, .ip_src = IPv4(192,168,2,50),
				.allow = 8, .deny = 9},               /* should match 8, 9 */
		{.vlan = 0x55, .ip_src = IPv4(192,168,1,49),
				.allow = 8},                          /* should match 8 */
		{.port_dst = 80, .port_src = 1024,
				.allow = 13, .deny = 20},             /* should match 13,20 */
		{.port_dst = 79, .port_src = 1024,
				.allow = 14, .deny = 20},             /* should match 14,20 */
		{.proto = 0x5, .ip_dst = IPv4(192,168,2,50),
				.allow = 1, .deny = 23},               /* should match 1, 23 */

		{.proto = 0x5, .ip_dst = IPv4(192,168,1,50),
				.allow = 2, .deny = 23},              /* should match 2, 23 */
		{.vlan = 0x64, .domain = 0x5,
				.allow = 11, .deny = 12},             /* should match 11, 12 */
		{.proto = 0x5, .port_src = 80,
				.allow = 17, .deny = 23},             /* should match 17, 23 */
		{.proto = 0x5, .port_dst = 80,
				.allow = 13, .deny = 23},             /* should match 13, 23 */
		{.proto = 0x51, .port_src = 5000},            /* should not match */
		{.ip_src = IPv4(192,168,1,50),
				.ip_dst = IPv4(10,0,0,0),
				.proto = 0x51,
				.port_src = 5000,
				.port_dst = 5000},                    /* should not match */

/* test full packet rules */
		{
				.ip_dst = IPv4(1,2,100,200),
				.ip_src = IPv4(5,6,7,254),
				.proto = 0x5,
				.vlan = 0x8100,
				.domain = 0x64,
				.port_src = 12345,
				.port_dst = 80,
				.allow = 24,
				.deny = 23
		}, /* should match 23, 24 */
		{
				.ip_dst = IPv4(5,6,7,254),
				.ip_src = IPv4(1,2,100,200),
				.proto = 0x5,
				.vlan = 0x8100,
				.domain = 0x64,
				.port_src = 12345,
				.port_dst = 80,
				.allow = 13,
				.deny = 25
		}, /* should match 13, 25 */
		{
				.ip_dst = IPv4(1,10,20,30),
				.ip_src = IPv4(5,6,7,8),
				.proto = 0x5,
				.vlan = 0x64,
				.port_src = 12345,
				.port_dst = 80,
				.allow = 26,
				.deny = 23
		}, /* should match 23, 26 */
		{
				.ip_dst = IPv4(5,6,7,8),
				.ip_src = IPv4(1,10,20,30),
				.proto = 0x5,
				.vlan = 0x64,
				.port_src = 12345,
				.port_dst = 80,
				.allow = 13,
				.deny = 27
		}, /* should match 13, 27 */
		{
				.ip_dst = IPv4(2,2,3,4),
				.ip_src = IPv4(4,6,7,8),
				.proto = 0x5,
				.vlan = 0x64,
				.port_src = 12345,
				.port_dst = 80,
				.allow = 13,
				.deny = 23
		}, /* should match 13, 23 */
		{
				.ip_dst = IPv4(1,2,3,4),
				.ip_src = IPv4(4,6,7,8),
				.proto = 0x5,
				.vlan = 0x64,
				.port_src = 12345,
				.port_dst = 80,
				.allow = 13,
				.deny = 23
		}, /* should match 13, 23 */


/* visual separator! */
		{
				.ip_dst = IPv4(1,2,100,200),
				.ip_src = IPv4(5,6,7,254),
				.proto = 0x55,
				.vlan = 0x8000,
				.domain = 0x6464,
				.port_src = 12345,
				.port_dst = 8080,
				.allow = 10
		}, /* should match 10 */
		{
				.ip_dst = IPv4(5,6,7,254),
				.ip_src = IPv4(1,2,100,200),
				.proto = 0x55,
				.vlan = 0x8100,
				.domain = 0x6464,
				.port_src = 12345,
				.port_dst = 180,
				.allow = 10
		}, /* should match 10 */
		{
				.ip_dst = IPv4(1,10,20,30),
				.ip_src = IPv4(5,6,7,8),
				.proto = 0x55,
				.vlan = 0x64,
				.port_src = 12345,
				.port_dst = 180,
				.allow = 7
		}, /* should match 7 */
		{
				.ip_dst = IPv4(5,6,7,8),
				.ip_src = IPv4(1,10,20,30),
				.proto = 0x55,
				.vlan = 0x64,
				.port_src = 12345,
				.port_dst = 180,
				.allow = 7
		}, /* should match 7 */
		{
				.ip_dst = IPv4(2,2,3,4),
				.ip_src = IPv4(4,6,7,8),
				.proto = 0x55,
				.vlan = 0x64,
				.port_src = 12345,
				.port_dst = 180,
				.allow = 7
		}, /* should match 7 */
		{
				.ip_dst = IPv4(1,2,3,4),
				.ip_src = IPv4(4,6,7,8),
				.proto = 0x50,
				.vlan = 0x6466,
				.port_src = 12345,
				.port_dst = 12345,
		}, /* should not match */
};

#endif /* TEST_ACL_H_ */
