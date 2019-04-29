/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/* Copyright (C) 2014-2017 aQuantia Corporation. */

/* File hw_atl_utils.h: Declaration of common functions for Atlantic hardware
 * abstraction layer.
 */

#ifndef HW_ATL_UTILS_H
#define HW_ATL_UTILS_H

#define BIT(x)  (1UL << (x))
#define HW_ATL_FLUSH() { (void)aq_hw_read_reg(self, 0x10); }

/* Hardware tx descriptor */
struct hw_atl_txd_s {
	u64 buf_addr;

	union {
		struct {
			u32 type:3;
			u32:1;
			u32 len:16;
			u32 dd:1;
			u32 eop:1;
			u32 cmd:8;
			u32:14;
			u32 ct_idx:1;
			u32 ct_en:1;
			u32 pay_len:18;
		} __attribute__((__packed__));
		u64 flags;
	};
} __attribute__((__packed__));

/* Hardware tx context descriptor */
union hw_atl_txc_s {
	struct {
		u64 flags1;
		u64 flags2;
	};

	struct {
		u64:40;
		u32 tun_len:8;
		u32 out_len:16;
		u32 type:3;
		u32 idx:1;
		u32 vlan_tag:16;
		u32 cmd:4;
		u32 l2_len:7;
		u32 l3_len:9;
		u32 l4_len:8;
		u32 mss_len:16;
	} __attribute__((__packed__));
} __attribute__((__packed__));

enum aq_tx_desc_type {
	tx_desc_type_desc = 1,
	tx_desc_type_ctx = 2,
};

enum aq_tx_desc_cmd {
	tx_desc_cmd_vlan = 1,
	tx_desc_cmd_fcs = 2,
	tx_desc_cmd_ipv4 = 4,
	tx_desc_cmd_l4cs = 8,
	tx_desc_cmd_lso = 0x10,
	tx_desc_cmd_wb = 0x20,
};


/* Hardware rx descriptor */
struct hw_atl_rxd_s {
	u64 buf_addr;
	u64 hdr_addr;
} __attribute__((__packed__));

/* Hardware rx descriptor writeback */
struct hw_atl_rxd_wb_s {
	u32 rss_type:4;
	u32 pkt_type:8;
	u32 type:20;
	u32 rss_hash;
	u16 dd:1;
	u16 eop:1;
	u16 rx_stat:4;
	u16 rx_estat:6;
	u16 rsc_cnt:4;
	u16 pkt_len;
	u16 next_desc_ptr;
	u16 vlan;
} __attribute__((__packed__));

struct hw_atl_stats_s {
	u32 uprc;
	u32 mprc;
	u32 bprc;
	u32 erpt;
	u32 uptc;
	u32 mptc;
	u32 bptc;
	u32 erpr;
	u32 mbtc;
	u32 bbtc;
	u32 mbrc;
	u32 bbrc;
	u32 ubrc;
	u32 ubtc;
	u32 dpc;
} __attribute__((__packed__));

union ip_addr {
	struct {
		u8 addr[16];
	} v6;
	struct {
		u8 padding[12];
		u8 addr[4];
	} v4;
} __attribute__((__packed__));

struct hw_aq_atl_utils_fw_rpc {
	u32 msg_id;

	union {
		struct {
			u32 pong;
		} msg_ping;

		struct {
			u8 mac_addr[6];
			u32 ip_addr_cnt;

			struct {
				union ip_addr addr;
				union ip_addr mask;
			} ip[1];
		} msg_arp;

		struct {
			u32 len;
			u8 packet[1514U];
		} msg_inject;

		struct {
			u32 priority;
			u32 wol_packet_type;
			u32 pattern_id;
			u32 next_wol_pattern_offset;
			union {
				struct {
					u32 flags;
					u8 ipv4_source_address[4];
					u8 ipv4_dest_address[4];
					u16 tcp_source_port_number;
					u16 tcp_dest_port_number;
				} ipv4_tcp_syn_parameters;

				struct {
					u32 flags;
					u8 ipv6_source_address[16];
					u8 ipv6_dest_address[16];
					u16 tcp_source_port_number;
					u16 tcp_dest_port_number;
				} ipv6_tcp_syn_parameters;

				struct {
					u32 flags;
				} eapol_request_id_message_parameters;

				struct {
					u32 flags;
					u32 mask_offset;
					u32 mask_size;
					u32 pattern_offset;
					u32 pattern_size;
				} wol_bit_map_pattern;
				struct {
					u8 mac_addr[6];
				} wol_magic_packet_pattern;

			} wol_pattern;
		} msg_wol;

