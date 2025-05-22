/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <rte_pci.h>
#include <ethdev_pci.h>
#include <rte_bus_pci.h>
#include <rte_bitops.h>

#include "xsc_defs.h"
#include "xsc_vfio_mbox.h"
#include "xsc_ethdev.h"
#include "xsc_rxtx.h"

#define XSC_FEATURE_ONCHIP_FT_MASK	RTE_BIT32(4)
#define XSC_FEATURE_DMA_RW_TBL_MASK	RTE_BIT32(8)
#define XSC_FEATURE_PCT_EXP_MASK	RTE_BIT32(19)
#define XSC_HOST_PCIE_NO_DEFAULT	0
#define XSC_SOC_PCIE_NO_DEFAULT		1

#define XSC_SW2HW_MTU(mtu)		((mtu) + 14 + 4)
#define XSC_SW2HW_RX_PKT_LEN(mtu)	((mtu) + 14 + 256)

enum xsc_cq_type {
	XSC_CQ_TYPE_NORMAL = 0,
	XSC_CQ_TYPE_VIRTIO = 1,
};

struct xsc_vfio_cq {
	const struct rte_memzone *mz;
	struct xsc_dev *xdev;
	uint32_t	cqn;
};

struct xsc_vfio_qp {
	const struct rte_memzone *mz;
	struct xsc_dev *xdev;
	uint32_t	qpn;
};

static void
xsc_vfio_pcie_no_init(struct xsc_hwinfo *hwinfo)
{
	uint func_id = hwinfo->func_id;

	if (func_id >= hwinfo->pf0_vf_funcid_base &&
	    func_id <= hwinfo->pf0_vf_funcid_top)
		hwinfo->pcie_no = hwinfo->pcie_host;
	else if (func_id >= hwinfo->pf1_vf_funcid_base &&
		 func_id <= hwinfo->pf1_vf_funcid_top)
		hwinfo->pcie_no = hwinfo->pcie_host;
	else if (func_id >= hwinfo->pcie0_pf_funcid_base &&
		 func_id <= hwinfo->pcie0_pf_funcid_top)
		hwinfo->pcie_no = XSC_HOST_PCIE_NO_DEFAULT;
	else
		hwinfo->pcie_no = XSC_SOC_PCIE_NO_DEFAULT;
}

