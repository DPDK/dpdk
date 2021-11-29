/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2012 Freescale Semiconductor Inc.
 * Copyright 2017-2020 NXP
 */

#ifndef __FM_EXT_H
#define __FM_EXT_H

#include "ncsw_ext.h"
#include "dpaa_integration.h"

#define FM_IOC_TYPE_BASE	(NCSW_IOC_TYPE_BASE + 1)
#define FMT_IOC_TYPE_BASE	(NCSW_IOC_TYPE_BASE + 3)

#define MODULE_FM		0x00010000
#define __ERR_MODULE__		MODULE_FM

/* #define FM_LIB_DBG */

#if defined(FM_LIB_DBG)
	#define _fml_dbg(fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaa_logtype_pmd, "fmlib:%s(): " fmt "\n", \
			__func__, ##args)
#else
	#define _fml_dbg(arg...)
#endif

/*#define FM_IOCTL_DBG*/

#if defined(FM_IOCTL_DBG)
	#define _fm_ioctl_dbg(fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaa_logtype_pmd, "fmioc:%s(): " fmt "\n", \
				__func__, ##args)
#else
	#define _fm_ioctl_dbg(arg...)
#endif

/*
 * @Group	lnx_ioctl_ncsw_grp	NetCommSw Linux User-Space (IOCTL) API
 * @{
 */

#define NCSW_IOC_TYPE_BASE	0xe0
	/**< defines the IOCTL type for all the NCSW Linux module commands */

/*
 * @Group	  lnx_usr_FM_grp Frame Manager API
 *
 * @Description   FM API functions, definitions and enums.
 *
 *	  The FM module is the main driver module and is a mandatory
 *	  module for FM driver users. This module must be initialized
 *	  first prior to any other drivers modules.
 *	  The FM is a "singleton" module. It is responsible of the
 *	  common HW modules: FPM, DMA, common QMI and common BMI
 *	  initializations and run-time control routines. This module
 *	  must be initialized always when working with any of the FM modules.
 *	  NOTE - We assume that the FM library will be initialized only
 *	  by core No. 0!
 *
 * @{
 */

/*
 * @Description   Enum for defining port types
 */
typedef enum e_fm_port_type {
	e_FM_PORT_TYPE_OH_OFFLINE_PARSING = 0,  /**< Offline parsing port */
	e_FM_PORT_TYPE_RX,			/**< 1G Rx port */
	e_FM_PORT_TYPE_RX_10G,			/**< 10G Rx port */
	e_FM_PORT_TYPE_TX,			/**< 1G Tx port */
	e_FM_PORT_TYPE_TX_10G,			/**< 10G Tx port */
	e_FM_PORT_TYPE_RX_2_5G,			/**< 2.5G Rx port */
	e_FM_PORT_TYPE_TX_2_5G,			/**< 2.5G Tx port */
	e_FM_PORT_TYPE_DUMMY
} e_fm_port_type;

/*
 * @Description   Parse results memory layout
 */
typedef struct t_fm_prs_result {
	volatile uint8_t	lpid;		/**< Logical port id */
	volatile uint8_t	shimr;		/**< Shim header result  */
	volatile uint16_t	l2r;		/**< Layer 2 result */
	volatile uint16_t	l3r;		/**< Layer 3 result */
	volatile uint8_t	l4r;		/**< Layer 4 result */
	volatile uint8_t	cplan;		/**< Classification plan id */
	volatile uint16_t	nxthdr;		/**< Next Header  */
	volatile uint16_t	cksum;		/**< Running-sum */
	volatile uint16_t	flags_frag_off;
			/**<Flags & fragment-offset field of the last IP-header
			 */
	volatile uint8_t	route_type;
			/**< Routing type field of a IPv6 routing extension
			 * header
			 */
	volatile uint8_t	rhp_ip_valid;
			/**< Routing Extension Header Present; last bit is IP
			 * valid
			 */
	volatile uint8_t	shim_off[2];	/**< Shim offset */
	volatile uint8_t	ip_pid_off;
			/**< IP PID (last IP-proto)offset */
	volatile uint8_t	eth_off;	/**< ETH offset */
	volatile uint8_t	llc_snap_off;	/**< LLC_SNAP offset */
	volatile uint8_t	vlan_off[2];	/**< VLAN offset */
	volatile uint8_t	etype_off;	/**< ETYPE offset */
	volatile uint8_t	pppoe_off;	/**< PPP offset */
	volatile uint8_t	mpls_off[2];	/**< MPLS offset */
	volatile uint8_t	ip_off[2];	/**< IP offset */
	volatile uint8_t	gre_off;	/**< GRE offset */
	volatile uint8_t	l4_off;		/**< Layer 4 offset */
	volatile uint8_t	nxthdr_off;	/**< Parser end point */
} __rte_packed t_fm_prs_result;

