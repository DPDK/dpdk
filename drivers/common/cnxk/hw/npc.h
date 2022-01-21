/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __NPC_HW_H__
#define __NPC_HW_H__

/* Register offsets */

#define NPC_AF_CFG		       (0x0ull)
#define NPC_AF_ACTIVE_PC	       (0x10ull)
#define NPC_AF_CONST		       (0x20ull)
#define NPC_AF_CONST1		       (0x30ull)
#define NPC_AF_BLK_RST		       (0x40ull)
#define NPC_AF_MCAM_SCRUB_CTL	       (0xa0ull)
#define NPC_AF_KCAM_SCRUB_CTL	       (0xb0ull)
#define NPC_AF_KPUX_CFG(a)	       (0x500ull | (uint64_t)(a) << 3)
#define NPC_AF_PCK_CFG		       (0x600ull)
#define NPC_AF_PCK_DEF_OL2	       (0x610ull)
#define NPC_AF_PCK_DEF_OIP4	       (0x620ull)
#define NPC_AF_PCK_DEF_OIP6	       (0x630ull)
#define NPC_AF_PCK_DEF_IIP4	       (0x640ull)
#define NPC_AF_KEX_LDATAX_FLAGS_CFG(a) (0x800ull | (uint64_t)(a) << 3)
#define NPC_AF_INTFX_KEX_CFG(a)	       (0x1010ull | (uint64_t)(a) << 8)
#define NPC_AF_PKINDX_ACTION0(a)       (0x80000ull | (uint64_t)(a) << 6)
#define NPC_AF_PKINDX_ACTION1(a)       (0x80008ull | (uint64_t)(a) << 6)
#define NPC_AF_PKINDX_CPI_DEFX(a, b)                                           \
	(0x80020ull | (uint64_t)(a) << 6 | (uint64_t)(b) << 3)
#define NPC_AF_CHLEN90B_PKIND (0x3bull)
#define NPC_AF_KPUX_ENTRYX_CAMX(a, b, c)                                       \
	(0x100000ull | (uint64_t)(a) << 14 | (uint64_t)(b) << 6 |              \
	 (uint64_t)(c) << 3)
#define NPC_AF_KPUX_ENTRYX_ACTION0(a, b)                                       \
	(0x100020ull | (uint64_t)(a) << 14 | (uint64_t)(b) << 6)
#define NPC_AF_KPUX_ENTRYX_ACTION1(a, b)                                       \
	(0x100028ull | (uint64_t)(a) << 14 | (uint64_t)(b) << 6)
#define NPC_AF_KPUX_ENTRY_DISX(a, b)                                           \
	(0x180000ull | (uint64_t)(a) << 6 | (uint64_t)(b) << 3)
#define NPC_AF_CPIX_CFG(a) (0x200000ull | (uint64_t)(a) << 3)
#define NPC_AF_INTFX_LIDX_LTX_LDX_CFG(a, b, c, d)                              \
	(0x900000ull | (uint64_t)(a) << 16 | (uint64_t)(b) << 12 |             \
	 (uint64_t)(c) << 5 | (uint64_t)(d) << 3)
#define NPC_AF_INTFX_LDATAX_FLAGSX_CFG(a, b, c)                                \
	(0x980000ull | (uint64_t)(a) << 16 | (uint64_t)(b) << 12 |             \
	 (uint64_t)(c) << 3)
#define NPC_AF_MCAMEX_BANKX_CAMX_INTF(a, b, c)                                 \
	(0x1000000ull | (uint64_t)(a) << 10 | (uint64_t)(b) << 6 |             \
	 (uint64_t)(c) << 3)
#define NPC_AF_MCAMEX_BANKX_CAMX_W0(a, b, c)                                   \
	(0x1000010ull | (uint64_t)(a) << 10 | (uint64_t)(b) << 6 |             \
	 (uint64_t)(c) << 3)
#define NPC_AF_MCAMEX_BANKX_CAMX_W1(a, b, c)                                   \
	(0x1000020ull | (uint64_t)(a) << 10 | (uint64_t)(b) << 6 |             \
	 (uint64_t)(c) << 3)
