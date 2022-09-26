/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Chelsio Communications.
 * All rights reserved.
 */

#include <netinet/in.h>

#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_branch_prediction.h>
#include <rte_memory.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <dev_driver.h>
#include <rte_byteorder.h>

#include "common.h"
#include "t4_regs.h"
#include "t4_regs_values.h"
#include "t4fw_interface.h"

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
		*rpl++ = cpu_to_be64(t4_read_reg64(adap, mbox_addr));
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

	u32 data_reg = PF_REG(mbox, A_CIM_PF_MAILBOX_DATA);
	u32 ctl_reg = PF_REG(mbox, A_CIM_PF_MAILBOX_CTRL);
	struct mbox_entry *entry;
	u32 v, ctl, pcie_fw = 0;
	unsigned int delay_idx;
	const __be64 *p;
	int i, ms, ret;
	u64 res;

	if ((size & 15) != 0 || size > MBOX_LEN)
		return -EINVAL;

	/*
	 * If we have a negative timeout, that implies that we can't sleep.
	 */
	if (timeout < 0) {
		sleep_ok = false;
		timeout = -timeout;
	}

	entry = t4_os_alloc(sizeof(*entry));
	if (entry == NULL)
		return -ENOMEM;

	/*
	 * Queue ourselves onto the mailbox access list.  When our entry is at
	 * the front of the list, we have rights to access the mailbox.  So we
	 * wait [for a while] till we're at the front [or bail out with an
	 * EBUSY] ...
	 */
	t4_os_atomic_add_tail(entry, &adap->mbox_list, &adap->mbox_lock);

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
			t4_os_atomic_list_del(entry, &adap->mbox_list,
					      &adap->mbox_lock);
			t4_report_fw_error(adap);
			ret = ((pcie_fw & F_PCIE_FW_ERR) != 0) ? -ENXIO : -EBUSY;
			goto out_free;
		}

		/*
		 * If we're at the head, break out and start the mailbox
		 * protocol.
		 */
		if (t4_os_list_first_entry(&adap->mbox_list) == entry)
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
		t4_os_atomic_list_del(entry, &adap->mbox_list,
				      &adap->mbox_lock);
		t4_report_fw_error(adap);
		ret = (v == X_MBOWNER_FW) ? -EBUSY : -ETIMEDOUT;
		goto out_free;
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
	for (i = 0, p = cmd; i < size; i += 8, p++)
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
			t4_os_atomic_list_del(entry, &adap->mbox_list,
					      &adap->mbox_lock);
			ret = -G_FW_CMD_RETVAL((int)res);
			goto out_free;
		}
	}

	/*
	 * We timed out waiting for a reply to our mailbox command.  Report
	 * the error and also check to see if the firmware reported any
	 * errors ...
	 */
	dev_err(adap, "command %#x in mailbox %d timed out\n",
		*(const u8 *)cmd, mbox);
	t4_os_atomic_list_del(entry, &adap->mbox_list, &adap->mbox_lock);
	t4_report_fw_error(adap);
	ret = ((pcie_fw & F_PCIE_FW_ERR) != 0) ? -ENXIO : -ETIMEDOUT;

out_free:
	t4_os_free(entry);
	return ret;
}

int t4_wr_mbox_meat(struct adapter *adap, int mbox, const void *cmd, int size,
		    void *rpl, bool sleep_ok)
{
	return t4_wr_mbox_meat_timeout(adap, mbox, cmd, size, rpl, sleep_ok,
				       FW_CMD_MAX_TIMEOUT);
}

/**
 * t4_get_regs_len - return the size of the chips register set
 * @adapter: the adapter
 *
 * Returns the size of the chip's BAR0 register space.
 */
unsigned int t4_get_regs_len(struct adapter *adapter)
{
	unsigned int chip_version = CHELSIO_CHIP_VERSION(adapter->params.chip);

	switch (chip_version) {
	case CHELSIO_T5:
	case CHELSIO_T6:
		return T5_REGMAP_SIZE;
	}

	dev_err(adapter,
		"Unsupported chip version %d\n", chip_version);
	return 0;
}

/**
 * t4_get_regs - read chip registers into provided buffer
 * @adap: the adapter
 * @buf: register buffer
 * @buf_size: size (in bytes) of register buffer
 *
 * If the provided register buffer isn't large enough for the chip's
 * full register range, the register dump will be truncated to the
 * register buffer's size.
 */
