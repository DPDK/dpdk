/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2012 Freescale Semiconductor Inc.
 * Copyright 2017-2020 NXP
 */

#ifndef __FM_PCD_EXT_H
#define __FM_PCD_EXT_H

#include "ncsw_ext.h"
#include "net_ext.h"
#include "fm_ext.h"

/*
 * @Description	  FM PCD ...
 * @Group	  lnx_ioctl_FM_grp Frame Manager Linux IOCTL API
 * @Description	  Frame Manager Linux ioctls definitions and enums
 * @{
 */

/*
 * @Group	  lnx_ioctl_FM_PCD_grp FM PCD
 * @Description	  Frame Manager PCD API functions, definitions and enums
 *
 *		  The FM PCD module is responsible for the initialization of all
 *		  global classifying FM modules. This includes the parser
 *		  general and common registers, the key generator global and
 *		  common registers, and the policer global and common registers.
 *		  In addition, the FM PCD SW module will initialize all required
 *		  key generator schemes, coarse classification flows, and
 *		  policer profiles. When an FM module is configured to work with
 *		  one of these entities, it will register to it using the FM
 *		  PORT API. The PCD module will manage the PCD resources - i.e.
 *		  resource management of KeyGen schemes, etc.
 *
 * @{
 */

/*
 * @Collection	General PCD defines
 */
#define IOC_FM_PCD_MAX_NUM_OF_PRIVATE_HDRS		2
/**< Number of units/headers saved for user */

#define IOC_FM_PCD_PRS_NUM_OF_HDRS			16
/**< Number of headers supported by HW parser */
#define IOC_FM_PCD_MAX_NUM_OF_DISTINCTION_UNITS \
	(32 - IOC_FM_PCD_MAX_NUM_OF_PRIVATE_HDRS)
/**< Number of distinction units is limited by register size (32 bits) minus
 * reserved bits for private headers.
 */
#define IOC_FM_PCD_MAX_NUM_OF_INTERCHANGEABLE_HDRS	4
/**< Maximum number of interchangeable headers in a distinction unit */
#define IOC_FM_PCD_KG_NUM_OF_GENERIC_REGS		8
/**< Total number of generic KeyGen registers */
#define IOC_FM_PCD_KG_MAX_EXTRACTS_PER_KEY	35
/**< Max number allowed on any configuration; For HW implementation reasons,
 * in most cases less than this will be allowed; The driver will return an
 * initialization error if resource is unavailable.
 */
#define IOC_FM_PCD_KG_NUM_OF_EXTRACT_MASKS		4
/**< Total number of masks allowed on KeyGen extractions. */
#define IOC_FM_PCD_KG_NUM_OF_DEFAULT_GROUPS		16
/**< Number of default value logical groups */
#define IOC_FM_PCD_PRS_NUM_OF_LABELS			32
/**< Maximum number of SW parser labels */
#define IOC_FM_PCD_SW_PRS_SIZE			0x00000800
/**< Total size of SW parser area */

#define IOC_FM_PCD_MAX_MANIP_INSRT_TEMPLATE_SIZE	128
/**< Maximum size of insertion template for insert manipulation */

#define IOC_FM_PCD_FRM_REPLIC_MAX_NUM_OF_ENTRIES	64
/**< Maximum possible entries for frame replicator group */
/* @} */

/*
 * @Group	  lnx_ioctl_FM_PCD_init_grp FM PCD Initialization Unit
 *
 * @Description   Frame Manager PCD Initialization Unit API
 *
 * @{
 */

/*
 * @Description   PCD counters
 *		  (must match enum ioc_fm_pcd_counters defined in fm_pcd_ext.h)
 */
typedef enum ioc_fm_pcd_counters {
	e_IOC_FM_PCD_KG_COUNTERS_TOTAL,		/**< KeyGen counter */
	e_IOC_FM_PCD_PLCR_COUNTERS_RED,
	/**< Policer counter - counts the total number of RED packets that exit
	 * the Policer.
	 */
	e_IOC_FM_PCD_PLCR_COUNTERS_YELLOW,
	/**< Policer counter - counts the total number of YELLOW packets that
	 * exit the Policer.
	 */
	e_IOC_FM_PCD_PLCR_COUNTERS_RECOLORED_TO_RED,
	/**< Policer counter - counts the number of packets that changed color
	 * to RED by the Policer; This is a subset of
	 * e_IOC_FM_PCD_PLCR_COUNTERS_RED packet count, indicating active color
	 * changes.
	 */
	e_IOC_FM_PCD_PLCR_COUNTERS_RECOLORED_TO_YELLOW,
	/**< Policer counter - counts the number of packets that changed color
	 * to YELLOW by the Policer; This is a subset of
	 * e_IOC_FM_PCD_PLCR_COUNTERS_YELLOW packet count, indicating active
	 * color changes.
	 */
	e_IOC_FM_PCD_PLCR_COUNTERS_TOTAL,
	/**< Policer counter - counts the total number of packets passed in the
	 * Policer.
	 */
	e_IOC_FM_PCD_PLCR_COUNTERS_LENGTH_MISMATCH,
	/**< Policer counter - counts the number of packets with length
	 * mismatch.
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_PARSE_DISPATCH,
	/**< Parser counter - counts the number of times the parser block is
	 * dispatched.
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_L2_PARSE_RESULT_RETURNED,
	/**< Parser counter - counts the number of times L2 parse result is
	 * returned (including errors).
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_L3_PARSE_RESULT_RETURNED,
	/**< Parser counter - counts the number of times L3 parse result is
	 * returned (including errors).
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_L4_PARSE_RESULT_RETURNED,
	/**< Parser counter - counts the number of times L4 parse result is
	 * returned (including errors).
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_SHIM_PARSE_RESULT_RETURNED,
	/**< Parser counter - counts the number of times SHIM parse result is
	 * returned (including errors).
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_L2_PARSE_RESULT_RETURNED_WITH_ERR,
	/**< Parser counter - counts the number of times L2 parse result is
	 * returned with errors.
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_L3_PARSE_RESULT_RETURNED_WITH_ERR,
	/**< Parser counter - counts the number of times L3 parse result is
	 * returned with errors.
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_L4_PARSE_RESULT_RETURNED_WITH_ERR,
	/**< Parser counter - counts the number of times L4 parse result is
	 * returned with errors.
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_SHIM_PARSE_RESULT_RETURNED_WITH_ERR,
	/**< Parser counter - counts the number of times SHIM parse result is
	 * returned with errors.
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_SOFT_PRS_CYCLES,
	/**< Parser counter - counts the number of cycles spent executing soft
	 * parser instruction (including stall cycles).
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_SOFT_PRS_STALL_CYCLES,
	/**< Parser counter - counts the number of cycles stalled waiting for
	 * parser internal memory reads while executing soft parser instruction.
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_HARD_PRS_CYCLE_INCL_STALL_CYCLES,
	/**< Parser counter - counts the number of cycles spent executing hard
	 * parser (including stall cycles).
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_MURAM_READ_CYCLES,
	/**< MURAM counter - counts the number of cycles while performing FMan
	 * Memory read.
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_MURAM_READ_STALL_CYCLES,
	/**< MURAM counter - counts the number of cycles stalled while
	 * performing FMan Memory read.
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_MURAM_WRITE_CYCLES,
	/**< MURAM counter - counts the number of cycles while performing FMan
	 * Memory write.
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_MURAM_WRITE_STALL_CYCLES,
	/**< MURAM counter - counts the number of cycles stalled while
	 * performing FMan Memory write.
	 */
	e_IOC_FM_PCD_PRS_COUNTERS_FPM_COMMAND_STALL_CYCLES
	/**< FPM counter - counts the number of cycles stalled while performing
	 * a FPM Command.
	 */
} ioc_fm_pcd_counters;

/*
 * @Description   PCD interrupts
 *		  (must match enum ioc_fm_pcd_exceptions defined in
 *		  fm_pcd_ext.h)
 */
typedef enum ioc_fm_pcd_exceptions {
	e_IOC_FM_PCD_KG_EXCEPTION_DOUBLE_ECC,
	/**< KeyGen double-bit ECC error is detected on internal memory read
	 * access.
	 */
	e_IOC_FM_PCD_KG_EXCEPTION_KEYSIZE_OVERFLOW,
	/**< KeyGen scheme configuration error indicating a key size larger than
	 * 56 bytes.
	 */
	e_IOC_FM_PCD_PLCR_EXCEPTION_DOUBLE_ECC,
	/**< Policer double-bit ECC error has been detected on PRAM read access.
	 */
	e_IOC_FM_PCD_PLCR_EXCEPTION_INIT_ENTRY_ERROR,
	/**< Policer access to a non-initialized profile has been detected. */
	e_IOC_FM_PCD_PLCR_EXCEPTION_PRAM_SELF_INIT_COMPLETE,
	/**< Policer RAM self-initialization complete */
	e_IOC_FM_PCD_PLCR_EXCEPTION_ATOMIC_ACTION_COMPLETE,
	/**< Policer atomic action complete */
	e_IOC_FM_PCD_PRS_EXCEPTION_DOUBLE_ECC,
	/**< Parser double-bit ECC error */
	e_IOC_FM_PCD_PRS_EXCEPTION_SINGLE_ECC
	/**< Parser single-bit ECC error */
} ioc_fm_pcd_exceptions;

/** @} */ /* end of lnx_ioctl_FM_PCD_init_grp group */

/*
 * @Group	  lnx_ioctl_FM_PCD_Runtime_grp FM PCD Runtime Unit
 *
 * @Description   Frame Manager PCD Runtime Unit
 *
 *		  The runtime control allows creation of PCD infrastructure
 *		  modules such as Network Environment Characteristics,
 *		  Classification Plan Groups and Coarse Classification Trees.
 *		  It also allows on-the-fly initialization, modification and
 *		  removal of PCD modules such as KeyGen schemes, coarse
 *		  classification nodes and Policer profiles.
 *
 *		  In order to explain the programming model of the PCD driver
 *		  interface a few terms should be explained, and will be used
 *		  below.
 *		  - Distinction Header - One of the 16 protocols supported by
 *		    the FM parser, or one of the SHIM headers (1 or 2). May be a
 *		    header with a special option (see below).
 *		  - Interchangeable Headers Group - This is a group of Headers
 *		    recognized by either one of them. For example, if in a
 *		    specific context the user chooses to treat IPv4 and IPV6 in
 *		    the same way, they may create an interchangeable Headers
 *		    Unit consisting of these 2 headers.
 *		  - A Distinction Unit - a Distinction Header or an
 *		    Interchangeable Headers Group.
 *		  - Header with special option - applies to Ethernet, MPLS,
 *		    VLAN, IPv4 and IPv6, includes multicast, broadcast and other
 *		    protocol specific options. In terms of hardware it relates
 *		    to the options available in the classification plan.
 *		  - Network Environment Characteristics - a set of Distinction
 *		    Units that define the total recognizable header selection
 *		    for a certain environment. This is NOT the list of all
 *		    headers that will ever appear in a flow, but rather
 *		    everything that needs distinction in a flow, where
 *		    distinction is made by KeyGen schemes and coarse
 *		    classification action descriptors.
 *
 *		  The PCD runtime modules initialization is done in stages. The
 *		  first stage after initializing the PCD module itself is to
 *		  establish a Network Flows Environment Definition. The
 *		  application may choose to establish one or more such
 *		  environments. Later, when needed, the application will have to
 *		  state, for some of its modules, to which single environment it
 *		  belongs.
 *
 * @{
 */

/*
 * @Description   structure for FM counters
 */
typedef struct ioc_fm_pcd_counters_params_t {
	ioc_fm_pcd_counters cnt;	/**< The requested counter */
	uint32_t	val;
			/**< The requested value to get/set from/into the
			 * counter
			 */
} ioc_fm_pcd_counters_params_t;

/*
 * @Description   structure for FM exception definitios
 */
typedef struct ioc_fm_pcd_exception_params_t {
	ioc_fm_pcd_exceptions exception;	/**< The requested exception */
	bool		enable;
			/**< TRUE to enable interrupt, FALSE to mask it. */
} ioc_fm_pcd_exception_params_t;

/*
 * @Description   A structure for SW parser labels (must be identical to struct
 *		  t_fm_pcd_prs_label_params defined in fm_pcd_ext.h)
 */
typedef struct ioc_fm_pcd_prs_label_params_t {
	uint32_t instruction_offset;
		/**< SW parser label instruction offset (2 bytes resolution),
		 * relative to Parser RAM
		 */
	ioc_net_header_type	hdr;
		/**< The existence of this header will invoke the SW parser
		 * code.
		 */
	uint8_t	index_per_hdr;
		/**< Normally 0, if more than one SW parser attachments for the
		 * same header, use this index to distinguish between them.
		 */
} ioc_fm_pcd_prs_label_params_t;

/*
 * @Description   A structure for SW parser (Must match struct
 *		  ioc_fm_pcd_prs_sw_params_t defined in fm_pcd_ext.h)
 */
typedef struct ioc_fm_pcd_prs_sw_params_t {
	bool		override;
			/**< FALSE to invoke a check that nothing else was
			 * loaded to this address, including internal patches.
			 * TRUE to override any existing code.
			 */
	uint32_t	size;		/**< SW parser code size */
	uint16_t	base;
			/**< SW parser base (in instruction counts! must be
			 * larger than 0x20)
			 */
	uint8_t		*p_code;	/**< SW parser code */
	uint32_t	sw_prs_data_params[IOC_FM_PCD_PRS_NUM_OF_HDRS];
					/**< SW parser data (parameters) */
	uint8_t		num_of_labels;	/**< Number of labels for SW parser. */
	ioc_fm_pcd_prs_label_params_t
			labels_table[IOC_FM_PCD_PRS_NUM_OF_LABELS];
			/**< SW parser labels table, containing num_of_labels
			 * entries
			 */
} ioc_fm_pcd_prs_sw_params_t;

/*
 * @Description   A structure to set the a KeyGen default value
 */
typedef struct ioc_fm_pcd_kg_dflt_value_params_t {
	uint8_t		value_id;/**< 0,1 - one of 2 global default values */
	uint32_t	value;	/**< The requested default value */
} ioc_fm_pcd_kg_dflt_value_params_t;

/*
 * @Function	  fm_pcd_enable
 *
 * @Description   This routine should be called after PCD is initialized for
 *		  enabling all PCD engines according to their existing
 *		  configuration.
 *
 * @Return	  0 on success; Error code otherwise.
 *
 * @Cautions	  Allowed only when PCD is disabled.
 */
#define FM_PCD_IOC_ENABLE  _IO(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(1))

/*
 * @Function	  fm_pcd_disable
 *
 * @Description   This routine may be called when PCD is enabled in order to
 *		  disable all PCD engines. It may be called only when none of
 *		  the ports in the system are using the PCD.
 *
 * @Return	  0 on success; Error code otherwise.
 *
 * @Cautions	  Allowed only when PCD is enabled.
 */
#define FM_PCD_IOC_DISABLE  _IO(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(2))

/*
 * @Function	  fm_pcd_prs_load_sw
 *
 * @Description   This routine may be called only when all ports in the
 *		  system are actively using the classification plan scheme.
 *		  In such cases it is recommended in order to save resources.
 *		  The driver automatically saves 8 classification plans for
 *		  ports that do NOT use the classification plan mechanism, to
 *		  avoid this (in order to save those entries) this routine may
 *		  be called.
 *
 * @Param[in]	  ioc_fm_pcd_prs_sw_params_t
 *		  A pointer to the image of the software parser code.
 *
 * @Return	  0 on success; Error code otherwise.
 *
 * @Cautions	  Allowed only when PCD is disabled.
 */
#define FM_PCD_IOC_PRS_LOAD_SW \
	_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(3), ioc_fm_pcd_prs_sw_params_t)

/*
 * @Function	  fm_pcd_kg_set_dflt_value
 *
 * @Description   Calling this routine sets a global default value to be used
 *		  by the KeyGen when parser does not recognize a required
 *		  field/header.
 *		  default value is 0.
 *
 * @Param[in]	  ioc_fm_pcd_kg_dflt_value_params_t	A pointer to a structure
 *							with the relevant
 *							parameters
 *
 * @Return	  0 on success; Error code otherwise.
 *
 * @Cautions	  Allowed only when PCD is disabled.
 */
#define FM_PCD_IOC_KG_SET_DFLT_VALUE \
	_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(6), \
	     ioc_fm_pcd_kg_dflt_value_params_t)

/*
 * @Function	  fm_pcd_kg_set_additional_data_after_parsing
 *
 * @Description   Calling this routine allows the keygen to access data past
 *		  the parser finishing point.
 *
 * @Param[in]	  uint8_t	payload-offset; the number of bytes beyond the
 *				parser location.
 *
 * @Return	  0 on success; Error code otherwise.
 *
 * @Cautions	  Allowed only when PCD is disabled.
 */
#define FM_PCD_IOC_KG_SET_ADDITIONAL_DATA_AFTER_PARSING \
	_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(7), uint8_t)

/*
 * @Function	  fm_pcd_set_exception
 *
 * @Description   Calling this routine enables/disables PCD interrupts.
 *
 * @Param[in]	  ioc_fm_pcd_exception_params_t
 *		  Arguments struct with exception to be enabled/disabled.
 *
 * @Return	  0 on success; Error code otherwise.
 */
#define FM_PCD_IOC_SET_EXCEPTION \
	_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(8), ioc_fm_pcd_exception_params_t)

/*
 * @Function	  fm_pcd_get_counter
 *
 * @Description   Reads one of the FM PCD counters.
 *
 * @Param[in,out] ioc_fm_pcd_counters_params_t The requested counter parameters.
 *
 * @Return	  0 on success; Error code otherwise.
 *
 * @Cautions	  It is user's responsibility to call this routine only
 *		  for enabled counters, and there will be no indication if a
 *		  disabled counter is accessed.
 */
#define FM_PCD_IOC_GET_COUNTER \
	_IOWR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(9), ioc_fm_pcd_counters_params_t)

/*
 * @Function	  fm_pcd_kg_scheme_get_counter
 *
 * @Description   Reads scheme packet counter.
 *
 * @Param[in]	  h_scheme	scheme handle as returned by
 *				fm_pcd_kg_scheme_set().
 *
 * @Return	  Counter's current value.
 *
 * @Cautions	  Allowed only following fm_pcd_init() & fm_pcd_kg_scheme_set().
 */
#define FM_PCD_IOC_KG_SCHEME_GET_CNTR \
	_IOR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(4), ioc_fm_pcd_kg_scheme_spc_t)

/*
 * @Function	  FM_PCD_ForceIntr
 *
 * @Description   Causes an interrupt event on the requested source.
 *
 * @Param[in]	  ioc_fm_pcd_exceptions - An exception to be forced.
 *
 * @Return	  0 on success; error code if the exception is not enabled,
 *		  or is not able to create interrupt.
 */
#define FM_PCD_IOC_FORCE_INTR \
	_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(11), ioc_fm_pcd_exceptions)

/*
 * @Collection	Definitions of coarse classification parameters as required by
 *		KeyGen (when coarse classification is the next engine after this
 *		scheme).
 */
#define IOC_FM_PCD_MAX_NUM_OF_CC_TREES		8
#define IOC_FM_PCD_MAX_NUM_OF_CC_GROUPS		16
#define IOC_FM_PCD_MAX_NUM_OF_CC_UNITS		4
#define IOC_FM_PCD_MAX_NUM_OF_KEYS		256
#define IOC_FM_PCD_MAX_NUM_OF_FLOWS		(4 * KILOBYTE)
#define IOC_FM_PCD_MAX_SIZE_OF_KEY		56
#define IOC_FM_PCD_MAX_CC_ENTRY_IN_GRP		16
#define IOC_FM_PCD_LAST_KEY_INDEX		0xffff
#define IOC_FM_PCD_MANIP_DSCP_VALUES		64
/* @} */

/*
 * @Collection	A set of definitions to allow protocol
 *		special option description.
 */
typedef uint32_t		ioc_protocol_opt_t;
		/**< A general type to define a protocol option. */

typedef ioc_protocol_opt_t  ioc_eth_protocol_opt_t;
			/**< Ethernet protocol options. */
#define IOC_ETH_BROADCAST		0x80000000   /**< Ethernet Broadcast. */
#define IOC_ETH_MULTICAST		0x40000000   /**< Ethernet Multicast. */

typedef ioc_protocol_opt_t  ioc_vlan_protocol_opt_t;
				/**< Vlan protocol options. */
#define IOC_VLAN_STACKED		0x20000000   /**< Stacked VLAN. */

typedef ioc_protocol_opt_t  ioc_mpls_protocol_opt_t;
				/**< MPLS protocol options. */
#define IOC_MPLS_STACKED		0x10000000   /**< Stacked MPLS. */

typedef ioc_protocol_opt_t  ioc_ipv4_protocol_opt_t;
			/**< IPv4 protocol options. */
#define IOC_IPV4_BROADCAST_1		0x08000000   /**< IPv4 Broadcast. */
#define IOC_IPV4_MULTICAST_1		0x04000000   /**< IPv4 Multicast. */
#define IOC_IPV4_UNICAST_2		0x02000000
					/**< Tunneled IPv4 - Unicast.
					 */
#define IOC_IPV4_MULTICAST_BROADCAST_2  0x01000000
					/**< Tunneled IPv4 -
					 * Broadcast/Multicast.
					 */

#define IOC_IPV4_FRAG_1		0x00000008
				/**< IPV4 reassembly option. IPV4
				 * Reassembly manipulation requires network
				 * environment with IPV4 header and IPV4_FRAG_1
				 * option
				 */

typedef ioc_protocol_opt_t  ioc_ipv6_protocol_opt_t;
					/**< IPv6 protocol options. */
#define IOC_IPV6_MULTICAST_1		0x00800000   /**< IPv6 Multicast. */
#define IOC_IPV6_UNICAST_2		0x00400000
					/**< Tunneled IPv6 - Unicast. */
#define IOC_IPV6_MULTICAST_2		0x00200000
					/**< Tunneled IPv6 - Multicast. */

#define IOC_IPV6_FRAG_1		0x00000004
				/**< IPV6 reassembly option. IPV6 Reassembly
				 * manipulation requires network environment
				 * with IPV6 header and IPV6_FRAG_1 option
				 */
typedef ioc_protocol_opt_t   ioc_capwap_protocol_opt_t;
					/**< CAPWAP protocol options. */
#define CAPWAP_FRAG_1		0x00000008
				/**< CAPWAP reassembly option. CAPWAP Reassembly
				 * manipulation requires network environment
				 * with CAPWAP header and CAPWAP_FRAG_1 option;
				 * in case where fragment found, the
				 * fragment-extension offset may be found at
				 * 'shim2' (in parser-result).
				 */

/* @} */

#define IOC_FM_PCD_MANIP_MAX_HDR_SIZE		256
#define IOC_FM_PCD_MANIP_DSCP_TO_VLAN_TRANS	64
/**
 * @Collection	A set of definitions to support Header Manipulation selection.
 */
typedef uint32_t			ioc_hdr_manip_flags_t;
	/**< A general type to define a HMan update command flags. */

typedef ioc_hdr_manip_flags_t	ioc_ipv4_hdr_manip_update_flags_t;
	/**< IPv4 protocol HMan update command flags. */

#define IOC_HDR_MANIP_IPV4_TOS	0x80000000
			/**< update TOS with the given value ('tos' field of
			 * ioc_fm_pcd_manip_hdr_field_update_ipv4_t)
			 */
#define IOC_HDR_MANIP_IPV4_ID	0x40000000
			/**< update IP ID with the given value ('id' field of
			 * ioc_fm_pcd_manip_hdr_field_update_ipv4_t)
			 */
#define IOC_HDR_MANIP_IPV4_TTL	0x20000000	/**< Decrement TTL by 1 */
#define IOC_HDR_MANIP_IPV4_SRC	0x10000000
		/**< update IP source address with the given value ('src' field
		 * of ioc_fm_pcd_manip_hdr_field_update_ipv4_t)
		 */
#define IOC_HDR_MANIP_IPV4_DST	0x08000000
		/**< update IP destination address with the given value
		 * ('dst' field of ioc_fm_pcd_manip_hdr_field_update_ipv4_t)
		 */

typedef ioc_hdr_manip_flags_t	ioc_ipv6_hdr_manip_update_flags_t;
			/**< IPv6 protocol HMan update command flags. */

#define IOC_HDR_MANIP_IPV6_TC	0x80000000
	/**< update Traffic Class address with the given value ('traffic_class'
	 * field of ioc_fm_pcd_manip_hdr_field_update_ipv6_t)
	 */
#define IOC_HDR_MANIP_IPV6_HL	0x40000000	/**< Decrement Hop Limit by 1 */
#define IOC_HDR_MANIP_IPV6_SRC	0x20000000
		/**< update IP source address with the given value ('src' field
		 * of ioc_fm_pcd_manip_hdr_field_update_ipv6_t)
		 */
#define IOC_HDR_MANIP_IPV6_DST	0x10000000
		/**< update IP destination address with the given value ('dst'
		 * field of ioc_fm_pcd_manip_hdr_field_update_ipv6_t)
		 */

typedef ioc_hdr_manip_flags_t	ioc_tcp_udp_hdr_manip_update_flags_t;
		/**< TCP/UDP protocol HMan update command flags. */

#define IOC_HDR_MANIP_TCP_UDP_SRC	0x80000000
		/**< update TCP/UDP source address with the given value
		 * ('src' field of ioc_fm_pcd_manip_hdr_field_update_tcp_udp_t)
		 */
#define IOC_HDR_MANIP_TCP_UDP_DST	0x40000000
		/**< update TCP/UDP destination address with the given value
		 * ('dst' field of ioc_fm_pcd_manip_hdr_field_update_tcp_udp_t)
		 */
#define IOC_HDR_MANIP_TCP_UDP_CHECKSUM  0x20000000
		/**< update TCP/UDP checksum */

/* @} */

/*
 * @Description   A type used for returning the order of the key extraction.
 *		  each value in this array represents the index of the
 *		  extraction command as defined by the user in the
 *		  initialization extraction array. The valid size of this array
 *		  is the user define number of extractions required (also
 *		  marked by the second '0' in this array).
 */
typedef	uint8_t
	ioc_fm_pcd_kg_key_order_t [IOC_FM_PCD_KG_MAX_EXTRACTS_PER_KEY];

/*
 *@Description   All PCD engines
 *		(must match enum e_FmPcdEngine defined in fm_pcd_ext.h)
 */

typedef enum ioc_fm_pcd_engine {
	e_IOC_FM_PCD_INVALID = 0,   /**< Invalid PCD engine */
	e_IOC_FM_PCD_DONE,	/**< No PCD Engine indicated */
	e_IOC_FM_PCD_KG,		/**< KeyGen */
	e_IOC_FM_PCD_CC,		/**< Coarse Classifier */
	e_IOC_FM_PCD_PLCR,	/**< Policer */
	e_IOC_FM_PCD_PRS,	/**< Parser */
	e_IOC_FM_PCD_FR,	/**< Frame Replicator */
	e_IOC_FM_PCD_HASH	/**< Hash Table */
} ioc_fm_pcd_engine;

/*
 * @Description   An enum for selecting extraction by header types
 *		  (Must match enum e_FmPcdExtractByHdrType defined in
 *		  fm_pcd_ext.h)
 */
typedef enum ioc_fm_pcd_extract_by_hdr_type {
	e_IOC_FM_PCD_EXTRACT_FROM_HDR,	/**< Extract bytes from header */
	e_IOC_FM_PCD_EXTRACT_FROM_FIELD,/**< Extract bytes from header field */
	e_IOC_FM_PCD_EXTRACT_FULL_FIELD	/**< Extract a full field */
} ioc_fm_pcd_extract_by_hdr_type;

/*
 * @Description   An enum for selecting extraction source (when it is not the
 *		  header) (Must match enum e_FmPcdExtractFrom defined in
 *		  fm_pcd_ext.h)
 */
typedef enum ioc_fm_pcd_extract_from {
	e_IOC_FM_PCD_EXTRACT_FROM_FRAME_START,
			/**< KG & CC: Extract from beginning of frame */
	e_IOC_FM_PCD_EXTRACT_FROM_DFLT_VALUE,
				/**< KG only: Extract from a default value */
	e_IOC_FM_PCD_EXTRACT_FROM_CURR_END_OF_PARSE,
			/**< KG only: Extract from the point where parsing had
			 * finished
			 */
	e_IOC_FM_PCD_EXTRACT_FROM_KEY,	/**< CC only: Field where saved KEY */
	e_IOC_FM_PCD_EXTRACT_FROM_HASH,	/**< CC only: Field where saved HASH */
	e_IOC_FM_PCD_EXTRACT_FROM_PARSE_RESULT,
				/**< KG & CC: Extract from the parser result */
	e_IOC_FM_PCD_EXTRACT_FROM_ENQ_FQID,
				/**< KG & CC: Extract from enqueue FQID */
	e_IOC_FM_PCD_EXTRACT_FROM_FLOW_ID
				/**< CC only: Field where saved Dequeue FQID */
} ioc_fm_pcd_extract_from;

