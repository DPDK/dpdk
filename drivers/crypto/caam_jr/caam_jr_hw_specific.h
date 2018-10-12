/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 NXP
 */

#ifndef CAAM_JR_HW_SPECIFIC_H
#define CAAM_JR_HW_SPECIFIC_H

#include <caam_jr_config.h>

/*
 * Offset to the registers of a job ring.
 * Is different for each job ring.
 */
#define CHAN_BASE(jr)   ((size_t)(jr)->register_base_addr)

#define SEC_JOB_RING_IS_FULL(pi, ci, ring_max_size, ring_threshold) \
		((((pi) + 1 + ((ring_max_size) - (ring_threshold))) & \
		  (ring_max_size - 1))  == ((ci)))

#define SEC_CIRCULAR_COUNTER(x, max)   (((x) + 1) & (max - 1))

/*
 * Assert that cond is true. If !cond is true, display str and the vararg list
 * in a printf-like syntax. also, if !cond is true, return altRet.
 *
 * \param cond          A boolean expression to be asserted true
 * \param altRet        The value to be returned if cond doesn't hold true
 * \param str           A quoted char string
 *
 * E.g.:
 *      SEC_ASSERT(ret > 0, 0, "ERROR initializing app: code = %d\n", ret);
 */
#define SEC_ASSERT(cond, altRet, ...) do {\
	if (unlikely(!(cond))) {\
		CAAM_JR_ERR(__VA_ARGS__); \
		return altRet; \
	} \
} while (0)

#define SEC_DP_ASSERT(cond, altRet, ...) do {\
	if (unlikely(!(cond))) {\
		CAAM_JR_DP_ERR(__VA_ARGS__); \
		return altRet; \
	} \
} while (0)

#define ASSERT(x)

/*
 * Constants representing various job ring registers
 */
#if CAAM_BYTE_ORDER == __BIG_ENDIAN
#define JR_REG_IRBA_OFFSET		0x0000
#define JR_REG_IRBA_OFFSET_LO		0x0004
#else
#define JR_REG_IRBA_OFFSET		0x0004
#define JR_REG_IRBA_OFFSET_LO		0x0000
#endif

#define JR_REG_IRSR_OFFSET		0x000C
#define JR_REG_IRSA_OFFSET		0x0014
#define JR_REG_IRJA_OFFSET		0x001C

#if CAAM_BYTE_ORDER == __BIG_ENDIAN
#define JR_REG_ORBA_OFFSET		0x0020
#define JR_REG_ORBA_OFFSET_LO		0x0024
#else
#define JR_REG_ORBA_OFFSET		0x0024
#define JR_REG_ORBA_OFFSET_LO		0x0020
#endif

#define JR_REG_ORSR_OFFSET		0x002C
#define JR_REG_ORJR_OFFSET		0x0034
#define JR_REG_ORSFR_OFFSET		0x003C
#define JR_REG_JROSR_OFFSET		0x0044
#define JR_REG_JRINT_OFFSET		0x004C

#define JR_REG_JRCFG_OFFSET		0x0050
#define JR_REG_JRCFG_OFFSET_LO		0x0054

#define JR_REG_IRRI_OFFSET		0x005C
#define JR_REG_ORWI_OFFSET		0x0064
#define JR_REG_JRCR_OFFSET		0x006C

/*
 * Constants for error handling on job ring
 */
#define JR_REG_JRINT_ERR_TYPE_SHIFT	8
#define JR_REG_JRINT_ERR_ORWI_SHIFT	16
#define JR_REG_JRINIT_JRE_SHIFT		1

