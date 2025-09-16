/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */
#include <rte_malloc.h>
#include <bus_pci_driver.h>

#include "xsc_vfio_mbox.h"
#include "xsc_log.h"

#define XSC_MBOX_BUF_NUM		2048
#define XSC_MBOX_BUF_CACHE_SIZE		256
#define XSC_CMDQ_DEPTH_LOG		5
#define XSC_CMDQ_ELEMENT_SIZE_LOG	6
#define XSC_CMDQ_REQ_TYPE		7
#define XSC_CMDQ_WAIT_TIMEOUT		10
#define XSC_CMDQ_WAIT_DELAY_MS		100
#define XSC_CMD_OP_DUMMY		0x10d

#define XSC_PF_CMDQ_ELEMENT_SZ		0x1020020
#define XSC_PF_CMDQ_REQ_BASE_H_ADDR	0x1022000
#define XSC_PF_CMDQ_REQ_BASE_L_ADDR	0x1024000
#define XSC_PF_CMDQ_RSP_BASE_H_ADDR	0x102a000
#define XSC_PF_CMDQ_RSP_BASE_L_ADDR	0x102c000
#define XSC_PF_CMDQ_REQ_PID		0x1026000
#define XSC_PF_CMDQ_REQ_CID		0x1028000
#define XSC_PF_CMDQ_RSP_PID		0x102e000
#define XSC_PF_CMDQ_RSP_CID		0x1030000
#define XSC_PF_CMDQ_DEPTH		0x1020028

#define XSC_VF_CMDQ_REQ_BASE_H_ADDR	0x0
#define XSC_VF_CMDQ_REQ_BASE_L_ADDR	0x4
#define XSC_VF_CMDQ_RSP_BASE_H_ADDR	0x10
#define XSC_VF_CMDQ_RSP_BASE_L_ADDR	0x14
#define XSC_VF_CMDQ_REQ_PID		0x8
#define XSC_VF_CMDQ_REQ_CID		0xc
#define XSC_VF_CMDQ_RSP_PID		0x18
#define XSC_VF_CMDQ_RSP_CID		0x1c
#define XSC_VF_CMDQ_ELEMENT_SZ		0x28
#define XSC_VF_CMDQ_DEPTH		0x2c

static const char * const xsc_cmd_error[] = {
	"xsc cmd success",
	"xsc cmd fail",
	"xsc cmd timeout"
};

static struct xsc_cmdq_config xsc_pf_config = {
	.req_pid_addr = XSC_PF_CMDQ_REQ_PID,
	.req_cid_addr = XSC_PF_CMDQ_REQ_CID,
	.rsp_pid_addr = XSC_PF_CMDQ_RSP_PID,
	.rsp_cid_addr = XSC_PF_CMDQ_RSP_CID,
	.req_h_addr = XSC_PF_CMDQ_REQ_BASE_H_ADDR,
	.req_l_addr = XSC_PF_CMDQ_REQ_BASE_L_ADDR,
	.rsp_h_addr = XSC_PF_CMDQ_RSP_BASE_H_ADDR,
	.rsp_l_addr = XSC_PF_CMDQ_RSP_BASE_L_ADDR,
	.elt_sz_addr = XSC_PF_CMDQ_ELEMENT_SZ,
	.depth_addr = XSC_PF_CMDQ_DEPTH,
};

static struct xsc_cmdq_config xsc_vf_config = {
	.req_pid_addr = XSC_VF_CMDQ_REQ_PID,
	.req_cid_addr = XSC_VF_CMDQ_REQ_CID,
	.rsp_pid_addr = XSC_VF_CMDQ_RSP_PID,
	.rsp_cid_addr = XSC_VF_CMDQ_RSP_CID,
	.req_h_addr = XSC_VF_CMDQ_REQ_BASE_H_ADDR,
	.req_l_addr = XSC_VF_CMDQ_REQ_BASE_L_ADDR,
	.rsp_h_addr = XSC_VF_CMDQ_RSP_BASE_H_ADDR,
	.rsp_l_addr = XSC_VF_CMDQ_RSP_BASE_L_ADDR,
	.elt_sz_addr = XSC_VF_CMDQ_ELEMENT_SZ,
	.depth_addr = XSC_VF_CMDQ_DEPTH,
};

