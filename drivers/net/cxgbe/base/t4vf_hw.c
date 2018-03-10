/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#include <rte_ethdev_driver.h>
#include <rte_ether.h>

#include "common.h"
#include "t4_regs.h"

/*
 * Get the reply to a mailbox command and store it in @rpl in big-endian order.
 */
static void get_mbox_rpl(struct adapter *adap, __be64 *rpl, int nflit,
			 u32 mbox_addr)
{
	for ( ; nflit; nflit--, mbox_addr += 8)
		*rpl++ = htobe64(t4_read_reg64(adap, mbox_addr));
}

/**
 * t4vf_wr_mbox_core - send a command to FW through the mailbox
 * @adapter: the adapter
 * @cmd: the command to write
 * @size: command length in bytes
 * @rpl: where to optionally store the reply
 * @sleep_ok: if true we may sleep while awaiting command completion
 *
 * Sends the given command to FW through the mailbox and waits for the
 * FW to execute the command.  If @rpl is not %NULL it is used to store
 * the FW's reply to the command.  The command and its optional reply
 * are of the same length.  FW can take up to 500 ms to respond.
 * @sleep_ok determines whether we may sleep while awaiting the response.
 * If sleeping is allowed we use progressive backoff otherwise we spin.
 *
 * The return value is 0 on success or a negative errno on failure.  A
 * failure can happen either because we are not able to execute the
 * command or FW executes it but signals an error.  In the latter case
 * the return value is the error code indicated by FW (negated).
 */
