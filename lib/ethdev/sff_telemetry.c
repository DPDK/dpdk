/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <errno.h>

#include "rte_ethdev.h"
#include <rte_common.h>
#include "sff_telemetry.h"
#include <telemetry_data.h>

static void
sff_port_module_eeprom_parse(uint16_t port_id, struct rte_tel_data *d)
{
	struct rte_eth_dev_module_info minfo;
	struct rte_dev_eeprom_info einfo;
	int ret;

	if (d == NULL) {
		RTE_ETHDEV_LOG(ERR, "Dict invalid\n");
		return;
	}

	ret = rte_eth_dev_get_module_info(port_id, &minfo);
	if (ret != 0) {
		switch (ret) {
		case -ENODEV:
			RTE_ETHDEV_LOG(ERR, "Port index %d invalid\n", port_id);
			break;
		case -ENOTSUP:
			RTE_ETHDEV_LOG(ERR, "Operation not supported by device\n");
			break;
		case -EIO:
			RTE_ETHDEV_LOG(ERR, "Device is removed\n");
			break;
		default:
			RTE_ETHDEV_LOG(ERR, "Unable to get port module info, %d\n", ret);
			break;
		}
		return;
	}

	einfo.offset = 0;
	einfo.length = minfo.eeprom_len;
	einfo.data = calloc(1, minfo.eeprom_len);
	if (einfo.data == NULL) {
		RTE_ETHDEV_LOG(ERR, "Allocation of port %u EEPROM data failed\n", port_id);
		return;
	}

	ret = rte_eth_dev_get_module_eeprom(port_id, &einfo);
	if (ret != 0) {
		switch (ret) {
		case -ENODEV:
			RTE_ETHDEV_LOG(ERR, "Port index %d invalid\n", port_id);
			break;
		case -ENOTSUP:
			RTE_ETHDEV_LOG(ERR, "Operation not supported by device\n");
			break;
		case -EIO:
			RTE_ETHDEV_LOG(ERR, "Device is removed\n");
			break;
		default:
			RTE_ETHDEV_LOG(ERR, "Unable to get port module EEPROM, %d\n", ret);
			break;
		}
		free(einfo.data);
		return;
	}

	switch (minfo.type) {
	/* parsing module EEPROM data base on different module type */
	default:
		RTE_ETHDEV_LOG(NOTICE, "Unsupported module type: %u\n", minfo.type);
		break;
	}

	free(einfo.data);
}

int
eth_dev_handle_port_module_eeprom(const char *cmd __rte_unused, const char *params,
				  struct rte_tel_data *d)
{
	char *end_param;
	int port_id;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	errno = 0;
	port_id = strtoul(params, &end_param, 0);

	if (errno != 0) {
		RTE_ETHDEV_LOG(ERR, "Invalid argument, %d\n", errno);
		return -1;
	}

	if (*end_param != '\0')
		RTE_ETHDEV_LOG(NOTICE,
			"Extra parameters [%s] passed to ethdev telemetry command, ignoring\n",
				end_param);

	rte_tel_data_start_dict(d);

	sff_port_module_eeprom_parse(port_id, d);

	return 0;
}
