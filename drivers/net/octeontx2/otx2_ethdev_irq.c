/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <inttypes.h>

#include <rte_bus_pci.h>

#include "otx2_ethdev.h"

static void
nix_lf_err_irq(void *param)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)param;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint64_t intr;

	intr = otx2_read64(dev->base + NIX_LF_ERR_INT);
	if (intr == 0)
		return;

	otx2_err("Err_intr=0x%" PRIx64 " pf=%d, vf=%d", intr, dev->pf, dev->vf);

	/* Clear interrupt */
	otx2_write64(intr, dev->base + NIX_LF_ERR_INT);
}

static int
nix_lf_register_err_irq(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int rc, vec;

	vec = dev->nix_msixoff + NIX_LF_INT_VEC_ERR_INT;

	/* Clear err interrupt */
	otx2_write64(~0ull, dev->base + NIX_LF_ERR_INT_ENA_W1C);
	/* Set used interrupt vectors */
	rc = otx2_register_irq(handle, nix_lf_err_irq, eth_dev, vec);
	/* Enable all dev interrupt except for RQ_DISABLED */
	otx2_write64(~BIT_ULL(11), dev->base + NIX_LF_ERR_INT_ENA_W1S);

	return rc;
}

static void
nix_lf_unregister_err_irq(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int vec;

	vec = dev->nix_msixoff + NIX_LF_INT_VEC_ERR_INT;

	/* Clear err interrupt */
	otx2_write64(~0ull, dev->base + NIX_LF_ERR_INT_ENA_W1C);
	otx2_unregister_irq(handle, nix_lf_err_irq, eth_dev, vec);
}

static void
nix_lf_ras_irq(void *param)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)param;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint64_t intr;

	intr = otx2_read64(dev->base + NIX_LF_RAS);
	if (intr == 0)
		return;

	otx2_err("Ras_intr=0x%" PRIx64 " pf=%d, vf=%d", intr, dev->pf, dev->vf);

	/* Clear interrupt */
	otx2_write64(intr, dev->base + NIX_LF_RAS);
}

static int
nix_lf_register_ras_irq(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int rc, vec;

	vec = dev->nix_msixoff + NIX_LF_INT_VEC_POISON;

	/* Clear err interrupt */
	otx2_write64(~0ull, dev->base + NIX_LF_RAS_ENA_W1C);
	/* Set used interrupt vectors */
	rc = otx2_register_irq(handle, nix_lf_ras_irq, eth_dev, vec);
	/* Enable dev interrupt */
	otx2_write64(~0ull, dev->base + NIX_LF_RAS_ENA_W1S);

	return rc;
}

static void
nix_lf_unregister_ras_irq(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct rte_intr_handle *handle = &pci_dev->intr_handle;
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int vec;

	vec = dev->nix_msixoff + NIX_LF_INT_VEC_POISON;

	/* Clear err interrupt */
	otx2_write64(~0ull, dev->base + NIX_LF_RAS_ENA_W1C);
	otx2_unregister_irq(handle, nix_lf_ras_irq, eth_dev, vec);
}

int
otx2_nix_register_irqs(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int rc;

	if (dev->nix_msixoff == MSIX_VECTOR_INVALID) {
		otx2_err("Invalid NIXLF MSIX vector offset vector: 0x%x",
			 dev->nix_msixoff);
		return -EINVAL;
	}

	/* Register lf err interrupt */
	rc = nix_lf_register_err_irq(eth_dev);
	/* Register RAS interrupt */
	rc |= nix_lf_register_ras_irq(eth_dev);

	return rc;
}

void
otx2_nix_unregister_irqs(struct rte_eth_dev *eth_dev)
{
	nix_lf_unregister_err_irq(eth_dev);
	nix_lf_unregister_ras_irq(eth_dev);
}
