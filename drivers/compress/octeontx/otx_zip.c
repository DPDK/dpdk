/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#include "otx_zip.h"

uint64_t
zip_reg_read64(uint8_t *hw_addr, uint64_t offset)
{
	uint8_t *base = hw_addr;
	return *(volatile uint64_t *)(base + offset);
}

void
zip_reg_write64(uint8_t *hw_addr, uint64_t offset, uint64_t val)
{
	uint8_t *base = hw_addr;
	*(uint64_t *)(base + offset) = val;
}

int
zipvf_create(struct rte_compressdev *compressdev)
{
	struct   rte_pci_device *pdev = RTE_DEV_TO_PCI(compressdev->device);
	struct   zip_vf *zipvf = NULL;
	char     *dev_name = compressdev->data->name;
	void     *vbar0;
	uint64_t reg;

	if (pdev->mem_resource[0].phys_addr == 0ULL)
		return -EIO;

	vbar0 = pdev->mem_resource[0].addr;
	if (!vbar0) {
		ZIP_PMD_ERR("Failed to map BAR0 of %s", dev_name);
		return -ENODEV;
	}

	zipvf = (struct zip_vf *)(compressdev->data->dev_private);

	if (!zipvf)
		return -ENOMEM;

	zipvf->vbar0 = vbar0;
	reg = zip_reg_read64(zipvf->vbar0, ZIP_VF_PF_MBOXX(0));
	/* Storing domain in local to ZIP VF */
	zipvf->dom_sdom = reg;
	zipvf->pdev = pdev;
	zipvf->max_nb_queue_pairs = ZIP_MAX_VF_QUEUE;
	return 0;
}

int
zipvf_destroy(struct rte_compressdev *compressdev)
{
	struct zip_vf *vf = (struct zip_vf *)(compressdev->data->dev_private);

	/* Rewriting the domain_id in ZIP_VF_MBOX for app rerun */
	zip_reg_write64(vf->vbar0, ZIP_VF_PF_MBOXX(0), vf->dom_sdom);

	return 0;
}