/*
 * @Collection   FM Parser results
 */
#define FM_PR_L2_VLAN_STACK	0x00000100  /**< Parse Result: VLAN stack */
#define FM_PR_L2_ETHERNET	0x00008000  /**< Parse Result: Ethernet*/
#define FM_PR_L2_VLAN		0x00004000  /**< Parse Result: VLAN */
#define FM_PR_L2_LLC_SNAP	0x00002000  /**< Parse Result: LLC_SNAP */
#define FM_PR_L2_MPLS		0x00001000  /**< Parse Result: MPLS */
#define FM_PR_L2_PPPoE		0x00000800  /**< Parse Result: PPPoE */
/* @} */

/*
 * @Collection   FM Frame descriptor macros
 */
#define FM_FD_CMD_FCO		0x80000000  /**< Frame queue Context Override */
#define FM_FD_CMD_RPD		0x40000000  /**< Read Prepended Data */
#define FM_FD_CMD_UPD		0x20000000  /**< Update Prepended Data */
#define FM_FD_CMD_DTC		0x10000000  /**< Do L4 Checksum */
#define FM_FD_CMD_DCL4C		0x10000000  /**< Didn't calculate L4 Checksum */
#define FM_FD_CMD_CFQ		0x00ffffff  /**< Confirmation Frame Queue */

#define FM_FD_ERR_UNSUPPORTED_FORMAT	0x04000000
					/**< Not for Rx-Port! Unsupported Format
					 */
#define FM_FD_ERR_LENGTH	0x02000000
					/**< Not for Rx-Port! Length Error */
#define FM_FD_ERR_DMA		0x01000000  /**< DMA Data error */

#define FM_FD_IPR		0x00000001  /**< IPR frame (not error) */

#define FM_FD_ERR_IPR_NCSP	(0x00100000 | FM_FD_IPR)
						/**< IPR non-consistent-sp */
#define FM_FD_ERR_IPR		(0x00200000 | FM_FD_IPR) /**< IPR error */
#define FM_FD_ERR_IPR_TO	(0x00300000 | FM_FD_IPR) /**< IPR timeout */

#ifdef FM_CAPWAP_SUPPORT
#define FM_FD_ERR_CRE		0x00200000
#define FM_FD_ERR_CHE		0x00100000
#endif /* FM_CAPWAP_SUPPORT */

#define FM_FD_ERR_PHYSICAL	0x00080000
			/**< Rx FIFO overflow, FCS error, code error, running
			 * disparity error (SGMII and TBI modes), FIFO parity
			 * error. PHY Sequence error, PHY error control
			 * character detected.
			 */
#define FM_FD_ERR_SIZE		0x00040000
		/**< Frame too long OR Frame size exceeds max_length_frame */
#define FM_FD_ERR_CLS_DISCARD	0x00020000  /**< classification discard */
#define FM_FD_ERR_EXTRACTION	0x00008000  /**< Extract Out of Frame */
#define FM_FD_ERR_NO_SCHEME	0x00004000  /**< No Scheme Selected */
#define FM_FD_ERR_KEYSIZE_OVERFLOW	0x00002000  /**< Keysize Overflow */
#define FM_FD_ERR_COLOR_RED	0x00000800  /**< Frame color is red */
#define FM_FD_ERR_COLOR_YELLOW	0x00000400  /**< Frame color is yellow */
#define FM_FD_ERR_ILL_PLCR	0x00000200
				/**< Illegal Policer Profile selected */
#define FM_FD_ERR_PLCR_FRAME_LEN 0x00000100  /**< Policer frame length error */
#define FM_FD_ERR_PRS_TIMEOUT	0x00000080  /**< Parser Time out Exceed */
#define FM_FD_ERR_PRS_ILL_INSTRUCT 0x00000040
					/**< Invalid Soft Parser instruction */
