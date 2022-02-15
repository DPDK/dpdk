/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017,2019-2020 NXP
 * Copyright(c) 2017-2020 Intel Corporation.
 */

#ifndef _RTE_SECURITY_H_
#define _RTE_SECURITY_H_

/**
 * @file rte_security.h
 *
 * RTE Security Common Definitions
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

#include <rte_compat.h>
#include <rte_common.h>
#include <rte_crypto.h>
#include <rte_ip.h>
#include <rte_mbuf_dyn.h>

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
 * IPSEC tunnel header verification mode
 *
 * Controls how outer IP header is verified in inbound.
 */
#define RTE_SECURITY_IPSEC_TUNNEL_VERIFY_DST_ADDR     0x1
#define RTE_SECURITY_IPSEC_TUNNEL_VERIFY_SRC_DST_ADDR 0x2

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
	uint32_t flags;
	/**< Flags for security context */
};

#define RTE_SEC_CTX_F_FAST_SET_MDATA 0x00000001
/**< Driver uses fast metadata update without using driver specific callback */

#define RTE_SEC_CTX_F_FAST_GET_UDATA 0x00000002
/**< Driver provides udata using fast method without using driver specific
 * callback. For fast mdata and udata, mbuf dynamic field would be registered
 * by driver via rte_security_dynfield_register().
 */

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

struct rte_security_ipsec_udp_param {
	uint16_t sport;
	uint16_t dport;
};

/**
 * IPsec Security Association option flags
 */
struct rte_security_ipsec_sa_options {
	/** Extended Sequence Numbers (ESN)
	 *
	 * * 1: Use extended (64 bit) sequence numbers
	 * * 0: Use normal sequence numbers
	 */
	uint32_t esn : 1;

	/** UDP encapsulation
	 *
	 * * 1: Do UDP encapsulation/decapsulation so that IPSEC packets can
	 *      traverse through NAT boxes.
	 * * 0: No UDP encapsulation
	 */
	uint32_t udp_encap : 1;

	/** Copy DSCP bits
	 *
	 * * 1: Copy IPv4 or IPv6 DSCP bits from inner IP header to
	 *      the outer IP header in encapsulation, and vice versa in
	 *      decapsulation.
	 * * 0: Do not change DSCP field.
	 */
	uint32_t copy_dscp : 1;

	/** Copy IPv6 Flow Label
	 *
	 * * 1: Copy IPv6 flow label from inner IPv6 header to the
	 *      outer IPv6 header.
	 * * 0: Outer header is not modified.
	 */
	uint32_t copy_flabel : 1;

	/** Copy IPv4 Don't Fragment bit
	 *
	 * * 1: Copy the DF bit from the inner IPv4 header to the outer
	 *      IPv4 header.
	 * * 0: Outer header is not modified.
	 */
	uint32_t copy_df : 1;

	/** Decrement inner packet Time To Live (TTL) field
	 *
	 * * 1: In tunnel mode, decrement inner packet IPv4 TTL or
	 *      IPv6 Hop Limit after tunnel decapsulation, or before tunnel
	 *      encapsulation.
	 * * 0: Inner packet is not modified.
	 */
	uint32_t dec_ttl : 1;

	/** Explicit Congestion Notification (ECN)
	 *
	 * * 1: In tunnel mode, enable outer header ECN Field copied from
	 *      inner header in tunnel encapsulation, or inner header ECN
	 *      field construction in decapsulation.
	 * * 0: Inner/outer header are not modified.
	 */
	uint32_t ecn : 1;

	/** Security statistics
	 *
	 * * 1: Enable per session security statistics collection for
	 *      this SA, if supported by the driver.
	 * * 0: Disable per session security statistics collection for this SA.
	 */
	uint32_t stats : 1;

	/** Disable IV generation in PMD
	 *
	 * * 1: Disable IV generation in PMD. When disabled, IV provided in
	 *      rte_crypto_op will be used by the PMD.
	 *
	 * * 0: Enable IV generation in PMD. When enabled, PMD generated random
	 *      value would be used and application is not required to provide
	 *      IV.
	 *
	 * Note: For inline cases, IV generation would always need to be handled
	 * by the PMD.
	 */
	uint32_t iv_gen_disable : 1;

	/** Verify tunnel header in inbound
	 * * ``RTE_SECURITY_IPSEC_TUNNEL_VERIFY_DST_ADDR``: Verify destination
	 *   IP address.
	 *
	 * * ``RTE_SECURITY_IPSEC_TUNNEL_VERIFY_SRC_DST_ADDR``: Verify both
	 *   source and destination IP addresses.
	 */
	uint32_t tunnel_hdr_verify : 2;

