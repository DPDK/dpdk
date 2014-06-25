/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#include <string.h>
#include <inttypes.h>
#include <sys/queue.h>

#include <rte_dev.h>
#include <rte_devargs.h>
#include <rte_debug.h>
#include <rte_devargs.h>

#include "eal_private.h"

/** Global list of device drivers. */
static struct rte_driver_list dev_driver_list =
	TAILQ_HEAD_INITIALIZER(dev_driver_list);

/* register a driver */
void
rte_eal_driver_register(struct rte_driver *driver)
{
	TAILQ_INSERT_TAIL(&dev_driver_list, driver, next);
}

/* unregister a driver */
void
rte_eal_driver_unregister(struct rte_driver *driver)
{
	TAILQ_REMOVE(&dev_driver_list, driver, next);
}

int
rte_eal_dev_init(uint8_t init_pri)
{
	struct rte_devargs *devargs;
	struct rte_driver *driver;

	/*
	 * Note that the dev_driver_list is populated here
	 * from calls made to rte_eal_driver_register from constructor functions
	 * embedded into PMD modules via the PMD_REGISTER_DRIVER macro
	 */

	/* call the init function for each virtual device */
	TAILQ_FOREACH(devargs, &devargs_list, next) {

		if (devargs->type != RTE_DEVTYPE_VIRTUAL)
			continue;

		TAILQ_FOREACH(driver, &dev_driver_list, next) {
			/* RTE_DEVTYPE_VIRTUAL can only be a virtual or bonded device,
			 * virtual devices are initialized pre PCI probing and bonded
			 * device are post pci probing */
			if ((driver->type == PMD_VDEV && init_pri ==
					PMD_INIT_PRE_PCI_PROBE) ||
				(driver->type == PMD_BDEV && init_pri ==
						PMD_INIT_POST_PCI_PROBE)) {

				/* search a driver prefix in virtual device name */
				if (!strncmp(driver->name, devargs->virtual.drv_name,
						strlen(driver->name))) {
					printf("init (%u) %s\n", init_pri, devargs->virtual.drv_name);
					driver->init(devargs->virtual.drv_name,
						devargs->args);
					break;
				}
			}
		}

		/* If initializing pre PCI probe, then we don't expect a bonded driver
		 * to be found */
		if (init_pri == PMD_INIT_PRE_PCI_PROBE &&
				strncmp(PMD_BOND_NAME, devargs->virtual.drv_name,
					strlen(PMD_BOND_NAME)) != 0) {
			if (driver == NULL) {
				rte_panic("no driver found for virtual device %s\n",
					devargs->virtual.drv_name);
			}
		} else if (init_pri == PMD_INIT_POST_PCI_PROBE &&
				strncmp(PMD_BOND_NAME, devargs->virtual.drv_name,
					strlen(PMD_BOND_NAME)) == 0) {
			if (driver == NULL) {
				rte_panic("no driver found for bonded device %s\n",
					devargs->virtual.drv_name);
			}
		}
	}

	/* Once the vdevs are initialized, start calling all the pdev drivers */
	if (init_pri == PMD_INIT_PRE_PCI_PROBE) {
		TAILQ_FOREACH(driver, &dev_driver_list, next) {
			if (driver->type != PMD_PDEV)
				continue;
			/* PDEV drivers don't get passed any parameters */
			driver->init(NULL, NULL);
		}
	}
	return 0;
}
