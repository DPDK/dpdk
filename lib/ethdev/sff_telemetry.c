/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

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
	case RTE_ETH_MODULE_SFF_8079:
		sff_8079_show_all(einfo.data, d);
		break;
	case RTE_ETH_MODULE_SFF_8472:
		sff_8079_show_all(einfo.data, d);
		sff_8472_show_all(einfo.data, d);
		break;
	case RTE_ETH_MODULE_SFF_8436:
	case RTE_ETH_MODULE_SFF_8636:
		sff_8636_show_all(einfo.data, einfo.length, d);
		break;
	default:
		RTE_ETHDEV_LOG(NOTICE, "Unsupported module type: %u\n", minfo.type);
		break;
	}

	free(einfo.data);
}

void
ssf_add_dict_string(struct rte_tel_data *d, const char *name_str, const char *value_str)
{
	struct tel_dict_entry *e = &d->data.dict[d->data_len];

	if (d->type != TEL_DICT)
		return;
	if (d->data_len >= RTE_TEL_MAX_DICT_ENTRIES) {
		RTE_ETHDEV_LOG(ERR, "data_len has exceeded the maximum number of inserts\n");
		return;
	}

	e->type = RTE_TEL_STRING_VAL;
	/* append different values for same keys */
	if (d->data_len > 0) {
		struct tel_dict_entry *previous = &d->data.dict[d->data_len - 1];
		if (strcmp(previous->name, name_str) == 0) {
			strlcat(previous->value.sval, "; ", RTE_TEL_MAX_STRING_LEN);
			strlcat(previous->value.sval, value_str, RTE_TEL_MAX_STRING_LEN);
			goto end;
		}
	}
	strlcpy(e->value.sval, value_str, RTE_TEL_MAX_STRING_LEN);
	strlcpy(e->name, name_str, RTE_TEL_MAX_STRING_LEN);
	d->data_len++;

end:
	return;
}

int
eth_dev_handle_port_module_eeprom(const char *cmd __rte_unused, const char *params,
				  struct rte_tel_data *d)
{
	char *end_param;
	uint64_t port_id;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	errno = 0;
	port_id = strtoul(params, &end_param, 0);

	if (errno != 0 || port_id >= UINT16_MAX) {
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
