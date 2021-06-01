/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_DEV_H
#define _OTX2_DEV_H

#include <rte_bus_pci.h>

#include "otx2_common.h"
#include "otx2_irq.h"
#include "otx2_mbox.h"
#include "otx2_mempool.h"

/* Common HWCAP flags. Use from LSB bits */
#define OTX2_HWCAP_F_VF		BIT_ULL(8) /* VF device */
#define otx2_dev_is_vf(dev)	(dev->hwcap & OTX2_HWCAP_F_VF)
#define otx2_dev_is_pf(dev)	(!(dev->hwcap & OTX2_HWCAP_F_VF))
#define otx2_dev_is_lbk(dev)	((dev->hwcap & OTX2_HWCAP_F_VF) && \
				 (dev->tx_chan_base < 0x700))
#define otx2_dev_revid(dev)	(dev->hwcap & 0xFF)
#define otx2_dev_is_sdp(dev)	(dev->sdp_link)

#define otx2_dev_is_vf_or_sdp(dev)				\
	(otx2_dev_is_vf(dev) || otx2_dev_is_sdp(dev))

#define otx2_dev_is_A0(dev)					\
	((RVU_PCI_REV_MAJOR(otx2_dev_revid(dev)) == 0x0) &&	\
	 (RVU_PCI_REV_MINOR(otx2_dev_revid(dev)) == 0x0))
#define otx2_dev_is_Ax(dev)					\
	((RVU_PCI_REV_MAJOR(otx2_dev_revid(dev)) == 0x0))

#define otx2_dev_is_95xx_A0(dev)				\
	((RVU_PCI_REV_MAJOR(otx2_dev_revid(dev)) == 0x0) &&	\
	 (RVU_PCI_REV_MINOR(otx2_dev_revid(dev)) == 0x0) &&	\
	 (RVU_PCI_REV_MIDR_ID(otx2_dev_revid(dev)) == 0x1))
#define otx2_dev_is_95xx_Ax(dev)				\
	((RVU_PCI_REV_MAJOR(otx2_dev_revid(dev)) == 0x0) &&	\
	 (RVU_PCI_REV_MIDR_ID(otx2_dev_revid(dev)) == 0x1))

#define otx2_dev_is_96xx_A0(dev)				\
	((RVU_PCI_REV_MAJOR(otx2_dev_revid(dev)) == 0x0) &&	\
	 (RVU_PCI_REV_MINOR(otx2_dev_revid(dev)) == 0x0) &&	\
	 (RVU_PCI_REV_MIDR_ID(otx2_dev_revid(dev)) == 0x0))
#define otx2_dev_is_96xx_Ax(dev)				\
	((RVU_PCI_REV_MAJOR(otx2_dev_revid(dev)) == 0x0) &&	\
	 (RVU_PCI_REV_MIDR_ID(otx2_dev_revid(dev)) == 0x0))

#define otx2_dev_is_96xx_Cx(dev)				\
	((RVU_PCI_REV_MAJOR(otx2_dev_revid(dev)) == 0x2) &&	\
	 (RVU_PCI_REV_MIDR_ID(otx2_dev_revid(dev)) == 0x0))

#define otx2_dev_is_96xx_C0(dev)				\
	((RVU_PCI_REV_MAJOR(otx2_dev_revid(dev)) == 0x2) &&	\
	 (RVU_PCI_REV_MINOR(otx2_dev_revid(dev)) == 0x0) &&	\
	 (RVU_PCI_REV_MIDR_ID(otx2_dev_revid(dev)) == 0x0))

#define otx2_dev_is_98xx(dev)                                   \
	 (RVU_PCI_REV_MIDR_ID(otx2_dev_revid(dev)) == 0x3)

struct otx2_dev;

/* Link status callback */
typedef void (*otx2_link_status_t)(struct otx2_dev *dev,
				   struct cgx_link_user_info *link);
/* PTP info callback */
typedef int (*otx2_ptp_info_t)(struct otx2_dev *dev, bool ptp_en);

struct otx2_dev_ops {
	otx2_link_status_t link_status_update;
	otx2_ptp_info_t ptp_info_update;
};

#define OTX2_DEV					\
	int node __rte_cache_aligned;			\
	uint16_t pf;					\
	int16_t vf;					\
	uint16_t pf_func;				\
	uint8_t mbox_active;				\
	bool drv_inited;				\
	uint64_t active_vfs[MAX_VFPF_DWORD_BITS];	\
	uintptr_t bar2;					\
	uintptr_t bar4;					\
	struct otx2_mbox mbox_local;			\
	struct otx2_mbox mbox_up;			\
	struct otx2_mbox mbox_vfpf;			\
	struct otx2_mbox mbox_vfpf_up;			\
	otx2_intr_t intr;				\
	int timer_set;	/* ~0 : no alarm handling */	\
	uint64_t hwcap;					\
	struct otx2_npa_lf npalf;			\
	struct otx2_mbox *mbox;				\
	uint16_t maxvf;					\
	const struct otx2_dev_ops *ops

struct otx2_dev {
	OTX2_DEV;
};

__rte_internal
int otx2_dev_priv_init(struct rte_pci_device *pci_dev, void *otx2_dev);

/* Common dev init and fini routines */

static __rte_always_inline int
otx2_dev_init(struct rte_pci_device *pci_dev, void *otx2_dev)
{
	struct otx2_dev *dev = otx2_dev;
	uint8_t rev_id;
	int rc;

	rc = rte_pci_read_config(pci_dev, &rev_id,
				 1, RVU_PCI_REVISION_ID);
	if (rc != 1) {
		otx2_err("Failed to read pci revision id, rc=%d", rc);
		return rc;
	}

	dev->hwcap = rev_id;
	return otx2_dev_priv_init(pci_dev, otx2_dev);
}

__rte_internal
void otx2_dev_fini(struct rte_pci_device *pci_dev, void *otx2_dev);
__rte_internal
int otx2_dev_active_vfs(void *otx2_dev);

#define RVU_PFVF_PF_SHIFT	10
#define RVU_PFVF_PF_MASK	0x3F
#define RVU_PFVF_FUNC_SHIFT	0
#define RVU_PFVF_FUNC_MASK	0x3FF

static inline int
otx2_get_vf(uint16_t pf_func)
{
	return (((pf_func >> RVU_PFVF_FUNC_SHIFT) & RVU_PFVF_FUNC_MASK) - 1);
}

static inline int
otx2_get_pf(uint16_t pf_func)
{
	return (pf_func >> RVU_PFVF_PF_SHIFT) & RVU_PFVF_PF_MASK;
}

static inline int
otx2_pfvf_func(int pf, int vf)
{
	return (pf << RVU_PFVF_PF_SHIFT) | ((vf << RVU_PFVF_FUNC_SHIFT) + 1);
}

static inline int
otx2_is_afvf(uint16_t pf_func)
{
	return !(pf_func & ~RVU_PFVF_FUNC_MASK);
}

#endif /* _OTX2_DEV_H */
