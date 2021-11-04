/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

 #ifndef _QAT_CRYPTO_H_
 #define _QAT_CRYPTO_H_

#include <rte_cryptodev.h>
#ifdef RTE_LIB_SECURITY
#include <rte_security.h>
#endif

#include "qat_device.h"

extern uint8_t qat_sym_driver_id;
extern uint8_t qat_asym_driver_id;

/** helper macro to set cryptodev capability range **/
#define CAP_RNG(n, l, r, i) .n = {.min = l, .max = r, .increment = i}

#define CAP_RNG_ZERO(n) .n = {.min = 0, .max = 0, .increment = 0}
/** helper macro to set cryptodev capability value **/
#define CAP_SET(n, v) .n = v

/** private data structure for a QAT device.
 * there can be one of these on each qat_pci_device (VF).
 */
struct qat_cryptodev_private {
	struct qat_pci_device *qat_dev;
	/**< The qat pci device hosting the service */
	uint8_t dev_id;
	/**< Device instance for this rte_cryptodev */
	const struct rte_cryptodev_capabilities *qat_dev_capabilities;
	/* QAT device symmetric crypto capabilities */
	const struct rte_memzone *capa_mz;
	/* Shared memzone for storing capabilities */
	uint16_t min_enq_burst_threshold;
	uint32_t internal_capabilities; /* see flags QAT_SYM_CAP_xxx */
	enum qat_service_type service_type;
};

struct qat_capabilities_info {
	struct rte_cryptodev_capabilities *data;
	uint64_t size;
};

int
qat_cryptodev_config(struct rte_cryptodev *dev,
		struct rte_cryptodev_config *config);

int
qat_cryptodev_start(struct rte_cryptodev *dev);

void
qat_cryptodev_stop(struct rte_cryptodev *dev);

int
qat_cryptodev_close(struct rte_cryptodev *dev);

void
qat_cryptodev_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *info);

void
qat_cryptodev_stats_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_stats *stats);

void
qat_cryptodev_stats_reset(struct rte_cryptodev *dev);

int
qat_cryptodev_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
	const struct rte_cryptodev_qp_conf *qp_conf, int socket_id);

int
qat_cryptodev_qp_release(struct rte_cryptodev *dev, uint16_t queue_pair_id);

#endif
