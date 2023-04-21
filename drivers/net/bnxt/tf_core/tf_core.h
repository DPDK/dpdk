/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2023 Broadcom
 * All rights reserved.
 */

#ifndef _TF_CORE_H_
#define _TF_CORE_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "hcapi_cfa_defs.h"
#include "tf_project.h"

/**
 * @file
 *
 * Truflow Core API Header File
 */

/********** BEGIN Truflow Core DEFINITIONS **********/

#define TF_KILOBYTE  1024
#define TF_MEGABYTE  (1024 * 1024)

/**
 * direction
 */
enum tf_dir {
	TF_DIR_RX,  /**< Receive */
	TF_DIR_TX,  /**< Transmit */
	TF_DIR_MAX
};

/**
 * memory choice
 */
enum tf_mem {
	TF_MEM_INTERNAL, /**< Internal */
	TF_MEM_EXTERNAL, /**< External */
	TF_MEM_MAX
};

/**
 * External memory control channel type
 */
enum tf_ext_mem_chan_type {
	/**
	 * Direct memory write(Wh+/SR)
	 */
	TF_EXT_MEM_CHAN_TYPE_DIRECT = 0,
	/**
	 * Ring interface MPC
	 */
	TF_EXT_MEM_CHAN_TYPE_RING_IF,
	/**
	 * Use HWRM message to firmware
	 */
	TF_EXT_MEM_CHAN_TYPE_FW,
	/**
	 * Use ring_if message to firmware
	 */
	TF_EXT_MEM_CHAN_TYPE_RING_IF_FW,
	TF_EXT_MEM_CHAN_TYPE_MAX
};

/**
 * WC TCAM number of slice per row that devices supported
 */
enum tf_wc_num_slice {
	TF_WC_TCAM_1_SLICE_PER_ROW = 1,
	TF_WC_TCAM_2_SLICE_PER_ROW = 2,
	TF_WC_TCAM_4_SLICE_PER_ROW = 4,
	TF_WC_TCAM_8_SLICE_PER_ROW = 8,
};

/**
 * Bank identifier
 */
enum tf_sram_bank_id {
	TF_SRAM_BANK_ID_0,		/**< SRAM Bank 0 id */
	TF_SRAM_BANK_ID_1,		/**< SRAM Bank 1 id */
	TF_SRAM_BANK_ID_2,		/**< SRAM Bank 2 id */
	TF_SRAM_BANK_ID_3,		/**< SRAM Bank 3 id */
	TF_SRAM_BANK_ID_MAX		/**< SRAM Bank index limit */
};

/**
 * EEM record AR helper
 *
 * Helper to handle the Action Record Pointer in the EEM Record Entry.
 *
 * Convert absolute offset to action record pointer in EEM record entry
 * Convert action record pointer in EEM record entry to absolute offset
 */
#define TF_ACT_REC_OFFSET_2_PTR(offset) ((offset) >> 4)
#define TF_ACT_REC_PTR_2_OFFSET(offset) ((offset) << 4)

/*
 * Helper Macros
 */
#define TF_BITS_2_BYTES(num_bits) (((num_bits) + 7) / 8)

/********** BEGIN API FUNCTION PROTOTYPES/PARAMETERS **********/

/**
 * @page general General
 *
 * @ref tf_open_session
 *
 * @ref tf_attach_session
 *
 * @ref tf_close_session
 *
 * @ref tf_get_session_info
 *
 * @ref tf_get_session_info
 */

/**
 * Session Version defines
 *
 * The version controls the format of the tf_session and
 * tf_session_info structure. This is to assure upgrade between
 * versions can be supported.
 */
#define TF_SESSION_VER_MAJOR  1   /**< Major Version */
#define TF_SESSION_VER_MINOR  0   /**< Minor Version */
#define TF_SESSION_VER_UPDATE 0   /**< Update Version */

/**
 * Session Name
 *
 * Name of the TruFlow control channel interface.  Expects
 * format to be RTE Name specific, i.e. rte_eth_dev_get_name_by_port()
 */
#define TF_SESSION_NAME_MAX       64

#define TF_FW_SESSION_ID_INVALID  0xFF  /**< Invalid FW Session ID define */

/**
 * Session Identifier
 *
 * Unique session identifier which includes PCIe bus info to
 * distinguish the PF and session info to identify the associated
 * TruFlow session. Session ID is constructed from the passed in
 * ctrl_chan_name in tf_open_session() together with an allocated
 * fw_session_id. Done by TruFlow on tf_open_session().
 */
union tf_session_id {
	uint32_t id;
	struct {
		uint8_t domain;
		uint8_t bus;
		uint8_t device;
		uint8_t fw_session_id;
	} internal;
};

/**
 * Session Client Identifier
 *
 * Unique identifier for a client within a session. Session Client ID
 * is constructed from the passed in session and a firmware allocated
 * fw_session_client_id. Done by TruFlow on tf_open_session().
 */
union tf_session_client_id {
	uint16_t id;
	struct {
		uint8_t fw_session_id;
		uint8_t fw_session_client_id;
	} internal;
};

/**
 * Session Version
 *
 * The version controls the format of the tf_session and
 * tf_session_info structure. This is to assure upgrade between
 * versions can be supported.
 *
 * Please see the TF_VER_MAJOR/MINOR and UPDATE defines.
 */
struct tf_session_version {
	uint8_t major;
	uint8_t minor;
	uint8_t update;
};

/**
 * Session supported device types
 */
enum tf_device_type {
	TF_DEVICE_TYPE_P4 = 0,
	TF_DEVICE_TYPE_SR,
	TF_DEVICE_TYPE_P5,
	TF_DEVICE_TYPE_MAX
};

/**
 * Module types
 */
enum tf_module_type {
	/**
	 * Identifier module
	 */
	TF_MODULE_TYPE_IDENTIFIER,
	/**
	 * Table type module
	 */
	TF_MODULE_TYPE_TABLE,
	/**
	 * TCAM module
	 */
	TF_MODULE_TYPE_TCAM,
	/**
	 * EM module
	 */
	TF_MODULE_TYPE_EM,
	TF_MODULE_TYPE_MAX
};

/**
 * Identifier resource types
 */
enum tf_identifier_type {
	/**
	 *  WH/SR/TH
	 *  The L2 Context is returned from the L2 Ctxt TCAM lookup
	 *  and can be used in WC TCAM or EM keys to virtualize further
	 *  lookups.
	 */
	TF_IDENT_TYPE_L2_CTXT_HIGH,
	/**
	 *  WH/SR/TH
	 *  The L2 Context is returned from the L2 Ctxt TCAM lookup
	 *  and can be used in WC TCAM or EM keys to virtualize further
	 *  lookups.
	 */
	TF_IDENT_TYPE_L2_CTXT_LOW,
	/**
	 *  WH/SR/TH
	 *  The WC profile func is returned from the L2 Ctxt TCAM lookup
	 *  to enable virtualization of the profile TCAM.
	 */
	TF_IDENT_TYPE_PROF_FUNC,
	/**
	 *  WH/SR/TH
	 *  The WC profile ID is included in the WC lookup key
	 *  to enable virtualization of the WC TCAM hardware.
	 */
	TF_IDENT_TYPE_WC_PROF,
	/**
	 *  WH/SR/TH
	 *  The EM profile ID is included in the EM lookup key
	 *  to enable virtualization of the EM hardware.
	 */
	TF_IDENT_TYPE_EM_PROF,
	/**
	 *  (Future)
	 *  The L2 func is included in the ILT result and from recycling to
	 *  enable virtualization of further lookups.
	 */
	TF_IDENT_TYPE_L2_FUNC,
	TF_IDENT_TYPE_MAX
};

/**
 * Enumeration of TruFlow table types. A table type is used to identify a
 * resource object.
 *
 * NOTE: The table type TF_TBL_TYPE_EXT is unique in that it is
 * the only table type that is connected with a table scope.
 */
enum tf_tbl_type {
	/* Internal */

