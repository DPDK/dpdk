/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright (c) 2016 NXP.
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

#ifndef __DESC_COMMON_H__
#define __DESC_COMMON_H__

#include "hw/rta.h"

/**
 * DOC: Shared Descriptor Constructors - shared structures
 *
 * Data structures shared between algorithm, protocol implementations.
 */

/**
 * struct alginfo - Container for algorithm details
 * @algtype: algorithm selector; for valid values, see documentation of the
 *           functions where it is used.
 * @keylen: length of the provided algorithm key, in bytes
 * @key: address where algorithm key resides; virtual address if key_type is
 *       RTA_DATA_IMM, physical (bus) address if key_type is RTA_DATA_PTR or
 *       RTA_DATA_IMM_DMA.
 * @key_enc_flags: key encryption flags; see encrypt_flags parameter of KEY
 *                 command for valid values.
 * @key_type: enum rta_data_type
 * @algmode: algorithm mode selector; for valid values, see documentation of the
 *           functions where it is used.
 */
struct alginfo {
	uint32_t algtype;
	uint32_t keylen;
	uint64_t key;
	uint32_t key_enc_flags;
	enum rta_data_type key_type;
	uint16_t algmode;
};

#define INLINE_KEY(alginfo)	inline_flags(alginfo->key_type)

/**
 * rta_inline_query() - Provide indications on which data items can be inlined
 *                      and which shall be referenced in a shared descriptor.
 * @sd_base_len: Shared descriptor base length - bytes consumed by the commands,
 *               excluding the data items to be inlined (or corresponding
 *               pointer if an item is not inlined). Each cnstr_* function that
 *               generates descriptors should have a define mentioning
 *               corresponding length.
 * @jd_len: Maximum length of the job descriptor(s) that will be used
 *          together with the shared descriptor.
 * @data_len: Array of lengths of the data items trying to be inlined
 * @inl_mask: 32bit mask with bit x = 1 if data item x can be inlined, 0
 *            otherwise.
 * @count: Number of data items (size of @data_len array); must be <= 32
 *
 * Return: 0 if data can be inlined / referenced, negative value if not. If 0,
 *         check @inl_mask for details.
 */
static inline int
rta_inline_query(unsigned int sd_base_len,
		 unsigned int jd_len,
		 unsigned int *data_len,
		 uint32_t *inl_mask,
		 unsigned int count)
{
	int rem_bytes = (int)(CAAM_DESC_BYTES_MAX - sd_base_len - jd_len);
	unsigned int i;

	*inl_mask = 0;
	for (i = 0; (i < count) && (rem_bytes > 0); i++) {
		if (rem_bytes - (int)(data_len[i] +
			(count - i - 1) * CAAM_PTR_SZ) >= 0) {
			rem_bytes -= data_len[i];
			*inl_mask |= (1 << i);
		} else {
			rem_bytes -= CAAM_PTR_SZ;
		}
	}

	return (rem_bytes >= 0) ? 0 : -1;
}

/**
 * struct protcmd - Container for Protocol Operation Command fields
 * @optype: command type
 * @protid: protocol Identifier
 * @protinfo: protocol Information
 */
struct protcmd {
	uint32_t optype;
	uint32_t protid;
	uint16_t protinfo;
};

#endif /* __DESC_COMMON_H__ */
