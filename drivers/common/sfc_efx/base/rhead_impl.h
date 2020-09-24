/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2018-2019 Solarflare Communications Inc.
 */

#ifndef	_SYS_RHEAD_IMPL_H
#define	_SYS_RHEAD_IMPL_H

#ifdef	__cplusplus
extern "C" {
#endif


#define	RHEAD_EVQ_MAXNEVS	16384
#define	RHEAD_EVQ_MINNEVS	256

#define	RHEAD_RXQ_MAXNDESCS	16384
#define	RHEAD_RXQ_MINNDESCS	256

#define	RHEAD_TXQ_MAXNDESCS	16384
#define	RHEAD_TXQ_MINNDESCS	256

#define	RHEAD_EVQ_DESC_SIZE	(sizeof (efx_qword_t))
#define	RHEAD_RXQ_DESC_SIZE	(sizeof (efx_qword_t))
#define	RHEAD_TXQ_DESC_SIZE	(sizeof (efx_oword_t))


/* NIC */

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_board_cfg(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_nic_probe(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_nic_set_drv_limits(
	__inout		efx_nic_t *enp,
	__in		efx_drv_limits_t *edlp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_nic_get_vi_pool(
	__in		efx_nic_t *enp,
	__out		uint32_t *vi_countp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_nic_get_bar_region(
	__in		efx_nic_t *enp,
	__in		efx_nic_region_t region,
	__out		uint32_t *offsetp,
	__out		size_t *sizep);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_nic_reset(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_nic_init(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn	boolean_t
rhead_nic_hw_unavailable(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern			void
rhead_nic_set_hw_unavailable(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_DIAG

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
rhead_nic_register_test(
	__in		efx_nic_t *enp);

#endif	/* EFSYS_OPT_DIAG */

LIBEFX_INTERNAL
extern			void
rhead_nic_fini(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern			void
rhead_nic_unprobe(
	__in		efx_nic_t *enp);


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_RHEAD_IMPL_H */
