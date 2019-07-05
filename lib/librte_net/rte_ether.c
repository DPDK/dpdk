/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <rte_ether.h>
#include <rte_errno.h>

void
rte_eth_random_addr(uint8_t *addr)
{
	uint64_t rand = rte_rand();
	uint8_t *p = (uint8_t *)&rand;

	rte_memcpy(addr, p, RTE_ETHER_ADDR_LEN);
	addr[0] &= (uint8_t)~RTE_ETHER_GROUP_ADDR;	/* clear multicast bit */
	addr[0] |= RTE_ETHER_LOCAL_ADMIN_ADDR;	/* set local assignment bit */
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

/*
 * Like ether_aton_r but can handle either
 * XX:XX:XX:XX:XX:XX or XXXX:XXXX:XXXX
 */
int
rte_ether_unformat_addr(const char *s, struct rte_ether_addr *ea)
{
	unsigned int o0, o1, o2, o3, o4, o5;
	int n;

	n = sscanf(s, "%x:%x:%x:%x:%x:%x",
		    &o0, &o1, &o2, &o3, &o4, &o5);

	if (n == 6) {
		/* Standard format XX:XX:XX:XX:XX:XX */
		if (o0 > UINT8_MAX || o1 > UINT8_MAX || o2 > UINT8_MAX ||
		    o3 > UINT8_MAX || o4 > UINT8_MAX || o5 > UINT8_MAX) {
			rte_errno = ERANGE;
			return -1;
		}

		ea->addr_bytes[0] = o0;
		ea->addr_bytes[1] = o1;
		ea->addr_bytes[2] = o2;
		ea->addr_bytes[3] = o3;
		ea->addr_bytes[4] = o4;
		ea->addr_bytes[5] = o5;
	} else if (n == 3) {
		/* Support the format XXXX:XXXX:XXXX */
		if (o0 > UINT16_MAX || o1 > UINT16_MAX || o2 > UINT16_MAX) {
			rte_errno = ERANGE;
			return -1;
		}

		ea->addr_bytes[0] = o0 >> 8;
		ea->addr_bytes[1] = o0 & 0xff;
		ea->addr_bytes[2] = o1 >> 8;
		ea->addr_bytes[3] = o1 & 0xff;
		ea->addr_bytes[4] = o2 >> 8;
		ea->addr_bytes[5] = o2 & 0xff;
	} else {
		/* unknown format */
		rte_errno = EINVAL;
		return -1;
	}
	return 0;
}
