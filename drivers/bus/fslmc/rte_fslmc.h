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
#include <linux/vfio.h>

#include <rte_debug.h>
#include <rte_interrupts.h>
#include <rte_dev.h>
#include <rte_bus.h>
#include <rte_tailq.h>

#include <fslmc_vfio.h>

#define FSLMC_OBJECT_MAX_LEN 32   /**< Length of each device on bus */

struct rte_dpaa2_driver;

/* DPAA2 Device and Driver lists for FSLMC bus */
TAILQ_HEAD(rte_fslmc_device_list, rte_dpaa2_device);
TAILQ_HEAD(rte_fslmc_driver_list, rte_dpaa2_driver);

extern struct rte_fslmc_bus rte_fslmc_bus;

enum rte_dpaa2_dev_type {
	/* Devices backed by DPDK driver */
	DPAA2_ETH,	/**< DPNI type device*/
	DPAA2_CRYPTO,	/**< DPSECI type device */
	DPAA2_CON,	/**< DPCONC type device */
	/* Devices not backed by a DPDK driver: DPIO, DPBP, DPCI, DPMCP */
	DPAA2_BPOOL,	/**< DPBP type device */
	DPAA2_IO,	/**< DPIO type device */
	DPAA2_CI,	/**< DPCI type device */
	DPAA2_MPORTAL,  /**< DPMCP type device */
	/* Unknown device placeholder */
	DPAA2_UNKNOWN
};

TAILQ_HEAD(rte_dpaa2_object_list, rte_dpaa2_object);

typedef int (*rte_dpaa2_obj_create_t)(int vdev_fd,
				      struct vfio_device_info *obj_info,
				      int object_id);

/**
 * A structure describing a DPAA2 object.
 */
struct rte_dpaa2_object {
	TAILQ_ENTRY(rte_dpaa2_object) next; /**< Next in list. */
	const char *name;                   /**< Name of Object. */
	enum rte_dpaa2_dev_type dev_type;   /**< Type of device */
	rte_dpaa2_obj_create_t create;
};

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
	enum rte_dpaa2_dev_type dev_type;   /**< Device Type */
	uint16_t object_id;                 /**< DPAA2 Object ID */
	struct rte_intr_handle intr_handle; /**< Interrupt handle */
	struct rte_dpaa2_driver *driver;    /**< Associated driver */
	char name[FSLMC_OBJECT_MAX_LEN];    /**< DPAA2 Object name*/
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
	enum rte_dpaa2_dev_type drv_type;   /**< Driver Type */
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

/**
 * Register a DPAA2 MC Object driver.
 *
 * @param mc_object
 *   A pointer to a rte_dpaa_object structure describing the mc object
 *   to be registered.
 */
void rte_fslmc_object_register(struct rte_dpaa2_object *object);

/** Helper for DPAA2 object registration */
#define RTE_PMD_REGISTER_DPAA2_OBJECT(nm, dpaa2_obj) \
RTE_INIT(dpaa2objinitfn_ ##nm); \
static void dpaa2objinitfn_ ##nm(void) \
{\
	(dpaa2_obj).name = RTE_STR(nm);\
	rte_fslmc_object_register(&dpaa2_obj); \
} \
RTE_PMD_EXPORT_NAME(nm, __COUNTER__)

#endif /* _RTE_FSLMC_H_ */
