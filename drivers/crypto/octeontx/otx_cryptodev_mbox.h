/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _OTX_CRYPTODEV_MBOX_H_
#define _OTX_CRYPTODEV_MBOX_H_

#include <rte_byteorder.h>
#include <rte_common.h>

#include "cpt_common.h"
#include "cpt_pmd_logs.h"

#include "otx_cryptodev_hw_access.h"

#define OTX_CPT_MBOX_MSG_TIMEOUT    2000 /* In Milli Seconds */

#define OTX_CPT_MBOX_MSG_TYPE_REQ	0
#define OTX_CPT_MBOX_MSG_TYPE_ACK	1
#define OTX_CPT_MBOX_MSG_TYPE_NACK	2
#define OTX_CPT_MBOX_MSG_TYPE_NOP	3

/* CPT mailbox structure */
struct cpt_mbox {
	/** Message type MBOX[0] */
	uint64_t msg;
	/** Data         MBOX[1] */
	uint64_t data;
};

typedef enum {
	OTX_CPT_MSG_VF_UP = 1,
	OTX_CPT_MSG_VF_DOWN,
	OTX_CPT_MSG_READY,
	OTX_CPT_MSG_QLEN,
	OTX_CPT_MSG_QBIND_GRP,
	OTX_CPT_MSG_VQ_PRIORITY,
	OTX_CPT_MSG_PF_TYPE,
} otx_cpt_mbox_opcode_t;

typedef union {
	uint64_t u64;
	struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		uint32_t chip_id;
		uint8_t vfid;
		uint8_t reserved[3];
#else
		uint8_t reserved[3];
		uint8_t vfid;
		uint32_t chip_id;
#endif
	} s;
} otx_cpt_chipid_vfid_t;

/* Poll handler to handle mailbox messages from VFs */
void
otx_cpt_handle_mbox_intr(struct cpt_vf *cptvf);

/*
 * Checks if VF is able to comminicate with PF
 * and also gets the CPT number this VF is associated to.
 */
int
otx_cpt_check_pf_ready(struct cpt_vf *cptvf);

/*
 * Communicate VQs size to PF to program CPT(0)_PF_Q(0-15)_CTL of the VF.
 * Must be ACKed.
 */
int
otx_cpt_send_vq_size_msg(struct cpt_vf *cptvf);

/*
 * Communicate VF group required to PF and get the VQ binded to that group
 */
int
otx_cpt_send_vf_grp_msg(struct cpt_vf *cptvf, uint32_t group);

/*
 * Communicate to PF that VF is UP and running
 */
int
otx_cpt_send_vf_up(struct cpt_vf *cptvf);

/*
 * Communicate to PF that VF is DOWN and running
 */
int
otx_cpt_send_vf_down(struct cpt_vf *cptvf);

#endif /* _OTX_CRYPTODEV_MBOX_H_ */
