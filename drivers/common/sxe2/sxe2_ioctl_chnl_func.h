/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_IOCTL_CHNL_FUNC_H
#define SXE2_IOCTL_CHNL_FUNC_H

#include <rte_version.h>
#include <bus_pci_driver.h>

#include "sxe2_common.h"
#include "sxe2_ioctl_chnl.h"

#ifdef	__cplusplus
extern "C" {
#endif

__rte_internal
void
sxe2_drv_cmd_close(struct sxe2_common_device *cdev);

__rte_internal
int32_t
sxe2_drv_cmd_exec(struct sxe2_common_device *cdev,
		struct sxe2_drv_cmd_params *cmd_params);

__rte_internal
int32_t
sxe2_drv_dev_open(struct sxe2_common_device *cdev,
		struct rte_pci_device *pci_dev);


__rte_internal
void
sxe2_drv_dev_close(struct sxe2_common_device *cdev);

__rte_internal
int32_t
sxe2_drv_dev_handshake(struct sxe2_common_device *cdev);

__rte_internal
int32_t
sxe2_drv_dev_rxq_irq_set(struct sxe2_common_device *cdev, uint16_t base_irq,
			 int32_t *efd, uint16_t nb_irq);

__rte_internal
int32_t
sxe2_drv_dev_other_irq_set(struct sxe2_common_device *cdev, int32_t efd, uint64_t event);

__rte_internal
int32_t
sxe2_drv_dev_other_irq_get(struct sxe2_common_device *cdev, uint64_t *event);

__rte_internal
int32_t
sxe2_drv_dev_reset_irq_set(struct sxe2_common_device *cdev, int32_t fd);

__rte_internal
void
*sxe2_drv_dev_mmap(struct sxe2_common_device *cdev, uint8_t bar_idx,
		uint64_t len, uint64_t offset);

__rte_internal
int32_t
sxe2_drv_dev_munmap(struct sxe2_common_device *cdev, void *virt, uint64_t len);

__rte_internal
int32_t
sxe2_drv_dev_dma_map(struct sxe2_common_device *cdev, uint64_t vaddr,
		uint64_t iova, uint64_t size);

__rte_internal
int32_t
sxe2_drv_dev_dma_unmap(struct sxe2_common_device *cdev, uint64_t iova);

#ifdef __cplusplus
}
#endif

#endif /* SXE2_IOCTL_CHNL_FUNC_H */
