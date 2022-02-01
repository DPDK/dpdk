/* * SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2019-2021 NXP
 *
 */

#include <time.h>
#include <net/if.h>

#include <rte_mbuf.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_dev.h>
#include <rte_fslmc.h>
#include <rte_flow_driver.h>

#include "dpaa2_pmd_logs.h"
#include <fslmc_vfio.h>
#include <dpaa2_hw_pvt.h>
#include <dpaa2_hw_mempool.h>
#include <dpaa2_hw_dpio.h>
#include <mc/fsl_dpmng.h>
#include "dpaa2_ethdev.h"
#include "dpaa2_sparser.h"
#include <fsl_qbman_debug.h>

#include <rte_io.h>
#include <unistd.h>
#include <sys/mman.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE			(sysconf(_SC_PAGESIZE))
#endif
#define PAGE_MASK			(~(PAGE_SIZE - 1))

#define LSX_SERDES_LAN_NB		8
#define LSX_SERDES_REG_BASE		0x1ea0000
#define LSX_LB_EN_BIT			0x10000000

#define CONFIG_SYS_IMMR			0x01000000

#define CONFIG_SYS_FSL_GUTS_ADDR	(CONFIG_SYS_IMMR + 0x00E00000)
#define CONFIG_SYS_FSL_SERDES_ADDR	(CONFIG_SYS_IMMR + 0xEA0000)

#define FSL_LX_SRDS1_PRTCL_SHIFT	16
#define FSL_LX_SRDS2_PRTCL_SHIFT	21
#define FSL_LX_SRDS3_PRTCL_SHIFT	26

#define FSL_LS_SRDS1_PRTCL_SHIFT	16
#define FSL_LS_SRDS2_PRTCL_SHIFT	0

#define FSL_LX_SRDS1_REGSR		29
#define FSL_LX_SRDS2_REGSR		29
#define FSL_LX_SRDS3_REGSR		29

#define FSL_LS_SRDS1_REGSR		29
#define FSL_LS_SRDS2_REGSR		30

#define FSL_LX_SRDS1_PRTCL_MASK		0x001F0000
#define FSL_LX_SRDS2_PRTCL_MASK		0x03E00000
#define FSL_LX_SRDS3_PRTCL_MASK		0x7C000000

#define FSL_LS_SRDS1_PRTCL_MASK		0xFFFF0000
#define FSL_LS_SRDS2_PRTCL_MASK		0x0000FFFF

struct ccsr_lx_serdes_lan {
	uint8_t unused1[0xa0];
	uint32_t lnatcsr0;
	uint8_t unused2[0x100 - 0xa4];
} __rte_packed;

struct ccsr_lx_serdes {
	uint8_t unused0[0x800];
	struct ccsr_lx_serdes_lan lane[LSX_SERDES_LAN_NB];
} __rte_packed;

struct ccsr_ls_serdes {
	uint8_t unused[0x800];
	struct serdes_lane {
		uint32_t gcr0;   /* General Control Register 0 */
		uint32_t gcr1;   /* General Control Register 1 */
		uint32_t gcr2;   /* General Control Register 2 */
		uint32_t ssc0;   /* Speed Switch Control 0 */
		uint32_t rec0;   /* Receive Equalization Control 0 */
		uint32_t rec1;   /* Receive Equalization Control 1 */
		uint32_t tec0;   /* Transmit Equalization Control 0 */
		uint32_t ssc1;   /* Speed Switch Control 1 */
		uint32_t ttlc;
		uint32_t rev[6];
		uint32_t tsc3;
	} lane[LSX_SERDES_LAN_NB];
	uint8_t res5[0x19fc - 0xa00];
} __rte_packed;

