/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2014 6WIND S.A.
 *   All rights reserved.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#ifndef _RTE_DEV_H_
#define _RTE_DEV_H_

/**
 * @file
 *
 * RTE PMD Driver Registration Interface
 *
 * This file manages the list of device drivers.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/queue.h>

/** Double linked list of device drivers. */
TAILQ_HEAD(rte_driver_list, rte_driver);

/**
 * Initialization function called for each device driver once.
 */
typedef int (rte_dev_init_t)(const char *name, const char *args);

/**
 * Uninitilization function called for each device driver once.
 */
typedef int (rte_dev_uninit_t)(const char *name);

/**
 * Driver type enumeration
 */
enum pmd_type {
	PMD_VDEV = 0,
	PMD_PDEV = 1,
};

/**
 * A structure describing a device driver.
 */
struct rte_driver {
	TAILQ_ENTRY(rte_driver) next;  /**< Next in list. */
	enum pmd_type type;		   /**< PMD Driver type */
	const char *name;                   /**< Driver name. */
	rte_dev_init_t *init;              /**< Device init. function. */
	rte_dev_uninit_t *uninit;          /**< Device uninit. function. */
};

/**
 * Register a device driver.
 *
 * @param driver
 *   A pointer to a rte_dev structure describing the driver
 *   to be registered.
 */
void rte_eal_driver_register(struct rte_driver *driver);

/**
 * Unregister a device driver.
 *
 * @param driver
 *   A pointer to a rte_dev structure describing the driver
 *   to be unregistered.
 */
void rte_eal_driver_unregister(struct rte_driver *driver);

/**
 * Initalize all the registered drivers in this process
 */
int rte_eal_dev_init(void);

/**
 * Initialize a driver specified by name.
 *
 * @param name
 *   The pointer to a driver name to be initialized.
 * @param args
 *   The pointer to arguments used by driver initialization.
 * @return
 *  0 on success, negative on error
 */
int rte_eal_vdev_init(const char *name, const char *args);

/**
 * Uninitalize a driver specified by name.
 *
 * @param name
 *   The pointer to a driver name to be initialized.
 * @return
 *  0 on success, negative on error
 */
int rte_eal_vdev_uninit(const char *name);

#define PMD_REGISTER_DRIVER(d)\
void devinitfn_ ##d(void);\
void __attribute__((constructor, used)) devinitfn_ ##d(void)\
{\
	rte_eal_driver_register(&d);\
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_VDEV_H_ */
