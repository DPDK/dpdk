/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <bus_pci_driver.h>
#include <rte_kvargs.h>
#include <rte_eal_paging.h>
#include <rte_bitops.h>
#include <rte_string_fns.h>

#include "xsc_log.h"
#include "xsc_defs.h"
#include "xsc_dev.h"
#include "xsc_cmd.h"

#define XSC_DEV_DEF_FLOW_MODE	7

TAILQ_HEAD(xsc_dev_ops_list, xsc_dev_ops);
static struct xsc_dev_ops_list dev_ops_list = TAILQ_HEAD_INITIALIZER(dev_ops_list);

static const struct xsc_dev_ops *
xsc_dev_ops_get(enum rte_pci_kernel_driver kdrv)
{
	const struct xsc_dev_ops *ops;

	TAILQ_FOREACH(ops, &dev_ops_list, entry) {
		if (ops->kdrv == kdrv)
			return ops;
	}

	return NULL;
}

void
xsc_dev_ops_register(struct xsc_dev_ops *new_ops)
{
	struct xsc_dev_ops *ops;

	TAILQ_FOREACH(ops, &dev_ops_list, entry) {
		if (ops->kdrv == new_ops->kdrv) {
			PMD_DRV_LOG(ERR, "xsc dev ops exists, kdrv=%d", new_ops->kdrv);
			return;
		}
	}

	TAILQ_INSERT_TAIL(&dev_ops_list, new_ops, entry);
}

int
xsc_dev_mailbox_exec(struct xsc_dev *xdev, void *data_in,
		     int in_len, void *data_out, int out_len)
{
	return xdev->dev_ops->mailbox_exec(xdev, data_in, in_len,
					   data_out, out_len);
}

int
xsc_dev_link_status_set(struct xsc_dev *xdev, uint16_t status)
{
	if (xdev->dev_ops->link_status_set == NULL)
		return -ENOTSUP;

	return xdev->dev_ops->link_status_set(xdev, status);
}

int
xsc_dev_link_get(struct xsc_dev *xdev, struct rte_eth_link *link)
{
	if (xdev->dev_ops->link_get == NULL)
		return -ENOTSUP;

	return xdev->dev_ops->link_get(xdev, link);
}

int
xsc_dev_set_mtu(struct xsc_dev *xdev, uint16_t mtu)
{
	return xdev->dev_ops->set_mtu(xdev, mtu);
}

int
xsc_dev_get_mac(struct xsc_dev *xdev, uint8_t *mac)
{
	return xdev->dev_ops->get_mac(xdev, mac);
}

int
xsc_dev_destroy_qp(struct xsc_dev *xdev, void *qp)
{
	return xdev->dev_ops->destroy_qp(qp);
}

int
xsc_dev_destroy_cq(struct xsc_dev *xdev, void *cq)
{
	return xdev->dev_ops->destroy_cq(cq);
}

int
xsc_dev_modify_qp_status(struct xsc_dev *xdev, uint32_t qpn, int num, int opcode)
{
	return xdev->dev_ops->modify_qp_status(xdev, qpn, num, opcode);
}

int
xsc_dev_modify_qp_qostree(struct xsc_dev *xdev, uint16_t qpn)
{
	return xdev->dev_ops->modify_qp_qostree(xdev, qpn);
}

int
xsc_dev_rx_cq_create(struct xsc_dev *xdev, struct xsc_rx_cq_params *cq_params,
		     struct xsc_rx_cq_info *cq_info)
{
	return xdev->dev_ops->rx_cq_create(xdev, cq_params, cq_info);
}

int
xsc_dev_tx_cq_create(struct xsc_dev *xdev, struct xsc_tx_cq_params *cq_params,
		     struct xsc_tx_cq_info *cq_info)
{
	return xdev->dev_ops->tx_cq_create(xdev, cq_params, cq_info);
}

int
xsc_dev_tx_qp_create(struct xsc_dev *xdev, struct xsc_tx_qp_params *qp_params,
		     struct xsc_tx_qp_info *qp_info)
{
	return xdev->dev_ops->tx_qp_create(xdev, qp_params, qp_info);
}

int
xsc_dev_close(struct xsc_dev *xdev, int repr_id)
{
	xsc_dev_clear_pct(xdev, repr_id);

	if (repr_id == xdev->num_repr_ports - 1)
		return xdev->dev_ops->dev_close(xdev);
	return 0;
}