static int
xsc_vfio_hwinfo_init(struct xsc_dev *xdev)
{
	int ret;
	uint32_t feature;
	int in_len, out_len, cmd_len;
	struct xsc_cmd_query_hca_cap_mbox_in *in;
	struct xsc_cmd_query_hca_cap_mbox_out *out;
	struct xsc_cmd_hca_cap *hca_cap;
	void *cmd_buf;

	in_len = sizeof(struct xsc_cmd_query_hca_cap_mbox_in);
	out_len = sizeof(struct xsc_cmd_query_hca_cap_mbox_out);
	cmd_len = RTE_MAX(in_len, out_len);

	cmd_buf = malloc(cmd_len);
	if (cmd_buf == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc dev hwinfo cmd memory");
		rte_errno = ENOMEM;
		return -rte_errno;
	}

	in = cmd_buf;
	memset(in, 0, cmd_len);
	in->hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_QUERY_HCA_CAP);
	in->hdr.ver = rte_cpu_to_be_16(XSC_CMD_QUERY_HCA_CAP_V1);
	out = cmd_buf;

	ret = xsc_vfio_mbox_exec(xdev, in, in_len, out, out_len);
	if (ret != 0 || out->hdr.status != 0) {
		PMD_DRV_LOG(ERR, "Failed to get dev hwinfo, err=%d, out.status=%u",
			    ret, out->hdr.status);
		rte_errno = ENOEXEC;
		ret = -rte_errno;
		goto exit;
	}

	hca_cap = &out->hca_cap;
	xdev->hwinfo.valid = 1;
	xdev->hwinfo.func_id = rte_be_to_cpu_32(hca_cap->glb_func_id);
	xdev->hwinfo.pcie_host = hca_cap->pcie_host;
	xdev->hwinfo.mac_phy_port = hca_cap->mac_port;
	xdev->hwinfo.funcid_to_logic_port_off = rte_be_to_cpu_16(hca_cap->funcid_to_logic_port);
	xdev->hwinfo.raw_qp_id_base = rte_be_to_cpu_16(hca_cap->raweth_qp_id_base);
	xdev->hwinfo.raw_rss_qp_id_base = rte_be_to_cpu_16(hca_cap->raweth_rss_qp_id_base);
	xdev->hwinfo.pf0_vf_funcid_base = rte_be_to_cpu_16(hca_cap->pf0_vf_funcid_base);
	xdev->hwinfo.pf0_vf_funcid_top = rte_be_to_cpu_16(hca_cap->pf0_vf_funcid_top);
	xdev->hwinfo.pf1_vf_funcid_base = rte_be_to_cpu_16(hca_cap->pf1_vf_funcid_base);
	xdev->hwinfo.pf1_vf_funcid_top = rte_be_to_cpu_16(hca_cap->pf1_vf_funcid_top);
	xdev->hwinfo.pcie0_pf_funcid_base = rte_be_to_cpu_16(hca_cap->pcie0_pf_funcid_base);
	xdev->hwinfo.pcie0_pf_funcid_top = rte_be_to_cpu_16(hca_cap->pcie0_pf_funcid_top);
	xdev->hwinfo.pcie1_pf_funcid_base = rte_be_to_cpu_16(hca_cap->pcie1_pf_funcid_base);
	xdev->hwinfo.pcie1_pf_funcid_top = rte_be_to_cpu_16(hca_cap->pcie1_pf_funcid_top);
	xdev->hwinfo.lag_port_start = hca_cap->lag_logic_port_ofst;
	xdev->hwinfo.raw_tpe_qp_num = rte_be_to_cpu_16(hca_cap->raw_tpe_qp_num);
	xdev->hwinfo.send_seg_num = hca_cap->send_seg_num;
	xdev->hwinfo.recv_seg_num = hca_cap->recv_seg_num;
	feature = rte_be_to_cpu_32(hca_cap->feature_flag);
	xdev->hwinfo.on_chip_tbl_vld = (feature & XSC_FEATURE_ONCHIP_FT_MASK) ? 1 : 0;
	xdev->hwinfo.dma_rw_tbl_vld = (feature & XSC_FEATURE_DMA_RW_TBL_MASK) ? 1 : 0;
	xdev->hwinfo.pct_compress_vld = (feature & XSC_FEATURE_PCT_EXP_MASK) ? 1 : 0;
	xdev->hwinfo.chip_version = rte_be_to_cpu_32(hca_cap->chip_ver_l);
	xdev->hwinfo.hca_core_clock = rte_be_to_cpu_32(hca_cap->hca_core_clock);
	xdev->hwinfo.mac_bit = hca_cap->mac_bit;
	xsc_vfio_pcie_no_init(&xdev->hwinfo);

exit:
	free(cmd_buf);
	return ret;
}

static int
xsc_vfio_dev_open(struct xsc_dev *xdev)
{
	struct rte_pci_addr *addr = &xdev->pci_dev->addr;
	struct xsc_vfio_priv *priv;

	snprintf(xdev->name, PCI_PRI_STR_SIZE, PCI_PRI_FMT,
		 addr->domain, addr->bus, addr->devid, addr->function);

	priv = rte_zmalloc(NULL, sizeof(*priv), RTE_CACHE_LINE_SIZE);
	if (priv == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc xsc vfio priv");
		return -ENOMEM;
	}

	xdev->dev_priv = (void *)priv;
	return 0;
}

static int
xsc_vfio_bar_init(struct xsc_dev *xdev)
{
	int ret;

	ret = rte_pci_map_device(xdev->pci_dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to map pci device");
		return -EINVAL;
	}

	xdev->bar_len = xdev->pci_dev->mem_resource[0].len;
	xdev->bar_addr = (void *)xdev->pci_dev->mem_resource[0].addr;
	if (xdev->bar_addr == NULL) {
		PMD_DRV_LOG(ERR, "Failed to attach dev(%s) bar", xdev->pci_dev->device.name);
		return -EINVAL;
	}

	return 0;
}

