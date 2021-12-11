/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#define REE0_PF 19
#define REE1_PF 20

static int
roc_ree_available_queues_get(struct roc_ree_vf *vf, uint16_t *nb_queues)
{
	struct free_rsrcs_rsp *rsp;
	struct dev *dev = vf->dev;
	int ret;

	mbox_alloc_msg_free_rsrc_cnt(dev->mbox);

	ret = mbox_process_msg(dev->mbox, (void *)&rsp);
	if (ret)
		return -EIO;

	if (vf->block_address == RVU_BLOCK_ADDR_REE0)
		*nb_queues = rsp->ree0;
	else
		*nb_queues = rsp->ree1;
	return 0;
}

static int
roc_ree_max_matches_get(struct roc_ree_vf *vf, uint8_t *max_matches)
{
	uint64_t val;
	int ret;

	ret = roc_ree_af_reg_read(vf, REE_AF_REEXM_MAX_MATCH, &val);
	if (ret)
		return ret;

	*max_matches = val;
	return 0;
}

int
roc_ree_queues_attach(struct roc_ree_vf *vf, uint8_t nb_queues)
{
	struct rsrc_attach_req *req;
	struct mbox *mbox;

	mbox = vf->dev->mbox;
	/* Ask AF to attach required LFs */
	req = mbox_alloc_msg_attach_resources(mbox);
	if (req == NULL) {
		plt_err("Could not allocate mailbox message");
		return -EFAULT;
	}

	/* 1 LF = 1 queue */
	req->reelfs = nb_queues;
	req->ree_blkaddr = vf->block_address;

	if (mbox_process(mbox) < 0)
		return -EIO;

	/* Update number of attached queues */
	vf->nb_queues = nb_queues;

	return 0;
}

int
roc_ree_queues_detach(struct roc_ree_vf *vf)
{
	struct rsrc_detach_req *req;
	struct mbox *mbox;

	mbox = vf->dev->mbox;
	req = mbox_alloc_msg_detach_resources(mbox);
	if (req == NULL) {
		plt_err("Could not allocate mailbox message");
		return -EFAULT;
	}
	req->reelfs = true;
	req->partial = true;
	if (mbox_process(mbox) < 0)
		return -EIO;

	/* Queues have been detached */
	vf->nb_queues = 0;

	return 0;
}

int
roc_ree_msix_offsets_get(struct roc_ree_vf *vf)
{
	struct msix_offset_rsp *rsp;
	struct mbox *mbox;
	uint32_t i, ret;

	/* Get REE MSI-X vector offsets */
	mbox = vf->dev->mbox;
	mbox_alloc_msg_msix_offset(mbox);

	ret = mbox_process_msg(mbox, (void *)&rsp);
	if (ret)
		return ret;

	for (i = 0; i < vf->nb_queues; i++) {
		if (vf->block_address == RVU_BLOCK_ADDR_REE0)
			vf->lf_msixoff[i] = rsp->ree0_lf_msixoff[i];
		else
			vf->lf_msixoff[i] = rsp->ree1_lf_msixoff[i];
		plt_ree_dbg("lf_msixoff[%d]  0x%x", i, vf->lf_msixoff[i]);
	}

	return 0;
}

static int
ree_send_mbox_msg(struct roc_ree_vf *vf)
{
	struct mbox *mbox = vf->dev->mbox;
	int ret;

	mbox_msg_send(mbox, 0);

	ret = mbox_wait_for_rsp(mbox, 0);
	if (ret < 0) {
		plt_err("Could not get mailbox response");
		return ret;
	}

	return 0;
}

int
roc_ree_config_lf(struct roc_ree_vf *vf, uint8_t lf, uint8_t pri, uint32_t size)
{
	struct ree_lf_req_msg *req;
	struct mbox *mbox;
	int ret;

	mbox = vf->dev->mbox;
	req = mbox_alloc_msg_ree_config_lf(mbox);
	if (req == NULL) {
		plt_err("Could not allocate mailbox message");
		return -EFAULT;
	}

	req->lf = lf;
	req->pri = pri ? 1 : 0;
	req->size = size;
	req->blkaddr = vf->block_address;

	ret = mbox_process(mbox);
	if (ret < 0) {
		plt_err("Could not get mailbox response");
		return ret;
	}
	return 0;
}

