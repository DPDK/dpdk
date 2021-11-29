/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2012 Freescale Semiconductor Inc.
 * Copyright 2017-2020 NXP
 */

#ifndef __FM_PORT_EXT_H
#define __FM_PORT_EXT_H

#include <errno.h>
#include "ncsw_ext.h"
#include "fm_pcd_ext.h"
#include "fm_ext.h"
#include "net_ext.h"
#include "dpaa_integration.h"

/*
 * @Description   FM Port routines
 */

/*
 *
 * @Group	  lnx_ioctl_FM_grp Frame Manager Linux IOCTL API
 *
 * @Description   FM Linux ioctls definitions and enums
 *
 * @{
 */

/*
 * @Group	  lnx_ioctl_FM_PORT_grp FM Port
 *
 * @Description   FM Port API
 *
 *		  The FM uses a general module called "port" to represent a Tx
 *		  port (MAC), an Rx port (MAC), offline parsing flow or host
 *		  command flow. There may be up to 17 (may change) ports in an
 *		  FM - 5 Tx ports (4 for the 1G MACs, 1 for the 10G MAC), 5 Rx
 *		  Ports, and 7 Host command/Offline parsing ports. The SW driver
 *		  manages these ports as sub-modules of the FM, i.e. after an FM
 *		  is initialized, its ports may be initialized and operated
 *		  upon.
 *
 *		  The port is initialized aware of its type, but other functions
 *		  on a port may be indifferent to its type. When necessary, the
 *		  driver verifies coherency and returns error if applicable.
 *
 *		  On initialization, user specifies the port type and it's index
 *		  (relative to the port's type). Host command and Offline
 *		  parsing ports share the same id range, I.e user may not
 *		  initialized host command port 0 and offline parsing port 0.
 *
 * @{
 */

/*
 * @Description   An enum for defining port PCD modes.
 *		  (Must match enum e_fm_port_pcd_support defined in
 *		  fm_port_ext.h)
 *
 *		  This enum defines the superset of PCD engines support - i.e.
 *		  not all engines have to be used, but all have to be enabled.
 *		  The real flow of a specific frame depends on the PCD
 *		  configuration and the frame headers and payload. Note: the
 *		  first engine and the first engine after the parser (if exists)
 *		  should be in order, the order is important as it will define
 *		  the flow of the port. However, as for the rest engines (the
 *		  ones that follows), the order is not important anymore as it
 *		  is defined by the PCD graph itself.
 */
typedef enum ioc_fm_port_pcd_support {
	e_IOC_FM_PCD_NONE = 0
			/**< BMI to BMI, PCD is not used */
	, e_IOC_FM_PCD_PRS_ONLY	/**< Use only Parser */
	, e_IOC_FM_PCD_PLCR_ONLY	/**< Use only Policer */
	, e_IOC_FM_PCD_PRS_PLCR/**< Use Parser and Policer */
	, e_IOC_FM_PCD_PRS_KG	/**< Use Parser and Keygen */
	, e_IOC_FM_PCD_PRS_KG_AND_CC
			/**< Use Parser, Keygen and Coarse Classification */
	, e_IOC_FM_PCD_PRS_KG_AND_CC_AND_PLCR
			/**< Use all PCD engines */
	, e_IOC_FM_PCD_PRS_KG_AND_PLCR
			/**< Use Parser, Keygen and Policer */
	, e_IOC_FM_PCD_PRS_CC
			/**< Use Parser and Coarse Classification */
	, e_IOC_FM_PCD_PRS_CC_AND_PLCR
			/**< Use Parser and Coarse Classification and Policer */
	, e_IOC_FM_PCD_CC_ONLY
			/**< Use only Coarse Classification */
} ioc_fm_port_pcd_support;

/*
 * @Collection   FM Frame error
 */
typedef uint32_t	ioc_fm_port_frame_err_select_t;
	/**< typedef for defining Frame Descriptor errors */

/* @} */

/*
 * @Description   An enum for defining Dual Tx rate limiting scale.
 *		  (Must match e_fm_port_dual_rate_limiter_scale_down defined in
 *		  fm_port_ext.h)
 */
typedef enum ioc_fm_port_dual_rate_limiter_scale_down {
	e_IOC_FM_PORT_DUAL_RATE_LIMITER_NONE = 0,
			/**< Use only single rate limiter*/
	e_IOC_FM_PORT_DUAL_RATE_LIMITER_SCALE_DOWN_BY_2,
			/**< Divide high rate limiter by 2 */
	e_IOC_FM_PORT_DUAL_RATE_LIMITER_SCALE_DOWN_BY_4,
			/**< Divide high rate limiter by 4 */
	e_IOC_FM_PORT_DUAL_RATE_LIMITER_SCALE_DOWN_BY_8
			/**< Divide high rate limiter by 8 */
} ioc_fm_port_dual_rate_limiter_scale_down;

/*
 * @Description   A structure for defining Tx rate limiting
 *		  (Must match struct t_fm_port_rate_limit defined in
 *		  fm_port_ext.h)
 */
typedef struct ioc_fm_port_rate_limit_t {
	uint16_t	max_burst_size;
			/**< in KBytes for Tx ports, in frames for offline
			 * parsing ports. (note that for early chips burst size
			 * is rounded up to a multiply of 1000 frames).
			 */
	uint32_t	rate_limit;
			/**< in Kb/sec for Tx ports, in frame/sec for offline
			 * parsing ports. Rate limit refers to data rate (rather
			 * than line rate).
			 */
	ioc_fm_port_dual_rate_limiter_scale_down rate_limit_divider;
			/**< For offline parsing ports only. Not-valid for some
			 * earlier chip revisions
			 */
} ioc_fm_port_rate_limit_t;


/*
 * @Group	  lnx_ioctl_FM_PORT_runtime_control_grp FM Port Runtime Control
 *		  Unit
 *
 * @Description   FM Port Runtime control unit API functions, definitions and
 *		  enums.
 *
 * @{
 */

/*
 * @Description   An enum for defining FM Port counters.
 *		  (Must match enum e_fm_port_counters defined in fm_port_ext.h)
 */
typedef enum ioc_fm_port_counters {
	e_IOC_FM_PORT_COUNTERS_CYCLE,	/**< BMI performance counter */
	e_IOC_FM_PORT_COUNTERS_TASK_UTIL,	/**< BMI performance counter */
	e_IOC_FM_PORT_COUNTERS_QUEUE_UTIL,	/**< BMI performance counter */
	e_IOC_FM_PORT_COUNTERS_DMA_UTIL,	/**< BMI performance counter */
	e_IOC_FM_PORT_COUNTERS_FIFO_UTIL,	/**< BMI performance counter */
	e_IOC_FM_PORT_COUNTERS_RX_PAUSE_ACTIVATION,
				/**< BMI Rx only performance counter */
	e_IOC_FM_PORT_COUNTERS_FRAME,		/**< BMI statistics counter */
	e_IOC_FM_PORT_COUNTERS_DISCARD_FRAME,	/**< BMI statistics counter */
	e_IOC_FM_PORT_COUNTERS_DEALLOC_BUF,
				/**< BMI deallocate buffer statistics counter */
	e_IOC_FM_PORT_COUNTERS_RX_BAD_FRAME,
				/**< BMI Rx only statistics counter */
	e_IOC_FM_PORT_COUNTERS_RX_LARGE_FRAME,
				/**< BMI Rx only statistics counter */
	e_IOC_FM_PORT_COUNTERS_RX_FILTER_FRAME,
				/**< BMI Rx & OP only statistics counter */
	e_IOC_FM_PORT_COUNTERS_RX_LIST_DMA_ERR,
				/**< BMI Rx, OP & HC only statistics counter */
	e_IOC_FM_PORT_COUNTERS_RX_OUT_OF_BUFFERS_DISCARD,
				/**< BMI Rx, OP & HC statistics counter */
	e_IOC_FM_PORT_COUNTERS_PREPARE_TO_ENQUEUE_COUNTER,
				/**< BMI Rx, OP & HC only statistics counter */
	e_IOC_FM_PORT_COUNTERS_WRED_DISCARD,
				/**< BMI OP & HC only statistics counter */
	e_IOC_FM_PORT_COUNTERS_LENGTH_ERR,
				/**< BMI non-Rx statistics counter */
	e_IOC_FM_PORT_COUNTERS_UNSUPPRTED_FORMAT,
				/**< BMI non-Rx statistics counter */
	e_IOC_FM_PORT_COUNTERS_DEQ_TOTAL,/**< QMI total QM dequeues counter */
	e_IOC_FM_PORT_COUNTERS_ENQ_TOTAL,/**< QMI total QM enqueues counter */
	e_IOC_FM_PORT_COUNTERS_DEQ_FROM_DEFAULT,/**< QMI counter */
	e_IOC_FM_PORT_COUNTERS_DEQ_CONFIRM	/**< QMI counter */
} ioc_fm_port_counters;

typedef struct ioc_fm_port_bmi_stats_t {
	uint32_t cnt_cycle;
	uint32_t cnt_task_util;
	uint32_t cnt_queue_util;
	uint32_t cnt_dma_util;
	uint32_t cnt_fifo_util;
	uint32_t cnt_rx_pause_activation;
	uint32_t cnt_frame;
	uint32_t cnt_discard_frame;
	uint32_t cnt_dealloc_buf;
	uint32_t cnt_rx_bad_frame;
	uint32_t cnt_rx_large_frame;
	uint32_t cnt_rx_filter_frame;
	uint32_t cnt_rx_list_dma_err;
	uint32_t cnt_rx_out_of_buffers_discard;
	uint32_t cnt_wred_discard;
	uint32_t cnt_length_err;
	uint32_t cnt_unsupported_format;
} ioc_fm_port_bmi_stats_t;

/*
 * @Description   Structure for Port id parameters.
 *		  (Description may be inaccurate;
 *		  must match struct t_fm_port_congestion_grps defined in
 *		  fm_port_ext.h)
 *
 *		  Fields commented 'IN' are passed by the port module to be used
 *		  by the FM module. Fields commented 'OUT' will be filled by FM
 *		  before returning to port.
 */
typedef struct ioc_fm_port_congestion_groups_t {
	uint16_t	num_of_congestion_grps_to_consider;
			/**< The number of required congestion groups to define
			 * the size of the following array
			 */
	uint8_t	congestion_grps_to_consider[FM_NUM_CONG_GRPS];
			/**< An array of CG indexes; Note that the size of the
			 * array should be 'num_of_congestion_grps_to_consider'.
			 */
	bool	pfc_priorities_enable[FM_NUM_CONG_GRPS][FM_MAX_PFC_PRIO];
			/**< A matrix that represents the map between the CG ids
			 * defined in 'congestion_grps_to_consider' to the
			 * priorities mapping array.
			 */
} ioc_fm_port_congestion_groups_t;


/*
 * @Function	  fm_port_disable
 *
 * @Description   Gracefully disable an FM port. The port will not start new
 *		  tasks after all tasks associated with the port are terminated.
 *
 * @Return	  0 on success; error code otherwise.
 *
 * @Cautions	  This is a blocking routine, it returns after port is
 *		  gracefully stopped, i.e. the port will not except new frames,
 *		  but it will finish all frames or tasks which were already
 *		  began
 */
#define FM_PORT_IOC_DISABLE   _IO(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(1))

/*
 * @Function	  fm_port_enable
 *
 * @Description   A runtime routine provided to allow disable/enable of port.
 *
 * @Return	  0 on success; error code otherwise.
 */
#define FM_PORT_IOC_ENABLE   _IO(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(2))

/*
 * @Function	  fm_port_set_rate_limit
 *
 * @Description   Calling this routine enables rate limit algorithm.
 *		  By default, this functionality is disabled.
 *
 *		  Note that rate - limit mechanism uses the FM time stamp.
 *		  The selected rate limit specified here would be
 *		  rounded DOWN to the nearest 16M.
 *
 *		  May be used for Tx and offline parsing ports only
 *
 * @Param[in]	  ioc_fm_port_rate_limit	A structure of rate limit
 *						parameters
 *
 * @Return	0 on success; error code otherwise.
 */
#define FM_PORT_IOC_SET_RATE_LIMIT \
	IOW(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(3), ioc_fm_port_rate_limit_t)

/*
 * @Function	  fm_port_delete_rate_limit
 *
 * @Description   Calling this routine disables the previously enabled rate
 *		  limit.
 *
 *		  May be used for Tx and offline parsing ports only
 *
 * @Return	  0 on success; error code otherwise.
 */
#define FM_PORT_IOC_DELETE_RATE_LIMIT _IO(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(5))
#define FM_PORT_IOC_REMOVE_RATE_LIMIT FM_PORT_IOC_DELETE_RATE_LIMIT

/*
 * @Function	  fm_port_add_congestion_grps
 *
 * @Description   This routine effects the corresponding Tx port.
 *		  It should be called in order to enable pause frame
 *		  transmission in case of congestion in one or more of the
 *		  congestion groups relevant to this port.
 *		  Each call to this routine may add one or more congestion
 *		  groups to be considered relevant to this port.
 *
 *		  May be used for Rx, or RX+OP ports only (depending on chip)
 *
 * @Param[in]	  ioc_fm_port_congestion_groups_t	A pointer to an array of
 *							congestion group ids to
 *							consider.
 *
 * @Return	  0 on success; error code otherwise.
 */
#define FM_PORT_IOC_ADD_CONGESTION_GRPS	\
	_IOW(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(34), \
	     ioc_fm_port_congestion_groups_t)

/*
 * @Function	fm_port_remove_congestion_grps
 *
 * @Description   This routine effects the corresponding Tx port. It should be
 *		  called when congestion groups were defined for this port and
 *		  are no longer relevant, or pause frames transmitting is not
 *		  required on their behalf. Each call to this routine may remove
 *		  one or more congestion groups to be considered relevant to
 *		  this port.
 *
 *		  May be used for Rx, or RX+OP ports only (depending on chip)
 *
 * @Param[in]	  ioc_fm_port_congestion_groups_t	A pointer to an array of
 *							congestion group ids to
 *							consider.
 *
 * @Return	0 on success; error code otherwise.
 */