	/** Wh+/SR/TH Action Record */
	TF_TBL_TYPE_FULL_ACT_RECORD,
	/** TH Compact Action Record */
	TF_TBL_TYPE_COMPACT_ACT_RECORD,
	/** (Future) Multicast Groups */
	TF_TBL_TYPE_MCAST_GROUPS,
	/** Wh+/SR/TH Action Encap 8 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_8B,
	/** Wh+/SR/TH Action Encap 16 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_16B,
	/** WH+/SR/TH Action Encap 32 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_32B,
	/** Wh+/SR/TH Action Encap 64 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_64B,
	/* TH Action Encap 128 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_128B,
	/** WH+/SR/TH Action Source Properties SMAC */
	TF_TBL_TYPE_ACT_SP_SMAC,
	/** Wh+/SR/TH Action Source Properties SMAC IPv4 */
	TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	/** WH+/SR/TH Action Source Properties SMAC IPv6 */
	TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	/** Wh+/SR/TH Action Statistics 64 Bits */
	TF_TBL_TYPE_ACT_STATS_64,
	/** Wh+/SR Action Modify IPv4 Source */
	TF_TBL_TYPE_ACT_MODIFY_IPV4,
	/** TH 8B Modify Record */
	TF_TBL_TYPE_ACT_MODIFY_8B,
	/** TH 16B Modify Record */
	TF_TBL_TYPE_ACT_MODIFY_16B,
	/** TH 32B Modify Record */
	TF_TBL_TYPE_ACT_MODIFY_32B,
	/** TH 64B Modify Record */
	TF_TBL_TYPE_ACT_MODIFY_64B,
	/** Meter Profiles */
	TF_TBL_TYPE_METER_PROF,
	/** Meter Instance */
	TF_TBL_TYPE_METER_INST,
	/** Wh+/SR/Th Mirror Config */
	TF_TBL_TYPE_MIRROR_CONFIG,
	/** (Future) UPAR */
	TF_TBL_TYPE_UPAR,
	/** (Future) TH Metadata  */
	TF_TBL_TYPE_METADATA,
	/** (Future) TH CT State  */
	TF_TBL_TYPE_CT_STATE,
	/** (Future) TH Range Profile  */
	TF_TBL_TYPE_RANGE_PROF,
	/** TH EM Flexible Key builder */
	TF_TBL_TYPE_EM_FKB,
	/** TH WC Flexible Key builder */
	TF_TBL_TYPE_WC_FKB,
	/** Meter Drop Counter */
	TF_TBL_TYPE_METER_DROP_CNT,

	/* External */

	/**
	 * External table type - initially 1 poolsize entries.
	 * All External table types are associated with a table
	 * scope. Internal types are not.  Currently this is
	 * a pool of 128B entries.
	 */
	TF_TBL_TYPE_EXT,
	TF_TBL_TYPE_MAX
};

/**
 * TCAM table type
 */
enum tf_tcam_tbl_type {
	/** L2 Context TCAM */
	TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	/** L2 Context TCAM */
	TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	/** Profile TCAM */
	TF_TCAM_TBL_TYPE_PROF_TCAM,
	/** Wildcard TCAM */
	TF_TCAM_TBL_TYPE_WC_TCAM,
	/** Source Properties TCAM */
	TF_TCAM_TBL_TYPE_SP_TCAM,
	/** Connection Tracking Rule TCAM */
	TF_TCAM_TBL_TYPE_CT_RULE_TCAM,
	/** Virtual Edge Bridge TCAM */
	TF_TCAM_TBL_TYPE_VEB_TCAM,
	/** Wildcard TCAM HI Priority */
	TF_TCAM_TBL_TYPE_WC_TCAM_HIGH,
	/** Wildcard TCAM Low Priority */
	TF_TCAM_TBL_TYPE_WC_TCAM_LOW,
	TF_TCAM_TBL_TYPE_MAX
};

/**
 * SEARCH STATUS
 */
enum tf_search_status {
	/** The entry was not found, but an idx was allocated if requested. */
	MISS,
	/** The entry was found, and the result/idx are valid */
	HIT,
	/** The entry was not found and the table is full */
	REJECT
};

/**
 * EM Resources
 * These defines are provisioned during
 * tf_open_session()
 */
enum tf_em_tbl_type {
	/** The number of internal EM records for the session */
	TF_EM_TBL_TYPE_EM_RECORD,
	/** The number of table scopes requested */
	TF_EM_TBL_TYPE_TBL_SCOPE,
	TF_EM_TBL_TYPE_MAX
};

/**
 * TruFlow Session Information
 *
 * Structure defining a TruFlow Session, also known as a Management
 * session. This structure is initialized at time of
 * tf_open_session(). It is passed to all of the TruFlow APIs as way
 * to prescribe and isolate resources between different TruFlow ULP
 * Applications.
 *
 * Ownership of the elements is split between ULP and TruFlow. Please
 * see the individual elements.
 */
struct tf_session_info {
	/**
	 * TruFlow Version. Used to control the structure layout when
	 * sharing sessions. No guarantee that a secondary process
	 * would come from the same version of an executable.
	 * TruFlow initializes this variable on tf_open_session().
	 *
	 * Owner:  TruFlow
	 * Access: TruFlow
	 */
	struct tf_session_version ver;
	/**
	 * will be STAILQ_ENTRY(tf_session_info) next
	 *
	 * Owner:  ULP
	 * Access: ULP
	 */
	void                 *next;
	/**
	 * Session ID is a unique identifier for the session. TruFlow
	 * initializes this variable during tf_open_session()
	 * processing.
	 *
	 * Owner:  TruFlow
	 * Access: Truflow & ULP
	 */
	union tf_session_id   session_id;
	/**
	 * Protects access to core_data. Lock is initialized and owned
	 * by ULP. TruFlow can access the core_data without checking
	 * the lock.
	 *
	 * Owner:  ULP
	 * Access: ULP
	 */
	uint8_t               spin_lock;
	/**
	 * The core_data holds the TruFlow tf_session data
	 * structure. This memory is allocated and owned by TruFlow on
	 * tf_open_session().
	 *
	 * TruFlow uses this memory for session management control
	 * until the session is closed by ULP. Access control is done
	 * by the spin_lock which ULP controls ahead of TruFlow API
	 * calls.
	 *
	 * Please see tf_open_session_parms for specification details
	 * on this variable.
	 *
	 * Owner:  TruFlow
	 * Access: TruFlow
	 */
	void                 *core_data;
	/**
	 * The core_data_sz_bytes specifies the size of core_data in
	 * bytes.
	 *
	 * The size is set by TruFlow on tf_open_session().
	 *
	 * Please see tf_open_session_parms for specification details
	 * on this variable.
	 *
	 * Owner:  TruFlow
	 * Access: TruFlow
	 */
	uint32_t              core_data_sz_bytes;
};

/**
 * TruFlow handle
 *
 * Contains a pointer to the session info. Allocated by ULP and passed
 * to TruFlow using tf_open_session(). TruFlow will populate the
 * session info at that time. A TruFlow Session can be used by more
 * than one PF/VF by using the tf_open_session().
 *
 * It is expected that ULP allocates this memory as shared memory.
 *
 * NOTE: This struct must be within the BNXT PMD struct bnxt
 *       (bp). This allows use of container_of() to get access to the PMD.
 */
struct tf {
	struct tf_session_info *session;
	/**
	 * the pointer to the parent bp struct
	 */
	void *bp;
};

/**
 * Identifier resource definition
 */
struct tf_identifier_resources {
	/**
	 * Array of TF Identifiers where each entry is expected to be
	 * set to the requested resource number of that specific type.
	 * The index used is tf_identifier_type.
	 */
	uint16_t cnt[TF_IDENT_TYPE_MAX];
};

/**
 * Table type resource definition
 */
struct tf_tbl_resources {
	/**
	 * Array of TF Table types where each entry is expected to be
	 * set to the requested resource number of that specific
	 * type. The index used is tf_tbl_type.
	 */
	uint16_t cnt[TF_TBL_TYPE_MAX];
};

/**
 * TCAM type resource definition
 */
struct tf_tcam_resources {
	/**
	 * Array of TF TCAM types where each entry is expected to be
	 * set to the requested resource number of that specific
	 * type. The index used is tf_tcam_tbl_type.
	 */
	uint16_t cnt[TF_TCAM_TBL_TYPE_MAX];
};

/**
 * EM type resource definition
 */
