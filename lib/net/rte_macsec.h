/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#ifndef RTE_MACSEC_H
#define RTE_MACSEC_H

/**
 * @file
 *
 * MACsec-related defines
 */

#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_MACSEC_TCI_VER_MASK	0x80 /**< Version mask for MACsec. Should be 0. */
#define RTE_MACSEC_TCI_ES	0x40 /**< Mask for End station (ES) bit - SCI is not valid. */
#define RTE_MACSEC_TCI_SC	0x20 /**< Mask for SCI present bit. */
#define RTE_MACSEC_TCI_SCB	0x10 /**< Mask for EPON single copy broadcast bit. */
#define RTE_MACSEC_TCI_E	0x08 /**< Mask for encrypted user data bit. */
#define RTE_MACSEC_TCI_C	0x04 /**< Mask for changed user data bit (because of encryption). */
#define RTE_MACSEC_AN_MASK	0x03 /**< Association number mask in tci_an. */

/**
 * MACsec Header (SecTAG)
 */
__extension__
struct rte_macsec_hdr {
	/**
	 * Tag control information and Association number of secure channel.
	 * Various bits of TCI and AN are masked using RTE_MACSEC_TCI_* and RTE_MACSEC_AN_MASK.
	 */
	uint8_t tci_an;
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	uint8_t short_length:6; /**< Short Length. */
	uint8_t unused:2;
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	uint8_t unused:2;
	uint8_t short_length:6; /**< Short Length. */
#endif
	rte_be32_t packet_number; /**< Packet number to support replay protection. */
} __rte_packed;

/** SCI length in MACsec header if present. */
#define RTE_MACSEC_SCI_LEN 8

/**
 * MACsec SCI header (8 bytes) after the MACsec header
 * which is present if SC bit is set in tci_an.
 */
struct rte_macsec_sci_hdr {
	uint8_t sci[RTE_MACSEC_SCI_LEN]; /**< Optional secure channel ID. */
} __rte_packed;

#ifdef __cplusplus
}
#endif

#endif /* RTE_MACSEC_H */
