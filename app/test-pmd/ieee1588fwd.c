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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <sys/queue.h>
#include <sys/stat.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>

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

/*
 * 1GbE 82576 Kawela registers used for IEEE1588 hardware support
 */
#define IGBE_82576_ETQF(n) (0x05CB0 + (4 * (n)))
#define IGBE_82576_ETQF_FILTER_ENABLE  (1 << 26)
#define IGBE_82576_ETQF_1588_TIMESTAMP (1 << 30)

#define IGBE_82576_TSYNCRXCTL  0x0B620
#define IGBE_82576_TSYNCRXCTL_RXTS_ENABLE (1 << 4)

#define IGBE_82576_RXSTMPL     0x0B624
#define IGBE_82576_RXSTMPH     0x0B628
#define IGBE_82576_RXSATRL     0x0B62C
#define IGBE_82576_RXSATRH     0x0B630
#define IGBE_82576_TSYNCTXCTL  0x0B614
#define IGBE_82576_TSYNCTXCTL_TXTS_ENABLE (1 << 4)

#define IGBE_82576_TXSTMPL     0x0B618
#define IGBE_82576_TXSTMPH     0x0B61C
#define IGBE_82576_SYSTIML     0x0B600
#define IGBE_82576_SYSTIMH     0x0B604
#define IGBE_82576_TIMINCA     0x0B608
#define IGBE_82576_TIMADJL     0x0B60C
#define IGBE_82576_TIMADJH     0x0B610
#define IGBE_82576_TSAUXC      0x0B640
#define IGBE_82576_TRGTTIML0   0x0B644
#define IGBE_82576_TRGTTIMH0   0x0B648
#define IGBE_82576_TRGTTIML1   0x0B64C
#define IGBE_82576_TRGTTIMH1   0x0B650
#define IGBE_82576_AUXSTMPL0   0x0B65C
#define IGBE_82576_AUXSTMPH0   0x0B660
#define IGBE_82576_AUXSTMPL1   0x0B664
#define IGBE_82576_AUXSTMPH1   0x0B668
#define IGBE_82576_TSYNCRXCFG  0x05F50
#define IGBE_82576_TSSDP       0x0003C

/*
 * 10GbE 82599 Niantic registers used for IEEE1588 hardware support
 */
#define IXGBE_82599_ETQF(n) (0x05128 + (4 * (n)))
#define IXGBE_82599_ETQF_FILTER_ENABLE  (1 << 31)
#define IXGBE_82599_ETQF_1588_TIMESTAMP (1 << 30)

#define IXGBE_82599_TSYNCRXCTL 0x05188
#define IXGBE_82599_TSYNCRXCTL_RXTS_ENABLE (1 << 4)

#define IXGBE_82599_RXSTMPL    0x051E8
#define IXGBE_82599_RXSTMPH    0x051A4
#define IXGBE_82599_RXSATRL    0x051A0
#define IXGBE_82599_RXSATRH    0x051A8
#define IXGBE_82599_RXMTRL     0x05120
#define IXGBE_82599_TSYNCTXCTL 0x08C00
#define IXGBE_82599_TSYNCTXCTL_TXTS_ENABLE (1 << 4)

#define IXGBE_82599_TXSTMPL    0x08C04
#define IXGBE_82599_TXSTMPH    0x08C08
#define IXGBE_82599_SYSTIML    0x08C0C
#define IXGBE_82599_SYSTIMH    0x08C10
#define IXGBE_82599_TIMINCA    0x08C14
#define IXGBE_82599_TIMADJL    0x08C18
#define IXGBE_82599_TIMADJH    0x08C1C
#define IXGBE_82599_TSAUXC     0x08C20
#define IXGBE_82599_TRGTTIML0  0x08C24
#define IXGBE_82599_TRGTTIMH0  0x08C28
#define IXGBE_82599_TRGTTIML1  0x08C2C
#define IXGBE_82599_TRGTTIMH1  0x08C30
#define IXGBE_82599_AUXSTMPL0  0x08C3C
#define IXGBE_82599_AUXSTMPH0  0x08C40
#define IXGBE_82599_AUXSTMPL1  0x08C44
#define IXGBE_82599_AUXSTMPH1  0x08C48

