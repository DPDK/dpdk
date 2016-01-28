/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2015 - 2016 CESNET
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

#ifndef RTE_PMD_SZEDATA2_H_
#define RTE_PMD_SZEDATA2_H_

#include <stdbool.h>

#include <rte_byteorder.h>

/* PCI Vendor ID */
#define PCI_VENDOR_ID_NETCOPE 0x1b26

/* PCI Device IDs */
#define PCI_DEVICE_ID_NETCOPE_COMBO80G 0xcb80
#define PCI_DEVICE_ID_NETCOPE_COMBO100G 0xc1c1
#define PCI_DEVICE_ID_NETCOPE_COMBO100G2 0xc2c1

/* number of PCI resource used by COMBO card */
#define PCI_RESOURCE_NUMBER 0

/* szedata2_packet header length == 4 bytes == 2B segment size + 2B hw size */
#define RTE_SZE2_PACKET_HEADER_SIZE 4

#define RTE_SZE2_MMIO_MAX 10

/*!
 * Round 'what' to the nearest larger (or equal) multiple of '8'
 * (szedata2 packet is aligned to 8 bytes)
 */
#define RTE_SZE2_ALIGN8(what) (((what) + ((8) - 1)) & (~((8) - 1)))

/*! main handle structure */
struct szedata {
	int fd;
	struct sze2_instance_info *info;
	uint32_t *write_size;
	void *space[RTE_SZE2_MMIO_MAX];
	struct szedata_lock lock[2][2];

	__u32 *rx_asize, *tx_asize;

	/* szedata_read_next variables - to keep context (ct) */

	/*
	 * rx
	 */
	/** initial sze lock ptr */
	const struct szedata_lock   *ct_rx_lck_orig;
	/** current sze lock ptr (initial or next) */
	const struct szedata_lock   *ct_rx_lck;
	/** remaining bytes (not read) within current lock */
	unsigned int                ct_rx_rem_bytes;
	/** current pointer to locked memory */
	unsigned char               *ct_rx_cur_ptr;
	/**
	 * allocated buffer to store RX packet if it was split
	 * into 2 buffers
	 */
	unsigned char               *ct_rx_buffer;
	/** registered function to provide filtering based on hwdata */
	int (*ct_rx_filter)(u_int16_t hwdata_len, u_char *hwdata);

	/*
	 * tx
	 */
	/**
	 * buffer for tx - packet is prepared here
	 * (in future for burst write)
	 */
	unsigned char               *ct_tx_buffer;
	/** initial sze TX lock ptrs - number according to TX interfaces */
	const struct szedata_lock   **ct_tx_lck_orig;
	/** current sze TX lock ptrs - number according to TX interfaces */
	const struct szedata_lock   **ct_tx_lck;
	/** already written bytes in both locks */
	unsigned int                *ct_tx_written_bytes;
	/** remaining bytes (not written) within current lock */
	unsigned int                *ct_tx_rem_bytes;
	/** current pointers to locked memory */
	unsigned char               **ct_tx_cur_ptr;
	/** NUMA node closest to PCIe device, or -1 */
	int                         numa_node;
};

/*
 * @return Byte from PCI resource at offset "offset".
 */
static inline uint8_t
pci_resource_read8(struct rte_eth_dev *dev, uint32_t offset)
{
	return *((uint8_t *)((uint8_t *)
			dev->pci_dev->mem_resource[PCI_RESOURCE_NUMBER].addr +
			offset));
}

/*
 * @return Two bytes from PCI resource starting at offset "offset".
 */
static inline uint16_t
pci_resource_read16(struct rte_eth_dev *dev, uint32_t offset)
{
	return rte_le_to_cpu_16(*((uint16_t *)((uint8_t *)
			dev->pci_dev->mem_resource[PCI_RESOURCE_NUMBER].addr +
			offset)));
}

/*
 * @return Four bytes from PCI resource starting at offset "offset".
 */
static inline uint32_t
pci_resource_read32(struct rte_eth_dev *dev, uint32_t offset)
{
	return rte_le_to_cpu_32(*((uint32_t *)((uint8_t *)
			dev->pci_dev->mem_resource[PCI_RESOURCE_NUMBER].addr +
			offset)));
}

/*
 * @return Eight bytes from PCI resource starting at offset "offset".
 */
static inline uint64_t
pci_resource_read64(struct rte_eth_dev *dev, uint32_t offset)
{
	return rte_le_to_cpu_64(*((uint64_t *)((uint8_t *)
			dev->pci_dev->mem_resource[PCI_RESOURCE_NUMBER].addr +
			offset)));
}

/*
 * Write one byte to PCI resource address space at offset "offset".
 */
