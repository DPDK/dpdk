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

#include <stdint.h>

#include <libsze2.h>

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

#endif /* RTE_PMD_SZEDATA2_H_ */