/**
 * Mandatory ETQF register for IEEE1588 packets filter.
 */
#define ETQF_FILTER_1588_REG 3

/**
 * Recommended value for increment and period of
 * the Increment Attribute Register.
 */
#define IEEE1588_TIMINCA_INIT ((0x02 << 24) | 0x00F42400)

/**
 * Data structure with pointers to port-specific functions.
 */
typedef void (*ieee1588_start_t)(portid_t pi); /**< Start IEEE1588 feature. */
typedef void (*ieee1588_stop_t)(portid_t pi);  /**< Stop IEEE1588 feature.  */
typedef int  (*tmst_read_t)(portid_t pi, uint64_t *tmst); /**< Read TMST regs */

struct port_ieee1588_ops {
	ieee1588_start_t ieee1588_start;
	ieee1588_stop_t  ieee1588_stop;
	tmst_read_t      rx_tmst_read;
	tmst_read_t      tx_tmst_read;
};

/**
 * 1GbE 82576 IEEE1588 operations.
 */
static void
igbe_82576_ieee1588_start(portid_t pi)
{
	uint32_t tsync_ctl;

	/*
	 * Start incrementation of the System Time registers used to
	 * timestamp PTP packets.
	 */
	port_id_pci_reg_write(pi, IGBE_82576_TIMINCA, IEEE1588_TIMINCA_INIT);
	port_id_pci_reg_write(pi, IGBE_82576_TSAUXC, 0);

	/*
	 * Enable L2 filtering of IEEE1588 Ethernet frame types.
	 */
	port_id_pci_reg_write(pi, IGBE_82576_ETQF(ETQF_FILTER_1588_REG),
			      (ETHER_TYPE_1588 |
			       IGBE_82576_ETQF_FILTER_ENABLE |
			       IGBE_82576_ETQF_1588_TIMESTAMP));

	/*
	 * Enable timestamping of received PTP packets.
	 */
	tsync_ctl = port_id_pci_reg_read(pi, IGBE_82576_TSYNCRXCTL);
	tsync_ctl |= IGBE_82576_TSYNCRXCTL_RXTS_ENABLE;
	port_id_pci_reg_write(pi, IGBE_82576_TSYNCRXCTL, tsync_ctl);

	/*
	 * Enable Timestamping of transmitted PTP packets.
	 */
	tsync_ctl = port_id_pci_reg_read(pi, IGBE_82576_TSYNCTXCTL);
	tsync_ctl |= IGBE_82576_TSYNCTXCTL_TXTS_ENABLE;
	port_id_pci_reg_write(pi, IGBE_82576_TSYNCTXCTL, tsync_ctl);
}

static void
igbe_82576_ieee1588_stop(portid_t pi)
{
	uint32_t tsync_ctl;

	/*
	 * Disable Timestamping of transmitted PTP packets.
	 */
	tsync_ctl = port_id_pci_reg_read(pi, IGBE_82576_TSYNCTXCTL);
	tsync_ctl &= ~IGBE_82576_TSYNCTXCTL_TXTS_ENABLE;
	port_id_pci_reg_write(pi, IGBE_82576_TSYNCTXCTL, tsync_ctl);

	/*
	 * Disable timestamping of received PTP packets.
	 */
	tsync_ctl = port_id_pci_reg_read(pi, IGBE_82576_TSYNCRXCTL);
	tsync_ctl &= ~IGBE_82576_TSYNCRXCTL_RXTS_ENABLE;
	port_id_pci_reg_write(pi, IGBE_82576_TSYNCRXCTL, tsync_ctl);

	/*
	 * Disable L2 filtering of IEEE1588 Ethernet types.
	 */
	port_id_pci_reg_write(pi, IGBE_82576_ETQF(ETQF_FILTER_1588_REG), 0);

	/*
	 * Stop incrementation of the System Time registers.
	 */
	port_id_pci_reg_write(pi, IGBE_82576_TIMINCA, 0);
}

