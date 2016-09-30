/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2016.
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
 *     * Neither the name of Cavium networks nor the names of its
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

#ifndef __THUNDERX_NICVF_MBOX__
#define __THUNDERX_NICVF_MBOX__

#include <stdint.h>

#include "nicvf_plat.h"
#include "../nicvf_struct.h"

/* PF <--> VF Mailbox communication
 * Two 64bit registers are shared between PF and VF for each VF
 * Writing into second register means end of message.
 */

/* PF <--> VF mailbox communication */
#define	NIC_PF_VF_MAILBOX_SIZE		2
#define	NIC_MBOX_MSG_TIMEOUT		2000	/* ms */

/* Mailbox message types */
#define	NIC_MBOX_MSG_INVALID		0x00	/* Invalid message */
#define	NIC_MBOX_MSG_READY		0x01	/* Is PF ready to rcv msgs */
#define	NIC_MBOX_MSG_ACK		0x02	/* ACK the message received */
#define	NIC_MBOX_MSG_NACK		0x03	/* NACK the message received */
#define	NIC_MBOX_MSG_QS_CFG		0x04	/* Configure Qset */
#define	NIC_MBOX_MSG_RQ_CFG		0x05	/* Configure receive queue */
#define	NIC_MBOX_MSG_SQ_CFG		0x06	/* Configure Send queue */
#define	NIC_MBOX_MSG_RQ_DROP_CFG	0x07	/* Configure receive queue */
#define	NIC_MBOX_MSG_SET_MAC		0x08	/* Add MAC ID to DMAC filter */
#define	NIC_MBOX_MSG_SET_MAX_FRS	0x09	/* Set max frame size */
#define	NIC_MBOX_MSG_CPI_CFG		0x0A	/* Config CPI, RSSI */
#define	NIC_MBOX_MSG_RSS_SIZE		0x0B	/* Get RSS indir_tbl size */
#define	NIC_MBOX_MSG_RSS_CFG		0x0C	/* Config RSS table */
#define	NIC_MBOX_MSG_RSS_CFG_CONT	0x0D	/* RSS config continuation */
#define	NIC_MBOX_MSG_RQ_BP_CFG		0x0E	/* RQ backpressure config */
#define	NIC_MBOX_MSG_RQ_SW_SYNC		0x0F	/* Flush inflight pkts to RQ */
#define	NIC_MBOX_MSG_BGX_LINK_CHANGE	0x11	/* BGX:LMAC link status */
#define	NIC_MBOX_MSG_ALLOC_SQS		0x12	/* Allocate secondary Qset */
#define	NIC_MBOX_MSG_LOOPBACK		0x16	/* Set interface in loopback */
#define	NIC_MBOX_MSG_RESET_STAT_COUNTER 0x17	/* Reset statistics counters */
#define	NIC_MBOX_MSG_CFG_DONE		0x7E	/* VF configuration done */
#define	NIC_MBOX_MSG_SHUTDOWN		0x7F	/* VF is being shutdown */
#define	NIC_MBOX_MSG_RES_BIT		0x80	/* Reset bit from PF */
#define	NIC_MBOX_MSG_MAX		0x100	/* Maximum number of messages */

#define NIC_MBOX_MSG_RSS_SIZE_RES_BIT \
	(NIC_MBOX_MSG_RSS_SIZE | NIC_MBOX_MSG_RES_BIT)
#define NIC_MBOX_MSG_ALLOC_SQS_RES_BIT \
	(NIC_MBOX_MSG_ALLOC_SQS | NIC_MBOX_MSG_RES_BIT)

/* Get vNIC VF configuration */
struct nic_cfg_msg {
	uint8_t    msg;
	uint8_t    vf_id;
	uint8_t    node_id;
	bool	   tns_mode:1;
	bool	   sqs_mode:1;
	bool	   loopback_supported:1;
	uint8_t    mac_addr[NICVF_MAC_ADDR_SIZE];
};

/* Qset configuration */
struct qs_cfg_msg {
	uint8_t    msg;
	uint8_t    num;
	uint8_t    sqs_count;
	uint64_t   cfg;
};

/* Receive queue configuration */
struct rq_cfg_msg {
	uint8_t    msg;
	uint8_t    qs_num;
	uint8_t    rq_num;
	uint64_t   cfg;
};

/* Send queue configuration */
struct sq_cfg_msg {
	uint8_t    msg;
	uint8_t    qs_num;
	uint8_t    sq_num;
	bool       sqs_mode;
	uint64_t   cfg;
};

/* Set VF's MAC address */
struct set_mac_msg {
	uint8_t    msg;
	uint8_t    vf_id;
	uint8_t    mac_addr[NICVF_MAC_ADDR_SIZE];
};

/* Set Maximum frame size */
struct set_frs_msg {
	uint8_t    msg;
	uint8_t    vf_id;
	uint16_t   max_frs;
};

/* Set CPI algorithm type */
struct cpi_cfg_msg {
	uint8_t    msg;
	uint8_t    vf_id;
	uint8_t    rq_cnt;
	uint8_t    cpi_alg;
};

/* Get RSS table size */
struct rss_sz_msg {
	uint8_t    msg;
	uint8_t    vf_id;
	uint16_t   ind_tbl_size;
};

