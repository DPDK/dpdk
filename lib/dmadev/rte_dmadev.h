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
 * About MT-safe, all the functions of the dmadev API implemented by a PMD are
 * lock-free functions which assume to not be invoked in parallel on different
 * logical cores to work on the same target dmadev object.
 * @note Different virtual DMA channels on the same dmadev *DO NOT* support
 * parallel invocation because these virtual DMA channels share the same
 * HW-DMA-channel.
 */

#include <stdint.h>

#include <rte_bitops.h>
#include <rte_common.h>
#include <rte_compat.h>
#include <rte_dev.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of devices if rte_dma_dev_max() is not called. */
#define RTE_DMADEV_DEFAULT_MAX 64

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
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
__rte_experimental
int rte_dma_dev_max(size_t dev_max);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get the device identifier for the named DMA device.
 *
 * @param name
 *   DMA device name.
 *
 * @return
 *   Returns DMA device identifier on success.
 *   - <0: Failure to find named DMA device.
 */
__rte_experimental
int rte_dma_get_dev_id_by_name(const char *name);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Check whether the dev_id is valid.
 *
 * @param dev_id
 *   DMA device index.
 *
 * @return
 *   - If the device index is valid (true) or not (false).
 */
__rte_experimental
bool rte_dma_is_valid(int16_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get the total number of DMA devices that have been successfully
 * initialised.
 *
 * @return
 *   The total number of usable DMA devices.
 */
__rte_experimental
uint16_t rte_dma_count_avail(void);

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
/** Support copy operation.
 * This capability start with index of 32, so that it could leave gap between
 * normal capability and ops capability.
 */
#define RTE_DMA_CAPA_OPS_COPY           RTE_BIT64(32)
/** Support scatter-gather list copy operation. */
#define RTE_DMA_CAPA_OPS_COPY_SG	RTE_BIT64(33)
/** Support fill operation. */
#define RTE_DMA_CAPA_OPS_FILL		RTE_BIT64(34)
/**@}*/

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
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
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
__rte_experimental
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
	/** Indicates whether to enable silent mode.
	 * false-default mode, true-silent mode.
	 * This value can be set to true only when the SILENT capability is
	 * supported.
	 *
	 * @see RTE_DMA_CAPA_SILENT
	 */
	bool enable_silent;
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
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
__rte_experimental
int rte_dma_configure(int16_t dev_id, const struct rte_dma_conf *dev_conf);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
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
__rte_experimental
int rte_dma_start(int16_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
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
__rte_experimental
int rte_dma_stop(int16_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
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
__rte_experimental
int rte_dma_close(int16_t dev_id);

/**
 * DMA transfer direction defines.
 *
 * @see struct rte_dma_vchan_conf::direction
 */
enum rte_dma_direction {
	/** DMA transfer direction - from memory to memory.
	 *
	 * @see struct rte_dma_vchan_conf::direction
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
	RTE_STD_C11
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
		 * and capablites.
		 */
		__extension__
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
		} pcie;
	};
	uint64_t reserved[2]; /**< Reserved for future fields. */
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
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
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
__rte_experimental
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
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
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
__rte_experimental
int rte_dma_stats_get(int16_t dev_id, uint16_t vchan,
		      struct rte_dma_stats *stats);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
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
__rte_experimental
int rte_dma_stats_reset(int16_t dev_id, uint16_t vchan);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
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
__rte_experimental
int rte_dma_dump(int16_t dev_id, FILE *f);

#ifdef __cplusplus
}
#endif

#endif /* RTE_DMADEV_H */