	/** Verify UDP encapsulation ports in inbound
	 *
	 * * 1: Match UDP source and destination ports
	 * * 0: Do not match UDP ports
	 */
	uint32_t udp_ports_verify : 1;

	/** Compute/verify inner packet IPv4 header checksum in tunnel mode
	 *
	 * * 1: For outbound, compute inner packet IPv4 header checksum
	 *      before tunnel encapsulation and for inbound, verify after
	 *      tunnel decapsulation.
	 * * 0: Inner packet IP header checksum is not computed/verified.
	 *
	 * The checksum verification status would be set in mbuf using
	 * RTE_MBUF_F_RX_IP_CKSUM_xxx flags.
	 *
	 * Inner IP checksum computation can also be enabled(per operation)
	 * by setting the flag RTE_MBUF_F_TX_IP_CKSUM in mbuf.
	 */
	uint32_t ip_csum_enable : 1;

	/** Compute/verify inner packet L4 checksum in tunnel mode
	 *
	 * * 1: For outbound, compute inner packet L4 checksum before
	 *      tunnel encapsulation and for inbound, verify after
	 *      tunnel decapsulation.
	 * * 0: Inner packet L4 checksum is not computed/verified.
	 *
	 * The checksum verification status would be set in mbuf using
	 * RTE_MBUF_F_RX_L4_CKSUM_xxx flags.
	 *
	 * Inner L4 checksum computation can also be enabled(per operation)
	 * by setting the flags RTE_MBUF_F_TX_TCP_CKSUM or RTE_MBUF_F_TX_SCTP_CKSUM or
	 * RTE_MBUF_F_TX_UDP_CKSUM or RTE_MBUF_F_TX_L4_MASK in mbuf.
	 */
	uint32_t l4_csum_enable : 1;

	/** Enable IP reassembly on inline inbound packets.
	 *
	 * * 1: Enable driver to try reassembly of encrypted IP packets for
	 *      this SA, if supported by the driver. This feature will work
	 *      only if user has successfully set IP reassembly config params
	 *      using rte_eth_ip_reassembly_conf_set() for the inline Ethernet
	 *      device. PMD need to register mbuf dynamic fields using
	 *      rte_eth_ip_reassembly_dynfield_register() and security session
	 *      creation would fail if dynfield is not registered successfully.
	 * * 0: Disable IP reassembly of packets (default).
	 */
	uint32_t ip_reassembly_en : 1;

	/** Reserved bit fields for future extension
	 *
	 * User should ensure reserved_opts is cleared as it may change in
	 * subsequent releases to support new options.
	 *
	 * Note: Reduce number of bits in reserved_opts for every new option.
	 */
	uint32_t reserved_opts : 17;
};

/** IPSec security association direction */
enum rte_security_ipsec_sa_direction {
	RTE_SECURITY_IPSEC_SA_DIR_EGRESS,
	/**< Encrypt and generate digest */
	RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
	/**< Verify digest and decrypt */
};

/**
 * Configure soft and hard lifetime of an IPsec SA
 *
 * Lifetime of an IPsec SA would specify the maximum number of packets or bytes
 * that can be processed. IPsec operations would start failing once any hard
 * limit is reached.
 *
 * Soft limits can be specified to generate notification when the SA is
 * approaching hard limits for lifetime. For inline operations, reaching soft
 * expiry limit would result in raising an eth event for the same. For lookaside
 * operations, this would result in a warning returned in
 * ``rte_crypto_op.aux_flags``.
 */
struct rte_security_ipsec_lifetime {
	uint64_t packets_soft_limit;
	/**< Soft expiry limit in number of packets */
	uint64_t bytes_soft_limit;
	/**< Soft expiry limit in bytes */
	uint64_t packets_hard_limit;
	/**< Soft expiry limit in number of packets */
	uint64_t bytes_hard_limit;
	/**< Soft expiry limit in bytes */
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
	struct rte_security_ipsec_lifetime life;
	/**< IPsec SA lifetime */
	uint32_t replay_win_sz;
	/**< Anti replay window size to enable sequence replay attack handling.
	 * replay checking is disabled if the window size is 0.
	 */
	union {
		uint64_t value;
		struct {
			uint32_t low;
			uint32_t hi;
		};
	} esn;
	/**< Extended Sequence Number */
	struct rte_security_ipsec_udp_param udp;
	/**< UDP parameters, ignored when udp_encap option not specified */
};

