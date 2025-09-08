/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTDRV_4GA_H__
#define __NTDRV_4GA_H__

#include "nt4ga_adapter.h"
#include <rte_spinlock.h>

typedef struct ntdrv_4ga_s {
	uint32_t pciident;
	struct adapter_info_s adapter_info;
	char *p_drv_name;

	volatile bool b_shutdown;
	rte_spinlock_t stat_lck;
} ntdrv_4ga_t;

#endif	/* __NTDRV_4GA_H__ */
