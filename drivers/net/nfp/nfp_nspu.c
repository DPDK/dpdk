#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <rte_log.h>

#include "nfp_nfpu.h"

#define CFG_EXP_BAR_ADDR_SZ     1
#define CFG_EXP_BAR_MAP_TYPE	1

#define EXP_BAR_TARGET_SHIFT     23
#define EXP_BAR_LENGTH_SHIFT     27 /* 0=32, 1=64 bit increment */
#define EXP_BAR_MAP_TYPE_SHIFT   29 /* Bulk BAR map */

/* NFP target for NSP access */
#define NFP_NSP_TARGET   7

/*
 * This is an NFP internal address used for configuring properly an NFP
 * expansion BAR.
 */
#define MEM_CMD_BASE_ADDR       0x8100000000

/* NSP interface registers */
#define NSP_BASE                (MEM_CMD_BASE_ADDR + 0x22100)
#define NSP_STATUS              0x00
#define NSP_COMMAND             0x08
#define NSP_BUFFER		0x10
#define NSP_DEFAULT_BUFFER      0x18
#define NSP_DEFAULT_BUFFER_CFG  0x20

#define NSP_MAGIC                0xab10
#define NSP_STATUS_MAGIC(x)      (((x) >> 48) & 0xffff)
#define NSP_STATUS_MAJOR(x)      (int)(((x) >> 44) & 0xf)
#define NSP_STATUS_MINOR(x)      (int)(((x) >> 32) & 0xfff)

#define NSP_REG_ADDR(d, off, reg) ((uint8_t *)(d)->mem_base + (off) + (reg))
#define NSP_REG_VAL(p) (*(uint64_t *)(p))

/*
 * An NFP expansion BAR is configured for allowing access to a specific NFP
 * target:
 *
 *  IN:
 *	desc: struct with basic NSP addresses to work with
 *	expbar: NFP PF expansion BAR index to configure
 *	tgt: NFP target to configure access
 *	addr: NFP target address
 *
 *  OUT:
 *	pcie_offset: NFP PCI BAR offset to work with
 */
static void
nfp_nspu_mem_bar_cfg(nspu_desc_t *desc, int expbar, int tgt,
		     uint64_t addr, uint64_t *pcie_offset)
{
	uint64_t x, y, barsz;
	uint32_t *expbar_ptr;

	barsz = desc->barsz;

	/*
	 * NFP CPP address to configure. This comes from NFP 6000
	 * datasheet document based on Bulk mapping.
	 */
	x = (addr >> (barsz - 3)) << (21 - (40 - (barsz - 3)));
	x |= CFG_EXP_BAR_MAP_TYPE << EXP_BAR_MAP_TYPE_SHIFT;
	x |= CFG_EXP_BAR_ADDR_SZ << EXP_BAR_LENGTH_SHIFT;
	x |= tgt << EXP_BAR_TARGET_SHIFT;

	/* Getting expansion bar configuration register address */
	expbar_ptr = (uint32_t *)desc->cfg_base;
	/* Each physical PCI BAR has 8 NFP expansion BARs */
	expbar_ptr += (desc->pcie_bar * 8) + expbar;

	/* Writing to the expansion BAR register */
	*expbar_ptr = (uint32_t)x;

	/* Getting the pcie offset to work with from userspace */
	y = addr & ((uint64_t)(1 << (barsz - 3)) - 1);
	*pcie_offset = y;
}

/*
 * Configuring an expansion bar for accessing NSP userspace interface. This
 * function configures always the same expansion bar, which implies access to
 * previously configured NFP target is lost.
 */
static void
nspu_xlate(nspu_desc_t *desc, uint64_t addr, uint64_t *pcie_offset)
{
	nfp_nspu_mem_bar_cfg(desc, desc->exp_bar, NFP_NSP_TARGET, addr,
			     pcie_offset);
}

int
nfp_nsp_get_abi_version(nspu_desc_t *desc, int *major, int *minor)
{
	uint64_t pcie_offset;
	uint64_t nsp_reg;

	nspu_xlate(desc, NSP_BASE, &pcie_offset);
	nsp_reg = NSP_REG_VAL(NSP_REG_ADDR(desc, pcie_offset, NSP_STATUS));

	if (NSP_STATUS_MAGIC(nsp_reg) != NSP_MAGIC)
		return -1;

	*major = NSP_STATUS_MAJOR(nsp_reg);
	*minor = NSP_STATUS_MINOR(nsp_reg);

	return 0;
}

int
nfp_nspu_init(nspu_desc_t *desc, int nfp, int pcie_bar, size_t pcie_barsz,
	      int exp_bar, void *exp_bar_cfg_base, void *exp_bar_mmap)
{
	desc->nfp = nfp;
	desc->pcie_bar = pcie_bar;
	desc->exp_bar = exp_bar;
	desc->barsz = pcie_barsz;
	desc->cfg_base = exp_bar_cfg_base;
	desc->mem_base = exp_bar_mmap;

	return 0;
}