struct ccsr_gur {
	uint32_t	porsr1;		/* POR status 1 */
	uint32_t	porsr2;		/* POR status 2 */
	uint8_t	res_008[0x20 - 0x8];
	uint32_t	gpporcr1; /* General-purpose POR configuration */
	uint32_t	gpporcr2; /* General-purpose POR configuration 2 */
	uint32_t	gpporcr3;
	uint32_t	gpporcr4;
	uint8_t	res_030[0x60 - 0x30];
	uint32_t	dcfg_fusesr;	/* Fuse status register */
	uint8_t	res_064[0x70 - 0x64];
	uint32_t	devdisr;	/* Device disable control 1 */
	uint32_t	devdisr2;	/* Device disable control 2 */
	uint32_t	devdisr3;	/* Device disable control 3 */
	uint32_t	devdisr4;	/* Device disable control 4 */
	uint32_t	devdisr5;	/* Device disable control 5 */
	uint32_t	devdisr6;	/* Device disable control 6 */
	uint8_t	res_088[0x94 - 0x88];
	uint32_t	coredisr;	/* Device disable control 7 */
	uint8_t	res_098[0xa0 - 0x98];
	uint32_t	pvr;		/* Processor version */
	uint32_t	svr;		/* System version */
	uint8_t	res_0a8[0x100 - 0xa8];
	uint32_t	rcwsr[30];	/* Reset control word status */

	uint8_t	res_178[0x200 - 0x178];
	uint32_t	scratchrw[16];	/* Scratch Read/Write */
	uint8_t	res_240[0x300 - 0x240];
	uint32_t	scratchw1r[4];	/* Scratch Read (Write once) */
	uint8_t	res_310[0x400 - 0x310];
	uint32_t	bootlocptrl; /* Boot location pointer low-order addr */
	uint32_t	bootlocptrh; /* Boot location pointer high-order addr */
	uint8_t	res_408[0x520 - 0x408];
	uint32_t	usb1_amqr;
	uint32_t	usb2_amqr;
	uint8_t	res_528[0x530 - 0x528];	/* add more registers when needed */
	uint32_t	sdmm1_amqr;
	uint32_t	sdmm2_amqr;
	uint8_t	res_538[0x550 - 0x538];	/* add more registers when needed */
	uint32_t	sata1_amqr;
	uint32_t	sata2_amqr;
	uint32_t	sata3_amqr;
	uint32_t	sata4_amqr;
	uint8_t	res_560[0x570 - 0x560];	/* add more registers when needed */
	uint32_t	misc1_amqr;
	uint8_t	res_574[0x590 - 0x574];	/* add more registers when needed */
	uint32_t	spare1_amqr;
	uint32_t	spare2_amqr;
	uint32_t	spare3_amqr;
	uint8_t	res_59c[0x620 - 0x59c];	/* add more registers when needed */
	uint32_t	gencr[7];	/* General Control Registers */
	uint8_t	res_63c[0x640 - 0x63c];	/* add more registers when needed */
	uint32_t	cgensr1;	/* Core General Status Register */
	uint8_t	res_644[0x660 - 0x644];	/* add more registers when needed */
	uint32_t	cgencr1;	/* Core General Control Register */
	uint8_t	res_664[0x740 - 0x664];	/* add more registers when needed */
	uint32_t	tp_ityp[64];	/* Topology Initiator Type Register */
	struct {
		uint32_t	upper;
		uint32_t	lower;
	} tp_cluster[4];	/* Core cluster n Topology Register */
	uint8_t	res_864[0x920 - 0x864];	/* add more registers when needed */
	uint32_t ioqoscr[8];	/*I/O Quality of Services Register */
	uint32_t uccr;
	uint8_t	res_944[0x960 - 0x944];	/* add more registers when needed */
	uint32_t ftmcr;
	uint8_t	res_964[0x990 - 0x964];	/* add more registers when needed */
	uint32_t coredisablesr;
	uint8_t	res_994[0xa00 - 0x994];	/* add more registers when needed */
	uint32_t sdbgcr; /*Secure Debug Configuration Register */
	uint8_t	res_a04[0xbf8 - 0xa04];	/* add more registers when needed */
	uint32_t ipbrr1;
	uint32_t ipbrr2;
	uint8_t	res_858[0x1000 - 0xc00];
} __rte_packed;

