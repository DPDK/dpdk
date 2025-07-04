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
#include <rte_bitmap.h>

#include "ionic_common.h"
#include "ionic_crypto_if.h"
#include "ionic_regs.h"

/* Devargs */
/* NONE */

#define IOCPT_MAX_RING_DESC		32768
#define IOCPT_MIN_RING_DESC		16
#define IOCPT_ADMINQ_LENGTH		16	/* must be a power of two */

#define IOCPT_CRYPTOQ_WAIT		10	/* 1s */

extern int iocpt_logtype;
#define RTE_LOGTYPE_IOCPT iocpt_logtype

#define IOCPT_PRINT(level, ...)						\
	RTE_LOG_LINE_PREFIX(level, IOCPT, "%s(): ", __func__, __VA_ARGS__)

#define IOCPT_PRINT_CALL() IOCPT_PRINT(DEBUG, " >>")

const struct rte_cryptodev_capabilities *iocpt_get_caps(uint64_t flags);

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

/* Queue watchdog */
#define IOCPT_Q_WDOG_SESS_IDX		0
#define IOCPT_Q_WDOG_KEY_LEN		16
#define IOCPT_Q_WDOG_IV_LEN		12
#define IOCPT_Q_WDOG_PLD_LEN		4
#define IOCPT_Q_WDOG_TAG_LEN		16
#define IOCPT_Q_WDOG_OP_TYPE		RTE_CRYPTO_OP_TYPE_UNDEFINED

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

#define IOCPT_Q_F_INITED	BIT(0)
#define IOCPT_Q_F_DEFERRED	BIT(1)
#define IOCPT_Q_F_SG		BIT(2)

#define Q_NEXT_TO_POST(_q, _n)	(((_q)->head_idx + (_n)) & ((_q)->size_mask))
#define Q_NEXT_TO_SRVC(_q, _n)	(((_q)->tail_idx + (_n)) & ((_q)->size_mask))

#define IOCPT_INFO_SZ(_q)	((_q)->num_segs * sizeof(void *))
#define IOCPT_INFO_IDX(_q, _i)	((_i) * (_q)->num_segs)
#define IOCPT_INFO_PTR(_q, _i)	(&(_q)->info[IOCPT_INFO_IDX((_q), _i)])

struct iocpt_queue {
	uint16_t num_descs;
	uint16_t num_segs;
	uint16_t head_idx;
	uint16_t tail_idx;
	uint16_t size_mask;
	uint8_t type;
	uint8_t hw_type;
	void *base;
	void *sg_base;
	struct ionic_doorbell __iomem *db;
	void **info;

	uint32_t index;
	uint32_t hw_index;
	rte_iova_t base_pa;
	rte_iova_t sg_base_pa;
};

struct iocpt_cq {
	uint16_t tail_idx;
	uint16_t num_descs;
	uint16_t size_mask;
	bool done_color;
	void *base;
	rte_iova_t base_pa;
};

#define IOCPT_COMMON_FIELDS				\
	struct iocpt_queue q;				\
	struct iocpt_cq cq;				\
	struct iocpt_dev *dev;				\
	const struct rte_memzone *base_z;		\
	void *base;					\
	rte_iova_t base_pa

struct iocpt_common_q {
	IOCPT_COMMON_FIELDS;
};

struct iocpt_admin_q {
	IOCPT_COMMON_FIELDS;

	uint16_t flags;
};

struct iocpt_crypto_q {
	/* cacheline0, cacheline1 */
	IOCPT_COMMON_FIELDS;

	/* cacheline2 */
	uint64_t last_wdog_cycles;
	uint16_t flags;

	/* cacheline3 */
	struct rte_cryptodev_stats stats;

	uint64_t enqueued_wdogs;
	uint64_t dequeued_wdogs;
	uint8_t wdog_iv[IOCPT_Q_WDOG_IV_LEN];
	uint8_t wdog_pld[IOCPT_Q_WDOG_PLD_LEN];
	uint8_t wdog_tag[IOCPT_Q_WDOG_TAG_LEN];
};

#define IOCPT_S_F_INITED	BIT(0)

struct iocpt_session_priv {
	struct iocpt_dev *dev;

	uint32_t index;

	uint16_t iv_offset;
	uint16_t iv_length;
	uint16_t digest_length;
	uint16_t aad_length;

