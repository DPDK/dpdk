/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _QAT_COMP_PMD_H_
#define _QAT_COMP_PMD_H_

#include "qat_device.h"


/**< Intel(R) QAT Compression PMD device name */
#define COMPRESSDEV_NAME_QAT_PMD	comp_qat


/** private data structure for a QAT compression device.
 * This QAT device is a device offering only a compression service,
 * there can be one of these on each qat_pci_device (VF).
 */
struct qat_comp_dev_private {
	struct qat_pci_device *qat_dev;
	/**< The qat pci device hosting the service */
};

int
qat_comp_dev_create(struct qat_pci_device *qat_pci_dev);

int
qat_comp_dev_destroy(struct qat_pci_device *qat_pci_dev);
#endif /* _QAT_COMP_PMD_H_ */