#define FM_PORT_IOC_REMOVE_CONGESTION_GRPS	\
	_IOW(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(35), \
	     ioc_fm_port_congestion_groups_t)

/*
 * @Function	  fm_port_set_errors_route
 *
 * @Description   Errors selected for this routine will cause a frame with that
 *		  error to be enqueued to error queue.
 *		  Errors not selected for this routine will cause a frame with
 *		  that error to be enqueued to the one of the other port queues.
 *		  By default all errors are defined to be enqueued to error
 *		  queue. Errors that were configured to be discarded (at
 *		  initialization) may not be selected here.
 *
 *		  May be used for Rx and offline parsing ports only
 *
 * @Param[in]	  ioc_fm_port_frame_err_select_t	A list of errors to
 *							enqueue to error queue
 *
 * @Return	  0 on success; error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 *		  (szbs001: How is it possible to have one function that needs
 *		  to be called BEFORE fm_port_init() implemented as an ioctl,
 *		  which will ALWAYS be called AFTER the fm_port_init() for that
 I		  port!?!?!?!???!?!??!?!?)
 */
#define FM_PORT_IOC_SET_ERRORS_ROUTE \
	_IOW(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(4), \
	     ioc_fm_port_frame_err_select_t)

/*
 * @Group	  lnx_ioctl_FM_PORT_pcd_runtime_control_grp FM Port PCD Runtime
 *		  Control Unit
 *
 * @Description   FM Port PCD Runtime control unit API functions, definitions
 *		  and enums.
 *
 * @{
 */

/*
 * @Description   A structure defining the KG scheme after the parser.
 *		  (Must match struct ioc_fm_pcd_kg_scheme_select_t defined in
 *		  fm_port_ext.h)
 *
 *		  This is relevant only to change scheme selection mode - from
 *		  direct to indirect and vice versa, or when the scheme is
 *		  selected directly, to select the scheme id.
 *
 */
typedef struct ioc_fm_pcd_kg_scheme_select_t {
	bool	direct;
		/**< TRUE to use 'scheme_id' directly, FALSE to use LCV.*/
	void	*scheme_id;
		/**< Relevant for 'direct'=TRUE only. 'scheme_id' selects the
		 * scheme after parser.
		 */
} ioc_fm_pcd_kg_scheme_select_t;

/*
 * @Description   Scheme IDs structure
 *		  (Must match struct ioc_fm_pcd_port_schemes_params_t defined
 *		  in fm_port_ext.h)
 */
typedef struct ioc_fm_pcd_port_schemes_params_t {
	uint8_t	num_schemes;
		/**< Number of schemes for port to be bound to. */
	void	*scheme_ids[FM_PCD_KG_NUM_OF_SCHEMES];
		/**< Array of 'num_schemes' schemes for the port to be bound
		 * to
		 */
} ioc_fm_pcd_port_schemes_params_t;

/*
 * @Description   A union for defining port protocol parameters for parser
 *		  (Must match union u_FmPcdHdrPrsOpts defined in fm_port_ext.h)
 */
typedef union ioc_fm_pcd_hdr_prs_opts_u {
	/* MPLS */
	struct {
	bool label_interpretation_enable;
		/**< When this bit is set, the last MPLS label will be
		 * interpreted as described in HW spec table. When the bit is
		 * cleared, the parser will advance to MPLS next parse
		 */
	ioc_net_header_type next_parse;
		/**< must be equal or higher than IPv4 */
	} mpls_prs_options;

	/* VLAN */
	struct {
	uint16_t	tag_protocol_id1;
		/**< User defined Tag Protocol Identifier, to be recognized on
		 * VLAN TAG on top of 0x8100 and 0x88A8
		 */
	uint16_t	tag_protocol_id2;
		/**< User defined Tag Protocol Identifier, to be recognized on
		 * VLAN TAG on top of 0x8100 and 0x88A8
		 */
	} vlan_prs_options;

	/* PPP */
	struct{
		bool		enable_mtu_check;
		/**< Check validity of MTU according to RFC2516 */
	} pppoe_prs_options;

	/* IPV6 */
	struct {
		bool		routing_hdr_disable;
		/**< Disable routing header */
	} ipv6_prs_options;

	/* UDP */
	struct {
		bool		pad_ignore_checksum;
		/**< TRUE to ignore pad in checksum */
	} udp_prs_options;

	/* TCP */
	struct {
		bool		pad_ignore_checksum;
		/**< TRUE to ignore pad in checksum */
	} tcp_prs_options;
} ioc_fm_pcd_hdr_prs_opts_u;

/*
 * @Description   A structure for defining each header for the parser
 *		  (must match struct t_FmPcdPrsAdditionalHdrParams defined in
 *		  fm_port_ext.h)
 */
typedef struct ioc_fm_pcd_prs_additional_hdr_params_t {
	ioc_net_header_type	hdr; /**< Selected header */
	bool	err_disable; /**< TRUE to disable error indication */
	bool	soft_prs_enable;
		/**< Enable jump to SW parser when this header is recognized by
		 * the HW parser.
		 */
	uint8_t	index_per_hdr;
		/**< Normally 0, if more than one sw parser attachments exists
		 * for the same header, (in the main sw parser code) use this
		 * index to distinguish between them.
		 */
	bool	use_prs_opts;	/**< TRUE to use parser options. */
	ioc_fm_pcd_hdr_prs_opts_u prs_opts;
		/**< A unuion according to header type, defining the parser
		 * options selected.
		 */
} ioc_fm_pcd_prs_additional_hdr_params_t;

/*
 * @Description   A structure for defining port PCD parameters
 *		  (Must match t_fm_portPcdPrsParams defined in fm_port_ext.h)
 */
typedef struct ioc_fm_port_pcd_prs_params_t {
	uint8_t		prs_res_priv_info;
		/**< The private info provides a method of inserting port
		 * information into the parser result. This information may be
		 * extracted by KeyGen and be used for frames distribution when
		 * a per-port distinction is required, it may also be used as a
		 * port logical id for analyzing incoming frames.
		 */
	uint8_t		parsing_offset;
		/**< Number of bytes from beginning of packet to start parsing
		 */
	ioc_net_header_type	first_prs_hdr;
		/**< The type of the first header expected at 'parsing_offset'
		 */
	bool		include_in_prs_statistics;
		/**< TRUE to include this port in the parser statistics */
	uint8_t		num_of_hdrs_with_additional_params;
		/**< Normally 0, some headers may get special parameters */
	ioc_fm_pcd_prs_additional_hdr_params_t
			additional_params[IOC_FM_PCD_PRS_NUM_OF_HDRS];
		/**< 'num_of_hdrs_with_additional_params' structures additional
		 * parameters for each header that requires them
		 */
	bool		set_vlan_tpid1;
		/**< TRUE to configure user selection of Ethertype to indicate a
		 * VLAN tag (in addition to the TPID values 0x8100 and 0x88A8).
		 */
	uint16_t	vlan_tpid1;
		/**< extra tag to use if set_vlan_tpid1=TRUE. */
	bool		set_vlan_tpid2;
		/**< TRUE to configure user selection of Ethertype to indicate a
		 * VLAN tag (in addition to the TPID values 0x8100 and 0x88A8).
		 */
	uint16_t	vlan_tpid2;
		/**< extra tag to use if set_vlan_tpid1=TRUE. */
} ioc_fm_port_pcd_prs_params_t;

/*
 * @Description   A structure for defining coarse classification parameters
 *		  (Must match t_fm_portPcdCcParams defined in fm_port_ext.h)
 */
typedef struct ioc_fm_port_pcd_cc_params_t {
	void		*cc_tree_id; /**< CC tree id */
} ioc_fm_port_pcd_cc_params_t;

/*
 * @Description   A structure for defining keygen parameters
 *		  (Must match t_fm_portPcdKgParams defined in fm_port_ext.h)
 */
typedef struct ioc_fm_port_pcd_kg_params_t {
	uint8_t		num_schemes;
			/**< Number of schemes for port to be bound to. */
	void		*scheme_ids[FM_PCD_KG_NUM_OF_SCHEMES];
			/**< Array of 'num_schemes' schemes for the port to
			 * be bound to
			 */
	bool		direct_scheme;
			/**< TRUE for going from parser to a specific scheme,
			 * regardless of parser result
			 */
	void		*direct_scheme_id;
			/**< Scheme id, as returned by FM_PCD_KgSetScheme;
			 * relevant only if direct=TRUE.
			 */
} ioc_fm_port_pcd_kg_params_t;

/*
 * @Description   A structure for defining policer parameters
 *		  (Must match t_fm_portPcdPlcrParams defined in fm_port_ext.h)
 */
typedef struct ioc_fm_port_pcd_plcr_params_t {
	void	*plcr_profile_id;
		/**< Selected profile handle;
		 * relevant in one of the following cases:
		 * e_IOC_FM_PCD_PLCR_ONLY or
		 * e_IOC_FM_PCD_PRS_PLCR were selected, or if
		 * any flow uses a KG scheme where policer profile is not
		 * generated (bypass_plcr_profile_generation selected)
		 */
} ioc_fm_port_pcd_plcr_params_t;

/*
 * @Description   A structure for defining port PCD parameters
 *		  (Must match struct t_fm_portPcdParams defined in
 *		  fm_port_ext.h)
 */
typedef struct ioc_fm_port_pcd_params_t {
	ioc_fm_port_pcd_support	pcd_support;
		/**< Relevant for Rx and offline ports only.
		 * Describes the active PCD engines for this port.
		 */
	void		*net_env_id;	/**< HL Unused in PLCR only mode */
	ioc_fm_port_pcd_prs_params_t	*p_prs_params;
		/**< Parser parameters for this port */
	ioc_fm_port_pcd_cc_params_t	*p_cc_params;
		/**< Coarse classification parameters for this port */
	ioc_fm_port_pcd_kg_params_t	*p_kg_params;
		/**< Keygen parameters for this port */
	ioc_fm_port_pcd_plcr_params_t	*p_plcr_params;
		/**< Policer parameters for this port */
	void		*p_ip_reassembly_manip;
		/**< IP Reassembly manipulation */
	void		*p_capwap_reassembly_manip;
		/**< CAPWAP Reassembly manipulation */
} ioc_fm_port_pcd_params_t;

/*
 * @Description   A structure for defining the Parser starting point
 *		  (Must match struct ioc_fm_pcd_prs_start_t defined in
 *		  fm_port_ext.h)
 */
typedef struct ioc_fm_pcd_prs_start_t {
	uint8_t	parsing_offset;
		/**< Number of bytes from beginning of packet to start parsing
		 */
	ioc_net_header_type first_prs_hdr;
		/**< The type of the first header expected at 'parsing_offset'
		 */
} ioc_fm_pcd_prs_start_t;

/*
 * @Description   FQID parameters structure
 */
typedef struct ioc_fm_port_pcd_fqids_params_t {
	uint32_t	num_fqids;
		/**< Number of fqids to be allocated for the port */
	uint8_t		alignment;
		/**< Alignment required for this port */
	uint32_t	base_fqid;
		/**< output parameter - the base fqid */
} ioc_fm_port_pcd_fqids_params_t;

/*
 * @Function	  FM_PORT_IOC_ALLOC_PCD_FQIDS
 *
 * @Description   Allocates FQID's
 *		  May be used for Rx and offline parsing ports only
 *
 * @Param[in,out] ioc_fm_port_pcd_fqids_params_t	Parameters for
 *							allocating FQID's
 *
 * @Return	  0 on success; error code otherwise.
 */
#define FM_PORT_IOC_ALLOC_PCD_FQIDS \
	_IOWR(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(19), \
	      ioc_fm_port_pcd_fqids_params_t)

/*
 * @Function	  FM_PORT_IOC_FREE_PCD_FQIDS
 *
 * @Description   Frees previously-allocated FQIDs
 *		  May be used for Rx and offline parsing ports only
 *
 * @Param[in]	  uint32_t	Base FQID of previously allocated range.
 *
 * @Return	  0 on success; error code otherwise.
 */
#define FM_PORT_IOC_FREE_PCD_FQIDS \
	_IOW(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(19), uint32_t)

/*
 * @Function	fm_port_set_pcd
 *
 * @Description   Calling this routine defines the port's PCD configuration.
 *		  It changes it from its default configuration which is PCD
 *		  disabled (BMI to BMI) and configures it according to the
 *		  passed parameters.
 *		  May be used for Rx and offline parsing ports only
 *
 * @Param[in]	  ioc_fm_port_pcd_params_t	A Structure of parameters
 *						defining the port's PCD
 *						configuration.
 *
 * @Return	  0 on success; error code otherwise.
 */
#define FM_PORT_IOC_SET_PCD \
	_IOW(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(20), ioc_fm_port_pcd_params_t)

/*
 * @Function	  fm_port_delete_pcd
 *
 * @Description   Calling this routine releases the port's PCD configuration.
 *		  The port returns to its default configuration which is PCD
 *		  disabled (BMI to BMI) and all PCD configuration is removed.
 *		  May be used for Rx and offline parsing ports which are in PCD
 *		  mode only
 *
 * @Return	  0 on success; error code otherwise.
 */
#define FM_PORT_IOC_DELETE_PCD _IO(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(21))

/*
 * @Function	  fm_port_attach_pcd
 *
 * @Description   This routine may be called after fm_port_detach_pcd was
 *		  called, to return to the originally configured PCD support
 *		  flow. The couple of routines are used to allow PCD
 *		  configuration changes that demand that PCD will not be used
 *		  while changes take place.
 *
 *		  May be used for Rx and offline parsing ports which are in PCD
 *		  mode only
 *
 * @Return	  0 on success; error code otherwise.
 */
#define FM_PORT_IOC_ATTACH_PCD _IO(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(23))

