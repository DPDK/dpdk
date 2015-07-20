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

#include <netinet/in.h>

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
#include <rte_byteorder.h>

#include "common.h"
#include "t4_regs.h"
#include "t4_regs_values.h"
#include "t4fw_interface.h"

static void init_link_config(struct link_config *lc, unsigned int caps);

/**
 * t4_read_mtu_tbl - returns the values in the HW path MTU table
 * @adap: the adapter
 * @mtus: where to store the MTU values
 * @mtu_log: where to store the MTU base-2 log (may be %NULL)
 *
 * Reads the HW path MTU table.
 */
void t4_read_mtu_tbl(struct adapter *adap, u16 *mtus, u8 *mtu_log)
{
	u32 v;
	int i;

	for (i = 0; i < NMTUS; ++i) {
		t4_write_reg(adap, A_TP_MTU_TABLE,
			     V_MTUINDEX(0xff) | V_MTUVALUE(i));
		v = t4_read_reg(adap, A_TP_MTU_TABLE);
		mtus[i] = G_MTUVALUE(v);
		if (mtu_log)
			mtu_log[i] = G_MTUWIDTH(v);
	}
}

/**
 * t4_tp_wr_bits_indirect - set/clear bits in an indirect TP register
 * @adap: the adapter
 * @addr: the indirect TP register address
 * @mask: specifies the field within the register to modify
 * @val: new value for the field
 *
 * Sets a field of an indirect TP register to the given value.
 */
void t4_tp_wr_bits_indirect(struct adapter *adap, unsigned int addr,
			    unsigned int mask, unsigned int val)
{
	t4_write_reg(adap, A_TP_PIO_ADDR, addr);
	val |= t4_read_reg(adap, A_TP_PIO_DATA) & ~mask;
	t4_write_reg(adap, A_TP_PIO_DATA, val);
}

/* The minimum additive increment value for the congestion control table */
#define CC_MIN_INCR 2U

/**
 * t4_load_mtus - write the MTU and congestion control HW tables
 * @adap: the adapter
 * @mtus: the values for the MTU table
 * @alpha: the values for the congestion control alpha parameter
 * @beta: the values for the congestion control beta parameter
 *
 * Write the HW MTU table with the supplied MTUs and the high-speed
 * congestion control table with the supplied alpha, beta, and MTUs.
 * We write the two tables together because the additive increments
 * depend on the MTUs.
 */
void t4_load_mtus(struct adapter *adap, const unsigned short *mtus,
		  const unsigned short *alpha, const unsigned short *beta)
{
	static const unsigned int avg_pkts[NCCTRL_WIN] = {
		2, 6, 10, 14, 20, 28, 40, 56, 80, 112, 160, 224, 320, 448, 640,
		896, 1281, 1792, 2560, 3584, 5120, 7168, 10240, 14336, 20480,
		28672, 40960, 57344, 81920, 114688, 163840, 229376
	};

	unsigned int i, w;

	for (i = 0; i < NMTUS; ++i) {
		unsigned int mtu = mtus[i];
		unsigned int log2 = cxgbe_fls(mtu);

		if (!(mtu & ((1 << log2) >> 2)))     /* round */
			log2--;
		t4_write_reg(adap, A_TP_MTU_TABLE, V_MTUINDEX(i) |
			     V_MTUWIDTH(log2) | V_MTUVALUE(mtu));

		for (w = 0; w < NCCTRL_WIN; ++w) {
			unsigned int inc;

			inc = max(((mtu - 40) * alpha[w]) / avg_pkts[w],
				  CC_MIN_INCR);

			t4_write_reg(adap, A_TP_CCTRL_TABLE, (i << 21) |
				     (w << 16) | (beta[w] << 13) | inc);
		}
	}
}

/**
 * t4_wait_op_done_val - wait until an operation is completed
 * @adapter: the adapter performing the operation
 * @reg: the register to check for completion
 * @mask: a single-bit field within @reg that indicates completion
 * @polarity: the value of the field when the operation is completed
 * @attempts: number of check iterations
 * @delay: delay in usecs between iterations
 * @valp: where to store the value of the register at completion time
 *
 * Wait until an operation is completed by checking a bit in a register
 * up to @attempts times.  If @valp is not NULL the value of the register
 * at the time it indicated completion is stored there.  Returns 0 if the
 * operation completes and -EAGAIN otherwise.
 */
int t4_wait_op_done_val(struct adapter *adapter, int reg, u32 mask,
			int polarity, int attempts, int delay, u32 *valp)
{
	while (1) {
		u32 val = t4_read_reg(adapter, reg);

		if (!!(val & mask) == polarity) {
			if (valp)
				*valp = val;
			return 0;
		}
		if (--attempts == 0)
			return -EAGAIN;
		if (delay)
			udelay(delay);
	}
}

/**
 * t4_set_reg_field - set a register field to a value
 * @adapter: the adapter to program
 * @addr: the register address
 * @mask: specifies the portion of the register to modify
 * @val: the new value for the register field
 *
 * Sets a register field specified by the supplied mask to the
 * given value.
 */
void t4_set_reg_field(struct adapter *adapter, unsigned int addr, u32 mask,
		      u32 val)
{
	u32 v = t4_read_reg(adapter, addr) & ~mask;

	t4_write_reg(adapter, addr, v | val);
	(void)t4_read_reg(adapter, addr);      /* flush */
}

/**
 * t4_read_indirect - read indirectly addressed registers
 * @adap: the adapter
 * @addr_reg: register holding the indirect address
 * @data_reg: register holding the value of the indirect register
 * @vals: where the read register values are stored
 * @nregs: how many indirect registers to read
 * @start_idx: index of first indirect register to read
 *
 * Reads registers that are accessed indirectly through an address/data
 * register pair.
 */
void t4_read_indirect(struct adapter *adap, unsigned int addr_reg,
		      unsigned int data_reg, u32 *vals, unsigned int nregs,
		      unsigned int start_idx)
{
	while (nregs--) {
		t4_write_reg(adap, addr_reg, start_idx);
		*vals++ = t4_read_reg(adap, data_reg);
		start_idx++;
	}
}

/**
 * t4_write_indirect - write indirectly addressed registers
 * @adap: the adapter
 * @addr_reg: register holding the indirect addresses
 * @data_reg: register holding the value for the indirect registers
 * @vals: values to write
 * @nregs: how many indirect registers to write
 * @start_idx: address of first indirect register to write
 *
 * Writes a sequential block of registers that are accessed indirectly
 * through an address/data register pair.
 */
void t4_write_indirect(struct adapter *adap, unsigned int addr_reg,
		       unsigned int data_reg, const u32 *vals,
		       unsigned int nregs, unsigned int start_idx)
{
	while (nregs--) {
		t4_write_reg(adap, addr_reg, start_idx++);
		t4_write_reg(adap, data_reg, *vals++);
	}
}

/**
 * t4_report_fw_error - report firmware error
 * @adap: the adapter
 *
 * The adapter firmware can indicate error conditions to the host.
 * If the firmware has indicated an error, print out the reason for
 * the firmware error.
 */
static void t4_report_fw_error(struct adapter *adap)
{
	static const char * const reason[] = {
		"Crash",			/* PCIE_FW_EVAL_CRASH */
		"During Device Preparation",	/* PCIE_FW_EVAL_PREP */
		"During Device Configuration",	/* PCIE_FW_EVAL_CONF */
		"During Device Initialization",	/* PCIE_FW_EVAL_INIT */
		"Unexpected Event",	/* PCIE_FW_EVAL_UNEXPECTEDEVENT */
		"Insufficient Airflow",		/* PCIE_FW_EVAL_OVERHEAT */
		"Device Shutdown",	/* PCIE_FW_EVAL_DEVICESHUTDOWN */
		"Reserved",			/* reserved */
	};
	u32 pcie_fw;

	pcie_fw = t4_read_reg(adap, A_PCIE_FW);
	if (pcie_fw & F_PCIE_FW_ERR)
		pr_err("%s: Firmware reports adapter error: %s\n",
		       __func__, reason[G_PCIE_FW_EVAL(pcie_fw)]);
}

/*
 * Get the reply to a mailbox command and store it in @rpl in big-endian order.
 */
static void get_mbox_rpl(struct adapter *adap, __be64 *rpl, int nflit,
			 u32 mbox_addr)
{
	for ( ; nflit; nflit--, mbox_addr += 8)
		*rpl++ = htobe64(t4_read_reg64(adap, mbox_addr));
}

/*
 * Handle a FW assertion reported in a mailbox.
 */
static void fw_asrt(struct adapter *adap, u32 mbox_addr)
{
	struct fw_debug_cmd asrt;

	get_mbox_rpl(adap, (__be64 *)&asrt, sizeof(asrt) / 8, mbox_addr);
	pr_warn("FW assertion at %.16s:%u, val0 %#x, val1 %#x\n",
		asrt.u.assert.filename_0_7, be32_to_cpu(asrt.u.assert.line),
		be32_to_cpu(asrt.u.assert.x), be32_to_cpu(asrt.u.assert.y));
}

#define X_CIM_PF_NOACCESS 0xeeeeeeee

/*
 * If the Host OS Driver needs locking arround accesses to the mailbox, this
 * can be turned on via the T4_OS_NEEDS_MBOX_LOCKING CPP define ...
 */
/* makes single-statement usage a bit cleaner ... */
#ifdef T4_OS_NEEDS_MBOX_LOCKING
#define T4_OS_MBOX_LOCKING(x) x
#else
#define T4_OS_MBOX_LOCKING(x) do {} while (0)
#endif

/**
 * t4_wr_mbox_meat_timeout - send a command to FW through the given mailbox
 * @adap: the adapter
 * @mbox: index of the mailbox to use
 * @cmd: the command to write
 * @size: command length in bytes
 * @rpl: where to optionally store the reply
 * @sleep_ok: if true we may sleep while awaiting command completion
 * @timeout: time to wait for command to finish before timing out
 *	     (negative implies @sleep_ok=false)
 *
 * Sends the given command to FW through the selected mailbox and waits
 * for the FW to execute the command.  If @rpl is not %NULL it is used to
 * store the FW's reply to the command.  The command and its optional
 * reply are of the same length.  Some FW commands like RESET and
 * INITIALIZE can take a considerable amount of time to execute.
 * @sleep_ok determines whether we may sleep while awaiting the response.
 * If sleeping is allowed we use progressive backoff otherwise we spin.
 * Note that passing in a negative @timeout is an alternate mechanism
 * for specifying @sleep_ok=false.  This is useful when a higher level
 * interface allows for specification of @timeout but not @sleep_ok ...
 *
 * Returns 0 on success or a negative errno on failure.  A
 * failure can happen either because we are not able to execute the
 * command or FW executes it but signals an error.  In the latter case
 * the return value is the error code indicated by FW (negated).
 */
