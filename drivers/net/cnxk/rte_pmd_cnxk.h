/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

/**
 * @file rte_pmd_cnxk.h
 * CNXK PMD specific functions.
 *
 **/

#ifndef _PMD_CNXK_H_
#define _PMD_CNXK_H_

#include <rte_compat.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_security.h>

/** Algorithm type to be used with security action to
 * calculate SA_index
 */
enum rte_pmd_cnxk_sec_action_alg {
	/** No swizzling of SPI bits into SA index.
	 * SA_index is from SA_XOR if enabled.
	 */
	RTE_PMD_CNXK_SEC_ACTION_ALG0,
	/** SPI<31:28> has 4 upper bits which segment the sequence number space.
	 * Initial SA_index is from SA_XOR if enabled.
	 * SA_alg = { 4'b0, SA_mcam[27:0] + SPI[31:28]}
	 */
	RTE_PMD_CNXK_SEC_ACTION_ALG1,
	/** SPI<27:25> segment the sequence number space.
	 *  Initial SA_index is from SA_XOR if enabled.
	 *  SA_alg = { 7'b0, SA_mcam[24:0] + SPI[27:25]}
	 */
	RTE_PMD_CNXK_SEC_ACTION_ALG2,
	/** SPI<28:25> segment the sequence number space.
	 * Initial SA_index is from SA_XOR if enabled.
	 * SA_alg = { 7'b0, SA_mcam[24:0] + SPI[28:25]}
	 */
	RTE_PMD_CNXK_SEC_ACTION_ALG3,
	/** The inbound SPI maybe "random", therefore we want the MCAM to be
	 * capable of remapping the SPI to an arbitrary SA_index.
	 * SPI to SA is done using a lookup in NIX/NPC cam entry with key as
	 * SPI, MATCH_ID, LFID.
	 */
	RTE_PMD_CNXK_SEC_ACTION_ALG4,
};

struct rte_pmd_cnxk_sec_action {
	/** Used as lookup result for ALG3 */
	uint32_t sa_index;
	/** When true XOR initial SA_INDEX with SA_HI/SA_LO to get SA_MCAM */
	bool sa_xor;
	/** SA_hi and SA_lo values for xor */
	uint16_t sa_hi, sa_lo;
	/** Determines alg to be applied post SA_MCAM computation with/without
	 * XOR.
	 */
	enum rte_pmd_cnxk_sec_action_alg alg;
};

/**
 * Read HW SA context from session.
 *
 * @param device
 *   Port identifier of Ethernet device.
 * @param sess
 *   Handle of the security session.
 * @param[out] data
 *   Destination pointer to copy SA context for application.
 * @param len
 *   Length of SA context to copy into data parameter.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
__rte_experimental
int rte_pmd_cnxk_hw_sa_read(void *device, struct rte_security_session *sess,
			    void *data, uint32_t len);
/**
 * Write HW SA context to session.
 *
 * @param device
 *   Port identifier of Ethernet device.
 * @param sess
 *   Handle of the security session.
 * @param[in] data
 *   Source data pointer from application to copy SA context into session.
 * @param len
 *   Length of SA context to copy from data parameter.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
__rte_experimental
int rte_pmd_cnxk_hw_sa_write(void *device, struct rte_security_session *sess,
			     void *data, uint32_t len);

/**
 * Get pointer to CPT result info for inline inbound processed pkt.
 *
 * It is recommended to use this API only when mbuf indicates packet
 * was processed with inline IPsec and there was a failure with the same i.e
 * mbuf->ol_flags indicates (RTE_MBUF_F_RX_SEC_OFFLOAD | RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED).
 *
 * @param mbuf
 *   Pointer to packet that was just received and was processed with Inline IPsec.
 *
 * @return
 *   - Pointer to mbuf location where CPT result info is stored on success.
 *   - NULL on failure.
 */
__rte_experimental
void *rte_pmd_cnxk_inl_ipsec_res(struct rte_mbuf *mbuf);
#endif /* _PMD_CNXK_H_ */
