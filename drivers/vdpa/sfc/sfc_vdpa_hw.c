/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 Xilinx, Inc.
 */

#include <unistd.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_vfio.h>
#include <rte_vhost.h>

#include "efx.h"
#include "sfc_vdpa.h"
#include "sfc_vdpa_ops.h"

extern uint32_t sfc_logtype_driver;

#ifndef PAGE_SIZE
#define PAGE_SIZE   (sysconf(_SC_PAGESIZE))
#endif

int
sfc_vdpa_dma_alloc(struct sfc_vdpa_adapter *sva, const char *name,
		   size_t len, efsys_mem_t *esmp)
{
	uint64_t mcdi_iova;
	size_t mcdi_buff_size;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz = NULL;
	int numa_node = sva->pdev->device.numa_node;
	int ret;

	mcdi_buff_size = RTE_ALIGN_CEIL(len, PAGE_SIZE);
	ret = snprintf(mz_name, RTE_MEMZONE_NAMESIZE, "%s_%s",
		       sva->pdev->name, name);
	if (ret < 0 || ret >= RTE_MEMZONE_NAMESIZE) {
		sfc_vdpa_err(sva, "%s_%s too long to fit in mz_name",
			     sva->pdev->name, name);
		return -EINVAL;
	}

	sfc_vdpa_log_init(sva, "name=%s, len=%zu", mz_name, len);

	mz = rte_memzone_reserve_aligned(mz_name, mcdi_buff_size,
					 numa_node,
					 RTE_MEMZONE_IOVA_CONTIG,
					 PAGE_SIZE);
	if (mz == NULL) {
		sfc_vdpa_err(sva, "cannot reserve memory for %s: len=%#x: %s",
			     mz_name, (unsigned int)len,
			     rte_strerror(rte_errno));
		return -ENOMEM;
	}

	/* IOVA address for MCDI would be re-calculated if mapping
	 * using default IOVA would fail.
	 * TODO: Earlier there was no way to get valid IOVA range.
	 * Recently a patch has been submitted to get the IOVA range
	 * using ioctl. VFIO_IOMMU_GET_INFO. This patch is available
	 * in the kernel version >= 5.4. Support to get the default
	 * IOVA address for MCDI buffer using available IOVA range
	 * would be added later. Meanwhile default IOVA for MCDI buffer
	 * is kept at high mem at 2TB. In case of overlap new available
	 * addresses would be searched and same would be used.
	 */
	mcdi_iova = SFC_VDPA_DEFAULT_MCDI_IOVA;

	for (;;) {
		ret = rte_vfio_container_dma_map(sva->vfio_container_fd,
						 (uint64_t)mz->addr, mcdi_iova,
						 mcdi_buff_size);
		if (ret == 0)
			break;

		mcdi_iova = mcdi_iova >> 1;
		if (mcdi_iova < mcdi_buff_size)	{
			sfc_vdpa_err(sva,
				     "DMA mapping failed for MCDI : %s",
				     rte_strerror(rte_errno));
			rte_memzone_free(mz);
			return ret;
		}
	}

	esmp->esm_addr = mcdi_iova;
	esmp->esm_base = mz->addr;
	sva->mcdi_buff_size = mcdi_buff_size;

	sfc_vdpa_info(sva,
		      "DMA name=%s len=%zu => virt=%p iova=0x%" PRIx64,
		      name, len, esmp->esm_base, esmp->esm_addr);

	return 0;
}

void
sfc_vdpa_dma_free(struct sfc_vdpa_adapter *sva, efsys_mem_t *esmp)
{
	int ret;

	sfc_vdpa_log_init(sva, "name=%s", esmp->esm_mz->name);

	ret = rte_vfio_container_dma_unmap(sva->vfio_container_fd,
					   (uint64_t)esmp->esm_base,
					   esmp->esm_addr, sva->mcdi_buff_size);
	if (ret < 0)
		sfc_vdpa_err(sva, "DMA unmap failed for MCDI : %s",
			     rte_strerror(rte_errno));

	sfc_vdpa_info(sva,
		      "DMA free name=%s => virt=%p iova=0x%" PRIx64,
		      esmp->esm_mz->name, esmp->esm_base, esmp->esm_addr);

	rte_free((void *)(esmp->esm_base));

	sva->mcdi_buff_size = 0;
	memset(esmp, 0, sizeof(*esmp));
}