int t4_wr_mbox_meat_timeout(struct adapter *adap, int mbox,
			    const void __attribute__((__may_alias__)) *cmd,
			    int size, void *rpl, bool sleep_ok, int timeout)
{
	/*
	 * We delay in small increments at first in an effort to maintain
	 * responsiveness for simple, fast executing commands but then back
	 * off to larger delays to a maximum retry delay.
	 */
	static const int delay[] = {
		1, 1, 3, 5, 10, 10, 20, 50, 100
	};

	u32 v;
	u64 res;
	int i, ms;
	unsigned int delay_idx;
	__be64 *temp = (__be64 *)malloc(size * sizeof(char));
	__be64 *p = temp;
	u32 data_reg = PF_REG(mbox, A_CIM_PF_MAILBOX_DATA);
	u32 ctl_reg = PF_REG(mbox, A_CIM_PF_MAILBOX_CTRL);
	u32 ctl;
	struct mbox_entry entry;
	u32 pcie_fw = 0;

	if ((size & 15) || size > MBOX_LEN) {
		free(temp);
		return -EINVAL;
	}

	bzero(p, size);
	memcpy(p, (const __be64 *)cmd, size);

	/*
	 * If we have a negative timeout, that implies that we can't sleep.
	 */
	if (timeout < 0) {
		sleep_ok = false;
		timeout = -timeout;
	}

#ifdef T4_OS_NEEDS_MBOX_LOCKING
	/*
	 * Queue ourselves onto the mailbox access list.  When our entry is at
	 * the front of the list, we have rights to access the mailbox.  So we
	 * wait [for a while] till we're at the front [or bail out with an
	 * EBUSY] ...
	 */
	t4_os_atomic_add_tail(&entry, &adap->mbox_list, &adap->mbox_lock);

	delay_idx = 0;
	ms = delay[0];

	for (i = 0; ; i += ms) {
		/*
		 * If we've waited too long, return a busy indication.  This
		 * really ought to be based on our initial position in the
		 * mailbox access list but this is a start.  We very rarely
		 * contend on access to the mailbox ...  Also check for a
		 * firmware error which we'll report as a device error.
		 */
		pcie_fw = t4_read_reg(adap, A_PCIE_FW);
		if (i > 4 * timeout || (pcie_fw & F_PCIE_FW_ERR)) {
			t4_os_atomic_list_del(&entry, &adap->mbox_list,
					      &adap->mbox_lock);
			t4_report_fw_error(adap);
			return (pcie_fw & F_PCIE_FW_ERR) ? -ENXIO : -EBUSY;
		}

		/*
		 * If we're at the head, break out and start the mailbox
		 * protocol.
		 */
		if (t4_os_list_first_entry(&adap->mbox_list) == &entry)
			break;

		/*
		 * Delay for a bit before checking again ...
		 */
		if (sleep_ok) {
			ms = delay[delay_idx];  /* last element may repeat */
			if (delay_idx < ARRAY_SIZE(delay) - 1)
				delay_idx++;
			msleep(ms);
		} else {
			rte_delay_ms(ms);
		}
	}
#endif /* T4_OS_NEEDS_MBOX_LOCKING */

	/*
	 * Attempt to gain access to the mailbox.
	 */
	for (i = 0; i < 4; i++) {
		ctl = t4_read_reg(adap, ctl_reg);
		v = G_MBOWNER(ctl);
		if (v != X_MBOWNER_NONE)
			break;
	}

	/*
	 * If we were unable to gain access, dequeue ourselves from the
	 * mailbox atomic access list and report the error to our caller.
	 */
	if (v != X_MBOWNER_PL) {
		T4_OS_MBOX_LOCKING(t4_os_atomic_list_del(&entry,
							 &adap->mbox_list,
							 &adap->mbox_lock));
		t4_report_fw_error(adap);
		return (v == X_MBOWNER_FW ? -EBUSY : -ETIMEDOUT);
	}

	/*
	 * If we gain ownership of the mailbox and there's a "valid" message
	 * in it, this is likely an asynchronous error message from the
	 * firmware.  So we'll report that and then proceed on with attempting
	 * to issue our own command ... which may well fail if the error
	 * presaged the firmware crashing ...
	 */
	if (ctl & F_MBMSGVALID) {
		dev_err(adap, "found VALID command in mbox %u: "
			"%llx %llx %llx %llx %llx %llx %llx %llx\n", mbox,
			(unsigned long long)t4_read_reg64(adap, data_reg),
			(unsigned long long)t4_read_reg64(adap, data_reg + 8),
			(unsigned long long)t4_read_reg64(adap, data_reg + 16),
			(unsigned long long)t4_read_reg64(adap, data_reg + 24),
			(unsigned long long)t4_read_reg64(adap, data_reg + 32),
			(unsigned long long)t4_read_reg64(adap, data_reg + 40),
			(unsigned long long)t4_read_reg64(adap, data_reg + 48),
			(unsigned long long)t4_read_reg64(adap, data_reg + 56));
	}

	/*
	 * Copy in the new mailbox command and send it on its way ...
	 */
	for (i = 0; i < size; i += 8, p++)
		t4_write_reg64(adap, data_reg + i, be64_to_cpu(*p));

	CXGBE_DEBUG_MBOX(adap, "%s: mbox %u: %016llx %016llx %016llx %016llx "
			"%016llx %016llx %016llx %016llx\n", __func__,  (mbox),
			(unsigned long long)t4_read_reg64(adap, data_reg),
			(unsigned long long)t4_read_reg64(adap, data_reg + 8),
			(unsigned long long)t4_read_reg64(adap, data_reg + 16),
			(unsigned long long)t4_read_reg64(adap, data_reg + 24),
			(unsigned long long)t4_read_reg64(adap, data_reg + 32),
			(unsigned long long)t4_read_reg64(adap, data_reg + 40),
			(unsigned long long)t4_read_reg64(adap, data_reg + 48),
			(unsigned long long)t4_read_reg64(adap, data_reg + 56));

	t4_write_reg(adap, ctl_reg, F_MBMSGVALID | V_MBOWNER(X_MBOWNER_FW));
	t4_read_reg(adap, ctl_reg);          /* flush write */

	delay_idx = 0;
	ms = delay[0];

	/*
	 * Loop waiting for the reply; bail out if we time out or the firmware
	 * reports an error.
	 */
	pcie_fw = t4_read_reg(adap, A_PCIE_FW);
	for (i = 0; i < timeout && !(pcie_fw & F_PCIE_FW_ERR); i += ms) {
		if (sleep_ok) {
			ms = delay[delay_idx];  /* last element may repeat */
			if (delay_idx < ARRAY_SIZE(delay) - 1)
				delay_idx++;
			msleep(ms);
		} else {
			msleep(ms);
		}

		pcie_fw = t4_read_reg(adap, A_PCIE_FW);
		v = t4_read_reg(adap, ctl_reg);
		if (v == X_CIM_PF_NOACCESS)
			continue;
		if (G_MBOWNER(v) == X_MBOWNER_PL) {
			if (!(v & F_MBMSGVALID)) {
				t4_write_reg(adap, ctl_reg,
					     V_MBOWNER(X_MBOWNER_NONE));
				continue;
			}

			CXGBE_DEBUG_MBOX(adap,
			"%s: mbox %u: %016llx %016llx %016llx %016llx "
			"%016llx %016llx %016llx %016llx\n", __func__,  (mbox),
			(unsigned long long)t4_read_reg64(adap, data_reg),
			(unsigned long long)t4_read_reg64(adap, data_reg + 8),
			(unsigned long long)t4_read_reg64(adap, data_reg + 16),
			(unsigned long long)t4_read_reg64(adap, data_reg + 24),
			(unsigned long long)t4_read_reg64(adap, data_reg + 32),
			(unsigned long long)t4_read_reg64(adap, data_reg + 40),
			(unsigned long long)t4_read_reg64(adap, data_reg + 48),
			(unsigned long long)t4_read_reg64(adap, data_reg + 56));

			CXGBE_DEBUG_MBOX(adap,
				"command %#x completed in %d ms (%ssleeping)\n",
				*(const u8 *)cmd,
				i + ms, sleep_ok ? "" : "non-");

			res = t4_read_reg64(adap, data_reg);
			if (G_FW_CMD_OP(res >> 32) == FW_DEBUG_CMD) {
				fw_asrt(adap, data_reg);
				res = V_FW_CMD_RETVAL(EIO);
			} else if (rpl) {
				get_mbox_rpl(adap, rpl, size / 8, data_reg);
			}
			t4_write_reg(adap, ctl_reg, V_MBOWNER(X_MBOWNER_NONE));
			T4_OS_MBOX_LOCKING(
				t4_os_atomic_list_del(&entry, &adap->mbox_list,
						      &adap->mbox_lock));
			return -G_FW_CMD_RETVAL((int)res);
		}
	}

	/*
	 * We timed out waiting for a reply to our mailbox command.  Report
	 * the error and also check to see if the firmware reported any
	 * errors ...
	 */
	dev_err(adap, "command %#x in mailbox %d timed out\n",
		*(const u8 *)cmd, mbox);
	T4_OS_MBOX_LOCKING(t4_os_atomic_list_del(&entry,
						 &adap->mbox_list,
						 &adap->mbox_lock));
	t4_report_fw_error(adap);
	free(temp);
	return (pcie_fw & F_PCIE_FW_ERR) ? -ENXIO : -ETIMEDOUT;
}

int t4_wr_mbox_meat(struct adapter *adap, int mbox, const void *cmd, int size,
		    void *rpl, bool sleep_ok)
{
	return t4_wr_mbox_meat_timeout(adap, mbox, cmd, size, rpl, sleep_ok,
				       FW_CMD_MAX_TIMEOUT);
}

/**
 * t4_config_rss_range - configure a portion of the RSS mapping table
 * @adapter: the adapter
 * @mbox: mbox to use for the FW command
 * @viid: virtual interface whose RSS subtable is to be written
 * @start: start entry in the table to write
 * @n: how many table entries to write
 * @rspq: values for the "response queue" (Ingress Queue) lookup table
 * @nrspq: number of values in @rspq
 *
 * Programs the selected part of the VI's RSS mapping table with the
 * provided values.  If @nrspq < @n the supplied values are used repeatedly
 * until the full table range is populated.
 *
 * The caller must ensure the values in @rspq are in the range allowed for
 * @viid.
 */
int t4_config_rss_range(struct adapter *adapter, int mbox, unsigned int viid,
			int start, int n, const u16 *rspq, unsigned int nrspq)
{
	int ret;
	const u16 *rsp = rspq;
	const u16 *rsp_end = rspq + nrspq;
	struct fw_rss_ind_tbl_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_RSS_IND_TBL_CMD) |
				     F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
				     V_FW_RSS_IND_TBL_CMD_VIID(viid));
	cmd.retval_len16 = cpu_to_be32(FW_LEN16(cmd));

	/*
	 * Each firmware RSS command can accommodate up to 32 RSS Ingress
	 * Queue Identifiers.  These Ingress Queue IDs are packed three to
	 * a 32-bit word as 10-bit values with the upper remaining 2 bits
	 * reserved.
	 */
	while (n > 0) {
		int nq = min(n, 32);
		int nq_packed = 0;
		__be32 *qp = &cmd.iq0_to_iq2;

		/*
		 * Set up the firmware RSS command header to send the next
		 * "nq" Ingress Queue IDs to the firmware.
		 */
		cmd.niqid = cpu_to_be16(nq);
		cmd.startidx = cpu_to_be16(start);

		/*
		 * "nq" more done for the start of the next loop.
		 */
		start += nq;
		n -= nq;

		/*
		 * While there are still Ingress Queue IDs to stuff into the
		 * current firmware RSS command, retrieve them from the
		 * Ingress Queue ID array and insert them into the command.
		 */
		while (nq > 0) {
			/*
			 * Grab up to the next 3 Ingress Queue IDs (wrapping
			 * around the Ingress Queue ID array if necessary) and
			 * insert them into the firmware RSS command at the
			 * current 3-tuple position within the commad.
			 */
			u16 qbuf[3];
			u16 *qbp = qbuf;
			int nqbuf = min(3, nq);

			nq -= nqbuf;
			qbuf[0] = 0;
			qbuf[1] = 0;
			qbuf[2] = 0;
			while (nqbuf && nq_packed < 32) {
				nqbuf--;
				nq_packed++;
				*qbp++ = *rsp++;
				if (rsp >= rsp_end)
					rsp = rspq;
			}
			*qp++ = cpu_to_be32(V_FW_RSS_IND_TBL_CMD_IQ0(qbuf[0]) |
					    V_FW_RSS_IND_TBL_CMD_IQ1(qbuf[1]) |
					    V_FW_RSS_IND_TBL_CMD_IQ2(qbuf[2]));
		}

		/*
		 * Send this portion of the RRS table update to the firmware;
		 * bail out on any errors.
		 */
		ret = t4_wr_mbox(adapter, mbox, &cmd, sizeof(cmd), NULL);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * t4_config_vi_rss - configure per VI RSS settings
 * @adapter: the adapter
 * @mbox: mbox to use for the FW command
 * @viid: the VI id
 * @flags: RSS flags
 * @defq: id of the default RSS queue for the VI.
 *
 * Configures VI-specific RSS properties.
 */
int t4_config_vi_rss(struct adapter *adapter, int mbox, unsigned int viid,
		     unsigned int flags, unsigned int defq)
{
	struct fw_rss_vi_config_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_RSS_VI_CONFIG_CMD) |
				   F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
				   V_FW_RSS_VI_CONFIG_CMD_VIID(viid));
	c.retval_len16 = cpu_to_be32(FW_LEN16(c));
	c.u.basicvirtual.defaultq_to_udpen = cpu_to_be32(flags |
			V_FW_RSS_VI_CONFIG_CMD_DEFAULTQ(defq));
	return t4_wr_mbox(adapter, mbox, &c, sizeof(c), NULL);
}

/**
 * init_cong_ctrl - initialize congestion control parameters
 * @a: the alpha values for congestion control
 * @b: the beta values for congestion control
 *
 * Initialize the congestion control parameters.
 */
static void init_cong_ctrl(unsigned short *a, unsigned short *b)
{
	int i;

	for (i = 0; i < 9; i++) {
		a[i] = 1;
		b[i] = 0;
	}

	a[9] = 2;
	a[10] = 3;
	a[11] = 4;
	a[12] = 5;
	a[13] = 6;
	a[14] = 7;
	a[15] = 8;
	a[16] = 9;
	a[17] = 10;
	a[18] = 14;
	a[19] = 17;
	a[20] = 21;
	a[21] = 25;
	a[22] = 30;
	a[23] = 35;
	a[24] = 45;
	a[25] = 60;
	a[26] = 80;
	a[27] = 100;
	a[28] = 200;
	a[29] = 300;
	a[30] = 400;
	a[31] = 500;

	b[9] = 1;
	b[10] = 1;
	b[11] = 2;
	b[12] = 2;
	b[13] = 3;
	b[14] = 3;
	b[15] = 3;
	b[16] = 3;
	b[17] = 4;
	b[18] = 4;
	b[19] = 4;
	b[20] = 4;
	b[21] = 4;
	b[22] = 5;
	b[23] = 5;
	b[24] = 5;
	b[25] = 5;
	b[26] = 5;
	b[27] = 5;
	b[28] = 6;
	b[29] = 6;
	b[30] = 7;
	b[31] = 7;
}