/*
 * @Description   An enum for selecting extraction type
 */
typedef enum ioc_fm_pcd_extract_type {
	e_IOC_FM_PCD_EXTRACT_BY_HDR,	/**< Extract according to header */
	e_IOC_FM_PCD_EXTRACT_NON_HDR,
		/**< Extract from data that is not the header */
	e_IOC_FM_PCD_KG_EXTRACT_PORT_PRIVATE_INFO
			/**< Extract private info as specified by user */
} ioc_fm_pcd_extract_type;

/*
 * @Description   An enum for selecting a default
 */
typedef enum ioc_fm_pcd_kg_extract_dflt_select {
	e_IOC_FM_PCD_KG_DFLT_GBL_0,
		/**< Default selection is KG register 0 */
	e_IOC_FM_PCD_KG_DFLT_GBL_1,
		/**< Default selection is KG register 1 */
	e_IOC_FM_PCD_KG_DFLT_PRIVATE_0,
		/**< Default selection is a per scheme register 0 */
	e_IOC_FM_PCD_KG_DFLT_PRIVATE_1,
		/**< Default selection is a per scheme register 1 */
	e_IOC_FM_PCD_KG_DFLT_ILLEGAL	/**< Illegal selection */
} ioc_fm_pcd_kg_extract_dflt_select;

/*
 * @Description   Enumeration type defining all default groups - each group
 *		  shares a default value, one of four user-initialized values.
 */
typedef enum ioc_fm_pcd_kg_known_fields_dflt_types {
	e_IOC_FM_PCD_KG_MAC_ADDR,		/**< MAC Address */
	e_IOC_FM_PCD_KG_TCI,			/**< TCI field */
	e_IOC_FM_PCD_KG_ENET_TYPE,		/**< ENET Type */
	e_IOC_FM_PCD_KG_PPP_SESSION_ID,		/**< PPP Session id */
	e_IOC_FM_PCD_KG_PPP_PROTOCOL_ID,	/**< PPP Protocol id */
	e_IOC_FM_PCD_KG_MPLS_LABEL,		/**< MPLS label */
	e_IOC_FM_PCD_KG_IP_ADDR,		/**< IP addr */
	e_IOC_FM_PCD_KG_PROTOCOL_TYPE,		/**< Protocol type */
	e_IOC_FM_PCD_KG_IP_TOS_TC,		/**< TOS or TC */
	e_IOC_FM_PCD_KG_IPV6_FLOW_LABEL,	/**< IPV6 flow label */
	e_IOC_FM_PCD_KG_IPSEC_SPI,		/**< IPSEC SPI */
	e_IOC_FM_PCD_KG_L4_PORT,		/**< L4 Port */
	e_IOC_FM_PCD_KG_TCP_FLAG,		/**< TCP Flag */
	e_IOC_FM_PCD_KG_GENERIC_FROM_DATA,
		/**< grouping implemented by SW, any data extraction that is not
		 * the full field described above
		 */
	e_IOC_FM_PCD_KG_GENERIC_FROM_DATA_NO_V,
		/**< grouping implemented by SW, any data extraction without
		 * validation
		 */
	e_IOC_FM_PCD_KG_GENERIC_NOT_FROM_DATA
		/**< grouping implemented by SW, extraction from parser result
		 * or direct use of default value
		 */
} ioc_fm_pcd_kg_known_fields_dflt_types;

/*
 * @Description   Enumeration type for defining header index for scenarios with
 *		  multiple (tunneled) headers
 */
typedef enum ioc_fm_pcd_hdr_index {
	e_IOC_FM_PCD_HDR_INDEX_NONE	=   0,
				/**< used when multiple headers not used, also
				 * to specify regular IP (not tunneled).
				 */
	e_IOC_FM_PCD_HDR_INDEX_1,/**< may be used for VLAN, MPLS, tunneled IP */
	e_IOC_FM_PCD_HDR_INDEX_2,/**< may be used for MPLS, tunneled IP */
	e_IOC_FM_PCD_HDR_INDEX_3,/**< may be used for MPLS */
	e_IOC_FM_PCD_HDR_INDEX_LAST =   0xFF /**< may be used for VLAN, MPLS */
} ioc_fm_pcd_hdr_index;

/*
 * @Description   Enumeration type for selecting the policer profile functional
 *		  type
 */
typedef enum ioc_fm_pcd_profile_type_selection {
	e_IOC_FM_PCD_PLCR_PORT_PRIVATE,		/**< Port dedicated profile */
	e_IOC_FM_PCD_PLCR_SHARED
			/**< Shared profile (shared within partition) */
} ioc_fm_pcd_profile_type_selection;

/*
 * @Description   Enumeration type for selecting the policer profile algorithm
 */
typedef enum ioc_fm_pcd_plcr_algorithm_selection {
	e_IOC_FM_PCD_PLCR_PASS_THROUGH, /**< Policer pass through */
	e_IOC_FM_PCD_PLCR_RFC_2698,	/**< Policer algorithm RFC 2698 */
	e_IOC_FM_PCD_PLCR_RFC_4115	/**< Policer algorithm RFC 4115 */
} ioc_fm_pcd_plcr_algorithm_selection;

/*
 * @Description   Enumeration type for selecting a policer profile color mode
 */
typedef enum ioc_fm_pcd_plcr_color_mode {
	e_IOC_FM_PCD_PLCR_COLOR_BLIND,  /**< Color blind */
	e_IOC_FM_PCD_PLCR_COLOR_AWARE   /**< Color aware */
} ioc_fm_pcd_plcr_color_mode;

/*
 * @Description   Enumeration type for selecting a policer profile color
 */
typedef enum ioc_fm_pcd_plcr_color {
	e_IOC_FM_PCD_PLCR_GREEN,	/**< Green */
	e_IOC_FM_PCD_PLCR_YELLOW,	/**< Yellow */
	e_IOC_FM_PCD_PLCR_RED,		/**< Red */
	e_IOC_FM_PCD_PLCR_OVERRIDE	/**< Color override */
} ioc_fm_pcd_plcr_color;

/*
 * @Description   Enumeration type for selecting the policer profile packet
 *		  frame length selector
 */
typedef enum ioc_fm_pcd_plcr_frame_length_select {
	e_IOC_FM_PCD_PLCR_L2_FRM_LEN,	/**< L2 frame length */
	e_IOC_FM_PCD_PLCR_L3_FRM_LEN,	/**< L3 frame length */
	e_IOC_FM_PCD_PLCR_L4_FRM_LEN,	/**< L4 frame length */
	e_IOC_FM_PCD_PLCR_FULL_FRM_LEN	/**< Full frame length */
} ioc_fm_pcd_plcr_frame_length_select;

/*
 * @Description   Enumeration type for selecting roll-back frame
 */
typedef enum ioc_fm_pcd_plcr_roll_back_frame_select {
	e_IOC_FM_PCD_PLCR_ROLLBACK_L2_FRM_LEN,	/**< Rollback L2 frame length */
	e_IOC_FM_PCD_PLCR_ROLLBACK_FULL_FRM_LEN
				/**< Rollback Full frame length */
} ioc_fm_pcd_plcr_roll_back_frame_select;

/*
 * @Description   Enumeration type for selecting the policer profile packet or
 *		  byte mode
 */
typedef enum ioc_fm_pcd_plcr_rate_mode {
	e_IOC_FM_PCD_PLCR_BYTE_MODE,	/**< Byte mode */
	e_IOC_FM_PCD_PLCR_PACKET_MODE   /**< Packet mode */
} ioc_fm_pcd_plcr_rate_mode;

/*
 * @Description   Enumeration type for defining action of frame
 */
typedef enum ioc_fm_pcd_done_action {
	e_IOC_FM_PCD_ENQ_FRAME = 0,	/**< Enqueue frame */
	e_IOC_FM_PCD_DROP_FRAME	/**< Drop frame */
} ioc_fm_pcd_done_action;

/*
 * @Description   Enumeration type for selecting the policer counter
 */
typedef enum ioc_fm_pcd_plcr_profile_counters {
	e_IOC_FM_PCD_PLCR_PROFILE_GREEN_PACKET_TOTAL_COUNTER,
					/**< Green packets counter */
	e_IOC_FM_PCD_PLCR_PROFILE_YELLOW_PACKET_TOTAL_COUNTER,
					/**< Yellow packets counter */
	e_IOC_FM_PCD_PLCR_PROFILE_RED_PACKET_TOTAL_COUNTER,
					/**< Red packets counter */
	e_IOC_FM_PCD_PLCR_PROFILE_RECOLOURED_YELLOW_PACKET_TOTAL_COUNTER,
					/**< Recolored yellow packets counter */
	e_IOC_FM_PCD_PLCR_PROFILE_RECOLOURED_RED_PACKET_TOTAL_COUNTER
					/**< Recolored red packets counter */
} ioc_fm_pcd_plcr_profile_counters;

/*
 * @Description   Enumeration type for selecting the PCD action after extraction
 */
typedef enum ioc_fm_pcd_action {
	e_IOC_FM_PCD_ACTION_NONE,		/**< NONE  */
	e_IOC_FM_PCD_ACTION_EXACT_MATCH,
		/**< Exact match on the selected extraction */
	e_IOC_FM_PCD_ACTION_INDEXED_LOOKUP
		/**< Indexed lookup on the selected extraction */
} ioc_fm_pcd_action;

/*
 * @Description   Enumeration type for selecting type of insert manipulation
 */
typedef enum ioc_fm_pcd_manip_hdr_insrt_type {
	e_IOC_FM_PCD_MANIP_INSRT_GENERIC,
		/**< Insert according to offset & size */
	e_IOC_FM_PCD_MANIP_INSRT_BY_HDR,
		/**< Insert according to protocol */
} ioc_fm_pcd_manip_hdr_insrt_type;

/*
 * @Description   Enumeration type for selecting type of remove manipulation
 */
typedef enum ioc_fm_pcd_manip_hdr_rmv_type {
	e_IOC_FM_PCD_MANIP_RMV_GENERIC,
		/**< Remove according to offset & size */
	e_IOC_FM_PCD_MANIP_RMV_BY_HDR
		/**< Remove according to offset & size */
} ioc_fm_pcd_manip_hdr_rmv_type;

/*
 * @Description   An enum for selecting specific L2 fields removal
 */
typedef enum ioc_fm_pcd_manip_hdr_rmv_specific_l2 {
	e_IOC_FM_PCD_MANIP_HDR_RMV_ETHERNET,	/**< Ethernet/802.3 MAC */
	e_IOC_FM_PCD_MANIP_HDR_RMV_STACKED_QTAGS,	/**< stacked QTags */
	e_IOC_FM_PCD_MANIP_HDR_RMV_ETHERNET_AND_MPLS,
			/**< MPLS and Ethernet/802.3 MAC header unitl the header
			 * which follows the MPLS header
			 */
	e_IOC_FM_PCD_MANIP_HDR_RMV_MPLS
			/**< Remove MPLS header (Unlimited MPLS labels) */
} ioc_fm_pcd_manip_hdr_rmv_specific_l2;

/*
 * @Description   Enumeration type for selecting specific fields updates
 */
typedef enum ioc_fm_pcd_manip_hdr_field_update_type {
	e_IOC_FM_PCD_MANIP_HDR_FIELD_UPDATE_VLAN,	/**< VLAN updates */
	e_IOC_FM_PCD_MANIP_HDR_FIELD_UPDATE_IPV4,	/**< IPV4 updates */
	e_IOC_FM_PCD_MANIP_HDR_FIELD_UPDATE_IPV6,	/**< IPV6 updates */
	e_IOC_FM_PCD_MANIP_HDR_FIELD_UPDATE_TCP_UDP,	/**< TCP_UDP updates */
} ioc_fm_pcd_manip_hdr_field_update_type;

/*
 * @Description   Enumeration type for selecting VLAN updates
 */
typedef enum ioc_fm_pcd_manip_hdr_field_update_vlan {
	e_IOC_FM_PCD_MANIP_HDR_FIELD_UPDATE_VLAN_VPRI,
				/**< Replace VPri of outer most VLAN tag. */
	e_IOC_FM_PCD_MANIP_HDR_FIELD_UPDATE_DSCP_TO_VLAN
				/**< DSCP to VLAN priority bits translation */
} ioc_fm_pcd_manip_hdr_field_update_vlan;

/*
 * @Description   Enumeration type for selecting specific L2 fields removal
 */
typedef enum ioc_fm_pcd_manip_hdr_insrt_specific_l2 {
	e_IOC_FM_PCD_MANIP_HDR_INSRT_MPLS
		/**< Insert MPLS header (Unlimited MPLS labels) */
} ioc_fm_pcd_manip_hdr_insrt_specific_l2;

/*
 * @Description   Enumeration type for selecting QoS mapping mode
 *
 *		  Note: In all cases except
 *		  'e_FM_PCD_MANIP_HDR_QOS_MAPPING_NONE' User should instruct the
 *		  port to read the parser-result
 */
typedef enum ioc_fm_pcd_manip_hdr_qos_mapping_mode {
	e_IOC_FM_PCD_MANIP_HDR_QOS_MAPPING_NONE = 0,
			/**< No mapping, QoS field will not be changed */
	e_IOC_FM_PCD_MANIP_HDR_QOS_MAPPING_AS_IS,
			/**< QoS field will be overwritten by the last byte in
			 * the parser-result.
			 */
} ioc_fm_pcd_manip_hdr_qos_mapping_mode;

/*
 * @Description   Enumeration type for selecting QoS source
 *
 *		  Note: In all cases except 'e_FM_PCD_MANIP_HDR_QOS_SRC_NONE'
 *		  User should left room for the parser-result on input/output
 *		  buffer and instruct the port to read/write the parser-result
 *		  to the buffer (RPD should be set)
 */
typedef enum ioc_fm_pcd_manip_hdr_qos_src {
	e_IOC_FM_PCD_MANIP_HDR_QOS_SRC_NONE = 0,
			/**< TODO */
	e_IOC_FM_PCD_MANIP_HDR_QOS_SRC_USER_DEFINED,
			/**< QoS will be taken from the last byte in the
			 * parser-result.
			 */
} ioc_fm_pcd_manip_hdr_qos_src;

/*
 * @Description   Enumeration type for selecting type of header insertion
 */
typedef enum ioc_fm_pcd_manip_hdr_insrt_by_hdr_type {
	e_IOC_FM_PCD_MANIP_INSRT_BY_HDR_SPECIFIC_L2,
			/**< Specific L2 fields insertion */
	e_IOC_FM_PCD_MANIP_INSRT_BY_HDR_IP,		/**< IP insertion */
	e_IOC_FM_PCD_MANIP_INSRT_BY_HDR_UDP,		/**< UDP insertion */
	e_IOC_FM_PCD_MANIP_INSRT_BY_HDR_UDP_LITE,
			/**< UDP lite insertion */
	e_IOC_FM_PCD_MANIP_INSRT_BY_HDR_CAPWAP		/**< CAPWAP insertion */
} ioc_fm_pcd_manip_hdr_insrt_by_hdr_type;

/*
 * @Description   Enumeration type for selecting specific custom command
 */
typedef enum ioc_fm_pcd_manip_hdr_custom_type {
	e_IOC_FM_PCD_MANIP_HDR_CUSTOM_IP_REPLACE,
			/**< Replace IPv4/IPv6 */
	e_IOC_FM_PCD_MANIP_HDR_CUSTOM_GEN_FIELD_REPLACE,
} ioc_fm_pcd_manip_hdr_custom_type;

/*
 * @Description   Enumeration type for selecting specific custom command
 */
typedef enum ioc_fm_pcd_manip_hdr_custom_ip_replace {
	e_IOC_FM_PCD_MANIP_HDR_CUSTOM_REPLACE_IPV4_BY_IPV6,
					/**< Replace IPv4 by IPv6 */
	e_IOC_FM_PCD_MANIP_HDR_CUSTOM_REPLACE_IPV6_BY_IPV4
					/**< Replace IPv6 by IPv4 */
} ioc_fm_pcd_manip_hdr_custom_ip_replace;

/*
 * @Description   Enumeration type for selecting type of header removal
 */
typedef enum ioc_fm_pcd_manip_hdr_rmv_by_hdr_type {
	e_IOC_FM_PCD_MANIP_RMV_BY_HDR_SPECIFIC_L2 = 0,
			/**< Specific L2 fields removal */
	e_IOC_FM_PCD_MANIP_RMV_BY_HDR_CAPWAP,	/**< CAPWAP removal */
	e_IOC_FM_PCD_MANIP_RMV_BY_HDR_FROM_START,
				/**< Locate from data that is not the header */
} ioc_fm_pcd_manip_hdr_rmv_by_hdr_type;

/*
 * @Description   Enumeration type for selecting type of timeout mode
 */
typedef enum ioc_fm_pcd_manip_reassem_time_out_mode {
	e_IOC_FM_PCD_MANIP_TIME_OUT_BETWEEN_FRAMES,
					/**< Limits the time of the reassembly
					 * process from the first fragment to
					 * the last
					 */
	e_IOC_FM_PCD_MANIP_TIME_OUT_BETWEEN_FRAG
					/**< Limits the time of receiving the
					 * fragment
					 */
} ioc_fm_pcd_manip_reassem_time_out_mode;

/*
 * @Description   Enumeration type for selecting type of WaysNumber mode
 */
typedef enum ioc_fm_pcd_manip_reassem_ways_number {
	e_IOC_FM_PCD_MANIP_ONE_WAY_HASH = 1,	/**< One way hash    */
	e_IOC_FM_PCD_MANIP_TWO_WAYS_HASH,	/**< Two ways hash   */
	e_IOC_FM_PCD_MANIP_THREE_WAYS_HASH,	/**< Three ways hash */
	e_IOC_FM_PCD_MANIP_FOUR_WAYS_HASH,	/**< Four ways hash  */
	e_IOC_FM_PCD_MANIP_FIVE_WAYS_HASH,	/**< Five ways hash  */
	e_IOC_FM_PCD_MANIP_SIX_WAYS_HASH,	/**< Six ways hash   */
	e_IOC_FM_PCD_MANIP_SEVEN_WAYS_HASH,	/**< Seven ways hash */
	e_IOC_FM_PCD_MANIP_EIGHT_WAYS_HASH	/**< Eight ways hash */
} ioc_fm_pcd_manip_reassem_ways_number;

/*
 * @Description   Enumeration type for selecting manipulation type
 */
typedef enum ioc_fm_pcd_manip_type {
	e_IOC_FM_PCD_MANIP_HDR = 0,		/**< Header manipulation */
	e_IOC_FM_PCD_MANIP_REASSEM,		/**< Reassembly */
	e_IOC_FM_PCD_MANIP_FRAG,		/**< Fragmentation */
	e_IOC_FM_PCD_MANIP_SPECIAL_OFFLOAD	/**< Special Offloading */
} ioc_fm_pcd_manip_type;

/*
 * @Description   Enumeration type for selecting type of statistics mode
 */
typedef enum ioc_fm_pcd_cc_stats_mode {
	e_IOC_FM_PCD_CC_STATS_MODE_NONE = 0,	/**< No statistics support */
	e_IOC_FM_PCD_CC_STATS_MODE_FRAME,	/**< Frame count statistics */
	e_IOC_FM_PCD_CC_STATS_MODE_BYTE_AND_FRAME,
			/**< Byte and frame count statistics */
	e_IOC_FM_PCD_CC_STATS_MODE_RMON,
			/**< Byte and frame length range count statistics */
} ioc_fm_pcd_cc_stats_mode;

/*
 * @Description   Enumeration type for determining the action in case an IP
 *		  packet is larger than MTU but its DF (Don't Fragment) bit is
 *		  set.
 */
typedef enum ioc_fm_pcd_manip_donot_frag_action {
	e_IOC_FM_PCD_MANIP_DISCARD_PACKET = 0,	/**< Discard packet */
	e_IOC_FM_PCD_MANIP_ENQ_TO_ERR_Q_OR_DISCARD_PACKET =
			e_IOC_FM_PCD_MANIP_DISCARD_PACKET,
				/**< Obsolete, cannot enqueue to error queue; In
				 * practice, selects to discard packets; Will be
				 * removed in the future
				 */
	e_IOC_FM_PCD_MANIP_FRAGMENT_PACKECT,
				/**< Fragment packet and continue normal
				 * processing
				 */
	e_IOC_FM_PCD_MANIP_CONTINUE_WITHOUT_FRAG
				/**< Continue normal processing without
				 * fragmenting the packet
				 */
} ioc_fm_pcd_manip_donot_frag_action;

/*
 * @Description   Enumeration type for selecting type of special offload
 *		  manipulation
 */
typedef enum ioc_fm_pcd_manip_special_offload_type {
	e_IOC_FM_PCD_MANIP_SPECIAL_OFFLOAD_IPSEC,
					/**< IPSec offload manipulation */
	e_IOC_FM_PCD_MANIP_SPECIAL_OFFLOAD_CAPWAP
					/**< CAPWAP offload manipulation */
} ioc_fm_pcd_manip_special_offload_type;

/*
 * @Description   A union of protocol dependent special options
 *		  (Must match union u_FmPcdHdrProtocolOpt defined in
 *		  fm_pcd_ext.h)
 */
typedef union ioc_fm_pcd_hdr_protocol_opt_u {
	ioc_eth_protocol_opt_t	eth_opt;	/**< Ethernet options */
	ioc_vlan_protocol_opt_t   vlan_opt;	/**< Vlan options */
	ioc_mpls_protocol_opt_t   mpls_opt;	/**< MPLS options */
	ioc_ipv4_protocol_opt_t   ipv4_opt;	/**< IPv4 options */
	ioc_ipv6_protocol_opt_t   ipv6_opt;	/**< IPv6 options */
	ioc_capwap_protocol_opt_t capwap_opt;  /**< CAPWAP options */
} ioc_fm_pcd_hdr_protocol_opt_u;

/*
 * @Description   A union holding all known protocol fields
 */
typedef union ioc_fm_pcd_fields_u {
	ioc_header_field_eth_t		eth;		/**< Ethernet*/
	ioc_header_field_vlan_t		vlan;		/**< VLAN*/
	ioc_header_field_llc_snap_t	llc_snap;	/**< LLC SNAP*/
	ioc_header_field_pppoe_t		pppoe;	/**< PPPoE*/
	ioc_header_field_mpls_t		mpls;		/**< MPLS*/
	ioc_header_field_ip_t		ip;		/**< IP	*/
	ioc_header_field_ipv4_t		ipv4;		/**< IPv4*/
	ioc_header_field_ipv6_t		ipv6;		/**< IPv6*/
	ioc_header_field_udp_t		udp;		/**< UDP	*/
	ioc_header_field_udp_lite_t	udp_lite;	/**< UDP_Lite*/
	ioc_header_field_tcp_t		tcp;		/**< TCP	*/
	ioc_header_field_sctp_t		sctp;		/**< SCTP*/
	ioc_header_field_dccp_t		dccp;		/**< DCCP*/
	ioc_header_field_gre_t		gre;		/**< GRE	*/
	ioc_header_field_minencap_t	minencap;/**< Minimal Encapsulation  */
	ioc_header_field_ipsec_ah_t	ipsec_ah;	/**< IPSec AH*/
	ioc_header_field_ipsec_esp_t	ipsec_esp;	/**< IPSec ESP*/
	ioc_header_field_udp_encap_esp_t	udp_encap_esp;
						/**< UDP Encapsulation ESP  */
} ioc_fm_pcd_fields_u;

/*
 * @Description   Parameters for defining header extraction for key generation
 */
typedef struct ioc_fm_pcd_from_hdr_t {
	uint8_t		size;	/**< Size in byte */
	uint8_t		offset;	/**< Byte offset */
} ioc_fm_pcd_from_hdr_t;

/*
 * @Description   Parameters for defining field extraction for key generation
 */
typedef struct ioc_fm_pcd_from_field_t {
	ioc_fm_pcd_fields_u field;	/**< Field selection */
	uint8_t		size;	/**< Size in byte */
	uint8_t		offset;	/**< Byte offset */
} ioc_fm_pcd_from_field_t;

/*
 * @Description   Parameters for defining a single network environment unit
 *		  A distinction unit should be defined if it will later be used
 *		  by one or more PCD engines to distinguish between flows.
 *		  (Must match struct t_FmPcdDistinctionUnit defined in
 *		  fm_pcd_ext.h)
 */
typedef struct ioc_fm_pcd_distinction_unit_t {
	struct {
	ioc_net_header_type	hdr;
				/**< One of the headers supported by the FM */
	ioc_fm_pcd_hdr_protocol_opt_u  opt;	/**< Select only one option! */
	} hdrs[IOC_FM_PCD_MAX_NUM_OF_INTERCHANGEABLE_HDRS];
} ioc_fm_pcd_distinction_unit_t;

/*
 * @Description   Parameters for defining all different distinction units
 *		  supported by a specific PCD Network Environment
 *		  Characteristics module.
 *
 *		  Each unit represent a protocol or a group of protocols that
 *		  may be used later by the different PCD engines to distinguish
 *		  between flows.
 *		  (Must match struct t_FmPcdNetEnvParams defined in
 *		  fm_pcd_ext.h)
 */
struct fm_pcd_net_env_params_t {
	uint8_t num_of_distinction_units;
	/**< Number of different units to be identified */
	ioc_fm_pcd_distinction_unit_t
		units[IOC_FM_PCD_MAX_NUM_OF_DISTINCTION_UNITS];
	/**< An array of num_of_distinction_units of the different units to be
	 * identified
	 */
};

typedef struct ioc_fm_pcd_net_env_params_t {
	struct fm_pcd_net_env_params_t param;
	void				*id;
		/**< Output parameter; Returns the net-env Id to be used */
} ioc_fm_pcd_net_env_params_t;

/*
 * @Description   Parameters for defining a single extraction action when
 *		  creating a key
 */
typedef struct ioc_fm_pcd_extract_entry_t {
	ioc_fm_pcd_extract_type		type;	/**< Extraction type select */
	union {
	struct {
		ioc_net_header_type	hdr;		/**< Header selection */
		bool			ignore_protocol_validation;
					/**< Ignore protocol validation */
		ioc_fm_pcd_hdr_index	hdr_index;
					/**< Relevant only for MPLS, VLAN and
					 * tunneled IP. Otherwise should be
					 * cleared.
					 */
		ioc_fm_pcd_extract_by_hdr_type  type;
					/**< Header extraction type select */
		union {
		ioc_fm_pcd_from_hdr_t	from_hdr;
					/**< Extract bytes from header
					 * parameters
					 */
		ioc_fm_pcd_from_field_t	from_field;
					/**< Extract bytes from field parameters
					 */
		ioc_fm_pcd_fields_u	full_field;
					/**< Extract full field parameters */
		} extract_by_hdr_type;
	} extract_by_hdr;/**< Used when type = e_IOC_FM_PCD_KG_EXTRACT_BY_HDR */
	struct {
		ioc_fm_pcd_extract_from	src;
					/**< Non-header extraction source */
		ioc_fm_pcd_action	action;	/**< Relevant for CC Only */
		uint16_t	ic_indx_mask;
				/**< Relevant only for CC whenaction =
				 * e_IOC_FM_PCD_ACTION_INDEXED_LOOKUP; Note that
				 * the number of bits that are set within this
				 * mask must be log2 of the CC-node
				 * 'num_of_keys'. Note that the mask cannot be
				 * set on the lower bits.
				 */
		uint8_t			offset;	/**< Byte offset */
		uint8_t			size;	/**< Size in bytes */
	} extract_non_hdr;
		/**< Used when type = e_IOC_FM_PCD_KG_EXTRACT_NON_HDR */
	} extract_params;
} ioc_fm_pcd_extract_entry_t;

/*
 * @Description   A structure for defining masks for each extracted
 *		  field in the key.
 */
typedef struct ioc_fm_pcd_kg_extract_mask_t {
	uint8_t		extract_array_index;
				/**< Index in the extraction array, as
				 * initialized by user
				 */
	uint8_t		offset;	/**< Byte offset */
	uint8_t		mask;
			/**< A byte mask (selected bits will be ignored) */
} ioc_fm_pcd_kg_extract_mask_t;

/*
 * @Description   A structure for defining default selection per groups of
 *		  fields
 */
typedef struct ioc_fm_pcd_kg_extract_dflt_t {
	ioc_fm_pcd_kg_known_fields_dflt_types	type;
						/**< Default type select */
	ioc_fm_pcd_kg_extract_dflt_select	dflt_select;
						/**< Default register select */
} ioc_fm_pcd_kg_extract_dflt_t;