static void *lsx_ccsr_map_region(uint64_t addr, size_t len)
{
	int fd;
	void *tmp;
	uint64_t start;
	uint64_t offset;

	fd = open("/dev/mem", O_RDWR);
	if (fd < 0) {
		DPAA2_PMD_ERR("Fail to open /dev/mem");
		return NULL;
	}

	start = addr & PAGE_MASK;
	offset = addr - start;
	len = len & PAGE_MASK;
	if (len < (size_t)PAGE_SIZE)
		len = PAGE_SIZE;

	tmp = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, start);

	close(fd);

	if (tmp != MAP_FAILED)
		return (uint8_t *)tmp + offset;
	else
		return NULL;
}

static const uint8_t ls_sd1_prot_idx_map[] = {
	0x03, 0x05, 0x07, 0x09, 0x0a, 0x0c, 0x0e,
	0x10, 0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c,
	0x1e, 0x20, 0x22, 0x24, 0x26, 0x28, 0x2a,
	0x2b, 0x2d, 0x2e, 0x30, 0x32, 0x33, 0x35,
	0x37, 0x39, 0x3b, 0x4b, 0x4c, 0x4d, 0x58
};

static const uint8_t ls_sd2_prot_idx_map[] = {
	0x07, 0x09, 0x0a, 0x0c, 0x0e, 0x10, 0x12,
	0x14, 0x16, 0x18, 0x1a, 0x1c, 0x1e, 0x20,
	0x22, 0x24, 0x3d, 0x3f, 0x41, 0x43, 0x45,
	0x47, 0x49, 0x4f, 0x50, 0x51, 0x52, 0x53,
	0x54, 0x55, 0x56, 0x57
};

static const uint8_t ls_sd1_eth_loopback_support[][LSX_SERDES_LAN_NB] = {
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 0x03*/
	{1, 1, 1, 1, 0, 0, 0, 0}, /* 0x05*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x07*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x09*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x0a*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x0c*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x0e*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x10*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x12*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x14*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x16*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x18*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x1a*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x1c*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x1e*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x20*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x22*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x24*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x26*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x28*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x2a*/

	{0, 0, 0, 0, 1, 1, 1, 1}, /* 0x2b*/
	{0, 0, 0, 0, 1, 1, 1, 1}, /* 0x2d*/
	{0, 0, 0, 0, 1, 1, 1, 1}, /* 0x2e*/
	{0, 0, 0, 0, 1, 1, 1, 1}, /* 0x30*/

	{0, 0, 0, 0, 0, 0, 0, 0}, /* 0x32*/
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 0x33*/

	{1, 1, 1, 1, 0, 0, 0, 0}, /* 0x35*/
	{1, 1, 0, 0, 0, 0, 0, 0}, /* 0x37*/

	{0, 1, 1, 1, 0, 1, 1, 1}, /* 0x39*/
	{0, 1, 1, 1, 0, 1, 1, 1}, /* 0x3b*/
	{1, 1, 1, 1, 0, 0, 0, 0}, /* 0x4b*/
	{0, 0, 0, 0, 1, 1, 1, 1}, /* 0x4c*/
	{0, 0, 1, 1, 0, 0, 1, 1}, /* 0x4d*/
	{0, 0, 0, 0, 0, 0, 1, 1}  /* 0x58*/
};

