/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <rte_string_fns.h>

#include <cmdline_parse.h>
#include <cmdline_parse_ipaddr.h>

#include "test_cmdline.h"

#define IP4(a,b,c,d) {((uint32_t)(((a) & 0xff)) | \
					   (((b) & 0xff) << 8) | \
					   (((c) & 0xff) << 16)  | \
					   ((d) & 0xff)  << 24)}

#define U16_SWAP(x) \
		(((x & 0xFF) << 8) | ((x & 0xFF00) >> 8))

/* create IPv6 address, swapping bytes where needed */
#ifndef s6_addr16
# define s6_addr16      __u6_addr.__u6_addr16
#endif
#define IP6(a,b,c,d,e,f,g,h) .ipv6 = \
		{.s6_addr16 = \
		{U16_SWAP(a),U16_SWAP(b),U16_SWAP(c),U16_SWAP(d),\
		 U16_SWAP(e),U16_SWAP(f),U16_SWAP(g),U16_SWAP(h)}}

/** these are defined in netinet/in.h but not present in linux headers */
#ifndef NIPQUAD

#define NIPQUAD_FMT "%u.%u.%u.%u"
#define NIPQUAD(addr)				\
	(unsigned)((unsigned char *)&addr)[0],	\
	(unsigned)((unsigned char *)&addr)[1],	\
	(unsigned)((unsigned char *)&addr)[2],	\
	(unsigned)((unsigned char *)&addr)[3]

#define NIP6_FMT "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x"
#define NIP6(addr)					\
	(unsigned)((addr).s6_addr[0]),			\
	(unsigned)((addr).s6_addr[1]),			\
	(unsigned)((addr).s6_addr[2]),			\
	(unsigned)((addr).s6_addr[3]),			\
	(unsigned)((addr).s6_addr[4]),			\
	(unsigned)((addr).s6_addr[5]),			\
	(unsigned)((addr).s6_addr[6]),			\
	(unsigned)((addr).s6_addr[7]),			\
	(unsigned)((addr).s6_addr[8]),			\
	(unsigned)((addr).s6_addr[9]),			\
	(unsigned)((addr).s6_addr[10]),			\
	(unsigned)((addr).s6_addr[11]),			\
	(unsigned)((addr).s6_addr[12]),			\
	(unsigned)((addr).s6_addr[13]),			\
	(unsigned)((addr).s6_addr[14]),			\
	(unsigned)((addr).s6_addr[15])

#endif



struct ipaddr_str {
	const char * str;
	cmdline_ipaddr_t addr;
	unsigned flags;
};

