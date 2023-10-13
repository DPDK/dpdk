/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_IPSEC_H__
#define __NFP_IPSEC_H__

#include <rte_security.h>

#define NFP_NET_IPSEC_MAX_SA_CNT       (16 * 1024)

struct ipsec_aesgcm {            /**< AES-GCM-ESP fields */
	uint32_t salt;           /**< Initialized with SA */
	uint32_t iv[2];          /**< Firmware use only */
	uint32_t cntrl;
	uint32_t zeros[4];       /**< Init to 0 with SA */
	uint32_t len_auth[2];    /**< Firmware use only */
	uint32_t len_cipher[2];
	uint32_t spare[4];
};

struct sa_ctrl_word {
	uint32_t hash   :4;      /**< From nfp_ipsec_hash_type */
	uint32_t cimode :4;      /**< From nfp_ipsec_cipher_mode */
	uint32_t cipher :4;      /**< From nfp_ipsec_cipher */
	uint32_t mode   :2;      /**< From nfp_ipsec_mode */
	uint32_t proto  :2;      /**< From nfp_ipsec_prot */
	uint32_t spare  :1;      /**< Should be 0 */
	uint32_t ena_arw:1;      /**< Anti-Replay Window */
	uint32_t ext_seq:1;      /**< 64-bit Sequence Num */
	uint32_t ext_arw:1;      /**< 64b Anti-Replay Window */
	uint32_t spare1 :9;      /**< Must be set to 0 */
	uint32_t encap_dsbl:1;   /**< Encap/decap disable */
	uint32_t gen_seq:1;      /**< Firmware Generate Seq #'s */
	uint32_t spare2 :1;      /**< Must be set to 0 */
};

union nfp_ip_addr {
	struct in6_addr v6;
	struct in_addr v4;
};

struct ipsec_add_sa {
	uint32_t cipher_key[8];           /**< Cipher Key */
	union {
		uint32_t auth_key[16];    /**< Authentication Key */
		struct ipsec_aesgcm aesgcm_fields;
	};
	struct sa_ctrl_word ctrl_word;
	uint32_t spi;                     /**< SPI Value */
	uint16_t pmtu_limit;              /**< PMTU Limit */
	uint32_t spare      :1;
	uint32_t frag_check :1;           /**< Stateful fragment checking flag */
	uint32_t bypass_DSCP:1;           /**< Bypass DSCP Flag */
	uint32_t df_ctrl    :2;           /**< DF Control bits */
	uint32_t ipv6       :1;           /**< Outbound IPv6 addr format */
	uint32_t udp_enable :1;           /**< Add/Remove UDP header for NAT */
	uint32_t tfc_enable :1;           /**< Traffic Flw Confidentiality */
	uint8_t spare1;
	uint32_t soft_byte_cnt;           /**< Soft lifetime byte count */
	uint32_t hard_byte_cnt;           /**< Hard lifetime byte count */
	union nfp_ip_addr src_ip;         /**< Src IP addr */
	union nfp_ip_addr dst_ip;         /**< Dst IP addr */
	uint16_t natt_dst_port;           /**< NAT-T UDP Header dst port */
	uint16_t natt_src_port;           /**< NAT-T UDP Header src port */
	uint32_t soft_lifetime_limit;     /**< Soft lifetime time limit */
	uint32_t hard_lifetime_limit;     /**< Hard lifetime time limit */
	uint32_t sa_time_lo;              /**< SA creation time lower 32bits, Ucode fills this in */
	uint32_t sa_time_hi;              /**< SA creation time high 32bits, Ucode fills this in */
	uint16_t spare2;
	uint16_t tfc_padding;             /**< Traffic Flow Confidential Pad */
};

struct ipsec_inv_sa {
	uint32_t spare;
};

