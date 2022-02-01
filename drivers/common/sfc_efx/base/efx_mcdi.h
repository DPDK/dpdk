/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2009-2019 Solarflare Communications Inc.
 */

#ifndef _SYS_EFX_MCDI_H
#define	_SYS_EFX_MCDI_H

#include "efx.h"
#include "efx_regs_mcdi.h"

#if EFSYS_OPT_NAMES
#include "efx_regs_mcdi_strs.h"
#endif /* EFSYS_OPT_NAMES */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * A reboot/assertion causes the MCDI status word to be set after the
 * command word is set or a REBOOT event is sent. If we notice a reboot
 * via these mechanisms then wait 10ms for the status word to be set.
 */
#define	EFX_MCDI_STATUS_SLEEP_US	10000

struct efx_mcdi_req_s {
	boolean_t	emr_quiet;
	/* Inputs: Command #, input buffer and length */
	unsigned int	emr_cmd;
	uint8_t		*emr_in_buf;
	size_t		emr_in_length;
	/* Outputs: retcode, buffer, length and length used */
	efx_rc_t	emr_rc;
	uint8_t		*emr_out_buf;
	size_t		emr_out_length;
	size_t		emr_out_length_used;
	/* Internals: low level transport details */
	unsigned int	emr_err_code;
	unsigned int	emr_err_arg;
#if EFSYS_OPT_MCDI_PROXY_AUTH
	uint32_t	emr_proxy_handle;
#endif
};

typedef struct efx_mcdi_iface_s {
	unsigned int		emi_port;
	unsigned int		emi_max_version;
	unsigned int		emi_seq;
	efx_mcdi_req_t		*emi_pending_req;
	boolean_t		emi_ev_cpl;
	boolean_t		emi_new_epoch;
	int			emi_aborted;
	uint32_t		emi_poll_cnt;
	uint32_t		emi_mc_reboot_status;
} efx_mcdi_iface_t;

LIBEFX_INTERNAL
extern			void
efx_mcdi_execute(
	__in		efx_nic_t *enp,
	__inout		efx_mcdi_req_t *emrp);

LIBEFX_INTERNAL
extern			void
efx_mcdi_execute_quiet(
	__in		efx_nic_t *enp,
	__inout		efx_mcdi_req_t *emrp);

LIBEFX_INTERNAL
extern			void
efx_mcdi_ev_cpl(
	__in		efx_nic_t *enp,
	__in		unsigned int seq,
	__in		unsigned int outlen,
	__in		int errcode);

#if EFSYS_OPT_MCDI_PROXY_AUTH
LIBEFX_API
extern	__checkReturn	efx_rc_t
efx_mcdi_get_proxy_handle(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp,
	__out		uint32_t *handlep);

LIBEFX_INTERNAL
extern			void
efx_mcdi_ev_proxy_response(
	__in		efx_nic_t *enp,
	__in		unsigned int handle,
	__in		unsigned int status);
#endif

#if EFSYS_OPT_MCDI_PROXY_AUTH_SERVER
LIBEFX_INTERNAL
extern			void
efx_mcdi_ev_proxy_request(
	__in		efx_nic_t *enp,
	__in		unsigned int index);
#endif /* EFSYS_OPT_MCDI_PROXY_AUTH_SERVER */

LIBEFX_INTERNAL
extern			void
efx_mcdi_ev_death(
	__in		efx_nic_t *enp,
	__in		int rc);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_request_errcode(
	__in		unsigned int err);

LIBEFX_INTERNAL
extern			void
efx_mcdi_raise_exception(
	__in		efx_nic_t *enp,
	__in_opt	efx_mcdi_req_t *emrp,
	__in		int rc);

/*
 * Flags that name portions of extended version information
 *
 * The values match their MCDI counterparts.
 */
#define	EFX_MCDI_VERSION_BOARD_INFO	(1U << 4)

typedef struct efx_mcdi_version_s {
	/* Basic version information */
	uint16_t		emv_version[4];
	uint32_t		emv_firmware;

	/*
	 * Extended version information
	 *
	 * Valid portions of obtained information are indicated by flags.
	 */
	uint32_t		emv_flags;

	/* Information valid if emv_flags has EFX_MCDI_VERSION_BOARD_INFO set */
	efx_nic_board_info_t	emv_board_info;
} efx_mcdi_version_t;

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_get_version(
	__in		efx_nic_t *enp,
	__in		uint32_t flags_req,
	__out		efx_mcdi_version_t *verp);