#define NPC_AF_MCAMEX_BANKX_CFG(a, b)                                          \
	(0x1800000ull | (uint64_t)(a) << 8 | (uint64_t)(b) << 4)
#define NPC_AF_MCAMEX_BANKX_STAT_ACT(a, b)                                     \
	(0x1880000ull | (uint64_t)(a) << 8 | (uint64_t)(b) << 4)
#define NPC_AF_MATCH_STATX(a)	      (0x1880008ull | (uint64_t)(a) << 8)
#define NPC_AF_INTFX_MISS_STAT_ACT(a) (0x1880040ull + 0x8 * (uint64_t)(a))
#define NPC_AF_MCAMEX_BANKX_ACTION(a, b)                                       \
	(0x1900000ull | (uint64_t)(a) << 8 | (uint64_t)(b) << 4)
#define NPC_AF_MCAMEX_BANKX_TAG_ACT(a, b)                                      \
	(0x1900008ull | (uint64_t)(a) << 8 | (uint64_t)(b) << 4)
#define NPC_AF_INTFX_MISS_ACT(a)     (0x1a00000ull | (uint64_t)(a) << 4)
#define NPC_AF_INTFX_MISS_TAG_ACT(a) (0x1b00008ull | (uint64_t)(a) << 4)
#define NPC_AF_MCAM_BANKX_HITX(a, b)                                           \
	(0x1c80000ull | (uint64_t)(a) << 8 | (uint64_t)(b) << 4)
#define NPC_AF_LKUP_CTL	       (0x2000000ull)
#define NPC_AF_LKUP_DATAX(a)   (0x2000200ull | (uint64_t)(a) << 4)
#define NPC_AF_LKUP_RESULTX(a) (0x2000400ull | (uint64_t)(a) << 4)
#define NPC_AF_INTFX_STAT(a)   (0x2000800ull | (uint64_t)(a) << 4)
#define NPC_AF_DBG_CTL	       (0x3000000ull)
#define NPC_AF_DBG_STATUS      (0x3000010ull)
#define NPC_AF_KPUX_DBG(a)     (0x3000020ull | (uint64_t)(a) << 8)
#define NPC_AF_IKPU_ERR_CTL    (0x3000080ull)
#define NPC_AF_KPUX_ERR_CTL(a) (0x30000a0ull | (uint64_t)(a) << 8)
#define NPC_AF_MCAM_DBG	       (0x3001000ull)
#define NPC_AF_DBG_DATAX(a)    (0x3001400ull | (uint64_t)(a) << 4)
#define NPC_AF_DBG_RESULTX(a)  (0x3001800ull | (uint64_t)(a) << 4)

/* Enum offsets */

#define NPC_INTF_NIX0_RX (0x0ull)
#define NPC_INTF_NIX0_TX (0x1ull)

#define NPC_LKUPOP_PKT (0x0ull)
#define NPC_LKUPOP_KEY (0x1ull)

#define NPC_MCAM_KEY_X1 (0x0ull)
#define NPC_MCAM_KEY_X2 (0x1ull)
#define NPC_MCAM_KEY_X4 (0x2ull)

#ifndef __NPC_ERRLEVELS__
#define __NPC_ERRLEVELS__

enum NPC_ERRLEV_E {
	NPC_ERRLEV_RE = 0,
	NPC_ERRLEV_LA = 1,
	NPC_ERRLEV_LB = 2,
	NPC_ERRLEV_LC = 3,
	NPC_ERRLEV_LD = 4,
	NPC_ERRLEV_LE = 5,
	NPC_ERRLEV_LF = 6,
	NPC_ERRLEV_LG = 7,
	NPC_ERRLEV_LH = 8,
	NPC_ERRLEV_R9 = 9,
	NPC_ERRLEV_R10 = 10,
	NPC_ERRLEV_R11 = 11,
	NPC_ERRLEV_R12 = 12,
	NPC_ERRLEV_R13 = 13,
	NPC_ERRLEV_R14 = 14,
	NPC_ERRLEV_NIX = 15,
	NPC_ERRLEV_ENUM_LAST = 16,
};

#endif