struct tf_em_resources {
	/**
	 * Array of TF EM table types where each entry is expected to
	 * be set to the requested resource number of that specific
	 * type. The index used is tf_em_tbl_type.
	 */
	uint16_t cnt[TF_EM_TBL_TYPE_MAX];
};

/**
 * tf_session_resources parameter definition.
 */
struct tf_session_resources {
	/**
	 * [in] Requested Identifier Resources
	 *
	 * Number of identifier resources requested for the
	 * session.
	 */
	struct tf_identifier_resources ident_cnt[TF_DIR_MAX];
	/**
	 * [in] Requested Index Table resource counts
	 *
	 * The number of index table resources requested for the
	 * session.
	 */
	struct tf_tbl_resources tbl_cnt[TF_DIR_MAX];
	/**
	 * [in] Requested TCAM Table resource counts
	 *
	 * The number of TCAM table resources requested for the
	 * session.
	 */

	struct tf_tcam_resources tcam_cnt[TF_DIR_MAX];
	/**
	 * [in] Requested EM resource counts
	 *
	 * The number of internal EM table resources requested for the
	 * session.
	 */
	struct tf_em_resources em_cnt[TF_DIR_MAX];
};

/**
 * tf_open_session parameters definition.
 */
struct tf_open_session_parms {
	/**
	 * [in] ctrl_chan_name
	 *
	 * String containing name of control channel interface to be
	 * used for this session to communicate with firmware.
	 *
	 * The ctrl_chan_name can be looked up by using
	 * rte_eth_dev_get_name_by_port() within the ULP.
	 *
	 * ctrl_chan_name will be used as part of a name for any
	 * shared memory allocation. The ctrl_chan_name is usually in format
	 * 0000:02:00.0. The name for shared session is 0000:02:00.0-tf_shared.
	 */
	char ctrl_chan_name[TF_SESSION_NAME_MAX];
	/**
	 * [in/out] session_id
	 *
	 * Session_id is unique per session.
	 *
	 * Session_id is composed of domain, bus, device and
	 * fw_session_id. The construction is done by parsing the
	 * ctrl_chan_name together with allocation of a fw_session_id.
	 *
	 * The session_id allows a session to be shared between devices.
	 */
	union tf_session_id session_id;
	/**
	 * [in/out] session_client_id
	 *
	 * Session_client_id is unique per client.
	 *
	 * Session_client_id is composed of session_id and the
	 * fw_session_client_id fw_session_id. The construction is
	 * done by parsing the ctrl_chan_name together with allocation
	 * of a fw_session_client_id during tf_open_session().
	 *
	 * A reference count will be incremented in the session on
	 * which a client is created.
	 *
	 * A session can first be closed if there is one Session
	 * Client left. Session Clients should closed using
	 * tf_close_session().
	 */
	union tf_session_client_id session_client_id;
	/**
	 * [in] device type
	 *
	 * Device type for the session.
	 */
	enum tf_device_type device_type;
	/**
	 * [in] resources
	 *
	 * Resource allocation for the session.
	 */
	struct tf_session_resources resources;

	/**
	 * [in] bp
	 * The pointer to the parent bp struct. This is only used for HWRM
	 * message passing within the portability layer. The type is struct
	 * bnxt.
	 */
	void *bp;

	/**
	 * [in]
	 *
	 * The number of slices per row for WC TCAM entry.
	 */
	enum tf_wc_num_slice wc_num_slices;

	/**
	 * [out] shared_session_creator
	 *
	 * Indicates whether the application created the session if set.
	 * Otherwise the shared session already existed.  Just for information
	 * purposes.
	 */
	int shared_session_creator;
};

/**
 * Opens a new TruFlow Session or session client.
 *
 * What gets created depends on the passed in tfp content. If the tfp does not
 * have prior session data a new session with associated session client. If tfp
 * has a session already a session client will be created. In both cases the
 * session client is created using the provided ctrl_chan_name.
 *
 * In case of session creation TruFlow will allocate session specific memory to
 * hold its session data. This data is private to TruFlow.
 *
 * No other TruFlow APIs will succeed unless this API is first called
 * and succeeds.
 *
 * tf_open_session() returns a session id and session client id.  These are
 * also stored within the tfp structure passed in to all other APIs.
 *
 * A Session or session client can be closed using tf_close_session().
 *
 * There are 2 types of sessions - shared and not.  For non-shared all
 * the allocated resources are owned and managed by a single session instance.
 * No other applications have access to the resources owned by the non-shared
 * session.  For a shared session, resources are shared between 2 applications.
 *
 * When the caller of tf_open_session() sets the ctrl_chan_name[] to a name
 * like "0000:02:00.0-tf_shared", it is a request to create a new "shared"
 * session in the firmware or access the existing shared session. There is
 * only 1 shared session that can be created. If the shared session has
 * already been created in the firmware, this API will return this indication
 * by clearing the shared_session_creator flag. Only the first shared session
 * create will have the shared_session_creator flag set.
 *
 * The shared session should always be the first session to be created by
 * application and the last session closed due to RM management preference.
 *
 * Sessions remain open in the firmware until the last client of the session
 * closes the session (tf_close_session()).
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] parms
 *   Pointer to open parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_open_session(struct tf *tfp,
		    struct tf_open_session_parms *parms);

/**
 * General internal resource info
 *
 */
struct tf_resource_info {
	uint16_t start;
	uint16_t stride;
};

/**
 * Identifier resource definition
 */
struct tf_identifier_resource_info {
	/**
	 * Array of TF Identifiers. The index used is tf_identifier_type.
	 */
	struct tf_resource_info info[TF_IDENT_TYPE_MAX];
};

/**
 * Table type resource info definition
 */
struct tf_tbl_resource_info {
	/**
	 * Array of TF Table types. The index used is tf_tbl_type.
	 */
	struct tf_resource_info info[TF_TBL_TYPE_MAX];
};

/**
 * TCAM type resource definition
 */
struct tf_tcam_resource_info {
	/**
	 * Array of TF TCAM types. The index used is tf_tcam_tbl_type.
	 */
	struct tf_resource_info info[TF_TCAM_TBL_TYPE_MAX];
};

/**
 * EM type resource definition
 */
struct tf_em_resource_info {
	/**
	 * Array of TF EM table types. The index used is tf_em_tbl_type.
	 */
	struct tf_resource_info info[TF_EM_TBL_TYPE_MAX];
};

/**
 * tf_session_resources parameter definition.
 */
struct tf_session_resource_info {
	/**
	 * [in] Requested Identifier Resources
	 *
	 * Number of identifier resources requested for the
	 * session.
	 */
	struct tf_identifier_resource_info ident[TF_DIR_MAX];
	/**
	 * [in] Requested Index Table resource counts
	 *
	 * The number of index table resources requested for the
	 * session.
	 */
	struct tf_tbl_resource_info tbl[TF_DIR_MAX];
	/**
	 * [in] Requested TCAM Table resource counts
	 *
	 * The number of TCAM table resources requested for the
	 * session.
	 */

	struct tf_tcam_resource_info tcam[TF_DIR_MAX];
	/**
	 * [in] Requested EM resource counts
	 *
	 * The number of internal EM table resources requested for the
	 * session.
	 */
	struct tf_em_resource_info em[TF_DIR_MAX];
};

/**
 * tf_get_session_resources parameter definition.
 */
struct tf_get_session_info_parms {
	/**
	 * [out] the structure is used to return the information of
	 * allocated resources.
	 *
	 */
	struct tf_session_resource_info session_info;
};

/** (experimental)
 * Gets info about a TruFlow Session
 *
 * Get info about the session which has been created.  Whether it exists and
 * what resource start and stride offsets are in use.  This API is primarily
 * intended to be used by an application which has created a shared session
 * This application needs to obtain the resources which have already been
 * allocated for the shared session.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] parms
 *   Pointer to get parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_get_session_info(struct tf *tfp,
			struct tf_get_session_info_parms *parms);
/**
 * Experimental
 *
 * tf_attach_session parameters definition.
 */
