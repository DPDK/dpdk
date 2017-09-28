/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2009-2016 Freescale Semiconductor Inc.
 * Copyright 2017 NXP.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *   GPL LICENSE SUMMARY
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
