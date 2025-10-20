/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#include "flow_hsh_cfg.h"

#define RTE_ETH_RSS_UDP_COMBINED (RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
								  RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
								  RTE_ETH_RSS_IPV6_UDP_EX)

#define RTE_ETH_RSS_TCP_COMBINED (RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
								  RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
								  RTE_ETH_RSS_IPV6_TCP_EX)

#define TOEPLITS_HSH_SIZE 9
/*
 * FPGA uses up to 10 32-bit words (320 bits) for hash calculation + 8 bits for L4 protocol number.
 * Hashed data are split between two 128-bit Quad Words (QW)
 * and two 32-bit Words (W), which can refer to different header parts.
 */
enum hsh_words_id {
	HSH_WORDS_QW0     = 0,
	HSH_WORDS_QW4,
	HSH_WORDS_W8,
	HSH_WORDS_W9,
	HSH_WORDS_SIZE,
};

/* struct with details about hash QWs & Ws */
struct hsh_words {
	/*
	 * index of W (word) or index of 1st word of QW (quad word)
	 * is used for hash mask calculation
	 */
	uint8_t index;
	enum hw_hsh_e pe;           /* offset to header part, e.g. beginning of L4 */
	enum hw_hsh_e ofs;          /* relative offset in BYTES to 'pe' header offset above */
	uint16_t bit_len;           /* max length of header part in bits to fit into QW/W */
	bool free;                  /* only free words can be used for hsh calculation */
};

static enum hsh_words_id get_free_word(struct hsh_words *words, uint16_t bit_len)
{
	enum hsh_words_id ret = HSH_WORDS_SIZE;
	uint16_t ret_bit_len = UINT16_MAX;
	for (enum hsh_words_id i = HSH_WORDS_QW0; i < HSH_WORDS_SIZE; i++) {
		if (words[i].free && bit_len <=
			words[i].bit_len && words[i].bit_len <
			ret_bit_len) {
			ret = i;
			ret_bit_len = words[i].bit_len;
		}
	}
	return ret;
}

static int hsh_set_part(struct flow_nic_dev *ndev, int hsh_idx, struct hsh_words *words,
	uint32_t pe, uint32_t ofs, int bit_len, bool toeplitz)
{
	int res = 0;

	/* check if there is any free word, which can accommodate header part of given 'bit_len' */
	enum hsh_words_id word = get_free_word(words, bit_len);

	if (word == HSH_WORDS_SIZE) {
		NT_LOG(ERR, FILTER, "Cannot add additional %d bits into hash", bit_len);
		return -1;
	}

	words[word].free = false;

	res |= nthw_mod_hsh_rcp_set(&ndev->be, words[word].pe, hsh_idx, 0, pe);
	NT_LOG(DBG, FILTER, "nthw_mod_hsh_rcp_set(&ndev->be, %d, %d, 0, %" PRIu32 ")",
		(int)words[word].pe, hsh_idx, pe);
	res |= nthw_mod_hsh_rcp_set(&ndev->be, words[word].ofs, hsh_idx, 0, ofs);
	NT_LOG(DBG, FILTER, "nthw_mod_hsh_rcp_set(&ndev->be, %d, %d, 0, %" PRIu32 ")",
		(int)words[word].ofs, hsh_idx, ofs);


	/* set HW_HSH_RCP_WORD_MASK based on used QW/W and given 'bit_len' */
	int mask_bit_len = bit_len;
	uint32_t mask = 0x0;
	uint32_t toeplitz_mask[TOEPLITS_HSH_SIZE] = {0x0};
	/* iterate through all words of QW */
	uint16_t words_count = words[word].bit_len / 32;
	for (uint16_t mask_off = 1; mask_off <= words_count; mask_off++) {
		if (mask_bit_len >= 32) {
			mask_bit_len -= 32;
			mask = 0xffffffff;
		} else if (mask_bit_len > 0) {
			mask = 0xffffffff >> (32 - mask_bit_len) << (32 - mask_bit_len);
			mask_bit_len = 0;
		} else {
			mask = 0x0;
		}
		/* reorder QW words mask from little to big endian */
		res |= nthw_mod_hsh_rcp_set(&ndev->be, HW_HSH_RCP_WORD_MASK, hsh_idx,
			words[word].index + words_count - mask_off, mask);
		NT_LOG(DBG, FILTER,
			"mod_hsh_rcp_set(&ndev->be, HW_HSH_RCP_WORD_MASK, %d, %d, 0x%08" PRIX32 ")",
				hsh_idx, words[word].index + words_count - mask_off, mask);
		toeplitz_mask[words[word].index + mask_off - 1] = mask;
	}
	if (toeplitz) {
		NT_LOG(DBG, FILTER,
			"Partial Toeplitz RSS key mask: %08" PRIX32 " %08" PRIX32 " %08" PRIX32
			" %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32 " %08" PRIX32
			" %08" PRIX32 "",
			toeplitz_mask[0], toeplitz_mask[1], toeplitz_mask[2], toeplitz_mask[3],
			toeplitz_mask[4], toeplitz_mask[5], toeplitz_mask[6], toeplitz_mask[7],
			toeplitz_mask[8]);
		NT_LOG(DBG, FILTER, "                               MSB                                                                          LSB");
	}
	return res;
}