#define INIT_CMD(var, cmd, rd_wr) do { \
	(var).op_to_write = cpu_to_be32(V_FW_CMD_OP(FW_##cmd##_CMD) | \
			F_FW_CMD_REQUEST | F_FW_CMD_##rd_wr); \
	(var).retval_len16 = cpu_to_be32(FW_LEN16(var)); \
} while (0)

int t4_get_core_clock(struct adapter *adapter, struct vpd_params *p)
{
	u32 cclk_param, cclk_val;
	int ret;

	/*
	 * Ask firmware for the Core Clock since it knows how to translate the
	 * Reference Clock ('V2') VPD field into a Core Clock value ...
	 */
	cclk_param = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
		      V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_CCLK));
	ret = t4_query_params(adapter, adapter->mbox, adapter->pf, 0,
			      1, &cclk_param, &cclk_val);
	if (ret) {
		dev_err(adapter, "%s: error in fetching from coreclock - %d\n",
			__func__, ret);
		return ret;
	}

	p->cclk = cclk_val;
	dev_debug(adapter, "%s: p->cclk = %u\n", __func__, p->cclk);
	return 0;
}

/* serial flash and firmware constants and flash config file constants */
enum {
	SF_ATTEMPTS = 10,             /* max retries for SF operations */

	/* flash command opcodes */
	SF_PROG_PAGE    = 2,          /* program page */
	SF_WR_DISABLE   = 4,          /* disable writes */
	SF_RD_STATUS    = 5,          /* read status register */
	SF_WR_ENABLE    = 6,          /* enable writes */
	SF_RD_DATA_FAST = 0xb,        /* read flash */
	SF_RD_ID        = 0x9f,       /* read ID */
	SF_ERASE_SECTOR = 0xd8,       /* erase sector */
};

/**
 * sf1_read - read data from the serial flash
 * @adapter: the adapter
 * @byte_cnt: number of bytes to read
 * @cont: whether another operation will be chained
 * @lock: whether to lock SF for PL access only
 * @valp: where to store the read data
 *
 * Reads up to 4 bytes of data from the serial flash.  The location of
 * the read needs to be specified prior to calling this by issuing the
 * appropriate commands to the serial flash.
 */
static int sf1_read(struct adapter *adapter, unsigned int byte_cnt, int cont,
		    int lock, u32 *valp)
{
	int ret;

	if (!byte_cnt || byte_cnt > 4)
		return -EINVAL;
	if (t4_read_reg(adapter, A_SF_OP) & F_BUSY)
		return -EBUSY;
	t4_write_reg(adapter, A_SF_OP,
		     V_SF_LOCK(lock) | V_CONT(cont) | V_BYTECNT(byte_cnt - 1));
	ret = t4_wait_op_done(adapter, A_SF_OP, F_BUSY, 0, SF_ATTEMPTS, 5);
	if (!ret)
		*valp = t4_read_reg(adapter, A_SF_DATA);
	return ret;
}

/**
 * sf1_write - write data to the serial flash
 * @adapter: the adapter
 * @byte_cnt: number of bytes to write
 * @cont: whether another operation will be chained
 * @lock: whether to lock SF for PL access only
 * @val: value to write
 *
 * Writes up to 4 bytes of data to the serial flash.  The location of
 * the write needs to be specified prior to calling this by issuing the
 * appropriate commands to the serial flash.
 */
static int sf1_write(struct adapter *adapter, unsigned int byte_cnt, int cont,
		     int lock, u32 val)
{
	if (!byte_cnt || byte_cnt > 4)
		return -EINVAL;
	if (t4_read_reg(adapter, A_SF_OP) & F_BUSY)
		return -EBUSY;
	t4_write_reg(adapter, A_SF_DATA, val);
	t4_write_reg(adapter, A_SF_OP, V_SF_LOCK(lock) |
		     V_CONT(cont) | V_BYTECNT(byte_cnt - 1) | V_OP(1));
	return t4_wait_op_done(adapter, A_SF_OP, F_BUSY, 0, SF_ATTEMPTS, 5);
}

/**
 * t4_read_flash - read words from serial flash
 * @adapter: the adapter
 * @addr: the start address for the read
 * @nwords: how many 32-bit words to read
 * @data: where to store the read data
 * @byte_oriented: whether to store data as bytes or as words
 *
 * Read the specified number of 32-bit words from the serial flash.
 * If @byte_oriented is set the read data is stored as a byte array
 * (i.e., big-endian), otherwise as 32-bit words in the platform's
 * natural endianness.
 */
int t4_read_flash(struct adapter *adapter, unsigned int addr,
		  unsigned int nwords, u32 *data, int byte_oriented)
{
	int ret;

	if (((addr + nwords * sizeof(u32)) > adapter->params.sf_size) ||
	    (addr & 3))
		return -EINVAL;

	addr = rte_constant_bswap32(addr) | SF_RD_DATA_FAST;

	ret = sf1_write(adapter, 4, 1, 0, addr);
	if (ret != 0)
		return ret;

	ret = sf1_read(adapter, 1, 1, 0, data);
	if (ret != 0)
		return ret;

	for ( ; nwords; nwords--, data++) {
		ret = sf1_read(adapter, 4, nwords > 1, nwords == 1, data);
		if (nwords == 1)
			t4_write_reg(adapter, A_SF_OP, 0);    /* unlock SF */
		if (ret)
			return ret;
		if (byte_oriented)
			*data = cpu_to_be32(*data);
	}
	return 0;
}

/**
 * t4_get_fw_version - read the firmware version
 * @adapter: the adapter
 * @vers: where to place the version
 *
 * Reads the FW version from flash.
 */
int t4_get_fw_version(struct adapter *adapter, u32 *vers)
{
	return t4_read_flash(adapter, FLASH_FW_START +
			     offsetof(struct fw_hdr, fw_ver), 1, vers, 0);
}

/**
 * t4_get_tp_version - read the TP microcode version
 * @adapter: the adapter
 * @vers: where to place the version
 *
 * Reads the TP microcode version from flash.
 */
int t4_get_tp_version(struct adapter *adapter, u32 *vers)
{
	return t4_read_flash(adapter, FLASH_FW_START +
			     offsetof(struct fw_hdr, tp_microcode_ver),
			     1, vers, 0);
}

#define ADVERT_MASK (FW_PORT_CAP_SPEED_100M | FW_PORT_CAP_SPEED_1G |\
		FW_PORT_CAP_SPEED_10G | FW_PORT_CAP_SPEED_40G | \
		FW_PORT_CAP_SPEED_100G | FW_PORT_CAP_ANEG)

/**
 * t4_link_l1cfg - apply link configuration to MAC/PHY
 * @phy: the PHY to setup
 * @mac: the MAC to setup
 * @lc: the requested link configuration
 *
 * Set up a port's MAC and PHY according to a desired link configuration.
 * - If the PHY can auto-negotiate first decide what to advertise, then
 *   enable/disable auto-negotiation as desired, and reset.
 * - If the PHY does not auto-negotiate just reset it.
 * - If auto-negotiation is off set the MAC to the proper speed/duplex/FC,
 *   otherwise do it later based on the outcome of auto-negotiation.
 */
int t4_link_l1cfg(struct adapter *adap, unsigned int mbox, unsigned int port,
		  struct link_config *lc)
{
	struct fw_port_cmd c;
	unsigned int fc = 0, mdi = V_FW_PORT_CAP_MDI(FW_PORT_CAP_MDI_AUTO);

	lc->link_ok = 0;
	if (lc->requested_fc & PAUSE_RX)
		fc |= FW_PORT_CAP_FC_RX;
	if (lc->requested_fc & PAUSE_TX)
		fc |= FW_PORT_CAP_FC_TX;

	memset(&c, 0, sizeof(c));
	c.op_to_portid = cpu_to_be32(V_FW_CMD_OP(FW_PORT_CMD) |
				     F_FW_CMD_REQUEST | F_FW_CMD_EXEC |
				     V_FW_PORT_CMD_PORTID(port));
	c.action_to_len16 =
		cpu_to_be32(V_FW_PORT_CMD_ACTION(FW_PORT_ACTION_L1_CFG) |
			    FW_LEN16(c));

