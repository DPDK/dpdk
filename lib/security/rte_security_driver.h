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
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_compat.h>
#include "rte_security.h"

/**
 * @internal
 * Security session to be used by library for internal usage
 */
struct rte_security_session {
	RTE_MARKER cacheline0;
	uint64_t opaque_data;
	/**< Opaque user defined data */
	uint64_t fast_mdata;
	/**< Fast metadata to be used for inline path */
	rte_iova_t driver_priv_data_iova;
	/**< session private data IOVA address */

	RTE_MARKER cacheline1 __rte_cache_min_aligned;
	uint8_t driver_priv_data[0];
	/**< Private session material, variable size (depends on driver) */
};

/**
 * Helper macro to get driver private data
 */
#define SECURITY_GET_SESS_PRIV(s) \
	((void *)(((struct rte_security_session *)s)->driver_priv_data))
#define SECURITY_GET_SESS_PRIV_IOVA(s) \
	(((struct rte_security_session *)s)->driver_priv_data_iova)

/**
 * Configure a security session on a device.
 *
 * @param	device		Crypto/eth device pointer
 * @param	conf		Security session configuration
 * @param	sess		Pointer to Security private session structure
 *
 * @return
 *  - Returns 0 if private session structure have been created successfully.
 *  - Returns -EINVAL if input parameters are invalid.
 *  - Returns -ENOTSUP if crypto device does not support the crypto transform.
 */
typedef int (*security_session_create_t)(void *device,
		struct rte_security_session_conf *conf,
		struct rte_security_session *sess);

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
 * Configure a MACsec secure channel (SC) on a device.
 *
 * @param	device		Crypto/eth device pointer
 * @param	conf		MACsec SC configuration params
 *
 * @return
 *  - positive sc_id if SC is created successfully.
 *  - -EINVAL if input parameters are invalid.
 *  - -ENOTSUP if device does not support MACsec.
 *  - -ENOMEM if the SC cannot be created.
 */
typedef int (*security_macsec_sc_create_t)(void *device, struct rte_security_macsec_sc *conf);

/**
 * Free MACsec secure channel (SC).
 *
 * @param	device		Crypto/eth device pointer
 * @param	sc_id		MACsec SC ID
 * @param	dir		Direction of SC
 */
typedef int (*security_macsec_sc_destroy_t)(void *device, uint16_t sc_id,
		enum rte_security_macsec_direction dir);

/**
 * Configure a MACsec security Association (SA) on a device.
 *
 * @param	device		Crypto/eth device pointer
 * @param	conf		MACsec SA configuration params
 *
 * @return
 *  - positive sa_id if SA is created successfully.
 *  - -EINVAL if input parameters are invalid.
 *  - -ENOTSUP if device does not support MACsec.
 *  - -ENOMEM if the SA cannot be created.
 */
typedef int (*security_macsec_sa_create_t)(void *device, struct rte_security_macsec_sa *conf);

/**
 * Free MACsec security association (SA).
 *
 * @param	device		Crypto/eth device pointer
 * @param	sa_id		MACsec SA ID
 * @param	dir		Direction of SA
 */
typedef int (*security_macsec_sa_destroy_t)(void *device, uint16_t sa_id,
		enum rte_security_macsec_direction dir);

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
 * Get MACsec secure channel stats from the PMD.
 *
 * @param	device		Crypto/eth device pointer
 * @param	sc_id		secure channel ID created by rte_security_macsec_sc_create()
 * @param	dir		direction of SC
 * @param	stats		SC stats of the driver
 *
 * @return
 *  - 0 if success.
 *  - -EINVAL if sc_id or device is invalid.
 */
typedef int (*security_macsec_sc_stats_get_t)(void *device, uint16_t sc_id,
		enum rte_security_macsec_direction dir,
		struct rte_security_macsec_sc_stats *stats);

/**
 * Get MACsec SA stats from the PMD.
 *
 * @param	device		Crypto/eth device pointer
 * @param	sa_id		secure channel ID created by rte_security_macsec_sc_create()
 * @param	dir		direction of SA
 * @param	stats		SC stats of the driver
 *
 * @return
 *  - 0 if success.
 *  - -EINVAL if sa_id or device is invalid.
 */
typedef int (*security_macsec_sa_stats_get_t)(void *device, uint16_t sa_id,
		enum rte_security_macsec_direction dir,
		struct rte_security_macsec_sa_stats *stats);



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
	security_capabilities_get_t capabilities_get;
	/**< Get security capabilities. */
	security_macsec_sc_create_t macsec_sc_create;
	/**< Configure a MACsec security channel (SC). */
	security_macsec_sc_destroy_t macsec_sc_destroy;
	/**< Free a MACsec security channel (SC). */
	security_macsec_sa_create_t macsec_sa_create;
	/**< Configure a MACsec security association (SA). */
	security_macsec_sa_destroy_t macsec_sa_destroy;
	/**< Free a MACsec security association (SA). */
	security_macsec_sc_stats_get_t macsec_sc_stats_get;
	/**< Get MACsec SC statistics. */
	security_macsec_sa_stats_get_t macsec_sa_stats_get;
	/**< Get MACsec SA statistics. */
};

#ifdef __cplusplus
}
#endif

#endif /* _RTE_SECURITY_DRIVER_H_ */