/*
 * @Description   A structure for defining all parameters needed for
 *		  generation a key and using a hash function
 */
typedef struct ioc_fm_pcd_kg_key_extract_and_hash_params_t {
	uint32_t			private_dflt0;
					/**< Scheme default register 0 */
	uint32_t			private_dflt1;
					/**< Scheme default register 1 */
	uint8_t				num_of_used_extracts;
					/**< defines the valid size of the
					 * following array
					 */
	ioc_fm_pcd_extract_entry_t
			extract_array[IOC_FM_PCD_KG_MAX_EXTRACTS_PER_KEY];
					/**< An array of extraction definitions.
					 */
	uint8_t				num_of_used_dflts;
					/**< defines the valid size of the
					 * following array
					 */
	ioc_fm_pcd_kg_extract_dflt_t
				dflts[IOC_FM_PCD_KG_NUM_OF_DEFAULT_GROUPS];
					/**< For each extraction used in this
					 * scheme, specify the required default
					 * register to be used when header is
					 * not found. types not specified in
					 * this array will get undefined value.
					 */
	uint8_t				num_of_used_masks;
					/**< Defines the valid size of the
					 * following array
					 */
	ioc_fm_pcd_kg_extract_mask_t
				masks[IOC_FM_PCD_KG_NUM_OF_EXTRACT_MASKS];
	uint8_t				hash_shift;
					/**< Hash result right shift. Selects
					 * the 24 bits out of the 64 hash
					 * result. 0 means using the 24 LSB's,
					 * otherwise use the 24 LSB's after
					 * shifting right.
					 */
	uint32_t			hash_dist_num_of_fqids;
					/**< must be > 1 and a power of 2.
					 * Represents the range of queues for
					 * the key and hash functionality
					 */
	uint8_t				hash_distribution_fqids_shift;
					/**< selects the FQID bits that will be
					 * effected by the hash
					 */
	bool				symmetric_hash;
					/**< TRUE to generate the same hash for
					 * frames with swapped source and
					 * destination fields on all layers; If
					 * TRUE, driver will check that for all
					 * layers, if SRC extraction is
					 * selected, DST extraction must also be
					 * selected, and vice versa.
					 */
} ioc_fm_pcd_kg_key_extract_and_hash_params_t;

/*
 * @Description   A structure of parameters for defining a single Qid mask
 *		  (extracted OR).
 */
typedef struct ioc_fm_pcd_kg_extracted_or_params_t {
	ioc_fm_pcd_extract_type		type;
					/**< Extraction type select */
	union {
	struct {
			/**< used when type = e_IOC_FM_PCD_KG_EXTRACT_BY_HDR */
		ioc_net_header_type		hdr;
		ioc_fm_pcd_hdr_index		hdr_index;
						/**< Relevant only for MPLS,
						 * VLAN and tunneled IP.
						 * Otherwise should be cleared.
						 */
		bool				ignore_protocol_validation;

	} extract_by_hdr;
	ioc_fm_pcd_extract_from		src;
					/**< used when type =
					 * e_IOC_FM_PCD_KG_EXTRACT_NON_HDR
					 */
	} extract_params;
	uint8_t				extraction_offset;
					/**< Offset for extraction */
	ioc_fm_pcd_kg_extract_dflt_select	dflt_value;
					/**< Select register from which
					 * extraction is taken if field not
					 * found
					 */
	uint8_t				mask;
					/**< Mask LSB byte of extraction
					 * (specified bits are ignored)
					 */

	uint8_t			bit_offset_in_fqid;
		/**< 0-31, Selects which bits of the 24 FQID bits to effect
		 * using the extracted byte; Assume byte is placed as the 8
		 * MSB's in a 32 bit word where the lower bits are the FQID; i.e
		 * if bitOffsetInFqid=1 than its LSB will effect the FQID MSB,
		 * if bitOffsetInFqid=24 than the extracted byte will effect the
		 * 8 LSB's of the FQID, if bitOffsetInFqid=31 than the byte's
		 * MSB will effect the FQID's LSB; 0 means - no effect on FQID;
		 * Note that one, and only one of bitOffsetInFqid or
		 * bitOffsetInPlcrProfile must be set (i.e, extracted byte must
		 * effect either FQID or Policer profile).
		 */
	uint8_t			bit_offset_in_plcr_profile;
		/**< 0-15, Selects which bits of the 8 policer profile id bits
		 * to effect using the extracted byte; Assume byte is placed as
		 * the 8 MSB's in a 16 bit word where the lower bits are the
		 * policer profile id; i.e if bitOffsetInPlcrProfile=1 than its
		 * LSB will effect the profile MSB, if bitOffsetInFqid=8 than
		 * the extracted byte will effect the whole policer profile id,
		 * if bitOffsetInFqid=15 than the byte's MSB will effect the
		 * Policer Profile id's LSB; 0 means - no effect on policer
		 * profile; Note that one, and only one of bitOffsetInFqid or
		 * bitOffsetInPlcrProfile must be set (i.e, extracted byte must
		 * effect either FQID or Policer profile).
		 */
} ioc_fm_pcd_kg_extracted_or_params_t;

/*
 * @Description   A structure for configuring scheme counter
 */
typedef struct ioc_fm_pcd_kg_scheme_counter_t {
	bool		update;
			/**< FALSE to keep the current counter state and
			 * continue from that point, TRUE to update/reset the
			 * counter when the scheme is written.
			 */
	uint32_t	value;
			/**< If update=TRUE, this value will be written into the
			 * counter; clear this field to reset the counter.
			 */
} ioc_fm_pcd_kg_scheme_counter_t;


/*
 * @Description   A structure for retrieving FMKG_SE_SPC
 */
typedef struct ioc_fm_pcd_kg_scheme_spc_t {
	uint32_t	val;	/**< return value */
	void	*id;		/**< scheme handle */
} ioc_fm_pcd_kg_scheme_spc_t;

/*
 * @Description   A structure for defining policer profile parameters as
 *		  required by keygen (when policer is the next engine after this
 *		  scheme).
 *		  (Must match struct t_FmPcdKgPlcrProfile defined in
 *		  fm_pcd_ext.h)
 */
typedef struct ioc_fm_pcd_kg_plcr_profile_t {
	bool		shared_profile;
			/**< TRUE if this profile is shared between ports (i.e.
			 * managed by primary partition) May not be TRUE if
			 * profile is after Coarse Classification
			 */
	bool		direct;
			/**< If TRUE, direct_relative_profile_id only selects
			 * the profile id, if FALSE
			 * fqid_offset_relative_profile_id_base is used together
			 * with fqid_offset_shift and num_of_profiles
			 * parameters, to define a range of profiles from which
			 * the KeyGen result will determine the destination
			 * policer profile.
			 */
	union {
	uint16_t	direct_relative_profile_id;
			/**< Used if 'direct' is TRUE, to select policer
			 * profile. This parameter should indicate the policer
			 * profile offset within the port's policer profiles or
			 * SHARED window.
			 */
	struct {
		uint8_t	fqid_offset_shift;
			/**< Shift of KG results without the qid base */
		uint8_t	fqid_offset_relative_profile_id_base;
			/**< OR of KG results without the qid base This
			 * parameter should indicate the policer profile offset
			 * within the port's policer profiles window or SHARED
			 * window depends on shared_profile
			 */
		uint8_t	num_of_profiles;
			/**< Range of profiles starting at base */
	} indirect_profile;		/**< Indirect profile parameters */
	} profile_select;
			/**< Direct/indirect profile selection and parameters */
} ioc_fm_pcd_kg_plcr_profile_t;

/*
 * @Description   Parameters for configuring a storage profile for a KeyGen
 *		  scheme.
 */
typedef struct ioc_fm_pcd_kg_storage_profile_t {
	bool	direct;
		/**< If TRUE, directRelativeProfileId only selects the profile
		 * id; If FALSE, fqidOffsetRelativeProfileIdBase is used
		 * together with fqidOffsetShift and num_of_profiles parameters
		 * to define a range of profiles from which the KeyGen result
		 * will determine the destination storage profile.
		 */
	union {
		uint16_t	direct_relative_profile_id;
		/**< Used when 'direct' is TRUE, to select a storage profile;
		 * should indicate the storage profile offset within the port's
		 * storage profiles window.
		 */
		struct {
			uint8_t	fqid_offset_shift;
			/**< Shift of KeyGen results without the FQID base */
			uint8_t	fqid_offset_relative_profile_id_base;
			/**< OR of KeyGen results without the FQID base; should
			 * indicate the policer profile offset within the port's
			 * storage profiles window.
			 */
			uint8_t	num_of_profiles;
			/**< Range of profiles starting at base. */
		} indirect_profile;
		/**< Indirect profile parameters. */
	} profile_select;
	/**< Direct/indirect profile selection and parameters. */
} ioc_fm_pcd_kg_storage_profile_t;

/*
 * @Description   Parameters for defining CC as the next engine after KeyGen
 *		  (Must match struct t_FmPcdKgCc defined in fm_pcd_ext.h)
 */
typedef struct ioc_fm_pcd_kg_cc_t {
	void				*tree_id;
					/**< CC Tree id */
	uint8_t				grp_id;
					/**< CC group id within the CC tree */
	bool				plcr_next;
					/**< TRUE if after CC, in case of data
					 * frame, policing is required.
					 */
	bool				bypass_plcr_profile_generation;
					/**< TRUE to bypass KeyGen policer
					 * profile generation; selected profile
					 * is the one set at port initialization
					 */
	ioc_fm_pcd_kg_plcr_profile_t	plcr_profile;
					/**< Valid only if plcr_next = TRUE and
					 * bypass_plcr_profile_generation =
					 * FALSE
					 */
} ioc_fm_pcd_kg_cc_t;

/*
 * @Description   Parameters for defining initializing a KeyGen scheme (Must
 *		  match struct t_FmPcdKgSchemeParams defined in fm_pcd_ext.h)
 */
struct fm_pcd_kg_scheme_params_t {
	bool modify;	/**< TRUE to change an existing scheme */
	union {
		uint8_t relative_scheme_id;
		/**< if modify=FALSE: partition-relative scheme id */
		void *scheme_id;
		/**< if modify=TRUE: the id of an existing scheme */
	} scm_id;
	bool always_direct;
		/**< This scheme is reached only directly, i.e. no need for
		 * match vector; KeyGen will ignore it when matching
		 */
	struct {
		/**< HL relevant only if always_direct=FALSE */
		void *net_env_id;
		/**< The id of the Network Environment as returned
		 * by fm_pcd_net_env_characteristics_set()
		 */
		uint8_t num_of_distinction_units;
		/**< Number of NetEnv units listed in unit_ids array */
		uint8_t unit_ids[IOC_FM_PCD_MAX_NUM_OF_DISTINCTION_UNITS];
		/**< Indexes as passed to SetNetEnvCharacteristics (?) array */
	} net_env_params;
	bool use_hash;
		/**< use the KG Hash functionality */
	ioc_fm_pcd_kg_key_extract_and_hash_params_t key_ext_and_hash;
		/**< used only if useHash = TRUE */
	bool bypass_fqid_generation;
		/**< Normally - FALSE, TRUE to avoid FQID update in the IC; In
		 * such a case FQID after KG will be the default FQID defined
		 * for the relevant port, or the FQID defined by CC in cases
		 * where CC was the previous engine.
		 */
	uint32_t base_fqid;
		/**< Base FQID; Relevant only if bypass_fqid_generation = FALSE;
		 * If hash is used and an even distribution is expected
		 * according to hash_dist_num_of_fqids, base_fqid must
		 * be aligned to hash_dist_num_of_fqids.
		 */
	uint8_t num_of_used_extracted_ors;
		/**< Number of FQID masks listed in extracted_ors array*/
	ioc_fm_pcd_kg_extracted_or_params_t
		extracted_ors[IOC_FM_PCD_KG_NUM_OF_GENERIC_REGS];
		/**< IOC_FM_PCD_KG_NUM_OF_GENERIC_REGS registers are shared
		 * between qid_masks functionality and some of the extraction
		 * actions; Normally only some will be used for qid_mask. Driver
		 * will return error if resource is full at initialization time.
		 */
	bool override_storage_profile;
		/**< TRUE if KeyGen override previously decided storage profile
		 */
	ioc_fm_pcd_kg_storage_profile_t storage_profile;
		/**< Used when override_storage_profile=TRUE */
	ioc_fm_pcd_engine next_engine;
		/**< may be BMI, PLCR or CC */
	union {
		/**< depends on nextEngine */
		ioc_fm_pcd_done_action done_action;
		/**< Used when next engine is BMI (done) */
		ioc_fm_pcd_kg_plcr_profile_t plcr_profile;
		/**< Used when next engine is PLCR */
		ioc_fm_pcd_kg_cc_t cc;
		/**< Used when next engine is CC */
	} kg_next_engine_params;
	ioc_fm_pcd_kg_scheme_counter_t scheme_counter;
		/**< A structure of parameters for updating the scheme counter*/
};

typedef struct ioc_fm_pcd_kg_scheme_params_t {
	struct fm_pcd_kg_scheme_params_t param;
	void *id;		/**< Returns the scheme Id to be used */
} ioc_fm_pcd_kg_scheme_params_t;

/*
 * @Collection
 */
#define IOC_FM_PCD_CC_STATS_MAX_FLR	10
			/* Maximal supported number of frame length ranges */
#define IOC_FM_PCD_CC_STATS_FLR_SIZE		2
			/* Size in bytes of a frame length range limit */
#define IOC_FM_PCD_CC_STATS_FLR_COUNT_SIZE	4
			/* Size in bytes of a frame length range counter */
/* @} */

/*
 * @Description   Parameters for defining CC as the next engine after a CC node.
 *		  (Must match struct t_FmPcdCcNextCcParams defined in
 *		  fm_pcd_ext.h)
 */
typedef struct ioc_fm_pcd_cc_next_cc_params_t {
	void	*cc_node_id;			/**< Id of the next CC node */
} ioc_fm_pcd_cc_next_cc_params_t;

/*
 * @Description   A structure for defining Frame Replicator as the next engine
 *		  after a CC node. (Must match struct t_FmPcdCcNextFrParams
 *		  defined in fm_pcd_ext.h)
 */
typedef struct ioc_fm_pcd_cc_next_fr_params_t {
	void *frm_replic_id;
			/**< The id of the next frame replicator group */
} ioc_fm_pcd_cc_next_fr_params_t;

/*
 * @Description   A structure for defining PLCR params when PLCR is the
 *		  next engine after a CC node
 *		  (Must match struct t_FmPcdCcNextPlcrParams defined in
 *		  fm_pcd_ext.h)
 */
typedef struct ioc_fm_pcd_cc_next_plcr_params_t {
	bool	override_params;
		/**< TRUE if CC override previously decided parameters*/
	bool	shared_profile;
		/**< Relevant only if overrideParams=TRUE: TRUE if this profile
		 * is shared between ports
		 */
	uint16_t	new_relative_profile_id;
		/**< Relevant only if overrideParams=TRUE: (otherwise profile id
		 * is taken from keygen); This parameter should indicate the
		 * policer profile offset within the port's policer profiles or
		 * from SHARED window.
		 */
	uint32_t	new_fqid;
		/**< Relevant only if overrideParams=TRUE: FQID for enquing the
		 * frame; In earlier chips  if policer next engine is KEYGEN,
		 * this parameter can be 0, because the KEYGEN always decides
		 * the enqueue FQID.
		 */
	uint8_t	new_relative_storage_profile_id;
		/**< Indicates the relative storage profile offset within the
		 * port's storage profiles window; Relevant only if the port was
		 * configured with VSP.
		 */
} ioc_fm_pcd_cc_next_plcr_params_t;

/*
 * @Description   A structure for defining enqueue params when BMI is the next
 *		  engine after a CC node (Must match struct
 *		  t_FmPcdCcNextEnqueueParams defined in fm_pcd_ext.h)
 */
typedef struct ioc_fm_pcd_cc_next_enqueue_params_t {
	ioc_fm_pcd_done_action  action;
				/**< Action - when next engine is BMI (done) */
	bool			override_fqid;
				/**< TRUE if CC override previously decided fqid
				 * and vspid, relevant if action =
				 * e_IOC_FM_PCD_ENQ_FRAME
				 */
	uint32_t		new_fqid;
				/**< Valid if overrideFqid=TRUE, FQID for
				 * enqueuing the frame (otherwise FQID is taken
				 * from KeyGen), relevant if action =
				 * e_IOC_FM_PCD_ENQ_FRAME
				 */
	uint8_t		new_relative_storage_profile_id;
			/**< Valid if override_fqid=TRUE, Indicates the relative
			 * virtual storage profile offset within the port's
			 * storage profiles window; Relevant only if the port
			 * was configured with VSP.
			 */

} ioc_fm_pcd_cc_next_enqueue_params_t;

/*
 * @Description   A structure for defining KG params when KG is the next engine
 *		  after a CC node (Must match struct t_FmPcdCcNextKgParams
 *		  defined in fm_pcd_ext.h)
 */
typedef struct ioc_fm_pcd_cc_next_kg_params_t {
	bool	override_fqid;
		/**< TRUE if CC override previously decided fqid and vspid,
		 * Note - this parameters are irrelevant for earlier chips
		 */
	uint32_t   new_fqid;
		/**< Valid if overrideFqid=TRUE, FQID for enqueuing the frame
		 * (otherwise FQID is taken from KeyGen),
		 * Note - this parameters are irrelevant for earlier chips
		 */
	uint8_t   new_relative_storage_profile_id;
		/**< Valid if override_fqid=TRUE, Indicates the relative virtual
		 * storage profile offset within the port's storage profiles
		 * window; Relevant only if the port was configured with VSP.
		 */
	void	*p_direct_scheme;	/**< Direct scheme id to go to. */
} ioc_fm_pcd_cc_next_kg_params_t;

/*
 * @Description   Parameters for defining the next engine after a CC node.
 *		  (Must match struct ioc_fm_pcd_cc_next_engine_params_t defined
 *		  in fm_pcd_ext.h)
 */
typedef struct ioc_fm_pcd_cc_next_engine_params_t {
	ioc_fm_pcd_engine			next_engine;
				/**< User has to initialize parameters according
				 * to nextEngine definition
				 */
	union {
		ioc_fm_pcd_cc_next_cc_params_t	cc_params;
				/**< Parameters in case next engine is CC */
		ioc_fm_pcd_cc_next_plcr_params_t	plcr_params;
				/**< Parameters in case next engine is PLCR */
		ioc_fm_pcd_cc_next_enqueue_params_t enqueue_params;
				/**< Parameters in case next engine is BMI */
		ioc_fm_pcd_cc_next_kg_params_t	kg_params;
				/**< Parameters in case next engine is KG */
		ioc_fm_pcd_cc_next_fr_params_t	fr_params;
				/**< Parameters in case next engine is FR */
	} params;
		/**< Union used for all the next-engine parameters options */
	void					*manip_id;
				/**< Handle to Manipulation object. Relevant if
				 * next engine is of type result
				 * (e_IOC_FM_PCD_PLCR, e_IOC_FM_PCD_KG,
				 * e_IOC_FM_PCD_DONE)
				 */
	bool					statistics_en;
				/**< If TRUE, statistics counters are
				 * incremented for each frame passing through
				 * this Coarse Classification entry.
				 */
} ioc_fm_pcd_cc_next_engine_params_t;

/*
 * @Description   Parameters for defining a single CC key
 */
typedef struct ioc_fm_pcd_cc_key_params_t {
	uint8_t		*p_key;
			/**< pointer to the key of the size defined in key_size
			 */
	uint8_t		*p_mask;
			/**< pointer to the Mask per key of the size defined in
			 * key_size. p_key and p_mask (if defined) has to be of
			 * the same size defined in the key_size
			 */
	ioc_fm_pcd_cc_next_engine_params_t  cc_next_engine_params;
			/**< parameters for the next for the defined Key in
			 * p_key
			 */

} ioc_fm_pcd_cc_key_params_t;

/*
 * @Description   Parameters for defining CC keys parameters
 *		  The driver supports two methods for CC node allocation:
 *		  dynamic and static. Static mode was created in order to
 *		  prevent runtime alloc/free of FMan memory (MURAM), which may
 *		  cause fragmentation; in this mode, the driver automatically
 *		  allocates the memory according to 'max_num_of_keys' parameter.
 *		  The driver calculates the maximal memory size that may be used
 *		  for this CC-Node taking into consideration 'mask_support' and
 *		  'statistics_mode' parameters. When 'action' =
 *		  e_IOC_FM_PCD_ACTION_INDEXED_LOOKUP in the extraction
 *		  parameters of this node, 'max_num_of_keys' must be equal to
 *		  'num_of_keys'. In dynamic mode, 'max_num_of_keys' must be
 *		  zero. At initialization, all required structures are allocated
 *		  according to 'num_of_keys' parameter. During runtime
 *		  modification, these structures are re-allocated according to
 *		  the updated number of keys.
 *		  Please note that 'action' and 'ic_indx_mask' mentioned in the
 *		  specific parameter explanations are passed in the extraction
 *		  parameters of the node (fields of
 *		  extractccparams.extractnonhdr).
 */
typedef struct ioc_keys_params_t {
	uint16_t		max_num_of_keys;
			/**< Maximum number of keys that will (ever) be used in
			 * this CC-Node; A value of zero may be used for dynamic
			 * memory allocation.
			 */
	bool			mask_support;
			/**< This parameter is relevant only if a node is
			 * initialized with action =
			 * e_IOC_FM_PCD_ACTION_EXACT_MATCH and max_num_of_keys >
			 * 0; Should be TRUE to reserve table memory for key
			 * masks, even if initial keys do not contain masks, or
			 * if the node was initialized as 'empty' (without
			 * keys); this will allow user to add keys with masks at
			 * runtime.
			 */
	ioc_fm_pcd_cc_stats_mode	statistics_mode;
			/**< Determines the supported statistics mode for all
			 * node's keys. To enable statistics gathering,
			 * statistics should be enabled per every key, using
			 * 'statistics_en' in next engine parameters structure
			 * of that key; If 'max_num_of_keys' is set, all
			 * required structures will be preallocated for all keys
			 */
	uint16_t	frame_length_ranges[IOC_FM_PCD_CC_STATS_MAX_FLR];
		/**< Relevant only for 'RMON' statistics mode (this feature is
		 * supported only on B4860 device); Holds a list of programmable
		 * thresholds. For each received frame, its length in bytes is
		 * examined against these range thresholds and the appropriate
		 * counter is incremented by 1. For example, to belong to range
		 * i, the following should hold: range i-1 threshold < frame
		 * length <= range i threshold Each range threshold must be
		 * larger then its preceding range threshold. Last range
		 * threshold must be 0xFFFF.
		 */
	uint16_t			num_of_keys;
		/**< Number of initial keys; Note that in case of 'action' =
		 * e_IOC_FM_PCD_ACTION_INDEXED_LOOKUP, this field should be
		 * power-of-2 of the number of bits that are set in
		 * 'ic_indx_mask'.
		 */
	uint8_t			key_size;
		/**< Size of key - for extraction of type FULL_FIELD, 'key_size'
		 * has to be the standard size of the selected key; For other
		 * extraction types, 'key_size' has to be as size of extraction;
		 * When 'action' = e_IOC_FM_PCD_ACTION_INDEXED_LOOKUP,
		 * 'key_size' must be 2.
		 */
	ioc_fm_pcd_cc_key_params_t  key_params[IOC_FM_PCD_MAX_NUM_OF_KEYS];
		/**< An array with 'num_of_keys' entries, each entry specifies
		 * the corresponding key parameters; When 'action' =
		 * e_IOC_FM_PCD_ACTION_EXACT_MATCH, this value must not exceed
		 * 255 (IOC_FM_PCD_MAX_NUM_OF_KEYS-1) as the last entry is saved
		 * for the 'miss' entry.
		 */
	ioc_fm_pcd_cc_next_engine_params_t  cc_next_engine_params_for_miss;
		/**< Parameters for defining the next engine when a key is not
		 * matched; Not relevant if action =
		 * e_IOC_FM_PCD_ACTION_INDEXED_LOOKUP.
		 */
} ioc_keys_params_t;

/*
 * @Description   Parameters for defining a CC node
 */
struct fm_pcd_cc_node_params_t {
	ioc_fm_pcd_extract_entry_t extract_cc_params;
	/**< Extraction parameters */
	ioc_keys_params_t keys_params;
	/**< Keys definition matching the selected extraction */
};

typedef struct ioc_fm_pcd_cc_node_params_t {
	struct fm_pcd_cc_node_params_t param;
	void *id;
	/**< Output parameter; returns the CC node Id to be used */
} ioc_fm_pcd_cc_node_params_t;

/*
 * @Description   Parameters for defining a hash table
 *		  (Must match struct ioc_fm_pcd_hash_table_params_t defined in
 *		  fm_pcd_ext.h)
 */
struct fm_pcd_hash_table_params_t {
	uint16_t max_num_of_keys;
		/**< Maximum Number Of Keys that will (ever) be used in this
		 * Hash-table
		 */
	ioc_fm_pcd_cc_stats_mode statistics_mode;
		/**< If not e_IOC_FM_PCD_CC_STATS_MODE_NONE, the required
		 * structures for the requested statistics mode will be
		 * allocated according to max_num_of_keys.
		 */
	uint8_t kg_hash_shift;
		/**< KG-Hash-shift as it was configured in the KG-scheme that
		 * leads to this hash-table.
		 */
	uint16_t hash_res_mask;
		/**< Mask that will be used on the hash-result; The
		 * number-of-sets for this hash will be calculated as (2^(number
		 * of bits set in 'hash_res_mask')); The 4 lower bits must be
		 * cleared.
		 */
	uint8_t hash_shift;
		/**< Byte offset from the beginning of the KeyGen hash result to
		 * the 2-bytes to be used as hash index.
		 */
	uint8_t match_key_size;
		/**< Size of the exact match keys held by the hash buckets */

	ioc_fm_pcd_cc_next_engine_params_t cc_next_engine_params_for_miss;
		/**< Parameters for defining the next engine when a key is not
		 * matched
		 */
};

typedef struct ioc_fm_pcd_hash_table_params_t {
	struct fm_pcd_hash_table_params_t param;
	void *id;
} ioc_fm_pcd_hash_table_params_t;

/*
 * @Description   A structure with the arguments for the
 *		  fm_pcd_hash_table_add_key ioctl() call
 */
typedef struct ioc_fm_pcd_hash_table_add_key_params_t {
	void			*p_hash_tbl;
	uint8_t			key_size;
	ioc_fm_pcd_cc_key_params_t  key_params;
} ioc_fm_pcd_hash_table_add_key_params_t;

/*
 * @Description   Parameters for defining a CC tree group.
 *
 *		  This structure defines a CC group in terms of NetEnv units and
 *		  the action to be taken in each case. The unit_ids list must be
 *		  given in order from low to high indices.
 *		  ioc_fm_pcd_cc_next_engine_params_t is a list of
 *		  2^num_of_distinction_units structures where each defines the
 *		  next action to be taken for each units combination. for
 *		  example: num_of_distinction_units = 2 unit_ids = {1,3}
 *		  next_engine_per_entries_in_grp[0] =
 *		  ioc_fm_pcd_cc_next_engine_params_t for the case that unit 1 -
 *		  not found; unit 3 - not found;
 *		  next_engine_per_entries_in_grp[1] =
 *		  ioc_fm_pcd_cc_next_engine_params_t for the case that unit 1 -
 *		  not found; unit 3 - found;
 *		  next_engine_per_entries_in_grp[2] =
 *		  ioc_fm_pcd_cc_next_engine_params_t for the case that unit 1 -
 *		  found; unit 3 - not found;
 *		  next_engine_per_entries_in_grp[3] =
 *		  ioc_fm_pcd_cc_next_engine_params_t for the case that unit 1 -
 *		  found; unit 3 - found;
 */
typedef struct ioc_fm_pcd_cc_grp_params_t {
	uint8_t		num_of_distinction_units;   /**< Up to 4 */
	uint8_t		unit_ids[IOC_FM_PCD_MAX_NUM_OF_CC_UNITS];
		/**< Indexes of the units as defined in
		 * fm_pcd_net_env_characteristics_set()
		 */
	ioc_fm_pcd_cc_next_engine_params_t
		next_engine_per_entries_in_grp[IOC_FM_PCD_MAX_CC_ENTRY_IN_GRP];
		/**< Maximum entries per group is 16 */
} ioc_fm_pcd_cc_grp_params_t;

/*
 * @Description   Parameters for defining the CC tree groups
 *		  (Must match struct ioc_fm_pcd_cc_tree_params_t defined in
 *		  fm_pcd_ext.h)
 */