const struct ipaddr_str ipaddr_valid_strs[] = {
		{"0.0.0.0", {AF_INET, {IP4(0,0,0,0)}, 0},
				CMDLINE_IPADDR_V4},
		{"0.0.0.0/0", {AF_INET, {IP4(0,0,0,0)}, 0},
				CMDLINE_IPADDR_V4 | CMDLINE_IPADDR_NETWORK},
		{"0.0.0.0/24", {AF_INET, {IP4(0,0,0,0)}, 24},
				CMDLINE_IPADDR_V4 | CMDLINE_IPADDR_NETWORK},
		{"192.168.1.0/24", {AF_INET, {IP4(192,168,1,0)}, 24},
				CMDLINE_IPADDR_V4 | CMDLINE_IPADDR_NETWORK},
		{"34.56.78.90/1", {AF_INET, {IP4(34,56,78,90)}, 1},
				CMDLINE_IPADDR_V4 | CMDLINE_IPADDR_NETWORK},
		{"::", {AF_INET6, {IP6(0,0,0,0,0,0,0,0)}, 0},
					CMDLINE_IPADDR_V6},
		{"::1", {AF_INET6, {IP6(0,0,0,0,0,0,0,1)}, 0},
				CMDLINE_IPADDR_V6},
		{"::1/32", {AF_INET6, {IP6(0,0,0,0,0,0,0,1)}, 32},
				CMDLINE_IPADDR_V6 | CMDLINE_IPADDR_NETWORK},
		{"::/32", {AF_INET6, {IP6(0,0,0,0,0,0,0,0)}, 32},
					CMDLINE_IPADDR_V6 | CMDLINE_IPADDR_NETWORK},
		/* RFC5952 requests that only lowercase should be used */
		{"1234:5678:90ab:cdef:4321:8765:BA09:FEDC", {AF_INET6,
				{IP6(0x1234,0x5678,0x90AB,0xCDEF,0x4321,0x8765,0xBA09,0xFEDC)},
				0},
				CMDLINE_IPADDR_V6},
		{"1234::1234/64", {AF_INET6,
				{IP6(0x1234,0,0,0,0,0,0,0x1234)},
				64},
				CMDLINE_IPADDR_V6 | CMDLINE_IPADDR_NETWORK},
		{"1234::/64", {AF_INET6,
				{IP6(0x1234,0,0,0,0,0,0,0)},
				64},
				CMDLINE_IPADDR_V6 | CMDLINE_IPADDR_NETWORK},
		{"1:1::1/32", {AF_INET6,
				{IP6(1,1,0,0,0,0,0,1)},
				32},
				CMDLINE_IPADDR_V6 | CMDLINE_IPADDR_NETWORK},
		{"1:2:3:4::/64", {AF_INET6,
				{IP6(1,2,3,4,0,0,0,0)},
				64},
			CMDLINE_IPADDR_V6 | CMDLINE_IPADDR_NETWORK},
		{"::ffff:192.168.1.0/64", {AF_INET6,
				{IP6(0,0,0,0,0,0xFFFF,0xC0A8,0x100)},
				64},
			CMDLINE_IPADDR_V6 | CMDLINE_IPADDR_NETWORK},
		/* RFC5952 requests not using :: to skip one block of zeros*/
		{"1::2:3:4:5:6:7", {AF_INET6,
				{IP6(1,0,2,3,4,5,6,7)},
				0},
			CMDLINE_IPADDR_V6},
};

const char * ipaddr_garbage_addr4_strs[] = {
		/* IPv4 */
		"192.168.1.0 garbage",
		"192.168.1.0\0garbage",
		"192.168.1.0#garbage",
		"192.168.1.0\tgarbage",
		"192.168.1.0\rgarbage",
		"192.168.1.0\ngarbage",
};
#define IPv4_GARBAGE_ADDR IP4(192,168,1,0)

const char * ipaddr_garbage_addr6_strs[] = {
		/* IPv6 */
		"1:2:3:4::8 garbage",
		"1:2:3:4::8#garbage",
		"1:2:3:4::8\0garbage",
		"1:2:3:4::8\rgarbage",
		"1:2:3:4::8\ngarbage",
		"1:2:3:4::8\tgarbage",
};
#define IPv6_GARBAGE_ADDR {IP6(1,2,3,4,0,0,0,8)}

const char * ipaddr_garbage_network4_strs[] = {
		/* IPv4 */
		"192.168.1.0/24 garbage",
		"192.168.1.0/24\0garbage",
		"192.168.1.0/24#garbage",
		"192.168.1.0/24\tgarbage",
		"192.168.1.0/24\rgarbage",
		"192.168.1.0/24\ngarbage",
};
#define IPv4_GARBAGE_PREFIX 24

const char * ipaddr_garbage_network6_strs[] = {
		/* IPv6 */
		"1:2:3:4::8/64 garbage",
		"1:2:3:4::8/64#garbage",
		"1:2:3:4::8/64\0garbage",
		"1:2:3:4::8/64\rgarbage",
		"1:2:3:4::8/64\ngarbage",
		"1:2:3:4::8/64\tgarbage",
};
#define IPv6_GARBAGE_PREFIX 64