/*
 * @Function	  fm_port_detach_pcd
 *
 * @Description   Calling this routine detaches the port from its PCD
 *		  functionality. The port returns to its default flow which is
 *		  BMI to BMI.
 *
 *		  May be used for Rx and offline parsing ports which are in PCD
 *		  mode only
 *
 * @Return	  0 on success; error code otherwise.
 */
#define FM_PORT_IOC_DETACH_PCD _IO(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(22))

/*
 * @Function	  fm_port_pcd_plcr_alloc_profiles
 *
 * @Description   This routine may be called only for ports that use the Policer
 *		  in order to allocate private policer profiles.
 *
 * @Param[in]	  uint16_t	The number of required policer profiles
 *
 * @Return	  0 on success; error code otherwise.
 *
 * @Cautions	  Allowed before fm_port_set_pcd() only.
 */
#define FM_PORT_IOC_PCD_PLCR_ALLOC_PROFILES	\
	_IOW(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(24), uint16_t)

/*
 * @Function	  fm_port_pcd_plcr_free_profiles
 *
 * @Description   This routine should be called for freeing private policer
 *		  profiles.
 *
 * @Return	  0 on success; error code otherwise.
 *
 * @Cautions	  Allowed before fm_port_set_pcd() only.
 */
#define FM_PORT_IOC_PCD_PLCR_FREE_PROFILES	\
	_IO(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(25))

/*
 * @Function	  fm_port_pcd_kg_modify_initial_scheme
 *
 * @Description   This routine may be called only for ports that use the keygen
 *		  in order to change the initial scheme frame should be routed
 *		  to.The change may be of a scheme id (in case of direct mode),
 *		  from direct to indirect, or from indirect to direct -
 *		  specifying the scheme id.
 *
 * @Param[in]	  ioc_fm_pcd_kg_scheme_select_t
 *		  A structure of parameters for defining whether a scheme is
 *		  direct/indirect, and if direct - scheme id.
 *
 * @Return	  0 on success; error code otherwise.
 */
#define FM_PORT_IOC_PCD_KG_MODIFY_INITIAL_SCHEME \
	_IOW(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(26), \
	     ioc_fm_pcd_kg_scheme_select_t)

/*
 * @Function	  fm_port_pcd_plcr_modify_initial_profile
 *
 * @Description   This routine may be called for ports with flows
 *		  e_IOC_FM_PCD_SUPPORT_PLCR_ONLY or
 *		  e_IOC_FM_PCD_SUPPORT_PRS_AND_PLCR only, to change the initial
 *		  Policer profile frame should be routed to.
 *		  The change may be of a profile and / or absolute / direct mode
 *		  selection.
 *
 * @Param[in]	  ioc_fm_obj_t		Policer profile Id as returned from
 *					FM_PCD_PlcrSetProfile.
 *
 * @Return	  0 on success; error code otherwise.
 */
#define FM_PORT_IOC_PCD_PLCR_MODIFY_INITIAL_PROFILE \
	_IOW(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(27), ioc_fm_obj_t)

/*
 * @Function	  fm_port_pcd_cc_modify_tree
 *
 * @Description   This routine may be called to change this port connection to
 *		  a pre - initializes coarse classification Tree.
 *
 * @Param[in]	  ioc_fm_obj_t	Id of new coarse classification tree selected
 *				for this port.
 *
 * @Return	  0 on success; error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_set_pcd() and
 *		  fm_port_detach_pcd()
 */
#define FM_PORT_IOC_PCD_CC_MODIFY_TREE \
	_IOW(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(28), ioc_fm_obj_t)

/*
 * @Function	  fm_port_pcd_kg_bind_schemes
 *
 * @Description   These routines may be called for modifying the binding of
 *		  ports to schemes. The scheme itself is not added, just this
 *		  specific port starts using it.
 *
 * @Param[in]	  ioc_fm_pcd_port_schemes_params_t	Schemes parameters
 *							structure
 *
 * @Return	  0 on success; error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_set_pcd().
 */
#define FM_PORT_IOC_PCD_KG_BIND_SCHEMES \
	_IOW(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(30), \
	     ioc_fm_pcd_port_schemes_params_t)

/*
 * @Function	  fm_port_pcd_kg_unbind_schemes
 *
 * @Description   These routines may be called for modifying the binding of
 *		  ports to schemes. The scheme itself is not removed or
 *		  invalidated, just this specific port stops using it.
 *
 * @Param[in]	  ioc_fm_pcd_port_schemes_params_t	Schemes parameters
 *							structure
 *
 * @Return	  0 on success; error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_set_pcd().
 */
#define FM_PORT_IOC_PCD_KG_UNBIND_SCHEMES \
	_IOW(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(31), \
	     ioc_fm_pcd_port_schemes_params_t)

#define ENET_NUM_OCTETS_PER_ADDRESS 6
		/**< Number of octets (8-bit bytes) in an ethernet address */
typedef struct ioc_fm_port_mac_addr_params_t {
	uint8_t addr[ENET_NUM_OCTETS_PER_ADDRESS];
} ioc_fm_port_mac_addr_params_t;

/*
 * @Function	  FM_MAC_AddHashMacAddr
 *
 * @Description   Add an Address to the hash table. This is for filter purpose
 *		  only.
 *
 * @Param[in]	  ioc_fm_port_mac_addr_params_t		Ethernet Mac address
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following FM_MAC_Init(). It is a filter only
 *		  address.
 * @Cautions	  Some address need to be filtered out in upper FM blocks.
 */
#define FM_PORT_IOC_ADD_RX_HASH_MAC_ADDR  \
	_IOW(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(36), \
	     ioc_fm_port_mac_addr_params_t)

/*
 * @Function	  FM_MAC_RemoveHashMacAddr
 *
 * @Description   Delete an Address to the hash table. This is for filter
 *		  purpose only.
 *
 * @Param[in]	  ioc_fm_port_mac_addr_params_t		Ethernet Mac address
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following FM_MAC_Init().
 */
#define FM_PORT_IOC_REMOVE_RX_HASH_MAC_ADDR  \
	_IOW(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(37), \
	     ioc_fm_port_mac_addr_params_t)

typedef struct ioc_fm_port_tx_pause_frames_t {
	uint8_t  priority;
	uint16_t pause_time;
	uint16_t thresh_time;
} ioc_fm_port_tx_pause_frames_t;

/*
 * @Function	  FM_MAC_SetTxPauseFrames
 *
 * @Description   Enable/Disable transmission of Pause-Frames. The routine
 *		  changes the default configuration: pause-time - [0xf000]
 *		  threshold-time - [0]
 *
 * @Param[in]	  ioc_fm_port_tx_pause_frames_params_t
 *		  A structure holding the required parameters.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following FM_MAC_Init(). PFC is supported only on
 *		  new mEMAC; i.e. in MACs that don't have PFC support (10G-MAC
 *		  and dTSEC), user should use 'FM_MAC_NO_PFC' in the 'priority'
 *		  field.
 */
#define FM_PORT_IOC_SET_TX_PAUSE_FRAMES	\
	_IOW(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(40), \
		ioc_fm_port_tx_pause_frames_t)

typedef struct ioc_fm_port_mac_statistics_t {
	/* RMON */
	uint64_t  e_stat_pkts_64;
		/**< r-10G tr-DT 64 byte frame counter */
	uint64_t  e_stat_pkts_65_to_127;
		/**< r-10G 65 to 127 byte frame counter */
	uint64_t  e_stat_pkts_128_to_255;
		/**< r-10G 128 to 255 byte frame counter */
	uint64_t  e_stat_pkts_256_to_511;
		/**< r-10G 256 to 511 byte frame counter */
	uint64_t  e_stat_pkts_512_to_1023;
		/**< r-10G 512 to 1023 byte frame counter*/
	uint64_t  e_stat_pkts_1024_to_1518;
		/**< r-10G 1024 to 1518 byte frame counter */
	uint64_t  e_stat_pkts_1519_to_1522;
		/**< r-10G 1519 to 1522 byte good frame count */
	/* */
	uint64_t  e_stat_fragments;
		/**< Total number of packets that were less than 64 octets long
		 * with a wrong CRC.
		 */
	uint64_t  e_stat_jabbers;
		/**< Total number of packets longer than valid maximum length
		 * octets
		 */
	uint64_t  e_stat_drop_events;
		/**< number of dropped packets due to internal errors of the MAC
		 * Client (during receive).
		 */
	uint64_t  e_stat_CRC_align_errors;
		/**< Incremented when frames of correct length but with CRC
		 * error are received.
		 */
	uint64_t  e_stat_undersize_pkts;
		/**< Incremented for frames under 64 bytes with a valid FCS and
		 * otherwise well formed; This count does not include range
		 * length errors
		 */
	uint64_t  e_stat_oversize_pkts;
		/**< Incremented for frames which exceed 1518 (non VLAN) or 1522
		 * (VLAN) and contains a valid FCS and otherwise well formed
		 */
	/* Pause */
	uint64_t  rx_stat_pause;	/**< Pause MAC Control received */
	uint64_t  tx_stat_pause;	/**< Pause MAC Control sent */
	/* MIB II */
	uint64_t  if_in_octets;		/**< Total number of byte received. */
	uint64_t  if_in_pkts;		/**< Total number of packets received.*/
	uint64_t  if_in_ucast_pkts;
		/**< Total number of unicast frame received;
		 * NOTE: this counter is not supported on dTSEC MAC
		 */
	uint64_t  if_in_mcast_pkts;
		/**< Total number of multicast frame received*/
	uint64_t  if_in_bcast_pkts;
		/**< Total number of broadcast frame received */
	uint64_t  if_in_discards;
		/**< Frames received, but discarded due to problems within the
		 * MAC RX.
		 */
	uint64_t  if_in_errors;
		/**< Number of frames received with error:
		 *	- FIFO Overflow Error
		 *	- CRC Error
		 *	- Frame Too Long Error
		 *	- Alignment Error
		 *	- The dedicated Error Code (0xfe, not a code error) was
		 *	  received
		 */
	uint64_t  if_out_octets;	/**< Total number of byte sent. */
	uint64_t  if_out_pkts;		/**< Total number of packets sent .*/
	uint64_t  if_out_ucast_pkts;
		/**< Total number of unicast frame sent;
		 * NOTE: this counter is not supported on dTSEC MAC
		 */
	uint64_t  if_out_mcast_pkts;
		/**< Total number of multicast frame sent */
	uint64_t  if_out_bcast_pkts;
		/**< Total number of multicast frame sent */
	uint64_t  if_out_discards;
		/**< Frames received, but discarded due to problems within the
		 * MAC TX N/A!.
		 */
	uint64_t  if_out_errors;
		/**< Number of frames transmitted with error:
		 *	- FIFO Overflow Error
		 *	- FIFO Underflow Error
		 *	- Other
		 */
} ioc_fm_port_mac_statistics_t;

/*
 * @Function	  FM_MAC_GetStatistics
 *
 * @Description   get all MAC statistics counters
 *
 * @Param[out]	  ioc_fm_port_mac_statistics_t	A structure holding the
 *						statistics
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following FM_Init().
 */
#define FM_PORT_IOC_GET_MAC_STATISTICS	\
	_IOR(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(41), \
	     ioc_fm_port_mac_statistics_t)

/*
 * @Function	  fm_port_get_bmi_counters
 *
 * @Description   Read port's BMI stat counters and place them into
 *		  a designated structure of counters.
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[out]	  p_bmi_stats	counters structure
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */

#define FM_PORT_IOC_GET_BMI_COUNTERS \
	_IOR(FM_IOC_TYPE_BASE, FM_PORT_IOC_NUM(42), ioc_fm_port_bmi_stats_t)

/** @} */ /* end of lnx_ioctl_FM_PORT_pcd_runtime_control_grp group */
/** @} */ /* end of lnx_ioctl_FM_PORT_runtime_control_grp group */

/** @} */ /* end of lnx_ioctl_FM_PORT_grp group */
/** @} */ /* end of lnx_ioctl_FM_grp group */


/*
 * @Group	  gen_id	General Drivers Utilities
 *
 * @Description   External routines.
 *
 * @{
 */

/*
 * @Group	  gen_error_id	Errors, Events and Debug
 *
 * @Description   External routines.
 *
 * @{
 */

/*
 * The scheme below provides the bits description for error codes:
 *
 *  0   1   2   3   4   5   6   7   8   9   10   11   12   13   14   15
 * |	Reserved (should be zero)	|	Module ID		|
 *
 *  16   17   18   19   20   21   22  23   24   25   26   27   28   29   30   31
 * |				Error Type			       |
 */

#define ERROR_CODE(_err)  ((((uint32_t)_err) & 0x0000FFFF) | __ERR_MODULE__)

#define GET_ERROR_TYPE(_errcode)	((_errcode) & 0x0000FFFF)
			/**< Extract module code from error code (#uint32_t) */

#define GET_ERROR_MODULE(_errcode)  ((_errcode) & 0x00FF0000)
			/**< Extract error type (#e_error_type) from
			 * error code (#uint32_t)
			 */

#define RETURN_ERROR(_level, _err, _vmsg) { return ERROR_CODE(_err); }

/*
 * @Description  Error Type Enumeration
 */