int
xsc_dev_rss_key_modify(struct xsc_dev *xdev, uint8_t *rss_key, uint8_t rss_key_len)
{
	struct xsc_cmd_modify_nic_hca_mbox_in in = {};
	struct xsc_cmd_modify_nic_hca_mbox_out out = {};
	uint8_t rss_caps_mask = 0;
	int ret, key_len = 0;

	in.hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_MODIFY_NIC_HCA);

	key_len = RTE_MIN(rss_key_len, XSC_RSS_HASH_KEY_LEN);
	rte_memcpy(in.rss.hash_key, rss_key, key_len);
	rss_caps_mask |= RTE_BIT32(XSC_RSS_HASH_KEY_UPDATE);

	in.rss.caps_mask = rss_caps_mask;
	in.rss.rss_en = 1;
	in.nic.caps_mask = rte_cpu_to_be_16(RTE_BIT32(XSC_TBM_CAP_RSS));
	in.nic.caps = in.nic.caps_mask;

	ret = xsc_dev_mailbox_exec(xdev, &in, sizeof(in), &out, sizeof(out));
	if (ret != 0 || out.hdr.status != 0)
		return -1;
	return 0;
}

static int
xsc_dev_alloc_vfos_info(struct xsc_dev *xdev)
{
	struct xsc_hwinfo *hwinfo;
	int base_lp = 0;

	if (xsc_dev_is_vf(xdev))
		return 0;

	hwinfo = &xdev->hwinfo;
	if (hwinfo->pcie_no == 1) {
		xdev->vfrep_offset = hwinfo->func_id -
			hwinfo->pcie1_pf_funcid_base +
			hwinfo->pcie0_pf_funcid_top -
			hwinfo->pcie0_pf_funcid_base  + 1;
	} else {
		xdev->vfrep_offset = hwinfo->func_id - hwinfo->pcie0_pf_funcid_base;
	}

	base_lp = XSC_VFREP_BASE_LOGICAL_PORT;
	if (xdev->devargs.nic_mode == XSC_NIC_MODE_LEGACY)
		base_lp += xdev->vfrep_offset;
	xdev->vfos_logical_in_port = base_lp;
	return 0;
}

static void
xsc_dev_args_parse(struct xsc_dev *xdev, struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;
	struct xsc_devargs *xdevargs = &xdev->devargs;
	const char *tmp;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return;

	tmp = rte_kvargs_get(kvlist, XSC_PPH_MODE_ARG);
	if (tmp != NULL)
		xdevargs->pph_mode = atoi(tmp);
	else
		xdevargs->pph_mode = XSC_PPH_NONE;

	tmp = rte_kvargs_get(kvlist, XSC_NIC_MODE_ARG);
	if (tmp != NULL)
		xdevargs->nic_mode = atoi(tmp);
	else
		xdevargs->nic_mode = XSC_NIC_MODE_LEGACY;

	tmp = rte_kvargs_get(kvlist, XSC_FLOW_MODE_ARG);
	if (tmp != NULL)
		xdevargs->flow_mode = atoi(tmp);
	else
		xdevargs->flow_mode = XSC_DEV_DEF_FLOW_MODE;

	rte_kvargs_free(kvlist);
}

int
xsc_dev_qp_set_id_get(struct xsc_dev *xdev, int repr_id)
{
	if (xsc_dev_is_vf(xdev))
		return 0;

	return (repr_id % 511 + 1);
}

static void
xsc_repr_info_init(struct xsc_dev *xdev, struct xsc_repr_info *info,
		   enum xsc_port_type port_type,
		   enum xsc_funcid_type funcid_type, int32_t repr_id)
{
	int qp_set_id, logical_port;
	struct xsc_hwinfo *hwinfo = &xdev->hwinfo;

	info->repr_id = repr_id;
	info->port_type = port_type;
	if (port_type == XSC_PORT_TYPE_UPLINK_BOND) {
		info->pf_bond = 1;
		info->funcid = XSC_PHYPORT_LAG_FUNCID << 14;
	} else if (port_type == XSC_PORT_TYPE_UPLINK) {
		info->pf_bond = -1;
		info->funcid = funcid_type << 14;
	} else if (port_type == XSC_PORT_TYPE_PFVF) {
		info->funcid = funcid_type << 14;
	}

