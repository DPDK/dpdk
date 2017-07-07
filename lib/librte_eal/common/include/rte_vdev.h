/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 RehiveTech. All rights reserved.
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
 *     * Neither the name of RehiveTech nor the names of its
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

#ifndef RTE_VDEV_H
#define RTE_VDEV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/queue.h>
#include <rte_dev.h>
#include <rte_devargs.h>

struct rte_vdev_device {
	TAILQ_ENTRY(rte_vdev_device) next;      /**< Next attached vdev */
	struct rte_device device;               /**< Inherit core device */
};

static inline const char *
rte_vdev_device_name(const struct rte_vdev_device *dev)
{
	if (dev && dev->device.devargs)
		return dev->device.devargs->virt.drv_name;
	return NULL;
}

static inline const char *
rte_vdev_device_args(const struct rte_vdev_device *dev)
{
	if (dev && dev->device.devargs)
		return dev->device.devargs->args;
	return "";
}

/** Double linked list of virtual device drivers. */
TAILQ_HEAD(vdev_driver_list, rte_vdev_driver);

/**
 * Probe function called for each virtual device driver once.
 */
typedef int (rte_vdev_probe_t)(struct rte_vdev_device *dev);

/**
 * Remove function called for each virtual device driver once.
 */
typedef int (rte_vdev_remove_t)(struct rte_vdev_device *dev);

/**
 * A virtual device driver abstraction.
 */
struct rte_vdev_driver {
	TAILQ_ENTRY(rte_vdev_driver) next; /**< Next in list. */
	struct rte_driver driver;      /**< Inherited general driver. */
	rte_vdev_probe_t *probe;       /**< Virtual device probe function. */
	rte_vdev_remove_t *remove;     /**< Virtual device remove function. */
};

/**
 * Register a virtual device driver.
 *
 * @param driver
 *   A pointer to a rte_vdev_driver structure describing the driver
 *   to be registered.
 */
void rte_vdev_register(struct rte_vdev_driver *driver);

/**
 * Unregister a virtual device driver.
 *
 * @param driver
 *   A pointer to a rte_vdev_driver structure describing the driver
 *   to be unregistered.
 */
void rte_vdev_unregister(struct rte_vdev_driver *driver);

#define RTE_PMD_REGISTER_VDEV(nm, vdrv)\
RTE_INIT(vdrvinitfn_ ##vdrv);\
static const char *vdrvinit_ ## nm ## _alias;\
static void vdrvinitfn_ ##vdrv(void)\
{\
	(vdrv).driver.name = RTE_STR(nm);\
	(vdrv).driver.alias = vdrvinit_ ## nm ## _alias;\
	rte_vdev_register(&vdrv);\
} \
RTE_PMD_EXPORT_NAME(nm, __COUNTER__)

#define RTE_PMD_REGISTER_ALIAS(nm, alias)\
static const char *vdrvinit_ ## nm ## _alias = RTE_STR(alias)

#ifdef __cplusplus
}
#endif

#endif
