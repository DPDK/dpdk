/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#include "nfp_logs.h"
#include "nfp_nsp.h"

struct nsp_identify {
	uint8_t version[40];
	uint8_t flags;
	uint8_t br_primary;
	uint8_t br_secondary;
	uint8_t br_nsp;
	uint16_t primary;
	uint16_t secondary;
	uint16_t nsp;
	uint8_t reserved[6];
	uint64_t sensor_mask;
};

struct nfp_nsp_identify *
nfp_nsp_identify(struct nfp_nsp *nsp)
{
	int ret;
	struct nsp_identify *ni;
	struct nfp_nsp_identify *nspi = NULL;

	if (nfp_nsp_get_abi_ver_minor(nsp) < 15)
		return NULL;

	ni = malloc(sizeof(*ni));
	if (ni == NULL)
		return NULL;

	memset(ni, 0, sizeof(*ni));
	ret = nfp_nsp_read_identify(nsp, ni, sizeof(*ni));
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "reading bsp version failed %d", ret);
		goto exit_free;
	}

	nspi = malloc(sizeof(*nspi));
	if (nspi == NULL)
		goto exit_free;

	memset(nspi, 0, sizeof(*nspi));
	memcpy(nspi->version, ni->version, sizeof(nspi->version));
	nspi->version[sizeof(nspi->version) - 1] = '\0';
	nspi->flags = ni->flags;
	nspi->br_primary = ni->br_primary;
	nspi->br_secondary = ni->br_secondary;
	nspi->br_nsp = ni->br_nsp;
	nspi->primary = rte_le_to_cpu_16(ni->primary);
	nspi->secondary = rte_le_to_cpu_16(ni->secondary);
	nspi->nsp = rte_le_to_cpu_16(ni->nsp);
	nspi->sensor_mask = rte_le_to_cpu_64(ni->sensor_mask);

exit_free:
	free(ni);
	return nspi;
}

struct nfp_sensors {
	uint32_t chip_temp;
	uint32_t assembly_power;
	uint32_t assembly_12v_power;
	uint32_t assembly_3v3_power;
};

int
nfp_hwmon_read_sensor(struct nfp_cpp *cpp,
		enum nfp_nsp_sensor_id id,
		uint32_t *val)
{
	int ret;
	struct nfp_nsp *nsp;
	struct nfp_sensors s;

	nsp = nfp_nsp_open(cpp);
	if (nsp == NULL)
		return -EIO;

	ret = nfp_nsp_read_sensors(nsp, RTE_BIT32(id), &s, sizeof(s));
	nfp_nsp_close(nsp);

	if (ret < 0)
		return ret;

	switch (id) {
	case NFP_SENSOR_CHIP_TEMPERATURE:
		*val = rte_le_to_cpu_32(s.chip_temp);
		break;
	case NFP_SENSOR_ASSEMBLY_POWER:
		*val = rte_le_to_cpu_32(s.assembly_power);
		break;
	case NFP_SENSOR_ASSEMBLY_12V_POWER:
		*val = rte_le_to_cpu_32(s.assembly_12v_power);
		break;
	case NFP_SENSOR_ASSEMBLY_3V3_POWER:
		*val = rte_le_to_cpu_32(s.assembly_3v3_power);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
