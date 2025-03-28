/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef __RNP_MBX_H__
#define __RNP_MBX_H__

#include "rnp_osdep.h"

#include "rnp_hw.h"

#define RNP_SRIOV_ADDR_CTRL	(0x7982fc)
#define RNP_SRIOV_ADDR_EN      RTE_BIT32(0)
#define RNP_SRIOV_INFO		(0x75f000)
#define RNP_PFVF_SHIFT		(4)
#define RNP_PF_BIT_MASK		RTE_BIT32(6)

#define RNP_MBX_MSG_BLOCK_SIZE	(14)
/* mailbox share memory detail divide */
/*|0------------------15|---------------------31|
 *|------master-req-----|-------master-ack------|
 *|------slave-req------|-------slave-ack-------|
 *|---------------------|-----------------------|
 *|		data(56 bytes)			|
 *----------------------------------------------|
 */
/* FW <--> PF */
#define RNP_FW2PF_MBOX_VEC	_MSI_(0x5300)
#define RNP_FW2PF_MEM_BASE	_MSI_(0xa000)
#define RNP_FW2PF_SYNC		(RNP_FW2PF_MEM_BASE + 0)
#define RNP_PF2FW_SYNC		(RNP_FW2PF_MEM_BASE + 4)
#define RNP_FW2PF_MSG_DATA	(RNP_FW2PF_MEM_BASE + 8)
#define RNP_PF2FW_MBX_CTRL	_MSI_(0xa100)
#define RNP_FW2PF_MBX_CTRL	_MSI_(0xa200)
#define RNP_FW2PF_INTR_MASK	_MSI_(0xa300)
/* PF <-> VF */
#define RNP_PF2VF_MBOX_VEC(vf)	_MSI_(0x5100 + (4 * (vf)))
#define RNP_PF2VF_MEM_BASE(vf)	_MSI_(0x6000 + (64 * (vf)))
#define RNP_PF2VF_SYNC(vf)	(RNP_PF2VF_MEM_BASE(vf) + 0)
#define RNP_VF2PF_SYNC(vf)	(RNP_PF2VF_MEM_BASE(vf) + 4)
#define RNP_PF2VF_MSG_DATA(vf)	(RNP_PF2VF_MEM_BASE(vf) + 8)
#define RNP_VF2PF_MBX_CTRL(vf)	_MSI_(0x7000 + ((vf) * 4))
#define RNP_PF2VF_MBX_CTRL(vf)	_MSI_(0x7100 + ((vf) * 4))
#define RNP_PF2VF_INTR_MASK(vf)	_MSI_(0x7200 + ((((vf) & 32) / 32) * 0x4))
/* sync memory define */
#define RNP_MBX_SYNC_REQ_MASK	RTE_GENMASK32(15, 0)
#define RNP_MBX_SYNC_ACK_MASK	RTE_GENMASK32(31, 16)
#define RNP_MBX_SYNC_ACK_S	(16)
/* for pf <--> fw/vf */
#define RNP_MBX_CTRL_PF_HOLD	RTE_BIT32(3) /* VF:RO, PF:WR */
#define RNP_MBX_CTRL_REQ	RTE_BIT32(0) /* msg write request */

#define RNP_MBX_DELAY_US	(100) /* delay us for retry */
#define RNP_MBX_MAX_TM_SEC	(4 * 1000 * 1000) /* 4 sec */

#define RNP_ERR_MBX		(-100)
int rnp_init_mbx_pf(struct rnp_hw *hw);

#endif /* _RNP_MBX_H_ */
