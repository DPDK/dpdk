/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef _ODM_H_
#define _ODM_H_

#include <stdint.h>

#include <rte_common.h>
#include <rte_compat.h>
#include <rte_io.h>
#include <rte_log.h>
#include <rte_memzone.h>

/* ODM VF register offsets from VF_BAR0 */
#define ODM_VDMA_EN(x)		(0x00 | (x << 3))
#define ODM_VDMA_REQQ_CTL(x)	(0x80 | (x << 3))
#define ODM_VDMA_DBELL(x)	(0x100 | (x << 3))
#define ODM_VDMA_RING_CFG(x)	(0x180 | (x << 3))
#define ODM_VDMA_IRING_BADDR(x) (0x200 | (x << 3))
#define ODM_VDMA_CRING_BADDR(x) (0x280 | (x << 3))
#define ODM_VDMA_COUNTS(x)	(0x300 | (x << 3))
#define ODM_VDMA_IRING_NADDR(x) (0x380 | (x << 3))
#define ODM_VDMA_CRING_NADDR(x) (0x400 | (x << 3))
#define ODM_VDMA_IRING_DBG(x)	(0x480 | (x << 3))
#define ODM_VDMA_CNT(x)		(0x580 | (x << 3))
#define ODM_VF_INT		(0x1000)
#define ODM_VF_INT_W1S		(0x1008)
#define ODM_VF_INT_ENA_W1C	(0x1010)
#define ODM_VF_INT_ENA_W1S	(0x1018)
#define ODM_MBOX_VF_PF_DATA(i)	(0x2000 | (i << 3))
#define ODM_MBOX_RETRY_CNT	(0xfffffff)
#define ODM_MBOX_ERR_CODE_MAX	(0xf)
#define ODM_IRING_IDLE_WAIT_CNT (0xfffffff)

/*
 * Enumeration odm_hdr_xtype_e
 *
 * ODM Transfer Type Enumeration
 * Enumerates the pointer type in ODM_DMA_INSTR_HDR_S[XTYPE]
 */
#define ODM_XTYPE_INTERNAL 2
#define ODM_XTYPE_FILL0	   4
#define ODM_XTYPE_FILL1	   5

/*
 *  ODM Header completion type enumeration
 *  Enumerates the completion type in ODM_DMA_INSTR_HDR_S[CT]
 */
#define ODM_HDR_CT_CW_CA 0x0
#define ODM_HDR_CT_CW_NC 0x1

#define ODM_MAX_QUEUES_PER_DEV 16

#define ODM_IRING_MAX_SIZE	 (256 * 1024)
#define ODM_IRING_ENTRY_SIZE_MIN 4
#define ODM_IRING_ENTRY_SIZE_MAX 13
#define ODM_IRING_MAX_WORDS	 (ODM_IRING_MAX_SIZE / 8)
#define ODM_IRING_MAX_ENTRY	 (ODM_IRING_MAX_WORDS / ODM_IRING_ENTRY_SIZE_MIN)

#define ODM_MAX_POINTER 4

#define odm_read64(addr)       rte_read64_relaxed((volatile void *)(addr))
#define odm_write64(val, addr) rte_write64_relaxed((val), (volatile void *)(addr))

extern int odm_logtype;
#define RTE_LOGTYPE_ODM odm_logtype

#define ODM_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ODM, "%s(): %u", __func__ RTE_LOG_COMMA __LINE__, __VA_ARGS__)

/*
 * Structure odm_instr_hdr_s for ODM
 *
 * ODM DMA Instruction Header Format
 */
union odm_instr_hdr_s {
	uint64_t u;
	struct odm_instr_hdr {
		uint64_t nfst : 3;
		uint64_t reserved_3 : 1;
		uint64_t nlst : 3;
		uint64_t reserved_7_9 : 3;
		uint64_t ct : 2;
		uint64_t stse : 1;
		uint64_t reserved_13_28 : 16;
		uint64_t sts : 1;
		uint64_t reserved_30_49 : 20;
		uint64_t xtype : 3;
		uint64_t reserved_53_63 : 11;
	} s;
};

/* ODM Completion Entry Structure */
union odm_cmpl_ent_s {
	uint32_t u;
	struct odm_cmpl_ent {
		uint32_t cmp_code : 8;
		uint32_t rsvd : 23;
		uint32_t valid : 1;
	} s;
};

/* ODM DMA Ring Configuration Register */
union odm_vdma_ring_cfg_s {
	uint64_t u;
	struct {
		uint64_t isize : 8;
		uint64_t rsvd_8_15 : 8;
		uint64_t csize : 8;
		uint64_t rsvd_24_63 : 40;
	} s;
};

/* ODM DMA Instruction Ring DBG */
union odm_vdma_iring_dbg_s {
	uint64_t u;
	struct {
		uint64_t dbell_cnt : 32;
		uint64_t offset : 16;
		uint64_t rsvd_48_62 : 15;
		uint64_t iwbusy : 1;
	} s;
};

/* ODM DMA Counts */
union odm_vdma_counts_s {
	uint64_t u;
	struct {
		uint64_t dbell : 32;
		uint64_t buf_used_cnt : 9;
		uint64_t rsvd_41_43 : 3;
		uint64_t rsvd_buf_used_cnt : 3;
		uint64_t rsvd_47_63 : 17;
	} s;
};

struct vq_stats {
	uint64_t submitted;
	uint64_t completed;
	uint64_t errors;
	/*
	 * Since stats.completed is used to return completion index, account for any packets
	 * received before stats is reset.
	 */
	uint64_t completed_offset;
};

struct odm_queue {
	struct odm_dev *dev;
	/* Instructions that are prepared on the iring, but is not pushed to hw yet. */
	uint16_t pending_submit_cnt;
	/* Length (in words) of instructions that are not yet pushed to hw. */
	uint16_t pending_submit_len;
	uint16_t desc_idx;
	/* Instruction ring head. Used for enqueue. */
	uint16_t iring_head;
	/* Completion ring head. Used for dequeue. */
	uint16_t cring_head;
	/* Extra instruction size ring head. Used in enqueue-dequeue.*/
	uint16_t ins_ring_head;
	/* Extra instruction size ring tail. Used in enqueue-dequeue.*/
	uint16_t ins_ring_tail;
	/* Instruction size available.*/
	uint16_t iring_sz_available;
	/* Number of 8-byte words in iring.*/
	uint16_t iring_max_words;
	/* Number of words in cring.*/
	uint16_t cring_max_entry;
	/* Extra instruction size used per inflight instruction.*/
	uint8_t *extra_ins_sz;
	struct vq_stats stats;
	const struct rte_memzone *iring_mz;
	const struct rte_memzone *cring_mz;
};

struct __rte_cache_aligned odm_dev {
	struct rte_pci_device *pci_dev;
	struct odm_queue vq[ODM_MAX_QUEUES_PER_DEV];
	uint8_t *rbase;
	uint16_t vfid;
	uint8_t max_qs;
	uint8_t num_qs;
};

int odm_dev_init(struct odm_dev *odm);
int odm_dev_fini(struct odm_dev *odm);
int odm_configure(struct odm_dev *odm);
int odm_enable(struct odm_dev *odm);
int odm_disable(struct odm_dev *odm);
int odm_vchan_setup(struct odm_dev *odm, int vchan, int nb_desc);

#endif /* _ODM_H_ */
