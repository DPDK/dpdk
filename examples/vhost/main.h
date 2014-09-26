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

#ifndef _MAIN_H_
#define _MAIN_H_

//#define DEBUG

#ifdef DEBUG
#define LOG_LEVEL RTE_LOG_DEBUG
#define LOG_DEBUG(log_type, fmt, args...) do {	\
	RTE_LOG(DEBUG, log_type, fmt, ##args);		\
} while (0)
#else
#define LOG_LEVEL RTE_LOG_INFO
#define LOG_DEBUG(log_type, fmt, args...) do{} while(0)
#endif

/* Macros for printing using RTE_LOG */
#define RTE_LOGTYPE_VHOST_CONFIG RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_VHOST_DATA   RTE_LOGTYPE_USER2
#define RTE_LOGTYPE_VHOST_PORT   RTE_LOGTYPE_USER3

/**
 * Information relating to memory regions including offsets to
 * addresses in host physical space.
 */
struct virtio_memory_regions_hpa {
	/**< Base guest physical address of region. */
	uint64_t    guest_phys_address;
	/**< End guest physical address of region. */
	uint64_t    guest_phys_address_end;
	/**< Size of region. */
	uint64_t    memory_size;
	/**< Offset of region for gpa to hpa translation. */
	uint64_t    host_phys_addr_offset;
};

/*
 * Device linked list structure for data path.
 */
struct vhost_dev {
	/**< Pointer to device created by vhost lib. */
	struct virtio_net      *dev;
	/**< Number of memory regions for gpa to hpa translation. */
	uint32_t nregions_hpa;
	/**< Memory region information for gpa to hpa translation. */
	struct virtio_memory_regions_hpa *regions_hpa;
	/**< Device MAC address (Obtained on first TX packet). */
	struct ether_addr mac_address;
	/**< RX VMDQ queue number. */
	uint16_t vmdq_rx_q;
	/**< Vlan tag assigned to the pool */
	uint32_t vlan_tag;
	/**< Data core that the device is added to. */
	uint16_t coreid;
	/**< A device is set as ready if the MAC address has been set. */
	volatile uint8_t ready;
	/**< Device is marked for removal from the data core. */
	volatile uint8_t remove;
} __rte_cache_aligned;

struct virtio_net_data_ll
{
	struct vhost_dev		*vdev;	/* Pointer to device created by configuration core. */
	struct virtio_net_data_ll	*next;  /* Pointer to next device in linked list. */
};

/*
 * Structure containing data core specific information.
 */
struct lcore_ll_info
{
	struct virtio_net_data_ll	*ll_root_free; 		/* Pointer to head in free linked list. */
	struct virtio_net_data_ll	*ll_root_used;		/* Pointer to head of used linked list. */
	uint32_t 					device_num;			/* Number of devices on lcore. */
	volatile uint8_t			dev_removal_flag;	/* Flag to synchronize device removal. */
};

struct lcore_info
{
	struct lcore_ll_info	*lcore_ll;	/* Pointer to data core specific lcore_ll_info struct */
};

#endif /* _MAIN_H_ */
