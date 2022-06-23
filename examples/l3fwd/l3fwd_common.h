/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2018 Intel Corporation.
 * Copyright(c) 2017-2018 Linaro Limited.
 */


#ifndef _L3FWD_COMMON_H_
#define _L3FWD_COMMON_H_

#include "pkt_group.h"

#ifdef DO_RFC_1812_CHECKS

#define	IPV4_MIN_VER_IHL	0x45
#define	IPV4_MAX_VER_IHL	0x4f
#define	IPV4_MAX_VER_IHL_DIFF	(IPV4_MAX_VER_IHL - IPV4_MIN_VER_IHL)

/* Minimum value of IPV4 total length (20B) in network byte order. */
#define	IPV4_MIN_LEN_BE	(sizeof(struct rte_ipv4_hdr) << 8)

/*
 * From http://www.rfc-editor.org/rfc/rfc1812.txt section 5.2.2:
 * - The IP version number must be 4.
 * - The IP header length field must be large enough to hold the
 *    minimum length legal IP datagram (20 bytes = 5 words).
 * - The IP total length field must be large enough to hold the IP
 *   datagram header, whose length is specified in the IP header length
 *   field.
 * If we encounter invalid IPV4 packet, then set destination port for it
 * to BAD_PORT value.
 */
static __rte_always_inline void
rfc1812_process(struct rte_ipv4_hdr *ipv4_hdr, uint16_t *dp, uint32_t ptype)
{
	uint8_t ihl;

	if (RTE_ETH_IS_IPV4_HDR(ptype)) {
		ihl = ipv4_hdr->version_ihl - IPV4_MIN_VER_IHL;

		ipv4_hdr->time_to_live--;
		ipv4_hdr->hdr_checksum++;

		if (ihl > IPV4_MAX_VER_IHL_DIFF ||
				((uint8_t)ipv4_hdr->total_length == 0 &&
				ipv4_hdr->total_length < IPV4_MIN_LEN_BE))
			dp[0] = BAD_PORT;

	}
}

#else
#define	rfc1812_process(mb, dp, ptype)	do { } while (0)
#endif /* DO_RFC_1812_CHECKS */

static __rte_always_inline void
send_packetsx4(struct lcore_conf *qconf, uint16_t port, struct rte_mbuf *m[],
		uint32_t num)
{
	uint32_t len, j, n;

	len = qconf->tx_mbufs[port].len;

	/*
	 * If TX buffer for that queue is empty, and we have enough packets,
	 * then send them straightway.
	 */
	if (num >= MAX_TX_BURST && len == 0) {
		n = rte_eth_tx_burst(port, qconf->tx_queue_id[port], m, num);
		if (unlikely(n < num)) {
			do {
				rte_pktmbuf_free(m[n]);
			} while (++n < num);
		}
		return;
	}

	/*
	 * Put packets into TX buffer for that queue.
	 */

	n = len + num;
	n = (n > MAX_PKT_BURST) ? MAX_PKT_BURST - len : num;

	j = 0;
	switch (n % FWDSTEP) {
	while (j < n) {
	case 0:
		qconf->tx_mbufs[port].m_table[len + j] = m[j];
		j++;
		/* fallthrough */
	case 3:
		qconf->tx_mbufs[port].m_table[len + j] = m[j];
		j++;
		/* fallthrough */
	case 2:
		qconf->tx_mbufs[port].m_table[len + j] = m[j];
		j++;
		/* fallthrough */
	case 1:
		qconf->tx_mbufs[port].m_table[len + j] = m[j];
		j++;
	}
	}

	len += n;

	/* enough pkts to be sent */
	if (unlikely(len == MAX_PKT_BURST)) {

		send_burst(qconf, MAX_PKT_BURST, port);

		/* copy rest of the packets into the TX buffer. */
		len = num - n;
		if (len == 0)
			goto exit;

		j = 0;
		switch (len % FWDSTEP) {
		while (j < len) {
		case 0:
			qconf->tx_mbufs[port].m_table[j] = m[n + j];
			j++;
			/* fallthrough */
		case 3:
			qconf->tx_mbufs[port].m_table[j] = m[n + j];
			j++;
			/* fallthrough */
		case 2:
			qconf->tx_mbufs[port].m_table[j] = m[n + j];
			j++;
			/* fallthrough */
		case 1:
			qconf->tx_mbufs[port].m_table[j] = m[n + j];
			j++;
		}
		}
	}

exit:
	qconf->tx_mbufs[port].len = len;
}

#endif /* _L3FWD_COMMON_H_ */