enum npc_kpu_err_code {
	NPC_EC_NOERR = 0, /* has to be zero */
	NPC_EC_UNK,
	NPC_EC_IH_LENGTH,
	NPC_EC_EDSA_UNK,
	NPC_EC_L2_K1,
	NPC_EC_L2_K2,
	NPC_EC_L2_K3,
	NPC_EC_L2_K3_ETYPE_UNK,
	NPC_EC_L2_K4,
	NPC_EC_MPLS_2MANY,
	NPC_EC_MPLS_UNK,
	NPC_EC_NSH_UNK,
	NPC_EC_IP_TTL_0,
	NPC_EC_IP_FRAG_OFFSET_1,
	NPC_EC_IP_VER,
	NPC_EC_IP6_HOP_0,
	NPC_EC_IP6_VER,
	NPC_EC_TCP_FLAGS_FIN_ONLY,
	NPC_EC_TCP_FLAGS_ZERO,
	NPC_EC_TCP_FLAGS_RST_FIN,
	NPC_EC_TCP_FLAGS_URG_SYN,
	NPC_EC_TCP_FLAGS_RST_SYN,
	NPC_EC_TCP_FLAGS_SYN_FIN,
	NPC_EC_VXLAN,
	NPC_EC_NVGRE,
	NPC_EC_GRE,
	NPC_EC_GRE_VER1,
	NPC_EC_L4,
	NPC_EC_OIP4_CSUM,
	NPC_EC_IIP4_CSUM,
	NPC_EC_LAST /* has to be the last item */
};

enum NPC_LID_E {
	NPC_LID_LA = 0,
	NPC_LID_LB,
	NPC_LID_LC,
	NPC_LID_LD,
	NPC_LID_LE,
	NPC_LID_LF,
	NPC_LID_LG,
	NPC_LID_LH,
};

#ifndef __NPC_LT_TYPES__
#define __NPC_LT_TYPES__
#define NPC_LT_NA 0

enum npc_kpu_la_ltype {
	NPC_LT_LA_8023 = 1,
	NPC_LT_LA_ETHER,
	NPC_LT_LA_IH_NIX_ETHER,
	NPC_LT_LA_HIGIG2_ETHER = 7,
	NPC_LT_LA_IH_NIX_HIGIG2_ETHER,
	NPC_LT_LA_CUSTOM_L2_90B_ETHER,
	NPC_LT_LA_CPT_HDR,
	NPC_LT_LA_CUSTOM_L2_24B_ETHER,
	NPC_LT_LA_CUSTOM_PRE_L2_ETHER,
	NPC_LT_LA_CUSTOM0 = 0xE,
	NPC_LT_LA_CUSTOM1 = 0xF,
};

enum npc_kpu_lb_ltype {
	NPC_LT_LB_ETAG = 1,
	NPC_LT_LB_CTAG,
	NPC_LT_LB_STAG_QINQ,
	NPC_LT_LB_BTAG,
	NPC_LT_LB_PPPOE,
	NPC_LT_LB_DSA,
	NPC_LT_LB_DSA_VLAN,
	NPC_LT_LB_EDSA,
	NPC_LT_LB_EDSA_VLAN,
	NPC_LT_LB_EXDSA,
	NPC_LT_LB_EXDSA_VLAN,
	NPC_LT_LB_FDSA,
	NPC_LT_LB_VLAN_EXDSA,
	NPC_LT_LB_CUSTOM0 = 0xE,
	NPC_LT_LB_CUSTOM1 = 0xF,
};

enum npc_kpu_lc_ltype {
	NPC_LT_LC_PTP = 1,
	NPC_LT_LC_IP,
	NPC_LT_LC_IP_OPT,
	NPC_LT_LC_IP6,
	NPC_LT_LC_IP6_EXT,
	NPC_LT_LC_ARP,
	NPC_LT_LC_RARP,
	NPC_LT_LC_MPLS,
	NPC_LT_LC_NSH,
	NPC_LT_LC_FCOE,
	NPC_LT_LC_NGIO,
	NPC_LT_LC_CUSTOM0 = 0xE,
	NPC_LT_LC_CUSTOM1 = 0xF,
};

