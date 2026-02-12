/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef _ROC_RVU_LF_PRIV_H_
#define _ROC_RVU_LF_PRIV_H_

enum rvu_err_status {
	RVU_ERR_PARAM = -1,
	RVU_ERR_NO_MEM = -2,
};

struct rvu_lf {
	struct plt_pci_device *pci_dev;
	struct dev dev;
	uint16_t msg_id_from;
	uint16_t msg_id_to;
};

struct rvu_lf_msg {
	struct mbox_msghdr hdr;
	uint8_t data[];
};

static inline struct rvu_lf *
roc_rvu_lf_to_rvu_priv(struct roc_rvu_lf *roc_rvu_lf)
{
	return (struct rvu_lf *)&roc_rvu_lf->reserved[0];
}

static inline struct roc_rvu_lf *
rvu_priv_to_roc_rvu_lf(struct rvu_lf *rvu_lf)
{
	return (struct roc_rvu_lf *)((char *)rvu_lf - offsetof(struct roc_rvu_lf, reserved));
}

#endif /* _ROC_RVU_LF_PRIV_H_ */