typedef struct ioc_fm_pcd_cc_tree_params_t {
	void		*net_env_id;
			/**< Id of the Network Environment as returned
			 * by fm_pcd_net_env_characteristics_set()
			 */
	uint8_t		num_of_groups;
			/**< Number of CC groups within the CC tree */
	ioc_fm_pcd_cc_grp_params_t
			fm_pcd_cc_group_params[IOC_FM_PCD_MAX_NUM_OF_CC_GROUPS];
			/**< Parameters for each group. */
	void		*id;
			/**< Output parameter; Returns the tree Id to be used */
} ioc_fm_pcd_cc_tree_params_t;

/*
 * @Description   Parameters for defining policer byte rate
 */
typedef struct ioc_fm_pcd_plcr_byte_rate_mode_param_t {
	ioc_fm_pcd_plcr_frame_length_select	frame_length_selection;
			/**< Frame length selection */
	ioc_fm_pcd_plcr_roll_back_frame_select  roll_back_frame_selection;
			/**< relevant option only e_IOC_FM_PCD_PLCR_L2_FRM_LEN,
			 * e_IOC_FM_PCD_PLCR_FULL_FRM_LEN
			 */
} ioc_fm_pcd_plcr_byte_rate_mode_param_t;

/*
 * @Description   Parameters for defining the policer profile (based on
 *		  RFC-2698 or RFC-4115 attributes).
 */
typedef struct ioc_fm_pcd_plcr_non_passthrough_alg_param_t {
	ioc_fm_pcd_plcr_rate_mode		rate_mode;
			/**< Byte / Packet */
	ioc_fm_pcd_plcr_byte_rate_mode_param_t  byte_mode_param;
			/**< Valid for Byte NULL for Packet */
	uint32_t				committed_info_rate;
			/**< KBits/Sec or Packets/Sec */
	uint32_t				committed_burst_size;
			/**< KBits or Packets */
	uint32_t				peak_or_excess_info_rate;
			/**< KBits/Sec or Packets/Sec */
	uint32_t				peak_or_excess_burst_size;
			/**< KBits or Packets */
} ioc_fm_pcd_plcr_non_passthrough_alg_param_t;

/*
 * @Description   Parameters for defining the next engine after policer
 */
typedef union ioc_fm_pcd_plcr_next_engine_params_u {
	ioc_fm_pcd_done_action	action;
				/**< Action - when next engine is BMI (done) */
	void			*p_profile;
				/**< Policer profile handle -  used when next
				 * engine is PLCR, must be a SHARED profile
				 */
	void			*p_direct_scheme;
				/**< Direct scheme select - when next engine is
				 * Keygen
				 */
} ioc_fm_pcd_plcr_next_engine_params_u;

typedef struct ioc_fm_pcd_port_params_t {
	ioc_fm_port_type			port_type;
				/**< Type of port for this profile */
	uint8_t				port_id;
				/**< FM-Port id of port for this profile */
} ioc_fm_pcd_port_params_t;

/*
 * @Description   Parameters for defining the policer profile entry
 *		  (Must match struct ioc_fm_pcd_plcr_profile_params_t defined in
 *		  fm_pcd_ext.h)
 */
struct fm_pcd_plcr_profile_params_t {
	bool modify;
		/**< TRUE to change an existing profile */
	union {
		struct {
			ioc_fm_pcd_profile_type_selection profile_type;
				/**< Type of policer profile */
			ioc_fm_pcd_port_params_t *p_fm_port;
				/**< Relevant for per-port profiles only */
			uint16_t relative_profile_id;
				/**< Profile id - relative to shared group or to
				 * port
				 */
		} new_params;
			/**< Use it when modify = FALSE */
		void *p_profile;
			/**< A handle to a profile - use it when modify=TRUE */
	} profile_select;
	ioc_fm_pcd_plcr_algorithm_selection alg_selection;
	/**< Profile Algorithm PASS_THROUGH, RFC_2698, RFC_4115 */
	ioc_fm_pcd_plcr_color_mode color_mode;
	/**< COLOR_BLIND, COLOR_AWARE */

	union {
		ioc_fm_pcd_plcr_color dflt_color;
		/**< For Color-Blind Pass-Through mode; the policer will
		 * re-color any incoming packet with the default value.
		 */
		ioc_fm_pcd_plcr_color override;
		/**< For Color-Aware modes; the profile response to a pre-color
		 * value of 2'b11.
		 */
	} color;

	ioc_fm_pcd_plcr_non_passthrough_alg_param_t
		non_passthrough_alg_param;
		/**< RFC2698 or RFC4115 parameters */

	ioc_fm_pcd_engine next_engine_on_green;
		/**< Next engine for green-colored frames */
	ioc_fm_pcd_plcr_next_engine_params_u params_on_green;
		/**< Next engine parameters for green-colored frames */

	ioc_fm_pcd_engine next_engine_on_yellow;
		/**< Next engine for yellow-colored frames */
	ioc_fm_pcd_plcr_next_engine_params_u params_on_yellow;
		/**< Next engine parameters for yellow-colored frames */

	ioc_fm_pcd_engine next_engine_on_red;
		/**< Next engine for red-colored frames */
	ioc_fm_pcd_plcr_next_engine_params_u params_on_red;
		/**< Next engine parameters for red-colored frames */

	bool trap_profile_on_flow_A;
		/**< Obsolete - do not use */
	bool trap_profile_on_flow_B;
		/**< Obsolete - do not use */
	bool trap_profile_on_flow_C;
		/**< Obsolete - do not use */
};

typedef struct ioc_fm_pcd_plcr_profile_params_t {
	struct fm_pcd_plcr_profile_params_t param;
	void	*id;
		/**< output parameter; Returns the profile Id to be used */
} ioc_fm_pcd_plcr_profile_params_t;

/*
 * @Description   A structure for modifying CC tree next engine
 */
typedef struct ioc_fm_pcd_cc_tree_modify_next_engine_params_t {
	void				*id;
			/**< CC tree Id to be used */
	uint8_t				grp_indx;
			/**< A Group index in the tree */
	uint8_t				indx;
			/**< Entry index in the group defined by grp_index */
	ioc_fm_pcd_cc_next_engine_params_t  cc_next_engine_params;
			/**< Parameters for the next for the defined Key in the
			 * p_key
			 */
} ioc_fm_pcd_cc_tree_modify_next_engine_params_t;

/*
 * @Description   A structure for modifying CC node next engine
 */
typedef struct ioc_fm_pcd_cc_node_modify_next_engine_params_t {
	void				*id;
			/**< CC node Id to be used */
	uint16_t				key_indx;
			/**< Key index for Next Engine Params modifications;
			 * NOTE: This parameter is IGNORED for miss-key!
			 */
	uint8_t				key_size;
			/**< Key size of added key */
	ioc_fm_pcd_cc_next_engine_params_t  cc_next_engine_params;
			/**< parameters for the next for the defined Key in the
			 * p_key
			 */
} ioc_fm_pcd_cc_node_modify_next_engine_params_t;

/*
 * @Description   A structure for remove CC node key
 */
typedef struct ioc_fm_pcd_cc_node_remove_key_params_t {
	void				*id;
			/**< CC node Id to be used */
	uint16_t				key_indx;
			/**< Key index for Next Engine Params modifications;
			 * NOTE: This parameter is IGNORED for miss-key!
			 */
} ioc_fm_pcd_cc_node_remove_key_params_t;

/*
 * @Description   A structure for modifying CC node key and next engine
 */
typedef struct ioc_fm_pcd_cc_node_modify_key_and_next_engine_params_t {
	void				*id;
			/**< CC node Id to be used */
	uint16_t				key_indx;
			/**< Key index for Next Engine Params modifications;
			 * NOTE: This parameter is IGNORED for miss-key!
			 */
	uint8_t				key_size;
			/**< Key size of added key */
	ioc_fm_pcd_cc_key_params_t	key_params;
			/**< it's array with num_of_keys entries each entry in
			 * the array of the type ioc_fm_pcd_cc_key_params_t
			 */
} ioc_fm_pcd_cc_node_modify_key_and_next_engine_params_t;

/*
 * @Description   A structure for modifying CC node key
 */
typedef struct ioc_fm_pcd_cc_node_modify_key_params_t {
	void				*id;
			/**< CC node Id to be used */
	uint16_t				key_indx;
			/**< Key index for Next Engine Params modifications;
			 * NOTE: This parameter is IGNORED for miss-key!
			 */
	uint8_t				key_size;
			/**< Key size of added key */
	uint8_t				*p_key;
			/**< Pointer to the key of the size defined in key_size
			 */
	uint8_t				*p_mask;
			/**< Pointer to the Mask per key of the size defined in
			 * key_size. p_key and p_mask (if defined) have to be of
			 * the same size as defined in the key_size
			 */
} ioc_fm_pcd_cc_node_modify_key_params_t;

/*
 * @Description   A structure with the arguments for the
 *		  fm_pcd_hash_table_remove_key ioctl() call
 */
typedef struct ioc_fm_pcd_hash_table_remove_key_params_t {
	void	*p_hash_tbl;	/**< The id of the hash table */
	uint8_t	key_size;	/**< The size of the key to remove */
	uint8_t	*p_key;		/**< Pointer to the key to remove */
} ioc_fm_pcd_hash_table_remove_key_params_t;

/*
 * @Description   Parameters for selecting a location for requested manipulation
 */
typedef struct ioc_fm_manip_hdr_info_t {
	ioc_net_header_type		hdr;		/**< Header selection */
	ioc_fm_pcd_hdr_index		hdr_index;
			/**< Relevant only for MPLS, VLAN and tunneled IP.
			 * Otherwise should be cleared.
			 */
	bool				by_field;
			/**< TRUE if the location of manipulation is according
			 * to some field in the specific header
			 */
	ioc_fm_pcd_fields_u		full_field;
			/**< Relevant only when by_field = TRUE: Extract field
			 */
} ioc_fm_manip_hdr_info_t;

/*
 * @Description   Parameters for defining header removal by header type
 */
typedef struct ioc_fm_pcd_manip_hdr_rmv_by_hdr_params_t {
	ioc_fm_pcd_manip_hdr_rmv_by_hdr_type	type;
			/**< Selection of header removal location */
	union {
	ioc_fm_manip_hdr_info_t		hdr_info;
		/**< Relevant when type = e_FM_PCD_MANIP_RMV_BY_HDR_FROM_START
		 */
	ioc_fm_pcd_manip_hdr_rmv_specific_l2	specific_l2;
		/**< Relevant when type = e_IOC_FM_PCD_MANIP_BY_HDR_SPECIFIC_L2;
		 * Defines which L2 headers to remove.
		 */
	} u;
} ioc_fm_pcd_manip_hdr_rmv_by_hdr_params_t;

/*
 * @Description   Parameters for configuring IP fragmentation manipulation
 */
typedef struct ioc_fm_pcd_manip_frag_ip_params_t {
	uint16_t			size_for_fragmentation;
		/**< If length of the frame is greater than this value, IP
		 * fragmentation will be executed.
		 */
	bool			sg_bpid_en;
		/**< Enable a dedicated buffer pool id for the Scatter/Gather
		 * buffer allocation; If disabled, the Scatter/Gather buffer
		 * will be allocated from the same pool as the received frame's
		 * buffer.
		 */
	uint8_t			sg_bpid;
		/**< Scatter/Gather buffer pool id; This parameter is relevant
		 * when 'sg_bpid_en=TRUE'; Same LIODN number is used for these
		 * buffers as for the received frames buffers, so buffers of
		 * this pool need to be allocated in the same memory area as the
		 * received buffers. If the received buffers arrive from
		 * different sources, the Scatter/Gather BP id should be mutual
		 * to all these sources.
		 */
	ioc_fm_pcd_manip_donot_frag_action  donot_frag_action;
		/**< Don't Fragment Action - If an IP packet is larger than MTU
		 * and its DF bit is set, then this field will determine the
		 * action to be taken.
		 */
} ioc_fm_pcd_manip_frag_ip_params_t;

/*
 * @Description   Parameters for configuring IP reassembly manipulation.
 *
 *		  This is a common structure for both IPv4 and IPv6 reassembly
 *		  manipulation. For reassembly of both IPv4 and IPv6, make sure
 *		  to set the 'hdr' field in ioc_fm_pcd_manip_reassem_params_t to
 *		  IOC_header_type_ipv_6.
 */
typedef struct ioc_fm_pcd_manip_reassem_ip_params_t {
	uint8_t			relative_scheme_id[2];
			/**< Partition relative scheme id: relativeSchemeId[0] -
			 * Relative scheme ID for IPV4 Reassembly manipulation;
			 * relativeSchemeId[1] -  Relative scheme ID for IPV6
			 * Reassembly manipulation; NOTE: The following comment
			 * is relevant only for FMAN v2 devices: Relative scheme
			 * ID for IPv4/IPv6 Reassembly manipulation must be
			 * smaller than the user schemes id to ensure that the
			 * reassembly's schemes will be first match. The
			 * remaining schemes, if defined, should have higher
			 * relative scheme ID.
			 */
	uint32_t			non_consistent_sp_fqid;
			/**< In case that other fragments of the frame
			 * corresponds to different storage profile than the
			 * opening fragment (Non-Consistent-SP state) then one
			 * of two possible scenarios occurs: if
			 * 'nonConsistentSpFqid != 0', the reassembled frame
			 * will be enqueued to this fqid, otherwise a 'Non
			 * Consistent SP' bit will be set in the FD[status].
			 */
	uint8_t				data_mem_id;
			/**< Memory partition ID for the IPR's external tables
			 * structure
			 */
	uint16_t			data_liodn_offset;
			/**< LIODN offset for access the IPR's external tables
			 * structure.
			 */
	uint16_t			min_frag_size[2];
			/**< Minimum fragment size: minFragSize[0] - for ipv4,
			 * minFragSize[1] - for ipv6
			 */
	ioc_fm_pcd_manip_reassem_ways_number   num_of_frames_per_hash_entry[2];
			/**< Number of frames per hash entry needed for
			 * reassembly process: num_of_frames_per_hash_entry[0] -
			 * for ipv4 (max value is
			 * e_IOC_FM_PCD_MANIP_EIGHT_WAYS_HASH);
			 * num_of_frames_per_hash_entry[1] - for ipv6 (max value
			 * is e_IOC_FM_PCD_MANIP_SIX_WAYS_HASH).
			 */
	uint16_t			max_num_frames_in_process;
			/**< Number of frames which can be processed by
			 * Reassembly in the same time; Must be power of 2; In
			 * the case num_of_frames_per_hash_entry ==
			 * e_IOC_FM_PCD_MANIP_FOUR_WAYS_HASH,
			 * max_num_frames_in_process has to be in the range of
			 * 4 - 512; In the case num_of_frames_per_hash_entry ==
			 * e_IOC_FM_PCD_MANIP_EIGHT_WAYS_HASH,
			 * max_num_frames_in_process has to be in the range of
			 * 8 - 2048.
			 */
	ioc_fm_pcd_manip_reassem_time_out_mode  time_out_mode;
			/**< Expiration delay initialized by Reassembly process
			 */
	uint32_t			fqid_for_time_out_frames;
			/**< FQID in which time out frames will enqueue during
			 * Time Out Process
			 */
	uint32_t			timeout_threshold_for_reassm_process;
			/**< Represents the time interval in microseconds which
			 * defines if opened frame (at least one fragment was
			 * processed but not all the fragments)is found as too
			 * old
			 */
} ioc_fm_pcd_manip_reassem_ip_params_t;

/*
 * @Description   Parameters for defining IPSEC manipulation
 */
typedef struct ioc_fm_pcd_manip_special_offload_ipsec_params_t {
	bool	decryption;
			/**< TRUE if being used in decryption direction;
			 * FALSE if being used in encryption direction.
			 */
	bool	ecn_copy;
			/**< TRUE to copy the ECN bits from inner/outer to
			 * outer/inner (direction depends on the 'decryption'
			 * field).
			 */
	bool	dscp_copy;
			/**< TRUE to copy the DSCP bits from inner/outer to
			 * outer/inner (direction depends on the 'decryption'
			 * field).
			 */
	bool	variable_ip_hdr_len;
			/**< TRUE for supporting variable IP header length in
			 * decryption.
			 */
	bool	variable_ip_version;
			/**< TRUE for supporting both IP version on the same SA
			 * in encryption
			 */
	uint8_t outer_ip_hdr_len;
			/**< If 'variable_ip_version == TRUE' than this field
			 * must be set to non-zero value; It is specifies the
			 * length of the outer IP header that was configured in
			 * the corresponding SA.
			 */
	uint16_t	arw_size;
			/**< if <> '0' then will perform ARW check for this SA;
			 * The value must be a multiplication of 16
			 */
	void	*arw_addr;
			/**< if arwSize <> '0' then this field must be set to
			 * non-zero value; MUST be allocated from FMAN's MURAM
			 * that the post-sec op-port belong Must be 4B aligned.
			 * Required MURAM size is
			 * '(NEXT_POWER_OF_2(arwSize+32))/8+4' Bytes
			 */
} ioc_fm_pcd_manip_special_offload_ipsec_params_t;

/*
 * @Description   Parameters for configuring CAPWAP fragmentation manipulation
 *
 *		  Restrictions:
 *		  - Maximum number of fragments per frame is 16.
 *		  - Transmit confirmation is not supported.
 *		  - Fragmentation nodes must be set as the last PCD action (i.e.
 *		    the corresponding CC node key must have next engine set to
 *		    e_FM_PCD_DONE).
 *		  - Only BMan buffers shall be used for frames to be fragmented.
 *		  - NOTE: The following comment is relevant only for FMAN v3
 *		    devices: IPF does not support VSP. Therefore, on the same
 *		    port where we have IPF we cannot support VSP.
 */
typedef struct ioc_fm_pcd_manip_frag_capwap_params_t {
	uint16_t	size_for_fragmentation;
			/**< If length of the frame is greater than this value,
			 * CAPWAP fragmentation will be executed.
			 */
	bool		sg_bpid_en;
			/**< Enable a dedicated buffer pool id for the
			 * Scatter/Gather buffer allocation; If disabled, the
			 * Scatter/Gather buffer will be allocated from the same
			 * pool as the received frame's buffer.
			 */
	uint8_t		sg_bpid;
			/**< Scatter/Gather buffer pool id; This parameters is
			 * relevant when 'sg_bpidEn=TRUE'; Same LIODN number is
			 * used for these buffers as for the received frames
			 * buffers, so buffers of this pool need to be allocated
			 * in the same memory area as the received buffers. If
			 * the received buffers arrive from different sources,
			 * the Scatter/Gather BP id should be mutual to all
			 * these sources.
			 */
	bool	compress_mode_en;
			/**< CAPWAP Header Options Compress Enable mode; When
			 * this mode is enabled then only the first fragment
			 * include the CAPWAP header options field (if user
			 * provides it in the input frame) and all other
			 * fragments exclude the CAPWAP options field (CAPWAP
			 * header is updated accordingly).
			 */
} ioc_fm_pcd_manip_frag_capwap_params_t;

/*
 * @Description   Parameters for configuring CAPWAP reassembly manipulation.
 *
 *		  Restrictions:
 *		  - Application must define one scheme to catch the reassembled
 *		    frames.
 *		  - Maximum number of fragments per frame is 16.
 */
typedef struct ioc_fm_pcd_manip_reassem_capwap_params_t {
	uint8_t		relative_scheme_id;
			/**< Partition relative scheme id; NOTE: this id must be
			 * smaller than the user schemes id to ensure that the
			 * reassembly scheme will be first match; Rest schemes,
			 * if defined, should have higher relative scheme ID.
			 */
	uint8_t		data_mem_id;
			/**< Memory partition ID for the IPR's external tables
			 * structure
			 */
	uint16_t	data_liodn_offset;
			/**< LIODN offset for access the IPR's external tables
			 * structure.
			 */
	uint16_t	max_reassembled_frame_length;
			/**< The maximum CAPWAP reassembled frame length in
			 * bytes; If maxReassembledFrameLength == 0, any
			 * successful reassembled frame length is considered as
			 * a valid length; if maxReassembledFrameLength > 0, a
			 * successful reassembled frame which its length exceeds
			 * this value is considered as an error frame (FD
			 * status[CRE] bit is set).
			 */
	ioc_fm_pcd_manip_reassem_ways_number   num_of_frames_per_hash_entry;
			/**< Number of frames per hash entry needed for
			 * reassembly process
			 */
	uint16_t	max_num_frames_in_process;
			/**< Number of frames which can be processed by
			 * reassembly in the same time; Must be power of 2; In
			 * the case num_of_frames_per_hash_entry ==
			 * e_FM_PCD_MANIP_FOUR_WAYS_HASH,
			 * max_num_frames_in_process has to be in the range of
			 * 4 - 512; In the case num_of_frames_per_hash_entry ==
			 * e_FM_PCD_MANIP_EIGHT_WAYS_HASH,
			 * max_num_frames_in_process has to be in the range of
			 * 8 - 2048.
			 */
	ioc_fm_pcd_manip_reassem_time_out_mode  time_out_mode;
			/**< Expiration delay initialized by Reassembly process
			 */
	uint32_t	fqid_for_time_out_frames;
			/**< FQID in which time out frames will enqueue during
			 * Time Out Process; Recommended value for this field is
			 * 0; in this way timed-out frames will be discarded
			 */
	uint32_t	timeout_threshold_for_reassm_process;
			/**< Represents the time interval in microseconds which
			 * defines if opened frame (at least one fragment was
			 * processed but not all the fragments)is found as too
			 * old
			 */
} ioc_fm_pcd_manip_reassem_capwap_params_t;

/*
 * @Description   structure for defining CAPWAP manipulation
 */
typedef struct ioc_fm_pcd_manip_special_offload_capwap_params_t {
	bool			dtls;
			/**< TRUE if continue to SEC DTLS encryption */
	ioc_fm_pcd_manip_hdr_qos_src   qos_src;
			/**< TODO */
} ioc_fm_pcd_manip_special_offload_capwap_params_t;

/*
 * @Description   Parameters for defining special offload manipulation
 */
typedef struct ioc_fm_pcd_manip_special_offload_params_t {
	ioc_fm_pcd_manip_special_offload_type		type;
		/**< Type of special offload manipulation */
	union {
	ioc_fm_pcd_manip_special_offload_ipsec_params_t ipsec;
		/**< Parameters for IPSec; Relevant when type =
		 * e_IOC_FM_PCD_MANIP_SPECIAL_OFFLOAD_IPSEC
		 */

	ioc_fm_pcd_manip_special_offload_capwap_params_t capwap;
		/**< Parameters for CAPWAP; Relevant when type =
		 * e_FM_PCD_MANIP_SPECIAL_OFFLOAD_CAPWAP
		 */
	} u;
} ioc_fm_pcd_manip_special_offload_params_t;

/*
 * @Description   Parameters for defining generic removal manipulation
 */
typedef struct ioc_fm_pcd_manip_hdr_rmv_generic_params_t {
	uint8_t			offset;
		/**< Offset from beginning of header to the start location of
		 * the removal
		 */
	uint8_t			size;	/**< Size of removed section */
} ioc_fm_pcd_manip_hdr_rmv_generic_params_t;

/*
 * @Description   Parameters for defining insertion manipulation
 */
typedef struct ioc_fm_pcd_manip_hdr_insrt_t {
	uint8_t size;		/**< size of inserted section */
	uint8_t *p_data;	/**< data to be inserted */
} ioc_fm_pcd_manip_hdr_insrt_t;

/*
 * @Description   Parameters for defining generic insertion manipulation
 */
typedef struct ioc_fm_pcd_manip_hdr_insrt_generic_params_t {
	uint8_t			offset;
			/**< Offset from beginning of header to the start
			 * location of the insertion
			 */
	uint8_t			size;	/**< Size of inserted section */
	bool			replace;
			/**< TRUE to override (replace) existing data at
			 * 'offset', FALSE to insert
			 */
	uint8_t			*p_data;
			/**< Pointer to data to be inserted */
} ioc_fm_pcd_manip_hdr_insrt_generic_params_t;

/*
 * @Description   Parameters for defining header manipulation VLAN DSCP To Vpri
 *		  translation
 */
typedef struct ioc_fm_pcd_manip_hdr_field_update_vlan_dscp_to_vpri_t {
	uint8_t		dscp_to_vpri_table[IOC_FM_PCD_MANIP_DSCP_TO_VLAN_TRANS];
		/**< A table of VPri values for each DSCP value; The index is
		 * the D_SCP value (0-0x3F) and the value is the corresponding
		 * VPRI (0-15).
		 */
	uint8_t		vpri_def_val;
		/**< 0-7, Relevant only if update_type =
		 * e_IOC_FM_PCD_MANIP_HDR_FIELD_UPDATE_DSCP_TO_VLAN, this field
		 * is the Q Tag default value if the IP header is not found.
		 */
} ioc_fm_pcd_manip_hdr_field_update_vlan_dscp_to_vpri_t;

/*
 * @Description   Parameters for defining header manipulation VLAN fields
 *		  updates
 */
typedef struct ioc_fm_pcd_manip_hdr_field_update_vlan_t {
	ioc_fm_pcd_manip_hdr_field_update_vlan  update_type;
		/**< Selects VLAN update type */
	union {
	uint8_t					vpri;
		/**< 0-7, Relevant only if If update_type =
		 * e_IOC_FM_PCD_MANIP_HDR_FIELD_UPDATE_VLAN_PRI, this is the new
		 * VLAN pri.
		 */
	ioc_fm_pcd_manip_hdr_field_update_vlan_dscp_to_vpri_t	dscp_to_vpri;
		/**<  Parameters structure, Relevant only if update_type =
		 * e_IOC_FM_PCD_MANIP_HDR_FIELD_UPDATE_DSCP_TO_VLAN.
		 */
	} u;
} ioc_fm_pcd_manip_hdr_field_update_vlan_t;

/*
 * @Description   Parameters for defining header manipulation IPV4 fields
 *		  updates
 */
typedef struct ioc_fm_pcd_manip_hdr_field_update_ipv4_t {
	ioc_ipv4_hdr_manip_update_flags_t	valid_updates;
			/**< ORed flag, selecting the required updates */
	uint8_t		tos;
			/**< 8 bit New TOS; Relevant if valid_updates contains
			 * IOC_HDR_MANIP_IPV4_TOS
			 */
	uint16_t	id;
			/**< 16 bit New IP ID; Relevant only if
			 * valid_updates contains IOC_HDR_MANIP_IPV4_ID
			 */
	uint32_t	src;
			/**< 32 bit New IP SRC; Relevant only if
			 * valid_updates contains IOC_HDR_MANIP_IPV4_SRC
			 */
	uint32_t	dst;
			/**< 32 bit New IP DST; Relevant only if
			 * valid_updates contains IOC_HDR_MANIP_IPV4_DST
			 */
} ioc_fm_pcd_manip_hdr_field_update_ipv4_t;

/*
 * @Description   Parameters for defining header manipulation IPV6 fields
 *		  updates
 */
typedef struct ioc_fm_pcd_manip_hdr_field_update_ipv6_t {
	ioc_ipv6_hdr_manip_update_flags_t	valid_updates;
			/**< ORed flag, selecting the required updates */
	uint8_t		traffic_class;
			/**< 8 bit New Traffic Class; Relevant if valid_updates
			 * contains IOC_HDR_MANIP_IPV6_TC
			 */
	uint8_t		src[ioc_net_hf_ipv6_addr_size];
			/**< 16 byte new IP SRC; Relevant only if valid_updates
			 * contains IOC_HDR_MANIP_IPV6_SRC
			 */
	uint8_t		dst[ioc_net_hf_ipv6_addr_size];
			/**< 16 byte new IP DST; Relevant only if valid_updates
			 * contains IOC_HDR_MANIP_IPV6_DST
			 */
} ioc_fm_pcd_manip_hdr_field_update_ipv6_t;

/*
 * @Description   Parameters for defining header manipulation TCP/UDP fields
 *		  updates
 */
typedef struct ioc_fm_pcd_manip_hdr_field_update_tcp_udp_t {
	ioc_tcp_udp_hdr_manip_update_flags_t	valid_updates;
			/**< ORed flag, selecting the required updates */
	uint16_t	src;
			/**< 16 bit New TCP/UDP SRC; Relevant only if
			 * valid_updates contains IOC_HDR_MANIP_TCP_UDP_SRC
			 */
	uint16_t	dst;
			/**< 16 bit New TCP/UDP DST; Relevant only if
			 * valid_updates contains IOC_HDR_MANIP_TCP_UDP_DST
			 */
} ioc_fm_pcd_manip_hdr_field_update_tcp_udp_t;

/*
 * @Description   Parameters for defining header manipulation fields updates
 */
