/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdbool.h>

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
	snprintf(buf, size, RTE_ETHER_ADDR_PRT_FMT,
		RTE_ETHER_ADDR_BYTES(eth_addr));
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
static bool get_ether_addr6(const char *s0, struct rte_ether_addr *ea,
			    const char sep)
{
	const char *s = s0;
	int i;

	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
		int8_t x;

		x = get_xdigit(*s++);
		if (x < 0)
			return false;	/* not a hex digit */

		ea->addr_bytes[i] = x;
		if (*s != sep && *s != '\0') {
			x = get_xdigit(*s++);
			if (x < 0)
				return false;	/* not a hex digit */
			ea->addr_bytes[i] <<= 4;
			ea->addr_bytes[i] |= x;
		}

		if (i < RTE_ETHER_ADDR_LEN - 1 &&
		    *s++ != sep)
			return false;	/* premature end of string */
	}

	/* return true if no trailing characters */
	return *s == '\0';
}

/* Convert 0011:2233:4455 to ethernet address */
static bool get_ether_addr3(const char *s, struct rte_ether_addr *ea,
			    const char sep)
{
	int i, j;

	for (i = 0; i < RTE_ETHER_ADDR_LEN; i += 2) {
		uint16_t w = 0;

		for (j = 0; j < 4; j++) {
			int8_t x;

			x = get_xdigit(*s++);
			if (x < 0)
				return false;	/* not a hex digit */
			w = (w << 4) | x;
		}

		ea->addr_bytes[i] = w >> 8;
		ea->addr_bytes[i + 1] = w & 0xff;

		if (i < RTE_ETHER_ADDR_LEN - 2 &&
		    *s++ != sep)
			return false;
	}

	return *s == '\0';
}

/*
 * Scan input to see if separated by dash, colon or period
 * Returns separator and number of matches
 * If separators are mixed will return
 */
static unsigned int get_ether_sep(const char *s, char *sep)
{
	static const char separators[] = "-:.";
	unsigned int count = 0;
	const char *cp;

	cp = strpbrk(s, separators);
	if (cp == NULL)
		return 0;	/* no separator found */

	*sep = *cp;		/* return the separator */
	do {
		++count;
		/* find next instance of separator */
		cp = strchr(cp + 1, *sep);
	} while (cp != NULL);

	return count;
}

/*
 * Be liberal in accepting a wide variety of notational formats
 * for MAC address including:
 *  - Linux format six groups of hexadecimal digits separated by colon
 *  - Windows format six groups separated by hyphen
 *  - two groups hexadecimal digits
 */
int
rte_ether_unformat_addr(const char *s, struct rte_ether_addr *ea)
{
	unsigned int count;
	char sep = '\0';

	count = get_ether_sep(s, &sep);
	switch (count) {
	case 5:	/* i.e 01:23:45:67:89:AB */
		if (get_ether_addr6(s, ea, sep))
			return 0;
		break;
	case 2: /* i.e 0123.4567.89AB */
		if (get_ether_addr3(s, ea, sep))
			return 0;
		break;
	default:
		break;
	}

	rte_errno = EINVAL;
	return -1;
}