struct tf_attach_session_parms {
	/**
	 * [in] ctrl_chan_name
	 *
	 * String containing name of control channel interface to be
	 * used for this session to communicate with firmware.
	 *
	 * The ctrl_chan_name can be looked up by using
	 * rte_eth_dev_get_name_by_port() within the ULP.
	 *
	 * ctrl_chan_name will be used as part of a name for any
	 * shared memory allocation.
	 */
	char ctrl_chan_name[TF_SESSION_NAME_MAX];

	/**
	 * [in] attach_chan_name
	 *
	 * String containing name of attach channel interface to be
	 * used for this session.
	 *
	 * The attach_chan_name must be given to a 2nd process after
	 * the primary process has been created. This is the
	 * ctrl_chan_name of the primary process and is used to find
	 * the shared memory for the session that the attach is going
	 * to use.
	 */
	char attach_chan_name[TF_SESSION_NAME_MAX];

	/**
	 * [in] session_id
	 *
	 * Session_id is unique per session. For Attach the session_id
	 * should be the session_id that was returned on the first
	 * open.
	 *
	 * Session_id is composed of domain, bus, device and
	 * fw_session_id. The construction is done by parsing the
	 * ctrl_chan_name together with allocation of a fw_session_id
	 * during tf_open_session().
	 *
	 * A reference count will be incremented on attach. A session
	 * is first fully closed when reference count is zero by
	 * calling tf_close_session().
	 */
	union tf_session_id session_id;
};

/**
 * Experimental
 *
 * Allows a 2nd application instance to attach to an existing
 * session. Used when a session is to be shared between two processes.
 *
 * Attach will increment a ref count as to manage the shared session data.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] parms
 *   Pointer to attach parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_attach_session(struct tf *tfp,
		      struct tf_attach_session_parms *parms);

/**
 * Closes an existing session client or the session it self. The
 * session client is default closed and if the session reference count
 * is 0 then the session is closed as well.
 *
 * On session close all hardware and firmware state associated with
 * the TruFlow application is cleaned up.
 *
 * The session client is extracted from the tfp. Thus tf_close_session()
 * cannot close a session client on behalf of another function.
 *
 * Returns success or failure code.
 */
int tf_close_session(struct tf *tfp);

/**
 * tf_set_session_hotup_state parameter definition.
 */
struct tf_set_session_hotup_state_parms {
	/**
	 * [in] the structure is used to set the state of
	 * the hotup shared session.
	 *
	 */
	uint16_t state;
};

/**
 * set hot upgrade shared session state
 *
 * This API is used to set the state of the shared session.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] parms
 *   Pointer to set hotup state parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_set_session_hotup_state(struct tf *tfp,
			       struct tf_set_session_hotup_state_parms *parms);

/**
 * tf_get_session_hotup_state parameter definition.
 */
struct tf_get_session_hotup_state_parms {
	/**
	 * [out] the structure is used to get the state of
	 * the hotup shared session.
	 *
	 */
	uint16_t state;
	/**
	 * [out] get the ref_cnt of the hotup shared session.
	 *
	 */
	uint16_t ref_cnt;
};

/**
 * get hot upgrade shared session state
 *
 * This API is used to set the state of the shared session.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] parms
 *   Pointer to get hotup state parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_get_session_hotup_state(struct tf *tfp,
			       struct tf_get_session_hotup_state_parms *parms);

/**
 * @page  ident Identity Management
 *
 * @ref tf_alloc_identifier
 *
 * @ref tf_free_identifier
 */
/**
 * tf_alloc_identifier parameter definition
 */
struct tf_alloc_identifier_parms {
	/**
	 * [in]	 receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Identifier type
	 */
	enum tf_identifier_type ident_type;
	/**
	 * [out] Allocated identifier
	 */
	uint32_t id;
};

/**
 * tf_free_identifier parameter definition
 */
struct tf_free_identifier_parms {
	/**
	 * [in]	 receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Identifier type
	 */
	enum tf_identifier_type ident_type;
	/**
	 * [in] ID to free
	 */
	uint32_t id;
	/**
	 * (experimental)
	 * [out] Current refcnt after free
	 */
	uint32_t ref_cnt;
};

/**
 * tf_search_identifier parameter definition (experimental)
 */
struct tf_search_identifier_parms {
	/**
	 * [in]	 receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Identifier type
	 */
	enum tf_identifier_type ident_type;
	/**
	 * [in] Identifier data to search for
	 */
	uint32_t search_id;
	/**
	 * [out] Set if matching identifier found
	 */
	bool hit;
	/**
	 * [out] Current ref count after allocation
	 */
	uint32_t ref_cnt;
};

/**
 * allocate identifier resource
 *
 * TruFlow core will allocate a free id from the per identifier resource type
 * pool reserved for the session during tf_open().  No firmware is involved.
 *
 * Returns success or failure code.
 */
int tf_alloc_identifier(struct tf *tfp,
			struct tf_alloc_identifier_parms *parms);

/**
 * free identifier resource
 *
 * TruFlow core will return an id back to the per identifier resource type pool
 * reserved for the session.  No firmware is involved.  During tf_close, the
 * complete pool is returned to the firmware.
 *
 * additional operation (experimental)
 * Decrement reference count.
 *
 * Returns success or failure code.
 */
int tf_free_identifier(struct tf *tfp,
		       struct tf_free_identifier_parms *parms);

/**
 * Search identifier resource (experimental)
 *
 * identifier alloc (search_en=1)
 * if (ident is allocated and ref_cnt >=1)
 *      return ident - hit is set, incr refcnt
 * else (not found)
 *      return
 *
 */
int tf_search_identifier(struct tf *tfp,
			 struct tf_search_identifier_parms *parms);

/**
 * @page dram_table DRAM Table Scope Interface
 *
 * @ref tf_alloc_tbl_scope
 *
 * @ref tf_free_tbl_scope
 *
 * If we allocate the EEM memory from the core, we need to store it in
 * the shared session data structure to make sure it can be freed later.
 * (for example if the PF goes away)
 *
 * Current thought is that memory is allocated within core.
 */

/**
 * tf_alloc_tbl_scope_parms definition
 */
struct tf_alloc_tbl_scope_parms {
	/**
	 * [in] All Maximum key size required.
	 */
	uint16_t rx_max_key_sz_in_bits;
	/**
	 * [in] Maximum Action size required (includes inlined items)
	 */
	uint16_t rx_max_action_entry_sz_in_bits;
	/**
	 * [in] Memory size in Megabytes
	 * Total memory size allocated by user to be divided
	 * up for actions, hash, counters.  Only inline external actions.
	 * Use this variable or the number of flows, do not set both.
	 */
	uint32_t rx_mem_size_in_mb;
	/**
	 * [in] Number of flows * 1000. If set, rx_mem_size_in_mb must equal 0.
	 */
	uint32_t rx_num_flows_in_k;
	/**
	 * [in] All Maximum key size required.
	 */
	uint16_t tx_max_key_sz_in_bits;
	/**
	 * [in] Maximum Action size required (includes inlined items)
	 */
	uint16_t tx_max_action_entry_sz_in_bits;
	/**
	 * [in] Memory size in Megabytes
	 * Total memory size allocated by user to be divided
	 * up for actions, hash, counters.  Only inline external actions.
	 */
	uint32_t tx_mem_size_in_mb;
	/**
	 * [in] Number of flows * 1000
	 */
	uint32_t tx_num_flows_in_k;
	/**
	 * [in] Flush pending HW cached flows every 1/10th of value
	 * set in seconds, both idle and active flows are flushed
	 * from the HW cache. If set to 0, this feature will be disabled.
	 */
	uint8_t hw_flow_cache_flush_timer;
	/**
	 * [out] table scope identifier
	 */
	uint32_t tbl_scope_id;
};
/**
 * tf_free_tbl_scope_parms definition
 */
struct tf_free_tbl_scope_parms {
	/**
	 * [in] table scope identifier
	 */
	uint32_t tbl_scope_id;
};

/**
 * tf_map_tbl_scope_parms definition
 */
struct tf_map_tbl_scope_parms {
	/**
	 * [in] table scope identifier
	 */
	uint32_t tbl_scope_id;
	/**
	 * [in] Which parifs are associated with this table scope.  Bit 0
	 *      indicates parif 0.
	 */
	uint16_t parif_bitmask;
};

