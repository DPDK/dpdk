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

#ifndef _LIO_23XX_REG_H_
#define _LIO_23XX_REG_H_

/* ###################### REQUEST QUEUE ######################### */

/* 64 registers for Input Queues Start Addr - SLI_PKT(0..63)_INSTR_BADDR */
#define CN23XX_SLI_PKT_INSTR_BADDR_START64	0x10010

/* 64 registers for Input Doorbell - SLI_PKT(0..63)_INSTR_BAOFF_DBELL */
#define CN23XX_SLI_PKT_INSTR_BADDR_DBELL_START	0x10020

/* 64 registers for Input Queue size - SLI_PKT(0..63)_INSTR_FIFO_RSIZE */
#define CN23XX_SLI_PKT_INSTR_FIFO_RSIZE_START	0x10030

/* 64 registers for Input Queue Instr Count - SLI_PKT_IN_DONE(0..63)_CNTS */
#define CN23XX_SLI_PKT_IN_DONE_CNTS_START64	0x10040

/* 64 registers (64-bit) - ES, RO, NS, Arbitration for Input Queue Data &
 * gather list fetches. SLI_PKT(0..63)_INPUT_CONTROL.
 */
#define CN23XX_SLI_PKT_INPUT_CONTROL_START64	0x10000

/* ------- Request Queue Macros --------- */

/* Each Input Queue register is at a 16-byte Offset in BAR0 */
#define CN23XX_IQ_OFFSET			0x20000

#define CN23XX_SLI_IQ_PKT_CONTROL64(iq)					\
	(CN23XX_SLI_PKT_INPUT_CONTROL_START64 + ((iq) * CN23XX_IQ_OFFSET))

#define CN23XX_SLI_IQ_BASE_ADDR64(iq)					\
	(CN23XX_SLI_PKT_INSTR_BADDR_START64 + ((iq) * CN23XX_IQ_OFFSET))

#define CN23XX_SLI_IQ_SIZE(iq)						\
	(CN23XX_SLI_PKT_INSTR_FIFO_RSIZE_START + ((iq) * CN23XX_IQ_OFFSET))

#define CN23XX_SLI_IQ_DOORBELL(iq)					\
	(CN23XX_SLI_PKT_INSTR_BADDR_DBELL_START + ((iq) * CN23XX_IQ_OFFSET))

#define CN23XX_SLI_IQ_INSTR_COUNT64(iq)					\
	(CN23XX_SLI_PKT_IN_DONE_CNTS_START64 + ((iq) * CN23XX_IQ_OFFSET))

/* Number of instructions to be read in one MAC read request.
 * setting to Max value(4)
 */
#define CN23XX_PKT_INPUT_CTL_RDSIZE			(3 << 25)
#define CN23XX_PKT_INPUT_CTL_IS_64B			(1 << 24)
#define CN23XX_PKT_INPUT_CTL_RST			(1 << 23)
#define CN23XX_PKT_INPUT_CTL_QUIET			(1 << 28)
#define CN23XX_PKT_INPUT_CTL_RING_ENB			(1 << 22)
#define CN23XX_PKT_INPUT_CTL_DATA_ES_64B_SWAP		(1 << 6)
#define CN23XX_PKT_INPUT_CTL_USE_CSR			(1 << 4)
#define CN23XX_PKT_INPUT_CTL_GATHER_ES_64B_SWAP		(2)

/* These bits[47:44] select the Physical function number within the MAC */
#define CN23XX_PKT_INPUT_CTL_PF_NUM_POS		45
/* These bits[43:32] select the function number within the PF */
#define CN23XX_PKT_INPUT_CTL_VF_NUM_POS		32

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
#define CN23XX_PKT_INPUT_CTL_MASK			\
	(CN23XX_PKT_INPUT_CTL_RDSIZE |			\
	 CN23XX_PKT_INPUT_CTL_DATA_ES_64B_SWAP |	\
	 CN23XX_PKT_INPUT_CTL_USE_CSR)
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
#define CN23XX_PKT_INPUT_CTL_MASK			\
	(CN23XX_PKT_INPUT_CTL_RDSIZE |			\
	 CN23XX_PKT_INPUT_CTL_DATA_ES_64B_SWAP |	\
	 CN23XX_PKT_INPUT_CTL_USE_CSR |			\
	 CN23XX_PKT_INPUT_CTL_GATHER_ES_64B_SWAP)
#endif

/* ############################ OUTPUT QUEUE ######################### */

/* 64 registers for Output queue control - SLI_PKT(0..63)_OUTPUT_CONTROL */
#define CN23XX_SLI_PKT_OUTPUT_CONTROL_START	0x10050

/* 64 registers for Output queue buffer and info size
 * SLI_PKT(0..63)_OUT_SIZE
 */
#define CN23XX_SLI_PKT_OUT_SIZE			0x10060

