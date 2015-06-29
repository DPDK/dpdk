/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2014-2015 Chelsio Communications.
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
 *     * Neither the name of Chelsio Communications nor the names of its
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

#include <linux/if_ether.h>
#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <netinet/in.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_atomic.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_dev.h>

#include "common.h"
#include "t4_regs.h"
#include "t4_msg.h"
#include "cxgbe.h"

/*
 * Rx buffer sizes for "usembufs" Free List buffers (one ingress packet
 * per mbuf buffer).  We currently only support two sizes for 1500- and
 * 9000-byte MTUs. We could easily support more but there doesn't seem to be
 * much need for that ...
 */
#define FL_MTU_SMALL 1500
#define FL_MTU_LARGE 9000

static inline unsigned int fl_mtu_bufsize(struct adapter *adapter,
					  unsigned int mtu)
{
	struct sge *s = &adapter->sge;

	return ALIGN(s->pktshift + ETH_HLEN + VLAN_HLEN + mtu, s->fl_align);
}

#define FL_MTU_SMALL_BUFSIZE(adapter) fl_mtu_bufsize(adapter, FL_MTU_SMALL)
#define FL_MTU_LARGE_BUFSIZE(adapter) fl_mtu_bufsize(adapter, FL_MTU_LARGE)

/*
 * Bits 0..3 of rx_sw_desc.dma_addr have special meaning.  The hardware uses
 * these to specify the buffer size as an index into the SGE Free List Buffer
 * Size register array.  We also use bit 4, when the buffer has been unmapped
 * for DMA, but this is of course never sent to the hardware and is only used
 * to prevent double unmappings.  All of the above requires that the Free List
 * Buffers which we allocate have the bottom 5 bits free (0) -- i.e. are
 * 32-byte or or a power of 2 greater in alignment.  Since the SGE's minimal
 * Free List Buffer alignment is 32 bytes, this works out for us ...
 */
enum {
	RX_BUF_FLAGS     = 0x1f,   /* bottom five bits are special */
	RX_BUF_SIZE      = 0x0f,   /* bottom three bits are for buf sizes */
	RX_UNMAPPED_BUF  = 0x10,   /* buffer is not mapped */

	/*
	 * XXX We shouldn't depend on being able to use these indices.
	 * XXX Especially when some other Master PF has initialized the
	 * XXX adapter or we use the Firmware Configuration File.  We
	 * XXX should really search through the Host Buffer Size register
	 * XXX array for the appropriately sized buffer indices.
	 */
	RX_SMALL_PG_BUF  = 0x0,   /* small (PAGE_SIZE) page buffer */
	RX_LARGE_PG_BUF  = 0x1,   /* buffer large page buffer */

	RX_SMALL_MTU_BUF = 0x2,   /* small MTU buffer */
	RX_LARGE_MTU_BUF = 0x3,   /* large MTU buffer */
};

/**
 * t4_sge_init - initialize SGE
 * @adap: the adapter
 *
 * Performs SGE initialization needed every time after a chip reset.
 * We do not initialize any of the queues here, instead the driver
 * top-level must request those individually.
 *
 * Called in two different modes:
 *
 *  1. Perform actual hardware initialization and record hard-coded
 *     parameters which were used.  This gets used when we're the
 *     Master PF and the Firmware Configuration File support didn't
 *     work for some reason.
 *
 *  2. We're not the Master PF or initialization was performed with
 *     a Firmware Configuration File.  In this case we need to grab
 *     any of the SGE operating parameters that we need to have in
 *     order to do our job and make sure we can live with them ...
 */
