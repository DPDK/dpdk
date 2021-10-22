/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 Cisco Systems, Inc.  All rights reserved.
 */

#ifndef _RTE_ETH_MEMIF_H_
#define _RTE_ETH_MEMIF_H_

#include <sys/queue.h>

#include <ethdev_driver.h>
#include <rte_ether.h>
#include <rte_interrupts.h>

#include "memif.h"

#define ETH_MEMIF_DEFAULT_SOCKET_FILENAME	"/run/memif.sock"
#define ETH_MEMIF_DEFAULT_RING_SIZE		10
#define ETH_MEMIF_DEFAULT_PKT_BUFFER_SIZE	2048

#define ETH_MEMIF_MAX_NUM_Q_PAIRS		255
#define ETH_MEMIF_MAX_LOG2_RING_SIZE		14
#define ETH_MEMIF_MAX_REGION_NUM		256

#define ETH_MEMIF_SHM_NAME_SIZE			32
#define ETH_MEMIF_DISC_STRING_SIZE		96
#define ETH_MEMIF_SECRET_SIZE			24

extern int memif_logtype;

#define MIF_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, memif_logtype, \
		"%s(): " fmt "\n", __func__, ##args)

enum memif_role_t {
	MEMIF_ROLE_SERVER,
	MEMIF_ROLE_CLIENT,
};

struct memif_region {
	void *addr;				/**< shared memory address */
	memif_region_size_t region_size;	/**< shared memory size */
	int fd;					/**< shared memory file descriptor */
	uint32_t pkt_buffer_offset;
	/**< offset from 'addr' to first packet buffer */
};

struct memif_queue {
	struct rte_mempool *mempool;		/**< mempool for RX packets */
	struct pmd_internals *pmd;		/**< device internals */

	memif_ring_type_t type;			/**< ring type */
	memif_region_index_t region;		/**< shared memory region index */

	uint16_t in_port;			/**< port id */

	memif_region_offset_t ring_offset;
	/**< ring offset from start of shm region (ring - memif_region.addr) */

	uint16_t last_head;			/**< last ring head */
	uint16_t last_tail;			/**< last ring tail */

	struct rte_mbuf **buffers;
	/**< Stored mbufs. Used in zero-copy tx. Client stores transmitted
	 * mbufs to free them once server has received them.
	 */

	/* rx/tx info */
	uint64_t n_pkts;			/**< number of rx/tx packets */
	uint64_t n_bytes;			/**< number of rx/tx bytes */

	struct rte_intr_handle *intr_handle;	/**< interrupt handle */

	memif_log2_ring_size_t log2_ring_size;	/**< log2 of ring size */
};

struct pmd_internals {
	memif_interface_id_t id;		/**< unique id */
	enum memif_role_t role;			/**< device role */
	uint32_t flags;				/**< device status flags */
#define ETH_MEMIF_FLAG_CONNECTING		(1 << 0)
/**< device is connecting */
#define ETH_MEMIF_FLAG_CONNECTED		(1 << 1)
/**< device is connected */
#define ETH_MEMIF_FLAG_ZERO_COPY		(1 << 2)
/**< device is zero-copy enabled */
#define ETH_MEMIF_FLAG_DISABLED			(1 << 3)
/**< device has not been configured and can not accept connection requests */
#define ETH_MEMIF_FLAG_SOCKET_ABSTRACT	(1 << 4)
/**< use abstract socket address */

	char *socket_filename;			/**< pointer to socket filename */
	char secret[ETH_MEMIF_SECRET_SIZE]; /**< secret (optional security parameter) */

	struct memif_control_channel *cc;	/**< control channel */
	rte_spinlock_t cc_lock;			/**< control channel lock */

	/* remote info */
	char remote_name[RTE_DEV_NAME_MAX_LEN];		/**< remote app name */
	char remote_if_name[RTE_DEV_NAME_MAX_LEN];	/**< remote peer name */

	struct {
		memif_log2_ring_size_t log2_ring_size; /**< log2 of ring size */
		uint8_t num_c2s_rings;		/**< number of client to server rings */
		uint8_t num_s2c_rings;		/**< number of server to client rings */
		uint16_t pkt_buffer_size;	/**< buffer size */
	} cfg;					/**< Configured parameters (max values) */

	struct {
		memif_log2_ring_size_t log2_ring_size; /**< log2 of ring size */
		uint8_t num_c2s_rings;		/**< number of client to server rings */
		uint8_t num_s2c_rings;		/**< number of server to client rings */
		uint16_t pkt_buffer_size;	/**< buffer size */
	} run;
	/**< Parameters used in active connection */

	char local_disc_string[ETH_MEMIF_DISC_STRING_SIZE];
	/**< local disconnect reason */
	char remote_disc_string[ETH_MEMIF_DISC_STRING_SIZE];
	/**< remote disconnect reason */
};

struct pmd_process_private {
	struct memif_region *regions[ETH_MEMIF_MAX_REGION_NUM];
	/**< shared memory regions */
	memif_region_index_t regions_num;	/**< number of regions */
};

/**
 * Unmap shared memory and free regions from memory.
 *
 * @param proc_private
 *   device process private data
 */
void memif_free_regions(struct rte_eth_dev *dev);

/**
 * Finalize connection establishment process. Map shared memory file
 * (server role), initialize ring queue, set link status up.
 *
 * @param dev
 *   memif device
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int memif_connect(struct rte_eth_dev *dev);

/**
 * Create shared memory file and initialize ring queue.
 * Only called by client when establishing connection
 *
 * @param dev
 *   memif device
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int memif_init_regions_and_queues(struct rte_eth_dev *dev);

/**
 * Get memif version string.
 *
 * @return
 *   - memif version string
 */
const char *memif_version(void);

#ifndef MFD_HUGETLB
#ifndef __NR_memfd_create

#if defined __x86_64__
#define __NR_memfd_create 319
#elif defined __x86_32__
#define __NR_memfd_create 1073742143
#elif defined __arm__
#define __NR_memfd_create 385
#elif defined __aarch64__
#define __NR_memfd_create 279
#elif defined __powerpc__
#define __NR_memfd_create 360
#elif defined __i386__
#define __NR_memfd_create 356
#else
#error "__NR_memfd_create unknown for this architecture"
#endif

#endif				/* __NR_memfd_create */

static inline int memfd_create(const char *name, unsigned int flags)
{
	return syscall(__NR_memfd_create, name, flags);
}
#endif				/* MFD_HUGETLB */

#ifndef F_LINUX_SPECIFIC_BASE
#define F_LINUX_SPECIFIC_BASE 1024
#endif

#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING       0x0002U
#endif

#ifndef F_ADD_SEALS
#define F_ADD_SEALS (F_LINUX_SPECIFIC_BASE + 9)
#define F_GET_SEALS (F_LINUX_SPECIFIC_BASE + 10)

#define F_SEAL_SEAL     0x0001	/* prevent further seals from being set */
#define F_SEAL_SHRINK   0x0002	/* prevent file from shrinking */
#define F_SEAL_GROW     0x0004	/* prevent file from growing */
#define F_SEAL_WRITE    0x0008	/* prevent writes */
#endif

#endif				/* RTE_ETH_MEMIF_H */