static void
xsc_cmdq_config_init(struct xsc_dev *xdev, struct xsc_cmd_queue *cmdq)
{
	if (xsc_dev_is_vf(xdev) || xdev->bar_len != XSC_DEV_BAR_LEN_256M)
		cmdq->config = &xsc_vf_config;
	else
		cmdq->config = &xsc_pf_config;
}

static void
xsc_cmdq_rsp_cid_update(struct xsc_dev *xdev, struct xsc_cmd_queue *cmdq)
{
	uint32_t rsp_pid;

	cmdq->rsp_cid = rte_read32((uint8_t *)xdev->bar_addr + cmdq->config->rsp_cid_addr);
	rsp_pid = rte_read32((uint8_t *)xdev->bar_addr + cmdq->config->rsp_pid_addr);
	if (rsp_pid != cmdq->rsp_cid) {
		PMD_DRV_LOG(INFO, "Update cid(%u) to latest pid(%u)",
			    cmdq->rsp_cid, rsp_pid);
		cmdq->rsp_cid = rsp_pid;
		rte_write32(cmdq->rsp_cid, (uint8_t *)xdev->bar_addr + cmdq->config->rsp_cid_addr);
	}
}

static void
xsc_cmdq_depth_set(struct xsc_dev *xdev, struct xsc_cmd_queue *cmdq)
{
	cmdq->depth_n = XSC_CMDQ_DEPTH_LOG;
	cmdq->depth_m = (1 << XSC_CMDQ_DEPTH_LOG) - 1;
	rte_write32(1 << cmdq->depth_n, (uint8_t *)xdev->bar_addr + cmdq->config->depth_addr);
}

static int
xsc_cmdq_elt_size_check(struct xsc_dev *xdev, struct xsc_cmd_queue *cmdq)
{
	uint32_t elts_n;

	elts_n = rte_read32((uint8_t *)xdev->bar_addr + cmdq->config->elt_sz_addr);
	if (elts_n != XSC_CMDQ_ELEMENT_SIZE_LOG) {
		PMD_DRV_LOG(ERR, "The cmdq elt size log(%u) is error, should be %u",
			    elts_n, XSC_CMDQ_ELEMENT_SIZE_LOG);
		rte_errno = ENODEV;
		return -1;
	}

	return 0;
}

static void
xsc_cmdq_req_base_addr_set(struct xsc_dev *xdev, struct xsc_cmd_queue *cmdq)
{
	uint32_t h_addr, l_addr;

	h_addr = (uint32_t)(cmdq->req_mz->iova >> 32);
	l_addr = (uint32_t)(cmdq->req_mz->iova);
	rte_write32(h_addr, (uint8_t *)xdev->bar_addr + cmdq->config->req_h_addr);
	rte_write32(l_addr, (uint8_t *)xdev->bar_addr + cmdq->config->req_l_addr);
}

static void
xsc_cmdq_rsp_base_addr_set(struct xsc_dev *xdev, struct xsc_cmd_queue *cmdq)
{
	uint32_t h_addr, l_addr;

	h_addr = (uint32_t)(cmdq->rsp_mz->iova >> 32);
	l_addr = (uint32_t)(cmdq->rsp_mz->iova);
	rte_write32(h_addr, (uint8_t *)xdev->bar_addr + cmdq->config->rsp_h_addr);
	rte_write32(l_addr, (uint8_t *)xdev->bar_addr + cmdq->config->rsp_l_addr);
}

static void
xsc_cmdq_mbox_free(struct xsc_dev *xdev, struct xsc_cmdq_mbox *mbox)
{
	struct xsc_cmdq_mbox *next, *head;
	struct xsc_vfio_priv *priv = xdev->dev_priv;

	head = mbox;
	while (head != NULL) {
		next = head->next;
		if (head->buf != NULL)
			rte_mempool_put(priv->cmdq->mbox_buf_pool, head->buf);
		free(head);
		head = next;
	}
}

