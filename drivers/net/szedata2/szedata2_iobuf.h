/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2017 CESNET
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of CESNET nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SZEDATA2_IOBUF_H_
#define _SZEDATA2_IOBUF_H_

#include <stdint.h>

#include <rte_dev.h>

/* IBUF offsets from the beginning of the PCI resource address space. */
extern const uint32_t szedata2_ibuf_base_table[];
extern const uint32_t szedata2_ibuf_count;

/* OBUF offsets from the beginning of the PCI resource address space. */
extern const uint32_t szedata2_obuf_base_table[];
extern const uint32_t szedata2_obuf_count;

/**
 * Macro takes pointer to pci resource structure (rsc)
 * and returns pointer to mapped resource memory at
 * specified offset (offset) typecast to the type (type).
 */
#define SZEDATA2_PCI_RESOURCE_PTR(rsc, offset, type) \
	((type)(((uint8_t *)(rsc)->addr) + (offset)))

/**
 * Get pointer to IBUF structure according to specified index.
 *
 * @param rsc
 *     Pointer to base address of memory resource.
 * @param index
 *     Index of IBUF.
 * @return
 *     Pointer to IBUF structure.
 */
static inline struct szedata2_ibuf *
ibuf_ptr_by_index(struct rte_mem_resource *rsc, uint32_t index)
{
	if (index >= szedata2_ibuf_count)
		index = szedata2_ibuf_count - 1;
	return SZEDATA2_PCI_RESOURCE_PTR(rsc, szedata2_ibuf_base_table[index],
		struct szedata2_ibuf *);
}

/**
 * Get pointer to OBUF structure according to specified idnex.
 *
 * @param rsc
 *     Pointer to base address of memory resource.
 * @param index
 *     Index of OBUF.
 * @return
 *     Pointer to OBUF structure.
 */
static inline struct szedata2_obuf *
obuf_ptr_by_index(struct rte_mem_resource *rsc, uint32_t index)
{
	if (index >= szedata2_obuf_count)
		index = szedata2_obuf_count - 1;
	return SZEDATA2_PCI_RESOURCE_PTR(rsc, szedata2_obuf_base_table[index],
		struct szedata2_obuf *);
}

#endif /* _SZEDATA2_IOBUF_H_ */