const char * ipaddr_invalid_strs[] = {
		/** IPv4 **/

		/* invalid numbers */
		"0.0.0.-1",
		"0.0.-1.0",
		"0.-1.0.0",
		"-1.0.0.0",
		"0.0.0.-1/24",
		"256.123.123.123",
		"255.256.123.123",
		"255.255.256.123",
		"255.255.255.256",
		"256.123.123.123/24",
		"255.256.123.123/24",
		"255.255.256.123/24",
		"255.255.255.256/24",
		/* invalid network mask */
		"1.2.3.4/33",
		"1.2.3.4/33231313",
		"1.2.3.4/-1",
		"1.2.3.4/24/33",
		"1.2.3.4/24/-1",
		"1.2.3.4/24/",
		/* wrong format */
		"1/24"
		"/24"
		"123.123.123",
		"123.123.123.",
		"123.123.123.123.",
		"123.123.123..123",
		"123.123.123.123.123",
		".123.123.123",
		".123.123.123.123",
		"123.123.123/24",
		"123.123.123./24",
		"123.123.123.123./24",
		"123.123.123..123/24",
		"123.123.123.123.123/24",
		".123.123.123/24",
		".123.123.123.123/24",
		/* invalid characters */
		"123.123.123.12F",
		"123.123.12F.123",
		"123.12F.123.123",
		"12F.123.123.123",
		"12J.123.123.123",
		"123,123,123,123",
		"123!123!123!12F",
		"123.123.123.123/4F",

		/** IPv6 **/

		/* wrong format */
		"::fffff",
		"ffff:",
		"1:2:3:4:5:6:7:192.168.1.1",
		"1234:192.168.1.1:ffff::",
		"1:2:3:4:5:6:7:890ab",
		"1:2:3:4:5:6:7890a:b",
		"1:2:3:4:5:67890:a:b",
		"1:2:3:4:56789:0:a:b",
		"1:2:3:45678:9:0:a:b",
		"1:2:34567:8:9:0:a:b",
		"1:23456:7:8:9:0:a:b",
		"12345:6:7:8:9:0:a:b",
		"1:::2",
		"1::::2",
		"::fffff/64",
		"1::2::3",
		"1::2::3/64",
		":1:2",
		":1:2/64",
		":1::2",
		":1::2/64",
		"1::2:3:4:5:6:7:8/64",

		/* invalid network mask */
		"1:2:3:4:5:6:7:8/129",
		"1:2:3:4:5:6:7:8/-1",

		/* invalid characters */
		"a:b:c:d:e:f:g::",

		/** misc **/

		/* too long */
		"1234:1234:1234:1234:1234:1234:1234:1234:1234:1234:1234",
		"random invalid text",
		"",
		"\0",
		" ",
};

static void
dump_addr(cmdline_ipaddr_t addr)
{
	switch (addr.family) {
	case AF_INET:
	{
		printf(NIPQUAD_FMT " prefixlen=%u\n",
				NIPQUAD(addr.addr.ipv4.s_addr), addr.prefixlen);
		break;
	}
	case AF_INET6:
	{
		printf(NIP6_FMT " prefixlen=%u\n",
				NIP6(addr.addr.ipv6), addr.prefixlen);
		break;
	}
	default:
		printf("Can't dump: unknown address family.\n");
		return;
	}
}


static int
is_addr_different(cmdline_ipaddr_t addr1, cmdline_ipaddr_t addr2)
{
	if (addr1.family != addr2.family)
		return 1;

	if (addr1.prefixlen != addr2.prefixlen)
		return 1;

	switch (addr1.family) {
	/* IPv4 */
	case AF_INET:
		if (memcmp(&addr1.addr.ipv4, &addr2.addr.ipv4,
				sizeof(struct in_addr)) != 0)
			return 1;
		break;
	/* IPv6 */
	case AF_INET6:
	{
		if (memcmp(&addr1.addr.ipv6, &addr2.addr.ipv6,
				sizeof(struct in6_addr)) != 0)
			return 1;
		break;
	}
	/* thing that should not be */
	default:
		return -1;
	}
	return 0;
}