/**
 * Return the 64-bit value contained in the RX IEEE1588 timestamp registers
 * of a 1GbE 82576 port.
 *
 * @param pi
 *   The port identifier.
 *
 * @param tmst
 *   The address of a 64-bit variable to return the value of the RX timestamp.
 *
 * @return
 *  -1: the RXSTMPL and RXSTMPH registers of the port are not valid.
 *   0: the variable pointed to by the "tmst" parameter contains the value
 *      of the RXSTMPL and RXSTMPH registers of the port.
 */
static int
igbe_82576_rx_timestamp_read(portid_t pi, uint64_t *tmst)
{
	uint32_t tsync_rxctl;
	uint32_t rx_stmpl;
	uint32_t rx_stmph;

	tsync_rxctl = port_id_pci_reg_read(pi, IGBE_82576_TSYNCRXCTL);
	if ((tsync_rxctl & 0x01) == 0)
		return (-1);

	rx_stmpl = port_id_pci_reg_read(pi, IGBE_82576_RXSTMPL);
	rx_stmph = port_id_pci_reg_read(pi, IGBE_82576_RXSTMPH);
	*tmst = (uint64_t)(((uint64_t) rx_stmph << 32) | rx_stmpl);
	return (0);
}

/**
 * Return the 64-bit value contained in the TX IEEE1588 timestamp registers
 * of a 1GbE 82576 port.
 *
 * @param pi
 *   The port identifier.
 *
 * @param tmst
 *   The address of a 64-bit variable to return the value of the TX timestamp.
 *
 * @return
 *  -1: the TXSTMPL and TXSTMPH registers of the port are not valid.
 *   0: the variable pointed to by the "tmst" parameter contains the value
 *      of the TXSTMPL and TXSTMPH registers of the port.
 */
static int
igbe_82576_tx_timestamp_read(portid_t pi, uint64_t *tmst)
{
	uint32_t tsync_txctl;
	uint32_t tx_stmpl;
	uint32_t tx_stmph;

	tsync_txctl = port_id_pci_reg_read(pi, IGBE_82576_TSYNCTXCTL);
	if ((tsync_txctl & 0x01) == 0)
		return (-1);

	tx_stmpl = port_id_pci_reg_read(pi, IGBE_82576_TXSTMPL);
	tx_stmph = port_id_pci_reg_read(pi, IGBE_82576_TXSTMPH);
	*tmst = (uint64_t)(((uint64_t) tx_stmph << 32) | tx_stmpl);
	return (0);
}

static struct port_ieee1588_ops igbe_82576_ieee1588_ops = {
	.ieee1588_start = igbe_82576_ieee1588_start,
	.ieee1588_stop  = igbe_82576_ieee1588_stop,
	.rx_tmst_read   = igbe_82576_rx_timestamp_read,
	.tx_tmst_read   = igbe_82576_tx_timestamp_read,
};

/**
 * 10GbE 82599 IEEE1588 operations.
 */
