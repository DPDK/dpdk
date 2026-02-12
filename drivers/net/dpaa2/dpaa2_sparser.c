/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2023 NXP
 */

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <dev_driver.h>

#include <fslmc_logs.h>
#include <fslmc_vfio.h>
#include <dpaa2_hw_pvt.h>
#include <dpaa2_hw_dpio.h>
#include <dpaa2_hw_mempool.h>
#include <dpaa2_pmd_logs.h>

#include "dpaa2_ethdev.h"
#include "dpaa2_sparser.h"
#include "base/dpaa2_hw_dpni_annot.h"
#define __STDC_FORMAT_MACROS
#include <stdint.h>
#include <inttypes.h>

static uint8_t wriop_bytecode[] = {
	0x00, 0x04, 0x29, 0x42, 0x03, 0xe0, 0x12, 0x00, 0x29, 0x02,
	0x18, 0x00, 0x87, 0x3c, 0x00, 0x02, 0x18, 0x00, 0x00, 0x00
};

int dpaa2_eth_load_wriop_soft_parser(struct dpaa2_dev_priv *priv,
				     enum dpni_soft_sequence_dest dest)
{
	struct fsl_mc_io *dpni = priv->hw;
	struct dpni_load_ss_cfg     cfg;
	struct dpni_drv_sparser_param	sp_param;
	uint8_t *addr;
	int ret;

	memset(&sp_param, 0, sizeof(sp_param));
	sp_param.start_pc = priv->ss_offset;
	sp_param.byte_code = &wriop_bytecode[0];
	sp_param.size = sizeof(wriop_bytecode);

	cfg.dest = dest;
	cfg.ss_offset = sp_param.start_pc;
	cfg.ss_size = sp_param.size;

	addr = rte_malloc(NULL, sp_param.size, 64);
	if (!addr) {
		DPAA2_PMD_ERR("Memory unavailable for soft parser param");
		return -1;
	}

	memcpy(addr, sp_param.byte_code, sp_param.size);
	cfg.ss_iova = DPAA2_VADDR_TO_IOVA_AND_CHECK(addr, sp_param.size);
	if (cfg.ss_iova == RTE_BAD_IOVA) {
		DPAA2_PMD_ERR("No IOMMU map for soft sequence(%p), size=%d",
			addr, sp_param.size);
		rte_free(addr);

		return -ENOBUFS;
	}

	ret = dpni_load_sw_sequence(dpni, CMD_PRI_LOW, priv->token, &cfg);
	if (ret) {
		DPAA2_PMD_ERR("dpni_load_sw_sequence failed");
		rte_free(addr);
		return ret;
	}

	priv->ss_iova = cfg.ss_iova;
	priv->ss_offset += sp_param.size;
	DPAA2_PMD_INFO("Soft parser loaded for dpni@%d", priv->hw_id);

	rte_free(addr);
	return 0;
}

int dpaa2_eth_enable_wriop_soft_parser(struct dpaa2_dev_priv *priv,
				       enum dpni_soft_sequence_dest dest)
{
	struct fsl_mc_io *dpni = priv->hw;
	struct dpni_enable_ss_cfg cfg;
	uint8_t pa[3];
	struct dpni_drv_sparser_param sp_param;
	uint8_t *param_addr = NULL;
	int ret;

	memset(&sp_param, 0, sizeof(sp_param));
	pa[0] = 32;	/* Custom Header Length in bytes */
	sp_param.custom_header_first = 1;
	sp_param.param_offset = 32;
	sp_param.param_size = 1;
	sp_param.start_pc = priv->ss_offset;
	sp_param.param_array = (uint8_t *)&pa[0];

	cfg.dest = dest;
	cfg.ss_offset = sp_param.start_pc;
	cfg.set_start = sp_param.custom_header_first;
	cfg.hxs = (uint16_t)sp_param.link_to_hard_hxs;
	cfg.param_offset = sp_param.param_offset;
	cfg.param_size = sp_param.param_size;
	if (cfg.param_size) {
		param_addr = rte_malloc(NULL, cfg.param_size, 64);
		if (!param_addr) {
			DPAA2_PMD_ERR("Memory unavailable for soft parser param");
			return -1;
		}

		memcpy(param_addr, sp_param.param_array, cfg.param_size);
		cfg.param_iova = DPAA2_VADDR_TO_IOVA_AND_CHECK(param_addr,
			cfg.param_size);
		if (cfg.param_iova == RTE_BAD_IOVA) {
			DPAA2_PMD_ERR("%s: No IOMMU map for %p, size=%d",
				__func__, param_addr, cfg.param_size);
			rte_free(param_addr);

			return -ENOBUFS;
		}
		priv->ss_param_iova = cfg.param_iova;
	} else {
		cfg.param_iova = 0;
	}

	ret = dpni_enable_sw_sequence(dpni, CMD_PRI_LOW, priv->token, &cfg);
	if (ret) {
		DPAA2_PMD_ERR("Soft parser enabled for dpni@%d failed",
			priv->hw_id);
		rte_free(param_addr);
		return ret;
	}

	rte_free(param_addr);
	DPAA2_PMD_INFO("Soft parser enabled for dpni@%d", priv->hw_id);
	return 0;
}