static const uint8_t ls_sd2_eth_loopback_support[][LSX_SERDES_LAN_NB] = {
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x07*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x09*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x0a*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x0c*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x0e*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x10*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x12*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x14*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x16*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x18*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x1a*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x1c*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x1e*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x20*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x22*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 0x24*/

	{0, 0, 0, 0, 0, 0, 0, 0}, /* 0x3d*/
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 0x3f*/
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 0x41*/
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 0x43*/

	{1, 1, 1, 1, 0, 0, 0, 0}, /* 0x45*/
	{0, 1, 1, 1, 0, 1, 1, 1}, /* 0x47*/
	{1, 1, 1, 1, 0, 0, 0, 0}, /* 0x49*/

	{0, 0, 1, 1, 0, 0, 1, 1}, /* 0x4f*/
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 0x50*/
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 0x51*/
	{1, 1, 1, 1, 0, 0, 0, 0}, /* 0x52*/
	{0, 1, 1, 1, 0, 1, 1, 1}, /* 0x53*/
	{0, 0, 1, 1, 0, 0, 1, 1}, /* 0x54*/
	{0, 1, 1, 1, 0, 1, 1, 1}, /* 0x55*/
	{0, 0, 1, 1, 0, 0, 1, 1}, /* 0x56*/
	{0, 0, 0, 0, 0, 0, 1, 1}  /* 0x57*/
};

enum lsx_serdes_id {
	LSX_SERDES_1 = 1,
	LSX_SERDES_2 = 2
};

static const uint8_t lx_sd1_loopback_support[][LSX_SERDES_LAN_NB] = {
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 0 prot*/
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 1 prot*/
	{1, 1, 1, 1, 0, 0, 0, 0}, /* 2 prot*/
	{1, 1, 1, 1, 0, 0, 0, 0}, /* 3 prot*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 4 prot*/
	{0, 0, 0, 0, 1, 1, 1, 1}, /* 5 prot*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 6 prot*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 7 prot*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 8 prot*/
	{0, 1, 1, 1, 0, 1, 1, 1}, /* 9 prot*/
	{0, 1, 1, 1, 0, 1, 1, 1}, /* 10 prot*/
	{0, 0, 1, 1, 0, 0, 1, 1}, /* 11 prot*/
	{0, 0, 0, 0, 0, 0, 1, 1}, /* 12 prot*/
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 13 prot*/
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 14 prot*/
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 15 prot*/
	{0, 0, 1, 1, 0, 0, 0, 0}, /* 16 prot*/
	{1, 1, 1, 1, 0, 0, 0, 0}, /* 17 prot*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 18 prot*/
	{1, 1, 1, 1, 0, 0, 0, 0}, /* 19 prot*/
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 20 prot*/
	{1, 1, 1, 1, 0, 0, 1, 1}, /* 21 prot*/
	{1, 1, 1, 1, 0, 0, 1, 1}  /* 22 prot*/
};

static const uint8_t lx_sd2_loopback_support[][LSX_SERDES_LAN_NB] = {
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 0 prot*/
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 1 prot*/
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 2 prot*/
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 3 prot*/
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 4 prot*/
	{0, 0, 0, 0, 0, 0, 0, 0}, /* 5 prot*/
	{0, 0, 0, 0, 1, 1, 1, 1}, /* 6 prot*/
	{0, 1, 1, 1, 0, 1, 1, 1}, /* 7 prot*/
	{0, 0, 0, 0, 0, 0, 1, 1}, /* 8 prot*/
	{1, 1, 1, 1, 1, 1, 1, 1}, /* 9 prot*/
	{1, 1, 1, 1, 0, 0, 0, 0}, /* 10 prot*/
	{0, 1, 1, 1, 0, 1, 1, 1}, /* 11 prot*/
	{1, 1, 1, 1, 0, 0, 0, 0}, /* 12 prot*/
	{0, 0, 0, 0, 0, 0, 1, 1}, /* 13 prot*/
	{0, 0, 1, 1, 0, 0, 1, 1}  /* 14 prot*/
};

static inline int
ls_mac_to_serdes_id(uint8_t mac_id)
{
	if (mac_id >= 1 && mac_id <= 8)
		return LSX_SERDES_1;
	if (mac_id >= 9 && mac_id <= 16)
		return LSX_SERDES_2;

	return -1;
}

