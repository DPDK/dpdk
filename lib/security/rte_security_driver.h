/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 NXP.
 * Copyright(c) 2017 Intel Corporation.
 */

#ifndef _RTE_SECURITY_DRIVER_H_
#define _RTE_SECURITY_DRIVER_H_

/**
 * @file rte_security_driver.h
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
 * @param	device		Crypto/eth device pointer
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

__rte_internal
int rte_security_dynfield_register(void);

/**
 * Update the mbuf with provided metadata.
 *
 * @param	device		Crypto/eth device pointer
 * @param	sess		Security session structure
 * @param	mb		Packet buffer
 * @param	params		Metadata
 *
 * @return
 *  - Returns 0 if metadata updated successfully.
 *  - Returns -ve value for errors.
 */
typedef int (*security_set_pkt_metadata_t)(void *device,
		struct rte_security_session *sess, struct rte_mbuf *mb,
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
