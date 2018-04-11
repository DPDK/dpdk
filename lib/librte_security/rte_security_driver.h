/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
 *   Copyright 2017 NXP.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_SECURITY_DRIVER_H_
#define _RTE_SECURITY_DRIVER_H_

/**
 * @file rte_security_driver.h
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * RTE Security Common Definitions
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "rte_security.h"

/**
 * Configure a security session on a device.
 *
 * @param	device		Crypto/eth device pointer
 * @param	conf		Security session configuration
 * @param	sess		Pointer to Security private session structure
 * @param	mp		Mempool where the private session is allocated
 *
 * @return
 *  - Returns 0 if private session structure have been created successfully.
 *  - Returns -EINVAL if input parameters are invalid.
 *  - Returns -ENOTSUP if crypto device does not support the crypto transform.
 *  - Returns -ENOMEM if the private session could not be allocated.
 */
typedef int (*security_session_create_t)(void *device,
		struct rte_security_session_conf *conf,
		struct rte_security_session *sess,
		struct rte_mempool *mp);

/**
 * Free driver private session data.
 *
 * @param	dev		Crypto/eth device pointer
 * @param	sess		Security session structure
 */
typedef int (*security_session_destroy_t)(void *device,
		struct rte_security_session *sess);

/**
 * Update driver private session data.
 *
 * @param	device		Crypto/eth device pointer
 * @param	sess		Pointer to Security private session structure
 * @param	conf		Security session configuration
 *
 * @return
 *  - Returns 0 if private session structure have been updated successfully.
 *  - Returns -EINVAL if input parameters are invalid.
 *  - Returns -ENOTSUP if crypto device does not support the crypto transform.
 */
typedef int (*security_session_update_t)(void *device,
		struct rte_security_session *sess,
		struct rte_security_session_conf *conf);

/**
 * Get the size of a security session
 *
 * @param	device		Crypto/eth device pointer
 *
 * @return
 *  - On success returns the size of the session structure for device
 *  - On failure returns 0
 */
typedef unsigned int (*security_session_get_size)(void *device);

/**
 * Get stats from the PMD.
 *
 * @param	device		Crypto/eth device pointer
 * @param	sess		Pointer to Security private session structure
 * @param	stats		Security stats of the driver
 *
 * @return
 *  - Returns 0 if private session structure have been updated successfully.
 *  - Returns -EINVAL if session parameters are invalid.
 */
typedef int (*security_session_stats_get_t)(void *device,
		struct rte_security_session *sess,
		struct rte_security_stats *stats);

/**
 * Update the mbuf with provided metadata.
 *
 * @param	sess		Security session structure
 * @param	mb		Packet buffer
 * @param	mt		Metadata
 *
 * @return
 *  - Returns 0 if metadata updated successfully.
 *  - Returns -ve value for errors.
 */
typedef int (*security_set_pkt_metadata_t)(void *device,
		struct rte_security_session *sess, struct rte_mbuf *m,
		void *params);

/**
 * Get application specific userdata associated with the security session.
 * Device specific metadata provided would be used to uniquely identify
 * the security session being referred to.
 *
 * @param	device		Crypto/eth device pointer
 * @param	md		Metadata
 * @param	userdata	Pointer to receive userdata
 *
 * @return
 *  - Returns 0 if userdata is retrieved successfully.
 *  - Returns -ve value for errors.
 */
typedef int (*security_get_userdata_t)(void *device,
		uint64_t md, void **userdata);

/**
 * Get security capabilities of the device.
 *
 * @param	device		crypto/eth device pointer
 *
 * @return
 *  - Returns rte_security_capability pointer on success.
 *  - Returns NULL on error.
 */
typedef const struct rte_security_capability *(*security_capabilities_get_t)(
		void *device);

/** Security operations function pointer table */
struct rte_security_ops {
	security_session_create_t session_create;
	/**< Configure a security session. */
	security_session_update_t session_update;
	/**< Update a security session. */
	security_session_get_size session_get_size;
	/**< Return size of security session. */
	security_session_stats_get_t session_stats_get;
	/**< Get security session statistics. */
	security_session_destroy_t session_destroy;
	/**< Clear a security sessions private data. */
	security_set_pkt_metadata_t set_pkt_metadata;
	/**< Update mbuf metadata. */
	security_get_userdata_t get_userdata;
	/**< Get userdata associated with session which processed the packet. */
	security_capabilities_get_t capabilities_get;
	/**< Get security capabilities. */
};

#ifdef __cplusplus
}
#endif

#endif /* _RTE_SECURITY_DRIVER_H_ */
