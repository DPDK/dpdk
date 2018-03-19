/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef _CCP_DEV_H_
#define _CCP_DEV_H_

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <rte_bus_pci.h>
#include <rte_atomic.h>
#include <rte_byteorder.h>
#include <rte_io.h>
#include <rte_pci.h>
#include <rte_spinlock.h>
#include <rte_crypto_sym.h>
#include <rte_cryptodev.h>

/**< CCP sspecific */
#define MAX_HW_QUEUES                   5
#define CCP_MAX_TRNG_RETRIES		10
#define CCP_ALIGN(x, y) ((((x) + (y - 1)) / y) * y)

/**< CCP Register Mappings */
#define Q_MASK_REG                      0x000
#define TRNG_OUT_REG                    0x00c

/* CCP Version 5 Specifics */
#define CMD_QUEUE_MASK_OFFSET		0x00
#define	CMD_QUEUE_PRIO_OFFSET		0x04
#define CMD_REQID_CONFIG_OFFSET		0x08
#define	CMD_CMD_TIMEOUT_OFFSET		0x10
#define LSB_PUBLIC_MASK_LO_OFFSET	0x18
#define LSB_PUBLIC_MASK_HI_OFFSET	0x1C
#define LSB_PRIVATE_MASK_LO_OFFSET	0x20
#define LSB_PRIVATE_MASK_HI_OFFSET	0x24

#define CMD_Q_CONTROL_BASE		0x0000
#define CMD_Q_TAIL_LO_BASE		0x0004
#define CMD_Q_HEAD_LO_BASE		0x0008
#define CMD_Q_INT_ENABLE_BASE		0x000C
#define CMD_Q_INTERRUPT_STATUS_BASE	0x0010

#define CMD_Q_STATUS_BASE		0x0100
#define CMD_Q_INT_STATUS_BASE		0x0104

#define	CMD_CONFIG_0_OFFSET		0x6000
#define	CMD_TRNG_CTL_OFFSET		0x6008
#define	CMD_AES_MASK_OFFSET		0x6010
#define	CMD_CLK_GATE_CTL_OFFSET		0x603C

/* Address offset between two virtual queue registers */
#define CMD_Q_STATUS_INCR		0x1000

/* Bit masks */
#define CMD_Q_RUN			0x1
#define CMD_Q_SIZE			0x1F
#define CMD_Q_SHIFT			3
#define COMMANDS_PER_QUEUE		2048

#define QUEUE_SIZE_VAL                  ((ffs(COMMANDS_PER_QUEUE) - 2) & \
					 CMD_Q_SIZE)
#define Q_DESC_SIZE                     sizeof(struct ccp_desc)
#define Q_SIZE(n)                       (COMMANDS_PER_QUEUE*(n))

#define INT_COMPLETION                  0x1
#define INT_ERROR                       0x2
#define INT_QUEUE_STOPPED               0x4
#define ALL_INTERRUPTS                  (INT_COMPLETION| \
					 INT_ERROR| \
					 INT_QUEUE_STOPPED)

#define LSB_REGION_WIDTH                5
#define MAX_LSB_CNT                     8

#define LSB_SIZE                        16
#define LSB_ITEM_SIZE                   32
#define SLSB_MAP_SIZE                   (MAX_LSB_CNT * LSB_SIZE)
#define LSB_ENTRY_NUMBER(LSB_ADDR)      (LSB_ADDR / LSB_ITEM_SIZE)

/* General CCP Defines */

#define CCP_SB_BYTES                    32
/* Word 0 */
#define CCP_CMD_DW0(p)		((p)->dw0)
#define CCP_CMD_SOC(p)		(CCP_CMD_DW0(p).soc)
#define CCP_CMD_IOC(p)		(CCP_CMD_DW0(p).ioc)
#define CCP_CMD_INIT(p)	        (CCP_CMD_DW0(p).init)
#define CCP_CMD_EOM(p)		(CCP_CMD_DW0(p).eom)
#define CCP_CMD_FUNCTION(p)	(CCP_CMD_DW0(p).function)
#define CCP_CMD_ENGINE(p)	(CCP_CMD_DW0(p).engine)
#define CCP_CMD_PROT(p)	        (CCP_CMD_DW0(p).prot)

/* Word 1 */
#define CCP_CMD_DW1(p)		((p)->length)
#define CCP_CMD_LEN(p)		(CCP_CMD_DW1(p))

/* Word 2 */
#define CCP_CMD_DW2(p)		((p)->src_lo)
#define CCP_CMD_SRC_LO(p)	(CCP_CMD_DW2(p))

