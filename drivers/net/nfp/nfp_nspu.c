#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <rte_log.h>
#include <rte_byteorder.h>

#include "nfp_nfpu.h"

#define CFG_EXP_BAR_ADDR_SZ     1
#define CFG_EXP_BAR_MAP_TYPE	1

#define EXP_BAR_TARGET_SHIFT     23
#define EXP_BAR_LENGTH_SHIFT     27 /* 0=32, 1=64 bit increment */
#define EXP_BAR_MAP_TYPE_SHIFT   29 /* Bulk BAR map */

/* NFP target for NSP access */
#define NFP_NSP_TARGET   7

/* Expansion BARs for mapping PF vnic BARs */
#define NFP_NET_PF_CFG_EXP_BAR          6
#define NFP_NET_PF_HW_QUEUES_EXP_BAR    5

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
#define NSP_CMD_RESET                   1
#define NSP_CMD_FW_LOAD                 6
#define NSP_CMD_READ_ETH_TABLE          7
#define NSP_CMD_WRITE_ETH_TABLE         8
#define NSP_CMD_GET_SYMBOL             14

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

static int
nfp_fw_reset(nspu_desc_t *nspu_desc)
{
	int res;

	res = nspu_command(nspu_desc, NSP_CMD_RESET, 0, 0, 0, 0, 0);

	if (res < 0)
		RTE_LOG(INFO, PMD, "fw reset failed: error %d", res);

	return res;
}

#define DEFAULT_FW_PATH       "/lib/firmware/netronome"
#define DEFAULT_FW_FILENAME   "nic_dpdk_default.nffw"

static int
nfp_fw_upload(nspu_desc_t *nspu_desc)
{
	int fw_f;
	char *fw_buf;
	char filename[100];
	struct stat file_stat;
	off_t fsize, bytes;
	ssize_t size;
	int ret;

	size = nspu_desc->buf_size;

	sprintf(filename, "%s/%s", DEFAULT_FW_PATH, DEFAULT_FW_FILENAME);
	fw_f = open(filename, O_RDONLY);
	if (fw_f < 0) {
		RTE_LOG(INFO, PMD, "Firmware file %s/%s not found.",
			DEFAULT_FW_PATH, DEFAULT_FW_FILENAME);
		return -ENOENT;
	}

	if (fstat(fw_f, &file_stat) < 0) {
		RTE_LOG(INFO, PMD, "Firmware file %s/%s size is unknown",
			DEFAULT_FW_PATH, DEFAULT_FW_FILENAME);
		close(fw_f);
		return -ENOENT;
	}

	fsize = file_stat.st_size;
	RTE_LOG(DEBUG, PMD, "Firmware file with size: %" PRIu64 "\n",
			    (uint64_t)fsize);

	if (fsize > (off_t)size) {
		RTE_LOG(INFO, PMD, "fw file too big: %" PRIu64
				   " bytes (%" PRIu64 " max)",
				  (uint64_t)fsize, (uint64_t)size);
		close(fw_f);
		return -EINVAL;
	}

	fw_buf = malloc((size_t)size);
	if (!fw_buf) {
		RTE_LOG(INFO, PMD, "malloc failed for fw buffer");
		close(fw_f);
		return -ENOMEM;
	}
	memset(fw_buf, 0, size);

	bytes = read(fw_f, fw_buf, fsize);
	if (bytes != fsize) {
		RTE_LOG(INFO, PMD, "Reading fw to buffer failed.\n"
				   "Just %" PRIu64 " of %" PRIu64 " bytes read.",
				   (uint64_t)bytes, (uint64_t)fsize);
		free(fw_buf);
		close(fw_f);
		return -EIO;
	}

	ret = nspu_command(nspu_desc, NSP_CMD_FW_LOAD, 0, 1, fw_buf, 0, bytes);

	free(fw_buf);
	close(fw_f);

	return ret;
}

/* Firmware symbol descriptor size */
#define NFP_SYM_DESC_LEN 40

#define SYMBOL_DATA(b, off)     (*(int64_t *)((b) + (off)))
#define SYMBOL_UDATA(b, off)     (*(uint64_t *)((b) + (off)))

