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
#include <rte_string_fns.h>
#ifdef RTE_LIBRTE_PMD_RING
#include <rte_eth_ring.h>
#endif
#ifdef RTE_LIBRTE_PMD_PCAP
#include <rte_eth_pcap.h>
#endif
#ifdef RTE_LIBRTE_PMD_XENVIRT
#include <rte_eth_xenvirt.h>
#endif
#include <rte_debug.h>
#include <rte_devargs.h>
#include "eal_private.h"

struct device_init {
	const char *dev_prefix;
	int (*init_fn)(const char*, const char *);
};

#define NUM_DEV_TYPES (sizeof(dev_types)/sizeof(dev_types[0]))
struct device_init dev_types[] = {
#ifdef RTE_LIBRTE_PMD_RING
		{
			.dev_prefix = RTE_ETH_RING_PARAM_NAME,
			.init_fn = rte_pmd_ring_init
		},
#endif
#ifdef RTE_LIBRTE_PMD_PCAP
		{
			.dev_prefix = RTE_ETH_PCAP_PARAM_NAME,
			.init_fn = rte_pmd_pcap_init
		},
#endif
#ifdef RTE_LIBRTE_PMD_XENVIRT
		{
			.dev_prefix = RTE_ETH_XENVIRT_PARAM_NAME,
			.init_fn = rte_pmd_xenvirt_init
		},
#endif
		{
			.dev_prefix = "-nodev-",
			.init_fn = NULL
		}
};

int
rte_eal_non_pci_ethdev_init(void)
{
	struct rte_devargs *devargs;
	uint8_t i;

	/* call the init function for each virtual device */
	TAILQ_FOREACH(devargs, &devargs_list, next) {

		if (devargs->type != RTE_DEVTYPE_VIRTUAL)
			continue;

		for (i = 0; i < NUM_DEV_TYPES; i++) {
			/* search a driver prefix in virtual device name */
			if (!strncmp(dev_types[i].dev_prefix,
				    devargs->virtual.drv_name,
				     sizeof(dev_types[i].dev_prefix) - 1)) {
				dev_types[i].init_fn(devargs->virtual.drv_name,
						     devargs->args);
				break;
			}
		}

		if (i == NUM_DEV_TYPES) {
			rte_panic("no driver found for %s\n",
				  devargs->virtual.drv_name);
		}
	}
	return 0;
}