int t4vf_wr_mbox_core(struct adapter *adapter,
		      const void __attribute__((__may_alias__)) *cmd,
		      int size, void *rpl, bool sleep_ok)
{
	/*
	 * We delay in small increments at first in an effort to maintain
	 * responsiveness for simple, fast executing commands but then back
	 * off to larger delays to a maximum retry delay.
	 */
	static const int delay[] = {
		1, 1, 3, 5, 10, 10, 20, 50, 100
	};


	u32 mbox_ctl = T4VF_CIM_BASE_ADDR + A_CIM_VF_EXT_MAILBOX_CTRL;
	__be64 cmd_rpl[MBOX_LEN / 8];
	struct mbox_entry entry;
	unsigned int delay_idx;
	u32 v, mbox_data;
	const __be64 *p;
	int i, ret;
	int ms;

	/* In T6, mailbox size is changed to 128 bytes to avoid
	 * invalidating the entire prefetch buffer.
	 */
	if (CHELSIO_CHIP_VERSION(adapter->params.chip) <= CHELSIO_T5)
		mbox_data = T4VF_MBDATA_BASE_ADDR;
	else
		mbox_data = T6VF_MBDATA_BASE_ADDR;

	/*
	 * Commands must be multiples of 16 bytes in length and may not be
	 * larger than the size of the Mailbox Data register array.
	 */
	if ((size % 16) != 0 ||
			size > NUM_CIM_VF_MAILBOX_DATA_INSTANCES * 4)
		return -EINVAL;

	/*
	 * Queue ourselves onto the mailbox access list.  When our entry is at
	 * the front of the list, we have rights to access the mailbox.  So we
	 * wait [for a while] till we're at the front [or bail out with an
	 * EBUSY] ...
	 */
	t4_os_atomic_add_tail(&entry, &adapter->mbox_list, &adapter->mbox_lock);

	delay_idx = 0;
	ms = delay[0];

	for (i = 0; ; i += ms) {
		/*
		 * If we've waited too long, return a busy indication.  This
		 * really ought to be based on our initial position in the
		 * mailbox access list but this is a start.  We very rarely
		 * contend on access to the mailbox ...
		 */
		if (i > (2 * FW_CMD_MAX_TIMEOUT)) {
			t4_os_atomic_list_del(&entry, &adapter->mbox_list,
					      &adapter->mbox_lock);
			ret = -EBUSY;
			return ret;
		}

		/*
		 * If we're at the head, break out and start the mailbox
		 * protocol.
		 */
		if (t4_os_list_first_entry(&adapter->mbox_list) == &entry)
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
	 * Loop trying to get ownership of the mailbox.  Return an error
	 * if we can't gain ownership.
	 */
	v = G_MBOWNER(t4_read_reg(adapter, mbox_ctl));
	for (i = 0; v == X_MBOWNER_NONE && i < 3; i++)
		v = G_MBOWNER(t4_read_reg(adapter, mbox_ctl));

	if (v != X_MBOWNER_PL) {
		t4_os_atomic_list_del(&entry, &adapter->mbox_list,
				      &adapter->mbox_lock);
		ret = (v == X_MBOWNER_FW) ? -EBUSY : -ETIMEDOUT;
		return ret;
	}

	/*
	 * Write the command array into the Mailbox Data register array and
	 * transfer ownership of the mailbox to the firmware.
	 */
	for (i = 0, p = cmd; i < size; i += 8)
		t4_write_reg64(adapter, mbox_data + i, be64_to_cpu(*p++));

	t4_read_reg(adapter, mbox_data);          /* flush write */
	t4_write_reg(adapter, mbox_ctl,
			F_MBMSGVALID | V_MBOWNER(X_MBOWNER_FW));
	t4_read_reg(adapter, mbox_ctl);          /* flush write */
	delay_idx = 0;
	ms = delay[0];

	/*
	 * Spin waiting for firmware to acknowledge processing our command.
	 */
	for (i = 0; i < FW_CMD_MAX_TIMEOUT; i++) {
		if (sleep_ok) {
			ms = delay[delay_idx];  /* last element may repeat */
			if (delay_idx < ARRAY_SIZE(delay) - 1)
				delay_idx++;
			msleep(ms);
		} else {
			rte_delay_ms(ms);
		}

		/*
		 * If we're the owner, see if this is the reply we wanted.
		 */
		v = t4_read_reg(adapter, mbox_ctl);
		if (G_MBOWNER(v) == X_MBOWNER_PL) {
			/*
			 * If the Message Valid bit isn't on, revoke ownership
			 * of the mailbox and continue waiting for our reply.
			 */
			if ((v & F_MBMSGVALID) == 0) {
				t4_write_reg(adapter, mbox_ctl,
					     V_MBOWNER(X_MBOWNER_NONE));
				continue;
			}

			/*
			 * We now have our reply.  Extract the command return
			 * value, copy the reply back to our caller's buffer
			 * (if specified) and revoke ownership of the mailbox.
			 * We return the (negated) firmware command return
			 * code (this depends on FW_SUCCESS == 0).  (Again we
			 * avoid clogging the log with FW_VI_STATS_CMD
			 * reply results.)
			 */

			/*
			 * Retrieve the command reply and release the mailbox.
			 */
			get_mbox_rpl(adapter, cmd_rpl, size / 8, mbox_data);
			t4_write_reg(adapter, mbox_ctl,
				     V_MBOWNER(X_MBOWNER_NONE));
			t4_os_atomic_list_del(&entry, &adapter->mbox_list,
					      &adapter->mbox_lock);

			/* return value in high-order host-endian word */
			v = be64_to_cpu(cmd_rpl[0]);

			if (rpl) {
				/* request bit in high-order BE word */
				WARN_ON((be32_to_cpu(*(const u32 *)cmd)
					 & F_FW_CMD_REQUEST) == 0);
				memcpy(rpl, cmd_rpl, size);
			}
			return -((int)G_FW_CMD_RETVAL(v));
		}
	}

	/*
	 * We timed out.  Return the error ...
	 */
	dev_err(adapter, "command %#x timed out\n",
		*(const u8 *)cmd);
	dev_err(adapter, "    Control = %#x\n", t4_read_reg(adapter, mbox_ctl));
	t4_os_atomic_list_del(&entry, &adapter->mbox_list, &adapter->mbox_lock);
	ret = -ETIMEDOUT;
	return ret;
}
