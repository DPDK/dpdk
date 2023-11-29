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
	uint8_t driver_priv_data[];
	/**< Private session material, variable size (depends on driver) */
};

/**
 * Security context for crypto/eth devices
 *
 * Security instance for each driver to register security operations.
 * The application can get the security context from the crypto/eth device id
 * using the APIs rte_cryptodev_get_sec_ctx()/rte_eth_dev_get_sec_ctx()
 * This structure is used to identify the device(crypto/eth) for which the
 * security operations need to be performed.
 */
struct rte_security_ctx {
	void *device;
	/**< Crypto/ethernet device attached */
	const struct rte_security_ops *ops;
	/**< Pointer to security ops for the device */
	uint32_t flags;
	/**< Flags for security context */
	uint16_t sess_cnt;
	/**< Number of sessions attached to this context */
	uint16_t macsec_sc_cnt;
	/**< Number of MACsec SC attached to this context */
	uint16_t macsec_sa_cnt;
	/**< Number of MACsec SA attached to this context */
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
 * @internal
 * Register mbuf dynamic field for security inline ingress Out-of-Place(OOP)
 * processing.
 */
__rte_internal
int rte_security_oop_dynfield_register(void);

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

/**
 * Configure security device to inject packets to an ethdev port.
 *
 * @param	device		Crypto/eth device pointer
 * @param	port_id		Port identifier of the ethernet device to which packets need to be
 *				injected.
 * @param	enable		Flag to enable and disable connection between a security device and
 *				an ethdev port.
 * @return
 *   - 0 if successful.
 *   - -EINVAL if context NULL or port_id is invalid.
 *   - -EBUSY if devices are not in stopped state.
 *   - -ENOTSUP if security device does not support injecting to the ethdev port.
 */
typedef int (*security_rx_inject_configure)(void *device, uint16_t port_id, bool enable);

/**
 * Perform security processing of packets and inject the processed packet to
 * ethdev Rx.
 *
 * Rx inject would behave similarly to ethdev loopback but with the additional
 * security processing.
 *
 * @param	device		Crypto/eth device pointer
 * @param	pkts		The address of an array of *nb_pkts* pointers to
 *				*rte_mbuf* structures which contain the packets.
 * @param	sess		The address of an array of *nb_pkts* pointers to
 *				*rte_security_session* structures corresponding
 *				to each packet.
 * @param	nb_pkts		The maximum number of packets to process.
 *
 * @return
 *   The number of packets successfully injected to ethdev Rx. The return
 *   value can be less than the value of the *nb_pkts* parameter when the
 *   PMD internal queues have been filled up.
 */
typedef uint16_t (*security_inb_pkt_rx_inject)(void *device,
		struct rte_mbuf **pkts, struct rte_security_session **sess,
		uint16_t nb_pkts);

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
	security_rx_inject_configure rx_inject_configure;
	/**< Rx inject configure. */
	security_inb_pkt_rx_inject inb_pkt_rx_inject;
	/**< Perform security processing and do Rx inject. */
};

#ifdef __cplusplus
}
#endif

#endif /* _RTE_SECURITY_DRIVER_H_ */