	if (!(lc->supported & FW_PORT_CAP_ANEG)) {
		c.u.l1cfg.rcap = cpu_to_be32((lc->supported & ADVERT_MASK) |
					     fc);
		lc->fc = lc->requested_fc & (PAUSE_RX | PAUSE_TX);
	} else if (lc->autoneg == AUTONEG_DISABLE) {
		c.u.l1cfg.rcap = cpu_to_be32(lc->requested_speed | fc | mdi);
		lc->fc = lc->requested_fc & (PAUSE_RX | PAUSE_TX);
	} else {
		c.u.l1cfg.rcap = cpu_to_be32(lc->advertising | fc | mdi);
	}

	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 * t4_flash_cfg_addr - return the address of the flash configuration file
 * @adapter: the adapter
 *
 * Return the address within the flash where the Firmware Configuration
 * File is stored, or an error if the device FLASH is too small to contain
 * a Firmware Configuration File.
 */
int t4_flash_cfg_addr(struct adapter *adapter)
{
	/*
	 * If the device FLASH isn't large enough to hold a Firmware
	 * Configuration File, return an error.
	 */
	if (adapter->params.sf_size < FLASH_CFG_START + FLASH_CFG_MAX_SIZE)
		return -ENOSPC;

	return FLASH_CFG_START;
}

#define PF_INTR_MASK (F_PFSW | F_PFCIM)

/**
 * t4_intr_enable - enable interrupts
 * @adapter: the adapter whose interrupts should be enabled
 *
 * Enable PF-specific interrupts for the calling function and the top-level
 * interrupt concentrator for global interrupts.  Interrupts are already
 * enabled at each module, here we just enable the roots of the interrupt
 * hierarchies.
 *
 * Note: this function should be called only when the driver manages
 * non PF-specific interrupts from the various HW modules.  Only one PCI
 * function at a time should be doing this.
 */
void t4_intr_enable(struct adapter *adapter)
{
	u32 val = 0;
	u32 pf = G_SOURCEPF(t4_read_reg(adapter, A_PL_WHOAMI));

	if (CHELSIO_CHIP_VERSION(adapter->params.chip) <= CHELSIO_T5)
		val = F_ERR_DROPPED_DB | F_ERR_EGR_CTXT_PRIO | F_DBFIFO_HP_INT;
	t4_write_reg(adapter, A_SGE_INT_ENABLE3, F_ERR_CPL_EXCEED_IQE_SIZE |
		     F_ERR_INVALID_CIDX_INC | F_ERR_CPL_OPCODE_0 |
		     F_ERR_DATA_CPL_ON_HIGH_QID1 | F_INGRESS_SIZE_ERR |
		     F_ERR_DATA_CPL_ON_HIGH_QID0 | F_ERR_BAD_DB_PIDX3 |
		     F_ERR_BAD_DB_PIDX2 | F_ERR_BAD_DB_PIDX1 |
		     F_ERR_BAD_DB_PIDX0 | F_ERR_ING_CTXT_PRIO |
		     F_DBFIFO_LP_INT | F_EGRESS_SIZE_ERR | val);
	t4_write_reg(adapter, MYPF_REG(A_PL_PF_INT_ENABLE), PF_INTR_MASK);
	t4_set_reg_field(adapter, A_PL_INT_MAP0, 0, 1 << pf);
}

/**
 * t4_intr_disable - disable interrupts
 * @adapter: the adapter whose interrupts should be disabled
 *
 * Disable interrupts.  We only disable the top-level interrupt
 * concentrators.  The caller must be a PCI function managing global
 * interrupts.
 */
void t4_intr_disable(struct adapter *adapter)
{
	u32 pf = G_SOURCEPF(t4_read_reg(adapter, A_PL_WHOAMI));

	t4_write_reg(adapter, MYPF_REG(A_PL_PF_INT_ENABLE), 0);
	t4_set_reg_field(adapter, A_PL_INT_MAP0, 1 << pf, 0);
}

/**
 * t4_get_port_type_description - return Port Type string description
 * @port_type: firmware Port Type enumeration
 */
const char *t4_get_port_type_description(enum fw_port_type port_type)
{
	static const char * const port_type_description[] = {
		"Fiber_XFI",
		"Fiber_XAUI",
		"BT_SGMII",
		"BT_XFI",
		"BT_XAUI",
		"KX4",
		"CX4",
		"KX",
		"KR",
		"SFP",
		"BP_AP",
		"BP4_AP",
		"QSFP_10G",
		"QSA",
		"QSFP",
		"BP40_BA",
	};

	if (port_type < ARRAY_SIZE(port_type_description))
		return port_type_description[port_type];
	return "UNKNOWN";
}

/**
 * t4_get_mps_bg_map - return the buffer groups associated with a port
 * @adap: the adapter
 * @idx: the port index
 *
 * Returns a bitmap indicating which MPS buffer groups are associated
 * with the given port.  Bit i is set if buffer group i is used by the
 * port.
 */
unsigned int t4_get_mps_bg_map(struct adapter *adap, int idx)
{
	u32 n = G_NUMPORTS(t4_read_reg(adap, A_MPS_CMN_CTL));

	if (n == 0)
		return idx == 0 ? 0xf : 0;
	if (n == 1)
		return idx < 2 ? (3 << (2 * idx)) : 0;
	return 1 << idx;
}

/**
 * t4_get_port_stats - collect port statistics
 * @adap: the adapter
 * @idx: the port index
 * @p: the stats structure to fill
 *
 * Collect statistics related to the given port from HW.
 */
void t4_get_port_stats(struct adapter *adap, int idx, struct port_stats *p)
{
	u32 bgmap = t4_get_mps_bg_map(adap, idx);

#define GET_STAT(name) \
	t4_read_reg64(adap, \
		      (is_t4(adap->params.chip) ? \
		       PORT_REG(idx, A_MPS_PORT_STAT_##name##_L) :\
		       T5_PORT_REG(idx, A_MPS_PORT_STAT_##name##_L)))
#define GET_STAT_COM(name) t4_read_reg64(adap, A_MPS_STAT_##name##_L)

	p->tx_octets           = GET_STAT(TX_PORT_BYTES);
	p->tx_frames           = GET_STAT(TX_PORT_FRAMES);
	p->tx_bcast_frames     = GET_STAT(TX_PORT_BCAST);
	p->tx_mcast_frames     = GET_STAT(TX_PORT_MCAST);
	p->tx_ucast_frames     = GET_STAT(TX_PORT_UCAST);
	p->tx_error_frames     = GET_STAT(TX_PORT_ERROR);
	p->tx_frames_64        = GET_STAT(TX_PORT_64B);
	p->tx_frames_65_127    = GET_STAT(TX_PORT_65B_127B);
	p->tx_frames_128_255   = GET_STAT(TX_PORT_128B_255B);
	p->tx_frames_256_511   = GET_STAT(TX_PORT_256B_511B);
	p->tx_frames_512_1023  = GET_STAT(TX_PORT_512B_1023B);
	p->tx_frames_1024_1518 = GET_STAT(TX_PORT_1024B_1518B);
	p->tx_frames_1519_max  = GET_STAT(TX_PORT_1519B_MAX);
	p->tx_drop             = GET_STAT(TX_PORT_DROP);
	p->tx_pause            = GET_STAT(TX_PORT_PAUSE);
	p->tx_ppp0             = GET_STAT(TX_PORT_PPP0);
	p->tx_ppp1             = GET_STAT(TX_PORT_PPP1);
	p->tx_ppp2             = GET_STAT(TX_PORT_PPP2);
	p->tx_ppp3             = GET_STAT(TX_PORT_PPP3);
	p->tx_ppp4             = GET_STAT(TX_PORT_PPP4);
	p->tx_ppp5             = GET_STAT(TX_PORT_PPP5);
	p->tx_ppp6             = GET_STAT(TX_PORT_PPP6);
	p->tx_ppp7             = GET_STAT(TX_PORT_PPP7);

	p->rx_octets           = GET_STAT(RX_PORT_BYTES);
	p->rx_frames           = GET_STAT(RX_PORT_FRAMES);
	p->rx_bcast_frames     = GET_STAT(RX_PORT_BCAST);
	p->rx_mcast_frames     = GET_STAT(RX_PORT_MCAST);
	p->rx_ucast_frames     = GET_STAT(RX_PORT_UCAST);
	p->rx_too_long         = GET_STAT(RX_PORT_MTU_ERROR);
	p->rx_jabber           = GET_STAT(RX_PORT_MTU_CRC_ERROR);
	p->rx_fcs_err          = GET_STAT(RX_PORT_CRC_ERROR);
	p->rx_len_err          = GET_STAT(RX_PORT_LEN_ERROR);
	p->rx_symbol_err       = GET_STAT(RX_PORT_SYM_ERROR);
	p->rx_runt             = GET_STAT(RX_PORT_LESS_64B);
	p->rx_frames_64        = GET_STAT(RX_PORT_64B);
	p->rx_frames_65_127    = GET_STAT(RX_PORT_65B_127B);
	p->rx_frames_128_255   = GET_STAT(RX_PORT_128B_255B);
	p->rx_frames_256_511   = GET_STAT(RX_PORT_256B_511B);
	p->rx_frames_512_1023  = GET_STAT(RX_PORT_512B_1023B);
	p->rx_frames_1024_1518 = GET_STAT(RX_PORT_1024B_1518B);
	p->rx_frames_1519_max  = GET_STAT(RX_PORT_1519B_MAX);
	p->rx_pause            = GET_STAT(RX_PORT_PAUSE);
	p->rx_ppp0             = GET_STAT(RX_PORT_PPP0);
	p->rx_ppp1             = GET_STAT(RX_PORT_PPP1);
	p->rx_ppp2             = GET_STAT(RX_PORT_PPP2);
	p->rx_ppp3             = GET_STAT(RX_PORT_PPP3);
	p->rx_ppp4             = GET_STAT(RX_PORT_PPP4);
	p->rx_ppp5             = GET_STAT(RX_PORT_PPP5);
	p->rx_ppp6             = GET_STAT(RX_PORT_PPP6);
	p->rx_ppp7             = GET_STAT(RX_PORT_PPP7);
	p->rx_ovflow0 = (bgmap & 1) ? GET_STAT_COM(RX_BG_0_MAC_DROP_FRAME) : 0;
	p->rx_ovflow1 = (bgmap & 2) ? GET_STAT_COM(RX_BG_1_MAC_DROP_FRAME) : 0;
	p->rx_ovflow2 = (bgmap & 4) ? GET_STAT_COM(RX_BG_2_MAC_DROP_FRAME) : 0;
	p->rx_ovflow3 = (bgmap & 8) ? GET_STAT_COM(RX_BG_3_MAC_DROP_FRAME) : 0;
	p->rx_trunc0 = (bgmap & 1) ? GET_STAT_COM(RX_BG_0_MAC_TRUNC_FRAME) : 0;
	p->rx_trunc1 = (bgmap & 2) ? GET_STAT_COM(RX_BG_1_MAC_TRUNC_FRAME) : 0;
	p->rx_trunc2 = (bgmap & 4) ? GET_STAT_COM(RX_BG_2_MAC_TRUNC_FRAME) : 0;
	p->rx_trunc3 = (bgmap & 8) ? GET_STAT_COM(RX_BG_3_MAC_TRUNC_FRAME) : 0;

#undef GET_STAT
#undef GET_STAT_COM
}

/**
 * t4_get_port_stats_offset - collect port stats relative to a previous snapshot
 * @adap: The adapter
 * @idx: The port
 * @stats: Current stats to fill
 * @offset: Previous stats snapshot
 */
void t4_get_port_stats_offset(struct adapter *adap, int idx,
			      struct port_stats *stats,
			      struct port_stats *offset)
{
	u64 *s, *o;
	unsigned int i;

	t4_get_port_stats(adap, idx, stats);
	for (i = 0, s = (u64 *)stats, o = (u64 *)offset;
	     i < (sizeof(struct port_stats) / sizeof(u64));
	     i++, s++, o++)
		*s -= *o;
}

/**
 * t4_clr_port_stats - clear port statistics
 * @adap: the adapter
 * @idx: the port index
 *
 * Clear HW statistics for the given port.
 */
void t4_clr_port_stats(struct adapter *adap, int idx)
{
	unsigned int i;
	u32 bgmap = t4_get_mps_bg_map(adap, idx);
	u32 port_base_addr;

	if (is_t4(adap->params.chip))
		port_base_addr = PORT_BASE(idx);
	else
		port_base_addr = T5_PORT_BASE(idx);

	for (i = A_MPS_PORT_STAT_TX_PORT_BYTES_L;
	     i <= A_MPS_PORT_STAT_TX_PORT_PPP7_H; i += 8)
		t4_write_reg(adap, port_base_addr + i, 0);
	for (i = A_MPS_PORT_STAT_RX_PORT_BYTES_L;
	     i <= A_MPS_PORT_STAT_RX_PORT_LESS_64B_H; i += 8)
		t4_write_reg(adap, port_base_addr + i, 0);
	for (i = 0; i < 4; i++)
		if (bgmap & (1 << i)) {
			t4_write_reg(adap,
				     A_MPS_STAT_RX_BG_0_MAC_DROP_FRAME_L +
				     i * 8, 0);
			t4_write_reg(adap,
				     A_MPS_STAT_RX_BG_0_MAC_TRUNC_FRAME_L +
				     i * 8, 0);
		}
}

/**
 * t4_fw_hello - establish communication with FW
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 * @evt_mbox: mailbox to receive async FW events
 * @master: specifies the caller's willingness to be the device master
 * @state: returns the current device state (if non-NULL)
 *
 * Issues a command to establish communication with FW.  Returns either
 * an error (negative integer) or the mailbox of the Master PF.
 */
int t4_fw_hello(struct adapter *adap, unsigned int mbox, unsigned int evt_mbox,
		enum dev_master master, enum dev_state *state)
{
	int ret;
	struct fw_hello_cmd c;
	u32 v;
	unsigned int master_mbox;
	int retries = FW_CMD_HELLO_RETRIES;

retry:
	memset(&c, 0, sizeof(c));
	INIT_CMD(c, HELLO, WRITE);
	c.err_to_clearinit = cpu_to_be32(
			V_FW_HELLO_CMD_MASTERDIS(master == MASTER_CANT) |
			V_FW_HELLO_CMD_MASTERFORCE(master == MASTER_MUST) |
			V_FW_HELLO_CMD_MBMASTER(master == MASTER_MUST ? mbox :
						M_FW_HELLO_CMD_MBMASTER) |
			V_FW_HELLO_CMD_MBASYNCNOT(evt_mbox) |
			V_FW_HELLO_CMD_STAGE(FW_HELLO_CMD_STAGE_OS) |
			F_FW_HELLO_CMD_CLEARINIT);

	/*
	 * Issue the HELLO command to the firmware.  If it's not successful
	 * but indicates that we got a "busy" or "timeout" condition, retry
	 * the HELLO until we exhaust our retry limit.  If we do exceed our
	 * retry limit, check to see if the firmware left us any error
	 * information and report that if so ...
	 */
	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	if (ret != FW_SUCCESS) {
		if ((ret == -EBUSY || ret == -ETIMEDOUT) && retries-- > 0)
			goto retry;
		if (t4_read_reg(adap, A_PCIE_FW) & F_PCIE_FW_ERR)
			t4_report_fw_error(adap);
		return ret;
	}

	v = be32_to_cpu(c.err_to_clearinit);
	master_mbox = G_FW_HELLO_CMD_MBMASTER(v);
	if (state) {
		if (v & F_FW_HELLO_CMD_ERR)
			*state = DEV_STATE_ERR;
		else if (v & F_FW_HELLO_CMD_INIT)
			*state = DEV_STATE_INIT;
		else
			*state = DEV_STATE_UNINIT;
	}

	/*
	 * If we're not the Master PF then we need to wait around for the
	 * Master PF Driver to finish setting up the adapter.
	 *
	 * Note that we also do this wait if we're a non-Master-capable PF and
	 * there is no current Master PF; a Master PF may show up momentarily
	 * and we wouldn't want to fail pointlessly.  (This can happen when an
	 * OS loads lots of different drivers rapidly at the same time).  In
	 * this case, the Master PF returned by the firmware will be
	 * M_PCIE_FW_MASTER so the test below will work ...
	 */
	if ((v & (F_FW_HELLO_CMD_ERR | F_FW_HELLO_CMD_INIT)) == 0 &&
	    master_mbox != mbox) {
		int waiting = FW_CMD_HELLO_TIMEOUT;

		/*
		 * Wait for the firmware to either indicate an error or
		 * initialized state.  If we see either of these we bail out
		 * and report the issue to the caller.  If we exhaust the
		 * "hello timeout" and we haven't exhausted our retries, try
		 * again.  Otherwise bail with a timeout error.
		 */
		for (;;) {
			u32 pcie_fw;

			msleep(50);
			waiting -= 50;

			/*
			 * If neither Error nor Initialialized are indicated
			 * by the firmware keep waiting till we exaust our
			 * timeout ... and then retry if we haven't exhausted
			 * our retries ...
			 */
			pcie_fw = t4_read_reg(adap, A_PCIE_FW);
			if (!(pcie_fw & (F_PCIE_FW_ERR | F_PCIE_FW_INIT))) {
				if (waiting <= 0) {
					if (retries-- > 0)
						goto retry;

					return -ETIMEDOUT;
				}
				continue;
			}

			/*
			 * We either have an Error or Initialized condition
			 * report errors preferentially.
			 */
			if (state) {
				if (pcie_fw & F_PCIE_FW_ERR)
					*state = DEV_STATE_ERR;
				else if (pcie_fw & F_PCIE_FW_INIT)
					*state = DEV_STATE_INIT;
			}

			/*
			 * If we arrived before a Master PF was selected and
			 * there's not a valid Master PF, grab its identity
			 * for our caller.
			 */
			if (master_mbox == M_PCIE_FW_MASTER &&
			    (pcie_fw & F_PCIE_FW_MASTER_VLD))
				master_mbox = G_PCIE_FW_MASTER(pcie_fw);
			break;
		}
	}

	return master_mbox;
}

/**
 * t4_fw_bye - end communication with FW
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 *
 * Issues a command to terminate communication with FW.
 */
int t4_fw_bye(struct adapter *adap, unsigned int mbox)
{
	struct fw_bye_cmd c;

	memset(&c, 0, sizeof(c));
	INIT_CMD(c, BYE, WRITE);
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 * t4_fw_reset - issue a reset to FW
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 * @reset: specifies the type of reset to perform
 *
 * Issues a reset command of the specified type to FW.
 */
int t4_fw_reset(struct adapter *adap, unsigned int mbox, int reset)
{
	struct fw_reset_cmd c;

	memset(&c, 0, sizeof(c));
	INIT_CMD(c, RESET, WRITE);
	c.val = cpu_to_be32(reset);
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 * t4_fw_halt - issue a reset/halt to FW and put uP into RESET
 * @adap: the adapter
 * @mbox: mailbox to use for the FW RESET command (if desired)
 * @force: force uP into RESET even if FW RESET command fails
 *
 * Issues a RESET command to firmware (if desired) with a HALT indication
 * and then puts the microprocessor into RESET state.  The RESET command
 * will only be issued if a legitimate mailbox is provided (mbox <=
 * M_PCIE_FW_MASTER).
 *
 * This is generally used in order for the host to safely manipulate the
 * adapter without fear of conflicting with whatever the firmware might
 * be doing.  The only way out of this state is to RESTART the firmware
 * ...
 */
int t4_fw_halt(struct adapter *adap, unsigned int mbox, int force)
{
	int ret = 0;

	/*
	 * If a legitimate mailbox is provided, issue a RESET command
	 * with a HALT indication.
	 */
	if (mbox <= M_PCIE_FW_MASTER) {
		struct fw_reset_cmd c;

		memset(&c, 0, sizeof(c));
		INIT_CMD(c, RESET, WRITE);
		c.val = cpu_to_be32(F_PIORST | F_PIORSTMODE);
		c.halt_pkd = cpu_to_be32(F_FW_RESET_CMD_HALT);
		ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
	}

	/*
	 * Normally we won't complete the operation if the firmware RESET
	 * command fails but if our caller insists we'll go ahead and put the
	 * uP into RESET.  This can be useful if the firmware is hung or even
	 * missing ...  We'll have to take the risk of putting the uP into
	 * RESET without the cooperation of firmware in that case.
	 *
	 * We also force the firmware's HALT flag to be on in case we bypassed
	 * the firmware RESET command above or we're dealing with old firmware
	 * which doesn't have the HALT capability.  This will serve as a flag
	 * for the incoming firmware to know that it's coming out of a HALT
	 * rather than a RESET ... if it's new enough to understand that ...
	 */
	if (ret == 0 || force) {
		t4_set_reg_field(adap, A_CIM_BOOT_CFG, F_UPCRST, F_UPCRST);
		t4_set_reg_field(adap, A_PCIE_FW, F_PCIE_FW_HALT,
				 F_PCIE_FW_HALT);
	}

	/*
	 * And we always return the result of the firmware RESET command
	 * even when we force the uP into RESET ...
	 */
	return ret;
}

/**
 * t4_fw_restart - restart the firmware by taking the uP out of RESET
 * @adap: the adapter
 * @mbox: mailbox to use for the FW RESET command (if desired)
 * @reset: if we want to do a RESET to restart things
 *
 * Restart firmware previously halted by t4_fw_halt().  On successful
 * return the previous PF Master remains as the new PF Master and there
 * is no need to issue a new HELLO command, etc.
 *
 * We do this in two ways:
 *
 * 1. If we're dealing with newer firmware we'll simply want to take
 *    the chip's microprocessor out of RESET.  This will cause the
 *    firmware to start up from its start vector.  And then we'll loop
 *    until the firmware indicates it's started again (PCIE_FW.HALT
 *    reset to 0) or we timeout.
 *
 * 2. If we're dealing with older firmware then we'll need to RESET
 *    the chip since older firmware won't recognize the PCIE_FW.HALT
 *    flag and automatically RESET itself on startup.
 */
int t4_fw_restart(struct adapter *adap, unsigned int mbox, int reset)
{
	if (reset) {
		/*
		 * Since we're directing the RESET instead of the firmware
		 * doing it automatically, we need to clear the PCIE_FW.HALT
		 * bit.
		 */
		t4_set_reg_field(adap, A_PCIE_FW, F_PCIE_FW_HALT, 0);

		/*
		 * If we've been given a valid mailbox, first try to get the
		 * firmware to do the RESET.  If that works, great and we can
		 * return success.  Otherwise, if we haven't been given a
		 * valid mailbox or the RESET command failed, fall back to
		 * hitting the chip with a hammer.
		 */
		if (mbox <= M_PCIE_FW_MASTER) {
			t4_set_reg_field(adap, A_CIM_BOOT_CFG, F_UPCRST, 0);
			msleep(100);
			if (t4_fw_reset(adap, mbox,
					F_PIORST | F_PIORSTMODE) == 0)
				return 0;
		}

		t4_write_reg(adap, A_PL_RST, F_PIORST | F_PIORSTMODE);
		msleep(2000);
	} else {
		int ms;

		t4_set_reg_field(adap, A_CIM_BOOT_CFG, F_UPCRST, 0);
		for (ms = 0; ms < FW_CMD_MAX_TIMEOUT; ) {
			if (!(t4_read_reg(adap, A_PCIE_FW) & F_PCIE_FW_HALT))
				return FW_SUCCESS;
			msleep(100);
			ms += 100;
		}
		return -ETIMEDOUT;
	}
	return 0;
}

/**
 * t4_fixup_host_params_compat - fix up host-dependent parameters
 * @adap: the adapter
 * @page_size: the host's Base Page Size
 * @cache_line_size: the host's Cache Line Size
 * @chip_compat: maintain compatibility with designated chip
 *
 * Various registers in the chip contain values which are dependent on the
 * host's Base Page and Cache Line Sizes.  This function will fix all of
 * those registers with the appropriate values as passed in ...
 *
 * @chip_compat is used to limit the set of changes that are made
 * to be compatible with the indicated chip release.  This is used by
 * drivers to maintain compatibility with chip register settings when
 * the drivers haven't [yet] been updated with new chip support.
 */
int t4_fixup_host_params_compat(struct adapter *adap,
				unsigned int page_size,
				unsigned int cache_line_size,
				enum chip_type chip_compat)
{
	unsigned int page_shift = cxgbe_fls(page_size) - 1;
	unsigned int sge_hps = page_shift - 10;
	unsigned int stat_len = cache_line_size > 64 ? 128 : 64;
	unsigned int fl_align = cache_line_size < 32 ? 32 : cache_line_size;
	unsigned int fl_align_log = cxgbe_fls(fl_align) - 1;

	t4_write_reg(adap, A_SGE_HOST_PAGE_SIZE,
		     V_HOSTPAGESIZEPF0(sge_hps) |
		     V_HOSTPAGESIZEPF1(sge_hps) |
		     V_HOSTPAGESIZEPF2(sge_hps) |
		     V_HOSTPAGESIZEPF3(sge_hps) |
		     V_HOSTPAGESIZEPF4(sge_hps) |
		     V_HOSTPAGESIZEPF5(sge_hps) |
		     V_HOSTPAGESIZEPF6(sge_hps) |
		     V_HOSTPAGESIZEPF7(sge_hps));

	if (is_t4(adap->params.chip) || is_t4(chip_compat))
		t4_set_reg_field(adap, A_SGE_CONTROL,
				 V_INGPADBOUNDARY(M_INGPADBOUNDARY) |
				 F_EGRSTATUSPAGESIZE,
				 V_INGPADBOUNDARY(fl_align_log -
						  X_INGPADBOUNDARY_SHIFT) |
				V_EGRSTATUSPAGESIZE(stat_len != 64));
	else {
		/*
		 * T5 introduced the separation of the Free List Padding and
		 * Packing Boundaries.  Thus, we can select a smaller Padding
		 * Boundary to avoid uselessly chewing up PCIe Link and Memory
		 * Bandwidth, and use a Packing Boundary which is large enough
		 * to avoid false sharing between CPUs, etc.
		 *
		 * For the PCI Link, the smaller the Padding Boundary the
		 * better.  For the Memory Controller, a smaller Padding
		 * Boundary is better until we cross under the Memory Line
		 * Size (the minimum unit of transfer to/from Memory).  If we
		 * have a Padding Boundary which is smaller than the Memory
		 * Line Size, that'll involve a Read-Modify-Write cycle on the
		 * Memory Controller which is never good.  For T5 the smallest
		 * Padding Boundary which we can select is 32 bytes which is
		 * larger than any known Memory Controller Line Size so we'll
		 * use that.
		 */

		/*
		 * N.B. T5 has a different interpretation of the "0" value for
		 * the Packing Boundary.  This corresponds to 16 bytes instead
		 * of the expected 32 bytes.  We never have a Packing Boundary
		 * less than 32 bytes so we can't use that special value but
		 * on the other hand, if we wanted 32 bytes, the best we can
		 * really do is 64 bytes ...
		 */
		if (fl_align <= 32) {
			fl_align = 64;
			fl_align_log = 6;
		}
		t4_set_reg_field(adap, A_SGE_CONTROL,
				 V_INGPADBOUNDARY(M_INGPADBOUNDARY) |
				 F_EGRSTATUSPAGESIZE,
				 V_INGPADBOUNDARY(X_INGPCIEBOUNDARY_32B) |
				 V_EGRSTATUSPAGESIZE(stat_len != 64));
		t4_set_reg_field(adap, A_SGE_CONTROL2,
				 V_INGPACKBOUNDARY(M_INGPACKBOUNDARY),
				 V_INGPACKBOUNDARY(fl_align_log -
						   X_INGPACKBOUNDARY_SHIFT));
	}

	/*
	 * Adjust various SGE Free List Host Buffer Sizes.
	 *
	 * The first four entries are:
	 *
	 *   0: Host Page Size
	 *   1: 64KB
	 *   2: Buffer size corresponding to 1500 byte MTU (unpacked mode)
	 *   3: Buffer size corresponding to 9000 byte MTU (unpacked mode)
	 *
	 * For the single-MTU buffers in unpacked mode we need to include
	 * space for the SGE Control Packet Shift, 14 byte Ethernet header,
	 * possible 4 byte VLAN tag, all rounded up to the next Ingress Packet
	 * Padding boundary.  All of these are accommodated in the Factory
	 * Default Firmware Configuration File but we need to adjust it for
	 * this host's cache line size.
	 */
	t4_write_reg(adap, A_SGE_FL_BUFFER_SIZE0, page_size);
	t4_write_reg(adap, A_SGE_FL_BUFFER_SIZE2,
		     (t4_read_reg(adap, A_SGE_FL_BUFFER_SIZE2) + fl_align - 1)
		     & ~(fl_align - 1));
	t4_write_reg(adap, A_SGE_FL_BUFFER_SIZE3,
		     (t4_read_reg(adap, A_SGE_FL_BUFFER_SIZE3) + fl_align - 1)
		     & ~(fl_align - 1));

	t4_write_reg(adap, A_ULP_RX_TDDP_PSZ, V_HPZ0(page_shift - 12));

	return 0;
}

/**
 * t4_fixup_host_params - fix up host-dependent parameters (T4 compatible)
 * @adap: the adapter
 * @page_size: the host's Base Page Size
 * @cache_line_size: the host's Cache Line Size
 *
 * Various registers in T4 contain values which are dependent on the
 * host's Base Page and Cache Line Sizes.  This function will fix all of
 * those registers with the appropriate values as passed in ...
 *
 * This routine makes changes which are compatible with T4 chips.
 */
int t4_fixup_host_params(struct adapter *adap, unsigned int page_size,
			 unsigned int cache_line_size)
{
	return t4_fixup_host_params_compat(adap, page_size, cache_line_size,
					   T4_LAST_REV);
}

/**
 * t4_fw_initialize - ask FW to initialize the device
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 *
 * Issues a command to FW to partially initialize the device.  This
 * performs initialization that generally doesn't depend on user input.
 */
int t4_fw_initialize(struct adapter *adap, unsigned int mbox)
{
	struct fw_initialize_cmd c;

	memset(&c, 0, sizeof(c));
	INIT_CMD(c, INITIALIZE, WRITE);
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 * t4_query_params_rw - query FW or device parameters
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 * @pf: the PF
 * @vf: the VF
 * @nparams: the number of parameters
 * @params: the parameter names
 * @val: the parameter values
 * @rw: Write and read flag
 *
 * Reads the value of FW or device parameters.  Up to 7 parameters can be
 * queried at once.
 */
static int t4_query_params_rw(struct adapter *adap, unsigned int mbox,
			      unsigned int pf, unsigned int vf,
			      unsigned int nparams, const u32 *params,
			      u32 *val, int rw)
{
	unsigned int i;
	int ret;
	struct fw_params_cmd c;
	__be32 *p = &c.param[0].mnem;

	if (nparams > 7)
		return -EINVAL;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_PARAMS_CMD) |
				  F_FW_CMD_REQUEST | F_FW_CMD_READ |
				  V_FW_PARAMS_CMD_PFN(pf) |
				  V_FW_PARAMS_CMD_VFN(vf));
	c.retval_len16 = cpu_to_be32(FW_LEN16(c));

	for (i = 0; i < nparams; i++) {
		*p++ = cpu_to_be32(*params++);
		if (rw)
			*p = cpu_to_be32(*(val + i));
		p++;
	}

	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	if (ret == 0)
		for (i = 0, p = &c.param[0].val; i < nparams; i++, p += 2)
			*val++ = be32_to_cpu(*p);
	return ret;
}

int t4_query_params(struct adapter *adap, unsigned int mbox, unsigned int pf,
		    unsigned int vf, unsigned int nparams, const u32 *params,
		    u32 *val)
{
	return t4_query_params_rw(adap, mbox, pf, vf, nparams, params, val, 0);
}

/**
 * t4_set_params_timeout - sets FW or device parameters
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 * @pf: the PF
 * @vf: the VF
 * @nparams: the number of parameters
 * @params: the parameter names
 * @val: the parameter values
 * @timeout: the timeout time
 *
 * Sets the value of FW or device parameters.  Up to 7 parameters can be
 * specified at once.
 */
int t4_set_params_timeout(struct adapter *adap, unsigned int mbox,
			  unsigned int pf, unsigned int vf,
			  unsigned int nparams, const u32 *params,
			  const u32 *val, int timeout)
{
	struct fw_params_cmd c;
	__be32 *p = &c.param[0].mnem;

	if (nparams > 7)
		return -EINVAL;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_PARAMS_CMD) |
				  F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
				  V_FW_PARAMS_CMD_PFN(pf) |
				  V_FW_PARAMS_CMD_VFN(vf));
	c.retval_len16 = cpu_to_be32(FW_LEN16(c));

	while (nparams--) {
		*p++ = cpu_to_be32(*params++);
		*p++ = cpu_to_be32(*val++);
	}

	return t4_wr_mbox_timeout(adap, mbox, &c, sizeof(c), NULL, timeout);
}

int t4_set_params(struct adapter *adap, unsigned int mbox, unsigned int pf,
		  unsigned int vf, unsigned int nparams, const u32 *params,
		  const u32 *val)
{
	return t4_set_params_timeout(adap, mbox, pf, vf, nparams, params, val,
				     FW_CMD_MAX_TIMEOUT);
}

/**
 * t4_alloc_vi_func - allocate a virtual interface
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 * @port: physical port associated with the VI
 * @pf: the PF owning the VI
 * @vf: the VF owning the VI
 * @nmac: number of MAC addresses needed (1 to 5)
 * @mac: the MAC addresses of the VI
 * @rss_size: size of RSS table slice associated with this VI
 * @portfunc: which Port Application Function MAC Address is desired
 * @idstype: Intrusion Detection Type
 *
 * Allocates a virtual interface for the given physical port.  If @mac is
 * not %NULL it contains the MAC addresses of the VI as assigned by FW.
 * @mac should be large enough to hold @nmac Ethernet addresses, they are
 * stored consecutively so the space needed is @nmac * 6 bytes.
 * Returns a negative error number or the non-negative VI id.
 */
int t4_alloc_vi_func(struct adapter *adap, unsigned int mbox,
		     unsigned int port, unsigned int pf, unsigned int vf,
		     unsigned int nmac, u8 *mac, unsigned int *rss_size,
		     unsigned int portfunc, unsigned int idstype)
{
	int ret;
	struct fw_vi_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_VI_CMD) | F_FW_CMD_REQUEST |
				  F_FW_CMD_WRITE | F_FW_CMD_EXEC |
				  V_FW_VI_CMD_PFN(pf) | V_FW_VI_CMD_VFN(vf));
	c.alloc_to_len16 = cpu_to_be32(F_FW_VI_CMD_ALLOC | FW_LEN16(c));
	c.type_to_viid = cpu_to_be16(V_FW_VI_CMD_TYPE(idstype) |
				     V_FW_VI_CMD_FUNC(portfunc));
	c.portid_pkd = V_FW_VI_CMD_PORTID(port);
	c.nmac = nmac - 1;

	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	if (ret)
		return ret;

	if (mac) {
		memcpy(mac, c.mac, sizeof(c.mac));
		switch (nmac) {
		case 5:
			memcpy(mac + 24, c.nmac3, sizeof(c.nmac3));
			/* FALLTHROUGH */
		case 4:
			memcpy(mac + 18, c.nmac2, sizeof(c.nmac2));
			/* FALLTHROUGH */
		case 3:
			memcpy(mac + 12, c.nmac1, sizeof(c.nmac1));
			/* FALLTHROUGH */
		case 2:
			memcpy(mac + 6,  c.nmac0, sizeof(c.nmac0));
			/* FALLTHROUGH */
		}
	}
	if (rss_size)
		*rss_size = G_FW_VI_CMD_RSSSIZE(be16_to_cpu(c.norss_rsssize));
	return G_FW_VI_CMD_VIID(cpu_to_be16(c.type_to_viid));
}

/**
 * t4_alloc_vi - allocate an [Ethernet Function] virtual interface
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 * @port: physical port associated with the VI
 * @pf: the PF owning the VI
 * @vf: the VF owning the VI
 * @nmac: number of MAC addresses needed (1 to 5)
 * @mac: the MAC addresses of the VI
 * @rss_size: size of RSS table slice associated with this VI
 *
 * Backwards compatible and convieniance routine to allocate a Virtual
 * Interface with a Ethernet Port Application Function and Intrustion
 * Detection System disabled.
 */
int t4_alloc_vi(struct adapter *adap, unsigned int mbox, unsigned int port,
		unsigned int pf, unsigned int vf, unsigned int nmac, u8 *mac,
		unsigned int *rss_size)
{
	return t4_alloc_vi_func(adap, mbox, port, pf, vf, nmac, mac, rss_size,
				FW_VI_FUNC_ETH, 0);
}

/**
 * t4_free_vi - free a virtual interface
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 * @pf: the PF owning the VI
 * @vf: the VF owning the VI
 * @viid: virtual interface identifiler
 *
 * Free a previously allocated virtual interface.
 */
int t4_free_vi(struct adapter *adap, unsigned int mbox, unsigned int pf,
	       unsigned int vf, unsigned int viid)
{
	struct fw_vi_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_VI_CMD) | F_FW_CMD_REQUEST |
				  F_FW_CMD_EXEC | V_FW_VI_CMD_PFN(pf) |
				  V_FW_VI_CMD_VFN(vf));
	c.alloc_to_len16 = cpu_to_be32(F_FW_VI_CMD_FREE | FW_LEN16(c));
	c.type_to_viid = cpu_to_be16(V_FW_VI_CMD_VIID(viid));