int
roc_ree_af_reg_read(struct roc_ree_vf *vf, uint64_t reg, uint64_t *val)
{
	struct ree_rd_wr_reg_msg *msg;
	struct mbox_dev *mdev;
	struct mbox *mbox;
	int ret, off;

	mbox = vf->dev->mbox;
	mdev = &mbox->dev[0];
	msg = (struct ree_rd_wr_reg_msg *)mbox_alloc_msg_rsp(
		mbox, 0, sizeof(*msg), sizeof(*msg));
	if (msg == NULL) {
		plt_err("Could not allocate mailbox message");
		return -EFAULT;
	}

	msg->hdr.id = MBOX_MSG_REE_RD_WR_REGISTER;
	msg->hdr.sig = MBOX_REQ_SIG;
	msg->hdr.pcifunc = vf->dev->pf_func;
	msg->is_write = 0;
	msg->reg_offset = reg;
	msg->ret_val = val;
	msg->blkaddr = vf->block_address;

	ret = ree_send_mbox_msg(vf);
	if (ret < 0)
		return ret;

	off = mbox->rx_start +
	      RTE_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
	msg = (struct ree_rd_wr_reg_msg *)((uintptr_t)mdev->mbase + off);

	*val = msg->val;

	return 0;
}

int
roc_ree_af_reg_write(struct roc_ree_vf *vf, uint64_t reg, uint64_t val)
{
	struct ree_rd_wr_reg_msg *msg;
	struct mbox *mbox;

	mbox = vf->dev->mbox;
	msg = (struct ree_rd_wr_reg_msg *)mbox_alloc_msg_rsp(
		mbox, 0, sizeof(*msg), sizeof(*msg));
	if (msg == NULL) {
		plt_err("Could not allocate mailbox message");
		return -EFAULT;
	}

	msg->hdr.id = MBOX_MSG_REE_RD_WR_REGISTER;
	msg->hdr.sig = MBOX_REQ_SIG;
	msg->hdr.pcifunc = vf->dev->pf_func;
	msg->is_write = 1;
	msg->reg_offset = reg;
	msg->val = val;
	msg->blkaddr = vf->block_address;

	return ree_send_mbox_msg(vf);
}

int
roc_ree_rule_db_get(struct roc_ree_vf *vf, char *rule_db, uint32_t rule_db_len,
		    char *rule_dbi, uint32_t rule_dbi_len)
{
	struct ree_rule_db_get_req_msg *req;
	struct ree_rule_db_get_rsp_msg *rsp;
	char *rule_db_ptr = (char *)rule_db;
	struct mbox *mbox;
	int ret, last = 0;
	uint32_t len = 0;

	mbox = vf->dev->mbox;
	if (!rule_db) {
		plt_err("Couldn't return rule db due to NULL pointer");
		return -EFAULT;
	}

	while (!last) {
		req = (struct ree_rule_db_get_req_msg *)mbox_alloc_msg_rsp(
			mbox, 0, sizeof(*req), sizeof(*rsp));
		if (!req) {
			plt_err("Could not allocate mailbox message");
			return -EFAULT;
		}

		req->hdr.id = MBOX_MSG_REE_RULE_DB_GET;
		req->hdr.sig = MBOX_REQ_SIG;
		req->hdr.pcifunc = vf->dev->pf_func;
		req->blkaddr = vf->block_address;
		req->is_dbi = 0;
		req->offset = len;
		ret = mbox_process_msg(mbox, (void *)&rsp);
		if (ret)
			return ret;
		if (rule_db_len < len + rsp->len) {
			plt_err("Rule db size is too small");
			return -EFAULT;
		}
		mbox_memcpy(rule_db_ptr, rsp->rule_db, rsp->len);
		len += rsp->len;
		rule_db_ptr = rule_db_ptr + rsp->len;
		last = rsp->is_last;
	}

	if (rule_dbi) {
		req = (struct ree_rule_db_get_req_msg *)mbox_alloc_msg_rsp(
			mbox, 0, sizeof(*req), sizeof(*rsp));
		if (!req) {
			plt_err("Could not allocate mailbox message");
			return -EFAULT;
		}

		req->hdr.id = MBOX_MSG_REE_RULE_DB_GET;
		req->hdr.sig = MBOX_REQ_SIG;
		req->hdr.pcifunc = vf->dev->pf_func;
		req->blkaddr = vf->block_address;
		req->is_dbi = 1;
		req->offset = 0;

		ret = mbox_process_msg(mbox, (void *)&rsp);
		if (ret)
			return ret;
		if (rule_dbi_len < rsp->len) {
			plt_err("Rule dbi size is too small");
			return -EFAULT;
		}
		mbox_memcpy(rule_dbi, rsp->rule_db, rsp->len);
	}
	return 0;
}

