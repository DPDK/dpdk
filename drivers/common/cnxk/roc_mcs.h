/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef ROC_MCS_H
#define ROC_MCS_H

#define MCS_AES_GCM_256_KEYLEN 32

#define ROC_MCS_MAX_AR_WINSZ BIT_ULL(31)
#define ROC_MCS_MAX_MTU      (BIT_ULL(16) - 1)

struct roc_mcs_alloc_rsrc_req {
	uint8_t rsrc_type;
	uint8_t rsrc_cnt; /* Resources count */
	uint8_t dir;	  /* Macsec ingress or egress side */
	uint8_t all;	  /* Allocate all resource type one each */
};

struct roc_mcs_alloc_rsrc_rsp {
	uint8_t flow_ids[128]; /* Index of reserved entries */
	uint8_t secy_ids[128];
	uint8_t sc_ids[128];
	uint8_t sa_ids[256];
	uint8_t rsrc_type;
	uint8_t rsrc_cnt; /* No of entries reserved */
	uint8_t dir;
	uint8_t all;
};

struct roc_mcs_free_rsrc_req {
	uint8_t rsrc_id; /* Index of the entry to be freed */
	uint8_t rsrc_type;
	uint8_t dir;
	uint8_t all; /* Free all the cam resources */
};

struct roc_mcs_flowid_entry_write_req {
	uint64_t data[4];
	uint64_t mask[4];
	uint64_t sci; /* 105N for tx_secy_mem_map */
	uint8_t flow_id;
	uint8_t secy_id; /* secyid for which flowid is mapped */
	uint8_t sc_id;	 /* Valid if dir = MCS_TX, SC_CAM id mapped to flowid */
	uint8_t ena;	 /* Enable tcam entry */
	uint8_t ctr_pkt;
	uint8_t dir;
};

struct roc_mcs_secy_plcy_write_req {
	uint64_t plcy;
	uint8_t secy_id;
	uint8_t dir;
};

/* RX SC_CAM mapping */
struct roc_mcs_rx_sc_cam_write_req {
	uint64_t sci;	  /* SCI */
	uint64_t secy_id; /* secy index mapped to SC */
	uint8_t sc_id;	  /* SC CAM entry index */
};

struct roc_mcs_sa_plcy_write_req {
	uint64_t plcy[2][9];
	uint8_t sa_index[2];
	uint8_t sa_cnt;
	uint8_t dir;
};

struct roc_mcs_tx_sc_sa_map {
	uint8_t sa_index0;
	uint8_t sa_index1;
	uint8_t rekey_ena;
	uint8_t sa_index0_vld;
	uint8_t sa_index1_vld;
	uint8_t tx_sa_active;
	uint64_t sectag_sci;
	uint8_t sc_id; /* used as index for SA_MEM_MAP */
};

struct roc_mcs_rx_sc_sa_map {
	uint8_t sa_index;
	uint8_t sa_in_use;
	uint8_t sc_id;
	uint8_t an; /* value range 0-3, sc_id + an used as index SA_MEM_MAP */
};

struct roc_mcs_flowid_ena_dis_entry {
	uint8_t flow_id;
	uint8_t ena;
	uint8_t dir;
};

struct roc_mcs_pn_table_write_req {
	uint64_t next_pn;
	uint8_t pn_id;
	uint8_t dir;
};

struct roc_mcs_cam_entry_read_req {
	uint8_t rsrc_type; /* TCAM/SECY/SC/SA/PN */
	uint8_t rsrc_id;
	uint8_t dir;
};

struct roc_mcs_cam_entry_read_rsp {
	uint64_t reg_val[10];
	uint8_t rsrc_type;
	uint8_t rsrc_id;
	uint8_t dir;
};

struct roc_mcs_hw_info {
	uint8_t num_mcs_blks; /* Number of MCS blocks */
	uint8_t tcam_entries; /* RX/TX Tcam entries per mcs block */
	uint8_t secy_entries; /* RX/TX SECY entries per mcs block */
	uint8_t sc_entries;   /* RX/TX SC CAM entries per mcs block */
	uint16_t sa_entries;  /* PN table entries = SA entries */
	uint64_t rsvd[16];
};

