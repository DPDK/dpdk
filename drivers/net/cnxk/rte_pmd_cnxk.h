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

/** CPT queue type for obtaining queue hardware statistics. */
enum rte_pmd_cnxk_cpt_q_stats_type {
	/** Type to get Inline Device queue(s) statistics */
	RTE_PMD_CNXK_CPT_Q_STATS_INL_DEV,
	/** Type to get Inline Inbound queue which is attached to kernel device
	 * statistics.
	 */
	RTE_PMD_CNXK_CPT_Q_STATS_KERNEL,
	/** Type to get CPT queue which is attached to ethdev statistics */
	RTE_PMD_CNXK_CPT_Q_STATS_ETHDEV,
};

/** CPT queue hardware statistics */
struct rte_pmd_cnxk_cpt_q_stats {
	/** Encrypted packet count */
	uint64_t enc_pkts;
	/** Encrypted byte count */
	uint64_t enc_bytes;
	/** Decrypted packet count */
	uint64_t dec_pkts;
	/** Decrypted byte count */
	uint64_t dec_bytes;
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
	bool is_non_inp;
	/** Inline profile ID (0-7) for custom profiles.
	 * Only used when use_custom_profile is true.
	 * Use profile_id returned by rte_pmd_cnxk_nix_inl_custom_profile_setup().
	 */
	uint16_t profile_id;
	/** When true, use custom inline profile specified by profile_id.
	 * When false, use default IPsec profile (backward compatible).
	 */
	bool use_custom_profile;
};

#define RTE_PMD_CNXK_CTX_MAX_CKEY_LEN	   32
#define RTE_PMD_CNXK_CTX_MAX_OPAD_IPAD_LEN 128

/** Anti reply window size supported */
#define RTE_PMD_CNXK_AR_WIN_SIZE_MIN	    64
#define RTE_PMD_CNXK_AR_WIN_SIZE_MAX	    4096
#define RTE_PMD_CNXK_LOG_MIN_AR_WIN_SIZE_M1 5

/** u64 array size to fit anti replay window bits */
#define RTE_PMD_CNXK_AR_WINBITS_SZ (RTE_ALIGN_CEIL(RTE_PMD_CNXK_AR_WIN_SIZE_MAX, 64) / 64)

/** Outer header info for Inbound or Outbound */
union rte_pmd_cnxk_ipsec_outer_ip_hdr {
	struct {
		/** IPv4 destination */
		uint32_t dst_addr;
		/** IPv4 source */
		uint32_t src_addr;
	} ipv4;
	struct {
		/** IPv6 source */
		uint8_t src_addr[16];
		/** IPv6 destination */
		uint8_t dst_addr[16];
	} ipv6;
};

/** Inbound IPsec context update region */
struct rte_pmd_cnxk_ipsec_inb_ctx_update_reg {
	/** Highest sequence number received */
	uint64_t ar_base;
	/** Valid bit for 64-bit words of replay window */
	uint64_t ar_valid_mask;
	/** Hard life for SA */
	uint64_t hard_life;
	/** Soft life for SA */
	uint64_t soft_life;
	/** MIB octets */
	uint64_t mib_octs;
	/** MIB packets */
	uint64_t mib_pkts;
	/** AR window bits */
	uint64_t ar_winbits[RTE_PMD_CNXK_AR_WINBITS_SZ];
};

/** Outbound IPsec IV data */
union rte_pmd_cnxk_ipsec_outb_iv {
	uint64_t u64[2];
	/** IV debug - 16B*/
	uint8_t iv_dbg[16];
	struct {
		/** IV debug - 8B */
		uint8_t iv_dbg1[4];
		/** Salt */
		uint8_t salt[4];

		uint32_t rsvd;
		/** IV debug - 8B */
		uint8_t iv_dbg2[4];
	} s;
};

/** Outbound IPsec context update region */
struct rte_pmd_cnxk_ipsec_outb_ctx_update_reg {
	union {
		struct {
			uint64_t reserved_0_2 : 3;
			uint64_t address : 57;
			uint64_t mode : 4;
		} s;
		uint64_t u64;
	} err_ctl;

	uint64_t esn_val;
	uint64_t hard_life;
	uint64_t soft_life;
	uint64_t mib_octs;
	uint64_t mib_pkts;
};

