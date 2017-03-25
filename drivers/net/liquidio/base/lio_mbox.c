/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Cavium, Inc.. All rights reserved.
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
 *     * Neither the name of Cavium, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER(S) OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_ethdev.h>
#include <rte_cycles.h>

#include "lio_logs.h"
#include "lio_struct.h"
#include "lio_mbox.h"

/**
 * lio_mbox_read:
 * @mbox: Pointer mailbox
 *
 * Reads the 8-bytes of data from the mbox register
 * Writes back the acknowledgment indicating completion of read
 */
int
lio_mbox_read(struct lio_mbox *mbox)
{
	union lio_mbox_message msg;
	int ret = 0;

	msg.mbox_msg64 = rte_read64(mbox->mbox_read_reg);

	if ((msg.mbox_msg64 == LIO_PFVFACK) || (msg.mbox_msg64 == LIO_PFVFSIG))
		return 0;

	if (mbox->state & LIO_MBOX_STATE_REQ_RECEIVING) {
		mbox->mbox_req.data[mbox->mbox_req.recv_len - 1] =
					msg.mbox_msg64;
		mbox->mbox_req.recv_len++;
	} else {
		if (mbox->state & LIO_MBOX_STATE_RES_RECEIVING) {
			mbox->mbox_resp.data[mbox->mbox_resp.recv_len - 1] =
					msg.mbox_msg64;
			mbox->mbox_resp.recv_len++;
		} else {
			if ((mbox->state & LIO_MBOX_STATE_IDLE) &&
					(msg.s.type == LIO_MBOX_REQUEST)) {
				mbox->state &= ~LIO_MBOX_STATE_IDLE;
				mbox->state |= LIO_MBOX_STATE_REQ_RECEIVING;
				mbox->mbox_req.msg.mbox_msg64 = msg.mbox_msg64;
				mbox->mbox_req.q_no = mbox->q_no;
				mbox->mbox_req.recv_len = 1;
			} else {
				if ((mbox->state &
				     LIO_MBOX_STATE_RES_PENDING) &&
				    (msg.s.type == LIO_MBOX_RESPONSE)) {
					mbox->state &=
						~LIO_MBOX_STATE_RES_PENDING;
					mbox->state |=
						LIO_MBOX_STATE_RES_RECEIVING;
					mbox->mbox_resp.msg.mbox_msg64 =
								msg.mbox_msg64;
					mbox->mbox_resp.q_no = mbox->q_no;
					mbox->mbox_resp.recv_len = 1;
				} else {
					rte_write64(LIO_PFVFERR,
						    mbox->mbox_read_reg);
					mbox->state |= LIO_MBOX_STATE_ERROR;
					return -1;
				}
			}
		}
	}

	if (mbox->state & LIO_MBOX_STATE_REQ_RECEIVING) {
		if (mbox->mbox_req.recv_len < msg.s.len) {
			ret = 0;
		} else {
			mbox->state &= ~LIO_MBOX_STATE_REQ_RECEIVING;
			mbox->state |= LIO_MBOX_STATE_REQ_RECEIVED;
			ret = 1;
		}
	} else {
		if (mbox->state & LIO_MBOX_STATE_RES_RECEIVING) {
			if (mbox->mbox_resp.recv_len < msg.s.len) {
				ret = 0;
			} else {
				mbox->state &= ~LIO_MBOX_STATE_RES_RECEIVING;
				mbox->state |= LIO_MBOX_STATE_RES_RECEIVED;
				ret = 1;
			}
		} else {
			RTE_ASSERT(0);
		}
	}

	rte_write64(LIO_PFVFACK, mbox->mbox_read_reg);

	return ret;
}

/**
 * lio_mbox_write:
 * @lio_dev: Pointer lio device
 * @mbox_cmd: Cmd to send to mailbox.
 *
 * Populates the queue specific mbox structure
 * with cmd information.
 * Write the cmd to mbox register
 */
int
lio_mbox_write(struct lio_device *lio_dev,
	       struct lio_mbox_cmd *mbox_cmd)
{
	struct lio_mbox *mbox = lio_dev->mbox[mbox_cmd->q_no];
	uint32_t count, i, ret = LIO_MBOX_STATUS_SUCCESS;

	if ((mbox_cmd->msg.s.type == LIO_MBOX_RESPONSE) &&
			!(mbox->state & LIO_MBOX_STATE_REQ_RECEIVED))
		return LIO_MBOX_STATUS_FAILED;

	if ((mbox_cmd->msg.s.type == LIO_MBOX_REQUEST) &&
			!(mbox->state & LIO_MBOX_STATE_IDLE))
		return LIO_MBOX_STATUS_BUSY;