	return t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
}

/**
 * t4_set_rxmode - set Rx properties of a virtual interface
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 * @viid: the VI id
 * @mtu: the new MTU or -1
 * @promisc: 1 to enable promiscuous mode, 0 to disable it, -1 no change
 * @all_multi: 1 to enable all-multi mode, 0 to disable it, -1 no change
 * @bcast: 1 to enable broadcast Rx, 0 to disable it, -1 no change
 * @vlanex: 1 to enable hardware VLAN Tag extraction, 0 to disable it,
 *          -1 no change
 * @sleep_ok: if true we may sleep while awaiting command completion
 *
 * Sets Rx properties of a virtual interface.
 */
int t4_set_rxmode(struct adapter *adap, unsigned int mbox, unsigned int viid,
		  int mtu, int promisc, int all_multi, int bcast, int vlanex,
		  bool sleep_ok)
{
	struct fw_vi_rxmode_cmd c;

	/* convert to FW values */
	if (mtu < 0)
		mtu = M_FW_VI_RXMODE_CMD_MTU;
	if (promisc < 0)
		promisc = M_FW_VI_RXMODE_CMD_PROMISCEN;
	if (all_multi < 0)
		all_multi = M_FW_VI_RXMODE_CMD_ALLMULTIEN;
	if (bcast < 0)
		bcast = M_FW_VI_RXMODE_CMD_BROADCASTEN;
	if (vlanex < 0)
		vlanex = M_FW_VI_RXMODE_CMD_VLANEXEN;

	memset(&c, 0, sizeof(c));
	c.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_VI_RXMODE_CMD) |
				   F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
				   V_FW_VI_RXMODE_CMD_VIID(viid));
	c.retval_len16 = cpu_to_be32(FW_LEN16(c));
	c.mtu_to_vlanexen = cpu_to_be32(V_FW_VI_RXMODE_CMD_MTU(mtu) |
			    V_FW_VI_RXMODE_CMD_PROMISCEN(promisc) |
			    V_FW_VI_RXMODE_CMD_ALLMULTIEN(all_multi) |
			    V_FW_VI_RXMODE_CMD_BROADCASTEN(bcast) |
			    V_FW_VI_RXMODE_CMD_VLANEXEN(vlanex));
	return t4_wr_mbox_meat(adap, mbox, &c, sizeof(c), NULL, sleep_ok);
}

