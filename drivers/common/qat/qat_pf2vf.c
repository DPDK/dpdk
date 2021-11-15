/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include "qat_pf2vf.h"
#include "adf_pf2vf_msg.h"

#include <rte_cycles.h>

int qat_pf2vf_exch_msg(struct qat_pci_device *qat_dev,
		struct qat_pf2vf_msg pf2vf_msg,
		int len, uint8_t *ret)
{
	int i = 0;
	struct qat_pf2vf_dev *qat_pf2vf =
	qat_gen_config[qat_dev->qat_dev_gen].pf2vf_dev;
	void *pmisc_bar_addr = qat_dev->misc_bar_io_addr;
	uint32_t msg = 0, count = 0, val = 0;
	uint32_t vf_csr_off = qat_pf2vf->vf2pf_offset;
	uint32_t pf_csr_off = qat_pf2vf->pf2vf_offset;
	int type_shift = qat_pf2vf->pf2vf_type_shift;
	uint32_t type_mask = qat_pf2vf->pf2vf_type_mask;
	int blck_hdr_shift = qat_pf2vf->pf2vf_data_shift;
	int data_shift = blck_hdr_shift;

	switch (pf2vf_msg.msg_type) {
	case ADF_VF2PF_MSGTYPE_GET_SMALL_BLOCK_REQ:
		data_shift += ADF_VF2PF_SMALL_BLOCK_BYTE_NUM_SHIFT;
		break;
	case ADF_VF2PF_MSGTYPE_GET_MEDIUM_BLOCK_REQ:
		data_shift += ADF_VF2PF_MEDIUM_BLOCK_BYTE_NUM_SHIFT;
		break;
	case ADF_VF2PF_MSGTYPE_GET_LARGE_BLOCK_REQ:
		data_shift += ADF_VF2PF_LARGE_BLOCK_BYTE_NUM_SHIFT;
		break;
	}

	if ((pf2vf_msg.msg_type & type_mask) != pf2vf_msg.msg_type) {
		QAT_LOG(ERR, "PF2VF message type 0x%X out of range\n",
			pf2vf_msg.msg_type);
		return -EINVAL;
	}

	for (; i < len; i++) {
		count = 0;
		if (len == 1) {
			msg = (pf2vf_msg.msg_type << type_shift) |
				(pf2vf_msg.msg_data << (data_shift));
		} else
			msg = (pf2vf_msg.msg_type << type_shift) |
				((pf2vf_msg.msg_data + i) << (data_shift));
		if (pf2vf_msg.block_hdr > 0)
			msg |= pf2vf_msg.block_hdr << blck_hdr_shift;
		msg |= ADF_PFVF_INT | ADF_PFVF_MSGORIGIN_SYSTEM;

		ADF_CSR_WR(pmisc_bar_addr, vf_csr_off, msg);
		/*
		 * Wait for confirmation from remote that it received
		 * the message
		 */
		do {
			rte_delay_us_sleep(5);
			val = ADF_CSR_RD(pmisc_bar_addr, vf_csr_off);
		} while ((val & ADF_PFVF_INT) &&
			(++count < ADF_IOV_MSG_ACK_MAX_RETRY));

		if (val & ADF_PFVF_INT) {
			QAT_LOG(ERR, "ACK not received from remote\n");
			return -EIO;
		}

		uint32_t pf_val = ADF_CSR_RD(pmisc_bar_addr, pf_csr_off);

		*(ret + i) = (uint8_t)(pf_val >> (pf2vf_msg.block_hdr > 0 ?
				10 : 8) & 0xff);
	}
	return 0;
}
