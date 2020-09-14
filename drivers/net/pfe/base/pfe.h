/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#ifndef _PFE_H_
#define _PFE_H_

#include "cbus.h"

/*
 * WARNING: non atomic version.
 */
static inline void
set_bit(unsigned long nr, void *addr)
{
	int *m = ((int *)addr) + (nr >> 5);
	*m |= 1 << (nr & 31);
}

static inline int
test_bit(int nr, const void *addr)
{
	return (1UL & (((const int *)addr)[nr >> 5] >> (nr & 31))) != 0UL;
}

/*
 * WARNING: non atomic version.
 */
static inline void
clear_bit(unsigned long nr, void *addr)
{
	int *m = ((int *)addr) + (nr >> 5);
	*m &= ~(1 << (nr & 31));
}

/*
 * WARNING: non atomic version.
 */
static inline int
test_and_clear_bit(unsigned long nr, void *addr)
{
	unsigned long mask = 1 << (nr & 0x1f);
	int *m = ((int *)addr) + (nr >> 5);
	int old = *m;

	*m = old & ~mask;
	return (old & mask) != 0;
}

/*
 * WARNING: non atomic version.
 */
static inline int
test_and_set_bit(unsigned long nr, void *addr)
{
	unsigned long mask = 1 << (nr & 0x1f);
	int *m = ((int *)addr) + (nr >> 5);
	int old = *m;

	*m = old | mask;
	return (old & mask) != 0;
}

#ifndef BIT
#define BIT(nr)                (1UL << (nr))
#endif
#define CLASS_DMEM_BASE_ADDR(i)	(0x00000000 | ((i) << 20))
/*
 * Only valid for mem access register interface
 */
#define CLASS_IMEM_BASE_ADDR(i)	(0x00000000 | ((i) << 20))
#define CLASS_DMEM_SIZE	0x00002000
#define CLASS_IMEM_SIZE	0x00008000

#define TMU_DMEM_BASE_ADDR(i)	(0x00000000 + ((i) << 20))
/*
 * Only valid for mem access register interface
 */
#define TMU_IMEM_BASE_ADDR(i)	(0x00000000 + ((i) << 20))
#define TMU_DMEM_SIZE	0x00000800
#define TMU_IMEM_SIZE	0x00002000

#define UTIL_DMEM_BASE_ADDR	0x00000000
#define UTIL_DMEM_SIZE	0x00002000

#define PE_LMEM_BASE_ADDR	0xc3010000
#define PE_LMEM_SIZE	0x8000
#define PE_LMEM_END	(PE_LMEM_BASE_ADDR + PE_LMEM_SIZE)

#define DMEM_BASE_ADDR	0x00000000
#define DMEM_SIZE	0x2000	/* TMU has less... */
#define DMEM_END	(DMEM_BASE_ADDR + DMEM_SIZE)

#define PMEM_BASE_ADDR	0x00010000
#define PMEM_SIZE	0x8000	/* TMU has less... */
#define PMEM_END	(PMEM_BASE_ADDR + PMEM_SIZE)

#define writel(v, p) ({*(volatile unsigned int *)(p) = (v); })
#define readl(p) (*(const volatile unsigned int *)(p))

/* These check memory ranges from PE point of view/memory map */
#define IS_DMEM(addr, len)				\
	({ typeof(addr) addr_ = (addr);			\
	((unsigned long)(addr_) >= DMEM_BASE_ADDR) &&	\
	(((unsigned long)(addr_) + (len)) <= DMEM_END); })

#define IS_PMEM(addr, len)				\
	({ typeof(addr) addr_ = (addr);			\
	((unsigned long)(addr_) >= PMEM_BASE_ADDR) &&	\
	(((unsigned long)(addr_) + (len)) <= PMEM_END); })

#define IS_PE_LMEM(addr, len)				\
	({ typeof(addr) addr_ = (addr);			\
	((unsigned long)(addr_) >=			\
	PE_LMEM_BASE_ADDR) &&				\
	(((unsigned long)(addr_) +			\
	(len)) <= PE_LMEM_END); })

#define IS_PFE_LMEM(addr, len)				\
	({ typeof(addr) addr_ = (addr);			\
	((unsigned long)(addr_) >=			\
	CBUS_VIRT_TO_PFE(LMEM_BASE_ADDR)) &&		\
	(((unsigned long)(addr_) + (len)) <=		\
	CBUS_VIRT_TO_PFE(LMEM_END)); })

