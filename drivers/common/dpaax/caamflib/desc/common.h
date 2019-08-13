/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2016 NXP
 *
 */

#ifndef __DESC_COMMON_H__
#define __DESC_COMMON_H__

#include "rta.h"

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