		struct {
			u16 tc_quanta[8];
			u16 tc_threshold[8];
		} msg_msm_pfc_quantas;

		struct {
			union {
				u32 pattern_mask;
				struct {
					u32 aq_pm_wol_reason_arp_v4_pkt : 1;
					u32 aq_pm_wol_reason_ipv4_ping_pkt : 1;
					u32 aq_pm_wol_reason_ipv6_ns_pkt : 1;
					u32 aq_pm_wol_reason_ipv6_ping_pkt : 1;
					u32 aq_pm_wol_reason_link_up : 1;
					u32 aq_pm_wol_reason_link_down : 1;
					u32 aq_pm_wol_reason_maximum : 1;
				};
			};
			union {
				u32 offload_mask;
			};
		} msg_enable_wakeup;

		struct {
			u32 priority;
			u32 protocol_offload_type;
			u32 protocol_offload_id;
			u32 next_protocol_offload_offset;

			union {
				struct {
					u32 flags;
					u8 remote_ipv4_addr[4];
					u8 host_ipv4_addr[4];
					u8 mac_addr[6];
				} ipv4_arp_params;
			};
		} msg_offload;

		struct {
			u32 id;
		} msg_del_id;

	};
} __attribute__((__packed__));

struct hw_aq_atl_utils_mbox_header {
	u32 version;
	u32 transaction_id;
	u32 error;
} __attribute__((__packed__));

struct hw_aq_info {
	u8 reserved[6];
	u16 phy_fault_code;
	u16 phy_temperature;
	u8 cable_len;
	u8 reserved1;
	u32 cable_diag_data[4];
	u8 reserved2[32];
	u32 caps_lo;
	u32 caps_hi;
} __attribute__((__packed__));

struct hw_aq_atl_utils_mbox {
	struct hw_aq_atl_utils_mbox_header header;
	struct hw_atl_stats_s stats;
	struct hw_aq_info info;
} __attribute__((__packed__));

/* fw2x */
typedef u16	in_port_t;
typedef u32	ip4_addr_t;
typedef int	int32_t;
typedef short	int16_t;
typedef u32	fw_offset_t;

struct ip6_addr {
	u32 addr[4];
} __attribute__((__packed__));

struct offload_ka_v4 {
	u32 timeout;
	in_port_t local_port;
	in_port_t remote_port;
	u8 remote_mac_addr[6];
	u16 win_size;
	u32 seq_num;
	u32 ack_num;
	ip4_addr_t local_ip;
	ip4_addr_t remote_ip;
} __attribute__((__packed__));

struct offload_ka_v6 {
	u32 timeout;
	in_port_t local_port;
	in_port_t remote_port;
	u8 remote_mac_addr[6];
	u16 win_size;
	u32 seq_num;
	u32 ack_num;
	struct ip6_addr local_ip;
	struct ip6_addr remote_ip;
} __attribute__((__packed__));

struct offload_ip_info {
	u8 v4_local_addr_count;
	u8 v4_addr_count;
	u8 v6_local_addr_count;
	u8 v6_addr_count;
	fw_offset_t v4_addr;
	fw_offset_t v4_prefix;
	fw_offset_t v6_addr;
	fw_offset_t v6_prefix;
} __attribute__((__packed__));

struct offload_port_info {
	u16 udp_port_count;
	u16 tcp_port_count;
	fw_offset_t udp_port;
	fw_offset_t tcp_port;
} __attribute__((__packed__));

struct offload_ka_info {
	u16 v4_ka_count;
	u16 v6_ka_count;
	u32 retry_count;
	u32 retry_interval;
	fw_offset_t v4_ka;
	fw_offset_t v6_ka;
} __attribute__((__packed__));

struct offload_rr_info {
	u32 rr_count;
	u32 rr_buf_len;
	fw_offset_t rr_id_x;
	fw_offset_t rr_buf;
} __attribute__((__packed__));

struct offload_info {
	u32 version;		// current version is 0x00000000
	u32 len;		// The whole structure length
				// including the variable-size buf
	u8 mac_addr[6];		// 8 bytes to keep alignment. Only
				// first 6 meaningful.

	u8 reserved[2];

	struct offload_ip_info ips;
	struct offload_port_info ports;
	struct offload_ka_info kas;
	struct offload_rr_info rrs;
	u8 buf[0];
} __attribute__((__packed__));

struct smbus_request {
	u32 msg_id; /* not used */
	u32 device_id;
	u32 address;
	u32 length;
} __attribute__((__packed__));