void t4_get_regs(struct adapter *adap, void *buf, size_t buf_size)
{
	static const unsigned int t5_reg_ranges[] = {
		0x1008, 0x10c0,
		0x10cc, 0x10f8,
		0x1100, 0x1100,
		0x110c, 0x1148,
		0x1180, 0x1184,
		0x1190, 0x1194,
		0x11a0, 0x11a4,
		0x11b0, 0x11b4,
		0x11fc, 0x123c,
		0x1280, 0x173c,
		0x1800, 0x18fc,
		0x3000, 0x3028,
		0x3060, 0x30b0,
		0x30b8, 0x30d8,
		0x30e0, 0x30fc,
		0x3140, 0x357c,
		0x35a8, 0x35cc,
		0x35ec, 0x35ec,
		0x3600, 0x5624,
		0x56cc, 0x56ec,
		0x56f4, 0x5720,
		0x5728, 0x575c,
		0x580c, 0x5814,
		0x5890, 0x589c,
		0x58a4, 0x58ac,
		0x58b8, 0x58bc,
		0x5940, 0x59c8,
		0x59d0, 0x59dc,
		0x59fc, 0x5a18,
		0x5a60, 0x5a70,
		0x5a80, 0x5a9c,
		0x5b94, 0x5bfc,
		0x6000, 0x6020,
		0x6028, 0x6040,
		0x6058, 0x609c,
		0x60a8, 0x614c,
		0x7700, 0x7798,
		0x77c0, 0x78fc,
		0x7b00, 0x7b58,
		0x7b60, 0x7b84,
		0x7b8c, 0x7c54,
		0x7d00, 0x7d38,
		0x7d40, 0x7d80,
		0x7d8c, 0x7ddc,
		0x7de4, 0x7e04,
		0x7e10, 0x7e1c,
		0x7e24, 0x7e38,
		0x7e40, 0x7e44,
		0x7e4c, 0x7e78,
		0x7e80, 0x7edc,
		0x7ee8, 0x7efc,
		0x8dc0, 0x8de0,
		0x8df8, 0x8e04,
		0x8e10, 0x8e84,
		0x8ea0, 0x8f84,
		0x8fc0, 0x9058,
		0x9060, 0x9060,
		0x9068, 0x90f8,
		0x9400, 0x9408,
		0x9410, 0x9470,
		0x9600, 0x9600,
		0x9608, 0x9638,
		0x9640, 0x96f4,
		0x9800, 0x9808,
		0x9820, 0x983c,
		0x9850, 0x9864,
		0x9c00, 0x9c6c,
		0x9c80, 0x9cec,
		0x9d00, 0x9d6c,
		0x9d80, 0x9dec,
		0x9e00, 0x9e6c,
		0x9e80, 0x9eec,
		0x9f00, 0x9f6c,
		0x9f80, 0xa020,
		0xd004, 0xd004,
		0xd010, 0xd03c,
		0xdfc0, 0xdfe0,
		0xe000, 0x1106c,
		0x11074, 0x11088,
		0x1109c, 0x1117c,
		0x11190, 0x11204,
		0x19040, 0x1906c,
		0x19078, 0x19080,
		0x1908c, 0x190e8,
		0x190f0, 0x190f8,
		0x19100, 0x19110,
		0x19120, 0x19124,
		0x19150, 0x19194,
		0x1919c, 0x191b0,
		0x191d0, 0x191e8,
		0x19238, 0x19290,
		0x193f8, 0x19428,
		0x19430, 0x19444,
		0x1944c, 0x1946c,
		0x19474, 0x19474,
		0x19490, 0x194cc,
		0x194f0, 0x194f8,
		0x19c00, 0x19c08,
		0x19c10, 0x19c60,
		0x19c94, 0x19ce4,
		0x19cf0, 0x19d40,
		0x19d50, 0x19d94,
		0x19da0, 0x19de8,
		0x19df0, 0x19e10,
		0x19e50, 0x19e90,
		0x19ea0, 0x19f24,
		0x19f34, 0x19f34,
		0x19f40, 0x19f50,
		0x19f90, 0x19fb4,
		0x19fc4, 0x19fe4,
		0x1a000, 0x1a004,
		0x1a010, 0x1a06c,
		0x1a0b0, 0x1a0e4,
		0x1a0ec, 0x1a0f8,
		0x1a100, 0x1a108,
		0x1a114, 0x1a120,
		0x1a128, 0x1a130,
		0x1a138, 0x1a138,
		0x1a190, 0x1a1c4,
		0x1a1fc, 0x1a1fc,
		0x1e008, 0x1e00c,
		0x1e040, 0x1e044,
		0x1e04c, 0x1e04c,
		0x1e284, 0x1e290,
		0x1e2c0, 0x1e2c0,
		0x1e2e0, 0x1e2e0,
		0x1e300, 0x1e384,
		0x1e3c0, 0x1e3c8,
		0x1e408, 0x1e40c,
		0x1e440, 0x1e444,
		0x1e44c, 0x1e44c,
		0x1e684, 0x1e690,
		0x1e6c0, 0x1e6c0,
		0x1e6e0, 0x1e6e0,
		0x1e700, 0x1e784,
		0x1e7c0, 0x1e7c8,
		0x1e808, 0x1e80c,
		0x1e840, 0x1e844,
		0x1e84c, 0x1e84c,
		0x1ea84, 0x1ea90,
		0x1eac0, 0x1eac0,
		0x1eae0, 0x1eae0,
		0x1eb00, 0x1eb84,
		0x1ebc0, 0x1ebc8,
		0x1ec08, 0x1ec0c,
		0x1ec40, 0x1ec44,
		0x1ec4c, 0x1ec4c,
		0x1ee84, 0x1ee90,
		0x1eec0, 0x1eec0,
		0x1eee0, 0x1eee0,
		0x1ef00, 0x1ef84,
		0x1efc0, 0x1efc8,
		0x1f008, 0x1f00c,
		0x1f040, 0x1f044,
		0x1f04c, 0x1f04c,
		0x1f284, 0x1f290,
		0x1f2c0, 0x1f2c0,
		0x1f2e0, 0x1f2e0,
		0x1f300, 0x1f384,
		0x1f3c0, 0x1f3c8,
		0x1f408, 0x1f40c,
		0x1f440, 0x1f444,
		0x1f44c, 0x1f44c,
		0x1f684, 0x1f690,
		0x1f6c0, 0x1f6c0,
		0x1f6e0, 0x1f6e0,
		0x1f700, 0x1f784,
		0x1f7c0, 0x1f7c8,
		0x1f808, 0x1f80c,
		0x1f840, 0x1f844,
		0x1f84c, 0x1f84c,
		0x1fa84, 0x1fa90,
		0x1fac0, 0x1fac0,
		0x1fae0, 0x1fae0,
		0x1fb00, 0x1fb84,
		0x1fbc0, 0x1fbc8,
		0x1fc08, 0x1fc0c,
		0x1fc40, 0x1fc44,
		0x1fc4c, 0x1fc4c,
		0x1fe84, 0x1fe90,
		0x1fec0, 0x1fec0,
		0x1fee0, 0x1fee0,
		0x1ff00, 0x1ff84,
		0x1ffc0, 0x1ffc8,
		0x30000, 0x30030,
		0x30038, 0x30038,
		0x30040, 0x30040,
		0x30100, 0x30144,
		0x30190, 0x301a0,
		0x301a8, 0x301b8,
		0x301c4, 0x301c8,
		0x301d0, 0x301d0,
		0x30200, 0x30318,
		0x30400, 0x304b4,
		0x304c0, 0x3052c,
		0x30540, 0x3061c,
		0x30800, 0x30828,
		0x30834, 0x30834,
		0x308c0, 0x30908,
		0x30910, 0x309ac,
		0x30a00, 0x30a14,
		0x30a1c, 0x30a2c,
		0x30a44, 0x30a50,
		0x30a74, 0x30a74,
		0x30a7c, 0x30afc,
		0x30b08, 0x30c24,
		0x30d00, 0x30d00,
		0x30d08, 0x30d14,
		0x30d1c, 0x30d20,
		0x30d3c, 0x30d3c,
		0x30d48, 0x30d50,
		0x31200, 0x3120c,
		0x31220, 0x31220,
		0x31240, 0x31240,
		0x31600, 0x3160c,
		0x31a00, 0x31a1c,
		0x31e00, 0x31e20,
		0x31e38, 0x31e3c,
		0x31e80, 0x31e80,
		0x31e88, 0x31ea8,
		0x31eb0, 0x31eb4,
		0x31ec8, 0x31ed4,
		0x31fb8, 0x32004,
		0x32200, 0x32200,
		0x32208, 0x32240,
		0x32248, 0x32280,
		0x32288, 0x322c0,
		0x322c8, 0x322fc,
		0x32600, 0x32630,
		0x32a00, 0x32abc,
		0x32b00, 0x32b10,
		0x32b20, 0x32b30,
		0x32b40, 0x32b50,
		0x32b60, 0x32b70,
		0x33000, 0x33028,
		0x33030, 0x33048,
		0x33060, 0x33068,
		0x33070, 0x3309c,
		0x330f0, 0x33128,
		0x33130, 0x33148,
		0x33160, 0x33168,
		0x33170, 0x3319c,
		0x331f0, 0x33238,
		0x33240, 0x33240,
		0x33248, 0x33250,
		0x3325c, 0x33264,
		0x33270, 0x332b8,
		0x332c0, 0x332e4,
		0x332f8, 0x33338,
		0x33340, 0x33340,
		0x33348, 0x33350,
		0x3335c, 0x33364,
		0x33370, 0x333b8,
		0x333c0, 0x333e4,
		0x333f8, 0x33428,
		0x33430, 0x33448,
		0x33460, 0x33468,
		0x33470, 0x3349c,
		0x334f0, 0x33528,
		0x33530, 0x33548,
		0x33560, 0x33568,
		0x33570, 0x3359c,
		0x335f0, 0x33638,
		0x33640, 0x33640,
		0x33648, 0x33650,
		0x3365c, 0x33664,
		0x33670, 0x336b8,
		0x336c0, 0x336e4,
		0x336f8, 0x33738,
		0x33740, 0x33740,
		0x33748, 0x33750,
		0x3375c, 0x33764,
		0x33770, 0x337b8,
		0x337c0, 0x337e4,
		0x337f8, 0x337fc,
		0x33814, 0x33814,
		0x3382c, 0x3382c,
		0x33880, 0x3388c,
		0x338e8, 0x338ec,
		0x33900, 0x33928,
		0x33930, 0x33948,
		0x33960, 0x33968,
		0x33970, 0x3399c,
		0x339f0, 0x33a38,
		0x33a40, 0x33a40,
		0x33a48, 0x33a50,
		0x33a5c, 0x33a64,
		0x33a70, 0x33ab8,
		0x33ac0, 0x33ae4,
		0x33af8, 0x33b10,
		0x33b28, 0x33b28,
		0x33b3c, 0x33b50,
		0x33bf0, 0x33c10,
		0x33c28, 0x33c28,
		0x33c3c, 0x33c50,
		0x33cf0, 0x33cfc,
		0x34000, 0x34030,
		0x34038, 0x34038,
		0x34040, 0x34040,
		0x34100, 0x34144,
		0x34190, 0x341a0,
		0x341a8, 0x341b8,
		0x341c4, 0x341c8,
		0x341d0, 0x341d0,
		0x34200, 0x34318,
		0x34400, 0x344b4,
		0x344c0, 0x3452c,
		0x34540, 0x3461c,
		0x34800, 0x34828,
		0x34834, 0x34834,
		0x348c0, 0x34908,
		0x34910, 0x349ac,
		0x34a00, 0x34a14,
		0x34a1c, 0x34a2c,
		0x34a44, 0x34a50,
		0x34a74, 0x34a74,
		0x34a7c, 0x34afc,
		0x34b08, 0x34c24,
		0x34d00, 0x34d00,
		0x34d08, 0x34d14,
		0x34d1c, 0x34d20,
		0x34d3c, 0x34d3c,
		0x34d48, 0x34d50,
		0x35200, 0x3520c,
		0x35220, 0x35220,
		0x35240, 0x35240,
		0x35600, 0x3560c,
		0x35a00, 0x35a1c,
		0x35e00, 0x35e20,
		0x35e38, 0x35e3c,
		0x35e80, 0x35e80,
		0x35e88, 0x35ea8,
		0x35eb0, 0x35eb4,
		0x35ec8, 0x35ed4,
		0x35fb8, 0x36004,
		0x36200, 0x36200,
		0x36208, 0x36240,
		0x36248, 0x36280,
		0x36288, 0x362c0,
		0x362c8, 0x362fc,
		0x36600, 0x36630,
		0x36a00, 0x36abc,
		0x36b00, 0x36b10,
		0x36b20, 0x36b30,
		0x36b40, 0x36b50,
		0x36b60, 0x36b70,
		0x37000, 0x37028,
		0x37030, 0x37048,
		0x37060, 0x37068,
		0x37070, 0x3709c,
		0x370f0, 0x37128,
		0x37130, 0x37148,
		0x37160, 0x37168,
		0x37170, 0x3719c,
		0x371f0, 0x37238,
		0x37240, 0x37240,
		0x37248, 0x37250,
		0x3725c, 0x37264,
		0x37270, 0x372b8,
		0x372c0, 0x372e4,
		0x372f8, 0x37338,
		0x37340, 0x37340,
		0x37348, 0x37350,
		0x3735c, 0x37364,
		0x37370, 0x373b8,
		0x373c0, 0x373e4,
		0x373f8, 0x37428,
		0x37430, 0x37448,
		0x37460, 0x37468,
		0x37470, 0x3749c,
		0x374f0, 0x37528,
		0x37530, 0x37548,
		0x37560, 0x37568,
		0x37570, 0x3759c,
		0x375f0, 0x37638,
		0x37640, 0x37640,
		0x37648, 0x37650,
		0x3765c, 0x37664,
		0x37670, 0x376b8,
		0x376c0, 0x376e4,
		0x376f8, 0x37738,
		0x37740, 0x37740,
		0x37748, 0x37750,
		0x3775c, 0x37764,
		0x37770, 0x377b8,
		0x377c0, 0x377e4,
		0x377f8, 0x377fc,
		0x37814, 0x37814,
		0x3782c, 0x3782c,
		0x37880, 0x3788c,
		0x378e8, 0x378ec,
		0x37900, 0x37928,
		0x37930, 0x37948,
		0x37960, 0x37968,
		0x37970, 0x3799c,
		0x379f0, 0x37a38,
		0x37a40, 0x37a40,
		0x37a48, 0x37a50,
		0x37a5c, 0x37a64,
		0x37a70, 0x37ab8,
		0x37ac0, 0x37ae4,
		0x37af8, 0x37b10,
		0x37b28, 0x37b28,
		0x37b3c, 0x37b50,
		0x37bf0, 0x37c10,
		0x37c28, 0x37c28,
		0x37c3c, 0x37c50,
		0x37cf0, 0x37cfc,
		0x38000, 0x38030,
		0x38038, 0x38038,
		0x38040, 0x38040,
		0x38100, 0x38144,
		0x38190, 0x381a0,
		0x381a8, 0x381b8,
		0x381c4, 0x381c8,
		0x381d0, 0x381d0,
		0x38200, 0x38318,
		0x38400, 0x384b4,
		0x384c0, 0x3852c,
		0x38540, 0x3861c,
		0x38800, 0x38828,
		0x38834, 0x38834,
		0x388c0, 0x38908,
		0x38910, 0x389ac,
		0x38a00, 0x38a14,
		0x38a1c, 0x38a2c,
		0x38a44, 0x38a50,
		0x38a74, 0x38a74,
		0x38a7c, 0x38afc,
		0x38b08, 0x38c24,
		0x38d00, 0x38d00,
		0x38d08, 0x38d14,
		0x38d1c, 0x38d20,
		0x38d3c, 0x38d3c,
		0x38d48, 0x38d50,
		0x39200, 0x3920c,
		0x39220, 0x39220,
		0x39240, 0x39240,
		0x39600, 0x3960c,
		0x39a00, 0x39a1c,
		0x39e00, 0x39e20,
		0x39e38, 0x39e3c,
		0x39e80, 0x39e80,
		0x39e88, 0x39ea8,
		0x39eb0, 0x39eb4,
		0x39ec8, 0x39ed4,
		0x39fb8, 0x3a004,
		0x3a200, 0x3a200,
		0x3a208, 0x3a240,
		0x3a248, 0x3a280,
		0x3a288, 0x3a2c0,
		0x3a2c8, 0x3a2fc,
		0x3a600, 0x3a630,
		0x3aa00, 0x3aabc,
		0x3ab00, 0x3ab10,
		0x3ab20, 0x3ab30,
		0x3ab40, 0x3ab50,
		0x3ab60, 0x3ab70,
		0x3b000, 0x3b028,
		0x3b030, 0x3b048,
		0x3b060, 0x3b068,
		0x3b070, 0x3b09c,
		0x3b0f0, 0x3b128,
		0x3b130, 0x3b148,
		0x3b160, 0x3b168,
		0x3b170, 0x3b19c,
		0x3b1f0, 0x3b238,
		0x3b240, 0x3b240,
		0x3b248, 0x3b250,
		0x3b25c, 0x3b264,
		0x3b270, 0x3b2b8,
		0x3b2c0, 0x3b2e4,
		0x3b2f8, 0x3b338,
		0x3b340, 0x3b340,
		0x3b348, 0x3b350,
		0x3b35c, 0x3b364,
		0x3b370, 0x3b3b8,
		0x3b3c0, 0x3b3e4,
		0x3b3f8, 0x3b428,
		0x3b430, 0x3b448,
		0x3b460, 0x3b468,
		0x3b470, 0x3b49c,
		0x3b4f0, 0x3b528,
		0x3b530, 0x3b548,
		0x3b560, 0x3b568,
		0x3b570, 0x3b59c,
		0x3b5f0, 0x3b638,
		0x3b640, 0x3b640,
		0x3b648, 0x3b650,
		0x3b65c, 0x3b664,
		0x3b670, 0x3b6b8,
		0x3b6c0, 0x3b6e4,
		0x3b6f8, 0x3b738,
		0x3b740, 0x3b740,
		0x3b748, 0x3b750,
		0x3b75c, 0x3b764,
		0x3b770, 0x3b7b8,
		0x3b7c0, 0x3b7e4,
		0x3b7f8, 0x3b7fc,
		0x3b814, 0x3b814,
		0x3b82c, 0x3b82c,
		0x3b880, 0x3b88c,
		0x3b8e8, 0x3b8ec,
		0x3b900, 0x3b928,
		0x3b930, 0x3b948,
		0x3b960, 0x3b968,
		0x3b970, 0x3b99c,
		0x3b9f0, 0x3ba38,
		0x3ba40, 0x3ba40,
		0x3ba48, 0x3ba50,
		0x3ba5c, 0x3ba64,
		0x3ba70, 0x3bab8,
		0x3bac0, 0x3bae4,
		0x3baf8, 0x3bb10,
		0x3bb28, 0x3bb28,
		0x3bb3c, 0x3bb50,
		0x3bbf0, 0x3bc10,
		0x3bc28, 0x3bc28,
		0x3bc3c, 0x3bc50,
		0x3bcf0, 0x3bcfc,
		0x3c000, 0x3c030,
		0x3c038, 0x3c038,
		0x3c040, 0x3c040,
		0x3c100, 0x3c144,
		0x3c190, 0x3c1a0,
		0x3c1a8, 0x3c1b8,
		0x3c1c4, 0x3c1c8,
		0x3c1d0, 0x3c1d0,
		0x3c200, 0x3c318,
		0x3c400, 0x3c4b4,
		0x3c4c0, 0x3c52c,
		0x3c540, 0x3c61c,
		0x3c800, 0x3c828,
		0x3c834, 0x3c834,
		0x3c8c0, 0x3c908,
		0x3c910, 0x3c9ac,
		0x3ca00, 0x3ca14,
		0x3ca1c, 0x3ca2c,
		0x3ca44, 0x3ca50,
		0x3ca74, 0x3ca74,
		0x3ca7c, 0x3cafc,
		0x3cb08, 0x3cc24,
		0x3cd00, 0x3cd00,
		0x3cd08, 0x3cd14,
		0x3cd1c, 0x3cd20,
		0x3cd3c, 0x3cd3c,
		0x3cd48, 0x3cd50,
		0x3d200, 0x3d20c,
		0x3d220, 0x3d220,
		0x3d240, 0x3d240,
		0x3d600, 0x3d60c,
		0x3da00, 0x3da1c,
		0x3de00, 0x3de20,
		0x3de38, 0x3de3c,
		0x3de80, 0x3de80,
		0x3de88, 0x3dea8,
		0x3deb0, 0x3deb4,
		0x3dec8, 0x3ded4,
		0x3dfb8, 0x3e004,
		0x3e200, 0x3e200,
		0x3e208, 0x3e240,
		0x3e248, 0x3e280,
		0x3e288, 0x3e2c0,
		0x3e2c8, 0x3e2fc,
		0x3e600, 0x3e630,
		0x3ea00, 0x3eabc,
		0x3eb00, 0x3eb10,
		0x3eb20, 0x3eb30,
		0x3eb40, 0x3eb50,
		0x3eb60, 0x3eb70,
		0x3f000, 0x3f028,
		0x3f030, 0x3f048,
		0x3f060, 0x3f068,
		0x3f070, 0x3f09c,
		0x3f0f0, 0x3f128,
		0x3f130, 0x3f148,
		0x3f160, 0x3f168,
		0x3f170, 0x3f19c,
		0x3f1f0, 0x3f238,
		0x3f240, 0x3f240,
		0x3f248, 0x3f250,
		0x3f25c, 0x3f264,
		0x3f270, 0x3f2b8,
		0x3f2c0, 0x3f2e4,
		0x3f2f8, 0x3f338,
		0x3f340, 0x3f340,
		0x3f348, 0x3f350,
		0x3f35c, 0x3f364,
		0x3f370, 0x3f3b8,
		0x3f3c0, 0x3f3e4,
		0x3f3f8, 0x3f428,
		0x3f430, 0x3f448,
		0x3f460, 0x3f468,
		0x3f470, 0x3f49c,
		0x3f4f0, 0x3f528,
		0x3f530, 0x3f548,
		0x3f560, 0x3f568,
		0x3f570, 0x3f59c,
		0x3f5f0, 0x3f638,
		0x3f640, 0x3f640,
		0x3f648, 0x3f650,
		0x3f65c, 0x3f664,
		0x3f670, 0x3f6b8,
		0x3f6c0, 0x3f6e4,
		0x3f6f8, 0x3f738,
		0x3f740, 0x3f740,
		0x3f748, 0x3f750,
		0x3f75c, 0x3f764,
		0x3f770, 0x3f7b8,
		0x3f7c0, 0x3f7e4,
		0x3f7f8, 0x3f7fc,
		0x3f814, 0x3f814,
		0x3f82c, 0x3f82c,
		0x3f880, 0x3f88c,
		0x3f8e8, 0x3f8ec,
		0x3f900, 0x3f928,
		0x3f930, 0x3f948,
		0x3f960, 0x3f968,
		0x3f970, 0x3f99c,
		0x3f9f0, 0x3fa38,
		0x3fa40, 0x3fa40,
		0x3fa48, 0x3fa50,
		0x3fa5c, 0x3fa64,
		0x3fa70, 0x3fab8,
		0x3fac0, 0x3fae4,
		0x3faf8, 0x3fb10,
		0x3fb28, 0x3fb28,
		0x3fb3c, 0x3fb50,
		0x3fbf0, 0x3fc10,
		0x3fc28, 0x3fc28,
		0x3fc3c, 0x3fc50,
		0x3fcf0, 0x3fcfc,
		0x40000, 0x4000c,
		0x40040, 0x40050,
		0x40060, 0x40068,
		0x4007c, 0x4008c,
		0x40094, 0x400b0,
		0x400c0, 0x40144,
		0x40180, 0x4018c,
		0x40200, 0x40254,
		0x40260, 0x40264,
		0x40270, 0x40288,
		0x40290, 0x40298,
		0x402ac, 0x402c8,
		0x402d0, 0x402e0,
		0x402f0, 0x402f0,
		0x40300, 0x4033c,
		0x403f8, 0x403fc,
		0x41304, 0x413c4,
		0x41400, 0x4140c,
		0x41414, 0x4141c,
		0x41480, 0x414d0,
		0x44000, 0x44054,
		0x4405c, 0x44078,
		0x440c0, 0x44174,
		0x44180, 0x441ac,
		0x441b4, 0x441b8,
		0x441c0, 0x44254,
		0x4425c, 0x44278,
		0x442c0, 0x44374,
		0x44380, 0x443ac,
		0x443b4, 0x443b8,
		0x443c0, 0x44454,
		0x4445c, 0x44478,
		0x444c0, 0x44574,
		0x44580, 0x445ac,
		0x445b4, 0x445b8,
		0x445c0, 0x44654,
		0x4465c, 0x44678,
		0x446c0, 0x44774,
		0x44780, 0x447ac,
		0x447b4, 0x447b8,
		0x447c0, 0x44854,
		0x4485c, 0x44878,
		0x448c0, 0x44974,
		0x44980, 0x449ac,
		0x449b4, 0x449b8,
		0x449c0, 0x449fc,
		0x45000, 0x45004,
		0x45010, 0x45030,
		0x45040, 0x45060,
		0x45068, 0x45068,
		0x45080, 0x45084,
		0x450a0, 0x450b0,
		0x45200, 0x45204,
		0x45210, 0x45230,
		0x45240, 0x45260,
		0x45268, 0x45268,
		0x45280, 0x45284,
		0x452a0, 0x452b0,
		0x460c0, 0x460e4,
		0x47000, 0x4703c,
		0x47044, 0x4708c,
		0x47200, 0x47250,
		0x47400, 0x47408,
		0x47414, 0x47420,
		0x47600, 0x47618,
		0x47800, 0x47814,
		0x48000, 0x4800c,
		0x48040, 0x48050,
		0x48060, 0x48068,
		0x4807c, 0x4808c,
		0x48094, 0x480b0,
		0x480c0, 0x48144,
		0x48180, 0x4818c,
		0x48200, 0x48254,
		0x48260, 0x48264,
		0x48270, 0x48288,
		0x48290, 0x48298,
		0x482ac, 0x482c8,
		0x482d0, 0x482e0,
		0x482f0, 0x482f0,
		0x48300, 0x4833c,
		0x483f8, 0x483fc,
		0x49304, 0x493c4,
		0x49400, 0x4940c,
		0x49414, 0x4941c,
		0x49480, 0x494d0,
		0x4c000, 0x4c054,
		0x4c05c, 0x4c078,
		0x4c0c0, 0x4c174,
		0x4c180, 0x4c1ac,
		0x4c1b4, 0x4c1b8,
		0x4c1c0, 0x4c254,
		0x4c25c, 0x4c278,
		0x4c2c0, 0x4c374,
		0x4c380, 0x4c3ac,
		0x4c3b4, 0x4c3b8,
		0x4c3c0, 0x4c454,
		0x4c45c, 0x4c478,
		0x4c4c0, 0x4c574,
		0x4c580, 0x4c5ac,
		0x4c5b4, 0x4c5b8,
		0x4c5c0, 0x4c654,
		0x4c65c, 0x4c678,
		0x4c6c0, 0x4c774,
		0x4c780, 0x4c7ac,
		0x4c7b4, 0x4c7b8,
		0x4c7c0, 0x4c854,
		0x4c85c, 0x4c878,
		0x4c8c0, 0x4c974,
		0x4c980, 0x4c9ac,
		0x4c9b4, 0x4c9b8,
		0x4c9c0, 0x4c9fc,
		0x4d000, 0x4d004,
		0x4d010, 0x4d030,
		0x4d040, 0x4d060,
		0x4d068, 0x4d068,
		0x4d080, 0x4d084,
		0x4d0a0, 0x4d0b0,
		0x4d200, 0x4d204,
		0x4d210, 0x4d230,
		0x4d240, 0x4d260,
		0x4d268, 0x4d268,
		0x4d280, 0x4d284,
		0x4d2a0, 0x4d2b0,
		0x4e0c0, 0x4e0e4,
		0x4f000, 0x4f03c,
		0x4f044, 0x4f08c,
		0x4f200, 0x4f250,
		0x4f400, 0x4f408,
		0x4f414, 0x4f420,
		0x4f600, 0x4f618,
		0x4f800, 0x4f814,
		0x50000, 0x50084,
		0x50090, 0x500cc,
		0x50400, 0x50400,
		0x50800, 0x50884,
		0x50890, 0x508cc,
		0x50c00, 0x50c00,
		0x51000, 0x5101c,
		0x51300, 0x51308,
	};

	static const unsigned int t6_reg_ranges[] = {
		0x1008, 0x101c,
		0x1024, 0x10a8,
		0x10b4, 0x10f8,
		0x1100, 0x1114,
		0x111c, 0x112c,
		0x1138, 0x113c,
		0x1144, 0x114c,
		0x1180, 0x1184,
		0x1190, 0x1194,
		0x11a0, 0x11a4,
		0x11b0, 0x11b4,
		0x11fc, 0x1274,
		0x1280, 0x133c,
		0x1800, 0x18fc,
		0x3000, 0x302c,
		0x3060, 0x30b0,
		0x30b8, 0x30d8,
		0x30e0, 0x30fc,
		0x3140, 0x357c,
		0x35a8, 0x35cc,
		0x35ec, 0x35ec,
		0x3600, 0x5624,
		0x56cc, 0x56ec,
		0x56f4, 0x5720,
		0x5728, 0x575c,
		0x580c, 0x5814,
		0x5890, 0x589c,
		0x58a4, 0x58ac,
		0x58b8, 0x58bc,
		0x5940, 0x595c,
		0x5980, 0x598c,
		0x59b0, 0x59c8,
		0x59d0, 0x59dc,
		0x59fc, 0x5a18,
		0x5a60, 0x5a6c,
		0x5a80, 0x5a8c,
		0x5a94, 0x5a9c,
		0x5b94, 0x5bfc,
		0x5c10, 0x5e48,
		0x5e50, 0x5e94,
		0x5ea0, 0x5eb0,
		0x5ec0, 0x5ec0,
		0x5ec8, 0x5ed0,
		0x5ee0, 0x5ee0,
		0x5ef0, 0x5ef0,
		0x5f00, 0x5f00,
		0x6000, 0x6020,
		0x6028, 0x6040,
		0x6058, 0x609c,
		0x60a8, 0x619c,
		0x7700, 0x7798,
		0x77c0, 0x7880,
		0x78cc, 0x78fc,
		0x7b00, 0x7b58,
		0x7b60, 0x7b84,
		0x7b8c, 0x7c54,
		0x7d00, 0x7d38,
		0x7d40, 0x7d84,
		0x7d8c, 0x7ddc,
		0x7de4, 0x7e04,
		0x7e10, 0x7e1c,
		0x7e24, 0x7e38,
		0x7e40, 0x7e44,
		0x7e4c, 0x7e78,
		0x7e80, 0x7edc,
		0x7ee8, 0x7efc,
		0x8dc0, 0x8de4,
		0x8df8, 0x8e04,
		0x8e10, 0x8e84,
		0x8ea0, 0x8f88,
		0x8fb8, 0x9058,
		0x9060, 0x9060,
		0x9068, 0x90f8,
		0x9100, 0x9124,
		0x9400, 0x9470,
		0x9600, 0x9600,
		0x9608, 0x9638,
		0x9640, 0x9704,
		0x9710, 0x971c,
		0x9800, 0x9808,
		0x9820, 0x983c,
		0x9850, 0x9864,
		0x9c00, 0x9c6c,
		0x9c80, 0x9cec,
		0x9d00, 0x9d6c,
		0x9d80, 0x9dec,
		0x9e00, 0x9e6c,
		0x9e80, 0x9eec,
		0x9f00, 0x9f6c,
		0x9f80, 0xa020,
		0xd004, 0xd03c,
		0xd100, 0xd118,
		0xd200, 0xd214,
		0xd220, 0xd234,
		0xd240, 0xd254,
		0xd260, 0xd274,
		0xd280, 0xd294,
		0xd2a0, 0xd2b4,
		0xd2c0, 0xd2d4,
		0xd2e0, 0xd2f4,
		0xd300, 0xd31c,
		0xdfc0, 0xdfe0,
		0xe000, 0xf008,
		0xf010, 0xf018,
		0xf020, 0xf028,
		0x11000, 0x11014,
		0x11048, 0x1106c,
		0x11074, 0x11088,
		0x11098, 0x11120,
		0x1112c, 0x1117c,
		0x11190, 0x112e0,
		0x11300, 0x1130c,
		0x12000, 0x1206c,
		0x19040, 0x1906c,
		0x19078, 0x19080,
		0x1908c, 0x190e8,
		0x190f0, 0x190f8,
		0x19100, 0x19110,
		0x19120, 0x19124,
		0x19150, 0x19194,
		0x1919c, 0x191b0,
		0x191d0, 0x191e8,
		0x19238, 0x19290,
		0x192a4, 0x192b0,
		0x192bc, 0x192bc,
		0x19348, 0x1934c,
		0x193f8, 0x19418,
		0x19420, 0x19428,
		0x19430, 0x19444,
		0x1944c, 0x1946c,
		0x19474, 0x19474,
		0x19490, 0x194cc,
		0x194f0, 0x194f8,
		0x19c00, 0x19c48,
		0x19c50, 0x19c80,
		0x19c94, 0x19c98,
		0x19ca0, 0x19cbc,
		0x19ce4, 0x19ce4,
		0x19cf0, 0x19cf8,
		0x19d00, 0x19d28,
		0x19d50, 0x19d78,
		0x19d94, 0x19d98,
		0x19da0, 0x19dc8,
		0x19df0, 0x19e10,
		0x19e50, 0x19e6c,
		0x19ea0, 0x19ebc,
		0x19ec4, 0x19ef4,
		0x19f04, 0x19f2c,
		0x19f34, 0x19f34,
		0x19f40, 0x19f50,
		0x19f90, 0x19fac,
		0x19fc4, 0x19fc8,
		0x19fd0, 0x19fe4,
		0x1a000, 0x1a004,
		0x1a010, 0x1a06c,
		0x1a0b0, 0x1a0e4,
		0x1a0ec, 0x1a0f8,
		0x1a100, 0x1a108,
		0x1a114, 0x1a120,
		0x1a128, 0x1a130,
		0x1a138, 0x1a138,
		0x1a190, 0x1a1c4,
		0x1a1fc, 0x1a1fc,
		0x1e008, 0x1e00c,
		0x1e040, 0x1e044,
		0x1e04c, 0x1e04c,
		0x1e284, 0x1e290,
		0x1e2c0, 0x1e2c0,
		0x1e2e0, 0x1e2e0,
		0x1e300, 0x1e384,
		0x1e3c0, 0x1e3c8,
		0x1e408, 0x1e40c,
		0x1e440, 0x1e444,
		0x1e44c, 0x1e44c,
		0x1e684, 0x1e690,
		0x1e6c0, 0x1e6c0,
		0x1e6e0, 0x1e6e0,
		0x1e700, 0x1e784,
		0x1e7c0, 0x1e7c8,
		0x1e808, 0x1e80c,
		0x1e840, 0x1e844,
		0x1e84c, 0x1e84c,
		0x1ea84, 0x1ea90,
		0x1eac0, 0x1eac0,
		0x1eae0, 0x1eae0,
		0x1eb00, 0x1eb84,
		0x1ebc0, 0x1ebc8,
		0x1ec08, 0x1ec0c,
		0x1ec40, 0x1ec44,
		0x1ec4c, 0x1ec4c,
		0x1ee84, 0x1ee90,
		0x1eec0, 0x1eec0,
		0x1eee0, 0x1eee0,
		0x1ef00, 0x1ef84,
		0x1efc0, 0x1efc8,
		0x1f008, 0x1f00c,
		0x1f040, 0x1f044,
		0x1f04c, 0x1f04c,
		0x1f284, 0x1f290,
		0x1f2c0, 0x1f2c0,
		0x1f2e0, 0x1f2e0,
		0x1f300, 0x1f384,
		0x1f3c0, 0x1f3c8,
		0x1f408, 0x1f40c,
		0x1f440, 0x1f444,
		0x1f44c, 0x1f44c,
		0x1f684, 0x1f690,
		0x1f6c0, 0x1f6c0,
		0x1f6e0, 0x1f6e0,
		0x1f700, 0x1f784,
		0x1f7c0, 0x1f7c8,
		0x1f808, 0x1f80c,
		0x1f840, 0x1f844,
		0x1f84c, 0x1f84c,
		0x1fa84, 0x1fa90,
		0x1fac0, 0x1fac0,
		0x1fae0, 0x1fae0,
		0x1fb00, 0x1fb84,
		0x1fbc0, 0x1fbc8,
		0x1fc08, 0x1fc0c,
		0x1fc40, 0x1fc44,
		0x1fc4c, 0x1fc4c,
		0x1fe84, 0x1fe90,
		0x1fec0, 0x1fec0,
		0x1fee0, 0x1fee0,
		0x1ff00, 0x1ff84,
		0x1ffc0, 0x1ffc8,
		0x30000, 0x30030,
		0x30100, 0x30168,
		0x30190, 0x301a0,
		0x301a8, 0x301b8,
		0x301c4, 0x301c8,
		0x301d0, 0x301d0,
		0x30200, 0x30320,
		0x30400, 0x304b4,
		0x304c0, 0x3052c,
		0x30540, 0x3061c,
		0x30800, 0x308a0,
		0x308c0, 0x30908,
		0x30910, 0x309b8,
		0x30a00, 0x30a04,
		0x30a0c, 0x30a14,
		0x30a1c, 0x30a2c,
		0x30a44, 0x30a50,
		0x30a74, 0x30a74,
		0x30a7c, 0x30afc,
		0x30b08, 0x30c24,
		0x30d00, 0x30d14,
		0x30d1c, 0x30d3c,
		0x30d44, 0x30d4c,
		0x30d54, 0x30d74,
		0x30d7c, 0x30d7c,
		0x30de0, 0x30de0,
		0x30e00, 0x30ed4,
		0x30f00, 0x30fa4,
		0x30fc0, 0x30fc4,
		0x31000, 0x31004,
		0x31080, 0x310fc,
		0x31208, 0x31220,
		0x3123c, 0x31254,
		0x31300, 0x31300,
		0x31308, 0x3131c,
		0x31338, 0x3133c,
		0x31380, 0x31380,
		0x31388, 0x313a8,
		0x313b4, 0x313b4,
		0x31400, 0x31420,
		0x31438, 0x3143c,
		0x31480, 0x31480,
		0x314a8, 0x314a8,
		0x314b0, 0x314b4,
		0x314c8, 0x314d4,
		0x31a40, 0x31a4c,
		0x31af0, 0x31b20,
		0x31b38, 0x31b3c,
		0x31b80, 0x31b80,
		0x31ba8, 0x31ba8,
		0x31bb0, 0x31bb4,
		0x31bc8, 0x31bd4,
		0x32140, 0x3218c,
		0x321f0, 0x321f4,
		0x32200, 0x32200,
		0x32218, 0x32218,
		0x32400, 0x32400,
		0x32408, 0x3241c,
		0x32618, 0x32620,
		0x32664, 0x32664,
		0x326a8, 0x326a8,
		0x326ec, 0x326ec,
		0x32a00, 0x32abc,
		0x32b00, 0x32b38,
		0x32b20, 0x32b38,
		0x32b40, 0x32b58,
		0x32b60, 0x32b78,
		0x32c00, 0x32c00,
		0x32c08, 0x32c3c,
		0x33000, 0x3302c,
		0x33034, 0x33050,
		0x33058, 0x33058,
		0x33060, 0x3308c,
		0x3309c, 0x330ac,
		0x330c0, 0x330c0,
		0x330c8, 0x330d0,
		0x330d8, 0x330e0,
		0x330ec, 0x3312c,
		0x33134, 0x33150,
		0x33158, 0x33158,
		0x33160, 0x3318c,
		0x3319c, 0x331ac,
		0x331c0, 0x331c0,
		0x331c8, 0x331d0,
		0x331d8, 0x331e0,
		0x331ec, 0x33290,
		0x33298, 0x332c4,
		0x332e4, 0x33390,
		0x33398, 0x333c4,
		0x333e4, 0x3342c,
		0x33434, 0x33450,
		0x33458, 0x33458,
		0x33460, 0x3348c,
		0x3349c, 0x334ac,
		0x334c0, 0x334c0,
		0x334c8, 0x334d0,
		0x334d8, 0x334e0,
		0x334ec, 0x3352c,
		0x33534, 0x33550,
		0x33558, 0x33558,
		0x33560, 0x3358c,
		0x3359c, 0x335ac,
		0x335c0, 0x335c0,
		0x335c8, 0x335d0,
		0x335d8, 0x335e0,
		0x335ec, 0x33690,
		0x33698, 0x336c4,
		0x336e4, 0x33790,
		0x33798, 0x337c4,
		0x337e4, 0x337fc,
		0x33814, 0x33814,
		0x33854, 0x33868,
		0x33880, 0x3388c,
		0x338c0, 0x338d0,
		0x338e8, 0x338ec,
		0x33900, 0x3392c,
		0x33934, 0x33950,
		0x33958, 0x33958,
		0x33960, 0x3398c,
		0x3399c, 0x339ac,
		0x339c0, 0x339c0,
		0x339c8, 0x339d0,
		0x339d8, 0x339e0,
		0x339ec, 0x33a90,
		0x33a98, 0x33ac4,
		0x33ae4, 0x33b10,
		0x33b24, 0x33b28,
		0x33b38, 0x33b50,
		0x33bf0, 0x33c10,
		0x33c24, 0x33c28,
		0x33c38, 0x33c50,
		0x33cf0, 0x33cfc,
		0x34000, 0x34030,
		0x34100, 0x34168,
		0x34190, 0x341a0,
		0x341a8, 0x341b8,
		0x341c4, 0x341c8,
		0x341d0, 0x341d0,
		0x34200, 0x34320,
		0x34400, 0x344b4,
		0x344c0, 0x3452c,
		0x34540, 0x3461c,
		0x34800, 0x348a0,
		0x348c0, 0x34908,
		0x34910, 0x349b8,
		0x34a00, 0x34a04,
		0x34a0c, 0x34a14,
		0x34a1c, 0x34a2c,
		0x34a44, 0x34a50,
		0x34a74, 0x34a74,
		0x34a7c, 0x34afc,
		0x34b08, 0x34c24,
		0x34d00, 0x34d14,
		0x34d1c, 0x34d3c,
		0x34d44, 0x34d4c,
		0x34d54, 0x34d74,
		0x34d7c, 0x34d7c,
		0x34de0, 0x34de0,
		0x34e00, 0x34ed4,
		0x34f00, 0x34fa4,
		0x34fc0, 0x34fc4,
		0x35000, 0x35004,
		0x35080, 0x350fc,
		0x35208, 0x35220,
		0x3523c, 0x35254,
		0x35300, 0x35300,
		0x35308, 0x3531c,
		0x35338, 0x3533c,
		0x35380, 0x35380,
		0x35388, 0x353a8,
		0x353b4, 0x353b4,
		0x35400, 0x35420,
		0x35438, 0x3543c,
		0x35480, 0x35480,
		0x354a8, 0x354a8,
		0x354b0, 0x354b4,
		0x354c8, 0x354d4,
		0x35a40, 0x35a4c,
		0x35af0, 0x35b20,
		0x35b38, 0x35b3c,
		0x35b80, 0x35b80,
		0x35ba8, 0x35ba8,
		0x35bb0, 0x35bb4,
		0x35bc8, 0x35bd4,
		0x36140, 0x3618c,
		0x361f0, 0x361f4,
		0x36200, 0x36200,
		0x36218, 0x36218,
		0x36400, 0x36400,
		0x36408, 0x3641c,
		0x36618, 0x36620,
		0x36664, 0x36664,
		0x366a8, 0x366a8,
		0x366ec, 0x366ec,
		0x36a00, 0x36abc,
		0x36b00, 0x36b38,
		0x36b20, 0x36b38,
		0x36b40, 0x36b58,
		0x36b60, 0x36b78,
		0x36c00, 0x36c00,
		0x36c08, 0x36c3c,
		0x37000, 0x3702c,
		0x37034, 0x37050,
		0x37058, 0x37058,
		0x37060, 0x3708c,
		0x3709c, 0x370ac,
		0x370c0, 0x370c0,
		0x370c8, 0x370d0,
		0x370d8, 0x370e0,
		0x370ec, 0x3712c,
		0x37134, 0x37150,
		0x37158, 0x37158,
		0x37160, 0x3718c,
		0x3719c, 0x371ac,
		0x371c0, 0x371c0,
		0x371c8, 0x371d0,
		0x371d8, 0x371e0,
		0x371ec, 0x37290,
		0x37298, 0x372c4,
		0x372e4, 0x37390,
		0x37398, 0x373c4,
		0x373e4, 0x3742c,
		0x37434, 0x37450,
		0x37458, 0x37458,
		0x37460, 0x3748c,
		0x3749c, 0x374ac,
		0x374c0, 0x374c0,
		0x374c8, 0x374d0,
		0x374d8, 0x374e0,
		0x374ec, 0x3752c,
		0x37534, 0x37550,
		0x37558, 0x37558,
		0x37560, 0x3758c,
		0x3759c, 0x375ac,
		0x375c0, 0x375c0,
		0x375c8, 0x375d0,
		0x375d8, 0x375e0,
		0x375ec, 0x37690,
		0x37698, 0x376c4,
		0x376e4, 0x37790,
		0x37798, 0x377c4,
		0x377e4, 0x377fc,
		0x37814, 0x37814,
		0x37854, 0x37868,
		0x37880, 0x3788c,
		0x378c0, 0x378d0,
		0x378e8, 0x378ec,
		0x37900, 0x3792c,
		0x37934, 0x37950,
		0x37958, 0x37958,
		0x37960, 0x3798c,
		0x3799c, 0x379ac,
		0x379c0, 0x379c0,
		0x379c8, 0x379d0,
		0x379d8, 0x379e0,
		0x379ec, 0x37a90,
		0x37a98, 0x37ac4,
		0x37ae4, 0x37b10,
		0x37b24, 0x37b28,
		0x37b38, 0x37b50,
		0x37bf0, 0x37c10,
		0x37c24, 0x37c28,
		0x37c38, 0x37c50,
		0x37cf0, 0x37cfc,
		0x40040, 0x40040,
		0x40080, 0x40084,
		0x40100, 0x40100,
		0x40140, 0x401bc,
		0x40200, 0x40214,
		0x40228, 0x40228,
		0x40240, 0x40258,
		0x40280, 0x40280,
		0x40304, 0x40304,
		0x40330, 0x4033c,
		0x41304, 0x413c8,
		0x413d0, 0x413dc,
		0x413f0, 0x413f0,
		0x41400, 0x4140c,
		0x41414, 0x4141c,
		0x41480, 0x414d0,
		0x44000, 0x4407c,
		0x440c0, 0x441ac,
		0x441b4, 0x4427c,
		0x442c0, 0x443ac,
		0x443b4, 0x4447c,
		0x444c0, 0x445ac,
		0x445b4, 0x4467c,
		0x446c0, 0x447ac,
		0x447b4, 0x4487c,
		0x448c0, 0x449ac,
		0x449b4, 0x44a7c,
		0x44ac0, 0x44bac,
		0x44bb4, 0x44c7c,
		0x44cc0, 0x44dac,
		0x44db4, 0x44e7c,
		0x44ec0, 0x44fac,
		0x44fb4, 0x4507c,
		0x450c0, 0x451ac,
		0x451b4, 0x451fc,
		0x45800, 0x45804,
		0x45810, 0x45830,
		0x45840, 0x45860,
		0x45868, 0x45868,
		0x45880, 0x45884,
		0x458a0, 0x458b0,
		0x45a00, 0x45a04,
		0x45a10, 0x45a30,
		0x45a40, 0x45a60,
		0x45a68, 0x45a68,
		0x45a80, 0x45a84,
		0x45aa0, 0x45ab0,
		0x460c0, 0x460e4,
		0x47000, 0x4703c,
		0x47044, 0x4708c,
		0x47200, 0x47250,
		0x47400, 0x47408,
		0x47414, 0x47420,
		0x47600, 0x47618,
		0x47800, 0x47814,
		0x47820, 0x4782c,
		0x50000, 0x50084,
		0x50090, 0x500cc,
		0x50300, 0x50384,
		0x50400, 0x50400,
		0x50800, 0x50884,
		0x50890, 0x508cc,
		0x50b00, 0x50b84,
		0x50c00, 0x50c00,
		0x51000, 0x51020,
		0x51028, 0x510b0,
		0x51300, 0x51324,
	};

	u32 *buf_end = (u32 *)((char *)buf + buf_size);
	const unsigned int *reg_ranges;
	int reg_ranges_size, range;
	unsigned int chip_version = CHELSIO_CHIP_VERSION(adap->params.chip);

	/* Select the right set of register ranges to dump depending on the
	 * adapter chip type.
	 */
	switch (chip_version) {
	case CHELSIO_T5:
		reg_ranges = t5_reg_ranges;
		reg_ranges_size = ARRAY_SIZE(t5_reg_ranges);
		break;

	case CHELSIO_T6:
		reg_ranges = t6_reg_ranges;
		reg_ranges_size = ARRAY_SIZE(t6_reg_ranges);
		break;

	default:
		dev_err(adap,
			"Unsupported chip version %d\n", chip_version);
		return;
	}

	/* Clear the register buffer and insert the appropriate register
	 * values selected by the above register ranges.
	 */
	memset(buf, 0, buf_size);
	for (range = 0; range < reg_ranges_size; range += 2) {
		unsigned int reg = reg_ranges[range];
		unsigned int last_reg = reg_ranges[range + 1];
		u32 *bufp = (u32 *)((char *)buf + reg);

		/* Iterate across the register range filling in the register
		 * buffer but don't write past the end of the register buffer.
		 */
		while (reg <= last_reg && bufp < buf_end) {
			*bufp++ = t4_read_reg(adap, reg);
			reg += sizeof(u32);
		}
	}
}

