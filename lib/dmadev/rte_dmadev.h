/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 HiSilicon Limited
 * Copyright(c) 2021 Intel Corporation
 * Copyright(c) 2021 Marvell International Ltd
 * Copyright(c) 2021 SmartShare Systems
 */

#ifndef RTE_DMADEV_H
#define RTE_DMADEV_H

/**
 * @file rte_dmadev.h
 *
 * DMA (Direct Memory Access) device API.
 *
 * The DMA framework is built on the following model:
 *
 *     ---------------   ---------------       ---------------
 *     | virtual DMA |   | virtual DMA |       | virtual DMA |
 *     | channel     |   | channel     |       | channel     |
 *     ---------------   ---------------       ---------------
 *            |                |                      |
 *            ------------------                      |
 *                     |                              |
 *               ------------                    ------------
 *               |  dmadev  |                    |  dmadev  |
 *               ------------                    ------------
 *                     |                              |
 *            ------------------               ------------------
 *            | HW DMA channel |               | HW DMA channel |
 *            ------------------               ------------------
 *                     |                              |
 *                     --------------------------------
 *                                     |
 *                           ---------------------
 *                           | HW DMA Controller |
 *                           ---------------------
 *
 * The DMA controller could have multiple HW-DMA-channels (aka. HW-DMA-queues),
 * each HW-DMA-channel should be represented by a dmadev.
 *
 * The dmadev could create multiple virtual DMA channels, each virtual DMA
 * channel represents a different transfer context. The DMA operation request
 * must be submitted to the virtual DMA channel. e.g. Application could create
 * virtual DMA channel 0 for memory-to-memory transfer scenario, and create
 * virtual DMA channel 1 for memory-to-device transfer scenario.
 *
 * This framework uses 'int16_t dev_id' as the device identifier of a dmadev,
 * and 'uint16_t vchan' as the virtual DMA channel identifier in one dmadev.
 *
 * The functions exported by the dmadev API to setup a device designated by its
 * device identifier must be invoked in the following order:
 *     - rte_dma_configure()
 *     - rte_dma_vchan_setup()
 *     - rte_dma_start()
 *
 * Then, the application can invoke dataplane functions to process jobs.
 *
 * If the application wants to change the configuration (i.e. invoke
 * rte_dma_configure() or rte_dma_vchan_setup()), it must invoke
 * rte_dma_stop() first to stop the device and then do the reconfiguration
 * before invoking rte_dma_start() again. The dataplane functions should not
 * be invoked when the device is stopped.
 *
 * Finally, an application can close a dmadev by invoking the rte_dma_close()
 * function.
 *
 * The dataplane APIs include two parts:
 * The first part is the submission of operation requests:
 *     - rte_dma_copy()
 *     - rte_dma_copy_sg()
 *     - rte_dma_fill()
 *     - rte_dma_submit()
 *
 * These APIs could work with different virtual DMA channels which have
 * different contexts.
 *
 * The first three APIs are used to submit the operation request to the virtual
 * DMA channel, if the submission is successful, a positive
 * ring_idx <= UINT16_MAX is returned, otherwise a negative number is returned.
 *
 * The last API is used to issue doorbell to hardware, and also there are flags
 * (@see RTE_DMA_OP_FLAG_SUBMIT) parameter of the first three APIs could do the
 * same work.
 * @note When enqueuing a set of jobs to the device, having a separate submit
 * outside a loop makes for clearer code than having a check for the last
 * iteration inside the loop to set a special submit flag.  However, for cases
 * where one item alone is to be submitted or there is a small set of jobs to
 * be submitted sequentially, having a submit flag provides a lower-overhead
 * way of doing the submission while still keeping the code clean.
 *
 * The second part is to obtain the result of requests:
 *     - rte_dma_completed()
 *         - return the number of operation requests completed successfully.
 *     - rte_dma_completed_status()
 *         - return the number of operation requests completed.
 *
 * @note If the dmadev works in silent mode (@see RTE_DMA_CAPA_SILENT),
 * application does not invoke the above two completed APIs.
 *
 * About the ring_idx which enqueue APIs (e.g. rte_dma_copy(), rte_dma_fill())
 * return, the rules are as follows:
 *     - ring_idx for each virtual DMA channel are independent.
 *     - For a virtual DMA channel, the ring_idx is monotonically incremented,
 *       when it reach UINT16_MAX, it wraps back to zero.
 *     - This ring_idx can be used by applications to track per-operation
 *       metadata in an application-defined circular ring.
 *     - The initial ring_idx of a virtual DMA channel is zero, after the
 *       device is stopped, the ring_idx needs to be reset to zero.
 *
 * One example:
 *     - step-1: start one dmadev
 *     - step-2: enqueue a copy operation, the ring_idx return is 0
 *     - step-3: enqueue a copy operation again, the ring_idx return is 1
 *     - ...
 *     - step-101: stop the dmadev
 *     - step-102: start the dmadev
 *     - step-103: enqueue a copy operation, the ring_idx return is 0
 *     - ...
 *     - step-x+0: enqueue a fill operation, the ring_idx return is 65535
 *     - step-x+1: enqueue a copy operation, the ring_idx return is 0
 *     - ...
 *
 * The DMA operation address used in enqueue APIs (i.e. rte_dma_copy(),
 * rte_dma_copy_sg(), rte_dma_fill()) is defined as rte_iova_t type.
 *
 * The dmadev supports two types of address: memory address and device address.
 *
 * - memory address: the source and destination address of the memory-to-memory
 * transfer type, or the source address of the memory-to-device transfer type,
 * or the destination address of the device-to-memory transfer type.
 * @note If the device support SVA (@see RTE_DMA_CAPA_SVA), the memory address
 * can be any VA address, otherwise it must be an IOVA address.
 *
 * - device address: the source and destination address of the device-to-device
 * transfer type, or the source address of the device-to-memory transfer type,
 * or the destination address of the memory-to-device transfer type.
 *
 * About MT-safe, all the functions of the dmadev API implemented by a PMD are
 * lock-free functions which assume to not be invoked in parallel on different
 * logical cores to work on the same target dmadev object.
 * @note Different virtual DMA channels on the same dmadev *DO NOT* support
 * parallel invocation because these virtual DMA channels share the same
 * HW-DMA-channel.
 */

#include <stdint.h>
#include <errno.h>

#include <rte_bitops.h>
#include <rte_common.h>
#include <rte_uuid.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of devices if rte_dma_dev_max() is not called. */
#define RTE_DMADEV_DEFAULT_MAX 64