typedef enum e_error_type {
	E_OK = 0
		/* Never use "RETURN_ERROR" with E_OK; Use "return E_OK;"*/
	, E_WRITE_FAILED = EIO
		/**< Write access failed on memory/device.*/
		/* String: none, or device name.*/
	, E_NO_DEVICE = ENXIO
		/**< The associated device is not initialized.*/
		/* String: none.*/
	, E_NOT_AVAILABLE = EAGAIN
		/**< Resource is unavailable.*/
		/* String: none, unless the operation is not the main goal of
		 *	   the function (in this case add resource description).
		 */
	, E_NO_MEMORY = ENOMEM
		/**< External memory allocation failed.*/
		/* String: description of item for which allocation failed. */
	, E_INVALID_ADDRESS = EFAULT
		/**< Invalid address.*/
		/*   String: description of the specific violation.*/
	, E_BUSY = EBUSY
		/**< Resource or module is busy.*/
		/* String: none, unless the operation is not the main goal
		 *	   of the function (in this case add resource
		 *	   description).
		 */
	, E_ALREADY_EXISTS = EEXIST
		/**< Requested resource or item already exists.*/
		/* Use when resource duplication or sharing are not allowed.
		 * String: none, unless the operation is not the main goal
		 *	   of the function (in this case add item description).
		 */
	, E_INVALID_OPERATION = ENODEV
		/**< The operation/command is invalid (unrecognized).*/
		/* String: none.*/
	, E_INVALID_VALUE = EDOM
		/**< Invalid value.*/
		/* Use for non-enumeration parameters, and only when other error
		 * types are not suitable.
		 * String: parameter description + "(should be <attribute>)",
		 *	   e.g: "Maximum Rx buffer length (should be divisible
		 *	   by 8)", "Channel number (should be even)".
		 */
	, E_NOT_IN_RANGE = ERANGE
		/**< Parameter value is out of range.*/
		/* Don't use this error for enumeration parameters.
		 * String: parameter description + "(should be %d-%d)",
		 *	   e.g: "Number of pad characters (should be 0-15)".
		 */
	, E_NOT_SUPPORTED = ENOSYS
		/**< The function is not supported or not implemented.*/
		/* String: none.*/
	, E_INVALID_STATE
		/**< The operation is not allowed in current module state.*/
		/* String: none.*/
	, E_INVALID_HANDLE
		/**< Invalid handle of module or object.*/
		/* String: none, unless the function takes in more than one
		 *	   handle (in this case add the handle description)
		 */
	, E_INVALID_ID
		/**< Invalid module ID (usually enumeration or index).*/
		/* String: none, unless the function takes in more than one ID
		 *	   (in this case add the ID description)
		 */
	, E_NULL_POINTER
		/**< Unexpected NULL pointer.*/
		/* String: pointer description.*/
	, E_INVALID_SELECTION
		/**< Invalid selection or mode.*/
		/* Use for enumeration values, only when other error types are
		 * not suitable.
		 * String: parameter description.
		 */
	, E_INVALID_COMM_MODE
		/**< Invalid communication mode.*/
		/* String: none, unless the function takes in more than one
		 *	   communication mode indications (in this case add
		 *	   parameter description).
		 */
	, E_INVALID_MEMORY_TYPE
		/**< Invalid memory type.*/
		/* String: none, unless the function takes in more than one
		 *	   memory types (in this case add memory description,
		 *	   e.g: "Data memory", "Buffer descriptors memory").
		 */
	, E_INVALID_CLOCK
		/**< Invalid clock.*/
		/* String: none, unless the function takes in more than one
		 *	   clocks (in this case add clock description, e.g: "Rx
		 *	   clock", "Tx clock").
		 */
	, E_CONFLICT
		/**< Some setting conflicts with another setting.*/
		/* String: description of the conflicting settings.*/
	, E_NOT_ALIGNED
		/**< Non-aligned address.*/
		/* String: parameter description + "(should be %d-bytes
		 *	   aligned)", e.g: "Rx data buffer (should be 32-bytes
		 *	   aligned)".
		 */
	, E_NOT_FOUND
		/**< Requested resource or item was not found.*/
		/* Use only when the resource/item is uniquely identified.
		 * String: none, unless the operation is not the main goal
		 *	   of the function (in this case add item description).
		 */
	, E_FULL
		/**< Resource is full.*/
		/* String: none, unless the operation is not the main goal of
		 *	   the function (in this case add resource description).
		 */
	, E_EMPTY
		/**< Resource is empty.*/
		/* String: none, unless the operation is not the main goal of
		 *	   the function (in this case add resource description).
		 */
	, E_ALREADY_FREE
		/**< Specified resource or item is already free or deleted.*/
		/* String: none, unless the operation is not the main goal
		 *	   of the function (in this case add item description).
		 */
	, E_READ_FAILED
		/**< Read access failed on memory/device.*/
		/* String: none, or device name.*/
	, E_INVALID_FRAME
		/**< Invalid frame object (NULL handle or missing buffers).*/
		/* String: none.*/
	, E_SEND_FAILED
		/**< Send operation failed on device.*/
		/* String: none, or device name.*/
	, E_RECEIVE_FAILED
		/**< Receive operation failed on device.*/
		/* String: none, or device name.*/
	, E_TIMEOUT/* = ETIMEDOUT*/
		/**< The operation timed out.*/
		/* String: none.*/

	, E_DUMMY_LAST	/* NEVER USED */

} e_error_type;

/*
 *
 * @Group	  FM_grp Frame Manager API
 *
 * @Description   FM API functions, definitions and enums
 *
 * @{
 */

/*
 * @Group	  FM_PORT_grp FM Port
 *
 * @Description   FM Port API
 *
 *		  The FM uses a general module called "port" to represent a Tx
 *		  port (MAC), an Rx port (MAC) or Offline Parsing port. The
 *		  number of ports in an FM varies between SOCs. The SW driver
 *		  manages these ports as sub-modules of the FM, i.e. after an FM
 *		  is initialized, its ports may be initialized and operated
 *		  upon.
 *
 *		  The port is initialized aware of its type, but other functions
 *		  on a port may be indifferent to its type. When necessary, the
 *		  driver verifies coherence and returns error if applicable.
 *
 *		  On initialization, user specifies the port type and it's index
 *		  (relative to the port's type) - always starting at 0.
 *
 * @{
 */

/*
 * @Description   An enum for defining port PCD modes.
 *		  This enum defines the superset of PCD engines support - i.e.
 *		  not all engines have to be used, but all have to be enabled.
 *		  The real flow of a specific frame depends on the PCD
 *		  configuration and the frame headers and payload. Note: the
 *		  first engine and the first engine after the parser (if exists)
 *		  should be in order, the order is important as it will define
 *		  the flow of the port. However, as for the rest engines (the
 *		  ones that follows), the order is not important anymore as it
 *		  is defined by the PCD graph itself.
 */
typedef enum e_fm_port_pcd_support {
	e_FM_PORT_PCD_SUPPORT_NONE = 0
		/**< BMI to BMI, PCD is not used */
	, e_FM_PORT_PCD_SUPPORT_PRS_ONLY
		/**< Use only Parser */
	, e_FM_PORT_PCD_SUPPORT_PLCR_ONLY
		/**< Use only Policer */
	, e_FM_PORT_PCD_SUPPORT_PRS_AND_PLCR
		/**< Use Parser and Policer */
	, e_FM_PORT_PCD_SUPPORT_PRS_AND_KG
		/**< Use Parser and Keygen */
	, e_FM_PORT_PCD_SUPPORT_PRS_AND_KG_AND_CC
		/**< Use Parser, Keygen and Coarse Classification */
	, e_FM_PORT_PCD_SUPPORT_PRS_AND_KG_AND_CC_AND_PLCR
		/**< Use all PCD engines */
	, e_FM_PORT_PCD_SUPPORT_PRS_AND_KG_AND_PLCR
		/**< Use Parser, Keygen and Policer */
	, e_FM_PORT_PCD_SUPPORT_PRS_AND_CC
		/**< Use Parser and Coarse Classification */
	, e_FM_PORT_PCD_SUPPORT_PRS_AND_CC_AND_PLCR
		/**< Use Parser and Coarse Classification and Policer */
	, e_FM_PORT_PCD_SUPPORT_CC_ONLY
		/**< Use only Coarse Classification */
#ifdef FM_CAPWAP_SUPPORT
	, e_FM_PORT_PCD_SUPPORT_CC_AND_KG
		/**< Use Coarse Classification,and Keygen */
	, e_FM_PORT_PCD_SUPPORT_CC_AND_KG_AND_PLCR
		/**< Use Coarse Classification, Keygen and Policer */
#endif /* FM_CAPWAP_SUPPORT */
} e_fm_port_pcd_support;

/*
 * @Description   Port interrupts
 */
typedef enum e_fm_port_exceptions {
	e_FM_PORT_EXCEPTION_IM_BUSY	/**< Independent-Mode Rx-BUSY */
} e_fm_port_exceptions;

/*
 * @Collection	General FM Port defines
 */
#define FM_PORT_PRS_RESULT_NUM_OF_WORDS	8
		/**< Number of 4 bytes words in parser result */
/* @} */

/*
 * @Collection   FM Frame error
 */
typedef uint32_t	fm_port_frame_err_select_t;
			/**< typedef for defining Frame Descriptor errors */

#define FM_PORT_FRM_ERR_UNSUPPORTED_FORMAT FM_FD_ERR_UNSUPPORTED_FORMAT
			/**< Not for Rx-Port! Unsupported Format */
#define FM_PORT_FRM_ERR_LENGTH		FM_FD_ERR_LENGTH
			/**< Not for Rx-Port! Length Error */
#define FM_PORT_FRM_ERR_DMA		FM_FD_ERR_DMA	/**< DMA Data error */
#define FM_PORT_FRM_ERR_NON_FM		FM_FD_RX_STATUS_ERR_NON_FM
			/**< non Frame-Manager error; probably come from SEC
			 * that was chained to FM
			 */

#define FM_PORT_FRM_ERR_IPRE		(FM_FD_ERR_IPR & ~FM_FD_IPR)
			/**< IPR error */
#define FM_PORT_FRM_ERR_IPR_NCSP	(FM_FD_ERR_IPR_NCSP & ~FM_FD_IPR)
			/**< IPR non-consistent-sp */

#define FM_PORT_FRM_ERR_IPFE		0
			/**< Obsolete; will be removed in the future */

#ifdef FM_CAPWAP_SUPPORT
#define FM_PORT_FRM_ERR_CRE		FM_FD_ERR_CRE
#define FM_PORT_FRM_ERR_CHE		FM_FD_ERR_CHE
#endif /* FM_CAPWAP_SUPPORT */

#define FM_PORT_FRM_ERR_PHYSICAL	FM_FD_ERR_PHYSICAL
			/**< Rx FIFO overflow, FCS error, code error, running
			 * disparity error (SGMII and TBI modes), FIFO parity
			 * error. PHY Sequence error, PHY error control
			 * character detected.
			 */
#define FM_PORT_FRM_ERR_SIZE		FM_FD_ERR_SIZE
			/**< Frame too long OR Frame size exceeds
			 * max_length_frame
			 */
#define FM_PORT_FRM_ERR_CLS_DISCARD	FM_FD_ERR_CLS_DISCARD
			/**< indicates a classifier "drop" operation */
#define FM_PORT_FRM_ERR_EXTRACTION	FM_FD_ERR_EXTRACTION
			/**< Extract Out of Frame */
#define FM_PORT_FRM_ERR_NO_SCHEME	FM_FD_ERR_NO_SCHEME
			/**< No Scheme Selected */
#define FM_PORT_FRM_ERR_KEYSIZE_OVERFLOW FM_FD_ERR_KEYSIZE_OVERFLOW
			/**< Keysize Overflow */
#define FM_PORT_FRM_ERR_COLOR_RED	FM_FD_ERR_COLOR_RED
			/**< Frame color is red */
#define FM_PORT_FRM_ERR_COLOR_YELLOW	FM_FD_ERR_COLOR_YELLOW
			/**< Frame color is yellow */
#define FM_PORT_FRM_ERR_ILL_PLCR	FM_FD_ERR_ILL_PLCR
			/**< Illegal Policer Profile selected */
#define FM_PORT_FRM_ERR_PLCR_FRAME_LEN	FM_FD_ERR_PLCR_FRAME_LEN
			/**< Policer frame length error */
#define FM_PORT_FRM_ERR_PRS_TIMEOUT	FM_FD_ERR_PRS_TIMEOUT
			/**< Parser Time out Exceed */
#define FM_PORT_FRM_ERR_PRS_ILL_INSTRUCT FM_FD_ERR_PRS_ILL_INSTRUCT
			/**< Invalid Soft Parser instruction */
#define FM_PORT_FRM_ERR_PRS_HDR_ERR	FM_FD_ERR_PRS_HDR_ERR
			/**< Header error was identified during parsing */
#define FM_PORT_FRM_ERR_BLOCK_LIMIT_EXCEEDED	FM_FD_ERR_BLOCK_LIMIT_EXCEEDED
			/**< Frame parsed beyond 256 first bytes */
#define FM_PORT_FRM_ERR_PROCESS_TIMEOUT	0x00000001
			/**< FPM Frame Processing Timeout Exceeded */
/* @} */


/*
 * @Group	  FM_PORT_init_grp FM Port Initialization Unit
 *
 * @Description   FM Port Initialization Unit
 *
 * @{
 */

/*
 * @Description   Exceptions user callback routine, will be called upon an
 *		  exception passing the exception identification.
 *
 * @Param[in]	  h_app		User's application descriptor.
 * @Param[in]	  exception	The exception.
 */
typedef void (t_fm_port_exception_callback) (t_handle h_app,
					e_fm_port_exceptions exception);

/*
 * @Description   User callback function called by driver with received data.
 *		  User provides this function. Driver invokes it.
 *
 * @Param[in]	  h_app		Application's handle originally specified to
 *				the API Config function
 * @Param[in]	  p_data	A pointer to data received
 * @Param[in]	  length	length of received data
 * @Param[in]	  status	receive status and errors
 * @Param[in]	  position	position of buffer in frame
 * @Param[in]	  h_buf_context	A handle of the user associated with this buffer
 *
 * @Retval	  e_RX_STORE_RESPONSE_CONTINUE
 *		  order the driver to continue Rx operation for all ready data.
 * @Retval	  e_RX_STORE_RESPONSE_PAUSE
 *		  order the driver to stop Rx operation.
 */
typedef e_rx_store_response(t_fm_port_im_rx_store_callback) (t_handle h_app,
					uint8_t  *p_data,
					uint16_t length,
					uint16_t status,
					uint8_t  position,
					t_handle h_buf_context);

/*
 * @Description   User callback function called by driver when transmit
 *		  completed.
 *		  User provides this function. Driver invokes it.
 *
 * @Param[in]	  h_app		Application's handle originally specified to
 *				the API Config function
 * @Param[in]	  p_data	A pointer to data received
 * @Param[in]	  status	transmit status and errors
 * @Param[in]	  last_buffer	is last buffer in frame
 * @Param[in]	  h_buf_context	A handle of the user associated with this buffer
 */