/**
 * MACsec security session configuration
 */
struct rte_security_macsec_xform {
	/** To be Filled */
	int dummy;
};

/**
 * PDCP Mode of session
 */
enum rte_security_pdcp_domain {
	RTE_SECURITY_PDCP_MODE_CONTROL,	/**< PDCP control plane */
	RTE_SECURITY_PDCP_MODE_DATA,	/**< PDCP data plane */
	RTE_SECURITY_PDCP_MODE_SHORT_MAC,	/**< PDCP short mac */
};

/** PDCP Frame direction */
enum rte_security_pdcp_direction {
	RTE_SECURITY_PDCP_UPLINK,	/**< Uplink */
	RTE_SECURITY_PDCP_DOWNLINK,	/**< Downlink */
};

/** PDCP Sequence Number Size selectors */
enum rte_security_pdcp_sn_size {
	/** PDCP_SN_SIZE_5: 5bit sequence number */
	RTE_SECURITY_PDCP_SN_SIZE_5 = 5,
	/** PDCP_SN_SIZE_7: 7bit sequence number */
	RTE_SECURITY_PDCP_SN_SIZE_7 = 7,
	/** PDCP_SN_SIZE_12: 12bit sequence number */
	RTE_SECURITY_PDCP_SN_SIZE_12 = 12,
	/** PDCP_SN_SIZE_15: 15bit sequence number */
	RTE_SECURITY_PDCP_SN_SIZE_15 = 15,
	/** PDCP_SN_SIZE_18: 18bit sequence number */
	RTE_SECURITY_PDCP_SN_SIZE_18 = 18
};

/**
 * PDCP security association configuration data.
 *
 * This structure contains data required to create a PDCP security session.
 */
struct rte_security_pdcp_xform {
	int8_t bearer;	/**< PDCP bearer ID */
	/** Enable in order delivery, this field shall be set only if
	 * driver/HW is capable. See RTE_SECURITY_PDCP_ORDERING_CAP.
	 */
	uint8_t en_ordering;
	/** Notify driver/HW to detect and remove duplicate packets.
	 * This field should be set only when driver/hw is capable.
	 * See RTE_SECURITY_PDCP_DUP_DETECT_CAP.
	 */
	uint8_t remove_duplicates;
	/** PDCP mode of operation: Control or data */
	enum rte_security_pdcp_domain domain;
	/** PDCP Frame Direction 0:UL 1:DL */
	enum rte_security_pdcp_direction pkt_dir;
	/** Sequence number size, 5/7/12/15/18 */
	enum rte_security_pdcp_sn_size sn_size;
	/** Starting Hyper Frame Number to be used together with the SN
	 * from the PDCP frames
	 */
	uint32_t hfn;
	/** HFN Threshold for key renegotiation */
	uint32_t hfn_threshold;
	/** HFN can be given as a per packet value also.
	 * As we do not have IV in case of PDCP, and HFN is
	 * used to generate IV. IV field can be used to get the
	 * per packet HFN while enq/deq.
	 * If hfn_ovrd field is set, user is expected to set the
	 * per packet HFN in place of IV. PMDs will extract the HFN
	 * and perform operations accordingly.
	 */
	uint8_t hfn_ovrd;
	/** In case of 5G NR, a new protocol (SDAP) header may be set
	 * inside PDCP payload which should be authenticated but not
	 * encrypted. Hence, driver should be notified if SDAP is
	 * enabled or not, so that SDAP header is not encrypted.
	 */
	uint8_t sdap_enabled;
	/** Reserved for future */
	uint16_t reserved;
};

/** DOCSIS direction */
enum rte_security_docsis_direction {
	RTE_SECURITY_DOCSIS_UPLINK,
	/**< Uplink
	 * - Decryption, followed by CRC Verification
	 */
	RTE_SECURITY_DOCSIS_DOWNLINK,
	/**< Downlink
	 * - CRC Generation, followed by Encryption
	 */
};

/**
 * DOCSIS security session configuration.
 *
 * This structure contains data required to create a DOCSIS security session.
 */
struct rte_security_docsis_xform {
	enum rte_security_docsis_direction direction;
	/**< DOCSIS direction */
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
	RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
	/**< All security protocol processing including crypto is performed
	 * on a lookaside accelerator
	 */
	RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO
	/**< Similar to ACTION_TYPE_NONE but crypto processing for security
	 * protocol is processed synchronously by a CPU.
	 */
};