/**
 * t4_change_mac - modifies the exact-match filter for a MAC address
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 * @viid: the VI id
 * @idx: index of existing filter for old value of MAC address, or -1
 * @addr: the new MAC address value
 * @persist: whether a new MAC allocation should be persistent
 * @add_smt: if true also add the address to the HW SMT
 *
 * Modifies an exact-match filter and sets it to the new MAC address if
 * @idx >= 0, or adds the MAC address to a new filter if @idx < 0.  In the
 * latter case the address is added persistently if @persist is %true.
 *
 * Note that in general it is not possible to modify the value of a given
 * filter so the generic way to modify an address filter is to free the one
 * being used by the old address value and allocate a new filter for the
 * new address value.
 *
 * Returns a negative error number or the index of the filter with the new
 * MAC value.  Note that this index may differ from @idx.
 */
int t4_change_mac(struct adapter *adap, unsigned int mbox, unsigned int viid,
		  int idx, const u8 *addr, bool persist, bool add_smt)
{
	int ret, mode;
	struct fw_vi_mac_cmd c;
	struct fw_vi_mac_exact *p = c.u.exact;
	int max_mac_addr = adap->params.arch.mps_tcam_size;

	if (idx < 0)                             /* new allocation */
		idx = persist ? FW_VI_MAC_ADD_PERSIST_MAC : FW_VI_MAC_ADD_MAC;
	mode = add_smt ? FW_VI_MAC_SMT_AND_MPSTCAM : FW_VI_MAC_MPS_TCAM_ENTRY;

	memset(&c, 0, sizeof(c));
	c.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_VI_MAC_CMD) |
				   F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
				   V_FW_VI_MAC_CMD_VIID(viid));
	c.freemacs_to_len16 = cpu_to_be32(V_FW_CMD_LEN16(1));
	p->valid_to_idx = cpu_to_be16(F_FW_VI_MAC_CMD_VALID |
				      V_FW_VI_MAC_CMD_SMAC_RESULT(mode) |
				      V_FW_VI_MAC_CMD_IDX(idx));
	memcpy(p->macaddr, addr, sizeof(p->macaddr));

	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	if (ret == 0) {
		ret = G_FW_VI_MAC_CMD_IDX(be16_to_cpu(p->valid_to_idx));
		if (ret >= max_mac_addr)
			ret = -ENOMEM;
	}
	return ret;
}

/**
 * t4_enable_vi_params - enable/disable a virtual interface
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 * @viid: the VI id
 * @rx_en: 1=enable Rx, 0=disable Rx
 * @tx_en: 1=enable Tx, 0=disable Tx
 * @dcb_en: 1=enable delivery of Data Center Bridging messages.
 *
 * Enables/disables a virtual interface.  Note that setting DCB Enable
 * only makes sense when enabling a Virtual Interface ...
 */