/* EEPROM reads take a few tens of us while writes can take a bit over 5 ms. */
#define EEPROM_DELAY            10              /* 10us per poll spin */
#define EEPROM_MAX_POLL         5000            /* x 5000 == 50ms */

#define EEPROM_STAT_ADDR        0x7bfc

/**
 * Small utility function to wait till any outstanding VPD Access is complete.
 * We have a per-adapter state variable "VPD Busy" to indicate when we have a
 * VPD Access in flight.  This allows us to handle the problem of having a
 * previous VPD Access time out and prevent an attempt to inject a new VPD
 * Request before any in-flight VPD request has completed.
 */
static int t4_seeprom_wait(struct adapter *adapter)
{
	unsigned int base = adapter->params.pci.vpd_cap_addr;
	int max_poll;

	/* If no VPD Access is in flight, we can just return success right
	 * away.
	 */
	if (!adapter->vpd_busy)
		return 0;

	/* Poll the VPD Capability Address/Flag register waiting for it
	 * to indicate that the operation is complete.
	 */
	max_poll = EEPROM_MAX_POLL;
	do {
		u16 val;

		udelay(EEPROM_DELAY);
		t4_os_pci_read_cfg2(adapter, base + PCI_VPD_ADDR, &val);

		/* If the operation is complete, mark the VPD as no longer
		 * busy and return success.
		 */
		if ((val & PCI_VPD_ADDR_F) == adapter->vpd_flag) {
			adapter->vpd_busy = 0;
			return 0;
		}
	} while (--max_poll);

	/* Failure!  Note that we leave the VPD Busy status set in order to
	 * avoid pushing a new VPD Access request into the VPD Capability till
	 * the current operation eventually succeeds.  It's a bug to issue a
	 * new request when an existing request is in flight and will result
	 * in corrupt hardware state.
	 */
	return -ETIMEDOUT;
}