	qp_set_id = xsc_dev_qp_set_id_get(xdev, repr_id);
	if (xsc_dev_is_vf(xdev))
		logical_port = xdev->hwinfo.func_id +
			       xdev->hwinfo.funcid_to_logic_port_off;
	else
		logical_port = xdev->vfos_logical_in_port + qp_set_id - 1;

	info->logical_port = logical_port;
	info->local_dstinfo = logical_port;
	info->peer_logical_port = hwinfo->mac_phy_port;
	info->peer_dstinfo = hwinfo->mac_phy_port;
}

int
xsc_dev_repr_ports_probe(struct xsc_dev *xdev, int nb_repr_ports, int max_eth_ports)
{
	int funcid_type;
	struct xsc_repr_port *repr_port;
	int i;

	PMD_INIT_FUNC_TRACE();

	xdev->num_repr_ports = nb_repr_ports + XSC_PHY_PORT_NUM;
	if (xdev->num_repr_ports > max_eth_ports) {
		PMD_DRV_LOG(ERR, "Repr ports num %d, should be less than max %d",
			    xdev->num_repr_ports, max_eth_ports);
		return -EINVAL;
	}

	xdev->repr_ports = rte_zmalloc(NULL,
				       sizeof(struct xsc_repr_port) * xdev->num_repr_ports,
				       RTE_CACHE_LINE_SIZE);
	if (xdev->repr_ports == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for repr ports");
		return -ENOMEM;
	}

	funcid_type = (xdev->devargs.nic_mode == XSC_NIC_MODE_SWITCHDEV) ?
		XSC_VF_IOCTL_FUNCID : XSC_PHYPORT_MAC_FUNCID;

	/* PF representor use the last repr_ports */
	repr_port = &xdev->repr_ports[xdev->num_repr_ports - 1];
	xsc_repr_info_init(xdev, &repr_port->info, XSC_PORT_TYPE_UPLINK,
			   XSC_PHYPORT_MAC_FUNCID, xdev->num_repr_ports - 1);
	repr_port->info.ifindex = xdev->ifindex;
	repr_port->xdev = xdev;
	LIST_INIT(&repr_port->def_pct_list);

	/* VF representor start from 0 */
	for (i = 0; i < nb_repr_ports; i++) {
		repr_port = &xdev->repr_ports[i];
		xsc_repr_info_init(xdev, &repr_port->info,
				   XSC_PORT_TYPE_PFVF, funcid_type, i);
		repr_port->xdev = xdev;
		LIST_INIT(&repr_port->def_pct_list);
	}

	return 0;
}

static void
xsc_dev_reg_addr_init(struct xsc_dev *xdev)
{
	struct xsc_dev_reg_addr *reg = &xdev->reg_addr;
	uint8_t *bar_addr = xdev->bar_addr;

	if (xsc_dev_is_vf(xdev) || xdev->bar_len != XSC_DEV_BAR_LEN_256M) {
		reg->rxq_db_addr = (uint32_t *)(bar_addr + XSC_VF_RX_DB_ADDR);
		reg->txq_db_addr = (uint32_t *)(bar_addr + XSC_VF_TX_DB_ADDR);
		reg->cq_db_addr = (uint32_t *)(bar_addr + XSC_VF_CQ_DB_ADDR);
		reg->cq_pi_start = XSC_VF_CQ_PID_START_ADDR;
	} else {
		reg->rxq_db_addr = (uint32_t *)(bar_addr + XSC_PF_RX_DB_ADDR);
		reg->txq_db_addr = (uint32_t *)(bar_addr + XSC_PF_TX_DB_ADDR);
		reg->cq_db_addr = (uint32_t *)(bar_addr + XSC_PF_CQ_DB_ADDR);
		reg->cq_pi_start = XSC_PF_CQ_PID_START_ADDR;
	}
}

void
xsc_dev_uninit(struct xsc_dev *xdev)
{
	PMD_INIT_FUNC_TRACE();
	xsc_dev_pct_uninit(xdev);
	xsc_dev_close(xdev, XSC_DEV_REPR_ID_INVALID);
	rte_free(xdev);
}