typedef struct ioc_fm_pcd_manip_hdr_field_update_params_t {
	ioc_fm_pcd_manip_hdr_field_update_type	type;
			/**< Type of header field update manipulation */
	union {
	ioc_fm_pcd_manip_hdr_field_update_vlan_t	vlan;
			/**< Parameters for VLAN update. Relevant when type =
			 * e_IOC_FM_PCD_MANIP_HDR_FIELD_UPDATE_VLAN
			 */
	ioc_fm_pcd_manip_hdr_field_update_ipv4_t	ipv4;
			/**< Parameters for IPv4 update. Relevant when type =
			 * e_IOC_FM_PCD_MANIP_HDR_FIELD_UPDATE_IPV4
			 */
	ioc_fm_pcd_manip_hdr_field_update_ipv6_t	ipv6;
			/**< Parameters for IPv6 update. Relevant when type =
			 * e_IOC_FM_PCD_MANIP_HDR_FIELD_UPDATE_IPV6
			 */
	ioc_fm_pcd_manip_hdr_field_update_tcp_udp_t tcp_udp;
			/**< Parameters for TCP/UDP update. Relevant when type =
			 * e_IOC_FM_PCD_MANIP_HDR_FIELD_UPDATE_TCP_UDP
			 */
	} u;
} ioc_fm_pcd_manip_hdr_field_update_params_t;

/*
 * @Description   Parameters for defining custom header manipulation for IP
 *		  replacement
 */
typedef struct ioc_fm_pcd_manip_hdr_custom_ip_hdr_replace_t {
	ioc_fm_pcd_manip_hdr_custom_ip_replace  replace_type;
			/**< Selects replace update type */
	bool	dec_ttl_hl;
			/**< Decrement TTL (IPV4) or Hop limit (IPV6) by 1 */
	bool	update_ipv4_id;
			/**< Relevant when replace_type =
			 * e_IOC_FM_PCD_MANIP_HDR_CUSTOM_REPLACE_IPV6_BY_IPV4
			 */
	uint16_t id;
		/**< 16 bit New IP ID; Relevant only if update_ipv4_id = TRUE */
	uint8_t	hdr_size;
			/**< The size of the new IP header */
	uint8_t	hdr[IOC_FM_PCD_MANIP_MAX_HDR_SIZE];
			/**< The new IP header */
} ioc_fm_pcd_manip_hdr_custom_ip_hdr_replace_t;

/*
 * @Description   Parameters for defining custom header manipulation
 */
typedef struct ioc_fm_pcd_manip_hdr_custom_params_t {
	ioc_fm_pcd_manip_hdr_custom_type		type;
			/**< Type of header field update manipulation */
	union {
	ioc_fm_pcd_manip_hdr_custom_ip_hdr_replace_t	ip_hdr_replace;
			/**< Parameters IP header replacement */
	} u;
} ioc_fm_pcd_manip_hdr_custom_params_t;

/*
 * @Description   Parameters for defining specific L2 insertion manipulation
 */
typedef struct ioc_fm_pcd_manip_hdr_insrt_specific_l2_params_t {
	ioc_fm_pcd_manip_hdr_insrt_specific_l2  specific_l2;
			/**< Selects which L2 headers to insert */
	bool					update;
			/**< TRUE to update MPLS header */
	uint8_t				size;
			/**< size of inserted section */
	uint8_t				*p_data;
			/**< data to be inserted */
} ioc_fm_pcd_manip_hdr_insrt_specific_l2_params_t;

/*
 * @Description   Parameters for defining IP insertion manipulation
 */
typedef struct ioc_fm_pcd_manip_hdr_insrt_ip_params_t {
	bool	calc_l4_checksum;
			/**< Calculate L4 checksum. */
	ioc_fm_pcd_manip_hdr_qos_mapping_mode   mapping_mode;
			/**< TODO */
	uint8_t last_pid_offset;
			/**< the offset of the last Protocol within the inserted
			 * header
			 */
	uint16_t  id;	/**< 16 bit New IP ID */
	bool	donot_frag_overwrite;
			/**< IPv4 only. DF is overwritten with the hash-result
			 * next-to-last byte. This byte is configured to be
			 * overwritten when RPD is set.
			 */
	uint8_t	last_dst_offset;
			/**< IPv6 only. if routing extension exist, user should
			 * set the offset of the destination address in order
			 * to calculate UDP checksum pseudo header; Otherwise
			 * set it to '0'.
			 */
	ioc_fm_pcd_manip_hdr_insrt_t insrt;
			/**< size and data to be inserted. */
} ioc_fm_pcd_manip_hdr_insrt_ip_params_t;

/*
 * @Description   Parameters for defining header insertion manipulation by
 *		  header type
 */
typedef struct ioc_fm_pcd_manip_hdr_insrt_by_hdr_params_t {
	ioc_fm_pcd_manip_hdr_insrt_by_hdr_type	type;
			/**< Selects manipulation type */
	union {
	ioc_fm_pcd_manip_hdr_insrt_specific_l2_params_t  specific_l2_params;
			/**< Used when type =
			 * e_IOC_FM_PCD_MANIP_INSRT_BY_HDR_SPECIFIC_L2: Selects
			 * which L2 headers to remove
			 */
	ioc_fm_pcd_manip_hdr_insrt_ip_params_t	ip_params;
			/**< Used when type = e_FM_PCD_MANIP_INSRT_BY_HDR_IP */
	ioc_fm_pcd_manip_hdr_insrt_t		insrt;
			/**< Used when type is one of
			 * e_FM_PCD_MANIP_INSRT_BY_HDR_UDP,
			 * e_FM_PCD_MANIP_INSRT_BY_HDR_UDP_LITE, or
			 * e_FM_PCD_MANIP_INSRT_BY_HDR_CAPWAP
			 */
	} u;
} ioc_fm_pcd_manip_hdr_insrt_by_hdr_params_t;

/*
 * @Description   Parameters for defining header insertion manipulation
 */
typedef struct ioc_fm_pcd_manip_hdr_insrt_params_t {
	ioc_fm_pcd_manip_hdr_insrt_type			type;
			/**< Type of insertion manipulation */
	union {
	ioc_fm_pcd_manip_hdr_insrt_by_hdr_params_t	by_hdr;
			/**< Parameters for defining header insertion
			 * manipulation by header type, relevant if 'type' =
			 * e_IOC_FM_PCD_MANIP_INSRT_BY_HDR
			 */
	ioc_fm_pcd_manip_hdr_insrt_generic_params_t	generic;
			/**< Parameters for defining generic header insertion
			 * manipulation, relevant if type =
			 * e_IOC_FM_PCD_MANIP_INSRT_GENERIC
			 */
	} u;
} ioc_fm_pcd_manip_hdr_insrt_params_t;

/*
 * @Description   Parameters for defining header removal manipulation
 */
typedef struct ioc_fm_pcd_manip_hdr_rmv_params_t {
	ioc_fm_pcd_manip_hdr_rmv_type		type;
			/**< Type of header removal manipulation */
	union {
	ioc_fm_pcd_manip_hdr_rmv_by_hdr_params_t   by_hdr;
			/**< Parameters for defining header removal manipulation
			 * by header type, relevant if type =
			 * e_IOC_FM_PCD_MANIP_RMV_BY_HDR
			 */
	ioc_fm_pcd_manip_hdr_rmv_generic_params_t  generic;
			/**< Parameters for defining generic header removal
			 * manipulation, relevant if type =
			 * e_IOC_FM_PCD_MANIP_RMV_GENERIC
			 */
	} u;
} ioc_fm_pcd_manip_hdr_rmv_params_t;

/*
 * @Description   Parameters for defining header manipulation node
 */
typedef struct ioc_fm_pcd_manip_hdr_params_t {
	bool					rmv;
			/**< TRUE, to define removal manipulation */
	ioc_fm_pcd_manip_hdr_rmv_params_t	rmv_params;
			/**< Parameters for removal manipulation, relevant if
			 * 'rmv' = TRUE
			 */

	bool					insrt;
			/**< TRUE, to define insertion manipulation */
	ioc_fm_pcd_manip_hdr_insrt_params_t	insrt_params;
			/**< Parameters for insertion manipulation, relevant if
			 * 'insrt' = TRUE
			 */

	bool					field_update;
			/**< TRUE, to define field update manipulation */
	ioc_fm_pcd_manip_hdr_field_update_params_t  field_update_params;
			/**< Parameters for field update manipulation, relevant
			 * if 'fieldUpdate' = TRUE
			 */

	bool					custom;
			/**< TRUE, to define custom manipulation */
	ioc_fm_pcd_manip_hdr_custom_params_t	custom_params;
			/**< Parameters for custom manipulation, relevant if
			 * 'custom' = TRUE
			 */

	bool				donot_parse_after_manip;
			/**< FALSE to activate the parser a second time after
			 * completing the manipulation on the frame
			 */
} ioc_fm_pcd_manip_hdr_params_t;

/*
 * @Description   structure for defining fragmentation manipulation
 */
typedef struct ioc_fm_pcd_manip_frag_params_t {
	ioc_net_header_type			hdr;
			/**< Header selection */
	union {
	ioc_fm_pcd_manip_frag_capwap_params_t	capwap_frag;
			/**< Parameters for defining CAPWAP fragmentation,
			 * relevant if 'hdr' = HEADER_TYPE_CAPWAP
			 */
	ioc_fm_pcd_manip_frag_ip_params_t   ip_frag;
			/**< Parameters for defining IP fragmentation, relevant
			 * if 'hdr' = HEADER_TYPE_Ipv4 or HEADER_TYPE_Ipv6
			 */
	} u;
} ioc_fm_pcd_manip_frag_params_t;

/*
 * @Description   structure for defining reassemble manipulation
 */
typedef struct ioc_fm_pcd_manip_reassem_params_t {
	ioc_net_header_type			hdr;
			/**< Header selection */
	union {
	ioc_fm_pcd_manip_reassem_capwap_params_t capwap_reassem;
			/**< Parameters for defining CAPWAP reassembly, relevant
			 * if 'hdr' = HEADER_TYPE_CAPWAP
			 */
	ioc_fm_pcd_manip_reassem_ip_params_t	ip_reassem;
			/**< Parameters for defining IP reassembly, relevant if
			 * 'hdr' = HEADER_TYPE_Ipv4 or HEADER_TYPE_Ipv6
			 */
	} u;
} ioc_fm_pcd_manip_reassem_params_t;

/*
 * @Description   Parameters for defining a manipulation node
 */
struct fm_pcd_manip_params_t {
	ioc_fm_pcd_manip_type type;
		/**< Selects type of manipulation node */
	union {
		ioc_fm_pcd_manip_hdr_params_t hdr;
			/**< Parameters for defining header manipulation node */
		ioc_fm_pcd_manip_reassem_params_t reassem;
			/**< Parameters for defining reassembly manipulation
			 * node
			 */
		ioc_fm_pcd_manip_frag_params_t frag;
			/**< Parameters for defining fragmentation manipulation
			 * node
			 */
		ioc_fm_pcd_manip_special_offload_params_t special_offload;
			/**< Parameters for defining special offload
			 * manipulation node
			 */
	} u;
	void *p_next_manip;
		/**< Handle to another (previously defined) manipulation node;
		 * Allows concatenation of manipulation actions. This parameter
		 * is optional and may be NULL.
		 */
};

typedef struct ioc_fm_pcd_manip_params_t {
	struct fm_pcd_manip_params_t param;
	void *id;
} ioc_fm_pcd_manip_params_t;

/*
 * @Description   Structure for retrieving IP reassembly statistics
 */
typedef struct ioc_fm_pcd_manip_reassem_ip_stats_t {
	/* common counters for both IPv4 and IPv6 */
	uint32_t	timeout;
		/**< Counts the number of TimeOut occurrences */
	uint32_t	rfd_pool_busy;
		/**< Counts the number of failed attempts to allocate a
		 * Reassembly Frame Descriptor
		 */
	uint32_t	internal_buffer_busy;
		/**< Counts the number of times an internal buffer busy occurred
		 */
	uint32_t	external_buffer_busy;
		/**< Counts the number of times external buffer busy occurred */
	uint32_t	sg_fragments;
		/**< Counts the number of Scatter/Gather fragments */
	uint32_t	dma_semaphore_depletion;
		/**< Counts the number of failed attempts to allocate a DMA
		 * semaphore
		 */
	uint32_t	non_consistent_sp;
		/**< Counts the number of Non Consistent Storage Profile events
		 * for successfully reassembled frames
		 */
struct {
	uint32_t	successfully_reassembled;
		/**< Counts the number of successfully reassembled frames */
	uint32_t	valid_fragments;
		/**< Counts the total number of valid fragments that have been
		 * processed for all frames
		 */
	uint32_t	processed_fragments;
		/**< Counts the number of processed fragments (valid and error
		 * fragments) for all frames
		 */
	uint32_t	malformed_fragments;
		/**< Counts the number of malformed fragments processed for all
		 * frames
		 */
	uint32_t	discarded_fragments;
		/**< Counts the number of fragments discarded by the reassembly
		 * process
		 */
	uint32_t	auto_learn_busy;
		/**< Counts the number of times a busy condition occurs when
		 * attempting to access an IP-Reassembly Automatic Learning Hash
		 * set
		 */
	uint32_t	more_than16fragments;
		/**< Counts the fragment occurrences in which the number of
		 * fragments-per-frame exceeds 16
		 */
	} specific_hdr_statistics[2];
		/**< slot '0' is for IPv4, slot '1' is for IPv6 */
} ioc_fm_pcd_manip_reassem_ip_stats_t;

/*
 * @Description   Structure for retrieving IP fragmentation statistics
 */
typedef struct ioc_fm_pcd_manip_frag_ip_stats_t {
	uint32_t	total_frames;
			/**< Number of frames that passed through the
			 * manipulation node
			 */
	uint32_t	fragmented_frames;
			/**< Number of frames that were fragmented */
	uint32_t	generated_fragments;
			/**< Number of fragments that were generated */
} ioc_fm_pcd_manip_frag_ip_stats_t;

/*
 * @Description   Structure for retrieving CAPWAP reassembly statistics
 */
typedef struct ioc_fm_pcd_manip_reassem_capwap_stats_t {
	uint32_t	timeout;
			/**< Counts the number of timeout occurrences */
	uint32_t	rfd_pool_busy;
			/**< Counts the number of failed attempts to allocate a
			 * Reassembly Frame Descriptor
			 */
	uint32_t	internal_buffer_busy;
			/**< Counts the number of times an internal buffer busy
			 * occurred
			 */
	uint32_t	external_buffer_busy;
			/**< Counts the number of times external buffer busy
			 * occurred
			 */
	uint32_t	sg_fragments;
			/**< Counts the number of Scatter/Gather fragments */
	uint32_t	dma_semaphore_depletion;
			/**< Counts the number of failed attempts to allocate a
			 * DMA semaphore
			 */
	uint32_t	successfully_reassembled;
			/**< Counts the number of successfully reassembled
			 * frames
			 */
	uint32_t	valid_fragments;
			/**< Counts the total number of valid fragments that
			 * have been processed for all frames
			 */
	uint32_t	processed_fragments;
			/**< Counts the number of processed fragments (valid and
			 * error fragments) for all frames
			 */
	uint32_t	malformed_fragments;
			/**< Counts the number of malformed fragments processed
			 * for all frames
			 */
	uint32_t	auto_learn_busy;
			/**< Counts the number of times a busy condition occurs
			 * when attempting to access an Reassembly Automatic
			 * Learning Hash set
			 */
	uint32_t	discarded_fragments;
			/**< Counts the number of fragments discarded by the
			 * reassembly process
			 */
	uint32_t	more_than16fragments;
			/**< Counts the fragment occurrences in which the number
			 * of fragments-per-frame exceeds 16
			 */
	uint32_t	exceed_max_reassembly_frame_len;
			/**< ounts the number of times that a successful
			 * reassembled frame length exceeds
			 * MaxReassembledFrameLength value
			 */
} ioc_fm_pcd_manip_reassem_capwap_stats_t;

/*
 * @Description   Structure for retrieving CAPWAP fragmentation statistics
 */
typedef struct ioc_fm_pcd_manip_frag_capwap_stats_t {
	uint32_t	total_frames;
			/**< Number of frames that passed through the
			 * manipulation node
			 */
	uint32_t	fragmented_frames;
			/**< Number of frames that were fragmented */
	uint32_t	generated_fragments;
			/**< Number of fragments that were generated */
#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
	uint8_t	sg_allocation_failure;
			/**< Number of allocation failure of s/g buffers */
#endif /* (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0)) */
} ioc_fm_pcd_manip_frag_capwap_stats_t;

/*
 * @Description   Structure for retrieving reassembly statistics
 */
typedef struct ioc_fm_pcd_manip_reassem_stats_t {
	union {
	ioc_fm_pcd_manip_reassem_ip_stats_t  ip_reassem;
			/**< Structure for IP reassembly statistics */
	ioc_fm_pcd_manip_reassem_capwap_stats_t  capwap_reassem;
			/**< Structure for CAPWAP reassembly statistics */
	} u;
} ioc_fm_pcd_manip_reassem_stats_t;

/*
 * @Description   structure for retrieving fragmentation statistics
 */
typedef struct ioc_fm_pcd_manip_frag_stats_t {
	union {
	ioc_fm_pcd_manip_frag_ip_stats_t	ip_frag;
			/**< Structure for IP fragmentation statistics */
	ioc_fm_pcd_manip_frag_capwap_stats_t capwap_frag;
			/**< Structure for CAPWAP fragmentation statistics */
	} u;
} ioc_fm_pcd_manip_frag_stats_t;

/*
 * @Description   structure for defining manipulation statistics
 */
typedef struct ioc_fm_pcd_manip_stats_t {
	union {
	ioc_fm_pcd_manip_reassem_stats_t  reassem;
				/**< Structure for reassembly statistics */
	ioc_fm_pcd_manip_frag_stats_t	frag;
				/**< Structure for fragmentation statistics */
	} u;
} ioc_fm_pcd_manip_stats_t;

/*
 * @Description   Parameters for acquiring manipulation statistics
 */
typedef struct ioc_fm_pcd_manip_get_stats_t {
	void				*id;
	ioc_fm_pcd_manip_stats_t	stats;
} ioc_fm_pcd_manip_get_stats_t;

/*
 * @Description   Parameters for defining frame replicator group and its members
 */
struct fm_pcd_frm_replic_group_params_t {
	uint8_t			max_num_of_entries;
				/**< Maximal number of members in the group -
				 * must be at least two
				 */
	uint8_t			num_of_entries;
				/**< Number of members in the group - must be at
				 * least 1
				 */
	ioc_fm_pcd_cc_next_engine_params_t
		next_engine_params[IOC_FM_PCD_FRM_REPLIC_MAX_NUM_OF_ENTRIES];
				/**< Array of members' parameters */
};

typedef struct ioc_fm_pcd_frm_replic_group_params_t {
	struct fm_pcd_frm_replic_group_params_t param;
	void *id;
} ioc_fm_pcd_frm_replic_group_params_t;

typedef struct ioc_fm_pcd_frm_replic_member_t {
	void *h_replic_group;
	uint16_t member_index;
} ioc_fm_pcd_frm_replic_member_t;

typedef struct ioc_fm_pcd_frm_replic_member_params_t {
	ioc_fm_pcd_frm_replic_member_t member;
	ioc_fm_pcd_cc_next_engine_params_t next_engine_params;
} ioc_fm_pcd_frm_replic_member_params_t;


typedef struct ioc_fm_pcd_cc_key_statistics_t {
	uint32_t	byte_count;
			/**< This counter reflects byte count of frames that
			 * were matched by this key.
			 */
	uint32_t	frame_count;
			/**< This counter reflects count of frames that were
			 * matched by this key.
			 */
	uint32_t	frame_length_range_count[IOC_FM_PCD_CC_STATS_MAX_FLR];
			/**< These counters reflect how many frames matched this
			 * key in 'RMON' statistics mode: Each counter holds the
			 * number of frames of a specific frames length range,
			 * according to the ranges provided at initialization.
			 */
} ioc_fm_pcd_cc_key_statistics_t;


typedef struct ioc_fm_pcd_cc_tbl_get_stats_t {
	void				*id;
	uint16_t			key_index;
	ioc_fm_pcd_cc_key_statistics_t  statistics;
} ioc_fm_pcd_cc_tbl_get_stats_t;

/*
 * @Function	  fm_pcd_match_table_get_key_statistics
 *
 * @Description   This routine may be used to get statistics counters of
 *		  specific key in a CC Node.
 *
 *		  If 'e_FM_PCD_CC_STATS_MODE_FRAME' and
 *		  'e_FM_PCD_CC_STATS_MODE_BYTE_AND_FRAME' were set for this
 *		  node, these counters reflect how many frames passed that were
 *		  matched this key; The total frames count will be returned in
 *		  the counter of the first range (as only one frame length range
 *		  was defined). If 'e_FM_PCD_CC_STATS_MODE_RMON' was set for
 *		  this node, the total frame count will be separated to frame
 *		  length counters, based on provided frame length ranges.
 *
 * @Param[in]	  h_cc_node		A handle to the node
 * @Param[in]	  key_index		Key index for adding
 * @Param[out]	  p_key_statistics	Key statistics counters
 *
 * @Return	  The specific key statistics.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set().
 */
#define FM_PCD_IOC_MATCH_TABLE_GET_KEY_STAT \
		_IOWR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(12), \
		      ioc_fm_pcd_cc_tbl_get_stats_t)

/*
 * @Function	  fm_pcd_match_table_get_miss_statistics
 *
 * @Description   This routine may be used to get statistics counters of miss
 *		  entry in a CC Node.
 *
 *		  If 'e_FM_PCD_CC_STATS_MODE_FRAME' and
 *		  'e_FM_PCD_CC_STATS_MODE_BYTE_AND_FRAME' were set for this
 *		  node, these counters reflect how many frames were not matched
 *		  to any existing key and therefore passed through the miss
 *		  entry; The total frames count will be returned in the counter
 *		  of the first range (as only one frame length range was
 *		  defined).
 *
 * @Param[in]	  h_cc_node		A handle to the node
 * @Param[out]	  p_miss_statistics	Statistics counters for 'miss'
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set().
 */

#define FM_PCD_IOC_MATCH_TABLE_GET_MISS_STAT \
		_IOWR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(13), \
		      ioc_fm_pcd_cc_tbl_get_stats_t)

/*
 * @Function	  fm_pcd_hash_table_get_miss_statistics
 *
 * @Description   This routine may be used to get statistics counters of 'miss'
 *		  entry of the a hash table.
 *
 *		  If 'e_FM_PCD_CC_STATS_MODE_FRAME' and
 *		  'e_FM_PCD_CC_STATS_MODE_BYTE_AND_FRAME' were set for this
 *		  node, these counters reflect how many frames were not matched
 *		  to any existing key and therefore passed through the miss
 *		  entry;
 *
 * @Param[in]	  h_hash_tbl		A handle to a hash table
 * @Param[out]	  p_miss_statistics	Statistics counters for 'miss'
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_hash_table_set().
 */
#define FM_PCD_IOC_HASH_TABLE_GET_MISS_STAT \
		_IOWR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(14), \
		      ioc_fm_pcd_cc_tbl_get_stats_t)

/*
 * @Function	  fm_pcd_net_env_characteristics_set
 *
 * @Description   Define a set of Network Environment Characteristics.
 *
 *		  When setting an environment it is important to understand its
 *		  application. It is not meant to describe the flows that will
 *		  run on the ports using this environment, but what the user
 *		  means TO DO with the PCD mechanisms in order to
 *		  parse-classify-distribute those frames.
 *		  By specifying a distinction unit, the user means it would use
 *		  that option for distinction between frames at either a KeyGen
 *		  scheme or a coarse classification action descriptor. Using
 *		  interchangeable headers to define a unit means that the user
 *		  is indifferent to which of the interchangeable headers is
 *		  present in the frame, and wants the distinction to be based on
 *		  the presence of either one of them.
 *
 *		  Depending on context, there are limitations to the use of
 *		  environments. A port using the PCD functionality is bound to
 *		  an environment. Some or even all ports may share an
 *		  environment but also an environment per port is possible. When
 *		  initializing a scheme, a classification plan group (see
 *		  below), or a coarse classification tree, one of the
 *		  initialized environments must be stated and related to. When a
 *		  port is bound to a scheme, a classification plan group, or a
 *		  coarse classification tree, it MUST be bound to the same
 *		  environment.
 *
 *		  The different PCD modules, may relate (for flows definition)
 *		  ONLY on distinction units as defined by their environment.
 *		  When initializing a scheme for example, it may not choose to
 *		  select IPV4 as a match for recognizing flows unless it was
 *		  defined in the relating environment. In fact, to guide the
 *		  user through the configuration of the PCD, each module's
 *		  characterization in terms of flows is not done using protocol
 *		  names, but using environment indexes.
 *
 *		  In terms of HW implementation, the list of distinction units
 *		  sets the LCV vectors and later used for match vector,
 *		  classification plan vectors and coarse classification
 *		  indexing.
 *
 * @Param[in,out] ioc_fm_pcd_net_env_params_t	A structure defining the
 *						distinction units for this
 *						configuration.
 *
 * @Return	  0 on success; Error code otherwise.
 */
#define FM_PCD_IOC_NET_ENV_CHARACTERISTICS_SET \
		_IOWR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(20), \
		      ioc_fm_pcd_net_env_params_t)

/*
 * @Function	  fm_pcd_net_env_characteristics_delete
 *
 * @Description   Deletes a set of Network Environment Charecteristics.
 *
 * @Param[in]	  ioc_fm_obj_t		The id of a Network Environment object.
 *
 * @Return	  0 on success; Error code otherwise.
 */
#define FM_PCD_IOC_NET_ENV_CHARACTERISTICS_DELETE \
		_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(21), ioc_fm_obj_t)

/*
 * @Function	  fm_pcd_kg_scheme_set
 *
 * @Description   Initializing or modifying and enabling a scheme for the
 *		  KeyGen. This routine should be called for adding or modifying
 *		  a scheme. When a scheme needs modifying, the API requires that
 *		  it will be rewritten. In such a case 'modify' should be TRUE.
 *		  If the routine is called for a valid scheme and 'modify' is
 *		  FALSE, it will return error.
 *
 * @Param[in,out] ioc_fm_pcd_kg_scheme_params_t		A structure of
 *							parameters for defining
 *							the scheme
 *
 * @Return	  0 on success; Error code otherwise.
 */
#define FM_PCD_IOC_KG_SCHEME_SET \
		_IOWR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(24), \
		      ioc_fm_pcd_kg_scheme_params_t)

/*
 * @Function	  fm_pcd_kg_scheme_delete
 *
 * @Description   Deleting an initialized scheme.
 *
 * @Param[in]	  ioc_fm_obj_t	scheme id as initialized by application at
 *				FM_PCD_IOC_KG_SET_SCHEME
 *
 * @Return	  0 on success; Error code otherwise.
 */
#define FM_PCD_IOC_KG_SCHEME_DELETE \
		_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(25), ioc_fm_obj_t)

/*
 * @Function	  fm_pcd_cc_root_build
 *
 * @Description   This routine must be called to define a complete coarse
 *		  classification tree. This is the way to define coarse
 *		  classification to a certain flow - the KeyGen schemes may
 *		  point only to trees defined in this way.
 *
 * @Param[in,out] ioc_fm_pcd_cc_tree_params_t	A structure of parameters to
 *						define the tree.
 *
 * @Return	  0 on success; Error code otherwise.
 */
#define FM_PCD_IOC_CC_ROOT_BUILD \
		_IOWR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(26), void *)
		/* workaround ...*/

/*
 * @Function	  fm_pcd_cc_root_delete
 *
 * @Description   Deleting a built tree.
 *
 * @Param[in]	  ioc_fm_obj_t - The id of a CC tree.
 */
#define FM_PCD_IOC_CC_ROOT_DELETE \
		_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(27), ioc_fm_obj_t)

/*
 * @Function	  fm_pcd_match_table_set
 *
 * @Description   This routine should be called for each CC (coarse
 *		  classification) node. The whole CC tree should be built bottom
 *		  up so that each node points to already defined nodes. p_NodeId
 *		  returns the node Id to be used by other nodes.
 *
 * @Param[in,out] ioc_fm_pcd_cc_node_params_t	A structure for defining the CC
 *						node params
 *
 * @Return	  0 on success; Error code otherwise.
 */
#define FM_PCD_IOC_MATCH_TABLE_SET \
		_IOWR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(28), void *)
		/* workaround ...*/

/*
 * @Function	  fm_pcd_match_table_delete
 *
 * @Description   Deleting a built node.
 *
 * @Param[in]	  ioc_fm_obj_t - The id of a CC node.
 *
 * @Return	  0 on success; Error code otherwise.
 */
#define FM_PCD_IOC_MATCH_TABLE_DELETE \
		_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(29), ioc_fm_obj_t)

