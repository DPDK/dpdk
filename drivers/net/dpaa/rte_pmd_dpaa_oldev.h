/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020-2026 NXP
 */

#ifndef _RTE_PMD_DPAA_OLDEV_H
#define _RTE_PMD_DPAA_OLDEV_H

/**
 * @file rte_pmd_dpaa_oldev.h
 *
 * NXP DPAA offline (O/H) port PMD-specific API.
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 *
 * These APIs configure the classification, local gateway (LGW) and related
 * uplink information for the DPAA offline port through the kernel control
 * device.
 */

#include <stdint.h>
#include <rte_common.h>
#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

/** IPv4 address type for the inner/static address pair used in uplink
 * classification.
 */
#define RTE_PMD_DPAA_OLDEV_IPV4_ADDR_TYPE  0x04
/** IPv6 address type for the inner/static address pair used in uplink
 * classification.
 */
#define RTE_PMD_DPAA_OLDEV_IPV6_ADDR_TYPE  0x06

/** Maximum number of GTP UDP ports an application can listen on. */
#define RTE_PMD_DPAA_OLDEV_MAX_NUM_PORTS 2

/** IPv4 subnet type for an LGW subnet entry. */
#define RTE_PMD_DPAA_OLDEV_IPV4_SUBNET_TYPE  0x04
/** IPv6 subnet type for an LGW subnet entry. */
#define RTE_PMD_DPAA_OLDEV_IPV6_SUBNET_TYPE  0x06
/** Maximum number of LGW subnets that can be programmed. */
#define RTE_PMD_DPAA_OLDEV_MAX_NUM_SUBNETS 4

/** Set in struct rte_pmd_dpaa_ip_pair::flags when the address pair carries
 * a valid inner IP address.
 */
#define RTE_PMD_DPAA_OLDEV_CLASSIF_INNER_IP 0x1

/** Set in struct rte_pmd_dpaa_ip_pair::flags when the address pair carries
 * a static IP address.
 */
#define RTE_PMD_DPAA_OLDEV_CLASSIF_STATIC_IP 0x2

/** Set in struct rte_pmd_dpaa_ip_pair::flags to indicate the telecom
 * application is listening only on the static IP address.
 */
#define RTE_PMD_DPAA_OLDEV_TELECOM_LISTEN_ON_ONLY_STATICIP 0x4

/** Set in struct rte_pmd_dpaa_ip_pair::flags to indicate the telecom
 * application is listening on both the static and the inner IP address.
 */
#define RTE_PMD_DPAA_OLDEV_TELECOM_LISTEN_ON_BOTH_STATIC_INNER_IP 0x8

/** Set in struct rte_pmd_dpaa_ip_pair::flags to indicate the telecom
 * application is listening only on the inner IP address.
 */
#define RTE_PMD_DPAA_OLDEV_TELECOM_LISTEN_ON_ONLY_INNERIP 0x10

/**
 * Holds an IPv4 or IPv6 address.
 *
 * For an IPv4 address only ip_addr[0] is used; for an IPv6 address all
 * four words are used. Which representation applies is determined by
 * the addr_type field of the enclosing struct rte_pmd_dpaa_ip_pair.
 */
struct rte_pmd_dpaa_ip_addr {
	uint32_t ip_addr[4]; /**< Address words, network byte order. */
};

/** Static/inner IP address pair used for uplink classification. */
struct __rte_packed_begin rte_pmd_dpaa_ip_pair {
	struct rte_pmd_dpaa_ip_addr static_ip; /**< Static IP address. */
	struct rte_pmd_dpaa_ip_addr inner_ip;  /**< Inner IP address. */
	uint8_t flags;		/**< Combination of
				  *  RTE_PMD_DPAA_OLDEV_CLASSIF_* and
				  *  RTE_PMD_DPAA_OLDEV_TELECOM_* flags.
				  */
	uint8_t addr_type;	/**< RTE_PMD_DPAA_OLDEV_IPV4_ADDR_TYPE or
				  *  RTE_PMD_DPAA_OLDEV_IPV6_ADDR_TYPE.
				  */
	uint8_t pad[2];		/**< Reserved, must be zero. */
} __rte_packed_end;

/** A single local gateway (LGW) subnet entry. */
struct __rte_packed_begin rte_pmd_dpaa_lgw_subnet {
	uint32_t subnet[4];	/**< Subnet address, network byte order. */
	uint8_t mask;		/**< Subnet mask length in bits. */
	uint8_t subnet_type;	/**< RTE_PMD_DPAA_OLDEV_IPV4_SUBNET_TYPE or
				  *   RTE_PMD_DPAA_OLDEV_IPV6_SUBNET_TYPE.
				  */
	uint8_t pad[2];		/**< Reserved, must be zero. */
} __rte_packed_end;

/** Uplink classification information for the DPAA offline port. */
struct rte_pmd_dpaa_uplink_cls_info_s {
	struct rte_pmd_dpaa_ip_pair addr_pair;	/**< Static/inner IP pair. */
	/** GTP UDP ports the application listens on. */
	uint16_t gtp_udp_port[RTE_PMD_DPAA_OLDEV_MAX_NUM_PORTS];
	uint8_t gtp_proto_id;	/**< UDP protocol ID the application listens
				  *   on.
				  */
	uint8_t num_ports;	/**< Number of valid entries in
				  *   gtp_udp_port.
				  */
	uint8_t sec_enabled;	/**< Non-zero if IPsec is enabled. */
	uint8_t pad;		/**< Reserved, must be zero. */
};

/** Local gateway (LGW) information for the DPAA offline port. */
struct __rte_packed_begin rte_pmd_dpaa_lgw_info_s {
	/** LGW subnet table. */
	struct rte_pmd_dpaa_lgw_subnet subnets[RTE_PMD_DPAA_OLDEV_MAX_NUM_SUBNETS];
	uint8_t num_subnets;	/**< Number of valid entries in subnets. */
	uint8_t pad[3];		/**< Reserved, must be zero. */
} __rte_packed_end;

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Program the uplink classification information for the DPAA offline port.
 *
 * @param port_id
 *   The port identifier of the DPAA offline (O/H) port.
 * @param cls_info
 *   Pointer to the classification information to program. Must not be NULL.
 * @return
 *   0 on success, negative value otherwise.
 */
__rte_experimental
int rte_pmd_dpaa_ol_set_classif_info(uint16_t port_id,
		struct rte_pmd_dpaa_uplink_cls_info_s *cls_info);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Reset the uplink classification information for the DPAA offline port.
 *
 * @param port_id
 *   The port identifier of the DPAA offline (O/H) port.
 * @return
 *   0 on success, negative value otherwise.
 */
__rte_experimental
int rte_pmd_dpaa_ol_reset_classif_info(uint16_t port_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Program the local gateway (LGW) information for the DPAA offline port.
 *
 * @param port_id
 *   The port identifier of the DPAA offline (O/H) port.
 * @param lgw_info
 *   Pointer to the LGW information to program. Must not be NULL.
 * @return
 *   0 on success, negative value otherwise.
 */
__rte_experimental
int rte_pmd_dpaa_ol_set_lgw_info(uint16_t port_id,
		struct rte_pmd_dpaa_lgw_info_s *lgw_info);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Reset the local gateway (LGW) information for the DPAA offline port.
 *
 * @param port_id
 *   The port identifier of the DPAA offline (O/H) port.
 * @return
 *   0 on success, negative value otherwise.
 */
__rte_experimental
int rte_pmd_dpaa_ol_reset_lgw_info(uint16_t port_id);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PMD_DPAA_OLDEV_H */