static inline void
pci_resource_write8(struct rte_eth_dev *dev, uint32_t offset, uint8_t val)
{
	*((uint8_t *)((uint8_t *)
			dev->pci_dev->mem_resource[PCI_RESOURCE_NUMBER].addr +
			offset)) = val;
}

/*
 * Write two bytes to PCI resource address space at offset "offset".
 */
static inline void
pci_resource_write16(struct rte_eth_dev *dev, uint32_t offset, uint16_t val)
{
	*((uint16_t *)((uint8_t *)
			dev->pci_dev->mem_resource[PCI_RESOURCE_NUMBER].addr +
			offset)) = rte_cpu_to_le_16(val);
}

/*
 * Write four bytes to PCI resource address space at offset "offset".
 */
static inline void
pci_resource_write32(struct rte_eth_dev *dev, uint32_t offset, uint32_t val)
{
	*((uint32_t *)((uint8_t *)
			dev->pci_dev->mem_resource[PCI_RESOURCE_NUMBER].addr +
			offset)) = rte_cpu_to_le_32(val);
}

/*
 * Write eight bytes to PCI resource address space at offset "offset".
 */
static inline void
pci_resource_write64(struct rte_eth_dev *dev, uint32_t offset, uint64_t val)
{
	*((uint64_t *)((uint8_t *)
			dev->pci_dev->mem_resource[PCI_RESOURCE_NUMBER].addr +
			offset)) = rte_cpu_to_le_64(val);
}

#define SZEDATA2_PCI_RESOURCE_PTR(dev, offset, type) \
	((type)((uint8_t *) \
	((dev)->pci_dev->mem_resource[PCI_RESOURCE_NUMBER].addr) \
	+ (offset)))

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

/*
 * Structure describes CGMII IBUF address space
 */
struct szedata2_cgmii_ibuf {
	/** Total Received Frames Counter low part */
	uint32_t trfcl;
	/** Correct Frames Counter low part */
	uint32_t cfcl;
	/** Discarded Frames Counter low part */
	uint32_t dfcl;
	/** Counter of frames discarded due to buffer overflow low part */
	uint32_t bodfcl;
	/** Total Received Frames Counter high part */
	uint32_t trfch;
	/** Correct Frames Counter high part */
	uint32_t cfch;
	/** Discarded Frames Counter high part */
	uint32_t dfch;
	/** Counter of frames discarded due to buffer overflow high part */
	uint32_t bodfch;
	/** IBUF enable register */
	uint32_t ibuf_en;
	/** Error mask register */
	uint32_t err_mask;
	/** IBUF status register */
	uint32_t ibuf_st;
	/** IBUF command register */
	uint32_t ibuf_cmd;
	/** Minimum frame length allowed */
	uint32_t mfla;
	/** Frame MTU */
	uint32_t mtu;
	/** MAC address check mode */
	uint32_t mac_chmode;
	/** Octets Received OK Counter low part */
	uint32_t orocl;
	/** Octets Received OK Counter high part */
	uint32_t oroch;
} __rte_packed;

/* Offset of CGMII IBUF memory for MAC addresses */
#define SZEDATA2_CGMII_IBUF_MAC_MEM_OFF 0x80

/*
 * @return
 *     true if IBUF is enabled
 *     false if IBUF is disabled
 */
static inline bool
cgmii_ibuf_is_enabled(volatile struct szedata2_cgmii_ibuf *ibuf)
{
	return ((rte_le_to_cpu_32(ibuf->ibuf_en) & 0x1) != 0) ? true : false;
}

/*
 * Enables IBUF.
 */
static inline void
cgmii_ibuf_enable(volatile struct szedata2_cgmii_ibuf *ibuf)
{
	ibuf->ibuf_en =
		rte_cpu_to_le_32(rte_le_to_cpu_32(ibuf->ibuf_en) | 0x1);
}

/*
 * Disables IBUF.
 */
static inline void
cgmii_ibuf_disable(volatile struct szedata2_cgmii_ibuf *ibuf)
{
	ibuf->ibuf_en =
		rte_cpu_to_le_32(rte_le_to_cpu_32(ibuf->ibuf_en) & ~0x1);
}

/*
 * @return
 *     true if ibuf link is up
 *     false if ibuf link is down
 */
static inline bool
cgmii_ibuf_is_link_up(volatile struct szedata2_cgmii_ibuf *ibuf)
{
	return ((rte_le_to_cpu_32(ibuf->ibuf_st) & 0x80) != 0) ? true : false;
}

/*
 * @return
 *     MAC address check mode
 */
