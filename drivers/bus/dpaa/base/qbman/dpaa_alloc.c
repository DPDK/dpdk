/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2009-2016 Freescale Semiconductor Inc.
 * Copyright 2017 NXP
 *
 */

#include "dpaa_sys.h"
#include <process.h>
#include <fsl_qman.h>
#include <fsl_bman.h>

int bman_alloc_bpid_range(u32 *result, u32 count, u32 align, int partial)
{
	return process_alloc(dpaa_id_bpid, result, count, align, partial);
}

void bman_release_bpid_range(u32 bpid, u32 count)
{
	process_release(dpaa_id_bpid, bpid, count);
}

int bman_reserve_bpid_range(u32 bpid, u32 count)
{
	return process_reserve(dpaa_id_bpid, bpid, count);
}

int qman_alloc_fqid_range(u32 *result, u32 count, u32 align, int partial)
{
	return process_alloc(dpaa_id_fqid, result, count, align, partial);
}

void qman_release_fqid_range(u32 fqid, u32 count)
{
	process_release(dpaa_id_fqid, fqid, count);
}

int qman_reserve_fqid_range(u32 fqid, unsigned int count)
{
	return process_reserve(dpaa_id_fqid, fqid, count);
}

int qman_alloc_pool_range(u32 *result, u32 count, u32 align, int partial)
{
	return process_alloc(dpaa_id_qpool, result, count, align, partial);
}

void qman_release_pool_range(u32 pool, u32 count)
{
	process_release(dpaa_id_qpool, pool, count);
}

int qman_reserve_pool_range(u32 pool, u32 count)
{
	return process_reserve(dpaa_id_qpool, pool, count);
}

int qman_alloc_cgrid_range(u32 *result, u32 count, u32 align, int partial)
{
	return process_alloc(dpaa_id_cgrid, result, count, align, partial);
}

void qman_release_cgrid_range(u32 cgrid, u32 count)
{
	process_release(dpaa_id_cgrid, cgrid, count);
}

int qman_reserve_cgrid_range(u32 cgrid, u32 count)
{
	return process_reserve(dpaa_id_cgrid, cgrid, count);
}
