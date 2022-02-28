/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#include "otx_zip.h"

uint64_t
zip_reg_read64(uint8_t *hw_addr, uint64_t offset)
{
	uint8_t *base = hw_addr;
	return *(volatile uint64_t *)(base + offset);
}

void
zip_reg_write64(uint8_t *hw_addr, uint64_t offset, uint64_t val)
{
	uint8_t *base = hw_addr;
	*(uint64_t *)(base + offset) = val;
}

static void
zip_q_enable(struct zipvf_qp *qp)
{
	zip_vqx_ena_t que_ena;

	/*ZIP VFx command queue init*/
	que_ena.u = 0ull;
	que_ena.s.ena = 1;

	zip_reg_write64(qp->vf->vbar0, ZIP_VQ_ENA, que_ena.u);
	rte_wmb();
}

/* initialize given qp on zip device */
int
zipvf_q_init(struct zipvf_qp *qp)
{
	zip_vqx_sbuf_addr_t que_sbuf_addr;

	uint64_t size;
	void *cmdq_addr;
	uint64_t iova;
	struct zipvf_cmdq *cmdq = &qp->cmdq;
	struct zip_vf *vf = qp->vf;

	/* allocate and setup instruction queue */
	size = ZIP_MAX_CMDQ_SIZE;
	size = ZIP_ALIGN_ROUNDUP(size, ZIP_CMDQ_ALIGN);

	cmdq_addr = rte_zmalloc(qp->name, size, ZIP_CMDQ_ALIGN);
	if (cmdq_addr == NULL)
		return -1;

	cmdq->sw_head = (uint64_t *)cmdq_addr;
	cmdq->va = (uint8_t *)cmdq_addr;
	iova = rte_mem_virt2iova(cmdq_addr);

	cmdq->iova = iova;

	que_sbuf_addr.u = 0ull;
	if (vf->pdev->id.device_id == PCI_DEVICE_ID_OCTEONTX2_ZIPVF)
		que_sbuf_addr.s9x.ptr = (cmdq->iova >> 7);
	else
		que_sbuf_addr.s.ptr = (cmdq->iova >> 7);

	zip_reg_write64(vf->vbar0, ZIP_VQ_SBUF_ADDR, que_sbuf_addr.u);

	zip_q_enable(qp);

	memset(cmdq->va, 0, ZIP_MAX_CMDQ_SIZE);
	rte_spinlock_init(&cmdq->qlock);

	return 0;
}

int
zipvf_q_term(struct zipvf_qp *qp)
{
	struct zipvf_cmdq *cmdq = &qp->cmdq;
	zip_vqx_ena_t que_ena;
	struct zip_vf *vf = qp->vf;

	if (cmdq->va != NULL) {
		memset(cmdq->va, 0, ZIP_MAX_CMDQ_SIZE);
		rte_free(cmdq->va);
	}

	/*Disabling the ZIP queue*/
	que_ena.u = 0ull;
	zip_reg_write64(vf->vbar0, ZIP_VQ_ENA, que_ena.u);

	return 0;
}

void
zipvf_push_command(struct zipvf_qp *qp, union zip_inst_s *cmd)
{
	zip_quex_doorbell_t dbell;
	union zip_nptr_s ncp;
	uint64_t *ncb_ptr;
	struct zipvf_cmdq *cmdq = &qp->cmdq;
	void *reg_base = qp->vf->vbar0;

	/*Held queue lock*/
	rte_spinlock_lock(&(cmdq->qlock));

	/* Check space availability in zip cmd queue */
	if ((((cmdq->sw_head - (uint64_t *)cmdq->va) * sizeof(uint64_t *)) +
		ZIP_CMD_SIZE) == (ZIP_MAX_CMDQ_SIZE - ZIP_MAX_NCBP_SIZE)) {
		/*Last buffer of the command queue*/
		memcpy((uint8_t *)cmdq->sw_head,
			(uint8_t *)cmd,
			sizeof(union zip_inst_s));
		/* move pointer to next loc in unit of 64-bit word */
		cmdq->sw_head += ZIP_CMD_SIZE_WORDS;

		/* now, point the "Next-Chunk Buffer Ptr" to sw_head */
		ncb_ptr = cmdq->sw_head;
		/* Pointing head again to cmdqueue base*/
		cmdq->sw_head = (uint64_t *)cmdq->va;

		ncp.u = 0ull;
		ncp.s.addr = cmdq->iova;
		*ncb_ptr = ncp.u;
	} else {
		/*Enough buffers available in the command queue*/
		memcpy((uint8_t *)cmdq->sw_head,
			(uint8_t *)cmd,
			sizeof(union zip_inst_s));
		cmdq->sw_head += ZIP_CMD_SIZE_WORDS;
	}

	rte_wmb();

	/* Ringing ZIP VF doorbell */
	dbell.u = 0ull;
	dbell.s.dbell_cnt = 1;
	zip_reg_write64(reg_base, ZIP_VQ_DOORBELL, dbell.u);

	rte_spinlock_unlock(&(cmdq->qlock));
}

int
zipvf_create(struct rte_compressdev *compressdev)
{
	struct   rte_pci_device *pdev = RTE_DEV_TO_PCI(compressdev->device);
	struct   zip_vf *zipvf = NULL;
	char     *dev_name = compressdev->data->name;
	void     *vbar0;
	uint64_t reg;

	if (pdev->mem_resource[0].phys_addr == 0ULL)
		return -EIO;

	vbar0 = pdev->mem_resource[0].addr;
	if (!vbar0) {
		ZIP_PMD_ERR("Failed to map BAR0 of %s", dev_name);
		return -ENODEV;
	}

	zipvf = (struct zip_vf *)(compressdev->data->dev_private);

	if (!zipvf)
		return -ENOMEM;

	zipvf->vbar0 = vbar0;
	reg = zip_reg_read64(zipvf->vbar0, ZIP_VF_PF_MBOXX(0));
	/* Storing domain in local to ZIP VF */
	zipvf->dom_sdom = reg;
	zipvf->pdev = pdev;
	zipvf->max_nb_queue_pairs = ZIP_MAX_VF_QUEUE;
	return 0;
}

int
zipvf_destroy(struct rte_compressdev *compressdev)
{
	struct zip_vf *vf = (struct zip_vf *)(compressdev->data->dev_private);

	/* Rewriting the domain_id in ZIP_VF_MBOX for app rerun */
	zip_reg_write64(vf->vbar0, ZIP_VF_PF_MBOXX(0), vf->dom_sdom);

	return 0;
}