static struct xsc_cmdq_mbox *
xsc_cmdq_mbox_alloc(struct xsc_dev *xdev)
{
	struct xsc_cmdq_mbox *mbox;
	int ret;
	struct xsc_vfio_priv *priv = (struct xsc_vfio_priv *)xdev->dev_priv;

	mbox = malloc(sizeof(*mbox));
	if (mbox == NULL) {
		rte_errno = -ENOMEM;
		goto error;
	}
	memset(mbox, 0, sizeof(struct xsc_cmdq_mbox));

	ret = rte_mempool_get(priv->cmdq->mbox_buf_pool, (void **)&mbox->buf);
	if (ret != 0)
		goto error;
	mbox->buf_dma = rte_mempool_virt2iova(mbox->buf);
	memset(mbox->buf, 0, sizeof(struct xsc_cmdq_mbox_buf));
	mbox->next = NULL;

	return mbox;

error:
	xsc_cmdq_mbox_free(xdev, mbox);
	return NULL;
}

static struct xsc_cmdq_mbox *
xsc_cmdq_mbox_alloc_bulk(struct xsc_dev *xdev, int n)
{
	int i;
	struct xsc_cmdq_mbox *head = NULL;
	struct xsc_cmdq_mbox *mbox;
	struct xsc_cmdq_mbox_buf *mbox_buf;

	for (i = 0; i < n; i++) {
		mbox = xsc_cmdq_mbox_alloc(xdev);
		if (mbox == NULL) {
			PMD_DRV_LOG(ERR, "Failed to alloc mailbox");
			goto error;
		}

		mbox_buf = mbox->buf;
		mbox->next = head;
		mbox_buf->next = rte_cpu_to_be_64(mbox->next ? mbox->next->buf_dma : 0);
		mbox_buf->block_num = rte_cpu_to_be_32(n - i - 1);
		head = mbox;
	}

	return head;

error:
	xsc_cmdq_mbox_free(xdev, head);
	return NULL;
}

static void
xsc_cmdq_req_msg_free(struct xsc_dev *xdev, struct xsc_cmdq_req_msg *msg)
{
	struct xsc_cmdq_mbox *head;

	if (msg == NULL)
		return;

	head = msg->next;
	xsc_cmdq_mbox_free(xdev, head);
	free(msg);
}

static struct xsc_cmdq_req_msg *
xsc_cmdq_req_msg_alloc(struct xsc_dev *xdev, int len)
{
	struct xsc_cmdq_req_msg *msg;
	struct xsc_cmdq_mbox *head = NULL;
	int cmd_len, nb_mbox;

	msg = malloc(sizeof(*msg));
	if (msg == NULL) {
		rte_errno = -ENOMEM;
		goto error;
	}
	memset(msg, 0, sizeof(*msg));

	cmd_len = len - RTE_MIN(sizeof(msg->hdr.data), (uint32_t)len);
	nb_mbox = (cmd_len + XSC_CMDQ_DATA_SIZE - 1) / XSC_CMDQ_DATA_SIZE;
	head = xsc_cmdq_mbox_alloc_bulk(xdev, nb_mbox);
	if (head == NULL && nb_mbox != 0)
		goto error;

	msg->next = head;
	msg->len = len;

	return msg;

error:
	xsc_cmdq_req_msg_free(xdev, msg);
	return NULL;
}

static void
xsc_cmdq_rsp_msg_free(struct xsc_dev *xdev, struct xsc_cmdq_rsp_msg *msg)
{
	struct xsc_cmdq_mbox *head;

	if (msg == NULL)
		return;

	head = msg->next;
	xsc_cmdq_mbox_free(xdev, head);
	free(msg);
}

