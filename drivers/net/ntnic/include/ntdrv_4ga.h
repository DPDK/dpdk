/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTDRV_4GA_H__
#define __NTDRV_4GA_H__


typedef struct ntdrv_4ga_s {
	uint32_t pciident;
	char *p_drv_name;

	volatile bool b_shutdown;
} ntdrv_4ga_t;

#endif	/* __NTDRV_4GA_H__ */
