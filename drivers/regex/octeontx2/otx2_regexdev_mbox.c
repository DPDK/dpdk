/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#include "otx2_common.h"
#include "otx2_dev.h"
#include "otx2_regexdev_mbox.h"
#include "otx2_regexdev.h"

int
otx2_ree_available_queues_get(const struct rte_regexdev *dev,
			      uint16_t *nb_queues)
{
	struct otx2_ree_data *data = dev->data->dev_private;
	struct otx2_ree_vf *vf = &data->vf;
	struct free_rsrcs_rsp *rsp;
	struct otx2_dev *otx2_dev;
	int ret;

	otx2_dev = &vf->otx2_dev;
	otx2_mbox_alloc_msg_free_rsrc_cnt(otx2_dev->mbox);

	ret = otx2_mbox_process_msg(otx2_dev->mbox, (void *)&rsp);
	if (ret)
		return -EIO;

	if (vf->block_address == RVU_BLOCK_ADDR_REE0)
		*nb_queues = rsp->ree0;
	else
		*nb_queues = rsp->ree1;
	return 0;
}

int
otx2_ree_queues_attach(const struct rte_regexdev *dev, uint8_t nb_queues)
{
	struct otx2_ree_data *data = dev->data->dev_private;
	struct otx2_ree_vf *vf = &data->vf;
	struct rsrc_attach_req *req;
	struct otx2_mbox *mbox;

	/* Ask AF to attach required LFs */
	mbox = vf->otx2_dev.mbox;
	req = otx2_mbox_alloc_msg_attach_resources(mbox);

	/* 1 LF = 1 queue */
	req->reelfs = nb_queues;
	req->ree_blkaddr = vf->block_address;

	if (otx2_mbox_process(mbox) < 0)
		return -EIO;

	/* Update number of attached queues */
	vf->nb_queues = nb_queues;

	return 0;
}

int
otx2_ree_queues_detach(const struct rte_regexdev *dev)
{
	struct otx2_ree_data *data = dev->data->dev_private;
	struct otx2_ree_vf *vf = &data->vf;
	struct rsrc_detach_req *req;
	struct otx2_mbox *mbox;

	mbox = vf->otx2_dev.mbox;
	req = otx2_mbox_alloc_msg_detach_resources(mbox);
	req->reelfs = true;
	req->partial = true;
	if (otx2_mbox_process(mbox) < 0)
		return -EIO;

	/* Queues have been detached */
	vf->nb_queues = 0;

	return 0;
}

int
otx2_ree_msix_offsets_get(const struct rte_regexdev *dev)
{
	struct otx2_ree_data *data = dev->data->dev_private;
	struct otx2_ree_vf *vf = &data->vf;
	struct msix_offset_rsp *rsp;
	struct otx2_mbox *mbox;
	uint32_t i, ret;

	/* Get REE MSI-X vector offsets */
	mbox = vf->otx2_dev.mbox;
	otx2_mbox_alloc_msg_msix_offset(mbox);

	ret = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (ret)
		return ret;

	for (i = 0; i < vf->nb_queues; i++) {
		if (vf->block_address == RVU_BLOCK_ADDR_REE0)
			vf->lf_msixoff[i] = rsp->ree0_lf_msixoff[i];
		else
			vf->lf_msixoff[i] = rsp->ree1_lf_msixoff[i];
		otx2_ree_dbg("lf_msixoff[%d]  0x%x", i, vf->lf_msixoff[i]);
	}

	return 0;
}

static int
ree_send_mbox_msg(struct otx2_ree_vf *vf)
{
	struct otx2_mbox *mbox = vf->otx2_dev.mbox;
	int ret;

	otx2_mbox_msg_send(mbox, 0);

	ret = otx2_mbox_wait_for_rsp(mbox, 0);
	if (ret < 0) {
		otx2_err("Could not get mailbox response");
		return ret;
	}

	return 0;
}

int
otx2_ree_config_lf(const struct rte_regexdev *dev, uint8_t lf, uint8_t pri,
		   uint32_t size)
{
	struct otx2_ree_data *data = dev->data->dev_private;
	struct otx2_ree_vf *vf = &data->vf;
	struct ree_lf_req_msg *req;
	struct otx2_mbox *mbox;
	int ret;

	mbox = vf->otx2_dev.mbox;
	req = otx2_mbox_alloc_msg_ree_config_lf(mbox);

	req->lf = lf;
	req->pri =  pri ? 1 : 0;
	req->size = size;
	req->blkaddr = vf->block_address;

	ret = otx2_mbox_process(mbox);
	if (ret < 0) {
		otx2_err("Could not get mailbox response");
		return ret;
	}
	return 0;
}