static struct xsc_cmdq_rsp_msg *
xsc_cmdq_rsp_msg_alloc(struct xsc_dev *xdev, int len)
{
	struct xsc_cmdq_rsp_msg *msg;
	struct xsc_cmdq_mbox *head = NULL;
	int cmd_len, nb_mbox;

	msg = malloc(sizeof(*msg));
	if (msg == NULL) {
		rte_errno = -ENOMEM;
		goto error;
	}
	memset(msg, 0, sizeof(*msg));

	cmd_len = len - RTE_MIN(sizeof(msg->hdr.data), (uint32_t)len);
	nb_mbox = (cmd_len + XSC_CMDQ_DATA_SIZE - 1) / XSC_CMDQ_DATA_SIZE;
	head = xsc_cmdq_mbox_alloc_bulk(xdev, nb_mbox);
	if (head == NULL && nb_mbox != 0)
		goto error;

	msg->next = head;
	msg->len = len;

	return msg;

error:
	xsc_cmdq_rsp_msg_free(xdev, msg);
	return NULL;
}

static void
xsc_cmdq_msg_destruct(struct xsc_dev *xdev,
		      struct xsc_cmdq_req_msg **req_msg,
		      struct xsc_cmdq_rsp_msg **rsp_msg)
{
	xsc_cmdq_req_msg_free(xdev, *req_msg);
	xsc_cmdq_rsp_msg_free(xdev, *rsp_msg);
	*req_msg = NULL;
	*rsp_msg = NULL;
}

static int
xsc_cmdq_msg_construct(struct xsc_dev *xdev,
		       struct xsc_cmdq_req_msg **req_msg, int in_len,
		       struct xsc_cmdq_rsp_msg **rsp_msg, int out_len)
{
	*req_msg = xsc_cmdq_req_msg_alloc(xdev, in_len);
	if (*req_msg == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc xsc cmd request msg");
		goto error;
	}

	*rsp_msg = xsc_cmdq_rsp_msg_alloc(xdev, out_len);
	if (*rsp_msg == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc xsc cmd response msg");
		goto error;
	}

	return 0;

error:
	xsc_cmdq_msg_destruct(xdev, req_msg, rsp_msg);
	return -1;
}

static int
xsc_cmdq_req_msg_copy(struct xsc_cmdq_req_msg *req_msg, void *data_in, int in_len)
{
	struct xsc_cmdq_mbox_buf *mbox_buf;
	struct xsc_cmdq_mbox *mbox;
	int copy;
	uint8_t *data = data_in;

	if (req_msg == NULL || data == NULL)
		return -1;

	copy = RTE_MIN((uint32_t)in_len, sizeof(req_msg->hdr.data));
	memcpy(req_msg->hdr.data, data, copy);

	in_len -= copy;
	data += copy;

	mbox = req_msg->next;
	while (in_len > 0) {
		if (mbox == NULL)
			return -1;

		copy = RTE_MIN(in_len, XSC_CMDQ_DATA_SIZE);
		mbox_buf = mbox->buf;
		memcpy(mbox_buf->data, data, copy);
		mbox_buf->owner_status = 0;
		data += copy;
		in_len -= copy;
		mbox = mbox->next;
	}

	return 0;
}

static int
xsc_cmdq_rsp_msg_copy(void *data_out, struct xsc_cmdq_rsp_msg *rsp_msg, int out_len)
{
	struct xsc_cmdq_mbox_buf *mbox_buf;
	struct xsc_cmdq_mbox *mbox;
	int copy;
	uint8_t *data = data_out;

	if (data == NULL || rsp_msg == NULL)
		return -1;

	copy = RTE_MIN((uint32_t)out_len, sizeof(rsp_msg->hdr.data));
	memcpy(data, rsp_msg->hdr.data, copy);
	out_len -= copy;
	data += copy;

	mbox = rsp_msg->next;
	while (out_len > 0) {
		if (mbox == NULL)
			return -1;
		copy = RTE_MIN(out_len, XSC_CMDQ_DATA_SIZE);
		mbox_buf = mbox->buf;
		if (!mbox_buf->owner_status)
			PMD_DRV_LOG(ERR, "Failed to check cmd owner");
		memcpy(data, mbox_buf->data, copy);
		data += copy;
		out_len -= copy;
		mbox = mbox->next;
	}

	return 0;
}