#define ROC_MCS_CPM_RX_SECTAG_V_EQ1_INT		 BIT_ULL(0)
#define ROC_MCS_CPM_RX_SECTAG_E_EQ0_C_EQ1_INT	 BIT_ULL(1)
#define ROC_MCS_CPM_RX_SECTAG_SL_GTE48_INT	 BIT_ULL(2)
#define ROC_MCS_CPM_RX_SECTAG_ES_EQ1_SC_EQ1_INT	 BIT_ULL(3)
#define ROC_MCS_CPM_RX_SECTAG_SC_EQ1_SCB_EQ1_INT BIT_ULL(4)
#define ROC_MCS_CPM_RX_PACKET_XPN_EQ0_INT	 BIT_ULL(5)
#define ROC_MCS_CPM_RX_PN_THRESH_REACHED_INT	 BIT_ULL(6)
#define ROC_MCS_CPM_TX_PACKET_XPN_EQ0_INT	 BIT_ULL(7)
#define ROC_MCS_CPM_TX_PN_THRESH_REACHED_INT	 BIT_ULL(8)
#define ROC_MCS_CPM_TX_SA_NOT_VALID_INT		 BIT_ULL(9)
#define ROC_MCS_BBE_RX_DFIFO_OVERFLOW_INT	 BIT_ULL(10)
#define ROC_MCS_BBE_RX_PLFIFO_OVERFLOW_INT	 BIT_ULL(11)
#define ROC_MCS_BBE_TX_DFIFO_OVERFLOW_INT	 BIT_ULL(12)
#define ROC_MCS_BBE_TX_PLFIFO_OVERFLOW_INT	 BIT_ULL(13)
#define ROC_MCS_PAB_RX_CHAN_OVERFLOW_INT	 BIT_ULL(14)
#define ROC_MCS_PAB_TX_CHAN_OVERFLOW_INT	 BIT_ULL(15)

struct roc_mcs_intr_cfg {
	uint64_t intr_mask; /* Interrupt enable mask */
};

struct roc_mcs_intr_info {
	uint64_t intr_mask;
	int sa_id;
	uint8_t lmac_id;
	uint64_t rsvd;
};

struct roc_mcs_set_lmac_mode {
	uint8_t mode; /* '1' for internal bypass mode (passthrough), '0' for MCS processing */
	uint8_t lmac_id;
	uint64_t rsvd;
};

struct roc_mcs_set_active_lmac {
	uint32_t lmac_bmap;    /* bitmap of active lmac per mcs block */
	uint16_t channel_base; /* MCS channel base */
	uint64_t rsvd;
};

struct roc_mcs_set_pn_threshold {
	uint64_t threshold;
	uint8_t xpn; /* '1' for setting xpn threshold */
	uint8_t dir;
	uint64_t rsvd;
};

enum roc_mcs_ctrl_pkt_rule_type {
	ROC_MCS_CTRL_PKT_RULE_TYPE_ETH,
	ROC_MCS_CTRL_PKT_RULE_TYPE_DA,
	ROC_MCS_CTRL_PKT_RULE_TYPE_RANGE,
	ROC_MCS_CTRL_PKT_RULE_TYPE_COMBO,
	ROC_MCS_CTRL_PKT_RULE_TYPE_MAC,
};

struct roc_mcs_alloc_ctrl_pkt_rule_req {
	uint8_t rule_type;
	uint8_t dir; /* Macsec ingress or egress side */
	uint64_t rsvd;
};

struct roc_mcs_alloc_ctrl_pkt_rule_rsp {
	uint8_t rule_idx;
	uint8_t rule_type;
	uint8_t dir;
	uint64_t rsvd;
};

struct roc_mcs_free_ctrl_pkt_rule_req {
	uint8_t rule_idx;
	uint8_t rule_type;
	uint8_t dir;
	uint8_t all; /* Free all the rule resources */
	uint64_t rsvd;
};

struct roc_mcs_ctrl_pkt_rule_write_req {
	uint64_t data0;
	uint64_t data1;
	uint64_t data2;
	uint8_t rule_idx;
	uint8_t rule_type;
	uint8_t dir;
	uint64_t rsvd;
};

struct roc_mcs_port_cfg_set_req {
	/* Index of custom tag (= cstm_indx[x] in roc_mcs_custom_tag_cfg_get_rsp struct) to use
	 * when TX SECY_PLCY_MEMX[SECTAG_INSERT_MODE] = 0 (relative offset mode)
	 */
	uint8_t cstm_tag_rel_mode_sel;
	/* In ingress path, custom_hdr_enb = 1 when the port is expected to receive pkts
	 * that have 8B custom header before DMAC
	 */
	uint8_t custom_hdr_enb;
	/* Valid fifo skid values are 14,28,56 for 25G,50G,100G respectively
	 * FIFOs need to be configured based on the port_mode, valid only for 105N
	 */
	uint8_t fifo_skid;
	uint8_t port_mode; /* 2'b00 - 25G or less, 2'b01 - 50G, 2'b10 - 100G */
	uint8_t port_id;
	uint64_t rsvd;
};

