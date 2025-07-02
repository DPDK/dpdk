/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#include <rte_ethdev.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_ether.h>
#include <rte_ethdev_core.h>
#include "hinic3_mml_lib.h"
#include "hinic3_dbg.h"
#include "hinic3_compat.h"
#include "hinic3_csr.h"
#include "hinic3_hwdev.h"
#include "hinic3_nic_cfg.h"
#include "hinic3_ethdev.h"

static int
get_tx_info(struct rte_eth_dev *dev, void *buf_in, uint16_t in_size,
	    void *buf_out, uint16_t *out_size)
{
	uint16_t q_id = *((uint16_t *)buf_in);
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	if (in_size != sizeof(int))
		return -UDA_EINVAL;

	return hinic3_dbg_get_sq_info(nic_dev->hwdev, q_id, buf_out, out_size);
}

static int
get_tx_wqe_info(struct rte_eth_dev *dev, void *buf_in, uint16_t in_size,
		void *buf_out, uint16_t *out_size)
{
	struct hinic_wqe_info *wqe_info = (struct hinic_wqe_info *)buf_in;
	uint16_t q_id = (uint16_t)wqe_info->q_id;
	uint16_t idx = (uint16_t)wqe_info->wqe_id;
	uint16_t wqebb_cnt = (uint16_t)wqe_info->wqebb_cnt;
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	if (in_size != sizeof(struct hinic_wqe_info))
		return -UDA_EINVAL;

	return hinic3_dbg_get_sq_wqe_info(nic_dev->hwdev, q_id, idx, wqebb_cnt,
					  buf_out, out_size);
}

static int
get_rx_info(struct rte_eth_dev *dev, void *buf_in, uint16_t in_size,
	    void *buf_out, uint16_t *out_size)
{
	uint16_t q_id = *((uint16_t *)buf_in);
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	if (in_size != sizeof(int))
		return -UDA_EINVAL;

	return hinic3_dbg_get_rq_info(nic_dev->hwdev, q_id, buf_out, out_size);
}

static int
get_rx_wqe_info(struct rte_eth_dev *dev, void *buf_in, uint16_t in_size,
		void *buf_out, uint16_t *out_size)
{
	struct hinic_wqe_info *wqe_info = (struct hinic_wqe_info *)buf_in;
	uint16_t q_id = (uint16_t)wqe_info->q_id;
	uint16_t idx = (uint16_t)wqe_info->wqe_id;
	uint16_t wqebb_cnt = (uint16_t)wqe_info->wqebb_cnt;
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	if (in_size != sizeof(struct hinic_wqe_info))
		return -UDA_EINVAL;

	return hinic3_dbg_get_rq_wqe_info(nic_dev->hwdev, q_id, idx, wqebb_cnt,
					  buf_out, out_size);
}

static int
get_rx_cqe_info(struct rte_eth_dev *dev, void *buf_in, uint16_t in_size,
		void *buf_out, uint16_t *out_size)
{
	struct hinic_wqe_info *wqe_info = (struct hinic_wqe_info *)buf_in;
	uint16_t q_id = (uint16_t)wqe_info->q_id;
	uint16_t idx = (uint16_t)wqe_info->wqe_id;
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	if (in_size != sizeof(struct hinic_wqe_info))
		return -UDA_EINVAL;

	return hinic3_dbg_get_rx_cqe_info(nic_dev->hwdev, q_id, idx, buf_out,
					  out_size);
}

typedef int (*nic_drv_module)(struct rte_eth_dev *dev, void *buf_in,
			      uint16_t in_size, void *buf_out,
			      uint16_t *out_size);

struct nic_drv_module_handle {
	enum driver_cmd_type drv_cmd_name;
	nic_drv_module drv_func;
};

const struct nic_drv_module_handle g_nic_drv_module_cmd_handle[] = {
	{TX_INFO, get_tx_info},		{TX_WQE_INFO, get_tx_wqe_info},
	{RX_INFO, get_rx_info},		{RX_WQE_INFO, get_rx_wqe_info},
	{RX_CQE_INFO, get_rx_cqe_info},
};

static int
send_to_nic_driver(struct rte_eth_dev *dev, struct msg_module *nt_msg)
{
	int index;
	int err = 0;
	enum driver_cmd_type cmd_type =
		(enum driver_cmd_type)nt_msg->msg_format;
	int num_cmds = sizeof(g_nic_drv_module_cmd_handle) /
		       sizeof(g_nic_drv_module_cmd_handle[0]);

	for (index = 0; index < num_cmds; index++) {
		if (cmd_type ==
		    g_nic_drv_module_cmd_handle[index].drv_cmd_name) {
			err = g_nic_drv_module_cmd_handle[index].drv_func(dev,
				nt_msg->in_buf,
				(uint16_t)nt_msg->buf_in_size, nt_msg->out_buf,
				(uint16_t *)&nt_msg->buf_out_size);
			break;
		}
	}

	if (index == num_cmds) {
		PMD_DRV_LOG(ERR, "Unknown nic driver cmd: %d", cmd_type);
		err = -UDA_EINVAL;
	}

	return err;
}

static int
hinic3_msg_handle(struct rte_eth_dev *dev, struct msg_module *nt_msg)
{
	int err;

	switch (nt_msg->module) {
	case SEND_TO_NIC_DRIVER:
		err = send_to_nic_driver(dev, nt_msg);
		if (err != 0)
			PMD_DRV_LOG(ERR, "Send message to driver failed");
		break;
	default:
		PMD_DRV_LOG(ERR, "Unknown message module: %d", nt_msg->module);
		err = -UDA_EINVAL;
		break;
	}

	return err;
}

static struct rte_eth_dev *
get_eth_dev_by_pci_addr(char *pci_addr, __rte_unused int len)
{
	uint32_t i;
	struct rte_eth_dev *eth_dev = NULL;
	struct rte_pci_device *pci_dev = NULL;
	int ret;
	uint32_t bus, devid, function;

	ret = sscanf(pci_addr, "%02x:%02x.%x", &bus, &devid, &function);
	if (ret <= 0) {
		PMD_DRV_LOG(ERR,
			    "Get pci bus devid and function id fail, err: %d",
			    ret);
		return NULL;
	}

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		eth_dev = &rte_eth_devices[i];
		if (eth_dev->state != RTE_ETH_DEV_ATTACHED)
			continue;

		pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

#ifdef CONFIG_SP_VID_DID
		if (pci_dev->id.vendor_id == PCI_VENDOR_ID_SPNIC &&
		    (pci_dev->id.device_id == HINIC3_DEV_ID_STANDARD ||
		     pci_dev->id.device_id == HINIC3_DEV_ID_VF) &&
#else
		if (pci_dev->id.vendor_id == PCI_VENDOR_ID_HUAWEI &&
		    (pci_dev->id.device_id == HINIC3_DEV_ID_STANDARD ||
		     pci_dev->id.device_id == HINIC3_DEV_ID_VF) &&
#endif
		    pci_dev->addr.bus == bus && pci_dev->addr.devid == devid &&
		    pci_dev->addr.function == function) {
			return eth_dev;
		}
	}

	return NULL;
}

int
hinic3_pmd_mml_ioctl(void *msg)
{
	struct msg_module *nt_msg = msg;
	struct rte_eth_dev *dev;

	dev = get_eth_dev_by_pci_addr(nt_msg->device_name,
				      sizeof(nt_msg->device_name));
	if (!dev) {
		PMD_DRV_LOG(ERR, "Can not get the device %s correctly",
			    nt_msg->device_name);
		return UDA_FAIL;
	}

	return hinic3_msg_handle(dev, nt_msg);
}