/**
 * Inbound IPsec SA
 */
struct rte_pmd_cnxk_ipsec_inb_sa {
	/** Word0 */
	union {
		struct {
			/** AR window size */
			uint64_t ar_win : 3;
			/** Hard life enable */
			uint64_t hard_life_dec : 1;
			/** Soft life enable */
			uint64_t soft_life_dec : 1;

			/** Count global octets */
			uint64_t count_glb_octets : 1;
			/** Count global pkts */
			uint64_t count_glb_pkts : 1;
			/** Count bytes */
			uint64_t count_mib_bytes : 1;

			/** Count pkts */
			uint64_t count_mib_pkts : 1;
			/** HW context offset */
			uint64_t hw_ctx_off : 7;

			/** Context ID */
			uint64_t ctx_id : 16;

			/** Original packet free absolute */
			uint64_t orig_pkt_fabs : 1;
			/** Original packet free */
			uint64_t orig_pkt_free : 1;
			/** PKIND for second pass */
			uint64_t pkind : 6;

			uint64_t rsvd0 : 1;
			/** Ether type overwrite */
			uint64_t et_ovrwr : 1;
			/** Packet output type */
			uint64_t pkt_output : 2;
			/** Packet format type */
			uint64_t pkt_format : 1;
			/** Defrag option */
			uint64_t defrag_opt : 2;
			/** Reserved for X2P dest */
			uint64_t x2p_dst : 1;

			/** Context push size */
			uint64_t ctx_push_size : 7;
			uint64_t rsvd1 : 1;

			/** Context header size */
			uint64_t ctx_hdr_size : 2;
			/** AOP enable */
			uint64_t aop_valid : 1;
			uint64_t rsvd2 : 1;
			/** Context size */
			uint64_t ctx_size : 4;
		} s;
		uint64_t u64;
	} w0;

	/** Word1 */
	union {
		struct {
			/** Original packet aura */
			uint64_t orig_pkt_aura : 20;
			uint64_t rsvd3 : 4;
			/** Original packet free offset */
			uint64_t orig_pkt_foff : 8;
			/** SA cookie */
			uint64_t cookie : 32;
		} s;
		uint64_t u64;
	} w1;

	/** Word 2 */
	union {
		struct {
			/** SA valid */
			uint64_t valid : 1;
			/** SA direction */
			uint64_t dir : 1;
			uint64_t rsvd11 : 1;
			uint64_t rsvd4 : 1;
			/** IPsec mode */
			uint64_t ipsec_mode : 1;
			/** IPsec protocol */
			uint64_t ipsec_protocol : 1;
			/** AES key length */
			uint64_t aes_key_len : 2;

			/** Encryption algo */
			uint64_t enc_type : 3;
			/** Soft life and hard life unit */
			uint64_t life_unit : 1;
			/** Authentication algo */
			uint64_t auth_type : 4;

			/** Encapsulation type */
			uint64_t encap_type : 2;
			/** Ether type override enable */
			uint64_t et_ovrwr_ddr_en : 1;
			/** ESN enable */
			uint64_t esn_en : 1;
			/** Transport mode L4 checksum incrementally update */
			uint64_t tport_l4_incr_csum : 1;
			/** Outer IP header verification */
			uint64_t ip_hdr_verify : 2;
			/** UDP enacapsulation ports verification */
			uint64_t udp_ports_verify : 1;

			/** Return 64B of L2/L3 header on error */
			uint64_t l3hdr_on_err : 1;
			uint64_t rsvd6 : 6;
			uint64_t rsvd12 : 1;

			/** SPI */
			uint64_t spi : 32;
		} s;
		uint64_t u64;
	} w2;

	/** Word3 */
	uint64_t rsvd7;

	/** Word4 - Word7 */
	uint8_t cipher_key[RTE_PMD_CNXK_CTX_MAX_CKEY_LEN];

	/** Word8 - Word9 */
	union {
		struct {
			uint32_t rsvd8;
			/** IV salt */
			uint8_t salt[4];
		} s;
		uint64_t u64;
	} w8;
	uint64_t rsvd9;

