/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Broadcom.
 * All rights reserved.
 */

#include "bcmfs_hw_defs.h"
#include "bcmfs_rm_common.h"

/* Completion descriptor format */
#define FS_CMPL_OPAQUE_SHIFT			0
#define FS_CMPL_OPAQUE_MASK			0xffff
#define FS_CMPL_ENGINE_STATUS_SHIFT		16
#define FS_CMPL_ENGINE_STATUS_MASK		0xffff
#define FS_CMPL_DME_STATUS_SHIFT		32
#define FS_CMPL_DME_STATUS_MASK			0xffff
#define FS_CMPL_RM_STATUS_SHIFT			48
#define FS_CMPL_RM_STATUS_MASK			0xffff
/* Completion RM status code */
#define FS_RM_STATUS_CODE_SHIFT			0
#define FS_RM_STATUS_CODE_MASK			0x3ff
#define FS_RM_STATUS_CODE_GOOD			0x0
#define FS_RM_STATUS_CODE_AE_TIMEOUT		0x3ff


/* Completion DME status code */
#define FS_DME_STATUS_MEM_COR_ERR		BIT(0)
#define FS_DME_STATUS_MEM_UCOR_ERR		BIT(1)
#define FS_DME_STATUS_FIFO_UNDRFLOW		BIT(2)
#define FS_DME_STATUS_FIFO_OVERFLOW		BIT(3)
#define FS_DME_STATUS_RRESP_ERR			BIT(4)
#define FS_DME_STATUS_BRESP_ERR			BIT(5)
#define FS_DME_STATUS_ERROR_MASK		(FS_DME_STATUS_MEM_COR_ERR | \
						 FS_DME_STATUS_MEM_UCOR_ERR | \
						 FS_DME_STATUS_FIFO_UNDRFLOW | \
						 FS_DME_STATUS_FIFO_OVERFLOW | \
						 FS_DME_STATUS_RRESP_ERR | \
						 FS_DME_STATUS_BRESP_ERR)

/* APIs related to ring manager descriptors */
uint64_t
rm_build_desc(uint64_t val, uint32_t shift,
	   uint64_t mask)
{
	return((val & mask) << shift);
}

uint64_t
rm_read_desc(void *desc_ptr)
{
	return le64_to_cpu(*((uint64_t *)desc_ptr));
}

void
rm_write_desc(void *desc_ptr, uint64_t desc)
{
	*((uint64_t *)desc_ptr) = cpu_to_le64(desc);
}

uint32_t
rm_cmpl_desc_to_reqid(uint64_t cmpl_desc)
{
	return (uint32_t)(cmpl_desc & FS_CMPL_OPAQUE_MASK);
}

int
rm_cmpl_desc_to_error(uint64_t cmpl_desc)
{
	uint32_t status;

	status = FS_DESC_DEC(cmpl_desc, FS_CMPL_DME_STATUS_SHIFT,
			     FS_CMPL_DME_STATUS_MASK);
	if (status & FS_DME_STATUS_ERROR_MASK)
		return -EIO;

	status = FS_DESC_DEC(cmpl_desc, FS_CMPL_RM_STATUS_SHIFT,
			     FS_CMPL_RM_STATUS_MASK);
	status &= FS_RM_STATUS_CODE_MASK;
	if (status == FS_RM_STATUS_CODE_AE_TIMEOUT)
		return -ETIMEDOUT;

	return 0;
}