int
xsc_dev_init(struct rte_pci_device *pci_dev, struct xsc_dev **xdev)
{
	struct xsc_dev *d;
	int ret;

	PMD_INIT_FUNC_TRACE();

	d = rte_zmalloc(NULL, sizeof(*d), RTE_CACHE_LINE_SIZE);
	if (d == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory for xsc_dev");
		return -ENOMEM;
	}

	d->dev_ops = xsc_dev_ops_get(pci_dev->kdrv);
	if (d->dev_ops == NULL) {
		PMD_DRV_LOG(ERR, "Could not get dev_ops, kdrv=%d", pci_dev->kdrv);
		return -ENODEV;
	}

	d->pci_dev = pci_dev;

	if (d->dev_ops->dev_init)
		d->dev_ops->dev_init(d);

	xsc_dev_reg_addr_init(d);
	xsc_dev_args_parse(d, pci_dev->device.devargs);

	ret = xsc_dev_alloc_vfos_info(d);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to alloc vfos info");
		ret = -EINVAL;
		goto hwinfo_init_fail;
	}

	ret = xsc_dev_mac_port_init(d);
	if (ret)
		goto hwinfo_init_fail;

	ret = xsc_dev_pct_init(d);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to init xsc pct");
		ret = -EINVAL;
		goto hwinfo_init_fail;
	}

	*xdev = d;

	return 0;

hwinfo_init_fail:
	xsc_dev_uninit(d);
	return ret;
}

bool
xsc_dev_is_vf(struct xsc_dev *xdev)
{
	uint16_t device_id = xdev->pci_dev->id.device_id;

	if (device_id == XSC_PCI_DEV_ID_MSVF ||
	    device_id == XSC_PCI_DEV_ID_MVHVF)
		return true;

	return false;
}

int
xsc_dev_fw_version_get(struct xsc_dev *xdev, char *fw_version, size_t fw_size)
{
	size_t size = strnlen(xdev->hwinfo.fw_ver, sizeof(xdev->hwinfo.fw_ver)) + 1;

	if (fw_size < size)
		return size;
	rte_strlcpy(fw_version, xdev->hwinfo.fw_ver, fw_size);

	return 0;
}

static int
xsc_dev_access_reg(struct xsc_dev *xdev, void *data_in, int size_in,
		   void *data_out, int size_out, uint16_t reg_num)
{
	struct xsc_cmd_access_reg_mbox_in *in;
	struct xsc_cmd_access_reg_mbox_out *out;
	int ret = -1;

	in = malloc(sizeof(*in) + size_in);
	if (in == NULL) {
		rte_errno = ENOMEM;
		PMD_DRV_LOG(ERR, "Failed to malloc access reg mbox in memory");
		return -rte_errno;
	}
	memset(in, 0, sizeof(*in) + size_in);

	out = malloc(sizeof(*out) + size_out);
	if (out == NULL) {
		rte_errno = ENOMEM;
		PMD_DRV_LOG(ERR, "Failed to malloc access reg mbox out memory");
		goto alloc_out_fail;
	}
	memset(out, 0, sizeof(*out) + size_out);

	memcpy(in->data, data_in, size_in);
	in->hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_ACCESS_REG);
	in->arg = 0;
	in->register_id = rte_cpu_to_be_16(reg_num);

	ret = xsc_dev_mailbox_exec(xdev, in, sizeof(*in) + size_in, out,
				   sizeof(*out) + size_out);
	if (ret != 0 || out->hdr.status != 0) {
		rte_errno = ENOEXEC;
		PMD_DRV_LOG(ERR, "Failed to access reg");
		goto exit;
	}

	memcpy(data_out, out->data, size_out);

exit:
	free(out);

alloc_out_fail:
	free(in);
	return ret;
}

static int
xsc_dev_query_mcia(struct xsc_dev *xdev,
		   struct xsc_module_eeprom_query_params *params,
		   uint8_t *data)
{
	struct xsc_dev_reg_mcia in = { };
	struct xsc_dev_reg_mcia out = { };
	int ret;
	void *ptr;
	uint16_t size;

	size = RTE_MIN(params->size, XSC_EEPROM_MAX_BYTES);
	in.i2c_device_address = params->i2c_address;
	in.module = params->module_number & 0x000000FF;
	in.device_address = params->offset;
	in.page_number = params->page;
	in.size = size;

	ret = xsc_dev_access_reg(xdev, &in, sizeof(in), &out, sizeof(out), XSC_REG_MCIA);
	if (ret != 0)
		return ret;

	ptr = out.dword_0;
	memcpy(data, ptr, size);

	return size;
}