static int t4_sge_init_soft(struct adapter *adap)
{
	struct sge *s = &adap->sge;
	u32 fl_small_pg, fl_large_pg, fl_small_mtu, fl_large_mtu;
	u32 timer_value_0_and_1, timer_value_2_and_3, timer_value_4_and_5;
	u32 ingress_rx_threshold;

	/*
	 * Verify that CPL messages are going to the Ingress Queue for
	 * process_responses() and that only packet data is going to the
	 * Free Lists.
	 */
	if ((t4_read_reg(adap, A_SGE_CONTROL) & F_RXPKTCPLMODE) !=
	    V_RXPKTCPLMODE(X_RXPKTCPLMODE_SPLIT)) {
		dev_err(adap, "bad SGE CPL MODE\n");
		return -EINVAL;
	}

	/*
	 * Validate the Host Buffer Register Array indices that we want to
	 * use ...
	 *
	 * XXX Note that we should really read through the Host Buffer Size
	 * XXX register array and find the indices of the Buffer Sizes which
	 * XXX meet our needs!
	 */
#define READ_FL_BUF(x) \
	t4_read_reg(adap, A_SGE_FL_BUFFER_SIZE0 + (x) * sizeof(u32))

	fl_small_pg = READ_FL_BUF(RX_SMALL_PG_BUF);
	fl_large_pg = READ_FL_BUF(RX_LARGE_PG_BUF);
	fl_small_mtu = READ_FL_BUF(RX_SMALL_MTU_BUF);
	fl_large_mtu = READ_FL_BUF(RX_LARGE_MTU_BUF);

	/*
	 * We only bother using the Large Page logic if the Large Page Buffer
	 * is larger than our Page Size Buffer.
	 */
	if (fl_large_pg <= fl_small_pg)
		fl_large_pg = 0;

#undef READ_FL_BUF

	/*
	 * The Page Size Buffer must be exactly equal to our Page Size and the
	 * Large Page Size Buffer should be 0 (per above) or a power of 2.
	 */
	if (fl_small_pg != PAGE_SIZE ||
	    (fl_large_pg & (fl_large_pg - 1)) != 0) {
		dev_err(adap, "bad SGE FL page buffer sizes [%d, %d]\n",
			fl_small_pg, fl_large_pg);
		return -EINVAL;
	}
	if (fl_large_pg)
		s->fl_pg_order = ilog2(fl_large_pg) - PAGE_SHIFT;

	if (adap->use_unpacked_mode) {
		int err = 0;

		if (fl_small_mtu < FL_MTU_SMALL_BUFSIZE(adap)) {
			dev_err(adap, "bad SGE FL small MTU %d\n",
				fl_small_mtu);
			err = -EINVAL;
		}
		if (fl_large_mtu < FL_MTU_LARGE_BUFSIZE(adap)) {
			dev_err(adap, "bad SGE FL large MTU %d\n",
				fl_large_mtu);
			err = -EINVAL;
		}
		if (err)
			return err;
	}

	/*
	 * Retrieve our RX interrupt holdoff timer values and counter
	 * threshold values from the SGE parameters.
	 */
	timer_value_0_and_1 = t4_read_reg(adap, A_SGE_TIMER_VALUE_0_AND_1);
	timer_value_2_and_3 = t4_read_reg(adap, A_SGE_TIMER_VALUE_2_AND_3);
	timer_value_4_and_5 = t4_read_reg(adap, A_SGE_TIMER_VALUE_4_AND_5);
	s->timer_val[0] = core_ticks_to_us(adap,
					   G_TIMERVALUE0(timer_value_0_and_1));
	s->timer_val[1] = core_ticks_to_us(adap,
					   G_TIMERVALUE1(timer_value_0_and_1));
	s->timer_val[2] = core_ticks_to_us(adap,
					   G_TIMERVALUE2(timer_value_2_and_3));
	s->timer_val[3] = core_ticks_to_us(adap,
					   G_TIMERVALUE3(timer_value_2_and_3));
	s->timer_val[4] = core_ticks_to_us(adap,
					   G_TIMERVALUE4(timer_value_4_and_5));
	s->timer_val[5] = core_ticks_to_us(adap,
					   G_TIMERVALUE5(timer_value_4_and_5));

	ingress_rx_threshold = t4_read_reg(adap, A_SGE_INGRESS_RX_THRESHOLD);
	s->counter_val[0] = G_THRESHOLD_0(ingress_rx_threshold);
	s->counter_val[1] = G_THRESHOLD_1(ingress_rx_threshold);
	s->counter_val[2] = G_THRESHOLD_2(ingress_rx_threshold);
	s->counter_val[3] = G_THRESHOLD_3(ingress_rx_threshold);

	return 0;
}

int t4_sge_init(struct adapter *adap)
{
	struct sge *s = &adap->sge;
	u32 sge_control, sge_control2, sge_conm_ctrl;
	unsigned int ingpadboundary, ingpackboundary;
	int ret, egress_threshold;

	/*
	 * Ingress Padding Boundary and Egress Status Page Size are set up by
	 * t4_fixup_host_params().
	 */
	sge_control = t4_read_reg(adap, A_SGE_CONTROL);
	s->pktshift = G_PKTSHIFT(sge_control);
	s->stat_len = (sge_control & F_EGRSTATUSPAGESIZE) ? 128 : 64;

	/*
	 * T4 uses a single control field to specify both the PCIe Padding and
	 * Packing Boundary.  T5 introduced the ability to specify these
	 * separately.  The actual Ingress Packet Data alignment boundary
	 * within Packed Buffer Mode is the maximum of these two
	 * specifications.
	 */
	ingpadboundary = 1 << (G_INGPADBOUNDARY(sge_control) +
			 X_INGPADBOUNDARY_SHIFT);
	s->fl_align = ingpadboundary;

	if (!is_t4(adap->params.chip) && !adap->use_unpacked_mode) {
		/*
		 * T5 has a weird interpretation of one of the PCIe Packing
		 * Boundary values.  No idea why ...
		 */
		sge_control2 = t4_read_reg(adap, A_SGE_CONTROL2);
		ingpackboundary = G_INGPACKBOUNDARY(sge_control2);
		if (ingpackboundary == X_INGPACKBOUNDARY_16B)
			ingpackboundary = 16;
		else
			ingpackboundary = 1 << (ingpackboundary +
					  X_INGPACKBOUNDARY_SHIFT);

		s->fl_align = max(ingpadboundary, ingpackboundary);
	}

	ret = t4_sge_init_soft(adap);
	if (ret < 0) {
		dev_err(adap, "%s: t4_sge_init_soft failed, error %d\n",
			__func__, -ret);
		return ret;
	}

	/*
	 * A FL with <= fl_starve_thres buffers is starving and a periodic
	 * timer will attempt to refill it.  This needs to be larger than the
	 * SGE's Egress Congestion Threshold.  If it isn't, then we can get
	 * stuck waiting for new packets while the SGE is waiting for us to
	 * give it more Free List entries.  (Note that the SGE's Egress
	 * Congestion Threshold is in units of 2 Free List pointers.)  For T4,
	 * there was only a single field to control this.  For T5 there's the
	 * original field which now only applies to Unpacked Mode Free List
	 * buffers and a new field which only applies to Packed Mode Free List
	 * buffers.
	 */
	sge_conm_ctrl = t4_read_reg(adap, A_SGE_CONM_CTRL);
	if (is_t4(adap->params.chip) || adap->use_unpacked_mode)
		egress_threshold = G_EGRTHRESHOLD(sge_conm_ctrl);
	else
		egress_threshold = G_EGRTHRESHOLDPACKING(sge_conm_ctrl);
	s->fl_starve_thres = 2 * egress_threshold + 1;

	return 0;
}
