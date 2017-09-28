/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 NXP.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of NXP nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#define DEV_TO_DPAA_DEVICE(ptr)	\
		container_of(ptr, struct rte_dpaa_device, device)

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
		if (paddr >= memseg[i].phys_addr && paddr <
			memseg[i].phys_addr + memseg[i].len)
			return (uint8_t *)(memseg[i].addr) +
			       (paddr - memseg[i].phys_addr);
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

#ifdef __cplusplus
}
#endif

#endif /* __RTE_DPAA_BUS_H__ */