typedef void (t_fm_port_im_tx_conf_callback) (t_handle   h_app,
				uint8_t	*p_data,
				uint16_t   status,
				t_handle   h_buf_context);

/*
 * @Description   A structure for additional Rx port parameters
 */
typedef struct t_fm_port_rx_params {
	uint32_t		err_fqid;	/**< Error Queue Id. */
	uint32_t		dflt_fqid;	/**< Default Queue Id.*/
	uint16_t		liodn_offset;	/**< Port's LIODN offset. */
	t_fm_ext_pools		ext_buf_pools;
			/**< Which external buffer pools are used
			 * (up to FM_PORT_MAX_NUM_OF_EXT_POOLS), and their sizes
			 */
} t_fm_port_rx_params;

/*
 * @Description   A structure for additional non-Rx port parameters
 */
typedef struct t_fm_port_non_rx_params {
	uint32_t		err_fqid;	/**< Error Queue Id. */
	uint32_t		dflt_fqid;
			/**< For Tx - Default Confirmation queue,
			 * 0 means no Tx confirmation for processed frames.
			 * For OP port - default Rx queue.
			 */
	uint32_t		qm_channel;
			/**< QM-channel dedicated to this port; will be used by
			 * the FM for dequeue.
			 */
} t_fm_port_non_rx_params;

/*
 * @Description   A structure for additional Rx port parameters
 */
typedef struct t_fm_port_im_rx_tx_params {
	t_handle			h_fm_muram;
			/**< A handle of the FM-MURAM partition */
	uint16_t			liodn_offset;
			/**< For Rx ports only. Port's LIODN Offset. */
	uint8_t			data_mem_id;
			/**< Memory partition ID for data buffers */
	uint32_t			data_mem_attributes;
			/**< Memory attributes for data buffers */
	t_buffer_pool_info		rx_pool_params;
			/**< For Rx ports only. */
	t_fm_port_im_rx_store_callback   *f_rx_store;
			/**< For Rx ports only. */
	t_fm_port_im_tx_conf_callback	*f_tx_conf;
			/**< For Tx ports only. */
} t_fm_port_im_rx_tx_params;

/*
 * @Description   A union for additional parameters depending on port type
 */
typedef union u_fm_port_specific_params {
	t_fm_port_im_rx_tx_params	im_rx_tx_params;
			/**< Rx/Tx Independent-Mode port parameter structure */
	t_fm_port_rx_params	rx_params;
			/**< Rx port parameters structure */
	t_fm_port_non_rx_params	non_rx_params;
			/**< Non-Rx port parameters structure */
} u_fm_port_specific_params;

/*
 * @Description   A structure representing FM initialization parameters
 */
typedef struct t_fm_port_params {
	uintptr_t	base_addr;
			/**< Virtual Address of memory mapped FM Port registers.
			 */
	t_handle	h_fm;
			/**< A handle to the FM object this port related to */
	e_fm_port_type	port_type;	/**< Port type */
	uint8_t		port_id;
			/**< Port Id - relative to type;
			 * NOTE: When configuring Offline Parsing port for
			 * FMANv3 devices (DPAA_VERSION 11 and higher),
			 * it is highly recommended NOT to use port_id=0 due to
			 * lack of HW resources on port_id=0.
			 */
	bool		independent_mode_enable;
			/**< This port is Independent-Mode - Used for Rx/Tx
			 * ports only!
			 */
	uint16_t		liodn_base;
			/**< Irrelevant for P4080 rev 1. LIODN base for this
			 * port, to be used together with LIODN offset.
			 */
	u_fm_port_specific_params	specific_params;
			/**< Additional parameters depending on port type. */

	t_fm_port_exception_callback   *f_exception;
			/**< Relevant for IM only Callback routine to be called
			 * on BUSY exception
			 */
	t_handle		h_app;
			/**< A handle to an application layer object; This
			 * handle will be passed by the driver upon calling the
			 * above callbacks
			 */
} t_fm_port_params;

/*
 * @Function	  fm_port_config
 *
 * @Description   Creates a descriptor for the FM PORT module.
 *
 *		  The routine returns a handle(descriptor) to the FM PORT
 *		  object. This descriptor must be passed as first parameter to
 *		  all other FM PORT function calls.
 *
 *		  No actual initialization or configuration of FM hardware is
 *		  done by this routine.
 *
 * @Param[in]	  p_fm_port_params	Pointer to data structure of parameters
 *
 * @Retval	  Handle to FM object, or NULL for Failure.
 */
t_handle fm_port_config(t_fm_port_params *p_fm_port_params);

/*
 * @Function	  fm_port_init
 *
 * @Description   Initializes the FM PORT module by defining the software
 *		  structure and configuring the hardware registers.
 *
 * @Param[in]	  h_fm_port - FM PORT module descriptor
 *
 * @Return	  E_OK on success; Error code otherwise.
 */
uint32_t fm_port_init(t_handle h_fm_port);

/*
 * @Function	  fm_port_free
 *
 * @Description   Frees all resources that were assigned to FM PORT module.
 *
 *		  Calling this routine invalidates the descriptor.
 *
 * @Param[in]	  h_fm_port - FM PORT module descriptor
 *
 * @Return	  E_OK on success; Error code otherwise.
 */
uint32_t fm_port_free(t_handle h_fm_port);

t_handle fm_port_open(t_fm_port_params *p_fm_port_params);
void fm_port_close(t_handle h_fm_port);


/*
 * @Group	  FM_PORT_advanced_init_grp	FM Port Advanced Configuration
 *						Unit
 *
 * @Description   Configuration functions used to change default values.
 *
 * @{
 */

/*
 * @Description   enum for defining QM frame dequeue
 */
typedef enum e_fm_port_deq_type {
	e_FM_PORT_DEQ_TYPE1,
		/**< Dequeue from the SP channel - with priority precedence,
		 * and Intra-Class Scheduling respected.
		 */
	e_FM_PORT_DEQ_TYPE2,
		/**< Dequeue from the SP channel - with active FQ precedence,
		 * and Intra-Class Scheduling respected.
		 */
	e_FM_PORT_DEQ_TYPE3
		/**< Dequeue from the SP channel - with active FQ precedence,
		 * and override Intra-Class Scheduling
		 */
} e_fm_port_deq_type;

/*
 * @Description   enum for defining QM frame dequeue
 */
typedef enum e_fm_port_deq_prefetch_option {
	e_FM_PORT_DEQ_NO_PREFETCH,
		/**< QMI performs a dequeue action for a single frame
		 * only when a dedicated portID Tnum is waiting.
		 */
	e_FM_PORT_DEQ_PARTIAL_PREFETCH,
		/**< QMI performs a dequeue action for 3 frames
		 * when one dedicated port_id tnum is waiting.
		 */
	e_FM_PORT_DEQ_FULL_PREFETCH
		/**< QMI performs a dequeue action for 3 frames when
		 * no dedicated port_id tnums are waiting.
		 */

} e_fm_port_deq_prefetch_option;

/*
 * @Description   enum for defining port default color
 */
typedef enum e_fm_port_color {
	e_FM_PORT_COLOR_GREEN,	/**< Default port color is green */
	e_FM_PORT_COLOR_YELLOW,	/**< Default port color is yellow */
	e_FM_PORT_COLOR_RED,	/**< Default port color is red */
	e_FM_PORT_COLOR_OVERRIDE/**< Ignore color */
} e_fm_port_color;

/*
 * @Description   A structure for defining Dual Tx rate limiting scale
 */
typedef enum e_fm_port_dual_rate_limiter_scale_down {
	e_FM_PORT_DUAL_RATE_LIMITER_NONE = 0,
		/**< Use only single rate limiter*/
	e_FM_PORT_DUAL_RATE_LIMITER_SCALE_DOWN_BY_2,
		/**< Divide high rate limiter by 2 */
	e_FM_PORT_DUAL_RATE_LIMITER_SCALE_DOWN_BY_4,
		/**< Divide high rate limiter by 4 */
	e_FM_PORT_DUAL_RATE_LIMITER_SCALE_DOWN_BY_8
		/**< Divide high rate limiter by 8 */
} e_fm_port_dual_rate_limiter_scale_down;

/*
 * @Description   A structure for defining FM port resources
 */
typedef struct t_fm_port_rsrc {
	uint32_t	num;
			/**< Committed required resource */
	uint32_t	extra;
			/**< Extra (not committed) required resource */
} t_fm_port_rsrc;

/*
 * @Description   A structure for defining Tx rate limiting
 */
typedef struct t_fm_port_rate_limit {
	uint16_t		max_burst_size;
				/**< in KBytes for Tx ports, in frames for OP
				 * ports. (note that for early chips burst size
				 * is rounded up to a multiply of 1000 frames).
				 */
	uint32_t		rate_limit;
				/**< in Kb/sec for Tx ports, in frame/sec for OP
				 * ports. Rate limit refers to data rate
				 * (rather than line rate).
				 */
	e_fm_port_dual_rate_limiter_scale_down	rate_limit_divider;
				/**< For OP ports only. Not-valid for some
				 * earlier chip revisions
				 */
} t_fm_port_rate_limit;

/*
 * @Description   A structure for defining the parameters of
 *		  the Rx port performance counters
 */
typedef struct t_fm_port_performance_cnt {
	uint8_t	task_comp_val;
		/**< Task compare value */
	uint8_t	queue_comp_val;
		/**< Rx queue/Tx confirm queue compare value (unused for H/O) */
	uint8_t	dma_comp_val;
		/**< Dma compare value */
	uint32_t	fifo_comp_val;
			/**< Fifo compare value (in bytes) */
} t_fm_port_performance_cnt;