static int
xsc_vfio_dev_close(struct xsc_dev *xdev)
{
	struct xsc_vfio_priv *vfio_priv = (struct xsc_vfio_priv *)xdev->dev_priv;

	xsc_vfio_mbox_destroy(vfio_priv->cmdq);
	rte_pci_unmap_device(xdev->pci_dev);
	rte_free(vfio_priv);

	return 0;
}

static int
xsc_vfio_destroy_qp(void *qp)
{
	int ret;
	int in_len, out_len, cmd_len;
	struct xsc_cmd_destroy_qp_mbox_in *in;
	struct xsc_cmd_destroy_qp_mbox_out *out;
	struct xsc_vfio_qp *data = (struct xsc_vfio_qp *)qp;
	void *cmd_buf;

	in_len = sizeof(struct xsc_cmd_destroy_qp_mbox_in);
	out_len = sizeof(struct xsc_cmd_destroy_qp_mbox_out);
	cmd_len = RTE_MAX(in_len, out_len);

	cmd_buf = malloc(cmd_len);
	if (cmd_buf == NULL) {
		rte_errno = ENOMEM;
		PMD_DRV_LOG(ERR, "Failed to alloc qp destroy cmd memory");
		return -rte_errno;
	}

	in = cmd_buf;
	memset(in, 0, cmd_len);
	in->hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_DESTROY_QP);
	in->qpn = rte_cpu_to_be_32(data->qpn);
	out = cmd_buf;
	ret = xsc_vfio_mbox_exec(data->xdev, in, in_len, out, out_len);
	if (ret != 0 || out->hdr.status != 0) {
		PMD_DRV_LOG(ERR, "Failed to destroy qp, type=%d, err=%d, out.status=%u",
			    XSC_QUEUE_TYPE_RAW, ret, out->hdr.status);
		rte_errno = ENOEXEC;
		ret = -rte_errno;
		goto exit;
	}

	rte_memzone_free(data->mz);
	rte_free(qp);

exit:
	free(cmd_buf);
	return ret;
}

static int
xsc_vfio_destroy_cq(void *cq)
{
	int ret;
	int in_len, out_len, cmd_len;
	struct xsc_cmd_destroy_cq_mbox_in *in;
	struct xsc_cmd_destroy_cq_mbox_out *out;
	struct xsc_vfio_cq *data = (struct xsc_vfio_cq *)cq;
	void *cmd_buf;

	in_len = sizeof(struct xsc_cmd_destroy_cq_mbox_in);
	out_len = sizeof(struct xsc_cmd_destroy_cq_mbox_out);
	cmd_len = RTE_MAX(in_len, out_len);

	cmd_buf = malloc(cmd_len);
	if (cmd_buf == NULL) {
		rte_errno = ENOMEM;
		PMD_DRV_LOG(ERR, "Failed to alloc cq destroy cmd memory");
		return -rte_errno;
	}

	in = cmd_buf;
	memset(in, 0, cmd_len);

	in->hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_DESTROY_CQ);
	in->cqn = rte_cpu_to_be_32(data->cqn);
	out = cmd_buf;
	ret = xsc_vfio_mbox_exec(data->xdev, in, in_len, out, out_len);
	if (ret != 0 || out->hdr.status != 0) {
		PMD_DRV_LOG(ERR, "Failed to destroy cq, type=%d, err=%d, out.status=%u",
			    XSC_QUEUE_TYPE_RAW, ret, out->hdr.status);
		rte_errno = ENOEXEC;
		ret = -rte_errno;
		goto exit;
	}

	rte_memzone_free(data->mz);
	rte_free(cq);

exit:
	free(cmd_buf);
	return ret;
}