int
roc_ree_rule_db_len_get(struct roc_ree_vf *vf, uint32_t *rule_db_len,
			uint32_t *rule_dbi_len)
{
	struct ree_rule_db_len_rsp_msg *rsp;
	struct ree_req_msg *req;
	struct mbox *mbox;
	int ret;

	mbox = vf->dev->mbox;
	req = (struct ree_req_msg *)mbox_alloc_msg_rsp(mbox, 0, sizeof(*req),
						       sizeof(*rsp));
	if (!req) {
		plt_err("Could not allocate mailbox message");
		return -EFAULT;
	}

	req->hdr.id = MBOX_MSG_REE_RULE_DB_LEN_GET;
	req->hdr.sig = MBOX_REQ_SIG;
	req->hdr.pcifunc = vf->dev->pf_func;
	req->blkaddr = vf->block_address;
	ret = mbox_process_msg(mbox, (void *)&rsp);
	if (ret)
		return ret;
	if (rule_db_len != NULL)
		*rule_db_len = rsp->len;
	if (rule_dbi_len != NULL)
		*rule_dbi_len = rsp->inc_len;

	return 0;
}

static int
ree_db_msg(struct roc_ree_vf *vf, const char *db, uint32_t db_len, int inc,
	   int dbi)
{
	uint32_t len_left = db_len, offset = 0;
	struct ree_rule_db_prog_req_msg *req;
	const char *rule_db_ptr = db;
	struct mbox *mbox;
	struct msg_rsp *rsp;
	int ret;

	mbox = vf->dev->mbox;
	while (len_left) {
		req = (struct ree_rule_db_prog_req_msg *)mbox_alloc_msg_rsp(
			mbox, 0, sizeof(*req), sizeof(*rsp));
		if (!req) {
			plt_err("Could not allocate mailbox message");
			return -EFAULT;
		}
		req->hdr.id = MBOX_MSG_REE_RULE_DB_PROG;
		req->hdr.sig = MBOX_REQ_SIG;
		req->hdr.pcifunc = vf->dev->pf_func;
		req->offset = offset;
		req->total_len = db_len;
		req->len = REE_RULE_DB_REQ_BLOCK_SIZE;
		req->is_incremental = inc;
		req->is_dbi = dbi;
		req->blkaddr = vf->block_address;

		if (len_left < REE_RULE_DB_REQ_BLOCK_SIZE) {
			req->is_last = true;
			req->len = len_left;
		}
		mbox_memcpy(req->rule_db, rule_db_ptr, req->len);
		ret = mbox_process_msg(mbox, (void *)&rsp);
		if (ret) {
			plt_err("Programming mailbox processing failed");
			return ret;
		}
		len_left -= req->len;
		offset += req->len;
		rule_db_ptr = rule_db_ptr + req->len;
	}
	return 0;
}

