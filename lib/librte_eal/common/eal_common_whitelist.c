/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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


/**
 * This file provides functions for managing a whitelist of devices. An EAL
 * command-line parameter should be used for specifying what devices to
 * whitelist, and the functions here should be called in handling that
 * parameter. Then when scanning the PCI bus, the is_whitelisted() function
 * can be used to query the previously set up whitelist.
 */
#include <string.h>
#include <rte_string_fns.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <ctype.h>
#ifdef RTE_LIBRTE_PMD_RING
#include <rte_eth_ring.h>
#endif
#ifdef RTE_LIBRTE_PMD_PCAP
#include <rte_eth_pcap.h>
#endif
#ifdef RTE_LIBRTE_PMD_XENVIRT
#include <rte_eth_xenvirt.h>
#endif
#include "eal_private.h"

static char dev_list_str[4096];
static size_t dev_list_str_len = 0;

/*
 * structure for storing a whitelist entry. Unlike for blacklists, we may
 * in future use this for dummy NICs not backed by a physical device, e.g.
 * backed by a file-system object instead, so we store the device path/addr
 * as a string, rather than as a PCI Bus-Device-Function.
 */
static struct whitelist_entry {
	const char *device_str;
	const char *device_params;
} whitelist[RTE_MAX_ETHPORTS] = { {NULL, NULL} };

static unsigned num_wl_entries = 0;

/* store a whitelist parameter for later parsing */
int
eal_dev_whitelist_add_entry(const char *entry)
{
	dev_list_str_len += rte_snprintf(&dev_list_str[dev_list_str_len],
			sizeof(dev_list_str)-dev_list_str_len, "%s,", entry);
	/* check for strings that won't fit (snprintf doesn't go beyond buffer) */
	if (dev_list_str_len >= sizeof(dev_list_str)) {
		dev_list_str_len = sizeof(dev_list_str) - 1;
		return -1;
	}

	return 0;
}

/* check if a whitelist has been set up */
int
eal_dev_whitelist_exists(void)
{
	return !!dev_list_str_len;
}

/* sanity checks a whitelist entry to ensure device is correct */
static int
is_valid_wl_entry(const char *device_str, size_t dev_buf_len)
{
#define NUM_PREFIXES (sizeof(non_pci_prefixes)/sizeof(non_pci_prefixes[0]))
	static const char *non_pci_prefixes[] = {
#ifdef  RTE_LIBRTE_PMD_RING
			RTE_ETH_RING_PARAM_NAME,
#endif
#ifdef RTE_LIBRTE_PMD_PCAP
			RTE_ETH_PCAP_PARAM_NAME,
#endif
#ifdef RTE_LIBRTE_PMD_XENVIRT
			RTE_ETH_XENVIRT_PARAM_NAME,
#endif
			"-nodev-" /* dummy value to prevent compiler warnings */
	};
	static uint8_t prefix_counts[NUM_PREFIXES] = {0};
	char buf[16];
	unsigned i;
	struct rte_pci_addr pci_addr = { .domain = 0 };

	if (eal_parse_pci_BDF(device_str, &pci_addr) == 0) {
		size_t n = rte_snprintf(buf, sizeof(buf), PCI_SHORT_PRI_FMT,
				pci_addr.bus, pci_addr.devid, pci_addr.function);
		return (n == dev_buf_len) && (!strncmp(buf, device_str, dev_buf_len));
	}
	if (eal_parse_pci_DomBDF(device_str, &pci_addr) == 0) {
		size_t n = rte_snprintf(buf, sizeof(buf), PCI_PRI_FMT, pci_addr.domain,
				pci_addr.bus, pci_addr.devid, pci_addr.function);
		return (n == dev_buf_len) && (!strncmp(buf, device_str, dev_buf_len));
	}
	for (i = 0; i < NUM_PREFIXES; i++) {
		size_t n = rte_snprintf(buf, sizeof(buf), "%s%u",
				non_pci_prefixes[i], prefix_counts[i]);
		if ((n == dev_buf_len) && (!strncmp(buf, device_str, dev_buf_len))) {
			prefix_counts[i]++;
			return 1;
		}
	}
	return 0;
}

/*
 * parse a whitelist string into a set of valid devices. To be called once
 * all parameters have been added to the whitelist string.
 */
int
eal_dev_whitelist_parse(void)
{
	char *devs[RTE_MAX_ETHPORTS + 1] = { NULL };
	int i, num_devs;
	unsigned dev_name_len, j;

	if (!eal_dev_whitelist_exists())
		return -1;

	/* strip off any extra commas */
	if (dev_list_str[dev_list_str_len - 1] == ',')
		dev_list_str[--dev_list_str_len] = '\0';

	/* split on commas into separate device entries */
	num_devs = rte_strsplit(dev_list_str, sizeof(dev_list_str), devs,
			RTE_MAX_ETHPORTS+1, ',');
	if (num_devs > RTE_MAX_ETHPORTS) {
		RTE_LOG(ERR, EAL, "Error, too many devices specified. "
				"[RTE_MAX_ETHPORTS = %u]\n", (unsigned)RTE_MAX_ETHPORTS);
		return -1;
	}

	size_t buf_len_rem = sizeof(dev_list_str); /* for tracking buffer length */
	for (i = 0; i < num_devs; i++) {
		char *dev_n_params[2]; /* possibly split device name from params*/

		size_t curr_len = strnlen(devs[i], buf_len_rem);
		buf_len_rem-= (curr_len + 1);

		int split_res = rte_strsplit(devs[i], curr_len, dev_n_params, 2, ';');

		/* device names go lower case, i.e. '00:0A.0' wouldn't work
		 * while '00:0a.0' would. */
		dev_name_len = strnlen(dev_n_params[0], curr_len);
		for (j = 0; j < dev_name_len; j++)
			dev_n_params[0][j] = (char)tolower((int)dev_n_params[0][j]);

		switch (split_res) {
		case 2:
			whitelist[i].device_params = dev_n_params[1]; /* fallthrough */
		case 1:
			whitelist[i].device_str = dev_n_params[0];
			break;
		default: /* should never ever occur */
			rte_panic("Fatal error parsing whitelist [--use-device] options\n");
		}

		if (!is_valid_wl_entry(whitelist[i].device_str,
				strnlen(whitelist[i].device_str, curr_len))) {
			RTE_LOG(ERR, EAL, "Error parsing device identifier: '%s'\n",
					whitelist[i].device_str);
			return -1;
		}
	}
	num_wl_entries = num_devs;
	return 0;
}

/* check if a device is on the whitelist */
int
eal_dev_is_whitelisted(const char *device_str, const char **params)
{
	unsigned i;
	if (!eal_dev_whitelist_exists())
		return 0; /* return false if no whitelist set up */

	for (i = 0; i < num_wl_entries; i++)
		if (strncmp(device_str, whitelist[i].device_str, 32) == 0) {
			if (params != NULL)
				*params = whitelist[i].device_params;
			return 1;
		}

	return 0;
}

/* clear the whole whitelist */
void
eal_dev_whitelist_clear(void)
{
	dev_list_str[0] = '\0';
	dev_list_str_len = num_wl_entries = 0;
}