int
otx2_ree_af_reg_read(const struct rte_regexdev *dev, uint64_t reg,
		     uint64_t *val)
{
	struct otx2_ree_data *data = dev->data->dev_private;
	struct otx2_ree_vf *vf = &data->vf;
	struct ree_rd_wr_reg_msg *msg;
	struct otx2_mbox_dev *mdev;
	struct otx2_mbox *mbox;
	int ret, off;

	mbox = vf->otx2_dev.mbox;
	mdev = &mbox->dev[0];
	msg = (struct ree_rd_wr_reg_msg *)otx2_mbox_alloc_msg_rsp(mbox, 0,
						sizeof(*msg), sizeof(*msg));
	if (msg == NULL) {
		otx2_err("Could not allocate mailbox message");
		return -EFAULT;
	}

	msg->hdr.id = MBOX_MSG_REE_RD_WR_REGISTER;
	msg->hdr.sig = OTX2_MBOX_REQ_SIG;
	msg->hdr.pcifunc = vf->otx2_dev.pf_func;
	msg->is_write = 0;
	msg->reg_offset = reg;
	msg->ret_val = val;
	msg->blkaddr = vf->block_address;

	ret = ree_send_mbox_msg(vf);
	if (ret < 0)
		return ret;

	off = mbox->rx_start +
			RTE_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
	msg = (struct ree_rd_wr_reg_msg *) ((uintptr_t)mdev->mbase + off);

	*val = msg->val;

	return 0;
}

int
otx2_ree_af_reg_write(const struct rte_regexdev *dev, uint64_t reg,
		      uint64_t val)
{
	struct otx2_ree_data *data = dev->data->dev_private;
	struct otx2_ree_vf *vf = &data->vf;
	struct ree_rd_wr_reg_msg *msg;
	struct otx2_mbox *mbox;

	mbox = vf->otx2_dev.mbox;
	msg = (struct ree_rd_wr_reg_msg *)otx2_mbox_alloc_msg_rsp(mbox, 0,
						sizeof(*msg), sizeof(*msg));
	if (msg == NULL) {
		otx2_err("Could not allocate mailbox message");
		return -EFAULT;
	}

	msg->hdr.id = MBOX_MSG_REE_RD_WR_REGISTER;
	msg->hdr.sig = OTX2_MBOX_REQ_SIG;
	msg->hdr.pcifunc = vf->otx2_dev.pf_func;
	msg->is_write = 1;
	msg->reg_offset = reg;
	msg->val = val;
	msg->blkaddr = vf->block_address;

	return ree_send_mbox_msg(vf);
}

int
otx2_ree_rule_db_get(const struct rte_regexdev *dev, char *rule_db,
		uint32_t rule_db_len, char *rule_dbi, uint32_t rule_dbi_len)
{
	struct otx2_ree_data *data = dev->data->dev_private;
	struct ree_rule_db_get_req_msg *req;
	struct ree_rule_db_get_rsp_msg *rsp;
	char *rule_db_ptr = (char *)rule_db;
	struct otx2_ree_vf *vf = &data->vf;
	struct otx2_mbox *mbox;
	int ret, last = 0;
	uint32_t len = 0;

	mbox = vf->otx2_dev.mbox;
	if (!rule_db) {
		otx2_err("Couldn't return rule db due to NULL pointer");
		return -EFAULT;
	}

	while (!last) {
		req = (struct ree_rule_db_get_req_msg *)
			otx2_mbox_alloc_msg_rsp(mbox, 0, sizeof(*req),
						sizeof(*rsp));
		if (!req) {
			otx2_err("Could not allocate mailbox message");
			return -EFAULT;
		}

		req->hdr.id = MBOX_MSG_REE_RULE_DB_GET;
		req->hdr.sig = OTX2_MBOX_REQ_SIG;
		req->hdr.pcifunc = vf->otx2_dev.pf_func;
		req->blkaddr = vf->block_address;
		req->is_dbi = 0;
		req->offset = len;
		ret = otx2_mbox_process_msg(mbox, (void *)&rsp);
		if (ret)
			return ret;
		if (rule_db_len < len + rsp->len) {
			otx2_err("Rule db size is too small");
			return -EFAULT;
		}
		otx2_mbox_memcpy(rule_db_ptr, rsp->rule_db, rsp->len);
		len += rsp->len;
		rule_db_ptr = rule_db_ptr + rsp->len;
		last = rsp->is_last;
	}

	if (rule_dbi) {
		req = (struct ree_rule_db_get_req_msg *)
			otx2_mbox_alloc_msg_rsp(mbox, 0, sizeof(*req),
						sizeof(*rsp));
		if (!req) {
			otx2_err("Could not allocate mailbox message");
			return -EFAULT;
		}

		req->hdr.id = MBOX_MSG_REE_RULE_DB_GET;
		req->hdr.sig = OTX2_MBOX_REQ_SIG;
		req->hdr.pcifunc = vf->otx2_dev.pf_func;
		req->blkaddr = vf->block_address;
		req->is_dbi = 1;
		req->offset = 0;

		ret = otx2_mbox_process_msg(mbox, (void *)&rsp);
		if (ret)
			return ret;
		if (rule_dbi_len < rsp->len) {
			otx2_err("Rule dbi size is too small");
			return -EFAULT;
		}
		otx2_mbox_memcpy(rule_dbi, rsp->rule_db, rsp->len);
	}
	return 0;
}