#define JRINT_JRE			(1 << JR_REG_JRINIT_JRE_SHIFT)
#define JRINT_ERR_WRITE_STATUS		(1 << JR_REG_JRINT_ERR_TYPE_SHIFT)
#define JRINT_ERR_BAD_INPUT_BASE	(3 << JR_REG_JRINT_ERR_TYPE_SHIFT)
#define JRINT_ERR_BAD_OUTPUT_BASE	(4 << JR_REG_JRINT_ERR_TYPE_SHIFT)
#define JRINT_ERR_WRITE_2_IRBA		(5 << JR_REG_JRINT_ERR_TYPE_SHIFT)
#define JRINT_ERR_WRITE_2_ORBA		(6 << JR_REG_JRINT_ERR_TYPE_SHIFT)
#define JRINT_ERR_RES_B4_HALT		(7 << JR_REG_JRINT_ERR_TYPE_SHIFT)
#define JRINT_ERR_REM_TOO_MANY		(8 << JR_REG_JRINT_ERR_TYPE_SHIFT)
#define JRINT_ERR_ADD_TOO_MANY		(9 << JR_REG_JRINT_ERR_TYPE_SHIFT)
#define JRINT_ERR_HALT_MASK		0x0C
#define JRINT_ERR_HALT_INPROGRESS	0x04
#define JRINT_ERR_HALT_COMPLETE		0x08

#define JR_REG_JRCR_VAL_RESET		0x00000001

#define JR_REG_JRCFG_LO_ICTT_SHIFT	0x10
#define JR_REG_JRCFG_LO_ICDCT_SHIFT	0x08
#define JR_REG_JRCFG_LO_ICEN_EN		0x02

/*
 * Constants for Descriptor Processing errors
 */
#define SEC_HW_ERR_SSRC_NO_SRC		0x00
#define SEC_HW_ERR_SSRC_CCB_ERR		0x02
#define SEC_HW_ERR_SSRC_JMP_HALT_U	0x03
#define SEC_HW_ERR_SSRC_DECO		0x04
#define SEC_HW_ERR_SSRC_JR		0x06
#define SEC_HW_ERR_SSRC_JMP_HALT_COND   0x07

#define SEC_HW_ERR_DECO_HFN_THRESHOLD   0xF1
#define SEC_HW_ERR_CCB_ICV_CHECK_FAIL   0x0A

/*
 * Constants for descriptors
 */
/* Return higher 32 bits of physical address */
#define PHYS_ADDR_HI(phys_addr) \
	    (uint32_t)(((uint64_t)phys_addr) >> 32)

/* Return lower 32 bits of physical address */
#define PHYS_ADDR_LO(phys_addr) \
	    (uint32_t)(((uint64_t)phys_addr) & 0xFFFFFFFF)

/*
 * Macros for extracting error codes for the job ring
 */
#define JR_REG_JRINT_ERR_TYPE_EXTRACT(value)      ((value) & 0x00000F00)
#define JR_REG_JRINT_ERR_ORWI_EXTRACT(value)     \
	(((value) & 0x3FFF0000) >> JR_REG_JRINT_ERR_ORWI_SHIFT)
#define JR_REG_JRINT_JRE_EXTRACT(value)	   ((value) & JRINT_JRE)

/*
 * Macros for managing the job ring
 */
/* Read pointer to job ring input ring start address */
#if defined(RTE_ARCH_ARM64)
#define hw_get_inp_queue_base(jr) ((((dma_addr_t)GET_JR_REG(IRBA, \
				      (jr))) << 32) | \
				      (GET_JR_REG_LO(IRBA, (jr))))

/* Read pointer to job ring output ring start address */
#define hw_get_out_queue_base(jr) (((dma_addr_t)(GET_JR_REG(ORBA, \
				     (jr))) << 32) | \
				     (GET_JR_REG_LO(ORBA, (jr))))
#else
#define hw_get_inp_queue_base(jr)   ((dma_addr_t)(GET_JR_REG_LO(IRBA, (jr))))

#define hw_get_out_queue_base(jr)   ((dma_addr_t)(GET_JR_REG_LO(ORBA, (jr))))
#endif

/*
 * IRJA - Input Ring Jobs Added Register shows
 * how many new jobs were added to the Input Ring.
 */
#define hw_enqueue_desc_on_job_ring(job_ring) SET_JR_REG(IRJA, (job_ring), 1)

#define hw_set_input_ring_size(job_ring, size) SET_JR_REG(IRSR, job_ring, \
							 (size))

#define hw_set_output_ring_size(job_ring, size) SET_JR_REG(ORSR, job_ring, \
							  (size))

