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
#define NSP_DEFAULT_BUF         0x18
#define NSP_DEFAULT_BUF_CFG  0x20

#define NSP_MAGIC                0xab10
#define NSP_STATUS_MAGIC(x)      (((x) >> 48) & 0xffff)
#define NSP_STATUS_MAJOR(x)      (int)(((x) >> 44) & 0xf)
#define NSP_STATUS_MINOR(x)      (int)(((x) >> 32) & 0xfff)

/* NSP commands */
#define NSP_CMD_RESET			1

#define NSP_BUFFER_CFG_SIZE_MASK	(0xff)

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
	uint64_t offset, buffaddr;
	uint64_t nsp_reg;

	desc->nfp = nfp;
	desc->pcie_bar = pcie_bar;
	desc->exp_bar = exp_bar;
	desc->barsz = pcie_barsz;
	desc->windowsz = 1 << (desc->barsz - 3);
	desc->cfg_base = exp_bar_cfg_base;
	desc->mem_base = exp_bar_mmap;

	nspu_xlate(desc, NSP_BASE, &offset);

	/*
	 * Other NSPU clients can use other buffers. Let's tell NSPU we use the
	 * default buffer.
	 */
	buffaddr = NSP_REG_VAL(NSP_REG_ADDR(desc, offset, NSP_DEFAULT_BUF));
	NSP_REG_VAL(NSP_REG_ADDR(desc, offset, NSP_BUFFER)) = buffaddr;

	/* NFP internal addresses are 40 bits. Clean all other bits here */
	buffaddr = buffaddr & (((uint64_t)1 << 40) - 1);
	desc->bufaddr = buffaddr;

	/* Lets get information about the buffer */
	nsp_reg = NSP_REG_VAL(NSP_REG_ADDR(desc, offset, NSP_DEFAULT_BUF_CFG));

	/* Buffer size comes in MBs. Coversion to bytes */
	desc->buf_size = ((size_t)nsp_reg & NSP_BUFFER_CFG_SIZE_MASK) << 20;

	return 0;
}

#define NSPU_NFP_BUF(addr, base, off) \
	(*(uint64_t *)((uint8_t *)(addr)->mem_base + ((base) | (off))))

#define NSPU_HOST_BUF(base, off) (*(uint64_t *)((uint8_t *)(base) + (off)))

static int
nspu_buff_write(nspu_desc_t *desc, void *buffer, size_t size)
{
	uint64_t pcie_offset, pcie_window_base, pcie_window_offset;
	uint64_t windowsz = desc->windowsz;
	uint64_t buffaddr, j, i = 0;
	int ret = 0;

	if (size > desc->buf_size)
		return -1;

	buffaddr = desc->bufaddr;
	windowsz = desc->windowsz;

	while (i < size) {
		/* Expansion bar reconfiguration per window size */
		nspu_xlate(desc, buffaddr + i, &pcie_offset);
		pcie_window_base = pcie_offset & (~(windowsz - 1));
		pcie_window_offset = pcie_offset & (windowsz - 1);
		for (j = pcie_window_offset; ((j < windowsz) && (i < size));
		     j += 8) {
			NSPU_NFP_BUF(desc, pcie_window_base, j) =
				NSPU_HOST_BUF(buffer, i);
			i += 8;
		}
	}

	return ret;
}

static int
nspu_buff_read(nspu_desc_t *desc, void *buffer, size_t size)
{
	uint64_t pcie_offset, pcie_window_base, pcie_window_offset;
	uint64_t windowsz, i = 0, j;
	uint64_t buffaddr;
	int ret = 0;

	if (size > desc->buf_size)
		return -1;

	buffaddr = desc->bufaddr;
	windowsz = desc->windowsz;

	while (i < size) {
		/* Expansion bar reconfiguration per window size */
		nspu_xlate(desc, buffaddr + i, &pcie_offset);
		pcie_window_base = pcie_offset & (~(windowsz - 1));
		pcie_window_offset = pcie_offset & (windowsz - 1);
		for (j = pcie_window_offset; ((j < windowsz) && (i < size));
		     j += 8) {
			NSPU_HOST_BUF(buffer, i) =
				NSPU_NFP_BUF(desc, pcie_window_base, j);
			i += 8;
		}
	}

	return ret;
}

static int
nspu_command(nspu_desc_t *desc, uint16_t cmd, int read, int write,
		 void *buffer, size_t rsize, size_t wsize)
{
	uint64_t status, cmd_reg;
	uint64_t offset;
	int retry = 0;
	int retries = 120;
	int ret = 0;

	/* Same expansion BAR is used for different things */
	nspu_xlate(desc, NSP_BASE, &offset);

	status = NSP_REG_VAL(NSP_REG_ADDR(desc, offset, NSP_STATUS));

	while ((status & 0x1) && (retry < retries)) {
		status = NSP_REG_VAL(NSP_REG_ADDR(desc, offset, NSP_STATUS));
		retry++;
		sleep(1);
	}

	if (retry == retries)
		return -1;

	if (write) {
		ret = nspu_buff_write(desc, buffer, wsize);
		if (ret)
			return ret;

		/* Expansion BAR changes when writing the buffer */
		nspu_xlate(desc, NSP_BASE, &offset);
	}

	NSP_REG_VAL(NSP_REG_ADDR(desc, offset, NSP_COMMAND)) =
		(uint64_t)wsize << 32 | (uint64_t)cmd << 16 | 1;

	retry = 0;

	cmd_reg = NSP_REG_VAL(NSP_REG_ADDR(desc, offset, NSP_COMMAND));
	while ((cmd_reg & 0x1) && (retry < retries)) {
		cmd_reg = NSP_REG_VAL(NSP_REG_ADDR(desc, offset, NSP_COMMAND));
		retry++;
		sleep(1);
	}
	if (retry == retries)
		return -1;

	retry = 0;
	status = NSP_REG_VAL(NSP_REG_ADDR(desc, offset, NSP_STATUS));
	while ((status & 0x1) && (retry < retries)) {
		status = NSP_REG_VAL(NSP_REG_ADDR(desc, offset, NSP_STATUS));
		retry++;
		sleep(1);
	}

	if (retry == retries)
		return -1;

	ret = status & (0xff << 8);
	if (ret)
		return ret;

	if (read) {
		ret = nspu_buff_read(desc, buffer, rsize);
		if (ret)
			return ret;
	}

	return ret;
}

int
nfp_fw_reset(nspu_desc_t *nspu_desc)
{
	int res;

	res = nspu_command(nspu_desc, NSP_CMD_RESET, 0, 0, 0, 0, 0);

	if (res < 0)
		RTE_LOG(INFO, PMD, "fw reset failed: error %d", res);

	return res;
}