	uint8_t flags;
	uint8_t op;
	uint8_t type;

	uint16_t key_len;
	uint8_t key[IOCPT_SESS_KEY_LEN_MAX_SYMM];
};

static inline uint32_t
iocpt_session_size(void)
{
	return sizeof(struct iocpt_session_priv);
}

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

	rte_spinlock_t adminq_lock;
	rte_spinlock_t adminq_service_lock;

	struct iocpt_admin_q *adminq;
	struct iocpt_crypto_q **cryptoqs;

	struct rte_bitmap  *sess_bm;	/* SET bit indicates index is free */

	uint64_t features;
	uint32_t hw_features;

	uint32_t info_sz;
	struct iocpt_lif_info *info;
	rte_iova_t info_pa;
	const struct rte_memzone *info_z;

	struct iocpt_qtype_info qtype_info[IOCPT_QTYPE_MAX];
	uint8_t qtype_ver[IOCPT_QTYPE_MAX];

	struct rte_cryptodev_stats stats_base;
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

	return dev->intf->setup_bars(dev);
}

/** iocpt_admin_ctx - Admin command context.
 * @pending_work:	Flag that indicates a completion.
 * @cmd:		Admin command (64B) to be copied to the queue.
 * @comp:		Admin completion (16B) copied from the queue.
 */
struct iocpt_admin_ctx {
	bool pending_work;
	union iocpt_adminq_cmd cmd;
	union iocpt_adminq_comp comp;
};

int iocpt_probe(void *bus_dev, struct rte_device *rte_dev,
	struct iocpt_dev_bars *bars, const struct iocpt_dev_intf *intf,
	uint8_t driver_id, uint8_t socket_id);
int iocpt_remove(struct rte_device *rte_dev);

void iocpt_configure(struct iocpt_dev *dev);
int iocpt_assign_ops(struct rte_cryptodev *cdev);
int iocpt_start(struct iocpt_dev *dev);
void iocpt_stop(struct iocpt_dev *dev);
void iocpt_deinit(struct iocpt_dev *dev);

int iocpt_dev_identify(struct iocpt_dev *dev);
int iocpt_dev_init(struct iocpt_dev *dev, rte_iova_t info_pa);
int iocpt_dev_adminq_init(struct iocpt_dev *dev);
void iocpt_dev_reset(struct iocpt_dev *dev);

int iocpt_adminq_post_wait(struct iocpt_dev *dev, struct iocpt_admin_ctx *ctx);

int iocpt_cryptoq_alloc(struct iocpt_dev *dev, uint32_t socket_id,
	uint32_t index, uint16_t ndescs);
void iocpt_cryptoq_free(struct iocpt_crypto_q *cptq);

int iocpt_session_init(struct iocpt_session_priv *priv);
int iocpt_session_update(struct iocpt_session_priv *priv);
void iocpt_session_deinit(struct iocpt_session_priv *priv);

struct ionic_doorbell __iomem *iocpt_db_map(struct iocpt_dev *dev,
	struct iocpt_queue *q);

typedef bool (*iocpt_cq_cb)(struct iocpt_cq *cq, uint16_t cq_desc_index,
		void *cb_arg);
uint32_t iocpt_cq_service(struct iocpt_cq *cq, uint32_t work_to_do,
	iocpt_cq_cb cb, void *cb_arg);

void iocpt_get_stats(const struct iocpt_dev *dev,
	struct rte_cryptodev_stats *stats);
void iocpt_reset_stats(struct iocpt_dev *dev);

static inline uint16_t
iocpt_q_space_avail(struct iocpt_queue *q)
{
	uint16_t avail = q->tail_idx;

	if (q->head_idx >= avail)
		avail += q->num_descs - q->head_idx - 1;
	else
		avail -= q->head_idx + 1;

	return avail;
}

static inline void
iocpt_q_flush(struct iocpt_queue *q)
{
	uint64_t val = IONIC_DBELL_QID(q->hw_index) | q->head_idx;

#if defined(RTE_LIBRTE_IONIC_PMD_BARRIER_ERRATA)
	/* On some devices the standard 'dmb' barrier is insufficient */
	asm volatile("dsb st" : : : "memory");
	rte_write64_relaxed(rte_cpu_to_le_64(val), q->db);
#else
	rte_write64(rte_cpu_to_le_64(val), q->db);
#endif
}

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