#if defined(RTE_ARCH_ARM64)
#define hw_set_input_ring_start_addr(job_ring, start_addr)	\
{								\
	SET_JR_REG(IRBA, job_ring, PHYS_ADDR_HI(start_addr));	\
	SET_JR_REG_LO(IRBA, job_ring, PHYS_ADDR_LO(start_addr));\
}

#define hw_set_output_ring_start_addr(job_ring, start_addr) \
{								\
	SET_JR_REG(ORBA, job_ring, PHYS_ADDR_HI(start_addr));	\
	SET_JR_REG_LO(ORBA, job_ring, PHYS_ADDR_LO(start_addr));\
}

#else
#define hw_set_input_ring_start_addr(job_ring, start_addr)	\
{								\
	SET_JR_REG(IRBA, job_ring, 0);	\
	SET_JR_REG_LO(IRBA, job_ring, PHYS_ADDR_LO(start_addr));\
}

#define hw_set_output_ring_start_addr(job_ring, start_addr) \
{								\
	SET_JR_REG(ORBA, job_ring, 0);	\
	SET_JR_REG_LO(ORBA, job_ring, PHYS_ADDR_LO(start_addr));\
}
#endif

/* ORJR - Output Ring Jobs Removed Register shows how many jobs were
 * removed from the Output Ring for processing by software. This is done after
 * the software has processed the entries.
 */
#define hw_remove_entries(jr, no_entries) SET_JR_REG(ORJR, (jr), (no_entries))

/* IRSA - Input Ring Slots Available register holds the number of entries in
 * the Job Ring's input ring. Once a job is enqueued, the value returned is
 * decremented by the hardware by the number of jobs enqueued.
 */
#define hw_get_available_slots(jr)		GET_JR_REG(IRSA, jr)

/* ORSFR - Output Ring Slots Full register holds the number of jobs which were
 * processed by the SEC and can be retrieved by the software. Once a job has
 * been processed by software, the user will call hw_remove_one_entry in order
 * to notify the SEC that the entry was processed.
 */
#define hw_get_no_finished_jobs(jr)		GET_JR_REG(ORSFR, jr)

/*
 * Macros for manipulating JR registers
 */
#if CORE_BYTE_ORDER == CAAM_BYTE_ORDER
#define sec_read_32(addr)	(*(volatile unsigned int *)(addr))
#define sec_write_32(addr, val)	(*(volatile unsigned int *)(addr) = (val))

#else
#define sec_read_32(addr)	rte_bswap32((*(volatile unsigned int *)(addr)))
#define sec_write_32(addr, val) \
			(*(volatile unsigned int *)(addr) = rte_bswap32(val))
#endif

#if CAAM_BYTE_ORDER == __LITTLE_ENDIAN
#define sec_read_64(addr)	(((u64)sec_read_32((u32 *)(addr) + 1) << 32) | \
				(sec_read_32((u32 *)(addr))))

#define sec_write_64(addr, val) {				\
	sec_write_32((u32 *)(addr) + 1, (u32)((val) >> 32));	\
	sec_write_32((u32 *)(addr), (u32)(val));		\
}
#else /* CAAM_BYTE_ORDER == __BIG_ENDIAN */
#define sec_read_64(addr)	(((u64)sec_read_32((u32 *)(addr)) << 32) | \
				(sec_read_32((u32 *)(addr) + 1)))

#define sec_write_64(addr, val) {				\
	sec_write_32((u32 *)(addr), (u32)((val) >> 32));	\
	sec_write_32((u32 *)(addr) + 1, (u32)(val));		\
}
#endif

#if defined(RTE_ARCH_ARM64)
#define sec_read_addr(a)	sec_read_64((a))
#define sec_write_addr(a, v)	sec_write_64((a), (v))
#else
#define sec_read_addr(a)	sec_read_32((a))
#define sec_write_addr(a, v)	sec_write_32((a), (v))
#endif

#define JR_REG(name, jr)	(CHAN_BASE(jr) + JR_REG_##name##_OFFSET)
#define JR_REG_LO(name, jr)	(CHAN_BASE(jr) + JR_REG_##name##_OFFSET_LO)