	/** Word10 */
	union {
		struct {
			uint64_t rsvd10 : 32;
			/** UDP encapsulation source port */
			uint64_t udp_src_port : 16;
			/** UDP encapsulation destination port */
			uint64_t udp_dst_port : 16;
		} s;
		uint64_t u64;
	} w10;

	/** Word11 - Word14 */
	union rte_pmd_cnxk_ipsec_outer_ip_hdr outer_hdr;

	/** Word15 - Word30 */
	uint8_t hmac_opad_ipad[RTE_PMD_CNXK_CTX_MAX_OPAD_IPAD_LEN];

	/** Word31 - Word100 */
	struct rte_pmd_cnxk_ipsec_inb_ctx_update_reg ctx;
};

/**
 * Outbound IPsec SA
 */
struct rte_pmd_cnxk_ipsec_outb_sa {
	/** Word0 */
	union {
		struct {
			/** ESN enable */
			uint64_t esn_en : 1;
			/** IP ID generation type */
			uint64_t ip_id : 1;
			uint64_t rsvd0 : 1;
			/** Hard life enable */
			uint64_t hard_life_dec : 1;
			/** Soft life enable */
			uint64_t soft_life_dec : 1;

			/** Count global octets */
			uint64_t count_glb_octets : 1;
			/** Count global pkts */
			uint64_t count_glb_pkts : 1;
			/** Count bytes */
			uint64_t count_mib_bytes : 1;

			/** Count pkts */
			uint64_t count_mib_pkts : 1;
			/** HW context offset */
			uint64_t hw_ctx_off : 7;

			/** Context ID */
			uint64_t ctx_id : 16;
			uint64_t rsvd1 : 16;

			/** Context push size */
			uint64_t ctx_push_size : 7;
			uint64_t rsvd2 : 1;

			/** Context header size */
			uint64_t ctx_hdr_size : 2;
			/** AOP enable */
			uint64_t aop_valid : 1;
			uint64_t rsvd3 : 1;
			/** Context size */
			uint64_t ctx_size : 4;
		} s;
		uint64_t u64;
	} w0;

	/** Word1 */
	union {
		struct {
			uint64_t rsvd4 : 32;
			/** SA cookie */
			uint64_t cookie : 32;
		} s;
		uint64_t u64;
	} w1;

	/** Word 2 */
	union {
		struct {
			/** SA valid */
			uint64_t valid : 1;
			/** SA direction */
			uint64_t dir : 1;
			uint64_t rsvd11 : 1;
			uint64_t rsvd5 : 1;
			/** IPsec mode */
			uint64_t ipsec_mode : 1;
			/** IPsec protocol */
			uint64_t ipsec_protocol : 1;

			/** AES key length */
			uint64_t aes_key_len : 2;

			/** Encryption algo */
			uint64_t enc_type : 3;
			/** Soft life and hard life unit */
			uint64_t life_unit : 1;
			/** Authentication algo */
			uint64_t auth_type : 4;

			/** Encapsulation type */
			uint64_t encap_type : 2;
			/** DF source */
			uint64_t ipv4_df_src_or_ipv6_flw_lbl_src : 1;
			/** DSCP source */
			uint64_t dscp_src : 1;
			/** IV source */
			uint64_t iv_src : 2;
			/** IPID value in outer header */
			uint64_t ipid_gen : 1;
			uint64_t rsvd6 : 1;

			uint64_t rsvd7 : 7;
			uint64_t rsvd12 : 1;

			/** SPI */
			uint64_t spi : 32;
		} s;
		uint64_t u64;
	} w2;

	/** Word3 */
	uint64_t rsvd8;

	/** Word4 - Word7 */
	uint8_t cipher_key[RTE_PMD_CNXK_CTX_MAX_CKEY_LEN];

	/** Word8 - Word9 */
	union rte_pmd_cnxk_ipsec_outb_iv iv;

	/** Word10 */
	union {
		struct {
			uint64_t rsvd9 : 4;
			/** Outer header IPv4 DF or IPv6 flow label */
			uint64_t ipv4_df_or_ipv6_flw_lbl : 20;

			/** DSCP for outer header */
			uint64_t dscp : 6;
			uint64_t rsvd10 : 2;

			/** UDP encapsulation destination port */
			uint64_t udp_dst_port : 16;

