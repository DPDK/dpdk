/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
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
 * 
 */
/*   BSD LICENSE
 *
 *   Copyright(c) 2013 6WIND.
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

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>

#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_pci.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>

#include "eal_private.h"

struct pci_driver_list driver_list;
struct pci_device_list device_list;

static struct rte_pci_addr *dev_blacklist = NULL;
static unsigned dev_blacklist_size = 0;

static int is_blacklisted(struct rte_pci_device *dev)
{
	struct rte_pci_addr *loc = &dev->addr;
	unsigned i;

	for (i = 0; i < dev_blacklist_size; i++) {
		if ((loc->domain == dev_blacklist[i].domain) &&
				(loc->bus == dev_blacklist[i].bus) &&
				(loc->devid == dev_blacklist[i].devid) &&
				(loc->function == dev_blacklist[i].function)) {
			return 1;
		}
	}

	return 0;           /* not in blacklist */
}

/*
 * If vendor/device ID match, call the devinit() function of all
 * registered driver for the given device. Return -1 if no driver is
 * found for this device.
 * For drivers with the RTE_PCI_DRV_MULTIPLE flag enabled, register
 * the same device multiple times until failure to do so.
 * It is required for non-Intel NIC drivers provided by third-parties such
 * as 6WIND.
 */
static int
pci_probe_all_drivers(struct rte_pci_device *dev)
{
	struct rte_pci_driver *dr = NULL;

	dev->blacklisted = !!is_blacklisted(dev);
	TAILQ_FOREACH(dr, &driver_list, next) {
		if (rte_eal_pci_probe_one_driver(dr, dev))
			continue;
		/* initialize subsequent driver instances for this device */
		if ((dr->drv_flags & RTE_PCI_DRV_MULTIPLE) &&
		    (!dev->blacklisted)) {
			while (rte_eal_pci_probe_one_driver(dr, dev) == 0)
				;
		}
		return 0;
	}
	return -1;
}

/*
 * Scan the content of the PCI bus, and call the devinit() function for
 * all registered drivers that have a matching entry in its id_table
 * for discovered devices.
 */
int
rte_eal_pci_probe(void)
{
	struct rte_pci_device *dev = NULL;

	TAILQ_FOREACH(dev, &device_list, next)
		pci_probe_all_drivers(dev);
	#ifdef RTE_EAL_UNBIND_PORTS
		if (atexit(rte_eal_pci_exit) != 0)
			RTE_LOG(ERR, EAL, "atexit failure\n");
	#endif
	return 0;
}

/* dump one device */
static int
pci_dump_one_device(struct rte_pci_device *dev)
{
	printf(PCI_PRI_FMT, dev->addr.domain, dev->addr.bus,
	       dev->addr.devid, dev->addr.function);
	printf(" - vendor:%x device:%x\n", dev->id.vendor_id,
	       dev->id.device_id);
	printf("   %16.16"PRIx64" %16.16"PRIx64"\n",
	       dev->mem_resource.phys_addr, dev->mem_resource.len);
	return 0;
}

/* dump devices on the bus */
void
rte_eal_pci_dump(void)
{
	struct rte_pci_device *dev = NULL;

	TAILQ_FOREACH(dev, &device_list, next) {
		pci_dump_one_device(dev);
	}
}

/* register a driver */
void
rte_eal_pci_register(struct rte_pci_driver *driver)
{
	TAILQ_INSERT_TAIL(&driver_list, driver, next);
}

void
rte_eal_pci_set_blacklist(struct rte_pci_addr *blacklist, unsigned size)
{
	dev_blacklist = blacklist;
	dev_blacklist_size = size;
}
