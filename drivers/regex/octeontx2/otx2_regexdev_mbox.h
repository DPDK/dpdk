/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2020 Marvell International Ltd.
 */

#ifndef _OTX2_REGEXDEV_MBOX_H_
#define _OTX2_REGEXDEV_MBOX_H_

#include <rte_regexdev.h>

int otx2_ree_available_queues_get(const struct rte_regexdev *dev,
				  uint16_t *nb_queues);

int otx2_ree_queues_attach(const struct rte_regexdev *dev, uint8_t nb_queues);

int otx2_ree_queues_detach(const struct rte_regexdev *dev);

int otx2_ree_msix_offsets_get(const struct rte_regexdev *dev);

int otx2_ree_config_lf(const struct rte_regexdev *dev, uint8_t lf, uint8_t pri,
		       uint32_t size);

int otx2_ree_af_reg_read(const struct rte_regexdev *dev, uint64_t reg,
			 uint64_t *val);

int otx2_ree_af_reg_write(const struct rte_regexdev *dev, uint64_t reg,
			  uint64_t val);

int otx2_ree_rule_db_get(const struct rte_regexdev *dev, char *rule_db,
		 uint32_t rule_db_len, char *rule_dbi, uint32_t rule_dbi_len);

int otx2_ree_rule_db_len_get(const struct rte_regexdev *dev,
			     uint32_t *rule_db_len, uint32_t *rule_dbi_len);

int otx2_ree_rule_db_prog(const struct rte_regexdev *dev, const char *rule_db,
		uint32_t rule_db_len, const char *rule_dbi,
		uint32_t rule_dbi_len);

#endif /* _OTX2_REGEXDEV_MBOX_H_ */