int t4_enable_vi_params(struct adapter *adap, unsigned int mbox,
			unsigned int viid, bool rx_en, bool tx_en, bool dcb_en)
{
	struct fw_vi_enable_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_VI_ENABLE_CMD) |
				   F_FW_CMD_REQUEST | F_FW_CMD_EXEC |
				   V_FW_VI_ENABLE_CMD_VIID(viid));
	c.ien_to_len16 = cpu_to_be32(V_FW_VI_ENABLE_CMD_IEN(rx_en) |
				     V_FW_VI_ENABLE_CMD_EEN(tx_en) |
				     V_FW_VI_ENABLE_CMD_DCB_INFO(dcb_en) |
				     FW_LEN16(c));
	return t4_wr_mbox_ns(adap, mbox, &c, sizeof(c), NULL);
}

/**
 * t4_enable_vi - enable/disable a virtual interface
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 * @viid: the VI id
 * @rx_en: 1=enable Rx, 0=disable Rx
 * @tx_en: 1=enable Tx, 0=disable Tx
 *
 * Enables/disables a virtual interface.  Note that setting DCB Enable
 * only makes sense when enabling a Virtual Interface ...
 */
int t4_enable_vi(struct adapter *adap, unsigned int mbox, unsigned int viid,
		 bool rx_en, bool tx_en)
{
	return t4_enable_vi_params(adap, mbox, viid, rx_en, tx_en, 0);
}

/**
 * t4_iq_start_stop - enable/disable an ingress queue and its FLs
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 * @start: %true to enable the queues, %false to disable them
 * @pf: the PF owning the queues
 * @vf: the VF owning the queues
 * @iqid: ingress queue id
 * @fl0id: FL0 queue id or 0xffff if no attached FL0
 * @fl1id: FL1 queue id or 0xffff if no attached FL1
 *
 * Starts or stops an ingress queue and its associated FLs, if any.
 */
int t4_iq_start_stop(struct adapter *adap, unsigned int mbox, bool start,
		     unsigned int pf, unsigned int vf, unsigned int iqid,
		     unsigned int fl0id, unsigned int fl1id)
{
	struct fw_iq_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_IQ_CMD) | F_FW_CMD_REQUEST |
				  F_FW_CMD_EXEC | V_FW_IQ_CMD_PFN(pf) |
				  V_FW_IQ_CMD_VFN(vf));
	c.alloc_to_len16 = cpu_to_be32(V_FW_IQ_CMD_IQSTART(start) |
				       V_FW_IQ_CMD_IQSTOP(!start) |
				       FW_LEN16(c));
	c.iqid = cpu_to_be16(iqid);
	c.fl0id = cpu_to_be16(fl0id);
	c.fl1id = cpu_to_be16(fl1id);
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 * t4_iq_free - free an ingress queue and its FLs
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 * @pf: the PF owning the queues
 * @vf: the VF owning the queues
 * @iqtype: the ingress queue type (FW_IQ_TYPE_FL_INT_CAP, etc.)
 * @iqid: ingress queue id
 * @fl0id: FL0 queue id or 0xffff if no attached FL0
 * @fl1id: FL1 queue id or 0xffff if no attached FL1
 *
 * Frees an ingress queue and its associated FLs, if any.
 */
int t4_iq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
	       unsigned int vf, unsigned int iqtype, unsigned int iqid,
	       unsigned int fl0id, unsigned int fl1id)
{
	struct fw_iq_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_IQ_CMD) | F_FW_CMD_REQUEST |
				  F_FW_CMD_EXEC | V_FW_IQ_CMD_PFN(pf) |
				  V_FW_IQ_CMD_VFN(vf));
	c.alloc_to_len16 = cpu_to_be32(F_FW_IQ_CMD_FREE | FW_LEN16(c));
	c.type_to_iqandstindex = cpu_to_be32(V_FW_IQ_CMD_TYPE(iqtype));
	c.iqid = cpu_to_be16(iqid);
	c.fl0id = cpu_to_be16(fl0id);
	c.fl1id = cpu_to_be16(fl1id);
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 * t4_eth_eq_free - free an Ethernet egress queue
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 * @pf: the PF owning the queue
 * @vf: the VF owning the queue
 * @eqid: egress queue id
 *
 * Frees an Ethernet egress queue.
 */
int t4_eth_eq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
		   unsigned int vf, unsigned int eqid)
{
	struct fw_eq_eth_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_EQ_ETH_CMD) |
				  F_FW_CMD_REQUEST | F_FW_CMD_EXEC |
				  V_FW_EQ_ETH_CMD_PFN(pf) |
				  V_FW_EQ_ETH_CMD_VFN(vf));
	c.alloc_to_len16 = cpu_to_be32(F_FW_EQ_ETH_CMD_FREE | FW_LEN16(c));
	c.eqid_pkd = cpu_to_be32(V_FW_EQ_ETH_CMD_EQID(eqid));
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 * t4_handle_fw_rpl - process a FW reply message
 * @adap: the adapter
 * @rpl: start of the FW message
 *
 * Processes a FW message, such as link state change messages.
 */
int t4_handle_fw_rpl(struct adapter *adap, const __be64 *rpl)
{
	u8 opcode = *(const u8 *)rpl;

	/*
	 * This might be a port command ... this simplifies the following
	 * conditionals ...  We can get away with pre-dereferencing
	 * action_to_len16 because it's in the first 16 bytes and all messages
	 * will be at least that long.
	 */
	const struct fw_port_cmd *p = (const void *)rpl;
	unsigned int action =
		G_FW_PORT_CMD_ACTION(be32_to_cpu(p->action_to_len16));

	if (opcode == FW_PORT_CMD && action == FW_PORT_ACTION_GET_PORT_INFO) {
		/* link/module state change message */
		int speed = 0, fc = 0, i;
		int chan = G_FW_PORT_CMD_PORTID(be32_to_cpu(p->op_to_portid));
		struct port_info *pi = NULL;
		struct link_config *lc;
		u32 stat = be32_to_cpu(p->u.info.lstatus_to_modtype);
		int link_ok = (stat & F_FW_PORT_CMD_LSTATUS) != 0;
		u32 mod = G_FW_PORT_CMD_MODTYPE(stat);

		if (stat & F_FW_PORT_CMD_RXPAUSE)
			fc |= PAUSE_RX;
		if (stat & F_FW_PORT_CMD_TXPAUSE)
			fc |= PAUSE_TX;
		if (stat & V_FW_PORT_CMD_LSPEED(FW_PORT_CAP_SPEED_100M))
			speed = ETH_LINK_SPEED_100;
		else if (stat & V_FW_PORT_CMD_LSPEED(FW_PORT_CAP_SPEED_1G))
			speed = ETH_LINK_SPEED_1000;
		else if (stat & V_FW_PORT_CMD_LSPEED(FW_PORT_CAP_SPEED_10G))
			speed = ETH_LINK_SPEED_10000;
		else if (stat & V_FW_PORT_CMD_LSPEED(FW_PORT_CAP_SPEED_40G))
			speed = ETH_LINK_SPEED_40G;

		for_each_port(adap, i) {
			pi = adap2pinfo(adap, i);
			if (pi->tx_chan == chan)
				break;
		}
		lc = &pi->link_cfg;

		if (mod != pi->mod_type) {
			pi->mod_type = mod;
			t4_os_portmod_changed(adap, i);
		}
		if (link_ok != lc->link_ok || speed != lc->speed ||
		    fc != lc->fc) {                    /* something changed */
			if (!link_ok && lc->link_ok) {
				static const char * const reason[] = {
					"Link Down",
					"Remote Fault",
					"Auto-negotiation Failure",
					"Reserved",
					"Insufficient Airflow",
					"Unable To Determine Reason",
					"No RX Signal Detected",
					"Reserved",
				};
				unsigned int rc = G_FW_PORT_CMD_LINKDNRC(stat);

				dev_warn(adap, "Port %d link down, reason: %s\n",
					 chan, reason[rc]);
			}
			lc->link_ok = link_ok;
			lc->speed = speed;
			lc->fc = fc;
			lc->supported = be16_to_cpu(p->u.info.pcap);
		}
	} else {
		dev_warn(adap, "Unknown firmware reply %d\n", opcode);
		return -EINVAL;
	}
	return 0;
}

void t4_reset_link_config(struct adapter *adap, int idx)
{
	struct port_info *pi = adap2pinfo(adap, idx);
	struct link_config *lc = &pi->link_cfg;

	lc->link_ok = 0;
	lc->requested_speed = 0;
	lc->requested_fc = 0;
	lc->speed = 0;
	lc->fc = 0;
}

/**
 * init_link_config - initialize a link's SW state
 * @lc: structure holding the link state
 * @caps: link capabilities
 *
 * Initializes the SW state maintained for each link, including the link's
 * capabilities and default speed/flow-control/autonegotiation settings.
 */
static void init_link_config(struct link_config *lc,
			     unsigned int caps)
{
	lc->supported = caps;
	lc->requested_speed = 0;
	lc->speed = 0;
	lc->requested_fc = 0;
	lc->fc = 0;
	if (lc->supported & FW_PORT_CAP_ANEG) {
		lc->advertising = lc->supported & ADVERT_MASK;
		lc->autoneg = AUTONEG_ENABLE;
	} else {
		lc->advertising = 0;
		lc->autoneg = AUTONEG_DISABLE;
	}
}

/**
 * t4_wait_dev_ready - wait till to reads of registers work
 *
 * Right after the device is RESET is can take a small amount of time
 * for it to respond to register reads.  Until then, all reads will
 * return either 0xff...ff or 0xee...ee.  Return an error if reads
 * don't work within a reasonable time frame.
 */
static int t4_wait_dev_ready(struct adapter *adapter)
{
	u32 whoami;

	whoami = t4_read_reg(adapter, A_PL_WHOAMI);

	if (whoami != 0xffffffff && whoami != X_CIM_PF_NOACCESS)
		return 0;

	msleep(500);
	whoami = t4_read_reg(adapter, A_PL_WHOAMI);
	return (whoami != 0xffffffff && whoami != X_CIM_PF_NOACCESS
			? 0 : -EIO);
}

struct flash_desc {
	u32 vendor_and_model_id;
	u32 size_mb;
};

int t4_get_flash_params(struct adapter *adapter)
{
	/*
	 * Table for non-Numonix supported flash parts.  Numonix parts are left
	 * to the preexisting well-tested code.  All flash parts have 64KB
	 * sectors.
	 */
	static struct flash_desc supported_flash[] = {
		{ 0x150201, 4 << 20 },       /* Spansion 4MB S25FL032P */
	};

	int ret;
	unsigned int i;
	u32 info = 0;

	ret = sf1_write(adapter, 1, 1, 0, SF_RD_ID);
	if (!ret)
		ret = sf1_read(adapter, 3, 0, 1, &info);
	t4_write_reg(adapter, A_SF_OP, 0);               /* unlock SF */
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(supported_flash); ++i)
		if (supported_flash[i].vendor_and_model_id == info) {
			adapter->params.sf_size = supported_flash[i].size_mb;
			adapter->params.sf_nsec =
				adapter->params.sf_size / SF_SEC_SIZE;
			return 0;
		}

	if ((info & 0xff) != 0x20)             /* not a Numonix flash */
		return -EINVAL;
	info >>= 16;                           /* log2 of size */
	if (info >= 0x14 && info < 0x18)
		adapter->params.sf_nsec = 1 << (info - 16);
	else if (info == 0x18)
		adapter->params.sf_nsec = 64;
	else
		return -EINVAL;
	adapter->params.sf_size = 1 << info;

	/*
	 * We should reject adapters with FLASHes which are too small. So, emit
	 * a warning.
	 */
	if (adapter->params.sf_size < FLASH_MIN_SIZE) {
		dev_warn(adapter, "WARNING!!! FLASH size %#x < %#x!!!\n",
			 adapter->params.sf_size, FLASH_MIN_SIZE);
	}

	return 0;
}

/**
 * t4_prep_adapter - prepare SW and HW for operation
 * @adapter: the adapter
 *
 * Initialize adapter SW state for the various HW modules, set initial
 * values for some adapter tunables, take PHYs out of reset, and
 * initialize the MDIO interface.
 */
int t4_prep_adapter(struct adapter *adapter)
{
	int ret, ver;
	u32 pl_rev;

	ret = t4_wait_dev_ready(adapter);
	if (ret < 0)
		return ret;

	pl_rev = G_REV(t4_read_reg(adapter, A_PL_REV));
	adapter->params.pci.device_id = adapter->pdev->id.device_id;
	adapter->params.pci.vendor_id = adapter->pdev->id.vendor_id;

	/*
	 * WE DON'T NEED adapter->params.chip CODE ONCE PL_REV CONTAINS
	 * ADAPTER (VERSION << 4 | REVISION)
	 */
	ver = CHELSIO_PCI_ID_VER(adapter->params.pci.device_id);
	adapter->params.chip = 0;
	switch (ver) {
	case CHELSIO_T5:
		adapter->params.chip |= CHELSIO_CHIP_CODE(CHELSIO_T5, pl_rev);
		adapter->params.arch.sge_fl_db = F_DBPRIO | F_DBTYPE;
		adapter->params.arch.mps_tcam_size =
						NUM_MPS_T5_CLS_SRAM_L_INSTANCES;
		adapter->params.arch.mps_rplc_size = 128;
		adapter->params.arch.nchan = NCHAN;
		adapter->params.arch.vfcount = 128;
		break;
	default:
		dev_err(adapter, "%s: Device %d is not supported\n",
			__func__, adapter->params.pci.device_id);
		return -EINVAL;
	}

	ret = t4_get_flash_params(adapter);
	if (ret < 0)
		return ret;

	adapter->params.cim_la_size = CIMLA_SIZE;

	init_cong_ctrl(adapter->params.a_wnd, adapter->params.b_wnd);

	/*
	 * Default port and clock for debugging in case we can't reach FW.
	 */
	adapter->params.nports = 1;
	adapter->params.portvec = 1;
	adapter->params.vpd.cclk = 50000;

	return 0;
}