/**
 * allocate a table scope
 *
 * The scope is a software construct to identify an EEM table.  This function will
 * divide the hash memory/buckets and records according to the device
 * device constraints based upon calculations using either the number of flows
 * requested or the size of memory indicated.  Other parameters passed in
 * determine the configuration (maximum key size, maximum external action record
 * size).
 *
 * A single API is used to allocate a common table scope identifier in both
 * receive and transmit CFA. The scope identifier is common due to nature of
 * connection tracking sending notifications between RX and TX direction.
 *
 * The receive and transmit table access identifiers specify which rings will
 * be used to initialize table DRAM.  The application must ensure mutual
 * exclusivity of ring usage for table scope allocation and any table update
 * operations.
 *
 * The hash table buckets, EM keys, and EM lookup results are stored in the
 * memory allocated based on the rx_em_hash_mb/tx_em_hash_mb parameters.  The
 * hash table buckets are stored at the beginning of that memory.
 *
 * NOTE:  No EM internal setup is done here. On chip EM records are managed
 * internally by TruFlow core.
 *
 * Returns success or failure code.
 */
int tf_alloc_tbl_scope(struct tf *tfp,
		       struct tf_alloc_tbl_scope_parms *parms);

/**
 * map a table scope (legacy device only Wh+/SR)
 *
 * Map a table scope to one or more partition interfaces (parifs).
 * The parif can be remapped in the L2 context lookup for legacy devices.  This
 * API allows a number of parifs to be mapped to the same table scope.  On
 * legacy devices a table scope identifies one of 16 sets of EEM table base
 * addresses and is associated with a PF communication channel.  The associated
 * PF must be configured for the table scope to operate.
 *
 * An L2 context TCAM lookup returns a remapped parif value used to
 * index into the set of 16 parif_to_pf registers which are used to map to one
 * of the 16 table scopes.  This API allows the user to map the parifs in the
 * mask to the previously allocated table scope (EEM table).

 * Returns success or failure code.
 */
int tf_map_tbl_scope(struct tf *tfp,
		      struct tf_map_tbl_scope_parms *parms);
/**
 * free a table scope
 *
 * Firmware checks that the table scope ID is owned by the TruFlow
 * session, verifies that no references to this table scope remains
 * or Profile TCAM entries for either CFA (RX/TX) direction,
 * then frees the table scope ID.
 *
 * Returns success or failure code.
 */
int tf_free_tbl_scope(struct tf *tfp,
		      struct tf_free_tbl_scope_parms *parms);

/**
 * @page tcam TCAM Access
 *
 * @ref tf_search_tcam_entry
 *
 * @ref tf_alloc_tcam_entry
 *
 * @ref tf_set_tcam_entry
 *
 * @ref tf_get_tcam_entry
 *
 * @ref tf_move_tcam_shared_entries
 *
 * @ref tf_clear_tcam_shared_entries
 */

/**
 * tf_search_tcam_entry parameter definition (experimental)
 */
struct tf_search_tcam_entry_parms {
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] TCAM table type
	 */
	enum tf_tcam_tbl_type tcam_tbl_type;
	/**
	 * [in] Key data to match on
	 */
	uint8_t *key;
	/**
	 * [in] key size in bits
	 */
	uint16_t key_sz_in_bits;
	/**
	 * [in] Mask data to match on
	 */
	uint8_t *mask;
	/**
	 * [in] Priority of entry requested (definition TBD)
	 */
	uint32_t priority;
	/**
	 * [in] Allocate on miss.
	 */
	uint8_t alloc;
	/**
	 * [out] Set if matching entry found
	 */
	uint8_t hit;
	/**
	 * [out] Search result status (hit, miss, reject)
	 */
	enum tf_search_status search_status;
	/**
	 * [out] Current refcnt after allocation
	 */
	uint16_t ref_cnt;
	/**
	 * [in out] The result data from the search is copied here
	 */
	uint8_t *result;
	/**
	 * [in out] result size in bits for the result data
	 */
	uint16_t result_sz_in_bits;
	/**
	 * [out] Index found
	 */
	uint16_t idx;
};

/**
 * search TCAM entry
 *
 * Search for a TCAM entry
 *
 * Implementation:
 *
 * If the full key/mask matches the
 * entry, hit is set, ref_cnt is incremented, and search_status indicates what
 * action the caller can take regarding setting the entry.
 *
 * search_status should be used as follows:
 * - On Miss, the caller should create a result and call tf_set_tcam_entry with
 * returned index.
 *
 * - On Reject, the hash table is full and the entry cannot be added.
 *
 * - On Hit, the result data is returned to the caller.  Additionally, the
 * ref_cnt is updated.
 *
 * Also returns success or failure code.
 */
int tf_search_tcam_entry(struct tf *tfp,
			 struct tf_search_tcam_entry_parms *parms);

/**
 * tf_alloc_tcam_entry parameter definition
 */
struct tf_alloc_tcam_entry_parms {
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] TCAM table type
	 */
	enum tf_tcam_tbl_type tcam_tbl_type;
	/**
	 * [in] Enable search for matching entry
	 */
	uint8_t search_enable;
	/**
	 * [in] Key data to match on (if search)
	 */
	uint8_t *key;
	/**
	 * [in] key size in bits (if search)
	 */
	uint16_t key_sz_in_bits;
	/**
	 * [in] Mask data to match on (if search)
	 */
	uint8_t *mask;
	/**
	 * [in] Priority of entry requested (definition TBD)
	 */
	uint32_t priority;
	/**
	 * [out] If search, set if matching entry found
	 */
	uint8_t hit;
	/**
	 * [out] Current refcnt after allocation
	 */
	uint16_t ref_cnt;
	/**
	 * [out] Idx allocated
	 *
	 */
	uint16_t idx;
};

/**
 * allocate TCAM entry
 *
 * Allocate a TCAM entry - one of these types:
 *
 * L2 Context
 * Profile TCAM
 * WC TCAM
 * VEB TCAM
 *
 * This function allocates a TCAM table record.	 This function
 * will attempt to allocate a TCAM table entry from the session
 * owned TCAM entries.  Key, mask and result must match for
 * hit to be set.  Only TruFlow core data is accessed.
 * A hash table to entry mapping is maintained for search purposes.  If
 * search is not enabled, the first available free entry is returned based
 * on priority and alloc_cnt is set to 1.  If search is enabled and a matching
 * entry to entry_data is found, hit is set to TRUE and alloc_cnt is set to 1.
 * RefCnt is also returned.
 *
 * Also returns success or failure code.
 */
int tf_alloc_tcam_entry(struct tf *tfp,
			struct tf_alloc_tcam_entry_parms *parms);

/**
 * tf_set_tcam_entry parameter definition
 */
struct	tf_set_tcam_entry_parms {
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] TCAM table type
	 */
	enum tf_tcam_tbl_type tcam_tbl_type;
	/**
	 * [in] base index of the entry to program
	 */
	uint16_t idx;
	/**
	 * [in] struct containing key
	 */
	uint8_t *key;
	/**
	 * [in] struct containing mask fields
	 */
	uint8_t *mask;
	/**
	 * [in] key size in bits (if search)
	 */
	uint16_t key_sz_in_bits;
	/**
	 * [in] struct containing result
	 */
	uint8_t *result;
	/**
	 * [in] struct containing result size in bits
	 */
	uint16_t result_sz_in_bits;
};

/**
 * set TCAM entry
 *
 * Program a TCAM table entry for a TruFlow session.
 *
 * If the entry has not been allocated, an error will be returned.
 *
 * Returns success or failure code.
 */
int tf_set_tcam_entry(struct tf	*tfp,
		      struct tf_set_tcam_entry_parms *parms);

/**
 * tf_get_tcam_entry parameter definition
 */
struct tf_get_tcam_entry_parms {
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] TCAM table type
	 */
	enum tf_tcam_tbl_type  tcam_tbl_type;
	/**
	 * [in] index of the entry to get
	 */
	uint16_t idx;
	/**
	 * [out] struct containing key
	 */
	uint8_t *key;
	/**
	 * [out] struct containing mask fields
	 */
	uint8_t *mask;
	/**
	 * [in/out] key size in bits
	 */
	uint16_t key_sz_in_bits;
	/**
	 * [out] struct containing result
	 */
	uint8_t *result;
	/**
	 * [in/out] struct containing result size in bits
	 */
	uint16_t result_sz_in_bits;
};