/* 64 registers for Output Queue Start Addr - SLI_PKT(0..63)_SLIST_BADDR */
#define CN23XX_SLI_SLIST_BADDR_START64		0x10070

/* 64 registers for Output Queue Packet Credits
 * SLI_PKT(0..63)_SLIST_BAOFF_DBELL
 */
#define CN23XX_SLI_PKT_SLIST_BAOFF_DBELL_START	0x10080

/* 64 registers for Output Queue size - SLI_PKT(0..63)_SLIST_FIFO_RSIZE */
#define CN23XX_SLI_PKT_SLIST_FIFO_RSIZE_START	0x10090

/* 64 registers for Output Queue Packet Count - SLI_PKT(0..63)_CNTS */
#define CN23XX_SLI_PKT_CNTS_START		0x100B0

/* Each Output Queue register is at a 16-byte Offset in BAR0 */
#define CN23XX_OQ_OFFSET			0x20000

/* ------- Output Queue Macros --------- */

#define CN23XX_SLI_OQ_PKT_CONTROL(oq)					\
	(CN23XX_SLI_PKT_OUTPUT_CONTROL_START + ((oq) * CN23XX_OQ_OFFSET))

#define CN23XX_SLI_OQ_BASE_ADDR64(oq)					\
	(CN23XX_SLI_SLIST_BADDR_START64 + ((oq) * CN23XX_OQ_OFFSET))

#define CN23XX_SLI_OQ_SIZE(oq)						\
	(CN23XX_SLI_PKT_SLIST_FIFO_RSIZE_START + ((oq) * CN23XX_OQ_OFFSET))

#define CN23XX_SLI_OQ_BUFF_INFO_SIZE(oq)				\
	(CN23XX_SLI_PKT_OUT_SIZE + ((oq) * CN23XX_OQ_OFFSET))

#define CN23XX_SLI_OQ_PKTS_SENT(oq)					\
	(CN23XX_SLI_PKT_CNTS_START + ((oq) * CN23XX_OQ_OFFSET))

#define CN23XX_SLI_OQ_PKTS_CREDIT(oq)					\
	(CN23XX_SLI_PKT_SLIST_BAOFF_DBELL_START + ((oq) * CN23XX_OQ_OFFSET))

/* ------------------ Masks ---------------- */
#define CN23XX_PKT_OUTPUT_CTL_IPTR		(1 << 11)
#define CN23XX_PKT_OUTPUT_CTL_ES		(1 << 9)
#define CN23XX_PKT_OUTPUT_CTL_NSR		(1 << 8)
#define CN23XX_PKT_OUTPUT_CTL_ROR		(1 << 7)
#define CN23XX_PKT_OUTPUT_CTL_DPTR		(1 << 6)
#define CN23XX_PKT_OUTPUT_CTL_BMODE		(1 << 5)
#define CN23XX_PKT_OUTPUT_CTL_ES_P		(1 << 3)
#define CN23XX_PKT_OUTPUT_CTL_NSR_P		(1 << 2)
#define CN23XX_PKT_OUTPUT_CTL_ROR_P		(1 << 1)
#define CN23XX_PKT_OUTPUT_CTL_RING_ENB		(1 << 0)

/* Rings per Virtual Function [RO] */
#define CN23XX_PKT_INPUT_CTL_RPVF_MASK		0x3F
#define CN23XX_PKT_INPUT_CTL_RPVF_POS		48

/* These bits[47:44][RO] give the Physical function
 * number info within the MAC
 */
#define CN23XX_PKT_INPUT_CTL_PF_NUM_MASK	0x7

/* These bits[43:32][RO] give the virtual function
 * number info within the PF
 */
#define CN23XX_PKT_INPUT_CTL_VF_NUM_MASK	0x1FFF

/* ######################### Mailbox Reg Macros ######################## */
#define CN23XX_SLI_PKT_PF_VF_MBOX_SIG_START	0x10200
#define CN23XX_VF_SLI_PKT_MBOX_INT_START	0x10210

#define CN23XX_SLI_MBOX_OFFSET			0x20000
#define CN23XX_SLI_MBOX_SIG_IDX_OFFSET		0x8

#define CN23XX_SLI_PKT_PF_VF_MBOX_SIG(q, idx)				\
	(CN23XX_SLI_PKT_PF_VF_MBOX_SIG_START +				\
	 ((q) * CN23XX_SLI_MBOX_OFFSET +				\
	  (idx) * CN23XX_SLI_MBOX_SIG_IDX_OFFSET))

#define CN23XX_VF_SLI_PKT_MBOX_INT(q)					\
	(CN23XX_VF_SLI_PKT_MBOX_INT_START + ((q) * CN23XX_SLI_MBOX_OFFSET))

#endif /* _LIO_23XX_REG_H_ */
