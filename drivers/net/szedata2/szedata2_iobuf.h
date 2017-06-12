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
#include <stdbool.h>

#include <rte_byteorder.h>
#include <rte_io.h>
#include <rte_dev.h>

/* IBUF offsets from the beginning of the PCI resource address space. */
extern const uint32_t szedata2_ibuf_base_table[];
extern const uint32_t szedata2_ibuf_count;

/* OBUF offsets from the beginning of the PCI resource address space. */
extern const uint32_t szedata2_obuf_base_table[];
extern const uint32_t szedata2_obuf_count;

enum szedata2_link_speed {
	SZEDATA2_LINK_SPEED_DEFAULT = 0,
	SZEDATA2_LINK_SPEED_10G,
	SZEDATA2_LINK_SPEED_40G,
	SZEDATA2_LINK_SPEED_100G,
};

enum szedata2_mac_check_mode {
	SZEDATA2_MAC_CHMODE_PROMISC       = 0x0,
	SZEDATA2_MAC_CHMODE_ONLY_VALID    = 0x1,
	SZEDATA2_MAC_CHMODE_ALL_BROADCAST = 0x2,
	SZEDATA2_MAC_CHMODE_ALL_MULTICAST = 0x3,
};

/**
 * Macro takes pointer to pci resource structure (rsc)
 * and returns pointer to mapped resource memory at
 * specified offset (offset) typecast to the type (type).
 */
#define SZEDATA2_PCI_RESOURCE_PTR(rsc, offset, type) \
	((type)(((uint8_t *)(rsc)->addr) + (offset)))

/**
 * Maximum possible number of MAC addresses (limited by IBUF status
 * register value MAC_COUNT which has 5 bits).
 */
#define SZEDATA2_IBUF_MAX_MAC_COUNT 32

/**
 * Structure describes IBUF address space.
 */
struct szedata2_ibuf {
	/** Total Received Frames Counter low part */
	uint32_t trfcl; /**< 0x00 */
	/** Correct Frames Counter low part */
	uint32_t cfcl; /**< 0x04 */
	/** Discarded Frames Counter low part */
	uint32_t dfcl; /**< 0x08 */
	/** Counter of frames discarded due to buffer overflow low part */
	uint32_t bodfcl; /**< 0x0C */
	/** Total Received Frames Counter high part */
	uint32_t trfch; /**< 0x10 */
	/** Correct Frames Counter high part */
	uint32_t cfch; /**< 0x14 */
	/** Discarded Frames Counter high part */
	uint32_t dfch; /**< 0x18 */
	/** Counter of frames discarded due to buffer overflow high part */
	uint32_t bodfch; /**< 0x1C */
	/** IBUF enable register */
	uint32_t ibuf_en; /**< 0x20 */
	/** Error mask register */
	uint32_t err_mask; /**< 0x24 */
	/** IBUF status register */
	uint32_t ibuf_st; /**< 0x28 */
	/** IBUF command register */
	uint32_t ibuf_cmd; /**< 0x2C */
	/** Minimum frame length allowed */
	uint32_t mfla; /**< 0x30 */
	/** Frame MTU */
	uint32_t mtu; /**< 0x34 */
	/** MAC address check mode */
	uint32_t mac_chmode; /**< 0x38 */
	/** Octets Received OK Counter low part */
	uint32_t orocl; /**< 0x3C */
	/** Octets Received OK Counter high part */
	uint32_t oroch; /**< 0x40 */
	/** reserved */
	uint8_t reserved[60]; /**< 0x4C */
	/** IBUF memory for MAC addresses */
	uint32_t mac_mem[2 * SZEDATA2_IBUF_MAX_MAC_COUNT]; /**< 0x80 */
} __rte_packed;

/**
 * Structure describes OBUF address space.
 */
struct szedata2_obuf {
	/** Total Sent Frames Counter low part */
	uint32_t tsfcl; /**< 0x00 */
	/** Octets Sent Counter low part */
	uint32_t oscl; /**< 0x04 */
	/** Total Discarded Frames Counter low part */
	uint32_t tdfcl; /**< 0x08 */
	/** reserved */
	uint32_t reserved1; /**< 0x0C */
	/** Total Sent Frames Counter high part */
	uint32_t tsfch; /**< 0x10 */
	/** Octets Sent Counter high part */
	uint32_t osch; /**< 0x14 */
	/** Total Discarded Frames Counter high part */
	uint32_t tdfch; /**< 0x18 */
	/** reserved */
	uint32_t reserved2; /**< 0x1C */
	/** OBUF enable register */
	uint32_t obuf_en; /**< 0x20 */
	/** reserved */
	uint64_t reserved3; /**< 0x24 */
	/** OBUF control register */
	uint32_t ctrl; /**< 0x2C */
	/** OBUF status register */
	uint32_t obuf_st; /**< 0x30 */
} __rte_packed;