/* Firmware symbols contain information about how to access what they
 * represent. It can be as simple as an numeric variable declared at a
 * specific NFP memory, but it can also be more complex structures and
 * related to specific hardware functionalities or components. Target,
 * domain and address allow to create the BAR window for accessing such
 * hw object and size defines the length to map.
 *
 * A vNIC is a network interface implemented inside the NFP and using a
 * subset of device PCI BARs. Specific firmware symbols allow to map those
 * vNIC bars by host drivers like the NFP PMD.
 *
 * Accessing what the symbol represents implies to map the access through
 * a PCI BAR window. NFP expansion BARs are used in this regard through
 * the NSPU interface.
 */
static int
nfp_nspu_set_bar_from_symbl(nspu_desc_t *desc, const char *symbl,
			    uint32_t expbar, uint64_t *pcie_offset,
			    ssize_t *size)
{
	int64_t type;
	int64_t target;
	int64_t domain;
	uint64_t addr;
	char *sym_buf;
	int ret = 0;

	sym_buf = malloc(desc->buf_size);
	if (!sym_buf)
		return -ENOMEM;

	strncpy(sym_buf, symbl, strlen(symbl));
	ret = nspu_command(desc, NSP_CMD_GET_SYMBOL, 1, 1, sym_buf,
			   NFP_SYM_DESC_LEN, strlen(symbl));
	if (ret) {
		RTE_LOG(DEBUG, PMD, "symbol resolution (%s) failed\n", symbl);
		goto clean;
	}

	/* Reading symbol information */
	type = SYMBOL_DATA(sym_buf, 0);
	target = SYMBOL_DATA(sym_buf, 8);
	domain =  SYMBOL_DATA(sym_buf, 16);
	addr = SYMBOL_UDATA(sym_buf, 24);
	*size = (ssize_t)SYMBOL_UDATA(sym_buf, 32);

	if (type != 1) {
		RTE_LOG(INFO, PMD, "wrong symbol type\n");
		ret = -EINVAL;
		goto clean;
	}
	if (!(target == 7 || target == -7)) {
		RTE_LOG(INFO, PMD, "wrong symbol target\n");
		ret = -EINVAL;
		goto clean;
	}
	if (domain == 8 || domain == 9) {
		RTE_LOG(INFO, PMD, "wrong symbol domain\n");
		ret = -EINVAL;
		goto clean;
	}

	/* Adjusting address based on symbol location */
	if ((domain >= 24) && (domain < 28) && (target == 7)) {
		addr = 1ULL << 37 | addr | ((uint64_t)domain & 0x3) << 35;
	} else {
		addr = 1ULL << 39 | addr | ((uint64_t)domain & 0x3f) << 32;
		if (target == -7)
			target = 7;
	}

	/* Configuring NFP expansion bar for mapping specific PCI BAR window */
	nfp_nspu_mem_bar_cfg(desc, expbar, target, addr, pcie_offset);

	/* This is the PCI BAR offset to use by the host */
	*pcie_offset |= ((expbar & 0x7) << (desc->barsz - 3));

clean:
	free(sym_buf);
	return ret;
}

int
nfp_nsp_fw_setup(nspu_desc_t *desc, const char *sym, uint64_t *pcie_offset)
{
	ssize_t bar0_sym_size;

	/* If the symbol resolution works, it implies a firmware app
	 * is already there.
	 */
	if (!nfp_nspu_set_bar_from_symbl(desc, sym, NFP_NET_PF_CFG_EXP_BAR,
					 pcie_offset, &bar0_sym_size))
		return 0;

	/* No firmware app detected or not the right one */
	RTE_LOG(INFO, PMD, "No firmware detected. Resetting NFP...\n");
	if (nfp_fw_reset(desc) < 0) {
		RTE_LOG(ERR, PMD, "nfp fw reset failed\n");
		return -ENODEV;
	}

	RTE_LOG(INFO, PMD, "Reset done.\n");
	RTE_LOG(INFO, PMD, "Uploading firmware...\n");

	if (nfp_fw_upload(desc) < 0) {
		RTE_LOG(ERR, PMD, "nfp fw upload failed\n");
		return -ENODEV;
	}

	RTE_LOG(INFO, PMD, "Done.\n");

	/* Now the symbol should be there */
	if (nfp_nspu_set_bar_from_symbl(desc, sym, NFP_NET_PF_CFG_EXP_BAR,
					pcie_offset, &bar0_sym_size)) {
		RTE_LOG(ERR, PMD, "nfp PF BAR symbol resolution failed\n");
		return -ENODEV;
	}

	return 0;
}

