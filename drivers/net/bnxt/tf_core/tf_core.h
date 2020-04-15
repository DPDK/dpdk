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

#include "tf_project.h"

/**
 * @file
 *
 * Truflow Core API Header File
 */

/********** BEGIN Truflow Core DEFINITIONS **********/

/**
 * direction
 */
enum tf_dir {
	TF_DIR_RX,  /**< Receive */
	TF_DIR_TX,  /**< Transmit */
	TF_DIR_MAX
};

/**
 * External pool size
 *
 * Defines a single pool of external action records of
 * fixed size.  Currently, this is an index.
 */
#define TF_EXT_POOL_ENTRY_SZ_BYTES 1

/**
 *  External pool entry count
 *
 *  Defines the number of entries in the external action pool
 */
#define TF_EXT_POOL_ENTRY_CNT (1 * 1024)

/**
 * Number of external pools
 */
#define TF_EXT_POOL_CNT_MAX 1

/**
 * External pool Id
 */
#define TF_EXT_POOL_0      0 /**< matches TF_TBL_TYPE_EXT   */
#define TF_EXT_POOL_1      1 /**< matches TF_TBL_TYPE_EXT_0 */

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


/** Session Version defines
 *
 * The version controls the format of the tf_session and
 * tf_session_info structure. This is to assure upgrade between
 * versions can be supported.
 */
#define TF_SESSION_VER_MAJOR  1   /**< Major Version */
#define TF_SESSION_VER_MINOR  0   /**< Minor Version */
#define TF_SESSION_VER_UPDATE 0   /**< Update Version */

/** Session Name
 *
 * Name of the TruFlow control channel interface.  Expects
 * format to be RTE Name specific, i.e. rte_eth_dev_get_name_by_port()
 */
#define TF_SESSION_NAME_MAX       64

#define TF_FW_SESSION_ID_INVALID  0xFF  /**< Invalid FW Session ID define */

/** Session Identifier
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

/** Session Version
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

/** Session supported device types
 *
 */
enum tf_device_type {
	TF_DEVICE_TYPE_WH = 0, /**< Whitney+  */
	TF_DEVICE_TYPE_BRD2,   /**< TBD       */
	TF_DEVICE_TYPE_BRD3,   /**< TBD       */
	TF_DEVICE_TYPE_BRD4,   /**< TBD       */
	TF_DEVICE_TYPE_MAX     /**< Maximum   */
};

/** TruFlow Session Information
 *
 * Structure defining a TruFlow Session, also known as a Management
 * session. This structure is initialized at time of
 * tf_open_session(). It is passed to all of the TruFlow APIs as way
 * to prescribe and isolate resources between different TruFlow ULP
 * Applications.
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

/** TruFlow handle
 *
 * Contains a pointer to the session info. Allocated by ULP and passed
 * to TruFlow using tf_open_session(). TruFlow will populate the
 * session info at that time. Additional 'opens' can be done using
 * same session_info by using tf_attach_session().
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
 * tf_open_session parameters definition.
 */
struct tf_open_session_parms {
	/** [in] ctrl_chan_name
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
	/** [in] shadow_copy
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
	/** [in/out] session_id
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
	/** [in] device type
	 *
	 * Device type is passed, one of Wh+, Brd2, Brd3, Brd4
	 */
	enum tf_device_type device_type;
};

/**
 * Opens a new TruFlow management session.
 *
 * TruFlow will allocate session specific memory, shared memory, to
 * hold its session data. This data is private to TruFlow.
 *
 * Multiple PFs can share the same session. An association, refcount,
 * between session and PFs is maintained within TruFlow. Thus, a PF
 * can attach to an existing session, see tf_attach_session().
 *
 * No other TruFlow APIs will succeed unless this API is first called and
 * succeeds.
 *
 * tf_open_session() returns a session id that can be used on attach.
 *
 * [in] tfp
 *   Pointer to TF handle
 * [in] parms
 *   Pointer to open parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_open_session(struct tf *tfp,
		    struct tf_open_session_parms *parms);

struct tf_attach_session_parms {
	/** [in] ctrl_chan_name
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

	/** [in] attach_chan_name
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

	/** [in] session_id
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
 * Attaches to an existing session. Used when more than one PF wants
 * to share a single session. In that case all TruFlow management
 * traffic will be sent to the TruFlow firmware using the 'PF' that
 * did the attach not the session ctrl channel.
 *
 * Attach will increment a ref count as to manage the shared session data.
 *
 * [in] tfp, pointer to TF handle
 * [in] parms, pointer to attach parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_attach_session(struct tf *tfp,
		      struct tf_attach_session_parms *parms);

/**
 * Closes an existing session. Cleans up all hardware and firmware
 * state associated with the TruFlow application session when the last
 * PF associated with the session results in refcount to be zero.
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
enum tf_identifier_type {
	/** The L2 Context is returned from the L2 Ctxt TCAM lookup
	 *  and can be used in WC TCAM or EM keys to virtualize further
	 *  lookups.
	 */
	TF_IDENT_TYPE_L2_CTXT,
	/** The WC profile func is returned from the L2 Ctxt TCAM lookup
	 *  to enable virtualization of the profile TCAM.
	 */
	TF_IDENT_TYPE_PROF_FUNC,
	/** The WC profile ID is included in the WC lookup key
	 *  to enable virtualization of the WC TCAM hardware.
	 */
	TF_IDENT_TYPE_WC_PROF,
	/** The EM profile ID is included in the EM lookup key
	 *  to enable virtualization of the EM hardware. (not required for Brd4
	 *  as it has table scope)
	 */
	TF_IDENT_TYPE_EM_PROF,
	/** The L2 func is included in the ILT result and from recycling to
	 *  enable virtualization of further lookups.
	 */
	TF_IDENT_TYPE_L2_FUNC
};

