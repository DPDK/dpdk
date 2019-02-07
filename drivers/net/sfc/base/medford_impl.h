/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2015-2018 Solarflare Communications Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MEDFORD_IMPL_H
#define	_SYS_MEDFORD_IMPL_H

#ifdef	__cplusplus
extern "C" {
#endif


#define	MEDFORD_TXQ_MAXNDESCS	2048

#define	MEDFORD_EVQ_MAXNBUFS	(64)

#ifndef	ER_EZ_TX_PIOBUF_SIZE
#define	ER_EZ_TX_PIOBUF_SIZE	4096
#endif


#define	MEDFORD_PIOBUF_NBUFS	(16)
#define	MEDFORD_PIOBUF_SIZE	(ER_EZ_TX_PIOBUF_SIZE)

#define	MEDFORD_MIN_PIO_ALLOC_SIZE	(MEDFORD_PIOBUF_SIZE / 32)


extern	__checkReturn	efx_rc_t
medford_board_cfg(
	__in		efx_nic_t *enp);


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MEDFORD_IMPL_H */