/*
 * @Function	  fm_port_config_num_of_open_dmas
 *
 * @Description   Calling this routine changes the max number of open DMA's
 *		  available for this port. It changes this parameter in the
 *		  internal driver data base from its default configuration
 *		  [OP: 1]
 *		  [1G-RX, 1G-TX: 1 (+1)]
 *		  [10G-RX, 10G-TX: 8 (+8)]
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  p_open_dmas	A pointer to a structure of parameters defining
 *				the open DMA allocation.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_num_of_open_dmas(t_handle h_fm_port,
						t_fm_port_rsrc *p_open_dmas);

/*
 * @Function	  fm_port_config_num_of_tasks
 *
 * @Description   Calling this routine changes the max number of tasks available
 *		  for this port. It changes this parameter in the internal
 *		  driver data base from its default configuration
 *		  [OP : 1]
 *		  [1G - RX, 1G - TX : 3 ( + 2)]
 *		  [10G - RX, 10G - TX : 16 ( + 8)]
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  p_num_of_tasks	A pointer to a structure of parameters
 *					defining the tasks allocation.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_num_of_tasks(t_handle h_fm_port,
					t_fm_port_rsrc *p_num_of_tasks);

/*
 * @Function	  fm_port_config_size_of_fifo
 *
 * @Description   Calling this routine changes the max FIFO size configured for
 *		  this port.
 *
 *		  This function changes the internal driver data base from its
 *		  default configuration. Please refer to the driver's User Guide
 *		  for information on default FIFO sizes in the various devices.
 *		  [OP: 2KB]
 *		  [1G-RX, 1G-TX: 11KB]
 *		  [10G-RX, 10G-TX: 12KB]
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  p_size_of_fifo	A pointer to a structure of parameters
 *					defining the FIFO allocation.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_size_of_fifo(t_handle h_fm_port,
					t_fm_port_rsrc *p_size_of_fifo);

/*
 * @Function	  fm_port_config_deq_high_priority
 *
 * @Description   Calling this routine changes the dequeue priority in the
 *		  internal driver data base from its default configuration
 *		  1G: [DEFAULT_PORT_deqHighPriority_1G]
 *		  10G: [DEFAULT_PORT_deqHighPriority_10G]
 *
 *		  May be used for Non - Rx ports only
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  high_pri	TRUE to select high priority, FALSE for normal
 *				operation.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_deq_high_priority(t_handle h_fm_port, bool high_pri);

/*
 * @Function	  fm_port_config_deq_type
 *
 * @Description   Calling this routine changes the dequeue type parameter in the
 *		  internal driver data base from its default configuration
 *		  [DEFAULT_PORT_deq_type].
 *
 *		  May be used for Non - Rx ports only
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  deq_type	According to QM definition.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_deq_type(t_handle h_fm_port,
					e_fm_port_deq_type deq_type);

/*
 * @Function	  fm_port_config_deq_prefetch_option
 *
 * @Description   Calling this routine changes the dequeue prefetch option
 *		  parameter in the internal driver data base from its default
 *		  configuration [DEFAULT_PORT_deq_prefetch_option]
 *		  Note: Available for some chips only
 *
 *		  May be used for Non - Rx ports only
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  deq_prefetch_option	New option
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_deq_prefetch_option(t_handle h_fm_port,
			e_fm_port_deq_prefetch_option deq_prefetch_option);

/*
 * @Function	  fm_port_config_deq_byte_cnt
 *
 * @Description   Calling this routine changes the dequeue byte count parameter
 *		  in the internal driver data base from its default
 *		  configuration.
 *		  1G:[DEFAULT_PORT_deq_byte_cnt_1G].
 *		  10G:[DEFAULT_PORT_deq_byte_cnt_10G].
 *
 *		  May be used for Non - Rx ports only
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  deq_byte_cnt	New byte count
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_deq_byte_cnt(t_handle h_fm_port,
					uint16_t deq_byte_cnt);

/*
 * @Function	  fm_port_config_buffer_prefix_content
 *
 * @Description   Defines the structure, size and content of the application
 *		  buffer. The prefix will In Tx ports, if 'pass_prs_result', the
 *		  application should set a value to their offsets in the prefix
 *		  of the FM will save the first 'priv_data_size', than,
 *		  depending on 'pass_prs_result' and 'pass_time_stamp', copy
 *		  parse result and timeStamp, and the packet itself (in this
 *		  order), to the application buffer, and to offset.
 *		  Calling this routine changes the buffer margins definitions in
 *		  the internal driver data base from its default configuration:
 *		  Data size:  [DEFAULT_PORT_bufferPrefixContent_priv_data_size]
 *		  Pass Parser result:
 *		  [DEFAULT_PORT_bufferPrefixContent_pass_prs_result].
 *		  Pass timestamp:
 *		  [DEFAULT_PORT_bufferPrefixContent_pass_time_stamp].
 *
 *		  May be used for all ports
 *
 *
 * @Param[in]		h_fm_port			A handle to a FM Port
 *							module.
 * @Param[in,out]	p_fm_buffer_prefix_content	A structure of
 *							parameters describing
 *							the structure of the
 *							buffer.
 *							Out parameter: Start
 *							margin - offset of data
 *							from start of external
 *							buffer.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_buffer_prefix_content(t_handle	h_fm_port,
		t_fm_buffer_prefix_content	*p_fm_buffer_prefix_content);

/*
 * @Function	  fm_port_config_checksum_last_bytes_ignore
 *
 * @Description   Calling this routine changes the number of checksum bytes to
 *		  ignore parameter in the internal driver data base from its
 *		  default configuration.
 *
 *		  May be used by Tx & Rx ports only
 *
 * @Param[in]	  h_fm_port			A handle to a FM Port module.
 * @Param[in]	  checksum_last_bytes_ignore	New value
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_checksum_last_bytes_ignore(t_handle h_fm_port,
			uint8_t checksum_last_bytes_ignore);

/*
 * @Function	  fm_port_config_cut_bytes_from_end
 *
 * @Description   Calling this routine changes the number of bytes to cut from a
 *		  frame's end parameter in the internal driver data base
 *		  from its default configuration
 *		  [DEFAULT_PORT_cut_bytes_from_end]
 *		  Note that if the result of (frame length before chop -
 *		  cut_bytes_from_end) is less than 14 bytes, the chop operation
 *		  is not executed.
 *
 *		  May be used for Rx ports only
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  cut_bytes_from_end	New value
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_cut_bytes_from_end(t_handle h_fm_port,
			uint8_t cut_bytes_from_end);

/*
 * @Function	  fm_port_config_ext_buf_pools
 *
 * @Description   This routine should be called for OP ports that internally use
 *		  BM buffer pools. In such cases, e.g. for fragmentation and
 *		  re-assembly, the FM needs new BM buffers. By calling this
 *		  routine the user specifies the BM buffer pools that should be
 *		  used.
 *
 *		  Note: Available for some chips only
 *
 *		  May be used for OP ports only
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  p_fm_ext_pools	A structure of parameters for the
 *					external pools.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_ext_buf_pools(t_handle h_fm_port,
			t_fm_ext_pools *p_fm_ext_pools);

/*
 * @Function	  fm_port_config_backup_pools
 *
 * @Description   Calling this routine allows the configuration of some of the
 *		  BM pools defined for this port as backup pools.
 *		  A pool configured to be a backup pool will be used only if all
 *		  other enabled non - backup pools are depleted.
 *
 *		  May be used for Rx ports only
 *
 * @Param[in]	  h_fm_port			A handle to a FM Port module.
 * @Param[in]	  p_fm_port_backup_bm_pools	An array of pool id's. All pools
 *						specified here will be defined
 *						as backup pools.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_backup_pools(t_handle h_fm_port,
			t_fm_backup_bm_pools *p_fm_port_backup_bm_pools);

/*
 * @Function	  fm_port_config_frm_discard_override
 *
 * @Description   Calling this routine changes the error frames destination
 *		  parameter in the internal driver data base from its default
 *		  configuration: override =[DEFAULT_PORT_frmDiscardOverride]
 *
 *		  May be used for Rx and OP ports only
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  override	TRUE to override discarding of error frames and
 *				enqueueing them to error queue.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_frm_discard_override(t_handle h_fm_port,
			bool override);

/*
 * @Function	fm_port_config_errors_to_discard
 *
 * @Description   Calling this routine changes the behaviour on error parameter
 *		  in the internal driver data base from its default
 *		  configuration: [DEFAULT_PORT_errorsToDiscard].
 *		  If a requested error was previously defined as
 *		  "ErrorsToEnqueue" it's definition will change and the frame
 *		  will be discarded. Errors that were not defined either as
 *		  "ErrorsToEnqueue" nor as "ErrorsToDiscard", will be forwarded
 *		  to CPU.
 *
 *		  May be used for Rx and OP ports only
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  errs		A list of errors to discard
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_errors_to_discard(t_handle h_fm_port,
		fm_port_frame_err_select_t errs);

/*
 * @Function	  fm_port_config_dma_ic_cache_attr
 *
 * @Description   Calling this routine changes the internal context cache
 *		  attribute parameter in the internal driver data base
 *		  from its default configuration:
 *		  [DEFAULT_PORT_dmaIntContextCacheAttr]
 *
 *		  May be used for all port types
 *
 * @Param[in]	  h_fm_port			A handle to a FM Port module.
 * @Param[in]	  int_context_cache_attr	New selection
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_dma_ic_cache_attr(t_handle h_fm_port,
		e_fm_dma_cache_option int_context_cache_attr);

/*
 * @Function	  fm_port_config_dma_hdr_attr
 *
 * @Description   Calling this routine changes the header cache attribute
 *		  parameter in the internal driver data base from its default
 *		  configuration[DEFAULT_PORT_dmaHeaderCacheAttr]
 *
 *		  May be used for all port types
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  header_cache_attr	New selection
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_dma_hdr_attr(t_handle h_fm_port,
		e_fm_dma_cache_option header_cache_attr);

/*
 * @Function	fm_port_config_dma_scatter_gather_attr
 *
 * @Description   Calling this routine changes the scatter gather cache
 *		  attribute parameter in the internal driver data base from its
 *		  default configuration[DEFAULT_PORT_dmaScatterGatherCacheAttr]
 *
 *		  May be used for all port types
 *
 * @Param[in]	  h_fm_port			A handle to a FM Port module.
 * @Param[in]	  scatter_gather_cache_attr	New selection
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_dma_scatter_gather_attr(t_handle h_fm_port,
		e_fm_dma_cache_option scatter_gather_cache_attr);

/*
 * @Function	  fm_port_config_dma_write_optimize
 *
 * @Description   Calling this routine changes the write optimization parameter
 *		  in the internal driver data base from its default
 *		  configuration : By default optimize =
 *		  [DEFAULT_PORT_dmaWriteOptimize].
 *		  Note:
 *		  1. For head optimization, data alignment must be >= 16
 *		     (supported by default).
 *
 *		  2. For tail optimization, note that the optimization is
 *		     performed by extending the write transaction of the frame
 *		     payload at the tail as needed to achieve optimal bus
 *		     transfers, so that the last write is extended to be on
 *		     16 / 64 bytes aligned block (chip dependent).
 *
 *		  Relevant for non - Tx port types
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  optimize	TRUE to enable optimization, FALSE for normal
 *				operation
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_dma_write_optimize(t_handle h_fm_port,
						bool optimize);

/*
 * @Function	  fm_port_config_no_scather_gather
 *
 * @Description   Calling this routine changes the no_scather_gather parameter
 *		  in internal driver data base from its default configuration.
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  no_scather_gather	TRUE - frame is discarded if can not be
 *					stored in single buffer,
 *					FALSE - frame can be stored in scatter
 *					gather (S / G) format.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_no_scather_gather(t_handle h_fm_port,
						bool no_scather_gather);

/*
 * @Function	  fm_port_config_dflt_color
 *
 * @Description   Calling this routine changes the internal default color
 *		  parameter in the internal driver data base
 *		  from its default configuration[DEFAULT_PORT_color]
 *
 *		  May be used for all port types
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  color		New selection
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_dflt_color(t_handle h_fm_port, e_fm_port_color color);

/*
 * @Function	  fm_port_config_sync_req
 *
 * @Description   Calling this routine changes the synchronization attribute
 *		  parameter in the internal driver data base from its default
 *		  configuration: sync_req =[DEFAULT_PORT_sync_req]
 *
 *		  May be used for all port types
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  sync_req	TRUE to request synchronization, FALSE
 *				otherwise.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_sync_req(t_handle h_fm_port, bool sync_req);

/*
 * @Function	  fm_port_config_forward_reuse_int_context
 *
 * @Description   This routine is relevant for Rx ports that are routed to OP
 *		  port. It changes the internal context reuse option in the
 *		  internal driver data base from its default configuration:
 *		  reuse =[DEFAULT_PORT_forwardIntContextReuse]
 *
 *		  May be used for Rx ports only
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  reuse		TRUE to reuse internal context on frames
 *				forwarded to OP port.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_forward_reuse_int_context(t_handle h_fm_port,
						bool reuse);

/*
 * @Function	  fm_port_config_donot_release_tx_buf_to_bm
 *
 * @Description   This routine should be called if no Tx confirmation
 *		  is done, and yet buffers should not be released to the BM.
 *
 *		  Normally, buffers are returned using the Tx confirmation
 *		  process. When Tx confirmation is not used (defFqid = 0),
 *		  buffers are typically released to the BM. This routine
 *		  may be called to avoid this behavior and not release the
 *		  buffers.
 *
 *		  May be used for Tx ports only
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_donot_release_tx_buf_to_bm(t_handle h_fm_port);

/*
 * @Function	  fm_port_config_immax_rx_buf_length
 *
 * @Description   Changes the maximum receive buffer length from its default
 *		  configuration: Closest rounded down power of 2 value of the
 *		  data buffer size.
 *
 *		  The maximum receive buffer length directly affects the
 *		  structure of received frames (single- or multi-buffered) and
 *		  the performance of both the FM and the driver.
 *
 *		  The selection between single- or multi-buffered frames should
 *		  be done according to the characteristics of the specific
 *		  application. The recommended mode is to use a single data
 *		  buffer per packet, as this mode provides the best performance.
 *		  However, the user can select to use multiple data buffers per
 *		  packet.
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  new_val	Maximum receive buffer length (in bytes).
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init(). This routine is to be used only if
 *		  Independent-Mode is enabled.
 */
uint32_t fm_port_config_immax_rx_buf_length(t_handle h_fm_port,
						uint16_t new_val);

/*
 * @Function	  fm_port_config_imrx_bd_ring_length
 *
 * @Description   Changes the receive BD ring length from its default
 *		  configuration:[DEFAULT_PORT_rxBdRingLength]
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  new_val	The desired BD ring length.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init(). This routine is to be used only if
 *		  Independent-Mode is enabled.
 */
uint32_t fm_port_config_imrx_bd_ring_length(t_handle h_fm_port,
						uint16_t new_val);

/*
 * @Function	fm_port_config_imtx_bd_ring_length
 *
 * @Description   Changes the transmit BD ring length from its default
 *		  configuration:[DEFAULT_PORT_txBdRingLength]
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  new_val	The desired BD ring length.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init(). This routine is to be used only if
 *		  Independent-Mode is enabled.
 */
uint32_t fm_port_config_imtx_bd_ring_length(t_handle h_fm_port,
					uint16_t new_val);

/*
 * @Function	  fm_port_config_imfman_ctrl_external_structs_memory
 *
 * @Description   Configures memory partition and attributes for FMan-Controller
 *		  data structures (e.g. BD rings).
 *		  Calling this routine changes the internal driver data base
 *		  from its default configuration
 *		  [DEFAULT_PORT_ImfwExtStructsMemId,
 *		  DEFAULT_PORT_ImfwExtStructsMemAttr].
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  mem_id		Memory partition ID.
 * @Param[in]	  mem_attributes	Memory attributes mask (a combination of
 *					MEMORY_ATTR_x flags).
 *
 * @Return	  E_OK on success; Error code otherwise.
 */
uint32_t  fm_port_config_imfman_ctrl_external_structs_memory(t_handle h_fm_port,
				uint8_t mem_id,
				uint32_t mem_attributes);

/*
 * @Function	  fm_port_config_impolling
 *
 * @Description   Changes the Rx flow from interrupt driven (default) to
 *		  polling.
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 *		  This routine is to be used only if Independent-Mode is
 *		  enabled.
 */
uint32_t fm_port_config_impolling(t_handle h_fm_port);

/*
 * @Function	  fm_port_config_max_frame_length
 *
 * @Description   Changes the definition of the max size of frame that should be
 *		  transmitted/received on this port from its default value
 *		  [DEFAULT_PORT_maxFrameLength].
 *		  This parameter is used for confirmation of the minimum Fifo
 *		  size calculations and only for Tx ports or ports working in
 *		  independent mode. This should be larger than the maximum
 *		  possible MTU that will be used for this port (i.e. its MAC).
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  length	Max size of frame
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init(). This routine is to be used only if
 *		  Independent-Mode is enabled.
 */
uint32_t fm_port_config_max_frame_length(t_handle h_fm_port,
					uint16_t length);