static enum xsc_cmd_status
xsc_cmdq_wait_completion(struct xsc_dev *xdev, struct xsc_cmdq_rsp_msg *rsp_msg)
{
	struct xsc_vfio_priv *priv = (struct xsc_vfio_priv *)xdev->dev_priv;
	struct xsc_cmd_queue *cmdq = priv->cmdq;
	volatile struct xsc_cmdq_rsp_layout *rsp_lay;
	struct xsc_cmd_outbox_hdr *out_hdr = (struct xsc_cmd_outbox_hdr *)rsp_msg->hdr.data;
	int count = (XSC_CMDQ_WAIT_TIMEOUT * 1000) / XSC_CMDQ_WAIT_DELAY_MS;
	uint32_t rsp_pid;
	uint8_t cmd_status;
	uint32_t i;

	while (count-- > 0) {
		rsp_pid = rte_read32((uint8_t *)xdev->bar_addr + cmdq->config->rsp_pid_addr);
		if (rsp_pid == cmdq->rsp_cid) {
			rte_delay_ms(XSC_CMDQ_WAIT_DELAY_MS);
			continue;
		}

		rsp_lay = cmdq->rsp_lay + cmdq->rsp_cid;
		if (cmdq->owner_learn == 0) {
			/* First time learning owner_bit from hardware */
			cmdq->owner_bit = rsp_lay->owner_bit;
			cmdq->owner_learn = 1;
		}

		/* Waiting for dma to complete */
		if (cmdq->owner_bit != rsp_lay->owner_bit)
			continue;

		for (i = 0; i < XSC_CMDQ_RSP_INLINE_SIZE; i++)
			rsp_msg->hdr.data[i] = rsp_lay->out[i];

		cmdq->rsp_cid = (cmdq->rsp_cid + 1) & cmdq->depth_m;
		rte_write32(cmdq->rsp_cid, (uint8_t *)xdev->bar_addr + cmdq->config->rsp_cid_addr);

		/* Change owner bit */
		if (cmdq->rsp_cid == 0)
			cmdq->owner_bit = !cmdq->owner_bit;

		cmd_status = out_hdr->status;
		if (cmd_status != 0)
			return XSC_CMD_FAIL;
		return XSC_CMD_SUCC;
	}

	return XSC_CMD_TIMEOUT;
}

static int
xsc_cmdq_dummy_invoke(struct xsc_dev *xdev, struct xsc_cmd_queue *cmdq, uint32_t start, int num)
{
	struct xsc_cmdq_dummy_mbox_in in;
	struct xsc_cmdq_dummy_mbox_out out;
	struct xsc_cmdq_req_msg *req_msg = NULL;
	struct xsc_cmdq_rsp_msg *rsp_msg = NULL;
	struct xsc_cmdq_req_layout *req_lay;
	int in_len = sizeof(in);
	int out_len = sizeof(out);
	int ret, i;
	uint32_t start_pid = start;

	memset(&in, 0, sizeof(in));
	in.hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_DUMMY);

	ret = xsc_cmdq_msg_construct(xdev, &req_msg, in_len, &rsp_msg, out_len);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to construct cmd msg for dummy exec");
		return -1;
	}

	ret = xsc_cmdq_req_msg_copy(req_msg, &in, in_len);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to copy cmd buf to request msg for dummy exec");
		goto error;
	}

	rte_spinlock_lock(&cmdq->lock);

	for (i = 0; i < num; i++) {
		req_lay = cmdq->req_lay + start_pid;
		memset(req_lay, 0, sizeof(*req_lay));
		memcpy(req_lay->in, req_msg->hdr.data, sizeof(req_lay->in));
		req_lay->inlen = rte_cpu_to_be_32(req_msg->len);
		req_lay->outlen = rte_cpu_to_be_32(rsp_msg->len);
		req_lay->sig = 0xff;
		req_lay->idx = 0;
		req_lay->type = XSC_CMDQ_REQ_TYPE;
		start_pid = (start_pid + 1) & cmdq->depth_m;
	}

	/* Ring doorbell after the descriptor is valid */
	rte_write32(cmdq->req_pid, (uint8_t *)xdev->bar_addr + cmdq->config->req_pid_addr);

	ret = xsc_cmdq_wait_completion(xdev, rsp_msg);
	rte_spinlock_unlock(&cmdq->lock);

