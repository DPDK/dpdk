/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#ifndef _TF_SESSION_H_
#define _TF_SESSION_H_

#include <stdint.h>
#include <stdlib.h>

#include "bitalloc.h"
#include "tf_core.h"
#include "tf_device.h"
#include "tf_rm.h"
#include "tf_resources.h"
#include "stack.h"
#include "ll.h"

/**
 * The Session module provides session control support. A session is
 * to the ULP layer known as a session_info instance. The session
 * private data is the actual session.
 *
 * Session manages:
 *   - The device and all the resources related to the device.
 *   - Any session sharing between ULP applications
 */

/** Session defines
 */
#define TF_SESSION_ID_INVALID     0xFFFFFFFF /** Invalid Session ID define */

/**
 * At this stage we are using fixed size entries so that each
 * stack entry represents either 2 or 4 RT (f/n)blocks. So we
 * take the total block allocation for truflow and divide that
 * by either 2 or 4.
 */
#ifdef TF_EM_ENTRY_IPV4_ONLY
#define TF_SESSION_EM_ENTRY_SIZE 2 /* 2 blocks per entry */
#else
#define TF_SESSION_EM_ENTRY_SIZE 4 /* 4 blocks per entry */
#endif

/**
 * Session
 *
 * Shared memory containing private TruFlow session information.
 * Through this structure the session can keep track of resource
 * allocations and (if so configured) any shadow copy of flow
 * information. It also holds info about Session Clients.
 *
 * Memory is assigned to the Truflow instance by way of
 * tf_open_session. Memory is allocated and owned by i.e. ULP.
 *
 * Access control to this shared memory is handled by the spin_lock in
 * tf_session_info.
 */
struct tf_session {
	/** TruFlow Version. Used to control the structure layout
	 * when sharing sessions. No guarantee that a secondary
	 * process would come from the same version of an executable.
	 */
	struct tf_session_version ver;

	/**
	 * Session ID, allocated by FW on tf_open_session()
	 */
	union tf_session_id session_id;

	/**
	 * Boolean controlling the use and availability of shared session.
	 * Shared session will allow the application to share resources
	 * on the firmware side without having to allocate them on firmware.
	 * Additional private session core_data will be allocated if this
	 * boolean is set to 'true', default 'false'.
	 *
	 */
	bool shared_session;

	/**
	 * This flag indicates the shared session on firmware side is created
	 * by this session. Some privileges may be assigned to this session.
	 *
	 */
	bool shared_session_creator;

	/**
	 * Boolean controlling the use and availability of shadow
	 * copy. Shadow copy will allow the TruFlow Core to keep track
	 * of resource content on the firmware side without having to
	 * query firmware. Additional private session core_data will
	 * be allocated if this boolean is set to 'true', default
	 * 'false'.
	 *
	 * Size of memory depends on the NVM Resource settings for the
	 * control channel.
	 */
	bool shadow_copy;

	/**
	 * Session Reference Count. To keep track of functions per
	 * session the ref_count is updated. There is also a
	 * parallel TruFlow Firmware ref_count in case the TruFlow
	 * Core goes away without informing the Firmware.
	 */
	uint8_t ref_count;

	/**
	 * Session Reference Count for attached sessions. To keep
	 * track of application sharing of a session the
	 * ref_count_attach is updated.
	 */
	uint8_t ref_count_attach;

	/**
	 * Device handle
	 */
	struct tf_dev_info dev;
	/**
	 * Device init flag. False if Device is not fully initialized,
	 * else true.
	 */
	bool dev_init;

	/**
	 * Linked list of clients registered for this session
	 */
	struct ll client_ll;

	/**
	 * em ext db reference for the session
	 */
	void *em_ext_db_handle;

	/**
	 * tcam db reference for the session
	 */
	void *tcam_db_handle;

	/**
	 * table db reference for the session
	 */
	void *tbl_db_handle;

	/**
	 * identifier db reference for the session
	 */
	void *id_db_handle;

	/**
	 * em db reference for the session
	 */
	void *em_db_handle;

	/**
	 * EM allocator for session
	 */
	void *em_pool[TF_DIR_MAX];

#ifdef TF_TCAM_SHARED
	/**
	 * tcam db reference for the session
	 */
	void *tcam_shared_db_handle;
#endif /* TF_TCAM_SHARED */

	/**
	 * SRAM db reference for the session
	 */
	void *sram_handle;

	/**
	 * if table db reference for the session
	 */
	void *if_tbl_db_handle;

	/**
	 * global db reference for the session
	 */
	void *global_db_handle;

	/**
	 * Number of slices per row for WC TCAM
	 */
	uint16_t wc_num_slices_per_row;
};

/**
 * Session Client
 *
 * Shared memory for each of the Session Clients. A session can have
 * one or more clients.
 */
struct tf_session_client {
	/**
	 * Linked list of clients
	 */
	struct ll_entry ll_entry; /* For inserting in link list, must be
				   * first field of struct.
				   */

