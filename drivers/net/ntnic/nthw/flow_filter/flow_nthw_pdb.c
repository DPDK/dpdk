/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdlib.h>
#include <string.h>

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "flow_nthw_pdb.h"

void nthw_pdb_set_debug_mode(struct pdb_nthw *p, unsigned int n_debug_mode)
{
	nthw_module_set_debug_mode(p->m_pdb, n_debug_mode);
}

struct pdb_nthw *nthw_pdb_new(void)
{
	struct pdb_nthw *p = malloc(sizeof(struct pdb_nthw));

	if (p)
		memset(p, 0, sizeof(*p));

	return p;
}

void nthw_pdb_delete(struct pdb_nthw *p)
{
	if (p) {
		memset(p, 0, sizeof(*p));
		free(p);
	}
}

int nthw_pdb_init(struct pdb_nthw *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_PDB, n_instance);
	RTE_ASSERT(n_instance >= 0 && n_instance < 256);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: Pdb %d: no such instance", p_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->m_physical_adapter_no = (uint8_t)n_instance;
	p->m_pdb = p_mod;

	/* RCP */
	p->mp_rcp_ctrl = nthw_module_get_register(p->m_pdb, PDB_RCP_CTRL);
	p->mp_rcp_addr = nthw_register_get_field(p->mp_rcp_ctrl, PDB_RCP_CTRL_ADR);
	p->mp_rcp_cnt = nthw_register_get_field(p->mp_rcp_ctrl, PDB_RCP_CTRL_CNT);
	p->mp_rcp_data = nthw_module_get_register(p->m_pdb, PDB_RCP_DATA);
	p->mp_rcp_data_descriptor =
		nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_DESCRIPTOR);
	p->mp_rcp_data_desc_len = nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_DESC_LEN);
	p->mp_rcp_data_tx_port = nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_TX_PORT);
	p->mp_rcp_data_tx_ignore = nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_TX_IGNORE);
	p->mp_rcp_data_tx_now = nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_TX_NOW);
	p->mp_rcp_data_crc_overwrite =
		nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_CRC_OVERWRITE);
	p->mp_rcp_data_align = nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_ALIGN);
	p->mp_rcp_data_ofs0_dyn = nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_OFS0_DYN);
	p->mp_rcp_data_ofs0_rel = nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_OFS0_REL);
	p->mp_rcp_data_ofs1_dyn = nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_OFS1_DYN);
	p->mp_rcp_data_ofs1_rel = nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_OFS1_REL);
	p->mp_rcp_data_ofs2_dyn = nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_OFS2_DYN);
	p->mp_rcp_data_ofs2_rel = nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_OFS2_REL);
	p->mp_rcp_data_ip_prot_tnl =
		nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_IP_PROT_TNL);
	p->mp_rcp_data_ppc_hsh = nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_PPC_HSH);
	p->mp_rcp_data_duplicate_en =
		nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_DUPLICATE_EN);
	p->mp_rcp_data_duplicate_bit =
		nthw_register_get_field(p->mp_rcp_data, PDB_RCP_DATA_DUPLICATE_BIT);
	p->mp_rcp_data_pcap_keep_fcs =
		nthw_register_query_field(p->mp_rcp_data, PDB_RCP_DATA_PCAP_KEEP_FCS);
	/* CONFIG */
	p->mp_config = nthw_module_get_register(p->m_pdb, PDB_CONFIG);
	p->mp_config_ts_format = nthw_register_get_field(p->mp_config, PDB_CONFIG_TS_FORMAT);
	p->mp_config_port_ofs = nthw_register_get_field(p->mp_config, PDB_CONFIG_PORT_OFS);

	return 0;
}

/* RCP */
void nthw_pdb_rcp_select(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_addr, val);
}

void nthw_pdb_rcp_cnt(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_cnt, val);
}

void nthw_pdb_rcp_descriptor(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_descriptor, val);
}

void nthw_pdb_rcp_desc_len(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_desc_len, val);
}

void nthw_pdb_rcp_tx_port(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_tx_port, val);
}

void nthw_pdb_rcp_tx_ignore(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_tx_ignore, val);
}

void nthw_pdb_rcp_tx_now(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_tx_now, val);
}

void nthw_pdb_rcp_crc_overwrite(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_crc_overwrite, val);
}

void nthw_pdb_rcp_align(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_align, val);
}

void nthw_pdb_rcp_ofs0_dyn(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_ofs0_dyn, val);
}

void nthw_pdb_rcp_ofs0_rel(const struct pdb_nthw *p, int32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_ofs0_rel, val);
}

void nthw_pdb_rcp_ofs1_dyn(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_ofs1_dyn, val);
}

void nthw_pdb_rcp_ofs1_rel(const struct pdb_nthw *p, int32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_ofs1_rel, val);
}

void nthw_pdb_rcp_ofs2_dyn(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_ofs2_dyn, val);
}

void nthw_pdb_rcp_ofs2_rel(const struct pdb_nthw *p, int32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_ofs2_rel, val);
}

void nthw_pdb_rcp_ip_prot_tnl(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_ip_prot_tnl, val);
}

void nthw_pdb_rcp_ppc_hsh(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_ppc_hsh, val);
}

void nthw_pdb_rcp_duplicate_en(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_duplicate_en, val);
}

void nthw_pdb_rcp_duplicate_bit(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_duplicate_bit, val);
}

void nthw_pdb_rcp_flush(const struct pdb_nthw *p)
{
	nthw_register_flush(p->mp_rcp_ctrl, 1);
	nthw_register_flush(p->mp_rcp_data, 1);
}

/* CONFIG */
void nthw_pdb_config_ts_format(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_config_ts_format, val);
}

void nthw_pdb_config_port_ofs(const struct pdb_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_config_port_ofs, val);
}

void nthw_pdb_config_flush(const struct pdb_nthw *p)
{
	nthw_register_flush(p->mp_config, 1);
}