error:
	xsc_cmdq_msg_destruct(xdev, &req_msg, &rsp_msg);
	return ret;
}

static int
xsc_cmdq_req_status_restore(struct xsc_dev *xdev, struct xsc_cmd_queue *cmdq)
{
	uint32_t req_pid, req_cid;
	uint32_t cnt;

	req_pid = rte_read32((uint8_t *)xdev->bar_addr + cmdq->config->req_pid_addr);
	req_cid = rte_read32((uint8_t *)xdev->bar_addr + cmdq->config->req_cid_addr);

	if (req_pid >= (uint32_t)(1 << cmdq->depth_n) ||
	    req_cid >= (uint32_t)(1 << cmdq->depth_n)) {
		PMD_DRV_LOG(ERR, "Request pid %u and cid %u must be less than %u",
			    req_pid, req_cid, (uint32_t)(1 << cmdq->depth_n));
		return -1;
	}

	cmdq->req_pid = req_pid;
	if (req_pid == req_cid)
		return 0;

	cnt = (req_pid > req_cid) ? (req_pid - req_cid) :
		((1 << cmdq->depth_n) + req_pid - req_cid);
	if (xsc_cmdq_dummy_invoke(xdev, cmdq, req_cid, cnt) != 0) {
		PMD_DRV_LOG(ERR, "Failed to dummy invoke xsc cmd");
		return -1;
	}

	return 0;
}

void
xsc_vfio_mbox_destroy(struct xsc_cmd_queue *cmdq)
{
	if (cmdq == NULL)
		return;

	rte_memzone_free(cmdq->req_mz);
	rte_memzone_free(cmdq->rsp_mz);
	rte_mempool_free(cmdq->mbox_buf_pool);
	rte_free(cmdq);
}

int
xsc_vfio_mbox_init(struct xsc_dev *xdev)
{
	struct xsc_cmd_queue *cmdq;
	struct xsc_vfio_priv *priv = (struct xsc_vfio_priv *)xdev->dev_priv;
	char name[RTE_MEMZONE_NAMESIZE] = { 0 };
	uint32_t size;

	cmdq = rte_zmalloc(NULL, sizeof(*cmdq), RTE_CACHE_LINE_SIZE);
	if (cmdq == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory for xsc_cmd_queue");
		return -1;
	}

	snprintf(name, RTE_MEMZONE_NAMESIZE, "%s_cmdq", xdev->pci_dev->device.name);
	size = (1 << XSC_CMDQ_DEPTH_LOG) * sizeof(struct xsc_cmdq_req_layout);
	cmdq->req_mz = rte_memzone_reserve_aligned(name,
						   size, SOCKET_ID_ANY,
						   RTE_MEMZONE_IOVA_CONTIG,
						   XSC_PAGE_SIZE);
	if (cmdq->req_mz == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory for cmd queue");
		goto error;
	}
	cmdq->req_lay = cmdq->req_mz->addr;

	snprintf(name, RTE_MEMZONE_NAMESIZE, "%s_cmd_cq", xdev->pci_dev->device.name);
	size = (1 << XSC_CMDQ_DEPTH_LOG) * sizeof(struct xsc_cmdq_rsp_layout); /* -V1048 */
	cmdq->rsp_mz = rte_memzone_reserve_aligned(name,
						   size, SOCKET_ID_ANY,
						   RTE_MEMZONE_IOVA_CONTIG,
						   XSC_PAGE_SIZE);
	if (cmdq->rsp_mz == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory for cmd cq");
		goto error;
	}
	cmdq->rsp_lay = cmdq->rsp_mz->addr;

	snprintf(name, RTE_MEMZONE_NAMESIZE, "%s_mempool", xdev->pci_dev->device.name);
	cmdq->mbox_buf_pool = rte_mempool_create(name, XSC_MBOX_BUF_NUM,
						 sizeof(struct xsc_cmdq_mbox_buf),
						 XSC_MBOX_BUF_CACHE_SIZE, 0,
						 NULL, NULL, NULL, NULL,
						 SOCKET_ID_ANY, 0);
	if (cmdq->mbox_buf_pool == NULL) {
		PMD_DRV_LOG(ERR, "Failed to create mailbox buf pool");
		goto error;
	}

	xsc_cmdq_config_init(xdev, cmdq);
	xsc_cmdq_rsp_cid_update(xdev, cmdq);
	xsc_cmdq_depth_set(xdev, cmdq);
	if (xsc_cmdq_elt_size_check(xdev, cmdq) != 0)
		goto error;

	xsc_cmdq_req_base_addr_set(xdev, cmdq);
	xsc_cmdq_rsp_base_addr_set(xdev, cmdq);
	/* Check request status and restore it */
	if (xsc_cmdq_req_status_restore(xdev, cmdq) != 0)
		goto error;

	rte_spinlock_init(&cmdq->lock);
	priv->cmdq = cmdq;
	return 0;

error:
	xsc_vfio_mbox_destroy(cmdq);
	return -1;
}

