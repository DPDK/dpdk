/*-
 * BSD LICENSE
 *
 * Copyright (c) 2015-2017 Atomic Rules LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 * * Neither the name of copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <unistd.h>
#include <sys/stat.h>
#include <dlfcn.h>

#include <rte_ethdev_pci.h>
#include <rte_kvargs.h>

#include "ark_global.h"
#include "ark_logs.h"
#include "ark_ethdev.h"

/*  Internal prototypes */
static int eth_ark_check_args(struct ark_adapter *ark, const char *params);
static int eth_ark_dev_init(struct rte_eth_dev *dev);
static int eth_ark_dev_uninit(struct rte_eth_dev *eth_dev);
static int eth_ark_dev_configure(struct rte_eth_dev *dev);
static void eth_ark_dev_info_get(struct rte_eth_dev *dev,
				 struct rte_eth_dev_info *dev_info);

/*
 * The packet generator is a functional block used to generate packet
 * patterns for testing.  It is not intended for nominal use.
 */
#define ARK_PKTGEN_ARG "Pkt_gen"

/*
 * The packet checker is a functional block used to verify packet
 * patterns for testing.  It is not intended for nominal use.
 */
#define ARK_PKTCHKR_ARG "Pkt_chkr"

/*
 * The packet director is used to select the internal ingress and
 * egress packets paths during testing.  It is not intended for
 * nominal use.
 */
#define ARK_PKTDIR_ARG "Pkt_dir"

/* Devinfo configurations */
#define ARK_RX_MAX_QUEUE (4096 * 4)
#define ARK_RX_MIN_QUEUE (512)
#define ARK_RX_MAX_PKT_LEN ((16 * 1024) - 128)
#define ARK_RX_MIN_BUFSIZE (1024)

#define ARK_TX_MAX_QUEUE (4096 * 4)
#define ARK_TX_MIN_QUEUE (256)

static const char * const valid_arguments[] = {
	ARK_PKTGEN_ARG,
	ARK_PKTCHKR_ARG,
	ARK_PKTDIR_ARG,
	NULL
};

static const struct rte_pci_id pci_id_ark_map[] = {
	{RTE_PCI_DEVICE(0x1d6c, 0x100d)},
	{RTE_PCI_DEVICE(0x1d6c, 0x100e)},
	{.vendor_id = 0, /* sentinel */ },
};

static int
eth_ark_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *eth_dev;
	int ret;

	eth_dev = rte_eth_dev_pci_allocate(pci_dev, sizeof(struct ark_adapter));

	if (eth_dev == NULL)
		return -ENOMEM;

	ret = eth_ark_dev_init(eth_dev);
	if (ret)
		rte_eth_dev_pci_release(eth_dev);

	return ret;
}

static int
eth_ark_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_ark_dev_uninit);
}

static struct rte_pci_driver rte_ark_pmd = {
	.id_table = pci_id_ark_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = eth_ark_pci_probe,
	.remove = eth_ark_pci_remove,
};

static const struct eth_dev_ops ark_eth_dev_ops = {
	.dev_configure = eth_ark_dev_configure,
	.dev_infos_get = eth_ark_dev_info_get,
};

static int
eth_ark_dev_init(struct rte_eth_dev *dev)
{
	struct ark_adapter *ark =
		(struct ark_adapter *)dev->data->dev_private;
	struct rte_pci_device *pci_dev;
	int ret = -1;

	ark->eth_dev = dev;

	PMD_FUNC_LOG(DEBUG, "\n");

	pci_dev = ARK_DEV_TO_PCI(dev);
	rte_eth_copy_pci_info(dev, pci_dev);

	ark->bar0 = (uint8_t *)pci_dev->mem_resource[0].addr;
	ark->a_bar = (uint8_t *)pci_dev->mem_resource[2].addr;

	dev->dev_ops = &ark_eth_dev_ops;
	dev->data->dev_flags |= RTE_ETH_DEV_DETACHABLE;

	if (pci_dev->device.devargs)
		ret = eth_ark_check_args(ark, pci_dev->device.devargs->args);
	else
		PMD_DRV_LOG(INFO, "No Device args found\n");

	return ret;
}

static int
eth_ark_dev_uninit(struct rte_eth_dev *dev)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	dev->dev_ops = NULL;
	dev->rx_pkt_burst = NULL;
	dev->tx_pkt_burst = NULL;
	return 0;
}

static int
eth_ark_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	PMD_FUNC_LOG(DEBUG, "\n");
	return 0;
}

