/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
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
#ifndef __FSL_DPMNG_CMD_H
#define __FSL_DPMNG_CMD_H

/* Command IDs */
#define DPMNG_CMDID_GET_VERSION			0x8311
#define DPMNG_CMDID_GET_SOC_VERSION		0x8321

/*                cmd, param, offset, width, type, arg_name */
#define DPMNG_RSP_GET_VERSION(cmd, mc_ver_info) \
do { \
	MC_RSP_OP(cmd, 0, 0,  32, uint32_t, mc_ver_info->revision); \
	MC_RSP_OP(cmd, 0, 32, 32, uint32_t, mc_ver_info->major); \
	MC_RSP_OP(cmd, 1, 0,  32, uint32_t, mc_ver_info->minor); \
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPMNG_RSP_GET_SOC_VERSION(cmd, mc_soc_version) \
do { \
	MC_RSP_OP(cmd, 0, 0,  32, uint32_t, mc_soc_version->svr); \
	MC_RSP_OP(cmd, 0, 32, 32, uint32_t, mc_soc_version->pvr); \
} while (0)

#endif /* __FSL_DPMNG_CMD_H */