static void
ixgbe_82599_ieee1588_start(portid_t pi)
{
	uint32_t tsync_ctl;

	/*
	 * Start incrementation of the System Time registers used to
	 * timestamp PTP packets.
	 */
	port_id_pci_reg_write(pi, IXGBE_82599_TIMINCA, IEEE1588_TIMINCA_INIT);

	/*
	 * Enable L2 filtering of IEEE1588 Ethernet frame types.
	 */
	port_id_pci_reg_write(pi, IXGBE_82599_ETQF(ETQF_FILTER_1588_REG),
			      (ETHER_TYPE_1588 |
			       IXGBE_82599_ETQF_FILTER_ENABLE |
			       IXGBE_82599_ETQF_1588_TIMESTAMP));

	/*
	 * Enable timestamping of received PTP packets.
	 */
	tsync_ctl = port_id_pci_reg_read(pi, IXGBE_82599_TSYNCRXCTL);
	tsync_ctl |= IXGBE_82599_TSYNCRXCTL_RXTS_ENABLE;
	port_id_pci_reg_write(pi, IXGBE_82599_TSYNCRXCTL, tsync_ctl);

	/*
	 * Enable Timestamping of transmitted PTP packets.
	 */
	tsync_ctl = port_id_pci_reg_read(pi, IXGBE_82599_TSYNCTXCTL);
	tsync_ctl |= IXGBE_82599_TSYNCTXCTL_TXTS_ENABLE;
	port_id_pci_reg_write(pi, IXGBE_82599_TSYNCTXCTL, tsync_ctl);
}

static void
ixgbe_82599_ieee1588_stop(portid_t pi)
{
	uint32_t tsync_ctl;

	/*
	 * Disable Timestamping of transmitted PTP packets.
	 */
	tsync_ctl = port_id_pci_reg_read(pi, IXGBE_82599_TSYNCTXCTL);
	tsync_ctl &= ~IXGBE_82599_TSYNCTXCTL_TXTS_ENABLE;
	port_id_pci_reg_write(pi, IXGBE_82599_TSYNCTXCTL, tsync_ctl);

	/*
	 * Disable timestamping of received PTP packets.
	 */
	tsync_ctl = port_id_pci_reg_read(pi, IXGBE_82599_TSYNCRXCTL);
	tsync_ctl &= ~IXGBE_82599_TSYNCRXCTL_RXTS_ENABLE;
	port_id_pci_reg_write(pi, IXGBE_82599_TSYNCRXCTL, tsync_ctl);

	/*
	 * Disable L2 filtering of IEEE1588 Ethernet frame types.
	 */
	port_id_pci_reg_write(pi, IXGBE_82599_ETQF(ETQF_FILTER_1588_REG), 0);

	/*
	 * Stop incrementation of the System Time registers.
	 */
	port_id_pci_reg_write(pi, IXGBE_82599_TIMINCA, 0);
}

/**
 * Return the 64-bit value contained in the RX IEEE1588 timestamp registers
 * of a 10GbE 82599 port.
 *
 * @param pi
 *   The port identifier.
 *
 * @param tmst
 *   The address of a 64-bit variable to return the value of the TX timestamp.
 *
 * @return
 *  -1: the RX timestamp registers of the port are not valid.
 *   0: the variable pointed to by the "tmst" parameter contains the value
 *      of the RXSTMPL and RXSTMPH registers of the port.
 */
static int
ixgbe_82599_rx_timestamp_read(portid_t pi, uint64_t *tmst)
{
	uint32_t tsync_rxctl;
	uint32_t rx_stmpl;
	uint32_t rx_stmph;

	tsync_rxctl = port_id_pci_reg_read(pi, IXGBE_82599_TSYNCRXCTL);
	if ((tsync_rxctl & 0x01) == 0)
		return (-1);

	rx_stmpl = port_id_pci_reg_read(pi, IXGBE_82599_RXSTMPL);
	rx_stmph = port_id_pci_reg_read(pi, IXGBE_82599_RXSTMPH);
	*tmst = (uint64_t)(((uint64_t) rx_stmph << 32) | rx_stmpl);
	return (0);
}

/**
 * Return the 64-bit value contained in the TX IEEE1588 timestamp registers
 * of a 10GbE 82599 port.
 *
 * @param pi
 *   The port identifier.
 *
 * @param tmst
 *   The address of a 64-bit variable to return the value of the TX timestamp.
 *
 * @return
 *  -1: the TXSTMPL and TXSTMPH registers of the port are not valid.
 *   0: the variable pointed to by the "tmst" parameter contains the value
 *      of the TXSTMPL and TXSTMPH registers of the port.
 */
