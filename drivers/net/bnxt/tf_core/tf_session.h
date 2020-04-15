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
#include "tf_rm.h"

/** Session defines
 */
#define TF_SESSIONS_MAX	          1          /** max # sessions */
#define TF_SESSION_ID_INVALID     0xFFFFFFFF /** Invalid Session ID define */

/** Session
 *
 * Shared memory containing private TruFlow session information.
 * Through this structure the session can keep track of resource
 * allocations and (if so configured) any shadow copy of flow
 * information.
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

	/** Device type, provided by tf_open_session().
	 */
	enum tf_device_type device_type;

	/** Session ID, allocated by FW on tf_open_session().
	 */
	union tf_session_id session_id;

	/**
	 * String containing name of control channel interface to be
	 * used for this session to communicate with firmware.
	 *
	 * ctrl_chan_name will be used as part of a name for any
	 * shared memory allocation.
	 */
	char ctrl_chan_name[TF_SESSION_NAME_MAX];

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
	 * session the ref_count is incremented. There is also a
	 * parallel TruFlow Firmware ref_count in case the TruFlow
	 * Core goes away without informing the Firmware.
	 */
	uint8_t ref_count;

	/** CRC32 seed table */
#define TF_LKUP_SEED_MEM_SIZE 512
	uint32_t lkup_em_seed_mem[TF_DIR_MAX][TF_LKUP_SEED_MEM_SIZE];
	/** Lookup3 init values */
	uint32_t lkup_lkup3_init_cfg[TF_DIR_MAX];

};
#endif /* _TF_SESSION_H_ */
