/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include "qat_device.h"

#ifndef QAT_PF2VF_H_
#define QAT_PF2VF_H_

struct qat_pf2vf_msg {
	uint32_t msg_data;
	int block_hdr;
	uint16_t msg_type;
};

int qat_pf2vf_exch_msg(struct qat_pci_device *qat_dev,
	struct qat_pf2vf_msg pf2vf_msg,	int len, uint8_t *ret);

#endif