/* Word 3 */
#define CCP_CMD_DW3(p)		((p)->dw3)
#define CCP_CMD_SRC_MEM(p)	((p)->dw3.src_mem)
#define CCP_CMD_SRC_HI(p)	((p)->dw3.src_hi)
#define CCP_CMD_LSB_ID(p)	((p)->dw3.lsb_cxt_id)
#define CCP_CMD_FIX_SRC(p)	((p)->dw3.fixed)

/* Words 4/5 */
#define CCP_CMD_DW4(p)		((p)->dw4)
#define CCP_CMD_DST_LO(p)	(CCP_CMD_DW4(p).dst_lo)
#define CCP_CMD_DW5(p)		((p)->dw5.fields.dst_hi)
#define CCP_CMD_DST_HI(p)	(CCP_CMD_DW5(p))
#define CCP_CMD_DST_MEM(p)	((p)->dw5.fields.dst_mem)
#define CCP_CMD_FIX_DST(p)	((p)->dw5.fields.fixed)
#define CCP_CMD_SHA_LO(p)	((p)->dw4.sha_len_lo)
#define CCP_CMD_SHA_HI(p)	((p)->dw5.sha_len_hi)

/* Word 6/7 */
#define CCP_CMD_DW6(p)		((p)->key_lo)
#define CCP_CMD_KEY_LO(p)	(CCP_CMD_DW6(p))
#define CCP_CMD_DW7(p)		((p)->dw7)
#define CCP_CMD_KEY_HI(p)	((p)->dw7.key_hi)
#define CCP_CMD_KEY_MEM(p)	((p)->dw7.key_mem)

/* bitmap */
enum {
	BITS_PER_WORD = sizeof(unsigned long) * CHAR_BIT
};

#define WORD_OFFSET(b) ((b) / BITS_PER_WORD)
#define BIT_OFFSET(b)  ((b) % BITS_PER_WORD)

#define CCP_DIV_ROUND_UP(n, d)  (((n) + (d) - 1) / (d))
#define CCP_BITMAP_SIZE(nr) \
	CCP_DIV_ROUND_UP(nr, CHAR_BIT * sizeof(unsigned long))

#define CCP_BITMAP_FIRST_WORD_MASK(start) \
	(~0UL << ((start) & (BITS_PER_WORD - 1)))
#define CCP_BITMAP_LAST_WORD_MASK(nbits) \
	(~0UL >> (-(nbits) & (BITS_PER_WORD - 1)))

#define __ccp_round_mask(x, y) ((typeof(x))((y)-1))
#define ccp_round_down(x, y) ((x) & ~__ccp_round_mask(x, y))

/** CCP registers Write/Read */

static inline void ccp_pci_reg_write(void *base, int offset,
				     uint32_t value)
{
	volatile void *reg_addr = ((uint8_t *)base + offset);

	rte_write32((rte_cpu_to_le_32(value)), reg_addr);
}

static inline uint32_t ccp_pci_reg_read(void *base, int offset)
{
	volatile void *reg_addr = ((uint8_t *)base + offset);

	return rte_le_to_cpu_32(rte_read32(reg_addr));
}

#define CCP_READ_REG(hw_addr, reg_offset) \
	ccp_pci_reg_read(hw_addr, reg_offset)

#define CCP_WRITE_REG(hw_addr, reg_offset, value) \
	ccp_pci_reg_write(hw_addr, reg_offset, value)

TAILQ_HEAD(ccp_list, ccp_device);

extern struct ccp_list ccp_list;

/**
 * CCP device version
 */
enum ccp_device_version {
	CCP_VERSION_5A = 0,
	CCP_VERSION_5B,
};

/**
 * A structure describing a CCP command queue.
 */
struct ccp_queue {
	struct ccp_device *dev;
	char memz_name[RTE_MEMZONE_NAMESIZE];

	rte_atomic64_t free_slots;
	/**< available free slots updated from enq/deq calls */

	/* Queue identifier */
	uint64_t id;	/**< queue id */
	uint64_t qidx;	/**< queue index */
	uint64_t qsize;	/**< queue size */

	/* Queue address */
	struct ccp_desc *qbase_desc;
	void *qbase_addr;
	phys_addr_t qbase_phys_addr;
	/**< queue-page registers addr */
	void *reg_base;

	uint32_t qcontrol;
	/**< queue ctrl reg */

