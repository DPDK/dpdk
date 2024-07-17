/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_fpga.h"

#include "nthw_fpga_instances.h"
#include "nthw_fpga_mod_str_map.h"

#include <arpa/inet.h>

int nthw_fpga_get_param_info(struct fpga_info_s *p_fpga_info, nthw_fpga_t *p_fpga)
{
	mcu_info_t *p_mcu_info = &p_fpga_info->mcu_info;

	const int n_phy_ports = nthw_fpga_get_product_param(p_fpga, NT_PHY_PORTS, -1);
	const int n_phy_quads = nthw_fpga_get_product_param(p_fpga, NT_PHY_QUADS, -1);
	const int n_rx_ports = nthw_fpga_get_product_param(p_fpga, NT_RX_PORTS, -1);
	const int n_tx_ports = nthw_fpga_get_product_param(p_fpga, NT_TX_PORTS, -1);
	const int n_vf_offset = nthw_fpga_get_product_param(p_fpga, NT_HIF_VF_OFFSET, 4);

	p_fpga_info->n_phy_ports = n_phy_ports;
	p_fpga_info->n_phy_quads = n_phy_quads;
	p_fpga_info->n_rx_ports = n_rx_ports;
	p_fpga_info->n_tx_ports = n_tx_ports;
	p_fpga_info->n_vf_offset = n_vf_offset;
	p_fpga_info->profile = FPGA_INFO_PROFILE_UNKNOWN;

	/* Check for MCU */
	if (nthw_fpga_get_product_param(p_fpga, NT_MCU_PRESENT, 0) != 0) {
		p_mcu_info->mb_has_mcu = true;
		/* Check MCU Type */
		p_mcu_info->mn_mcu_type = nthw_fpga_get_product_param(p_fpga, NT_MCU_TYPE, -1);
		/* MCU DRAM size */
		p_mcu_info->mn_mcu_dram_size =
			nthw_fpga_get_product_param(p_fpga, NT_MCU_DRAM_SIZE, -1);

	} else {
		p_mcu_info->mb_has_mcu = false;
		p_mcu_info->mn_mcu_type = -1;
		p_mcu_info->mn_mcu_dram_size = -1;
	}

	/* Check for VSWITCH FPGA */
	if (nthw_fpga_get_product_param(p_fpga, NT_NFV_OVS_PRODUCT, 0) != 0) {
		p_fpga_info->profile = FPGA_INFO_PROFILE_VSWITCH;

	} else if (nthw_fpga_get_product_param(p_fpga, NT_IOA_PRESENT, 0) != 0) {
		/* Check for VSWITCH FPGA - legacy */
		p_fpga_info->profile = FPGA_INFO_PROFILE_VSWITCH;

	} else if (nthw_fpga_get_product_param(p_fpga, NT_QM_PRESENT, 0) != 0) {
		p_fpga_info->profile = FPGA_INFO_PROFILE_CAPTURE;

	} else {
		p_fpga_info->profile = FPGA_INFO_PROFILE_INLINE;
	}

	return 0;
}

