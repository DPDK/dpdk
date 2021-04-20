/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdbool.h>

#include <rte_ether.h>
#include <rte_errno.h>

void
rte_eth_random_addr(uint8_t *addr)
{
#ifdef RTE_EXEC_ENV_WINDOWS /* FIXME: random is not supported */
	RTE_SET_USED(addr);
#else
	uint64_t rand = rte_rand();
	uint8_t *p = (uint8_t *)&rand;

	rte_memcpy(addr, p, RTE_ETHER_ADDR_LEN);
	addr[0] &= (uint8_t)~RTE_ETHER_GROUP_ADDR;	/* clear multicast bit */
	addr[0] |= RTE_ETHER_LOCAL_ADMIN_ADDR;	/* set local assignment bit */
#endif
}

void
rte_ether_format_addr(char *buf, uint16_t size,
		      const struct rte_ether_addr *eth_addr)
{
	snprintf(buf, size, "%02X:%02X:%02X:%02X:%02X:%02X",
		 eth_addr->addr_bytes[0],
		 eth_addr->addr_bytes[1],
		 eth_addr->addr_bytes[2],
		 eth_addr->addr_bytes[3],
		 eth_addr->addr_bytes[4],
		 eth_addr->addr_bytes[5]);
}

static int8_t get_xdigit(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	return -1;
}

/* Convert 00:11:22:33:44:55 to ethernet address */
static bool get_ether_addr6(const char *s0, struct rte_ether_addr *ea)
{
	const char *s = s0;
	int i;

	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
		int8_t x;

		x = get_xdigit(*s++);
		if (x < 0)
			return false;

		ea->addr_bytes[i] = x << 4;
		x = get_xdigit(*s++);
		if (x < 0)
			return false;
		ea->addr_bytes[i] |= x;

		if (i < RTE_ETHER_ADDR_LEN - 1 &&
		    *s++ != ':')
			return false;
	}

	/* return true if at end of string */
	return *s == '\0';
}

/* Convert 0011:2233:4455 to ethernet address */
static bool get_ether_addr3(const char *s, struct rte_ether_addr *ea)
{
	int i, j;

	for (i = 0; i < RTE_ETHER_ADDR_LEN; i += 2) {
		uint16_t w = 0;

		for (j = 0; j < 4; j++) {
			int8_t x;

			x = get_xdigit(*s++);
			if (x < 0)
				return false;
			w = (w << 4) | x;
		}
		ea->addr_bytes[i] = w >> 8;
		ea->addr_bytes[i + 1] = w & 0xff;

		if (i < RTE_ETHER_ADDR_LEN - 2 &&
		    *s++ != ':')
			return false;
	}

	return *s == '\0';
}

/*
 * Like ether_aton_r but can handle either
 * XX:XX:XX:XX:XX:XX or XXXX:XXXX:XXXX
 * and is more restrictive.
 */
int
rte_ether_unformat_addr(const char *s, struct rte_ether_addr *ea)
{
	if (get_ether_addr6(s, ea))
		return 0;
	if (get_ether_addr3(s, ea))
		return 0;

	rte_errno = EINVAL;
	return -1;
}