int
roc_ree_rule_db_prog(struct roc_ree_vf *vf, const char *rule_db,
		     uint32_t rule_db_len, const char *rule_dbi,
		     uint32_t rule_dbi_len)
{
	int inc, ret;

	if (rule_db_len == 0) {
		plt_err("Couldn't program empty rule db");
		return -EFAULT;
	}
	inc = (rule_dbi_len != 0);
	if ((rule_db == NULL) || (inc && (rule_dbi == NULL))) {
		plt_err("Couldn't program NULL rule db");
		return -EFAULT;
	}
	if (inc) {
		ret = ree_db_msg(vf, rule_dbi, rule_dbi_len, inc, 1);
		if (ret)
			return ret;
	}
	return ree_db_msg(vf, rule_db, rule_db_len, inc, 0);
}

static int
ree_get_blkaddr(struct dev *dev)
{
	int pf;

	pf = dev_get_pf(dev->pf_func);
	if (pf == REE0_PF)
		return RVU_BLOCK_ADDR_REE0;
	else if (pf == REE1_PF)
		return RVU_BLOCK_ADDR_REE1;
	else
		return 0;
}

uintptr_t
roc_ree_qp_get_base(struct roc_ree_vf *vf, uint16_t qp_id)
{
	return REE_LF_BAR2(vf, qp_id);
}

static void
roc_ree_lf_err_intr_handler(void *param)
{
	uintptr_t base = (uintptr_t)param;
	uint8_t lf_id;
	uint64_t intr;

	lf_id = (base >> 12) & 0xFF;

	intr = plt_read64(base + REE_LF_MISC_INT);
	if (intr == 0)
		return;

	plt_ree_dbg("LF %d MISC_INT: 0x%" PRIx64 "", lf_id, intr);

	/* Clear interrupt */
	plt_write64(intr, base + REE_LF_MISC_INT);
}

static void
roc_ree_lf_err_intr_unregister(struct roc_ree_vf *vf, uint16_t msix_off,
			       uintptr_t base)
{
	struct rte_pci_device *pci_dev = vf->pci_dev;

	/* Disable error interrupts */
	plt_write64(~0ull, base + REE_LF_MISC_INT_ENA_W1C);

	dev_irq_unregister(pci_dev->intr_handle,
			   roc_ree_lf_err_intr_handler, (void *)base, msix_off);
}

void
roc_ree_err_intr_unregister(struct roc_ree_vf *vf)
{
	uintptr_t base;
	uint32_t i;

	for (i = 0; i < vf->nb_queues; i++) {
		base = REE_LF_BAR2(vf, i);
		roc_ree_lf_err_intr_unregister(vf, vf->lf_msixoff[i], base);
	}

	vf->err_intr_registered = 0;
}

static int
roc_ree_lf_err_intr_register(struct roc_ree_vf *vf, uint16_t msix_off,
			     uintptr_t base)
{
	struct rte_pci_device *pci_dev = vf->pci_dev;
	int ret;

	/* Disable error interrupts */
	plt_write64(~0ull, base + REE_LF_MISC_INT_ENA_W1C);

	/* Register error interrupt handler */
	ret = dev_irq_register(pci_dev->intr_handle,
			       roc_ree_lf_err_intr_handler, (void *)base,
			       msix_off);
	if (ret)
		return ret;

	/* Enable error interrupts */
	plt_write64(~0ull, base + REE_LF_MISC_INT_ENA_W1S);

	return 0;
}

int
roc_ree_err_intr_register(struct roc_ree_vf *vf)
{
	uint32_t i, j, ret;
	uintptr_t base;

	for (i = 0; i < vf->nb_queues; i++) {
		if (vf->lf_msixoff[i] == MSIX_VECTOR_INVALID) {
			plt_err("Invalid REE LF MSI-X offset: 0x%x",
				vf->lf_msixoff[i]);
			return -EINVAL;
		}
	}

	for (i = 0; i < vf->nb_queues; i++) {
		base = REE_LF_BAR2(vf, i);
		ret = roc_ree_lf_err_intr_register(vf, vf->lf_msixoff[i], base);
		if (ret)
			goto intr_unregister;
	}

	vf->err_intr_registered = 1;
	return 0;

intr_unregister:
	/* Unregister the ones already registered */
	for (j = 0; j < i; j++) {
		base = REE_LF_BAR2(vf, j);
		roc_ree_lf_err_intr_unregister(vf, vf->lf_msixoff[j], base);
	}
	return ret;
}