static enum xsc_cmd_status
xsc_cmdq_invoke(struct xsc_dev *xdev, struct xsc_cmdq_req_msg *req_msg,
		struct xsc_cmdq_rsp_msg *rsp_msg)
{
	struct xsc_vfio_priv *priv = (struct xsc_vfio_priv *)xdev->dev_priv;
	struct xsc_cmd_queue *cmdq = priv->cmdq;
	struct xsc_cmdq_req_layout *req_lay;
	enum xsc_cmd_status status = XSC_CMD_FAIL;

	rte_spinlock_lock(&cmdq->lock);
	req_lay = cmdq->req_lay + cmdq->req_pid;
	memset(req_lay, 0, sizeof(*req_lay));
	memcpy(req_lay->in, req_msg->hdr.data, sizeof(req_lay->in));
	if (req_msg->next != NULL)
		req_lay->in_ptr = rte_cpu_to_be_64(req_msg->next->buf_dma);
	req_lay->inlen = rte_cpu_to_be_32(req_msg->len);

	if (rsp_msg->next != NULL)
		req_lay->out_ptr = rte_cpu_to_be_64(rsp_msg->next->buf_dma);
	req_lay->outlen = rte_cpu_to_be_32(rsp_msg->len);

	req_lay->sig = 0xff;
	req_lay->idx = 0;
	req_lay->type = XSC_CMDQ_REQ_TYPE;

	/* Ring doorbell after the descriptor is valid */
	cmdq->req_pid = (cmdq->req_pid + 1) & cmdq->depth_m;
	rte_write32(cmdq->req_pid, (uint8_t *)xdev->bar_addr + cmdq->config->req_pid_addr);

	status = xsc_cmdq_wait_completion(xdev, rsp_msg);
	rte_spinlock_unlock(&cmdq->lock);

	return status;
}

int
xsc_vfio_mbox_exec(struct xsc_dev *xdev, void *data_in,
	      int in_len, void *data_out, int out_len)
{
	struct xsc_cmdq_req_msg *req_msg = NULL;
	struct xsc_cmdq_rsp_msg *rsp_msg = NULL;
	int ret;
	enum xsc_cmd_status status;

	ret = xsc_cmdq_msg_construct(xdev, &req_msg, in_len, &rsp_msg, out_len);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to construct cmd msg");
		return -1;
	}

	ret = xsc_cmdq_req_msg_copy(req_msg, data_in, in_len);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to copy cmd buf to request msg");
		goto error;
	}

	status = xsc_cmdq_invoke(xdev, req_msg, rsp_msg);
	if (status != XSC_CMD_SUCC) {
		PMD_DRV_LOG(ERR, "Failed to invoke xsc cmd, %s",
			    xsc_cmd_error[status]);
		ret = -1;
		goto error;
	}

	ret = xsc_cmdq_rsp_msg_copy(data_out, rsp_msg, out_len);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to copy response msg to out data");
		goto error;
	}

error:
	xsc_cmdq_msg_destruct(xdev, &req_msg, &rsp_msg);
	return ret;
}