/*
 * @Function	  fm_port_config_tx_fifo_min_fill_level
 *
 * @Description   Calling this routine changes the fifo minimum fill level
 *		  parameter in the internal driver data base from its default
 *		  configuration[DEFAULT_PORT_txFifoMinFillLevel]
 *
 *		  May be used for Tx ports only
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  min_fill_level	New value
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_tx_fifo_min_fill_level(t_handle h_fm_port,
					uint32_t min_fill_level);

/*
 * @Function	  fm_port_config_fifo_deq_pipeline_depth
 *
 * @Description   Calling this routine changes the fifo dequeue pipeline depth
 *		  parameter in the internal driver data base
 *
 *		  from its default configuration :
 *		  1G ports : [DEFAULT_PORT_fifoDeqPipelineDepth_1G],
 *		  10G port : [DEFAULT_PORT_fifoDeqPipelineDepth_10G],
 *		  OP port : [DEFAULT_PORT_fifoDeqPipelineDepth_OH]
 *
 *		  May be used for Tx / OP ports only
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  deq_pipeline_depth	New value
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_fifo_deq_pipeline_depth(t_handle h_fm_port,
				uint8_t deq_pipeline_depth);

/*
 * @Function	  fm_port_config_tx_fifo_low_comf_level
 *
 * @Description   Calling this routine changes the fifo low comfort level
 *		  parameter in internal driver data base from its default
 *		  configuration[DEFAULT_PORT_txFifoLowComfLevel]
 *
 *		  May be used for Tx ports only
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  fifo_low_comf_level	New value
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_tx_fifo_low_comf_level(t_handle h_fm_port,
					uint32_t fifo_low_comf_level);

/*
 * @Function	  fm_port_config_rx_fifo_threshold
 *
 * @Description   Calling this routine changes the threshold of the FIFO fill
 *		  level parameter in the internal driver data base from its
 *		  default configuration[DEFAULT_PORT_rxFifoThreshold]
 *
 *		  If the total number of buffers which are currently in use and
 *		  associated with the specific RX port exceed this threshold,
 *		  the BMI will signal the MAC to send a pause frame over the
 *		  link.
 *
 *		  May be used for Rx ports only
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  fifo_threshold	New value
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_rx_fifo_threshold(t_handle h_fm_port,
						uint32_t fifo_threshold);

/*
 * @Function	  fm_port_config_rx_fifo_pri_elevation_level
 *
 * @Description   Calling this routine changes the priority elevation level
 *		  parameter in the internal driver data base from its default
 *		  configuration[DEFAULT_PORT_rxFifoPriElevationLevel]
 *
 *		  If the total number of buffers which are currently in use and
 *		  associated with the specific RX port exceed the amount
 *		  specified in pri_elevation_level, BMI will signal the main
 *		  FM's DMA to elevate the FM priority on the system bus.
 *
 *		  May be used for Rx ports only
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  pri_elevation_level   New value
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_rx_fifo_pri_elevation_level(t_handle h_fm_port,
						uint32_t pri_elevation_level);

#ifdef FM_HEAVY_TRAFFIC_HANG_ERRATA_FMAN_A005669
/*
 * @Function	  fm_port_config_bcbworkaround
 *
 * @Description   Configures BCB errata workaround.
 *
 *		  When BCB errata is applicable, the workaround is always
 *		  performed by FM Controller. Thus, this function does not
 *		  actually enable errata workaround but rather allows driver to
 *		  perform adjustments required due to errata workaround
 *		  execution in FM controller.
 *
 *		  Applying BCB workaround also configures
 *		  FM_PORT_FRM_ERR_PHYSICAL errors to be discarded. Thus
 *		  FM_PORT_FRM_ERR_PHYSICAL can't be set by
 *		  fm_port_set_errors_route() function.
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_bcbworkaround(t_handle h_fm_port);
#endif /* FM_HEAVY_TRAFFIC_HANG_ERRATA_FMAN_A005669 */

/*
 * @Function	  fm_port_config_internal_buff_offset
 *
 * @Description   Configures internal buffer offset.
 *
 *		  May be used for Rx and OP ports only
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  val		New value
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_config_internal_buff_offset(t_handle h_fm_port, uint8_t val);

/** @} */ /* end of FM_PORT_advanced_init_grp group */
/** @} */ /* end of FM_PORT_init_grp group */

/*
 * @Group	  FM_PORT_runtime_control_grp FM Port Runtime Control Unit
 *
 * @Description   FM Port Runtime control unit API functions, definitions and
 *		  enums.
 *
 * @{
 */

/*
 * @Description   enum for defining FM Port counters
 */
typedef enum e_fm_port_counters {
	e_FM_PORT_COUNTERS_CYCLE,		/**< BMI performance counter */
	e_FM_PORT_COUNTERS_TASK_UTIL,		/**< BMI performance counter */
	e_FM_PORT_COUNTERS_QUEUE_UTIL,		/**< BMI performance counter */
	e_FM_PORT_COUNTERS_DMA_UTIL,		/**< BMI performance counter */
	e_FM_PORT_COUNTERS_FIFO_UTIL,		/**< BMI performance counter */
	e_FM_PORT_COUNTERS_RX_PAUSE_ACTIVATION,
			/**< BMI Rx only performance counter */
	e_FM_PORT_COUNTERS_FRAME,		/**< BMI statistics counter */
	e_FM_PORT_COUNTERS_DISCARD_FRAME,	/**< BMI statistics counter */
	e_FM_PORT_COUNTERS_DEALLOC_BUF,
			/**< BMI deallocate buffer statistics counter */
	e_FM_PORT_COUNTERS_RX_BAD_FRAME,
			/**< BMI Rx only statistics counter */
	e_FM_PORT_COUNTERS_RX_LARGE_FRAME,
			/**< BMI Rx only statistics counter */
	e_FM_PORT_COUNTERS_RX_FILTER_FRAME,
			/**< BMI Rx & OP only statistics counter */
	e_FM_PORT_COUNTERS_RX_LIST_DMA_ERR,
			/**< BMI Rx, OP & HC only statistics counter */
	e_FM_PORT_COUNTERS_RX_OUT_OF_BUFFERS_DISCARD,
			/**< BMI Rx, OP & HC statistics counter */
	e_FM_PORT_COUNTERS_PREPARE_TO_ENQUEUE_COUNTER,
			/**< BMI Rx, OP & HC only statistics counter */
	e_FM_PORT_COUNTERS_WRED_DISCARD,
			/**< BMI OP & HC only statistics counter */
	e_FM_PORT_COUNTERS_LENGTH_ERR,
			/**< BMI non-Rx statistics counter */
	e_FM_PORT_COUNTERS_UNSUPPRTED_FORMAT,
			/**< BMI non-Rx statistics counter */
	e_FM_PORT_COUNTERS_DEQ_TOTAL,	/**< QMI total QM dequeues counter */
	e_FM_PORT_COUNTERS_ENQ_TOTAL,	/**< QMI total QM enqueues counter */
	e_FM_PORT_COUNTERS_DEQ_FROM_DEFAULT,	/**< QMI counter */
	e_FM_PORT_COUNTERS_DEQ_CONFIRM		/**< QMI counter */
} e_fm_port_counters;

typedef struct t_fm_port_bmi_stats {
	uint32_t cnt_cycle;
	uint32_t cnt_task_util;
	uint32_t cnt_queue_util;
	uint32_t cnt_dma_util;
	uint32_t cnt_fifo_util;
	uint32_t cnt_rx_pause_activation;
	uint32_t cnt_frame;
	uint32_t cnt_discard_frame;
	uint32_t cnt_dealloc_buf;
	uint32_t cnt_rx_bad_frame;
	uint32_t cnt_rx_large_frame;
	uint32_t cnt_rx_filter_frame;
	uint32_t cnt_rx_list_dma_err;
	uint32_t cnt_rx_out_of_buffers_discard;
	uint32_t cnt_wred_discard;
	uint32_t cnt_length_err;
	uint32_t cnt_unsupported_format;
} t_fm_port_bmi_stats;

/*
 * @Description   Structure for Port id parameters.
 *		  Fields commented 'IN' are passed by the port module to be used
 *		  by the FM module.
 *		  Fields commented 'OUT' will be filled by FM before returning
 *		  to port.
 */
typedef struct t_fm_port_congestion_grps {
	uint16_t	num_of_congestion_grps_to_consider;
			/**< The number of required CGs to define the size of
			 * the following array
			 */
	uint8_t	congestion_grps_to_consider[FM_NUM_CONG_GRPS];
			/**< An array of CG indexes; Note that the size of the
			 * array should be 'num_of_congestion_grps_to_consider'.
			 */
	bool	pfc_prio_enable[FM_NUM_CONG_GRPS][FM_MAX_PFC_PRIO];
			/**< a matrix that represents the map between the CG ids
			 * defined in 'congestion_grps_to_consider' to the
			 * priorities mapping array.
			 */
} t_fm_port_congestion_grps;


#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
/*
 * @Function	  fm_port_dump_regs
 *
 * @Description   Dump all regs.
 *
 *		  Calling this routine invalidates the descriptor.
 *
 * @Param[in]	  h_fm_port	FM PORT module descriptor
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_dump_regs(t_handle h_fm_port);
#endif /* (defined(DEBUG_ERRORS) && ... */

/*
 * @Function	  fm_port_get_buffer_data_offset
 *
 * @Description   Relevant for Rx ports. Returns the data offset from the
 *		  beginning of the data buffer
 *
 * @Param[in]	  h_fm_port	FM PORT module descriptor
 *
 * @Return	  data offset.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_get_buffer_data_offset(t_handle h_fm_port);

/*
 * @Function	  fm_port_get_buffer_icinfo
 *
 * @Description   Returns the Internal Context offset from the beginning of the
 *		  data buffer
 *
 * @Param[in]	  h_fm_port	FM PORT module descriptor
 * @Param[in]	  p_data	A pointer to the data buffer.
 *
 * @Return	  Internal context info pointer on success, NULL if
 *		  'allOtherInfo' was not configured for this port.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint8_t *fm_port_get_buffer_icinfo(t_handle h_fm_port, char *p_data);

/*
 * @Function	  fm_port_get_buffer_prs_result
 *
 * @Description   Returns the pointer to the parse result in the data buffer.
 *		  In Rx ports this is relevant after reception, if parse result
 *		  is configured to be part of the data passed to the
 *		  application. For non Rx ports it may be used to get the
 *		  pointer of the area in the buffer where parse result should be
 *		  initialized - if so configured.
 *		  See fm_port_config_buffer_prefix_content for data buffer
 *		  prefix configuration.
 *
 * @Param[in]	  h_fm_port	FM PORT module descriptor
 * @Param[in]	  p_data	A pointer to the data buffer.
 *
 * @Return	  Parse result pointer on success, NULL if parse result was not
 *		  configured for this port.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
t_fm_prs_result *fm_port_get_buffer_prs_result(t_handle h_fm_port,
						char *p_data);

/*
 * @Function	  fm_port_get_buffer_time_stamp
 *
 * @Description   Returns the time stamp in the data buffer.
 *		  Relevant for Rx ports for getting the buffer time stamp.
 *		  See fm_port_config_buffer_prefix_content for data buffer
 *		  prefix configuration.
 *
 * @Param[in]	  h_fm_port	FM PORT module descriptor
 * @Param[in]	  p_data	A pointer to the data buffer.
 *
 * @Return	  A pointer to the hash result on success, NULL otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint64_t *fm_port_get_buffer_time_stamp(t_handle h_fm_port, char *p_data);

/*
 * @Function	  fm_port_get_buffer_hash_result
 *
 * @Description   Given a data buffer, on the condition that hash result was
 *		  defined as a part of the buffer content(see
 *		  fm_port_config_buffer_prefix_content) this routine will return
 *		  the pointer to the hash result location in the buffer prefix.
 *
 * @Param[in]	  h_fm_port	FM PORT module descriptor
 * @Param[in]	  p_data	A pointer to the data buffer.
 *
 * @Return	  A pointer to the hash result on success, NULL otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint8_t *fm_port_get_buffer_hash_result(t_handle h_fm_port, char *p_data);

/*
 * @Function	  fm_port_disable
 *
 * @Description   Gracefully disable an FM port. The port will not start new
 *		  tasks after all tasks associated with the port are terminated.
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 *		  This is a blocking routine, it returns after port is
 *		  gracefully stopped, i.e. the port will not except new frames,
 *		  but it will finish all frames or tasks which were already
 *		  began
 */
uint32_t fm_port_disable(t_handle h_fm_port);

/*
 * @Function	  fm_port_enable
 *
 * @Description   A runtime routine provided to allow disable/enable of port.
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	 Allowed only following fm_port_init().
 */
uint32_t fm_port_enable(t_handle h_fm_port);

/*
 * @Function	  fm_port_set_rate_limit
 *
 * @Description   Calling this routine enables rate limit algorithm.
 *		  By default, this functionality is disabled.
 *
 *		  Note that rate - limit mechanism uses the FM time stamp.
 *		  The selected rate limit specified here would be
 *		  rounded DOWN to the nearest 16M.
 *
 *		  May be used for Tx and OP ports only
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  p_rate_limit	A structure of rate limit parameters
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init(). If rate limit is set
 *		  on a port that need to send PFC frames, it might violate the
 *		  stop transmit timing.
 */
uint32_t fm_port_set_rate_limit(t_handle h_fm_port,
				t_fm_port_rate_limit *p_rate_limit);

/*
 * @Function	  fm_port_delete_rate_limit
 *
 * @Description   Calling this routine disables and clears rate limit
 *		  initialization.
 *
 *		  May be used for Tx and OP ports only
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_delete_rate_limit(t_handle h_fm_port);

/*
 * @Function	  fm_port_set_pfc_priorities_mapping_to_qman_wq

 * @Description   Calling this routine maps each PFC received priority to the
 *		  transmit WQ. This WQ will be blocked upon receiving a PFC
 *		  frame with this priority.
 *
 *		  May be used for Tx ports only.
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  prio		PFC priority (0 - 7).
 * @Param[in]	  wq		Work Queue (0 - 7).
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_set_pfc_priorities_mapping_to_qman_wq(t_handle h_fm_port,
						uint8_t prio, uint8_t wq);

/*
 * @Function	  fm_port_set_statistics_counters
 *
 * @Description   Calling this routine enables/disables port's statistics
 *		  counters. By default, counters are enabled.
 *
 *		  May be used for all port types
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  enable	TRUE to enable, FALSE to disable.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_set_statistics_counters(t_handle h_fm_port, bool enable);

/*
 * @Function	  fm_port_set_frame_queue_counters
 *
 * @Description   Calling this routine enables/disables port's enqueue/dequeue
 *		  counters. By default, counters are enabled.
 *
 *		  May be used for all ports
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  enable	TRUE to enable, FALSE to disable.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_set_frame_queue_counters(t_handle h_fm_port,
						bool enable);

/*
 * @Function	  fm_port_analyze_performance_params
 *
 * @Description   User may call this routine to so the driver will analyze if
 *		  the basic performance parameters are correct and also the
 *		  driver may suggest of improvements; The basic parameters are
 *		  FIFO sizes, number of DMAs and number of TNUMs for the port.
 *
 *		  May be used for all port types
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_analyze_performance_params(t_handle h_fm_port);

/*
 * @Function	  fm_port_set_alloc_buf_counter
 *
 * @Description   Calling this routine enables/disables BM pool allocate
 *		  buffer counters.
 *		  By default, counters are enabled.
 *
 *		  May be used for Rx ports only
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  pool_id	BM pool id.
 * @Param[in]	  enable	TRUE to enable, FALSE to disable.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_set_alloc_buf_counter(t_handle h_fm_port,
						uint8_t pool_id, bool enable);

/*
 * @Function	fm_port_get_bmi_counters
 *
 * @Description   Read port's BMI stat counters and place them into
 *		  a designated structure of counters.
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[out]	  p_bmi_stats	counters structure
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_get_bmi_counters(t_handle h_fm_port,
					t_fm_port_bmi_stats *p_bmi_stats);

/*
 * @Function	  fm_port_get_counter
 *
 * @Description   Reads one of the FM PORT counters.
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  fm_port_counter	The requested counter.
 *
 * @Return	  Counter's current value.
 *
 * @Cautions	  Allowed only following fm_port_init().
 *		  Note that it is user's responsibility to call this routine
 *		  only for enabled counters, and there will be no indication if
 *		  a disabled counter is accessed.
 */