/* Set RSS configuration */
struct rss_cfg_msg {
	uint8_t    msg;
	uint8_t    vf_id;
	uint8_t    hash_bits;
	uint8_t    tbl_len;
	uint8_t    tbl_offset;
#define RSS_IND_TBL_LEN_PER_MBX_MSG	8
	uint8_t    ind_tbl[RSS_IND_TBL_LEN_PER_MBX_MSG];
};

/* Physical interface link status */
struct bgx_link_status {
	uint8_t    msg;
	uint8_t    link_up;
	uint8_t    duplex;
	uint32_t   speed;
};

/* Allocate additional SQS to VF */
struct sqs_alloc {
	uint8_t    msg;
	uint8_t    spec;
	uint8_t    qs_count;
	uint8_t    svf[MAX_SQS_PER_VF];
};

/* Set interface in loopback mode */
struct set_loopback {
	uint8_t    msg;
	uint8_t    vf_id;
	bool	   enable;
};

/* Reset statistics counters */
struct reset_stat_cfg {
	uint8_t    msg;
	/* Bitmap to select NIC_PF_VNIC(vf_id)_RX_STAT(0..13) */
	uint16_t   rx_stat_mask;
	/* Bitmap to select NIC_PF_VNIC(vf_id)_TX_STAT(0..4) */
	uint8_t    tx_stat_mask;
	/* Bitmap to select NIC_PF_QS(0..127)_RQ(0..7)_STAT(0..1)
	 * bit14, bit15 NIC_PF_QS(vf_id)_RQ7_STAT(0..1)
	 * bit12, bit13 NIC_PF_QS(vf_id)_RQ6_STAT(0..1)
	 * ..
	 * bit2, bit3 NIC_PF_QS(vf_id)_RQ1_STAT(0..1)
	 * bit0, bit1 NIC_PF_QS(vf_id)_RQ0_STAT(0..1)
	 */
	uint16_t   rq_stat_mask;
	/* Bitmap to select NIC_PF_QS(0..127)_SQ(0..7)_STAT(0..1)
	 * bit14, bit15 NIC_PF_QS(vf_id)_SQ7_STAT(0..1)
	 * bit12, bit13 NIC_PF_QS(vf_id)_SQ6_STAT(0..1)
	 * ..
	 * bit2, bit3 NIC_PF_QS(vf_id)_SQ1_STAT(0..1)
	 * bit0, bit1 NIC_PF_QS(vf_id)_SQ0_STAT(0..1)
	 */
	uint16_t   sq_stat_mask;
};

struct nic_mbx {
/* 128 bit shared memory between PF and each VF */
union {
	struct { uint8_t msg; }	msg;
	struct nic_cfg_msg	nic_cfg;
	struct qs_cfg_msg	qs;
	struct rq_cfg_msg	rq;
	struct sq_cfg_msg	sq;
	struct set_mac_msg	mac;
	struct set_frs_msg	frs;
	struct cpi_cfg_msg	cpi_cfg;
	struct rss_sz_msg	rss_size;
	struct rss_cfg_msg	rss_cfg;
	struct bgx_link_status  link_status;
	struct sqs_alloc	sqs_alloc;
	struct set_loopback	lbk;
	struct reset_stat_cfg	reset_stat;
};
};

NICVF_STATIC_ASSERT(sizeof(struct nic_mbx) <= 16);

int nicvf_handle_mbx_intr(struct nicvf *nic);
int nicvf_mbox_check_pf_ready(struct nicvf *nic);
int nicvf_mbox_qset_config(struct nicvf *nic, struct pf_qs_cfg *qs_cfg);
int nicvf_mbox_request_sqs(struct nicvf *nic);
int nicvf_mbox_rq_config(struct nicvf *nic, uint16_t qidx,
			 struct pf_rq_cfg *pf_rq_cfg);
int nicvf_mbox_sq_config(struct nicvf *nic, uint16_t qidx);
int nicvf_mbox_rq_drop_config(struct nicvf *nic, uint16_t qidx, bool enable);
int nicvf_mbox_rq_bp_config(struct nicvf *nic, uint16_t qidx, bool enable);
int nicvf_mbox_set_mac_addr(struct nicvf *nic,
			    const uint8_t mac[NICVF_MAC_ADDR_SIZE]);
int nicvf_mbox_config_cpi(struct nicvf *nic, uint32_t qcnt);
int nicvf_mbox_get_rss_size(struct nicvf *nic);
int nicvf_mbox_config_rss(struct nicvf *nic);
int nicvf_mbox_update_hw_max_frs(struct nicvf *nic, uint16_t mtu);
int nicvf_mbox_rq_sync(struct nicvf *nic);
int nicvf_mbox_loopback_config(struct nicvf *nic, bool enable);
int nicvf_mbox_reset_stat_counters(struct nicvf *nic, uint16_t rx_stat_mask,
	uint8_t tx_stat_mask, uint16_t rq_stat_mask, uint16_t sq_stat_mask);
void nicvf_mbox_shutdown(struct nicvf *nic);
void nicvf_mbox_cfg_done(struct nicvf *nic);

#endif /* __THUNDERX_NICVF_MBOX__ */