#define FM_FD_ERR_PRS_HDR_ERR	0x00000020
		/**< Header error was identified during parsing */
#define FM_FD_ERR_BLOCK_LIMIT_EXCEEDED  0x00000008
			/**< Frame parsed beyond 256 first bytes */

#define FM_FD_TX_STATUS_ERR_MASK	(FM_FD_ERR_UNSUPPORTED_FORMAT   | \
					FM_FD_ERR_LENGTH		| \
					FM_FD_ERR_DMA) /**< TX Error FD bits */

#define FM_FD_RX_STATUS_ERR_MASK	(FM_FD_ERR_UNSUPPORTED_FORMAT   | \
					FM_FD_ERR_LENGTH		| \
					FM_FD_ERR_DMA		| \
					FM_FD_ERR_IPR		| \
					FM_FD_ERR_IPR_TO		| \
					FM_FD_ERR_IPR_NCSP		| \
					FM_FD_ERR_PHYSICAL		| \
					FM_FD_ERR_SIZE		| \
					FM_FD_ERR_CLS_DISCARD	| \
					FM_FD_ERR_COLOR_RED		| \
					FM_FD_ERR_COLOR_YELLOW	| \
					FM_FD_ERR_ILL_PLCR		| \
					FM_FD_ERR_PLCR_FRAME_LEN	| \
					FM_FD_ERR_EXTRACTION	| \
					FM_FD_ERR_NO_SCHEME		| \
					FM_FD_ERR_KEYSIZE_OVERFLOW	| \
					FM_FD_ERR_PRS_TIMEOUT	| \
					FM_FD_ERR_PRS_ILL_INSTRUCT	| \
					FM_FD_ERR_PRS_HDR_ERR	| \
					FM_FD_ERR_BLOCK_LIMIT_EXCEEDED)
					/**< RX Error FD bits */

#define FM_FD_RX_STATUS_ERR_NON_FM	0x00400000
					/**< non Frame-Manager error */
/* @} */

/*
 * @Description   FM Exceptions
 */
typedef enum e_fm_exceptions {
	e_FM_EX_DMA_BUS_ERROR = 0,	/**< DMA bus error. */
	e_FM_EX_DMA_READ_ECC,
		/**< Read Buffer ECC error (Valid for FM rev < 6)*/
	e_FM_EX_DMA_SYSTEM_WRITE_ECC,
		/**< Write Buffer ECC error on system side
		 * (Valid for FM rev < 6)
		 */
	e_FM_EX_DMA_FM_WRITE_ECC,
		/**< Write Buffer ECC error on FM side (Valid for FM rev < 6)*/
	e_FM_EX_DMA_SINGLE_PORT_ECC,
		/**< Single Port ECC error on FM side (Valid for FM rev > 6)*/
	e_FM_EX_FPM_STALL_ON_TASKS,	/**< Stall of tasks on FPM */
	e_FM_EX_FPM_SINGLE_ECC,		/**< Single ECC on FPM. */
	e_FM_EX_FPM_DOUBLE_ECC,
		/**< Double ECC error on FPM ram access */
	e_FM_EX_QMI_SINGLE_ECC,		/**< Single ECC on QMI. */
	e_FM_EX_QMI_DOUBLE_ECC,		/**< Double bit ECC occurred on QMI */
	e_FM_EX_QMI_DEQ_FROM_UNKNOWN_PORTID,/**< Dequeue from unknown port id */
	e_FM_EX_BMI_LIST_RAM_ECC,	/**< Linked List RAM ECC error */
	e_FM_EX_BMI_STORAGE_PROFILE_ECC,/**< Storage Profile ECC Error */
	e_FM_EX_BMI_STATISTICS_RAM_ECC,
		/**< Statistics Count RAM ECC Error Enable */
	e_FM_EX_BMI_DISPATCH_RAM_ECC,	/**< Dispatch RAM ECC Error Enable */
	e_FM_EX_IRAM_ECC,		/**< Double bit ECC occurred on IRAM*/
	e_FM_EX_MURAM_ECC		/**< Double bit ECC occurred on MURAM*/
} e_fm_exceptions;

/*
 * @Description   Enum for defining port DMA cache attributes
 */