static inline int
lx_mac_to_serdes_id(uint8_t mac_id)
{
	if (mac_id >= 1 && mac_id <= 10)
		return LSX_SERDES_1;
	if (mac_id >= 11 && mac_id <= 18)
		return LSX_SERDES_2;

	return -1;
}

static inline int
ls_serdes_cfg_to_idx(uint8_t sd_cfg, int sd_id)
{
	int i;

	if (sd_id == LSX_SERDES_1) {
		for (i = 0; i < (int)sizeof(ls_sd1_prot_idx_map); i++) {
			if (ls_sd1_prot_idx_map[i] == sd_cfg)
				return i;
		}
	} else if (sd_id == LSX_SERDES_2) {
		for (i = 0; i < (int)sizeof(ls_sd2_prot_idx_map); i++) {
			if (ls_sd2_prot_idx_map[i] == sd_cfg)
				return i;
		}
	}

	return -1;
}

static inline int
lx_serdes_cfg_to_idx(uint8_t sd_cfg, int sd_id __rte_unused)
{
	return sd_cfg;
}

static inline int
ls_mac_serdes_lpbk_support(uint16_t mac_id,
	uint16_t *serdes_id, uint16_t *lan_id)
{
	struct ccsr_gur *gur_base =
		lsx_ccsr_map_region(CONFIG_SYS_FSL_GUTS_ADDR,
			sizeof(struct ccsr_gur) / 64 * 64 + 64);
	uint32_t sd_cfg;
	int sd_id, sd_idx;
	uint16_t lan_id_tmp = 0;
	const uint8_t *ls_sd_loopback_support;

	sd_id = ls_mac_to_serdes_id(mac_id);

	if (sd_id == LSX_SERDES_1) {
		sd_cfg = rte_read32(&gur_base->rcwsr[FSL_LS_SRDS1_REGSR - 1]) &
				FSL_LS_SRDS1_PRTCL_MASK;
		sd_cfg >>= FSL_LS_SRDS1_PRTCL_SHIFT;
	} else if (sd_id == LSX_SERDES_2) {
		sd_cfg = rte_read32(&gur_base->rcwsr[FSL_LS_SRDS2_REGSR - 1]) &
				FSL_LS_SRDS2_PRTCL_MASK;
		sd_cfg >>= FSL_LS_SRDS2_PRTCL_SHIFT;
	} else {
		return false;
	}
	sd_cfg = sd_cfg & 0xff;

	sd_idx = ls_serdes_cfg_to_idx(sd_cfg, sd_id);
	if (sd_idx < 0) {
		DPAA2_PMD_ERR("Serdes protocol(0x%02x) does not exist\n",
			sd_cfg);
		return false;
	}

	if (sd_id == LSX_SERDES_1) {
		ls_sd_loopback_support =
			&ls_sd1_eth_loopback_support[sd_idx][0];
	} else {
		ls_sd_loopback_support =
			&ls_sd2_eth_loopback_support[sd_idx][0];
	}

	if (sd_id == LSX_SERDES_1)
		lan_id_tmp = (mac_id - 1);
	else
		lan_id_tmp = (mac_id - 9);

	if (lan_id_tmp >= LSX_SERDES_LAN_NB) {
		DPAA2_PMD_ERR("Invalid serdes lan(%d).", lan_id_tmp);
		return false;
	}

	if (!ls_sd_loopback_support[lan_id_tmp])
		return false;

	if (lan_id)
		*lan_id = lan_id_tmp;
	if (serdes_id)
		*serdes_id = sd_id;

	return true;
}