int
otx2_ree_rule_db_len_get(const struct rte_regexdev *dev,
		uint32_t *rule_db_len,
		uint32_t *rule_dbi_len)
{
	struct otx2_ree_data *data = dev->data->dev_private;
	struct ree_rule_db_len_rsp_msg *rsp;
	struct otx2_ree_vf *vf = &data->vf;
	struct ree_req_msg *req;
	struct otx2_mbox *mbox;
	int ret;

	mbox = vf->otx2_dev.mbox;
	req = (struct ree_req_msg *)
		otx2_mbox_alloc_msg_rsp(mbox, 0, sizeof(*req), sizeof(*rsp));
	if (!req) {
		otx2_err("Could not allocate mailbox message");
		return -EFAULT;
	}

	req->hdr.id = MBOX_MSG_REE_RULE_DB_LEN_GET;
	req->hdr.sig = OTX2_MBOX_REQ_SIG;
	req->hdr.pcifunc = vf->otx2_dev.pf_func;
	req->blkaddr = vf->block_address;
	ret = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (ret)
		return ret;
	if (rule_db_len != NULL)
		*rule_db_len = rsp->len;
	if (rule_dbi_len != NULL)
		*rule_dbi_len = rsp->inc_len;

	return 0;
}

static int
ree_db_msg(const struct rte_regexdev *dev, const char *db, uint32_t db_len,
		int inc, int dbi)
{
	struct otx2_ree_data *data = dev->data->dev_private;
	uint32_t len_left = db_len, offset = 0;
	struct ree_rule_db_prog_req_msg *req;
	struct otx2_ree_vf *vf = &data->vf;
	const char *rule_db_ptr = db;
	struct otx2_mbox *mbox;
	struct msg_rsp *rsp;
	int ret;

	mbox = vf->otx2_dev.mbox;
	while (len_left) {
		req = (struct ree_rule_db_prog_req_msg *)
			otx2_mbox_alloc_msg_rsp(mbox, 0, sizeof(*req),
						sizeof(*rsp));
		if (!req) {
			otx2_err("Could not allocate mailbox message");
			return -EFAULT;
		}
		req->hdr.id = MBOX_MSG_REE_RULE_DB_PROG;
		req->hdr.sig = OTX2_MBOX_REQ_SIG;
		req->hdr.pcifunc = vf->otx2_dev.pf_func;
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
		otx2_mbox_memcpy(req->rule_db, rule_db_ptr, req->len);
		ret = otx2_mbox_process_msg(mbox, (void *)&rsp);
		if (ret) {
			otx2_err("Programming mailbox processing failed");
			return ret;
		}
		len_left -= req->len;
		offset += req->len;
		rule_db_ptr = rule_db_ptr + req->len;
	}
	return 0;
}

int
otx2_ree_rule_db_prog(const struct rte_regexdev *dev, const char *rule_db,
		uint32_t rule_db_len, const char *rule_dbi,
		uint32_t rule_dbi_len)
{
	int inc, ret;

	if (rule_db_len == 0) {
		otx2_err("Couldn't program empty rule db");
		return -EFAULT;
	}
	inc = (rule_dbi_len != 0);
	if ((rule_db == NULL) || (inc && (rule_dbi == NULL))) {
		otx2_err("Couldn't program NULL rule db");
		return -EFAULT;
	}
	if (inc) {
		ret = ree_db_msg(dev, rule_dbi, rule_dbi_len, inc, 1);
		if (ret)
			return ret;
	}
	return ree_db_msg(dev, rule_db, rule_db_len, inc, 0);
}