uint32_t fm_port_get_counter(t_handle h_fm_port,
		e_fm_port_counters fm_port_counter);

/*
 * @Function	  fm_port_modify_counter
 *
 * @Description   Sets a value to an enabled counter. Use "0" to reset the
 *		  counter.
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  fm_port_counter	The requested counter.
 * @Param[in]	  value			The requested value to be written into
 *					the counter.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_modify_counter(t_handle h_fm_port,
		e_fm_port_counters fm_port_counter, uint32_t value);

/*
 * @Function	  fm_port_get_alloc_buf_counter
 *
 * @Description   Reads one of the FM PORT buffer counters.
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  pool_id		The requested pool.
 *
 * @Return	  Counter's current value.
 *
 * @Cautions	  Allowed only following fm_port_init().
 *		  Note that it is user's responsibility to call this routine
 *		  only for enabled counters, and there will be no indication if
 *		  a disabled counter is accessed.
 */
uint32_t fm_port_get_alloc_buf_counter(t_handle h_fm_port,
			uint8_t pool_id);

/*
 * @Function	  fm_port_modify_alloc_buf_counter
 *
 * @Description   Sets a value to an enabled counter. Use "0" to reset the
 *		  counter.
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  pool_id	The requested pool.
 * @Param[in]	  value		The requested value to be written into the
 *				counter.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_modify_alloc_buf_counter(t_handle h_fm_port,
			uint8_t pool_id, uint32_t value);

/*
 * @Function	fm_port_add_congestion_grps
 *
 * @Description   This routine effects the corresponding Tx port.
 *		  It should be called in order to enable pause
 *		  frame transmission in case of congestion in one or more
 *		  of the congestion groups relevant to this port.
 *		  Each call to this routine may add one or more congestion
 *		  groups to be considered relevant to this port.
 *
 *		  May be used for Rx, or RX + OP ports only (depending on chip)
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  p_congestion_grps	A pointer to an array of congestion
 *					groups id's to consider.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_add_congestion_grps(t_handle h_fm_port,
			t_fm_port_congestion_grps *p_congestion_grps);

/*
 * @Function	  fm_port_remove_congestion_grps
 *
 * @Description   This routine effects the corresponding Tx port. It should be
 *		  called when congestion groups were defined for this port and
 *		  are no longer relevant, or pause frames transmitting is not
 *		  required on their behalf.
 *		  Each call to this routine may remove one or more congestion
 *		  groups to be considered relevant to this port.
 *
 *		  May be used for Rx, or RX + OP ports only (depending on chip)
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  p_congestion_grps	A pointer to an array of congestion
 *					groups id's to consider.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_remove_congestion_grps(t_handle h_fm_port,
			t_fm_port_congestion_grps *p_congestion_grps);

/*
 * @Function	  fm_port_is_stalled
 *
 * @Description   A routine for checking whether the specified port is stalled.
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 *
 * @Return	  TRUE if port is stalled, FALSE otherwise
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
bool fm_port_is_stalled(t_handle h_fm_port);

/*
 * @Function	fm_port_release_stalled
 *
 * @Description   This routine may be called in case the port was stalled and
 *		  may now be released.
 *		  Note that this routine is available only on older FMan
 *		  revisions (FMan v2, DPAA v1.0 only).
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_release_stalled(t_handle h_fm_port);

/*
 * @Function	  fm_port_set_rx_l4checksum_verify
 *
 * @Description   This routine is relevant for Rx ports (1G and 10G). The
 *		  routine set / clear the L3 / L4 checksum verification (on RX
 *		  side). Note that this takes affect only if hw - parser is
 *		  enabled !
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  l_4checksum	boolean indicates whether to do L3/L4 checksum
 *				on frames or not.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_set_rx_l4checksum_verify(t_handle h_fm_port,
			bool l_4checksum);

/*
 * @Function	  fm_port_set_errors_route
 *
 * @Description   Errors selected for this routine will cause a frame with that
 *		  error to be enqueued to error queue.
 *		  Errors not selected for this routine will cause a frame with
 *		  that error to be enqueued to the one of the other port queues.
 *		  By default all errors are defined to be enqueued to error
 *		  queue. Errors that were configured to be discarded(at
 *		  initialization) may not be selected here.
 *
 *		  May be used for Rx and OP ports only
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  errs		A list of errors to enqueue to error queue
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_config() and before
 *		  fm_port_init().
 */
uint32_t fm_port_set_errors_route(t_handle h_fm_port,
				fm_port_frame_err_select_t errs);

/*
 * @Function	  fm_port_set_imexceptions
 *
 * @Description   Calling this routine enables/disables FM PORT interrupts.
 *
 * @Param[in]	  h_fm_port	FM PORT module descriptor.
 * @Param[in]	  exception	The exception to be selected.
 * @Param[in]	  enable	TRUE to enable interrupt, FALSE to mask it.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 *		  This routine should NOT be called from guest-partition
 *		  (i.e. guestId != NCSW_PRIMARY_ID)
 */
uint32_t fm_port_set_imexceptions(t_handle h_fm_port,
				e_fm_port_exceptions exception, bool enable);

/*
 * @Function	  fm_port_set_performance_counters
 *
 * @Description   Calling this routine enables/disables port's performance
 *		  counters. By default, counters are enabled.
 *
 *		  May be used for all port types
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  enable		TRUE to enable, FALSE to disable.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	Allowed only following fm_port_init().
 */
uint32_t fm_port_set_performance_counters(t_handle h_fm_port,
						bool enable);

/*
 * @Function	  fm_port_set_performance_counters_params
 *
 * @Description   Calling this routine defines port's performance counters
 *		  parameters.
 *
 *		  May be used for all port types
 *
 * @Param[in]	  h_fm_port			A handle to a FM Port module.
 * @Param[in]	  p_fm_port_performance_cnt	A pointer to a structure of
 *						performance counters parameters.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_set_performance_counters_params(t_handle h_fm_port,
			t_fm_port_performance_cnt *p_fm_port_performance_cnt);

/*
 * @Group	  FM_PORT_pcd_runtime_control_grp
 *		  FM Port PCD Runtime Control Unit
 *
 * @Description   FM Port PCD Runtime control unit API functions, definitions
 *		  and enums.
 *
 * @Function	  fm_port_set_pcd
 *
 * @Description   Calling this routine defines the port's PCD configuration. It
 *		  changes it from its default configuration which is PCD
 *		  disabled (BMI to BMI) and configures it according to the
 *		  passed parameters.
 *
 *		  May be used for Rx and OP ports only
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  p_fm_port_pcd	A Structure of parameters defining the port's
 *				PCD configuration.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_set_pcd(t_handle h_fm_port,
			ioc_fm_port_pcd_params_t *p_fm_port_pcd);

/*
 * @Function	  fm_port_delete_pcd
 *
 * @Description   Calling this routine releases the port's PCD configuration.
 *		  The port returns to its default configuration which is PCD
 *		  disabled (BMI to BMI) and all PCD configuration is removed.
 *
 *		  May be used for Rx and OP ports which are in PCD mode only
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_delete_pcd(t_handle h_fm_port);

/*
 * @Function	  fm_port_attach_pcd
 *
 * @Description   This routine may be called after fm_port_detach_pcd was
 *		  called, to return to the originally configured PCD support
 *		  flow. The couple of routines are used to allow PCD
 *		  configuration changes that demand that PCD will not be used
 *		  while changes take place.
 *
 *		  May be used for Rx and OP ports which are in PCD mode only
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init().
 */
uint32_t fm_port_attach_pcd(t_handle h_fm_port);

/*
 * @Function	  fm_port_detach_pcd
 *
 * @Description   Calling this routine detaches the port from its PCD
 *		  functionality. The port returns to its default flow which is
 *		  BMI to BMI.
 *
 *		  May be used for Rx and OP ports which are in PCD mode only
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_attach_pcd().
 */
uint32_t fm_port_detach_pcd(t_handle h_fm_port);

/*
 * @Function	  fm_port_pcd_plcr_alloc_profiles
 *
 * @Description   This routine may be called only for ports that use the Policer
 *		  in order to allocate private policer profiles.
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  num_of_profiles	The number of required policer profiles
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init() and fm_pcd_init(), and
 *		  before fm_port_set_pcd().
 */
uint32_t fm_port_pcd_plcr_alloc_profiles(t_handle h_fm_port,
			uint16_t num_of_profiles);

/*
 * @Function	  fm_port_pcd_plcr_free_profiles
 *
 * @Description   This routine should be called for freeing private policer
 *		  profiles.
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init() and fm_pcd_init(), and
 *		  before fm_port_set_pcd().
 */
uint32_t fm_port_pcd_plcr_free_profiles(t_handle h_fm_port);

/*
 * @Function	  fm_port_pcd_kg_modify_initial_scheme
 *
 * @Description   This routine may be called only for ports that use the keygen
 *		  in order to change the initial scheme frame should be routed
 *		  to. The change may be of a scheme id(in case of direct mode),
 *		  from direct to indirect, or from indirect to direct -
 *		  specifying the scheme id.
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  p_fm_pcd_kg_scheme	A structure of parameters for defining
 *					whether a scheme is direct / indirect,
 *					and if direct - scheme id.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init() and fm_port_set_pcd().
 */
uint32_t fm_port_pcd_kg_modify_initial_scheme(t_handle h_fm_port,
		ioc_fm_pcd_kg_scheme_select_t *p_fm_pcd_kg_scheme);

/*
 * @Function	  fm_port_pcd_plcr_modify_initial_profile
 *
 * @Description   This routine may be called for ports with flows
 *		  e_FM_PORT_PCD_SUPPORT_PLCR_ONLY or
 *		  e_FM_PORT_PCD_SUPPORT_PRS_AND_PLCR only, to change the initial
 *		  Policer profile frame should be routed to. The change may be
 *		  of a profile and / or absolute / direct mode selection.
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  h_profile		Policer profile handle
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init() and fm_port_set_pcd().
 */
uint32_t fm_port_pcd_plcr_modify_initial_profile(t_handle h_fm_port,
						t_handle h_profile);

/*
 * @Function	  fm_port_pcd_cc_modify_tree
 *
 * @Description   This routine may be called for ports that use coarse
 *		  classification tree if the user wishes to replace the tree.
 *		  The routine may not be called while port receives packets
 *		  using the PCD functionalities, therefore port must be first
 *		  detached from the PCD, only than the routine may be called,
 *		  and than port be attached to PCD again.
 *
 * @Param[in]	  h_fm_port	A handle to a FM Port module.
 * @Param[in]	  h_cc_tree	A CC tree that was already built. The tree id as
 *				returned from the BuildTree routine.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init(), fm_port_set_pcd() and
 *		  fm_port_detach_pcd()
 */
uint32_t fm_port_pcd_cc_modify_tree(t_handle h_fm_port, t_handle h_cc_tree);

/*
 * @Function	  fm_port_pcd_kg_bind_schemes
 *
 * @Description   These routines may be called for adding more schemes for the
 *		  port to be bound to. The selected schemes are not added, just
 *		  this specific port starts using them.
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  p_port_scheme		A structure defining the list of schemes
 *					to be added.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init() and fm_port_set_pcd().
 */
uint32_t fm_port_pcd_kg_bind_schemes(t_handle h_fm_port,
			ioc_fm_pcd_port_schemes_params_t *p_port_scheme);

/*
 * @Function	  fm_port_pcd_kg_unbind_schemes
 *
 * @Description   These routines may be called for adding more schemes for the
 *		  port to be bound to. The selected schemes are not removed or
 *		  invalidated, just this specific port stops using them.
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[in]	  p_port_scheme		A structure defining the list of schemes
 *					to be added.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init() and fm_port_set_pcd().
 */
uint32_t fm_port_pcd_kg_unbind_schemes(t_handle h_fm_port,
			ioc_fm_pcd_port_schemes_params_t *p_port_scheme);

/*
 * @Function	  fm_port_get_ipv_4options_count
 *
 * @Description   TODO
 *
 * @Param[in]	  h_fm_port		A handle to a FM Port module.
 * @Param[out]	  p_ipv_4options_count  will hold the counter value
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_port_init()
 */
uint32_t fm_port_get_ipv_4options_count(t_handle h_fm_port,
				uint32_t *p_ipv_4options_count);

/** @} */ /* end of FM_PORT_pcd_runtime_control_grp group */
/** @} */ /* end of FM_PORT_runtime_control_grp group */
/** @} */ /* end of FM_PORT_grp group */
/** @} */ /* end of FM_grp group */
#endif /* __FM_PORT_EXT_H */
