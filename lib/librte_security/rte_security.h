/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 NXP.
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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
 *     * Neither the name of NXP nor the names of its
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

#ifndef _RTE_SECURITY_H_
#define _RTE_SECURITY_H_

/**
 * @file rte_security.h
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * RTE Security Common Definitions
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

#include <rte_compat.h>
#include <rte_common.h>
#include <rte_crypto.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_mempool.h>

/** IPSec protocol mode */
enum rte_security_ipsec_sa_mode {
	RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT = 1,
	/**< IPSec Transport mode */
	RTE_SECURITY_IPSEC_SA_MODE_TUNNEL,
	/**< IPSec Tunnel mode */
};

/** IPSec Protocol */
enum rte_security_ipsec_sa_protocol {
	RTE_SECURITY_IPSEC_SA_PROTO_AH = 1,
	/**< AH protocol */
	RTE_SECURITY_IPSEC_SA_PROTO_ESP,
	/**< ESP protocol */
};

/** IPSEC tunnel type */
enum rte_security_ipsec_tunnel_type {
	RTE_SECURITY_IPSEC_TUNNEL_IPV4 = 1,
	/**< Outer header is IPv4 */
	RTE_SECURITY_IPSEC_TUNNEL_IPV6,
	/**< Outer header is IPv6 */
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
	uint16_t sess_cnt;
	/**< Number of sessions attached to this context */
};

/**
 * IPSEC tunnel parameters
 *
 * These parameters are used to build outbound tunnel headers.
 */
struct rte_security_ipsec_tunnel_param {
	enum rte_security_ipsec_tunnel_type type;
	/**< Tunnel type: IPv4 or IPv6 */
	RTE_STD_C11
	union {
		struct {
			struct in_addr src_ip;
			/**< IPv4 source address */
			struct in_addr dst_ip;
			/**< IPv4 destination address */
			uint8_t dscp;
			/**< IPv4 Differentiated Services Code Point */
			uint8_t df;
			/**< IPv4 Don't Fragment bit */
			uint8_t ttl;
			/**< IPv4 Time To Live */
		} ipv4;
		/**< IPv4 header parameters */
		struct {
			struct in6_addr src_addr;
			/**< IPv6 source address */
			struct in6_addr dst_addr;
			/**< IPv6 destination address */
			uint8_t dscp;
			/**< IPv6 Differentiated Services Code Point */
			uint32_t flabel;
			/**< IPv6 flow label */
			uint8_t hlimit;
			/**< IPv6 hop limit */
		} ipv6;
		/**< IPv6 header parameters */
	};
};

/**
 * IPsec Security Association option flags
 */
struct rte_security_ipsec_sa_options {
	/**< Extended Sequence Numbers (ESN)
	 *
	 * * 1: Use extended (64 bit) sequence numbers
	 * * 0: Use normal sequence numbers
	 */
	uint32_t esn : 1;

	/**< UDP encapsulation
	 *
	 * * 1: Do UDP encapsulation/decapsulation so that IPSEC packets can
	 *      traverse through NAT boxes.
	 * * 0: No UDP encapsulation
	 */
	uint32_t udp_encap : 1;

	/**< Copy DSCP bits
	 *
	 * * 1: Copy IPv4 or IPv6 DSCP bits from inner IP header to
	 *      the outer IP header in encapsulation, and vice versa in
	 *      decapsulation.
	 * * 0: Do not change DSCP field.
	 */
	uint32_t copy_dscp : 1;

	/**< Copy IPv6 Flow Label
	 *
	 * * 1: Copy IPv6 flow label from inner IPv6 header to the
	 *      outer IPv6 header.
	 * * 0: Outer header is not modified.
	 */
	uint32_t copy_flabel : 1;

	/**< Copy IPv4 Don't Fragment bit
	 *
	 * * 1: Copy the DF bit from the inner IPv4 header to the outer
	 *      IPv4 header.
	 * * 0: Outer header is not modified.
	 */
	uint32_t copy_df : 1;

	/**< Decrement inner packet Time To Live (TTL) field
	 *
	 * * 1: In tunnel mode, decrement inner packet IPv4 TTL or
	 *      IPv6 Hop Limit after tunnel decapsulation, or before tunnel
	 *      encapsulation.
	 * * 0: Inner packet is not modified.
	 */
	uint32_t dec_ttl : 1;
};

/** IPSec security association direction */
enum rte_security_ipsec_sa_direction {
	RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
	/**< Encrypt and generate digest */
	RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
	/**< Verify digest and decrypt */
};

/**
 * IPsec security association configuration data.
 *
 * This structure contains data required to create an IPsec SA security session.
 */