static __rte_always_inline bool all_bits_enabled(uint64_t hash_mask, uint64_t hash_bits)
{
	return (hash_mask & hash_bits) == hash_bits;
}

static __rte_always_inline void unset_bits(uint64_t *hash_mask, uint64_t hash_bits)
{
	*hash_mask &= ~hash_bits;
}

static __rte_always_inline void unset_bits_and_log(uint64_t *hash_mask, uint64_t hash_bits)
{
	char rss_buffer[4096];
	uint16_t rss_buffer_len = sizeof(rss_buffer);

	if (sprint_nt_rss_mask(rss_buffer, rss_buffer_len, " ", *hash_mask & hash_bits) == 0)
		NT_LOG(DBG, FILTER, "Configured RSS types:%s", rss_buffer);
	unset_bits(hash_mask, hash_bits);
}

static __rte_always_inline void unset_bits_if_all_enabled(uint64_t *hash_mask, uint64_t hash_bits)
{
	if (all_bits_enabled(*hash_mask, hash_bits))
		unset_bits(hash_mask, hash_bits);
}

int hsh_set(struct flow_nic_dev *ndev, int hsh_idx, struct nt_eth_rss_conf rss_conf)
{
	uint64_t fields = rss_conf.rss_hf;

	char rss_buffer[4096];
	uint16_t rss_buffer_len = sizeof(rss_buffer);

	if (sprint_nt_rss_mask(rss_buffer, rss_buffer_len, " ", fields) == 0)
		NT_LOG(DBG, FILTER, "Requested RSS types:%s", rss_buffer);

	/*
	 * configure all (Q)Words usable for hash calculation
	 * Hash can be calculated from 4 independent header parts:
	 *      | QW0           | Qw4           | W8| W9|
	 * word | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
	 */
	struct hsh_words words[HSH_WORDS_SIZE] = {
		{ 0, HW_HSH_RCP_QW0_PE, HW_HSH_RCP_QW0_OFS, 128, true },
		{ 4, HW_HSH_RCP_QW4_PE, HW_HSH_RCP_QW4_OFS, 128, true },
		{ 8, HW_HSH_RCP_W8_PE,  HW_HSH_RCP_W8_OFS,   32, true },
		 /* not supported for Toeplitz */
		{ 9, HW_HSH_RCP_W9_PE,  HW_HSH_RCP_W9_OFS,   32, true },
	};

	int res = 0;
	res |= nthw_mod_hsh_rcp_set(&ndev->be, HW_HSH_RCP_PRESET_ALL, hsh_idx, 0, 0);
	/* enable hashing */
	res |= nthw_mod_hsh_rcp_set(&ndev->be, HW_HSH_RCP_LOAD_DIST_TYPE, hsh_idx, 0, 2);

	/* configure selected hash function and its key */
	bool toeplitz = false;
	switch (rss_conf.algorithm) {
	case RTE_ETH_HASH_FUNCTION_DEFAULT:
		/* Use default NTH10 hashing algorithm */
		res |= nthw_mod_hsh_rcp_set(&ndev->be, HW_HSH_RCP_TOEPLITZ, hsh_idx, 0, 0);
		/* Use 1st 32-bits from rss_key to configure NTH10 SEED */
		res |= nthw_mod_hsh_rcp_set(&ndev->be, HW_HSH_RCP_SEED, hsh_idx, 0,
			rss_conf.rss_key[0] << 24 | rss_conf.rss_key[1] << 16 |
				rss_conf.rss_key[2] << 8 | rss_conf.rss_key[3]);
		break;
	case RTE_ETH_HASH_FUNCTION_TOEPLITZ:
		toeplitz = true;
		res |= nthw_mod_hsh_rcp_set(&ndev->be, HW_HSH_RCP_TOEPLITZ, hsh_idx, 0, 1);
		uint8_t empty_key = 0;

		/* Toeplitz key (always 40B) words have to be programmed in reverse order */
		for (uint8_t i = 0; i <= (MAX_RSS_KEY_LEN - 4); i += 4) {
			res |= nthw_mod_hsh_rcp_set(&ndev->be, HW_HSH_RCP_K, hsh_idx, 9 - i / 4,
				rss_conf.rss_key[i] << 24 | rss_conf.rss_key[i + 1] << 16 |
				rss_conf.rss_key[i + 2] << 8 | rss_conf.rss_key[i + 3]);
			NT_LOG(DBG, FILTER,
				"nthw_mod_hsh_rcp_set(&ndev->be, HW_HSH_RCP_K, %d, %d, 0x%" PRIX32 ")",
				hsh_idx, 9 - i / 4, rss_conf.rss_key[i] << 24 |
				rss_conf.rss_key[i + 1] << 16 | rss_conf.rss_key[i + 2] << 8 |
				rss_conf.rss_key[i + 3]);
			empty_key |= rss_conf.rss_key[i] | rss_conf.rss_key[i + 1] |
				rss_conf.rss_key[i + 2] | rss_conf.rss_key[i + 3];
		}

		if (empty_key == 0) {
			NT_LOG(ERR, FILTER, "Toeplitz key must be configured. Key with all bytes set to zero is not allowed.");
			return -1;
		}
		words[HSH_WORDS_W9].free = false;
		NT_LOG(DBG, FILTER, "Toeplitz hashing is enabled thus W9 and P_MASK cannot be used.");
		break;
	default:
		NT_LOG(ERR, FILTER, "Unknown hashing function %d requested", rss_conf.algorithm);
		return -1;
	}

	/* indication that some IPv6 flag is present */
	bool ipv6 = fields & (NT_ETH_RSS_IPV6_MASK);
	/* store proto mask for later use at IP and L4 checksum handling */
	uint64_t l4_proto_mask = fields &
				(RTE_ETH_RSS_NONFRAG_IPV4_TCP | RTE_ETH_RSS_NONFRAG_IPV4_UDP |
				RTE_ETH_RSS_NONFRAG_IPV4_SCTP | RTE_ETH_RSS_NONFRAG_IPV4_OTHER |
				RTE_ETH_RSS_NONFRAG_IPV6_TCP | RTE_ETH_RSS_NONFRAG_IPV6_UDP |
				RTE_ETH_RSS_NONFRAG_IPV6_SCTP | RTE_ETH_RSS_NONFRAG_IPV6_OTHER |
				RTE_ETH_RSS_IPV6_TCP_EX | RTE_ETH_RSS_IPV6_UDP_EX);

	/* outermost headers are used by default, so innermost bit takes precedence if detected */
	bool outer = (fields & RTE_ETH_RSS_LEVEL_INNERMOST) ? false : true;
	unset_bits(&fields, RTE_ETH_RSS_LEVEL_MASK);

	if (fields == 0) {
		NT_LOG(ERR, FILTER, "RSS hash configuration 0x%" PRIX64 " is not valid.",
			rss_conf.rss_hf);
		return -1;
	}

	/* indication that IPv4 `protocol` or IPv6 `next header` fields shall be part of the hash */
	bool l4_proto_hash = false;

	/*
	 * check if SRC_ONLY & DST_ONLY are used simultaneously;
	 * According to DPDK, we shall behave like none of these bits is set
	 */
	unset_bits_if_all_enabled(&fields, RTE_ETH_RSS_L2_SRC_ONLY | RTE_ETH_RSS_L2_DST_ONLY);
	unset_bits_if_all_enabled(&fields, RTE_ETH_RSS_L3_SRC_ONLY | RTE_ETH_RSS_L3_DST_ONLY);
	unset_bits_if_all_enabled(&fields, RTE_ETH_RSS_L4_SRC_ONLY | RTE_ETH_RSS_L4_DST_ONLY);

	/* L2 */
	if (fields & (RTE_ETH_RSS_ETH | RTE_ETH_RSS_L2_SRC_ONLY | RTE_ETH_RSS_L2_DST_ONLY)) {
		if (outer) {
			if (fields & RTE_ETH_RSS_L2_SRC_ONLY) {
				NT_LOG(DBG, FILTER, "Set outer src MAC hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_L2,
					6, 48, toeplitz);
			} else if (fields & RTE_ETH_RSS_L2_DST_ONLY) {
				NT_LOG(DBG, FILTER, "Set outer dst MAC hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_L2,
					0, 48, toeplitz);
			} else {
				NT_LOG(DBG, FILTER, "Set outer src & dst MAC hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_L2,
					0, 96, toeplitz);
			}
		} else {
			if (fields & RTE_ETH_RSS_L2_SRC_ONLY) {
				NT_LOG(DBG, FILTER, "Set inner src MAC hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_L2,
					6, 48, toeplitz);
			} else if (fields & RTE_ETH_RSS_L2_DST_ONLY) {
				NT_LOG(DBG, FILTER, "Set inner dst MAC hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_L2,
					0, 48, toeplitz);
			} else {
				NT_LOG(DBG, FILTER, "Set inner src & dst MAC hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_L2,
					0, 96, toeplitz);
			}
		}
		unset_bits_and_log(&fields, RTE_ETH_RSS_ETH | RTE_ETH_RSS_L2_SRC_ONLY |
			RTE_ETH_RSS_L2_DST_ONLY);
	}

	/*
	 * VLAN support of multiple VLAN headers,
	 * where S-VLAN is the first and C-VLAN the last VLAN header
	 */
	if (fields & RTE_ETH_RSS_C_VLAN) {
		/*
		 * use MPLS protocol offset, which points
		 * just after ethertype with relative
		 * offset -6 (i.e. 2 bytes
		 * of ethertype & size + 4 bytes of VLAN header field)
		 * to access last vlan header
		 */
		if (outer) {
			NT_LOG(DBG, FILTER, "Set outer C-VLAN hasher.");
			/*
			 * use whole 32-bit 802.1a tag - backward compatible
			 * with VSWITCH implementation
			 */
			res |= hsh_set_part(ndev, hsh_idx, words, DYN_MPLS,
				-6, 32, toeplitz);
		} else {
			NT_LOG(DBG, FILTER, "Set inner C-VLAN hasher.");
			/*
			 * use whole 32-bit 802.1a tag - backward compatible
			 * with VSWITCH implementation
			 */
			res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_MPLS,
				-6, 32, toeplitz);
		}
		unset_bits_and_log(&fields, RTE_ETH_RSS_C_VLAN);
	}

	if (fields & RTE_ETH_RSS_S_VLAN) {
		if (outer) {
			NT_LOG(DBG, FILTER, "Set outer S-VLAN hasher.");
			/*
			 * use whole 32-bit 802.1a tag - backward compatible
			 * with VSWITCH implementation
			 */
			res |= hsh_set_part(ndev, hsh_idx, words, DYN_FIRST_VLAN,
				0, 32, toeplitz);
		} else {
			NT_LOG(DBG, FILTER, "Set inner S-VLAN hasher.");
			/*
			 * use whole 32-bit 802.1a tag - backward compatible
			 * with VSWITCH implementation
			 */
			res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_VLAN,
				0, 32, toeplitz);
		}
		unset_bits_and_log(&fields, RTE_ETH_RSS_S_VLAN);
	}

	/* L2 payload */
	/* calculate hash of 128-bits of l2 payload;
	 * Use MPLS protocol offset to address the beginning
	 * of L2 payload even if MPLS header is not present
	 */
	if (fields & RTE_ETH_RSS_L2_PAYLOAD) {
		uint64_t outer_fields_enabled = 0;
		if (outer) {
			NT_LOG(DBG, FILTER, "Set outer L2 payload hasher.");
			res |= hsh_set_part(ndev, hsh_idx, words, DYN_MPLS,
				0, 128, toeplitz);
		} else {
			NT_LOG(DBG, FILTER, "Set inner L2 payload hasher.");
			res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_MPLS,
				0, 128, toeplitz);
			outer_fields_enabled = fields & RTE_ETH_RSS_GTPU;
		}
		/*
		 * L2 PAYLOAD hashing overrides all L3 & L4 RSS flags.
		 * Thus we can clear all remaining (supported)
		 * RSS flags...
		 */
		unset_bits_and_log(&fields, NT_ETH_RSS_OFFLOAD_MASK);
		/*
		 * ...but in case of INNER L2 PAYLOAD we must process
		 * "always outer" GTPU field if enabled
		 */
		fields |= outer_fields_enabled;
	}

	/* L3 + L4 protocol number */
	if (fields & RTE_ETH_RSS_IPV4_CHKSUM) {
		/* only IPv4 checksum is supported by DPDK RTE_ETH_RSS_* types */
		if (ipv6) {
			NT_LOG(ERR, FILTER, "RSS: IPv4 checksum requested with IPv6 header hashing!");
			res = 1;
		} else {
			if (outer) {
				NT_LOG(DBG, FILTER, "Set outer IPv4 checksum hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_L3,
					10, 16, toeplitz);
			} else {
				NT_LOG(DBG, FILTER, "Set inner IPv4 checksum hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_L3,
					10, 16, toeplitz);
			}
		}
		/*
		 * L3 checksum is made from whole L3 header, i.e. no need to process other
		 * L3 hashing flags
		 */
		unset_bits_and_log(&fields, RTE_ETH_RSS_IPV4_CHKSUM | NT_ETH_RSS_IP_MASK);
	}

	if (fields & NT_ETH_RSS_IP_MASK) {
		if (ipv6) {
			if (outer) {
				if (fields & RTE_ETH_RSS_L3_SRC_ONLY) {
					NT_LOG(DBG, FILTER, "Set outer IPv6/IPv4 src hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_FINAL_IP_DST,
						-16, 128, toeplitz);
				} else if (fields & RTE_ETH_RSS_L3_DST_ONLY) {
					NT_LOG(DBG, FILTER, "Set outer IPv6/IPv4 dst hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_FINAL_IP_DST,
						0, 128, toeplitz);
				} else {
					NT_LOG(DBG, FILTER, "Set outer IPv6/IPv4 src & dst hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_FINAL_IP_DST,
						-16, 128, toeplitz);
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_FINAL_IP_DST,
						0, 128, toeplitz);
				}
			} else {
				if (fields & RTE_ETH_RSS_L3_SRC_ONLY) {
					NT_LOG(DBG, FILTER, "Set inner IPv6/IPv4 src hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words,
						DYN_TUN_FINAL_IP_DST, -16, 128, toeplitz);
				} else if (fields & RTE_ETH_RSS_L3_DST_ONLY) {
					NT_LOG(DBG, FILTER, "Set inner IPv6/IPv4 dst hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words,
						DYN_TUN_FINAL_IP_DST, 0, 128, toeplitz);
				} else {
					NT_LOG(DBG, FILTER, "Set inner IPv6/IPv4 src & dst hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words,
						DYN_TUN_FINAL_IP_DST, -16, 128, toeplitz);
					res |= hsh_set_part(ndev, hsh_idx, words,
						DYN_TUN_FINAL_IP_DST, 0, 128, toeplitz);
				}
			}
			/* check if fragment ID shall be part of hash */
			if (fields & (RTE_ETH_RSS_FRAG_IPV4 | RTE_ETH_RSS_FRAG_IPV6)) {
				if (outer) {
					NT_LOG(DBG, FILTER, "Set outer IPv6/IPv4 fragment ID hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_ID_IPV4_6,
						0, 32, toeplitz);
				} else {
					NT_LOG(DBG, FILTER, "Set inner IPv6/IPv4 fragment ID hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_ID_IPV4_6,
						0, 32, toeplitz);
				}
			}
			res |= nthw_mod_hsh_rcp_set(&ndev->be, HW_HSH_RCP_AUTO_IPV4_MASK,
				hsh_idx, 0, 1);
		} else {
			/* IPv4 */
			if (outer) {
				if (fields & RTE_ETH_RSS_L3_SRC_ONLY) {
					NT_LOG(DBG, FILTER, "Set outer IPv4 src only hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_L3,
						12, 32, toeplitz);
				} else if (fields & RTE_ETH_RSS_L3_DST_ONLY) {
					NT_LOG(DBG, FILTER, "Set outer IPv4 dst only hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_L3,
						16, 32, toeplitz);
				} else {
					NT_LOG(DBG, FILTER, "Set outer IPv4 src & dst hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_L3,
						12, 64, toeplitz);
				}
			} else {
				if (fields & RTE_ETH_RSS_L3_SRC_ONLY) {
					NT_LOG(DBG, FILTER, "Set inner IPv4 src only hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_L3,
						12, 32, toeplitz);
				} else if (fields & RTE_ETH_RSS_L3_DST_ONLY) {
					NT_LOG(DBG, FILTER, "Set inner IPv4 dst only hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_L3,
						16, 32, toeplitz);
				} else {
					NT_LOG(DBG, FILTER, "Set inner IPv4 src & dst hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_L3,
						12, 64, toeplitz);
				}
			}
			/* check if fragment ID shall be part of hash */
			if (fields & RTE_ETH_RSS_FRAG_IPV4) {
				if (outer) {
					NT_LOG(DBG, FILTER, "Set outer IPv4 fragment ID hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_ID_IPV4_6,
						0, 16, toeplitz);
				} else {
					NT_LOG(DBG, FILTER, "Set inner IPv4 fragment ID hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_ID_IPV4_6,
						0, 16, toeplitz);
				}
			}
		}

		/* check if L4 protocol type shall be part of hash */
		if (l4_proto_mask)
			l4_proto_hash = true;
		unset_bits_and_log(&fields, NT_ETH_RSS_IP_MASK);
	}

	/* L4 */
	if (fields & (RTE_ETH_RSS_PORT | RTE_ETH_RSS_L4_SRC_ONLY |
		RTE_ETH_RSS_L4_DST_ONLY)) {
		if (outer) {
			if (fields & RTE_ETH_RSS_L4_SRC_ONLY) {
				NT_LOG(DBG, FILTER, "Set outer L4 src hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_L4,
					0, 16, toeplitz);
			} else if (fields & RTE_ETH_RSS_L4_DST_ONLY) {
				NT_LOG(DBG, FILTER, "Set outer L4 dst hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_L4,
					2, 16, toeplitz);
			} else {
				NT_LOG(DBG, FILTER, "Set outer L4 src & dst hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_L4,
					0, 32, toeplitz);
			}
		} else {
			if (fields & RTE_ETH_RSS_L4_SRC_ONLY) {
				NT_LOG(DBG, FILTER, "Set inner L4 src hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_L4,
					0, 16, toeplitz);
			} else if (fields & RTE_ETH_RSS_L4_DST_ONLY) {
				NT_LOG(DBG, FILTER, "Set inner L4 dst hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_L4,
					2, 16, toeplitz);
			} else {
				NT_LOG(DBG, FILTER, "Set inner L4 src & dst hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_L4,
					0, 32, toeplitz);
			}
		}
		l4_proto_hash = true;
		unset_bits_and_log(&fields, RTE_ETH_RSS_PORT | RTE_ETH_RSS_L4_SRC_ONLY |
			RTE_ETH_RSS_L4_DST_ONLY);
	}

	/* IPv4 protocol / IPv6 next header fields */
	if (l4_proto_hash) {
		/* NOTE: HW_HSH_RCP_P_MASK is not supported for
		 *Toeplitz and thus one of SW0, SW4 or W8
		 * must be used to hash on `protocol` field of IPv4 or
		 * `next header` field of IPv6 header.
		 */
		if (outer) {
			NT_LOG(DBG, FILTER, "Set outer L4 protocol type / next header hasher.");
			if (toeplitz) {
				if (ipv6)
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_L3, 6, 8,
						toeplitz);
				else
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_L3, 9, 8,
						toeplitz);
			} else {
				res |= nthw_mod_hsh_rcp_set(&ndev->be, HW_HSH_RCP_P_MASK,
					hsh_idx, 0, 1);
				res |= nthw_mod_hsh_rcp_set(&ndev->be, HW_HSH_RCP_TNL_P,
					hsh_idx, 0, 0);
			}
		} else {
			NT_LOG(DBG, FILTER, "Set inner L4 protocol type / next header hasher.");
			if (toeplitz) {
				if (ipv6) {
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_L3, 6, 8,
						toeplitz);
				} else {
					res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_L3, 9, 8,
						toeplitz);
				}
			} else {
				res |= nthw_mod_hsh_rcp_set(&ndev->be,
					HW_HSH_RCP_P_MASK, hsh_idx, 0, 1);
				res |= nthw_mod_hsh_rcp_set(&ndev->be,
					HW_HSH_RCP_TNL_P, hsh_idx, 0, 1);
			}
		}
		l4_proto_hash = false;
	}

	/*
	 * GTPU - for UPF use cases we always use TEID from outermost GTPU header
	 * even if other headers are innermost
	 */
	if (fields & RTE_ETH_RSS_GTPU) {
		NT_LOG(DBG, FILTER, "Set outer GTPU TEID hasher.");
		res |= hsh_set_part(ndev, hsh_idx, words, DYN_L4_PAYLOAD, 4, 32, toeplitz);
		unset_bits_and_log(&fields, RTE_ETH_RSS_GTPU);
	}

	/* Checksums */
	/* only UDP, TCP and SCTP checksums are supported */
	if (fields & RTE_ETH_RSS_L4_CHKSUM) {
		switch (l4_proto_mask) {
		case RTE_ETH_RSS_NONFRAG_IPV4_UDP:
		case RTE_ETH_RSS_NONFRAG_IPV6_UDP:
		case RTE_ETH_RSS_IPV6_UDP_EX:
		case RTE_ETH_RSS_NONFRAG_IPV4_UDP | RTE_ETH_RSS_NONFRAG_IPV6_UDP:
		case RTE_ETH_RSS_NONFRAG_IPV4_UDP | RTE_ETH_RSS_IPV6_UDP_EX:
		case RTE_ETH_RSS_NONFRAG_IPV6_UDP | RTE_ETH_RSS_IPV6_UDP_EX:
		case RTE_ETH_RSS_UDP_COMBINED:
			if (outer) {
				NT_LOG(DBG, FILTER, "Set outer UDP checksum hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_L4, 6, 16,
					toeplitz);
			} else {
				NT_LOG(DBG, FILTER, "Set inner UDP checksum hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_L4, 6, 16,
					toeplitz);
			}
			unset_bits_and_log(&fields, RTE_ETH_RSS_L4_CHKSUM | l4_proto_mask);
			break;
		case RTE_ETH_RSS_NONFRAG_IPV4_TCP:
		case RTE_ETH_RSS_NONFRAG_IPV6_TCP:
		case RTE_ETH_RSS_IPV6_TCP_EX:
		case RTE_ETH_RSS_NONFRAG_IPV4_TCP | RTE_ETH_RSS_NONFRAG_IPV6_TCP:
		case RTE_ETH_RSS_NONFRAG_IPV4_TCP | RTE_ETH_RSS_IPV6_TCP_EX:
		case RTE_ETH_RSS_NONFRAG_IPV6_TCP | RTE_ETH_RSS_IPV6_TCP_EX:
		case RTE_ETH_RSS_TCP_COMBINED:
				if (outer) {
					NT_LOG(DBG, FILTER, "Set outer TCP checksum hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words,
						DYN_L4, 16, 16, toeplitz);
				} else {
					NT_LOG(DBG, FILTER, "Set inner TCP checksum hasher.");
					res |= hsh_set_part(ndev, hsh_idx, words,
						DYN_TUN_L4, 16, 16, toeplitz);
				}
				unset_bits_and_log(&fields, RTE_ETH_RSS_L4_CHKSUM | l4_proto_mask);
				break;
		case RTE_ETH_RSS_NONFRAG_IPV4_SCTP:
		case RTE_ETH_RSS_NONFRAG_IPV6_SCTP:
		case RTE_ETH_RSS_NONFRAG_IPV4_SCTP | RTE_ETH_RSS_NONFRAG_IPV6_SCTP:
			if (outer) {
				NT_LOG(DBG, FILTER, "Set outer SCTP checksum hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_L4, 8, 32,
					toeplitz);
			} else {
				NT_LOG(DBG, FILTER, "Set inner SCTP checksum hasher.");
				res |= hsh_set_part(ndev, hsh_idx, words, DYN_TUN_L4, 8, 32,
					toeplitz);
			}
			unset_bits_and_log(&fields, RTE_ETH_RSS_L4_CHKSUM | l4_proto_mask);
			break;
		case RTE_ETH_RSS_NONFRAG_IPV4_OTHER:
		case RTE_ETH_RSS_NONFRAG_IPV6_OTHER:
		/* none or unsupported protocol was chosen */
		case 0:
			NT_LOG(ERR, FILTER, "L4 checksum hashing is supported only for UDP, TCP and SCTP protocols");
			res = -1;
			break;
		/* multiple L4 protocols were selected */
		default:
			NT_LOG(ERR, FILTER, "L4 checksum hashing can be enabled just for one of UDP, TCP or SCTP protocols");
			res = -1;
			break;
		}
	}

	if (fields || res != 0) {
		nthw_mod_hsh_rcp_set(&ndev->be, HW_HSH_RCP_PRESET_ALL, hsh_idx, 0, 0);
		if (sprint_nt_rss_mask(rss_buffer, rss_buffer_len, " ", rss_conf.rss_hf) == 0) {
			NT_LOG(ERR, FILTER, "RSS configuration%s is not supported for hash func %s.",
				rss_buffer, (enum rte_eth_hash_function)toeplitz ?
					"Toeplitz" : "NTH10");
		} else {
			NT_LOG(ERR, FILTER, "RSS configuration 0x%" PRIX64 " is not supported for hash func %s.",
				rss_conf.rss_hf, (enum rte_eth_hash_function)toeplitz ?
					"Toeplitz" : "NTH10");
		}
		return -1;
	}

	return res;
}