int nthw_fpga_init(struct fpga_info_s *p_fpga_info)
{
	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;

	nthw_hif_t *p_nthw_hif = NULL;
	nthw_pcie3_t *p_nthw_pcie3 = NULL;
	nthw_rac_t *p_nthw_rac = NULL;

	mcu_info_t *p_mcu_info = &p_fpga_info->mcu_info;
	uint64_t n_fpga_ident = 0;
	nthw_fpga_mgr_t *p_fpga_mgr = NULL;
	nthw_fpga_t *p_fpga = NULL;

	char s_fpga_prod_ver_rev_str[32] = { 0 };

	int res = 0;

	assert(p_fpga_info);

	{
		const uint64_t n_fpga_ident = nthw_fpga_read_ident(p_fpga_info);
		const uint32_t n_fpga_build_time = nthw_fpga_read_buildtime(p_fpga_info);
		const int n_fpga_type_id = nthw_fpga_extract_type_id(n_fpga_ident);
		const int n_fpga_prod_id = nthw_fpga_extract_prod_id(n_fpga_ident);
		const int n_fpga_ver_id = nthw_fpga_extract_ver_id(n_fpga_ident);
		const int n_fpga_rev_id = nthw_fpga_extract_rev_id(n_fpga_ident);

		p_fpga_info->n_fpga_ident = n_fpga_ident;
		p_fpga_info->n_fpga_type_id = n_fpga_type_id;
		p_fpga_info->n_fpga_prod_id = n_fpga_prod_id;
		p_fpga_info->n_fpga_ver_id = n_fpga_ver_id;
		p_fpga_info->n_fpga_rev_id = n_fpga_rev_id;
		p_fpga_info->n_fpga_build_time = n_fpga_build_time;

		snprintf(s_fpga_prod_ver_rev_str, sizeof(s_fpga_prod_ver_rev_str),
			"%04d-%04d-%02d-%02d", n_fpga_type_id, n_fpga_prod_id, n_fpga_ver_id,
			n_fpga_rev_id);

		NT_LOG(INF, NTHW, "%s: FPGA %s (%" PRIX64 ") [%08X]\n", p_adapter_id_str,
			s_fpga_prod_ver_rev_str, n_fpga_ident, n_fpga_build_time);
	}

	n_fpga_ident = p_fpga_info->n_fpga_ident;

	p_fpga_mgr = nthw_fpga_mgr_new();
	nthw_fpga_mgr_init(p_fpga_mgr, nthw_fpga_instances,
		(const void *)sa_nthw_fpga_mod_str_map);
	nthw_fpga_mgr_log_dump(p_fpga_mgr);
	p_fpga = nthw_fpga_mgr_query_fpga(p_fpga_mgr, n_fpga_ident, p_fpga_info);
	p_fpga_info->mp_fpga = p_fpga;

	if (p_fpga == NULL) {
		NT_LOG(ERR, NTHW, "%s: Unsupported FPGA: %s (%08X)\n", p_adapter_id_str,
			s_fpga_prod_ver_rev_str, p_fpga_info->n_fpga_build_time);
		return -1;
	}

	if (p_fpga_mgr) {
		nthw_fpga_mgr_delete(p_fpga_mgr);
		p_fpga_mgr = NULL;
	}

	/* Read Fpga param info */
	nthw_fpga_get_param_info(p_fpga_info, p_fpga);

	/* debug: report params */
	NT_LOG(DBG, NTHW, "%s: NT_PHY_PORTS=%d\n", p_adapter_id_str, p_fpga_info->n_phy_ports);
	NT_LOG(DBG, NTHW, "%s: NT_PHY_QUADS=%d\n", p_adapter_id_str, p_fpga_info->n_phy_quads);
	NT_LOG(DBG, NTHW, "%s: NT_RX_PORTS=%d\n", p_adapter_id_str, p_fpga_info->n_rx_ports);
	NT_LOG(DBG, NTHW, "%s: NT_TX_PORTS=%d\n", p_adapter_id_str, p_fpga_info->n_tx_ports);
	NT_LOG(DBG, NTHW, "%s: nProfile=%d\n", p_adapter_id_str, (int)p_fpga_info->profile);
	NT_LOG(DBG, NTHW, "%s: bHasMcu=%d\n", p_adapter_id_str, p_mcu_info->mb_has_mcu);
	NT_LOG(DBG, NTHW, "%s: McuType=%d\n", p_adapter_id_str, p_mcu_info->mn_mcu_type);
	NT_LOG(DBG, NTHW, "%s: McuDramSize=%d\n", p_adapter_id_str, p_mcu_info->mn_mcu_dram_size);

	p_nthw_rac = nthw_rac_new();

	if (p_nthw_rac == NULL) {
		NT_LOG(ERR, NTHW, "%s: Unsupported FPGA: RAC is not found: %s (%08X)\n",
			p_adapter_id_str, s_fpga_prod_ver_rev_str, p_fpga_info->n_fpga_build_time);
		return -1;
	}

	nthw_rac_init(p_nthw_rac, p_fpga, p_fpga_info);
	nthw_rac_rab_flush(p_nthw_rac);
	p_fpga_info->mp_nthw_rac = p_nthw_rac;

	bool included = true;
	struct nt200a0x_ops *nt200a0x_ops = get_nt200a0x_ops();

	switch (p_fpga_info->n_nthw_adapter_id) {
	case NT_HW_ADAPTER_ID_NT200A02:
		if (nt200a0x_ops != NULL)
			res = nt200a0x_ops->nthw_fpga_nt200a0x_init(p_fpga_info);

		else
			included = false;

		break;
	default:
		NT_LOG(ERR, NTHW, "%s: Unsupported HW product id: %d\n", p_adapter_id_str,
			p_fpga_info->n_nthw_adapter_id);
		res = -1;
		break;
	}

	if (!included) {
		NT_LOG(ERR, NTHW, "%s: NOT INCLUDED HW product: %d\n", p_adapter_id_str,
			p_fpga_info->n_nthw_adapter_id);
		res = -1;
	}

	if (res) {
		NT_LOG(ERR, NTHW, "%s: status: 0x%08X\n", p_adapter_id_str, res);
		return res;
	}

	res = nthw_pcie3_init(NULL, p_fpga, 0);	/* Probe for module */

	if (res == 0) {
		p_nthw_pcie3 = nthw_pcie3_new();

		if (p_nthw_pcie3) {
			res = nthw_pcie3_init(p_nthw_pcie3, p_fpga, 0);

			if (res == 0) {
				NT_LOG(DBG, NTHW, "%s: Pcie3 module found\n", p_adapter_id_str);
				nthw_pcie3_trigger_sample_time(p_nthw_pcie3);

			} else {
				nthw_pcie3_delete(p_nthw_pcie3);
				p_nthw_pcie3 = NULL;
			}
		}

		p_fpga_info->mp_nthw_pcie3 = p_nthw_pcie3;
	}

	if (p_nthw_pcie3 == NULL) {
		p_nthw_hif = nthw_hif_new();

		if (p_nthw_hif) {
			res = nthw_hif_init(p_nthw_hif, p_fpga, 0);

			if (res == 0) {
				NT_LOG(DBG, NTHW, "%s: Hif module found\n", p_adapter_id_str);
				nthw_hif_trigger_sample_time(p_nthw_hif);

			} else {
				nthw_hif_delete(p_nthw_hif);
				p_nthw_hif = NULL;
			}
		}
	}

	p_fpga_info->mp_nthw_hif = p_nthw_hif;


	return res;
}

int nthw_fpga_shutdown(struct fpga_info_s *p_fpga_info)
{
	int res = -1;

	if (p_fpga_info) {
		if (p_fpga_info && p_fpga_info->mp_nthw_rac)
			res = nthw_rac_rab_reset(p_fpga_info->mp_nthw_rac);
	}

	return res;
}

static struct nt200a0x_ops *nt200a0x_ops;

void register_nt200a0x_ops(struct nt200a0x_ops *ops)
{
	nt200a0x_ops = ops;
}

struct nt200a0x_ops *get_nt200a0x_ops(void)
{
	if (nt200a0x_ops == NULL)
		nt200a0x_ops_init();
	return nt200a0x_ops;
}