#define GET_JR_REG(name, jr)	(sec_read_32(JR_REG(name, (jr))))
#define GET_JR_REG_LO(name, jr)	(sec_read_32(JR_REG_LO(name, (jr))))

#define SET_JR_REG(name, jr, value) \
				(sec_write_32(JR_REG(name, (jr)), value))
#define SET_JR_REG_LO(name, jr, value) \
				(sec_write_32(JR_REG_LO(name, (jr)), value))

/* Lists the possible states for a job ring. */
typedef enum sec_job_ring_state_e {
	SEC_JOB_RING_STATE_STARTED,	/* Job ring is initialized */
	SEC_JOB_RING_STATE_RESET,	/* Job ring reset is in progress */
} sec_job_ring_state_t;

/* code or cmd block to caam */
struct sec_cdb {
	struct {
		union {
			uint32_t word;
			struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
				uint16_t rsvd63_48;
				unsigned int rsvd47_39:9;
				unsigned int idlen:7;
#else
				unsigned int idlen:7;
				unsigned int rsvd47_39:9;
				uint16_t rsvd63_48;
#endif
			} field;
		} __rte_packed hi;

		union {
			uint32_t word;
			struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
				unsigned int rsvd31_30:2;
				unsigned int fsgt:1;
				unsigned int lng:1;
				unsigned int offset:2;
				unsigned int abs:1;
				unsigned int add_buf:1;
				uint8_t pool_id;
				uint16_t pool_buffer_size;
#else
				uint16_t pool_buffer_size;
				uint8_t pool_id;
				unsigned int add_buf:1;
				unsigned int abs:1;
				unsigned int offset:2;
				unsigned int lng:1;
				unsigned int fsgt:1;
				unsigned int rsvd31_30:2;
#endif
			} field;
		} __rte_packed lo;
	} __rte_packed sh_hdr;

	uint32_t sh_desc[SEC_JOB_DESCRIPTOR_SIZE];
};

struct caam_jr_qp {
	struct sec_job_ring_t *ring;
	uint64_t rx_pkts;
	uint64_t rx_errs;
	uint64_t rx_poll_err;
	uint64_t tx_pkts;
	uint64_t tx_errs;
	uint64_t tx_ring_full;
};

struct sec_job_ring_t {
	/* TODO: Add wrapper macro to make it obvious this is the consumer index
	 * on the output ring
	 */
	uint32_t cidx;		/* Consumer index for job ring (jobs array).
				 * @note: cidx and pidx are accessed from
				 * different threads. Place the cidx and pidx
				 * inside the structure so that they lay on
				 * different cachelines, to avoid false sharing
				 * between threads when the threads run on
				 * different cores!
				 */
	/* TODO: Add wrapper macro to make it obvious this is the producer index
	 * on the input ring
	 */
	uint32_t pidx;		/* Producer index for job ring (jobs array) */

	phys_addr_t *input_ring;/* Ring of output descriptors received from SEC.
				 * Size of array is power of 2 to allow fast
				 * update of producer/consumer indexes with
				 * bitwise operations.
				 */

	struct sec_outring_entry *output_ring;
				/* Ring of output descriptors received from SEC.
				 * Size of array is power of 2 to allow fast
				 * update of producer/consumer indexes with
				 * bitwise operations.
				 */

	uint32_t irq_fd;	/* The file descriptor used for polling from
				 * user space for interrupts notifications
				 */
	uint32_t jr_mode;	/* Model used by SEC Driver to receive
				 * notifications from SEC.  Can be either
				 * of the three: #SEC_NOTIFICATION_TYPE_NAPI
				 * #SEC_NOTIFICATION_TYPE_IRQ or
				 * #SEC_NOTIFICATION_TYPE_POLL
				 */
	uint32_t napi_mode;	/* Job ring mode if NAPI mode is chosen
				 * Used only when jr_mode is set to
				 * #SEC_NOTIFICATION_TYPE_NAPI
				 */
	void *register_base_addr;	/* Base address for SEC's
					 * register memory for this job ring.
					 */
	uint8_t coalescing_en;		/* notifies if coelescing is
					 * enabled for the job ring
					 */
	sec_job_ring_state_t jr_state;	/* The state of this job ring */