static inline int
lx_mac_serdes_lpbk_support(uint16_t mac_id,
	uint16_t *serdes_id, uint16_t *lan_id)
{
	struct ccsr_gur *gur_base =
		lsx_ccsr_map_region(CONFIG_SYS_FSL_GUTS_ADDR,
			sizeof(struct ccsr_gur) / 64 * 64 + 64);
	uint32_t sd_cfg;
	int sd_id, sd_idx;
	uint16_t lan_id_tmp = 0;
	const uint8_t *lx_sd_loopback_support;

	sd_id = lx_mac_to_serdes_id(mac_id);

	if (sd_id == LSX_SERDES_1) {
		sd_cfg = rte_read32(&gur_base->rcwsr[FSL_LX_SRDS1_REGSR - 1]) &
				FSL_LX_SRDS1_PRTCL_MASK;
		sd_cfg >>= FSL_LX_SRDS1_PRTCL_SHIFT;
	} else if (sd_id == LSX_SERDES_2) {
		sd_cfg = rte_read32(&gur_base->rcwsr[FSL_LX_SRDS2_REGSR - 1]) &
				FSL_LX_SRDS2_PRTCL_MASK;
		sd_cfg >>= FSL_LX_SRDS2_PRTCL_SHIFT;
	} else {
		return false;
	}
	sd_cfg = sd_cfg & 0xff;

	sd_idx = lx_serdes_cfg_to_idx(sd_cfg, sd_id);
	if (sd_idx < 0)
		return false;

	if (sd_id == LSX_SERDES_1)
		lx_sd_loopback_support = &lx_sd1_loopback_support[sd_idx][0];
	else
		lx_sd_loopback_support = &lx_sd2_loopback_support[sd_idx][0];

	if (sd_id == LSX_SERDES_1) {
		if (mac_id == 1)
			lan_id_tmp = 0;
		else if (mac_id == 2)
			lan_id_tmp = 4;
		else
			lan_id_tmp = (mac_id - 3);
	} else {
		if (mac_id == 11)
			lan_id_tmp = 0;
		else if (mac_id == 12)
			lan_id_tmp = 1;
		else if (mac_id == 13)
			lan_id_tmp = 6;
		else if (mac_id == 14)
			lan_id_tmp = 7;
		else if (mac_id == 15)
			lan_id_tmp = 4;
		else if (mac_id == 16)
			lan_id_tmp = 5;
		else if (mac_id == 17)
			lan_id_tmp = 2;
		else if (mac_id == 18)
			lan_id_tmp = 3;
		else
			return false;
	}

	if (lan_id_tmp >= LSX_SERDES_LAN_NB)
		return false;

	if (!lx_sd_loopback_support[lan_id_tmp])
		return false;

	if (lan_id)
		*lan_id = lan_id_tmp;
	if (serdes_id)
		*serdes_id = sd_id;

	return true;
}

static inline int
ls_serdes_eth_lpbk(uint16_t mac_id, int en)
{
	uint16_t serdes_id, lan_id;
	int ret;
	uint32_t data;
	struct ccsr_ls_serdes *serdes_base;
	void *reg = 0;

	ret = ls_mac_serdes_lpbk_support(mac_id, &serdes_id, &lan_id);
	if (!ret)
		return -ENOTSUP;

	serdes_base = lsx_ccsr_map_region(CONFIG_SYS_FSL_SERDES_ADDR +
				(serdes_id - LSX_SERDES_1) * 0x10000,
				sizeof(struct ccsr_ls_serdes) / 64 * 64 + 64);
	if (!serdes_base) {
		DPAA2_PMD_ERR("Serdes register map failed\n");
		return -ENOMEM;
	}

	if (serdes_id == LSX_SERDES_1)
		lan_id = LSX_SERDES_LAN_NB - lan_id - 1;

	reg = &serdes_base->lane[lan_id].tsc3;

	data = rte_read32(reg);
	if (en)
		rte_write32(data | LSX_LB_EN_BIT, reg);
	else
		rte_write32(data & (~LSX_LB_EN_BIT), reg);

	return 0;
}