static int
ixgbe_82599_tx_timestamp_read(portid_t pi, uint64_t *tmst)
{
	uint32_t tsync_txctl;
	uint32_t tx_stmpl;
	uint32_t tx_stmph;

	tsync_txctl = port_id_pci_reg_read(pi, IXGBE_82599_TSYNCTXCTL);
	if ((tsync_txctl & 0x01) == 0)
		return (-1);

	tx_stmpl = port_id_pci_reg_read(pi, IXGBE_82599_TXSTMPL);
	tx_stmph = port_id_pci_reg_read(pi, IXGBE_82599_TXSTMPH);
	*tmst = (uint64_t)(((uint64_t) tx_stmph << 32) | tx_stmpl);
	return (0);
}

static struct port_ieee1588_ops ixgbe_82599_ieee1588_ops = {
	.ieee1588_start = ixgbe_82599_ieee1588_start,
	.ieee1588_stop  = ixgbe_82599_ieee1588_stop,
	.rx_tmst_read   = ixgbe_82599_rx_timestamp_read,
	.tx_tmst_read   = ixgbe_82599_tx_timestamp_read,
};

static void
port_ieee1588_rx_timestamp_check(portid_t pi)
{
	struct port_ieee1588_ops *ieee_ops;
	uint64_t rx_tmst;

	ieee_ops = (struct port_ieee1588_ops *)ports[pi].fwd_ctx;
	if (ieee_ops->rx_tmst_read(pi, &rx_tmst) < 0) {
		printf("Port %u: RX timestamp registers not valid\n",
		       (unsigned) pi);
		return;
	}
	printf("Port %u RX timestamp value 0x%"PRIu64"\n",
	       (unsigned) pi, rx_tmst);
}

#define MAX_TX_TMST_WAIT_MICROSECS 1000 /**< 1 milli-second */

static void
port_ieee1588_tx_timestamp_check(portid_t pi)
{
	struct port_ieee1588_ops *ieee_ops;
	uint64_t tx_tmst;
	unsigned wait_us;

	ieee_ops = (struct port_ieee1588_ops *)ports[pi].fwd_ctx;
	wait_us = 0;
	while ((ieee_ops->tx_tmst_read(pi, &tx_tmst) < 0) &&
	       (wait_us < MAX_TX_TMST_WAIT_MICROSECS)) {
		rte_delay_us(1);
		wait_us++;
	}
	if (wait_us >= MAX_TX_TMST_WAIT_MICROSECS) {
		printf("Port %u: TX timestamp registers not valid after"
		       "%u micro-seconds\n",
		       (unsigned) pi, (unsigned) MAX_TX_TMST_WAIT_MICROSECS);
		return;
	}
	printf("Port %u TX timestamp value 0x%"PRIu64" validated after "
	       "%u micro-second%s\n",
	       (unsigned) pi, tx_tmst, wait_us,
	       (wait_us == 1) ? "" : "s");
}