struct ipsec_discard_stats {
	uint32_t discards_auth;                  /**< Auth failures */
	uint32_t discards_unsupported;           /**< Unsupported crypto mode */
	uint32_t discards_alignment;             /**< Alignment error */
	uint32_t discards_hard_bytelimit;        /**< Hard byte Count limit */
	uint32_t discards_seq_num_wrap;          /**< Sequ Number wrap */
	uint32_t discards_pmtu_exceeded;         /**< PMTU Limit exceeded */
	uint32_t discards_arw_old_seq;           /**< Anti-Replay seq small */
	uint32_t discards_arw_replay;            /**< Anti-Replay seq rcvd */
	uint32_t discards_ctrl_word;             /**< Bad SA Control word */
	uint32_t discards_ip_hdr_len;            /**< Hdr offset from too high */
	uint32_t discards_eop_buf;               /**< No EOP buffer */
	uint32_t ipv4_id_counter;                /**< IPv4 ID field counter */
	uint32_t discards_isl_fail;              /**< Inbound SPD Lookup failure */
	uint32_t discards_ext_unfound;           /**< Ext header end */
	uint32_t discards_max_ext_hdrs;          /**< Max ext header */
	uint32_t discards_non_ext_hdrs;          /**< Non-extension headers */
	uint32_t discards_ext_hdr_too_big;       /**< Ext header chain */
	uint32_t discards_hard_timelimit;        /**< Time Limit  */
};

struct ipsec_get_sa_stats {
	uint32_t seq_lo;                         /**< Sequence Number (low 32bits) */
	uint32_t seq_high;                       /**< Sequence Number (high 32bits) */
	uint32_t arw_counter_lo;                 /**< Anti-replay wndw cntr */
	uint32_t arw_counter_high;               /**< Anti-replay wndw cntr */
	uint32_t arw_bitmap_lo;                  /**< Anti-replay wndw bitmap */
	uint32_t arw_bitmap_high;                /**< Anti-replay wndw bitmap */
	uint32_t spare:1;
	uint32_t soft_byte_exceeded :1;          /**< Soft lifetime byte cnt exceeded */
	uint32_t hard_byte_exceeded :1;          /**< Hard lifetime byte cnt exceeded */
	uint32_t soft_time_exceeded :1;          /**< Soft lifetime time limit exceeded */
	uint32_t hard_time_exceeded :1;          /**< Hard lifetime time limit exceeded */
	uint32_t spare1:27;
	uint32_t lifetime_byte_count;
	uint32_t pkt_count;
	struct ipsec_discard_stats sa_discard_stats;
};

struct ipsec_get_seq {
	uint32_t seq_nums;      /**< Sequence numbers to allocate */
	uint32_t seq_num_low;   /**< Return start seq num 31:00 */
	uint32_t seq_num_hi;    /**< Return start seq num 63:32 */
};

struct nfp_ipsec_msg {
	union {
		struct {
			/** NFP IPsec SA cmd message codes */
			uint16_t cmd;
			/** NFP IPsec SA response message */
			uint16_t rsp;
			/** NFP IPsec SA index in driver SA table */
			uint16_t sa_idx;
			/** Reserved */
			uint16_t spare;
			union {
				/** IPsec configure message for add SA */
				struct ipsec_add_sa cfg_add_sa;
				/** IPsec configure message for del SA */
				struct ipsec_inv_sa cfg_inv_sa;
				/** IPsec configure message for get SA stats */
				struct ipsec_get_sa_stats cfg_get_stats;
				/** IPsec configure message for get SA seq numbers */
				struct ipsec_get_seq cfg_get_seq;
			};
		};
		uint32_t raw[64];
	};
};

struct nfp_ipsec_session {
	/** Opaque user defined data */
	void *user_data;
	/** NFP sa_entries database parameter index */
	uint32_t sa_index;
	/** Point to physical ports ethernet device */
	struct rte_eth_dev *dev;
	/** SA related NPF configuration data */
	struct ipsec_add_sa msg;
	/** Security association configuration data */
	struct rte_security_ipsec_xform ipsec;
	/** Security session action type */
	enum rte_security_session_action_type action;
} __rte_cache_aligned;

struct nfp_net_ipsec_data {
	int pkt_dynfield_offset;
	uint32_t sa_free_cnt;
	struct nfp_ipsec_session *sa_entries[NFP_NET_IPSEC_MAX_SA_CNT];
};

enum nfp_ipsec_meta_layer {
	NFP_IPSEC_META_SAIDX,       /**< Order of SA index in metadata */
	NFP_IPSEC_META_SEQLOW,      /**< Order of Sequence Number (low 32bits) in metadata */
	NFP_IPSEC_META_SEQHI,       /**< Order of Sequence Number (high 32bits) in metadata */
};

int nfp_ipsec_init(struct rte_eth_dev *dev);
void nfp_ipsec_uninit(struct rte_eth_dev *dev);

#endif /* __NFP_IPSEC_H__ */
