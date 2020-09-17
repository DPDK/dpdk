/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2012-2019 Solarflare Communications Inc.
 */

#ifndef _SYS_HUNT_IMPL_H
#define	_SYS_HUNT_IMPL_H

#include "efx.h"
#include "efx_regs.h"
#include "efx_regs_ef10.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define	HUNT_TXQ_MAXNDESCS			4096
#define	HUNT_TXQ_MAXNDESCS_BUG35388_WORKAROUND	2048

#define	HUNT_EVQ_MAXNBUFS	(64)

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

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
hunt_board_cfg(
	__in		efx_nic_t *enp);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_HUNT_IMPL_H */