	int lsb;
	/**< lsb region assigned to queue */
	unsigned long lsbmask;
	/**< lsb regions queue can access */
	unsigned long lsbmap[CCP_BITMAP_SIZE(LSB_SIZE)];
	/**< all lsb resources which queue is using */
	uint32_t sb_key;
	/**< lsb assigned for queue */
	uint32_t sb_iv;
	/**< lsb assigned for iv */
	uint32_t sb_sha;
	/**< lsb assigned for sha ctx */
	uint32_t sb_hmac;
	/**< lsb assigned for hmac ctx */
} ____cacheline_aligned;

/**
 * A structure describing a CCP device.
 */
struct ccp_device {
	TAILQ_ENTRY(ccp_device) next;
	int id;
	/**< ccp dev id on platform */
	struct ccp_queue cmd_q[MAX_HW_QUEUES];
	/**< ccp queue */
	int cmd_q_count;
	/**< no. of ccp Queues */
	struct rte_pci_device pci;
	/**< ccp pci identifier */
	unsigned long lsbmap[CCP_BITMAP_SIZE(SLSB_MAP_SIZE)];
	/**< shared lsb mask of ccp */
	rte_spinlock_t lsb_lock;
	/**< protection for shared lsb region allocation */
	int qidx;
	/**< current queue index */
	int hwrng_retries;
	/**< retry counter for CCP TRNG */
} __rte_cache_aligned;

/**< CCP H/W engine related */
/**
 * ccp_engine - CCP operation identifiers
 *
 * @CCP_ENGINE_AES: AES operation
 * @CCP_ENGINE_XTS_AES: 128-bit XTS AES operation
 * @CCP_ENGINE_3DES: DES/3DES operation
 * @CCP_ENGINE_SHA: SHA operation
 * @CCP_ENGINE_RSA: RSA operation
 * @CCP_ENGINE_PASSTHRU: pass-through operation
 * @CCP_ENGINE_ZLIB_DECOMPRESS: unused
 * @CCP_ENGINE_ECC: ECC operation
 */
enum ccp_engine {
	CCP_ENGINE_AES = 0,
	CCP_ENGINE_XTS_AES_128,
	CCP_ENGINE_3DES,
	CCP_ENGINE_SHA,
	CCP_ENGINE_RSA,
	CCP_ENGINE_PASSTHRU,
	CCP_ENGINE_ZLIB_DECOMPRESS,
	CCP_ENGINE_ECC,
	CCP_ENGINE__LAST,
};

/* Passthru engine */
/**
 * ccp_passthru_bitwise - type of bitwise passthru operation
 *
 * @CCP_PASSTHRU_BITWISE_NOOP: no bitwise operation performed
 * @CCP_PASSTHRU_BITWISE_AND: perform bitwise AND of src with mask
 * @CCP_PASSTHRU_BITWISE_OR: perform bitwise OR of src with mask
 * @CCP_PASSTHRU_BITWISE_XOR: perform bitwise XOR of src with mask
 * @CCP_PASSTHRU_BITWISE_MASK: overwrite with mask
 */
enum ccp_passthru_bitwise {
	CCP_PASSTHRU_BITWISE_NOOP = 0,
	CCP_PASSTHRU_BITWISE_AND,
	CCP_PASSTHRU_BITWISE_OR,
	CCP_PASSTHRU_BITWISE_XOR,
	CCP_PASSTHRU_BITWISE_MASK,
	CCP_PASSTHRU_BITWISE__LAST,
};

/**
 * ccp_passthru_byteswap - type of byteswap passthru operation
 *
 * @CCP_PASSTHRU_BYTESWAP_NOOP: no byte swapping performed
 * @CCP_PASSTHRU_BYTESWAP_32BIT: swap bytes within 32-bit words
 * @CCP_PASSTHRU_BYTESWAP_256BIT: swap bytes within 256-bit words
 */
enum ccp_passthru_byteswap {
	CCP_PASSTHRU_BYTESWAP_NOOP = 0,
	CCP_PASSTHRU_BYTESWAP_32BIT,
	CCP_PASSTHRU_BYTESWAP_256BIT,
	CCP_PASSTHRU_BYTESWAP__LAST,
};

/**
 * CCP passthru
 */
struct ccp_passthru {
	phys_addr_t src_addr;
	phys_addr_t dest_addr;
	enum ccp_passthru_bitwise bit_mod;
	enum ccp_passthru_byteswap byte_swap;
	int len;
	int dir;
};

