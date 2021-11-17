/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Xilinx, Inc.
 */

#include <rte_mempool.h>
#include <rte_memzone.h>

#include "efx.h"

#include "sfc_log.h"
#include "sfc.h"
#include "sfc_nic_dma.h"

static int
sfc_nic_dma_add_region(struct sfc_nic_dma_info *nic_dma_info,
		       rte_iova_t nic_base, rte_iova_t trgt_base,
		       size_t map_len)
{
	struct sfc_nic_dma_region *region;

	if (nic_dma_info->nb_regions >= RTE_DIM(nic_dma_info->regions))
		return ENOMEM;

	region = &nic_dma_info->regions[nic_dma_info->nb_regions];
	region->nic_base = nic_base;
	region->trgt_base = trgt_base;
	region->trgt_end = trgt_base + map_len;

	nic_dma_info->nb_regions++;
	return 0;
}

/*
 * Register mapping for all IOVA mempools at the time of creation to
 * have mapping for all mbufs.
 */

struct sfc_nic_dma_register_mempool_data {
	struct sfc_adapter		*sa;
	int				rc;
};

static void
sfc_nic_dma_register_mempool_chunk(struct rte_mempool *mp __rte_unused,
				   void *opaque,
				   struct rte_mempool_memhdr *memhdr,
				   unsigned mem_idx __rte_unused)
{
	struct sfc_nic_dma_register_mempool_data *register_data = opaque;
	struct sfc_adapter *sa = register_data->sa;
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	efsys_dma_addr_t nic_base;
	efsys_dma_addr_t trgt_base;
	size_t map_len;
	int rc;

	if (memhdr->iova == RTE_BAD_IOVA)
		return;

	/*
	 * Check if the memory chunk is mapped already. In that case, there's
	 * nothing left to do.
	 */
	nic_base = sfc_nic_dma_map(&sas->nic_dma_info, memhdr->iova,
				   memhdr->len);
	if (nic_base != RTE_BAD_IOVA)
		return;

	rc = efx_nic_dma_config_add(sa->nic, memhdr->iova, memhdr->len,
				    &nic_base, &trgt_base, &map_len);
	if (rc != 0) {
		sfc_err(sa,
			"cannot handle memory buffer VA=%p IOVA=%" PRIx64 " length=0x%" PRIx64 ": %s",
			memhdr->addr, (uint64_t)memhdr->iova, memhdr->len,
			rte_strerror(rc));
		register_data->rc = rc;
		return;
	}

	sfc_info(sa,
		 "registered memory buffer VA=%p IOVA=%" PRIx64 " length=0x%" PRIx64 " -> NIC_BASE=%" PRIx64 " TRGT_BASE=%" PRIx64 " MAP_LEN=%" PRIx64,
		 memhdr->addr, (uint64_t)memhdr->iova, memhdr->len,
		 (uint64_t)nic_base, (uint64_t)trgt_base, (uint64_t)map_len);

	rc = sfc_nic_dma_add_region(&sas->nic_dma_info, nic_base, trgt_base,
				    map_len);
	if (rc != 0) {
		sfc_err(sa, "failed to add regioned NIC DMA mapping: %s",
			rte_strerror(rc));
		register_data->rc = rc;
	}
}

static int
sfc_nic_dma_register_mempool(struct sfc_adapter *sa, struct rte_mempool *mp)
{
	struct sfc_nic_dma_register_mempool_data register_data = {
		.sa = sa,
	};
	uint32_t iters;
	int result = 0;
	int rc;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	if (mp->flags & RTE_MEMPOOL_F_NON_IO)
		return 0;

	iters = rte_mempool_mem_iter(mp, sfc_nic_dma_register_mempool_chunk,
				     &register_data);
	if (iters != mp->nb_mem_chunks) {
		sfc_err(sa,
			"failed to iterate over memory chunks, some mbufs may be unusable");
		result = EFAULT;
		/*
		 * Return an error, but try to continue if error is
		 * async and cannot be handled properly.
		 */
	}

	if (register_data.rc != 0) {
		sfc_err(sa,
			"failed to map some memory chunks (%s), some mbufs may be unusable",
			rte_strerror(register_data.rc));
		result = register_data.rc;
		/* Try to continue */
	}

	/*
	 * There is no point to apply mapping changes triggered by mempool
	 * registration. Configuration will be propagated on start and
	 * mbufs mapping is required in started state only.
	 */
	if (sa->state == SFC_ETHDEV_STARTED) {
		/*
		 * It's safe to reconfigure the DMA mapping even if no changes
		 * have been made during memory chunks iteration. In that case,
		 * this operation will not change anything either.
		 */
		rc = efx_nic_dma_reconfigure(sa->nic);
		if (rc != 0) {
			sfc_err(sa, "cannot reconfigure NIC DMA: %s",
				rte_strerror(rc));
			result = rc;
		}
	}

	return result;
}

static void
sfc_mempool_event_cb(enum rte_mempool_event event, struct rte_mempool *mp,
		     void *user_data)
{
	struct sfc_adapter *sa = user_data;

	if (event != RTE_MEMPOOL_EVENT_READY)
		return;

	sfc_adapter_lock(sa);

	(void)sfc_nic_dma_register_mempool(sa, mp);

	sfc_adapter_unlock(sa);
}

