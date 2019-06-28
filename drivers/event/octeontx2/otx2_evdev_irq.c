/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_evdev.h"

static void
sso_lf_irq(void *param)
{
	uintptr_t base = (uintptr_t)param;
	uint64_t intr;
	uint8_t ggrp;

	ggrp = (base >> 12) & 0xFF;

	intr = otx2_read64(base + SSO_LF_GGRP_INT);
	if (intr == 0)
		return;

	otx2_err("GGRP %d GGRP_INT=0x%" PRIx64 "", ggrp, intr);

	/* Clear interrupt */
	otx2_write64(intr, base + SSO_LF_GGRP_INT);
}

static int
sso_lf_register_irq(const struct rte_eventdev *event_dev, uint16_t ggrp_msixoff,
		    uintptr_t base)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(event_dev->dev);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	int rc, vec;

	vec = ggrp_msixoff + SSO_LF_INT_VEC_GRP;

	/* Clear err interrupt */
	otx2_write64(~0ull, base + SSO_LF_GGRP_INT_ENA_W1C);
	/* Set used interrupt vectors */
	rc = otx2_register_irq(handle, sso_lf_irq, (void *)base, vec);
	/* Enable hw interrupt */
	otx2_write64(~0ull, base + SSO_LF_GGRP_INT_ENA_W1S);

	return rc;
}

static void
ssow_lf_irq(void *param)
{
	uintptr_t base = (uintptr_t)param;
	uint8_t gws = (base >> 12) & 0xFF;
	uint64_t intr;

	intr = otx2_read64(base + SSOW_LF_GWS_INT);
	if (intr == 0)
		return;

	otx2_err("GWS %d GWS_INT=0x%" PRIx64 "", gws, intr);

	/* Clear interrupt */
	otx2_write64(intr, base + SSOW_LF_GWS_INT);
}

static int
ssow_lf_register_irq(const struct rte_eventdev *event_dev, uint16_t gws_msixoff,
		     uintptr_t base)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(event_dev->dev);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	int rc, vec;

	vec = gws_msixoff + SSOW_LF_INT_VEC_IOP;

	/* Clear err interrupt */
	otx2_write64(~0ull, base + SSOW_LF_GWS_INT_ENA_W1C);
	/* Set used interrupt vectors */
	rc = otx2_register_irq(handle, ssow_lf_irq, (void *)base, vec);
	/* Enable hw interrupt */
	otx2_write64(~0ull, base + SSOW_LF_GWS_INT_ENA_W1S);

	return rc;
}

static void
sso_lf_unregister_irq(const struct rte_eventdev *event_dev,
		      uint16_t ggrp_msixoff, uintptr_t base)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(event_dev->dev);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	int vec;

	vec = ggrp_msixoff + SSO_LF_INT_VEC_GRP;

	/* Clear err interrupt */
	otx2_write64(~0ull, base + SSO_LF_GGRP_INT_ENA_W1C);
	otx2_unregister_irq(handle, sso_lf_irq, (void *)base, vec);
}

static void
ssow_lf_unregister_irq(const struct rte_eventdev *event_dev,
		       uint16_t gws_msixoff, uintptr_t base)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(event_dev->dev);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	int vec;

	vec = gws_msixoff + SSOW_LF_INT_VEC_IOP;

	/* Clear err interrupt */
	otx2_write64(~0ull, base + SSOW_LF_GWS_INT_ENA_W1C);
	otx2_unregister_irq(handle, ssow_lf_irq, (void *)base, vec);
}

int
sso_register_irqs(const struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	int i, rc = -EINVAL;
	uint8_t nb_ports;

	nb_ports = dev->nb_event_ports;

	for (i = 0; i < dev->nb_event_queues; i++) {
		if (dev->sso_msixoff[i] == MSIX_VECTOR_INVALID) {
			otx2_err("Invalid SSOLF MSIX offset[%d] vector: 0x%x",
				 i, dev->sso_msixoff[i]);
			goto fail;
		}
	}

	for (i = 0; i < nb_ports; i++) {
		if (dev->ssow_msixoff[i] == MSIX_VECTOR_INVALID) {
			otx2_err("Invalid SSOWLF MSIX offset[%d] vector: 0x%x",
				 i, dev->ssow_msixoff[i]);
			goto fail;
		}
	}

	for (i = 0; i < dev->nb_event_queues; i++) {
		uintptr_t base = dev->bar2 + (RVU_BLOCK_ADDR_SSO << 20 |
					      i << 12);
		rc = sso_lf_register_irq(event_dev, dev->sso_msixoff[i], base);
	}

	for (i = 0; i < nb_ports; i++) {
		uintptr_t base = dev->bar2 + (RVU_BLOCK_ADDR_SSOW << 20 |
					      i << 12);
		rc = ssow_lf_register_irq(event_dev, dev->ssow_msixoff[i],
					  base);
	}

fail:
	return rc;
}

void
sso_unregister_irqs(const struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	uint8_t nb_ports;
	int i;

	nb_ports = dev->nb_event_ports;

	for (i = 0; i < dev->nb_event_queues; i++) {
		uintptr_t base = dev->bar2 + (RVU_BLOCK_ADDR_SSO << 20 |
					      i << 12);
		sso_lf_unregister_irq(event_dev, dev->sso_msixoff[i], base);
	}

	for (i = 0; i < nb_ports; i++) {
		uintptr_t base = dev->bar2 + (RVU_BLOCK_ADDR_SSOW << 20 |
					      i << 12);
		ssow_lf_unregister_irq(event_dev, dev->ssow_msixoff[i], base);
	}
}
