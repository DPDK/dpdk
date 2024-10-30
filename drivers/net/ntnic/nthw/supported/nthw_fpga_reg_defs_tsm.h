/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#ifndef _NTHW_FPGA_REG_DEFS_TSM_
#define _NTHW_FPGA_REG_DEFS_TSM_

/* TSM */
#define TSM_CONFIG (0xef5dec83UL)
#define TSM_CONFIG_TS_FORMAT (0xe6efc2faUL)
#define TSM_TIMER_CTRL (0x648da051UL)
#define TSM_TIMER_CTRL_TIMER_EN_T0 (0x17cee154UL)
#define TSM_TIMER_CTRL_TIMER_EN_T1 (0x60c9d1c2UL)
#define TSM_TIMER_T0 (0x417217a5UL)
#define TSM_TIMER_T0_MAX_COUNT (0xaa601706UL)
#define TSM_TIMER_T1 (0x36752733UL)
#define TSM_TIMER_T1_MAX_COUNT (0x6beec8c6UL)
#define TSM_TIME_HI (0x175acea1UL)
#define TSM_TIME_HI_SEC (0xc0e9c9a1UL)
#define TSM_TIME_LO (0x9a55ae90UL)
#define TSM_TIME_LO_NS (0x879c5c4bUL)
#define TSM_TS_HI (0xccfe9e5eUL)
#define TSM_TS_HI_TIME (0xc23fed30UL)
#define TSM_TS_LO (0x41f1fe6fUL)
#define TSM_TS_LO_TIME (0xe0292a3eUL)

#endif	/* _NTHW_FPGA_REG_DEFS_TSM_ */