/**
 * get TCAM entry
 *
 * Program a TCAM table entry for a TruFlow session.
 *
 * If the entry has not been allocated, an error will be returned.
 *
 * Returns success or failure code.
 */
int tf_get_tcam_entry(struct tf *tfp,
		      struct tf_get_tcam_entry_parms *parms);

/**
 * tf_free_tcam_entry parameter definition
 */
struct tf_free_tcam_entry_parms {
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] TCAM table type
	 */
	enum tf_tcam_tbl_type tcam_tbl_type;
	/**
	 * [in] Index to free
	 */
	uint16_t idx;
	/**
	 * [out] reference count after free
	 */
	uint16_t ref_cnt;
};

/**
 * free TCAM entry
 *
 * Free TCAM entry.
 *
 * Firmware checks to ensure the TCAM entries are owned by the TruFlow
 * session.  TCAM entry will be invalidated.  All-ones mask.
 * writes to hw.
 *
 * WCTCAM profile id of 0 must be used to invalidate an entry.
 *
 * Returns success or failure code.
 */
int tf_free_tcam_entry(struct tf *tfp,
		       struct tf_free_tcam_entry_parms *parms);

/**
 * tf_move_tcam_shared_entries parameter definition
 */
struct tf_move_tcam_shared_entries_parms {
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] TCAM table type
	 */
	enum tf_tcam_tbl_type tcam_tbl_type;
};

/**
 * Move TCAM entries
 *
 * This API only affects the following TCAM pools within a shared session:
 *
 * TF_TCAM_TBL_TYPE_WC_TCAM_HIGH
 * TF_TCAM_TBL_TYPE_WC_TCAM_LOW
 *
 * When called, all allocated entries from the high pool will be moved to
 * the low pool.  Then the allocated entries in the high pool will be
 * cleared and freed.
 *
 * This API is not supported on a non-shared session.
 *
 * Returns success or failure code.
 */
int tf_move_tcam_shared_entries(struct tf *tfp,
				struct tf_move_tcam_shared_entries_parms *parms);

/**
 * tf_clear_tcam_shared_entries parameter definition
 */
struct tf_clear_tcam_shared_entries_parms {
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] TCAM table type
	 */
	enum tf_tcam_tbl_type tcam_tbl_type;
};

/**
 * Clear TCAM shared entries pool
 *
 * This API only affects the following TCAM pools within a shared session:
 *
 * TF_TCAM_TBL_TYPE_WC_TCAM_HIGH
 * TF_TCAM_TBL_TYPE_WC_TCAM_LOW
 *
 * When called, the indicated WC TCAM high or low pool will be cleared.
 *
 * This API is not supported on a non-shared session.
 *
 * Returns success or failure code.
 */
int tf_clear_tcam_shared_entries(struct tf *tfp,
			      struct tf_clear_tcam_shared_entries_parms *parms);

/**
 * @page table Table Access
 *
 * @ref tf_alloc_tbl_entry
 *
 * @ref tf_free_tbl_entry
 *
 * @ref tf_set_tbl_entry
 *
 * @ref tf_get_tbl_entry
 *
 * @ref tf_bulk_get_tbl_entry
 *
 * @ref tf_get_shared_tbl_increment
 */

/**
 * tf_alloc_tbl_entry parameter definition
 */
struct tf_alloc_tbl_entry_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of the allocation
	 */
	enum tf_tbl_type type;
	/**
	 * [in] Table scope identifier (ignored unless TF_TBL_TYPE_EXT)
	 */
	uint32_t tbl_scope_id;

	/**
	 * [out] Idx of allocated entry
	 */
	uint32_t idx;
};

/**
 * allocate index table entries
 *
 * Internal types:
 *
 * Allocate an on chip index table entry or search for a matching
 * entry of the indicated type for this TruFlow session.
 *
 * Allocates an index table record. This function will attempt to
 * allocate an index table entry.
 *
 * External types:
 *
 * These are used to allocate inlined action record memory.
 *
 * Allocates an external index table action record.
 *
 * NOTE:
 * Implementation of the internals of the external function will be a stack with
 * push and pop.
 *
 * Returns success or failure code.
 */
int tf_alloc_tbl_entry(struct tf *tfp,
		       struct tf_alloc_tbl_entry_parms *parms);

/**
 * tf_free_tbl_entry parameter definition
 */
struct tf_free_tbl_entry_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of the allocation type
	 */
	enum tf_tbl_type type;
	/**
	 * [in] Table scope identifier (ignored unless TF_TBL_TYPE_EXT)
	 */
	uint32_t tbl_scope_id;
	/**
	 * [in] Index to free
	 */
	uint32_t idx;
};

/**
 * free index table entry
 *
 * Used to free a previously allocated table entry.
 *
 * Internal types:
 *
 * The element is freed and given back to the session pool.
 *
 * External types:
 *
 * Frees an external index table action record.
 *
 * NOTE:
 * Implementation of the internals of the external table will be a stack with
 * push and pop.
 *
 * Returns success or failure code.
 */
int tf_free_tbl_entry(struct tf *tfp,
		      struct tf_free_tbl_entry_parms *parms);

/**
 * tf_set_tbl_entry parameter definition
 */
struct tf_set_tbl_entry_parms {
	/**
	 * [in] Table scope identifier
	 */
	uint32_t tbl_scope_id;
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of object to set
	 */
	enum tf_tbl_type type;
	/**
	 * [in] Entry data
	 */
	uint8_t *data;
	/**
	 * [in] Entry size
	 */
	uint16_t data_sz_in_bytes;
	/**
	 * [in] External memory channel type to use
	 */
	enum tf_ext_mem_chan_type chan_type;
	/**
	 * [in] Entry index to write to
	 */
	uint32_t idx;
};

/**
 * set index table entry
 *
 * Used to set an application programmed index table entry into a
 * previous allocated table location.
 *
 * Returns success or failure code.
 */
int tf_set_tbl_entry(struct tf *tfp,
		     struct tf_set_tbl_entry_parms *parms);

/**
 * tf_get_shared_tbl_increment parameter definition
 */
struct tf_get_shared_tbl_increment_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of object to set
	 */
	enum tf_tbl_type type;
	/**
	 * [out] Value to increment by for resource type
	 */
	uint32_t increment_cnt;
};

/**
 * tf_get_shared_tbl_increment
 *
 * This API is currently only required for use in the shared
 * session for P5 actions.  An increment count is returned per
 * type to indicate how much to increment the start by for each
 * entry (see tf_resource_info)
 *
 * Returns success or failure code.
 */
int tf_get_shared_tbl_increment(struct tf *tfp,
				struct tf_get_shared_tbl_increment_parms *parms);

/**
 * tf_get_tbl_entry parameter definition
 */
struct tf_get_tbl_entry_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of object to get
	 */
	enum tf_tbl_type type;
	/**
	 * [out] Entry data
	 */
	uint8_t *data;
	/**
	 * [in] Entry size
	 */
	uint16_t data_sz_in_bytes;
	/**
	 * [in] External memory channel type to use
	 */
	enum tf_ext_mem_chan_type chan_type;
	/**
	 * [in] Entry index to read
	 */
	uint32_t idx;
};

/**
 * get index table entry
 *
 * Used to retrieve a previous set index table entry.
 *
 * Returns success or failure code. Failure will be returned if the
 * provided data buffer is too small for the data type requested.
 */
int tf_get_tbl_entry(struct tf *tfp,
		     struct tf_get_tbl_entry_parms *parms);

/**
 * tf_bulk_get_tbl_entry parameter definition
 */
struct tf_bulk_get_tbl_entry_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of object to get
	 */
	enum tf_tbl_type type;
	/**
	 * [in] Starting index to read from
	 */
	uint32_t starting_idx;
	/**
	 * [in] Number of sequential entries
	 */
	uint16_t num_entries;
	/**
	 * [in] Size of the single entry
	 */
	uint16_t entry_sz_in_bytes;
	/**
	 * [out] Host physical address, where the data
	 * will be copied to by the firmware.
	 * Use tfp_calloc() API and mem_pa
	 * variable of the tfp_calloc_parms
	 * structure for the physical address.
	 */
	uint64_t physical_mem_addr;
	/**
	 * [in] External memory channel type to use
	 */
	enum tf_ext_mem_chan_type chan_type;
};