/**
 * t4_seeprom_read - read a serial EEPROM location
 * @adapter: adapter to read
 * @addr: EEPROM virtual address
 * @data: where to store the read data
 *
 * Read a 32-bit word from a location in serial EEPROM using the card's PCI
 * VPD capability.  Note that this function must be called with a virtual
 * address.
 */
int t4_seeprom_read(struct adapter *adapter, u32 addr, u32 *data)
{
	unsigned int base = adapter->params.pci.vpd_cap_addr;
	int ret;

	/* VPD Accesses must alway be 4-byte aligned!
	 */
	if (addr >= EEPROMVSIZE || (addr & 3))
		return -EINVAL;

	/* Wait for any previous operation which may still be in flight to
	 * complete.
	 */
	ret = t4_seeprom_wait(adapter);
	if (ret) {
		dev_err(adapter, "VPD still busy from previous operation\n");
		return ret;
	}

	/* Issue our new VPD Read request, mark the VPD as being busy and wait
	 * for our request to complete.  If it doesn't complete, note the
	 * error and return it to our caller.  Note that we do not reset the
	 * VPD Busy status!
	 */
	t4_os_pci_write_cfg2(adapter, base + PCI_VPD_ADDR, (u16)addr);
	adapter->vpd_busy = 1;
	adapter->vpd_flag = PCI_VPD_ADDR_F;
	ret = t4_seeprom_wait(adapter);
	if (ret) {
		dev_err(adapter, "VPD read of address %#x failed\n", addr);
		return ret;
	}

	/* Grab the returned data, swizzle it into our endianness and
	 * return success.
	 */
	t4_os_pci_read_cfg4(adapter, base + PCI_VPD_DATA, data);
	*data = le32_to_cpu(*data);
	return 0;
}

/**
 * t4_seeprom_write - write a serial EEPROM location
 * @adapter: adapter to write
 * @addr: virtual EEPROM address
 * @data: value to write
 *
 * Write a 32-bit word to a location in serial EEPROM using the card's PCI
 * VPD capability.  Note that this function must be called with a virtual
 * address.
 */
int t4_seeprom_write(struct adapter *adapter, u32 addr, u32 data)
{
	unsigned int base = adapter->params.pci.vpd_cap_addr;
	int ret;
	u32 stats_reg = 0;
	int max_poll;

	/* VPD Accesses must alway be 4-byte aligned!
	 */
	if (addr >= EEPROMVSIZE || (addr & 3))
		return -EINVAL;

	/* Wait for any previous operation which may still be in flight to
	 * complete.
	 */
	ret = t4_seeprom_wait(adapter);
	if (ret) {
		dev_err(adapter, "VPD still busy from previous operation\n");
		return ret;
	}

	/* Issue our new VPD Read request, mark the VPD as being busy and wait
	 * for our request to complete.  If it doesn't complete, note the
	 * error and return it to our caller.  Note that we do not reset the
	 * VPD Busy status!
	 */
	t4_os_pci_write_cfg4(adapter, base + PCI_VPD_DATA,
			     cpu_to_le32(data));
	t4_os_pci_write_cfg2(adapter, base + PCI_VPD_ADDR,
			     (u16)addr | PCI_VPD_ADDR_F);
	adapter->vpd_busy = 1;
	adapter->vpd_flag = 0;
	ret = t4_seeprom_wait(adapter);
	if (ret) {
		dev_err(adapter, "VPD write of address %#x failed\n", addr);
		return ret;
	}

	/* Reset PCI_VPD_DATA register after a transaction and wait for our
	 * request to complete. If it doesn't complete, return error.
	 */
	t4_os_pci_write_cfg4(adapter, base + PCI_VPD_DATA, 0);
	max_poll = EEPROM_MAX_POLL;
	do {
		udelay(EEPROM_DELAY);
		t4_seeprom_read(adapter, EEPROM_STAT_ADDR, &stats_reg);
	} while ((stats_reg & 0x1) && --max_poll);
	if (!max_poll)
		return -ETIMEDOUT;

	/* Return success! */
	return 0;
}

/**
 * t4_seeprom_wp - enable/disable EEPROM write protection
 * @adapter: the adapter
 * @enable: whether to enable or disable write protection
 *
 * Enables or disables write protection on the serial EEPROM.
 */
int t4_seeprom_wp(struct adapter *adapter, int enable)
{
	return t4_seeprom_write(adapter, EEPROM_STAT_ADDR, enable ? 0xc : 0);
}

/**
 * t4_fw_tp_pio_rw - Access TP PIO through LDST
 * @adap: the adapter
 * @vals: where the indirect register values are stored/written
 * @nregs: how many indirect registers to read/write
 * @start_idx: index of first indirect register to read/write
 * @rw: Read (1) or Write (0)
 *
 * Access TP PIO registers through LDST
 */
void t4_fw_tp_pio_rw(struct adapter *adap, u32 *vals, unsigned int nregs,
		     unsigned int start_index, unsigned int rw)
{
	int cmd = FW_LDST_ADDRSPC_TP_PIO;
	struct fw_ldst_cmd c;
	unsigned int i;
	int ret;

	for (i = 0 ; i < nregs; i++) {
		memset(&c, 0, sizeof(c));
		c.op_to_addrspace = cpu_to_be32(V_FW_CMD_OP(FW_LDST_CMD) |
						F_FW_CMD_REQUEST |
						(rw ? F_FW_CMD_READ :
						      F_FW_CMD_WRITE) |
						V_FW_LDST_CMD_ADDRSPACE(cmd));
		c.cycles_to_len16 = cpu_to_be32(FW_LEN16(c));

		c.u.addrval.addr = cpu_to_be32(start_index + i);
		c.u.addrval.val  = rw ? 0 : cpu_to_be32(vals[i]);
		ret = t4_wr_mbox(adap, adap->mbox, &c, sizeof(c), &c);
		if (ret == 0) {
			if (rw)
				vals[i] = be32_to_cpu(c.u.addrval.val);
		}
	}
}

/**
 * t4_read_rss_key - read the global RSS key
 * @adap: the adapter
 * @key: 10-entry array holding the 320-bit RSS key
 *
 * Reads the global 320-bit RSS key.
 */
void t4_read_rss_key(struct adapter *adap, u32 *key)
{
	t4_fw_tp_pio_rw(adap, key, 10, A_TP_RSS_SECRET_KEY0, 1);
}

/**
 * t4_write_rss_key - program one of the RSS keys
 * @adap: the adapter
 * @key: 10-entry array holding the 320-bit RSS key
 * @idx: which RSS key to write
 *
 * Writes one of the RSS keys with the given 320-bit value.  If @idx is
 * 0..15 the corresponding entry in the RSS key table is written,
 * otherwise the global RSS key is written.
 */
void t4_write_rss_key(struct adapter *adap, u32 *key, int idx)
{
	u32 vrt = t4_read_reg(adap, A_TP_RSS_CONFIG_VRT);
	u8 rss_key_addr_cnt = 16;

	/* T6 and later: for KeyMode 3 (per-vf and per-vf scramble),
	 * allows access to key addresses 16-63 by using KeyWrAddrX
	 * as index[5:4](upper 2) into key table
	 */
	if ((CHELSIO_CHIP_VERSION(adap->params.chip) > CHELSIO_T5) &&
	    (vrt & F_KEYEXTEND) && (G_KEYMODE(vrt) == 3))
		rss_key_addr_cnt = 32;

	t4_fw_tp_pio_rw(adap, key, 10, A_TP_RSS_SECRET_KEY0, 0);

	if (idx >= 0 && idx < rss_key_addr_cnt) {
		if (rss_key_addr_cnt > 16)
			t4_write_reg(adap, A_TP_RSS_CONFIG_VRT,
				     V_KEYWRADDRX(idx >> 4) |
				     V_T6_VFWRADDR(idx) | F_KEYWREN);
		else
			t4_write_reg(adap, A_TP_RSS_CONFIG_VRT,
				     V_KEYWRADDR(idx) | F_KEYWREN);
	}
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
		if (is_pf4(adapter))
			ret = t4_wr_mbox(adapter, mbox, &cmd, sizeof(cmd),
					 NULL);
		else
			ret = t4vf_wr_mbox(adapter, &cmd, sizeof(cmd), NULL);
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
	if (is_pf4(adapter))
		return t4_wr_mbox(adapter, mbox, &c, sizeof(c), NULL);
	else
		return t4vf_wr_mbox(adapter, &c, sizeof(c), NULL);
}

/**
 * t4_read_config_vi_rss - read the configured per VI RSS settings
 * @adapter: the adapter
 * @mbox: mbox to use for the FW command
 * @viid: the VI id
 * @flags: where to place the configured flags
 * @defq: where to place the id of the default RSS queue for the VI.
 *
 * Read configured VI-specific RSS properties.
 */