static void
ieee1588_packet_fwd(struct fwd_stream *fs)
{
	struct rte_mbuf  *mb;
	struct ether_hdr *eth_hdr;
	struct ptpv2_msg *ptp_hdr;
	uint16_t eth_type;

	/*
	 * Receive 1 packet at a time.
	 */
	if (rte_eth_rx_burst(fs->rx_port, fs->rx_queue, &mb, 1) == 0)
		return;

	fs->rx_packets += 1;

	/*
	 * Check that the received packet is a PTP packet that was detected
	 * by the hardware.
	 */
	eth_hdr = rte_pktmbuf_mtod(mb, struct ether_hdr *);
	eth_type = rte_be_to_cpu_16(eth_hdr->ether_type);
	if (! (mb->ol_flags & PKT_RX_IEEE1588_PTP)) {
		if (eth_type == ETHER_TYPE_1588) {
			printf("Port %u Received PTP packet not filtered"
			       " by hardware\n",
			       (unsigned) fs->rx_port);
		} else {
			printf("Port %u Received non PTP packet type=0x%4x "
			       "len=%u\n",
			       (unsigned) fs->rx_port, eth_type,
			       (unsigned) mb->pkt_len);
		}
		rte_pktmbuf_free(mb);
		return;
	}
	if (eth_type != ETHER_TYPE_1588) {
		printf("Port %u Received NON PTP packet wrongly"
		       " detected by hardware\n",
		       (unsigned) fs->rx_port);
		rte_pktmbuf_free(mb);
		return;
	}

	/*
	 * Check that the received PTP packet is a PTP V2 packet of type
	 * PTP_SYNC_MESSAGE.
	 */
	ptp_hdr = (struct ptpv2_msg *) (rte_pktmbuf_mtod(mb, char *) +
					sizeof(struct ether_hdr));
	if (ptp_hdr->version != 0x02) {
		printf("Port %u Received PTP V2 Ethernet frame with wrong PTP"
		       " protocol version 0x%x (should be 0x02)\n",
		       (unsigned) fs->rx_port, ptp_hdr->version);
		rte_pktmbuf_free(mb);
		return;
	}
	if (ptp_hdr->msg_id != PTP_SYNC_MESSAGE) {
		printf("Port %u Received PTP V2 Ethernet frame with unexpected"
		       " messageID 0x%x (expected 0x0 - PTP_SYNC_MESSAGE)\n",
		       (unsigned) fs->rx_port, ptp_hdr->msg_id);
		rte_pktmbuf_free(mb);
		return;
	}
	printf("Port %u IEEE1588 PTP V2 SYNC Message filtered by hardware\n",
	       (unsigned) fs->rx_port);

	/*
	 * Check that the received PTP packet has been timestamped by the
	 * hardware.
	 */
	if (! (mb->ol_flags & PKT_RX_IEEE1588_TMST)) {
		printf("Port %u Received PTP packet not timestamped"
		       " by hardware\n",
		       (unsigned) fs->rx_port);
		rte_pktmbuf_free(mb);
		return;
	}

	/* Check the RX timestamp */
	port_ieee1588_rx_timestamp_check(fs->rx_port);

	/* Forward PTP packet with hardware TX timestamp */
	mb->ol_flags |= PKT_TX_IEEE1588_TMST;
	fs->tx_packets += 1;
	if (rte_eth_tx_burst(fs->rx_port, fs->tx_queue, &mb, 1) == 0) {
		printf("Port %u sent PTP packet dropped\n",
		       (unsigned) fs->rx_port);
		fs->fwd_dropped += 1;
		rte_pktmbuf_free(mb);
		return;
	}

	/*
	 * Check the TX timestamp.
	 */
	port_ieee1588_tx_timestamp_check(fs->rx_port);
}

static void
port_ieee1588_fwd_begin(portid_t pi)
{
	struct port_ieee1588_ops *ieee_ops;

	if (strcmp(ports[pi].dev_info.driver_name, "rte_igb_pmd") == 0)
		ieee_ops = &igbe_82576_ieee1588_ops;
	else
		ieee_ops = &ixgbe_82599_ieee1588_ops;
	ports[pi].fwd_ctx = ieee_ops;
	(ieee_ops->ieee1588_start)(pi);
}

static void
port_ieee1588_fwd_end(portid_t pi)
{
	struct port_ieee1588_ops *ieee_ops;

	ieee_ops = (struct port_ieee1588_ops *)ports[pi].fwd_ctx;
	(ieee_ops->ieee1588_stop)(pi);
}

struct fwd_engine ieee1588_fwd_engine = {
	.fwd_mode_name  = "ieee1588",
	.port_fwd_begin = port_ieee1588_fwd_begin,
	.port_fwd_end   = port_ieee1588_fwd_end,
	.packet_fwd     = ieee1588_packet_fwd,
};