static inline int
lx_serdes_eth_lpbk(uint16_t mac_id, int en)
{
	uint16_t serdes_id = 0xffff, lan_id = 0xffff;
	int ret;
	uint32_t data;
	struct ccsr_lx_serdes *serdes_base;
	void *reg = 0;

	ret = lx_mac_serdes_lpbk_support(mac_id, &serdes_id, &lan_id);
	if (!ret)
		return -ENOTSUP;

	serdes_base = lsx_ccsr_map_region(CONFIG_SYS_FSL_SERDES_ADDR +
					(serdes_id - LSX_SERDES_1) * 0x10000,
					sizeof(struct ccsr_lx_serdes) / 64 * 64 + 64);
	if (!serdes_base) {
		DPAA2_PMD_ERR("Serdes register map failed\n");
		return -ENOMEM;
	}

	if (serdes_id == LSX_SERDES_1)
		lan_id = LSX_SERDES_LAN_NB - lan_id - 1;

	reg = &serdes_base->lane[lan_id].lnatcsr0;

	data = rte_read32(reg);
	if (en)
		rte_write32(data | LSX_LB_EN_BIT, reg);
	else
		rte_write32(data & (~LSX_LB_EN_BIT), reg);

	return 0;
}

/* Configure dpaa2 port as recycle port */
int
dpaa2_dev_recycle_config(struct rte_eth_dev *eth_dev)
{
	struct rte_device *dev = eth_dev->device;
	struct dpaa2_dev_priv *priv = eth_dev->data->dev_private;
	struct rte_dpaa2_device *dpaa2_dev =
			container_of(dev, struct rte_dpaa2_device, device);
	struct fsl_mc_io *dpni_dev = eth_dev->process_private;
	struct dpni_port_cfg port_cfg;
	int ret;

	if (priv->flags & DPAA2_TX_LOOPBACK_MODE) {
		DPAA2_PMD_INFO("%s has been configured recycle device.",
			eth_dev->data->name);

		return 0;
	}

	if (dpaa2_dev->ep_dev_type == DPAA2_MAC) {
		/** For dpmac-dpni connection,
		 * try setting serdes loopback as recycle device at first.
		 */
		if (dpaa2_svr_family == SVR_LS2088A) {
			ret = ls_serdes_eth_lpbk(dpaa2_dev->ep_object_id, 1);
			if (!ret) {
				priv->flags |= DPAA2_TX_SERDES_LOOPBACK_MODE;
				return 0;
			}
		} else if (dpaa2_svr_family == SVR_LX2160A) {
			ret = lx_serdes_eth_lpbk(dpaa2_dev->ep_object_id, 1);
			if (!ret) {
				priv->flags |= DPAA2_TX_SERDES_LOOPBACK_MODE;
				return 0;
			}
		} else {
			DPAA2_PMD_DEBUG("Serdes loopback not support SoC(0x%08x)",
				dpaa2_svr_family);
		}

		/** If serdes loopback is not supported for this mac,
		 * trying set mac loopback.
		 */

		port_cfg.loopback_en = 1;
		ret = dpni_set_port_cfg(dpni_dev, CMD_PRI_LOW,
				priv->token,
				DPNI_PORT_CFG_LOOPBACK,
				&port_cfg);
		if (ret) {
			DPAA2_PMD_ERR("Error(%d) to enable loopback", ret);
			return -ENOTSUP;
		}

		priv->flags |= DPAA2_TX_MAC_LOOPBACK_MODE;

		return 0;
	}

	if (dpaa2_dev->ep_dev_type == DPAA2_ETH &&
		dpaa2_dev->object_id == dpaa2_dev->ep_object_id) {
		priv->flags |= DPAA2_TX_DPNI_LOOPBACK_MODE;

		return 0;
	}

	return -ENOTSUP;
}

