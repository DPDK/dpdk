/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell International Ltd.
 */

#include <rte_telemetry.h>

#include "cnxk_ethdev.h"

/* Macro to count no of words in eth_info_s size */
#define ETH_INFO_SZ                                                            \
	(RTE_ALIGN_CEIL(sizeof(struct eth_info_s), sizeof(uint64_t)) /         \
	 sizeof(uint64_t))
#define MACADDR_LEN 18

static int
ethdev_tel_handle_info(const char *cmd __rte_unused,
		       const char *params __rte_unused, struct rte_tel_data *d)
{
	struct rte_eth_dev *eth_dev;
	struct rte_tel_data *i_data;
	struct cnxk_eth_dev *dev;
	union eth_info_u {
		struct eth_info_s {
			/** PF/VF information */
			uint16_t pf_func;
			uint8_t max_mac_entries;
			bool dmac_filter_ena;
			uint8_t dmac_filter_count;
			uint8_t ptype_disable;
			bool scalar_ena;
			bool ptp_ena;
			/* Platform specific offload flags */
			uint16_t rx_offload_flags;
			uint16_t tx_offload_flags;
		} info;
		uint64_t val[ETH_INFO_SZ];
	} eth_info;
	struct eth_info_s *info;
	unsigned int i, j = 0;
	int n_ports;

	n_ports = rte_eth_dev_count_avail();
	if (!n_ports) {
		plt_err("No active ethernet ports found.");
		return -1;
	}

	rte_tel_data_start_dict(d);
	rte_tel_data_add_dict_int(d, "n_ports", n_ports);

	i_data = rte_tel_data_alloc();
	if (i_data == NULL)
		return -ENOMEM;
	rte_tel_data_start_array(i_data, RTE_TEL_U64_VAL);

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		/* Skip if port is unused */
		if (!rte_eth_dev_is_valid_port(i))
			continue;

		eth_dev = &rte_eth_devices[i];
		if (eth_dev) {
			memset(&eth_info, 0, sizeof(eth_info));
			info = &eth_info.info;
			dev = cnxk_eth_pmd_priv(eth_dev);
			if (dev) {
				info->pf_func = roc_nix_get_pf_func(&dev->nix);
				info->max_mac_entries = dev->max_mac_entries;
				info->dmac_filter_ena = dev->dmac_filter_enable;
				info->dmac_filter_count =
					dev->dmac_filter_count;
				info->ptype_disable = dev->ptype_disable;
				info->scalar_ena = dev->scalar_ena;
				info->ptp_ena = dev->ptp_en;
				info->rx_offload_flags = dev->rx_offload_flags;
				info->tx_offload_flags = dev->tx_offload_flags;
			}

			for (j = 0; j < ETH_INFO_SZ; j++)
				rte_tel_data_add_array_u64(i_data,
							   eth_info.val[j]);

			j++;
		}
	}

	rte_tel_data_add_dict_container(d, "info", i_data, 0);
	return 0;
}

RTE_INIT(cnxk_ethdev_init_telemetry)
{
	rte_telemetry_register_cmd("/cnxk/ethdev/info", ethdev_tel_handle_info,
				   "Returns ethdev device information");
}