			/** UDP encapsulation source port */
			uint64_t udp_src_port : 16;
		} s;
		uint64_t u64;
	} w10;

	/** Word11 - Word14 */
	union rte_pmd_cnxk_ipsec_outer_ip_hdr outer_hdr;

	/** Word15 - Word30 */
	uint8_t hmac_opad_ipad[RTE_PMD_CNXK_CTX_MAX_OPAD_IPAD_LEN];

	/** Word31 - Word36 */
	struct rte_pmd_cnxk_ipsec_outb_ctx_update_reg ctx;
};

/** Inbound/Outbound IPsec SA */
union rte_pmd_cnxk_ipsec_hw_sa {
	/** Inbound SA */
	struct rte_pmd_cnxk_ipsec_inb_sa inb;
	/** Outbound SA */
	struct rte_pmd_cnxk_ipsec_outb_sa outb;
};

/** CPT HW result format */
union rte_pmd_cnxk_cpt_res_s {
	/** CN10K CPT result */
	struct rte_pmd_cpt_cn10k_res_s {
		/** Completion code */
		uint64_t compcode : 7;
		/** Done interrupt */
		uint64_t doneint : 1;
		/** Microcode completion code */
		uint64_t uc_compcode : 8;
		/** Result length */
		uint64_t rlen : 16;
		/** SPI */
		uint64_t spi : 32;

		/** Extended sequence number */
		uint64_t esn;
	} cn10k;

	/** CN9K CPT result */
	struct rte_pmd_cpt_cn9k_res_s {
		/** Completion code */
		uint64_t compcode : 8;
		/** Microcode completion code */
		uint64_t uc_compcode : 8;
		/** Done interrupt */
		uint64_t doneint : 1;
		uint64_t reserved_17_63 : 47;

		uint64_t reserved_64_127;
	} cn9k;

	/** CPT RES */
	uint64_t u64[2];
};

/** Inline IPsec inbound queue configuration */
struct rte_pmd_cnxk_ipsec_inb_cfg {
	/** Param1 of PROCESS_INBOUND_IPSEC_PACKET as mentioned in the CPT
	 * microcode document.
	 */
	uint16_t param1;
	/** Param2 of PROCESS_INBOUND_IPSEC_PACKET as mentioned in the CPT
	 * microcode document.
	 */
	uint16_t param2;
};

/** Forward structure declaration for inline device queue. Applications obtain a pointer
 * to this structure using the ``rte_pmd_cnxk_inl_dev_qptr_get`` API and use it to submit
 * CPT instructions (cpt_inst_s) to the inline device via the
 * ``rte_pmd_cnxk_inl_dev_submit`` API.
 */
struct rte_pmd_cnxk_inl_dev_q;

/**
 * Read HW SA context from session.
 *
 * @param portid
 *   Port identifier of Ethernet device.
 * @param sess
 *   Handle of the security session as void *.
 * @param[out] data
 *   Destination pointer to copy SA context for application.
 * @param len
 *   Length of SA context to copy into data parameter.
 * @param inb
 *   Determines the type of specified SA.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
__rte_experimental
int rte_pmd_cnxk_hw_sa_read(uint16_t portid, void *sess, union rte_pmd_cnxk_ipsec_hw_sa *data,
			    uint32_t len, bool inb);
/**
 * Write HW SA context to session.
 *
 * @param portid
 *   Port identifier of Ethernet device.
 * @param sess
 *   Handle of the security session as void *.
 * @param[in] data
 *   Source data pointer from application to copy SA context into session.
 * @param len
 *   Length of SA context to copy from data parameter.
 * @param inb
 *   Determines the type of specified SA.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
__rte_experimental
int rte_pmd_cnxk_hw_sa_write(uint16_t portid, void *sess, union rte_pmd_cnxk_ipsec_hw_sa *data,
			     uint32_t len, bool inb);

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
 *   - Pointer to mbuf location where `union rte_pmd_cnxk_cpt_res_s` is stored on success.
 *   - NULL on failure.
 */
__rte_experimental
union rte_pmd_cnxk_cpt_res_s *rte_pmd_cnxk_inl_ipsec_res(struct rte_mbuf *mbuf);