/** Security session protocol definition */
enum rte_security_session_protocol {
	RTE_SECURITY_PROTOCOL_IPSEC = 1,
	/**< IPsec Protocol */
	RTE_SECURITY_PROTOCOL_MACSEC,
	/**< MACSec Protocol */
	RTE_SECURITY_PROTOCOL_PDCP,
	/**< PDCP Protocol */
	RTE_SECURITY_PROTOCOL_DOCSIS,
	/**< DOCSIS Protocol */
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
		struct rte_security_pdcp_xform pdcp;
		struct rte_security_docsis_xform docsis;
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
	uint64_t opaque_data;
	/**< Opaque user defined data */
};

/**
 * Create security session as specified by the session configuration
 *
 * @param   instance	security instance
 * @param   conf	session configuration parameters
 * @param   mp		mempool to allocate session objects from
 * @param   priv_mp	mempool to allocate session private data objects from
 * @return
 *  - On success, pointer to session
 *  - On failure, NULL
 */
struct rte_security_session *
rte_security_session_create(struct rte_security_ctx *instance,
			    struct rte_security_session_conf *conf,
			    struct rte_mempool *mp,
			    struct rte_mempool *priv_mp);

/**
 * Update security session as specified by the session configuration
 *
 * @param   instance	security instance
 * @param   sess	session to update parameters
 * @param   conf	update configuration parameters
 * @return
 *  - On success returns 0
 *  - On failure returns a negative errno value.
 */
__rte_experimental
int
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
unsigned int
rte_security_session_get_size(struct rte_security_ctx *instance);

/**
 * Free security session header and the session private data and
 * return it to its original mempool.
 *
 * @param   instance	security instance
 * @param   sess	security session to be freed
 *
 * @return
 *  - 0 if successful.
 *  - -EINVAL if session or context instance is NULL.
 *  - -EBUSY if not all device private data has been freed.
 *  - -ENOTSUP if destroying private data is not supported.
 *  - other negative values in case of freeing private data errors.
 */
int
rte_security_session_destroy(struct rte_security_ctx *instance,
			     struct rte_security_session *sess);

/** Device-specific metadata field type */
typedef uint64_t rte_security_dynfield_t;
/** Dynamic mbuf field for device-specific metadata */
extern int rte_security_dynfield_offset;

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Get pointer to mbuf field for device-specific metadata.
 *
 * For performance reason, no check is done,
 * the dynamic field may not be registered.
 * @see rte_security_dynfield_is_registered
 *
 * @param	mbuf	packet to access
 * @return pointer to mbuf field
 */
__rte_experimental
static inline rte_security_dynfield_t *
rte_security_dynfield(struct rte_mbuf *mbuf)
{
	return RTE_MBUF_DYNFIELD(mbuf,
		rte_security_dynfield_offset,
		rte_security_dynfield_t *);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Check whether the dynamic field is registered.
 *
 * @return true if rte_security_dynfield_register() has been called.
 */
__rte_experimental
static inline bool rte_security_dynfield_is_registered(void)
{
	return rte_security_dynfield_offset >= 0;
}

/** Function to call PMD specific function pointer set_pkt_metadata() */
__rte_experimental
extern int __rte_security_set_pkt_metadata(struct rte_security_ctx *instance,
					   struct rte_security_session *sess,
					   struct rte_mbuf *m, void *params);

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
static inline int
rte_security_set_pkt_metadata(struct rte_security_ctx *instance,
			      struct rte_security_session *sess,
			      struct rte_mbuf *mb, void *params)
{
	/* Fast Path */
	if (instance->flags & RTE_SEC_CTX_F_FAST_SET_MDATA) {
		*rte_security_dynfield(mb) =
			(rte_security_dynfield_t)(sess->sess_private_data);
		return 0;
	}

	/* Jump to PMD specific function pointer */
	return __rte_security_set_pkt_metadata(instance, sess, mb, params);
}

/** Function to call PMD specific function pointer get_userdata() */
__rte_experimental
extern void *__rte_security_get_userdata(struct rte_security_ctx *instance,
					 uint64_t md);

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
__rte_experimental
static inline void *
rte_security_get_userdata(struct rte_security_ctx *instance, uint64_t md)
{
	/* Fast Path */
	if (instance->flags & RTE_SEC_CTX_F_FAST_GET_UDATA)
		return (void *)(uintptr_t)md;

	/* Jump to PMD specific function pointer */
	return __rte_security_get_userdata(instance, md);
}

/**
 * Attach a session to a symmetric crypto operation
 *
 * @param	sym_op	crypto operation
 * @param	sess	security session
 */
static inline int
__rte_security_attach_session(struct rte_crypto_sym_op *sym_op,
			      struct rte_security_session *sess)
{
	sym_op->sec_session = sess;

	return 0;
}

static inline void *
get_sec_session_private_data(const struct rte_security_session *sess)
{
	return sess->sess_private_data;
}

static inline void
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
static inline int
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
	uint64_t ipackets;  /**< Successfully received IPsec packets. */
	uint64_t opackets;  /**< Successfully transmitted IPsec packets.*/
	uint64_t ibytes;    /**< Successfully received IPsec bytes. */
	uint64_t obytes;    /**< Successfully transmitted IPsec bytes. */
	uint64_t ierrors;   /**< IPsec packets receive/decrypt errors. */
	uint64_t oerrors;   /**< IPsec packets transmit/encrypt errors. */
	uint64_t reserved1; /**< Reserved for future use. */
	uint64_t reserved2; /**< Reserved for future use. */
};