struct roc_mcs_port_cfg_get_req {
	uint8_t port_id;
	uint64_t rsvd;
};

struct roc_mcs_port_cfg_get_rsp {
	uint8_t cstm_tag_rel_mode_sel;
	uint8_t custom_hdr_enb;
	uint8_t fifo_skid;
	uint8_t port_mode;
	uint8_t port_id;
	uint64_t rsvd;
};

struct roc_mcs_custom_tag_cfg_get_req {
	uint8_t dir;
	uint64_t rsvd;
};

struct roc_mcs_custom_tag_cfg_get_rsp {
	uint16_t cstm_etype[8]; /* EthType/TPID */
	uint8_t cstm_indx[8];	/* Custom tag index used to identify the VLAN etype */
	uint8_t cstm_etype_en;	/* bitmap of enabled custom tags */
	uint8_t dir;
	uint64_t rsvd;
};

struct roc_mcs_port_reset_req {
	uint8_t port_id;
	uint64_t rsvd;
};

struct roc_mcs_stats_req {
	uint8_t id;
	uint8_t dir;
};

struct roc_mcs_flowid_stats {
	uint64_t tcam_hit_cnt;
};

struct roc_mcs_secy_stats {
	uint64_t ctl_pkt_bcast_cnt;
	uint64_t ctl_pkt_mcast_cnt;
	uint64_t ctl_pkt_ucast_cnt;
	uint64_t ctl_octet_cnt;
	uint64_t unctl_pkt_bcast_cnt;
	uint64_t unctl_pkt_mcast_cnt;
	uint64_t unctl_pkt_ucast_cnt;
	uint64_t unctl_octet_cnt;
	/* Valid only for RX */
	uint64_t octet_decrypted_cnt;
	uint64_t octet_validated_cnt;
	uint64_t pkt_port_disabled_cnt;
	uint64_t pkt_badtag_cnt;
	uint64_t pkt_nosa_cnt;
	uint64_t pkt_nosaerror_cnt;
	uint64_t pkt_tagged_ctl_cnt;
	uint64_t pkt_untaged_cnt;
	uint64_t pkt_ctl_cnt;	/* CN10K-B */
	uint64_t pkt_notag_cnt; /* CNF10K-B */
	/* Valid only for TX */
	uint64_t octet_encrypted_cnt;
	uint64_t octet_protected_cnt;
	uint64_t pkt_noactivesa_cnt;
	uint64_t pkt_toolong_cnt;
	uint64_t pkt_untagged_cnt;
};

struct roc_mcs_sc_stats {
	/* RX */
	uint64_t hit_cnt;
	uint64_t pkt_invalid_cnt;
	uint64_t pkt_late_cnt;
	uint64_t pkt_notvalid_cnt;
	uint64_t pkt_unchecked_cnt;
	uint64_t pkt_delay_cnt;	     /* CNF10K-B */
	uint64_t pkt_ok_cnt;	     /* CNF10K-B */
	uint64_t octet_decrypt_cnt;  /* CN10K-B */
	uint64_t octet_validate_cnt; /* CN10K-B */
	/* TX */
	uint64_t pkt_encrypt_cnt;
	uint64_t pkt_protected_cnt;
	uint64_t octet_encrypt_cnt;   /* CN10K-B */
	uint64_t octet_protected_cnt; /* CN10K-B */
};

struct roc_mcs_port_stats {
	uint64_t tcam_miss_cnt;
	uint64_t parser_err_cnt;
	uint64_t preempt_err_cnt; /* CNF10K-B */
	uint64_t sectag_insert_err_cnt;
};

struct roc_mcs_clear_stats {
	uint8_t type; /* FLOWID, SECY, SC, SA, PORT */
	/* type = PORT, If id = FF(invalid) port no is derived from pcifunc */
	uint8_t id;
	uint8_t dir;
	uint8_t all; /* All resources stats mapped to PF are cleared */
};

enum roc_mcs_event_subtype {
	ROC_MCS_SUBEVENT_UNKNOWN,