/* CCP version 5: Union to define the function field (cmd_reg1/dword0) */
union ccp_function {
	struct {
		uint16_t size:7;
		uint16_t encrypt:1;
		uint16_t mode:5;
		uint16_t type:2;
	} aes;
	struct {
		uint16_t size:7;
		uint16_t encrypt:1;
		uint16_t mode:5;
		uint16_t type:2;
	} des;
	struct {
		uint16_t size:7;
		uint16_t encrypt:1;
		uint16_t rsvd:5;
		uint16_t type:2;
	} aes_xts;
	struct {
		uint16_t rsvd1:10;
		uint16_t type:4;
		uint16_t rsvd2:1;
	} sha;
	struct {
		uint16_t mode:3;
		uint16_t size:12;
	} rsa;
	struct {
		uint16_t byteswap:2;
		uint16_t bitwise:3;
		uint16_t reflect:2;
		uint16_t rsvd:8;
	} pt;
	struct  {
		uint16_t rsvd:13;
	} zlib;
	struct {
		uint16_t size:10;
		uint16_t type:2;
		uint16_t mode:3;
	} ecc;
	uint16_t raw;
};


/**
 * descriptor for version 5 CPP commands
 * 8 32-bit words:
 * word 0: function; engine; control bits
 * word 1: length of source data
 * word 2: low 32 bits of source pointer
 * word 3: upper 16 bits of source pointer; source memory type
 * word 4: low 32 bits of destination pointer
 * word 5: upper 16 bits of destination pointer; destination memory
 * type
 * word 6: low 32 bits of key pointer
 * word 7: upper 16 bits of key pointer; key memory type
 */
struct dword0 {
	uint32_t soc:1;
	uint32_t ioc:1;
	uint32_t rsvd1:1;
	uint32_t init:1;
	uint32_t eom:1;
	uint32_t function:15;
	uint32_t engine:4;
	uint32_t prot:1;
	uint32_t rsvd2:7;
};

struct dword3 {
	uint32_t src_hi:16;
	uint32_t src_mem:2;
	uint32_t lsb_cxt_id:8;
	uint32_t rsvd1:5;
	uint32_t fixed:1;
};

union dword4 {
	uint32_t dst_lo;	/* NON-SHA */
	uint32_t sha_len_lo;	/* SHA */
};

union dword5 {
	struct {
		uint32_t dst_hi:16;
		uint32_t dst_mem:2;
		uint32_t rsvd1:13;
		uint32_t fixed:1;
	}
	fields;
	uint32_t sha_len_hi;
};

struct dword7 {
	uint32_t key_hi:16;
	uint32_t key_mem:2;
	uint32_t rsvd1:14;
};

struct ccp_desc {
	struct dword0 dw0;
	uint32_t length;
	uint32_t src_lo;
	struct dword3 dw3;
	union dword4 dw4;
	union dword5 dw5;
	uint32_t key_lo;
	struct dword7 dw7;
};

/**
 * ccp memory type
 */
enum ccp_memtype {
	CCP_MEMTYPE_SYSTEM = 0,
	CCP_MEMTYPE_SB,
	CCP_MEMTYPE_LOCAL,
	CCP_MEMTYPE_LAST,
};

/**
 * cmd id to follow order
 */
enum ccp_cmd_order {
	CCP_CMD_CIPHER = 0,
	CCP_CMD_AUTH,
	CCP_CMD_CIPHER_HASH,
	CCP_CMD_HASH_CIPHER,
	CCP_CMD_COMBINED,
	CCP_CMD_NOT_SUPPORTED,
};

static inline uint32_t
low32_value(unsigned long addr)
{
	return ((uint64_t)addr) & 0x0ffffffff;
}

static inline uint32_t
high32_value(unsigned long addr)
{
	return ((uint64_t)addr >> 32) & 0x00000ffff;
}

/*
 * Start CCP device
 */
int ccp_dev_start(struct rte_cryptodev *dev);

/**
 * Detect ccp platform and initialize all ccp devices
 *
 * @param ccp_id rte_pci_id list for supported CCP devices
 * @return no. of successfully initialized CCP devices
 */
int ccp_probe_devices(const struct rte_pci_id *ccp_id);

/**
 * allocate a ccp command queue
 *
 * @dev rte crypto device
 * @param slot_req number of required
 * @return allotted CCP queue on success otherwise NULL
 */
struct ccp_queue *ccp_allot_queue(struct rte_cryptodev *dev, int slot_req);

/**
 * read hwrng value
 *
 * @param trng_value data pointer to write RNG value
 * @return 0 on success otherwise -1
 */
int ccp_read_hwrng(uint32_t *trng_value);

#endif /* _CCP_DEV_H_ */
