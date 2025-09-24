/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_NTHW_PDB_H__
#define __FLOW_NTHW_PDB_H__

#include <stdint.h>

#include "nthw_fpga_model.h"

struct pdb_nthw {
	uint8_t m_physical_adapter_no;
	nthw_fpga_t *mp_fpga;

	nthw_module_t *m_pdb;

	nthw_register_t *mp_rcp_ctrl;
	nthw_field_t *mp_rcp_addr;
	nthw_field_t *mp_rcp_cnt;
	nthw_register_t *mp_rcp_data;
	nthw_field_t *mp_rcp_data_descriptor;
	nthw_field_t *mp_rcp_data_desc_len;
	nthw_field_t *mp_rcp_data_tx_port;
	nthw_field_t *mp_rcp_data_tx_ignore;
	nthw_field_t *mp_rcp_data_tx_now;
	nthw_field_t *mp_rcp_data_crc_overwrite;
	nthw_field_t *mp_rcp_data_align;
	nthw_field_t *mp_rcp_data_ofs0_dyn;
	nthw_field_t *mp_rcp_data_ofs0_rel;
	nthw_field_t *mp_rcp_data_ofs1_dyn;
	nthw_field_t *mp_rcp_data_ofs1_rel;
	nthw_field_t *mp_rcp_data_ofs2_dyn;
	nthw_field_t *mp_rcp_data_ofs2_rel;
	nthw_field_t *mp_rcp_data_ip_prot_tnl;
	nthw_field_t *mp_rcp_data_ppc_hsh;
	nthw_field_t *mp_rcp_data_duplicate_en;
	nthw_field_t *mp_rcp_data_duplicate_bit;
	nthw_field_t *mp_rcp_data_pcap_keep_fcs;

	nthw_register_t *mp_config;
	nthw_field_t *mp_config_ts_format;
	nthw_field_t *mp_config_port_ofs;
};

typedef struct pdb_nthw pdb_nthw_t;

struct pdb_nthw *nthw_pdb_new(void);
void nthw_pdb_delete(struct pdb_nthw *p);
int nthw_pdb_init(struct pdb_nthw *p, nthw_fpga_t *p_fpga, int n_instance);

int pdb_nthw_setup(struct pdb_nthw *p, int n_idx, int n_idx_cnt);
void nthw_pdb_set_debug_mode(struct pdb_nthw *p, unsigned int n_debug_mode);

/* RCP */
void nthw_pdb_rcp_select(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_rcp_cnt(const struct pdb_nthw *p, uint32_t val);

void nthw_pdb_rcp_descriptor(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_rcp_desc_len(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_rcp_tx_port(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_rcp_tx_ignore(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_rcp_tx_now(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_rcp_crc_overwrite(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_rcp_align(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_rcp_ofs0_dyn(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_rcp_ofs0_rel(const struct pdb_nthw *p, int32_t val);
void nthw_pdb_rcp_ofs1_dyn(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_rcp_ofs1_rel(const struct pdb_nthw *p, int32_t val);
void nthw_pdb_rcp_ofs2_dyn(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_rcp_ofs2_rel(const struct pdb_nthw *p, int32_t val);
void nthw_pdb_rcp_ip_prot_tnl(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_rcp_ppc_hsh(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_rcp_duplicate_en(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_rcp_duplicate_bit(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_rcp_flush(const struct pdb_nthw *p);

/* CONFIG */
void nthw_pdb_config_ts_format(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_config_port_ofs(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_config_port_ofs(const struct pdb_nthw *p, uint32_t val);
void nthw_pdb_config_flush(const struct pdb_nthw *p);

#endif	/* __FLOW_NTHW_PDB_H__ */