	/* subevents of ROC_MCS_EVENT_SECTAG_VAL_ERR sectag validation events
	 * ROC_MCS_EVENT_RX_SECTAG_V_EQ1
	 *	Validation check: SecTag.TCI.V = 1
	 * ROC_MCS_EVENT_RX_SECTAG_E_EQ0_C_EQ1
	 *	Validation check: SecTag.TCI.E = 0 && SecTag.TCI.C = 1
	 * ROC_MCS_EVENT_RX_SECTAG_SL_GTE48
	 *	Validation check: SecTag.SL >= 'd48
	 * ROC_MCS_EVENT_RX_SECTAG_ES_EQ1_SC_EQ1
	 *	Validation check: SecTag.TCI.ES = 1 && SecTag.TCI.SC = 1
	 * ROC_MCS_EVENT_RX_SECTAG_SC_EQ1_SCB_EQ1
	 *	Validation check: SecTag.TCI.SC = 1 && SecTag.TCI.SCB = 1
	 */
	ROC_MCS_EVENT_RX_SECTAG_V_EQ1,
	ROC_MCS_EVENT_RX_SECTAG_E_EQ0_C_EQ1,
	ROC_MCS_EVENT_RX_SECTAG_SL_GTE48,
	ROC_MCS_EVENT_RX_SECTAG_ES_EQ1_SC_EQ1,
	ROC_MCS_EVENT_RX_SECTAG_SC_EQ1_SCB_EQ1,

	/* subevents of ROC_MCS_EVENT_FIFO_OVERFLOW error event
	 * ROC_MCS_EVENT_DATA_FIFO_OVERFLOW:
	 *	Notifies data FIFO overflow fatal error in BBE unit.
	 * ROC_MCS_EVENT_POLICY_FIFO_OVERFLOW
	 *	Notifies policy FIFO overflow fatal error in BBE unit.
	 * ROC_MCS_EVENT_PKT_ASSM_FIFO_OVERFLOW,
	 *	Notifies output FIFO overflow fatal error in PAB unit.
	 */
	ROC_MCS_EVENT_DATA_FIFO_OVERFLOW,
	ROC_MCS_EVENT_POLICY_FIFO_OVERFLOW,
	ROC_MCS_EVENT_PKT_ASSM_FIFO_OVERFLOW,
};

enum roc_mcs_event_type {
	ROC_MCS_EVENT_UNKNOWN,

	/* Notifies BBE_INT_DFIFO/PLFIFO_OVERFLOW or PAB_INT_OVERFLOW
	 * interrupts, it's a fatal error that causes packet corruption.
	 */
	ROC_MCS_EVENT_FIFO_OVERFLOW,

	/* Notifies CPM_RX_SECTAG_X validation error interrupt */
	ROC_MCS_EVENT_SECTAG_VAL_ERR,
	/* Notifies CPM_RX_PACKET_XPN_EQ0 (SecTag.PN == 0 in ingress) interrupt */
	ROC_MCS_EVENT_RX_SA_PN_HARD_EXP,
	/* Notifies CPM_RX_PN_THRESH_REACHED interrupt */
	ROC_MCS_EVENT_RX_SA_PN_SOFT_EXP,
	/* Notifies CPM_TX_PACKET_XPN_EQ0 (PN wrapped in egress) interrupt */
	ROC_MCS_EVENT_TX_SA_PN_HARD_EXP,
	/* Notifies CPM_TX_PN_THRESH_REACHED interrupt */
	ROC_MCS_EVENT_TX_SA_PN_SOFT_EXP,
	/* Notifies CPM_TX_SA_NOT_VALID interrupt */
	ROC_MCS_EVENT_SA_NOT_VALID,
	/* Notifies recovery of software driven port reset */
	ROC_MCS_EVENT_PORT_RESET_RECOVERY,
};

