/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2026 Marvell.
 */

#ifndef _CN20K_EP_MBOX_H_
#define _CN20K_EP_MBOX_H_

#include <stdint.h>

#include "otx_ep_mbox.h"

struct otx_ep_device;

/* CN20K-specific mailbox opcodes */
enum otx_ep_cn20k_mbox_opcode {
	OCTEP_CN20K_MBOX_CMD_VF_READY = 0x100,  /* CN20K VF readiness notification */
};

/* SDP mbox IDs */
#define MBOX_MSG_SDP_RING_ALLOC	0x1002
#define MBOX_MSG_SDP_RING_FREE	0x1003

/* Mailbox message header */
struct otx_ep_cn20k_mbox_msghdr {
	uint16_t pcifunc;       /* VF/PF identifier */
	uint16_t id;            /* Message ID (opcode) */
#define OCTEP_CN20K_MBOX_REQ_SIG (0xdead)
#define OCTEP_CN20K_MBOX_RSP_SIG (0xbeef)
	uint16_t sig;           /* Signature for verification */
#define OCTEP_CN20K_MBOX_VERSION (0x000b)
	uint16_t ver;           /* Mailbox version */
	uint16_t next_msgoff;   /* Offset to next message */
	int rc;                 /* Return code */
};

/* Generic request msg used for those mbox messages which
 * don't send any data in the request.
 */
struct otx_ep_cn20k_msg_req {
	struct otx_ep_cn20k_mbox_msghdr hdr;
};

/* Generic response msg used as ack or response for those mbox
 * messages which don't have a specific rsp msg format.
 */
struct otx_ep_cn20k_msg_rsp {
	struct otx_ep_cn20k_mbox_msghdr hdr;
};

/* VF_READY request message (VF → PF) */
struct otx_ep_cn20k_ready_msg_req {
	struct otx_ep_cn20k_mbox_msghdr hdr;
};

/* VF_READY response message (PF → VF) */
struct otx_ep_cn20k_ready_msg_rsp {
	struct otx_ep_cn20k_mbox_msghdr hdr;
};

#define MBOX_MSG_ALIGN 16
#define MBOX_DOWN_MSG  1
#define MBOX_UP_MSG    2

/* Mailbox message header */
struct cn20k_mbox_hdr {
	uint64_t msg_size;
	uint16_t num_msgs;
	uint16_t opt_msg;
	uint16_t sig;
};

enum cn20k_mbox_dir {
	MBOX_DIR_HOSTVF_HOSTPF = 0,
	MBOX_DIR_HOSTVF_HOSTPF_UP = 1,
};

struct otx_ep_cn20k_mbox_priv {
	struct otx_ep_device *otx_ep;
	void *hwbase;
	void *mbase;
	uint64_t trigger;
	uint64_t rx_start;
	uint64_t tx_start;
	uint16_t rx_size;
	uint16_t tx_size;
	uint16_t msg_size;
	uint16_t rsp_size;
	uint16_t num_msgs;
	uint16_t msgs_acked;
};

/* Main mailbox container for VF */
struct otx_ep_cn20k_mbox {
	struct otx_ep_cn20k_mbox_priv mbox;
	struct otx_ep_cn20k_mbox_priv mbox_up;
	struct otx_ep_device *otx_ep;
	void *bbuf_base;
	int num_msgs;
	int up_num_msgs;
};

/* SDP Ring allocation/free structures (same as octeon_ep) */
struct sdp_rings_alloc_req {
	struct otx_ep_cn20k_mbox_msghdr hdr;
	uint16_t nr_rings;
	uint16_t rsvd[16];   /* Reserved */
};

struct sdp_rings_alloc_rsp {
	struct otx_ep_cn20k_mbox_msghdr hdr;
	uint16_t count; /* Number of rings allocated */
	uint16_t rsvd[16];   /* Reserved */
};

struct sdp_rings_free_req {
	struct otx_ep_cn20k_mbox_msghdr hdr;
	uint16_t ring;
	uint8_t all;
};

int otx_ep_cn20k_mbox_init(struct rte_eth_dev *eth_dev);
void otx_ep_cn20k_mbox_uninit(struct rte_eth_dev *eth_dev);

int otx_ep_cn20k_mbox_send_cmd(struct otx_ep_device *otx_ep, union otx_ep_mbox_word cmd,
			       union otx_ep_mbox_word *rsp);
int otx_ep_cn20k_mbox_bulk_read(struct otx_ep_device *otx_ep, enum otx_ep_mbox_opcode opcode,
				uint8_t *data, int32_t max_size, int32_t *size);

int otx_ep_cn20k_mbox_send_ready(struct otx_ep_device *otx_ep);
int otx_ep_cn20k_mbox_alloc_sdp_rings(struct otx_ep_device *otx_ep, uint16_t nr_rings);
int otx_ep_cn20k_mbox_free_sdp_rings(struct otx_ep_device *otx_ep, uint16_t ring, uint8_t all);

#endif /* _CN20K_EP_MBOX_H_ */