static int
can_parse_addr(unsigned addr_flags, unsigned test_flags)
{
	if ((test_flags & addr_flags) == addr_flags) {
		/* if we are not trying to parse network addresses */
		if (test_flags < CMDLINE_IPADDR_NETWORK)
			return 1;
		/* if this is a network address */
		else if (addr_flags & CMDLINE_IPADDR_NETWORK)
			return 1;
	}
	return 0;
}

int
test_parse_ipaddr_valid(void)
{
	cmdline_parse_token_ipaddr_t token;
	char buf[CMDLINE_TEST_BUFSIZE];
	cmdline_ipaddr_t result;
	unsigned i;
	uint8_t flags;
	int ret;

	/* cover all cases in help */
	for (flags = 0x1; flags < 0x8; flags++) {
		token.ipaddr_data.flags = flags;

		memset(buf, 0, sizeof(buf));

		if (cmdline_get_help_ipaddr((cmdline_parse_token_hdr_t*)&token,
				buf, sizeof(buf)) == -1) {
			printf("Error: help rejected valid parameters!\n");
			return -1;
		}
	}

	/* test valid strings */
	for (i = 0; i < RTE_DIM(ipaddr_valid_strs); i++) {

		/* test each valid string against different flags */
		for (flags = 1; flags < 0x8; flags++) {

			/* skip bad flag */
			if (flags == CMDLINE_IPADDR_NETWORK)
				continue;

			/* clear out everything */
			memset(buf, 0, sizeof(buf));
			memset(&result, 0, sizeof(result));
			memset(&token, 0, sizeof(token));

			token.ipaddr_data.flags = flags;

			cmdline_get_help_ipaddr((cmdline_parse_token_hdr_t*)&token,
							buf, sizeof(buf));

			ret = cmdline_parse_ipaddr((cmdline_parse_token_hdr_t*)&token,
				ipaddr_valid_strs[i].str, (void*)&result,
				sizeof(result));

			/* if should have passed, or should have failed */
			if ((ret < 0) ==
					(can_parse_addr(ipaddr_valid_strs[i].flags, flags))) {
				printf("Error: unexpected behavior when parsing %s as %s!\n",
						ipaddr_valid_strs[i].str, buf);
				printf("Parsed result: ");
				dump_addr(result);
				printf("Expected result: ");
				dump_addr(ipaddr_valid_strs[i].addr);
				return -1;
			}
			if (ret != -1 &&
					is_addr_different(result, ipaddr_valid_strs[i].addr)) {
				printf("Error: result mismatch when parsing %s as %s!\n",
						ipaddr_valid_strs[i].str, buf);
				printf("Parsed result: ");
				dump_addr(result);
				printf("Expected result: ");
				dump_addr(ipaddr_valid_strs[i].addr);
				return -1;
			}
		}
	}

	/* test garbage ipv4 address strings */
	for (i = 0; i < RTE_DIM(ipaddr_garbage_addr4_strs); i++) {

		struct in_addr tmp = IPv4_GARBAGE_ADDR;

		/* test each valid string against different flags */
		for (flags = 1; flags < 0x8; flags++) {

			/* skip bad flag */
			if (flags == CMDLINE_IPADDR_NETWORK)
				continue;

			/* clear out everything */
			memset(buf, 0, sizeof(buf));
			memset(&result, 0, sizeof(result));
			memset(&token, 0, sizeof(token));

			token.ipaddr_data.flags = flags;

			cmdline_get_help_ipaddr((cmdline_parse_token_hdr_t*)&token,
							buf, sizeof(buf));

			ret = cmdline_parse_ipaddr((cmdline_parse_token_hdr_t*)&token,
				ipaddr_garbage_addr4_strs[i], (void*)&result,
				sizeof(result));

			/* if should have passed, or should have failed */
			if ((ret < 0) ==
					(can_parse_addr(CMDLINE_IPADDR_V4, flags))) {
				printf("Error: unexpected behavior when parsing %s as %s!\n",
						ipaddr_garbage_addr4_strs[i], buf);
				return -1;
			}
			if (ret != -1 &&
					memcmp(&result.addr.ipv4, &tmp, sizeof(tmp))) {
				printf("Error: result mismatch when parsing %s as %s!\n",
						ipaddr_garbage_addr4_strs[i], buf);
				return -1;
			}
		}
	}

	/* test garbage ipv6 address strings */
	for (i = 0; i < RTE_DIM(ipaddr_garbage_addr6_strs); i++) {

		cmdline_ipaddr_t tmp = {.addr = IPv6_GARBAGE_ADDR};

		/* test each valid string against different flags */
		for (flags = 1; flags < 0x8; flags++) {

			/* skip bad flag */
			if (flags == CMDLINE_IPADDR_NETWORK)
				continue;

			/* clear out everything */
			memset(buf, 0, sizeof(buf));
			memset(&result, 0, sizeof(result));
			memset(&token, 0, sizeof(token));

			token.ipaddr_data.flags = flags;

			cmdline_get_help_ipaddr((cmdline_parse_token_hdr_t*)&token,
							buf, sizeof(buf));

			ret = cmdline_parse_ipaddr((cmdline_parse_token_hdr_t*)&token,
				ipaddr_garbage_addr6_strs[i], (void*)&result,
				sizeof(result));

			/* if should have passed, or should have failed */
			if ((ret < 0) ==
					(can_parse_addr(CMDLINE_IPADDR_V6, flags))) {
				printf("Error: unexpected behavior when parsing %s as %s!\n",
						ipaddr_garbage_addr6_strs[i], buf);
				return -1;
			}
			if (ret != -1 &&
					memcmp(&result.addr.ipv6, &tmp.addr.ipv6, sizeof(struct in6_addr))) {
				printf("Error: result mismatch when parsing %s as %s!\n",
						ipaddr_garbage_addr6_strs[i], buf);
				return -1;
			}
		}
	}


	/* test garbage ipv4 network strings */
	for (i = 0; i < RTE_DIM(ipaddr_garbage_network4_strs); i++) {

		struct in_addr tmp = IPv4_GARBAGE_ADDR;

		/* test each valid string against different flags */
		for (flags = 1; flags < 0x8; flags++) {

			/* skip bad flag */
			if (flags == CMDLINE_IPADDR_NETWORK)
				continue;

			/* clear out everything */
			memset(buf, 0, sizeof(buf));
			memset(&result, 0, sizeof(result));
			memset(&token, 0, sizeof(token));

			token.ipaddr_data.flags = flags;

			cmdline_get_help_ipaddr((cmdline_parse_token_hdr_t*)&token,
							buf, sizeof(buf));

			ret = cmdline_parse_ipaddr((cmdline_parse_token_hdr_t*)&token,
				ipaddr_garbage_network4_strs[i], (void*)&result,
				sizeof(result));

			/* if should have passed, or should have failed */
			if ((ret < 0) ==
					(can_parse_addr(CMDLINE_IPADDR_V4 | CMDLINE_IPADDR_NETWORK, flags))) {
				printf("Error: unexpected behavior when parsing %s as %s!\n",
						ipaddr_garbage_network4_strs[i], buf);
				return -1;
			}
			if (ret != -1 &&
					memcmp(&result.addr.ipv4, &tmp, sizeof(tmp))) {
				printf("Error: result mismatch when parsing %s as %s!\n",
						ipaddr_garbage_network4_strs[i], buf);
				return -1;
			}
		}
	}

	/* test garbage ipv6 address strings */
	for (i = 0; i < RTE_DIM(ipaddr_garbage_network6_strs); i++) {

		cmdline_ipaddr_t tmp = {.addr = IPv6_GARBAGE_ADDR};

		/* test each valid string against different flags */
		for (flags = 1; flags < 0x8; flags++) {

			/* skip bad flag */
			if (flags == CMDLINE_IPADDR_NETWORK)
				continue;

			/* clear out everything */
			memset(buf, 0, sizeof(buf));
			memset(&result, 0, sizeof(result));
			memset(&token, 0, sizeof(token));

			token.ipaddr_data.flags = flags;

			cmdline_get_help_ipaddr((cmdline_parse_token_hdr_t*)&token,
							buf, sizeof(buf));

			ret = cmdline_parse_ipaddr((cmdline_parse_token_hdr_t*)&token,
				ipaddr_garbage_network6_strs[i], (void*)&result,
				sizeof(result));

			/* if should have passed, or should have failed */
			if ((ret < 0) ==
					(can_parse_addr(CMDLINE_IPADDR_V6 | CMDLINE_IPADDR_NETWORK, flags))) {
				printf("Error: unexpected behavior when parsing %s as %s!\n",
						ipaddr_garbage_network6_strs[i], buf);
				return -1;
			}
			if (ret != -1 &&
					memcmp(&result.addr.ipv6, &tmp.addr.ipv6, sizeof(struct in6_addr))) {
				printf("Error: result mismatch when parsing %s as %s!\n",
						ipaddr_garbage_network6_strs[i], buf);
				return -1;
			}
		}
	}

	return 0;
}

