/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2012-2018 Solarflare Communications Inc.
 * All rights reserved.
 */

#ifndef _SYS_HUNT_IMPL_H
#define	_SYS_HUNT_IMPL_H

#include "efx.h"
#include "efx_regs.h"
#include "efx_regs_ef10.h"
#include "efx_mcdi.h"

#ifdef	__cplusplus
extern "C" {
#endif

/* Missing register definitions */
#ifndef	ER_DZ_TX_PIOBUF_OFST
#define	ER_DZ_TX_PIOBUF_OFST 0x00001000
#endif
#ifndef	ER_DZ_TX_PIOBUF_STEP
#define	ER_DZ_TX_PIOBUF_STEP 8192
#endif
#ifndef	ER_DZ_TX_PIOBUF_ROWS
#define	ER_DZ_TX_PIOBUF_ROWS 2048
#endif

#ifndef	ER_DZ_TX_PIOBUF_SIZE
#define	ER_DZ_TX_PIOBUF_SIZE 2048
#endif

#define	HUNT_PIOBUF_NBUFS	(16)
#define	HUNT_PIOBUF_SIZE	(ER_DZ_TX_PIOBUF_SIZE)

#define	HUNT_MIN_PIO_ALLOC_SIZE	(HUNT_PIOBUF_SIZE / 32)


/* NIC */

extern	__checkReturn	efx_rc_t
hunt_board_cfg(
	__in		efx_nic_t *enp);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_HUNT_IMPL_H */