struct rte_security_ipsec_xform {
	uint32_t spi;
	/**< SA security parameter index */
	uint32_t salt;
	/**< SA salt */
	struct rte_security_ipsec_sa_options options;
	/**< various SA options */
	enum rte_security_ipsec_sa_direction direction;
	/**< IPSec SA Direction - Egress/Ingress */
	enum rte_security_ipsec_sa_protocol proto;
	/**< IPsec SA Protocol - AH/ESP */
	enum rte_security_ipsec_sa_mode mode;
	/**< IPsec SA Mode - transport/tunnel */
	struct rte_security_ipsec_tunnel_param tunnel;
	/**< Tunnel parameters, NULL for transport mode */
	uint64_t esn_soft_limit;
	/**< ESN for which the overflow event need to be raised */
};

/**
 * MACsec security session configuration
 */
struct rte_security_macsec_xform {
	/** To be Filled */
	int dummy;
};

/**
 * Security session action type.
 */
enum rte_security_session_action_type {
	RTE_SECURITY_ACTION_TYPE_NONE,
	/**< No security actions */
	RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO,
	/**< Crypto processing for security protocol is processed inline
	 * during transmission
	 */
	RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL,
	/**< All security protocol processing is performed inline during
	 * transmission
	 */
	RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL
	/**< All security protocol processing including crypto is performed
	 * on a lookaside accelerator
	 */
};

/** Security session protocol definition */
enum rte_security_session_protocol {
	RTE_SECURITY_PROTOCOL_IPSEC = 1,
	/**< IPsec Protocol */
	RTE_SECURITY_PROTOCOL_MACSEC,
	/**< MACSec Protocol */
};

/**
 * Security session configuration
 */
struct rte_security_session_conf {
	enum rte_security_session_action_type action_type;
	/**< Type of action to be performed on the session */
	enum rte_security_session_protocol protocol;
	/**< Security protocol to be configured */
	RTE_STD_C11
	union {
		struct rte_security_ipsec_xform ipsec;
		struct rte_security_macsec_xform macsec;
	};
	/**< Configuration parameters for security session */
	struct rte_crypto_sym_xform *crypto_xform;
	/**< Security Session Crypto Transformations */
	void *userdata;
	/**< Application specific userdata to be saved with session */
};

struct rte_security_session {
	void *sess_private_data;
	/**< Private session material */
};

/**
 * Create security session as specified by the session configuration
 *
 * @param   instance	security instance
 * @param   conf	session configuration parameters
 * @param   mp		mempool to allocate session objects from
 * @return
 *  - On success, pointer to session
 *  - On failure, NULL
 */
struct rte_security_session * __rte_experimental
rte_security_session_create(struct rte_security_ctx *instance,
			    struct rte_security_session_conf *conf,
			    struct rte_mempool *mp);

/**
 * Update security session as specified by the session configuration
 *
 * @param   instance	security instance
 * @param   sess	session to update parameters
 * @param   conf	update configuration parameters
 * @return
 *  - On success returns 0
 *  - On failure return errno
 */
int __rte_experimental
rte_security_session_update(struct rte_security_ctx *instance,
			    struct rte_security_session *sess,
			    struct rte_security_session_conf *conf);

/**
 * Get the size of the security session data for a device.
 *
 * @param   instance	security instance.
 *
 * @return
 *   - Size of the private data, if successful
 *   - 0 if device is invalid or does not support the operation.
 */
unsigned int __rte_experimental
rte_security_session_get_size(struct rte_security_ctx *instance);

/**
 * Free security session header and the session private data and
 * return it to its original mempool.
 *
 * @param   instance	security instance
 * @param   sess	security session to freed
 *
 * @return
 *  - 0 if successful.
 *  - -EINVAL if session is NULL.
 *  - -EBUSY if not all device private data has been freed.
 */
int __rte_experimental
rte_security_session_destroy(struct rte_security_ctx *instance,
			     struct rte_security_session *sess);

/**
 *  Updates the buffer with device-specific defined metadata
 *
 * @param	instance	security instance
 * @param	sess		security session
 * @param	mb		packet mbuf to set metadata on.
 * @param	params		device-specific defined parameters
 *				required for metadata
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
int __rte_experimental
rte_security_set_pkt_metadata(struct rte_security_ctx *instance,
			      struct rte_security_session *sess,
			      struct rte_mbuf *mb, void *params);

/**
 * Get userdata associated with the security session. Device specific metadata
 * provided would be used to uniquely identify the security session being
 * referred to. This userdata would be registered while creating the session,
 * and application can use this to identify the SA etc.
 *
 * Device specific metadata would be set in mbuf for inline processed inbound
 * packets. In addition, the same metadata would be set for IPsec events
 * reported by rte_eth_event framework.
 *
 * @param   instance	security instance
 * @param   md		device-specific metadata
 *
 * @return
 *  - On success, userdata
 *  - On failure, NULL
 */
void * __rte_experimental
rte_security_get_userdata(struct rte_security_ctx *instance, uint64_t md);

/**
 * Attach a session to a symmetric crypto operation
 *
 * @param	sym_op	crypto operation
 * @param	sess	security session
 */
