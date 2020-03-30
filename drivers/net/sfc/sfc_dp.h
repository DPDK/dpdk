/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2017-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_DP_H
#define _SFC_DP_H

#include <stdbool.h>
#include <sys/queue.h>

#include <rte_pci.h>

#include "sfc_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SFC_DIV_ROUND_UP(a, b) \
	__extension__ ({		\
		typeof(a) _a = (a);	\
		typeof(b) _b = (b);	\
					\
		(_a + (_b - 1)) / _b;	\
	})

/**
 * Datapath exception handler to be provided by the control path.
 */
typedef void (sfc_dp_exception_t)(void *ctrl);

enum sfc_dp_type {
	SFC_DP_RX = 0,	/**< Receive datapath */
	SFC_DP_TX,	/**< Transmit datapath */
};


/** Datapath queue run-time information */
struct sfc_dp_queue {
	uint16_t			port_id;
	uint16_t			queue_id;
	struct rte_pci_addr		pci_addr;
};

void sfc_dp_queue_init(struct sfc_dp_queue *dpq,
		       uint16_t port_id, uint16_t queue_id,
		       const struct rte_pci_addr *pci_addr);

/*
 * Helper macro to define datapath logging macros and have uniform
 * logging.
 */
#define SFC_DP_LOG(dp_name, level, dpq, ...) \
	do {								\
		const struct sfc_dp_queue *_dpq = (dpq);		\
		const struct rte_pci_addr *_addr = &(_dpq)->pci_addr;	\
									\
		SFC_GENERIC_LOG(level,					\
			RTE_FMT("%s " PCI_PRI_FMT			\
				" #%" PRIu16 ".%" PRIu16 ": "		\
				RTE_FMT_HEAD(__VA_ARGS__ ,),		\
				dp_name,				\
				_addr->domain, _addr->bus,		\
				_addr->devid, _addr->function,		\
				_dpq->port_id, _dpq->queue_id,		\
				RTE_FMT_TAIL(__VA_ARGS__,)));		\
	} while (0)


/** Datapath definition */
struct sfc_dp {
	TAILQ_ENTRY(sfc_dp)		links;
	const char			*name;
	enum sfc_dp_type		type;
	/* Mask of required hardware/firmware capabilities */
	unsigned int			hw_fw_caps;
#define SFC_DP_HW_FW_CAP_EF10				0x1
#define SFC_DP_HW_FW_CAP_RX_ES_SUPER_BUFFER		0x2
};

/** List of datapath variants */
TAILQ_HEAD(sfc_dp_list, sfc_dp);

/* Check if available HW/FW capabilities are sufficient for the datapath */
static inline bool
sfc_dp_match_hw_fw_caps(const struct sfc_dp *dp, unsigned int avail_caps)
{
	return (dp->hw_fw_caps & avail_caps) == dp->hw_fw_caps;
}

struct sfc_dp *sfc_dp_find_by_name(struct sfc_dp_list *head,
				   enum sfc_dp_type type, const char *name);
struct sfc_dp *sfc_dp_find_by_caps(struct sfc_dp_list *head,
				   enum sfc_dp_type type,
				   unsigned int avail_caps);
int sfc_dp_register(struct sfc_dp_list *head, struct sfc_dp *entry);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_DP_H */