/* Don't modify Ltypes up to SCTP, otherwise it will
 * effect flow tag calculation and thus RSS.
 */
enum npc_kpu_ld_ltype {
	NPC_LT_LD_TCP = 1,
	NPC_LT_LD_UDP,
	NPC_LT_LD_ICMP,
	NPC_LT_LD_SCTP,
	NPC_LT_LD_ICMP6,
	NPC_LT_LD_CUSTOM0,
	NPC_LT_LD_CUSTOM1,
	NPC_LT_LD_IGMP = 8,
	NPC_LT_LD_AH,
	NPC_LT_LD_GRE,
	NPC_LT_LD_NVGRE,
	NPC_LT_LD_NSH,
	NPC_LT_LD_TU_MPLS_IN_NSH,
	NPC_LT_LD_TU_MPLS_IN_IP,
};

enum npc_kpu_le_ltype {
	NPC_LT_LE_VXLAN = 1,
	NPC_LT_LE_GENEVE,
	NPC_LT_LE_ESP,
	NPC_LT_LE_GTPU = 4,
	NPC_LT_LE_VXLANGPE,
	NPC_LT_LE_GTPC,
	NPC_LT_LE_NSH,
	NPC_LT_LE_TU_MPLS_IN_GRE,
	NPC_LT_LE_TU_NSH_IN_GRE,
	NPC_LT_LE_TU_MPLS_IN_UDP,
	NPC_LT_LE_CUSTOM0 = 0xE,
	NPC_LT_LE_CUSTOM1 = 0xF,
};

#endif

enum npc_kpu_lf_ltype {
	NPC_LT_LF_TU_ETHER = 1,
	NPC_LT_LF_TU_PPP,
	NPC_LT_LF_TU_MPLS_IN_VXLANGPE,
	NPC_LT_LF_TU_NSH_IN_VXLANGPE,
	NPC_LT_LF_TU_MPLS_IN_NSH,
	NPC_LT_LF_TU_3RD_NSH,
	NPC_LT_LF_CUSTOM0 = 0xE,
	NPC_LT_LF_CUSTOM1 = 0xF,
};

enum npc_kpu_lg_ltype {
	NPC_LT_LG_TU_IP = 1,
	NPC_LT_LG_TU_IP6,
	NPC_LT_LG_TU_ARP,
	NPC_LT_LG_TU_ETHER_IN_NSH,
	NPC_LT_LG_CUSTOM0 = 0xE,
	NPC_LT_LG_CUSTOM1 = 0xF,
};

/* Don't modify Ltypes up to SCTP, otherwise it will
 * effect flow tag calculation and thus RSS.
 */
enum npc_kpu_lh_ltype {
	NPC_LT_LH_TU_TCP = 1,
	NPC_LT_LH_TU_UDP,
	NPC_LT_LH_TU_ICMP,
	NPC_LT_LH_TU_SCTP,
	NPC_LT_LH_TU_ICMP6,
	NPC_LT_LH_TU_IGMP = 8,
	NPC_LT_LH_TU_ESP,
	NPC_LT_LH_TU_AH,
	NPC_LT_LH_CUSTOM0 = 0xE,
	NPC_LT_LH_CUSTOM1 = 0xF,
};

enum npc_kpu_lb_uflag {
	NPC_F_LB_U_UNK_ETYPE = 0x80,
	NPC_F_LB_U_MORE_TAG = 0x40,
};

enum npc_kpu_lb_lflag {
	NPC_F_LB_L_WITH_CTAG = 1,
	NPC_F_LB_L_WITH_CTAG_UNK,
	NPC_F_LB_L_WITH_STAG_CTAG,
	NPC_F_LB_L_WITH_STAG_STAG,
	NPC_F_LB_L_WITH_QINQ_CTAG,
	NPC_F_LB_L_WITH_QINQ_QINQ,
	NPC_F_LB_L_WITH_ITAG,
	NPC_F_LB_L_WITH_ITAG_STAG,
	NPC_F_LB_L_WITH_ITAG_CTAG,
	NPC_F_LB_L_WITH_ITAG_UNK,
	NPC_F_LB_L_WITH_BTAG_ITAG,
	NPC_F_LB_L_WITH_STAG,
	NPC_F_LB_L_WITH_QINQ,
	NPC_F_LB_L_DSA,
	NPC_F_LB_L_DSA_VLAN,
	NPC_F_LB_L_EDSA,
	NPC_F_LB_L_EDSA_VLAN,
	NPC_F_LB_L_EXDSA,
	NPC_F_LB_L_EXDSA_VLAN,
	NPC_F_LB_L_FDSA,
};