int t4_read_config_vi_rss(struct adapter *adapter, int mbox, unsigned int viid,
			  u64 *flags, unsigned int *defq)
{
	struct fw_rss_vi_config_cmd c;
	unsigned int result;
	int ret;

	memset(&c, 0, sizeof(c));
	c.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_RSS_VI_CONFIG_CMD) |
				   F_FW_CMD_REQUEST | F_FW_CMD_READ |
				   V_FW_RSS_VI_CONFIG_CMD_VIID(viid));
	c.retval_len16 = cpu_to_be32(FW_LEN16(c));
	ret = t4_wr_mbox(adapter, mbox, &c, sizeof(c), &c);
	if (!ret) {
		result = be32_to_cpu(c.u.basicvirtual.defaultq_to_udpen);
		if (defq)
			*defq = G_FW_RSS_VI_CONFIG_CMD_DEFAULTQ(result);
		if (flags)
			*flags = result & M_FW_RSS_VI_CONFIG_CMD_DEFAULTQ;
	}

	return ret;
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

/**
 * t4_get_pfres - retrieve VF resource limits
 * @adapter: the adapter
 *
 * Retrieves configured resource limits and capabilities for a physical
 * function.  The results are stored in @adapter->pfres.
 */
int t4_get_pfres(struct adapter *adapter)
{
	struct pf_resources *pfres = &adapter->params.pfres;
	struct fw_pfvf_cmd cmd, rpl;
	u32 word;
	int v;

	/*
	 * Execute PFVF Read command to get VF resource limits; bail out early
	 * with error on command failure.
	 */
	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_PFVF_CMD) |
				    F_FW_CMD_REQUEST |
				    F_FW_CMD_READ |
				    V_FW_PFVF_CMD_PFN(adapter->pf) |
				    V_FW_PFVF_CMD_VFN(0));
	cmd.retval_len16 = cpu_to_be32(FW_LEN16(cmd));
	v = t4_wr_mbox(adapter, adapter->mbox, &cmd, sizeof(cmd), &rpl);
	if (v != FW_SUCCESS)
		return v;

	/*
	 * Extract PF resource limits and return success.
	 */
	word = be32_to_cpu(rpl.niqflint_niq);
	pfres->niqflint = G_FW_PFVF_CMD_NIQFLINT(word);

	word = be32_to_cpu(rpl.type_to_neq);
	pfres->neq = G_FW_PFVF_CMD_NEQ(word);

	word = be32_to_cpu(rpl.r_caps_to_nethctrl);
	pfres->nethctrl = G_FW_PFVF_CMD_NETHCTRL(word);

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
 * t4_get_exprom_version - return the Expansion ROM version (if any)
 * @adapter: the adapter
 * @vers: where to place the version
 *
 * Reads the Expansion ROM header from FLASH and returns the version
 * number (if present) through the @vers return value pointer.  We return
 * this in the Firmware Version Format since it's convenient.  Return
 * 0 on success, -ENOENT if no Expansion ROM is present.
 */
static int t4_get_exprom_version(struct adapter *adapter, u32 *vers)
{
	struct exprom_header {
		unsigned char hdr_arr[16];      /* must start with 0x55aa */
		unsigned char hdr_ver[4];       /* Expansion ROM version */
	} *hdr;
	u32 exprom_header_buf[DIV_ROUND_UP(sizeof(struct exprom_header),
					   sizeof(u32))];
	int ret;

	ret = t4_read_flash(adapter, FLASH_EXP_ROM_START,
			    ARRAY_SIZE(exprom_header_buf),
			    exprom_header_buf, 0);
	if (ret)
		return ret;

	hdr = (struct exprom_header *)exprom_header_buf;
	if (hdr->hdr_arr[0] != 0x55 || hdr->hdr_arr[1] != 0xaa)
		return -ENOENT;

	*vers = (V_FW_HDR_FW_VER_MAJOR(hdr->hdr_ver[0]) |
		 V_FW_HDR_FW_VER_MINOR(hdr->hdr_ver[1]) |
		 V_FW_HDR_FW_VER_MICRO(hdr->hdr_ver[2]) |
		 V_FW_HDR_FW_VER_BUILD(hdr->hdr_ver[3]));
	return 0;
}

/**
 * t4_get_fw_version - read the firmware version
 * @adapter: the adapter
 * @vers: where to place the version
 *
 * Reads the FW version from flash.
 */
static int t4_get_fw_version(struct adapter *adapter, u32 *vers)
{
	return t4_read_flash(adapter, FLASH_FW_START +
			     offsetof(struct fw_hdr, fw_ver), 1, vers, 0);
}

/**
 *     t4_get_bs_version - read the firmware bootstrap version
 *     @adapter: the adapter
 *     @vers: where to place the version
 *
 *     Reads the FW Bootstrap version from flash.
 */
static int t4_get_bs_version(struct adapter *adapter, u32 *vers)
{
	return t4_read_flash(adapter, FLASH_FWBOOTSTRAP_START +
			     offsetof(struct fw_hdr, fw_ver), 1,
			     vers, 0);
}

/**
 * t4_get_tp_version - read the TP microcode version
 * @adapter: the adapter
 * @vers: where to place the version
 *
 * Reads the TP microcode version from flash.
 */
static int t4_get_tp_version(struct adapter *adapter, u32 *vers)
{
	return t4_read_flash(adapter, FLASH_FW_START +
			     offsetof(struct fw_hdr, tp_microcode_ver),
			     1, vers, 0);
}

/**
 * t4_get_version_info - extract various chip/firmware version information
 * @adapter: the adapter
 *
 * Reads various chip/firmware version numbers and stores them into the
 * adapter Adapter Parameters structure.  If any of the efforts fails
 * the first failure will be returned, but all of the version numbers
 * will be read.
 */
int t4_get_version_info(struct adapter *adapter)
{
	int ret = 0;

#define FIRST_RET(__getvinfo) \
	do { \
		int __ret = __getvinfo; \
		if (__ret && !ret) \
			ret = __ret; \
	} while (0)

	FIRST_RET(t4_get_fw_version(adapter, &adapter->params.fw_vers));
	FIRST_RET(t4_get_bs_version(adapter, &adapter->params.bs_vers));
	FIRST_RET(t4_get_tp_version(adapter, &adapter->params.tp_vers));
	FIRST_RET(t4_get_exprom_version(adapter, &adapter->params.er_vers));

#undef FIRST_RET

	return ret;
}

/**
 * t4_dump_version_info - dump all of the adapter configuration IDs
 * @adapter: the adapter
 *
 * Dumps all of the various bits of adapter configuration version/revision
 * IDs information.  This is typically called at some point after
 * t4_get_version_info() has been called.
 */
void t4_dump_version_info(struct adapter *adapter)
{
	/**
	 * Device information.
	 */
	dev_info(adapter, "Chelsio rev %d\n",
		 CHELSIO_CHIP_RELEASE(adapter->params.chip));

	/**
	 * Firmware Version.
	 */
	if (!adapter->params.fw_vers)
		dev_warn(adapter, "No firmware loaded\n");
	else
		dev_info(adapter, "Firmware version: %u.%u.%u.%u\n",
			 G_FW_HDR_FW_VER_MAJOR(adapter->params.fw_vers),
			 G_FW_HDR_FW_VER_MINOR(adapter->params.fw_vers),
			 G_FW_HDR_FW_VER_MICRO(adapter->params.fw_vers),
			 G_FW_HDR_FW_VER_BUILD(adapter->params.fw_vers));

	/**
	 * Bootstrap Firmware Version.
	 */
	if (!adapter->params.bs_vers)
		dev_warn(adapter, "No bootstrap loaded\n");
	else
		dev_info(adapter, "Bootstrap version: %u.%u.%u.%u\n",
			 G_FW_HDR_FW_VER_MAJOR(adapter->params.bs_vers),
			 G_FW_HDR_FW_VER_MINOR(adapter->params.bs_vers),
			 G_FW_HDR_FW_VER_MICRO(adapter->params.bs_vers),
			 G_FW_HDR_FW_VER_BUILD(adapter->params.bs_vers));

	/**
	 * TP Microcode Version.
	 */
	if (!adapter->params.tp_vers)
		dev_warn(adapter, "No TP Microcode loaded\n");
	else
		dev_info(adapter, "TP Microcode version: %u.%u.%u.%u\n",
			 G_FW_HDR_FW_VER_MAJOR(adapter->params.tp_vers),
			 G_FW_HDR_FW_VER_MINOR(adapter->params.tp_vers),
			 G_FW_HDR_FW_VER_MICRO(adapter->params.tp_vers),
			 G_FW_HDR_FW_VER_BUILD(adapter->params.tp_vers));

	/**
	 * Expansion ROM version.
	 */
	if (!adapter->params.er_vers)
		dev_info(adapter, "No Expansion ROM loaded\n");
	else
		dev_info(adapter, "Expansion ROM version: %u.%u.%u.%u\n",
			 G_FW_HDR_FW_VER_MAJOR(adapter->params.er_vers),
			 G_FW_HDR_FW_VER_MINOR(adapter->params.er_vers),
			 G_FW_HDR_FW_VER_MICRO(adapter->params.er_vers),
			 G_FW_HDR_FW_VER_BUILD(adapter->params.er_vers));
}

/**
 * t4_link_l1cfg_core - apply link configuration to MAC/PHY
 * @pi: the port info
 * @caps: link capabilities to configure
 * @sleep_ok: if true we may sleep while awaiting command completion
 *
 * Set up a port's MAC and PHY according to a desired link configuration.
 * - If the PHY can auto-negotiate first decide what to advertise, then
 *   enable/disable auto-negotiation as desired, and reset.
 * - If the PHY does not auto-negotiate just reset it.
 * - If auto-negotiation is off set the MAC to the proper speed/duplex/FC,
 *   otherwise do it later based on the outcome of auto-negotiation.
 */
int t4_link_l1cfg_core(struct port_info *pi, u32 caps, u8 sleep_ok)
{
	struct link_config *lc = &pi->link_cfg;
	struct adapter *adap = pi->adapter;
	struct fw_port_cmd cmd;
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_portid = cpu_to_be32(V_FW_CMD_OP(FW_PORT_CMD) |
				       F_FW_CMD_REQUEST | F_FW_CMD_EXEC |
				       V_FW_PORT_CMD_PORTID(pi->port_id));
	cmd.action_to_len16 =
		cpu_to_be32(V_FW_PORT_CMD_ACTION(FW_PORT_ACTION_L1_CFG32) |
			    FW_LEN16(cmd));

	cmd.u.l1cfg32.rcap32 = cpu_to_be32(caps);

	if (sleep_ok)
		ret = t4_wr_mbox(adap, adap->mbox, &cmd, sizeof(cmd), NULL);
	else
		ret = t4_wr_mbox_ns(adap, adap->mbox, &cmd, sizeof(cmd), NULL);

	if (ret == FW_SUCCESS)
		lc->link_caps = caps;
	else
		dev_err(adap,
			"Requested Port Capabilities %#x rejected, error %d\n",
			caps, ret);

	return ret;
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
	u32 whoami = t4_read_reg(adapter, A_PL_WHOAMI);
	u32 pf = CHELSIO_CHIP_VERSION(adapter->params.chip) <= CHELSIO_T5 ?
		 G_SOURCEPF(whoami) : G_T6_SOURCEPF(whoami);

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
	u32 whoami = t4_read_reg(adapter, A_PL_WHOAMI);
	u32 pf = CHELSIO_CHIP_VERSION(adapter->params.chip) <= CHELSIO_T5 ?
		 G_SOURCEPF(whoami) : G_T6_SOURCEPF(whoami);

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
		"KR4_100G",
		"CR4_QSFP",
		"CR_QSFP",
		"CR2_QSFP",
		"SFP28",
		"KR_SFP28",
	};

	if (port_type < ARRAY_SIZE(port_type_description))
		return port_type_description[port_type];
	return "UNKNOWN";
}

/**
 * t4_get_mps_bg_map - return the buffer groups associated with a port
 * @adap: the adapter
 * @pidx: the port index
 *
 * Returns a bitmap indicating which MPS buffer groups are associated
 * with the given port.  Bit i is set if buffer group i is used by the
 * port.
 */
unsigned int t4_get_mps_bg_map(struct adapter *adap, unsigned int pidx)
{
	unsigned int chip_version = CHELSIO_CHIP_VERSION(adap->params.chip);
	unsigned int nports = 1 << G_NUMPORTS(t4_read_reg(adap,
							  A_MPS_CMN_CTL));

	if (pidx >= nports) {
		dev_warn(adap, "MPS Port Index %d >= Nports %d\n",
			 pidx, nports);
		return 0;
	}

	switch (chip_version) {
	case CHELSIO_T4:
	case CHELSIO_T5:
		switch (nports) {
		case 1: return 0xf;
		case 2: return 3 << (2 * pidx);
		case 4: return 1 << pidx;
		}
		break;

	case CHELSIO_T6:
		switch (nports) {
		case 2: return 1 << (2 * pidx);
		}
		break;
	}

	dev_err(adap, "Need MPS Buffer Group Map for Chip %0x, Nports %d\n",
		chip_version, nports);
	return 0;
}

/**
 * t4_get_tp_ch_map - return TP ingress channels associated with a port
 * @adapter: the adapter
 * @pidx: the port index
 *
 * Returns a bitmap indicating which TP Ingress Channels are associated with
 * a given Port.  Bit i is set if TP Ingress Channel i is used by the Port.
 */