/**
 * TCAM table type
 */
enum tf_tcam_tbl_type {
	TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	TF_TCAM_TBL_TYPE_PROF_TCAM,
	TF_TCAM_TBL_TYPE_WC_TCAM,
	TF_TCAM_TBL_TYPE_SP_TCAM,
	TF_TCAM_TBL_TYPE_CT_RULE_TCAM,
	TF_TCAM_TBL_TYPE_VEB_TCAM,
	TF_TCAM_TBL_TYPE_MAX

};

/**
 * Enumeration of TruFlow table types. A table type is used to identify a
 * resource object.
 *
 * NOTE: The table type TF_TBL_TYPE_EXT is unique in that it is
 * the only table type that is connected with a table scope.
 */
enum tf_tbl_type {
	/** Wh+/Brd2 Action Record */
	TF_TBL_TYPE_FULL_ACT_RECORD,
	/** Multicast Groups */
	TF_TBL_TYPE_MCAST_GROUPS,
	/** Action Encap 8 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_8B,
	/** Action Encap 16 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_16B,
	/** Action Encap 64 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_32B,
	/** Action Encap 64 Bytes */
	TF_TBL_TYPE_ACT_ENCAP_64B,
	/** Action Source Properties SMAC */
	TF_TBL_TYPE_ACT_SP_SMAC,
	/** Action Source Properties SMAC IPv4 */
	TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	/** Action Source Properties SMAC IPv6 */
	TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	/** Action Statistics 64 Bits */
	TF_TBL_TYPE_ACT_STATS_64,
	/** Action Modify L4 Src Port */
	TF_TBL_TYPE_ACT_MODIFY_SPORT,
	/** Action Modify L4 Dest Port */
	TF_TBL_TYPE_ACT_MODIFY_DPORT,
	/** Action Modify IPv4 Source */
	TF_TBL_TYPE_ACT_MODIFY_IPV4_SRC,
	/** Action _Modify L4 Dest Port */
	TF_TBL_TYPE_ACT_MODIFY_IPV4_DEST,
	/** Action Modify IPv6 Source */
	TF_TBL_TYPE_ACT_MODIFY_IPV6_SRC,
	/** Action Modify IPv6 Destination */
	TF_TBL_TYPE_ACT_MODIFY_IPV6_DEST,

	/* HW */

	/** Meter Profiles */
	TF_TBL_TYPE_METER_PROF,
	/** Meter Instance */
	TF_TBL_TYPE_METER_INST,
	/** Mirror Config */
	TF_TBL_TYPE_MIRROR_CONFIG,
	/** UPAR */
	TF_TBL_TYPE_UPAR,
	/** Brd4 Epoch 0 table */
	TF_TBL_TYPE_EPOCH0,
	/** Brd4 Epoch 1 table  */
	TF_TBL_TYPE_EPOCH1,
	/** Brd4 Metadata  */
	TF_TBL_TYPE_METADATA,
	/** Brd4 CT State  */
	TF_TBL_TYPE_CT_STATE,
	/** Brd4 Range Profile  */
	TF_TBL_TYPE_RANGE_PROF,
	/** Brd4 Range Entry  */
	TF_TBL_TYPE_RANGE_ENTRY,
	/** Brd4 LAG Entry  */
	TF_TBL_TYPE_LAG,
	/** Brd4 only VNIC/SVIF Table */
	TF_TBL_TYPE_VNIC_SVIF,

	/* External */

	/** External table type - initially 1 poolsize entries.
	 * All External table types are associated with a table
	 * scope. Internal types are not.
	 */
	TF_TBL_TYPE_EXT,
	/** Future - external pool of size0 entries */
	TF_TBL_TYPE_EXT_0,
	TF_TBL_TYPE_MAX
};
#endif /* _TF_CORE_H_ */