/**
 * Configure the maximum number of dmadevs.
 * @note This function can be invoked before the primary process rte_eal_init()
 * to change the maximum number of dmadevs. If not invoked, the maximum number
 * of dmadevs is @see RTE_DMADEV_DEFAULT_MAX
 *
 * @param dev_max
 *   maximum number of dmadevs.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
int rte_dma_dev_max(size_t dev_max);

/**
 * Get the device identifier for the named DMA device.
 *
 * @param name
 *   DMA device name.
 *
 * @return
 *   Returns DMA device identifier on success.
 *   - <0: Failure to find named DMA device.
 */
int rte_dma_get_dev_id_by_name(const char *name);

/**
 * Check whether the dev_id is valid.
 *
 * @param dev_id
 *   DMA device index.
 *
 * @return
 *   - If the device index is valid (true) or not (false).
 */
bool rte_dma_is_valid(int16_t dev_id);

/**
 * Get the total number of DMA devices that have been successfully
 * initialised.
 *
 * @return
 *   The total number of usable DMA devices.
 */
uint16_t rte_dma_count_avail(void);

/**
 * Iterates over valid dmadev instances.
 *
 * @param start_dev_id
 *   The id of the next possible dmadev.
 * @return
 *   Next valid dmadev, UINT16_MAX if there is none.
 */
int16_t rte_dma_next_dev(int16_t start_dev_id);

/** Utility macro to iterate over all available dmadevs */
#define RTE_DMA_FOREACH_DEV(p) \
	for (p = rte_dma_next_dev(0); \
	     p != -1; \
	     p = rte_dma_next_dev(p + 1))


/**@{@name DMA capability
 * @see struct rte_dma_info::dev_capa
 */
/** Support memory-to-memory transfer */
#define RTE_DMA_CAPA_MEM_TO_MEM		RTE_BIT64(0)
/** Support memory-to-device transfer. */
#define RTE_DMA_CAPA_MEM_TO_DEV		RTE_BIT64(1)
/** Support device-to-memory transfer. */
#define RTE_DMA_CAPA_DEV_TO_MEM		RTE_BIT64(2)
/** Support device-to-device transfer. */
#define RTE_DMA_CAPA_DEV_TO_DEV		RTE_BIT64(3)
/** Support SVA which could use VA as DMA address.
 * If device support SVA then application could pass any VA address like memory
 * from rte_malloc(), rte_memzone(), malloc, stack memory.
 * If device don't support SVA, then application should pass IOVA address which
 * from rte_malloc(), rte_memzone().
 */
#define RTE_DMA_CAPA_SVA                RTE_BIT64(4)
/** Support work in silent mode.
 * In this mode, application don't required to invoke rte_dma_completed*()
 * API.
 * @see struct rte_dma_conf::silent_mode
 */
#define RTE_DMA_CAPA_SILENT             RTE_BIT64(5)
/** Supports error handling
 *
 * With this bit set, invalid input addresses will be reported as operation failures
 * to the user but other operations can continue.
 * Without this bit set, invalid data is not handled by either HW or driver, so user
 * must ensure that all memory addresses are valid and accessible by HW.
 */
#define RTE_DMA_CAPA_HANDLES_ERRORS	RTE_BIT64(6)
/** Support auto free for source buffer once mem to dev transfer is completed.
 *
 * @note Even though the DMA driver has this capability, it may not support all
 * mempool drivers. If the mempool is not supported by the DMA driver,
 * rte_dma_vchan_setup() will fail.
 */
#define RTE_DMA_CAPA_M2D_AUTO_FREE      RTE_BIT64(7)
/** Support strict priority scheduling.
 *
 * Application could assign fixed priority to the DMA device using 'priority'
 * field in struct rte_dma_conf. Number of supported priority levels will be
 * known from 'nb_priorities' field in struct rte_dma_info.
 */
#define RTE_DMA_CAPA_PRI_POLICY_SP	RTE_BIT64(8)
/** Support inter-process DMA transfers.
 *
 * When this bit is set, the DMA device can perform memory transfers
 * between different process memory spaces.
 */
#define RTE_DMA_CAPA_INTER_PROCESS_DOMAIN	RTE_BIT64(9)
/** Support inter-OS domain DMA transfers.
 *
 * The DMA device can perform memory transfers
 * across different operating system domains.
 */
#define RTE_DMA_CAPA_INTER_OS_DOMAIN		RTE_BIT64(10)

/** Support copy operation.
 * This capability start with index of 32, so that it could leave gap between
 * normal capability and ops capability.
 */
#define RTE_DMA_CAPA_OPS_COPY           RTE_BIT64(32)
/** Support scatter-gather list copy operation. */
#define RTE_DMA_CAPA_OPS_COPY_SG	RTE_BIT64(33)
/** Support fill operation. */
#define RTE_DMA_CAPA_OPS_FILL		RTE_BIT64(34)
/** Support enqueue and dequeue operations. */
#define RTE_DMA_CAPA_OPS_ENQ_DEQ	RTE_BIT64(35)
/**@}*/

/** DMA device configuration flags.
 * @see struct rte_dma_conf::flags
 */
/** Operate in silent mode
 * @see RTE_DMA_CAPA_SILENT
 */
#define RTE_DMA_CFG_FLAG_SILENT RTE_BIT64(0)
/** Enable enqueue and dequeue operations
 * @see RTE_DMA_CAPA_OPS_ENQ_DEQ
 */
#define RTE_DMA_CFG_FLAG_ENQ_DEQ RTE_BIT64(1)

/**
 * A structure used to retrieve the information of a DMA device.
 *
 * @see rte_dma_info_get
 */
struct rte_dma_info {
	const char *dev_name; /**< Unique device name. */
	/** Device capabilities (RTE_DMA_CAPA_*). */
	uint64_t dev_capa;
	/** Maximum number of virtual DMA channels supported. */
	uint16_t max_vchans;
	/** Maximum allowed number of virtual DMA channel descriptors. */
	uint16_t max_desc;
	/** Minimum allowed number of virtual DMA channel descriptors. */
	uint16_t min_desc;
	/** Maximum number of source or destination scatter-gather entry
	 * supported.
	 * If the device does not support COPY_SG capability, this value can be
	 * zero.
	 * If the device supports COPY_SG capability, then rte_dma_copy_sg()
	 * parameter nb_src/nb_dst should not exceed this value.
	 */
	uint16_t max_sges;
	/** NUMA node connection, -1 if unknown. */
	int16_t numa_node;
	/** Number of virtual DMA channel configured. */
	uint16_t nb_vchans;
	/** Number of priority levels (must be > 1) if priority scheduling is supported,
	 * 0 otherwise.
	 */
	uint16_t nb_priorities;
};