struct sfc_mempool_walk_data {
	struct sfc_adapter		*sa;
	int				rc;
};

static void
sfc_mempool_walk_cb(struct rte_mempool *mp, void *arg)
{
	struct sfc_mempool_walk_data *walk_data = arg;
	int rc;

	rc = sfc_nic_dma_register_mempool(walk_data->sa, mp);
	if (rc != 0)
		walk_data->rc = rc;
}

static int
sfc_nic_dma_attach_regioned(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	struct sfc_mempool_walk_data walk_data = {
		.sa = sa,
	};
	int rc;

	rc = rte_mempool_event_callback_register(sfc_mempool_event_cb, sa);
	if (rc != 0) {
		sfc_err(sa, "failed to register mempool event callback");
		rc = EFAULT;
		goto fail_mempool_event_callback_register;
	}

	rte_mempool_walk(sfc_mempool_walk_cb, &walk_data);
	if (walk_data.rc != 0) {
		rc = walk_data.rc;
		goto fail_mempool_walk;
	}

	return 0;

fail_mempool_walk:
	rte_mempool_event_callback_unregister(sfc_mempool_event_cb, sa);
	sas->nic_dma_info.nb_regions = 0;

fail_mempool_event_callback_register:
	return rc;
}

static void
sfc_nic_dma_detach_regioned(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);

	rte_mempool_event_callback_unregister(sfc_mempool_event_cb, sa);
	sas->nic_dma_info.nb_regions = 0;
}

int
sfc_nic_dma_attach(struct sfc_adapter *sa)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	int rc;

	sfc_log_init(sa, "dma_mapping_type=%u", encp->enc_dma_mapping);

	switch (encp->enc_dma_mapping) {
	case EFX_NIC_DMA_MAPPING_FLAT:
		/* No mapping required */
		rc = 0;
		break;
	case EFX_NIC_DMA_MAPPING_REGIONED:
		rc = sfc_nic_dma_attach_regioned(sa);
		break;
	default:
		rc = ENOTSUP;
		break;
	}

	sfc_log_init(sa, "done: %s", rte_strerror(rc));
	return rc;
}

void
sfc_nic_dma_detach(struct sfc_adapter *sa)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);

	sfc_log_init(sa, "dma_mapping_type=%u", encp->enc_dma_mapping);

	switch (encp->enc_dma_mapping) {
	case EFX_NIC_DMA_MAPPING_FLAT:
		/* Nothing to do here */
		break;
	case EFX_NIC_DMA_MAPPING_REGIONED:
		sfc_nic_dma_detach_regioned(sa);
		break;
	default:
		break;
	}

	sfc_log_init(sa, "done");
}

int
sfc_nic_dma_mz_map(struct sfc_adapter *sa, const struct rte_memzone *mz,
		   efx_nic_dma_addr_type_t addr_type,
		   efsys_dma_addr_t *dma_addr)
{
	efsys_dma_addr_t nic_base;
	efsys_dma_addr_t trgt_base;
	size_t map_len;
	int rc;

	/*
	 * Check if the memzone can be mapped already without changing the DMA
	 * configuration.
	 * libefx is used instead of the driver cache since it can take the type
	 * of the buffer into account and make a better decision when it comes
	 * to buffers that are mapped by the FW itself.
	 */
	rc = efx_nic_dma_map(sa->nic, addr_type, mz->iova, mz->len, dma_addr);
	if (rc == 0)
		return 0;

	if (rc != ENOENT) {
		sfc_err(sa,
			"failed to map memory buffer VA=%p IOVA=%" PRIx64 " length=0x%" PRIx64 ": %s",
			mz->addr, (uint64_t)mz->iova, mz->len,
			rte_strerror(rc));
		return rc;
	}

	rc = efx_nic_dma_config_add(sa->nic, mz->iova, mz->len,
				    &nic_base, &trgt_base, &map_len);
	if (rc != 0) {
		sfc_err(sa,
			"cannot handle memory buffer VA=%p IOVA=%" PRIx64 " length=0x%" PRIx64 ": %s",
			mz->addr, (uint64_t)mz->iova, mz->len,
			rte_strerror(rc));
		return EFAULT;
	}

	rc = sfc_nic_dma_add_region(&sfc_sa2shared(sa)->nic_dma_info,
				    nic_base, trgt_base, map_len);
	if (rc != 0) {
		sfc_err(sa,
			"failed to add DMA region VA=%p IOVA=%" PRIx64 " length=0x%" PRIx64 ": %s",
			mz->addr, (uint64_t)mz->iova, mz->len,
			rte_strerror(rc));
		return rc;
	}

	rc = efx_nic_dma_reconfigure(sa->nic);
	if (rc != 0) {
		sfc_err(sa, "failed to reconfigure DMA");
		return rc;
	}

	rc = efx_nic_dma_map(sa->nic, addr_type, mz->iova, mz->len, dma_addr);
	if (rc != 0) {
		sfc_err(sa,
			"failed to map memory buffer VA=%p IOVA=%" PRIx64 " length=0x%" PRIx64 ": %s",
			mz->addr, (uint64_t)mz->iova, mz->len,
			rte_strerror(rc));
		return rc;
	}

	return 0;
}