enum macsec_msg_type {
	macsec_cfg_msg = 0,
	macsec_add_rx_sc_msg,
	macsec_add_tx_sc_msg,
	macsec_add_rx_sa_msg,
	macsec_add_tx_sa_msg,
	macsec_get_stats_msg,
};

struct macsec_cfg {
	uint32_t enabled;
	uint32_t egress_threshold;
	uint32_t ingress_threshold;
	uint32_t interrupts_enabled;
} __attribute__((__packed__));

struct add_rx_sc {
	uint32_t index;
	uint32_t pi; /* Port identifier */
	uint32_t sci[2]; /* Secure Channel identifier */
	uint32_t sci_mask; /* 1: enable comparison of SCI, 0: don't care */
	uint32_t tci;
	uint32_t tci_mask;
	uint32_t mac_sa[2];
	uint32_t sa_mask; /* 0: ignore mac_sa */
	uint32_t mac_da[2];
	uint32_t da_mask; /* 0: ignore mac_da */
	uint32_t validate_frames; /* 0: strict, 1:check, 2:disabled */
	uint32_t replay_protect; /* 1: enabled, 0:disabled */
	uint32_t anti_replay_window; /* default 0 */
	/* 1: auto_rollover enabled (when SA next_pn is saturated */
	uint32_t an_rol;
} __attribute__((__packed__));

struct add_tx_sc {
	uint32_t index;
	uint32_t pi; /* Port identifier */
	uint32_t sci[2]; /* Secure Channel identifier */
	uint32_t sci_mask; /* 1: enable comparison of SCI, 0: don't care */
	uint32_t tci; /* TCI value, used if packet is not explicitly tagged */
	uint32_t tci_mask;
	uint32_t mac_sa[2];
	uint32_t sa_mask; /* 0: ignore mac_sa */
	uint32_t mac_da[2];
	uint32_t da_mask; /* 0: ignore mac_da */
	uint32_t protect;
	uint32_t curr_an; /* SA index which currently used */
} __attribute__((__packed__));

struct add_rx_sa {
	uint32_t index;
	uint32_t next_pn;
	uint32_t key[4]; /* 128 bit key */
} __attribute__((__packed__));

struct add_tx_sa {
	uint32_t index;
	uint32_t next_pn;
	uint32_t key[4]; /* 128 bit key */
} __attribute__((__packed__));

struct get_stats {
	uint32_t version_only;
	uint32_t ingress_sa_index;
	uint32_t egress_sa_index;
	uint32_t egress_sc_index;
} __attribute__((__packed__));

struct macsec_stats {
	uint32_t api_version;
	/* Ingress Common Counters */
	uint64_t in_ctl_pkts;
	uint64_t in_tagged_miss_pkts;
	uint64_t in_untagged_miss_pkts;
	uint64_t in_notag_pkts;
	uint64_t in_untagged_pkts;
	uint64_t in_bad_tag_pkts;
	uint64_t in_no_sci_pkts;
	uint64_t in_unknown_sci_pkts;
	uint64_t in_ctrl_prt_pass_pkts;
	uint64_t in_unctrl_prt_pass_pkts;
	uint64_t in_ctrl_prt_fail_pkts;
	uint64_t in_unctrl_prt_fail_pkts;
	uint64_t in_too_long_pkts;
	uint64_t in_igpoc_ctl_pkts;
	uint64_t in_ecc_error_pkts;
	uint64_t in_unctrl_hit_drop_redir;

	/* Egress Common Counters */
	uint64_t out_ctl_pkts;
	uint64_t out_unknown_sa_pkts;
	uint64_t out_untagged_pkts;
	uint64_t out_too_long;
	uint64_t out_ecc_error_pkts;
	uint64_t out_unctrl_hit_drop_redir;

	/* Ingress SA Counters */
	uint64_t in_untagged_hit_pkts;
	uint64_t in_ctrl_hit_drop_redir_pkts;
	uint64_t in_not_using_sa;
	uint64_t in_unused_sa;
	uint64_t in_not_valid_pkts;
	uint64_t in_invalid_pkts;
	uint64_t in_ok_pkts;
	uint64_t in_late_pkts;
	uint64_t in_delayed_pkts;
	uint64_t in_unchecked_pkts;
	uint64_t in_validated_octets;
	uint64_t in_decrypted_octets;

	/* Egress SA Counters */
	uint64_t out_sa_hit_drop_redirect;
	uint64_t out_sa_protected2_pkts;
	uint64_t out_sa_protected_pkts;
	uint64_t out_sa_encrypted_pkts;

