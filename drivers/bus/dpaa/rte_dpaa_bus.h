/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2017 NXP
 *
 */
#ifndef __RTE_DPAA_BUS_H__
#define __RTE_DPAA_BUS_H__

#include <rte_bus.h>
#include <rte_mempool.h>

#include <fsl_usd.h>
#include <fsl_qman.h>
#include <fsl_bman.h>
#include <of.h>
#include <netcfg.h>

#define FSL_DPAA_BUS_NAME	"FSL_DPAA_BUS"

#define DPAA_MEMPOOL_OPS_NAME	"dpaa"

#define DEV_TO_DPAA_DEVICE(ptr)	\
		container_of(ptr, struct rte_dpaa_device, device)

/* DPAA SoC identifier; If this is not available, it can be concluded
 * that board is non-DPAA. Single slot is currently supported.
 */
#define DPAA_SOC_ID_FILE	"/sys/devices/soc0/soc_id"

#define SVR_LS1043A_FAMILY	0x87920000
#define SVR_LS1046A_FAMILY	0x87070000
#define SVR_MASK		0xffff0000

extern unsigned int dpaa_svr_family;

extern RTE_DEFINE_PER_LCORE(bool, dpaa_io);

struct rte_dpaa_device;
struct rte_dpaa_driver;

/* DPAA Device and Driver lists for DPAA bus */
TAILQ_HEAD(rte_dpaa_device_list, rte_dpaa_device);
TAILQ_HEAD(rte_dpaa_driver_list, rte_dpaa_driver);

/* Configuration variables exported from DPAA bus */
extern struct netcfg_info *dpaa_netcfg;

enum rte_dpaa_type {
	FSL_DPAA_ETH = 1,
	FSL_DPAA_CRYPTO,
};

struct rte_dpaa_bus {
	struct rte_bus bus;
	struct rte_dpaa_device_list device_list;
	struct rte_dpaa_driver_list driver_list;
	int device_count;
};

struct dpaa_device_id {
	uint8_t fman_id; /**< Fman interface ID, for ETH type device */
	uint8_t mac_id; /**< Fman MAC interface ID, for ETH type device */
	uint16_t dev_id; /**< Device Identifier from DPDK */
};

struct rte_dpaa_device {
	TAILQ_ENTRY(rte_dpaa_device) next;
	struct rte_device device;
	union {
		struct rte_eth_dev *eth_dev;
		struct rte_cryptodev *crypto_dev;
	};
	struct rte_dpaa_driver *driver;
	struct dpaa_device_id id;
	enum rte_dpaa_type device_type; /**< Ethernet or crypto type device */
	char name[RTE_ETH_NAME_MAX_LEN];
};

typedef int (*rte_dpaa_probe_t)(struct rte_dpaa_driver *dpaa_drv,
				struct rte_dpaa_device *dpaa_dev);
typedef int (*rte_dpaa_remove_t)(struct rte_dpaa_device *dpaa_dev);

struct rte_dpaa_driver {
	TAILQ_ENTRY(rte_dpaa_driver) next;
	struct rte_driver driver;
	struct rte_dpaa_bus *dpaa_bus;
	enum rte_dpaa_type drv_type;
	rte_dpaa_probe_t probe;
	rte_dpaa_remove_t remove;
};

struct dpaa_portal {
	uint32_t bman_idx; /**< BMAN Portal ID*/
	uint32_t qman_idx; /**< QMAN Portal ID*/
	uint64_t tid;/**< Parent Thread id for this portal */
};

/* TODO - this is costly, need to write a fast coversion routine */
static inline void *rte_dpaa_mem_ptov(phys_addr_t paddr)
{
	const struct rte_memseg *memseg = rte_eal_get_physmem_layout();
	int i;

	for (i = 0; i < RTE_MAX_MEMSEG && memseg[i].addr != NULL; i++) {
		if (paddr >= memseg[i].iova && paddr <
			memseg[i].iova + memseg[i].len)
			return (uint8_t *)(memseg[i].addr) +
			       (paddr - memseg[i].iova);
	}

	return NULL;
}

/**
 * Register a DPAA driver.
 *
 * @param driver
 *   A pointer to a rte_dpaa_driver structure describing the driver
 *   to be registered.
 */
void rte_dpaa_driver_register(struct rte_dpaa_driver *driver);

/**
 * Unregister a DPAA driver.
 *
 * @param driver
 *	A pointer to a rte_dpaa_driver structure describing the driver
 *	to be unregistered.
 */
void rte_dpaa_driver_unregister(struct rte_dpaa_driver *driver);

/**
 * Initialize a DPAA portal
 *
 * @param arg
 *	Per thread ID
 *
 * @return
 *	0 in case of success, error otherwise
 */
int rte_dpaa_portal_init(void *arg);

int rte_dpaa_portal_fq_init(void *arg, struct qman_fq *fq);

int rte_dpaa_portal_fq_close(struct qman_fq *fq);

/**
 * Cleanup a DPAA Portal
 */
void dpaa_portal_finish(void *arg);

/** Helper for DPAA device registration from driver (eth, crypto) instance */
#define RTE_PMD_REGISTER_DPAA(nm, dpaa_drv) \
RTE_INIT(dpaainitfn_ ##nm); \
static void dpaainitfn_ ##nm(void) \
{\
	(dpaa_drv).driver.name = RTE_STR(nm);\
	rte_dpaa_driver_register(&dpaa_drv); \
} \
RTE_PMD_EXPORT_NAME(nm, __COUNTER__)

/* Create storage for dqrr entries per lcore */
#define DPAA_PORTAL_DEQUEUE_DEPTH	16
struct dpaa_portal_dqrr {
	void *mbuf[DPAA_PORTAL_DEQUEUE_DEPTH];
	uint64_t dqrr_held;
	uint8_t dqrr_size;
};

RTE_DECLARE_PER_LCORE(struct dpaa_portal_dqrr, held_bufs);

#define DPAA_PER_LCORE_DQRR_SIZE       RTE_PER_LCORE(held_bufs).dqrr_size
#define DPAA_PER_LCORE_DQRR_HELD       RTE_PER_LCORE(held_bufs).dqrr_held
#define DPAA_PER_LCORE_DQRR_MBUF(i)    RTE_PER_LCORE(held_bufs).mbuf[i]

#ifdef __cplusplus
}
#endif

#endif /* __RTE_DPAA_BUS_H__ */