static void
xsc_dev_sfp_eeprom_params_set(uint16_t *i2c_addr, uint32_t *page_num, uint16_t *offset)
{
	*i2c_addr = XSC_I2C_ADDR_LOW;
	*page_num = 0;

	if (*offset < XSC_EEPROM_PAGE_LENGTH)
		return;

	*i2c_addr = XSC_I2C_ADDR_HIGH;
	*offset -= XSC_EEPROM_PAGE_LENGTH;
}

static int
xsc_dev_qsfp_eeprom_page(uint16_t offset)
{
	if (offset < XSC_EEPROM_PAGE_LENGTH)
		/* Addresses between 0-255 - page 00 */
		return 0;

	/*
	 * Addresses between 256 - 639 belongs to pages 01, 02 and 03
	 * For example, offset = 400 belongs to page 02:
	 * 1 + ((400 - 256)/128) = 2
	 */
	return 1 + ((offset - XSC_EEPROM_PAGE_LENGTH) / XSC_EEPROM_HIGH_PAGE_LENGTH);
}

static int
xsc_dev_qsfp_eeprom_high_page_offset(int page_num)
{
	/* Page 0 always start from low page */
	if (!page_num)
		return 0;

	/* High page */
	return page_num * XSC_EEPROM_HIGH_PAGE_LENGTH;
}

static void
xsc_dev_qsfp_eeprom_params_set(uint16_t *i2c_addr, uint32_t *page_num, uint16_t *offset)
{
	*i2c_addr = XSC_I2C_ADDR_LOW;
	*page_num = xsc_dev_qsfp_eeprom_page(*offset);
	*offset -=  xsc_dev_qsfp_eeprom_high_page_offset(*page_num);
}

static int
xsc_dev_query_module_id(struct xsc_dev *xdev, uint32_t module_num, uint8_t *module_id)
{
	struct xsc_dev_reg_mcia in = { 0 };
	struct xsc_dev_reg_mcia out = { 0 };
	int ret;
	uint8_t *ptr;

	in.i2c_device_address = XSC_I2C_ADDR_LOW;
	in.module = module_num & 0x000000FF;
	in.device_address = 0;
	in.page_number = 0;
	in.size = 1;

	ret = xsc_dev_access_reg(xdev, &in, sizeof(in), &out, sizeof(out), XSC_REG_MCIA);
	if (ret != 0)
		return ret;

	ptr = out.dword_0;
	*module_id = ptr[0];
	return 0;
}

int
xsc_dev_query_module_eeprom(struct xsc_dev *xdev, uint16_t offset,
			    uint16_t size, uint8_t *data)
{
	struct xsc_module_eeprom_query_params query = { 0 };
	uint8_t module_id;
	int ret;

	query.module_number = xdev->hwinfo.mac_phy_port;
	ret = xsc_dev_query_module_id(xdev, query.module_number, &module_id);
	if (ret != 0)
		return ret;

	switch (module_id) {
	case XSC_MODULE_ID_SFP:
		xsc_dev_sfp_eeprom_params_set(&query.i2c_address, &query.page, &offset);
		break;
	case XSC_MODULE_ID_QSFP:
	case XSC_MODULE_ID_QSFP_PLUS:
	case XSC_MODULE_ID_QSFP28:
	case XSC_MODULE_ID_QSFP_DD:
	case XSC_MODULE_ID_DSFP:
	case XSC_MODULE_ID_QSFP_PLUS_CMIS:
		xsc_dev_qsfp_eeprom_params_set(&query.i2c_address, &query.page, &offset);
		break;
	default:
		PMD_DRV_LOG(ERR, "Module ID not recognized: 0x%x", module_id);
		return -EINVAL;
	}

	if (offset + size > XSC_EEPROM_PAGE_LENGTH)
		/* Cross pages read, read until offset 256 in low page */
		size = XSC_EEPROM_PAGE_LENGTH - offset;

	query.size = size;
	query.offset = offset;

	return xsc_dev_query_mcia(xdev, &query, data);
}

int
xsc_dev_intr_event_get(struct xsc_dev *xdev)
{
	if (xdev->dev_ops->intr_event_get == NULL)
		return -ENOTSUP;

	return xdev->dev_ops->intr_event_get(xdev);
}

int
xsc_dev_intr_handler_install(struct xsc_dev *xdev, rte_intr_callback_fn cb, void *cb_arg)
{
	if (xdev->dev_ops->intr_handler_install == NULL)
		return -ENOTSUP;

	return xdev->dev_ops->intr_handler_install(xdev, cb, cb_arg);
}