/**
 * Retrieve information of a DMA device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param[out] dev_info
 *   A pointer to a structure of type *rte_dma_info* to be filled with the
 *   information of the device.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
int rte_dma_info_get(int16_t dev_id, struct rte_dma_info *dev_info);

/**
 * A structure used to configure a DMA device.
 *
 * @see rte_dma_configure
 */
struct rte_dma_conf {
	/** The number of virtual DMA channels to set up for the DMA device.
	 * This value cannot be greater than the field 'max_vchans' of struct
	 * rte_dma_info which get from rte_dma_info_get().
	 */
	uint16_t nb_vchans;
	/* The priority of the DMA device.
	 * This value should be lower than the field 'nb_priorities' of struct
	 * rte_dma_info which get from rte_dma_info_get(). If the DMA device
	 * does not support priority scheduling, this value should be zero.
	 *
	 * Lowest value indicates higher priority and vice-versa.
	 */
	uint16_t priority;
	/** DMA device configuration flags defined as RTE_DMA_CFG_FLAG_*. */
	uint64_t flags;
};

/**
 * Configure a DMA device.
 *
 * This function must be invoked first before any other function in the
 * API. This function can also be re-invoked when a device is in the
 * stopped state.
 *
 * @param dev_id
 *   The identifier of the device to configure.
 * @param dev_conf
 *   The DMA device configuration structure encapsulated into rte_dma_conf
 *   object.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
int rte_dma_configure(int16_t dev_id, const struct rte_dma_conf *dev_conf);

/**
 * Start a DMA device.
 *
 * The device start step is the last one and consists of setting the DMA
 * to start accepting jobs.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
int rte_dma_start(int16_t dev_id);

/**
 * Stop a DMA device.
 *
 * The device can be restarted with a call to rte_dma_start().
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
int rte_dma_stop(int16_t dev_id);

/**
 * Close a DMA device.
 *
 * The device cannot be restarted after this call.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
int rte_dma_close(int16_t dev_id);

/**
 * DMA transfer direction defines.
 *
 * @see struct rte_dma_vchan_conf::direction
 */
enum rte_dma_direction {
	/** DMA transfer direction - from memory to memory.
	 * When the device supports inter-process or inter-OS domain transfers,
	 * the field `type` in `struct rte_dma_vchan_conf::domain`
	 * specifies the type of domain.
	 * For memory-to-memory transfers within the same domain or process,
	 * `type` should be set to `RTE_DMA_INTER_DOMAIN_NONE`.
	 *
	 * @see struct rte_dma_vchan_conf::direction
	 * @see struct rte_dma_inter_domain_param::type
	 */
	RTE_DMA_DIR_MEM_TO_MEM,
	/** DMA transfer direction - from memory to device.
	 * In a typical scenario, the SoCs are installed on host servers as
	 * iNICs through the PCIe interface. In this case, the SoCs works in
	 * EP(endpoint) mode, it could initiate a DMA move request from memory
	 * (which is SoCs memory) to device (which is host memory).
	 *
	 * @see struct rte_dma_vchan_conf::direction
	 */
	RTE_DMA_DIR_MEM_TO_DEV,
	/** DMA transfer direction - from device to memory.
	 * In a typical scenario, the SoCs are installed on host servers as
	 * iNICs through the PCIe interface. In this case, the SoCs works in
	 * EP(endpoint) mode, it could initiate a DMA move request from device
	 * (which is host memory) to memory (which is SoCs memory).
	 *
	 * @see struct rte_dma_vchan_conf::direction
	 */
	RTE_DMA_DIR_DEV_TO_MEM,
	/** DMA transfer direction - from device to device.
	 * In a typical scenario, the SoCs are installed on host servers as
	 * iNICs through the PCIe interface. In this case, the SoCs works in
	 * EP(endpoint) mode, it could initiate a DMA move request from device
	 * (which is host memory) to the device (which is another host memory).
	 *
	 * @see struct rte_dma_vchan_conf::direction
	 */
	RTE_DMA_DIR_DEV_TO_DEV,
};

/**
 * DMA access port type defines.
 *
 * @see struct rte_dma_port_param::port_type
 */
enum rte_dma_port_type {
	RTE_DMA_PORT_NONE,
	RTE_DMA_PORT_PCIE, /**< The DMA access port is PCIe. */
};

/**
 * A structure used to descript DMA access port parameters.
 *
 * @see struct rte_dma_vchan_conf::src_port
 * @see struct rte_dma_vchan_conf::dst_port
 */
struct rte_dma_port_param {
	/** The device access port type.
	 *
	 * @see enum rte_dma_port_type
	 */
	enum rte_dma_port_type port_type;
	union {
		/** PCIe access port parameters.
		 *
		 * The following model shows SoC's PCIe module connects to
		 * multiple PCIe hosts and multiple endpoints. The PCIe module
		 * has an integrated DMA controller.
		 *
		 * If the DMA wants to access the memory of host A, it can be
		 * initiated by PF1 in core0, or by VF0 of PF0 in core0.
		 *
		 * \code{.unparsed}
		 * System Bus
		 *    |     ----------PCIe module----------
		 *    |     Bus
		 *    |     Interface
		 *    |     -----        ------------------
		 *    |     |   |        | PCIe Core0     |
		 *    |     |   |        |                |        -----------
		 *    |     |   |        |   PF-0 -- VF-0 |        | Host A  |
		 *    |     |   |--------|        |- VF-1 |--------| Root    |
		 *    |     |   |        |   PF-1         |        | Complex |
		 *    |     |   |        |   PF-2         |        -----------
		 *    |     |   |        ------------------
		 *    |     |   |
		 *    |     |   |        ------------------
		 *    |     |   |        | PCIe Core1     |
		 *    |     |   |        |                |        -----------
		 *    |     |   |        |   PF-0 -- VF-0 |        | Host B  |
		 *    |-----|   |--------|   PF-1 -- VF-0 |--------| Root    |
		 *    |     |   |        |        |- VF-1 |        | Complex |
		 *    |     |   |        |   PF-2         |        -----------
		 *    |     |   |        ------------------
		 *    |     |   |
		 *    |     |   |        ------------------
		 *    |     |DMA|        |                |        ------
		 *    |     |   |        |                |--------| EP |
		 *    |     |   |--------| PCIe Core2     |        ------
		 *    |     |   |        |                |        ------
		 *    |     |   |        |                |--------| EP |
		 *    |     |   |        |                |        ------
		 *    |     -----        ------------------
		 *
		 * \endcode
		 *
		 * @note If some fields can not be supported by the
		 * hardware/driver, then the driver ignores those fields.
		 * Please check driver-specific documentation for limitations
		 * and capabilities.
		 */
		__extension__
		union {
			struct {
				uint64_t coreid : 4; /**< PCIe core id used. */
				uint64_t pfid : 8; /**< PF id used. */
				uint64_t vfen : 1; /**< VF enable bit. */
				uint64_t vfid : 16; /**< VF id used. */
				/** The pasid filed in TLP packet. */
				uint64_t pasid : 20;
				/** The attributes filed in TLP packet. */
				uint64_t attr : 3;
				/** The processing hint filed in TLP packet. */
				uint64_t ph : 2;
				/** The steering tag filed in TLP packet. */
				uint64_t st : 16;
			};
			uint64_t val;
		} pcie;
	};
	uint64_t reserved[2]; /**< Reserved for future fields. */
};