typedef enum e_fm_dma_cache_option {
	e_FM_DMA_NO_STASH = 0,	/**< Cacheable, no Allocate (No Stashing) */
	e_FM_DMA_STASH = 1	/**< Cacheable and Allocate (Stashing on) */
} e_fm_dma_cache_option;
/*
 * @Group	lnx_usr_FM_init_grp FM Initialization Unit
 *
 * @Description   FM Initialization Unit
 *
 *		  Initialization Flow
 *		  Initialization of the FM Module will be carried out by the
 *		  application according to the following sequence:
 *		  -  Calling the configuration routine with basic parameters.
 *		  -  Calling the advance initialization routines to change
 *		     driver's defaults.
 *		  -  Calling the initialization routine.
 *
 * @{
 */

t_handle fm_open(uint8_t id);
void	fm_close(t_handle h_fm);

/*
 * @Description   A structure for defining buffer prefix area content.
 */
typedef struct t_fm_buffer_prefix_content {
	uint16_t	priv_data_size;
		/**< Number of bytes to be left at the beginning of the external
		 * buffer Note that the private-area will start from the base of
		 * the buffer address.
		 */
	bool	pass_prs_result;
		/**< TRUE to pass the parse result to/from the FM; User may use
		 * fm_port_get_buffer_prs_result() in order to get the
		 * parser-result from a buffer.
		 */
	bool	pass_time_stamp;
		/**< TRUE to pass the timeStamp to/from the FM User may use
		 * fm_port_get_buffer_time_stamp() in order to get the
		 * parser-result from a buffer.
		 */
	bool	pass_hash_result;
		/**< TRUE to pass the KG hash result to/from the FM User may use
		 * fm_port_get_buffer_hash_result() in order to get the
		 * parser-result from a buffer.
		 */
	bool	pass_all_other_pcdinfo;
		/**< Add all other Internal-Context information: AD,
		 * hash-result, key, etc.
		 */
	uint16_t	data_align;
		/**< 0 to use driver's default alignment [64], other value for
		 * selecting a data alignment (must be a power of 2); if write
		 * optimization is used, must be >= 16.
		 */
	uint8_t	manip_ext_space;
		/**< Maximum extra size needed (insertion-size minus
		 * removal-size);
		 * Note that this field impacts the size of the buffer-prefix
		 * (i.e. it pushes the data offset);
		 */
} t_fm_buffer_prefix_content;

/*
 * @Description   A structure of information about each of the external
 *		  buffer pools used by a port or storage-profile.
 */
typedef struct t_fm_ext_pool_params {
	uint8_t		id;	/**< External buffer pool id */
	uint16_t	size;	/**< External buffer pool buffer size */
} t_fm_ext_pool_params;

/*
 * @Description   A structure for informing the driver about the external
 *		  buffer pools allocated in the BM and used by a port or a
 *		  storage-profile.
 */
typedef struct t_fm_ext_pools {
	uint8_t		num_of_pools_used;
			/**< Number of pools use by this port*/
	t_fm_ext_pool_params	ext_buf_pool[FM_PORT_MAX_NUM_OF_EXT_POOLS];
			/**< Parameters for each port */
} t_fm_ext_pools;

/*
 * @Description   A structure for defining backup BM Pools.
 */
typedef struct t_fm_backup_bm_pools {
	uint8_t	num_bkup_pools;
			/**< Number of BM backup pools - must be smaller than
			 * the total number of pools defined for the specified
			 * port.
			 */
	uint8_t	pool_ids[FM_PORT_MAX_NUM_OF_EXT_POOLS];
			/**< num_bkup_pools pool id's, specifying which pools
			 * should be used only as backup. Pool id's specified
			 * here must be a subset of the pools used by the
			 * specified port.
			 */
} t_fm_backup_bm_pools;

/** @} */ /* end of lnx_usr_FM_init_grp group */

/*
 * @Group	lnx_usr_FM_runtime_control_grp FM Runtime Control Unit
 *
 * @Description   FM Runtime control unit API functions, definitions and enums.
 *
 *		  The FM driver provides a set of control routines.
 *		  These routines may only be called after the module was fully
 *		  initialized (both configuration and initialization routines
 *		  were called). They are typically used to get information from
 *		  hardware (status, counters/statistics, revision etc.), to
 *		  modify a current state or to force/enable a required action.
 *		  Run-time control may be called whenever necessary and as many
 *		  times as needed.
 * @{
 */