/**
 * Get pointer to the Inline Inbound or Outbound SA table base.
 *
 * @param portid
 *   Port identifier of Ethernet device.
 * @param inb
 *   Determines the type of SA base to be returned.
 *   When inb is true, the method returns the Inbound SA base.
 *   When inb is false, the method returns the Outbound SA base.
 *
 * @return
 *   Pointer to Inbound or Outbound SA base.
 */
__rte_experimental
union rte_pmd_cnxk_ipsec_hw_sa *rte_pmd_cnxk_hw_session_base_get(uint16_t portid, bool inb);

/**
 * Get pointer to the Inline Inbound SA table base for a specific custom profile.
 *
 * @param portid
 *   Port identifier of Ethernet device.
 * @param profile_id
 *   Custom profile ID to get the SA base for.
 *
 * @return
 *   Pointer to Inbound SA base for the specified profile, or NULL on error.
 */
__rte_experimental
union rte_pmd_cnxk_ipsec_hw_sa *rte_pmd_cnxk_inl_inb_prof_sa_base_get(uint16_t portid,
								       uint16_t profile_id);

/**
 * Executes a CPT flush on the specified session.
 *
 * @param portid
 *   Port identifier of Ethernet device.
 * @param sess
 *   Handle of the session on which the CPT flush will be executed.
 * @param inb
 *   Determines the type of SA to be flushed, Inbound or Outbound.
 *
 * @return
 *   0 Upon success, a negative errno value otherwise.
 */
__rte_experimental
int rte_pmd_cnxk_sa_flush(uint16_t portid, union rte_pmd_cnxk_ipsec_hw_sa *sess, bool inb);

/**
 * Get queue pointer of Inline Device.
 *
 * @return
 *   - Pointer to queue structure that would be the input to submit API.
 *   - NULL upon failure.
 */
__rte_experimental
struct rte_pmd_cnxk_inl_dev_q *rte_pmd_cnxk_inl_dev_qptr_get(void);

/**
 * Submit CPT instruction(s) (cpt_inst_s) to Inline Device.
 *
 * @param qptr
 *   Pointer obtained with ``rte_pmd_cnxk_inl_dev_qptr_get``.
 * @param inst
 *   Pointer to an array of ``cpt_inst_s`` prapared by application.
 * @param nb_inst
 *   Number of instructions to be processed.
 *
 * @return
 *   Number of instructions processed.
 */
__rte_experimental
uint16_t rte_pmd_cnxk_inl_dev_submit(struct rte_pmd_cnxk_inl_dev_q *qptr, void *inst,
				     uint16_t nb_inst);

/**
 * Retrieves the hardware statistics of a given port and stats type.
 *
 * @param portid
 *   Port identifier of Ethernet device.
 * @param type
 *   The type of hardware statistics to retrieve, as defined in the
 *   ``enum rte_pmd_cnxk_cpt_q_stats_type``.
 * @param stats
 *   Pointer where the retrieved statistics will be stored.
 * @param idx
 *   The index of the queue of a given type.
 *
 * @return
 *   0 Upon success, a negative errno value otherwise.
 */
__rte_experimental
int rte_pmd_cnxk_cpt_q_stats_get(uint16_t portid, enum rte_pmd_cnxk_cpt_q_stats_type type,
				 struct rte_pmd_cnxk_cpt_q_stats *stats, uint16_t idx);

/**
 * Set the configuration for hardware inline inbound IPsec processing. This API must be
 * called before calling the ``rte_eth_dev_configure`` API.
 *
 * @param portid
 *   Port identifier of Ethernet device.
 * @param cfg
 *   Pointer to the IPsec inbound configuration structure.
 */
__rte_experimental
void rte_pmd_cnxk_hw_inline_inb_cfg_set(uint16_t portid, struct rte_pmd_cnxk_ipsec_inb_cfg *cfg);

/**
 * RX Default Inline Configuration structure
 * This structure represents the NIX_AF_RX_DEF_INLINE register fields
 * used for configuring inline IPsec processing.
 */
