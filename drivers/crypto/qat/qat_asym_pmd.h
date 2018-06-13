/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _QAT_ASYM_PMD_H_
#define _QAT_ASYM_PMD_H_

#include "qat_device.h"

int
qat_asym_dev_create(struct qat_pci_device *qat_pci_dev);

int
qat_asym_dev_destroy(struct qat_pci_device *qat_pci_dev);
#endif /* _QAT_ASYM_PMD_H_ */