/*
 * @Function	  fm_pcd_cc_root_modify_next_engine
 *
 * @Description   Modify the Next Engine Parameters in the entry of the tree.
 *
 * @Param[in]	  ioc_fm_pcd_cc_tree_modify_next_engine_params_t
 *		  Pointer to a structure with the relevant parameters
 *
 * @Return	  0 on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_cc_root_build().
 */
#define FM_PCD_IOC_CC_ROOT_MODIFY_NEXT_ENGINE \
		_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(30), \
		     ioc_fm_pcd_cc_tree_modify_next_engine_params_t)

/*
 * @Function	  fm_pcd_match_table_modify_next_engine
 *
 * @Description   Modify the Next Engine Parameters in the relevant key entry of
 *		  the node.
 *
 * @Param[in]	  ioc_fm_pcd_cc_node_modify_next_engine_params_t
 *		  A pointer to a structure with the relevant parameters
 *
 * @Return	  0 on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set().
 */
#define FM_PCD_IOC_MATCH_TABLE_MODIFY_NEXT_ENGINE \
		_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(31), \
		     ioc_fm_pcd_cc_node_modify_next_engine_params_t)

/*
 * @Function	  fm_pcd_match_table_modify_miss_next_engine
 *
 * @Description   Modify the Next Engine Parameters of the Miss key case of the
 *		  node.
 *
 * @Param[in]	  ioc_fm_pcd_cc_node_modify_next_engine_params_t
 *		  Pointer to a structure with the relevant parameters
 *
 * @Return	  0 on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set().
 */
#define FM_PCD_IOC_MATCH_TABLE_MODIFY_MISS_NEXT_ENGINE \
		_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(32), \
		     ioc_fm_pcd_cc_node_modify_next_engine_params_t)

/*
 * @Function	  fm_pcd_match_table_remove_key
 *
 * @Description   Remove the key (including next engine parameters of this key)
 *		  defined by the index of the relevant node.
 *
 * @Param[in]	  ioc_fm_pcd_cc_node_remove_key_params_t
 *		  A pointer to a structure with the relevant parameters
 *
 * @Return	  0 on success; Error code otherwise.
 *
 * @Cautions	  Allowed only after fm_pcd_match_table_set() has been called
 *		  for this node and for all of the nodes that lead to it.
 */
#define FM_PCD_IOC_MATCH_TABLE_REMOVE_KEY \
		_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(33), \
		     ioc_fm_pcd_cc_node_remove_key_params_t)

/*
 * @Function	  fm_pcd_match_table_add_key
 *
 * @Description   Add the key (including next engine parameters of this key in
 *		  the index defined by the key_index. Note that
 *		  'FM_PCD_LAST_KEY_INDEX' may be used when the user don't care
 *		  about the position of the key in the table - in that case, the
 *		  key will be automatically added by the driver in the last
 *		  available entry.
 *
 * @Param[in]	  ioc_fm_pcd_cc_node_modify_key_and_next_engine_params_t
 *		  A pointer to a structure with the relevant parameters
 *
 * @Return	  0 on success; Error code otherwise.
 *
 * @Cautions	  Allowed only after fm_pcd_match_table_set() has been called
 *		  for this node and for all of the nodes that lead to it.
 */
#define FM_PCD_IOC_MATCH_TABLE_ADD_KEY \
		_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(34), \
		     ioc_fm_pcd_cc_node_modify_key_and_next_engine_params_t)

/*
 * @Function	  fm_pcd_match_table_modify_key_and_next_engine
 *
 * @Description   Modify the key and Next Engine Parameters of this key in the
 *		  index defined by key_index.
 *
 * @Param[in]	  ioc_fm_pcd_cc_node_modify_key_and_next_engine_params_t
 *		  A pointer to a structure with the relevant parameters
 *
 * @Return	  0 on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set() not only of
 *		  the relevnt node but also the node that points to this node.
 */
#define FM_PCD_IOC_MATCH_TABLE_MODIFY_KEY_AND_NEXT_ENGINE \
		_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(35), \
		     ioc_fm_pcd_cc_node_modify_key_and_next_engine_params_t)

/*
 * @Function	  fm_pcd_match_table_modify_key
 *
 * @Description   Modify the key at the index defined by key_index.
 *
 * @Param[in]	  ioc_fm_pcd_cc_node_modify_key_params_t - Pointer to a
 * structure with the relevant parameters
 *
 * @Return	  0 on success; Error code otherwise.
 *
 * @Cautions	  Allowed only after fm_pcd_match_table_set() has been called
 *		  for this node and for all of the nodes that lead to it.
 */
#define FM_PCD_IOC_MATCH_TABLE_MODIFY_KEY \
		_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(36), \
		     ioc_fm_pcd_cc_node_modify_key_params_t)

/*
 * @Function	  fm_pcd_hash_table_set
 *
 * @Description   This routine initializes a hash table structure.
 *		  KeyGen hash result determines the hash bucket.
 *		  Next, KeyGen key is compared against all keys of this bucket
 *		  (exact match).
 *		  Number of sets (number of buckets) of the hash equals to the
 *		  number of 1-s in 'hash_res_mask' in the provided parameters.
 *		  Number of hash table ways is then calculated by dividing
 *		  'max_num_of_keys' equally between the hash sets. This is the
 *		  maximal number of keys that a hash bucket may hold.
 *		  The hash table is initialized empty and keys may be added to
 *		  it following the initialization. Keys masks are not supported
 *		  in current hash table implementation. The initialized hash
 *		  table can be integrated as a node in a CC tree.
 *
 * @Param[in,out] ioc_fm_pcd_hash_table_params_t	Pointer to a structure
 *							with the relevant
 *							parameters.
 *
 * @Return	  0 on success; Error code otherwise.
 */
#define FM_PCD_IOC_HASH_TABLE_SET \
		_IOWR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(37), \
		      ioc_fm_pcd_hash_table_params_t)

/*
 * @Function	  fm_pcd_hash_table_delete
 *
 * @Description   This routine deletes the provided hash table and released all
 *		  its allocated resources.
 *
 * @Param[in]	  ioc_fm_obj_t		The ID of a hash table.
 *
 * @Return	  0 on success; Error code otherwise.
 *
 * @Cautions	Allowed only following fm_pcd_hash_table_set().
 */
#define FM_PCD_IOC_HASH_TABLE_DELETE \
		_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(37), ioc_fm_obj_t)

/*
 * @Function	  fm_pcd_hash_table_add_key
 *
 * @Description   This routine adds the provided key (including next engine
 *		  parameters of this key) to the hash table.
 *		  The key is added as the last key of the bucket that it is
 *		  mapped to.
 *
 * @Param[in]	  ioc_fm_pcd_hash_table_add_key_params_t
 *		  Pointer to a structure with the relevant parameters
 *
 * @Return	  0 on success; error code otherwise.
 *
 * @Cautions	Allowed only following fm_pcd_hash_table_set().
 */
#define FM_PCD_IOC_HASH_TABLE_ADD_KEY \
		_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(39), \
		     ioc_fm_pcd_hash_table_add_key_params_t)

/*
 * @Function	  fm_pcd_hash_table_remove_key
 *
 * @Description   This routine removes the requested key (including next engine
 *		  parameters of this key) from the hash table.
 *
 * @Param[in]	  ioc_fm_pcd_hash_table_remove_key_params_t - Pointer to a
 *		  structure with the relevant parameters
 *
 * @Return	  0 on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_hash_table_set().
 */
#define FM_PCD_IOC_HASH_TABLE_REMOVE_KEY \
		_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(40), \
		     ioc_fm_pcd_hash_table_remove_key_params_t)

/*
 * @Function	  fm_pcd_plcr_profile_set
 *
 * @Description   Sets a profile entry in the policer profile table.
 *		  The routine overrides any existing value.
 *
 * @Param[in,out] ioc_fm_pcd_plcr_profile_params_t	A structure of
 *							parameters for defining
 *							a policer profile entry.
 *
 * @Return	  0 on success; Error code otherwise.
 */
#define FM_PCD_IOC_PLCR_PROFILE_SET \
		_IOWR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(41), \
		      ioc_fm_pcd_plcr_profile_params_t)

/*
 * @Function	  fm_pcd_plcr_profile_delete
 *
 * @Description   Delete a profile entry in the policer profile table.
 *		  The routine set entry to invalid.
 *
 * @Param[in]	  ioc_fm_obj_t		The id of a policer profile.
 *
 * @Return	  0 on success; Error code otherwise.
 */
#define FM_PCD_IOC_PLCR_PROFILE_DELETE  \
		_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(41), ioc_fm_obj_t)

/*
 * @Function	  fm_pcd_manip_node_set
 *
 * @Description   This routine should be called for defining a manipulation
 *		  node. A manipulation node must be defined before the CC node
 *		  that precedes it.
 *
 * @Param[in]	  ioc_fm_pcd_manip_params_t	A structure of parameters
 *						defining the manipulation.
 *
 * @Return	  A handle to the initialized object on success; NULL code
 *		  otherwise.
 */
#define FM_PCD_IOC_MANIP_NODE_SET \
		_IOWR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(43), \
		      ioc_fm_pcd_manip_params_t)

/*
 * @Function	  fm_pcd_manip_node_replace
 *
 * @Description   Change existing manipulation node to be according to new
 *		  requirement. (Here, it's implemented as a variant of the same
 *		  IOCTL as for fm_pcd_manip_node_set(), and one that when
 *		  called, the 'id' member in its 'ioc_fm_pcd_manip_params_t'
 *		  argument is set to contain the manip node's handle)
 *
 * @Param[in]	  ioc_fm_pcd_manip_params_t	A structure of parameters
 *						defining the manipulation.
 *
 * @Return	  0 on success; error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_manip_node_set().
 */
#define FM_PCD_IOC_MANIP_NODE_REPLACE	FM_PCD_IOC_MANIP_NODE_SET

/*
 * @Function	  fm_pcd_manip_node_delete
 *
 * @Description   Delete an existing manipulation node.
 *
 * @Param[in]	  ioc_fm_obj_t	The id of the manipulation node to delete.
 *
 * @Return	  0 on success; error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_manip_node_set().
 */
#define FM_PCD_IOC_MANIP_NODE_DELETE \
		_IOW(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(44), ioc_fm_obj_t)

/*
 * @Function	  fm_pcd_manip_get_statistics
 *
 * @Description   Retrieve the manipulation statistics.
 *
 * @Param[in]	  h_manip_node		A handle to a manipulation node.
 * @Param[out]	  p_fm_pcd_manip_stats	A structure for retrieving the
 *					manipulation statistics.
 *
 * @Return	E_OK on success; Error code otherwise.
 *
 * @Cautions	Allowed only following fm_pcd_manip_node_set().
 */
#define FM_PCD_IOC_MANIP_GET_STATS \
		_IOWR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(50), \
		      ioc_fm_pcd_manip_get_stats_t)

/*
 * @Function	  fm_pcd_set_advanced_offload_support
 *
 * @Description   This routine must be called in order to support the following
 *		  features: IP-fragmentation, IP-reassembly, IPsec,
 *		  Header-manipulation, frame-replicator.
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 *
 * @Return	  0 on success; error code otherwise.
 *
 * @Cautions	  Allowed only when PCD is disabled.
 */
#define FM_PCD_IOC_SET_ADVANCED_OFFLOAD_SUPPORT \
		_IO(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(45))

/*
 * @Function	  fm_pcd_frm_replic_set_group
 *
 * @Description   Initialize a Frame Replicator group.
 *
 * @Param[in]	  h_fm_pcd			FM PCD module descriptor.
 * @Param[in]	  p_frm_replic_group_param	A structure of parameters for
 *						the initialization of the frame
 *						replicator group.
 *
 * @Return	  A handle to the initialized object on success; NULL code
 *		  otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 */
#define FM_PCD_IOC_FRM_REPLIC_GROUP_SET \
		_IOWR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(46), \
		      ioc_fm_pcd_frm_replic_group_params_t)

/*
 * @Function	  fm_pcd_frm_replic_delete_group
 *
 * @Description   Delete a Frame Replicator group.
 *
 * @Param[in]	  h_frm_replic_group  A handle to the frame replicator group.
 *
 * @Return	  E_OK on success;  Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_frm_replic_set_group().
 */
#define FM_PCD_IOC_FRM_REPLIC_GROUP_DELETE \
		_IOWR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(47), ioc_fm_obj_t)

/*
 * @Function	  fm_pcd_frm_replic_add_member
 *
 * @Description   Add the member in the index defined by the member_index.
 *
 * @Param[in]	  h_frm_replic_group	A handle to the frame replicator group.
 * @Param[in]	  member_index		Member index for adding.
 * @Param[in]	  p_member_params	A pointer to the new member parameters.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_frm_replic_set_group() of this
 *		  group.
 */
#define FM_PCD_IOC_FRM_REPLIC_MEMBER_ADD \
		_IOWR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(48), \
			ioc_fm_pcd_frm_replic_member_params_t)

/*
 * @Function	  fm_pcd_frm_replic_remove_member
 *
 * @Description   Remove the member defined by the index from the relevant group
 *
 * @Param[in]	  h_frm_replic_group	A handle to the frame replicator group.
 * @Param[in]	  member_index		Member index for removing.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_frm_replic_set_group() of this
 *		  group.
 */
#define FM_PCD_IOC_FRM_REPLIC_MEMBER_REMOVE \
		_IOWR(FM_IOC_TYPE_BASE, FM_PCD_IOC_NUM(49), \
		      ioc_fm_pcd_frm_replic_member_t)

/*
 * @Group	  FM_grp Frame Manager API
 *
 * @Description   Frame Manager Application Programming Interface
 *
 * @{
 */

/*
 * @Group	  FM_PCD_grp FM PCD
 *
 * @Description   Frame Manager PCD (Parse-Classify-Distribute) API.
 *
 *		  The FM PCD module is responsible for the initialization of all
 *		  global classifying FM modules. This includes the parser
 *		  general and common registers, the key generator global and
 *		  common registers, and the policer global and common registers.
 *		  In addition, the FM PCD SW module will initialize all required
 *		  key generator schemes, coarse classification flows, and
 *		  policer profiles. When FM module is configured to work with
 *		  one of these entities, it will register to it using the FM
 *		  PORT API. The PCD module will manage the PCD resources - i.e.
 *		  resource management of KeyGen schemes, etc.
 *
 * @{
 */

/*
 * @Collection	  General PCD defines
 */
#define FM_PCD_MAX_NUM_OF_PRIVATE_HDRS		2
/**< Number of units/headers saved for user */

#define FM_PCD_PRS_NUM_OF_HDRS			16
/**< Number of headers supported by HW parser */
#define FM_PCD_MAX_NUM_OF_DISTINCTION_UNITS \
		(32 - FM_PCD_MAX_NUM_OF_PRIVATE_HDRS)
/**< Number of distinction units is limited by register size (32 bits) minus
 *reserved bits for private headers.
 */
#define FM_PCD_MAX_NUM_OF_INTERCHANGEABLE_HDRS	4
/**< Maximum number of interchangeable headers in a distinction unit */
#define FM_PCD_KG_NUM_OF_GENERIC_REGS		FM_KG_NUM_OF_GENERIC_REGS
/**< Total number of generic KeyGen registers */
#define FM_PCD_KG_MAX_NUM_OF_EXTRACTS_PER_KEY	35
/**< Max number allowed on any configuration; For HW implementation reasons, in
 * most cases less than this will be allowed; The driver will return an
 * initialization error if resource is unavailable.
 */
#define FM_PCD_KG_NUM_OF_EXTRACT_MASKS		4
/**< Total number of masks allowed on KeyGen extractions. */
#define FM_PCD_KG_NUM_OF_DEFAULT_GROUPS		16
/**< Number of default value logical groups */

#define FM_PCD_PRS_NUM_OF_LABELS			32
/**< Maximum number of SW parser labels */
#define FM_SW_PRS_MAX_IMAGE_SIZE \
	(FM_PCD_SW_PRS_SIZE \
	 /*- FM_PCD_PRS_SW_OFFSET -FM_PCD_PRS_SW_TAIL_SIZE*/ \
	 - FM_PCD_PRS_SW_PATCHES_SIZE)
/**< Maximum size of SW parser code */

#define FM_PCD_MAX_MANIP_INSRT_TEMPLATE_SIZE	128
/**< Maximum size of insertion template for insert manipulation */

#define FM_PCD_FRM_REPLIC_MAX_NUM_OF_ENTRIES	64
/**< Maximum possible entries for frame replicator group */
/* @} */

/*
 * @Group	  FM_PCD_init_grp FM PCD Initialization Unit
 *
 * @Description   Frame Manager PCD Initialization Unit API
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
typedef void (t_fm_pcd_exception_callback) (t_handle h_app,
					ioc_fm_pcd_exceptions exception);

/*
 * @Description   Exceptions user callback routine, will be called upon an
 *		  exception passing the exception identification.
 *
 * @Param[in]	  h_app		User's application descriptor.
 * @Param[in]	  exception	The exception.
 * @Param[in]	  index		id of the relevant source (may be scheme or
 *				profile id).
 */
typedef void (t_fm_pcd_id_exception_callback) (t_handle	h_app,
					ioc_fm_pcd_exceptions  exception,
					uint16_t	index);

/*
 * @Description   A callback for enqueuing frame onto a QM queue.
 *
 * @Param[in]	  h_qm_arg	Application's handle passed to QM module on
 *				enqueue.
 * @Param[in]	  p_fd		Frame descriptor for the frame.
 *
 * @Return	  E_OK on success; Error code otherwise.
 */
typedef uint32_t (t_fm_pcd_qm_enqueue_callback) (t_handle h_qm_arg, void *p_fd);

/*
 * @Description   Host-Command parameters structure.
 *
 *		  When using Host command for PCD functionalities, a dedicated
 *		  port must be used. If this routine is called for a PCD in a
 *		  single partition environment, or it is the Master partition in
 *		  a Multi-partition environment, The port will be initialized by
 *		  the PCD driver initialization routine.
 */
typedef struct t_fm_pcd_hc_params {
	uintptr_t		port_base_addr;
	/**< Virtual Address of Host-Command Port memory mapped registers.*/
	uint8_t			port_id;
	/**< Port Id (0-6 relative to Host-Command/Offline-Parsing ports);
	 * NOTE: When configuring Host Command port for FMANv3 devices
	 * (DPAA_VERSION 11 and higher), port_id=0 MUST be used.
	 */
	uint16_t			liodn_base;
	/**< LIODN base for this port, to be used together with LIODN offset
	 * (irrelevant for P4080 revision 1.0)
	 */
	uint32_t			err_fqid;
	/**< Host-Command Port error queue Id. */
	uint32_t			conf_fqid;
	/**< Host-Command Port confirmation queue Id. */
	uint32_t			qm_channel;
	/**< QM channel dedicated to this Host-Command port; will be used by the
	 * FM for dequeue.
	 */
	t_fm_pcd_qm_enqueue_callback	*f_qm_enqueue;
	/**< Callback routine for enqueuing a frame to the QM */
	t_handle			h_qm_arg;
	/**< Application's handle passed to QM module on enqueue */
} t_fm_pcd_hc_params;

/*
 * @Description   The main structure for PCD initialization
 */
typedef struct t_fm_pcd_params {
	bool			prs_support;
	/**< TRUE if Parser will be used for any of the FM ports. */
	bool			cc_support;
	/**< TRUE if Coarse Classification will be used for any of the FM ports.
	 */
	bool			kg_support;
	/**< TRUE if KeyGen will be used for any of the FM ports. */
	bool			plcr_support;
	/**< TRUE if Policer will be used for any of the FM ports. */
	t_handle		h_fm;
	/**< A handle to the FM module. */
	uint8_t			num_schemes;
	/**< Number of schemes dedicated to this partition.
	 * this parameter is relevant if 'kg_support'=TRUE.
	 */
	bool			use_host_command;
	/**< Optional for single partition, Mandatory for Multi partition */
	t_fm_pcd_hc_params		hc;
	/**< Host Command parameters, relevant only if 'use_host_command'=TRUE;
	 * Relevant when FM not runs in "guest-mode".
	 */
	t_fm_pcd_exception_callback	*f_exception;
	/**< Callback routine for general PCD exceptions; Relevant when FM not
	 * runs in "guest-mode".
	 */
	t_fm_pcd_id_exception_callback	*f_exception_id;
	/**< Callback routine for specific KeyGen scheme or Policer profile
	 * exceptions; Relevant when FM not runs in "guest-mode".
	 */
	t_handle		h_app;
	/**< A handle to an application layer object; This handle will be passed
	 * by the driver upon calling the above callbacks; Relevant when FM not
	 * runs in "guest-mode".
	 */
	uint8_t			part_plcr_profiles_base;
	/**< The first policer-profile-id dedicated to this partition. this
	 * parameter is relevant if 'plcr_support'=TRUE. NOTE: this parameter
	 * relevant only when working with multiple partitions.
	 */
	uint16_t		part_num_of_plcr_profiles;
	/**< Number of policer-profiles dedicated to this partition. This
	 * parameter is relevant if 'plcr_support'=TRUE. NOTE: this parameter
	 * relevant only when working with multiple partitions.
	 */
} t_fm_pcd_params;

typedef struct t_fm_pcd_prs_label_params {
	uint32_t instruction_offset;
	ioc_net_header_type hdr;
	uint8_t index_per_hdr;
} t_fm_pcd_prs_label_params;

typedef struct t_fm_pcd_prs_sw_params {
	bool override;
	uint32_t size;
	uint16_t base;
	uint8_t *p_code;
	uint32_t sw_prs_data_params[FM_PCD_PRS_NUM_OF_HDRS];
	uint8_t num_of_labels;
	t_fm_pcd_prs_label_params labels_table[FM_PCD_PRS_NUM_OF_LABELS];
} t_fm_pcd_prs_sw_params;

/*
 * @Function	  fm_pcd_config
 *
 * @Description   Basic configuration of the PCD module.
 *		  Creates descriptor for the FM PCD module.
 *
 * @Param[in]	  p_fm_pcd_params	A structure of parameters for the
					initialization of PCD.
 *
 * @Return	  A handle to the initialized module.
 */
t_handle fm_pcd_config(t_fm_pcd_params *p_fm_pcd_params);

/*
 * @Function	  fm_pcd_init
 *
 * @Description   Initialization of the PCD module.
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 *
 * @Return	  E_OK on success; Error code otherwise.
 */
uint32_t fm_pcd_init(t_handle h_fm_pcd);

/*
 * @Function	  fm_pcd_free
 *
 * @Description   Frees all resources that were assigned to FM module.
 *		  Calling this routine invalidates the descriptor.
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 *
 * @Return	  E_OK on success; Error code otherwise.
 */
uint32_t fm_pcd_free(t_handle h_fm_pcd);

/*
 * @Group	  FM_PCD_advanced_cfg_grp	FM PCD Advanced Configuration
 *						Unit
 *
 * @Description   Frame Manager PCD Advanced Configuration API.
 *
 * @{
 */

/*
 * @Function	  fm_pcd_config_exception
 *
 * @Description   Calling this routine changes the internal driver data base
 *		  from its default selection of exceptions enabling.
 *		  [DEFAULT_num_of_shared_plcr_profiles].
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 * @Param[in]	  exception	The exception to be selected.
 * @Param[in]	  enable	TRUE to enable interrupt, FALSE to mask it.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  This routine should NOT be called from guest-partition (i.e.
 *		  guestId != NCSW_PRIMARY_ID)
 */
uint32_t fm_pcd_config_exception(t_handle h_fm_pcd,
		ioc_fm_pcd_exceptions exception, bool enable);

/*
 * @Function	  fm_pcd_config_hc_frames_data_memory
 *
 * @Description   Configures memory-partition-id for FMan-Controller
 *		  Host-Command frames. Calling this routine changes the internal
 *		  driver data base from its default configuration [0].
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 * @Param[in]	  mem_id	Memory partition ID.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  This routine may be called only if 'use_host_command' was TRUE
 *		  when fm_pcd_config() routine was called.
 */
uint32_t fm_pcd_config_hc_frames_data_memory(t_handle h_fm_pcd, uint8_t mem_id);

/*
 * @Function	  fm_pcd_config_plcr_num_of_shared_profiles
 *
 * @Description   Calling this routine changes the internal driver data base
 *		  from its default selection of exceptions enablement.
 *		  [DEFAULT_num_of_shared_plcr_profiles].
 *
 * @Param[in]	  h_fm_pcd			FM PCD module descriptor.
 * @Param[in]	  num_of_shared_plcr_profiles	Number of profiles to be shared
 *						between ports on this partition
 *
 * @Return	  E_OK on success; Error code otherwise.
 */
uint32_t fm_pcd_config_plcr_num_of_shared_profiles(t_handle h_fm_pcd,
		uint16_t num_of_shared_plcr_profiles);

/*
 * @Function	  fm_pcd_config_plcr_auto_refresh_mode
 *
 * @Description   Calling this routine changes the internal driver data base
 *		  from its default selection of exceptions enablement. By
 *		  default auto-refresh is [DEFAULT_plcrAutoRefresh].
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 * @Param[in]	  enable	TRUE to enable, FALSE to disable
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  This routine should NOT be called from guest-partition
 *		  (i.e. guestId != NCSW_PRIMARY_ID)
 */
uint32_t fm_pcd_config_plcr_auto_refresh_mode(t_handle h_fm_pcd, bool enable);

/*
 * @Function	  fm_pcd_config_prs_max_cycle_limit
 *
 * @Description   Calling this routine changes the internal data structure for
 *		  the maximum parsing time from its default value
 *		  [DEFAULT_MAX_PRS_CYC_LIM].
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 * @Param[in]	  value		0 to disable the mechanism, or new maximum
 *				parsing time.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  This routine should NOT be called from guest-partition
 *		  (i.e. guestId != NCSW_PRIMARY_ID)
 */
uint32_t fm_pcd_config_prs_max_cycle_limit(t_handle h_fm_pcd, uint16_t value);

/** @} */ /* end of FM_PCD_advanced_cfg_grp group */
/** @} */ /* end of FM_PCD_init_grp group */

/*
 * @Group	  FM_PCD_Runtime_grp FM PCD Runtime Unit
 *
 * @Description   Frame Manager PCD Runtime Unit API
 *
 *		  The runtime control allows creation of PCD infrastructure
 *		  modules such as Network Environment Characteristics,
 *		  Classification Plan Groups and Coarse Classification Trees.
 *		  It also allows on-the-fly initialization, modification and
 *		  removal of PCD modules such as KeyGen schemes, coarse
 *		  classification nodes and Policer profiles.
 *
 *		  In order to explain the programming model of the PCD driver
 *		  interface a few terms should be explained, and will be used
 *		  below.
 *		  - Distinction Header - One of the 16 protocols supported by
 *		    the FM parser, or one of the SHIM headers (1 or 2). May be a
 *		    header with a special option (see below).
 *		  - Interchangeable Headers Group - This is a group of Headers
 *		    recognized by either one of them. For example, if in a
 *		    specific context the user chooses to treat IPv4 and IPV6 in
 *		    the same way, they may create an interchangeable Headers
 *		    Unit consisting of these 2 headers.
 *		  - A Distinction Unit - a Distinction Header or an
 *		    Interchangeable Headers Group.
 *		  - Header with special option - applies to Ethernet, MPLS,
 *		    VLAN, IPv4 and IPv6, includes multicast, broadcast and other
 *		    protocol specific options. In terms of hardware it relates
 *		    to the options available in the classification plan.
 *		  - Network Environment Characteristics - a set of Distinction
 *		    Units that define the total recognizable header selection
 *		    for a certain environment. This is NOT the list of all
 *		    headers that will ever appear in a flow, but rather
 *		    everything that needs distinction in a flow, where
 *		    distinction is made by KeyGen schemes and coarse
 *		    classification action descriptors.
 *
 *		  The PCD runtime modules initialization is done in stages. The
 *		  first stage after initializing the PCD module itself is to
 *		  establish a Network Flows Environment Definition. The
 *		  application may choose to establish one or more such
 *		  environments. Later, when needed, the application will have to
 *		  state, for some of its modules, to which single environment it
 *		  belongs.
 *
 * @{
 */

t_handle fm_pcd_open(t_fm_pcd_params *p_fm_pcd_params);
void fm_pcd_close(t_handle h_fm_pcd);

/*
 * @Function	  fm_pcd_enable
 *
 * @Description   This routine should be called after PCD is initialized for
 *		  enabling all PCD engines according to their existing
 *		  configuration.
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init() and when PCD is disabled.
 */
uint32_t fm_pcd_enable(t_handle h_fm_pcd);

/*
 * @Function	  fm_pcd_disable
 *
 * @Description   This routine may be called when PCD is enabled in order to
 *		  disable all PCD engines. It may be called only when none of
 *		  the ports in the system are using the PCD.
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init() and when PCD is enabled.
 */
uint32_t fm_pcd_disable(t_handle h_fm_pcd);