int
sfc_vdpa_dma_map(struct sfc_vdpa_ops_data *ops_data, bool do_map)
{
	uint32_t i, j;
	int rc;
	struct rte_vhost_memory *vhost_mem = NULL;
	struct rte_vhost_mem_region *mem_reg = NULL;
	int vfio_container_fd;
	void *dev;

	dev = ops_data->dev_handle;
	vfio_container_fd =
		sfc_vdpa_adapter_by_dev_handle(dev)->vfio_container_fd;

	rc = rte_vhost_get_mem_table(ops_data->vid, &vhost_mem);
	if (rc < 0) {
		sfc_vdpa_err(dev,
			     "failed to get VM memory layout");
		goto error;
	}

	for (i = 0; i < vhost_mem->nregions; i++) {
		mem_reg = &vhost_mem->regions[i];

		if (do_map) {
			rc = rte_vfio_container_dma_map(vfio_container_fd,
						mem_reg->host_user_addr,
						mem_reg->guest_phys_addr,
						mem_reg->size);
			if (rc < 0) {
				sfc_vdpa_err(dev,
					     "DMA map failed : %s",
					     rte_strerror(rte_errno));
				goto failed_vfio_dma_map;
			}
		} else {
			rc = rte_vfio_container_dma_unmap(vfio_container_fd,
						mem_reg->host_user_addr,
						mem_reg->guest_phys_addr,
						mem_reg->size);
			if (rc < 0) {
				sfc_vdpa_err(dev,
					     "DMA unmap failed : %s",
					     rte_strerror(rte_errno));
				goto error;
			}
		}
	}

	free(vhost_mem);

	return 0;

failed_vfio_dma_map:
	for (j = 0; j < i; j++) {
		mem_reg = &vhost_mem->regions[j];
		rte_vfio_container_dma_unmap(vfio_container_fd,
					     mem_reg->host_user_addr,
					     mem_reg->guest_phys_addr,
					     mem_reg->size);
	}

error:
	free(vhost_mem);

	return rc;
}

static int
sfc_vdpa_mem_bar_init(struct sfc_vdpa_adapter *sva,
		      const efx_bar_region_t *mem_ebrp)
{
	struct rte_pci_device *pci_dev = sva->pdev;
	efsys_bar_t *ebp = &sva->mem_bar;
	struct rte_mem_resource *res =
		&pci_dev->mem_resource[mem_ebrp->ebr_index];

	SFC_BAR_LOCK_INIT(ebp, pci_dev->name);
	ebp->esb_rid = mem_ebrp->ebr_index;
	ebp->esb_dev = pci_dev;
	ebp->esb_base = res->addr;

	return 0;
}

static void
sfc_vdpa_mem_bar_fini(struct sfc_vdpa_adapter *sva)
{
	efsys_bar_t *ebp = &sva->mem_bar;

	SFC_BAR_LOCK_DESTROY(ebp);
	memset(ebp, 0, sizeof(*ebp));
}

static int
sfc_vdpa_nic_probe(struct sfc_vdpa_adapter *sva)
{
	efx_nic_t *enp = sva->nic;
	int rc;

	rc = efx_nic_probe(enp, EFX_FW_VARIANT_DONT_CARE);
	if (rc != 0)
		sfc_vdpa_err(sva, "nic probe failed: %s", rte_strerror(rc));

	return rc;
}

static int
sfc_vdpa_estimate_resource_limits(struct sfc_vdpa_adapter *sva)
{
	efx_drv_limits_t limits;
	int rc;
	uint32_t evq_allocated;
	uint32_t rxq_allocated;
	uint32_t txq_allocated;
	uint32_t max_queue_cnt;

	memset(&limits, 0, sizeof(limits));

	/* Request at least one Rx and Tx queue */
	limits.edl_min_rxq_count = 1;
	limits.edl_min_txq_count = 1;
	/* Management event queue plus event queue for Tx/Rx queue */
	limits.edl_min_evq_count =
		1 + RTE_MAX(limits.edl_min_rxq_count, limits.edl_min_txq_count);

	limits.edl_max_rxq_count = SFC_VDPA_MAX_QUEUE_PAIRS;
	limits.edl_max_txq_count = SFC_VDPA_MAX_QUEUE_PAIRS;
	limits.edl_max_evq_count = 1 + SFC_VDPA_MAX_QUEUE_PAIRS;

	SFC_VDPA_ASSERT(limits.edl_max_evq_count >= limits.edl_min_rxq_count);
	SFC_VDPA_ASSERT(limits.edl_max_rxq_count >= limits.edl_min_rxq_count);
	SFC_VDPA_ASSERT(limits.edl_max_txq_count >= limits.edl_min_rxq_count);

	/* Configure the minimum required resources needed for the
	 * driver to operate, and the maximum desired resources that the
	 * driver is capable of using.
	 */
	sfc_vdpa_log_init(sva, "set drv limit");
	efx_nic_set_drv_limits(sva->nic, &limits);

	sfc_vdpa_log_init(sva, "init nic");
	rc = efx_nic_init(sva->nic);
	if (rc != 0) {
		sfc_vdpa_err(sva, "nic init failed: %s", rte_strerror(rc));
		goto fail_nic_init;
	}

	/* Find resource dimensions assigned by firmware to this function */
	rc = efx_nic_get_vi_pool(sva->nic, &evq_allocated, &rxq_allocated,
				 &txq_allocated);
	if (rc != 0) {
		sfc_vdpa_err(sva, "vi pool get failed: %s", rte_strerror(rc));
		goto fail_get_vi_pool;
	}

	/* It still may allocate more than maximum, ensure limit */
	evq_allocated = RTE_MIN(evq_allocated, limits.edl_max_evq_count);
	rxq_allocated = RTE_MIN(rxq_allocated, limits.edl_max_rxq_count);
	txq_allocated = RTE_MIN(txq_allocated, limits.edl_max_txq_count);


	max_queue_cnt = RTE_MIN(rxq_allocated, txq_allocated);
	/* Subtract management EVQ not used for traffic */
	max_queue_cnt = RTE_MIN(evq_allocated - 1, max_queue_cnt);

	SFC_VDPA_ASSERT(max_queue_cnt > 0);

	sva->max_queue_count = max_queue_cnt;

	return 0;

fail_get_vi_pool:
	efx_nic_fini(sva->nic);
fail_nic_init:
	sfc_vdpa_log_init(sva, "failed: %s", rte_strerror(rc));
	return rc;
}