#define __IS_PHYS_DDR(addr, len)			\
	({ typeof(addr) addr_ = (addr);			\
	((unsigned long)(addr_) >=			\
	DDR_PHYS_BASE_ADDR) &&				\
	(((unsigned long)(addr_) + (len)) <=		\
	DDR_PHYS_END); })

#define IS_PHYS_DDR(addr, len)	__IS_PHYS_DDR(DDR_PFE_TO_PHYS(addr), len)

/*
 * If using a run-time virtual address for the cbus base address use this code
 */
extern void *cbus_base_addr;
extern void *ddr_base_addr;
extern unsigned long ddr_phys_base_addr;
extern unsigned int ddr_size;

#define CBUS_BASE_ADDR	cbus_base_addr
#define DDR_PHYS_BASE_ADDR	ddr_phys_base_addr
#define DDR_BASE_ADDR	ddr_base_addr
#define DDR_SIZE	ddr_size

#define DDR_PHYS_END	(DDR_PHYS_BASE_ADDR + DDR_SIZE)

#define LS1012A_PFE_RESET_WA	/*
				 * PFE doesn't have global reset and re-init
				 * should takecare few things to make PFE
				 * functional after reset
				 */
#define PFE_CBUS_PHYS_BASE_ADDR	0xc0000000	/* CBUS physical base address
						 * as seen by PE's.
						 */
/* CBUS physical base address as seen by PE's. */
#define PFE_CBUS_PHYS_BASE_ADDR_FROM_PFE	0xc0000000

#define DDR_PHYS_TO_PFE(p)	(((unsigned long)(p)) & 0x7FFFFFFF)
#define DDR_PFE_TO_PHYS(p)	(((unsigned long)(p)) | 0x80000000)
#define CBUS_PHYS_TO_PFE(p)	(((p) - PFE_CBUS_PHYS_BASE_ADDR) + \
				PFE_CBUS_PHYS_BASE_ADDR_FROM_PFE)
/* Translates to PFE address map */

#define DDR_PHYS_TO_VIRT(p)	(((p) - DDR_PHYS_BASE_ADDR) + DDR_BASE_ADDR)
#define DDR_VIRT_TO_PHYS(v)	(((v) - DDR_BASE_ADDR) + DDR_PHYS_BASE_ADDR)
#define DDR_VIRT_TO_PFE(p)	(DDR_PHYS_TO_PFE(DDR_VIRT_TO_PHYS(p)))

#define CBUS_VIRT_TO_PFE(v)	(((v) - CBUS_BASE_ADDR) + \
				PFE_CBUS_PHYS_BASE_ADDR)
#define CBUS_PFE_TO_VIRT(p)	(((unsigned long)(p) - \
				PFE_CBUS_PHYS_BASE_ADDR) + CBUS_BASE_ADDR)

/* The below part of the code is used in QOS control driver from host */
#define TMU_APB_BASE_ADDR       0xc1000000      /* TMU base address seen by
						 * pe's
						 */

enum {
	CLASS0_ID = 0,
	CLASS1_ID,
	CLASS2_ID,
	CLASS3_ID,
	CLASS4_ID,
	CLASS5_ID,
	TMU0_ID,
	TMU1_ID,
	TMU2_ID,
	TMU3_ID,
#if !defined(CONFIG_FSL_PFE_UTIL_DISABLED)
	UTIL_ID,
#endif
	MAX_PE
};

#define CLASS_MASK	(BIT(CLASS0_ID) | BIT(CLASS1_ID) |\
			BIT(CLASS2_ID) | BIT(CLASS3_ID) |\
			BIT(CLASS4_ID) | BIT(CLASS5_ID))
#define CLASS_MAX_ID	CLASS5_ID

#define TMU_MASK	(BIT(TMU0_ID) | BIT(TMU1_ID) |\
			BIT(TMU3_ID))

#define TMU_MAX_ID	TMU3_ID

#if !defined(CONFIG_FSL_PFE_UTIL_DISABLED)
#define UTIL_MASK	BIT(UTIL_ID)
#endif

