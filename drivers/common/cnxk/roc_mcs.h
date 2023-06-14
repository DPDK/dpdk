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


struct roc_mcs_sa_plcy_write_req {
	uint64_t plcy[2][9];
	uint8_t sa_index[2];
	uint8_t sa_cnt;
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

#endif /* ROC_MCS_H */