static int
xsc_vfio_set_mtu(struct xsc_dev *xdev, uint16_t mtu)
{
	struct xsc_cmd_set_mtu_mbox_in in = { };
	struct xsc_cmd_set_mtu_mbox_out out = { };
	int ret;

	in.hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_SET_MTU);
	in.mtu = rte_cpu_to_be_16(XSC_SW2HW_MTU(mtu));
	in.rx_buf_sz_min = rte_cpu_to_be_16(XSC_SW2HW_RX_PKT_LEN(mtu));
	in.mac_port = (uint8_t)xdev->hwinfo.mac_phy_port;

	ret = xsc_vfio_mbox_exec(xdev, &in, sizeof(in), &out, sizeof(out));
	if (ret != 0 || out.hdr.status != 0) {
		PMD_DRV_LOG(ERR, "Failed to set mtu, port=%d, err=%d, out.status=%u",
			    xdev->port_id, ret, out.hdr.status);
		rte_errno = ENOEXEC;
		ret = -rte_errno;
	}

	return ret;
}

static int
xsc_vfio_get_mac(struct xsc_dev *xdev, uint8_t *mac)
{
	struct xsc_cmd_query_eth_mac_mbox_in in = { };
	struct xsc_cmd_query_eth_mac_mbox_out out = { };
	int ret;

	in.hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_QUERY_ETH_MAC);
	ret = xsc_vfio_mbox_exec(xdev, &in, sizeof(in), &out, sizeof(out));
	if (ret != 0 || out.hdr.status != 0) {
		PMD_DRV_LOG(ERR, "Failed to get mtu, port=%d, err=%d, out.status=%u",
			    xdev->port_id, ret, out.hdr.status);
		rte_errno = ENOEXEC;
		return -rte_errno;
	}

	memcpy(mac, out.mac, RTE_ETHER_ADDR_LEN);

	return 0;
}

static int
xsc_vfio_modify_qp_status(struct xsc_dev *xdev, uint32_t qpn, int num, int opcode)
{
	int i, ret;
	int in_len, out_len, cmd_len;
	struct xsc_cmd_modify_qp_mbox_in *in;
	struct xsc_cmd_modify_qp_mbox_out *out;
	void *cmd_buf;

	in_len = sizeof(struct xsc_cmd_modify_qp_mbox_in);
	out_len = sizeof(struct xsc_cmd_modify_qp_mbox_out);
	cmd_len = RTE_MAX(in_len, out_len);

	cmd_buf = malloc(cmd_len);
	if (cmd_buf == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc cmdq qp modify status");
		rte_errno = ENOMEM;
		return -rte_errno;
	}

	in = cmd_buf;
	memset(in, 0, cmd_len);
	out = cmd_buf;

	for (i = 0; i < num; i++) {
		in->hdr.opcode = rte_cpu_to_be_16(opcode);
		in->hdr.ver = 0;
		in->qpn = rte_cpu_to_be_32(qpn + i);
		in->no_need_wait = 1;

		ret = xsc_vfio_mbox_exec(xdev, in, in_len, out, out_len);
		if (ret != 0 || out->hdr.status != 0) {
			PMD_DRV_LOG(ERR, "Modify qp status failed, qpn=%u, err=%d, out.status=%u",
				    qpn + i, ret, out->hdr.status);
			rte_errno = ENOEXEC;
			ret = -rte_errno;
			goto exit;
		}
	}

exit:
	free(cmd_buf);
	return ret;
}

static int
xsc_vfio_modify_qp_qostree(struct xsc_dev *xdev, uint16_t qpn)
{
	int ret;
	int in_len, out_len, cmd_len;
	struct xsc_cmd_modify_raw_qp_mbox_in *in;
	struct xsc_cmd_modify_raw_qp_mbox_out *out;
	void *cmd_buf;

	in_len = sizeof(struct xsc_cmd_modify_raw_qp_mbox_in);
	out_len = sizeof(struct xsc_cmd_modify_raw_qp_mbox_out);
	cmd_len = RTE_MAX(in_len, out_len);

	cmd_buf = malloc(cmd_len);
	if (cmd_buf == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc cmdq qp modify qostree");
		rte_errno = ENOMEM;
		return -rte_errno;
	}

	in = cmd_buf;
	memset(in, 0, cmd_len);
	in->hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_MODIFY_RAW_QP);
	in->req.prio = 0;
	in->req.qp_out_port = 0xFF;
	in->req.lag_id = rte_cpu_to_be_16(xdev->hwinfo.lag_id);
	in->req.func_id = rte_cpu_to_be_16(xdev->hwinfo.func_id);
	in->req.dma_direct = 0;
	in->req.qpn = rte_cpu_to_be_16(qpn);
	out = cmd_buf;

	ret = xsc_vfio_mbox_exec(xdev, in, in_len, out, out_len);
	if (ret != 0 || out->hdr.status != 0) {
		PMD_DRV_LOG(ERR, "Filed to modify qp qostree, qpn=%d, err=%d, out.status=%u",
			    qpn, ret, out->hdr.status);
		rte_errno = ENOEXEC;
		ret = -rte_errno;
		goto exit;
	}

