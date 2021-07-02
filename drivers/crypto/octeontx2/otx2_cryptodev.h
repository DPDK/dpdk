/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_CRYPTODEV_H_
#define _OTX2_CRYPTODEV_H_

#include "cpt_common.h"
#include "cpt_hw_types.h"

#include "otx2_dev.h"

/* Marvell OCTEON TX2 Crypto PMD device name */
#define CRYPTODEV_NAME_OCTEONTX2_PMD	crypto_octeontx2

#define OTX2_CPT_MAX_LFS		128
#define OTX2_CPT_MAX_QUEUES_PER_VF	64
#define OTX2_CPT_MAX_BLKS		2
#define OTX2_CPT_PMD_VERSION		3
#define OTX2_CPT_REVISION_ID_3		3

/**
 * Device private data
 */
struct otx2_cpt_vf {
	struct otx2_dev otx2_dev;
	/**< Base class */
	uint16_t max_queues;
	/**< Max queues supported */
	uint8_t nb_queues;
	/**< Number of crypto queues attached */
	uint16_t lf_msixoff[OTX2_CPT_MAX_LFS];
	/**< MSI-X offsets */
	uint8_t lf_blkaddr[OTX2_CPT_MAX_LFS];
	/**<  CPT0/1 BLKADDR of LFs */
	uint8_t cpt_revision;
	/**<  CPT revision */
	uint8_t err_intr_registered:1;
	/**< Are error interrupts registered? */
	union cpt_eng_caps hw_caps[CPT_MAX_ENG_TYPES];
	/**< CPT device capabilities */
};

struct cpt_meta_info {
	uint64_t deq_op_info[5];
	uint64_t comp_code_sz;
	union cpt_res_s cpt_res __rte_aligned(16);
	struct cpt_request_info cpt_req;
};

#define CPT_LOGTYPE otx2_cpt_logtype

extern int otx2_cpt_logtype;

/*
 * Crypto device driver ID
 */
extern uint8_t otx2_cryptodev_driver_id;

uint64_t otx2_cpt_default_ff_get(void);
void otx2_cpt_set_enqdeq_fns(struct rte_cryptodev *dev);

#endif /* _OTX2_CRYPTODEV_H_ */