enum npc_kpu_lc_uflag {
	NPC_F_LC_U_UNK_PROTO = 0x10,
	NPC_F_LC_U_IP_FRAG = 0x20,
	NPC_F_LC_U_IP6_FRAG = 0x40,
};

enum npc_kpu_lc_lflag {
	NPC_F_LC_L_IP_IN_IP = 1,
	NPC_F_LC_L_6TO4,
	NPC_F_LC_L_MPLS_IN_IP,
	NPC_F_LC_L_IP6_TUN_IP6,
	NPC_F_LC_L_IP6_MPLS_IN_IP,
	NPC_F_LC_L_MPLS_4_LABELS,
	NPC_F_LC_L_MPLS_3_LABELS,
	NPC_F_LC_L_MPLS_2_LABELS,
	NPC_F_LC_L_EXT_HOP,
	NPC_F_LC_L_EXT_DEST,
	NPC_F_LC_L_EXT_ROUT,
	NPC_F_LC_L_EXT_MOBILITY,
	NPC_F_LC_L_EXT_HOSTID,
	NPC_F_LC_L_EXT_SHIM6,
};

/* Structures definitions */
struct npc_kpu_profile_cam {
	uint8_t state;
	uint8_t state_mask;
	uint16_t dp0;
	uint16_t dp0_mask;
	uint16_t dp1;
	uint16_t dp1_mask;
	uint16_t dp2;
	uint16_t dp2_mask;
};

struct npc_kpu_profile_action {
	uint8_t errlev;
	uint8_t errcode;
	uint8_t dp0_offset;
	uint8_t dp1_offset;
	uint8_t dp2_offset;
	uint8_t bypass_count;
	uint8_t parse_done;
	uint8_t next_state;
	uint8_t ptr_advance;
	uint8_t cap_ena;
	uint8_t lid;
	uint8_t ltype;
	uint8_t flags;
	uint8_t offset;
	uint8_t mask;
	uint8_t right;
	uint8_t shift;
};

struct npc_kpu_profile {
	int cam_entries;
	int action_entries;
	struct npc_kpu_profile_cam *cam;
	struct npc_kpu_profile_action *action;
};

/* NPC KPU register formats */
struct npc_kpu_cam {
	uint64_t dp0_data : 16;
	uint64_t dp1_data : 16;
	uint64_t dp2_data : 16;
	uint64_t state : 8;
	uint64_t rsvd_63_56 : 8;
};

struct npc_kpu_action0 {
	uint64_t var_len_shift : 3;
	uint64_t var_len_right : 1;
	uint64_t var_len_mask : 8;
	uint64_t var_len_offset : 8;
	uint64_t ptr_advance : 8;
	uint64_t capture_flags : 8;
	uint64_t capture_ltype : 4;
	uint64_t capture_lid : 3;
	uint64_t rsvd_43 : 1;
	uint64_t next_state : 8;
	uint64_t parse_done : 1;
	uint64_t capture_ena : 1;
	uint64_t byp_count : 3;
	uint64_t rsvd_63_57 : 7;
};

struct npc_kpu_action1 {
	uint64_t dp0_offset : 8;
	uint64_t dp1_offset : 8;
	uint64_t dp2_offset : 8;
	uint64_t errcode : 8;
	uint64_t errlev : 4;
	uint64_t rsvd_63_36 : 28;
};

struct npc_kpu_pkind_cpi_def {
	uint64_t cpi_base : 10;
	uint64_t rsvd_11_10 : 2;
	uint64_t add_shift : 3;
	uint64_t rsvd_15 : 1;
	uint64_t add_mask : 8;
	uint64_t add_offset : 8;
	uint64_t flags_mask : 8;
	uint64_t flags_match : 8;
	uint64_t ltype_mask : 4;
	uint64_t ltype_match : 4;
	uint64_t lid : 3;
	uint64_t rsvd_62_59 : 4;
	uint64_t ena : 1;
};

