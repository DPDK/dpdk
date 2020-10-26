/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_CORE_H_
#define _TF_CORE_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "hcapi/hcapi_cfa_defs.h"
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
	TF_DEVICE_TYPE_WH = 0, /**< Whitney+  */
	TF_DEVICE_TYPE_SR,     /**< Stingray  */
	TF_DEVICE_TYPE_THOR,   /**< Thor      */
	TF_DEVICE_TYPE_SR2,    /**< Stingray2 */
	TF_DEVICE_TYPE_MAX     /**< Maximum   */
};

/**
 * Identifier resource types
 */
enum tf_identifier_type {
	/**
	 *  The L2 Context is returned from the L2 Ctxt TCAM lookup
	 *  and can be used in WC TCAM or EM keys to virtualize further
	 *  lookups.
	 */
	TF_IDENT_TYPE_L2_CTXT_HIGH,
	/**
	 *  The L2 Context is returned from the L2 Ctxt TCAM lookup
	 *  and can be used in WC TCAM or EM keys to virtualize further
	 *  lookups.
	 */
	TF_IDENT_TYPE_L2_CTXT_LOW,
	/**
	 *  The WC profile func is returned from the L2 Ctxt TCAM lookup
	 *  to enable virtualization of the profile TCAM.
	 */
	TF_IDENT_TYPE_PROF_FUNC,
	/**
	 *  The WC profile ID is included in the WC lookup key
	 *  to enable virtualization of the WC TCAM hardware.
	 */
	TF_IDENT_TYPE_WC_PROF,
	/**
	 *  The EM profile ID is included in the EM lookup key
	 *  to enable virtualization of the EM hardware. (not required for SR2
	 *  as it has table scope)
	 */
	TF_IDENT_TYPE_EM_PROF,
	/**
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

	/** Wh+/SR Action Record */
	TF_TBL_TYPE_FULL_ACT_RECORD,
	/** Wh+/SR/Th Multicast Groups */
	TF_TBL_TYPE_MCAST_GROUPS,
	/** Wh+/SR Action Encap 8 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_8B,
	/** Wh+/SR Action Encap 16 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_16B,
	/** Action Encap 32 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_32B,
	/** Wh+/SR Action Encap 64 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_64B,
	/** Action Source Properties SMAC */
	TF_TBL_TYPE_ACT_SP_SMAC,
	/** Wh+/SR Action Source Properties SMAC IPv4 */
	TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	/** Action Source Properties SMAC IPv6 */
	TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	/** Wh+/SR Action Statistics 64 Bits */
	TF_TBL_TYPE_ACT_STATS_64,
	/** Wh+/SR Action Modify L4 Src Port */
	TF_TBL_TYPE_ACT_MODIFY_SPORT,
	/** Wh+/SR Action Modify L4 Dest Port */
	TF_TBL_TYPE_ACT_MODIFY_DPORT,
	/** Wh+/SR Action Modify IPv4 Source */
	TF_TBL_TYPE_ACT_MODIFY_IPV4,
	/** Meter Profiles */
	TF_TBL_TYPE_METER_PROF,
	/** Meter Instance */
	TF_TBL_TYPE_METER_INST,
	/** Mirror Config */
	TF_TBL_TYPE_MIRROR_CONFIG,
	/** UPAR */
	TF_TBL_TYPE_UPAR,
	/** SR2 Epoch 0 table */
	TF_TBL_TYPE_EPOCH0,
	/** SR2 Epoch 1 table  */
	TF_TBL_TYPE_EPOCH1,
	/** SR2 Metadata  */
	TF_TBL_TYPE_METADATA,
	/** SR2 CT State  */
	TF_TBL_TYPE_CT_STATE,
	/** SR2 Range Profile  */
	TF_TBL_TYPE_RANGE_PROF,
	/** SR2 Range Entry  */
	TF_TBL_TYPE_RANGE_ENTRY,
	/** SR2 LAG Entry  */
	TF_TBL_TYPE_LAG,
	/** SR2 VNIC/SVIF Table */
	TF_TBL_TYPE_VNIC_SVIF,
	/** Th/SR2 EM Flexible Key builder */
	TF_TBL_TYPE_EM_FKB,
	/** Th/SR2 WC Flexible Key builder */
	TF_TBL_TYPE_WC_FKB,

	/* External */

	/**
	 * External table type - initially 1 poolsize entries.
	 * All External table types are associated with a table
	 * scope. Internal types are not.
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
	/** The number of table scopes reequested */
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
	 * TrueFlow Version. Used to control the structure layout when
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
	 * set to the requeste resource number of that specific
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
	 * shared memory allocation.
	 */
	char ctrl_chan_name[TF_SESSION_NAME_MAX];
	/**
	 * [in] shadow_copy
	 *
	 * Boolean controlling the use and availability of shadow
	 * copy. Shadow copy will allow the TruFlow to keep track of
	 * resource content on the firmware side without having to
	 * query firmware. Additional private session core_data will
	 * be allocated if this boolean is set to 'true', default
	 * 'false'.
	 *
	 * Size of memory depends on the NVM Resource settings for the
	 * control channel.
	 */
	bool shadow_copy;
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
};