union roc_mcs_event_data {
	/* Valid for below events
	 * - ROC_MCS_EVENT_RX_SA_PN_SOFT_EXP
	 * - ROC_MCS_EVENT_TX_SA_PN_SOFT_EXP
	 */
	struct {
		uint8_t secy_idx;
		uint8_t sc_idx;
		uint8_t sa_idx;
	};
	/* Valid for below event
	 * - ROC_MCS_EVENT_FIFO_OVERFLOW
	 *
	 * Upon fatal error notification on a MCS port, ROC driver resets below attributes of active
	 * flow entities(sc & sa) and than resets the port.
	 * - Reset NEXT_PN of active SAs to 1.
	 * - Reset TX active SA for each SC, TX_SA_ACTIVE = 0, SA_INDEX0_VLD = 1.
	 * - Clear SA_IN_USE for active ANs in RX_SA_MAP_MEM.
	 * - Clear all stats mapping to this port.
	 * - Reactivate SA_IN_USE for active ANs in RX_SA_MAP_MEM.
	 *
	 *  ROC driver notifies the following flow entity(sc & sa) details in application callback,
	 *  application is expected to exchange the Tx/Rx NEXT_PN, TX_SA_ACTIVE, active RX SC AN
	 *  details with peer device so that peer device can resets it's MACsec flow states and than
	 *  resume packet transfers.
	 */
	struct {
		uint16_t *tx_sa_array; /* Tx SAs whose PN memories were reset (NEXT_PN=1) */
		uint16_t *rx_sa_array; /* Rx SAs whose PN memories were reset (NEXT_PN=1) */
		uint16_t *tx_sc_array; /* Tx SCs whose active SAs were reset (TX_SA_ACTIVE=0) */
		uint16_t *rx_sc_array; /* Rx SCs whose state was reset */
		uint8_t *sc_an_array;  /* AN of Rx SCs(in rx_sc_array) which were reactivated */
		uint8_t num_tx_sa;     /* num entries in tx_sa_array */
		uint8_t num_rx_sa;     /* num entries in rx_sa_array */
		uint8_t num_tx_sc;     /* num entries in tx_sc_array */
		uint8_t num_rx_sc;     /* num entries in rx_sc_array */
		uint8_t lmac_id;       /* lmac_id/port which was recovered from fatal error */
	};
};

struct roc_mcs_event_desc {
	enum roc_mcs_event_type type;
	enum roc_mcs_event_subtype subtype;
	union roc_mcs_event_data metadata;
};

struct roc_mcs_fips_req {
	uint8_t dir;
};

struct roc_mcs_fips_mode {
	uint64_t mode;
	uint8_t dir;
};

struct roc_mcs_fips_ctl {
	uint64_t ctl;
	uint8_t dir;
};

struct roc_mcs_fips_iv {
	uint32_t iv_bits95_64;
	uint64_t iv_bits63_0;
	uint8_t dir;
};

struct roc_mcs_fips_ctr {
	uint32_t fips_ctr;
	uint8_t dir;
};

struct roc_mcs_fips_key {
	uint64_t sak_bits255_192;
	uint64_t sak_bits191_128;
	uint64_t sak_bits127_64;
	uint64_t sak_bits63_0;
	uint64_t hashkey_bits127_64;
	uint64_t hashkey_bits63_0;
	uint8_t sak_len;
	uint8_t dir;
};

struct roc_mcs_fips_block {
	uint64_t blk_bits127_64;
	uint64_t blk_bits63_0;
	uint8_t dir;
};

struct roc_mcs_fips_result_rsp {
	uint64_t blk_bits127_64;
	uint64_t blk_bits63_0;
	uint64_t icv_bits127_64;
	uint64_t icv_bits63_0;
	uint8_t result_pass;
};

/** User application callback to be registered for any notifications from driver. */
typedef int (*roc_mcs_dev_cb_fn)(void *userdata, struct roc_mcs_event_desc *desc, void *cb_arg,
				 uint8_t port_id);

struct roc_mcs {
	TAILQ_ENTRY(roc_mcs) next;
	struct plt_pci_device *pci_dev;
	struct mbox *mbox;
	uint8_t idx;
	uint8_t refcount;
	bool intr_cfg_once;
	uint8_t *sa_port_map;

#define ROC_MCS_MEM_SZ (1 * 1024)
	uint8_t reserved[ROC_MCS_MEM_SZ] __plt_cache_aligned;
} __plt_cache_aligned;

TAILQ_HEAD(roc_mcs_head, roc_mcs);

