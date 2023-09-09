/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_DPI_PRIV_H_
#define _ROC_DPI_PRIV_H_

#define DPI_MAX_VFS 8

/* DPI PF DBDF information macros */
#define DPI_PF_DBDF_DEVICE   0
#define DPI_PF_DBDF_FUNCTION 0

#define DPI_QUEUE_OPEN	0x1
#define DPI_QUEUE_CLOSE 0x2
#define DPI_REG_DUMP	0x3
#define DPI_GET_REG_CFG 0x4

typedef union dpi_mbox_msg_t {
	uint64_t u[2];
	struct dpi_mbox_message_s {
		/* VF ID to configure */
		uint64_t vfid : 8;
		/* Command code */
		uint64_t cmd : 4;
		/* Command buffer size in 8-byte words */
		uint64_t csize : 14;
		/* aura of the command buffer */
		uint64_t aura : 20;
		/* SSO PF function */
		uint64_t sso_pf_func : 16;
		/* NPA PF function */
		uint64_t npa_pf_func : 16;
	} s;
} dpi_mbox_msg_t;

#endif