/**
 * Wrapper for reading 4 bytes from device memory in correct endianness.
 *
 * @param addr
 *     Address for reading.
 * @return
 *     4 B value.
 */
static inline uint32_t
szedata2_read32(const volatile void *addr)
{
	return rte_le_to_cpu_32(rte_read32(addr));
}

/**
 * Wrapper for writing 4 bytes to device memory in correct endianness.
 *
 * @param value
 *     Value to write.
 * @param addr
 *     Address for writing.
 */
static inline void
szedata2_write32(uint32_t value, volatile void *addr)
{
	rte_write32(rte_cpu_to_le_32(value), addr);
}

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

/**
 * Checks if IBUF is enabled.
 *
 * @param ibuf
 *     Pointer to IBUF structure.
 * @return
 *     true if IBUF is enabled.
 *     false if IBUF is disabled.
 */
static inline bool
ibuf_is_enabled(const volatile struct szedata2_ibuf *ibuf)
{
	return ((szedata2_read32(&ibuf->ibuf_en) & 0x1) != 0) ? true : false;
}

/**
 * Enables IBUF.
 *
 * @param ibuf
 *     Pointer to IBUF structure.
 */
static inline void
ibuf_enable(volatile struct szedata2_ibuf *ibuf)
{
	szedata2_write32(szedata2_read32(&ibuf->ibuf_en) | 0x1, &ibuf->ibuf_en);
}

/**
 * Disables IBUF.
 *
 * @param ibuf
 *     Pointer to IBUF structure.
 */
static inline void
ibuf_disable(volatile struct szedata2_ibuf *ibuf)
{
	szedata2_write32(szedata2_read32(&ibuf->ibuf_en) & ~0x1,
			&ibuf->ibuf_en);
}

/**
 * Checks if link is up.
 *
 * @param ibuf
 *     Pointer to IBUF structure.
 * @return
 *     true if ibuf link is up.
 *     false if ibuf link is down.
 */
static inline bool
ibuf_is_link_up(const volatile struct szedata2_ibuf *ibuf)
{
	return ((szedata2_read32(&ibuf->ibuf_st) & 0x80) != 0) ? true : false;
}

/**
 * Get current MAC address check mode from IBUF.
 *
 * @param ibuf
 *     Pointer to IBUF structure.
 * @return
 *     MAC address check mode constant.
 */
static inline enum szedata2_mac_check_mode
ibuf_mac_mode_read(const volatile struct szedata2_ibuf *ibuf)
{
	switch (szedata2_read32(&ibuf->mac_chmode) & 0x3) {
	case 0x0:
		return SZEDATA2_MAC_CHMODE_PROMISC;
	case 0x1:
		return SZEDATA2_MAC_CHMODE_ONLY_VALID;
	case 0x2:
		return SZEDATA2_MAC_CHMODE_ALL_BROADCAST;
	case 0x3:
		return SZEDATA2_MAC_CHMODE_ALL_MULTICAST;
	default:
		return SZEDATA2_MAC_CHMODE_PROMISC;
	}
}

/**
 * Writes mode in MAC address check mode register in IBUF.
 *
 * @param ibuf
 *     Pointer to IBUF structure.
 * @param mode
 *     MAC address check mode to set.
 */
static inline void
ibuf_mac_mode_write(volatile struct szedata2_ibuf *ibuf,
		enum szedata2_mac_check_mode mode)
{
	szedata2_write32((szedata2_read32(&ibuf->mac_chmode) & ~0x3) | mode,
			&ibuf->mac_chmode);
}

/**
 * Checks if obuf is enabled.
 *
 * @param obuf
 *     Pointer to OBUF structure.
 * @return
 *     true if OBUF is enabled.
 *     false if OBUF is disabled.
 */
static inline bool
obuf_is_enabled(const volatile struct szedata2_obuf *obuf)
{
	return ((szedata2_read32(&obuf->obuf_en) & 0x1) != 0) ? true : false;
}

/**
 * Enables OBUF.
 *
 * @param obuf
 *     Pointer to OBUF structure.
 */
static inline void
obuf_enable(volatile struct szedata2_obuf *obuf)
{
	szedata2_write32(szedata2_read32(&obuf->obuf_en) | 0x1, &obuf->obuf_en);
}

/**
 * Disables OBUF.
 *
 * @param obuf
 *     Pointer to OBUF structure.
 */
static inline void
obuf_disable(volatile struct szedata2_obuf *obuf)
{
	szedata2_write32(szedata2_read32(&obuf->obuf_en) & ~0x1,
			&obuf->obuf_en);
}

#endif /* _SZEDATA2_IOBUF_H_ */
