/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <stdint.h>

#include <bus_pci_driver.h>

#include <rte_io.h>

#include "odm.h"
#include "odm_priv.h"

static void
odm_vchan_resc_free(struct odm_dev *odm, int qno)
{
	RTE_SET_USED(odm);
	RTE_SET_USED(qno);
}

static int
send_mbox_to_pf(struct odm_dev *odm, union odm_mbox_msg *msg, union odm_mbox_msg *rsp)
{
	int retry_cnt = ODM_MBOX_RETRY_CNT;
	union odm_mbox_msg pf_msg;

	msg->d.err = ODM_MBOX_ERR_CODE_MAX;
	odm_write64(msg->u[0], odm->rbase + ODM_MBOX_VF_PF_DATA(0));
	odm_write64(msg->u[1], odm->rbase + ODM_MBOX_VF_PF_DATA(1));

	pf_msg.u[0] = 0;
	pf_msg.u[1] = 0;
	pf_msg.u[0] = odm_read64(odm->rbase + ODM_MBOX_VF_PF_DATA(0));

	while (pf_msg.d.rsp == 0 && retry_cnt > 0) {
		pf_msg.u[0] = odm_read64(odm->rbase + ODM_MBOX_VF_PF_DATA(0));
		--retry_cnt;
	}

	if (retry_cnt <= 0)
		return -EBADE;

	pf_msg.u[1] = odm_read64(odm->rbase + ODM_MBOX_VF_PF_DATA(1));

	if (rsp) {
		rsp->u[0] = pf_msg.u[0];
		rsp->u[1] = pf_msg.u[1];
	}

	if (pf_msg.d.rsp == msg->d.err && pf_msg.d.err != 0)
		return -EBADE;

	return 0;
}

int
odm_dev_init(struct odm_dev *odm)
{
	struct rte_pci_device *pci_dev = odm->pci_dev;
	union odm_mbox_msg mbox_msg;
	uint16_t vfid;
	int rc;

	odm->rbase = pci_dev->mem_resource[0].addr;
	vfid = ((pci_dev->addr.devid & 0x1F) << 3) | (pci_dev->addr.function & 0x7);
	vfid -= 1;
	odm->vfid = vfid;
	odm->num_qs = 0;

	mbox_msg.u[0] = 0;
	mbox_msg.u[1] = 0;
	mbox_msg.q.vfid = odm->vfid;
	mbox_msg.q.cmd = ODM_DEV_INIT;
	rc = send_mbox_to_pf(odm, &mbox_msg, &mbox_msg);
	if (!rc)
		odm->max_qs = 1 << (4 - mbox_msg.d.nvfs);

	return rc;
}

int
odm_dev_fini(struct odm_dev *odm)
{
	union odm_mbox_msg mbox_msg;
	int qno, rc = 0;

	mbox_msg.u[0] = 0;
	mbox_msg.u[1] = 0;
	mbox_msg.q.vfid = odm->vfid;
	mbox_msg.q.cmd = ODM_DEV_CLOSE;
	rc = send_mbox_to_pf(odm, &mbox_msg, &mbox_msg);

	for (qno = 0; qno < odm->num_qs; qno++)
		odm_vchan_resc_free(odm, qno);

	return rc;
}