/* Initialization */
__roc_api struct roc_mcs *roc_mcs_dev_init(uint8_t mcs_idx);
__roc_api void roc_mcs_dev_fini(struct roc_mcs *mcs);
/* Get roc mcs dev structure */
__roc_api struct roc_mcs *roc_mcs_dev_get(uint8_t mcs_idx);
/* HW info get */
__roc_api int roc_mcs_hw_info_get(struct roc_mcs_hw_info *hw_info);
/* Active lmac bmap set */
__roc_api int roc_mcs_active_lmac_set(struct roc_mcs *mcs, struct roc_mcs_set_active_lmac *lmac);
/* Port bypass mode set */
__roc_api int roc_mcs_lmac_mode_set(struct roc_mcs *mcs, struct roc_mcs_set_lmac_mode *port);
/* (X)PN threshold set */
__roc_api int roc_mcs_pn_threshold_set(struct roc_mcs *mcs, struct roc_mcs_set_pn_threshold *pn);
/* Reset port */
__roc_api int roc_mcs_port_reset(struct roc_mcs *mcs, struct roc_mcs_port_reset_req *port);
/* Get port config */
__roc_api int roc_mcs_port_cfg_set(struct roc_mcs *mcs, struct roc_mcs_port_cfg_set_req *req);
/* Set port config */
__roc_api int roc_mcs_port_cfg_get(struct roc_mcs *mcs, struct roc_mcs_port_cfg_get_req *req,
				   struct roc_mcs_port_cfg_get_rsp *rsp);
/* Get custom tag config */
__roc_api int roc_mcs_custom_tag_cfg_get(struct roc_mcs *mcs,
					 struct roc_mcs_custom_tag_cfg_get_req *req,
					 struct roc_mcs_custom_tag_cfg_get_rsp *rsp);

/* Resource allocation and free */
__roc_api int roc_mcs_rsrc_alloc(struct roc_mcs *mcs, struct roc_mcs_alloc_rsrc_req *req,
				 struct roc_mcs_alloc_rsrc_rsp *rsp);
__roc_api int roc_mcs_rsrc_free(struct roc_mcs *mcs, struct roc_mcs_free_rsrc_req *req);
/* SA policy read and write */
__roc_api int roc_mcs_sa_policy_write(struct roc_mcs *mcs,
				      struct roc_mcs_sa_plcy_write_req *sa_plcy);
__roc_api int roc_mcs_sa_policy_read(struct roc_mcs *mcs,
				     struct roc_mcs_sa_plcy_write_req *sa_plcy);

/* PN Table read and write */
__roc_api int roc_mcs_pn_table_write(struct roc_mcs *mcs,
				     struct roc_mcs_pn_table_write_req *pn_table);
__roc_api int roc_mcs_pn_table_read(struct roc_mcs *mcs,
				    struct roc_mcs_pn_table_write_req *pn_table);

/* RX SC read, write and enable */
__roc_api int roc_mcs_rx_sc_cam_write(struct roc_mcs *mcs,
				      struct roc_mcs_rx_sc_cam_write_req *rx_sc_cam);
__roc_api int roc_mcs_rx_sc_cam_read(struct roc_mcs *mcs,
				     struct roc_mcs_rx_sc_cam_write_req *rx_sc_cam);
__roc_api int roc_mcs_rx_sc_cam_enable(struct roc_mcs *mcs,
				       struct roc_mcs_rx_sc_cam_write_req *rx_sc_cam);
/* SECY policy read and write */
__roc_api int roc_mcs_secy_policy_write(struct roc_mcs *mcs,
					struct roc_mcs_secy_plcy_write_req *secy_plcy);
__roc_api int roc_mcs_secy_policy_read(struct roc_mcs *mcs,
				       struct roc_mcs_rx_sc_cam_write_req *rx_sc_cam);
/* RX SC-SA MAP read and write */
__roc_api int roc_mcs_rx_sc_sa_map_write(struct roc_mcs *mcs,
					 struct roc_mcs_rx_sc_sa_map *rx_sc_sa_map);
__roc_api int roc_mcs_rx_sc_sa_map_read(struct roc_mcs *mcs,
					struct roc_mcs_rx_sc_sa_map *rx_sc_sa_map);
/* TX SC-SA MAP read and write */
__roc_api int roc_mcs_tx_sc_sa_map_write(struct roc_mcs *mcs,
					 struct roc_mcs_tx_sc_sa_map *tx_sc_sa_map);
__roc_api int roc_mcs_tx_sc_sa_map_read(struct roc_mcs *mcs,
					struct roc_mcs_tx_sc_sa_map *tx_sc_sa_map);
/* SA to Port map update */
__roc_api void roc_mcs_sa_port_map_update(struct roc_mcs *mcs, int sa_id, uint8_t port_id);

