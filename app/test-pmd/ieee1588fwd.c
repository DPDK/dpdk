/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */


#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_flow.h>

#include "testpmd.h"

/**
 * The structure of a PTP V2 packet.
 *
 * Only the minimum fields used by the ieee1588 test are represented.
 */
struct ptpv2_msg {
	uint8_t msg_id;
	uint8_t version; /**< must be 0x02 */
	uint8_t unused[34];
};

#define PTP_SYNC_MESSAGE                0x0
#define PTP_DELAY_REQ_MESSAGE           0x1
#define PTP_PATH_DELAY_REQ_MESSAGE      0x2
#define PTP_PATH_DELAY_RESP_MESSAGE     0x3
#define PTP_FOLLOWUP_MESSAGE            0x8
#define PTP_DELAY_RESP_MESSAGE          0x9
#define PTP_PATH_DELAY_FOLLOWUP_MESSAGE 0xA
#define PTP_ANNOUNCE_MESSAGE            0xB
#define PTP_SIGNALLING_MESSAGE          0xC
#define PTP_MANAGEMENT_MESSAGE          0xD

/*
 * Forwarding of IEEE1588 Precise Time Protocol (PTP) packets.
 *
 * In this mode, packets are received one by one and are expected to be
 * PTP V2 L2 Ethernet frames (with the specific Ethernet type "0x88F7")
 * containing PTP "sync" messages (version 2 at offset 1, and message ID
 * 0 at offset 0).
 *
 * Check that each received packet is a IEEE1588 PTP V2 packet of type
 * PTP_SYNC_MESSAGE, and that it has been identified and timestamped
 * by the hardware.
 * Check that the value of the last RX timestamp recorded by the controller
 * is greater than the previous one.
 *
 * If everything is OK, send the received packet back on the same port,
 * requesting for it to be timestamped by the hardware.
 * Check that the value of the last TX timestamp recorded by the controller
 * is greater than the previous one.
 */

static void
port_ieee1588_rx_timestamp_check(portid_t pi, uint32_t index)
{
	struct timespec timestamp = {0, 0};

	if (rte_eth_timesync_read_rx_timestamp(pi, &timestamp, index) < 0) {
		printf("Port %u RX timestamp registers not valid\n", pi);
		return;
	}
	printf("Port %u RX timestamp value %ju s %lu ns\n",
		pi, (uintmax_t)timestamp.tv_sec, timestamp.tv_nsec);
}

#define MAX_TX_TMST_WAIT_MICROSECS 1000 /**< 1 milli-second */

static void
port_ieee1588_tx_timestamp_check(portid_t pi)
{
	struct timespec timestamp = {0, 0};
	unsigned wait_us = 0;

	while ((rte_eth_timesync_read_tx_timestamp(pi, &timestamp) < 0) &&
	       (wait_us < MAX_TX_TMST_WAIT_MICROSECS)) {
		rte_delay_us(1);
		wait_us++;
	}
	if (wait_us >= MAX_TX_TMST_WAIT_MICROSECS) {
		printf("Port %u TX timestamp registers not valid after "
		       "%u micro-seconds\n",
		       pi, MAX_TX_TMST_WAIT_MICROSECS);
		return;
	}
	printf("Port %u TX timestamp value %ju s %lu ns validated after "
	       "%u micro-second%s\n",
	       pi, (uintmax_t)timestamp.tv_sec, timestamp.tv_nsec, wait_us,
	       (wait_us == 1) ? "" : "s");
}