exit:
	free(cmd_buf);
	return ret;
}

static int
xsc_vfio_rx_cq_create(struct xsc_dev *xdev, struct xsc_rx_cq_params *cq_params,
		      struct xsc_rx_cq_info *cq_info)
{
	int ret;
	int pa_len;
	uint16_t i;
	uint16_t pa_num;
	uint8_t log_cq_sz;
	uint16_t cqe_n;
	uint32_t cqe_total_sz;
	int in_len, out_len, cmd_len;
	char name[RTE_ETH_NAME_MAX_LEN] = { 0 };
	uint16_t port_id = cq_params->port_id;
	uint16_t idx = cq_params->qp_id;
	struct xsc_vfio_cq *cq;
	const struct rte_memzone *cq_pas = NULL;
	struct xsc_cqe *cqes;
	struct xsc_cmd_create_cq_mbox_in *in = NULL;
	struct xsc_cmd_create_cq_mbox_out *out = NULL;
	void *cmd_buf;

	cqe_n = cq_params->wqe_s;
	log_cq_sz = rte_log2_u32(cqe_n);
	cqe_total_sz = cqe_n * sizeof(struct xsc_cqe);
	pa_num = (cqe_total_sz + XSC_PAGE_SIZE - 1) / XSC_PAGE_SIZE;
	pa_len = sizeof(uint64_t) * pa_num;
	in_len = sizeof(struct xsc_cmd_create_cq_mbox_in) + pa_len;
	out_len = sizeof(struct xsc_cmd_create_cq_mbox_out);
	cmd_len = RTE_MAX(in_len, out_len);

	cq = rte_zmalloc(NULL, sizeof(struct xsc_vfio_cq), 0);
	if (cq == NULL) {
		rte_errno = ENOMEM;
		PMD_DRV_LOG(ERR, "Failed to alloc rx cq memory");
		return -rte_errno;
	}

	cmd_buf = malloc(cmd_len);
	if (cmd_buf == NULL) {
		rte_errno = ENOMEM;
		PMD_DRV_LOG(ERR, "Failed to alloc rx cq exec cmd memory");
		goto error;
	}

	in = cmd_buf;
	memset(in, 0, cmd_len);
	in->hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_CREATE_CQ);
	in->ctx.eqn = 0;
	in->ctx.pa_num = rte_cpu_to_be_16(pa_num);
	in->ctx.glb_func_id = rte_cpu_to_be_16((uint16_t)xdev->hwinfo.func_id);
	in->ctx.log_cq_sz = log_cq_sz;
	in->ctx.cq_type = XSC_CQ_TYPE_NORMAL;

	snprintf(name, sizeof(name), "mz_cqe_mem_rx_%u_%u", port_id, idx);
	cq_pas = rte_memzone_reserve_aligned(name,
					     (XSC_PAGE_SIZE * pa_num),
					     SOCKET_ID_ANY,
					     0, XSC_PAGE_SIZE);
	if (cq_pas == NULL) {
		rte_errno = ENOMEM;
		PMD_DRV_LOG(ERR, "Failed to alloc rx cq pas memory");
		goto error;
	}
	cq->mz = cq_pas;

	for (i = 0; i < pa_num; i++)
		in->pas[i] = rte_cpu_to_be_64(cq_pas->iova + i * XSC_PAGE_SIZE);

	out = cmd_buf;
	ret = xsc_vfio_mbox_exec(xdev, in, in_len, out, out_len);
	if (ret != 0 || out->hdr.status != 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to exec rx cq create cmd, port id=%d, err=%d, out.status=%u",
			    port_id, ret, out->hdr.status);
		rte_errno = ENOEXEC;
		goto error;
	}

	cq_info->cq = (void *)cq;
	cq_info->cqe_n = log_cq_sz;
	cqes = (struct xsc_cqe *)cq_pas->addr;
	for (i = 0; i < (1 << cq_info->cqe_n); i++)
		((volatile struct xsc_cqe *)(cqes + i))->owner = 1;
	cq_info->cqes = cqes;
	if (xsc_dev_is_vf(xdev))
		cq_info->cq_db = (uint32_t *)((uint8_t *)xdev->bar_addr + XSC_VF_CQ_DB_ADDR);
	else
		cq_info->cq_db = (uint32_t *)((uint8_t *)xdev->bar_addr + XSC_PF_CQ_DB_ADDR);
	cq_info->cqn = rte_be_to_cpu_32(out->cqn);
	cq->cqn = cq_info->cqn;
	cq->xdev = xdev;
	PMD_DRV_LOG(INFO, "Port id=%d, Rx cqe_n:%d, cqn:%u",
		    port_id, cq_info->cqe_n, cq_info->cqn);

	free(cmd_buf);
	return 0;