/**
 * Opens a new TruFlow Session or session client.
 *
 * What gets created depends on the passed in tfp content. If the tfp
 * does not have prior session data a new session with associated
 * session client. If tfp has a session already a session client will
 * be created. In both cases the session client is created using the
 * provided ctrl_chan_name.
 *
 * In case of session creation TruFlow will allocate session specific
 * memory, shared memory, to hold its session data. This data is
 * private to TruFlow.
 *
 * No other TruFlow APIs will succeed unless this API is first called
 * and succeeds.
 *
 * tf_open_session() returns a session id and session client id that
 * is used on all other TF APIs.
 *
 * A Session or session client can be closed using tf_close_session().
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
 * If shadow copy is enabled, the internal ref_cnt is set to 1 in the
 * shadow table for a newly allocated resource.
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
 * Decrement reference count.  Only release resource once refcnt goes to 0 if
 * shadow copy is enabled.
 *
 * Returns success or failure code.
 */
int tf_free_identifier(struct tf *tfp,
		       struct tf_free_identifier_parms *parms);

/**
 * Search identifier resource (experimental)
 *
 * If the shadow copy is enabled search_id is used to search for a matching
 * entry in the shadow table.  The shadow table consists of an array of
 * reference counts indexed by identifier.  If a matching entry is found hit is
 * set to TRUE, refcnt is increased by 1 and returned.  Otherwise, hit is
 * set to false and refcnt is set to 0.
 *
 * TODO: we may need a per table internal shadow copy enable flag to stage
 * the shadow table implementation.  We do not need the shadow table for other
 * tables at this time so we may only want to enable the identifier shadow.
 *
 * TODO: remove this pseudocode below added to show that if search fails
 * we shouldn't allocate a new entry but return.
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
	 * [in] SR2 only receive table access interface id
	 */
	uint32_t rx_tbl_if_id;
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
	 * [in] SR2 only receive table access interface id
	 */
	uint32_t tx_tbl_if_id;
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
 * On SR2 Firmware will allocate a scope ID.  On other devices, the scope
 * is a software construct to identify an EEM table.  This function will
 * divide the hash memory/buckets and records according to the device
 * device constraints based upon calculations using either the number of flows
 * requested or the size of memory indicated.  Other parameters passed in
 * determine the configuration (maximum key size, maximum external action record
 * size).
 *
 * This API will allocate the table region in DRAM, program the PTU page table
 * entries, and program the number of static buckets (if SR2) in the RX and TX
 * CFAs.  Buckets are assumed to start at 0 in the EM memory for the scope.
 * Upon successful completion of this API, hash tables are fully initialized and
 * ready for entries to be inserted.
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
 * (SR2 ILT) or Profile TCAM entries for either CFA (RX/TX) direction,
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
 * @ref tf_free_tcam_entry
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
 * search TCAM entry (experimental)
 *
 * Search for a TCAM entry
 *
 * This function searches the shadow copy of the TCAM table for a matching
 * entry.  Key and mask must match for hit to be set.  Only TruFlow core data
 * is accessed.  If shadow_copy is not enabled, an error is returned.
 *
 * Implementation:
 *
 * A hash is performed on the key/mask data and mapped to a shadow copy entry
 * where the full key/mask is populated.  If the full key/mask matches the
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
 * owned TCAM entries or search a shadow copy of the TCAM table for a
 * matching entry if search is enabled.	 Key, mask and result must match for
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
	 * [out] key size in bits
	 */
	uint16_t key_sz_in_bits;
	/**
	 * [out] struct containing result
	 */
	uint8_t *result;
	/**
	 * [out] struct containing result size in bits
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
 */

/**
 * tf_alloc_tbl_entry parameter definition
 */
struct tf_search_tbl_entry_parms {
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
	 * [in] Result data to search for
	 */
	uint8_t *result;
	/**
	 * [in] Result data size in bytes
	 */
	uint16_t result_sz_in_bytes;
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
	 * [out] Current ref count after allocation
	 */
	uint16_t ref_cnt;
	/**
	 * [out] Idx of allocated entry or found entry
	 */
	uint32_t idx;
};

