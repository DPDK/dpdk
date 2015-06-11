/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#ifndef _VIRTIO_PCI_H_
#define _VIRTIO_PCI_H_

#include <stdint.h>

#ifdef __FreeBSD__
#include <sys/types.h>
#include <machine/cpufunc.h>
#else
#include <sys/io.h>
#endif

#include <rte_ethdev.h>

struct virtqueue;

/* VirtIO PCI vendor/device ID. */
#define VIRTIO_PCI_VENDORID     0x1AF4
#define VIRTIO_PCI_DEVICEID_MIN 0x1000
#define VIRTIO_PCI_DEVICEID_MAX 0x103F

/* VirtIO ABI version, this must match exactly. */
#define VIRTIO_PCI_ABI_VERSION 0

/*
 * VirtIO Header, located in BAR 0.
 */
#define VIRTIO_PCI_HOST_FEATURES  0  /* host's supported features (32bit, RO)*/
#define VIRTIO_PCI_GUEST_FEATURES 4  /* guest's supported features (32, RW) */
#define VIRTIO_PCI_QUEUE_PFN      8  /* physical address of VQ (32, RW) */
#define VIRTIO_PCI_QUEUE_NUM      12 /* number of ring entries (16, RO) */
#define VIRTIO_PCI_QUEUE_SEL      14 /* current VQ selection (16, RW) */
#define VIRTIO_PCI_QUEUE_NOTIFY   16 /* notify host regarding VQ (16, RW) */
#define VIRTIO_PCI_STATUS         18 /* device status register (8, RW) */
#define VIRTIO_PCI_ISR		  19 /* interrupt status register, reading
				      * also clears the register (8, RO) */
/* Only if MSIX is enabled: */
#define VIRTIO_MSI_CONFIG_VECTOR  20 /* configuration change vector (16, RW) */
#define VIRTIO_MSI_QUEUE_VECTOR	  22 /* vector for selected VQ notifications
				      (16, RW) */

/* The bit of the ISR which indicates a device has an interrupt. */
#define VIRTIO_PCI_ISR_INTR   0x1
/* The bit of the ISR which indicates a device configuration change. */
#define VIRTIO_PCI_ISR_CONFIG 0x2
/* Vector value used to disable MSI for queue. */
#define VIRTIO_MSI_NO_VECTOR 0xFFFF

/* VirtIO device IDs. */
#define VIRTIO_ID_NETWORK  0x01
#define VIRTIO_ID_BLOCK    0x02
#define VIRTIO_ID_CONSOLE  0x03
#define VIRTIO_ID_ENTROPY  0x04
#define VIRTIO_ID_BALLOON  0x05
#define VIRTIO_ID_IOMEMORY 0x06
#define VIRTIO_ID_9P       0x09

/* Status byte for guest to report progress. */
#define VIRTIO_CONFIG_STATUS_RESET     0x00
#define VIRTIO_CONFIG_STATUS_ACK       0x01
#define VIRTIO_CONFIG_STATUS_DRIVER    0x02
#define VIRTIO_CONFIG_STATUS_DRIVER_OK 0x04
#define VIRTIO_CONFIG_STATUS_FAILED    0x80

/*
 * Each virtqueue indirect descriptor list must be physically contiguous.
 * To allow us to malloc(9) each list individually, limit the number
 * supported to what will fit in one page. With 4KB pages, this is a limit
 * of 256 descriptors. If there is ever a need for more, we can switch to
 * contigmalloc(9) for the larger allocations, similar to what
 * bus_dmamem_alloc(9) does.
 *
 * Note the sizeof(struct vring_desc) is 16 bytes.
 */
#define VIRTIO_MAX_INDIRECT ((int) (PAGE_SIZE / 16))

/* The feature bitmap for virtio net */
#define VIRTIO_NET_F_CSUM	0	/* Host handles pkts w/ partial csum */
#define VIRTIO_NET_F_GUEST_CSUM	1	/* Guest handles pkts w/ partial csum */
#define VIRTIO_NET_F_MAC	5	/* Host has given MAC address. */
#define VIRTIO_NET_F_GUEST_TSO4	7	/* Guest can handle TSOv4 in. */
#define VIRTIO_NET_F_GUEST_TSO6	8	/* Guest can handle TSOv6 in. */
#define VIRTIO_NET_F_GUEST_ECN	9	/* Guest can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_GUEST_UFO	10	/* Guest can handle UFO in. */
#define VIRTIO_NET_F_HOST_TSO4	11	/* Host can handle TSOv4 in. */
#define VIRTIO_NET_F_HOST_TSO6	12	/* Host can handle TSOv6 in. */
#define VIRTIO_NET_F_HOST_ECN	13	/* Host can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_HOST_UFO	14	/* Host can handle UFO in. */
#define VIRTIO_NET_F_MRG_RXBUF	15	/* Host can merge receive buffers. */
#define VIRTIO_NET_F_STATUS	16	/* virtio_net_config.status available */
#define VIRTIO_NET_F_CTRL_VQ	17	/* Control channel available */
#define VIRTIO_NET_F_CTRL_RX	18	/* Control channel RX mode support */
#define VIRTIO_NET_F_CTRL_VLAN	19	/* Control channel VLAN filtering */
#define VIRTIO_NET_F_CTRL_RX_EXTRA 20	/* Extra RX mode control support */
#define VIRTIO_NET_F_GUEST_ANNOUNCE 21	/* Guest can announce device on the
					 * network */