/*
 * @Function	  fm_pcd_get_counter
 *
 * @Description   Reads one of the FM PCD counters.
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 * @Param[in]	  counter	The requested counter.
 *
 * @Return	  Counter's current value.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 *		  Note that it is user's responsibility to call this routine
 *		  only for enabled counters, and there will be no indication if
 *		  a disabled counter is accessed.
 */
uint32_t fm_pcd_get_counter(t_handle h_fm_pcd, ioc_fm_pcd_counters counter);

/*
 * @Function	fm_pcd_prs_load_sw
 *
 * @Description	This routine may be called in order to load software parsing
 *		code.
 *
 * @Param[in]	h_fm_pcd	FM PCD module descriptor.
 * @Param[in]	p_sw_prs	A pointer to a structure of software
 *				parser parameters, including the software
 *				parser image.
 *
 * @Return	E_OK on success; Error code otherwise.
 *
 * @Cautions	Allowed only following fm_pcd_init() and when PCD is disabled.
 *		This routine should NOT be called from guest-partition
 *		(i.e. guestId != NCSW_PRIMARY_ID)
 */
uint32_t fm_pcd_prs_load_sw(t_handle h_fm_pcd,
		ioc_fm_pcd_prs_sw_params_t *p_sw_prs);

/*
 * @Function	  fm_pcd_set_advanced_offload_support
 *
 * @Description   This routine must be called in order to support the following
 *		  features: IP-fragmentation, IP-reassembly, IPsec,
 *		  Header-manipulation, frame-replicator.
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init() and when PCD is disabled.
 *		  This routine should NOT be called from guest-partition
 *		  (i.e. guestId != NCSW_PRIMARY_ID)
 */
uint32_t fm_pcd_set_advanced_offload_support(t_handle h_fm_pcd);

/*
 * @Function	  fm_pcd_kg_set_dflt_value
 *
 * @Description   Calling this routine sets a global default value to be used
 *		  by the KeyGen when parser does not recognize a required
 *		  field/header.
 *		  default value is 0.
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 * @Param[in]	  value_id	0,1 - one of 2 global default values.
 * @Param[in]	  value		The requested default value.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init() and when PCD is disabled.
 *		  This routine should NOT be called from guest-partition
 *		  (i.e. guestId != NCSW_PRIMARY_ID)
 */
uint32_t fm_pcd_kg_set_dflt_value(t_handle h_fm_pcd,
		uint8_t value_id, uint32_t value);

/*
 * @Function	  fm_pcd_kg_set_additional_data_after_parsing
 *
 * @Description   Calling this routine allows the KeyGen to access data past
 *		  the parser finishing point.
 *
 * @Param[in]	  h_fm_pcd		FM PCD module descriptor.
 * @Param[in]	  payload_offset	the number of bytes beyond the parser
 *					location.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init() and when PCD is disabled.
 *		  This routine should NOT be called from guest-partition (i.e.
 *		  guestId != NCSW_PRIMARY_ID)
 */
uint32_t fm_pcd_kg_set_additional_data_after_parsing(t_handle h_fm_pcd,
		uint8_t payload_offset);

/*
 * @Function	  fm_pcd_set_exception
 *
 * @Description   Calling this routine enables/disables PCD interrupts.
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 * @Param[in]	  exception	The exception to be selected.
 * @Param[in]	  enable	TRUE to enable interrupt, FALSE to mask it.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 *		  This routine should NOT be called from guest-partition
 *		  (i.e. guestId != NCSW_PRIMARY_ID)
 */
uint32_t fm_pcd_set_exception(t_handle h_fm_pcd,
		ioc_fm_pcd_exceptions exception, bool enable);

/*
 * @Function	  fm_pcd_modify_counter
 *
 * @Description   Sets a value to an enabled counter. Use "0" to reset the
 *		  counter.
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 * @Param[in]	  counter	The requested counter.
 * @Param[in]	  value		The requested value to be written into the
 *				counter.
 *
 * @Return	E_OK on success; Error code otherwise.
 *
 * @Cautions	Allowed only following fm_pcd_init().
 *		This routine should NOT be called from guest-partition
 *		(i.e. guestId != NCSW_PRIMARY_ID)
 */
uint32_t fm_pcd_modify_counter(t_handle h_fm_pcd,
		ioc_fm_pcd_counters counter, uint32_t value);

/*
 * @Function	  fm_pcd_set_plcr_statistics
 *
 * @Description   This routine may be used to enable/disable policer statistics
 *		  counter. By default the statistics is enabled.
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor
 * @Param[in]	  enable	TRUE to enable, FALSE to disable.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 *		  This routine should NOT be called from guest-partition
 *		  (i.e. guestId != NCSW_PRIMARY_ID)
 */
uint32_t fm_pcd_set_plcr_statistics(t_handle h_fm_pcd, bool enable);

/*
 * @Function	  fm_pcd_set_prs_statistics
 *
 * @Description   Defines whether to gather parser statistics including all
 *		  ports.
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 * @Param[in]	  enable	TRUE to enable, FALSE to disable.
 *
 * @Return	  None
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 *		  This routine should NOT be called from guest-partition
 *		  (i.e. guestId != NCSW_PRIMARY_ID)
 */
void fm_pcd_set_prs_statistics(t_handle h_fm_pcd, bool enable);

#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
/*
 * @Function	  fm_pcd_dump_regs
 *
 * @Description   Dumps all PCD registers
 *
 * @Param[in]	  h_fm_pcd	A handle to an FM PCD Module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 *		  NOTE: this routine may be called only for FM in master mode
 *		  (i.e. 'guestId'=NCSW_PRIMARY_ID) or in a case that the
 *		  registers are mapped.
 */
uint32_t fm_pcd_dump_regs(t_handle h_fm_pcd);

/*
 * @Function	  fm_pcd_kg_dump_regs
 *
 * @Description   Dumps all PCD KG registers
 *
 * @Param[in]	  h_fm_pcd	A handle to an FM PCD Module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 *		  NOTE: this routine may be called only for FM in master mode
 *		  (i.e. 'guestId'=NCSW_PRIMARY_ID) or in a case that the
 *		  registers are mapped.
 */
uint32_t fm_pcd_kg_dump_regs(t_handle h_fm_pcd);

/*
 * @Function	  fm_pcd_plcr_dump_regs
 *
 * @Description   Dumps all PCD Policer registers
 *
 * @Param[in]	  h_fm_pcd	A handle to an FM PCD Module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 *		  NOTE: this routine may be called only for FM in master mode
 *		  (i.e. 'guestId'=NCSW_PRIMARY_ID) or in a case that the
 *		  registers are mapped.
 */
uint32_t fm_pcd_plcr_dump_regs(t_handle h_fm_pcd);

/*
 * @Function	  fm_pcd_plcr_profile_dump_regs
 *
 * @Description   Dumps all PCD Policer profile registers
 *
 * @Param[in]	  h_profile	A handle to a Policer profile.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 *		  NOTE: this routine may be called only for FM in master mode
 *		  (i.e. 'guestId'=NCSW_PRIMARY_ID) or in a case that the
 *		  registers are mapped.
 */
uint32_t fm_pcd_plcr_profile_dump_regs(t_handle h_profile);

/*
 * @Function	  fm_pcd_prs_dump_regs
 *
 * @Description   Dumps all PCD Parser registers
 *
 * @Param[in]	  h_fm_pcd	A handle to an FM PCD Module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 *		  NOTE: this routine may be called only for FM in master mode
 *		  (i.e. 'guestId'=NCSW_PRIMARY_ID) or in a case that the
 *		  registers are mapped.
 */
uint32_t fm_pcd_prs_dump_regs(t_handle h_fm_pcd);

/*
 * @Function	  fm_pcd_hc_dump_regs
 *
 * @Description   Dumps HC Port registers
 *
 * @Param[in]	  h_fm_pcd	A handle to an FM PCD Module.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 *		  NOTE: this routine may be called only for FM in master mode
 *		  (i.e. 'guestId'=NCSW_PRIMARY_ID).
 */
uint32_t	fm_pcd_hc_dump_regs(t_handle h_fm_pcd);
#endif /* (defined(DEBUG_ERRORS) && ... */


/*
 * KeyGen	  FM_PCD_Runtime_build_grp FM PCD Runtime Building Unit
 *
 * @Description   Frame Manager PCD Runtime Building API
 *
 *		  This group contains routines for setting, deleting and
 *		  modifying PCD resources, for defining the total PCD tree.
 * @{
 */

/*
 * @Collection	  Definitions of coarse classification
 *		  parameters as required by KeyGen (when coarse classification
 *		  is the next engine after this scheme).
 */
#define FM_PCD_MAX_NUM_OF_CC_TREES		8
#define FM_PCD_MAX_NUM_OF_CC_GROUPS		16
#define FM_PCD_MAX_NUM_OF_CC_UNITS		4
#define FM_PCD_MAX_NUM_OF_KEYS		256
#define FM_PCD_MAX_NUM_OF_FLOWS		(4 * KILOBYTE)
#define FM_PCD_MAX_SIZE_OF_KEY		56
#define FM_PCD_MAX_NUM_OF_CC_ENTRIES_IN_GRP	16
#define FM_PCD_LAST_KEY_INDEX		0xffff

#define FM_PCD_MAX_NUM_OF_CC_NODES	255
			/* Obsolete, not used - will be removed in the future */
/* @} */

/*
 * @Collection	  A set of definitions to allow protocol
 *		  special option description.
 */
typedef uint32_t	protocol_opt_t;
			/**< A general type to define a protocol option. */

typedef protocol_opt_t   eth_protocol_opt_t;
			/**< Ethernet protocol options. */
#define ETH_BROADCAST		0x80000000  /**< Ethernet Broadcast. */
#define ETH_MULTICAST		0x40000000  /**< Ethernet Multicast. */

typedef protocol_opt_t   vlan_protocol_opt_t;	/**< VLAN protocol options. */
#define VLAN_STACKED		0x20000000  /**< Stacked VLAN. */

typedef protocol_opt_t   mpls_protocol_opt_t;	/**< MPLS protocol options. */
#define MPLS_STACKED		0x10000000  /**< Stacked MPLS. */

typedef protocol_opt_t   ipv_4protocol_opt_t;	/**< IPv4 protocol options. */
#define IPV4_BROADCAST_1		0x08000000  /**< IPv4 Broadcast. */
#define IPV4_MULTICAST_1		0x04000000  /**< IPv4 Multicast. */
#define IPV4_UNICAST_2		0x02000000  /**< Tunneled IPv4 - Unicast. */
#define IPV4_MULTICAST_BROADCAST_2  0x01000000
				/**< Tunneled IPv4 - Broadcast/Multicast. */

#define IPV4_FRAG_1		0x00000008
				/**< IPV4 reassembly option. IPV4 Reassembly
				 * manipulation requires network environment
				 * with IPV4 header and IPV4_FRAG_1 option
				 */

typedef protocol_opt_t   ipv_6protocol_opt_t;	/**< IPv6 protocol options. */
#define IPV6_MULTICAST_1	0x00800000  /**< IPv6 Multicast. */
#define IPV6_UNICAST_2		0x00400000  /**< Tunneled IPv6 - Unicast. */
#define IPV6_MULTICAST_2	0x00200000  /**< Tunneled IPv6 - Multicast. */

#define IPV6_FRAG_1		0x00000004
				/**< IPV6 reassembly option. IPV6 Reassembly
				 * manipulation requires network environment
				 * with IPV6 header and IPV6_FRAG_1 option; in
				 * case where fragment found, the
				 * fragment-extension offset may be found at
				 * 'shim2' (in parser-result).
				 */
typedef protocol_opt_t   capwap_protocol_opt_t;	/**< CAPWAP protocol options. */
#define CAPWAP_FRAG_1		0x00000008
				/**< CAPWAP reassembly option. CAPWAP Reassembly
				 * manipulation requires network environment
				 * with CAPWAP header and CAPWAP_FRAG_1 option;
				 * in case where fragment found, the
				 * fragment-extension offset may be found at
				 * 'shim2' (in parser-result).
				 */

/* @} */

#define FM_PCD_MANIP_MAX_HDR_SIZE	256
#define FM_PCD_MANIP_DSCP_TO_VLAN_TRANS	64

/*
 * @Collection	  A set of definitions to support Header Manipulation selection.
 */
typedef uint32_t		hdr_manip_flags_t;
		/**< A general type to define a HMan update command flags. */

typedef hdr_manip_flags_t	ipv_4hdr_manip_update_flags_t;
		/**< IPv4 protocol HMan update command flags. */

#define HDR_MANIP_IPV4_TOS	0x80000000
			/**< update TOS with the given value ('tos' field
			 * of t_FmPcdManipHdrFieldUpdateIpv4)
			 */
#define HDR_MANIP_IPV4_ID	0x40000000
			/**< update IP ID with the given value ('id' field
			 * of t_FmPcdManipHdrFieldUpdateIpv4)
			 */
#define HDR_MANIP_IPV4_TTL	0x20000000
			/**< Decrement TTL by 1 */
#define HDR_MANIP_IPV4_SRC	0x10000000
			/**< update IP source address with the given value
			 * ('src' field of t_FmPcdManipHdrFieldUpdateIpv4)
			 */
#define HDR_MANIP_IPV4_DST	0x08000000
			/**< update IP destination address with the given value
			 * ('dst' field of t_FmPcdManipHdrFieldUpdateIpv4)
			 */

typedef hdr_manip_flags_t	ipv_6hdr_manip_update_flags_t;
			/**< IPv6 protocol HMan update command flags. */

#define HDR_MANIP_IPV6_TC	0x80000000
			/**< update Traffic Class address with the given value
			 * ('trafficClass' field of
			 * t_FmPcdManipHdrFieldUpdateIpv6)
			 */
#define HDR_MANIP_IPV6_HL	0x40000000
			/**< Decrement Hop Limit by 1 */
#define HDR_MANIP_IPV6_SRC	0x20000000
			/**< update IP source address with the given value
			 * ('src' field of t_FmPcdManipHdrFieldUpdateIpv6)
			 */
#define HDR_MANIP_IPV6_DST	0x10000000
			/**< update IP destination address with the given value
			 * ('dst' field of t_FmPcdManipHdrFieldUpdateIpv6)
			 */

typedef hdr_manip_flags_t	tcp_udp_hdr_manip_update_flags_t;
		/**< TCP/UDP protocol HMan update command flags. */

#define HDR_MANIP_TCP_UDP_SRC	0x80000000
		/**< update TCP/UDP source address with the given value
		 * ('src' field of t_FmPcdManipHdrFieldUpdateTcpUdp)
		 */
#define HDR_MANIP_TCP_UDP_DST	0x40000000
		/**< update TCP/UDP destination address with the given value
		 * ('dst' field of t_FmPcdManipHdrFieldUpdateTcpUdp)
		 */
#define HDR_MANIP_TCP_UDP_CHECKSUM  0x20000000
		/**< update TCP/UDP checksum */

/* @} */

/*
 * @Description   A type used for returning the order of the key extraction.
 *		  each value in this array represents the index of the
 *		  extraction command as defined by the user in the
 *		  initialization extraction array. The valid size of this array
 *		  is the user define number of extractions required (also marked
 *		  by the second '0' in this array).
 */
typedef	uint8_t	t_fm_pcd_kg_key_order[FM_PCD_KG_MAX_NUM_OF_EXTRACTS_PER_KEY];

/*
 * @Collection	  Definitions for CC statistics
 */
#define FM_PCD_CC_STATS_MAX_NUM_OF_FLR	10
	/* Maximal supported number of frame length ranges */
#define FM_PCD_CC_STATS_FLR_SIZE	2
	/* Size in bytes of a frame length range limit */
#define FM_PCD_CC_STATS_COUNTER_SIZE	4
	/* Size in bytes of a frame length range counter */
/* @} */

/*
 * @Description   Parameters for defining CC keys parameters
 *		  The driver supports two methods for CC node allocation:
 *		  dynamic and static. Static mode was created in order to
 *		  prevent runtime alloc/free of FMan memory (MURAM), which may
 *		  cause fragmentation; in this mode, the driver automatically
 *		  allocates the memory according to 'max_num_of_keys' parameter.
 *		  The driver calculates the maximal memory size that may be used
 *		  for this CC-Node taking into consideration 'mask_support' and
 *		  'statistics_mode' parameters. When 'action' =
 *		  e_FM_PCD_ACTION_INDEXED_LOOKUP in the extraction parameters of
 *		  this node, 'max_num_of_keys' must be equal to 'num_of_keys'.
 *		  In dynamic mode, 'max_num_of_keys' must be zero. At
 *		  initialization, all required structures are allocated
 *		  according to 'num_of_keys' parameter. During runtime
 *		  modification, these structures are re-allocated according to
 *		  the updated number of keys.
 *
 *		  Please note that 'action' and 'icIndxMask' mentioned in the
 *		  specific parameter explanations are passed in the extraction
 *		  parameters of the node (fields of
 *		  extractCcParams.extractNonHdr).
 */
typedef struct t_keys_params {
	uint16_t	max_num_of_keys;
		/**< Maximum number of keys that will (ever) be used in this
		 * CC-Node; A value of zero may be used for dynamic memory
		 * allocation.
		 */
	bool		mask_support;
		/**< This parameter is relevant only if a node is initialized
		 * with 'action' = e_FM_PCD_ACTION_EXACT_MATCH and
		 * max_num_of_keys > 0; Should be TRUE to reserve table memory
		 * for key masks, even if initial keys do not contain masks, or
		 * if the node was initialized as 'empty' (without keys); this
		 * will allow user to add keys with masks at runtime.
		 * NOTE that if user want to use only global-masks (i.e. one
		 * common mask for all the entries within this table, this
		 * parameter should set to 'FALSE'.
		 */
	ioc_fm_pcd_cc_stats_mode	statistics_mode;
		/**< Determines the supported statistics mode for all node's
		 * keys. To enable statistics gathering, statistics should be
		 * enabled per every key, using 'statisticsEn' in next engine
		 * parameters structure of that key; If 'max_num_of_keys' is
		 * set, all required structures will be preallocated for all
		 * keys.
		 */
	uint16_t	frame_length_ranges[FM_PCD_CC_STATS_MAX_NUM_OF_FLR];
		/**< Relevant only for 'RMON' statistics mode (this feature is
		 * supported only on B4860 device); Holds a list of programmable
		 * thresholds - for each received frame, its length in bytes is
		 * examined against these range thresholds and the appropriate
		 * counter is incremented by 1 - for example, to belong to range
		 * i, the following should hold: range i-1 threshold < frame
		 * length <= range i threshold. Each range threshold must be
		 * larger then its preceding range threshold, and last range
		 * threshold must be 0xFFFF.
		 */
	uint16_t	num_of_keys;
		/**< Number of initial keys; Note that in case of 'action' =
		 * e_FM_PCD_ACTION_INDEXED_LOOKUP, this field should be
		 * power-of-2 of the number of bits that are set in 'icIndxMask'
		 */
	uint8_t		key_size;
		/**< Size of key - for extraction of type FULL_FIELD, 'key_size'
		 * has to be the standard size of the selected key; For other
		 * extraction types, 'key_size' has to be as size of extraction;
		 * When 'action' = e_FM_PCD_ACTION_INDEXED_LOOKUP, 'key_size'
		 * must be 2.
		 */
	ioc_fm_pcd_cc_key_params_t	key_params[FM_PCD_MAX_NUM_OF_KEYS];
		/**< An array with 'num_of_keys' entries, each entry specifies
		 * the corresponding key parameters; When 'action' =
		 * e_FM_PCD_ACTION_EXACT_MATCH, this value must not exceed 255
		 * (FM_PCD_MAX_NUM_OF_KEYS-1) as the last entry is saved for the
		 * 'miss' entry.
		 */
	ioc_fm_pcd_cc_next_engine_params_t   cc_next_engine_params_for_miss;
		/**< Parameters for defining the next engine when a key is not
		 * matched; Not relevant if action =
		 * e_FM_PCD_ACTION_INDEXED_LOOKUP.
		 */
} t_keys_params;

/*
 * @Description   Parameters for defining custom header manipulation for generic
 *		  field replacement
 */
typedef struct ioc_fm_pcd_manip_hdr_custom_gen_field_replace {
	uint8_t		src_offset;
			/**< Location of new data - Offset from Parse Result
			 * (>= 16, src_offset+size <= 32, )
			 */
	uint8_t		dst_offset;
			/**< Location of data to be overwritten - Offset from
			 * start of frame (dst_offset + size <= 256).
			 */
	uint8_t		size;
			/**< The number of bytes (<=16) to be replaced */
	uint8_t		mask;
			/**< Optional 1 byte mask. Set to select bits for
			 * replacement (1 - bit will be replaced); Clear to use
			 * field as is.
			 */
	uint8_t		mask_offset;
			/**< Relevant if mask != 0; Mask offset within the
			 * replaces "size"
			 */
} ioc_fm_pcd_manip_hdr_custom_gen_field_replace;

/*
 * @Function	  fm_pcd_net_env_characteristics_set
 *
 * @Description   Define a set of Network Environment Characteristics.
 *
 *		  When setting an environment it is important to understand its
 *		  application. It is not meant to describe the flows that will
 *		  run on the ports using this environment, but what the user
 *		  means TO DO with the PCD mechanisms in order to
 *		  parse-classify-distribute those frames.
 *		  By specifying a distinction unit, the user means it would use
 *		  that option for distinction between frames at either a KeyGen
 *		  scheme or a coarse classification action descriptor. Using
 *		  interchangeable headers to define a unit means that the user
 *		  is indifferent to which of the interchangeable headers is
 *		  present in the frame, and wants the distinction to be based on
 *		  the presence of either one of them.
 *
 *		  Depending on context, there are limitations to the use of
 *		  environments. A port using the PCD functionality is bound to
 *		  an environment. Some or even all ports may share an
 *		  environment but also an environment per port is possible. When
 *		  initializing a scheme, a classification plan group (see
 *		  below), or a coarse classification tree, one of the
 *		  initialized environments must be stated and related to. When a
 *		  port is bound to a scheme, a classification plan group, or a
 *		  coarse classification tree, it MUST be bound to the same
 *		  environment.
 *
 *		  The different PCD modules, may relate (for flows definition)
 *		  ONLY on distinction units as defined by their environment.
 *		  When initializing a scheme for example, it may not choose to
 *		  select IPV4 as a match for recognizing flows unless it was
 *		  defined in the relating environment. In fact, to guide the
 *		  user through the configuration of the PCD, each module's
 *		  characterization in terms of flows is not done using protocol
 *		  names, but using environment indexes.
 *
 *		  In terms of HW implementation, the list of distinction units
 *		  sets the LCV vectors and later used for match vector,
 *		  classification plan vectors and coarse classification
 *		  indexing.
 *
 * @Param[in]	  h_fm_pcd		FM PCD module descriptor.
 * @Param[in]	  p_netenv_params	A structure of parameters for the
 *					initialization of the network
 *					environment.
 *
 * @Return	  A handle to the initialized object on success; NULL code
 *		  otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 */
t_handle fm_pcd_net_env_characteristics_set(t_handle h_fm_pcd,
				 ioc_fm_pcd_net_env_params_t *p_netenv_params);

/*
 * @Function	  fm_pcd_net_env_characteristics_delete
 *
 * @Description   Deletes a set of Network Environment Characteristics.
 *
 * @Param[in]	  h_net_env	A handle to the Network environment.
 *
 * @Return	  E_OK on success; Error code otherwise.
 */
uint32_t fm_pcd_net_env_characteristics_delete(t_handle h_net_env);

/*
 * @Function	  fm_pcd_kg_scheme_set
 *
 * @Description   Initializing or modifying and enabling a scheme for the
 *		  KeyGen. This routine should be called for adding or modifying
 *		  a scheme. When a scheme needs modifying, the API requires that
 *		  it will be rewritten. In such a case 'modify' should be TRUE.
 *		  If the routine is called for a valid scheme and 'modify' is
 *		  FALSE, it will return error.
 *
 * @Param[in]	  h_fm_pcd		If this is a new scheme - A handle to an
 *					FM PCD Module. Otherwise NULL (ignored
 *					by driver).
 * @Param[in,out] p_scheme_params	A structure of parameters for defining
 *					the scheme
 *
 * @Return	  A handle to the initialized scheme on success; NULL code
 *		  otherwise. When used as "modify" (rather than for setting a
 *		  new scheme), p_scheme_params->id.h_scheme will return NULL if
 *		  action fails due to scheme BUSY state.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 */
t_handle fm_pcd_kg_scheme_set(t_handle h_fm_pcd,
			    ioc_fm_pcd_kg_scheme_params_t *p_scheme_params);

/*
 * @Function	  fm_pcd_kg_scheme_delete
 *
 * @Description   Deleting an initialized scheme.
 *
 * @Param[in]	  h_scheme	scheme handle as returned by
 *				fm_pcd_kg_scheme_set()
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init() & fm_pcd_kg_scheme_set().
 */
uint32_t	fm_pcd_kg_scheme_delete(t_handle h_scheme);

/*
 * @Function	  fm_pcd_kg_scheme_get_counter
 *
 * @Description   Reads scheme packet counter.
 *
 * @Param[in]	  h_scheme	scheme handle as returned by
 *				fm_pcd_kg_scheme_set().
 *
 * @Return	  Counter's current value.
 *
 * @Cautions	  Allowed only following fm_pcd_init() & fm_pcd_kg_scheme_set().
 */
uint32_t  fm_pcd_kg_scheme_get_counter(t_handle h_scheme);

/*
 * @Function	  fm_pcd_kg_scheme_set_counter
 *
 * @Description   Writes scheme packet counter.
 *
 * @Param[in]	  h_scheme	scheme handle as returned by
 *				fm_pcd_kg_scheme_set().
 * @Param[in]	  value		New scheme counter value - typically '0' for
 *				resetting the counter.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init() & fm_pcd_kg_scheme_set().
 */
uint32_t  fm_pcd_kg_scheme_set_counter(t_handle h_scheme,
			uint32_t value);

/*
 * @Function	  fm_pcd_plcr_profile_set
 *
 * @Description   Sets a profile entry in the policer profile table.
 *		  The routine overrides any existing value.
 *
 * @Param[in]	  h_fm_pcd	A handle to an FM PCD Module.
 * @Param[in]	  p_profile	A structure of parameters for defining a
 *				policer profile entry.
 *
 * @Return	  A handle to the initialized object on success; NULL code
 *		  otherwise. When used as "modify" (rather than for setting a
 *		  new profile), p_profile->id.h_profile will return NULL if
 *		  action fails due to profile BUSY state.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 */
t_handle fm_pcd_plcr_profile_set(t_handle h_fm_pcd,
			       ioc_fm_pcd_plcr_profile_params_t  *p_profile);

/*
 * @Function	  fm_pcd_plcr_profile_delete
 *
 * @Description   Delete a profile entry in the policer profile table.
 *		  The routine set entry to invalid.
 *
 * @Param[in]	  h_profile	A handle to the profile.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 */
uint32_t fm_pcd_plcr_profile_delete(t_handle h_profile);

/*
 * @Function	  fm_pcd_plcr_profile_get_counter
 *
 * @Description   Sets an entry in the classification plan.
 *		  The routine overrides any existing value.
 *
 * @Param[in]	  h_profile	A handle to the profile.
 * @Param[in]	  counter	Counter selector.
 *
 * @Return	  specific counter value.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 */
uint32_t fm_pcd_plcr_profile_get_counter(t_handle	h_profile,
			ioc_fm_pcd_plcr_profile_counters	counter);

/*
 * @Function	  fm_pcd_plcr_profile_set_counter
 *
 * @Description   Sets an entry in the classification plan.
 *		  The routine overrides any existing value.
 *
 * @Param[in]	  h_profile	A handle to the profile.
 * @Param[in]	  counter	Counter selector.
 * @Param[in]	  value		value to set counter with.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 */
uint32_t fm_pcd_plcr_profile_set_counter(t_handle h_profile,
				      ioc_fm_pcd_plcr_profile_counters counter,
					uint32_t		value);

/*
 * @Function	  fm_pcd_cc_root_build
 *
 * @Description   This routine must be called to define a complete coarse
 *		  classification tree. This is the way to define coarse
 *		  classification to a certain flow - the KeyGen schemes may
 *		  point only to trees defined in this way.
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 * @Param[in]	  p_params	A structure of parameters to define the tree.
 *
 * @Return	  A handle to the initialized object on success; NULL code
 *		  otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 */
t_handle fm_pcd_cc_root_build(t_handle h_fm_pcd,
			     ioc_fm_pcd_cc_tree_params_t  *p_params);

/*
 * @Function	  fm_pcd_cc_root_delete
 *
 * @Description   Deleting an built tree.
 *
 * @Param[in]	  h_cc_tree	A handle to a CC tree.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 */
uint32_t fm_pcd_cc_root_delete(t_handle h_cc_tree);