error:
	free(cmd_buf);
	rte_memzone_free(cq_pas);
	rte_free(cq);
	return -rte_errno;
}

static int
xsc_vfio_tx_cq_create(struct xsc_dev *xdev, struct xsc_tx_cq_params *cq_params,
		      struct xsc_tx_cq_info *cq_info)
{
	struct xsc_vfio_cq *cq = NULL;
	char name[RTE_ETH_NAME_MAX_LEN] = {0};
	struct xsc_cmd_create_cq_mbox_in *in = NULL;
	struct xsc_cmd_create_cq_mbox_out *out = NULL;
	const struct rte_memzone *cq_pas = NULL;
	struct xsc_cqe *cqes;
	int in_len, out_len, cmd_len;
	uint16_t pa_num;
	uint16_t log_cq_sz;
	int ret = 0;
	int cqe_s = 1 << cq_params->elts_n;
	uint64_t iova;
	int i;
	void *cmd_buf = NULL;

	cq = rte_zmalloc(NULL, sizeof(struct xsc_vfio_cq), 0);
	if (cq == NULL) {
		rte_errno = ENOMEM;
		PMD_DRV_LOG(ERR, "Failed to alloc tx cq memory");
		return -rte_errno;
	}

	log_cq_sz = rte_log2_u32(cqe_s);
	pa_num = (((1 << log_cq_sz) * sizeof(struct xsc_cqe)) / XSC_PAGE_SIZE);

	snprintf(name, sizeof(name), "mz_cqe_mem_tx_%u_%u", cq_params->port_id, cq_params->qp_id);
	cq_pas = rte_memzone_reserve_aligned(name,
					     (XSC_PAGE_SIZE * pa_num),
					     SOCKET_ID_ANY,
					     0, XSC_PAGE_SIZE);
	if (cq_pas == NULL) {
		rte_errno = ENOMEM;
		PMD_DRV_LOG(ERR, "Failed to alloc tx cq pas memory");
		goto error;
	}

	cq->mz = cq_pas;
	in_len = (sizeof(struct xsc_cmd_create_cq_mbox_in) + (pa_num * sizeof(uint64_t)));
	out_len = sizeof(struct xsc_cmd_create_cq_mbox_out);
	cmd_len = RTE_MAX(in_len, out_len);
	cmd_buf = malloc(cmd_len);
	if (cmd_buf == NULL) {
		rte_errno = ENOMEM;
		PMD_DRV_LOG(ERR, "Failed to alloc tx cq exec cmd memory");
		goto error;
	}

	in = cmd_buf;
	memset(in, 0, cmd_len);

