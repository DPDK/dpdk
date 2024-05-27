/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <stdint.h>

#include <bus_pci_driver.h>

#include <rte_io.h>
#include <rte_malloc.h>

#include "odm.h"
#include "odm_priv.h"

static void
odm_vchan_resc_free(struct odm_dev *odm, int qno)
{
	struct odm_queue *vq = &odm->vq[qno];

	rte_memzone_free(vq->iring_mz);
	rte_memzone_free(vq->cring_mz);
	rte_free(vq->extra_ins_sz);

	vq->iring_mz = NULL;
	vq->cring_mz = NULL;
	vq->extra_ins_sz = NULL;
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

static int
odm_queue_ring_config(struct odm_dev *odm, int vchan, int isize, int csize)
{
	union odm_vdma_ring_cfg_s ring_cfg = {0};
	struct odm_queue *vq = &odm->vq[vchan];

	if (vq->iring_mz == NULL || vq->cring_mz == NULL)
		return -EINVAL;

	ring_cfg.s.isize = (isize / 1024) - 1;
	ring_cfg.s.csize = (csize / 1024) - 1;

	odm_write64(ring_cfg.u, odm->rbase + ODM_VDMA_RING_CFG(vchan));
	odm_write64(vq->iring_mz->iova, odm->rbase + ODM_VDMA_IRING_BADDR(vchan));
	odm_write64(vq->cring_mz->iova, odm->rbase + ODM_VDMA_CRING_BADDR(vchan));

	return 0;
}

int
odm_enable(struct odm_dev *odm)
{
	struct odm_queue *vq;
	int qno, rc = 0;

	for (qno = 0; qno < odm->num_qs; qno++) {
		vq = &odm->vq[qno];

		vq->desc_idx = vq->stats.completed_offset;
		vq->pending_submit_len = 0;
		vq->pending_submit_cnt = 0;
		vq->iring_head = 0;
		vq->cring_head = 0;
		vq->ins_ring_head = 0;
		vq->iring_sz_available = vq->iring_max_words;

		rc = odm_queue_ring_config(odm, qno, vq->iring_max_words * 8,
					   vq->cring_max_entry * 4);
		if (rc < 0)
			break;

		odm_write64(0x1, odm->rbase + ODM_VDMA_EN(qno));
	}

	return rc;
}

int
odm_disable(struct odm_dev *odm)
{
	int qno, wait_cnt = ODM_IRING_IDLE_WAIT_CNT;
	uint64_t val;

	/* Disable the queue and wait for the queue to became idle */
	for (qno = 0; qno < odm->num_qs; qno++) {
		odm_write64(0x0, odm->rbase + ODM_VDMA_EN(qno));
		do {
			val = odm_read64(odm->rbase + ODM_VDMA_IRING_BADDR(qno));
		} while ((!(val & 1ULL << 63)) && (--wait_cnt > 0));
	}

	return 0;
}

int
odm_vchan_setup(struct odm_dev *odm, int vchan, int nb_desc)
{
	struct odm_queue *vq = &odm->vq[vchan];
	int isize, csize, max_nb_desc, rc = 0;
	union odm_mbox_msg mbox_msg;
	const struct rte_memzone *mz;
	char name[32];

	if (vq->iring_mz != NULL)
		odm_vchan_resc_free(odm, vchan);

	mbox_msg.u[0] = 0;
	mbox_msg.u[1] = 0;

	/* ODM PF driver expects vfid starts from index 0 */
	mbox_msg.q.vfid = odm->vfid;
	mbox_msg.q.cmd = ODM_QUEUE_OPEN;
	mbox_msg.q.qidx = vchan;
	rc = send_mbox_to_pf(odm, &mbox_msg, &mbox_msg);
	if (rc < 0)
		return rc;

	/* Determine instruction & completion ring sizes. */

	/* Create iring that can support nb_desc. Round up to a multiple of 1024. */
	isize = RTE_ALIGN_CEIL(nb_desc * ODM_IRING_ENTRY_SIZE_MAX * 8, 1024);
	isize = RTE_MIN(isize, ODM_IRING_MAX_SIZE);
	snprintf(name, sizeof(name), "vq%d_iring%d", odm->vfid, vchan);
	mz = rte_memzone_reserve_aligned(name, isize, SOCKET_ID_ANY, 0, 1024);
	if (mz == NULL)
		return -ENOMEM;
	vq->iring_mz = mz;
	vq->iring_max_words = isize / 8;

	/* Create cring that can support max instructions that can be inflight in hw. */
	max_nb_desc = (isize / (ODM_IRING_ENTRY_SIZE_MIN * 8));
	csize = RTE_ALIGN_CEIL(max_nb_desc * sizeof(union odm_cmpl_ent_s), 1024);
	snprintf(name, sizeof(name), "vq%d_cring%d", odm->vfid, vchan);
	mz = rte_memzone_reserve_aligned(name, csize, SOCKET_ID_ANY, 0, 1024);
	if (mz == NULL) {
		rc = -ENOMEM;
		goto iring_free;
	}
	vq->cring_mz = mz;
	vq->cring_max_entry = csize / 4;

	/* Allocate memory to track the size of each instruction. */
	snprintf(name, sizeof(name), "vq%d_extra%d", odm->vfid, vchan);
	vq->extra_ins_sz = rte_zmalloc(name, vq->cring_max_entry, 0);
	if (vq->extra_ins_sz == NULL) {
		rc = -ENOMEM;
		goto cring_free;
	}

	vq->stats = (struct vq_stats){0};
	return rc;

cring_free:
	rte_memzone_free(odm->vq[vchan].cring_mz);
	vq->cring_mz = NULL;
iring_free:
	rte_memzone_free(odm->vq[vchan].iring_mz);
	vq->iring_mz = NULL;

	return rc;
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