static void
eth_ark_dev_info_get(struct rte_eth_dev *dev,
		     struct rte_eth_dev_info *dev_info)
{
	dev_info->max_rx_pktlen = ARK_RX_MAX_PKT_LEN;
	dev_info->min_rx_bufsize = ARK_RX_MIN_BUFSIZE;

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = ARK_RX_MAX_QUEUE,
		.nb_min = ARK_RX_MIN_QUEUE,
		.nb_align = ARK_RX_MIN_QUEUE}; /* power of 2 */

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = ARK_TX_MAX_QUEUE,
		.nb_min = ARK_TX_MIN_QUEUE,
		.nb_align = ARK_TX_MIN_QUEUE}; /* power of 2 */

	/* ARK PMD supports all line rates, how do we indicate that here ?? */
	dev_info->speed_capa = (ETH_LINK_SPEED_1G |
				ETH_LINK_SPEED_10G |
				ETH_LINK_SPEED_25G |
				ETH_LINK_SPEED_40G |
				ETH_LINK_SPEED_50G |
				ETH_LINK_SPEED_100G);
	dev_info->pci_dev = ARK_DEV_TO_PCI(dev);
}

static inline int
process_pktdir_arg(const char *key, const char *value,
		   void *extra_args)
{
	PMD_FUNC_LOG(DEBUG, "key = %s, value = %s\n",
		    key, value);
	struct ark_adapter *ark =
		(struct ark_adapter *)extra_args;

	ark->pkt_dir_v = strtol(value, NULL, 16);
	PMD_FUNC_LOG(DEBUG, "pkt_dir_v = 0x%x\n", ark->pkt_dir_v);
	return 0;
}

static inline int
process_file_args(const char *key, const char *value, void *extra_args)
{
	PMD_FUNC_LOG(DEBUG, "key = %s, value = %s\n",
		    key, value);
	char *args = (char *)extra_args;

	/* Open the configuration file */
	FILE *file = fopen(value, "r");
	char line[ARK_MAX_ARG_LEN];
	int  size = 0;
	int first = 1;

	while (fgets(line, sizeof(line), file)) {
		size += strlen(line);
		if (size >= ARK_MAX_ARG_LEN) {
			PMD_DRV_LOG(ERR, "Unable to parse file %s args, "
				    "parameter list is too long\n", value);
			fclose(file);
			return -1;
		}
		if (first) {
			strncpy(args, line, ARK_MAX_ARG_LEN);
			first = 0;
		} else {
			strncat(args, line, ARK_MAX_ARG_LEN);
		}
	}
	PMD_FUNC_LOG(DEBUG, "file = %s\n", args);
	fclose(file);
	return 0;
}

static int
eth_ark_check_args(struct ark_adapter *ark, const char *params)
{
	struct rte_kvargs *kvlist;
	unsigned int k_idx;
	struct rte_kvargs_pair *pair = NULL;

	kvlist = rte_kvargs_parse(params, valid_arguments);
	if (kvlist == NULL)
		return 0;

	ark->pkt_gen_args[0] = 0;
	ark->pkt_chkr_args[0] = 0;

	for (k_idx = 0; k_idx < kvlist->count; k_idx++) {
		pair = &kvlist->pairs[k_idx];
		PMD_FUNC_LOG(DEBUG, "**** Arg passed to PMD = %s:%s\n",
			     pair->key,
			     pair->value);
	}

	if (rte_kvargs_process(kvlist,
			       ARK_PKTDIR_ARG,
			       &process_pktdir_arg,
			       ark) != 0) {
		PMD_DRV_LOG(ERR, "Unable to parse arg %s\n", ARK_PKTDIR_ARG);
		return -1;
	}

	if (rte_kvargs_process(kvlist,
			       ARK_PKTGEN_ARG,
			       &process_file_args,
			       ark->pkt_gen_args) != 0) {
		PMD_DRV_LOG(ERR, "Unable to parse arg %s\n", ARK_PKTGEN_ARG);
		return -1;
	}

	if (rte_kvargs_process(kvlist,
			       ARK_PKTCHKR_ARG,
			       &process_file_args,
			       ark->pkt_chkr_args) != 0) {
		PMD_DRV_LOG(ERR, "Unable to parse arg %s\n", ARK_PKTCHKR_ARG);
		return -1;
	}

	PMD_DRV_LOG(INFO, "packet director set to 0x%x\n", ark->pkt_dir_v);

	return 0;
}

RTE_PMD_REGISTER_PCI(net_ark, rte_ark_pmd);
RTE_PMD_REGISTER_KMOD_DEP(net_ark, "* igb_uio | uio_pci_generic ");
RTE_PMD_REGISTER_PCI_TABLE(net_ark, pci_id_ark_map);
RTE_PMD_REGISTER_PARAM_STRING(net_ark,
			      ARK_PKTGEN_ARG "=<filename> "
			      ARK_PKTCHKR_ARG "=<filename> "
			      ARK_PKTDIR_ARG "=<bitmap>");
