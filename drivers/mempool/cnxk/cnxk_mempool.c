/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_atomic.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_devargs.h>
#include <rte_eal.h>
#include <rte_io.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_pci.h>

#include "roc_api.h"

#define CNXK_NPA_DEV_NAME	 RTE_STR(cnxk_npa_dev_)
#define CNXK_NPA_DEV_NAME_LEN	 (sizeof(CNXK_NPA_DEV_NAME) + PCI_PRI_STR_SIZE)
#define CNXK_NPA_MAX_POOLS_PARAM "max_pools"

static inline uint32_t
npa_aura_size_to_u32(uint8_t val)
{
	if (val == NPA_AURA_SZ_0)
		return 128;
	if (val >= NPA_AURA_SZ_MAX)
		return BIT_ULL(20);

	return 1 << (val + 6);
}

static int
parse_max_pools_handler(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint32_t val;

	val = rte_align32pow2(atoi(value));
	if (val < npa_aura_size_to_u32(NPA_AURA_SZ_128))
		val = 128;
	if (val > npa_aura_size_to_u32(NPA_AURA_SZ_1M))
		val = BIT_ULL(20);

	*(uint32_t *)extra_args = val;
	return 0;
}

static inline uint32_t
parse_max_pools(struct rte_devargs *devargs)
{
	uint32_t max_pools = npa_aura_size_to_u32(NPA_AURA_SZ_128);
	struct rte_kvargs *kvlist;

	if (devargs == NULL)
		goto exit;
	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		goto exit;

	rte_kvargs_process(kvlist, CNXK_NPA_MAX_POOLS_PARAM,
			   &parse_max_pools_handler, &max_pools);
	rte_kvargs_free(kvlist);
exit:
	return max_pools;
}

static int
cnxk_mempool_plt_parse_devargs(struct rte_pci_device *pci_dev)
{
	roc_idev_npa_maxpools_set(parse_max_pools(pci_dev->device.devargs));
	return 0;
}

static inline char *
npa_dev_to_name(struct rte_pci_device *pci_dev, char *name)
{
	snprintf(name, CNXK_NPA_DEV_NAME_LEN, CNXK_NPA_DEV_NAME PCI_PRI_FMT,
		 pci_dev->addr.domain, pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);

	return name;
}

static int
npa_init(struct rte_pci_device *pci_dev)
{
	char name[CNXK_NPA_DEV_NAME_LEN];
	const struct rte_memzone *mz;
	struct roc_npa *dev;
	int rc = -ENOMEM;

	mz = rte_memzone_reserve_aligned(npa_dev_to_name(pci_dev, name),
					 sizeof(*dev), SOCKET_ID_ANY, 0,
					 RTE_CACHE_LINE_SIZE);
	if (mz == NULL)
		goto error;

	dev = mz->addr;
	dev->pci_dev = pci_dev;

	rc = roc_npa_dev_init(dev);
	if (rc)
		goto mz_free;

	return 0;

mz_free:
	rte_memzone_free(mz);
error:
	plt_err("failed to initialize npa device rc=%d", rc);
	return rc;
}

static int
npa_fini(struct rte_pci_device *pci_dev)
{
	char name[CNXK_NPA_DEV_NAME_LEN];
	const struct rte_memzone *mz;
	int rc;

	mz = rte_memzone_lookup(npa_dev_to_name(pci_dev, name));
	if (mz == NULL)
		return -EINVAL;

	rc = roc_npa_dev_fini(mz->addr);
	if (rc) {
		if (rc != -EAGAIN)
			plt_err("Failed to remove npa dev, rc=%d", rc);
		return rc;
	}
	rte_memzone_free(mz);

	return 0;
}

static int
npa_remove(struct rte_pci_device *pci_dev)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	return npa_fini(pci_dev);
}

static int
npa_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	int rc;

	RTE_SET_USED(pci_drv);

	rc = roc_plt_init();
	if (rc < 0)
		return rc;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	return npa_init(pci_dev);
}

static const struct rte_pci_id npa_pci_map[] = {
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KA, PCI_DEVID_CNXK_RVU_NPA_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KAS, PCI_DEVID_CNXK_RVU_NPA_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN9KA, PCI_DEVID_CNXK_RVU_NPA_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN9KB, PCI_DEVID_CNXK_RVU_NPA_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN9KC, PCI_DEVID_CNXK_RVU_NPA_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN9KD, PCI_DEVID_CNXK_RVU_NPA_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN9KE, PCI_DEVID_CNXK_RVU_NPA_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KA, PCI_DEVID_CNXK_RVU_NPA_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KAS, PCI_DEVID_CNXK_RVU_NPA_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN9KA, PCI_DEVID_CNXK_RVU_NPA_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN9KB, PCI_DEVID_CNXK_RVU_NPA_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN9KC, PCI_DEVID_CNXK_RVU_NPA_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN9KD, PCI_DEVID_CNXK_RVU_NPA_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN9KE, PCI_DEVID_CNXK_RVU_NPA_VF),
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver npa_pci = {
	.id_table = npa_pci_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = npa_probe,
	.remove = npa_remove,
};

RTE_PMD_REGISTER_PCI(mempool_cnxk, npa_pci);
RTE_PMD_REGISTER_PCI_TABLE(mempool_cnxk, npa_pci_map);
RTE_PMD_REGISTER_KMOD_DEP(mempool_cnxk, "vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(mempool_cnxk,
			      CNXK_NPA_MAX_POOLS_PARAM "=<128-1048576>");

RTE_INIT(cnxk_mempool_parse_devargs)
{
	roc_npa_lf_init_cb_register(cnxk_mempool_plt_parse_devargs);
}
