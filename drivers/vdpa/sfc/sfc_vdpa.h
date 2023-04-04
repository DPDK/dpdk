/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 Xilinx, Inc.
 */

#ifndef _SFC_VDPA_H
#define _SFC_VDPA_H

#include <stdint.h>
#include <sys/queue.h>

#include <bus_pci_driver.h>

#include "sfc_efx.h"
#include "sfc_efx_mcdi.h"
#include "sfc_vdpa_debug.h"
#include "sfc_vdpa_log.h"
#include "sfc_vdpa_ops.h"

#define SFC_VDPA_MAC_ADDR			"mac"
#define SFC_VDPA_DEFAULT_MCDI_IOVA		0x200000000000

/* Broadcast & Unicast MAC filters are supported */
#define SFC_MAX_SUPPORTED_FILTERS		3

/*
 * Get function-local index of the associated VI from the
 * virtqueue number. Queue 0 is reserved for MCDI
 */
#define SFC_VDPA_GET_VI_INDEX(vq_num) (((vq_num) / 2) + 1)

enum sfc_vdpa_filter_type {
	SFC_VDPA_BCAST_MAC_FILTER = 0,
	SFC_VDPA_UCAST_MAC_FILTER = 1,
	SFC_VDPA_MCAST_DST_FILTER = 2,
	SFC_VDPA_FILTER_NTYPE
};

typedef struct sfc_vdpa_filter_s {
	int				filter_cnt;
	efx_filter_spec_t		spec[SFC_MAX_SUPPORTED_FILTERS];
} sfc_vdpa_filter_t;

/* Adapter private data */
struct sfc_vdpa_adapter {
	TAILQ_ENTRY(sfc_vdpa_adapter)	next;
	/*
	 * PMD setup and configuration is not thread safe. Since it is not
	 * performance sensitive, it is better to guarantee thread-safety
	 * and add device level lock. vDPA control operations which
	 * change its state should acquire the lock.
	 */
	rte_spinlock_t			lock;
	struct rte_pci_device		*pdev;

	struct rte_kvargs		*kvargs;

	efx_family_t			family;
	efx_nic_t			*nic;
	rte_spinlock_t			nic_lock;

	efsys_bar_t			mem_bar;

	struct sfc_efx_mcdi		mcdi;
	size_t				mcdi_buff_size;

	uint32_t			max_queue_count;

	char				log_prefix[SFC_VDPA_LOG_PREFIX_MAX];
	uint32_t			logtype_main;

	sfc_vdpa_filter_t		filters;

	int				vfio_group_fd;
	int				vfio_dev_fd;
	int				vfio_container_fd;
	int				iommu_group_num;
	struct sfc_vdpa_ops_data	*ops_data;
};

uint32_t
sfc_vdpa_register_logtype(const struct rte_pci_addr *pci_addr,
			  const char *lt_prefix_str,
			  uint32_t ll_default);

struct sfc_vdpa_adapter *
sfc_vdpa_get_adapter_by_dev(struct rte_pci_device *pdev);
struct sfc_vdpa_ops_data *
sfc_vdpa_get_data_by_dev(struct rte_vdpa_device *vdpa_dev);

int
sfc_vdpa_hw_init(struct sfc_vdpa_adapter *sva);
void
sfc_vdpa_hw_fini(struct sfc_vdpa_adapter *sva);

int
sfc_vdpa_mcdi_init(struct sfc_vdpa_adapter *sva);
void
sfc_vdpa_mcdi_fini(struct sfc_vdpa_adapter *sva);

int
sfc_vdpa_dma_alloc(struct sfc_vdpa_adapter *sva, const char *name,
		   size_t len, efsys_mem_t *esmp);

void
sfc_vdpa_dma_free(struct sfc_vdpa_adapter *sva, efsys_mem_t *esmp);

int
sfc_vdpa_dma_map(struct sfc_vdpa_ops_data *vdpa_data, bool do_map);

int
sfc_vdpa_filter_remove(struct sfc_vdpa_ops_data *ops_data);
int
sfc_vdpa_filter_config(struct sfc_vdpa_ops_data *ops_data);

static inline struct sfc_vdpa_adapter *
sfc_vdpa_adapter_by_dev_handle(void *dev_handle)
{
	return (struct sfc_vdpa_adapter *)dev_handle;
}

/*
 * Add wrapper functions to acquire/release lock to be able to remove or
 * change the lock in one place.
 */
#define sfc_vdpa_adapter_lock_init(sva) rte_spinlock_init(&(sva)->lock)
#define sfc_vdpa_adapter_is_locked(sva) rte_spinlock_is_locked(&(sva)->lock)
#define sfc_vdpa_adapter_lock(sva) rte_spinlock_lock(&(sva)->lock)
#define sfc_vdpa_adapter_trylock(sva) rte_spinlock_trylock(&(sva)->lock)
#define sfc_vdpa_adapter_unlock(sva) rte_spinlock_unlock(&(sva)->lock)
#define sfc_vdpa_adapter_lock_fini(sva) RTE_SET_USED(sva)

#endif  /* _SFC_VDPA_H */