/**
 * A structure used for offload auto free params.
 */
struct rte_dma_auto_free_param {
	union {
		struct {
			/**
			 * Mempool from which buffer is allocated. Mempool info
			 * is used for freeing buffer by hardware.
			 *
			 * @note If the mempool is not supported by the DMA device,
			 * rte_dma_vchan_setup() will fail.
			 */
			struct rte_mempool *pool;
		} m2d;
	};
	/** Reserved for future fields. */
	uint64_t reserved[2];
};

/**
 * Inter-DMA transfer domain type.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * This enum defines the types of transfer domains applicable to DMA operations.
 * It helps categorize whether a DMA transfer is occurring within the same domain,
 * across different processes, or between distinct operating system domains.
 *
 * @see struct rte_dma_inter_domain_param:type
 */
enum rte_dma_inter_domain_type {
	/** No inter-domain transfer; standard DMA within same domain. */
	RTE_DMA_INTER_DOMAIN_NONE,
	/** Transfer occurs between different user-space processes. */
	RTE_DMA_INTER_PROCESS_DOMAIN,
	/** Transfer spans across different operating system domains. */
	RTE_DMA_INTER_OS_DOMAIN,
};

/**
 * Parameters for inter-process or inter-OS DMA transfers.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * This structure defines the parameters required to perform DMA transfers
 * across different domains, such as between processes or operating systems.
 * It includes the domain type and handler identifiers
 * for both the source and destination domains.
 *
 * When domain type is RTE_DMA_INTER_DOMAIN_NONE, both source and destination
 * handlers are invalid, DMA operation is confined within the local process.
 *
 * For DMA transfers between the local process or OS domain to another process
 * or OS domain, valid source and destination handlers must be provided.
 */
struct rte_dma_inter_domain_param {
	/** Type of inter-domain. */
	enum rte_dma_inter_domain_type type;
	/** Source domain handler identifier. */
	uint16_t src_handler;
	/** Destination domain handler identifier. */
	uint16_t dst_handler;
	/** Reserved for future fields. */
	uint64_t reserved[2];
};

/**
 * A structure used to configure a virtual DMA channel.
 *
 * @see rte_dma_vchan_setup
 */
struct rte_dma_vchan_conf {
	/** Transfer direction
	 *
	 * @see enum rte_dma_direction
	 */
	enum rte_dma_direction direction;
	/** Number of descriptor for the virtual DMA channel */
	uint16_t nb_desc;
	/** 1) Used to describes the device access port parameter in the
	 * device-to-memory transfer scenario.
	 * 2) Used to describes the source device access port parameter in the
	 * device-to-device transfer scenario.
	 *
	 * @see struct rte_dma_port_param
	 */
	struct rte_dma_port_param src_port;
	/** 1) Used to describes the device access port parameter in the
	 * memory-to-device transfer scenario.
	 * 2) Used to describes the destination device access port parameter in
	 * the device-to-device transfer scenario.
	 *
	 * @see struct rte_dma_port_param
	 */
	struct rte_dma_port_param dst_port;
	/** Buffer params to auto free buffer by hardware. To free the buffer
	 * by hardware, RTE_DMA_OP_FLAG_AUTO_FREE must be set while calling
	 * rte_dma_copy and rte_dma_copy_sg().
	 *
	 * @see RTE_DMA_OP_FLAG_AUTO_FREE
	 * @see struct rte_dma_auto_free_param
	 */
	struct rte_dma_auto_free_param auto_free;
	/** Parameters for inter-process or inter-OS domain DMA transfers.
	 * This field specifies the source and destination domain handlers
	 * required for DMA operations that span
	 * across different processes or operating system domains.
	 *
	 * @see RTE_DMA_CAPA_INTER_PROCESS_DOMAIN
	 * @see RTE_DMA_CAPA_INTER_OS_DOMAIN
	 * @see struct rte_dma_inter_domain_param
	 */
	struct rte_dma_inter_domain_param domain;
};

/**
 * Allocate and set up a virtual DMA channel.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel. The value must be in the range
 *   [0, nb_vchans - 1] previously supplied to rte_dma_configure().
 * @param conf
 *   The virtual DMA channel configuration structure encapsulated into
 *   rte_dma_vchan_conf object.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
int rte_dma_vchan_setup(int16_t dev_id, uint16_t vchan,
			const struct rte_dma_vchan_conf *conf);

/**
 * A structure used to retrieve statistics.
 *
 * @see rte_dma_stats_get
 */
struct rte_dma_stats {
	/** Count of operations which were submitted to hardware. */
	uint64_t submitted;
	/** Count of operations which were completed, including successful and
	 * failed completions.
	 */
	uint64_t completed;
	/** Count of operations which failed to complete. */
	uint64_t errors;
};

/**
 * Special ID, which is used to represent all virtual DMA channels.
 *
 * @see rte_dma_stats_get
 * @see rte_dma_stats_reset
 */
#define RTE_DMA_ALL_VCHAN	0xFFFFu