static inline enum szedata2_mac_check_mode
cgmii_ibuf_mac_mode_read(volatile struct szedata2_cgmii_ibuf *ibuf)
{
	switch (rte_le_to_cpu_32(ibuf->mac_chmode) & 0x3) {
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

/*
 * Writes "mode" in MAC address check mode register.
 */
static inline void
cgmii_ibuf_mac_mode_write(volatile struct szedata2_cgmii_ibuf *ibuf,
		enum szedata2_mac_check_mode mode)
{
	ibuf->mac_chmode = rte_cpu_to_le_32(
			(rte_le_to_cpu_32(ibuf->mac_chmode) & ~0x3) | mode);
}

/*
 * Structure describes CGMII OBUF address space
 */
struct szedata2_cgmii_obuf {
	/** Total Sent Frames Counter low part */
	uint32_t tsfcl;
	/** Octets Sent Counter low part */
	uint32_t oscl;
	/** Total Discarded Frames Counter low part */
	uint32_t tdfcl;
	/** reserved */
	uint32_t reserved1;
	/** Total Sent Frames Counter high part */
	uint32_t tsfch;
	/** Octets Sent Counter high part */
	uint32_t osch;
	/** Total Discarded Frames Counter high part */
	uint32_t tdfch;
	/** reserved */
	uint32_t reserved2;
	/** OBUF enable register */
	uint32_t obuf_en;
	/** reserved */
	uint64_t reserved3;
	/** OBUF control register */
	uint32_t ctrl;
	/** OBUF status register */
	uint32_t obuf_st;
} __rte_packed;

/*
 * @return
 *     true if OBUF is enabled
 *     false if OBUF is disabled
 */
static inline bool
cgmii_obuf_is_enabled(volatile struct szedata2_cgmii_obuf *obuf)
{
	return ((rte_le_to_cpu_32(obuf->obuf_en) & 0x1) != 0) ? true : false;
}

/*
 * Enables OBUF.
 */
static inline void
cgmii_obuf_enable(volatile struct szedata2_cgmii_obuf *obuf)
{
	obuf->obuf_en =
		rte_cpu_to_le_32(rte_le_to_cpu_32(obuf->obuf_en) | 0x1);
}

/*
 * Disables OBUF.
 */
static inline void
cgmii_obuf_disable(volatile struct szedata2_cgmii_obuf *obuf)
{
	obuf->obuf_en =
		rte_cpu_to_le_32(rte_le_to_cpu_32(obuf->obuf_en) & ~0x1);
}

/*
 * Function takes value from IBUF status register. Values in IBUF and OBUF
 * should be same.
 *
 * @return Link speed constant.
 */
static inline enum szedata2_link_speed
cgmii_link_speed(volatile struct szedata2_cgmii_ibuf *ibuf)
{
	uint32_t speed = (rte_le_to_cpu_32(ibuf->ibuf_st) & 0x70) >> 4;
	switch (speed) {
	case 0x03:
		return SZEDATA2_LINK_SPEED_10G;
	case 0x04:
		return SZEDATA2_LINK_SPEED_40G;
	case 0x05:
		return SZEDATA2_LINK_SPEED_100G;
	default:
		return SZEDATA2_LINK_SPEED_DEFAULT;
	}
}

/*
 * IBUFs and OBUFs can generally be located at different offsets in different
 * firmwares.
 * This part defines base offsets of IBUFs and OBUFs through various firmwares.
 * Currently one firmware type is supported.
 * Type of firmware is set through configuration option
 * CONFIG_RTE_LIBRTE_PMD_SZEDATA_AS.
 * Possible values are:
 * 0 - for firmwares:
 *     NIC_100G1_LR4
 *     HANIC_100G1_LR4
 *     HANIC_100G1_SR10
 */
#if !defined(RTE_LIBRTE_PMD_SZEDATA2_AS)
#error "RTE_LIBRTE_PMD_SZEDATA2_AS has to be defined"
#elif RTE_LIBRTE_PMD_SZEDATA2_AS == 0

/*
 * CGMII IBUF offset from the beginning of PCI resource address space.
 */
#define SZEDATA2_CGMII_IBUF_BASE_OFF 0x8000
/*
 * Size of CGMII IBUF.
 */
#define SZEDATA2_CGMII_IBUF_SIZE 0x200

/*
 * GCMII OBUF offset from the beginning of PCI resource address space.
 */
#define SZEDATA2_CGMII_OBUF_BASE_OFF 0x9000
/*
 * Size of CGMII OBUF.
 */
#define SZEDATA2_CGMII_OBUF_SIZE 0x100

#else
#error "RTE_LIBRTE_PMD_SZEDATA2_AS has wrong value, see comments in config file"
#endif

#endif
