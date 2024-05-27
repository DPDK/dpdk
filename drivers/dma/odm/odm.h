/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef _ODM_H_
#define _ODM_H_

#include <rte_log.h>

extern int odm_logtype;

#define odm_err(...)                                                                               \
	rte_log(RTE_LOG_ERR, odm_logtype,                                                          \
		RTE_FMT("%s(): %u" RTE_FMT_HEAD(__VA_ARGS__, ), __func__, __LINE__,                \
			RTE_FMT_TAIL(__VA_ARGS__, )))
#define odm_info(...)                                                                              \
	rte_log(RTE_LOG_INFO, odm_logtype,                                                         \
		RTE_FMT("%s(): %u" RTE_FMT_HEAD(__VA_ARGS__, ), __func__, __LINE__,                \
			RTE_FMT_TAIL(__VA_ARGS__, )))

struct __rte_cache_aligned odm_dev {
	struct rte_pci_device *pci_dev;
	uint8_t *rbase;
	uint16_t vfid;
	uint8_t max_qs;
	uint8_t num_qs;
};

#endif /* _ODM_H_ */