typedef enum efx_mcdi_boot_e {
	EFX_MCDI_BOOT_PRIMARY,
	EFX_MCDI_BOOT_SECONDARY,
	EFX_MCDI_BOOT_ROM,
} efx_mcdi_boot_t;

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_version(
	__in			efx_nic_t *enp,
	__out_ecount_opt(4)	uint16_t versionp[4],
	__out_opt		uint32_t *buildp,
	__out_opt		efx_mcdi_boot_t *statusp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_get_capabilities(
	__in		efx_nic_t *enp,
	__out_opt	uint32_t *flagsp,
	__out_opt	uint16_t *rx_dpcpu_fw_idp,
	__out_opt	uint16_t *tx_dpcpu_fw_idp,
	__out_opt	uint32_t *flags2p,
	__out_opt	uint32_t *tso2ncp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_read_assertion(
	__in			efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_exit_assertion_handler(
	__in			efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_drv_attach(
	__in			efx_nic_t *enp,
	__in			boolean_t attach);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_get_board_cfg(
	__in			efx_nic_t *enp,
	__out_opt		uint32_t *board_typep,
	__out_opt		efx_dword_t *capabilitiesp,
	__out_ecount_opt(6)	uint8_t mac_addrp[6]);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_get_phy_cfg(
	__in			efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_firmware_update_supported(
	__in			efx_nic_t *enp,
	__out			boolean_t *supportedp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_macaddr_change_supported(
	__in			efx_nic_t *enp,
	__out			boolean_t *supportedp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_link_control_supported(
	__in			efx_nic_t *enp,
	__out			boolean_t *supportedp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_mac_spoofing_supported(
	__in			efx_nic_t *enp,
	__out			boolean_t *supportedp);


#if EFSYS_OPT_BIST
#if EFX_OPTS_EF10()
LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_bist_enable_offline(
	__in			efx_nic_t *enp);
#endif /* EFX_OPTS_EF10() */
LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_bist_start(
	__in			efx_nic_t *enp,
	__in			efx_bist_type_t type);
#endif /* EFSYS_OPT_BIST */

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
efx_mcdi_get_resource_limits(
	__in			efx_nic_t *enp,
	__out_opt		uint32_t *nevqp,
	__out_opt		uint32_t *nrxqp,
	__out_opt		uint32_t *ntxqp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_log_ctrl(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_mac_stats_clear(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_mac_stats_upload(
	__in		efx_nic_t *enp,
	__in		efsys_mem_t *esmp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_mac_stats_periodic(
	__in		efx_nic_t *enp,
	__in		efsys_mem_t *esmp,
	__in		uint16_t period_ms,
	__in		boolean_t events);


#if EFSYS_OPT_LOOPBACK
LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_get_loopback_modes(
	__in		efx_nic_t *enp);
#endif /* EFSYS_OPT_LOOPBACK */

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_phy_module_get_info(
	__in			efx_nic_t *enp,
	__in			uint8_t dev_addr,
	__in			size_t offset,
	__in			size_t len,
	__out_bcount(len)	uint8_t *data);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_get_nic_addr_info(
	__in		efx_nic_t *enp,
	__out		uint32_t *mapping_typep);

struct efx_nic_dma_region_info_s;

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_get_nic_addr_regions(
	__in		efx_nic_t *enp,
	__out		struct efx_nic_dma_region_info_s *endrip);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
efx_mcdi_set_nic_addr_regions(
	__in		efx_nic_t *enp,
	__in		const struct efx_nic_dma_region_info_s *endrip);

#define	MCDI_IN(_emr, _type, _ofst)					\
	((_type *)((_emr).emr_in_buf + (_ofst)))

#define	MCDI_IN2(_emr, _type, _ofst)					\
	MCDI_IN(_emr, _type, MC_CMD_ ## _ofst ## _OFST)

#define	MCDI_INDEXED_IN2(_emr, _type, _ofst, _idx)			\
	MCDI_IN(_emr, _type, MC_CMD_ ## _ofst ## _OFST +		\
		_idx * MC_CMD_ ## _ofst ## _LEN)

#define	MCDI_IN_SET_BYTE(_emr, _ofst, _value)				\
	EFX_POPULATE_BYTE_1(*MCDI_IN2(_emr, efx_byte_t, _ofst),		\
		EFX_BYTE_0, _value)

#define	MCDI_IN_SET_WORD(_emr, _ofst, _value)				\
	EFX_POPULATE_WORD_1(*MCDI_IN2(_emr, efx_word_t, _ofst),		\
		EFX_WORD_0, _value)

#define	MCDI_IN_SET_DWORD(_emr, _ofst, _value)				\
	EFX_POPULATE_DWORD_1(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		EFX_DWORD_0, _value)

#define	MCDI_IN_SET_DWORD_FIELD(_emr, _ofst, _field, _value)		\
	EFX_SET_DWORD_FIELD(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field, _value)

#define	MCDI_IN_SET_INDEXED_DWORD(_emr, _ofst, _idx, _value)		\
	EFX_POPULATE_DWORD_1(*(MCDI_IN2(_emr, efx_dword_t, _ofst) +	\
			     (_idx)), EFX_DWORD_0, _value)		\

#define	MCDI_IN_SET_QWORD(_emr, _ofst, _value)				\
	EFX_POPULATE_QWORD_2(*MCDI_IN2(_emr, efx_qword_t, _ofst),	\
		EFX_DWORD_0, ((_value) & 0xffffffff),			\
		EFX_DWORD_1, ((_value) >> 32))

#define	MCDI_IN_SET_INDEXED_QWORD(_emr, _ofst, _idx, _value)		\
	EFX_POPULATE_QWORD_2(*(MCDI_IN2(_emr, efx_qword_t, _ofst) +	\
			(_idx)),					\
		EFX_DWORD_0, ((_value) & 0xffffffff),	\
		EFX_DWORD_1, ((_value) >> 32))

#define	MCDI_IN_POPULATE_DWORD_1(_emr, _ofst, _field1, _value1)		\
	EFX_POPULATE_DWORD_1(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1)

#define	MCDI_IN_POPULATE_DWORD_2(_emr, _ofst, _field1, _value1,		\
		_field2, _value2)					\
	EFX_POPULATE_DWORD_2(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2)

#define	MCDI_IN_POPULATE_INDEXED_DWORD_2(_emr, _ofst, _idx,		\
		_field1, _value1, _field2, _value2)			\
	EFX_POPULATE_DWORD_2(						\
		*MCDI_INDEXED_IN2(_emr, efx_dword_t, _ofst, _idx),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2)

#define	MCDI_IN_POPULATE_DWORD_3(_emr, _ofst, _field1, _value1,		\
		_field2, _value2, _field3, _value3)			\
	EFX_POPULATE_DWORD_3(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2,				\
		MC_CMD_ ## _field3, _value3)

#define	MCDI_IN_POPULATE_DWORD_4(_emr, _ofst, _field1, _value1,		\
		_field2, _value2, _field3, _value3, _field4, _value4)	\
	EFX_POPULATE_DWORD_4(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2,				\
		MC_CMD_ ## _field3, _value3,				\
		MC_CMD_ ## _field4, _value4)

#define	MCDI_IN_POPULATE_DWORD_5(_emr, _ofst, _field1, _value1,		\
		_field2, _value2, _field3, _value3, _field4, _value4,	\
		_field5, _value5)					\
	EFX_POPULATE_DWORD_5(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2,				\
		MC_CMD_ ## _field3, _value3,				\
		MC_CMD_ ## _field4, _value4,				\
		MC_CMD_ ## _field5, _value5)

#define	MCDI_IN_POPULATE_DWORD_6(_emr, _ofst, _field1, _value1,		\
		_field2, _value2, _field3, _value3, _field4, _value4,	\
		_field5, _value5, _field6, _value6)			\
	EFX_POPULATE_DWORD_6(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2,				\
		MC_CMD_ ## _field3, _value3,				\
		MC_CMD_ ## _field4, _value4,				\
		MC_CMD_ ## _field5, _value5,				\
		MC_CMD_ ## _field6, _value6)

#define	MCDI_IN_POPULATE_DWORD_7(_emr, _ofst, _field1, _value1,		\
		_field2, _value2, _field3, _value3, _field4, _value4,	\
		_field5, _value5, _field6, _value6, _field7, _value7)	\
	EFX_POPULATE_DWORD_7(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2,				\
		MC_CMD_ ## _field3, _value3,				\
		MC_CMD_ ## _field4, _value4,				\
		MC_CMD_ ## _field5, _value5,				\
		MC_CMD_ ## _field6, _value6,				\
		MC_CMD_ ## _field7, _value7)

#define	MCDI_IN_POPULATE_DWORD_8(_emr, _ofst, _field1, _value1,		\
		_field2, _value2, _field3, _value3, _field4, _value4,	\
		_field5, _value5, _field6, _value6, _field7, _value7,	\
		_field8, _value8)					\
	EFX_POPULATE_DWORD_8(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2,				\
		MC_CMD_ ## _field3, _value3,				\
		MC_CMD_ ## _field4, _value4,				\
		MC_CMD_ ## _field5, _value5,				\
		MC_CMD_ ## _field6, _value6,				\
		MC_CMD_ ## _field7, _value7,				\
		MC_CMD_ ## _field8, _value8)

#define	MCDI_IN_POPULATE_DWORD_9(_emr, _ofst, _field1, _value1,		\
		_field2, _value2, _field3, _value3, _field4, _value4,	\
		_field5, _value5, _field6, _value6, _field7, _value7,	\
		_field8, _value8, _field9, _value9)			\
	EFX_POPULATE_DWORD_9(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2,				\
		MC_CMD_ ## _field3, _value3,				\
		MC_CMD_ ## _field4, _value4,				\
		MC_CMD_ ## _field5, _value5,				\
		MC_CMD_ ## _field6, _value6,				\
		MC_CMD_ ## _field7, _value7,				\
		MC_CMD_ ## _field8, _value8,				\
		MC_CMD_ ## _field9, _value9)

#define	MCDI_IN_POPULATE_DWORD_10(_emr, _ofst, _field1, _value1,	\
		_field2, _value2, _field3, _value3, _field4, _value4,	\
		_field5, _value5, _field6, _value6, _field7, _value7,	\
		_field8, _value8, _field9, _value9, _field10, _value10)	\
	EFX_POPULATE_DWORD_10(*MCDI_IN2(_emr, efx_dword_t, _ofst),	\
		MC_CMD_ ## _field1, _value1,				\
		MC_CMD_ ## _field2, _value2,				\
		MC_CMD_ ## _field3, _value3,				\
		MC_CMD_ ## _field4, _value4,				\
		MC_CMD_ ## _field5, _value5,				\
		MC_CMD_ ## _field6, _value6,				\
		MC_CMD_ ## _field7, _value7,				\
		MC_CMD_ ## _field8, _value8,				\
		MC_CMD_ ## _field9, _value9,				\
		MC_CMD_ ## _field10, _value10)

/*
 * Native setters (MCDI_IN_SET_*_NATIVE) are used when MCDI field is in
 * network order to avoid conversion to little-endian that is done in
 * other setters.
 */
#define	MCDI_IN_SET_WORD_NATIVE(_emr, _ofst, _value)			\
	MCDI_IN2((_emr), efx_word_t, _ofst)->ew_u16[0] = (_value)

#define	MCDI_IN_SET_DWORD_NATIVE(_emr, _ofst, _value)			\
	MCDI_IN2((_emr), efx_dword_t, _ofst)->ed_u32[0] = (_value)

#define	MCDI_OUT(_emr, _type, _ofst)					\
	((_type *)((_emr).emr_out_buf + (_ofst)))

#define	MCDI_OUT2(_emr, _type, _ofst)					\
	MCDI_OUT(_emr, _type, MC_CMD_ ## _ofst ## _OFST)

#define	MCDI_OUT_BYTE(_emr, _ofst)					\
	EFX_BYTE_FIELD(*MCDI_OUT2(_emr, efx_byte_t, _ofst),		\
		    EFX_BYTE_0)

#define	MCDI_OUT_WORD(_emr, _ofst)					\
	EFX_WORD_FIELD(*MCDI_OUT2(_emr, efx_word_t, _ofst),		\
		    EFX_WORD_0)

#define	MCDI_OUT_WORD_FIELD(_emr, _ofst, _field)			\
	EFX_WORD_FIELD(*MCDI_OUT2(_emr, efx_word_t, _ofst),		\
		       MC_CMD_ ## _field)

#define	MCDI_OUT_DWORD(_emr, _ofst)					\
	EFX_DWORD_FIELD(*MCDI_OUT2(_emr, efx_dword_t, _ofst),		\
			EFX_DWORD_0)

#define	MCDI_OUT_DWORD_FIELD(_emr, _ofst, _field)			\
	EFX_DWORD_FIELD(*MCDI_OUT2(_emr, efx_dword_t, _ofst),		\
			MC_CMD_ ## _field)

#define	MCDI_OUT_INDEXED_DWORD(_emr, _ofst, _idx)			\
	MCDI_OUT_INDEXED_DWORD_FIELD(_emr, _ofst, _idx, EFX_DWORD_0)

#define	MCDI_OUT_INDEXED_DWORD_FIELD(_emr, _ofst, _idx, _field)		\
	EFX_DWORD_FIELD(*(MCDI_OUT2(_emr, efx_dword_t, _ofst) +		\
			(_idx)), _field)

#define	MCDI_OUT_INDEXED_STRUCT_MEMBER(_emr, _type, _arr_ofst, _idx,	\
		_member_ofst)						\
	((_type *)(MCDI_OUT2(_emr, uint8_t, _arr_ofst) +		\
		   _idx * MC_CMD_ ## _arr_ofst ## _LEN +		\
		   _member_ofst ## _OFST))

#define	MCDI_OUT_INDEXED_MEMBER_DWORD(_emr, _arr_ofst, _idx,		\
		_member_ofst)						\
	EFX_DWORD_FIELD(						\
		*(MCDI_OUT_INDEXED_STRUCT_MEMBER(_emr, efx_dword_t,	\
						 _arr_ofst, _idx,	\
						 _member_ofst)),	\
		EFX_DWORD_0)

#define	MCDI_OUT_INDEXED_MEMBER_QWORD(_emr, _arr_ofst, _idx,		\
		_member_ofst)						\
	((uint64_t)EFX_QWORD_FIELD(					\
		*(MCDI_OUT_INDEXED_STRUCT_MEMBER(_emr, efx_qword_t,	\
						 _arr_ofst, _idx,	\
						 _member_ofst)),	\
		EFX_DWORD_0) |						\
	(uint64_t)EFX_QWORD_FIELD(					\
		*(MCDI_OUT_INDEXED_STRUCT_MEMBER(_emr, efx_qword_t,	\
						 _arr_ofst, _idx,	\
						 _member_ofst)),	\
		EFX_DWORD_1) << 32)

#define MCDI_STRUCT_MEMBER(_buf, _type, _ofst)				\
	((_type *)((char *)_buf + _ofst ## _OFST))	\

#define MCDI_STRUCT_BYTE(_buf, _ofst)					\
	EFX_BYTE_FIELD(*MCDI_STRUCT_MEMBER(_buf, efx_byte_t, _ofst),	\
		       EFX_BYTE_0)

#define MCDI_STRUCT_BYTE_FIELD(_buf, _ofst, _field)			\
	EFX_BYTE_FIELD(*MCDI_STRUCT_MEMBER(_buf, efx_byte_t, _ofst),	\
		       _field)

#define MCDI_STRUCT_WORD(_buf, _ofst)					\
	EFX_WORD_FIELD(*MCDI_STRUCT_MEMBER(_buf, efx_word_t, _ofst),	\
		       EFX_WORD_0)

#define MCDI_STRUCT_WORD_FIELD(_buf, _ofst, _field)			\
	EFX_WORD_FIELD(*MCDI_STRUCT_MEMBER(_buf, efx_word_t, _ofst),	\
		       _field)

#define MCDI_STRUCT_DWORD(_buf, _ofst)					\
	EFX_DWORD_FIELD(*MCDI_STRUCT_MEMBER(_buf, efx_dword_t, _ofst),	\
			EFX_DWORD_0)

#define MCDI_STRUCT_DWORD_FIELD(_buf, _ofst, _field)			\
	EFX_DWORD_FIELD(*MCDI_STRUCT_MEMBER(_buf, efx_dword_t, _ofst),	\
			_field)

#define	MCDI_EV_FIELD(_eqp, _field)					\
	EFX_QWORD_FIELD(*_eqp, MCDI_EVENT_ ## _field)

#define	MCDI_CMD_DWORD_FIELD(_edp, _field)				\
	EFX_DWORD_FIELD(*_edp, MC_CMD_ ## _field)

#define	EFX_MCDI_HAVE_PRIVILEGE(mask, priv)				\
	(((mask) & (MC_CMD_PRIVILEGE_MASK_IN_GRP_ ## priv)) ==		\
	(MC_CMD_PRIVILEGE_MASK_IN_GRP_ ## priv))

#define	EFX_MCDI_BUF_SIZE(_in_len, _out_len)				\
	EFX_P2ROUNDUP(size_t,						\
		MAX(MAX(_in_len, _out_len), (2 * sizeof (efx_dword_t))),\
		sizeof (efx_dword_t))

/*
 * The buffer size must be a multiple of dword to ensure that MCDI works
 * properly with Siena based boards (which use on-chip buffer). Also, it
 * should be at minimum the size of two dwords to allow space for extended
 * error responses if the request/response buffer sizes are smaller.
 */
#define EFX_MCDI_DECLARE_BUF(_name, _in_len, _out_len)			\
	uint8_t _name[EFX_MCDI_BUF_SIZE(_in_len, _out_len)] = {0}

typedef enum efx_mcdi_feature_id_e {
	EFX_MCDI_FEATURE_FW_UPDATE = 0,
	EFX_MCDI_FEATURE_LINK_CONTROL,
	EFX_MCDI_FEATURE_MACADDR_CHANGE,
	EFX_MCDI_FEATURE_MAC_SPOOFING,
	EFX_MCDI_FEATURE_NIDS
} efx_mcdi_feature_id_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_EFX_MCDI_H */