struct rte_pmd_cnxk_rx_def_inl_cfg {
	/** Layer type mask (bits 3:0) */
	uint64_t ltype_mask;
	/** Layer type match value (bits 7:4) */
	uint64_t ltype_match;
	/** Layer ID (bits 10:8) - Enumerated by NPC_LID_E */
	uint64_t lid;
	/** Generic offset negative (bit 11) */
	uint64_t gen_offset_ng;
	/** Generic offset in bytes (bits 15:12) */
	uint64_t gen_offset;
	/** Generic field non-zero (bit 16) */
	uint64_t gen_nz;
	/** CPT L3 Layer ID (bits 19:17) */
	uint64_t cpt_l3_lid;
	/** Layer type mask (bits 23:20) */
	uint64_t flags_mask;
	/** Layer type match value (bits 27:24) */
	uint64_t flags_match;
	/** Match on IPv4 (bit 28) */
	uint64_t match_oipv4;
	/** Match on IPv6 (bit 29) */
	uint64_t match_oipv6;
	/** OIP Length Enable (bit 30) */
	uint64_t oiplen_ena;
	/** Inline shift (bits 33:32) - Valid values: 0, 1, 2 */
	uint64_t inline_shift;
};

/**
 * RX Generic Inline Configuration structure
 * This structure represents the NIX_AF_RX_INLINE_GEN_CFG(0..7) register fields.
 */
struct rte_pmd_cnxk_rx_gen_inl_cfg {
	/** Layer type mask (bits 3:0) */
	uint64_t ltype_mask;
	/** Layer type match value (bits 7:4) */
	uint64_t ltype_match;
	/** Layer ID (bits 10:8) */
	uint64_t lid;
	/** Nibble Offset (bit 11) - 0: IPv4, 1: IPv6 */
	uint64_t noffset;
	/** Offset (bits 17:12) - Offset to DSCP field */
	uint64_t offset;
	/** PARAM2 (bits 15:0) - CPT_INST_S[PARAM2] */
	uint64_t param2;
	/** PARAM1 (bits 31:16) - CPT_INST_S[PARAM1] */
	uint64_t param1;
	/** OPCODE (bits 47:32) - CPT_INST_S[OPCODE] */
	uint64_t opcode;
	/** Engine Group (bits 50:48) - CPT_INST_S[EGRP] */
	uint64_t egrp;
	/** Context Valid (bit 51) - CPT_INST_S[CTX_VAL] */
	uint64_t ctx_val;
};

/**
 * RX Extract Inline Configuration structure
 * This structure represents the NIX_AF_RX_EXTRACT_INLINE(0..7) register fields.
 */
struct rte_pmd_cnxk_rx_extract_inl_cfg {
	/** LEN_L (bits 5:0) - Bit length from BITPOS_L (0-32) */
	uint64_t len_l;
	/** BITPOS_L (bits 13:8) - Bit pos to start extraction */
	uint64_t bitpos_l;
	/** LEN_M (bits 21:16) - Bit length from BITPOS_M (0-32) */
	uint64_t len_m;
	/** BITPOS_M (bits 29:24) - Bit pos to start extraction */
	uint64_t bitpos_m;
	/** LEN_H (bits 37:32) - Bit length from BITPOS_H (0-32) */
	uint64_t len_h;
	/** BITPOS_H (bits 45:40) - Bit pos to start extraction */
	uint64_t bitpos_h;
};

/**
 * Protocol field types for inline IPsec configuration
 */
enum rte_pmd_cnxk_nix_rx_prot_e {
	RTE_PMD_CNXK_RX_PROT_OPCODE = 0,     /**< Opcode (max 2 bytes) */
	RTE_PMD_CNXK_RX_PROT_PARAM1 = 1,     /**< Parameter 1 (max 2 bytes) */
	RTE_PMD_CNXK_RX_PROT_PARAM2 = 2,     /**< Parameter 2 (max 2 bytes) */
	RTE_PMD_CNXK_RX_PROT_SA_INDEX = 3,   /**< SA index (max 4 bytes) */
	RTE_PMD_CNXK_RX_PROT_RPTR_H = 4,     /**< RPTR high bits (max 4 bytes) */
	RTE_PMD_CNXK_RX_PROT_IP_OFFSET = 5,  /**< IP offset (max 1 byte) */
	RTE_PMD_CNXK_RX_PROT_INL_OFFSET = 6, /**< Inline offset (max 4 bytes) */
	RTE_PMD_CNXK_RX_PROT_ALG = 7,	     /**< ALG override (max 1 nibble) */
	RTE_PMD_CNXK_RX_PROT_SUB_INDEX = 8,  /**< Sub index (max 1 nibble) */
};