int
test_parse_ipaddr_invalid_data(void)
{
	cmdline_parse_token_ipaddr_t token;
	char buf[CMDLINE_TEST_BUFSIZE];
	cmdline_ipaddr_t result;
	unsigned i;
	uint8_t flags;
	int ret;

	memset(&result, 0, sizeof(result));

	/* test invalid strings */
	for (i = 0; i < RTE_DIM(ipaddr_invalid_strs); i++) {

		/* test each valid string against different flags */
		for (flags = 1; flags < 0x8; flags++) {

			/* skip bad flag */
			if (flags == CMDLINE_IPADDR_NETWORK)
				continue;

			/* clear out everything */
			memset(buf, 0, sizeof(buf));
			memset(&token, 0, sizeof(token));

			token.ipaddr_data.flags = flags;

			cmdline_get_help_ipaddr((cmdline_parse_token_hdr_t*)&token,
					buf, sizeof(buf));

			ret = cmdline_parse_ipaddr((cmdline_parse_token_hdr_t*)&token,
				ipaddr_invalid_strs[i], (void*)&result,
				sizeof(result));

			if (ret != -1) {
				printf("Error: parsing %s as %s succeeded!\n",
						ipaddr_invalid_strs[i], buf);
				printf("Parsed result: ");
				dump_addr(result);
				return -1;
			}
		}
	}

	return 0;
}