int
nfp_nsp_map_ctrl_bar(nspu_desc_t *desc, uint64_t *pcie_offset)
{
	ssize_t bar0_sym_size;

	if (nfp_nspu_set_bar_from_symbl(desc, "_pf0_net_bar0",
					NFP_NET_PF_CFG_EXP_BAR,
					pcie_offset, &bar0_sym_size))
		return -ENODEV;

	return 0;
}

/*
 * This is a hardcoded fixed NFP internal CPP bus address for the hw queues unit
 * inside the PCIE island.
 */
#define NFP_CPP_PCIE_QUEUES ((uint64_t)(1ULL << 39) |  0x80000 | \
			     ((uint64_t)0x4 & 0x3f) << 32)

/* Configure a specific NFP expansion bar for accessing the vNIC rx/tx BARs */
void
nfp_nsp_map_queues_bar(nspu_desc_t *desc, uint64_t *pcie_offset)
{
	nfp_nspu_mem_bar_cfg(desc, NFP_NET_PF_HW_QUEUES_EXP_BAR, 0,
			     NFP_CPP_PCIE_QUEUES, pcie_offset);

	/* This is the pcie offset to use by the host */
	*pcie_offset |= ((NFP_NET_PF_HW_QUEUES_EXP_BAR & 0x7) << (27 - 3));
}

int
nfp_nsp_eth_config(nspu_desc_t *desc, int port, int up)
{
	union eth_table_entry *entries, *entry;
	int modified;
	int ret, idx;
	int i;

	idx = port;

	RTE_LOG(INFO, PMD, "Hw ethernet port %d configure...\n", port);
	rte_spinlock_lock(&desc->nsp_lock);
	entries = malloc(NSP_ETH_TABLE_SIZE);
	if (!entries) {
		rte_spinlock_unlock(&desc->nsp_lock);
		return -ENOMEM;
	}

	ret = nspu_command(desc, NSP_CMD_READ_ETH_TABLE, 1, 0, entries,
			   NSP_ETH_TABLE_SIZE, 0);
	if (ret) {
		rte_spinlock_unlock(&desc->nsp_lock);
		free(entries);
		return ret;
	}

	entry = entries;

	for (i = 0; i < NSP_ETH_MAX_COUNT; i++) {
		/* ports in use do not appear sequentially in the table */
		if (!(entry->port & NSP_ETH_PORT_LANES_MASK)) {
			/* entry not in use */
			entry++;
			continue;
		}
		if (idx == 0)
			break;
		idx--;
		entry++;
	}

	if (i == NSP_ETH_MAX_COUNT) {
		rte_spinlock_unlock(&desc->nsp_lock);
		free(entries);
		return -EINVAL;
	}

	if (up && !(entry->state & NSP_ETH_STATE_CONFIGURED)) {
		entry->control |= NSP_ETH_STATE_CONFIGURED;
		modified = 1;
	}

	if (!up && (entry->state & NSP_ETH_STATE_CONFIGURED)) {
		entry->control &= ~NSP_ETH_STATE_CONFIGURED;
		modified = 1;
	}

	if (modified) {
		ret = nspu_command(desc, NSP_CMD_WRITE_ETH_TABLE, 0, 1, entries,
				   0, NSP_ETH_TABLE_SIZE);
		if (!ret)
			RTE_LOG(INFO, PMD,
				"Hw ethernet port %d configure done\n", port);
		else
			RTE_LOG(INFO, PMD,
				"Hw ethernet port %d configure failed\n", port);
	}
	rte_spinlock_unlock(&desc->nsp_lock);
	free(entries);
	return ret;
}

int
nfp_nsp_eth_read_table(nspu_desc_t *desc, union eth_table_entry **table)
{
	int ret;

	if (!table)
		return -EINVAL;

	RTE_LOG(INFO, PMD, "Reading hw ethernet table...\n");

	/* port 0 allocates the eth table and read it using NSPU */
	*table = malloc(NSP_ETH_TABLE_SIZE);
	if (!*table)
		return -ENOMEM;

	ret = nspu_command(desc, NSP_CMD_READ_ETH_TABLE, 1, 0, *table,
			   NSP_ETH_TABLE_SIZE, 0);
	if (ret)
		return ret;

	RTE_LOG(INFO, PMD, "Done\n");

	return 0;
}