static inline int __rte_experimental
__rte_security_attach_session(struct rte_crypto_sym_op *sym_op,
			      struct rte_security_session *sess)
{
	sym_op->sec_session = sess;

	return 0;
}

static inline void * __rte_experimental
get_sec_session_private_data(const struct rte_security_session *sess)
{
	return sess->sess_private_data;
}

static inline void __rte_experimental
set_sec_session_private_data(struct rte_security_session *sess,
			     void *private_data)
{
	sess->sess_private_data = private_data;
}

/**
 * Attach a session to a crypto operation.
 * This API is needed only in case of RTE_SECURITY_SESS_CRYPTO_PROTO_OFFLOAD
 * For other rte_security_session_action_type, ol_flags in rte_mbuf may be
 * defined to perform security operations.
 *
 * @param	op	crypto operation
 * @param	sess	security session
 */
static inline int __rte_experimental
rte_security_attach_session(struct rte_crypto_op *op,
			    struct rte_security_session *sess)
{
	if (unlikely(op->type != RTE_CRYPTO_OP_TYPE_SYMMETRIC))
		return -EINVAL;

	op->sess_type =  RTE_CRYPTO_OP_SECURITY_SESSION;

	return __rte_security_attach_session(op->sym, sess);
}

struct rte_security_macsec_stats {
	uint64_t reserved;
};

struct rte_security_ipsec_stats {
	uint64_t reserved;

};

struct rte_security_stats {
	enum rte_security_session_protocol protocol;
	/**< Security protocol to be configured */

	RTE_STD_C11
	union {
		struct rte_security_macsec_stats macsec;
		struct rte_security_ipsec_stats ipsec;
	};
};

/**
 * Get security session statistics
 *
 * @param	instance	security instance
 * @param	sess		security session
 * @param	stats		statistics
 * @return
 *  - On success return 0
 *  - On failure errno
 */
int __rte_experimental
rte_security_session_stats_get(struct rte_security_ctx *instance,
			       struct rte_security_session *sess,
			       struct rte_security_stats *stats);

/**
 * Security capability definition
 */
struct rte_security_capability {
	enum rte_security_session_action_type action;
	/**< Security action type*/
	enum rte_security_session_protocol protocol;
	/**< Security protocol */
	RTE_STD_C11
	union {
		struct {
			enum rte_security_ipsec_sa_protocol proto;
			/**< IPsec SA protocol */
			enum rte_security_ipsec_sa_mode mode;
			/**< IPsec SA mode */
			enum rte_security_ipsec_sa_direction direction;
			/**< IPsec SA direction */
			struct rte_security_ipsec_sa_options options;
			/**< IPsec SA supported options */
		} ipsec;
		/**< IPsec capability */
		struct {
			/* To be Filled */
			int dummy;
		} macsec;
		/**< MACsec capability */
	};

	const struct rte_cryptodev_capabilities *crypto_capabilities;
	/**< Corresponding crypto capabilities for security capability  */

	uint32_t ol_flags;
	/**< Device offload flags */
};

#define RTE_SECURITY_TX_OLOAD_NEED_MDATA	0x00000001
/**< HW needs metadata update, see rte_security_set_pkt_metadata().
 */

#define RTE_SECURITY_TX_HW_TRAILER_OFFLOAD	0x00000002
/**< HW constructs trailer of packets
 * Transmitted packets will have the trailer added to them
 * by hardawre. The next protocol field will be based on
 * the mbuf->inner_esp_next_proto field.
 */
#define RTE_SECURITY_RX_HW_TRAILER_OFFLOAD	0x00010000
/**< HW removes trailer of packets
 * Received packets have no trailer, the next protocol field
 * is supplied in the mbuf->inner_esp_next_proto field.
 * Inner packet is not modified.
 */

/**
 * Security capability index used to query a security instance for a specific
 * security capability
 */
struct rte_security_capability_idx {
	enum rte_security_session_action_type action;
	enum rte_security_session_protocol protocol;

	RTE_STD_C11
	union {
		struct {
			enum rte_security_ipsec_sa_protocol proto;
			enum rte_security_ipsec_sa_mode mode;
			enum rte_security_ipsec_sa_direction direction;
		} ipsec;
	};
};

/**
 *  Returns array of security instance capabilities
 *
 * @param	instance	Security instance.
 *
 * @return
 *   - Returns array of security capabilities.
 *   - Return NULL if no capabilities available.
 */
const struct rte_security_capability * __rte_experimental
rte_security_capabilities_get(struct rte_security_ctx *instance);

/**
 * Query if a specific capability is available on security instance
 *
 * @param	instance	security instance.
 * @param	idx		security capability index to match against
 *
 * @return
 *   - Returns pointer to security capability on match of capability
 *     index criteria.
 *   - Return NULL if the capability not matched on security instance.
 */
const struct rte_security_capability * __rte_experimental
rte_security_capability_get(struct rte_security_ctx *instance,
			    struct rte_security_capability_idx *idx);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_SECURITY_H_ */