static bool
ieee1588_packet_fwd(struct fwd_stream *fs)
{
	struct rte_mbuf  *mb;
	struct rte_ether_hdr *eth_hdr;
	struct rte_ether_addr addr;
	struct ptpv2_msg *ptp_hdr;
	uint16_t eth_type;
	uint32_t timesync_index;

	/*
	 * Receive 1 packet at a time.
	 */
	if (common_fwd_stream_receive(fs, &mb, 1) == 0)
		return false;

	/*
	 * Check that the received packet is a PTP packet that was detected
	 * by the hardware.
	 */
	eth_hdr = rte_pktmbuf_mtod(mb, struct rte_ether_hdr *);
	eth_type = rte_be_to_cpu_16(eth_hdr->ether_type);

	if (!(mb->ol_flags & RTE_MBUF_F_RX_IEEE1588_PTP)) {
		if (eth_type == RTE_ETHER_TYPE_1588) {
			printf("Port %u Received PTP packet not filtered"
			       " by hardware\n",
			       fs->rx_port);
		} else {
			printf("Port %u Received non PTP packet type=0x%4x "
			       "len=%u\n",
			       fs->rx_port, eth_type,
			       (unsigned) mb->pkt_len);
		}
		rte_pktmbuf_free(mb);
		return false;
	}
	if (eth_type != RTE_ETHER_TYPE_1588) {
		printf("Port %u Received NON PTP packet incorrectly"
		       " detected by hardware\n",
		       fs->rx_port);
		rte_pktmbuf_free(mb);
		return false;
	}

	/*
	 * Check that the received PTP packet is a PTP V2 packet of type
	 * PTP_SYNC_MESSAGE.
	 */
	ptp_hdr = (struct ptpv2_msg *) (rte_pktmbuf_mtod(mb, char *) +
					sizeof(struct rte_ether_hdr));
	if (ptp_hdr->version != 0x02) {
		printf("Port %u Received PTP V2 Ethernet frame with wrong PTP"
		       " protocol version 0x%x (should be 0x02)\n",
		       fs->rx_port, ptp_hdr->version);
		rte_pktmbuf_free(mb);
		return false;
	}
	if (ptp_hdr->msg_id != PTP_SYNC_MESSAGE) {
		printf("Port %u Received PTP V2 Ethernet frame with unexpected"
		       " message ID 0x%x (expected 0x0 - PTP_SYNC_MESSAGE)\n",
		       fs->rx_port, ptp_hdr->msg_id);
		rte_pktmbuf_free(mb);
		return false;
	}
	printf("Port %u IEEE1588 PTP V2 SYNC Message filtered by hardware\n",
	       fs->rx_port);

	/*
	 * Check that the received PTP packet has been timestamped by the
	 * hardware.
	 */
	if (!(mb->ol_flags & RTE_MBUF_F_RX_IEEE1588_TMST)) {
		printf("Port %u Received PTP packet not timestamped"
		       " by hardware\n",
		       fs->rx_port);
		rte_pktmbuf_free(mb);
		return false;
	}

	/* For i40e we need the timesync register index. It is ignored for the
	 * other PMDs. */
	timesync_index = mb->timesync & 0x3;
	/* Read and check the RX timestamp. */
	port_ieee1588_rx_timestamp_check(fs->rx_port, timesync_index);

	/* Swap dest and src mac addresses. */
	rte_ether_addr_copy(&eth_hdr->dst_addr, &addr);
	rte_ether_addr_copy(&eth_hdr->src_addr, &eth_hdr->dst_addr);
	rte_ether_addr_copy(&addr, &eth_hdr->src_addr);

	/* Forward PTP packet with hardware TX timestamp */
	mb->ol_flags |= RTE_MBUF_F_TX_IEEE1588_TMST;
	if (common_fwd_stream_transmit(fs, &mb, 1) == 0) {
		printf("Port %u sent PTP packet dropped\n", fs->tx_port);
		return false;
	}

	/*
	 * Check the TX timestamp.
	 */
	port_ieee1588_tx_timestamp_check(fs->tx_port);
	return true;
}

static int
port_ieee1588_fwd_begin(portid_t pi)
{
	rte_eth_timesync_enable(pi);
	return 0;
}

static void
port_ieee1588_fwd_end(portid_t pi)
{
	rte_eth_timesync_disable(pi);
}

static void
port_ieee1588_stream_init(struct fwd_stream *fs)
{
	/* Force transmission on reception port */
	fs->tx_port = fs->rx_port;

	common_fwd_stream_init(fs);
}

struct fwd_engine ieee1588_fwd_engine = {
	.fwd_mode_name  = "ieee1588",
	.port_fwd_begin = port_ieee1588_fwd_begin,
	.port_fwd_end   = port_ieee1588_fwd_end,
	.stream_init    = port_ieee1588_stream_init,
	.packet_fwd     = ieee1588_packet_fwd,
};