/**
 * Retrieve basic statistics of a or all virtual DMA channel(s).
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 *   If equal RTE_DMA_ALL_VCHAN means all channels.
 * @param[out] stats
 *   The basic statistics structure encapsulated into rte_dma_stats
 *   object.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
int rte_dma_stats_get(int16_t dev_id, uint16_t vchan,
		      struct rte_dma_stats *stats);

/**
 * Reset basic statistics of a or all virtual DMA channel(s).
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 *   If equal RTE_DMA_ALL_VCHAN means all channels.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
int rte_dma_stats_reset(int16_t dev_id, uint16_t vchan);

/**
 * device vchannel status
 *
 * Enum with the options for the channel status, either idle, active or halted due to error
 * @see rte_dma_vchan_status
 */
enum rte_dma_vchan_status {
	RTE_DMA_VCHAN_IDLE,          /**< not processing, awaiting ops */
	RTE_DMA_VCHAN_ACTIVE,        /**< currently processing jobs */
	RTE_DMA_VCHAN_HALTED_ERROR,  /**< not processing due to error, cannot accept new ops */
};

/**
 * Determine if all jobs have completed on a device channel.
 * This function is primarily designed for testing use, as it allows a process to check if
 * all jobs are completed, without actually gathering completions from those jobs.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 * @param[out] status
 *   The vchan status
 * @return
 *   0 - call completed successfully
 *   < 0 - error code indicating there was a problem calling the API
 */
int
rte_dma_vchan_status(int16_t dev_id, uint16_t vchan, enum rte_dma_vchan_status *status);

/**
 * Dump DMA device info.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param f
 *   The file to write the output to.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
int rte_dma_dump(int16_t dev_id, FILE *f);

/**
 * Event types for DMA access pair group notifications.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * When the event type is RTE_DMA_GROUP_EVENT_MEMBER_LEFT,
 * the handler associated with the departing member's domain is no longer valid.
 * Inter-domain DMA operations targeting that domain should be avoided.
 *
 * When the event type is RTE_DMA_GROUP_EVENT_GROUP_DESTROYED,
 * all handlers associated with the group become invalid.
 * No further inter-domain DMA operations should be initiated using those handlers.
 */
enum rte_dma_access_pair_group_event_type {
	/** A member left the group (notifies creator and joiners). */
	RTE_DMA_GROUP_EVENT_MEMBER_LEFT,
	/** Group was destroyed (notifies joiners). */
	RTE_DMA_GROUP_EVENT_GROUP_DESTROYED
};

/**
 * This callback is used to notify interested parties
 * (either the group creator or group joiners)
 * about significant events related to the lifecycle of a DMA access pair group.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * It can be registered by:
 * - **Group creators or group joiners** to be notified when a member leaves the group.
 * - **Group joiners** to be notified when the group is destroyed.
 *
 * @param dev_id
 *   Identifier of the DMA device.
 * @param group_id
 *   Identifier of the access pair group where the event occurred.
 * @param domain_id
 *   UUID of the domain_id associated with the event.
 *   For member leave events, this is the domain_id of the member that left.
 *   For group destruction events,
 *   this may refer to the domain_id of the respective member.
 * @param event
 *   Type of event that occurred.
 *   @see rte_dma_access_pair_group_event_type
 */
typedef void (*rte_dma_access_pair_group_event_cb_t)(int16_t dev_id,
	int16_t group_id,
	rte_uuid_t domain_id,
	enum rte_dma_access_pair_group_event_type event);

/**
 * Create an access pair group to enable secure DMA transfers
 * between devices across different processes or operating system domains.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @param dev_id
 *   Identifier of the DMA device initiating the group.
 * @param domain_id
 *   Unique identifier representing the current process or OS domain.
 * @param token
 *   Authentication token used to establish the access group.
 * @param[out] group_id
 *   Pointer to store the ID of the newly created access group.
 * @param cb
 *   Callback function to be invoked when a member leaves the group.
 *
 * @return
 *   0 on success,
 *   negative error code on failure.
 */
__rte_experimental
int rte_dma_access_pair_group_create(int16_t dev_id, rte_uuid_t domain_id, rte_uuid_t token,
				     int16_t *group_id, rte_dma_access_pair_group_event_cb_t cb);

/**
 * Destroy an access pair group if all participating devices have exited.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * This operation is only permitted by the device that originally created the group;
 * attempts by other devices will result in failure.
 *
 * @param dev_id
 *   Identifier of the device requesting group destruction.
 * @param group_id
 *   ID of the access group to be destroyed.
 * @return
 *   0 on success,
 *   negative value on failure indicating the error code.
 */
__rte_experimental
int rte_dma_access_pair_group_destroy(int16_t dev_id, int16_t group_id);

/**
 * Join an existing access group to enable secure DMA transfers
 * between devices across different processes or OS domains.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @param dev_id
 *   Identifier of the DMA device attempting to join the group.
 * @param domain_id
 *   Unique identifier representing the current process or OS domain.
 * @param token
 *   Authentication token used to validate group membership.
 * @param group_id
 *   ID of the access group to join.
 * @param cb
 *   Callback function to be invoked when the device leaves the group
 *   or when the group is destroyed due to some exception or failure.
 *
 * @return
 *   0 on success,
 *   negative value on failure indicating the error code.
 */
__rte_experimental
int rte_dma_access_pair_group_join(int16_t dev_id, rte_uuid_t domain_id, rte_uuid_t token,
				   int16_t group_id, rte_dma_access_pair_group_event_cb_t cb);

/**
 * Leave an access group, removing the device's entry from the group table
 * and disabling inter-domain DMA transfers to and from this device.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * This operation is not permitted for the device that originally created the group.
 *
 * @param dev_id
 *   Identifier of the device requesting to leave the group.
 * @param group_id
 *   ID of the access group to leave.
 * @return
 *   0 on success,
 *   negative value on failure indicating the error code.
 */
__rte_experimental
int rte_dma_access_pair_group_leave(int16_t dev_id, int16_t group_id);

/**
 * Retrieve the handler associated with a specific domain ID,
 * which is used by the application to query source or destinationin handler
 * to initiate inter-process or inter-OS DMA transfers.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @param dev_id
 *   Identifier of the DMA device requesting the handler.
 * @param group_id
 *   ID of the access group to query.
 * @param domain_id
 *   Unique identifier of the target process or OS domain.
 * @param[out] handler
 *   Pointer to store the retrieved handler value.
 * @return
 *   0 on success,
 *   negative value on failure indicating the error code.
 */
__rte_experimental
int rte_dma_access_pair_group_handler_get(int16_t dev_id, int16_t group_id, rte_uuid_t domain_id,
					  uint16_t *handler);