/**
 * t4_bar2_sge_qregs - return BAR2 SGE Queue register information
 * @adapter: the adapter
 * @qid: the Queue ID
 * @qtype: the Ingress or Egress type for @qid
 * @pbar2_qoffset: BAR2 Queue Offset
 * @pbar2_qid: BAR2 Queue ID or 0 for Queue ID inferred SGE Queues
 *
 * Returns the BAR2 SGE Queue Registers information associated with the
 * indicated Absolute Queue ID.  These are passed back in return value
 * pointers.  @qtype should be T4_BAR2_QTYPE_EGRESS for Egress Queue
 * and T4_BAR2_QTYPE_INGRESS for Ingress Queues.
 *
 * This may return an error which indicates that BAR2 SGE Queue
 * registers aren't available.  If an error is not returned, then the
 * following values are returned:
 *
 *   *@pbar2_qoffset: the BAR2 Offset of the @qid Registers
 *   *@pbar2_qid: the BAR2 SGE Queue ID or 0 of @qid
 *
 * If the returned BAR2 Queue ID is 0, then BAR2 SGE registers which
 * require the "Inferred Queue ID" ability may be used.  E.g. the
 * Write Combining Doorbell Buffer. If the BAR2 Queue ID is not 0,
 * then these "Inferred Queue ID" register may not be used.
 */
int t4_bar2_sge_qregs(struct adapter *adapter, unsigned int qid,
		      enum t4_bar2_qtype qtype, u64 *pbar2_qoffset,
		      unsigned int *pbar2_qid)
{
	unsigned int page_shift, page_size, qpp_shift, qpp_mask;
	u64 bar2_page_offset, bar2_qoffset;
	unsigned int bar2_qid, bar2_qid_offset, bar2_qinferred;

	/*
	 * T4 doesn't support BAR2 SGE Queue registers.
	 */
	if (is_t4(adapter->params.chip))
		return -EINVAL;

	/*
	 * Get our SGE Page Size parameters.
	 */
	page_shift = adapter->params.sge.hps + 10;
	page_size = 1 << page_shift;

	/*
	 * Get the right Queues per Page parameters for our Queue.
	 */
	qpp_shift = (qtype == T4_BAR2_QTYPE_EGRESS ?
			      adapter->params.sge.eq_qpp :
			      adapter->params.sge.iq_qpp);
	qpp_mask = (1 << qpp_shift) - 1;

	/*
	 * Calculate the basics of the BAR2 SGE Queue register area:
	 *  o The BAR2 page the Queue registers will be in.
	 *  o The BAR2 Queue ID.
	 *  o The BAR2 Queue ID Offset into the BAR2 page.
	 */
	bar2_page_offset = ((qid >> qpp_shift) << page_shift);
	bar2_qid = qid & qpp_mask;
	bar2_qid_offset = bar2_qid * SGE_UDB_SIZE;

	/*
	 * If the BAR2 Queue ID Offset is less than the Page Size, then the
	 * hardware will infer the Absolute Queue ID simply from the writes to
	 * the BAR2 Queue ID Offset within the BAR2 Page (and we need to use a
	 * BAR2 Queue ID of 0 for those writes).  Otherwise, we'll simply
	 * write to the first BAR2 SGE Queue Area within the BAR2 Page with
	 * the BAR2 Queue ID and the hardware will infer the Absolute Queue ID
	 * from the BAR2 Page and BAR2 Queue ID.
	 *
	 * One important censequence of this is that some BAR2 SGE registers
	 * have a "Queue ID" field and we can write the BAR2 SGE Queue ID
	 * there.  But other registers synthesize the SGE Queue ID purely
	 * from the writes to the registers -- the Write Combined Doorbell
	 * Buffer is a good example.  These BAR2 SGE Registers are only
	 * available for those BAR2 SGE Register areas where the SGE Absolute
	 * Queue ID can be inferred from simple writes.
	 */
	bar2_qoffset = bar2_page_offset;
	bar2_qinferred = (bar2_qid_offset < page_size);
	if (bar2_qinferred) {
		bar2_qoffset += bar2_qid_offset;
		bar2_qid = 0;
	}

	*pbar2_qoffset = bar2_qoffset;
	*pbar2_qid = bar2_qid;
	return 0;
}

/**
 * t4_init_sge_params - initialize adap->params.sge
 * @adapter: the adapter
 *
 * Initialize various fields of the adapter's SGE Parameters structure.
 */
int t4_init_sge_params(struct adapter *adapter)
{
	struct sge_params *sge_params = &adapter->params.sge;
	u32 hps, qpp;
	unsigned int s_hps, s_qpp;

	/*
	 * Extract the SGE Page Size for our PF.
	 */
	hps = t4_read_reg(adapter, A_SGE_HOST_PAGE_SIZE);
	s_hps = (S_HOSTPAGESIZEPF0 + (S_HOSTPAGESIZEPF1 - S_HOSTPAGESIZEPF0) *
		 adapter->pf);
	sge_params->hps = ((hps >> s_hps) & M_HOSTPAGESIZEPF0);

	/*
	 * Extract the SGE Egress and Ingess Queues Per Page for our PF.
	 */
	s_qpp = (S_QUEUESPERPAGEPF0 +
		 (S_QUEUESPERPAGEPF1 - S_QUEUESPERPAGEPF0) * adapter->pf);
	qpp = t4_read_reg(adapter, A_SGE_EGRESS_QUEUES_PER_PAGE_PF);
	sge_params->eq_qpp = ((qpp >> s_qpp) & M_QUEUESPERPAGEPF0);
	qpp = t4_read_reg(adapter, A_SGE_INGRESS_QUEUES_PER_PAGE_PF);
	sge_params->iq_qpp = ((qpp >> s_qpp) & M_QUEUESPERPAGEPF0);

	return 0;
}

/**
 * t4_init_tp_params - initialize adap->params.tp
 * @adap: the adapter
 *
 * Initialize various fields of the adapter's TP Parameters structure.
 */
int t4_init_tp_params(struct adapter *adap)
{
	int chan;
	u32 v;

	v = t4_read_reg(adap, A_TP_TIMER_RESOLUTION);
	adap->params.tp.tre = G_TIMERRESOLUTION(v);
	adap->params.tp.dack_re = G_DELAYEDACKRESOLUTION(v);

	/* MODQ_REQ_MAP defaults to setting queues 0-3 to chan 0-3 */
	for (chan = 0; chan < NCHAN; chan++)
		adap->params.tp.tx_modq[chan] = chan;

	/*
	 * Cache the adapter's Compressed Filter Mode and global Incress
	 * Configuration.
	 */
	t4_read_indirect(adap, A_TP_PIO_ADDR, A_TP_PIO_DATA,
			 &adap->params.tp.vlan_pri_map, 1, A_TP_VLAN_PRI_MAP);
	t4_read_indirect(adap, A_TP_PIO_ADDR, A_TP_PIO_DATA,
			 &adap->params.tp.ingress_config, 1,
			 A_TP_INGRESS_CONFIG);

	/*
	 * Now that we have TP_VLAN_PRI_MAP cached, we can calculate the field
	 * shift positions of several elements of the Compressed Filter Tuple
	 * for this adapter which we need frequently ...
	 */
	adap->params.tp.vlan_shift = t4_filter_field_shift(adap, F_VLAN);
	adap->params.tp.vnic_shift = t4_filter_field_shift(adap, F_VNIC_ID);
	adap->params.tp.port_shift = t4_filter_field_shift(adap, F_PORT);
	adap->params.tp.protocol_shift = t4_filter_field_shift(adap,
							       F_PROTOCOL);

	/*
	 * If TP_INGRESS_CONFIG.VNID == 0, then TP_VLAN_PRI_MAP.VNIC_ID
	 * represents the presense of an Outer VLAN instead of a VNIC ID.
	 */
	if ((adap->params.tp.ingress_config & F_VNIC) == 0)
		adap->params.tp.vnic_shift = -1;

	return 0;
}

/**
 * t4_filter_field_shift - calculate filter field shift
 * @adap: the adapter
 * @filter_sel: the desired field (from TP_VLAN_PRI_MAP bits)
 *
 * Return the shift position of a filter field within the Compressed
 * Filter Tuple.  The filter field is specified via its selection bit
 * within TP_VLAN_PRI_MAL (filter mode).  E.g. F_VLAN.
 */
int t4_filter_field_shift(const struct adapter *adap, unsigned int filter_sel)
{
	unsigned int filter_mode = adap->params.tp.vlan_pri_map;
	unsigned int sel;
	int field_shift;

	if ((filter_mode & filter_sel) == 0)
		return -1;

	for (sel = 1, field_shift = 0; sel < filter_sel; sel <<= 1) {
		switch (filter_mode & sel) {
		case F_FCOE:
			field_shift += W_FT_FCOE;
			break;
		case F_PORT:
			field_shift += W_FT_PORT;
			break;
		case F_VNIC_ID:
			field_shift += W_FT_VNIC_ID;
			break;
		case F_VLAN:
			field_shift += W_FT_VLAN;
			break;
		case F_TOS:
			field_shift += W_FT_TOS;
			break;
		case F_PROTOCOL:
			field_shift += W_FT_PROTOCOL;
			break;
		case F_ETHERTYPE:
			field_shift += W_FT_ETHERTYPE;
			break;
		case F_MACMATCH:
			field_shift += W_FT_MACMATCH;
			break;
		case F_MPSHITTYPE:
			field_shift += W_FT_MPSHITTYPE;
			break;
		case F_FRAGMENTATION:
			field_shift += W_FT_FRAGMENTATION;
			break;
		}
	}
	return field_shift;
}

int t4_init_rss_mode(struct adapter *adap, int mbox)
{
	int i, ret;
	struct fw_rss_vi_config_cmd rvc;

	memset(&rvc, 0, sizeof(rvc));

	for_each_port(adap, i) {
		struct port_info *p = adap2pinfo(adap, i);

		rvc.op_to_viid = htonl(V_FW_CMD_OP(FW_RSS_VI_CONFIG_CMD) |
				       F_FW_CMD_REQUEST | F_FW_CMD_READ |
				       V_FW_RSS_VI_CONFIG_CMD_VIID(p->viid));
		rvc.retval_len16 = htonl(FW_LEN16(rvc));
		ret = t4_wr_mbox(adap, mbox, &rvc, sizeof(rvc), &rvc);
		if (ret)
			return ret;
		p->rss_mode = ntohl(rvc.u.basicvirtual.defaultq_to_udpen);
	}
	return 0;
}

int t4_port_init(struct adapter *adap, int mbox, int pf, int vf)
{
	u8 addr[6];
	int ret, i, j = 0;
	struct fw_port_cmd c;

	memset(&c, 0, sizeof(c));

	for_each_port(adap, i) {
		unsigned int rss_size = 0;
		struct port_info *p = adap2pinfo(adap, i);

		while ((adap->params.portvec & (1 << j)) == 0)
			j++;

		c.op_to_portid = cpu_to_be32(V_FW_CMD_OP(FW_PORT_CMD) |
					     F_FW_CMD_REQUEST | F_FW_CMD_READ |
					     V_FW_PORT_CMD_PORTID(j));
		c.action_to_len16 = cpu_to_be32(V_FW_PORT_CMD_ACTION(
						FW_PORT_ACTION_GET_PORT_INFO) |
						FW_LEN16(c));
		ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
		if (ret)
			return ret;

		ret = t4_alloc_vi(adap, mbox, j, pf, vf, 1, addr, &rss_size);
		if (ret < 0)
			return ret;

		p->viid = ret;
		p->tx_chan = j;
		p->rss_size = rss_size;
		t4_os_set_hw_addr(adap, i, addr);

		ret = be32_to_cpu(c.u.info.lstatus_to_modtype);
		p->mdio_addr = (ret & F_FW_PORT_CMD_MDIOCAP) ?
				G_FW_PORT_CMD_MDIOADDR(ret) : -1;
		p->port_type = G_FW_PORT_CMD_PTYPE(ret);
		p->mod_type = FW_PORT_MOD_TYPE_NA;

		init_link_config(&p->link_cfg, be16_to_cpu(c.u.info.pcap));
		j++;
	}
	return 0;
}
