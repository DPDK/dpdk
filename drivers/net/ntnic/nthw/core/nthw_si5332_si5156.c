/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <pthread.h>
#include "nt_util.h"
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_si5332_si5156.h"

int nthw_pca9849_set_channel(nthw_pca9849_t *p, uint8_t channel)
{
	int res;

	if (p->m_current_channel != channel) {
		uint8_t data = channel + 4;
		res = nthw_i2cm_write(p->mp_nt_i2cm, p->m_dev_address, 0, data);

		if (res)
			return res;

		p->m_current_channel = channel;
		nt_os_wait_usec(10000);
	}

	return 0;
}
