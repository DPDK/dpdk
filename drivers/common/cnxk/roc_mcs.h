/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef ROC_MCS_H
#define ROC_MCS_H

#define MCS_AES_GCM_256_KEYLEN 32

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

#endif /* ROC_MCS_H */