unsigned int t4_get_tp_ch_map(struct adapter *adapter, unsigned int pidx)
{
	unsigned int chip_version = CHELSIO_CHIP_VERSION(adapter->params.chip);
	unsigned int nports = 1 << G_NUMPORTS(t4_read_reg(adapter,
							  A_MPS_CMN_CTL));

	if (pidx >= nports) {
		dev_warn(adap, "TP Port Index %d >= Nports %d\n",
			 pidx, nports);
		return 0;
	}

	switch (chip_version) {
	case CHELSIO_T4:
	case CHELSIO_T5:
		/* Note that this happens to be the same values as the MPS
		 * Buffer Group Map for these Chips.  But we replicate the code
		 * here because they're really separate concepts.
		 */
		switch (nports) {
		case 1: return 0xf;
		case 2: return 3 << (2 * pidx);
		case 4: return 1 << pidx;
		}
		break;

	case CHELSIO_T6:
		switch (nports) {
		case 2: return 1 << pidx;
		}
		break;
	}

	dev_err(adapter, "Need TP Channel Map for Chip %0x, Nports %d\n",
		chip_version, nports);
	return 0;
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
	u32 stat_ctl = t4_read_reg(adap, A_MPS_STAT_CTL);
	u32 bgmap = t4_get_mps_bg_map(adap, idx);
	u32 val[NCHAN] = { 0 };
	u8 i;

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

	if (CHELSIO_CHIP_VERSION(adap->params.chip) >= CHELSIO_T5) {
		if (stat_ctl & F_COUNTPAUSESTATTX) {
			p->tx_frames -= p->tx_pause;
			p->tx_octets -= p->tx_pause * 64;
		}
		if (stat_ctl & F_COUNTPAUSEMCTX)
			p->tx_mcast_frames -= p->tx_pause;
	}

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

	if (CHELSIO_CHIP_VERSION(adap->params.chip) >= CHELSIO_T5) {
		if (stat_ctl & F_COUNTPAUSESTATRX) {
			p->rx_frames -= p->rx_pause;
			p->rx_octets -= p->rx_pause * 64;
		}
		if (stat_ctl & F_COUNTPAUSEMCRX)
			p->rx_mcast_frames -= p->rx_pause;
	}

	p->rx_ovflow0 = (bgmap & 1) ? GET_STAT_COM(RX_BG_0_MAC_DROP_FRAME) : 0;
	p->rx_ovflow1 = (bgmap & 2) ? GET_STAT_COM(RX_BG_1_MAC_DROP_FRAME) : 0;
	p->rx_ovflow2 = (bgmap & 4) ? GET_STAT_COM(RX_BG_2_MAC_DROP_FRAME) : 0;
	p->rx_ovflow3 = (bgmap & 8) ? GET_STAT_COM(RX_BG_3_MAC_DROP_FRAME) : 0;
	p->rx_trunc0 = (bgmap & 1) ? GET_STAT_COM(RX_BG_0_MAC_TRUNC_FRAME) : 0;
	p->rx_trunc1 = (bgmap & 2) ? GET_STAT_COM(RX_BG_1_MAC_TRUNC_FRAME) : 0;
	p->rx_trunc2 = (bgmap & 4) ? GET_STAT_COM(RX_BG_2_MAC_TRUNC_FRAME) : 0;
	p->rx_trunc3 = (bgmap & 8) ? GET_STAT_COM(RX_BG_3_MAC_TRUNC_FRAME) : 0;

	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, &val[idx], 1,
			 A_TP_MIB_TNL_CNG_DROP_0 + idx);

	for (i = 0; i < NCHAN; i++)
		p->rx_tp_tnl_cong_drops[i] = val[i];
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
	u32 bgmap = t4_get_mps_bg_map(adap, idx);
	u32 port_base_addr;
	unsigned int i;
	u32 val = 0;

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
	t4_write_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA,
			  &val, 1, A_TP_MIB_TNL_CNG_DROP_0 + idx);
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
		unsigned int pack_align;
		unsigned int ingpad, ingpack;
		unsigned int pcie_cap;

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
		 * Memory Controller which is never good.
		 */

		/* We want the Packing Boundary to be based on the Cache Line
		 * Size in order to help avoid False Sharing performance
		 * issues between CPUs, etc.  We also want the Packing
		 * Boundary to incorporate the PCI-E Maximum Payload Size.  We
		 * get best performance when the Packing Boundary is a
		 * multiple of the Maximum Payload Size.
		 */
		pack_align = fl_align;
		pcie_cap = t4_os_find_pci_capability(adap, PCI_CAP_ID_EXP);
		if (pcie_cap) {
			unsigned int mps, mps_log;
			u16 devctl;

			/* The PCIe Device Control Maximum Payload Size field
			 * [bits 7:5] encodes sizes as powers of 2 starting at
			 * 128 bytes.
			 */
			t4_os_pci_read_cfg2(adap, pcie_cap + PCI_EXP_DEVCTL,
					    &devctl);
			mps_log = ((devctl & PCI_EXP_DEVCTL_PAYLOAD) >> 5) + 7;
			mps = 1 << mps_log;
			if (mps > pack_align)
				pack_align = mps;
		}

		/*
		 * N.B. T5 has a different interpretation of the "0" value for
		 * the Packing Boundary.  This corresponds to 16 bytes instead
		 * of the expected 32 bytes.  We never have a Packing Boundary
		 * less than 32 bytes so we can't use that special value but
		 * on the other hand, if we wanted 32 bytes, the best we can
		 * really do is 64 bytes ...
		 */
		if (pack_align <= 16) {
			ingpack = X_INGPACKBOUNDARY_16B;
			fl_align = 16;
		} else if (pack_align == 32) {
			ingpack = X_INGPACKBOUNDARY_64B;
			fl_align = 64;
		} else {
			unsigned int pack_align_log = cxgbe_fls(pack_align) - 1;

			ingpack = pack_align_log - X_INGPACKBOUNDARY_SHIFT;
			fl_align = pack_align;
		}

		/* Use the smallest Ingress Padding which isn't smaller than
		 * the Memory Controller Read/Write Size.  We'll take that as
		 * being 8 bytes since we don't know of any system with a
		 * wider Memory Controller Bus Width.
		 */
		if (is_t5(adap->params.chip))
			ingpad = X_INGPADBOUNDARY_32B;
		else
			ingpad = X_T6_INGPADBOUNDARY_8B;
		t4_set_reg_field(adap, A_SGE_CONTROL,
				 V_INGPADBOUNDARY(M_INGPADBOUNDARY) |
				 F_EGRSTATUSPAGESIZE,
				 V_INGPADBOUNDARY(ingpad) |
				 V_EGRSTATUSPAGESIZE(stat_len != 64));
		t4_set_reg_field(adap, A_SGE_CONTROL2,
				 V_INGPACKBOUNDARY(M_INGPACKBOUNDARY),
				 V_INGPACKBOUNDARY(ingpack));
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
		     unsigned int portfunc, unsigned int idstype,
		     u8 *vivld, u8 *vin)
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
	if (vivld)
		*vivld = G_FW_VI_CMD_VFVLD(be32_to_cpu(c.alloc_to_len16));
	if (vin)
		*vin = G_FW_VI_CMD_VIN(be32_to_cpu(c.alloc_to_len16));
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
		unsigned int *rss_size, u8 *vivld, u8 *vin)
{
	return t4_alloc_vi_func(adap, mbox, port, pf, vf, nmac, mac, rss_size,
				FW_VI_FUNC_ETH, 0, vivld, vin);
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
				  F_FW_CMD_EXEC);
	if (is_pf4(adap))
		c.op_to_vfn |= cpu_to_be32(V_FW_VI_CMD_PFN(pf) |
					   V_FW_VI_CMD_VFN(vf));
	c.alloc_to_len16 = cpu_to_be32(F_FW_VI_CMD_FREE | FW_LEN16(c));
	c.type_to_viid = cpu_to_be16(V_FW_VI_CMD_VIID(viid));

	if (is_pf4(adap))
		return t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	else
		return t4vf_wr_mbox(adap, &c, sizeof(c), NULL);
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
	if (is_pf4(adap))
		return t4_wr_mbox_meat(adap, mbox, &c, sizeof(c), NULL,
				       sleep_ok);
	else
		return t4vf_wr_mbox(adap, &c, sizeof(c), NULL);
}

/**
 *	t4_alloc_raw_mac_filt - Adds a raw mac entry in mps tcam
 *	@adap: the adapter
 *	@viid: the VI id
 *	@mac: the MAC address
 *	@mask: the mask
 *	@idx: index at which to add this entry
 *	@port_id: the port index
 *	@lookup_type: MAC address for inner (1) or outer (0) header
 *	@sleep_ok: call is allowed to sleep
 *
 *	Adds the mac entry at the specified index using raw mac interface.
 *
 *	Returns a negative error number or the allocated index for this mac.
 */
int t4_alloc_raw_mac_filt(struct adapter *adap, unsigned int viid,
			  const u8 *addr, const u8 *mask, unsigned int idx,
			  u8 lookup_type, u8 port_id, bool sleep_ok)
{
	int ret = 0;
	struct fw_vi_mac_cmd c;
	struct fw_vi_mac_raw *p = &c.u.raw;
	u32 val;

	memset(&c, 0, sizeof(c));
	c.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_VI_MAC_CMD) |
				   F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
				   V_FW_VI_MAC_CMD_VIID(viid));
	val = V_FW_CMD_LEN16(1) |
	      V_FW_VI_MAC_CMD_ENTRY_TYPE(FW_VI_MAC_TYPE_RAW);
	c.freemacs_to_len16 = cpu_to_be32(val);

	/* Specify that this is an inner mac address */
	p->raw_idx_pkd = cpu_to_be32(V_FW_VI_MAC_CMD_RAW_IDX(idx));

	/* Lookup Type. Outer header: 0, Inner header: 1 */
	p->data0_pkd = cpu_to_be32(V_DATALKPTYPE(lookup_type) |
				   V_DATAPORTNUM(port_id));
	/* Lookup mask and port mask */
	p->data0m_pkd = cpu_to_be64(V_DATALKPTYPE(M_DATALKPTYPE) |
				    V_DATAPORTNUM(M_DATAPORTNUM));

	/* Copy the address and the mask */
	memcpy((u8 *)&p->data1[0] + 2, addr, ETHER_ADDR_LEN);
	memcpy((u8 *)&p->data1m[0] + 2, mask, ETHER_ADDR_LEN);

	ret = t4_wr_mbox_meat(adap, adap->mbox, &c, sizeof(c), &c, sleep_ok);
	if (ret == 0) {
		ret = G_FW_VI_MAC_CMD_RAW_IDX(be32_to_cpu(p->raw_idx_pkd));
		if (ret != (int)idx)
			ret = -ENOMEM;
	}

	return ret;
}

/**
 *	t4_free_raw_mac_filt - Frees a raw mac entry in mps tcam
 *	@adap: the adapter
 *	@viid: the VI id
 *	@addr: the MAC address
 *	@mask: the mask
 *	@idx: index of the entry in mps tcam
 *	@lookup_type: MAC address for inner (1) or outer (0) header
 *	@port_id: the port index
 *	@sleep_ok: call is allowed to sleep
 *
 *	Removes the mac entry at the specified index using raw mac interface.
 *
 *	Returns a negative error number on failure.
 */
int t4_free_raw_mac_filt(struct adapter *adap, unsigned int viid,
			 const u8 *addr, const u8 *mask, unsigned int idx,
			 u8 lookup_type, u8 port_id, bool sleep_ok)
{
	struct fw_vi_mac_cmd c;
	struct fw_vi_mac_raw *p = &c.u.raw;
	u32 raw;

	memset(&c, 0, sizeof(c));
	c.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_VI_MAC_CMD) |
				   F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
				   V_FW_CMD_EXEC(0) |
				   V_FW_VI_MAC_CMD_VIID(viid));
	raw = V_FW_VI_MAC_CMD_ENTRY_TYPE(FW_VI_MAC_TYPE_RAW);
	c.freemacs_to_len16 = cpu_to_be32(V_FW_VI_MAC_CMD_FREEMACS(0U) |
					  raw |
					  V_FW_CMD_LEN16(1));

	p->raw_idx_pkd = cpu_to_be32(V_FW_VI_MAC_CMD_RAW_IDX(idx) |
				     FW_VI_MAC_ID_BASED_FREE);

	/* Lookup Type. Outer header: 0, Inner header: 1 */
	p->data0_pkd = cpu_to_be32(V_DATALKPTYPE(lookup_type) |
				   V_DATAPORTNUM(port_id));
	/* Lookup mask and port mask */
	p->data0m_pkd = cpu_to_be64(V_DATALKPTYPE(M_DATALKPTYPE) |
				    V_DATAPORTNUM(M_DATAPORTNUM));

	/* Copy the address and the mask */
	memcpy((u8 *)&p->data1[0] + 2, addr, ETHER_ADDR_LEN);
	memcpy((u8 *)&p->data1m[0] + 2, mask, ETHER_ADDR_LEN);

	return t4_wr_mbox_meat(adap, adap->mbox, &c, sizeof(c), &c, sleep_ok);
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

	if (is_pf4(adap))
		ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	else
		ret = t4vf_wr_mbox(adap, &c, sizeof(c), &c);
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
	if (is_pf4(adap))
		return t4_wr_mbox_ns(adap, mbox, &c, sizeof(c), NULL);
	else
		return t4vf_wr_mbox_ns(adap, &c, sizeof(c), NULL);
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
				  F_FW_CMD_EXEC);
	c.alloc_to_len16 = cpu_to_be32(V_FW_IQ_CMD_IQSTART(start) |
				       V_FW_IQ_CMD_IQSTOP(!start) |
				       FW_LEN16(c));
	c.iqid = cpu_to_be16(iqid);
	c.fl0id = cpu_to_be16(fl0id);
	c.fl1id = cpu_to_be16(fl1id);
	if (is_pf4(adap)) {
		c.op_to_vfn |= cpu_to_be32(V_FW_IQ_CMD_PFN(pf) |
					   V_FW_IQ_CMD_VFN(vf));
		return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
	} else {
		return t4vf_wr_mbox(adap, &c, sizeof(c), NULL);
	}
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
				  F_FW_CMD_EXEC);
	if (is_pf4(adap))
		c.op_to_vfn |= cpu_to_be32(V_FW_IQ_CMD_PFN(pf) |
					   V_FW_IQ_CMD_VFN(vf));
	c.alloc_to_len16 = cpu_to_be32(F_FW_IQ_CMD_FREE | FW_LEN16(c));
	c.type_to_iqandstindex = cpu_to_be32(V_FW_IQ_CMD_TYPE(iqtype));
	c.iqid = cpu_to_be16(iqid);
	c.fl0id = cpu_to_be16(fl0id);
	c.fl1id = cpu_to_be16(fl1id);
	if (is_pf4(adap))
		return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
	else
		return t4vf_wr_mbox(adap, &c, sizeof(c), NULL);
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
				  F_FW_CMD_REQUEST | F_FW_CMD_EXEC);
	if (is_pf4(adap))
		c.op_to_vfn |= cpu_to_be32(V_FW_IQ_CMD_PFN(pf) |
					   V_FW_IQ_CMD_VFN(vf));
	c.alloc_to_len16 = cpu_to_be32(F_FW_EQ_ETH_CMD_FREE | FW_LEN16(c));
	c.eqid_pkd = cpu_to_be32(V_FW_EQ_ETH_CMD_EQID(eqid));
	if (is_pf4(adap))
		return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
	else
		return t4vf_wr_mbox(adap, &c, sizeof(c), NULL);
}

/**
 * t4_link_down_rc_str - return a string for a Link Down Reason Code
 * @link_down_rc: Link Down Reason Code
 *
 * Returns a string representation of the Link Down Reason Code.
 */
static const char *t4_link_down_rc_str(unsigned char link_down_rc)
{
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

	if (link_down_rc >= ARRAY_SIZE(reason))
		return "Bad Reason Code";

	return reason[link_down_rc];
}

static u32 t4_speed_to_fwcap(u32 speed)
{
	switch (speed) {
	case 100000:
		return FW_PORT_CAP32_SPEED_100G;
	case 50000:
		return FW_PORT_CAP32_SPEED_50G;
	case 40000:
		return FW_PORT_CAP32_SPEED_40G;
	case 25000:
		return FW_PORT_CAP32_SPEED_25G;
	case 10000:
		return FW_PORT_CAP32_SPEED_10G;
	case 1000:
		return FW_PORT_CAP32_SPEED_1G;
	case 100:
		return FW_PORT_CAP32_SPEED_100M;
	default:
		break;
	}

	return 0;
}