struct rte_security_pdcp_stats {
	uint64_t reserved;
};

struct rte_security_docsis_stats {
	uint64_t reserved;
};

struct rte_security_stats {
	enum rte_security_session_protocol protocol;
	/**< Security protocol to be configured */

	RTE_STD_C11
	union {
		struct rte_security_macsec_stats macsec;
		struct rte_security_ipsec_stats ipsec;
		struct rte_security_pdcp_stats pdcp;
		struct rte_security_docsis_stats docsis;
	};
};

/**
 * Get security session statistics
 *
 * @param	instance	security instance
 * @param	sess		security session
 * If security session is NULL then global (per security instance) statistics
 * will be retrieved, if supported. Global statistics collection is not
 * dependent on the per session statistics configuration.
 * @param	stats		statistics
 * @return
 *  - On success, return 0
 *  - On failure, a negative value
 */
__rte_experimental
int
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
			uint32_t replay_win_sz_max;
			/**< IPsec Anti Replay Window Size. A '0' value
			 * indicates that Anti Replay is not supported.
			 */
		} ipsec;
		/**< IPsec capability */
		struct {
			/* To be Filled */
			int dummy;
		} macsec;
		/**< MACsec capability */
		struct {
			enum rte_security_pdcp_domain domain;
			/**< PDCP mode of operation: Control or data */
			uint32_t capa_flags;
			/**< Capability flags, see RTE_SECURITY_PDCP_* */
		} pdcp;
		/**< PDCP capability */
		struct {
			enum rte_security_docsis_direction direction;
			/**< DOCSIS direction */
		} docsis;
		/**< DOCSIS capability */
	};

	const struct rte_cryptodev_capabilities *crypto_capabilities;
	/**< Corresponding crypto capabilities for security capability  */

	uint32_t ol_flags;
	/**< Device offload flags */
};

/** Underlying Hardware/driver which support PDCP may or may not support
 * packet ordering. Set RTE_SECURITY_PDCP_ORDERING_CAP if it support.
 * If it is not set, driver/HW assumes packets received are in order
 * and it will be application's responsibility to maintain ordering.
 */
#define RTE_SECURITY_PDCP_ORDERING_CAP		0x00000001

/** Underlying Hardware/driver which support PDCP may or may not detect
 * duplicate packet. Set RTE_SECURITY_PDCP_DUP_DETECT_CAP if it support.
 * If it is not set, driver/HW assumes there is no duplicate packet received.
 */
#define RTE_SECURITY_PDCP_DUP_DETECT_CAP	0x00000002

#define RTE_SECURITY_TX_OLOAD_NEED_MDATA	0x00000001
/**< HW needs metadata update, see rte_security_set_pkt_metadata().
 */

#define RTE_SECURITY_TX_HW_TRAILER_OFFLOAD	0x00000002
/**< HW constructs trailer of packets
 * Transmitted packets will have the trailer added to them
 * by hardware. The next protocol field will be based on
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
		struct {
			enum rte_security_pdcp_domain domain;
			uint32_t capa_flags;
		} pdcp;
		struct {
			enum rte_security_docsis_direction direction;
		} docsis;
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
const struct rte_security_capability *
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
const struct rte_security_capability *
rte_security_capability_get(struct rte_security_ctx *instance,
			    struct rte_security_capability_idx *idx);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_SECURITY_H_ */
