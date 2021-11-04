/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <rte_dev.h>
#include <rte_pci.h>

#include "qat_device.h"
#include "qat_qp.h"
#include "adf_transport_access_macros_gen4vf.h"
#include "adf_pf2vf_msg.h"
#include "qat_pf2vf.h"
#include "qat_dev_gens.h"

#include <stdint.h>

struct qat_dev_gen4_extra {
	struct qat_qp_hw_data qp_gen4_data[QAT_GEN4_BUNDLE_NUM]
		[QAT_GEN4_QPS_PER_BUNDLE_NUM];
};

static struct qat_pf2vf_dev qat_pf2vf_gen4 = {
	.pf2vf_offset = ADF_4XXXIOV_PF2VM_OFFSET,
	.vf2pf_offset = ADF_4XXXIOV_VM2PF_OFFSET,
	.pf2vf_type_shift = ADF_PFVF_2X_MSGTYPE_SHIFT,
	.pf2vf_type_mask = ADF_PFVF_2X_MSGTYPE_MASK,
	.pf2vf_data_shift = ADF_PFVF_2X_MSGDATA_SHIFT,
	.pf2vf_data_mask = ADF_PFVF_2X_MSGDATA_MASK,
};

int
qat_query_svc_gen4(struct qat_pci_device *qat_dev, uint8_t *val)
{
	struct qat_pf2vf_msg pf2vf_msg;

	pf2vf_msg.msg_type = ADF_VF2PF_MSGTYPE_GET_SMALL_BLOCK_REQ;
	pf2vf_msg.block_hdr = ADF_VF2PF_BLOCK_MSG_GET_RING_TO_SVC_REQ;
	pf2vf_msg.msg_data = 2;
	return qat_pf2vf_exch_msg(qat_dev, pf2vf_msg, 2, val);
}

static enum qat_service_type
gen4_pick_service(uint8_t hw_service)
{
	switch (hw_service) {
	case QAT_SVC_SYM:
		return QAT_SERVICE_SYMMETRIC;
	case QAT_SVC_COMPRESSION:
		return QAT_SERVICE_COMPRESSION;
	case QAT_SVC_ASYM:
		return QAT_SERVICE_ASYMMETRIC;
	default:
		return QAT_SERVICE_INVALID;
	}
}

static int
qat_dev_read_config_gen4(struct qat_pci_device *qat_dev)
{
	int i = 0;
	uint16_t svc = 0;
	struct qat_dev_gen4_extra *dev_extra = qat_dev->dev_private;
	struct qat_qp_hw_data *hw_data;
	enum qat_service_type service_type;
	uint8_t hw_service;

	if (qat_query_svc_gen4(qat_dev, (uint8_t *)&svc))
		return -EFAULT;
	for (; i < QAT_GEN4_BUNDLE_NUM; i++) {
		hw_service = (svc >> (3 * i)) & 0x7;
		service_type = gen4_pick_service(hw_service);
		if (service_type == QAT_SERVICE_INVALID) {
			QAT_LOG(ERR,
				"Unrecognized service on bundle %d",
				i);
			return -ENOTSUP;
		}
		hw_data = &dev_extra->qp_gen4_data[i][0];
		memset(hw_data, 0, sizeof(*hw_data));
		hw_data->service_type = service_type;
		if (service_type == QAT_SERVICE_ASYMMETRIC) {
			hw_data->tx_msg_size = 64;
			hw_data->rx_msg_size = 32;
		} else if (service_type == QAT_SERVICE_SYMMETRIC ||
				service_type ==
					QAT_SERVICE_COMPRESSION) {
			hw_data->tx_msg_size = 128;
			hw_data->rx_msg_size = 32;
		}
		hw_data->tx_ring_num = 0;
		hw_data->rx_ring_num = 1;
		hw_data->hw_bundle_num = i;
	}
	return 0;
}

static int
qat_reset_ring_pairs_gen4(struct qat_pci_device *qat_pci_dev)
{
	int ret = 0, i;
	uint8_t data[4];
	struct qat_pf2vf_msg pf2vf_msg;

	pf2vf_msg.msg_type = ADF_VF2PF_MSGTYPE_RP_RESET;
	pf2vf_msg.block_hdr = -1;
	for (i = 0; i < QAT_GEN4_BUNDLE_NUM; i++) {
		pf2vf_msg.msg_data = i;
		ret = qat_pf2vf_exch_msg(qat_pci_dev, pf2vf_msg, 1, data);
		if (ret) {
			QAT_LOG(ERR, "QAT error when reset bundle no %d",
				i);
			return ret;
		}
	}

	return 0;
}

static const struct
rte_mem_resource *qat_dev_get_transport_bar_gen4(struct rte_pci_device *pci_dev)
{
	return &pci_dev->mem_resource[0];
}

static int
qat_dev_get_misc_bar_gen4(struct rte_mem_resource **mem_resource,
		struct rte_pci_device *pci_dev)
{
	*mem_resource = &pci_dev->mem_resource[2];
	return 0;
}

static int
qat_dev_get_extra_size_gen4(void)
{
	return sizeof(struct qat_dev_gen4_extra);
}

static struct qat_dev_hw_spec_funcs qat_dev_hw_spec_gen4 = {
	.qat_dev_reset_ring_pairs = qat_reset_ring_pairs_gen4,
	.qat_dev_get_transport_bar = qat_dev_get_transport_bar_gen4,
	.qat_dev_get_misc_bar = qat_dev_get_misc_bar_gen4,
	.qat_dev_read_config = qat_dev_read_config_gen4,
	.qat_dev_get_extra_size = qat_dev_get_extra_size_gen4,
};

RTE_INIT(qat_dev_gen_4_init)
{
	qat_dev_hw_spec[QAT_GEN4] = &qat_dev_hw_spec_gen4;
	qat_gen_config[QAT_GEN4].dev_gen = QAT_GEN4;
	qat_gen_config[QAT_GEN4].pf2vf_dev = &qat_pf2vf_gen4;
}
