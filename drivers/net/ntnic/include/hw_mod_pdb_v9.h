/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _HW_MOD_PDB_V9_H_
#define _HW_MOD_PDB_V9_H_

#include <stdint.h>

struct pdb_v9_rcp_s {
	uint32_t descriptor;
	uint32_t desc_len;
	uint32_t tx_port;
	uint32_t tx_ignore;
	uint32_t tx_now;
	uint32_t crc_overwrite;
	uint32_t align;
	uint32_t ofs0_dyn;
	int32_t ofs0_rel;
	uint32_t ofs1_dyn;
	int32_t ofs1_rel;
	uint32_t ofs2_dyn;
	int32_t ofs2_rel;
	uint32_t ip_prot_tnl;
	uint32_t ppc_hsh;
	uint32_t duplicate_en;
	uint32_t duplicate_bit;
	uint32_t pcap_keep_fcs;	/* only field added to v9 cmp to v7/8 */
};

struct pdb_v9_config_s {
	uint32_t ts_format;
	uint32_t port_ofs;
};

struct hw_mod_pdb_v9_s {
	struct pdb_v9_rcp_s *rcp;
	struct pdb_v9_config_s *config;
};

#endif	/* _HW_MOD_PDB_V9_H_ */