/**
 * Bulk get index table entry
 *
 * Used to retrieve a set of index table entries.
 *
 * Entries within the range may not have been allocated using
 * tf_alloc_tbl_entry() at the time of access. But the range must
 * be within the bounds determined from tf_open_session() for the
 * given table type.  Currently, this is only used for collecting statistics.
 *
 * Returns success or failure code. Failure will be returned if the
 * provided data buffer is too small for the data type requested.
 */
int tf_bulk_get_tbl_entry(struct tf *tfp,
			  struct tf_bulk_get_tbl_entry_parms *parms);

/**
 * @page exact_match Exact Match Table
 *
 * @ref tf_insert_em_entry
 *
 * @ref tf_delete_em_entry
 *
 * @ref tf_search_em_entry
 *
 */
/**
 * tf_insert_em_entry parameter definition
 */
struct tf_insert_em_entry_parms {
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] internal or external
	 */
	enum tf_mem mem;
	/**
	 * [in] ID of table scope to use (external only)
	 */
	uint32_t tbl_scope_id;
	/**
	 * [in] ptr to structure containing key fields
	 */
	uint8_t *key;
	/**
	 * [in] key bit length
	 */
	uint16_t key_sz_in_bits;
	/**
	 * [in] ptr to structure containing result field
	 */
	uint8_t *em_record;
	/**
	 * [out] result size in bits
	 */
	uint16_t em_record_sz_in_bits;
	/**
	 * [in] duplicate check flag
	 */
	uint8_t	dup_check;
	/**
	 * [in] External memory channel type to use
	 */
	enum tf_ext_mem_chan_type chan_type;
	/**
	 * [out] Flow handle value for the inserted entry.  This is encoded
	 * as the entries[4]:bucket[2]:hashId[1]:hash[14]
	 */
	uint64_t flow_handle;
	/**
	 * [out] Flow id is returned as null (internal)
	 * Flow id is the GFID value for the inserted entry (external)
	 * This is the value written to the BD and useful information for mark.
	 */
	uint64_t flow_id;
};

/**
 * tf_delete_em_entry parameter definition
 */
struct tf_delete_em_entry_parms {
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] internal or external
	 */
	enum tf_mem mem;
	/**
	 * [in] ID of table scope to use (external only)
	 */
	uint32_t tbl_scope_id;
	/**
	 * [out] The index of the entry
	 */
	uint16_t index;
	/**
	 * [in] External memory channel type to use
	 */
	enum tf_ext_mem_chan_type chan_type;
	/**
	 * [in] structure containing flow delete handle information
	 */
	uint64_t flow_handle;
};

/**
 * tf_move_em_entry parameter definition
 */
struct tf_move_em_entry_parms {
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] internal or external
	 */
	enum tf_mem mem;
	/**
	 * [in] ID of table scope to use (external only)
	 */
	uint32_t tbl_scope_id;
	/**
	 * [in] ID of table interface to use (SR2 only)
	 */
	uint32_t tbl_if_id;
	/**
	 * [in] epoch group IDs of entry to delete
	 * 2 element array with 2 ids. (SR2 only)
	 */
	uint16_t *epochs;
	/**
	 * [out] The index of the entry
	 */
	uint16_t index;
	/**
	 * [in] External memory channel type to use
	 */
	enum tf_ext_mem_chan_type chan_type;
	/**
	 * [in] The index of the new EM record
	 */
	uint32_t new_index;
	/**
	 * [in] structure containing flow delete handle information
	 */
	uint64_t flow_handle;
};

/**
 * tf_search_em_entry parameter definition (Future)
 */
struct tf_search_em_entry_parms {
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] internal or external
	 */
	enum tf_mem mem;
	/**
	 * [in] ID of table scope to use (external only)
	 */
	uint32_t tbl_scope_id;
	/**
	 * [in] ptr to structure containing key fields
	 */
	uint8_t *key;
	/**
	 * [in] key bit length
	 */
	uint16_t key_sz_in_bits;
	/**
	 * [in/out] ptr to structure containing EM record fields
	 */
	uint8_t *em_record;
	/**
	 * [out] result size in bits
	 */
	uint16_t em_record_sz_in_bits;
	/**
	 * [in] External memory channel type to use
	 */
	enum tf_ext_mem_chan_type chan_type;
	/**
	 * [in] ptr to structure containing flow delete handle
	 */
	uint64_t flow_handle;
};

/**
 * insert em hash entry in internal table memory
 *
 * Internal:
 *
 * This API inserts an exact match entry into internal EM table memory
 * of the specified direction.
 *
 * Note: The EM record is managed within the TruFlow core and not the
 * application.
 *
 * Shadow copy of internal record table an association with hash and 1,2, or 4
 * associated buckets
 *
 * External:
 * This API inserts an exact match entry into DRAM EM table memory of the
 * specified direction and table scope.
 *
 * The insertion of duplicate entries in an EM table is not permitted.	If a
 * TruFlow application can guarantee that it will never insert duplicates, it
 * can disable duplicate checking by passing a zero value in the  dup_check
 * parameter to this API.  This will optimize performance. Otherwise, the
 * TruFlow library will enforce protection against inserting duplicate entries.
 *
 * Flow handle is defined in this document:
 *
 * https://docs.google.com
 * /document/d/1NESu7RpTN3jwxbokaPfYORQyChYRmJgs40wMIRe8_-Q/edit
 *
 * Returns success or busy code.
 *
 */
int tf_insert_em_entry(struct tf *tfp,
		       struct tf_insert_em_entry_parms *parms);

/**
 * delete em hash entry table memory
 *
 * Internal:
 *
 * This API deletes an exact match entry from internal EM table memory of the
 * specified direction. If a valid flow ptr is passed in then that takes
 * precedence over the pointer to the complete key passed in.
 *
 *
 * External:
 *
 * This API deletes an exact match entry from EM table memory of the specified
 * direction and table scope. If a valid flow handle is passed in then that
 * takes precedence over the pointer to the complete key passed in.
 *
 * The TruFlow library may release a dynamic bucket when an entry is deleted.
 *
 *
 * Returns success or not found code
 *
 *
 */
int tf_delete_em_entry(struct tf *tfp,
		       struct tf_delete_em_entry_parms *parms);

/**
 * search em hash entry table memory (Future)
 *
 * Internal:

 * This API looks up an EM entry in table memory with the specified EM
 * key or flow (flow takes precedence) and direction.
 *
 * The status will be one of: success or entry not found.  If the lookup
 * succeeds, a pointer to the matching entry and the result record associated
 * with the matching entry will be provided.
 *
 * Query the fw with key to get result.
 *
 * External:
 *
 * This API looks up an EM entry in table memory with the specified EM
 * key or flow_handle (flow takes precedence), direction and table scope.
 *
 * The status will be one of: success or entry not found.  If the lookup
 * succeeds, a pointer to the matching entry and the result record associated
 * with the matching entry will be provided.
 *
 * Returns success or not found code
 *
 */
int tf_search_em_entry(struct tf *tfp,
		       struct tf_search_em_entry_parms *parms);

/**
 * @page global Global Configuration
 *
 * @ref tf_set_global_cfg
 *
 * @ref tf_get_global_cfg
 */

/**
 * Tunnel Encapsulation Offsets
 */
enum tf_tunnel_encap_offsets {
	TF_TUNNEL_ENCAP_L2,
	TF_TUNNEL_ENCAP_NAT,
	TF_TUNNEL_ENCAP_MPLS,
	TF_TUNNEL_ENCAP_VXLAN,
	TF_TUNNEL_ENCAP_GENEVE,
	TF_TUNNEL_ENCAP_NVGRE,
	TF_TUNNEL_ENCAP_GRE,
	TF_TUNNEL_ENCAP_FULL_GENERIC
};

/**
 * Global Configuration Table Types
 */
