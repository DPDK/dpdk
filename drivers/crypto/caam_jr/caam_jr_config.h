/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017-2018 NXP
 */

#ifndef CAAM_JR_CONFIG_H
#define CAAM_JR_CONFIG_H

#include <rte_byteorder.h>

#include <compat.h>

#ifdef RTE_LIBRTE_PMD_CAAM_JR_BE
#define CAAM_BYTE_ORDER __BIG_ENDIAN
#else
#define CAAM_BYTE_ORDER __LITTLE_ENDIAN
#endif

#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
#define CORE_BYTE_ORDER __BIG_ENDIAN
#else
#define CORE_BYTE_ORDER __LITTLE_ENDIAN
#endif

#if CORE_BYTE_ORDER != CAAM_BYTE_ORDER

#define cpu_to_caam64 rte_cpu_to_be_64
#define cpu_to_caam32 rte_cpu_to_be_32
#else
#define cpu_to_caam64
#define cpu_to_caam32

#endif

/*
 * SEC is configured to start work in polling mode,
 * when configured for NAPI notification style.
 */
#define SEC_STARTUP_POLLING_MODE     0
/*
 * SEC is configured to start work in interrupt mode,
 * when configured for NAPI notification style.
 */
#define SEC_STARTUP_INTERRUPT_MODE   1

/*
 * SEC driver will use NAPI model to receive notifications
 * for processed packets from SEC engine hardware:
 * - IRQ for low traffic
 * - polling for high traffic.
 */
#define SEC_NOTIFICATION_TYPE_NAPI  0
/*
 * SEC driver will use ONLY interrupts to receive notifications
 * for processed packets from SEC engine hardware.
 */
#define SEC_NOTIFICATION_TYPE_IRQ   1
/*
 * SEC driver will use ONLY polling to receive notifications
 * for processed packets from SEC engine hardware.
 */
#define SEC_NOTIFICATION_TYPE_POLL  2

/*
 * SEC USER SPACE DRIVER related configuration.
 */

/*
 * Determines how SEC user space driver will receive notifications
 * for processed packets from SEC engine.
 * Valid values are: #SEC_NOTIFICATION_TYPE_POLL, #SEC_NOTIFICATION_TYPE_IRQ
 * and #SEC_NOTIFICATION_TYPE_NAPI.
 */
#define SEC_NOTIFICATION_TYPE   SEC_NOTIFICATION_TYPE_POLL

/* Maximum number of job rings supported by SEC hardware */
#define MAX_SEC_JOB_RINGS         4

/* Maximum number of QP per job ring */
#define RTE_CAAM_MAX_NB_SEC_QPS    1

/*
 * Size of cryptographic context that is used directly in communicating
 * with SEC device. SEC device works only with physical addresses. This
 * is the maximum size for a SEC descriptor ( = 64 words).
 */
#define SEC_CRYPTO_DESCRIPTOR_SIZE  256

/*
 * Size of job descriptor submitted to SEC device for each packet to
 * be processed.
 * Job descriptor contains 3 DMA address pointers:
 *	- to shared descriptor, to input buffer and to output buffer.
 * The job descriptor contains other SEC specific commands as well:
 *	- HEADER command, SEQ IN PTR command SEQ OUT PTR command and opaque data
 *      each measuring 4 bytes.
 * Job descriptor size, depending on physical address representation:
 *	- 32 bit - size is 28 bytes - cacheline-aligned size is 64 bytes
 *	- 36 bit - size is 40 bytes - cacheline-aligned size is 64 bytes
 * @note: Job descriptor must be cacheline-aligned to ensure efficient
 *	memory access.
 * @note: If other format is used for job descriptor, then the size must be
 *	revised.
 */
#define SEC_JOB_DESCRIPTOR_SIZE     64

/*
 * Size of one entry in the input ring of a job ring.
 * Input ring contains pointers to job descriptors.
 * The memory used for an input ring and output ring must be physically
 * contiguous.
 */
#define SEC_JOB_INPUT_RING_ENTRY_SIZE  sizeof(dma_addr_t)