/*
 * @Function	  fm_pcd_cc_root_modify_next_engine
 *
 * @Description   Modify the Next Engine Parameters in the entry of the tree.
 *
 * @Param[in]	  h_cc_tree			A handle to the tree
 * @Param[in]	  grp_id			A Group index in the tree
 * @Param[in]	  index				Entry index in the group
 *						defined by grp_id
 * @Param[in]	  p_fm_pcd_cc_next_engine	Pointer to new next
 *						engine parameters
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following FM_PCD_CcBuildTree().
 */
uint32_t fm_pcd_cc_root_modify_next_engine(t_handle h_cc_tree,
		uint8_t		grp_id,
		uint8_t		index,
		ioc_fm_pcd_cc_next_engine_params_t *p_fm_pcd_cc_next_engine);

/*
 * @Function	  fm_pcd_match_table_set
 *
 * @Description   This routine should be called for each CC (coarse
 *		  classification) node. The whole CC tree should be built bottom
 *		  up so that each node points to already defined nodes.
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 * @Param[in]	  p_param	A structure of parameters defining the CC node
 *
 * @Return	  A handle to the initialized object on success; NULL code
 *		  otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 */
t_handle   fm_pcd_match_table_set(t_handle h_fm_pcd,
		ioc_fm_pcd_cc_node_params_t *p_param);

/*
 * @Function	  fm_pcd_match_table_delete
 *
 * @Description   Deleting an built node.
 *
 * @Param[in]	  h_cc_node	A handle to a CC node.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 */
uint32_t fm_pcd_match_table_delete(t_handle h_cc_node);

/*
 * @Function	  fm_pcd_match_table_modify_miss_next_engine
 *
 * @Description   Modify the Next Engine Parameters of the Miss key case of the
 *		  node.
 *
 * @Param[in]	  h_cc_node				A handle to the node
 * @Param[in]	  p_fm_pcd_cc_next_engine_params	Parameters for defining
 *							next engine
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set(); Not
 *		  relevant in the case the node is of type 'INDEXED_LOOKUP'.
 *		  When configuring nextEngine = e_FM_PCD_CC, note that
 *		  p_fm_pcd_cc_next_engine_params->ccParams.h_cc_node must be
 *		  different from the currently changed table.
 *
 */
uint32_t fm_pcd_match_table_modify_miss_next_engine(t_handle h_cc_node,
	ioc_fm_pcd_cc_next_engine_params_t *p_fm_pcd_cc_next_engine_params);

/*
 * @Function	  fm_pcd_match_table_remove_key
 *
 * @Description   Remove the key (including next engine parameters of this key)
 *		  defined by the index of the relevant node.
 *
 * @Param[in]	  h_cc_node	A handle to the node
 * @Param[in]	  key_index	Key index for removing
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set() was called for
 *		  this node and the nodes that lead to it.
 */
uint32_t fm_pcd_match_table_remove_key(t_handle h_cc_node,
			uint16_t key_index);

/*
 * @Function	  fm_pcd_match_table_add_key
 *
 * @Description   Add the key (including next engine parameters of this key in
 *		  the index defined by the key_index. Note that
 *		  'FM_PCD_LAST_KEY_INDEX' may be used by user that don't care
 *		  about the position of the key in the table - in that case, the
 *		  key will be automatically added by the driver in the last
 *		  available entry.
 *
 * @Param[in]	  h_cc_node	A handle to the node
 * @Param[in]	  key_index	Key index for adding.
 * @Param[in]	  key_size	Key size of added key
 * @Param[in]	  p_key_params	A pointer to the parameters includes new key
 *				with Next Engine Parameters
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set() was called for
 *		  this node and the nodes that lead to it.
 */
uint32_t fm_pcd_match_table_add_key(t_handle h_cc_node,
				uint16_t		key_index,
				uint8_t		key_size,
				ioc_fm_pcd_cc_key_params_t  *p_key_params);

/*
 * @Function	  fm_pcd_match_table_modify_next_engine
 *
 * @Description   Modify the Next Engine Parameters in the relevant key entry of
 *		  the node.
 *
 * @Param[in]	  h_cc_node			A handle to the node
 * @Param[in]	  key_index			Key index for Next
 *						Engine modifications
 * @Param[in]	  p_fm_pcd_cc_next_engine	Parameters for defining
 *						next engine
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set(). When
 *		  configuring nextEngine = e_FM_PCD_CC, note that
 *		  p_fm_pcd_cc_next_engine_params->ccParams.h_cc_node must be
 *		  different from the currently changed table.
 *
 */
uint32_t fm_pcd_match_table_modify_next_engine(t_handle h_cc_node,
		uint16_t		key_index,
		ioc_fm_pcd_cc_next_engine_params_t *p_fm_pcd_cc_next_engine);

/*
 * @Function	  fm_pcd_match_table_modify_key_and_next_engine
 *
 * @Description   Modify the key and Next Engine Parameters of this key in the
 *		  index defined by the key_index.
 *
 * @Param[in]	  h_cc_node		A handle to the node
 * @Param[in]	  key_index		Key index for adding
 * @Param[in]	  key_size		Key size of added key
 * @Param[in]	  p_key_params		A pointer to the parameters includes
 *					modified key and modified Next Engine
 *					Params
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	Allowed only following fm_pcd_match_table_set() was called for
 *		this node and the nodes that lead to it. When configuring
 *		nextEngine = e_FM_PCD_CC, note that
 *		p_fm_pcd_cc_next_engine_params->ccParams.h_cc_node must be
 *		different from the currently changed table.
 */
uint32_t fm_pcd_match_table_modify_key_and_next_engine(t_handle h_cc_node,
				uint16_t		key_index,
				uint8_t		key_size,
				ioc_fm_pcd_cc_key_params_t  *p_key_params);

/*
 * @Function	  fm_pcd_match_table_modify_key
 *
 * @Description   Modify the key in the index defined by the key_index.
 *
 * @Param[in]	  h_cc_node		A handle to the node
 * @Param[in]	  key_index		Key index for adding
 * @Param[in]	  key_size		Key size of added key
 * @Param[in]	  p_key			A pointer to the new key
 * @Param[in]	  p_mask		A pointer to the new mask if relevant,
 *					otherwise pointer to NULL
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set() was called for
 *		  this node and the nodes that lead to it.
 */
uint32_t fm_pcd_match_table_modify_key(t_handle h_cc_node,
				uint16_t key_index,
				uint8_t  key_size,
				uint8_t  *p_key,
				uint8_t  *p_mask);

/*
 * @Function	  fm_pcd_match_table_find_nremove_key
 *
 * @Description   Remove the key (including next engine parameters of this key)
 *		  defined by the key and mask. Note that this routine will
 *		  search the node to locate the index of the required key
 *		  (& mask) to remove.
 *
 * @Param[in]	  h_cc_node	A handle to the node
 * @Param[in]	  key_size	Key size of the one to remove.
 * @Param[in]	  p_key		A pointer to the requested key to remove.
 * @Param[in]	  p_mask	A pointer to the mask if relevant,
 *				otherwise pointer to NULL
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set() was called for
 *		  this node and the nodes that lead to it.
 */
uint32_t fm_pcd_match_table_find_nremove_key(t_handle h_cc_node,
					uint8_t  key_size,
					uint8_t  *p_key,
					uint8_t  *p_mask);

/*
 * @Function	  fm_pcd_match_table_find_nmodify_next_engine
 *
 * @Description   Modify the Next Engine Parameters in the relevant key entry of
 *		  the node. Note that this routine will search the node to
 *		  locate the index of the required key (& mask) to modify.
 *
 * @Param[in]	  h_cc_node		A handle to the node
 * @Param[in]	  key_size		Key size of the one to modify.
 * @Param[in]	  p_key			A pointer to the requested key to modify
 * @Param[in]	  p_mask		A pointer to the mask if relevant,
 *					otherwise pointer to NULL
 * @Param[in]	  p_fm_pcd_cc_next_engine	Parameters for defining
 *							next engine
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set(). When
 *		  configuring nextEngine = e_FM_PCD_CC, note that
 *		  p_fm_pcd_cc_next_engine_params->ccParams.h_cc_node must be
 *		  different from the currently changed table.
 */
uint32_t fm_pcd_match_table_find_nmodify_next_engine(t_handle h_cc_node,
		uint8_t		key_size,
		uint8_t		*p_key,
		uint8_t		*p_mask,
		ioc_fm_pcd_cc_next_engine_params_t *p_fm_pcd_cc_next_engine);

/*
 * @Function	 fm_pcd_match_table_find_nmodify_key_and_next_engine
 *
 * @Description   Modify the key and Next Engine Parameters of this key in the
 *		  index defined by the key_index. Note that this routine will
 *		  search the node to locate the index of the required key
 *		  (& mask) to modify.
 *
 * @Param[in]	  h_cc_node		A handle to the node
 * @Param[in]	  key_size		Key size of the one to modify.
 * @Param[in]	  p_key			A pointer to the requested key to modify
 * @Param[in]	  p_mask		A pointer to the mask if relevant,
 *					otherwise pointer to NULL
 * @Param[in]	  p_key_params		A pointer to the parameters includes
 *					modified key and modified Next Engine
 *					Params
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set() was called for
 *		  this node and the nodes that lead to it.
 *		  When configuring nextEngine = e_FM_PCD_CC, note that
 *		  p_fm_pcd_cc_next_engine_params->ccParams.h_cc_node must be
 *		  different from the currently changed table.
 */
uint32_t fm_pcd_match_table_find_nmodify_key_and_next_engine(t_handle h_cc_node,
				uint8_t key_size,
				uint8_t *p_key,
				uint8_t *p_mask,
				ioc_fm_pcd_cc_key_params_t *p_key_params);

/*
 * @Function	  fm_pcd_match_table_find_nmodify_key
 *
 * @Description   Modify the key  in the index defined by the key_index. Note
 *		  that this routine will search the node to locate the index of
 *		  the required key (& mask) to modify.
 *
 * @Param[in]	  h_cc_node	A handle to the node
 * @Param[in]	  key_size	Key size of the one to modify.
 * @Param[in]	  p_key		A pointer to the requested key to modify.
 * @Param[in]	  p_mask	A pointer to the mask if relevant,
 *				otherwise pointer to NULL
 * @Param[in]	  p_new_key	A pointer to the new key
 * @Param[in]	  p_new_mask	A pointer to the new mask if relevant,
 *				otherwise pointer to NULL
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set() was called for
 *		  this node and the nodes that lead to it.
 */
uint32_t fm_pcd_match_table_find_nmodify_key(t_handle h_cc_node,
					uint8_t  key_size,
					uint8_t  *p_key,
					uint8_t  *p_mask,
					uint8_t  *p_new_key,
					uint8_t  *p_new_mask);

/*
 * @Function	  fm_pcd_match_table_get_key_counter
 *
 * @Description   This routine may be used to get a counter of specific key in a
 *		  CC Node; This counter reflects how many frames passed that
 *		  were matched this key.
 *
 * @Param[in]	  h_cc_node	A handle to the node
 * @Param[in]	  key_index	Key index for adding
 *
 * @Return	  The specific key counter.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set().
 */
uint32_t fm_pcd_match_table_get_key_counter(t_handle h_cc_node,
				uint16_t key_index);

/*
 * @Function	  fm_pcd_match_table_get_key_statistics
 *
 * @Description   This routine may be used to get statistics counters of
 *		  specific key in a CC Node.
 *
 *		  If 'e_FM_PCD_CC_STATS_MODE_FRAME' and
 *		  'e_FM_PCD_CC_STATS_MODE_BYTE_AND_FRAME' were set for this
 *		  node, these counters reflect how many frames passed that were
 *		  matched this key; The total frames count will be returned in
 *		  the counter of the first range (as only one frame length range
 *		  was defined). If 'e_FM_PCD_CC_STATS_MODE_RMON' was set for
 *		  this node, the total frame count will be separated to frame
 *		  length counters, based on provided frame length ranges.
 *
 * @Param[in]	  h_cc_node		A handle to the node
 * @Param[in]	  key_index		Key index for adding
 * @Param[out]	  p_key_statistics	Key statistics counters
 *
 * @Return	  The specific key statistics.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set().
 */
uint32_t fm_pcd_match_table_get_key_statistics(t_handle h_cc_node,
			uint16_t		key_index,
			ioc_fm_pcd_cc_key_statistics_t	*p_key_statistics);

/*
 * @Function	  fm_pcd_match_table_get_miss_statistics
 *
 * @Description   This routine may be used to get statistics counters of miss
 *		  entry in a CC Node.
 *
 *		  If 'e_FM_PCD_CC_STATS_MODE_FRAME' and
 *		  'e_FM_PCD_CC_STATS_MODE_BYTE_AND_FRAME' were set for this
 *		  node, these counters reflect how many frames were not matched
 *		  to any existing key and therefore passed through the miss
 *		  entry; The total frames count will be returned in the counter
 *		  of the first range (as only one frame length range was
 *		  defined).
 *
 * @Param[in]	  h_cc_node		A handle to the node
 * @Param[out]	  p_miss_statistics	Statistics counters for 'miss'
 *
 * @Return	  The statistics for 'miss'.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set().
 */
uint32_t fm_pcd_match_table_get_miss_statistics(t_handle h_cc_node,
		    ioc_fm_pcd_cc_key_statistics_t	*p_miss_statistics);

/*
 * @Function	  fm_pcd_match_table_find_nget_key_statistics
 *
 * @Description   This routine may be used to get statistics counters of
 *		  specific key in a CC Node.
 *
 *		  If 'e_FM_PCD_CC_STATS_MODE_FRAME' and
 *		  'e_FM_PCD_CC_STATS_MODE_BYTE_AND_FRAME' were set for this
 *		  node, these counters reflect how many frames passed that were
 *		  matched this key; The total frames count will be returned in
 *		  the counter of the first range (as only one frame length range
 *		  was defined). If 'e_FM_PCD_CC_STATS_MODE_RMON' was set for
 *		  this node, the total frame count will be separated to frame
 *		  length counters, based on provided frame length ranges.
 *		  Note that this routine will search the node to locate the
 *		  index of the required key based on received key parameters.
 *
 * @Param[in]	  h_cc_node		A handle to the node
 * @Param[in]	  key_size		Size of the requested key
 * @Param[in]	  p_key			A pointer to the requested key
 * @Param[in]	  p_mask		A pointer to the mask if relevant,
 *					otherwise pointer to NULL
 * @Param[out]	  p_key_statistics	Key statistics counters
 *
 * @Return	  The specific key statistics.
 *
 * @Cautions	  Allowed only following fm_pcd_match_table_set().
 */
uint32_t fm_pcd_match_table_find_nget_key_statistics(t_handle h_cc_node,
			uint8_t		key_size,
			uint8_t		*p_key,
			uint8_t		*p_mask,
			ioc_fm_pcd_cc_key_statistics_t   *p_key_statistics);

/*
 * @Function	  fm_pcd_match_table_get_next_engine
 *
 * @Description   Gets NextEngine of the relevant key_index.
 *
 * @Param[in]	  h_cc_node				A handle to the node.
 * @Param[in]	  key_index				key_index in the
 *							relevant node.
 * @Param[out]	  p_fm_pcd_cc_next_engine_params	here updated
 *							nextEngine parameters
 *							for the relevant
 *							key_index of the CC Node
 *							received as parameter to
 *							this function
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 */
uint32_t fm_pcd_match_table_get_next_engine(t_handle	h_cc_node,
	uint16_t			key_index,
	ioc_fm_pcd_cc_next_engine_params_t *p_fm_pcd_cc_next_engine_params);

/*
 * @Function	  fm_pcd_match_table_get_indexed_hash_bucket
 *
 * @Description   This routine simulates KeyGen operation on the provided key
 *		  and calculates to which hash bucket it will be mapped.
 *
 * @Param[in]	  h_cc_node			A handle to the node.
 * @Param[in]	  kg_key_size			Key size as it was configured in
 *						the KG scheme that leads to this
 *						hash.
 * @Param[in]	  p_kg_key			Pointer to the key; must be like
 *						the key that the KG is
 *						generated, i.e. the same
 *						extraction and with mask if
 *						exist.
 * @Param[in]	  kg_hash_shift			Hash-shift as it was configured
 *						in the KG scheme that leads to
 *						this hash.
 * @Param[out]	  p_cc_node_bucket_handle	Pointer to the bucket of the
 *						provided key.
 * @Param[out]	  p_bucket_index		Index to the bucket of the
 *						provided key
 * @Param[out]	  p_last_index			Pointer to last index in the
 *						bucket of the provided key.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_hash_table_set()
 */
uint32_t fm_pcd_match_table_get_indexed_hash_bucket(t_handle h_cc_node,
				uint8_t	kg_key_size,
				uint8_t	*p_kg_key,
				uint8_t	kg_hash_shift,
				t_handle	*p_cc_node_bucket_handle,
				uint8_t	*p_bucket_index,
				uint16_t	*p_last_index);

/*
 * @Function	  fm_pcd_hash_table_set
 *
 * @Description   This routine initializes a hash table structure.
 *		  KeyGen hash result determines the hash bucket.
 *		  Next, KeyGen key is compared against all keys of this bucket
 *		  (exact match).
 *		  Number of sets (number of buckets) of the hash equals to the
 *		  number of 1-s in 'hashResMask' in the provided parameters.
 *		  Number of hash table ways is then calculated by dividing
 *		  'max_num_of_keys' equally between the hash sets. This is the
 *		  maximal number of keys that a hash bucket may hold.
 *		  The hash table is initialized empty and keys may be added to
 *		  it following the initialization. Keys masks are not supported
 *		  in current hash table implementation.
 *		  The initialized hash table can be integrated as a node in a CC
 *		  tree.
 *
 * @Param[in]	  h_fm_pcd	FM PCD module descriptor.
 * @Param[in]	  p_param	A structure of parameters defining the hash
 *				table
 *
 * @Return	  A handle to the initialized object on success; NULL code
 *		  otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 */
t_handle fm_pcd_hash_table_set(t_handle h_fm_pcd,
	ioc_fm_pcd_hash_table_params_t *p_param);

/*
 * @Function	  fm_pcd_hash_table_delete
 *
 * @Description   This routine deletes the provided hash table and released all
 *		  its allocated resources.
 *
 * @Param[in]	  h_hash_tbl	A handle to a hash table
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_hash_table_set().
 */
uint32_t fm_pcd_hash_table_delete(t_handle h_hash_tbl);

/*
 * @Function	  fm_pcd_hash_table_add_key
 *
 * @Description   This routine adds the provided key (including next engine
 *		  parameters of this key) to the hash table.
 *		  The key is added as the last key of the bucket that it is
 *		  mapped to.
 *
 * @Param[in]	  h_hash_tbl	A handle to a hash table
 * @Param[in]	  key_size	Key size of added key
 * @Param[in]	  p_key_params  A pointer to the parameters includes
 *				new key with next engine parameters; The pointer
 *				to the key mask must be NULL, as masks are not
 *				supported in hash table implementation.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_hash_table_set().
 */
uint32_t fm_pcd_hash_table_add_key(t_handle h_hash_tbl,
				uint8_t		key_size,
				ioc_fm_pcd_cc_key_params_t  *p_key_params);

/*
 * @Function	  fm_pcd_hash_table_remove_key
 *
 * @Description   This routine removes the requested key (including next engine
 *		  parameters of this key) from the hash table.
 *
 * @Param[in]	  h_hash_tbl	A handle to a hash table
 * @Param[in]	  key_size	Key size of the one to remove.
 * @Param[in]	  p_key		A pointer to the requested key to remove.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_hash_table_set().
 */
uint32_t fm_pcd_hash_table_remove_key(t_handle h_hash_tbl,
				uint8_t  key_size,
				uint8_t  *p_key);

/*
 * @Function	  fm_pcd_hash_table_modify_next_engine
 *
 * @Description   This routine modifies the next engine for the provided key.
 *		  The key should be previously added to the hash table.
 *
 * @Param[in]	  h_hash_tbl			A handle to a hash table
 * @Param[in]	  key_size			Key size of the key to modify.
 * @Param[in]	  p_key				A pointer to the requested key
 *						to modify.
 * @Param[in]	  p_fm_pcd_cc_next_engine	A structure for defining
 *						new next engine parameters.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_hash_table_set().
 *		  When configuring nextEngine = e_FM_PCD_CC, note that
 *		  p_fm_pcd_cc_next_engine_params->ccParams.h_cc_node must be
 *		  different from the currently changed table.
 */
uint32_t fm_pcd_hash_table_modify_next_engine(t_handle h_hash_tbl,
		uint8_t		key_size,
		uint8_t		*p_key,
		ioc_fm_pcd_cc_next_engine_params_t *p_fm_pcd_cc_next_engine);

/*
 * @Function	  fm_pcd_hash_table_modify_miss_next_engine
 *
 * @Description   This routine modifies the next engine on key match miss.
 *
 * @Param[in]	  h_hash_tbl			A handle to a hash table
 * @Param[in]	  p_fm_pcd_cc_next_engine	A structure for defining
 *						new next engine parameters.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_hash_table_set().
 *		  When configuring nextEngine = e_FM_PCD_CC, note that
 *		  p_fm_pcd_cc_next_engine_params->ccParams.h_cc_node must be
 *		  different from the currently changed table.
 */
uint32_t fm_pcd_hash_table_modify_miss_next_engine(t_handle h_hash_tbl,
	      ioc_fm_pcd_cc_next_engine_params_t *p_fm_pcd_cc_next_engine);

/*
 * @Function	  fm_pcd_hash_table_get_miss_next_engine
 *
 * @Description   Gets NextEngine in case of key match miss.
 *
 * @Param[in]	  h_hash_tbl				A handle to a hash table
 * @Param[out]	  p_fm_pcd_cc_next_engine_params	Next engine parameters
 *							for the specified hash
 *							table.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_hash_table_set().
 */
uint32_t fm_pcd_hash_table_get_miss_next_engine(t_handle	h_hash_tbl,
	ioc_fm_pcd_cc_next_engine_params_t *p_fm_pcd_cc_next_engine_params);

/*
 * @Function	  fm_pcd_hash_table_find_nget_key_statistics
 *
 * @Description   This routine may be used to get statistics counters of
 *		  specific key in a hash table.
 *
 *		  If 'e_FM_PCD_CC_STATS_MODE_FRAME' and
 *		  'e_FM_PCD_CC_STATS_MODE_BYTE_AND_FRAME' were set for this
 *		  node, these counters reflect how many frames passed that were
 *		  matched this key; The total frames count will be returned in
 *		  the counter of the first range (as only one frame length range
 *		  was defined). If 'e_FM_PCD_CC_STATS_MODE_RMON' was set for
 *		  this node, the total frame count will be separated to frame
 *		  length counters, based on provided frame length ranges. Note
 *		  that this routine will identify the bucket of this key in the
 *		  hash table and will search the bucket to locate the index of
 *		  the required key based on received key parameters.
 *
 * @Param[in]	  h_hash_tbl		A handle to a hash table
 * @Param[in]	  key_size		Size of the requested key
 * @Param[in]	  p_key			A pointer to the requested key
 * @Param[out]	  p_key_statistics	Key statistics counters
 *
 * @Return	  The specific key statistics.
 *
 * @Cautions	  Allowed only following fm_pcd_hash_table_set().
 */
uint32_t fm_pcd_hash_table_find_nget_key_statistics(t_handle h_hash_tbl,
			uint8_t		key_size,
			uint8_t		*p_key,
			ioc_fm_pcd_cc_key_statistics_t   *p_key_statistics);

/*
 * @Function	  fm_pcd_hash_table_get_miss_statistics
 *
 * @Description   This routine may be used to get statistics counters of 'miss'
 *		  entry of the a hash table.
 *
 *		  If 'e_FM_PCD_CC_STATS_MODE_FRAME' and
 *		  'e_FM_PCD_CC_STATS_MODE_BYTE_AND_FRAME' were set for this
 *		  node, these counters reflect how many frames were not matched
 *		  to any existing key and therefore passed through the miss
 *		  entry;
 *
 * @Param[in]	  h_hash_tbl		A handle to a hash table
 * @Param[out]	  p_miss_statistics	Statistics counters for 'miss'
 *
 * @Return	  The statistics for 'miss'.
 *
 * @Cautions	  Allowed only following fm_pcd_hash_table_set().
 */
uint32_t fm_pcd_hash_table_get_miss_statistics(t_handle	h_hash_tbl,
			ioc_fm_pcd_cc_key_statistics_t   *p_miss_statistics);

/*
 * @Function	  fm_pcd_manip_node_set
 *
 * @Description   This routine should be called for defining a manipulation
 *		  node. A manipulation node must be defined before the CC node
 *		  that precedes it.
 *
 * @Param[in]	  h_fm_pcd			FM PCD module descriptor.
 * @Param[in]	  p_fm_pcd_manip_params		A structure of parameters
 *						defining the manipulation
 *
 * @Return	  A handle to the initialized object on success; NULL code
 *		  otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 */
t_handle fm_pcd_manip_node_set(t_handle h_fm_pcd,
	ioc_fm_pcd_manip_params_t *p_fm_pcd_manip_params);

/*
 * @Function	  fm_pcd_manip_node_delete
 *
 * @Description   Delete an existing manipulation node.
 *
 * @Param[in]	  h_manip_node		A handle to a manipulation node.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_manip_node_set().
 */
uint32_t  fm_pcd_manip_node_delete(t_handle h_manip_node);

/*
 * @Function	  fm_pcd_manip_get_statistics
 *
 * @Description   Retrieve the manipulation statistics.
 *
 * @Param[in]	  h_manip_node		A handle to a manipulation node.
 * @Param[out]	  p_fm_pcd_manip_stats	A structure for retrieving the
 *					manipulation statistics
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_manip_node_set().
 */
uint32_t fm_pcd_manip_get_statistics(t_handle h_manip_node,
	ioc_fm_pcd_manip_stats_t *p_fm_pcd_manip_stats);

/*
 * @Function	  fm_pcd_manip_node_replace
 *
 * @Description   Change existing manipulation node to be according to new
 *		  requirement.
 *
 * @Param[in]	  h_manip_node		A handle to a manipulation node.
 * @Param[out]	  p_manip_params	A structure of parameters defining the
 *					change requirement
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_manip_node_set().
 */
uint32_t fm_pcd_manip_node_replace(t_handle h_manip_node,
ioc_fm_pcd_manip_params_t *p_manip_params);

/*
 * @Function	  fm_pcd_frm_replic_set_group
 *
 * @Description   Initialize a Frame Replicator group.
 *
 * @Param[in]	  h_fm_pcd			FM PCD module descriptor.
 * @Param[in]	  p_frm_replic_group_param	A structure of parameters for
 *						the initialization of the frame
 *						replicator group.
 *
 * @Return	  A handle to the initialized object on success; NULL code
 *		  otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_init().
 */
t_handle fm_pcd_frm_replic_set_group(t_handle h_fm_pcd,
		ioc_fm_pcd_frm_replic_group_params_t *p_frm_replic_group_param);

/*
 * @Function	  fm_pcd_frm_replic_delete_group
 *
 * @Description   Delete a Frame Replicator group.
 *
 * @Param[in]	  h_frm_replic_group	A handle to the frame replicator group.
 *
 * @Return	  E_OK on success;  Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_frm_replic_set_group().
 */
uint32_t fm_pcd_frm_replic_delete_group(t_handle h_frm_replic_group);

/*
 * @Function	  fm_pcd_frm_replic_add_member
 *
 * @Description   Add the member in the index defined by the member_index.
 *
 * @Param[in]	  h_frm_replic_group	A handle to the frame replicator group.
 * @Param[in]	  member_index		member index for adding.
 * @Param[in]	  p_member_params	A pointer to the new member parameters.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_frm_replic_set_group() of this
 *		  group.
 */
uint32_t fm_pcd_frm_replic_add_member(t_handle h_frm_replic_group,
			uint16_t		member_index,
			ioc_fm_pcd_cc_next_engine_params_t *p_member_params);

/*
 * @Function	  fm_pcd_frm_replic_remove_member
 *
 * @Description   Remove the member defined by the index from the relevant
 *		  group.
 *
 * @Param[in]	  h_frm_replic_group	A handle to the frame replicator group.
 * @Param[in]	  member_index		member index for removing.
 *
 * @Return	  E_OK on success; Error code otherwise.
 *
 * @Cautions	  Allowed only following fm_pcd_frm_replic_set_group() of this
 * group.
 */
uint32_t fm_pcd_frm_replic_remove_member(t_handle h_frm_replic_group,
				      uint16_t member_index);

/** @} */ /* end of FM_PCD_Runtime_build_grp group */
/** @} */ /* end of FM_PCD_Runtime_grp group */
/** @} */ /* end of FM_PCD_grp group */
/** @} */ /* end of FM_grp group */

#endif /* __FM_PCD_EXT_H */
