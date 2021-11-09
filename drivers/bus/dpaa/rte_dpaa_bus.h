/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2017-2020 NXP
 *
 */
#ifndef __RTE_DPAA_BUS_H__
#define __RTE_DPAA_BUS_H__

#include <rte_bus.h>
#include <rte_mbuf_dyn.h>
#include <rte_mempool.h>
#include <dpaax_iova_table.h>

#include <dpaa_of.h>
#include <fsl_usd.h>
#include <fsl_qman.h>
#include <fsl_bman.h>
#include <netcfg.h>

/* This sequence number field is used to store event entry index for
 * driver specific usage. For parallel mode queues, invalid
 * index will be set and for atomic mode queues, valid value
 * ranging from 1 to 16.
 */
#define DPAA_INVALID_MBUF_SEQN  0

typedef uint32_t dpaa_seqn_t;
extern int dpaa_seqn_dynfield_offset;

/**
 * Read dpaa sequence number from mbuf.
 *
 * @param mbuf Structure to read from.
 * @return pointer to dpaa sequence number.
 */
__rte_internal
static inline dpaa_seqn_t *
dpaa_seqn(struct rte_mbuf *mbuf)
{
	return RTE_MBUF_DYNFIELD(mbuf, dpaa_seqn_dynfield_offset,
		dpaa_seqn_t *);
}

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

/** Device driver supports link state interrupt */
#define RTE_DPAA_DRV_INTR_LSC  0x0008

/** Number of supported QDMA devices */
#define RTE_DPAA_QDMA_DEVICES  1

#define RTE_DEV_TO_DPAA_CONST(ptr) \
	container_of(ptr, const struct rte_dpaa_device, device)

extern unsigned int dpaa_svr_family;

struct rte_dpaa_device;
struct rte_dpaa_driver;

/* DPAA Device and Driver lists for DPAA bus */
TAILQ_HEAD(rte_dpaa_device_list, rte_dpaa_device);
TAILQ_HEAD(rte_dpaa_driver_list, rte_dpaa_driver);

enum rte_dpaa_type {
	FSL_DPAA_ETH = 1,
	FSL_DPAA_CRYPTO,
	FSL_DPAA_QDMA
};

struct rte_dpaa_bus {
	struct rte_bus bus;
	struct rte_dpaa_device_list device_list;
	struct rte_dpaa_driver_list driver_list;
	int device_count;
	int detected;
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
		struct rte_dma_dev *dmadev;
	};
	struct rte_dpaa_driver *driver;
	struct dpaa_device_id id;
	struct rte_intr_handle *intr_handle;
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
	uint32_t drv_flags;                 /**< Flags for controlling device.*/
};

/* Create storage for dqrr entries per lcore */
#define DPAA_PORTAL_DEQUEUE_DEPTH	16
struct dpaa_portal_dqrr {
	void *mbuf[DPAA_PORTAL_DEQUEUE_DEPTH];
	uint64_t dqrr_held;
	uint8_t dqrr_size;
};

struct dpaa_portal {
	uint32_t bman_idx; /**< BMAN Portal ID*/
	uint32_t qman_idx; /**< QMAN Portal ID*/
	struct dpaa_portal_dqrr dpaa_held_bufs;
	struct rte_crypto_op **dpaa_sec_ops;
	int dpaa_sec_op_nb;
	uint64_t tid;/**< Parent Thread id for this portal */
};

RTE_DECLARE_PER_LCORE(struct dpaa_portal *, dpaa_io);

#define DPAA_PER_LCORE_PORTAL \
	RTE_PER_LCORE(dpaa_io)
#define DPAA_PER_LCORE_DQRR_SIZE \
	RTE_PER_LCORE(dpaa_io)->dpaa_held_bufs.dqrr_size
#define DPAA_PER_LCORE_DQRR_HELD \
	RTE_PER_LCORE(dpaa_io)->dpaa_held_bufs.dqrr_held
#define DPAA_PER_LCORE_DQRR_MBUF(i) \
	RTE_PER_LCORE(dpaa_io)->dpaa_held_bufs.mbuf[i]
#define DPAA_PER_LCORE_RTE_CRYPTO_OP \
	RTE_PER_LCORE(dpaa_io)->dpaa_sec_ops
#define DPAA_PER_LCORE_DPAA_SEC_OP_NB \
	RTE_PER_LCORE(dpaa_io)->dpaa_sec_op_nb

/* Various structures representing contiguous memory maps */
struct dpaa_memseg {
	TAILQ_ENTRY(dpaa_memseg) next;
	char *vaddr;
	rte_iova_t iova;
	size_t len;
};

TAILQ_HEAD(dpaa_memseg_list, dpaa_memseg);
extern struct dpaa_memseg_list rte_dpaa_memsegs;

/* Either iterate over the list of internal memseg references or fallback to
 * EAL memseg based iova2virt.
 */
static inline void *rte_dpaa_mem_ptov(phys_addr_t paddr)
{
	struct dpaa_memseg *ms;
	void *va;

	va = dpaax_iova_table_get_va(paddr);
	if (likely(va != NULL))
		return va;

	/* Check if the address is already part of the memseg list internally
	 * maintained by the dpaa driver.
	 */
	TAILQ_FOREACH(ms, &rte_dpaa_memsegs, next) {
		if (paddr >= ms->iova && paddr <
			ms->iova + ms->len)
			return RTE_PTR_ADD(ms->vaddr, (uintptr_t)(paddr - ms->iova));
	}

	/* If not, Fallback to full memseg list searching */
	va = rte_mem_iova2virt(paddr);

	dpaax_iova_table_update(paddr, va, RTE_CACHE_LINE_SIZE);

	return va;
}

static inline rte_iova_t
rte_dpaa_mem_vtop(void *vaddr)
{
	const struct rte_memseg *ms;

	ms = rte_mem_virt2memseg(vaddr, NULL);
	if (ms)
		return ms->iova + RTE_PTR_DIFF(vaddr, ms->addr);

	return (size_t)NULL;
}

/**
 * Register a DPAA driver.
 *
 * @param driver
 *   A pointer to a rte_dpaa_driver structure describing the driver
 *   to be registered.
 */
__rte_internal
void rte_dpaa_driver_register(struct rte_dpaa_driver *driver);

/**
 * Unregister a DPAA driver.
 *
 * @param driver
 *	A pointer to a rte_dpaa_driver structure describing the driver
 *	to be unregistered.
 */
__rte_internal
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
__rte_internal
int rte_dpaa_portal_init(void *arg);

__rte_internal
int rte_dpaa_portal_fq_init(void *arg, struct qman_fq *fq);

__rte_internal
int rte_dpaa_portal_fq_close(struct qman_fq *fq);

/**
 * Cleanup a DPAA Portal
 */
void dpaa_portal_finish(void *arg);

/** Helper for DPAA device registration from driver (eth, crypto) instance */
#define RTE_PMD_REGISTER_DPAA(nm, dpaa_drv) \
RTE_INIT(dpaainitfn_ ##nm) \
{\
	(dpaa_drv).driver.name = RTE_STR(nm);\
	rte_dpaa_driver_register(&dpaa_drv); \
} \
RTE_PMD_EXPORT_NAME(nm, __COUNTER__)

__rte_internal
struct fm_eth_port_cfg *dpaa_get_eth_port_cfg(int dev_id);

#ifdef __cplusplus
}
#endif

#endif /* __RTE_DPAA_BUS_H__ */