	in->hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_CREATE_CQ);
	in->ctx.eqn = 0;
	in->ctx.pa_num = rte_cpu_to_be_16(pa_num);
	in->ctx.glb_func_id = rte_cpu_to_be_16((uint16_t)xdev->hwinfo.func_id);
	in->ctx.log_cq_sz = rte_log2_u32(cqe_s);
	in->ctx.cq_type = XSC_CQ_TYPE_NORMAL;
	iova = cq->mz->iova;
	for (i = 0; i < pa_num; i++)
		in->pas[i] = rte_cpu_to_be_64(iova + i * XSC_PAGE_SIZE);

	out = cmd_buf;
	ret = xsc_vfio_mbox_exec(xdev, in, in_len, out, out_len);
	if (ret != 0 || out->hdr.status != 0) {
		PMD_DRV_LOG(ERR, "Failed to create tx cq, port id=%u, err=%d, out.status=%u",
			    cq_params->port_id, ret, out->hdr.status);
		rte_errno = ENOEXEC;
		goto error;
	}

	cq->cqn = rte_be_to_cpu_32(out->cqn);
	cq->xdev = xdev;

	cq_info->cq = cq;
	cqes = (struct xsc_cqe *)((uint8_t *)cq->mz->addr);
	if (xsc_dev_is_vf(xdev))
		cq_info->cq_db = (uint32_t *)((uint8_t *)xdev->bar_addr + XSC_VF_CQ_DB_ADDR);
	else
		cq_info->cq_db = (uint32_t *)((uint8_t *)xdev->bar_addr + XSC_PF_CQ_DB_ADDR);
	cq_info->cqn = cq->cqn;
	cq_info->cqe_s = cqe_s;
	cq_info->cqe_n = log_cq_sz;

	for (i = 0; i < cq_info->cqe_s; i++)
		((volatile struct xsc_cqe *)(cqes + i))->owner = 1;
	cq_info->cqes = cqes;

	free(cmd_buf);
	return 0;

error:
	free(cmd_buf);
	rte_memzone_free(cq_pas);
	rte_free(cq);
	return -rte_errno;
}

static int
xsc_vfio_tx_qp_create(struct xsc_dev *xdev, struct xsc_tx_qp_params *qp_params,
		      struct xsc_tx_qp_info *qp_info)
{
	struct xsc_cmd_create_qp_mbox_in *in = NULL;
	struct xsc_cmd_create_qp_mbox_out *out = NULL;
	const struct rte_memzone *qp_pas = NULL;
	struct xsc_vfio_cq *cq = (struct xsc_vfio_cq *)qp_params->cq;
	struct xsc_vfio_qp *qp = NULL;
	int in_len, out_len, cmd_len;
	int ret = 0;
	uint32_t send_ds_num = xdev->hwinfo.send_seg_num;
	int wqe_s = 1 << qp_params->elts_n;
	uint16_t pa_num;
	uint8_t log_ele = 0;
	uint32_t log_rq_sz = 0;
	uint32_t log_sq_sz = 0;
	int i;
	uint64_t iova;
	char name[RTE_ETH_NAME_MAX_LEN] = {0};
	void *cmd_buf = NULL;

	qp = rte_zmalloc(NULL, sizeof(struct xsc_vfio_qp), 0);
	if (qp == NULL) {
		rte_errno = ENOMEM;
		PMD_DRV_LOG(ERR, "Failed to alloc tx qp memory");
		return -rte_errno;
	}

	log_sq_sz = rte_log2_u32(wqe_s * send_ds_num);
	log_ele = rte_log2_u32(sizeof(struct xsc_wqe_data_seg));
	pa_num = ((1 << (log_rq_sz + log_sq_sz + log_ele))) / XSC_PAGE_SIZE;

	snprintf(name, sizeof(name), "mz_wqe_mem_tx_%u_%u", qp_params->port_id, qp_params->qp_id);
	qp_pas = rte_memzone_reserve_aligned(name,
					     (XSC_PAGE_SIZE * pa_num),
					     SOCKET_ID_ANY,
					     0, XSC_PAGE_SIZE);
	if (qp_pas == NULL) {
		rte_errno = ENOMEM;
		PMD_DRV_LOG(ERR, "Failed to alloc tx qp pas memory");
		goto error;
	}
	qp->mz = qp_pas;