int
sfc_vdpa_hw_init(struct sfc_vdpa_adapter *sva)
{
	efx_bar_region_t mem_ebr;
	efx_nic_t *enp;
	int rc;

	sfc_vdpa_log_init(sva, "entry");

	sfc_vdpa_log_init(sva, "get family");
	rc = sfc_efx_family(sva->pdev, &mem_ebr, &sva->family);
	if (rc != 0)
		goto fail_family;
	sfc_vdpa_log_init(sva,
			  "family is %u, membar is %d,"
			  "function control window offset is %#" PRIx64,
			  sva->family, mem_ebr.ebr_index, mem_ebr.ebr_offset);

	sfc_vdpa_log_init(sva, "init mem bar");
	rc = sfc_vdpa_mem_bar_init(sva, &mem_ebr);
	if (rc != 0)
		goto fail_mem_bar_init;

	sfc_vdpa_log_init(sva, "create nic");
	rte_spinlock_init(&sva->nic_lock);
	rc = efx_nic_create(sva->family, (efsys_identifier_t *)sva,
			    &sva->mem_bar, mem_ebr.ebr_offset,
			    &sva->nic_lock, &enp);
	if (rc != 0) {
		sfc_vdpa_err(sva, "nic create failed: %s", rte_strerror(rc));
		goto fail_nic_create;
	}
	sva->nic = enp;

	sfc_vdpa_log_init(sva, "init mcdi");
	rc = sfc_vdpa_mcdi_init(sva);
	if (rc != 0) {
		sfc_vdpa_err(sva, "mcdi init failed: %s", rte_strerror(rc));
		goto fail_mcdi_init;
	}

	sfc_vdpa_log_init(sva, "probe nic");
	rc = sfc_vdpa_nic_probe(sva);
	if (rc != 0)
		goto fail_nic_probe;

	sfc_vdpa_log_init(sva, "reset nic");
	rc = efx_nic_reset(enp);
	if (rc != 0) {
		sfc_vdpa_err(sva, "nic reset failed: %s", rte_strerror(rc));
		goto fail_nic_reset;
	}

	sfc_vdpa_log_init(sva, "estimate resource limits");
	rc = sfc_vdpa_estimate_resource_limits(sva);
	if (rc != 0)
		goto fail_estimate_rsrc_limits;

	sfc_vdpa_log_init(sva, "init virtio");
	rc = efx_virtio_init(enp);
	if (rc != 0) {
		sfc_vdpa_err(sva, "virtio init failed: %s", rte_strerror(rc));
		goto fail_virtio_init;
	}

	sfc_vdpa_log_init(sva, "init filter");
	rc = efx_filter_init(enp);
	if (rc != 0) {
		sfc_vdpa_err(sva, "filter init failed: %s", rte_strerror(rc));
		goto fail_filter_init;
	}

	sfc_vdpa_log_init(sva, "done");

	return 0;

fail_filter_init:
	efx_virtio_fini(enp);

fail_virtio_init:
	efx_nic_fini(enp);

fail_estimate_rsrc_limits:
fail_nic_reset:
	efx_nic_unprobe(enp);

fail_nic_probe:
	sfc_vdpa_mcdi_fini(sva);

fail_mcdi_init:
	sfc_vdpa_log_init(sva, "destroy nic");
	sva->nic = NULL;
	efx_nic_destroy(enp);

fail_nic_create:
	sfc_vdpa_mem_bar_fini(sva);

fail_mem_bar_init:
fail_family:
	sfc_vdpa_log_init(sva, "failed: %s", rte_strerror(rc));
	return rc;
}

void
sfc_vdpa_hw_fini(struct sfc_vdpa_adapter *sva)
{
	efx_nic_t *enp = sva->nic;

	sfc_vdpa_log_init(sva, "entry");

	sfc_vdpa_log_init(sva, "virtio fini");
	efx_virtio_fini(enp);

	sfc_vdpa_log_init(sva, "unprobe nic");
	efx_nic_unprobe(enp);

	sfc_vdpa_log_init(sva, "mcdi fini");
	sfc_vdpa_mcdi_fini(sva);

	sfc_vdpa_log_init(sva, "nic fini");
	efx_nic_fini(enp);

	sfc_vdpa_log_init(sva, "destroy nic");
	sva->nic = NULL;
	efx_nic_destroy(enp);

	sfc_vdpa_mem_bar_fini(sva);
}
