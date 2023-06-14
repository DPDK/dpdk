/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef ROC_MCS_H
#define ROC_MCS_H

#define MCS_AES_GCM_256_KEYLEN 32

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

struct roc_mcs_hw_info {
	uint8_t num_mcs_blks; /* Number of MCS blocks */
	uint8_t tcam_entries; /* RX/TX Tcam entries per mcs block */
	uint8_t secy_entries; /* RX/TX SECY entries per mcs block */
	uint8_t sc_entries;   /* RX/TX SC CAM entries per mcs block */
	uint16_t sa_entries;  /* PN table entries = SA entries */
	uint64_t rsvd[16];
};


struct roc_mcs {
	TAILQ_ENTRY(roc_mcs) next;
	struct plt_pci_device *pci_dev;
	struct mbox *mbox;
	void *userdata;
	uint8_t idx;
	uint8_t refcount;

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

/* Resource allocation and free */
__roc_api int roc_mcs_rsrc_alloc(struct roc_mcs *mcs, struct roc_mcs_alloc_rsrc_req *req,
				 struct roc_mcs_alloc_rsrc_rsp *rsp);
__roc_api int roc_mcs_rsrc_free(struct roc_mcs *mcs, struct roc_mcs_free_rsrc_req *req);
/* SA policy read and write */
__roc_api int roc_mcs_sa_policy_write(struct roc_mcs *mcs,
				      struct roc_mcs_sa_plcy_write_req *sa_plcy);
__roc_api int roc_mcs_sa_policy_read(struct roc_mcs *mcs,
				     struct roc_mcs_sa_plcy_write_req *sa_plcy);

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

/* Flow entry read, write and enable */
__roc_api int roc_mcs_flowid_entry_write(struct roc_mcs *mcs,
					 struct roc_mcs_flowid_entry_write_req *flowid_req);
__roc_api int roc_mcs_flowid_entry_read(struct roc_mcs *mcs,
					struct roc_mcs_flowid_entry_write_req *flowid_rsp);
__roc_api int roc_mcs_flowid_entry_enable(struct roc_mcs *mcs,
					  struct roc_mcs_flowid_ena_dis_entry *entry);

#endif /* ROC_MCS_H */