int
roc_ree_iq_enable(struct roc_ree_vf *vf, const struct roc_ree_qp *qp,
		  uint8_t pri, uint32_t size_div2)
{
	uint64_t val;

	/* Set instruction queue size and priority */
	roc_ree_config_lf(vf, qp->id, pri, size_div2);

	/* Set instruction queue base address */
	/* Should be written after SBUF_CTL and before LF_ENA */

	val = plt_read64(qp->base + REE_LF_SBUF_ADDR);
	val &= ~REE_LF_SBUF_ADDR_PTR_MASK;
	val |= FIELD_PREP(REE_LF_SBUF_ADDR_PTR_MASK, qp->iq_dma_addr >> 7);
	plt_write64(val, qp->base + REE_LF_SBUF_ADDR);

	/* Enable instruction queue */

	val = plt_read64(qp->base + REE_LF_ENA);
	val &= ~REE_LF_ENA_ENA_MASK;
	val |= FIELD_PREP(REE_LF_ENA_ENA_MASK, 1);
	plt_write64(val, qp->base + REE_LF_ENA);

	return 0;
}

void
roc_ree_iq_disable(struct roc_ree_qp *qp)
{
	uint64_t val;

	/* Stop instruction execution */
	val = plt_read64(qp->base + REE_LF_ENA);
	val &= ~REE_LF_ENA_ENA_MASK;
	val |= FIELD_PREP(REE_LF_ENA_ENA_MASK, 0);
	plt_write64(val, qp->base + REE_LF_ENA);
}

int
roc_ree_dev_init(struct roc_ree_vf *vf)
{
	struct plt_pci_device *pci_dev;
	struct ree *ree;
	struct dev *dev;
	uint8_t max_matches = 0;
	uint16_t nb_queues = 0;
	int rc;

	if (vf == NULL || vf->pci_dev == NULL)
		return -EINVAL;

	PLT_STATIC_ASSERT(sizeof(struct ree) <= ROC_REE_MEM_SZ);

	ree = roc_ree_to_ree_priv(vf);
	memset(ree, 0, sizeof(*ree));
	vf->dev = &ree->dev;

	pci_dev = vf->pci_dev;
	dev = vf->dev;

	/* Initialize device  */
	rc = dev_init(dev, pci_dev);
	if (rc) {
		plt_err("Failed to init roc device");
		goto fail;
	}

	/* Get REE block address */
	vf->block_address = ree_get_blkaddr(dev);
	if (!vf->block_address) {
		plt_err("Could not determine block PF number");
		goto fail;
	}

	/* Get number of queues available on the device */
	rc = roc_ree_available_queues_get(vf, &nb_queues);
	if (rc) {
		plt_err("Could not determine the number of queues available");
		goto fail;
	}

	/* Don't exceed the limits set per VF */
	nb_queues = RTE_MIN(nb_queues, REE_MAX_QUEUES_PER_VF);

	if (nb_queues == 0) {
		plt_err("No free queues available on the device");
		goto fail;
	}

	vf->max_queues = nb_queues;

	plt_ree_dbg("Max queues supported by device: %d", vf->max_queues);

	/* Get number of maximum matches supported on the device */
	rc = roc_ree_max_matches_get(vf, &max_matches);
	if (rc) {
		plt_err("Could not determine the maximum matches supported");
		goto fail;
	}
	/* Don't exceed the limits set per VF */
	max_matches = RTE_MIN(max_matches, REE_MAX_MATCHES_PER_VF);
	if (max_matches == 0) {
		plt_err("Could not determine the maximum matches supported");
		goto fail;
	}

	vf->max_matches = max_matches;

	plt_ree_dbg("Max matches supported by device: %d", vf->max_matches);
fail:
	return rc;
}

int
roc_ree_dev_fini(struct roc_ree_vf *vf)
{
	if (vf == NULL)
		return -EINVAL;

	vf->max_matches = 0;
	vf->max_queues = 0;

	return dev_fini(vf->dev, vf->pci_dev);
}