/**
 * DMA transfer result status code defines.
 *
 * @see rte_dma_completed_status
 */
enum rte_dma_status_code {
	/** The operation completed successfully. */
	RTE_DMA_STATUS_SUCCESSFUL,
	/** The operation failed to complete due abort by user.
	 * This is mainly used when processing dev_stop, user could modify the
	 * descriptors (e.g. change one bit to tell hardware abort this job),
	 * it allows outstanding requests to be complete as much as possible,
	 * so reduce the time to stop the device.
	 */
	RTE_DMA_STATUS_USER_ABORT,
	/** The operation failed to complete due to following scenarios:
	 * The jobs in a particular batch are not attempted because they
	 * appeared after a fence where a previous job failed. In some HW
	 * implementation it's possible for jobs from later batches would be
	 * completed, though, so report the status from the not attempted jobs
	 * before reporting those newer completed jobs.
	 */
	RTE_DMA_STATUS_NOT_ATTEMPTED,
	/** The operation failed to complete due invalid source address. */
	RTE_DMA_STATUS_INVALID_SRC_ADDR,
	/** The operation failed to complete due invalid destination address. */
	RTE_DMA_STATUS_INVALID_DST_ADDR,
	/** The operation failed to complete due invalid source or destination
	 * address, cover the case that only knows the address error, but not
	 * sure which address error.
	 */
	RTE_DMA_STATUS_INVALID_ADDR,
	/** The operation failed to complete due invalid length. */
	RTE_DMA_STATUS_INVALID_LENGTH,
	/** The operation failed to complete due invalid opcode.
	 * The DMA descriptor could have multiple format, which are
	 * distinguished by the opcode field.
	 */
	RTE_DMA_STATUS_INVALID_OPCODE,
	/** The operation failed to complete due bus read error. */
	RTE_DMA_STATUS_BUS_READ_ERROR,
	/** The operation failed to complete due bus write error. */
	RTE_DMA_STATUS_BUS_WRITE_ERROR,
	/** The operation failed to complete due bus error, cover the case that
	 * only knows the bus error, but not sure which direction error.
	 */
	RTE_DMA_STATUS_BUS_ERROR,
	/** The operation failed to complete due data poison. */
	RTE_DMA_STATUS_DATA_POISION,
	/** The operation failed to complete due descriptor read error. */
	RTE_DMA_STATUS_DESCRIPTOR_READ_ERROR,
	/** The operation failed to complete due device link error.
	 * Used to indicates that the link error in the memory-to-device/
	 * device-to-memory/device-to-device transfer scenario.
	 */
	RTE_DMA_STATUS_DEV_LINK_ERROR,
	/** The operation failed to complete due lookup page fault. */
	RTE_DMA_STATUS_PAGE_FAULT,
	/** The operation failed to complete due unknown reason.
	 * The initial value is 256, which reserves space for future errors.
	 */
	RTE_DMA_STATUS_ERROR_UNKNOWN = 0x100,
};

/**
 * A structure used to hold scatter-gather DMA operation request entry.
 *
 * @see rte_dma_copy_sg
 */
struct rte_dma_sge {
	rte_iova_t addr; /**< The DMA operation address. */
	uint32_t length; /**< The DMA operation length. */
};

/**
 * A structure used to hold event based DMA operation entry.
 * All the information required for a DMA transfer
 * shall be populated in "struct rte_dma_op" instance.
 */
struct rte_dma_op {
	/** Flags related to the operation.
	 * @see RTE_DMA_OP_FLAG_*
	 */
	uint64_t flags;
	/** Mempool from which op is allocated. */
	struct rte_mempool *op_mp;
	/** Status code for this operation. */
	enum rte_dma_status_code status;
	/** Reserved for future use. */
	uint32_t rsvd;
	/** Implementation-specific opaque data.
	 * A DMA device implementation use this field to hold
	 * implementation-specific values
	 * to share between dequeue and enqueue operations.
	 * The application should not modify this field.
	 */
	uint64_t impl_opaque[2];
	/** Memory to store user specific metadata.
	 * The DMA device implementation should not modify this area.
	 */
	uint64_t user_meta;
	/** Event metadata of DMA completion event.
	 * Used when RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_VCHAN_EV_BIND
	 * is not supported in OP_NEW mode.
	 * @see rte_event_dma_adapter_mode::RTE_EVENT_DMA_ADAPTER_OP_NEW
	 * @see RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_VCHAN_EV_BIND
	 *
	 * Used when RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD
	 * is not supported in OP_FWD mode.
	 * @see rte_event_dma_adapter_mode::RTE_EVENT_DMA_ADAPTER_OP_FORWARD
	 * @see RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD
	 *
	 * @see struct rte_event::event
	 */
	uint64_t event_meta;
	/** DMA device ID to be used with OP_FORWARD mode.
	 * @see rte_event_dma_adapter_mode::RTE_EVENT_DMA_ADAPTER_OP_FORWARD
	 */
	int16_t dma_dev_id;
	/** DMA vchan ID to be used with OP_FORWARD mode
	 * @see rte_event_dma_adapter_mode::RTE_EVENT_DMA_ADAPTER_OP_FORWARD
	 */
	uint16_t vchan;
	/** Number of source segments. */
	uint16_t nb_src;
	/** Number of destination segments. */
	uint16_t nb_dst;
	/** Source and destination segments. */
	struct rte_dma_sge src_dst_seg[];
};

#ifdef __cplusplus
}
#endif

#include "rte_dmadev_core.h"
#include "rte_dmadev_trace_fp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**@{@name DMA operation flag
 * @see rte_dma_copy()
 * @see rte_dma_copy_sg()
 * @see rte_dma_fill()
 */
/** Fence flag.
 * It means the operation with this flag must be processed only after all
 * previous operations are completed.
 * If the specify DMA HW works in-order (it means it has default fence between
 * operations), this flag could be NOP.
 */
#define RTE_DMA_OP_FLAG_FENCE   RTE_BIT64(0)
/** Submit flag.
 * It means the operation with this flag must issue doorbell to hardware after
 * enqueued jobs.
 */
#define RTE_DMA_OP_FLAG_SUBMIT  RTE_BIT64(1)
/** Write data to low level cache hint.
 * Used for performance optimization, this is just a hint, and there is no
 * capability bit for this, driver should not return error if this flag was set.
 */