/* Return the highest speed set in the port capabilities, in Mb/s. */
unsigned int t4_fwcap_to_speed(u32 caps)
{
#define TEST_SPEED_RETURN(__caps_speed, __speed) \
	do { \
		if (caps & FW_PORT_CAP32_SPEED_##__caps_speed) \
			return __speed; \
	} while (0)

	TEST_SPEED_RETURN(100G, 100000);
	TEST_SPEED_RETURN(50G,   50000);
	TEST_SPEED_RETURN(40G,   40000);
	TEST_SPEED_RETURN(25G,   25000);
	TEST_SPEED_RETURN(10G,   10000);
	TEST_SPEED_RETURN(1G,     1000);
	TEST_SPEED_RETURN(100M,    100);

#undef TEST_SPEED_RETURN

	return 0;
}

static void t4_set_link_autoneg_speed(struct port_info *pi, u32 *new_caps)
{
	struct link_config *lc = &pi->link_cfg;
	u32 caps = *new_caps;

	caps &= ~V_FW_PORT_CAP32_SPEED(M_FW_PORT_CAP32_SPEED);
	caps |= G_FW_PORT_CAP32_SPEED(lc->acaps);

	*new_caps = caps;
}

int t4_set_link_speed(struct port_info *pi, u32 speed, u32 *new_caps)
{
	u32 fw_speed_cap = t4_speed_to_fwcap(speed);
	struct link_config *lc = &pi->link_cfg;
	u32 caps = *new_caps;

	if (!(lc->pcaps & fw_speed_cap))
		return -EOPNOTSUPP;

	caps &= ~V_FW_PORT_CAP32_SPEED(M_FW_PORT_CAP32_SPEED);
	caps |= fw_speed_cap;

	*new_caps = caps;

	return 0;
}

int t4_set_link_pause(struct port_info *pi, u8 autoneg, u8 pause_tx,
		      u8 pause_rx, u32 *new_caps)
{
	struct link_config *lc = &pi->link_cfg;
	u32 caps = *new_caps;
	u32 max_speed;

	max_speed = t4_fwcap_to_speed(lc->link_caps);

	if (autoneg) {
		if (!(lc->pcaps & FW_PORT_CAP32_ANEG))
			return -EINVAL;

		caps |= FW_PORT_CAP32_ANEG;
		t4_set_link_autoneg_speed(pi, &caps);
	} else {
		if (!max_speed)
			max_speed = t4_fwcap_to_speed(lc->acaps);

		caps &= ~FW_PORT_CAP32_ANEG;
		t4_set_link_speed(pi, max_speed, &caps);
	}

	if (lc->pcaps & FW_PORT_CAP32_MDIAUTO)
		caps |= V_FW_PORT_CAP32_MDI(FW_PORT_CAP32_MDI_AUTO);

	caps &= ~V_FW_PORT_CAP32_FC(M_FW_PORT_CAP32_FC);
	caps &= ~V_FW_PORT_CAP32_802_3(M_FW_PORT_CAP32_802_3);
	if (pause_tx && pause_rx) {
		caps |= FW_PORT_CAP32_FC_TX | FW_PORT_CAP32_FC_RX;
		if (lc->pcaps & FW_PORT_CAP32_802_3_PAUSE)
			caps |= FW_PORT_CAP32_802_3_PAUSE;
	} else if (pause_tx) {
		caps |= FW_PORT_CAP32_FC_TX;
		if (lc->pcaps & FW_PORT_CAP32_802_3_ASM_DIR)
			caps |= FW_PORT_CAP32_802_3_ASM_DIR;
	} else if (pause_rx) {
		caps |= FW_PORT_CAP32_FC_RX;
		if (lc->pcaps & FW_PORT_CAP32_802_3_PAUSE)
			caps |= FW_PORT_CAP32_802_3_PAUSE;

		if (lc->pcaps & FW_PORT_CAP32_802_3_ASM_DIR)
			caps |= FW_PORT_CAP32_802_3_ASM_DIR;
	}

	*new_caps = caps;

	return 0;
}

int t4_set_link_fec(struct port_info *pi, u8 fec_rs, u8 fec_baser,
		    u8 fec_none, u32 *new_caps)
{
	struct link_config *lc = &pi->link_cfg;
	u32 max_speed, caps = *new_caps;

	if (!(lc->pcaps & V_FW_PORT_CAP32_FEC(M_FW_PORT_CAP32_FEC)))
		return -EOPNOTSUPP;

	/* Link might be down. In that case consider the max
	 * speed advertised
	 */
	max_speed = t4_fwcap_to_speed(lc->link_caps);
	if (!max_speed)
		max_speed = t4_fwcap_to_speed(lc->acaps);

	caps &= ~V_FW_PORT_CAP32_FEC(M_FW_PORT_CAP32_FEC);
	if (fec_rs) {
		switch (max_speed) {
		case 100000:
		case 25000:
			caps |= FW_PORT_CAP32_FEC_RS;
			break;
		default:
			return -EOPNOTSUPP;
		}
	}

	if (fec_baser) {
		switch (max_speed) {
		case 50000:
		case 25000:
			caps |= FW_PORT_CAP32_FEC_BASER_RS;
			break;
		default:
			return -EOPNOTSUPP;
		}
	}

	if (fec_none)
		caps |= FW_PORT_CAP32_FEC_NO_FEC;

	if (!(caps & V_FW_PORT_CAP32_FEC(M_FW_PORT_CAP32_FEC))) {
		/* No explicit encoding is requested.
		 * So, default back to AUTO.
		 */
		switch (max_speed) {
		case 100000:
			caps |= FW_PORT_CAP32_FEC_RS |
				FW_PORT_CAP32_FEC_NO_FEC;
			break;
		case 50000:
			caps |= FW_PORT_CAP32_FEC_BASER_RS |
				FW_PORT_CAP32_FEC_NO_FEC;
			break;
		case 25000:
			caps |= FW_PORT_CAP32_FEC_RS |
				FW_PORT_CAP32_FEC_BASER_RS |
				FW_PORT_CAP32_FEC_NO_FEC;
			break;
		default:
			return -EOPNOTSUPP;
		}
	}

	*new_caps = caps;

	return 0;
}

/**
 * t4_handle_get_port_info - process a FW reply message
 * @pi: the port info
 * @rpl: start of the FW message
 *
 * Processes a GET_PORT_INFO FW reply message.
 */
static void t4_handle_get_port_info(struct port_info *pi, const __be64 *rpl)
{
	const struct fw_port_cmd *cmd = (const void *)rpl;
	u8 link_ok, link_down_rc, mod_type, port_type;
	u32 action, pcaps, acaps, link_caps, lstatus;
	struct link_config *lc = &pi->link_cfg;
	struct adapter *adapter = pi->adapter;
	u8 mod_changed = 0;

	/* Extract the various fields from the Port Information message.
	 */
	action = be32_to_cpu(cmd->action_to_len16);
	if (G_FW_PORT_CMD_ACTION(action) != FW_PORT_ACTION_GET_PORT_INFO32) {
		dev_warn(adapter, "Handle Port Information: Bad Command/Action %#x\n",
			 action);
		return;
	}

	lstatus = be32_to_cpu(cmd->u.info32.lstatus32_to_cbllen32);
	link_ok = (lstatus & F_FW_PORT_CMD_LSTATUS32) ? 1 : 0;
	link_down_rc = G_FW_PORT_CMD_LINKDNRC32(lstatus);
	port_type = G_FW_PORT_CMD_PORTTYPE32(lstatus);
	mod_type = G_FW_PORT_CMD_MODTYPE32(lstatus);

	pcaps = be32_to_cpu(cmd->u.info32.pcaps32);
	acaps = be32_to_cpu(cmd->u.info32.acaps32);
	link_caps = be32_to_cpu(cmd->u.info32.linkattr32);

	if (mod_type != lc->mod_type) {
		t4_init_link_config(pi, pcaps, acaps, lc->mdio_addr,
				    port_type, mod_type);
		t4_os_portmod_changed(adapter, pi->pidx);
		mod_changed = 1;
	}
	if (link_ok != lc->link_ok || acaps != lc->acaps ||
	    link_caps != lc->link_caps) { /* something changed */
		if (!link_ok && lc->link_ok) {
			lc->link_down_rc = link_down_rc;
			dev_warn(adap, "Port %d link down, reason: %s\n",
				 pi->port_id,
				 t4_link_down_rc_str(link_down_rc));
		}
		lc->link_ok = link_ok;
		lc->acaps = acaps;
		lc->link_caps = link_caps;
		t4_os_link_changed(adapter, pi->pidx);
	}

	if (mod_changed != 0 && is_pf4(adapter) != 0) {
		u32 mod_caps = lc->admin_caps;
		int ret;

		ret = t4_link_l1cfg_ns(pi, mod_caps);
		if (ret != FW_SUCCESS)
			dev_warn(adapter,
				 "Attempt to update new Transceiver Module settings %#x failed with error: %d\n",
				 mod_caps, ret);
	}
}

/**
 * t4_ctrl_eq_free - free a control egress queue
 * @adap: the adapter
 * @mbox: mailbox to use for the FW command
 * @pf: the PF owning the queue
 * @vf: the VF owning the queue
 * @eqid: egress queue id
 *
 * Frees a control egress queue.
 */
int t4_ctrl_eq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
		    unsigned int vf, unsigned int eqid)
{
	struct fw_eq_ctrl_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_EQ_CTRL_CMD) |
				  F_FW_CMD_REQUEST | F_FW_CMD_EXEC |
				  V_FW_EQ_CTRL_CMD_PFN(pf) |
				  V_FW_EQ_CTRL_CMD_VFN(vf));
	c.alloc_to_len16 = cpu_to_be32(F_FW_EQ_CTRL_CMD_FREE | FW_LEN16(c));
	c.cmpliqid_eqid = cpu_to_be32(V_FW_EQ_CTRL_CMD_EQID(eqid));
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

	if (opcode == FW_PORT_CMD && action == FW_PORT_ACTION_GET_PORT_INFO32) {
		/* link/module state change message */
		int chan = G_FW_PORT_CMD_PORTID(be32_to_cpu(p->op_to_portid));
		struct port_info *pi = NULL;
		int i;

		for_each_port(adap, i) {
			pi = adap2pinfo(adap, i);
			if (pi->tx_chan == chan)
				break;
		}

		t4_handle_get_port_info(pi, rpl);
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
	lc->link_down_rc = 0;
	lc->link_caps = 0;
}

/**
 * t4_init_link_config - initialize a link's SW state
 * @pi: the port info
 * @pcaps: link Port Capabilities
 * @acaps: link current Advertised Port Capabilities
 * @mdio_addr : address of the PHY
 * @port_type : firmware port type
 * @mod_type  : firmware module type
 *
 * Initializes the SW state maintained for each link, including the link's
 * capabilities and default speed/flow-control/autonegotiation settings.
 */
void t4_init_link_config(struct port_info *pi, u32 pcaps, u32 acaps,
			 u8 mdio_addr, u8 port_type, u8 mod_type)
{
	u8 fec_rs = 0, fec_baser = 0, fec_none = 0;
	struct link_config *lc = &pi->link_cfg;

	lc->pcaps = pcaps;
	lc->acaps = acaps;
	lc->admin_caps = acaps;
	lc->link_caps = 0;

	lc->mdio_addr = mdio_addr;
	lc->port_type = port_type;
	lc->mod_type = mod_type;

	lc->link_ok = 0;
	lc->link_down_rc = 0;

	/* Turn Tx and Rx pause off by default */
	lc->admin_caps &= ~V_FW_PORT_CAP32_FC(M_FW_PORT_CAP32_FC);
	lc->admin_caps &= ~V_FW_PORT_CAP32_802_3(M_FW_PORT_CAP32_802_3);
	if (lc->pcaps & FW_PORT_CAP32_FORCE_PAUSE)
		lc->admin_caps &= ~FW_PORT_CAP32_FORCE_PAUSE;

	/* Reset FEC caps to default values */
	if (lc->pcaps & V_FW_PORT_CAP32_FEC(M_FW_PORT_CAP32_FEC)) {
		if (lc->acaps & FW_PORT_CAP32_FEC_RS)
			fec_rs = 1;
		else if (lc->acaps & FW_PORT_CAP32_FEC_BASER_RS)
			fec_baser = 1;
		else
			fec_none = 1;

		lc->admin_caps &= ~V_FW_PORT_CAP32_FEC(M_FW_PORT_CAP32_FEC);
		t4_set_link_fec(pi, fec_rs, fec_baser, fec_none,
				&lc->admin_caps);
	}

	if (lc->pcaps & FW_PORT_CAP32_FORCE_FEC)
		lc->admin_caps &= ~FW_PORT_CAP32_FORCE_FEC;

	/* Reset MDI to AUTO */
	if (lc->pcaps & FW_PORT_CAP32_MDIAUTO) {
		lc->admin_caps &= ~V_FW_PORT_CAP32_MDI(M_FW_PORT_CAP32_MDI);
		lc->admin_caps |= V_FW_PORT_CAP32_MDI(FW_PORT_CAP32_MDI_AUTO);
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
	if (whoami != 0xffffffff && whoami != X_CIM_PF_NOACCESS)
		return 0;

	dev_err(adapter, "Device didn't become ready for access, whoami = %#x\n",
		whoami);
	return -EIO;
}

struct flash_desc {
	u32 vendor_and_model_id;
	u32 size_mb;
};

int t4_get_flash_params(struct adapter *adapter)
{
	/*
	 * Table for non-standard supported Flash parts.  Note, all Flash
	 * parts must have 64KB sectors.
	 */
	static struct flash_desc supported_flash[] = {
		{ 0x00150201, 4 << 20 },       /* Spansion 4MB S25FL032P */
	};

	int ret;
	u32 flashid = 0;
	unsigned int part, manufacturer;
	unsigned int density, size = 0;

	/**
	 * Issue a Read ID Command to the Flash part.  We decode supported
	 * Flash parts and their sizes from this.  There's a newer Query
	 * Command which can retrieve detailed geometry information but
	 * many Flash parts don't support it.
	 */
	ret = sf1_write(adapter, 1, 1, 0, SF_RD_ID);
	if (!ret)
		ret = sf1_read(adapter, 3, 0, 1, &flashid);
	t4_write_reg(adapter, A_SF_OP, 0);               /* unlock SF */
	if (ret < 0)
		return ret;

	/**
	 * Check to see if it's one of our non-standard supported Flash parts.
	 */
	for (part = 0; part < ARRAY_SIZE(supported_flash); part++) {
		if (supported_flash[part].vendor_and_model_id == flashid) {
			adapter->params.sf_size =
				supported_flash[part].size_mb;
			adapter->params.sf_nsec =
				adapter->params.sf_size / SF_SEC_SIZE;
			goto found;
		}
	}

	/**
	 * Decode Flash part size.  The code below looks repetative with
	 * common encodings, but that's not guaranteed in the JEDEC
	 * specification for the Read JADEC ID command.  The only thing that
	 * we're guaranteed by the JADEC specification is where the
	 * Manufacturer ID is in the returned result.  After that each
	 * Manufacturer ~could~ encode things completely differently.
	 * Note, all Flash parts must have 64KB sectors.
	 */
	manufacturer = flashid & 0xff;
	switch (manufacturer) {
	case 0x20: { /* Micron/Numonix */
		/**
		 * This Density -> Size decoding table is taken from Micron
		 * Data Sheets.
		 */
		density = (flashid >> 16) & 0xff;
		switch (density) {
		case 0x14:
			size = 1 << 20; /* 1MB */
			break;
		case 0x15:
			size = 1 << 21; /* 2MB */
			break;
		case 0x16:
			size = 1 << 22; /* 4MB */
			break;
		case 0x17:
			size = 1 << 23; /* 8MB */
			break;
		case 0x18:
			size = 1 << 24; /* 16MB */
			break;
		case 0x19:
			size = 1 << 25; /* 32MB */
			break;
		case 0x20:
			size = 1 << 26; /* 64MB */
			break;
		case 0x21:
			size = 1 << 27; /* 128MB */
			break;
		case 0x22:
			size = 1 << 28; /* 256MB */
			break;
		}
		break;
	}

	case 0x9d: { /* ISSI -- Integrated Silicon Solution, Inc. */
		/**
		 * This Density -> Size decoding table is taken from ISSI
		 * Data Sheets.
		 */
		density = (flashid >> 16) & 0xff;
		switch (density) {
		case 0x16:
			size = 1 << 25; /* 32MB */
			break;
		case 0x17:
			size = 1 << 26; /* 64MB */
			break;
		}
		break;
	}

	case 0xc2: { /* Macronix */
		/**
		 * This Density -> Size decoding table is taken from Macronix
		 * Data Sheets.
		 */
		density = (flashid >> 16) & 0xff;
		switch (density) {
		case 0x17:
			size = 1 << 23; /* 8MB */
			break;
		case 0x18:
			size = 1 << 24; /* 16MB */
			break;
		}
		break;
	}

	case 0xef: { /* Winbond */
		/**
		 * This Density -> Size decoding table is taken from Winbond
		 * Data Sheets.
		 */
		density = (flashid >> 16) & 0xff;
		switch (density) {
		case 0x17:
			size = 1 << 23; /* 8MB */
			break;
		case 0x18:
			size = 1 << 24; /* 16MB */
			break;
		}
		break;
	}
	}

	/* If we didn't recognize the FLASH part, that's no real issue: the
	 * Hardware/Software contract says that Hardware will _*ALWAYS*_
	 * use a FLASH part which is at least 4MB in size and has 64KB
	 * sectors.  The unrecognized FLASH part is likely to be much larger
	 * than 4MB, but that's all we really need.
	 */
	if (size == 0) {
		dev_warn(adapter,
			 "Unknown Flash Part, ID = %#x, assuming 4MB\n",
			 flashid);
		size = 1 << 22;
	}

	/**
	 * Store decoded Flash size and fall through into vetting code.
	 */
	adapter->params.sf_size = size;
	adapter->params.sf_nsec = size / SF_SEC_SIZE;

found:
	/*
	 * We should reject adapters with FLASHes which are too small. So, emit
	 * a warning.
	 */
	if (adapter->params.sf_size < FLASH_MIN_SIZE)
		dev_warn(adapter, "WARNING: Flash Part ID %#x, size %#x < %#x\n",
			 flashid, adapter->params.sf_size, FLASH_MIN_SIZE);

	return 0;
}

static void set_pcie_completion_timeout(struct adapter *adapter,
					u8 range)
{
	u32 pcie_cap;
	u16 val;

	pcie_cap = t4_os_find_pci_capability(adapter, PCI_CAP_ID_EXP);
	if (pcie_cap) {
		t4_os_pci_read_cfg2(adapter, pcie_cap + PCI_EXP_DEVCTL2, &val);
		val &= 0xfff0;
		val |= range;
		t4_os_pci_write_cfg2(adapter, pcie_cap + PCI_EXP_DEVCTL2, val);
	}
}

/**
 * t4_get_chip_type - Determine chip type from device ID
 * @adap: the adapter
 * @ver: adapter version
 */
int t4_get_chip_type(struct adapter *adap, int ver)
{
	enum chip_type chip = 0;
	u32 pl_rev = G_REV(t4_read_reg(adap, A_PL_REV));

	/* Retrieve adapter's device ID */
	switch (ver) {
	case CHELSIO_T5:
		chip |= CHELSIO_CHIP_CODE(CHELSIO_T5, pl_rev);
		break;
	case CHELSIO_T6:
		chip |= CHELSIO_CHIP_CODE(CHELSIO_T6, pl_rev);
		break;
	default:
		dev_err(adap, "Device %d is not supported\n",
			adap->params.pci.device_id);
		return -EINVAL;
	}

	return chip;
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
		/* Congestion map is for 4 channels so that
		 * MPS can have 4 priority per port.
		 */
		adapter->params.arch.cng_ch_bits_log = 2;
		break;
	case CHELSIO_T6:
		adapter->params.chip |= CHELSIO_CHIP_CODE(CHELSIO_T6, pl_rev);
		adapter->params.arch.sge_fl_db = 0;
		adapter->params.arch.mps_tcam_size =
						NUM_MPS_T5_CLS_SRAM_L_INSTANCES;
		adapter->params.arch.mps_rplc_size = 256;
		adapter->params.arch.nchan = 2;
		adapter->params.arch.vfcount = 256;
		/* Congestion map is for 2 channels so that
		 * MPS can have 8 priority per port.
		 */
		adapter->params.arch.cng_ch_bits_log = 3;
		break;
	default:
		dev_err(adapter, "%s: Device %d is not supported\n",
			__func__, adapter->params.pci.device_id);
		return -EINVAL;
	}

	adapter->params.pci.vpd_cap_addr =
		t4_os_find_pci_capability(adapter, PCI_CAP_ID_VPD);

	ret = t4_get_flash_params(adapter);
	if (ret < 0) {
		dev_err(adapter, "Unable to retrieve Flash Parameters, ret = %d\n",
			-ret);
		return ret;
	}

	adapter->params.cim_la_size = CIMLA_SIZE;

	init_cong_ctrl(adapter->params.a_wnd, adapter->params.b_wnd);

	/*
	 * Default port and clock for debugging in case we can't reach FW.
	 */
	adapter->params.nports = 1;
	adapter->params.portvec = 1;
	adapter->params.vpd.cclk = 50000;

	/* Set pci completion timeout value to 4 seconds. */
	set_pcie_completion_timeout(adapter, 0xd);
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
	int chan, ret;
	u32 param, v;

	v = t4_read_reg(adap, A_TP_TIMER_RESOLUTION);
	adap->params.tp.tre = G_TIMERRESOLUTION(v);
	adap->params.tp.dack_re = G_DELAYEDACKRESOLUTION(v);

	/* MODQ_REQ_MAP defaults to setting queues 0-3 to chan 0-3 */
	for (chan = 0; chan < NCHAN; chan++)
		adap->params.tp.tx_modq[chan] = chan;

	/*
	 * Cache the adapter's Compressed Filter Mode/Mask and global Ingress
	 * Configuration.
	 */
	param = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
		 V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_FILTER) |
		 V_FW_PARAMS_PARAM_Y(FW_PARAM_DEV_FILTER_MODE_MASK));

	/* Read current value */
	ret = t4_query_params(adap, adap->mbox, adap->pf, 0,
			      1, &param, &v);
	if (!ret) {
		dev_info(adap, "Current filter mode/mask 0x%x:0x%x\n",
			 G_FW_PARAMS_PARAM_FILTER_MODE(v),
			 G_FW_PARAMS_PARAM_FILTER_MASK(v));
		adap->params.tp.vlan_pri_map =
			G_FW_PARAMS_PARAM_FILTER_MODE(v);
		adap->params.tp.filter_mask =
			G_FW_PARAMS_PARAM_FILTER_MASK(v);
	} else {
		dev_info(adap,
			 "Failed to read filter mode/mask via fw api, using indirect-reg-read\n");

		/* In case of older-fw (which doesn't expose the api
		 * FW_PARAM_DEV_FILTER_MODE_MASK) and newer-driver (which uses
		 * the fw api) combination, fall-back to older method of reading
		 * the filter mode from indirect-register
		 */
		t4_read_indirect(adap, A_TP_PIO_ADDR, A_TP_PIO_DATA,
				 &adap->params.tp.vlan_pri_map, 1,
				 A_TP_VLAN_PRI_MAP);

		/* With the older-fw and newer-driver combination we might run
		 * into an issue when user wants to use hash filter region but
		 * the filter_mask is zero, in this case filter_mask validation
		 * is tough. To avoid that we set the filter_mask same as filter
		 * mode, which will behave exactly as the older way of ignoring
		 * the filter mask validation.
		 */
		adap->params.tp.filter_mask = adap->params.tp.vlan_pri_map;
	}

	t4_read_indirect(adap, A_TP_PIO_ADDR, A_TP_PIO_DATA,
			 &adap->params.tp.ingress_config, 1,
			 A_TP_INGRESS_CONFIG);

	/* For T6, cache the adapter's compressed error vector
	 * and passing outer header info for encapsulated packets.
	 */
	if (CHELSIO_CHIP_VERSION(adap->params.chip) > CHELSIO_T5) {
		v = t4_read_reg(adap, A_TP_OUT_CONFIG);
		adap->params.tp.rx_pkt_encap = (v & F_CRXPKTENC) ? 1 : 0;
	}

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
	adap->params.tp.ethertype_shift = t4_filter_field_shift(adap,
								F_ETHERTYPE);
	adap->params.tp.macmatch_shift = t4_filter_field_shift(adap,
							       F_MACMATCH);
	adap->params.tp.tos_shift = t4_filter_field_shift(adap, F_TOS);

	v = t4_read_reg(adap, LE_3_DB_HASH_MASK_GEN_IPV4_T6_A);
	adap->params.tp.hash_filter_mask = v;
	v = t4_read_reg(adap, LE_4_DB_HASH_MASK_GEN_IPV4_T6_A);
	adap->params.tp.hash_filter_mask |= ((u64)v << 32);

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
	u32 param, val, pcaps, acaps;
	enum fw_port_type port_type;
	struct fw_port_cmd cmd;
	u8 vivld = 0, vin = 0;
	int ret, i, j = 0;
	int mdio_addr;
	u8 addr[6];

	param = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_PFVF) |
		 V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_PFVF_PORT_CAPS32));
	val = 1;
	ret = t4_set_params(adap, mbox, pf, vf, 1, &param, &val);
	if (ret < 0)
		return ret;

	memset(&cmd, 0, sizeof(cmd));

	for_each_port(adap, i) {
		struct port_info *pi = adap2pinfo(adap, i);
		unsigned int rss_size = 0;
		u32 lstatus32;

		while ((adap->params.portvec & (1 << j)) == 0)
			j++;

		memset(&cmd, 0, sizeof(cmd));
		cmd.op_to_portid = cpu_to_be32(V_FW_CMD_OP(FW_PORT_CMD) |
					       F_FW_CMD_REQUEST |
					       F_FW_CMD_READ |
					       V_FW_PORT_CMD_PORTID(j));
		val = FW_PORT_ACTION_GET_PORT_INFO32;
		cmd.action_to_len16 = cpu_to_be32(V_FW_PORT_CMD_ACTION(val) |
						  FW_LEN16(cmd));
		ret = t4_wr_mbox(pi->adapter, mbox, &cmd, sizeof(cmd), &cmd);
		if (ret)
			return ret;

		/* Extract the various fields from the Port Information
		 * message.
		 */
		lstatus32 = be32_to_cpu(cmd.u.info32.lstatus32_to_cbllen32);

		port_type = G_FW_PORT_CMD_PORTTYPE32(lstatus32);
		mdio_addr = (lstatus32 & F_FW_PORT_CMD_MDIOCAP32) ?
			    (int)G_FW_PORT_CMD_MDIOADDR32(lstatus32) : -1;
		pcaps = be32_to_cpu(cmd.u.info32.pcaps32);
		acaps = be32_to_cpu(cmd.u.info32.acaps32);

		ret = t4_alloc_vi(adap, mbox, j, pf, vf, 1, addr, &rss_size,
				  &vivld, &vin);
		if (ret < 0)
			return ret;

		pi->viid = ret;
		pi->tx_chan = j;
		pi->rss_size = rss_size;
		t4_os_set_hw_addr(adap, i, addr);

		/* If fw supports returning the VIN as part of FW_VI_CMD,
		 * save the returned values.
		 */
		if (adap->params.viid_smt_extn_support) {
			pi->vivld = vivld;
			pi->vin = vin;
		} else {
			/* Retrieve the values from VIID */
			pi->vivld = G_FW_VIID_VIVLD(pi->viid);
			pi->vin =  G_FW_VIID_VIN(pi->viid);
		}

		t4_init_link_config(pi, pcaps, acaps, mdio_addr, port_type,
				    FW_PORT_MOD_TYPE_NA);
		j++;
	}
	return 0;
}

