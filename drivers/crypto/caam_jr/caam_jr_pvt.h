/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017-2018 NXP
 */

#ifndef CAAM_JR_PVT_H
#define CAAM_JR_PVT_H

#include <desc/ipsec.h>
#include <dpaax_iova_table.h>

/* NXP CAAM JR PMD device name */

#define CAAM_JR_ALG_UNSUPPORT	(-1)

/* Minimum job descriptor consists of a oneword job descriptor HEADER and
 * a pointer to the shared descriptor.
 */
#define MIN_JOB_DESC_SIZE	(CAAM_CMD_SZ + CAAM_PTR_SZ)
#define CAAM_JOB_DESC_SIZE	13

/* CTX_POOL_NUM_BUFS is set as per the ipsec-secgw application */
#define CTX_POOL_NUM_BUFS	32000
#define CTX_POOL_CACHE_SIZE	512

#define DIR_ENC                 1
#define DIR_DEC                 0

#define JR_MAX_NB_MAX_DIGEST	32

#define RTE_CAAM_JR_PMD_MAX_NB_SESSIONS 2048


/* Return codes for SEC user space driver APIs */
enum sec_return_code_e {
	SEC_SUCCESS = 0,	       /* Operation executed successfully.*/
	SEC_INVALID_INPUT_PARAM,       /* API received an invalid input
					* parameter
					*/
	SEC_OUT_OF_MEMORY,	       /* Memory allocation failed. */
	SEC_DESCRIPTOR_IN_FLIGHT,      /* API function indicates there are
					* descriptors in flight
					* for SEC to process.
					*/
	SEC_LAST_DESCRIPTOR_IN_FLIGHT, /* API function indicates there is one
					* last descriptor in flight
					* for SEC to process that.
					*/
	SEC_PROCESSING_ERROR,	       /* Indicates a SEC processing error
					* occurred on a Job Ring which requires
					* a SEC user space driver shutdown. Can
					* be returned from sec_poll_job_ring().
					* Then the only other API that can be
					* called after this error is
					* sec_release().
					*/
	SEC_DESC_PROCESSING_ERROR,     /* Indicates a SEC descriptor processing
					* error occurred on a Job Ring. Can be
					* returned from sec_poll_job_ring().
					* The driver was able to reset job ring
					* and job ring can be used like in a
					* normal case.
					*/
	SEC_JR_IS_FULL,			/* Job Ring is full. There is no more
					 * room in the JR for new descriptors.
					 * This can happen if the descriptor RX
					 * rate is higher than SEC's capacity.
					 */
	SEC_DRIVER_RELEASE_IN_PROGRESS, /* SEC driver shutdown is in progress,
					 * descriptors processing or polling is
					 * allowed.
					 */
	SEC_DRIVER_ALREADY_INITIALIZED, /* SEC driver is already initialized.*/
	SEC_DRIVER_NOT_INITIALIZED,	/* SEC driver is NOT initialized. */
	SEC_JOB_RING_RESET_IN_PROGRESS, /* Job ring is resetting due to a
					 * per-descriptor SEC processing error
					 * ::SEC_desc_PROCESSING_ERROR. Reset is
					 * finished when sec_poll_job_ring()
					 * return. Then the job ring can be used
					 * again.
					 */
	SEC_RESET_ENGINE_FAILED,	/* Resetting of SEC Engine by SEC Kernel
					 * Driver Failed
					 */
	SEC_ENABLE_IRQS_FAILED,		/* Enabling of IRQs in SEC Kernel Driver
					 * Failed
					 */
	SEC_DISABLE_IRQS_FAILED,	/* Disabling of IRQs in SEC Kernel
					 * Driver Failed
					 */
	/* END OF VALID VALUES */

	SEC_RETURN_CODE_MAX_VALUE,	/* Invalid value for return code. It is
					 * used to mark the end of the return
					 * code values. @note ALL new return
					 * code values MUST be added before
					 * ::SEC_RETURN_CODE_MAX_VALUE!
					 */
};

enum caam_jr_op_type {
	CAAM_JR_NONE,  /* No Cipher operations*/
	CAAM_JR_CIPHER,/* CIPHER operations */
	CAAM_JR_AUTH,  /* Authentication Operations */
	CAAM_JR_AEAD,  /* Authenticated Encryption with associated data */
	CAAM_JR_IPSEC, /* IPSEC protocol operations*/
	CAAM_JR_PDCP,  /* PDCP protocol operations*/
	CAAM_JR_PKC,   /* Public Key Cryptographic Operations */
	CAAM_JR_MAX
};

struct caam_jr_session {
	uint8_t dir;         /* Operation Direction */
	enum rte_crypto_cipher_algorithm cipher_alg; /* Cipher Algorithm*/
	enum rte_crypto_auth_algorithm auth_alg; /* Authentication Algorithm*/
	enum rte_crypto_aead_algorithm aead_alg; /* AEAD Algorithm*/
	enum rte_security_session_protocol proto_alg; /* Security Algorithm*/
	union {
		struct {
			uint8_t *data;	/* pointer to key data */
			size_t length;	/* key length in bytes */
		} aead_key;
		struct {
			struct {
				uint8_t *data;	/* pointer to key data */
				size_t length;	/* key length in bytes */
			} cipher_key;
			struct {
				uint8_t *data;	/* pointer to key data */
				size_t length;	/* key length in bytes */
			} auth_key;
		};
	};
	struct {
		uint16_t length;
		uint16_t offset;
	} iv;	/* Initialisation vector parameters */
	uint16_t auth_only_len; /* Length of data for Auth only */
	uint32_t digest_length;
	struct ipsec_encap_pdb encap_pdb;
	struct ip ip4_hdr;
	struct ipsec_decap_pdb decap_pdb;
	struct caam_jr_qp *qp;
	struct sec_cdb *cdb;	/* cmd block associated with qp */
	struct rte_mempool *ctx_pool; /* session mempool for caam_jr_op_ctx */
};