struct pe_status {
	u32	cpu_state;
	u32	activity_counter;
	u32	rx;
	union {
	u32	tx;
	u32	tmu_qstatus;
	};
	u32	drop;
#if defined(CFG_PE_DEBUG)
	u32	debug_indicator;
	u32	debug[16];
#endif
} __rte_aligned(16);

struct pe_sync_mailbox {
	u32 stop;
	u32 stopped;
};

/* Drop counter definitions */

#define	CLASS_NUM_DROP_COUNTERS	13
#define	UTIL_NUM_DROP_COUNTERS	8

/* PE information.
 * Structure containing PE's specific information. It is used to create
 * generic C functions common to all PE's.
 * Before using the library functions this structure needs to be initialized
 * with the different registers virtual addresses
 * (according to the ARM MMU mmaping). The default initialization supports a
 * virtual == physical mapping.
 */
struct pe_info {
	u32 dmem_base_addr;	/* PE's dmem base address */
	u32 pmem_base_addr;	/* PE's pmem base address */
	u32 pmem_size;	/* PE's pmem size */

	void *mem_access_wdata;	/* PE's _MEM_ACCESS_WDATA register
				 * address
				 */
	void *mem_access_addr;	/* PE's _MEM_ACCESS_ADDR register
				 * address
				 */
	void *mem_access_rdata;	/* PE's _MEM_ACCESS_RDATA register
				 * address
				 */
};

void pe_lmem_read(u32 *dst, u32 len, u32 offset);
void pe_lmem_write(u32 *src, u32 len, u32 offset);

void pe_dmem_memcpy_to32(int id, u32 dst, const void *src, unsigned int len);
void pe_pmem_memcpy_to32(int id, u32 dst, const void *src, unsigned int len);

u32 pe_pmem_read(int id, u32 addr, u8 size);

void pe_dmem_write(int id, u32 val, u32 addr, u8 size);
u32 pe_dmem_read(int id, u32 addr, u8 size);
void class_pe_lmem_memcpy_to32(u32 dst, const void *src, unsigned int len);
void class_pe_lmem_memset(u32 dst, int val, unsigned int len);
void class_bus_write(u32 val, u32 addr, u8 size);
u32 class_bus_read(u32 addr, u8 size);

#define class_bus_readl(addr)	class_bus_read(addr, 4)
#define class_bus_readw(addr)	class_bus_read(addr, 2)
#define class_bus_readb(addr)	class_bus_read(addr, 1)

#define class_bus_writel(val, addr)	class_bus_write(val, addr, 4)
#define class_bus_writew(val, addr)	class_bus_write(val, addr, 2)
#define class_bus_writeb(val, addr)	class_bus_write(val, addr, 1)

#define pe_dmem_readl(id, addr)	pe_dmem_read(id, addr, 4)
#define pe_dmem_readw(id, addr)	pe_dmem_read(id, addr, 2)
#define pe_dmem_readb(id, addr)	pe_dmem_read(id, addr, 1)

#define pe_dmem_writel(id, val, addr)	pe_dmem_write(id, val, addr, 4)
#define pe_dmem_writew(id, val, addr)	pe_dmem_write(id, val, addr, 2)
#define pe_dmem_writeb(id, val, addr)	pe_dmem_write(id, val, addr, 1)

/*int pe_load_elf_section(int id, const void *data, elf32_shdr *shdr); */
//int pe_load_elf_section(int id, const void *data, struct elf32_shdr *shdr,
//			struct device *dev);

void pfe_lib_init(void *cbus_base, void *ddr_base, unsigned long ddr_phys_base,
		  unsigned int ddr_size);
void bmu_init(void *base, struct BMU_CFG *cfg);
void bmu_reset(void *base);
void bmu_enable(void *base);
void bmu_disable(void *base);
void bmu_set_config(void *base, struct BMU_CFG *cfg);

/*
 * An enumerated type for loopback values.  This can be one of three values, no
 * loopback -normal operation, local loopback with internal loopback module of
 * MAC or PHY loopback which is through the external PHY.
 */
#ifndef __MAC_LOOP_ENUM__
#define __MAC_LOOP_ENUM__
enum mac_loop {LB_NONE, LB_EXT, LB_LOCAL};
#endif