	/* Egress SC Counters */
	uint64_t out_sc_protected_pkts;
	uint64_t out_sc_encrypted_pkts;
	uint64_t out_sc_protected_octets;
	uint64_t out_sc_encrypted_octets;

	/* SA Counters expiration info */
	uint32_t egress_threshold_expired;
	uint32_t ingress_threshold_expired;
	uint32_t egress_expired;
	uint32_t ingress_expired;
} __attribute__((__packed__));

struct macsec_msg_fw_request {
	uint32_t offset; /* not used */
	uint32_t msg_type;

	union {
		struct macsec_cfg cfg;
		struct add_rx_sc rxsc;
		struct add_tx_sc txsc;
		struct add_rx_sa rxsa;
		struct add_tx_sa txsa;
		struct get_stats stats;
	};
} __attribute__((__packed__));

struct macsec_msg_fw_response {
	uint32_t result;
	struct macsec_stats stats;
} __attribute__((__packed__));

#define HAL_ATLANTIC_UTILS_CHIP_MIPS         0x00000001U
#define HAL_ATLANTIC_UTILS_CHIP_TPO2         0x00000002U
#define HAL_ATLANTIC_UTILS_CHIP_RPF2         0x00000004U
#define HAL_ATLANTIC_UTILS_CHIP_MPI_AQ       0x00000010U
#define HAL_ATLANTIC_UTILS_CHIP_REVISION_A0  0x01000000U
#define HAL_ATLANTIC_UTILS_CHIP_REVISION_B0  0x02000000U
#define HAL_ATLANTIC_UTILS_CHIP_REVISION_B1  0x04000000U


#define IS_CHIP_FEATURE(_F_) (HAL_ATLANTIC_UTILS_CHIP_##_F_ & \
	self->chip_features)

enum hal_atl_utils_fw_state_e {
	MPI_DEINIT = 0,
	MPI_RESET = 1,
	MPI_INIT = 2,
	MPI_POWER = 4,
};

#define HAL_ATLANTIC_RATE_10G        BIT(0)
#define HAL_ATLANTIC_RATE_5G         BIT(1)
#define HAL_ATLANTIC_RATE_5GSR       BIT(2)
#define HAL_ATLANTIC_RATE_2GS        BIT(3)
#define HAL_ATLANTIC_RATE_1G         BIT(4)
#define HAL_ATLANTIC_RATE_100M       BIT(5)
#define HAL_ATLANTIC_RATE_INVALID    BIT(6)

#define HAL_ATLANTIC_UTILS_FW_MSG_PING     1U
#define HAL_ATLANTIC_UTILS_FW_MSG_ARP      2U
#define HAL_ATLANTIC_UTILS_FW_MSG_INJECT   3U
#define HAL_ATLANTIC_UTILS_FW_MSG_WOL_ADD 4U
#define HAL_ATLANTIC_UTILS_FW_MSG_WOL_DEL 5U
#define HAL_ATLANTIC_UTILS_FW_MSG_ENABLE_WAKEUP 6U
#define HAL_ATLANTIC_UTILS_FW_MSG_MSM_PFC  7U
#define HAL_ATLANTIC_UTILS_FW_MSG_PROVISIONING 8U
#define HAL_ATLANTIC_UTILS_FW_MSG_OFFLOAD_ADD  9U
#define HAL_ATLANTIC_UTILS_FW_MSG_OFFLOAD_DEL  10U
#define HAL_ATLANTIC_UTILS_FW_MSG_CABLE_DIAG   13U // 0xd

#define SMBUS_DEVICE_ID 0x50

enum hw_atl_fw2x_caps_lo {
	CAPS_LO_10BASET_HD = 0x00,
	CAPS_LO_10BASET_FD,
	CAPS_LO_100BASETX_HD,
	CAPS_LO_100BASET4_HD,
	CAPS_LO_100BASET2_HD,
	CAPS_LO_100BASETX_FD,
	CAPS_LO_100BASET2_FD,
	CAPS_LO_1000BASET_HD,
	CAPS_LO_1000BASET_FD,
	CAPS_LO_2P5GBASET_FD,
	CAPS_LO_5GBASET_FD,
	CAPS_LO_10GBASET_FD,
	CAPS_LO_AUTONEG,
	CAPS_LO_SMBUS_READ,
	CAPS_LO_SMBUS_WRITE,
	CAPS_LO_MACSEC
};

