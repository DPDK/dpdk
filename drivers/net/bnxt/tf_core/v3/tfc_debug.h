/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TFC_DEBUG_H_
#define _TFC_DEBUG_H_

/* #define EM_DEBUG */
/* #define ACT_DEBUG */

int tfc_mpc_table_write_zero(struct tfc *tfcp,
			     uint8_t tsid,
			     enum cfa_dir dir,
			     uint32_t type,
			     uint32_t offset,
			     uint8_t words,
			     uint8_t *data);

int tfc_act_show(struct tfc *tfcp, uint8_t tsid, enum cfa_dir dir);
int tfc_em_show(struct tfc *tfcp, uint8_t tsid, enum cfa_dir dir);
int tfc_mpc_table_invalidate(struct tfc *tfcp,
			     uint8_t tsid,
			     enum cfa_dir dir,
			     uint32_t type,
			     uint32_t offset,
			     uint32_t words);
#endif
