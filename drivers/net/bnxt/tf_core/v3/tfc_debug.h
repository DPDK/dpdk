/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TFC_DEBUG_H_
#define _TFC_DEBUG_H_

/* #define EM_DEBUG */
/* #define WC_DEBUG */
/* #define ACT_DEBUG */

int tfc_mpc_table_write_zero(struct tfc *tfcp,
			     uint8_t tsid,
			     enum cfa_dir dir,
			     uint32_t type,
			     uint32_t offset,
			     uint8_t words,
			     uint8_t *data);
const char *get_lrec_opcode_str(uint8_t opcode);
void act_show(FILE *fd, struct act_info_t *act_info, uint32_t offset);
int tfc_em_show(FILE *fd, struct tfc *tfcp, uint8_t tsid, enum cfa_dir dir);
int tfc_wc_show(FILE *fd, struct tfc *tfcp, uint8_t tsid, enum cfa_dir dir);
int tfc_mpc_table_invalidate(struct tfc *tfcp,
			     uint8_t tsid,
			     enum cfa_dir dir,
			     uint32_t type,
			     uint32_t offset,
			     uint32_t words);
void act_process(uint32_t act_rec_ptr,
		 struct act_info_t *act_info,
		 struct tfc_ts_mem_cfg *act_mem_cfg);
void tfc_backing_store_dump(FILE *fd);
#endif
