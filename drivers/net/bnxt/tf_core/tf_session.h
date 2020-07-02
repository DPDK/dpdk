/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
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
#include "tf_tbl.h"
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
 * Number of EM entries. Static for now will be removed
 * when parameter added at a later date. At this stage we
 * are using fixed size entries so that each stack entry
 * represents 4 RT (f/n)blocks. So we take the total block
 * allocation for truflow and divide that by 4.
 */
#define TF_SESSION_TOTAL_FN_BLOCKS (1024 * 8) /* 8K blocks */
#define TF_SESSION_EM_ENTRY_SIZE 4 /* 4 blocks per entry */
#define TF_SESSION_EM_POOL_SIZE \
	(TF_SESSION_TOTAL_FN_BLOCKS / TF_SESSION_EM_ENTRY_SIZE)

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
	/** TrueFlow Version. Used to control the structure layout
	 * when sharing sessions. No guarantee that a secondary
	 * process would come from the same version of an executable.
	 */
	struct tf_session_version ver;

	/**
	 * Session ID, allocated by FW on tf_open_session()
	 */
	union tf_session_id session_id;

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
 * [in] tfp
 *   Pointer to TF handle
 *
 * [out] tfd
 *   Pointer pointer to the device
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
int tf_session_get_device(struct tf_session *tfs,
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

#endif /* _TF_SESSION_H_ */