struct nix_rx_action {
	uint64_t op : 4;
	uint64_t pf_func : 16;
	uint64_t index : 20;
	uint64_t match_id : 16;
	uint64_t flow_key_alg : 5;
	uint64_t rsvd_63_61 : 3;
};

struct nix_tx_action {
	uint64_t op : 4;
	uint64_t rsvd_11_4 : 8;
	uint64_t index : 20;
	uint64_t match_id : 16;
	uint64_t rsvd_63_48 : 16;
};

/* NPC layer parse information structure */
struct npc_layer_info_s {
	uint32_t lptr : 8;
	uint32_t flags : 8;
	uint32_t ltype : 4;
	uint32_t rsvd_31_20 : 12;
};

/* NPC layer mcam search key extract structure */
struct npc_layer_kex_s {
	uint16_t flags : 8;
	uint16_t ltype : 4;
	uint16_t rsvd_15_12 : 4;
};

/* NPC mcam search key x1 structure */
struct npc_mcam_key_x1_s {
	uint64_t intf : 2;
	uint64_t rsvd_63_2 : 62;
	uint64_t kw0 : 64; /* W1 */
	uint64_t kw1 : 48;
	uint64_t rsvd_191_176 : 16;
};

/* NPC mcam search key x2 structure */
struct npc_mcam_key_x2_s {
	uint64_t intf : 2;
	uint64_t rsvd_63_2 : 62;
	uint64_t kw0 : 64; /* W1 */
	uint64_t kw1 : 64; /* W2 */
	uint64_t kw2 : 64; /* W3 */
	uint64_t kw3 : 32;
	uint64_t rsvd_319_288 : 32;
};

/* NPC mcam search key x4 structure */
struct npc_mcam_key_x4_s {
	uint64_t intf : 2;
	uint64_t rsvd_63_2 : 62;
	uint64_t kw0 : 64; /* W1 */
	uint64_t kw1 : 64; /* W2 */
	uint64_t kw2 : 64; /* W3 */
	uint64_t kw3 : 64; /* W4 */
	uint64_t kw4 : 64; /* W5 */
	uint64_t kw5 : 64; /* W6 */
	uint64_t kw6 : 64; /* W7 */
};

/* NPC parse key extract structure */
struct npc_parse_kex_s {
	uint64_t chan : 12;
	uint64_t errlev : 4;
	uint64_t errcode : 8;
	uint64_t l2m : 1;
	uint64_t l2b : 1;
	uint64_t l3m : 1;
	uint64_t l3b : 1;
	uint64_t la : 12;
	uint64_t lb : 12;
	uint64_t lc : 12;
	uint64_t ld : 12;
	uint64_t le : 12;
	uint64_t lf : 12;
	uint64_t lg : 12;
	uint64_t lh : 12;
	uint64_t rsvd_127_124 : 4;
};

/* NPC result structure */
struct npc_result_s {
	uint64_t intf : 2;
	uint64_t pkind : 6;
	uint64_t chan : 12;
	uint64_t errlev : 4;
	uint64_t errcode : 8;
	uint64_t l2m : 1;
	uint64_t l2b : 1;
	uint64_t l3m : 1;
	uint64_t l3b : 1;
	uint64_t eoh_ptr : 8;
	uint64_t rsvd_63_44 : 20;
	uint64_t action : 64;	   /* W1 */
	uint64_t vtag_action : 64; /* W2 */
	uint64_t la : 20;
	uint64_t lb : 20;
	uint64_t lc : 20;
	uint64_t rsvd_255_252 : 4;
	uint64_t ld : 20;
	uint64_t le : 20;
	uint64_t lf : 20;
	uint64_t rsvd_319_316 : 4;
	uint64_t lg : 20;
	uint64_t lh : 20;
	uint64_t rsvd_383_360 : 24;
};

#endif /* __NPC_HW_H__ */