	/**
	 * String containing name of control channel interface to be
	 * used for this session to communicate with firmware.
	 *
	 * ctrl_chan_name will be used as part of a name for any
	 * shared memory allocation.
	 */
	char ctrl_chan_name[TF_SESSION_NAME_MAX];

	/**
	 * Firmware FID, learned at time of Session Client create.
	 */
	uint16_t fw_fid;

	/**
	 * Session Client ID, allocated by FW on tf_register_session()
	 */
	union tf_session_client_id session_client_id;
};

/**
 * Session open parameter definition
 */
struct tf_session_open_session_parms {
	/**
	 * [in] Pointer to the TF open session configuration
	 */
	struct tf_open_session_parms *open_cfg;
};

/**
 * Session attach parameter definition
 */
struct tf_session_attach_session_parms {
	/**
	 * [in] Pointer to the TF attach session configuration
	 */
	struct tf_attach_session_parms *attach_cfg;
};

/**
 * Session close parameter definition
 */
struct tf_session_close_session_parms {
	/**
	 * []
	 */
	uint8_t *ref_count;
	/**
	 * []
	 */
	union tf_session_id *session_id;
};

/**
 * @page session Session Management
 *
 * @ref tf_session_open_session
 *
 * @ref tf_session_attach_session
 *
 * @ref tf_session_close_session
 *
 * @ref tf_session_is_fid_supported
 *
 * @ref tf_session_get_session_internal
 *
 * @ref tf_session_get_session
 *
 * @ref tf_session_get_session_client
 *
 * @ref tf_session_find_session_client_by_name
 *
 * @ref tf_session_find_session_client_by_fid
 *
 * @ref tf_session_get_device
 *
 * @ref tf_session_get_fw_session_id
 *
 * @ref tf_session_get_session_id
 *
 * @ref tf_session_is_shared_session_creator
 *
 * @ref tf_session_get_db
 *
 * @ref tf_session_set_db
 *
 * @ref tf_session_get_bp
 *
 * @ref tf_session_is_shared_session
 *
 * #define TF_SHARED
 * @ref tf_session_get_tcam_shared_db
 *
 * @ref tf_session_set_tcam_shared_db
 * #endif
 *
 * @ref tf_session_get_sram_db
 *
 * @ref tf_session_set_sram_db
 */

/**
 * Creates a host session with a corresponding firmware session.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] parms
 *   Pointer to the session open parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_open_session(struct tf *tfp,
			    struct tf_session_open_session_parms *parms);

/**
 * Attaches a previous created session.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] parms
 *   Pointer to the session attach parameters
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_attach_session(struct tf *tfp,
			      struct tf_session_attach_session_parms *parms);

/**
 * Closes a previous created session. Only possible if previous
 * registered Clients had been unregistered first.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in/out] parms
 *   Pointer to the session close parameters.
 *
 * Returns
 *   - (0) if successful.
 *   - (-EUSERS) if clients are still registered with the session.
 *   - (-EINVAL) on failure.
 */
int tf_session_close_session(struct tf *tfp,
			     struct tf_session_close_session_parms *parms);

/**
 * Verifies that the fid is supported by the session. Used to assure
 * that a function i.e. client/control channel is registered with the
 * session.
 *
 * [in] tfs
 *   Pointer to TF Session handle
 *
 * [in] fid
 *   FID value to check
 *
 * Returns
 *   - (true) if successful, else false
 *   - (-EINVAL) on failure.
 */
bool
tf_session_is_fid_supported(struct tf_session *tfs,
			    uint16_t fid);

/**
 * Looks up the private session information from the TF session
 * info. Does not perform a fid check against the registered
 * clients. Should be used if tf_session_get_session() was used
 * previously i.e. at the TF API boundary.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [out] tfs
 *   Pointer pointer to the session
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get_session_internal(struct tf *tfp,
				    struct tf_session **tfs);

/**
 * Looks up the private session information from the TF session
 * info. Performs a fid check against the clients on the session.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [out] tfs
 *   Pointer pointer to the session
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get_session(struct tf *tfp,
			   struct tf_session **tfs);

/**
 * Looks up client within the session.
 *
 * [in] tfs
 *   Pointer pointer to the session
 *
 * [in] session_client_id
 *   Client id to look for within the session
 *
 * Returns
 *   client if successful.
 *   - (NULL) on failure, client not found.
 */
struct tf_session_client *
tf_session_get_session_client(struct tf_session *tfs,
			      union tf_session_client_id session_client_id);

/**
 * Looks up client using name within the session.
 *
 * [in] session, pointer to the session
 *
 * [in] session_client_name, name of the client to lookup in the session
 *
 * Returns:
 *   - Pointer to the session, if found.
 *   - (NULL) on failure, client not found.
 */
struct tf_session_client *
tf_session_find_session_client_by_name(struct tf_session *tfs,
				       const char *ctrl_chan_name);