#define RTE_DMA_OP_FLAG_LLC     RTE_BIT64(2)
/** Auto free buffer flag.
 * Operation with this flag must issue command to hardware to free the DMA
 * buffer after DMA transfer is completed.
 *
 * @see struct rte_dma_vchan_conf::auto_free
 */
#define RTE_DMA_OP_FLAG_AUTO_FREE	RTE_BIT64(3)
/**@}*/

/**
 * Enqueue a copy operation onto the virtual DMA channel.
 *
 * This queues up a copy operation to be performed by hardware, if the 'flags'
 * parameter contains RTE_DMA_OP_FLAG_SUBMIT then trigger doorbell to begin
 * this operation, otherwise do not trigger doorbell.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 * @param src
 *   The address of the source buffer.
 * @param dst
 *   The address of the destination buffer.
 * @param length
 *   The length of the data to be copied.
 * @param flags
 *   An flags for this operation.
 *   @see RTE_DMA_OP_FLAG_*
 *
 * @return
 *   - 0..UINT16_MAX: index of enqueued job.
 *   - -ENOSPC: if no space left to enqueue.
 *   - other values < 0 on failure.
 */
static inline int
rte_dma_copy(int16_t dev_id, uint16_t vchan, rte_iova_t src, rte_iova_t dst,
	     uint32_t length, uint64_t flags)
{
	struct rte_dma_fp_object *obj = &rte_dma_fp_objs[dev_id];
	int ret;

#ifdef RTE_DMADEV_DEBUG
	if (!rte_dma_is_valid(dev_id) || length == 0)
		return -EINVAL;
	if (obj->copy == NULL)
		return -ENOTSUP;
#endif

	ret = obj->copy(obj->dev_private, vchan, src, dst, length, flags);
	rte_dma_trace_copy(dev_id, vchan, src, dst, length, flags, ret);

	return ret;
}

/**
 * Enqueue a scatter-gather list copy operation onto the virtual DMA channel.
 *
 * This queues up a scatter-gather list copy operation to be performed by
 * hardware, if the 'flags' parameter contains RTE_DMA_OP_FLAG_SUBMIT then
 * trigger doorbell to begin this operation, otherwise do not trigger doorbell.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 * @param src
 *   The pointer of source scatter-gather entry array.
 * @param dst
 *   The pointer of destination scatter-gather entry array.
 * @param nb_src
 *   The number of source scatter-gather entry.
 *   @see struct rte_dma_info::max_sges
 * @param nb_dst
 *   The number of destination scatter-gather entry.
 *   @see struct rte_dma_info::max_sges
 * @param flags
 *   An flags for this operation.
 *   @see RTE_DMA_OP_FLAG_*
 *
 * @return
 *   - 0..UINT16_MAX: index of enqueued job.
 *   - -ENOSPC: if no space left to enqueue.
 *   - other values < 0 on failure.
 */
static inline int
rte_dma_copy_sg(int16_t dev_id, uint16_t vchan, struct rte_dma_sge *src,
		struct rte_dma_sge *dst, uint16_t nb_src, uint16_t nb_dst,
		uint64_t flags)
{
	struct rte_dma_fp_object *obj = &rte_dma_fp_objs[dev_id];
	int ret;

#ifdef RTE_DMADEV_DEBUG
	if (!rte_dma_is_valid(dev_id) || src == NULL || dst == NULL ||
	    nb_src == 0 || nb_dst == 0)
		return -EINVAL;
	if (obj->copy_sg == NULL)
		return -ENOTSUP;
#endif

	ret = obj->copy_sg(obj->dev_private, vchan, src, dst, nb_src, nb_dst, flags);
	rte_dma_trace_copy_sg(dev_id, vchan, src, dst, nb_src, nb_dst, flags,
			      ret);

	return ret;
}

/**
 * Enqueue a fill operation onto the virtual DMA channel.
 *
 * This queues up a fill operation to be performed by hardware, if the 'flags'
 * parameter contains RTE_DMA_OP_FLAG_SUBMIT then trigger doorbell to begin
 * this operation, otherwise do not trigger doorbell.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 * @param pattern
 *   The pattern to populate the destination buffer with.
 * @param dst
 *   The address of the destination buffer.
 * @param length
 *   The length of the destination buffer.
 * @param flags
 *   An flags for this operation.
 *   @see RTE_DMA_OP_FLAG_*
 *
 * @return
 *   - 0..UINT16_MAX: index of enqueued job.
 *   - -ENOSPC: if no space left to enqueue.
 *   - other values < 0 on failure.
 */
static inline int
rte_dma_fill(int16_t dev_id, uint16_t vchan, uint64_t pattern,
	     rte_iova_t dst, uint32_t length, uint64_t flags)
{
	struct rte_dma_fp_object *obj = &rte_dma_fp_objs[dev_id];
	int ret;

#ifdef RTE_DMADEV_DEBUG
	if (!rte_dma_is_valid(dev_id) || length == 0)
		return -EINVAL;
	if (obj->fill == NULL)
		return -ENOTSUP;
#endif

	ret = obj->fill(obj->dev_private, vchan, pattern, dst, length, flags);
	rte_dma_trace_fill(dev_id, vchan, pattern, dst, length, flags, ret);

	return ret;
}

/**
 * Trigger hardware to begin performing enqueued operations.
 *
 * Writes the "doorbell" to the hardware to trigger it
 * to begin the operations previously enqueued by rte_dma_copy/fill().
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 *
 * @return
 *   0 on success. Otherwise negative value is returned.
 */
static inline int
rte_dma_submit(int16_t dev_id, uint16_t vchan)
{
	struct rte_dma_fp_object *obj = &rte_dma_fp_objs[dev_id];
	int ret;

#ifdef RTE_DMADEV_DEBUG
	if (!rte_dma_is_valid(dev_id))
		return -EINVAL;
	if (obj->submit == NULL)
		return -ENOTSUP;
#endif

	ret = obj->submit(obj->dev_private, vchan);
	rte_dma_trace_submit(dev_id, vchan, ret);

	return ret;
}

/**
 * Return the number of operations that have been successfully completed.
 * Once an operation has been reported as completed, the results of that
 * operation will be visible to all cores on the system.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 * @param nb_cpls
 *   The maximum number of completed operations that can be processed.
 * @param[out] last_idx
 *   The last completed operation's ring_idx.
 *   If not required, NULL can be passed in.
 * @param[out] has_error
 *   Indicates if there are transfer error.
 *   If not required, NULL can be passed in.
 *
 * @return
 *   The number of operations that successfully completed. This return value
 *   must be less than or equal to the value of nb_cpls.
 */
