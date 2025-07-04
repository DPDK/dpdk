/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_EFX_LOG_H_
#define _SFC_EFX_LOG_H_

/** Generic driver log type */
extern int sfc_efx_logtype;
#define RTE_LOGTYPE_SFC_EFX sfc_efx_logtype

/** Log message, add a prefix and a line break */
#define SFC_EFX_LOG(level, ...) \
	RTE_LOG_LINE(level, SFC_EFX, ## __VA_ARGS__)

#endif /* _SFC_EFX_LOG_H_ */
