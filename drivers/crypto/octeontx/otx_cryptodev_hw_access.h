/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _OTX_CRYPTODEV_HW_ACCESS_H_
#define _OTX_CRYPTODEV_HW_ACCESS_H_

#include <stdbool.h>

#include <rte_memory.h>

#include "cpt_common.h"

#define CPT_INTR_POLL_INTERVAL_MS	(50)

/* Default command queue length */
#define DEFAULT_CMD_QCHUNKS		2

/* cpt instance */
struct cpt_instance {
	uint32_t queue_id;
	uintptr_t rsvd;
};

struct command_chunk {
	/** 128-byte aligned real_vaddr */
	uint8_t *head;
	/** 128-byte aligned real_dma_addr */
	phys_addr_t dma_addr;
};

/**
 * Command queue structure
 */
struct command_queue {
	/** Command queue host write idx */
	uint32_t idx;
	/** Command queue chunk */
	uint32_t cchunk;
	/** Command queue head; instructions are inserted here */
	uint8_t *qhead;
	/** Command chunk list head */
	struct command_chunk chead[DEFAULT_CMD_QCHUNKS];
};

/**
 * CPT VF device structure
 */
struct cpt_vf {
	/** CPT instance */
	struct cpt_instance instance;
	/** Register start address */
	uint8_t *reg_base;
	/** Command queue information */
	struct command_queue cqueue;
	/** Pending queue information */
	struct pending_queue pqueue;
	/** Meta information per vf */
	struct cptvf_meta_info meta_info;

	/** Below fields are accessed only in control path */

	/** Env specific pdev representing the pci dev */
	void *pdev;
	/** Calculated queue size */
	uint32_t qsize;
	/** Device index (0...CPT_MAX_VQ_NUM)*/
	uint8_t  vfid;
	/** VF type of cpt_vf_type_t (SE_TYPE(2) or AE_TYPE(1) */
	uint8_t  vftype;
	/** VF group (0 - 8) */
	uint8_t  vfgrp;
	/** Operating node: Bits (46:44) in BAR0 address */
	uint8_t  node;

	/** VF-PF mailbox communication */

	/** Flag if acked */
	bool pf_acked;
	/** Flag if not acked */
	bool pf_nacked;

	/** Device name */
	char dev_name[32];
} __rte_cache_aligned;

/*
 * CPT Registers map for 81xx
 */

/* VF registers */
#define CPTX_VQX_CTL(a, b)		(0x0000100ll + 0x1000000000ll * \
					 ((a) & 0x0) + 0x100000ll * (b))
#define CPTX_VQX_SADDR(a, b)		(0x0000200ll + 0x1000000000ll * \
					 ((a) & 0x0) + 0x100000ll * (b))
#define CPTX_VQX_DONE_WAIT(a, b)	(0x0000400ll + 0x1000000000ll * \
					 ((a) & 0x0) + 0x100000ll * (b))
#define CPTX_VQX_INPROG(a, b)		(0x0000410ll + 0x1000000000ll * \
					 ((a) & 0x0) + 0x100000ll * (b))
#define CPTX_VQX_DONE(a, b)		(0x0000420ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_DONE_ACK(a, b)		(0x0000440ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_DONE_INT_W1S(a, b)	(0x0000460ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_DONE_INT_W1C(a, b)	(0x0000468ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_DONE_ENA_W1S(a, b)	(0x0000470ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_DONE_ENA_W1C(a, b)	(0x0000478ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_MISC_INT(a, b)		(0x0000500ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_MISC_INT_W1S(a, b)	(0x0000508ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_MISC_ENA_W1S(a, b)	(0x0000510ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_MISC_ENA_W1C(a, b)	(0x0000518ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VQX_DOORBELL(a, b)		(0x0000600ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b))
#define CPTX_VFX_PF_MBOXX(a, b, c)	(0x0001000ll + 0x1000000000ll * \
					 ((a) & 0x1) + 0x100000ll * (b) + \
					 8ll * ((c) & 0x1))

/* VF HAL functions */

void
otx_cpt_poll_misc(struct cpt_vf *cptvf);

int
otx_cpt_hw_init(struct cpt_vf *cptvf, void *pdev, void *reg_base, char *name);

#endif /* _OTX_CRYPTODEV_HW_ACCESS_H_ */
