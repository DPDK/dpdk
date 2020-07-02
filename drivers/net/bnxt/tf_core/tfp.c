/* SPDX-License-Identifier: BSD-3-Clause
 * see the individual elements.
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <rte_memcpy.h>
#include <rte_byteorder.h>
#include <rte_config.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_spinlock.h>

#include "tf_core.h"
#include "tfp.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "tf_msg_common.h"

/**
 * Sends TruFlow msg to the TruFlow Firmware using
 * a message specific HWRM message type.
 *
 * Returns success or failure code.
 */
int
tfp_send_msg_direct(struct tf *tfp,
		    struct tfp_send_msg_parms *parms)
{
	int      rc = 0;
	uint8_t  use_kong_mb = 1;

	if (parms == NULL)
		return -EINVAL;

	if (parms->mailbox == TF_CHIMP_MB)
		use_kong_mb = 0;

	rc = bnxt_hwrm_tf_message_direct(container_of(tfp,
					       struct bnxt,
					       tfp),
					 use_kong_mb,
					 parms->tf_type,
					 parms->req_data,
					 parms->req_size,
					 parms->resp_data,
					 parms->resp_size);

	return rc;
}

/**
 * Sends preformatted TruFlow msg to the TruFlow Firmware using
 * the Truflow tunnel HWRM message type.
 *
 * Returns success or failure code.
 */
int
tfp_send_msg_tunneled(struct tf *tfp,
		      struct tfp_send_msg_parms *parms)
{
	int      rc = 0;
	uint8_t  use_kong_mb = 1;

	if (parms == NULL)
		return -EINVAL;

	if (parms->mailbox == TF_CHIMP_MB)
		use_kong_mb = 0;

	rc = bnxt_hwrm_tf_message_tunneled(container_of(tfp,
						  struct bnxt,
						  tfp),
					   use_kong_mb,
					   parms->tf_type,
					   parms->tf_subtype,
					   &parms->tf_resp_code,
					   parms->req_data,
					   parms->req_size,
					   parms->resp_data,
					   parms->resp_size);

	return rc;
}

/**
 * Allocates zero'ed memory from the heap.
 *
 * Returns success or failure code.
 */
int
tfp_calloc(struct tfp_calloc_parms *parms)
{
	if (parms == NULL)
		return -EINVAL;

	parms->mem_va = rte_zmalloc("tf",
				    (parms->nitems * parms->size),
				    parms->alignment);
	if (parms->mem_va == NULL) {
		TFP_DRV_LOG(ERR, "Allocate failed mem_va\n");
		return -ENOMEM;
	}

	parms->mem_pa = (void *)((uintptr_t)rte_mem_virt2iova(parms->mem_va));
	if (parms->mem_pa == (void *)((uintptr_t)RTE_BAD_IOVA)) {
		TFP_DRV_LOG(ERR, "Allocate failed mem_pa\n");
		return -ENOMEM;
	}

	return 0;
}

/**
 * Frees the memory space pointed to by the provided pointer. The
 * pointer must have been returned from the tfp_calloc().
 */
void
tfp_free(void *addr)
{
	rte_free(addr);
}

/**
 * Copies n bytes from src memory to dest memory. The memory areas
 * must not overlap.
 */
void
tfp_memcpy(void *dest, void *src, size_t n)
{
	rte_memcpy(dest, src, n);
}

/**
 * Used to initialize portable spin lock
 */
void
tfp_spinlock_init(struct tfp_spinlock_parms *parms)
{
	rte_spinlock_init(&parms->slock);
}

/**
 * Used to lock portable spin lock
 */
void
tfp_spinlock_lock(struct tfp_spinlock_parms *parms)
{
	rte_spinlock_lock(&parms->slock);
}

/**
 * Used to unlock portable spin lock
 */
void
tfp_spinlock_unlock(struct tfp_spinlock_parms *parms)
{
	rte_spinlock_unlock(&parms->slock);
}

int
tfp_get_fid(struct tf *tfp, uint16_t *fw_fid)
{
	struct bnxt *bp = NULL;

	if (tfp == NULL || fw_fid == NULL)
		return -EINVAL;

	bp = container_of(tfp, struct bnxt, tfp);
	if (bp == NULL)
		return -EINVAL;

	*fw_fid = bp->fw_fid;

	return 0;
}