void gemac_init(void *base, void *config);
void gemac_disable_rx_checksum_offload(void *base);
void gemac_enable_rx_checksum_offload(void *base);
void gemac_set_mdc_div(void *base, int mdc_div);
void gemac_set_speed(void *base, enum mac_speed gem_speed);
void gemac_set_duplex(void *base, int duplex);
void gemac_set_mode(void *base, int mode);
void gemac_enable(void *base);
void gemac_tx_disable(void *base);
void gemac_tx_enable(void *base);
void gemac_disable(void *base);
void gemac_reset(void *base);
void gemac_set_address(void *base, struct spec_addr *addr);
struct spec_addr gemac_get_address(void *base);
void gemac_set_loop(void *base, enum mac_loop gem_loop);
void gemac_set_laddr1(void *base, struct pfe_mac_addr *address);
void gemac_set_laddr2(void *base, struct pfe_mac_addr *address);
void gemac_set_laddr3(void *base, struct pfe_mac_addr *address);
void gemac_set_laddr4(void *base, struct pfe_mac_addr *address);
void gemac_set_laddrN(void *base, struct pfe_mac_addr *address,
		      unsigned int entry_index);
void gemac_clear_laddr1(void *base);
void gemac_clear_laddr2(void *base);
void gemac_clear_laddr3(void *base);
void gemac_clear_laddr4(void *base);
void gemac_clear_laddrN(void *base, unsigned int entry_index);
struct pfe_mac_addr gemac_get_hash(void *base);
void gemac_set_hash(void *base, struct pfe_mac_addr *hash);
struct pfe_mac_addr gem_get_laddr1(void *base);
struct pfe_mac_addr gem_get_laddr2(void *base);
struct pfe_mac_addr gem_get_laddr3(void *base);
struct pfe_mac_addr gem_get_laddr4(void *base);
struct pfe_mac_addr gem_get_laddrN(void *base, unsigned int entry_index);
void gemac_set_config(void *base, struct gemac_cfg *cfg);
void gemac_allow_broadcast(void *base);
void gemac_no_broadcast(void *base);
void gemac_enable_1536_rx(void *base);
void gemac_disable_1536_rx(void *base);
int gemac_set_rx(void *base, int mtu);
void gemac_enable_rx_jmb(void *base);
void gemac_disable_rx_jmb(void *base);
void gemac_enable_stacked_vlan(void *base);
void gemac_disable_stacked_vlan(void *base);
void gemac_enable_pause_rx(void *base);
void gemac_disable_pause_rx(void *base);
void gemac_enable_pause_tx(void *base);
void gemac_disable_pause_tx(void *base);
void gemac_enable_copy_all(void *base);
void gemac_disable_copy_all(void *base);
void gemac_set_bus_width(void *base, int width);
void gemac_set_wol(void *base, u32 wol_conf);

void gpi_init(void *base, struct gpi_cfg *cfg);
void gpi_reset(void *base);
void gpi_enable(void *base);
void gpi_disable(void *base);
void gpi_set_config(void *base, struct gpi_cfg *cfg);

void hif_init(void);
void hif_tx_enable(void);
void hif_tx_disable(void);
void hif_rx_enable(void);
void hif_rx_disable(void);

/* Get Chip Revision level
 *
 */
static inline unsigned int CHIP_REVISION(void)
{
	/*For LS1012A return always 1 */
	return 1;
}

/* Start HIF rx DMA
 *
 */
static inline void hif_rx_dma_start(void)
{
	writel(HIF_CTRL_DMA_EN | HIF_CTRL_BDP_CH_START_WSTB, HIF_RX_CTRL);
}

/* Start HIF tx DMA
 *
 */
static inline void hif_tx_dma_start(void)
{
	writel(HIF_CTRL_DMA_EN | HIF_CTRL_BDP_CH_START_WSTB, HIF_TX_CTRL);
}


static inline void *pfe_mem_ptov(phys_addr_t paddr)
{
	return rte_mem_iova2virt(paddr);
}

static phys_addr_t pfe_mem_vtop(uint64_t vaddr) __rte_unused;

static inline phys_addr_t pfe_mem_vtop(uint64_t vaddr)
{
	const struct rte_memseg *memseg;

	memseg = rte_mem_virt2memseg((void *)(uintptr_t)vaddr, NULL);
	if (memseg)
		return memseg->iova + RTE_PTR_DIFF(vaddr, memseg->addr);

	return (size_t)NULL;
}

#endif /* _PFE_H_ */