enum tf_global_config_type {
	TF_TUNNEL_ENCAP,  /**< Tunnel Encap Config(TECT) */
	TF_ACTION_BLOCK,  /**< Action Block Config(ABCR) */
	TF_COUNTER_CFG,   /**< Counter Configuration (CNTRS_CTRL) */
	TF_METER_CFG,     /**< Meter Config(ACTP4_FMTCR) */
	TF_METER_INTERVAL_CFG, /**< Meter Interval Config(FMTCR_INTERVAL)  */
	TF_GLOBAL_CFG_TYPE_MAX
};

/**
 * tf_global_cfg parameter definition
 */
struct tf_global_cfg_parms {
	/**
	 * [in] receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Global config type
	 */
	enum tf_global_config_type type;
	/**
	 * [in] Offset @ the type
	 */
	uint32_t offset;
	/**
	 * [in/out] Value of the configuration
	 * set - Read, Modify and Write
	 * get - Read the full configuration
	 */
	uint8_t *config;
	/**
	 * [in] Configuration mask
	 * set - Read, Modify with mask and Write
	 * get - unused
	 */
	uint8_t *config_mask;
	/**
	 * [in] struct containing size
	 */
	uint16_t config_sz_in_bytes;
};

/**
 * Get global configuration
 *
 * Retrieve the configuration
 *
 * Returns success or failure code.
 */
int tf_get_global_cfg(struct tf *tfp,
		      struct tf_global_cfg_parms *parms);

/**
 * Update the global configuration table
 *
 * Read, modify write the value.
 *
 * Returns success or failure code.
 */
int tf_set_global_cfg(struct tf *tfp,
		      struct tf_global_cfg_parms *parms);

/**
 * @page if_tbl Interface Table Access
 *
 * @ref tf_set_if_tbl_entry
 *
 * @ref tf_get_if_tbl_entry
 */

/**
 * Enumeration of TruFlow interface table types.
 */
enum tf_if_tbl_type {
	/** Default Profile L2 Context Entry */
	TF_IF_TBL_TYPE_PROF_SPIF_DFLT_L2_CTXT,
	/** Default Profile TCAM/Lookup Action Record Pointer Table */
	TF_IF_TBL_TYPE_PROF_PARIF_DFLT_ACT_REC_PTR,
	/** Error Profile TCAM Miss Action Record Pointer Table */
	TF_IF_TBL_TYPE_PROF_PARIF_ERR_ACT_REC_PTR,
	/** Default Error Profile TCAM Miss Action Record Pointer Table */
	TF_IF_TBL_TYPE_LKUP_PARIF_DFLT_ACT_REC_PTR,
	/** Ingress lookup table */
	TF_IF_TBL_TYPE_ILT,
	/** VNIC/SVIF Properties Table */
	TF_IF_TBL_TYPE_VSPT,
	TF_IF_TBL_TYPE_MAX
};

/**
 * tf_set_if_tbl_entry parameter definition
 */
struct tf_set_if_tbl_entry_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of object to set
	 */
	enum tf_if_tbl_type type;
	/**
	 * [in] Entry data
	 */
	uint8_t *data;
	/**
	 * [in] Entry size
	 */
	uint16_t data_sz_in_bytes;
	/**
	 * [in] Interface to write
	 */
	uint32_t idx;
};

/**
 * set interface table entry
 *
 * Used to set an interface table. This API is used for managing tables indexed
 * by SVIF/SPIF/PARIF interfaces. In current implementation only the value is
 * set.
 * Returns success or failure code.
 */
int tf_set_if_tbl_entry(struct tf *tfp,
			struct tf_set_if_tbl_entry_parms *parms);

/**
 * tf_get_if_tbl_entry parameter definition
 */
struct tf_get_if_tbl_entry_parms {
	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;
	/**
	 * [in] Type of table to get
	 */
	enum tf_if_tbl_type type;
	/**
	 * [out] Entry data
	 */
	uint8_t *data;
	/**
	 * [in] Entry size
	 */
	uint16_t data_sz_in_bytes;
	/**
	 * [in] Entry index to read
	 */
	uint32_t idx;
};

/**
 * get interface table entry
 *
 * Used to retrieve an interface table entry.
 *
 * Reads the interface table entry value
 *
 * Returns success or failure code. Failure will be returned if the
 * provided data buffer is too small for the data type requested.
 */
int tf_get_if_tbl_entry(struct tf *tfp,
			struct tf_get_if_tbl_entry_parms *parms);

/**
 * tf_get_version parameters definition.
 */
struct tf_get_version_parms {
	/**
	 * [in] device type
	 *
	 * Device type for the session.
	 */
	enum tf_device_type device_type;

	/**
	 * [in] bp
	 * The pointer to the parent bp struct. This is only used for HWRM
	 * message passing within the portability layer. The type is struct
	 * bnxt.
	 */
	void *bp;

	/* [out] major
	 *
	 * Version Major number.
	 */
	uint8_t	major;

	/* [out] minor
	 *
	 * Version Minor number.
	 */
	uint8_t	minor;

	/* [out] update
	 *
	 * Version Update number.
	 */
	uint8_t	update;

	/**
	 * [out] dev_ident_caps
	 *
	 * fw available identifier resource list
	 */
	uint32_t dev_ident_caps;

	/**
	 * [out] dev_tbl_caps
	 *
	 * fw available table resource list
	 */
	uint32_t dev_tbl_caps;

	/**
	 * [out] dev_tcam_caps
	 *
	 * fw available tcam resource list
	 */
	uint32_t dev_tcam_caps;

	/**
	 * [out] dev_em_caps
	 *
	 * fw available em resource list
	 */
	uint32_t dev_em_caps;
};

/**
 * Get tf fw version
 *
 * Used to retrieve Truflow fw version information.
 *
 * Returns success or failure code.
 */
int tf_get_version(struct tf *tfp,
		   struct tf_get_version_parms *parms);

/**
 * tf_query_sram_resources parameter definition
 */
struct tf_query_sram_resources_parms {
	/**
	 * [in] Device type
	 *
	 * Device type for the session.
	 */
	enum tf_device_type device_type;

	/**
	 * [in] bp
	 * The pointer to the parent bp struct. This is only used for HWRM
	 * message passing within the portability layer. The type is struct
	 * bnxt.
	 */
	void *bp;

	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;

	/**
	 * [out] Bank resource count in 8 bytes entry
	 */

	uint32_t bank_resc_count[TF_SRAM_BANK_ID_MAX];

	/**
	 * [out] Dynamic SRAM Enable
	 */
	bool dynamic_sram_capable;

	/**
	 * [out] SRAM profile
	 */
	uint8_t sram_profile;
};

/**
 * Get SRAM resources information
 *
 * Used to retrieve sram bank partition information
 *
 * Returns success or failure code.
 */
int tf_query_sram_resources(struct tf *tfp,
			    struct tf_query_sram_resources_parms *parms);

/**
 * tf_set_sram_policy parameter definition
 */
struct tf_set_sram_policy_parms {
	/**
	 * [in] Device type
	 *
	 * Device type for the session.
	 */
	enum tf_device_type device_type;

	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;

	/**
	 * [in] Array of Bank id for each truflow tbl type
	 */
	enum tf_sram_bank_id bank_id[TF_TBL_TYPE_ACT_MODIFY_64B + 1];
};

/**
 * Set SRAM policy
 *
 * Used to assign SRAM bank index to all truflow table type.
 *
 * Returns success or failure code.
 */
int tf_set_sram_policy(struct tf *tfp,
		       struct tf_set_sram_policy_parms *parms);

/**
 * tf_get_sram_policy parameter definition
 */
struct tf_get_sram_policy_parms {
	/**
	 * [in] Device type
	 *
	 * Device type for the session.
	 */
	enum tf_device_type device_type;

	/**
	 * [in] Receive or transmit direction
	 */
	enum tf_dir dir;

	/**
	 * [out] Array of Bank id for each truflow tbl type
	 */
	enum tf_sram_bank_id bank_id[TF_TBL_TYPE_ACT_MODIFY_64B + 1];
};

/**
 * Get SRAM policy
 *
 * Used to get the assigned bank of table types.
 *
 * Returns success or failure code.
 */
int tf_get_sram_policy(struct tf *tfp,
		       struct tf_get_sram_policy_parms *parms);
#endif /* _TF_CORE_H_ */
