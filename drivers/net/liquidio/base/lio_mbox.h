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

#ifndef _LIO_MBOX_H_
#define _LIO_MBOX_H_

#include <stdint.h>

#include <rte_spinlock.h>

/* Macros for Mail Box Communication */

#define LIO_MBOX_DATA_MAX			32

#define LIO_VF_ACTIVE				0x1
#define LIO_VF_FLR_REQUEST			0x2
#define LIO_CORES_CRASHED			0x3

/* Macro for Read acknowledgment */
#define LIO_PFVFACK				0xffffffffffffffff
#define LIO_PFVFSIG				0x1122334455667788
#define LIO_PFVFERR				0xDEADDEADDEADDEAD

enum lio_mbox_cmd_status {
	LIO_MBOX_STATUS_SUCCESS		= 0,
	LIO_MBOX_STATUS_FAILED		= 1,
	LIO_MBOX_STATUS_BUSY		= 2
};

enum lio_mbox_message_type {
	LIO_MBOX_REQUEST	= 0,
	LIO_MBOX_RESPONSE	= 1
};

union lio_mbox_message {
	uint64_t mbox_msg64;
	struct {
		uint16_t type : 1;
		uint16_t resp_needed : 1;
		uint16_t cmd : 6;
		uint16_t len : 8;
		uint8_t params[6];
	} s;
};

typedef void (*lio_mbox_callback)(void *, void *, void *);

struct lio_mbox_cmd {
	union lio_mbox_message msg;
	uint64_t data[LIO_MBOX_DATA_MAX];
	uint32_t q_no;
	uint32_t recv_len;
	uint32_t recv_status;
	lio_mbox_callback fn;
	void *fn_arg;
};

enum lio_mbox_state {
	LIO_MBOX_STATE_IDLE		= 1,
	LIO_MBOX_STATE_REQ_RECEIVING	= 2,
	LIO_MBOX_STATE_REQ_RECEIVED	= 4,
	LIO_MBOX_STATE_RES_PENDING	= 8,
	LIO_MBOX_STATE_RES_RECEIVING	= 16,
	LIO_MBOX_STATE_RES_RECEIVED	= 16,
	LIO_MBOX_STATE_ERROR		= 32
};

struct lio_mbox {
	/* A spinlock to protect access to this q_mbox. */
	rte_spinlock_t lock;

	struct lio_device *lio_dev;

	uint32_t q_no;

	enum lio_mbox_state state;

	/* SLI_MAC_PF_MBOX_INT for PF, SLI_PKT_MBOX_INT for VF. */
	void *mbox_int_reg;

	/* SLI_PKT_PF_VF_MBOX_SIG(0) for PF,
	 * SLI_PKT_PF_VF_MBOX_SIG(1) for VF.
	 */
	void *mbox_write_reg;

	/* SLI_PKT_PF_VF_MBOX_SIG(1) for PF,
	 * SLI_PKT_PF_VF_MBOX_SIG(0) for VF.
	 */
	void *mbox_read_reg;

	struct lio_mbox_cmd mbox_req;

	struct lio_mbox_cmd mbox_resp;

};

int lio_mbox_read(struct lio_mbox *mbox);
int lio_mbox_write(struct lio_device *lio_dev,
		   struct lio_mbox_cmd *mbox_cmd);
int lio_mbox_process_message(struct lio_mbox *mbox);
#endif	/* _LIO_MBOX_H_ */