/**
 * search Table Entry (experimental)
 *
 * This function searches the shadow copy of an index table for a matching
 * entry.  The result data must match for hit to be set.  Only TruFlow core
 * data is accessed.  If shadow_copy is not enabled, an error is returned.
 *
 * Implementation:
 *
 * A hash is performed on the result data and mappe3d to a shadow copy entry
 * where the result is populated.  If the result matches the entry, hit is set,
 * ref_cnt is incremented (if alloc), and the search status indicates what
 * action the caller can take regarding setting the entry.
 *
 * search status should be used as follows:
 * - On MISS, the caller should set the result into the returned index.
 *
 * - On REJECT, the caller should reject the flow since there are no resources.
 *
 * - On Hit, the matching index is returned to the caller.  Additionally, the
 *   ref_cnt is updated.
 *
 * Also returns success or failure code.
 */
int tf_search_tbl_entry(struct tf *tfp,
			struct tf_search_tbl_entry_parms *parms);

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
	 * [in] Enable search for matching entry. If the table type is
	 * internal the shadow copy will be searched before
	 * alloc. Session must be configured with shadow copy enabled.
	 */
	uint8_t search_enable;
	/**
	 * [in] Result data to search for (if search_enable)
	 */
	uint8_t *result;
	/**
	 * [in] Result data size in bytes (if search_enable)
	 */
	uint16_t result_sz_in_bytes;
	/**
	 * [out] If search_enable, set if matching entry found
	 */
	uint8_t hit;
	/**
	 * [out] Current ref count after allocation (if search_enable)
	 */
	uint16_t ref_cnt;
	/**
	 * [out] Idx of allocated entry or found entry (if search_enable)
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
 * allocate an entry or search an index table for a matching entry if
 * search is enabled (only the shadow copy of the table is accessed).
 *
 * If search is not enabled, the first available free entry is
 * returned. If search is enabled and a matching entry to entry_data
 * is found hit is set to TRUE and success is returned.
 *
 * External types:
 *
 * These are used to allocate inlined action record memory.
 *
 * Allocates an external index table action record.
 *
 * NOTE:
 * Implementation of the internals of this function will be a stack with push
 * and pop.
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
	/**
	 * [out] Reference count after free, only valid if session has been
	 * created with shadow_copy.
	 */
	uint16_t ref_cnt;
};

/**
 * free index table entry
 *
 * Used to free a previously allocated table entry.
 *
 * Internal types:
 *
 * If session has shadow_copy enabled the shadow DB is searched and if
 * found the element ref_cnt is decremented. If ref_cnt goes to
 * zero then the element is returned to the session pool.
 *
 * If the session does not have a shadow DB the element is free'ed and
 * given back to the session pool.
 *
 * External types:
 *
 * Free's an external index table action record.
 *
 * NOTE:
 * Implementation of the internals of this function will be a stack with push
 * and pop.
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
	 * [in] Entry index to write to
	 */
	uint32_t idx;
};

/**
 * set index table entry
 *
 * Used to insert an application programmed index table entry into a
 * previous allocated table location.  A shadow copy of the table
 * is maintained (if enabled) (only for internal objects)
 *
 * Returns success or failure code.
 */
int tf_set_tbl_entry(struct tf *tfp,
		     struct tf_set_tbl_entry_parms *parms);

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
	 * [in] Entry index to read
	 */
	uint32_t idx;
};

/**
 * get index table entry
 *
 * Used to retrieve a previous set index table entry.
 *
 * Reads and compares with the shadow table copy (if enabled) (only
 * for internal objects).
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
	 * [in] ID of table interface to use (SR2 only)
	 */
	uint32_t tbl_if_id;
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
	 * [in] structure containing flow delete handle information
	 */
	uint64_t flow_handle;
};
/**
 * tf_search_em_entry parameter definition
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
	 * [in] ID of table interface to use (SR2 only)
	 */
	uint32_t tbl_if_id;
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
	 * [in] epoch group IDs of entry to lookup
	 * 2 element array with 2 ids. (SR2 only)
	 */
	uint16_t *epochs;
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
 * When inserting an entry into an exact match table, the TruFlow library may
 * need to allocate a dynamic bucket for the entry (SR2 only).
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
 * search em hash entry table memory
 *
 * Internal:

 * This API looks up an EM entry in table memory with the specified EM
 * key or flow (flow takes precedence) and direction.
 *
 * The status will be one of: success or entry not found.  If the lookup
 * succeeds, a pointer to the matching entry and the result record associated
 * with the matching entry will be provided.
 *
 * If flow_handle is set, search shadow copy.
 *
 * Otherwise, query the fw with key to get result.
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
 *
 * @ref tf_restore_if_tbl_entry
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
	/** SR2 Ingress lookup table */
	TF_IF_TBL_TYPE_ILT,
	/** SR2 VNIC/SVIF Table */
	TF_IF_TBL_TYPE_VNIC_SVIF,
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

#endif /* _TF_CORE_H_ */