/** Maximum number of inline profiles supported */
#define RTE_PMD_CNXK_MAX_PROT_FIELDS 9

/**
 * RX Protocol Field Inline Configuration structure
 * This structure represents the NIX_AF_RX_PROT_FIELD(0..8)_INLINE(0..7) registers.
 */
struct rte_pmd_cnxk_rx_prot_field_inl_cfg {
	/** LOGMULT (bits 13:12) - Multiply: 0=1, 1=2, 2=4, 3=8 */
	uint64_t logmult;
	/** SIZEM1 (bits 11:8) - Size of field in nibbles */
	uint64_t sizem1;
	/** OFFSET (bits 7:2) - Offset from header start in nibbles */
	uint64_t offset;
	/** VALID (bit 0) - Valid flag */
	uint64_t valid;
};

/**
 * Combined profile configuration parameters structure
 * This structure combines all configuration parameters needed for
 * setting up a custom inline IPsec profile.
 */
struct rte_pmd_cnxk_profile_cfg_params {
	/** Default inline configuration (NIX_AF_RX_DEF_INLINE) */
	struct rte_pmd_cnxk_rx_def_inl_cfg def_cfg;

	/** Generic inline configuration (NIX_AF_RX_INLINE_GEN_CFG) */
	struct rte_pmd_cnxk_rx_gen_inl_cfg gen_cfg;

	/** Extract inline configuration (NIX_AF_RX_EXTRACT_INLINE) */
	struct rte_pmd_cnxk_rx_extract_inl_cfg extract_cfg;

	/** Protocol field inline configuration
	 * Array of configs for NIX_AF_RX_PROT_FIELD(prot_field)_INLINE(0..7)
	 */
	struct rte_pmd_cnxk_rx_prot_field_inl_cfg prot_field_cfg[RTE_PMD_CNXK_MAX_PROT_FIELDS];

	/** SA size in bytes.
	 * Size of each SA entry for this profile.
	 * Must be a power of 2 and >= 128 bytes.
	 * If 0, default SA size is used.
	 */
	uint32_t sa_size;

	/** Maximum number of SAs for this profile.
	 * Number of SA entries to allocate for this profile.
	 * If 0, default max_sa is used.
	 */
	uint32_t max_sa;
};

/**
 * Setup custom inline IPsec profile
 *
 * This function configures a custom inline IPsec profile for the specified port.
 * The profile includes default inline configuration, generic inline configuration,
 * extract inline configuration, protocol field inline configuration, and
 * optional custom SA size and max SA count.
 *
 * @param portid
 *   Port identifier of the Ethernet device.
 * @param cfg
 *   Pointer to the combined profile configuration structure.
 * @param[out] profile_id
 *   Pointer to store the returned profile ID (0-7).
 *   This ID can be used in rte_pmd_cnxk_sec_action.profile_id.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 *   - -EINVAL: Invalid parameters
 *   - -ENOTSUP: Function not supported on this device
 *   - -ENOSPC: No free profile available
 *   - -ENOMEM: Memory allocation failed for SA table
 */
__rte_experimental
int rte_pmd_cnxk_nix_inl_custom_profile_setup(uint16_t portid,
		const struct rte_pmd_cnxk_profile_cfg_params *cfg, uint16_t *profile_id);

/**
 * Release a custom inline profile.
 *
 * This function releases resources associated with a custom inline profile
 * that was previously created using rte_pmd_cnxk_nix_inl_custom_profile_setup().
 *
 * @param portid
 *   Port identifier of the Ethernet device.
 * @param profile_id
 *   Custom profile ID to release.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
__rte_experimental
int rte_pmd_cnxk_nix_inl_custom_profile_release(uint16_t portid,
						uint16_t profile_id);

/**
 * Retrieves model name on which it is running as a string.
 *
 * @return
 *   Returns model string, ex."cn10ka_a1"
 */
__rte_experimental
const char *rte_pmd_cnxk_model_str_get(void);
#endif /* _PMD_CNXK_H_ */