/* Flow entry read, write and enable */
__roc_api int roc_mcs_flowid_entry_write(struct roc_mcs *mcs,
					 struct roc_mcs_flowid_entry_write_req *flowid_req);
__roc_api int roc_mcs_flowid_entry_read(struct roc_mcs *mcs,
					struct roc_mcs_flowid_entry_write_req *flowid_rsp);
__roc_api int roc_mcs_flowid_entry_enable(struct roc_mcs *mcs,
					  struct roc_mcs_flowid_ena_dis_entry *entry);

/* Control packet rule alloc, free and write */
__roc_api int roc_mcs_ctrl_pkt_rule_alloc(struct roc_mcs *mcs,
					  struct roc_mcs_alloc_ctrl_pkt_rule_req *req,
					  struct roc_mcs_alloc_ctrl_pkt_rule_rsp *rsp);
__roc_api int roc_mcs_ctrl_pkt_rule_free(struct roc_mcs *mcs,
					 struct roc_mcs_free_ctrl_pkt_rule_req *req);
__roc_api int roc_mcs_ctrl_pkt_rule_write(struct roc_mcs *mcs,
					  struct roc_mcs_ctrl_pkt_rule_write_req *req);

/* Flow id stats get */
__roc_api int roc_mcs_flowid_stats_get(struct roc_mcs *mcs, struct roc_mcs_stats_req *mcs_req,
				       struct roc_mcs_flowid_stats *stats);
/* Secy stats get */
__roc_api int roc_mcs_secy_stats_get(struct roc_mcs *mcs, struct roc_mcs_stats_req *mcs_req,
				     struct roc_mcs_secy_stats *stats);
/* SC stats get */
__roc_api int roc_mcs_sc_stats_get(struct roc_mcs *mcs, struct roc_mcs_stats_req *mcs_req,
				   struct roc_mcs_sc_stats *stats);
/* Port stats get */
__roc_api int roc_mcs_port_stats_get(struct roc_mcs *mcs, struct roc_mcs_stats_req *mcs_req,
				     struct roc_mcs_port_stats *stats);
/* Clear stats */
__roc_api int roc_mcs_stats_clear(struct roc_mcs *mcs, struct roc_mcs_clear_stats *mcs_req);

/* Register user callback routines */
__roc_api int roc_mcs_event_cb_register(struct roc_mcs *mcs, enum roc_mcs_event_type event,
					roc_mcs_dev_cb_fn cb_fn, void *cb_arg, void *userdata);
/* Unregister user callback routines */
__roc_api int roc_mcs_event_cb_unregister(struct roc_mcs *mcs, enum roc_mcs_event_type event);

/* Configure interrupts */
__roc_api int roc_mcs_intr_configure(struct roc_mcs *mcs, struct roc_mcs_intr_cfg *config);

/* Port recovery from fatal errors */
__roc_api int roc_mcs_port_recovery(struct roc_mcs *mcs, union roc_mcs_event_data *mdata,
				    uint8_t port_id);

/* FIPS reset */
__roc_api int roc_mcs_fips_reset(struct roc_mcs *mcs, struct roc_mcs_fips_req *req);
/* FIPS mode set */
__roc_api int roc_mcs_fips_mode_set(struct roc_mcs *mcs, struct roc_mcs_fips_mode *req);
/* FIPS ctl set */
__roc_api int roc_mcs_fips_ctl_set(struct roc_mcs *mcs, struct roc_mcs_fips_ctl *req);
/* FIPS iv set */
__roc_api int roc_mcs_fips_iv_set(struct roc_mcs *mcs, struct roc_mcs_fips_iv *req);
/* FIPS ctr set */
__roc_api int roc_mcs_fips_ctr_set(struct roc_mcs *mcs, struct roc_mcs_fips_ctr *req);
/* FIPS key set */
__roc_api int roc_mcs_fips_key_set(struct roc_mcs *mcs, struct roc_mcs_fips_key *req);
/* FIPS block set */
__roc_api int roc_mcs_fips_block_set(struct roc_mcs *mcs, struct roc_mcs_fips_block *req);
/* FIPS start */
__roc_api int roc_mcs_fips_start(struct roc_mcs *mcs, struct roc_mcs_fips_req *req);
/* FIPS result */
__roc_api int roc_mcs_fips_result_get(struct roc_mcs *mcs, struct roc_mcs_fips_req *req,
				      struct roc_mcs_fips_result_rsp *rsp);
#endif /* ROC_MCS_H */