enum hw_atl_fw2x_caps_hi {
	CAPS_HI_RESERVED1 = 0x00,
	CAPS_HI_10BASET_EEE,
	CAPS_HI_RESERVED2,
	CAPS_HI_PAUSE,
	CAPS_HI_ASYMMETRIC_PAUSE,
	CAPS_HI_100BASETX_EEE,
	CAPS_HI_RESERVED3,
	CAPS_HI_RESERVED4,
	CAPS_HI_1000BASET_FD_EEE,
	CAPS_HI_2P5GBASET_FD_EEE,
	CAPS_HI_5GBASET_FD_EEE,
	CAPS_HI_10GBASET_FD_EEE,
	CAPS_HI_RESERVED5,
	CAPS_HI_RESERVED6,
	CAPS_HI_RESERVED7,
	CAPS_HI_RESERVED8,
	CAPS_HI_RESERVED9,
	CAPS_HI_CABLE_DIAG,
	CAPS_HI_TEMPERATURE,
	CAPS_HI_DOWNSHIFT,
	CAPS_HI_PTP_AVB_EN,
	CAPS_HI_MEDIA_DETECT,
	CAPS_HI_LINK_DROP,
	CAPS_HI_SLEEP_PROXY,
	CAPS_HI_WOL,
	CAPS_HI_MAC_STOP,
	CAPS_HI_EXT_LOOPBACK,
	CAPS_HI_INT_LOOPBACK,
	CAPS_HI_EFUSE_AGENT,
	CAPS_HI_WOL_TIMER,
	CAPS_HI_STATISTICS,
	CAPS_HI_TRANSACTION_ID,
};

enum hw_atl_fw2x_rate {
	FW2X_RATE_100M    = BIT(CAPS_LO_100BASETX_FD),
	FW2X_RATE_1G      = BIT(CAPS_LO_1000BASET_FD),
	FW2X_RATE_2G5     = BIT(CAPS_LO_2P5GBASET_FD),
	FW2X_RATE_5G      = BIT(CAPS_LO_5GBASET_FD),
	FW2X_RATE_10G     = BIT(CAPS_LO_10GBASET_FD),
};

struct aq_hw_s;
struct aq_fw_ops;
struct aq_hw_link_status_s;

int hw_atl_utils_initfw(struct aq_hw_s *self, const struct aq_fw_ops **fw_ops);

int hw_atl_utils_soft_reset(struct aq_hw_s *self);

void hw_atl_utils_hw_chip_features_init(struct aq_hw_s *self, u32 *p);

int hw_atl_utils_mpi_read_mbox(struct aq_hw_s *self,
			       struct hw_aq_atl_utils_mbox_header *pmbox);

void hw_atl_utils_mpi_read_stats(struct aq_hw_s *self,
				 struct hw_aq_atl_utils_mbox *pmbox);

void hw_atl_utils_mpi_set(struct aq_hw_s *self,
			  enum hal_atl_utils_fw_state_e state,
			  u32 speed);

int hw_atl_utils_mpi_get_link_status(struct aq_hw_s *self);

unsigned int hw_atl_utils_mbps_2_speed_index(unsigned int mbps);

unsigned int hw_atl_utils_hw_get_reg_length(void);

int hw_atl_utils_hw_get_regs(struct aq_hw_s *self,
			     u32 *regs_buff);

int hw_atl_utils_hw_set_power(struct aq_hw_s *self,
			      unsigned int power_state);

int hw_atl_utils_hw_deinit(struct aq_hw_s *self);

int hw_atl_utils_get_fw_version(struct aq_hw_s *self, u32 *fw_version);

int hw_atl_utils_update_stats(struct aq_hw_s *self);

struct aq_stats_s *hw_atl_utils_get_hw_stats(struct aq_hw_s *self);

int hw_atl_utils_fw_downld_dwords(struct aq_hw_s *self, u32 a,
				  u32 *p, u32 cnt);

int hw_atl_utils_fw_upload_dwords(struct aq_hw_s *self, u32 a, u32 *p,
				u32 cnt);

int hw_atl_utils_fw_set_wol(struct aq_hw_s *self, bool wol_enabled, u8 *mac);

int hw_atl_utils_fw_rpc_call(struct aq_hw_s *self, unsigned int rpc_size);

int hw_atl_utils_fw_rpc_wait(struct aq_hw_s *self,
		    struct hw_aq_atl_utils_fw_rpc **rpc);

extern const struct aq_fw_ops aq_fw_1x_ops;
extern const struct aq_fw_ops aq_fw_2x_ops;

#endif /* HW_ATL_UTILS_H */