/*
 * @Collection   General FM defines.
 */
#define FM_MAX_NUM_OF_VALID_PORTS   (FM_MAX_NUM_OF_OH_PORTS +	\
				FM_MAX_NUM_OF_1G_RX_PORTS +	\
				FM_MAX_NUM_OF_10G_RX_PORTS +   \
				FM_MAX_NUM_OF_1G_TX_PORTS +	\
				FM_MAX_NUM_OF_10G_TX_PORTS)
				/**< Number of available FM ports */
/* @} */

/** @} */ /* end of lnx_usr_FM_runtime_control_grp group */
/** @} */ /* end of lnx_usr_FM_lib_grp group */
/** @} */ /* end of lnx_usr_FM_grp group */

/*
 * @Description   FM Char device ioctls
 */

/*
 * @Group	lnx_ioctl_FM_grp Frame Manager Linux IOCTL API
 *
 * @Description   FM Linux ioctls definitions and enums
 *
 * @{
 */

/*
 * @Collection	FM IOCTL device ('/dev') definitions
 */
#define DEV_FM_NAME		"fm" /**< Name of the FM chardev */

#define DEV_FM_MINOR_BASE	0
#define DEV_FM_PCD_MINOR_BASE	(DEV_FM_MINOR_BASE + 1)
				/*/dev/fmx-pcd */
#define DEV_FM_OH_PORTS_MINOR_BASE  (DEV_FM_PCD_MINOR_BASE + 1)
				/*/dev/fmx-port-ohy */
#define DEV_FM_RX_PORTS_MINOR_BASE \
	(DEV_FM_OH_PORTS_MINOR_BASE + FM_MAX_NUM_OF_OH_PORTS)
				/*/dev/fmx-port-rxy */
#define DEV_FM_TX_PORTS_MINOR_BASE \
	(DEV_FM_RX_PORTS_MINOR_BASE + FM_MAX_NUM_OF_RX_PORTS)
				/*/dev/fmx-port-txy */
#define DEV_FM_MAX_MINORS \
	(DEV_FM_TX_PORTS_MINOR_BASE + FM_MAX_NUM_OF_TX_PORTS)

#define FM_IOC_NUM(n)	(n)
#define FM_PCD_IOC_NUM(n)   ((n) + 20)
#define FM_PORT_IOC_NUM(n)  ((n) + 70)
/* @} */

#define IOC_FM_MAX_NUM_OF_PORTS	64

/*
 * @Description   Enum for defining port types
 *		  (must match enum e_fm_port_type defined in fm_ext.h)
 */
typedef enum ioc_fm_port_type {
	e_IOC_FM_PORT_TYPE_OH_OFFLINE_PARSING = 0,  /**< Offline parsing port */
	e_IOC_FM_PORT_TYPE_RX,			/**< 1G Rx port */
	e_IOC_FM_PORT_TYPE_RX_10G,		/**< 10G Rx port */
	e_IOC_FM_PORT_TYPE_TX,			/**< 1G Tx port */
	e_IOC_FM_PORT_TYPE_TX_10G,		/**< 10G Tx port */
	e_IOC_FM_PORT_TYPE_DUMMY
} ioc_fm_port_type;

typedef struct ioc_fm_obj_t {
	void	*obj;
} ioc_fm_obj_t;

typedef union ioc_fm_api_version_t {
	struct {
	uint8_t major;
	uint8_t minor;
	uint8_t respin;
	uint8_t reserved;
	} version;
	uint32_t ver;
} ioc_fm_api_version_t;

/*
 * @Function	  FM_IOC_GET_API_VERSION
 *
 * @Description   Reads the FMD IOCTL API version.
 *
 * @Param[in,out] ioc_fm_api_version_t		The requested counter parameters
 *
 * @Return	  Version's value.
 */
#define FM_IOC_GET_API_VERSION	\
	_IOR(FM_IOC_TYPE_BASE, FM_IOC_NUM(7), ioc_fm_api_version_t)
#define FMD_API_VERSION_MAJOR 21
#define FMD_API_VERSION_MINOR 1
#define FMD_API_VERSION_RESPIN 0

uint32_t fm_get_api_version(t_handle h_fm, ioc_fm_api_version_t *p_version);


#endif /* __FM_EXT_H */
