/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef _ROC_MCS_PRIV_H_
#define _ROC_MCS_PRIV_H_

#define MAX_PORTS_PER_MCS 4

enum mcs_error_status {
	MCS_ERR_PARAM = -900,
	MCS_ERR_HW_NOTSUP = -901,
	MCS_ERR_DEVICE_NOT_FOUND = -902,
};

#define MCS_SUPPORT_CHECK                                                                          \
	do {                                                                                       \
		if (!(roc_feature_bphy_has_macsec() || roc_feature_nix_has_macsec()))              \
			return MCS_ERR_HW_NOTSUP;                                                  \
	} while (0)

struct mcs_sc_conf {
	struct {
		uint64_t sci;
		uint16_t sa_idx0;
		uint16_t sa_idx1;
		uint8_t rekey_enb;
	} tx;
	struct {
		uint16_t sa_idx;
		uint8_t an;
	} rx;
};

struct mcs_rsrc {
	struct plt_bitmap *tcam_bmap;
	void *tcam_bmap_mem;
	struct plt_bitmap *secy_bmap;
	void *secy_bmap_mem;
	struct plt_bitmap *sc_bmap;
	void *sc_bmap_mem;
	struct plt_bitmap *sa_bmap;
	void *sa_bmap_mem;
	struct mcs_sc_conf *sc_conf;
};

struct mcs_priv {
	struct mcs_rsrc *port_rsrc;
	struct mcs_rsrc dev_rsrc;
	uint64_t default_sci;
	uint32_t lmac_bmap;
	uint8_t num_mcs_blks;
	uint8_t tcam_entries;
	uint8_t secy_entries;
	uint8_t sc_entries;
	uint16_t sa_entries;
};

static inline struct mcs_priv *
roc_mcs_to_mcs_priv(struct roc_mcs *roc_mcs)
{
	return (struct mcs_priv *)&roc_mcs->reserved[0];
}

static inline void *
roc_mcs_to_mcs_cb_list(struct roc_mcs *roc_mcs)
{
	return (void *)((uintptr_t)roc_mcs->reserved + sizeof(struct mcs_priv));
}

int mcs_event_cb_process(struct roc_mcs *mcs, struct roc_mcs_event_desc *desc);

#endif /* _ROC_MCS_PRIV_H_ */