/**
 * Looks up client using the fid.
 *
 * [in] session, pointer to the session
 *
 * [in] fid, fid of the client to find
 *
 * Returns:
 *   - Pointer to the session, if found.
 *   - (NULL) on failure, client not found.
 */
struct tf_session_client *
tf_session_find_session_client_by_fid(struct tf_session *tfs,
				      uint16_t fid);

/**
 * Looks up the device information from the TF Session.
 *
 * [in] tfs
 *   Pointer to session handle
 *
 * [out] tfd
 *   Pointer to the device
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get_device(struct tf_session *tfs,
			  struct tf_dev_info **tfd);

/**
 * Returns the session and the device from the tfp.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [out] tfs
 *   Pointer to the session
 *
 * [out] tfd
 *   Pointer to the device

 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get(struct tf *tfp,
		   struct tf_session **tfs,
		   struct tf_dev_info **tfd);

/**
 * Looks up the FW Session id the requested TF handle.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [out] session_id
 *   Pointer to the session_id
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get_fw_session_id(struct tf *tfp,
				 uint8_t *fw_session_id);

/**
 * Looks up the Session id the requested TF handle.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [out] session_id
 *   Pointer to the session_id
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get_session_id(struct tf *tfp,
			      union tf_session_id *session_id);

/**
 * API to get the em_ext_db from tf_session.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [out] em_ext_db_handle, pointer to eem handle
 *
 * Returns:
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int
tf_session_get_em_ext_db(struct tf *tfp,
			void **em_ext_db_handle);

/**
 * API to set the em_ext_db in tf_session.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] em_ext_db_handle, pointer to eem handle
 *
 * Returns:
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int
tf_session_set_em_ext_db(struct tf *tfp,
			void *em_ext_db_handle);

/**
 * API to get the db from tf_session.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [out] db_handle, pointer to db handle
 *
 * Returns:
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int
tf_session_get_db(struct tf *tfp,
		   enum tf_module_type type,
		  void **db_handle);

/**
 * API to set the db in tf_session.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] db_handle, pointer to db handle
 *
 * Returns:
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int
tf_session_set_db(struct tf *tfp,
		   enum tf_module_type type,
		  void *db_handle);

/**
 * Check if the session is shared session.
 *
 * [in] session, pointer to the session
 *
 * Returns:
 *   - true if it is shared session
 *   - false if it is not shared session
 */
static inline bool
tf_session_is_shared_session(struct tf_session *tfs)
{
	return tfs->shared_session;
}

/**
 * Check if the session is the shared session creator
 *
 * [in] session, pointer to the session
 *
 * Returns:
 *   - true if it is the shared session creator
 *   - false if it is not the shared session creator
 */
static inline bool
tf_session_is_shared_session_creator(struct tf_session *tfs)
{
	return tfs->shared_session_creator;
}

/**
 * Get the pointer to the parent bnxt struct
 *
 * [in] session, pointer to the session
 *
 * Returns:
 *   - the pointer to the parent bnxt struct
 */
static inline struct bnxt*
tf_session_get_bp(struct tf *tfp)
{
	return tfp->bp;
}

/**
 * Set the pointer to the tcam shared database
 *
 * [in] session, pointer to the session
 *
 * Returns:
 *   - the pointer to the parent bnxt struct
 */
int
tf_session_set_tcam_shared_db(struct tf *tfp,
			      void *tcam_shared_db_handle);

/**
 * Get the pointer to the tcam shared database
 *
 * [in] session, pointer to the session
 *
 * Returns:
 *   - the pointer to the parent bnxt struct
 */
int
tf_session_get_tcam_shared_db(struct tf *tfp,
			      void **tcam_shared_db_handle);

/**
 * Set the pointer to the SRAM database
 *
 * [in] session, pointer to the session
 *
 * Returns:
 *   - the pointer to the parent bnxt struct
 */
int
tf_session_set_sram_db(struct tf *tfp,
		       void *sram_handle);

/**
 * Get the pointer to the SRAM database
 *
 * [in] session, pointer to the session
 *
 * Returns:
 *   - the pointer to the parent bnxt struct
 */
int
tf_session_get_sram_db(struct tf *tfp,
		       void **sram_handle);

/**
 * Set the pointer to the global cfg database
 *
 * [in] session, pointer to the session
 *
 * Returns:
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int
tf_session_set_global_db(struct tf *tfp,
			 void *global_handle);

/**
 * Get the pointer to the global cfg database
 *
 * [in] session, pointer to the session
 *
 * Returns:
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int
tf_session_get_global_db(struct tf *tfp,
			 void **global_handle);

/**
 * Set the pointer to the if table cfg database
 *
 * [in] session, pointer to the session
 *
 * Returns:
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int
tf_session_set_if_tbl_db(struct tf *tfp,
			 void *if_tbl_handle);

/**
 * Get the pointer to the if table cfg database
 *
 * [in] session, pointer to the session
 *
 * Returns:
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int
tf_session_get_if_tbl_db(struct tf *tfp,
			 void **if_tbl_handle);

#endif /* _TF_SESSION_H_ */