/*
 * Size of one entry in the output ring of a job ring.
 * Output ring entry is a pointer to a job descriptor followed by a 4 byte
 * status word.
 * The memory used for an input ring and output ring must be physically
 * contiguous.
 * @note If desired to use also the optional SEQ OUT indication in output ring
 * entries,
 * then 4 more bytes must be added to the size.
 */
#define SEC_JOB_OUTPUT_RING_ENTRY_SIZE  (SEC_JOB_INPUT_RING_ENTRY_SIZE + 4)

/*
 * DMA memory required for an input ring of a job ring.
 */
#define SEC_DMA_MEM_INPUT_RING_SIZE     ((SEC_JOB_INPUT_RING_ENTRY_SIZE) * \
					(SEC_JOB_RING_SIZE))

/*
 * DMA memory required for an output ring of a job ring.
 *  Required extra 4 byte for status word per each entry.
 */
#define SEC_DMA_MEM_OUTPUT_RING_SIZE    ((SEC_JOB_OUTPUT_RING_ENTRY_SIZE) * \
					(SEC_JOB_RING_SIZE))

/* DMA memory required for a job ring, including both input and output rings. */
#define SEC_DMA_MEM_JOB_RING_SIZE       ((SEC_DMA_MEM_INPUT_RING_SIZE) + \
					(SEC_DMA_MEM_OUTPUT_RING_SIZE))

/*
 * When calling sec_init() UA will provide an area of virtual memory
 *  of size #SEC_DMA_MEMORY_SIZE to be  used internally by the driver
 *  to allocate data (like SEC descriptors) that needs to be passed to
 *  SEC device in physical addressing and later on retrieved from SEC device.
 *  At initialization the UA provides specialized ptov/vtop functions/macros to
 *  translate addresses allocated from this memory area.
 */
#define SEC_DMA_MEMORY_SIZE          ((SEC_DMA_MEM_JOB_RING_SIZE) * \
					(MAX_SEC_JOB_RINGS))

#define L1_CACHE_BYTES 64

/* SEC JOB RING related configuration. */

/*
 * Configure the size of the JOB RING.
 * The maximum size of the ring in hardware limited to 1024.
 * However the number of packets in flight in a time interval of 1ms can
 * be calculated from the traffic rate (Mbps) and packet size.
 * Here it was considered a packet size of 64 bytes.
 *
 * @note Round up to nearest power of 2 for optimized update
 * of producer/consumer indexes of each job ring
 */
#define SEC_JOB_RING_SIZE     512

/*
 * Interrupt coalescing related configuration.
 * NOTE: SEC hardware enabled interrupt
 * coalescing is not supported on SEC version 3.1!
 * SEC version 4.4 has support for interrupt
 * coalescing.
 */

#if SEC_NOTIFICATION_TYPE != SEC_NOTIFICATION_TYPE_POLL

#define SEC_INT_COALESCING_ENABLE   1
/*
 * Interrupt Coalescing Descriptor Count Threshold.
 * While interrupt coalescing is enabled (ICEN=1), this value determines
 * how many Descriptors are completed before raising an interrupt.
 *
 * Valid values for this field are from 0 to 255.
 * Note that a value of 1 functionally defeats the advantages of interrupt
 * coalescing since the threshold value is reached each time that a
 * Job Descriptor is completed. A value of 0 is treated in the same
 * manner as a value of 1.
 */
#define SEC_INTERRUPT_COALESCING_DESCRIPTOR_COUNT_THRESH  10

/*
 * Interrupt Coalescing Timer Threshold.
 * While interrupt coalescing is enabled (ICEN=1), this value determines the
 * maximum amount of time after processing a Descriptor before raising an
 * interrupt.
 * The threshold value is represented in units equal to 64 CAAM interface
 * clocks. Valid values for this field are from 1 to 65535.
 * A value of 0 results in behavior identical to that when interrupt
 * coalescing is disabled.
 */
#define SEC_INTERRUPT_COALESCING_TIMER_THRESH  100
#endif /* SEC_NOTIFICATION_TYPE_POLL */

#endif /* CAAM_JR_CONFIG_H */