	if (mbox_cmd->msg.s.type == LIO_MBOX_REQUEST) {
		rte_memcpy(&mbox->mbox_resp, mbox_cmd,
			   sizeof(struct lio_mbox_cmd));
		mbox->state = LIO_MBOX_STATE_RES_PENDING;
	}

	count = 0;

	while (rte_read64(mbox->mbox_write_reg) != LIO_PFVFSIG) {
		rte_delay_ms(1);
		if (count++ == 1000) {
			ret = LIO_MBOX_STATUS_FAILED;
			break;
		}
	}

	if (ret == LIO_MBOX_STATUS_SUCCESS) {
		rte_write64(mbox_cmd->msg.mbox_msg64, mbox->mbox_write_reg);
		for (i = 0; i < (uint32_t)(mbox_cmd->msg.s.len - 1); i++) {
			count = 0;
			while (rte_read64(mbox->mbox_write_reg) !=
					LIO_PFVFACK) {
				rte_delay_ms(1);
				if (count++ == 1000) {
					ret = LIO_MBOX_STATUS_FAILED;
					break;
				}
			}
			rte_write64(mbox_cmd->data[i], mbox->mbox_write_reg);
		}
	}

	if (mbox_cmd->msg.s.type == LIO_MBOX_RESPONSE) {
		mbox->state = LIO_MBOX_STATE_IDLE;
		rte_write64(LIO_PFVFSIG, mbox->mbox_read_reg);
	} else {
		if ((!mbox_cmd->msg.s.resp_needed) ||
				(ret == LIO_MBOX_STATUS_FAILED)) {
			mbox->state &= ~LIO_MBOX_STATE_RES_PENDING;
			if (!(mbox->state & (LIO_MBOX_STATE_REQ_RECEIVING |
					     LIO_MBOX_STATE_REQ_RECEIVED)))
				mbox->state = LIO_MBOX_STATE_IDLE;
		}
	}

	return ret;
}

/**
 * lio_mbox_process_cmd:
 * @mbox: Pointer mailbox
 * @mbox_cmd: Pointer to command received
 *
 * Process the cmd received in mbox
 */
static int
lio_mbox_process_cmd(struct lio_mbox *mbox,
		     struct lio_mbox_cmd *mbox_cmd)
{
	struct lio_device *lio_dev = mbox->lio_dev;

	if (mbox_cmd->msg.s.cmd == LIO_CORES_CRASHED)
		lio_dev_err(lio_dev, "Octeon core(s) crashed or got stuck!\n");

	return 0;
}

/**
 * Process the received mbox message.
 */
int
lio_mbox_process_message(struct lio_mbox *mbox)
{
	struct lio_mbox_cmd mbox_cmd;

	if (mbox->state & LIO_MBOX_STATE_ERROR) {
		if (mbox->state & (LIO_MBOX_STATE_RES_PENDING |
				   LIO_MBOX_STATE_RES_RECEIVING)) {
			rte_memcpy(&mbox_cmd, &mbox->mbox_resp,
				   sizeof(struct lio_mbox_cmd));
			mbox->state = LIO_MBOX_STATE_IDLE;
			rte_write64(LIO_PFVFSIG, mbox->mbox_read_reg);
			mbox_cmd.recv_status = 1;
			if (mbox_cmd.fn)
				mbox_cmd.fn(mbox->lio_dev, &mbox_cmd,
					    mbox_cmd.fn_arg);

			return 0;
		}

		mbox->state = LIO_MBOX_STATE_IDLE;
		rte_write64(LIO_PFVFSIG, mbox->mbox_read_reg);

		return 0;
	}

	if (mbox->state & LIO_MBOX_STATE_RES_RECEIVED) {
		rte_memcpy(&mbox_cmd, &mbox->mbox_resp,
			   sizeof(struct lio_mbox_cmd));
		mbox->state = LIO_MBOX_STATE_IDLE;
		rte_write64(LIO_PFVFSIG, mbox->mbox_read_reg);
		mbox_cmd.recv_status = 0;
		if (mbox_cmd.fn)
			mbox_cmd.fn(mbox->lio_dev, &mbox_cmd, mbox_cmd.fn_arg);

		return 0;
	}

	if (mbox->state & LIO_MBOX_STATE_REQ_RECEIVED) {
		rte_memcpy(&mbox_cmd, &mbox->mbox_req,
			   sizeof(struct lio_mbox_cmd));
		if (!mbox_cmd.msg.s.resp_needed) {
			mbox->state &= ~LIO_MBOX_STATE_REQ_RECEIVED;
			if (!(mbox->state & LIO_MBOX_STATE_RES_PENDING))
				mbox->state = LIO_MBOX_STATE_IDLE;
			rte_write64(LIO_PFVFSIG, mbox->mbox_read_reg);
		}

		lio_mbox_process_cmd(mbox, &mbox_cmd);

		return 0;
	}

	RTE_ASSERT(0);

	return 0;
}
