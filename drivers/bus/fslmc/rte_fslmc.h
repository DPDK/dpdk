/*-
 *   BSD LICENSE
 *
 *   Copyright 2016 NXP.
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

#ifndef _RTE_FSLMC_H_
#define _RTE_FSLMC_H_

/**
 * @file
 *
 * RTE FSLMC Bus Interface
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_debug.h>
#include <rte_interrupts.h>
#include <rte_dev.h>
#include <rte_bus.h>

struct rte_dpaa2_driver;

/* DPAA2 Device and Driver lists for FSLMC bus */
TAILQ_HEAD(rte_fslmc_device_list, rte_dpaa2_device);
TAILQ_HEAD(rte_fslmc_driver_list, rte_dpaa2_driver);

extern struct rte_fslmc_bus rte_fslmc_bus;

/**
 * A structure describing a DPAA2 device.
 */
struct rte_dpaa2_device {
	TAILQ_ENTRY(rte_dpaa2_device) next; /**< Next probed DPAA2 device. */
	struct rte_device device;           /**< Inherit core device */
	union {
		struct rte_eth_dev *eth_dev;        /**< ethernet device */
		struct rte_cryptodev *cryptodev;    /**< Crypto Device */
	};
	uint16_t dev_type;                  /**< Device Type */
	uint16_t object_id;             /**< DPAA2 Object ID */
	struct rte_intr_handle intr_handle; /**< Interrupt handle */
	struct rte_dpaa2_driver *driver;    /**< Associated driver */
	char name[32];          /**< DPAA2 Object name*/
};

typedef int (*rte_dpaa2_probe_t)(struct rte_dpaa2_driver *dpaa2_drv,
				 struct rte_dpaa2_device *dpaa2_dev);
typedef int (*rte_dpaa2_remove_t)(struct rte_dpaa2_device *dpaa2_dev);

/**
 * A structure describing a DPAA2 driver.
 */
struct rte_dpaa2_driver {
	TAILQ_ENTRY(rte_dpaa2_driver) next; /**< Next in list. */
	struct rte_driver driver;           /**< Inherit core driver. */
	struct rte_fslmc_bus *fslmc_bus;    /**< FSLMC bus reference */
	uint32_t drv_flags;                 /**< Flags for controlling device.*/
	uint16_t drv_type;                  /**< Driver Type */
	rte_dpaa2_probe_t probe;
	rte_dpaa2_remove_t remove;
};

/*
 * FSLMC bus
 */
struct rte_fslmc_bus {
	struct rte_bus bus;     /**< Generic Bus object */
	struct rte_fslmc_device_list device_list;
				/**< FSLMC DPAA2 Device list */
	struct rte_fslmc_driver_list driver_list;
				/**< FSLMC DPAA2 Driver list */
	int device_count;
				/**< Optional: Count of devices on bus */
};

/**
 * Register a DPAA2 driver.
 *
 * @param driver
 *   A pointer to a rte_dpaa2_driver structure describing the driver
 *   to be registered.
 */
void rte_fslmc_driver_register(struct rte_dpaa2_driver *driver);

/**
 * Unregister a DPAA2 driver.
 *
 * @param driver
 *   A pointer to a rte_dpaa2_driver structure describing the driver
 *   to be unregistered.
 */
void rte_fslmc_driver_unregister(struct rte_dpaa2_driver *driver);

/** Helper for DPAA2 device registration from driver (eth, crypto) instance */
#define RTE_PMD_REGISTER_DPAA2(nm, dpaa2_drv) \
RTE_INIT(dpaa2initfn_ ##nm); \
static void dpaa2initfn_ ##nm(void) \
{\
	(dpaa2_drv).driver.name = RTE_STR(nm);\
	rte_fslmc_driver_register(&dpaa2_drv); \
} \
RTE_PMD_EXPORT_NAME(nm, __COUNTER__)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_FSLMC_H_ */