/*
 * 16-byte hardware scatter/gather table
 */

#define SEC4_SG_LEN_EXT		0x80000000	/* Entry points to table */
#define SEC4_SG_LEN_FIN		0x40000000	/* Last ent in table */
#define SEC4_SG_BPID_MASK	0x000000ff
#define SEC4_SG_BPID_SHIFT	16
#define SEC4_SG_LEN_MASK	0x3fffffff	/* Excludes EXT and FINAL */
#define SEC4_SG_OFFSET_MASK	0x00001fff

struct sec4_sg_entry {
	uint64_t ptr;
	uint32_t len;
	uint32_t bpid_offset;
};

#define MAX_SG_ENTRIES		16
#define SG_CACHELINE_0		0
#define SG_CACHELINE_1		4
#define SG_CACHELINE_2		8
#define SG_CACHELINE_3		12

/* Structure encompassing a job descriptor which is to be processed
 * by SEC. User should also initialise this structure with the callback
 * function pointer which will be called by driver after recieving proccessed
 * descriptor from SEC. User data is also passed in this data structure which
 * will be sent as an argument to the user callback function.
 */
struct job_descriptor {
	uint32_t desc[CAAM_JOB_DESC_SIZE];
};

struct caam_jr_op_ctx {
	struct job_descriptor jobdes;
	/* sg[0] output, sg[1] input, others are possible sub frames */
	struct sec4_sg_entry sg[MAX_SG_ENTRIES];
	struct rte_crypto_op *op;
	struct rte_mempool *ctx_pool; /* mempool pointer for caam_jr_op_ctx */
	int64_t vtop_offset;
	uint8_t digest[JR_MAX_NB_MAX_DIGEST];
};

/**
 * Checksum
 *
 * @param buffer calculate chksum for buffer
 * @param len    buffer length
 *
 * @return checksum value in host cpu order
 */
static inline uint16_t
calc_chksum(void *buffer, int len)
{
	uint16_t *buf = (uint16_t *)buffer;
	uint32_t sum = 0;
	uint16_t result;

	for (sum = 0; len > 1; len -= 2)
		sum += *buf++;

	if (len == 1)
		sum += *(unsigned char *)buf;

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	result = ~sum;

	return  result;
}
struct uio_job_ring {
	uint32_t jr_id;
	int uio_fd;
	void *register_base_addr;
	int map_size;
	int uio_minor_number;
};

int sec_cleanup(void);
int sec_configure(void);
void sec_uio_job_rings_init(void);
struct uio_job_ring *config_job_ring(void);
void free_job_ring(int uio_fd);

/* For Dma memory allocation of specified length and alignment */
static inline void *
caam_jr_dma_mem_alloc(size_t align, size_t len)
{
	return rte_malloc("mem_alloc", len, align);
}

/* For freeing dma memory */
static inline void
caam_jr_dma_free(void *ptr)
{
	rte_free(ptr);
}

static inline rte_iova_t
caam_jr_mem_vtop(void *vaddr)
{
	const struct rte_memseg *ms;

	ms = rte_mem_virt2memseg(vaddr, NULL);
	if (ms)
		return ms->iova + RTE_PTR_DIFF(vaddr, ms->addr);
	return (size_t)NULL;
}

static inline void *
caam_jr_dma_ptov(rte_iova_t paddr)
{
	void *va;
	va = dpaax_iova_table_get_va(paddr);
	if (likely(va != NULL))
		return va;

	return rte_mem_iova2virt(paddr);
}

/* Virtual to physical address conversion */
static inline rte_iova_t caam_jr_dma_vtop(void *ptr)
{
	return caam_jr_mem_vtop(ptr);
}

/** @brief Request to SEC kernel driver to enable interrupts for
 *         descriptor finished processing
 *  Use UIO to communicate with SEC kernel driver: write command
 *  value that indicates an IRQ enable action into UIO file descriptor
 *  of this job ring.
 *
 * @param [in]  uio_fd     Job Ring UIO File descriptor
 * @retval 0 for success
 * @retval -1 value for error
 */
uint32_t caam_jr_enable_irqs(int uio_fd);

/** @brief Request to SEC kernel driver to disable interrupts for descriptor
 *  finished processing
 *  Use UIO to communicate with SEC kernel driver: write command
 *  value that indicates an IRQ disable action into UIO file descriptor
 *  of this job ring.
 *
 * @param [in]  uio_fd    UIO File descripto
 * @retval 0 for success
 * @retval -1 value for error
 *
 */
uint32_t caam_jr_disable_irqs(int uio_fd);

#endif