#define VIRTIO_NET_F_MQ		22	/* Device supports Receive Flow
					 * Steering */
#define VIRTIO_NET_F_CTRL_MAC_ADDR 23	/* Set MAC address */

/* Do we get callbacks when the ring is completely used, even if we've
 * suppressed them? */
#define VIRTIO_F_NOTIFY_ON_EMPTY	24

/* Can the device handle any descriptor layout? */
#define VIRTIO_F_ANY_LAYOUT		27

/* We support indirect buffer descriptors */
#define VIRTIO_RING_F_INDIRECT_DESC	28

/*
 * Some VirtIO feature bits (currently bits 28 through 31) are
 * reserved for the transport being used (eg. virtio_ring), the
 * rest are per-device feature bits.
 */
#define VIRTIO_TRANSPORT_F_START 28
#define VIRTIO_TRANSPORT_F_END   32

/* The Guest publishes the used index for which it expects an interrupt
 * at the end of the avail ring. Host should ignore the avail->flags field. */
/* The Host publishes the avail index for which it expects a kick
 * at the end of the used ring. Guest should ignore the used->flags field. */
#define VIRTIO_RING_F_EVENT_IDX		29

#define VIRTIO_NET_S_LINK_UP	1	/* Link is up */
#define VIRTIO_NET_S_ANNOUNCE	2	/* Announcement is needed */

/*
 * Maximum number of virtqueues per device.
 */
#define VIRTIO_MAX_VIRTQUEUES 8

struct virtio_hw {
	struct virtqueue *cvq;
	uint32_t    io_base;
	uint32_t    guest_features;
	uint32_t    max_tx_queues;
	uint32_t    max_rx_queues;
	uint16_t    vtnet_hdr_size;
	uint8_t	    vlan_strip;
	uint8_t	    use_msix;
	uint8_t     started;
	uint8_t     mac_addr[ETHER_ADDR_LEN];
};

/*
 * This structure is just a reference to read
 * net device specific config space; it just a chodu structure
 *
 */
struct virtio_net_config {
	/* The config defining mac address (if VIRTIO_NET_F_MAC) */
	uint8_t    mac[ETHER_ADDR_LEN];
	/* See VIRTIO_NET_F_STATUS and VIRTIO_NET_S_* above */
	uint16_t   status;
	uint16_t   max_virtqueue_pairs;
} __attribute__((packed));

/*
 * The remaining space is defined by each driver as the per-driver
 * configuration space.
 */
#define VIRTIO_PCI_CONFIG(hw) (((hw)->use_msix) ? 24 : 20)

/*
 * How many bits to shift physical queue address written to QUEUE_PFN.
 * 12 is historical, and due to x86 page size.
 */
#define VIRTIO_PCI_QUEUE_ADDR_SHIFT 12

/* The alignment to use between consumer and producer parts of vring. */
#define VIRTIO_PCI_VRING_ALIGN 4096

#ifdef __FreeBSD__

static inline void
outb_p(unsigned char data, unsigned int port)
{

	outb(port, (u_char)data);
}

static inline void
outw_p(unsigned short data, unsigned int port)
{
	outw(port, (u_short)data);
}

static inline void
outl_p(unsigned int data, unsigned int port)
{
	outl(port, (u_int)data);
}
#endif

#define VIRTIO_PCI_REG_ADDR(hw, reg) \
	(unsigned short)((hw)->io_base + (reg))

#define VIRTIO_READ_REG_1(hw, reg) \
	inb((VIRTIO_PCI_REG_ADDR((hw), (reg))))
#define VIRTIO_WRITE_REG_1(hw, reg, value) \
	outb_p((unsigned char)(value), (VIRTIO_PCI_REG_ADDR((hw), (reg))))

#define VIRTIO_READ_REG_2(hw, reg) \
	inw((VIRTIO_PCI_REG_ADDR((hw), (reg))))
#define VIRTIO_WRITE_REG_2(hw, reg, value) \
	outw_p((unsigned short)(value), (VIRTIO_PCI_REG_ADDR((hw), (reg))))

#define VIRTIO_READ_REG_4(hw, reg) \
	inl((VIRTIO_PCI_REG_ADDR((hw), (reg))))
#define VIRTIO_WRITE_REG_4(hw, reg, value) \
	outl_p((unsigned int)(value), (VIRTIO_PCI_REG_ADDR((hw), (reg))))

static inline int
vtpci_with_feature(struct virtio_hw *hw, uint32_t bit)
{
	return (hw->guest_features & (1u << bit)) != 0;
}

/*
 * Function declaration from virtio_pci.c
 */
void vtpci_reset(struct virtio_hw *);

void vtpci_reinit_complete(struct virtio_hw *);

void vtpci_set_status(struct virtio_hw *, uint8_t);

uint32_t vtpci_negotiate_features(struct virtio_hw *, uint32_t);

void vtpci_write_dev_config(struct virtio_hw *, uint64_t, void *, int);

void vtpci_read_dev_config(struct virtio_hw *, uint64_t, void *, int);

uint8_t vtpci_isr(struct virtio_hw *);

uint16_t vtpci_irq_config(struct virtio_hw *, uint16_t);

#endif /* _VIRTIO_PCI_H_ */
