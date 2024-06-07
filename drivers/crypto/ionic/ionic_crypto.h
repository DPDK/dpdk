/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021-2024 Advanced Micro Devices, Inc.
 */

#ifndef _IONIC_CRYPTO_H_
#define _IONIC_CRYPTO_H_

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include <rte_common.h>
#include <rte_dev.h>
#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_log.h>

#include "ionic_common.h"
#include "ionic_crypto_if.h"
#include "ionic_regs.h"

/* Devargs */
/* NONE */

extern int iocpt_logtype;
#define RTE_LOGTYPE_IOCPT iocpt_logtype

#define IOCPT_PRINT(level, ...)						\
	RTE_LOG_LINE_PREFIX(level, IOCPT, "%s(): ", __func__, __VA_ARGS__)

#define IOCPT_PRINT_CALL() IOCPT_PRINT(DEBUG, " >>")

static inline void iocpt_struct_size_checks(void)
{
	RTE_BUILD_BUG_ON(sizeof(struct ionic_doorbell) != 8);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_intr) != 32);
	RTE_BUILD_BUG_ON(sizeof(struct ionic_intr_status) != 8);

	RTE_BUILD_BUG_ON(sizeof(union iocpt_dev_regs) != 4096);
	RTE_BUILD_BUG_ON(sizeof(union iocpt_dev_info_regs) != 2048);
	RTE_BUILD_BUG_ON(sizeof(union iocpt_dev_cmd_regs) != 2048);

	RTE_BUILD_BUG_ON(sizeof(struct iocpt_admin_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_admin_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_nop_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_nop_comp) != 16);

	/* Device commands */
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_dev_identify_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_dev_identify_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_dev_reset_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_dev_reset_comp) != 16);

	/* LIF commands */
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_lif_identify_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_lif_identify_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_lif_init_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_lif_init_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_lif_reset_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_lif_getattr_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_lif_getattr_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_lif_setattr_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_lif_setattr_comp) != 16);

	/* Queue commands */
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_q_identify_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_q_identify_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_q_init_cmd) != 64);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_q_init_comp) != 16);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_q_control_cmd) != 64);

	/* Crypto */
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_crypto_desc) != 32);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_crypto_sg_desc) != 256);
	RTE_BUILD_BUG_ON(sizeof(struct iocpt_crypto_comp) != 16);
}

struct iocpt_dev_bars {
	struct ionic_dev_bar bar[IONIC_BARS_MAX];
	uint32_t num_bars;
};

struct iocpt_qtype_info {
	uint8_t	 version;
	uint8_t	 supported;
	uint64_t features;
	uint16_t desc_sz;
	uint16_t comp_sz;
	uint16_t sg_desc_sz;
	uint16_t max_sg_elems;
	uint16_t sg_desc_stride;
};

#define IOCPT_DEV_F_INITED		BIT(0)
#define IOCPT_DEV_F_UP			BIT(1)
#define IOCPT_DEV_F_FW_RESET		BIT(2)

/* Combined dev / LIF object */
struct iocpt_dev {
	const char *name;
	char fw_version[IOCPT_FWVERS_BUFLEN];
	struct iocpt_dev_bars bars;
	struct iocpt_identity ident;

	const struct iocpt_dev_intf *intf;
	void *bus_dev;
	struct rte_cryptodev *crypto_dev;

	union iocpt_dev_info_regs __iomem *dev_info;
	union iocpt_dev_cmd_regs __iomem *dev_cmd;

	struct ionic_doorbell __iomem *db_pages;
	struct ionic_intr __iomem *intr_ctrl;

	uint32_t max_qps;
	uint32_t max_sessions;
	uint16_t state;
	uint8_t driver_id;
	uint8_t socket_id;

	uint64_t features;
	uint32_t hw_features;

	uint32_t info_sz;
	struct iocpt_lif_info *info;
	rte_iova_t info_pa;
	const struct rte_memzone *info_z;

	struct iocpt_qtype_info qtype_info[IOCPT_QTYPE_MAX];
	uint8_t qtype_ver[IOCPT_QTYPE_MAX];
};

struct iocpt_dev_intf {
	int  (*setup_bars)(struct iocpt_dev *dev);
	void (*unmap_bars)(struct iocpt_dev *dev);
};

static inline int
iocpt_setup_bars(struct iocpt_dev *dev)
{
	if (dev->intf->setup_bars == NULL)
		return -EINVAL;

	return (*dev->intf->setup_bars)(dev);
}

int iocpt_probe(void *bus_dev, struct rte_device *rte_dev,
	struct iocpt_dev_bars *bars, const struct iocpt_dev_intf *intf,
	uint8_t driver_id, uint8_t socket_id);
int iocpt_remove(struct rte_device *rte_dev);

void iocpt_configure(struct iocpt_dev *dev);
void iocpt_deinit(struct iocpt_dev *dev);

int iocpt_dev_identify(struct iocpt_dev *dev);
int iocpt_dev_init(struct iocpt_dev *dev, rte_iova_t info_pa);
void iocpt_dev_reset(struct iocpt_dev *dev);

static inline bool
iocpt_is_embedded(void)
{
#if defined(RTE_LIBRTE_IONIC_PMD_EMBEDDED)
	return true;
#else
	return false;
#endif
}

#endif /* _IONIC_CRYPTO_H_ */