/**
 * t4_memory_rw_addr - read/write adapter memory via PCIE memory window
 * @adap: the adapter
 * @win: PCI-E Memory Window to use
 * @addr: address within adapter memory
 * @len: amount of memory to transfer
 * @hbuf: host memory buffer
 * @dir: direction of transfer T4_MEMORY_READ (1) or T4_MEMORY_WRITE (0)
 *
 * Reads/writes an [almost] arbitrary memory region in the firmware: the
 * firmware memory address and host buffer must be aligned on 32-bit
 * boudaries; the length may be arbitrary.
 *
 * NOTES:
 *  1. The memory is transferred as a raw byte sequence from/to the
 *     firmware's memory.  If this memory contains data structures which
 *     contain multi-byte integers, it's the caller's responsibility to
 *     perform appropriate byte order conversions.
 *
 *  2. It is the Caller's responsibility to ensure that no other code
 *     uses the specified PCI-E Memory Window while this routine is
 *     using it.  This is typically done via the use of OS-specific
 *     locks, etc.
 */
int t4_memory_rw_addr(struct adapter *adap, int win, u32 addr,
		      u32 len, void *hbuf, int dir)
{
	u32 pos, offset, resid;
	u32 win_pf, mem_reg, mem_aperture, mem_base;
	u32 *buf;

	/* Argument sanity checks ...*/
	if (addr & 0x3 || (uintptr_t)hbuf & 0x3)
		return -EINVAL;
	buf = (u32 *)hbuf;

	/* It's convenient to be able to handle lengths which aren't a
	 * multiple of 32-bits because we often end up transferring files to
	 * the firmware.  So we'll handle that by normalizing the length here
	 * and then handling any residual transfer at the end.
	 */
	resid = len & 0x3;
	len -= resid;

	/* Each PCI-E Memory Window is programmed with a window size -- or
	 * "aperture" -- which controls the granularity of its mapping onto
	 * adapter memory.  We need to grab that aperture in order to know
	 * how to use the specified window.  The window is also programmed
	 * with the base address of the Memory Window in BAR0's address
	 * space.  For T4 this is an absolute PCI-E Bus Address.  For T5
	 * the address is relative to BAR0.
	 */
	mem_reg = t4_read_reg(adap,
			      PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_BASE_WIN,
						  win));
	mem_aperture = 1 << (G_WINDOW(mem_reg) + X_WINDOW_SHIFT);
	mem_base = G_PCIEOFST(mem_reg) << X_PCIEOFST_SHIFT;

	win_pf = is_t4(adap->params.chip) ? 0 : V_PFNUM(adap->pf);

	/* Calculate our initial PCI-E Memory Window Position and Offset into
	 * that Window.
	 */
	pos = addr & ~(mem_aperture - 1);
	offset = addr - pos;

	/* Set up initial PCI-E Memory Window to cover the start of our
	 * transfer.  (Read it back to ensure that changes propagate before we
	 * attempt to use the new value.)
	 */
	t4_write_reg(adap,
		     PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_OFFSET, win),
		     pos | win_pf);
	t4_read_reg(adap,
		    PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_OFFSET, win));

	/* Transfer data to/from the adapter as long as there's an integral
	 * number of 32-bit transfers to complete.
	 *
	 * A note on Endianness issues:
	 *
	 * The "register" reads and writes below from/to the PCI-E Memory
	 * Window invoke the standard adapter Big-Endian to PCI-E Link
	 * Little-Endian "swizzel."  As a result, if we have the following
	 * data in adapter memory:
	 *
	 *     Memory:  ... | b0 | b1 | b2 | b3 | ...
	 *     Address:      i+0  i+1  i+2  i+3
	 *
	 * Then a read of the adapter memory via the PCI-E Memory Window
	 * will yield:
	 *
	 *     x = readl(i)
	 *         31                  0
	 *         [ b3 | b2 | b1 | b0 ]
	 *
	 * If this value is stored into local memory on a Little-Endian system
	 * it will show up correctly in local memory as:
	 *
	 *     ( ..., b0, b1, b2, b3, ... )
	 *
	 * But on a Big-Endian system, the store will show up in memory
	 * incorrectly swizzled as:
	 *
	 *     ( ..., b3, b2, b1, b0, ... )
	 *
	 * So we need to account for this in the reads and writes to the
	 * PCI-E Memory Window below by undoing the register read/write
	 * swizzels.
	 */
	while (len > 0) {
		if (dir == T4_MEMORY_READ)
			*buf++ = le32_to_cpu((__le32)t4_read_reg(adap,
								 mem_base +
								 offset));
		else
			t4_write_reg(adap, mem_base + offset,
				     (u32)cpu_to_le32(*buf++));
		offset += sizeof(__be32);
		len -= sizeof(__be32);

		/* If we've reached the end of our current window aperture,
		 * move the PCI-E Memory Window on to the next.  Note that
		 * doing this here after "len" may be 0 allows us to set up
		 * the PCI-E Memory Window for a possible final residual
		 * transfer below ...
		 */
		if (offset == mem_aperture) {
			pos += mem_aperture;
			offset = 0;
			t4_write_reg(adap,
				PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_OFFSET,
						    win), pos | win_pf);
			t4_read_reg(adap,
				PCIE_MEM_ACCESS_REG(A_PCIE_MEM_ACCESS_OFFSET,
						    win));
		}
	}

	/* If the original transfer had a length which wasn't a multiple of
	 * 32-bits, now's where we need to finish off the transfer of the
	 * residual amount.  The PCI-E Memory Window has already been moved
	 * above (if necessary) to cover this final transfer.
	 */
	if (resid) {
		union {
			u32 word;
			char byte[4];
		} last;
		unsigned char *bp;
		int i;

		if (dir == T4_MEMORY_READ) {
			last.word = le32_to_cpu((__le32)t4_read_reg(adap,
								    mem_base +
								    offset));
			for (bp = (unsigned char *)buf, i = resid; i < 4; i++)
				bp[i] = last.byte[i];
		} else {
			last.word = *buf;
			for (i = resid; i < 4; i++)
				last.byte[i] = 0;
			t4_write_reg(adap, mem_base + offset,
				     (u32)cpu_to_le32(last.word));
		}
	}

	return 0;
}

/**
 * t4_memory_rw_mtype -read/write EDC 0, EDC 1 or MC via PCIE memory window
 * @adap: the adapter
 * @win: PCI-E Memory Window to use
 * @mtype: memory type: MEM_EDC0, MEM_EDC1 or MEM_MC
 * @maddr: address within indicated memory type
 * @len: amount of memory to transfer
 * @hbuf: host memory buffer
 * @dir: direction of transfer T4_MEMORY_READ (1) or T4_MEMORY_WRITE (0)
 *
 * Reads/writes adapter memory using t4_memory_rw_addr().  This routine
 * provides an (memory type, address within memory type) interface.
 */
int t4_memory_rw_mtype(struct adapter *adap, int win, int mtype, u32 maddr,
		       u32 len, void *hbuf, int dir)
{
	u32 mtype_offset;
	u32 edc_size, mc_size;

	/* Offset into the region of memory which is being accessed
	 * MEM_EDC0 = 0
	 * MEM_EDC1 = 1
	 * MEM_MC   = 2 -- MEM_MC for chips with only 1 memory controller
	 * MEM_MC1  = 3 -- for chips with 2 memory controllers (e.g. T5)
	 */
	edc_size  = G_EDRAM0_SIZE(t4_read_reg(adap, A_MA_EDRAM0_BAR));
	if (mtype != MEM_MC1) {
		mtype_offset = (mtype * (edc_size * 1024 * 1024));
	} else {
		mc_size = G_EXT_MEM0_SIZE(t4_read_reg(adap,
						      A_MA_EXT_MEMORY0_BAR));
		mtype_offset = (MEM_MC0 * edc_size + mc_size) * 1024 * 1024;
	}

	return t4_memory_rw_addr(adap, win,
				 mtype_offset + maddr, len,
				 hbuf, dir);
}