	struct rte_mempool *ctx_pool;   /* per dev mempool for caam_jr_op_ctx */
	unsigned int max_nb_queue_pairs;
	unsigned int max_nb_sessions;
	struct caam_jr_qp qps[RTE_CAAM_MAX_NB_SEC_QPS]; /* i/o queue for sec */
};

/* Union describing the possible error codes that
 * can be set in the descriptor status word
 */
union hw_error_code {
	uint32_t error;
	union {
		struct {
			uint32_t ssrc:4;
			uint32_t ssed_val:28;
		} __rte_packed value;
		struct {
			uint32_t ssrc:4;
			uint32_t res:28;
		} __rte_packed no_status_src;
		struct {
			uint32_t ssrc:4;
			uint32_t jmp:1;
			uint32_t res:11;
			uint32_t desc_idx:8;
			uint32_t cha_id:4;
			uint32_t err_id:4;
		} __rte_packed ccb_status_src;
		struct {
			uint32_t ssrc:4;
			uint32_t jmp:1;
			uint32_t res:11;
			uint32_t desc_idx:8;
			uint32_t offset:8;
		} __rte_packed jmp_halt_user_src;
		struct {
			uint32_t ssrc:4;
			uint32_t jmp:1;
			uint32_t res:11;
			uint32_t desc_idx:8;
			uint32_t desc_err:8;
		} __rte_packed deco_src;
		struct {
			uint32_t ssrc:4;
			uint32_t res:17;
			uint32_t naddr:3;
			uint32_t desc_err:8;
		} __rte_packed jr_src;
		struct {
			uint32_t ssrc:4;
			uint32_t jmp:1;
			uint32_t res:11;
			uint32_t desc_idx:8;
			uint32_t cond:8;
		} __rte_packed jmp_halt_cond_src;
	} __rte_packed error_desc;
} __rte_packed;

/* @brief Initialize a job ring/channel in SEC device.
 * Write configuration register/s to properly initialize a job ring.
 *
 * @param [in] job_ring     The job ring
 *
 * @retval 0 for success
 * @retval other for error
 */
int hw_reset_job_ring(struct sec_job_ring_t *job_ring);

/* @brief Reset a job ring/channel in SEC device.
 * Write configuration register/s to reset a job ring.
 *
 * @param [in] job_ring     The job ring
 *
 * @retval 0 for success
 * @retval -1 in case job ring reset failed
 */
int hw_shutdown_job_ring(struct sec_job_ring_t *job_ring);

/* @brief Handle a job ring/channel error in SEC device.
 * Identify the error type and clear error bits if required.
 *
 * @param [in]  job_ring	The job ring
 * @param [in]  sec_error_code  The job ring's error code
 */
void hw_handle_job_ring_error(struct sec_job_ring_t *job_ring,
			      uint32_t sec_error_code);

/* @brief Handle a job ring error in the device.
 * Identify the error type and printout a explanatory
 * messages.
 *
 * @param [in]  job_ring	The job ring
 *
 */
void hw_job_ring_error_print(struct sec_job_ring_t *job_ring, int code);

/* @brief Set interrupt coalescing parameters on the Job Ring.
 * @param [in]  job_ring		The job ring
 * @param [in]  irq_coalesing_timer     Interrupt coalescing timer threshold.
 *					This value determines the maximum
 *					amount of time after processing a
 *					descriptor before raising an interrupt.
 * @param [in]  irq_coalescing_count    Interrupt coalescing descriptor count
 *					threshold.
 */
int hw_job_ring_set_coalescing_param(struct sec_job_ring_t *job_ring,
				     uint16_t irq_coalescing_timer,
				     uint8_t irq_coalescing_count);

/* @brief Enable interrupt coalescing on a job ring
 * @param [in]  job_ring		The job ring
 */
int hw_job_ring_enable_coalescing(struct sec_job_ring_t *job_ring);

/* @brief Disable interrupt coalescing on a job ring
 * @param [in]  job_ring		The job ring
 */
int hw_job_ring_disable_coalescing(struct sec_job_ring_t *job_ring);

#endif /* CAAM_JR_HW_SPECIFIC_H */
