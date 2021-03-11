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
extern uint32_t sfc_efx_logtype;

/** Log message, add a prefix and a line break */
#define SFC_EFX_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, sfc_efx_logtype,			\
		RTE_FMT("sfc_efx: " RTE_FMT_HEAD(__VA_ARGS__ ,) "\n",	\
			RTE_FMT_TAIL(__VA_ARGS__ ,)))

#endif /* _SFC_EFX_LOG_H_ */