int
xsc_dev_intr_handler_uninstall(struct xsc_dev *xdev)
{
	if (xdev->dev_ops->intr_handler_uninstall == NULL)
		return -ENOTSUP;

	return xdev->dev_ops->intr_handler_uninstall(xdev);
}

int
xsc_dev_fec_get(struct xsc_dev *xdev, uint32_t *fec_capa)
{
	struct xsc_cmd_query_fecparam_mbox_in in = { };
	struct xsc_cmd_query_fecparam_mbox_out out = { };
	int ret = 0, fec;

	in.hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_QUERY_FEC_PARAM);
	ret = xsc_dev_mailbox_exec(xdev, &in, sizeof(in), &out, sizeof(out));
	if (ret != 0 || out.hdr.status != 0) {
		PMD_DRV_LOG(ERR, "Failed to get fec, ret=%d, status=%u",
			    ret, out.hdr.status);
		rte_errno = ENOEXEC;
		return -rte_errno;
	}

	fec = rte_be_to_cpu_32(out.active_fec);
	switch (fec) {
	case XSC_DEV_FEC_OFF:
		fec = RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC);
		break;
	case XSC_DEV_FEC_BASER:
		fec = RTE_ETH_FEC_MODE_CAPA_MASK(BASER);
		break;
	case XSC_DEV_FEC_RS:
		fec = RTE_ETH_FEC_MODE_CAPA_MASK(RS);
		break;
	default:
		fec = RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC);
		break;
	}

	*fec_capa = fec;

	return 0;
}

int
xsc_dev_fec_set(struct xsc_dev *xdev, uint32_t mode)
{
	struct xsc_cmd_modify_fecparam_mbox_in in = { };
	struct xsc_cmd_modify_fecparam_mbox_out out = { };
	int ret = 0;

	in.hdr.opcode = rte_cpu_to_be_16(XSC_CMD_OP_MODIFY_FEC_PARAM);
	switch (mode) {
	case RTE_ETH_FEC_MODE_CAPA_MASK(NOFEC):
		mode = XSC_DEV_FEC_OFF;
		break;
	case RTE_ETH_FEC_MODE_CAPA_MASK(AUTO):
		mode = XSC_DEV_FEC_AUTO;
		break;
	case RTE_ETH_FEC_MODE_CAPA_MASK(BASER):
		mode = XSC_DEV_FEC_BASER;
		break;
	case RTE_ETH_FEC_MODE_CAPA_MASK(RS):
		mode = XSC_DEV_FEC_RS;
		break;
	default:
		PMD_DRV_LOG(ERR, "Failed to set fec, fec mode %u not support", mode);
		return 0;
	}

	in.fec = rte_cpu_to_be_32(mode);
	ret = xsc_dev_mailbox_exec(xdev, &in, sizeof(in), &out, sizeof(out));
	if (ret != 0 || out.hdr.status != 0) {
		PMD_DRV_LOG(ERR, "Failed to set fec param, ret=%d, status=%u",
			    ret, out.hdr.status);
		rte_errno = ENOEXEC;
		return -rte_errno;
	}

	return 0;
}

int
xsc_dev_mac_port_init(struct xsc_dev *xdev)
{
	uint32_t i;
	uint8_t num = 0;
	uint32_t mac_port = xdev->hwinfo.mac_phy_port;
	uint8_t mac_bit = xdev->hwinfo.mac_bit;
	uint8_t *mac_idx = &xdev->hwinfo.mac_port_idx;
	uint8_t *mac_num = &xdev->hwinfo.mac_port_num;
	uint8_t bit_sz = sizeof(mac_bit) * 8;

	*mac_idx = 0xff;
	for (i = 0; i < bit_sz; i++) {
		if (mac_bit & (1U << i)) {
			if (i == mac_port)
				*mac_idx = num;
			num++;
		}
	}
	*mac_num = num;

	if (*mac_num == 0 || *mac_idx == 0xff) {
		PMD_DRV_LOG(ERR, "Failed to parse mac port %u index, mac bit 0x%x",
			    mac_port, mac_bit);
		return -1;
	}

	PMD_DRV_LOG(DEBUG, "Mac port num %u, mac port %u, mac index %u",
		    *mac_num, mac_port, *mac_idx);
	return 0;
}