static inline uint16_t
rte_dma_completed(int16_t dev_id, uint16_t vchan, const uint16_t nb_cpls,
		  uint16_t *last_idx, bool *has_error)
{
	struct rte_dma_fp_object *obj = &rte_dma_fp_objs[dev_id];
	uint16_t idx, ret;
	bool err;

#ifdef RTE_DMADEV_DEBUG
	if (!rte_dma_is_valid(dev_id) || nb_cpls == 0)
		return 0;
	if (obj->completed == NULL)
		return 0;
#endif

	/* Ensure the pointer values are non-null to simplify drivers.
	 * In most cases these should be compile time evaluated, since this is
	 * an inline function.
	 * - If NULL is explicitly passed as parameter, then compiler knows the
	 *   value is NULL
	 * - If address of local variable is passed as parameter, then compiler
	 *   can know it's non-NULL.
	 */
	if (last_idx == NULL)
		last_idx = &idx;
	if (has_error == NULL)
		has_error = &err;

	*has_error = false;
	ret = obj->completed(obj->dev_private, vchan, nb_cpls, last_idx, has_error);
	rte_dma_trace_completed(dev_id, vchan, nb_cpls, last_idx, has_error,
				ret);

	return ret;
}

/**
 * Return the number of operations that have been completed, and the operations
 * result may succeed or fail.
 * Once an operation has been reported as completed successfully, the results of that
 * operation will be visible to all cores on the system.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 * @param nb_cpls
 *   Indicates the size of status array.
 * @param[out] last_idx
 *   The last completed operation's ring_idx.
 *   If not required, NULL can be passed in.
 * @param[out] status
 *   This is a pointer to an array of length 'nb_cpls' that holds the completion
 *   status code of each operation.
 *   @see enum rte_dma_status_code
 *
 * @return
 *   The number of operations that completed. This return value must be less
 *   than or equal to the value of nb_cpls.
 *   If this number is greater than zero (assuming n), then n values in the
 *   status array are also set.
 */
static inline uint16_t
rte_dma_completed_status(int16_t dev_id, uint16_t vchan,
			 const uint16_t nb_cpls, uint16_t *last_idx,
			 enum rte_dma_status_code *status)
{
	struct rte_dma_fp_object *obj = &rte_dma_fp_objs[dev_id];
	uint16_t idx, ret;

#ifdef RTE_DMADEV_DEBUG
	if (!rte_dma_is_valid(dev_id) || nb_cpls == 0 || status == NULL)
		return 0;
	if (obj->completed_status == NULL)
		return 0;
#endif

	if (last_idx == NULL)
		last_idx = &idx;

	ret = obj->completed_status(obj->dev_private, vchan, nb_cpls, last_idx, status);
	rte_dma_trace_completed_status(dev_id, vchan, nb_cpls, last_idx, status,
				       ret);

	return ret;
}

/**
 * Check remaining capacity in descriptor ring for the current burst.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 *
 * @return
 *   - Remaining space in the descriptor ring for the current burst.
 *   - 0 on error
 */
static inline uint16_t
rte_dma_burst_capacity(int16_t dev_id, uint16_t vchan)
{
	struct rte_dma_fp_object *obj = &rte_dma_fp_objs[dev_id];
	uint16_t ret;

#ifdef RTE_DMADEV_DEBUG
	if (!rte_dma_is_valid(dev_id))
		return 0;
	if (obj->burst_capacity == NULL)
		return 0;
#endif
	ret = obj->burst_capacity(obj->dev_private, vchan);
	rte_dma_trace_burst_capacity(dev_id, vchan, ret);

	return ret;
}

/**
 * Enqueue rte_dma_ops to DMA device, can only be used underlying supports
 * RTE_DMA_CAPA_OPS_ENQ_DEQ and rte_dma_conf::enable_enq_deq is enabled in
 * rte_dma_configure().
 * The ops enqueued will be immediately submitted to the DMA device.
 * The enqueue should be coupled with dequeue to retrieve completed ops,
 * calls to rte_dma_submit(), rte_dma_completed() and rte_dma_completed_status()
 * are not valid.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 * @param ops
 *   Pointer to rte_dma_op array.
 * @param nb_ops
 *   Number of rte_dma_op in the ops array
 * @return uint16_t
 *   Number of successfully submitted ops.
 */
static inline uint16_t
rte_dma_enqueue_ops(int16_t dev_id, uint16_t vchan, struct rte_dma_op **ops, uint16_t nb_ops)
{
	struct rte_dma_fp_object *obj = &rte_dma_fp_objs[dev_id];
	uint16_t ret;

#ifdef RTE_DMADEV_DEBUG
	if (!rte_dma_is_valid(dev_id))
		return 0;
	if (*obj->enqueue == NULL)
		return 0;
#endif

	ret = (*obj->enqueue)(obj->dev_private, vchan, ops, nb_ops);
	rte_dma_trace_enqueue_ops(dev_id, vchan, (void **)ops, nb_ops);

	return ret;
}

/**
 * Dequeue completed rte_dma_ops submitted to the DMA device, can only be used
 * underlying supports RTE_DMA_CAPA_OPS_ENQ_DEQ and rte_dma_conf::enable_enq_deq
 * is enabled in rte_dma_configure().
 *
 * @param dev_id
 *   The identifier of the device.
 * @param vchan
 *   The identifier of virtual DMA channel.
 * @param ops
 *   Pointer to rte_dma_op array.
 * @param nb_ops
 *   Size of rte_dma_op array.
 * @return
 *   Number of successfully completed ops. Should be less or equal to nb_ops.
 */
static inline uint16_t
rte_dma_dequeue_ops(int16_t dev_id, uint16_t vchan, struct rte_dma_op **ops, uint16_t nb_ops)
{
	struct rte_dma_fp_object *obj = &rte_dma_fp_objs[dev_id];
	uint16_t ret;

#ifdef RTE_DMADEV_DEBUG
	if (!rte_dma_is_valid(dev_id))
		return 0;
	if (*obj->dequeue == NULL)
		return 0;
#endif

	ret = (*obj->dequeue)(obj->dev_private, vchan, ops, nb_ops);
	rte_dma_trace_dequeue_ops(dev_id, vchan, (void **)ops, nb_ops);

	return ret;
}

#ifdef __cplusplus
}
#endif

#endif /* RTE_DMADEV_H */