	in_len = (sizeof(struct xsc_cmd_create_qp_mbox_in) + (pa_num * sizeof(uint64_t)));
	out_len = sizeof(struct xsc_cmd_create_qp_mbox_out);
	cmd_len = RTE_MAX(in_len, out_len);
	cmd_buf = malloc(cmd_len);
	if (cmd_buf == NULL) {
		rte_errno = ENOMEM;
		PMD_DRV_LOG(ERR, "Failed to alloc tx qp exec cmd memory");
		goto error;
	}

	in = cmd_buf;
	memset(in, 0, cmd_len);

	in->hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_CREATE_QP);
	in->req.input_qpn = 0;
	in->req.pa_num = rte_cpu_to_be_16(pa_num);
	in->req.qp_type = XSC_QUEUE_TYPE_RAW_TX;
	in->req.log_sq_sz = log_sq_sz;
	in->req.log_rq_sz = log_rq_sz;
	in->req.dma_direct = 0;
	in->req.pdn = 0;
	in->req.cqn_send = rte_cpu_to_be_16((uint16_t)cq->cqn);
	in->req.cqn_recv = 0;
	in->req.glb_funcid = rte_cpu_to_be_16((uint16_t)xdev->hwinfo.func_id);
	iova = qp->mz->iova;
	for (i = 0; i < pa_num; i++)
		in->req.pas[i] = rte_cpu_to_be_64(iova + i * XSC_PAGE_SIZE);

	out = cmd_buf;
	ret = xsc_vfio_mbox_exec(xdev, in, in_len, out, out_len);
	if (ret != 0 || out->hdr.status != 0) {
		PMD_DRV_LOG(ERR, "Failed to create tx qp, port id=%u, err=%d, out.status=%u",
			    qp_params->port_id, ret, out->hdr.status);
		rte_errno = ENOEXEC;
		goto error;
	}

	qp->qpn = rte_be_to_cpu_32(out->qpn);
	qp->xdev = xdev;

	qp_info->qp = qp;
	qp_info->qpn = qp->qpn;
	qp_info->wqes = (struct xsc_wqe *)qp->mz->addr;
	qp_info->wqe_n = rte_log2_u32(wqe_s);

	if (xsc_dev_is_vf(xdev))
		qp_info->qp_db = (uint32_t *)((uint8_t *)xdev->bar_addr + XSC_VF_TX_DB_ADDR);
	else
		qp_info->qp_db = (uint32_t *)((uint8_t *)xdev->bar_addr + XSC_PF_TX_DB_ADDR);

	free(cmd_buf);
	return 0;

error:
	free(cmd_buf);
	rte_memzone_free(qp_pas);
	rte_free(qp);
	return -rte_errno;
}

static int
xsc_vfio_dev_init(struct xsc_dev *xdev)
{
	int ret;

	ret = xsc_vfio_dev_open(xdev);
	if (ret != 0)
		goto open_fail;

	ret = xsc_vfio_bar_init(xdev);
	if (ret != 0)
		goto init_fail;

	if (xsc_vfio_mbox_init(xdev) != 0)
		goto init_fail;

	ret = xsc_vfio_hwinfo_init(xdev);
	if (ret != 0)
		goto init_fail;

	return 0;

init_fail:
	xsc_vfio_dev_close(xdev);

open_fail:
	return -1;
}

static struct xsc_dev_ops *xsc_vfio_ops = &(struct xsc_dev_ops) {
	.kdrv = RTE_PCI_KDRV_VFIO,
	.dev_init = xsc_vfio_dev_init,
	.dev_close = xsc_vfio_dev_close,
	.set_mtu = xsc_vfio_set_mtu,
	.get_mac = xsc_vfio_get_mac,
	.destroy_qp = xsc_vfio_destroy_qp,
	.destroy_cq = xsc_vfio_destroy_cq,
	.modify_qp_status = xsc_vfio_modify_qp_status,
	.modify_qp_qostree = xsc_vfio_modify_qp_qostree,
	.rx_cq_create = xsc_vfio_rx_cq_create,
	.tx_cq_create = xsc_vfio_tx_cq_create,
	.tx_qp_create = xsc_vfio_tx_qp_create,
	.mailbox_exec = xsc_vfio_mbox_exec,
};

RTE_INIT(xsc_vfio_ops_reg)
{
	xsc_dev_ops_register(xsc_vfio_ops);
}