int
test_parse_ipaddr_invalid_param(void)
{
	cmdline_parse_token_ipaddr_t token;
	char buf[CMDLINE_TEST_BUFSIZE];
	cmdline_ipaddr_t result;

	snprintf(buf, sizeof(buf), "1.2.3.4");
	token.ipaddr_data.flags = CMDLINE_IPADDR_V4;

	/* null token */
	if (cmdline_parse_ipaddr(NULL, buf, (void*)&result,
			sizeof(result)) != -1) {
		printf("Error: parser accepted invalid parameters!\n");
		return -1;
	}
	/* null buffer */
	if (cmdline_parse_ipaddr((cmdline_parse_token_hdr_t*)&token,
			NULL, (void*)&result, sizeof(result)) != -1) {
		printf("Error: parser accepted invalid parameters!\n");
		return -1;
	}
	/* empty buffer */
	if (cmdline_parse_ipaddr((cmdline_parse_token_hdr_t*)&token,
			"", (void*)&result, sizeof(result)) != -1) {
		printf("Error: parser accepted invalid parameters!\n");
		return -1;
	}
	/* null result */
	if (cmdline_parse_ipaddr((cmdline_parse_token_hdr_t*)&token,
			buf, NULL, 0) == -1) {
		printf("Error: parser rejected null result!\n");
		return -1;
	}

	/* null token */
	if (cmdline_get_help_ipaddr(NULL, buf, 0) != -1) {
		printf("Error: help accepted invalid parameters!\n");
		return -1;
	}
	/* null buffer */
	if (cmdline_get_help_ipaddr((cmdline_parse_token_hdr_t*)&token,
			NULL, 0) != -1) {
		printf("Error: help accepted invalid parameters!\n");
		return -1;
	}
	return 0;
}