int
dpaa2_dev_recycle_deconfig(struct rte_eth_dev *eth_dev)
{
	struct rte_device *dev = eth_dev->device;
	struct dpaa2_dev_priv *priv = eth_dev->data->dev_private;
	struct rte_dpaa2_device *dpaa2_dev =
			container_of(dev, struct rte_dpaa2_device, device);
	struct fsl_mc_io *dpni_dev = eth_dev->process_private;
	struct dpni_port_cfg port_cfg;
	int ret = 0;

	if (!(priv->flags & DPAA2_TX_LOOPBACK_MODE))
		return 0;

	if (priv->flags & DPAA2_TX_SERDES_LOOPBACK_MODE) {
		if (dpaa2_svr_family == SVR_LS2088A) {
			ret = ls_serdes_eth_lpbk(dpaa2_dev->ep_object_id, 0);
			if (ret) {
				DPAA2_PMD_WARN("Error(%d) to disable Serdes loopback",
					ret);
			} else {
				priv->flags &= ~DPAA2_TX_SERDES_LOOPBACK_MODE;
			}
		} else if (dpaa2_svr_family == SVR_LX2160A) {
			ret = lx_serdes_eth_lpbk(dpaa2_dev->ep_object_id, 0);
			if (ret) {
				DPAA2_PMD_WARN("Error(%d) to disable Serdes loopback",
					ret);
			} else {
				priv->flags &= ~DPAA2_TX_SERDES_LOOPBACK_MODE;
			}
		} else {
			DPAA2_PMD_DEBUG("Serdes loopback not support SoC(0x%08x)",
				dpaa2_svr_family);
		}
	}

	if (priv->flags & DPAA2_TX_MAC_LOOPBACK_MODE) {
		port_cfg.loopback_en = 0;
		ret = dpni_set_port_cfg(dpni_dev, CMD_PRI_LOW,
				priv->token,
				DPNI_PORT_CFG_LOOPBACK,
				&port_cfg);
		if (ret) {
			DPAA2_PMD_ERR("Error(%d) to disable TX mac loopback",
				ret);
		} else {
			priv->flags &= ~DPAA2_TX_MAC_LOOPBACK_MODE;
		}
	}

	if (priv->flags & DPAA2_TX_DPNI_LOOPBACK_MODE)
		priv->flags &= ~DPAA2_TX_DPNI_LOOPBACK_MODE;

	return ret;
}

int
dpaa2_dev_recycle_qp_setup(struct rte_dpaa2_device *dpaa2_dev,
	uint16_t qidx, uint64_t cntx,
	eth_rx_burst_t tx_lpbk, eth_tx_burst_t rx_lpbk,
	struct dpaa2_queue **txq,
	struct dpaa2_queue **rxq)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_data *data;
	struct dpaa2_queue *txq_tmp;
	struct dpaa2_queue *rxq_tmp;
	struct dpaa2_dev_priv *priv;

	dev = dpaa2_dev->eth_dev;
	data = dev->data;
	priv = data->dev_private;

	if (!(priv->flags & DPAA2_TX_LOOPBACK_MODE) &&
		(tx_lpbk || rx_lpbk)) {
		DPAA2_PMD_ERR("%s is NOT recycle device!", data->name);

		return -EINVAL;
	}

	if (qidx >= data->nb_rx_queues || qidx >= data->nb_tx_queues)
		return -EINVAL;

	rte_spinlock_lock(&priv->lpbk_qp_lock);

	if (tx_lpbk)
		dev->tx_pkt_burst = tx_lpbk;

	if (rx_lpbk)
		dev->rx_pkt_burst = rx_lpbk;

	txq_tmp = data->tx_queues[qidx];
	txq_tmp->lpbk_cntx = cntx;
	rxq_tmp = data->rx_queues[qidx];
	rxq_tmp->lpbk_cntx = cntx;

	if (txq)
		*txq = txq_tmp;
	if (rxq)
		*rxq = rxq_tmp;

	rte_spinlock_unlock(&priv->lpbk_qp_lock);

	return 0;
}
