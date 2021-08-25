/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2007-2013 Broadcom Corporation.
 *
 * Eric Davis        <edavis@broadcom.com>
 * David Christensen <davidch@broadcom.com>
 * Gary Zambrano     <zambrano@broadcom.com>
 *
 * Copyright (c) 2013-2015 Brocade Communications Systems, Inc.
 * Copyright (c) 2015-2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#define BNX2X_DRIVER_VERSION "1.78.18"

#include "bnx2x.h"
#include "bnx2x_vfpf.h"
#include "ecore_sp.h"
#include "ecore_init.h"
#include "ecore_init_ops.h"

#include "rte_version.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <zlib.h>

#include <rte_bitops.h>
#include <rte_string_fns.h>

#include "eal_firmware.h"

#define BNX2X_PMD_VER_PREFIX "BNX2X PMD"
#define BNX2X_PMD_VERSION_MAJOR 1
#define BNX2X_PMD_VERSION_MINOR 1
#define BNX2X_PMD_VERSION_REVISION 0
#define BNX2X_PMD_VERSION_PATCH 1

static inline const char *
bnx2x_pmd_version(void)
{
	static char version[32];

	snprintf(version, sizeof(version), "%s %s_%d.%d.%d.%d",
			BNX2X_PMD_VER_PREFIX,
			BNX2X_DRIVER_VERSION,
			BNX2X_PMD_VERSION_MAJOR,
			BNX2X_PMD_VERSION_MINOR,
			BNX2X_PMD_VERSION_REVISION,
			BNX2X_PMD_VERSION_PATCH);

	return version;
}

static z_stream zlib_stream;

#define EVL_VLID_MASK 0x0FFF

#define BNX2X_DEF_SB_ATT_IDX 0x0001
#define BNX2X_DEF_SB_IDX     0x0002

/*
 * FLR Support - bnx2x_pf_flr_clnup() is called during nic_load in the per
 * function HW initialization.
 */
#define FLR_WAIT_USEC     10000	/* 10 msecs */
#define FLR_WAIT_INTERVAL 50	/* usecs */
#define FLR_POLL_CNT      (FLR_WAIT_USEC / FLR_WAIT_INTERVAL)	/* 200 */

struct pbf_pN_buf_regs {
	int pN;
	uint32_t init_crd;
	uint32_t crd;
	uint32_t crd_freed;
};

struct pbf_pN_cmd_regs {
	int pN;
	uint32_t lines_occup;
	uint32_t lines_freed;
};

/* resources needed for unloading a previously loaded device */

#define BNX2X_PREV_WAIT_NEEDED 1
rte_spinlock_t bnx2x_prev_mtx;
struct bnx2x_prev_list_node {
	LIST_ENTRY(bnx2x_prev_list_node) node;
	uint8_t bus;
	uint8_t slot;
	uint8_t path;
	uint8_t aer;
	uint8_t undi;
};

static LIST_HEAD(, bnx2x_prev_list_node) bnx2x_prev_list
	= LIST_HEAD_INITIALIZER(bnx2x_prev_list);

static int load_count[2][3] = { { 0 } };
	/* per-path: 0-common, 1-port0, 2-port1 */

static void bnx2x_cmng_fns_init(struct bnx2x_softc *sc, uint8_t read_cfg,
				uint8_t cmng_type);
static int bnx2x_get_cmng_fns_mode(struct bnx2x_softc *sc);
static void storm_memset_cmng(struct bnx2x_softc *sc, struct cmng_init *cmng,
			      uint8_t port);
static void bnx2x_set_reset_global(struct bnx2x_softc *sc);
static void bnx2x_set_reset_in_progress(struct bnx2x_softc *sc);
static uint8_t bnx2x_reset_is_done(struct bnx2x_softc *sc, int engine);
static uint8_t bnx2x_clear_pf_load(struct bnx2x_softc *sc);
static uint8_t bnx2x_chk_parity_attn(struct bnx2x_softc *sc, uint8_t * global,
				     uint8_t print);
static void bnx2x_int_disable(struct bnx2x_softc *sc);
static int bnx2x_release_leader_lock(struct bnx2x_softc *sc);
static void bnx2x_pf_disable(struct bnx2x_softc *sc);
static void bnx2x_update_rx_prod(struct bnx2x_softc *sc,
				 struct bnx2x_fastpath *fp,
				 uint16_t rx_bd_prod, uint16_t rx_cq_prod);
static void bnx2x_link_report_locked(struct bnx2x_softc *sc);
static void bnx2x_link_report(struct bnx2x_softc *sc);
void bnx2x_link_status_update(struct bnx2x_softc *sc);
static int bnx2x_alloc_mem(struct bnx2x_softc *sc);
static void bnx2x_free_mem(struct bnx2x_softc *sc);
static int bnx2x_alloc_fw_stats_mem(struct bnx2x_softc *sc);
static void bnx2x_free_fw_stats_mem(struct bnx2x_softc *sc);
static __rte_noinline
int bnx2x_nic_load(struct bnx2x_softc *sc);

static int bnx2x_handle_sp_tq(struct bnx2x_softc *sc);
static void bnx2x_handle_fp_tq(struct bnx2x_fastpath *fp);
static void bnx2x_ack_sb(struct bnx2x_softc *sc, uint8_t igu_sb_id,
			 uint8_t storm, uint16_t index, uint8_t op,
			 uint8_t update);

int bnx2x_cmpxchg(volatile int *addr, int old, int new)
{
	return __sync_val_compare_and_swap(addr, old, new);
}

int
bnx2x_dma_alloc(struct bnx2x_softc *sc, size_t size, struct bnx2x_dma *dma,
	      const char *msg, uint32_t align)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *z;

	dma->sc = sc;
	if (IS_PF(sc))
		snprintf(mz_name, sizeof(mz_name), "bnx2x%d_%s_%" PRIx64, SC_ABS_FUNC(sc), msg,
			rte_get_timer_cycles());
	else
		snprintf(mz_name, sizeof(mz_name), "bnx2x%d_%s_%" PRIx64, sc->pcie_device, msg,
			rte_get_timer_cycles());

	/* Caller must take care that strlen(mz_name) < RTE_MEMZONE_NAMESIZE */
	z = rte_memzone_reserve_aligned(mz_name, (uint64_t)size,
					SOCKET_ID_ANY,
					RTE_MEMZONE_IOVA_CONTIG, align);
	if (z == NULL) {
		PMD_DRV_LOG(ERR, sc, "DMA alloc failed for %s", msg);
		return -ENOMEM;
	}
	dma->paddr = (uint64_t) z->iova;
	dma->vaddr = z->addr;
	dma->mzone = (const void *)z;

	PMD_DRV_LOG(DEBUG, sc,
		    "%s: virt=%p phys=%" PRIx64, msg, dma->vaddr, dma->paddr);

	return 0;
}

void bnx2x_dma_free(struct bnx2x_dma *dma)
{
	if (dma->mzone == NULL)
		return;

	rte_memzone_free((const struct rte_memzone *)dma->mzone);
	dma->sc = NULL;
	dma->paddr = 0;
	dma->vaddr = NULL;
	dma->nseg = 0;
	dma->mzone = NULL;
}

static int bnx2x_acquire_hw_lock(struct bnx2x_softc *sc, uint32_t resource)
{
	uint32_t lock_status;
	uint32_t resource_bit = (1 << resource);
	int func = SC_FUNC(sc);
	uint32_t hw_lock_control_reg;
	int cnt;

#ifndef RTE_LIBRTE_BNX2X_DEBUG_PERIODIC
	if (resource)
		PMD_INIT_FUNC_TRACE(sc);
#else
	PMD_INIT_FUNC_TRACE(sc);
#endif

	/* validate the resource is within range */
	if (resource > HW_LOCK_MAX_RESOURCE_VALUE) {
		PMD_DRV_LOG(NOTICE, sc,
			    "resource 0x%x > HW_LOCK_MAX_RESOURCE_VALUE",
			    resource);
		return -1;
	}

	if (func <= 5) {
		hw_lock_control_reg = (MISC_REG_DRIVER_CONTROL_1 + (func * 8));
	} else {
		hw_lock_control_reg =
		    (MISC_REG_DRIVER_CONTROL_7 + ((func - 6) * 8));
	}

	/* validate the resource is not already taken */
	lock_status = REG_RD(sc, hw_lock_control_reg);
	if (lock_status & resource_bit) {
		PMD_DRV_LOG(NOTICE, sc,
			    "resource in use (status 0x%x bit 0x%x)",
			    lock_status, resource_bit);
		return -1;
	}

	/* try every 5ms for 5 seconds */
	for (cnt = 0; cnt < 1000; cnt++) {
		REG_WR(sc, (hw_lock_control_reg + 4), resource_bit);
		lock_status = REG_RD(sc, hw_lock_control_reg);
		if (lock_status & resource_bit) {
			return 0;
		}
		DELAY(5000);
	}

	PMD_DRV_LOG(NOTICE, sc, "Resource 0x%x resource_bit 0x%x lock timeout!",
		    resource, resource_bit);
	return -1;
}

static int bnx2x_release_hw_lock(struct bnx2x_softc *sc, uint32_t resource)
{
	uint32_t lock_status;
	uint32_t resource_bit = (1 << resource);
	int func = SC_FUNC(sc);
	uint32_t hw_lock_control_reg;

#ifndef RTE_LIBRTE_BNX2X_DEBUG_PERIODIC
	if (resource)
		PMD_INIT_FUNC_TRACE(sc);
#else
	PMD_INIT_FUNC_TRACE(sc);
#endif

	/* validate the resource is within range */
	if (resource > HW_LOCK_MAX_RESOURCE_VALUE) {
		PMD_DRV_LOG(NOTICE, sc,
			    "(resource 0x%x > HW_LOCK_MAX_RESOURCE_VALUE)"
			    " resource_bit 0x%x", resource, resource_bit);
		return -1;
	}

	if (func <= 5) {
		hw_lock_control_reg = (MISC_REG_DRIVER_CONTROL_1 + (func * 8));
	} else {
		hw_lock_control_reg =
		    (MISC_REG_DRIVER_CONTROL_7 + ((func - 6) * 8));
	}

	/* validate the resource is currently taken */
	lock_status = REG_RD(sc, hw_lock_control_reg);
	if (!(lock_status & resource_bit)) {
		PMD_DRV_LOG(NOTICE, sc,
			    "resource not in use (status 0x%x bit 0x%x)",
			    lock_status, resource_bit);
		return -1;
	}

	REG_WR(sc, hw_lock_control_reg, resource_bit);
	return 0;
}

static void bnx2x_acquire_phy_lock(struct bnx2x_softc *sc)
{
	BNX2X_PHY_LOCK(sc);
	bnx2x_acquire_hw_lock(sc, HW_LOCK_RESOURCE_MDIO);
}

static void bnx2x_release_phy_lock(struct bnx2x_softc *sc)
{
	bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_MDIO);
	BNX2X_PHY_UNLOCK(sc);
}

/* copy command into DMAE command memory and set DMAE command Go */
void bnx2x_post_dmae(struct bnx2x_softc *sc, struct dmae_command *dmae, int idx)
{
	uint32_t cmd_offset;
	uint32_t i;

	cmd_offset = (DMAE_REG_CMD_MEM + (sizeof(struct dmae_command) * idx));
	for (i = 0; i < ((sizeof(struct dmae_command) / 4)); i++) {
		REG_WR(sc, (cmd_offset + (i * 4)), *(((uint32_t *) dmae) + i));
	}

	REG_WR(sc, dmae_reg_go_c[idx], 1);
}

uint32_t bnx2x_dmae_opcode_add_comp(uint32_t opcode, uint8_t comp_type)
{
	return opcode | ((comp_type << DMAE_COMMAND_C_DST_SHIFT) |
			  DMAE_COMMAND_C_TYPE_ENABLE);
}

uint32_t bnx2x_dmae_opcode_clr_src_reset(uint32_t opcode)
{
	return opcode & ~DMAE_COMMAND_SRC_RESET;
}

uint32_t
bnx2x_dmae_opcode(struct bnx2x_softc * sc, uint8_t src_type, uint8_t dst_type,
		uint8_t with_comp, uint8_t comp_type)
{
	uint32_t opcode = 0;

	opcode |= ((src_type << DMAE_COMMAND_SRC_SHIFT) |
		   (dst_type << DMAE_COMMAND_DST_SHIFT));

	opcode |= (DMAE_COMMAND_SRC_RESET | DMAE_COMMAND_DST_RESET);

	opcode |= (SC_PORT(sc) ? DMAE_CMD_PORT_1 : DMAE_CMD_PORT_0);

	opcode |= ((SC_VN(sc) << DMAE_COMMAND_E1HVN_SHIFT) |
		   (SC_VN(sc) << DMAE_COMMAND_DST_VN_SHIFT));

	opcode |= (DMAE_COM_SET_ERR << DMAE_COMMAND_ERR_POLICY_SHIFT);

#ifdef __BIG_ENDIAN
	opcode |= DMAE_CMD_ENDIANITY_B_DW_SWAP;
#else
	opcode |= DMAE_CMD_ENDIANITY_DW_SWAP;
#endif

	if (with_comp) {
		opcode = bnx2x_dmae_opcode_add_comp(opcode, comp_type);
	}

	return opcode;
}

static void
bnx2x_prep_dmae_with_comp(struct bnx2x_softc *sc, struct dmae_command *dmae,
			uint8_t src_type, uint8_t dst_type)
{
	memset(dmae, 0, sizeof(struct dmae_command));

	/* set the opcode */
	dmae->opcode = bnx2x_dmae_opcode(sc, src_type, dst_type,
				       TRUE, DMAE_COMP_PCI);

	/* fill in the completion parameters */
	dmae->comp_addr_lo = U64_LO(BNX2X_SP_MAPPING(sc, wb_comp));
	dmae->comp_addr_hi = U64_HI(BNX2X_SP_MAPPING(sc, wb_comp));
	dmae->comp_val = DMAE_COMP_VAL;
}

/* issue a DMAE command over the init channel and wait for completion */
static int
bnx2x_issue_dmae_with_comp(struct bnx2x_softc *sc, struct dmae_command *dmae)
{
	uint32_t *wb_comp = BNX2X_SP(sc, wb_comp);
	int timeout = CHIP_REV_IS_SLOW(sc) ? 400000 : 4000;

	/* reset completion */
	*wb_comp = 0;

	/* post the command on the channel used for initializations */
	bnx2x_post_dmae(sc, dmae, INIT_DMAE_C(sc));

	/* wait for completion */
	DELAY(500);

	while ((*wb_comp & ~DMAE_PCI_ERR_FLAG) != DMAE_COMP_VAL) {
		if (!timeout ||
		    (sc->recovery_state != BNX2X_RECOVERY_DONE &&
		     sc->recovery_state != BNX2X_RECOVERY_NIC_LOADING)) {
			PMD_DRV_LOG(INFO, sc, "DMAE timeout!");
			return DMAE_TIMEOUT;
		}

		timeout--;
		DELAY(50);
	}

	if (*wb_comp & DMAE_PCI_ERR_FLAG) {
		PMD_DRV_LOG(INFO, sc, "DMAE PCI error!");
		return DMAE_PCI_ERROR;
	}

	return 0;
}

void bnx2x_read_dmae(struct bnx2x_softc *sc, uint32_t src_addr, uint32_t len32)
{
	struct dmae_command dmae;
	uint32_t *data;
	uint32_t i;
	int rc;

	if (!sc->dmae_ready) {
		data = BNX2X_SP(sc, wb_data[0]);

		for (i = 0; i < len32; i++) {
			data[i] = REG_RD(sc, (src_addr + (i * 4)));
		}

		return;
	}

	/* set opcode and fixed command fields */
	bnx2x_prep_dmae_with_comp(sc, &dmae, DMAE_SRC_GRC, DMAE_DST_PCI);

	/* fill in addresses and len */
	dmae.src_addr_lo = (src_addr >> 2);	/* GRC addr has dword resolution */
	dmae.src_addr_hi = 0;
	dmae.dst_addr_lo = U64_LO(BNX2X_SP_MAPPING(sc, wb_data));
	dmae.dst_addr_hi = U64_HI(BNX2X_SP_MAPPING(sc, wb_data));
	dmae.len = len32;

	/* issue the command and wait for completion */
	if ((rc = bnx2x_issue_dmae_with_comp(sc, &dmae)) != 0) {
		rte_panic("DMAE failed (%d)", rc);
	};
}

void
bnx2x_write_dmae(struct bnx2x_softc *sc, rte_iova_t dma_addr, uint32_t dst_addr,
	       uint32_t len32)
{
	struct dmae_command dmae;
	int rc;

	if (!sc->dmae_ready) {
		ecore_init_str_wr(sc, dst_addr, BNX2X_SP(sc, wb_data[0]), len32);
		return;
	}

	/* set opcode and fixed command fields */
	bnx2x_prep_dmae_with_comp(sc, &dmae, DMAE_SRC_PCI, DMAE_DST_GRC);

	/* fill in addresses and len */
	dmae.src_addr_lo = U64_LO(dma_addr);
	dmae.src_addr_hi = U64_HI(dma_addr);
	dmae.dst_addr_lo = (dst_addr >> 2);	/* GRC addr has dword resolution */
	dmae.dst_addr_hi = 0;
	dmae.len = len32;

	/* issue the command and wait for completion */
	if ((rc = bnx2x_issue_dmae_with_comp(sc, &dmae)) != 0) {
		rte_panic("DMAE failed (%d)", rc);
	}
}

static void
bnx2x_write_dmae_phys_len(struct bnx2x_softc *sc, rte_iova_t phys_addr,
			uint32_t addr, uint32_t len)
{
	uint32_t dmae_wr_max = DMAE_LEN32_WR_MAX(sc);
	uint32_t offset = 0;

	while (len > dmae_wr_max) {
		bnx2x_write_dmae(sc, (phys_addr + offset),	/* src DMA address */
			       (addr + offset),	/* dst GRC address */
			       dmae_wr_max);
		offset += (dmae_wr_max * 4);
		len -= dmae_wr_max;
	}

	bnx2x_write_dmae(sc, (phys_addr + offset),	/* src DMA address */
		       (addr + offset),	/* dst GRC address */
		       len);
}

void
bnx2x_set_ctx_validation(struct bnx2x_softc *sc, struct eth_context *cxt,
		       uint32_t cid)
{
	/* ustorm cxt validation */
	cxt->ustorm_ag_context.cdu_usage =
	    CDU_RSRVD_VALUE_TYPE_A(HW_CID(sc, cid),
				   CDU_REGION_NUMBER_UCM_AG,
				   ETH_CONNECTION_TYPE);
	/* xcontext validation */
	cxt->xstorm_ag_context.cdu_reserved =
	    CDU_RSRVD_VALUE_TYPE_A(HW_CID(sc, cid),
				   CDU_REGION_NUMBER_XCM_AG,
				   ETH_CONNECTION_TYPE);
}

static void
bnx2x_storm_memset_hc_timeout(struct bnx2x_softc *sc, uint8_t fw_sb_id,
			    uint8_t sb_index, uint8_t ticks)
{
	uint32_t addr =
	    (BAR_CSTRORM_INTMEM +
	     CSTORM_STATUS_BLOCK_DATA_TIMEOUT_OFFSET(fw_sb_id, sb_index));

	REG_WR8(sc, addr, ticks);
}

static void
bnx2x_storm_memset_hc_disable(struct bnx2x_softc *sc, uint16_t fw_sb_id,
			    uint8_t sb_index, uint8_t disable)
{
	uint32_t enable_flag =
	    (disable) ? 0 : (1 << HC_INDEX_DATA_HC_ENABLED_SHIFT);
	uint32_t addr =
	    (BAR_CSTRORM_INTMEM +
	     CSTORM_STATUS_BLOCK_DATA_FLAGS_OFFSET(fw_sb_id, sb_index));
	uint8_t flags;

	/* clear and set */
	flags = REG_RD8(sc, addr);
	flags &= ~HC_INDEX_DATA_HC_ENABLED;
	flags |= enable_flag;
	REG_WR8(sc, addr, flags);
}

void
bnx2x_update_coalesce_sb_index(struct bnx2x_softc *sc, uint8_t fw_sb_id,
			     uint8_t sb_index, uint8_t disable, uint16_t usec)
{
	uint8_t ticks = (usec / 4);

	bnx2x_storm_memset_hc_timeout(sc, fw_sb_id, sb_index, ticks);

	disable = (disable) ? 1 : ((usec) ? 0 : 1);
	bnx2x_storm_memset_hc_disable(sc, fw_sb_id, sb_index, disable);
}

uint32_t elink_cb_reg_read(struct bnx2x_softc *sc, uint32_t reg_addr)
{
	return REG_RD(sc, reg_addr);
}

void elink_cb_reg_write(struct bnx2x_softc *sc, uint32_t reg_addr, uint32_t val)
{
	REG_WR(sc, reg_addr, val);
}

void
elink_cb_event_log(__rte_unused struct bnx2x_softc *sc,
		   __rte_unused const elink_log_id_t elink_log_id, ...)
{
	PMD_DRV_LOG(DEBUG, sc, "ELINK EVENT LOG (%d)", elink_log_id);
}

static int bnx2x_set_spio(struct bnx2x_softc *sc, int spio, uint32_t mode)
{
	uint32_t spio_reg;

	/* Only 2 SPIOs are configurable */
	if ((spio != MISC_SPIO_SPIO4) && (spio != MISC_SPIO_SPIO5)) {
		PMD_DRV_LOG(NOTICE, sc, "Invalid SPIO 0x%x", spio);
		return -1;
	}

	bnx2x_acquire_hw_lock(sc, HW_LOCK_RESOURCE_SPIO);

	/* read SPIO and mask except the float bits */
	spio_reg = (REG_RD(sc, MISC_REG_SPIO) & MISC_SPIO_FLOAT);

	switch (mode) {
	case MISC_SPIO_OUTPUT_LOW:
		/* clear FLOAT and set CLR */
		spio_reg &= ~(spio << MISC_SPIO_FLOAT_POS);
		spio_reg |= (spio << MISC_SPIO_CLR_POS);
		break;

	case MISC_SPIO_OUTPUT_HIGH:
		/* clear FLOAT and set SET */
		spio_reg &= ~(spio << MISC_SPIO_FLOAT_POS);
		spio_reg |= (spio << MISC_SPIO_SET_POS);
		break;

	case MISC_SPIO_INPUT_HI_Z:
		/* set FLOAT */
		spio_reg |= (spio << MISC_SPIO_FLOAT_POS);
		break;

	default:
		break;
	}

	REG_WR(sc, MISC_REG_SPIO, spio_reg);
	bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_SPIO);

	return 0;
}

static int bnx2x_gpio_read(struct bnx2x_softc *sc, int gpio_num, uint8_t port)
{
	/* The GPIO should be swapped if swap register is set and active */
	int gpio_port = ((REG_RD(sc, NIG_REG_PORT_SWAP) &&
			  REG_RD(sc, NIG_REG_STRAP_OVERRIDE)) ^ port);
	int gpio_shift = gpio_num;
	if (gpio_port)
		gpio_shift += MISC_REGISTERS_GPIO_PORT_SHIFT;

	uint32_t gpio_mask = (1 << gpio_shift);
	uint32_t gpio_reg;

	if (gpio_num > MISC_REGISTERS_GPIO_3) {
		PMD_DRV_LOG(NOTICE, sc, "Invalid GPIO %d", gpio_num);
		return -1;
	}

	/* read GPIO value */
	gpio_reg = REG_RD(sc, MISC_REG_GPIO);

	/* get the requested pin value */
	return ((gpio_reg & gpio_mask) == gpio_mask) ? 1 : 0;
}

static int
bnx2x_gpio_write(struct bnx2x_softc *sc, int gpio_num, uint32_t mode, uint8_t port)
{
	/* The GPIO should be swapped if swap register is set and active */
	int gpio_port = ((REG_RD(sc, NIG_REG_PORT_SWAP) &&
			  REG_RD(sc, NIG_REG_STRAP_OVERRIDE)) ^ port);
	int gpio_shift = gpio_num;
	if (gpio_port)
		gpio_shift += MISC_REGISTERS_GPIO_PORT_SHIFT;

	uint32_t gpio_mask = (1 << gpio_shift);
	uint32_t gpio_reg;

	if (gpio_num > MISC_REGISTERS_GPIO_3) {
		PMD_DRV_LOG(NOTICE, sc, "Invalid GPIO %d", gpio_num);
		return -1;
	}

	bnx2x_acquire_hw_lock(sc, HW_LOCK_RESOURCE_GPIO);

	/* read GPIO and mask except the float bits */
	gpio_reg = (REG_RD(sc, MISC_REG_GPIO) & MISC_REGISTERS_GPIO_FLOAT);

	switch (mode) {
	case MISC_REGISTERS_GPIO_OUTPUT_LOW:
		/* clear FLOAT and set CLR */
		gpio_reg &= ~(gpio_mask << MISC_REGISTERS_GPIO_FLOAT_POS);
		gpio_reg |= (gpio_mask << MISC_REGISTERS_GPIO_CLR_POS);
		break;

	case MISC_REGISTERS_GPIO_OUTPUT_HIGH:
		/* clear FLOAT and set SET */
		gpio_reg &= ~(gpio_mask << MISC_REGISTERS_GPIO_FLOAT_POS);
		gpio_reg |= (gpio_mask << MISC_REGISTERS_GPIO_SET_POS);
		break;

	case MISC_REGISTERS_GPIO_INPUT_HI_Z:
		/* set FLOAT */
		gpio_reg |= (gpio_mask << MISC_REGISTERS_GPIO_FLOAT_POS);
		break;

	default:
		break;
	}

	REG_WR(sc, MISC_REG_GPIO, gpio_reg);
	bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_GPIO);

	return 0;
}

static int
bnx2x_gpio_mult_write(struct bnx2x_softc *sc, uint8_t pins, uint32_t mode)
{
	uint32_t gpio_reg;

	/* any port swapping should be handled by caller */

	bnx2x_acquire_hw_lock(sc, HW_LOCK_RESOURCE_GPIO);

	/* read GPIO and mask except the float bits */
	gpio_reg = REG_RD(sc, MISC_REG_GPIO);
	gpio_reg &= ~(pins << MISC_REGISTERS_GPIO_FLOAT_POS);
	gpio_reg &= ~(pins << MISC_REGISTERS_GPIO_CLR_POS);
	gpio_reg &= ~(pins << MISC_REGISTERS_GPIO_SET_POS);

	switch (mode) {
	case MISC_REGISTERS_GPIO_OUTPUT_LOW:
		/* set CLR */
		gpio_reg |= (pins << MISC_REGISTERS_GPIO_CLR_POS);
		break;

	case MISC_REGISTERS_GPIO_OUTPUT_HIGH:
		/* set SET */
		gpio_reg |= (pins << MISC_REGISTERS_GPIO_SET_POS);
		break;

	case MISC_REGISTERS_GPIO_INPUT_HI_Z:
		/* set FLOAT */
		gpio_reg |= (pins << MISC_REGISTERS_GPIO_FLOAT_POS);
		break;

	default:
		PMD_DRV_LOG(NOTICE, sc,
			    "Invalid GPIO mode assignment %d", mode);
		bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_GPIO);
		return -1;
	}

	REG_WR(sc, MISC_REG_GPIO, gpio_reg);
	bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_GPIO);

	return 0;
}

static int
bnx2x_gpio_int_write(struct bnx2x_softc *sc, int gpio_num, uint32_t mode,
		   uint8_t port)
{
	/* The GPIO should be swapped if swap register is set and active */
	int gpio_port = ((REG_RD(sc, NIG_REG_PORT_SWAP) &&
			  REG_RD(sc, NIG_REG_STRAP_OVERRIDE)) ^ port);
	int gpio_shift = gpio_num;
	if (gpio_port)
		gpio_shift += MISC_REGISTERS_GPIO_PORT_SHIFT;

	uint32_t gpio_mask = (1 << gpio_shift);
	uint32_t gpio_reg;

	if (gpio_num > MISC_REGISTERS_GPIO_3) {
		PMD_DRV_LOG(NOTICE, sc, "Invalid GPIO %d", gpio_num);
		return -1;
	}

	bnx2x_acquire_hw_lock(sc, HW_LOCK_RESOURCE_GPIO);

	/* read GPIO int */
	gpio_reg = REG_RD(sc, MISC_REG_GPIO_INT);

	switch (mode) {
	case MISC_REGISTERS_GPIO_INT_OUTPUT_CLR:
		/* clear SET and set CLR */
		gpio_reg &= ~(gpio_mask << MISC_REGISTERS_GPIO_INT_SET_POS);
		gpio_reg |= (gpio_mask << MISC_REGISTERS_GPIO_INT_CLR_POS);
		break;

	case MISC_REGISTERS_GPIO_INT_OUTPUT_SET:
		/* clear CLR and set SET */
		gpio_reg &= ~(gpio_mask << MISC_REGISTERS_GPIO_INT_CLR_POS);
		gpio_reg |= (gpio_mask << MISC_REGISTERS_GPIO_INT_SET_POS);
		break;

	default:
		break;
	}

	REG_WR(sc, MISC_REG_GPIO_INT, gpio_reg);
	bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_GPIO);

	return 0;
}

uint32_t
elink_cb_gpio_read(struct bnx2x_softc * sc, uint16_t gpio_num, uint8_t port)
{
	return bnx2x_gpio_read(sc, gpio_num, port);
}

uint8_t elink_cb_gpio_write(struct bnx2x_softc * sc, uint16_t gpio_num, uint8_t mode,	/* 0=low 1=high */
			    uint8_t port)
{
	return bnx2x_gpio_write(sc, gpio_num, mode, port);
}

uint8_t
elink_cb_gpio_mult_write(struct bnx2x_softc * sc, uint8_t pins,
			 uint8_t mode /* 0=low 1=high */ )
{
	return bnx2x_gpio_mult_write(sc, pins, mode);
}

uint8_t elink_cb_gpio_int_write(struct bnx2x_softc * sc, uint16_t gpio_num, uint8_t mode,	/* 0=low 1=high */
				uint8_t port)
{
	return bnx2x_gpio_int_write(sc, gpio_num, mode, port);
}

void elink_cb_notify_link_changed(struct bnx2x_softc *sc)
{
	REG_WR(sc, (MISC_REG_AEU_GENERAL_ATTN_12 +
		    (SC_FUNC(sc) * sizeof(uint32_t))), 1);
}

/* send the MCP a request, block until there is a reply */
uint32_t
elink_cb_fw_command(struct bnx2x_softc *sc, uint32_t command, uint32_t param)
{
	int mb_idx = SC_FW_MB_IDX(sc);
	uint32_t seq;
	uint32_t rc = 0;
	uint32_t cnt = 1;
	uint8_t delay = CHIP_REV_IS_SLOW(sc) ? 100 : 10;

	seq = ++sc->fw_seq;
	SHMEM_WR(sc, func_mb[mb_idx].drv_mb_param, param);
	SHMEM_WR(sc, func_mb[mb_idx].drv_mb_header, (command | seq));

	PMD_DRV_LOG(DEBUG, sc,
		    "wrote command 0x%08x to FW MB param 0x%08x",
		    (command | seq), param);

	/* Let the FW do it's magic. GIve it up to 5 seconds... */
	do {
		DELAY(delay * 1000);
		rc = SHMEM_RD(sc, func_mb[mb_idx].fw_mb_header);
	} while ((seq != (rc & FW_MSG_SEQ_NUMBER_MASK)) && (cnt++ < 500));

	/* is this a reply to our command? */
	if (seq == (rc & FW_MSG_SEQ_NUMBER_MASK)) {
		rc &= FW_MSG_CODE_MASK;
	} else {
		/* Ruh-roh! */
		PMD_DRV_LOG(NOTICE, sc, "FW failed to respond!");
		rc = 0;
	}

	return rc;
}

static uint32_t
bnx2x_fw_command(struct bnx2x_softc *sc, uint32_t command, uint32_t param)
{
	return elink_cb_fw_command(sc, command, param);
}

static void
__storm_memset_dma_mapping(struct bnx2x_softc *sc, uint32_t addr,
			   rte_iova_t mapping)
{
	REG_WR(sc, addr, U64_LO(mapping));
	REG_WR(sc, (addr + 4), U64_HI(mapping));
}

static void
storm_memset_spq_addr(struct bnx2x_softc *sc, rte_iova_t mapping,
		      uint16_t abs_fid)
{
	uint32_t addr = (XSEM_REG_FAST_MEMORY +
			 XSTORM_SPQ_PAGE_BASE_OFFSET(abs_fid));
	__storm_memset_dma_mapping(sc, addr, mapping);
}

static void
storm_memset_vf_to_pf(struct bnx2x_softc *sc, uint16_t abs_fid, uint16_t pf_id)
{
	REG_WR8(sc, (BAR_XSTRORM_INTMEM + XSTORM_VF_TO_PF_OFFSET(abs_fid)),
		pf_id);
	REG_WR8(sc, (BAR_CSTRORM_INTMEM + CSTORM_VF_TO_PF_OFFSET(abs_fid)),
		pf_id);
	REG_WR8(sc, (BAR_TSTRORM_INTMEM + TSTORM_VF_TO_PF_OFFSET(abs_fid)),
		pf_id);
	REG_WR8(sc, (BAR_USTRORM_INTMEM + USTORM_VF_TO_PF_OFFSET(abs_fid)),
		pf_id);
}

static void
storm_memset_func_en(struct bnx2x_softc *sc, uint16_t abs_fid, uint8_t enable)
{
	REG_WR8(sc, (BAR_XSTRORM_INTMEM + XSTORM_FUNC_EN_OFFSET(abs_fid)),
		enable);
	REG_WR8(sc, (BAR_CSTRORM_INTMEM + CSTORM_FUNC_EN_OFFSET(abs_fid)),
		enable);
	REG_WR8(sc, (BAR_TSTRORM_INTMEM + TSTORM_FUNC_EN_OFFSET(abs_fid)),
		enable);
	REG_WR8(sc, (BAR_USTRORM_INTMEM + USTORM_FUNC_EN_OFFSET(abs_fid)),
		enable);
}

static void
storm_memset_eq_data(struct bnx2x_softc *sc, struct event_ring_data *eq_data,
		     uint16_t pfid)
{
	uint32_t addr;
	size_t size;

	addr = (BAR_CSTRORM_INTMEM + CSTORM_EVENT_RING_DATA_OFFSET(pfid));
	size = sizeof(struct event_ring_data);
	ecore_storm_memset_struct(sc, addr, size, (uint32_t *) eq_data);
}

static void
storm_memset_eq_prod(struct bnx2x_softc *sc, uint16_t eq_prod, uint16_t pfid)
{
	uint32_t addr = (BAR_CSTRORM_INTMEM +
			 CSTORM_EVENT_RING_PROD_OFFSET(pfid));
	REG_WR16(sc, addr, eq_prod);
}

/*
 * Post a slowpath command.
 *
 * A slowpath command is used to propagate a configuration change through
 * the controller in a controlled manner, allowing each STORM processor and
 * other H/W blocks to phase in the change.  The commands sent on the
 * slowpath are referred to as ramrods.  Depending on the ramrod used the
 * completion of the ramrod will occur in different ways.  Here's a
 * breakdown of ramrods and how they complete:
 *
 * RAMROD_CMD_ID_ETH_PORT_SETUP
 *   Used to setup the leading connection on a port.  Completes on the
 *   Receive Completion Queue (RCQ) of that port (typically fp[0]).
 *
 * RAMROD_CMD_ID_ETH_CLIENT_SETUP
 *   Used to setup an additional connection on a port.  Completes on the
 *   RCQ of the multi-queue/RSS connection being initialized.
 *
 * RAMROD_CMD_ID_ETH_STAT_QUERY
 *   Used to force the storm processors to update the statistics database
 *   in host memory.  This ramrod is send on the leading connection CID and
 *   completes as an index increment of the CSTORM on the default status
 *   block.
 *
 * RAMROD_CMD_ID_ETH_UPDATE
 *   Used to update the state of the leading connection, usually to udpate
 *   the RSS indirection table.  Completes on the RCQ of the leading
 *   connection. (Not currently used under FreeBSD until OS support becomes
 *   available.)
 *
 * RAMROD_CMD_ID_ETH_HALT
 *   Used when tearing down a connection prior to driver unload.  Completes
 *   on the RCQ of the multi-queue/RSS connection being torn down.  Don't
 *   use this on the leading connection.
 *
 * RAMROD_CMD_ID_ETH_SET_MAC
 *   Sets the Unicast/Broadcast/Multicast used by the port.  Completes on
 *   the RCQ of the leading connection.
 *
 * RAMROD_CMD_ID_ETH_CFC_DEL
 *   Used when tearing down a conneciton prior to driver unload.  Completes
 *   on the RCQ of the leading connection (since the current connection
 *   has been completely removed from controller memory).
 *
 * RAMROD_CMD_ID_ETH_PORT_DEL
 *   Used to tear down the leading connection prior to driver unload,
 *   typically fp[0].  Completes as an index increment of the CSTORM on the
 *   default status block.
 *
 * RAMROD_CMD_ID_ETH_FORWARD_SETUP
 *   Used for connection offload.  Completes on the RCQ of the multi-queue
 *   RSS connection that is being offloaded.  (Not currently used under
 *   FreeBSD.)
 *
 * There can only be one command pending per function.
 *
 * Returns:
 *   0 = Success, !0 = Failure.
 */

/* must be called under the spq lock */
static inline struct eth_spe *bnx2x_sp_get_next(struct bnx2x_softc *sc)
{
	struct eth_spe *next_spe = sc->spq_prod_bd;

	if (sc->spq_prod_bd == sc->spq_last_bd) {
		/* wrap back to the first eth_spq */
		sc->spq_prod_bd = sc->spq;
		sc->spq_prod_idx = 0;
	} else {
		sc->spq_prod_bd++;
		sc->spq_prod_idx++;
	}

	return next_spe;
}

/* must be called under the spq lock */
static void bnx2x_sp_prod_update(struct bnx2x_softc *sc)
{
	int func = SC_FUNC(sc);

	/*
	 * Make sure that BD data is updated before writing the producer.
	 * BD data is written to the memory, the producer is read from the
	 * memory, thus we need a full memory barrier to ensure the ordering.
	 */
	mb();

	REG_WR16(sc, (BAR_XSTRORM_INTMEM + XSTORM_SPQ_PROD_OFFSET(func)),
		 sc->spq_prod_idx);

	mb();
}

/**
 * bnx2x_is_contextless_ramrod - check if the current command ends on EQ
 *
 * @cmd:      command to check
 * @cmd_type: command type
 */
static int bnx2x_is_contextless_ramrod(int cmd, int cmd_type)
{
	if ((cmd_type == NONE_CONNECTION_TYPE) ||
	    (cmd == RAMROD_CMD_ID_ETH_FORWARD_SETUP) ||
	    (cmd == RAMROD_CMD_ID_ETH_CLASSIFICATION_RULES) ||
	    (cmd == RAMROD_CMD_ID_ETH_FILTER_RULES) ||
	    (cmd == RAMROD_CMD_ID_ETH_MULTICAST_RULES) ||
	    (cmd == RAMROD_CMD_ID_ETH_SET_MAC) ||
	    (cmd == RAMROD_CMD_ID_ETH_RSS_UPDATE)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/**
 * bnx2x_sp_post - place a single command on an SP ring
 *
 * @sc:         driver handle
 * @command:    command to place (e.g. SETUP, FILTER_RULES, etc.)
 * @cid:        SW CID the command is related to
 * @data_hi:    command private data address (high 32 bits)
 * @data_lo:    command private data address (low 32 bits)
 * @cmd_type:   command type (e.g. NONE, ETH)
 *
 * SP data is handled as if it's always an address pair, thus data fields are
 * not swapped to little endian in upper functions. Instead this function swaps
 * data as if it's two uint32 fields.
 */
int
bnx2x_sp_post(struct bnx2x_softc *sc, int command, int cid, uint32_t data_hi,
	    uint32_t data_lo, int cmd_type)
{
	struct eth_spe *spe;
	uint16_t type;
	int common;

	common = bnx2x_is_contextless_ramrod(command, cmd_type);

	if (common) {
		if (!atomic_load_acq_long(&sc->eq_spq_left)) {
			PMD_DRV_LOG(INFO, sc, "EQ ring is full!");
			return -1;
		}
	} else {
		if (!atomic_load_acq_long(&sc->cq_spq_left)) {
			PMD_DRV_LOG(INFO, sc, "SPQ ring is full!");
			return -1;
		}
	}

	spe = bnx2x_sp_get_next(sc);

	/* CID needs port number to be encoded int it */
	spe->hdr.conn_and_cmd_data =
	    htole32((command << SPE_HDR_CMD_ID_SHIFT) | HW_CID(sc, cid));

	type = (cmd_type << SPE_HDR_CONN_TYPE_SHIFT) & SPE_HDR_CONN_TYPE;

	/* TBD: Check if it works for VFs */
	type |= ((SC_FUNC(sc) << SPE_HDR_FUNCTION_ID_SHIFT) &
		 SPE_HDR_FUNCTION_ID);

	spe->hdr.type = htole16(type);

	spe->data.update_data_addr.hi = htole32(data_hi);
	spe->data.update_data_addr.lo = htole32(data_lo);

	/*
	 * It's ok if the actual decrement is issued towards the memory
	 * somewhere between the lock and unlock. Thus no more explict
	 * memory barrier is needed.
	 */
	if (common) {
		atomic_subtract_acq_long(&sc->eq_spq_left, 1);
	} else {
		atomic_subtract_acq_long(&sc->cq_spq_left, 1);
	}

	PMD_DRV_LOG(DEBUG, sc,
		    "SPQE[%x] (%x:%x) (cmd, common?) (%d,%d) hw_cid %x"
		    "data (%x:%x) type(0x%x) left (CQ, EQ) (%lx,%lx)",
		    sc->spq_prod_idx,
		    (uint32_t) U64_HI(sc->spq_dma.paddr),
		    (uint32_t) (U64_LO(sc->spq_dma.paddr) +
				(uint8_t *) sc->spq_prod_bd -
				(uint8_t *) sc->spq), command, common,
		    HW_CID(sc, cid), data_hi, data_lo, type,
		    atomic_load_acq_long(&sc->cq_spq_left),
		    atomic_load_acq_long(&sc->eq_spq_left));

	/* RAMROD completion is processed in bnx2x_intr_legacy()
	 * which can run from different contexts.
	 * Ask bnx2x_intr_intr() to process RAMROD
	 * completion whenever it gets scheduled.
	 */
	rte_atomic32_set(&sc->scan_fp, 1);
	bnx2x_sp_prod_update(sc);

	return 0;
}

static void bnx2x_drv_pulse(struct bnx2x_softc *sc)
{
	SHMEM_WR(sc, func_mb[SC_FW_MB_IDX(sc)].drv_pulse_mb,
		 sc->fw_drv_pulse_wr_seq);
}

static int bnx2x_tx_queue_has_work(const struct bnx2x_fastpath *fp)
{
	uint16_t hw_cons;
	struct bnx2x_tx_queue *txq = fp->sc->tx_queues[fp->index];

	if (unlikely(!txq)) {
		PMD_TX_LOG(ERR, "ERROR: TX queue is NULL");
		return 0;
	}

	mb();			/* status block fields can change */
	hw_cons = le16toh(*fp->tx_cons_sb);
	return hw_cons != txq->tx_pkt_head;
}

static uint8_t bnx2x_has_tx_work(struct bnx2x_fastpath *fp)
{
	/* expand this for multi-cos if ever supported */
	return bnx2x_tx_queue_has_work(fp);
}

static int bnx2x_has_rx_work(struct bnx2x_fastpath *fp)
{
	uint16_t rx_cq_cons_sb;
	struct bnx2x_rx_queue *rxq;
	rxq = fp->sc->rx_queues[fp->index];
	if (unlikely(!rxq)) {
		PMD_RX_LOG(ERR, "ERROR: RX queue is NULL");
		return 0;
	}

	mb();			/* status block fields can change */
	rx_cq_cons_sb = le16toh(*fp->rx_cq_cons_sb);
	if (unlikely((rx_cq_cons_sb & MAX_RCQ_ENTRIES(rxq)) ==
		     MAX_RCQ_ENTRIES(rxq)))
		rx_cq_cons_sb++;

	PMD_RX_LOG(DEBUG, "hw CQ cons = %d, sw CQ cons = %d",
		   rx_cq_cons_sb, rxq->rx_cq_head);

	return rxq->rx_cq_head != rx_cq_cons_sb;
}

static void
bnx2x_sp_event(struct bnx2x_softc *sc, struct bnx2x_fastpath *fp,
	     union eth_rx_cqe *rr_cqe)
{
	int cid = SW_CID(rr_cqe->ramrod_cqe.conn_and_cmd_data);
	int command = CQE_CMD(rr_cqe->ramrod_cqe.conn_and_cmd_data);
	enum ecore_queue_cmd drv_cmd = ECORE_Q_CMD_MAX;
	struct ecore_queue_sp_obj *q_obj = &BNX2X_SP_OBJ(sc, fp).q_obj;

	PMD_DRV_LOG(DEBUG, sc,
		    "fp=%d cid=%d got ramrod #%d state is %x type is %d",
		    fp->index, cid, command, sc->state,
		    rr_cqe->ramrod_cqe.ramrod_type);

	switch (command) {
	case (RAMROD_CMD_ID_ETH_CLIENT_UPDATE):
		PMD_DRV_LOG(DEBUG, sc, "got UPDATE ramrod. CID %d", cid);
		drv_cmd = ECORE_Q_CMD_UPDATE;
		break;

	case (RAMROD_CMD_ID_ETH_CLIENT_SETUP):
		PMD_DRV_LOG(DEBUG, sc, "got MULTI[%d] setup ramrod", cid);
		drv_cmd = ECORE_Q_CMD_SETUP;
		break;

	case (RAMROD_CMD_ID_ETH_TX_QUEUE_SETUP):
		PMD_DRV_LOG(DEBUG, sc,
			    "got MULTI[%d] tx-only setup ramrod", cid);
		drv_cmd = ECORE_Q_CMD_SETUP_TX_ONLY;
		break;

	case (RAMROD_CMD_ID_ETH_HALT):
		PMD_DRV_LOG(DEBUG, sc, "got MULTI[%d] halt ramrod", cid);
		drv_cmd = ECORE_Q_CMD_HALT;
		break;

	case (RAMROD_CMD_ID_ETH_TERMINATE):
		PMD_DRV_LOG(DEBUG, sc, "got MULTI[%d] teminate ramrod", cid);
		drv_cmd = ECORE_Q_CMD_TERMINATE;
		break;

	case (RAMROD_CMD_ID_ETH_EMPTY):
		PMD_DRV_LOG(DEBUG, sc, "got MULTI[%d] empty ramrod", cid);
		drv_cmd = ECORE_Q_CMD_EMPTY;
		break;

	default:
		PMD_DRV_LOG(DEBUG, sc,
			    "ERROR: unexpected MC reply (%d)"
			    "on fp[%d]", command, fp->index);
		return;
	}

	if ((drv_cmd != ECORE_Q_CMD_MAX) &&
	    q_obj->complete_cmd(sc, q_obj, drv_cmd)) {
		/*
		 * q_obj->complete_cmd() failure means that this was
		 * an unexpected completion.
		 *
		 * In this case we don't want to increase the sc->spq_left
		 * because apparently we haven't sent this command the first
		 * place.
		 */
		// rte_panic("Unexpected SP completion");
		return;
	}

	atomic_add_acq_long(&sc->cq_spq_left, 1);

	PMD_DRV_LOG(DEBUG, sc, "sc->cq_spq_left 0x%lx",
		    atomic_load_acq_long(&sc->cq_spq_left));
}

static uint8_t bnx2x_rxeof(struct bnx2x_softc *sc, struct bnx2x_fastpath *fp)
{
	struct bnx2x_rx_queue *rxq;
	uint16_t bd_cons, bd_prod, bd_prod_fw, comp_ring_cons;
	uint16_t hw_cq_cons, sw_cq_cons, sw_cq_prod;

	rte_spinlock_lock(&(fp)->rx_mtx);

	rxq = sc->rx_queues[fp->index];
	if (!rxq) {
		PMD_RX_LOG(ERR, "RX queue %d is NULL", fp->index);
		rte_spinlock_unlock(&(fp)->rx_mtx);
		return 0;
	}

	/* CQ "next element" is of the size of the regular element */
	hw_cq_cons = le16toh(*fp->rx_cq_cons_sb);
	if (unlikely((hw_cq_cons & USABLE_RCQ_ENTRIES_PER_PAGE) ==
		     USABLE_RCQ_ENTRIES_PER_PAGE)) {
		hw_cq_cons++;
	}

	bd_cons = rxq->rx_bd_head;
	bd_prod = rxq->rx_bd_tail;
	bd_prod_fw = bd_prod;
	sw_cq_cons = rxq->rx_cq_head;
	sw_cq_prod = rxq->rx_cq_tail;

	/*
	 * Memory barrier necessary as speculative reads of the rx
	 * buffer can be ahead of the index in the status block
	 */
	rmb();

	while (sw_cq_cons != hw_cq_cons) {
		union eth_rx_cqe *cqe;
		struct eth_fast_path_rx_cqe *cqe_fp;
		uint8_t cqe_fp_flags;
		enum eth_rx_cqe_type cqe_fp_type;

		comp_ring_cons = RCQ_ENTRY(sw_cq_cons, rxq);
		bd_prod = RX_BD(bd_prod, rxq);
		bd_cons = RX_BD(bd_cons, rxq);

		cqe = &rxq->cq_ring[comp_ring_cons];
		cqe_fp = &cqe->fast_path_cqe;
		cqe_fp_flags = cqe_fp->type_error_flags;
		cqe_fp_type = cqe_fp_flags & ETH_FAST_PATH_RX_CQE_TYPE;

		/* is this a slowpath msg? */
		if (CQE_TYPE_SLOW(cqe_fp_type)) {
			bnx2x_sp_event(sc, fp, cqe);
			goto next_cqe;
		}

		/* is this an error packet? */
		if (unlikely(cqe_fp_flags &
			     ETH_FAST_PATH_RX_CQE_PHY_DECODE_ERR_FLG)) {
			PMD_RX_LOG(DEBUG, "flags 0x%x rx packet %u",
				   cqe_fp_flags, sw_cq_cons);
			goto next_rx;
		}

		PMD_RX_LOG(DEBUG, "Dropping fastpath called from attn poller!");

next_rx:
		bd_cons = NEXT_RX_BD(bd_cons);
		bd_prod = NEXT_RX_BD(bd_prod);
		bd_prod_fw = NEXT_RX_BD(bd_prod_fw);

next_cqe:
		sw_cq_prod = NEXT_RCQ_IDX(sw_cq_prod);
		sw_cq_cons = NEXT_RCQ_IDX(sw_cq_cons);

	}			/* while work to do */

	rxq->rx_bd_head = bd_cons;
	rxq->rx_bd_tail = bd_prod_fw;
	rxq->rx_cq_head = sw_cq_cons;
	rxq->rx_cq_tail = sw_cq_prod;

	PMD_RX_LOG(DEBUG, "BD prod = %d, sw CQ prod = %d",
		   bd_prod_fw, sw_cq_prod);

	/* Update producers */
	bnx2x_update_rx_prod(sc, fp, bd_prod_fw, sw_cq_prod);

	rte_spinlock_unlock(&(fp)->rx_mtx);

	return sw_cq_cons != hw_cq_cons;
}

static uint16_t
bnx2x_free_tx_pkt(__rte_unused struct bnx2x_fastpath *fp, struct bnx2x_tx_queue *txq,
		uint16_t pkt_idx, uint16_t bd_idx)
{
	struct eth_tx_start_bd *tx_start_bd =
	    &txq->tx_ring[TX_BD(bd_idx, txq)].start_bd;
	uint16_t nbd = rte_le_to_cpu_16(tx_start_bd->nbd);
	struct rte_mbuf *tx_mbuf = txq->sw_ring[TX_BD(pkt_idx, txq)];

	if (likely(tx_mbuf != NULL)) {
		rte_pktmbuf_free_seg(tx_mbuf);
	} else {
		PMD_RX_LOG(ERR, "fp[%02d] lost mbuf %lu",
			   fp->index, (unsigned long)TX_BD(pkt_idx, txq));
	}

	txq->sw_ring[TX_BD(pkt_idx, txq)] = NULL;
	txq->nb_tx_avail += nbd;

	while (nbd--)
		bd_idx = NEXT_TX_BD(bd_idx);

	return bd_idx;
}

/* processes transmit completions */
uint8_t bnx2x_txeof(__rte_unused struct bnx2x_softc * sc, struct bnx2x_fastpath * fp)
{
	uint16_t bd_cons, hw_cons, sw_cons;
	__rte_unused uint16_t tx_bd_avail;

	struct bnx2x_tx_queue *txq = fp->sc->tx_queues[fp->index];

	if (unlikely(!txq)) {
		PMD_TX_LOG(ERR, "ERROR: TX queue is NULL");
		return 0;
	}

	bd_cons = txq->tx_bd_head;
	hw_cons = rte_le_to_cpu_16(*fp->tx_cons_sb);
	sw_cons = txq->tx_pkt_head;

	while (sw_cons != hw_cons) {
		bd_cons = bnx2x_free_tx_pkt(fp, txq, sw_cons, bd_cons);
		sw_cons++;
	}

	txq->tx_pkt_head = sw_cons;
	txq->tx_bd_head = bd_cons;

	tx_bd_avail = txq->nb_tx_avail;

	PMD_TX_LOG(DEBUG, "fp[%02d] avail=%u cons_sb=%u, "
		   "pkt_head=%u pkt_tail=%u bd_head=%u bd_tail=%u",
		   fp->index, tx_bd_avail, hw_cons,
		   txq->tx_pkt_head, txq->tx_pkt_tail,
		   txq->tx_bd_head, txq->tx_bd_tail);
	return TRUE;
}

static void bnx2x_drain_tx_queues(struct bnx2x_softc *sc)
{
	struct bnx2x_fastpath *fp;
	int i, count;

	/* wait until all TX fastpath tasks have completed */
	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];

		count = 1000;

		while (bnx2x_has_tx_work(fp)) {
			bnx2x_txeof(sc, fp);

			if (count == 0) {
				PMD_TX_LOG(ERR,
					   "Timeout waiting for fp[%d] "
					   "transmits to complete!", i);
				rte_panic("tx drain failure");
				return;
			}

			count--;
			DELAY(1000);
			rmb();
		}
	}

	return;
}

static int
bnx2x_del_all_macs(struct bnx2x_softc *sc, struct ecore_vlan_mac_obj *mac_obj,
		 int mac_type, uint8_t wait_for_comp)
{
	uint32_t ramrod_flags = 0, vlan_mac_flags = 0;
	int rc;

	/* wait for completion of requested */
	if (wait_for_comp) {
		rte_bit_relaxed_set32(RAMROD_COMP_WAIT, &ramrod_flags);
	}

	/* Set the mac type of addresses we want to clear */
	rte_bit_relaxed_set32(mac_type, &vlan_mac_flags);

	rc = mac_obj->delete_all(sc, mac_obj, &vlan_mac_flags, &ramrod_flags);
	if (rc < 0)
		PMD_DRV_LOG(ERR, sc, "Failed to delete MACs (%d)", rc);

	return rc;
}

static int
bnx2x_fill_accept_flags(struct bnx2x_softc *sc, uint32_t rx_mode,
			uint32_t *rx_accept_flags, uint32_t *tx_accept_flags)
{
	/* Clear the flags first */
	*rx_accept_flags = 0;
	*tx_accept_flags = 0;

	switch (rx_mode) {
	case BNX2X_RX_MODE_NONE:
		/*
		 * 'drop all' supersedes any accept flags that may have been
		 * passed to the function.
		 */
		break;

	case BNX2X_RX_MODE_NORMAL:
		rte_bit_relaxed_set32(ECORE_ACCEPT_UNICAST, rx_accept_flags);
		rte_bit_relaxed_set32(ECORE_ACCEPT_MULTICAST, rx_accept_flags);
		rte_bit_relaxed_set32(ECORE_ACCEPT_BROADCAST, rx_accept_flags);

		/* internal switching mode */
		rte_bit_relaxed_set32(ECORE_ACCEPT_UNICAST, tx_accept_flags);
		rte_bit_relaxed_set32(ECORE_ACCEPT_MULTICAST, tx_accept_flags);
		rte_bit_relaxed_set32(ECORE_ACCEPT_BROADCAST, tx_accept_flags);

		break;

	case BNX2X_RX_MODE_ALLMULTI:
		rte_bit_relaxed_set32(ECORE_ACCEPT_UNICAST, rx_accept_flags);
		rte_bit_relaxed_set32(ECORE_ACCEPT_ALL_MULTICAST,
				      rx_accept_flags);
		rte_bit_relaxed_set32(ECORE_ACCEPT_BROADCAST, rx_accept_flags);

		/* internal switching mode */
		rte_bit_relaxed_set32(ECORE_ACCEPT_UNICAST, tx_accept_flags);
		rte_bit_relaxed_set32(ECORE_ACCEPT_ALL_MULTICAST,
				      tx_accept_flags);
		rte_bit_relaxed_set32(ECORE_ACCEPT_BROADCAST, tx_accept_flags);

		break;

	case BNX2X_RX_MODE_ALLMULTI_PROMISC:
	case BNX2X_RX_MODE_PROMISC:
		/*
		 * According to deffinition of SI mode, iface in promisc mode
		 * should receive matched and unmatched (in resolution of port)
		 * unicast packets.
		 */
		rte_bit_relaxed_set32(ECORE_ACCEPT_UNMATCHED, rx_accept_flags);
		rte_bit_relaxed_set32(ECORE_ACCEPT_UNICAST, rx_accept_flags);
		rte_bit_relaxed_set32(ECORE_ACCEPT_ALL_MULTICAST,
				      rx_accept_flags);
		rte_bit_relaxed_set32(ECORE_ACCEPT_BROADCAST, rx_accept_flags);

		/* internal switching mode */
		rte_bit_relaxed_set32(ECORE_ACCEPT_ALL_MULTICAST,
				      tx_accept_flags);
		rte_bit_relaxed_set32(ECORE_ACCEPT_BROADCAST, tx_accept_flags);

		if (IS_MF_SI(sc)) {
			rte_bit_relaxed_set32(ECORE_ACCEPT_ALL_UNICAST,
					tx_accept_flags);
		} else {
			rte_bit_relaxed_set32(ECORE_ACCEPT_UNICAST,
					      tx_accept_flags);
		}

		break;

	default:
		PMD_RX_LOG(ERR, "Unknown rx_mode (%d)", rx_mode);
		return -1;
	}

	/* Set ACCEPT_ANY_VLAN as we do not enable filtering by VLAN */
	if (rx_mode != BNX2X_RX_MODE_NONE) {
		rte_bit_relaxed_set32(ECORE_ACCEPT_ANY_VLAN, rx_accept_flags);
		rte_bit_relaxed_set32(ECORE_ACCEPT_ANY_VLAN, tx_accept_flags);
	}

	return 0;
}

static int
bnx2x_set_q_rx_mode(struct bnx2x_softc *sc, uint8_t cl_id,
		  unsigned long rx_mode_flags,
		  unsigned long rx_accept_flags,
		  unsigned long tx_accept_flags, unsigned long ramrod_flags)
{
	struct ecore_rx_mode_ramrod_params ramrod_param;
	int rc;

	memset(&ramrod_param, 0, sizeof(ramrod_param));

	/* Prepare ramrod parameters */
	ramrod_param.cid = 0;
	ramrod_param.cl_id = cl_id;
	ramrod_param.rx_mode_obj = &sc->rx_mode_obj;
	ramrod_param.func_id = SC_FUNC(sc);

	ramrod_param.pstate = &sc->sp_state;
	ramrod_param.state = ECORE_FILTER_RX_MODE_PENDING;

	ramrod_param.rdata = BNX2X_SP(sc, rx_mode_rdata);
	ramrod_param.rdata_mapping =
	    (rte_iova_t)BNX2X_SP_MAPPING(sc, rx_mode_rdata),
	    rte_bit_relaxed_set32(ECORE_FILTER_RX_MODE_PENDING, &sc->sp_state);

	ramrod_param.ramrod_flags = ramrod_flags;
	ramrod_param.rx_mode_flags = rx_mode_flags;

	ramrod_param.rx_accept_flags = rx_accept_flags;
	ramrod_param.tx_accept_flags = tx_accept_flags;

	rc = ecore_config_rx_mode(sc, &ramrod_param);
	if (rc < 0) {
		PMD_RX_LOG(ERR, "Set rx_mode %d failed", sc->rx_mode);
		return rc;
	}

	return 0;
}

int bnx2x_set_storm_rx_mode(struct bnx2x_softc *sc)
{
	uint32_t rx_mode_flags = 0, ramrod_flags = 0;
	uint32_t rx_accept_flags = 0, tx_accept_flags = 0;
	int rc;

	rc = bnx2x_fill_accept_flags(sc, sc->rx_mode, &rx_accept_flags,
				   &tx_accept_flags);
	if (rc) {
		return rc;
	}

	rte_bit_relaxed_set32(RAMROD_RX, &ramrod_flags);
	rte_bit_relaxed_set32(RAMROD_TX, &ramrod_flags);
	rte_bit_relaxed_set32(RAMROD_COMP_WAIT, &ramrod_flags);

	return bnx2x_set_q_rx_mode(sc, sc->fp[0].cl_id, rx_mode_flags,
				 rx_accept_flags, tx_accept_flags,
				 ramrod_flags);
}

/* returns the "mcp load_code" according to global load_count array */
static int bnx2x_nic_load_no_mcp(struct bnx2x_softc *sc)
{
	int path = SC_PATH(sc);
	int port = SC_PORT(sc);

	PMD_DRV_LOG(INFO, sc, "NO MCP - load counts[%d]      %d, %d, %d",
		    path, load_count[path][0], load_count[path][1],
		    load_count[path][2]);

	load_count[path][0]++;
	load_count[path][1 + port]++;
	PMD_DRV_LOG(INFO, sc, "NO MCP - new load counts[%d]  %d, %d, %d",
		    path, load_count[path][0], load_count[path][1],
		    load_count[path][2]);
	if (load_count[path][0] == 1)
		return FW_MSG_CODE_DRV_LOAD_COMMON;
	else if (load_count[path][1 + port] == 1)
		return FW_MSG_CODE_DRV_LOAD_PORT;
	else
		return FW_MSG_CODE_DRV_LOAD_FUNCTION;
}

/* returns the "mcp load_code" according to global load_count array */
static int bnx2x_nic_unload_no_mcp(struct bnx2x_softc *sc)
{
	int port = SC_PORT(sc);
	int path = SC_PATH(sc);

	PMD_DRV_LOG(INFO, sc, "NO MCP - load counts[%d]      %d, %d, %d",
		    path, load_count[path][0], load_count[path][1],
		    load_count[path][2]);
	load_count[path][0]--;
	load_count[path][1 + port]--;
	PMD_DRV_LOG(INFO, sc, "NO MCP - new load counts[%d]  %d, %d, %d",
		    path, load_count[path][0], load_count[path][1],
		    load_count[path][2]);
	if (load_count[path][0] == 0) {
		return FW_MSG_CODE_DRV_UNLOAD_COMMON;
	} else if (load_count[path][1 + port] == 0) {
		return FW_MSG_CODE_DRV_UNLOAD_PORT;
	} else {
		return FW_MSG_CODE_DRV_UNLOAD_FUNCTION;
	}
}

/* request unload mode from the MCP: COMMON, PORT or FUNCTION */
static uint32_t bnx2x_send_unload_req(struct bnx2x_softc *sc, int unload_mode)
{
	uint32_t reset_code = 0;

	/* Select the UNLOAD request mode */
	if (unload_mode == UNLOAD_NORMAL) {
		reset_code = DRV_MSG_CODE_UNLOAD_REQ_WOL_DIS;
	} else {
		reset_code = DRV_MSG_CODE_UNLOAD_REQ_WOL_DIS;
	}

	/* Send the request to the MCP */
	if (!BNX2X_NOMCP(sc)) {
		reset_code = bnx2x_fw_command(sc, reset_code, 0);
	} else {
		reset_code = bnx2x_nic_unload_no_mcp(sc);
	}

	return reset_code;
}

/* send UNLOAD_DONE command to the MCP */
static void bnx2x_send_unload_done(struct bnx2x_softc *sc, uint8_t keep_link)
{
	uint32_t reset_param =
	    keep_link ? DRV_MSG_CODE_UNLOAD_SKIP_LINK_RESET : 0;

	/* Report UNLOAD_DONE to MCP */
	if (!BNX2X_NOMCP(sc)) {
		bnx2x_fw_command(sc, DRV_MSG_CODE_UNLOAD_DONE, reset_param);
	}
}

static int bnx2x_func_wait_started(struct bnx2x_softc *sc)
{
	int tout = 50;

	if (!sc->port.pmf) {
		return 0;
	}

	/*
	 * (assumption: No Attention from MCP at this stage)
	 * PMF probably in the middle of TX disable/enable transaction
	 * 1. Sync IRS for default SB
	 * 2. Sync SP queue - this guarantees us that attention handling started
	 * 3. Wait, that TX disable/enable transaction completes
	 *
	 * 1+2 guarantee that if DCBX attention was scheduled it already changed
	 * pending bit of transaction from STARTED-->TX_STOPPED, if we already
	 * received completion for the transaction the state is TX_STOPPED.
	 * State will return to STARTED after completion of TX_STOPPED-->STARTED
	 * transaction.
	 */

	while (ecore_func_get_state(sc, &sc->func_obj) !=
	       ECORE_F_STATE_STARTED && tout--) {
		DELAY(20000);
	}

	if (ecore_func_get_state(sc, &sc->func_obj) != ECORE_F_STATE_STARTED) {
		/*
		 * Failed to complete the transaction in a "good way"
		 * Force both transactions with CLR bit.
		 */
		struct ecore_func_state_params func_params = { NULL };

		PMD_DRV_LOG(NOTICE, sc, "Unexpected function state! "
			    "Forcing STARTED-->TX_STOPPED-->STARTED");

		func_params.f_obj = &sc->func_obj;
		rte_bit_relaxed_set32(RAMROD_DRV_CLR_ONLY,
				      &func_params.ramrod_flags);

		/* STARTED-->TX_STOPPED */
		func_params.cmd = ECORE_F_CMD_TX_STOP;
		ecore_func_state_change(sc, &func_params);

		/* TX_STOPPED-->STARTED */
		func_params.cmd = ECORE_F_CMD_TX_START;
		return ecore_func_state_change(sc, &func_params);
	}

	return 0;
}

static int bnx2x_stop_queue(struct bnx2x_softc *sc, int index)
{
	struct bnx2x_fastpath *fp = &sc->fp[index];
	struct ecore_queue_state_params q_params = { NULL };
	int rc;

	PMD_DRV_LOG(DEBUG, sc, "stopping queue %d cid %d", index, fp->index);

	q_params.q_obj = &sc->sp_objs[fp->index].q_obj;
	/* We want to wait for completion in this context */
	rte_bit_relaxed_set32(RAMROD_COMP_WAIT, &q_params.ramrod_flags);

	/* Stop the primary connection: */

	/* ...halt the connection */
	q_params.cmd = ECORE_Q_CMD_HALT;
	rc = ecore_queue_state_change(sc, &q_params);
	if (rc) {
		return rc;
	}

	/* ...terminate the connection */
	q_params.cmd = ECORE_Q_CMD_TERMINATE;
	memset(&q_params.params.terminate, 0,
	       sizeof(q_params.params.terminate));
	q_params.params.terminate.cid_index = FIRST_TX_COS_INDEX;
	rc = ecore_queue_state_change(sc, &q_params);
	if (rc) {
		return rc;
	}

	/* ...delete cfc entry */
	q_params.cmd = ECORE_Q_CMD_CFC_DEL;
	memset(&q_params.params.cfc_del, 0, sizeof(q_params.params.cfc_del));
	q_params.params.cfc_del.cid_index = FIRST_TX_COS_INDEX;
	return ecore_queue_state_change(sc, &q_params);
}

/* wait for the outstanding SP commands */
static uint8_t bnx2x_wait_sp_comp(struct bnx2x_softc *sc, uint32_t mask)
{
	uint32_t tmp;
	int tout = 5000;	/* wait for 5 secs tops */

	while (tout--) {
		mb();
		if (!(atomic_load_acq_int(&sc->sp_state) & mask))
			return TRUE;

		DELAY(1000);
	}

	mb();

	tmp = atomic_load_acq_int(&sc->sp_state);
	if (tmp & mask) {
		PMD_DRV_LOG(INFO, sc, "Filtering completion timed out: "
			    "sp_state 0x%x, mask 0x%x", tmp, mask);
		return FALSE;
	}

	return FALSE;
}

static int bnx2x_func_stop(struct bnx2x_softc *sc)
{
	struct ecore_func_state_params func_params = { NULL };
	int rc;

	/* prepare parameters for function state transitions */
	rte_bit_relaxed_set32(RAMROD_COMP_WAIT, &func_params.ramrod_flags);
	func_params.f_obj = &sc->func_obj;
	func_params.cmd = ECORE_F_CMD_STOP;

	/*
	 * Try to stop the function the 'good way'. If it fails (in case
	 * of a parity error during bnx2x_chip_cleanup()) and we are
	 * not in a debug mode, perform a state transaction in order to
	 * enable further HW_RESET transaction.
	 */
	rc = ecore_func_state_change(sc, &func_params);
	if (rc) {
		PMD_DRV_LOG(NOTICE, sc, "FUNC_STOP ramrod failed. "
			    "Running a dry transaction");
		rte_bit_relaxed_set32(RAMROD_DRV_CLR_ONLY,
				      &func_params.ramrod_flags);
		return ecore_func_state_change(sc, &func_params);
	}

	return 0;
}

static int bnx2x_reset_hw(struct bnx2x_softc *sc, uint32_t load_code)
{
	struct ecore_func_state_params func_params = { NULL };

	/* Prepare parameters for function state transitions */
	rte_bit_relaxed_set32(RAMROD_COMP_WAIT, &func_params.ramrod_flags);

	func_params.f_obj = &sc->func_obj;
	func_params.cmd = ECORE_F_CMD_HW_RESET;

	func_params.params.hw_init.load_phase = load_code;

	return ecore_func_state_change(sc, &func_params);
}

static void bnx2x_int_disable_sync(struct bnx2x_softc *sc, int disable_hw)
{
	if (disable_hw) {
		/* prevent the HW from sending interrupts */
		bnx2x_int_disable(sc);
	}
}

static void
bnx2x_chip_cleanup(struct bnx2x_softc *sc, uint32_t unload_mode, uint8_t keep_link)
{
	int port = SC_PORT(sc);
	struct ecore_mcast_ramrod_params rparam = { NULL };
	uint32_t reset_code;
	int i, rc = 0;

	bnx2x_drain_tx_queues(sc);

	/* give HW time to discard old tx messages */
	DELAY(1000);

	/* Clean all ETH MACs */
	rc = bnx2x_del_all_macs(sc, &sc->sp_objs[0].mac_obj, ECORE_ETH_MAC,
			      FALSE);
	if (rc < 0) {
		PMD_DRV_LOG(NOTICE, sc,
			    "Failed to delete all ETH MACs (%d)", rc);
	}

	/* Clean up UC list  */
	rc = bnx2x_del_all_macs(sc, &sc->sp_objs[0].mac_obj, ECORE_UC_LIST_MAC,
			      TRUE);
	if (rc < 0) {
		PMD_DRV_LOG(NOTICE, sc,
			    "Failed to delete UC MACs list (%d)", rc);
	}

	/* Disable LLH */
	REG_WR(sc, NIG_REG_LLH0_FUNC_EN + port * 8, 0);

	/* Set "drop all" to stop Rx */

	/*
	 * We need to take the if_maddr_lock() here in order to prevent
	 * a race between the completion code and this code.
	 */

	if (rte_bit_relaxed_get32(ECORE_FILTER_RX_MODE_PENDING, &sc->sp_state))
		rte_bit_relaxed_set32(ECORE_FILTER_RX_MODE_SCHED,
				      &sc->sp_state);
	else
		bnx2x_set_storm_rx_mode(sc);

	/* Clean up multicast configuration */
	rparam.mcast_obj = &sc->mcast_obj;
	rc = ecore_config_mcast(sc, &rparam, ECORE_MCAST_CMD_DEL);
	if (rc < 0) {
		PMD_DRV_LOG(NOTICE, sc,
			    "Failed to send DEL MCAST command (%d)", rc);
	}

	/*
	 * Send the UNLOAD_REQUEST to the MCP. This will return if
	 * this function should perform FUNCTION, PORT, or COMMON HW
	 * reset.
	 */
	reset_code = bnx2x_send_unload_req(sc, unload_mode);

	/*
	 * (assumption: No Attention from MCP at this stage)
	 * PMF probably in the middle of TX disable/enable transaction
	 */
	rc = bnx2x_func_wait_started(sc);
	if (rc) {
		PMD_DRV_LOG(NOTICE, sc, "bnx2x_func_wait_started failed");
	}

	/*
	 * Close multi and leading connections
	 * Completions for ramrods are collected in a synchronous way
	 */
	for (i = 0; i < sc->num_queues; i++) {
		if (bnx2x_stop_queue(sc, i)) {
			goto unload_error;
		}
	}

	/*
	 * If SP settings didn't get completed so far - something
	 * very wrong has happen.
	 */
	if (!bnx2x_wait_sp_comp(sc, ~0x0U))
		PMD_DRV_LOG(NOTICE, sc, "Common slow path ramrods got stuck!");

unload_error:

	rc = bnx2x_func_stop(sc);
	if (rc) {
		PMD_DRV_LOG(NOTICE, sc, "Function stop failed!");
	}

	/* disable HW interrupts */
	bnx2x_int_disable_sync(sc, TRUE);

	/* Reset the chip */
	rc = bnx2x_reset_hw(sc, reset_code);
	if (rc) {
		PMD_DRV_LOG(NOTICE, sc, "Hardware reset failed");
	}

	/* Report UNLOAD_DONE to MCP */
	bnx2x_send_unload_done(sc, keep_link);
}

static void bnx2x_disable_close_the_gate(struct bnx2x_softc *sc)
{
	uint32_t val;

	PMD_DRV_LOG(DEBUG, sc, "Disabling 'close the gates'");

	val = REG_RD(sc, MISC_REG_AEU_GENERAL_MASK);
	val &= ~(MISC_AEU_GENERAL_MASK_REG_AEU_PXP_CLOSE_MASK |
		 MISC_AEU_GENERAL_MASK_REG_AEU_NIG_CLOSE_MASK);
	REG_WR(sc, MISC_REG_AEU_GENERAL_MASK, val);
}

/*
 * Cleans the object that have internal lists without sending
 * ramrods. Should be run when interrutps are disabled.
 */
static void bnx2x_squeeze_objects(struct bnx2x_softc *sc)
{
	uint32_t ramrod_flags = 0, vlan_mac_flags = 0;
	struct ecore_mcast_ramrod_params rparam = { NULL };
	struct ecore_vlan_mac_obj *mac_obj = &sc->sp_objs->mac_obj;
	int rc;

	/* Cleanup MACs' object first... */

	/* Wait for completion of requested */
	rte_bit_relaxed_set32(RAMROD_COMP_WAIT, &ramrod_flags);
	/* Perform a dry cleanup */
	rte_bit_relaxed_set32(RAMROD_DRV_CLR_ONLY, &ramrod_flags);

	/* Clean ETH primary MAC */
	rte_bit_relaxed_set32(ECORE_ETH_MAC, &vlan_mac_flags);
	rc = mac_obj->delete_all(sc, &sc->sp_objs->mac_obj, &vlan_mac_flags,
				 &ramrod_flags);
	if (rc != 0) {
		PMD_DRV_LOG(NOTICE, sc, "Failed to clean ETH MACs (%d)", rc);
	}

	/* Cleanup UC list */
	vlan_mac_flags = 0;
	rte_bit_relaxed_set32(ECORE_UC_LIST_MAC, &vlan_mac_flags);
	rc = mac_obj->delete_all(sc, mac_obj, &vlan_mac_flags, &ramrod_flags);
	if (rc != 0) {
		PMD_DRV_LOG(NOTICE, sc,
			    "Failed to clean UC list MACs (%d)", rc);
	}

	/* Now clean mcast object... */

	rparam.mcast_obj = &sc->mcast_obj;
	rte_bit_relaxed_set32(RAMROD_DRV_CLR_ONLY, &rparam.ramrod_flags);

	/* Add a DEL command... */
	rc = ecore_config_mcast(sc, &rparam, ECORE_MCAST_CMD_DEL);
	if (rc < 0) {
		PMD_DRV_LOG(NOTICE, sc,
			    "Failed to send DEL MCAST command (%d)", rc);
	}

	/* now wait until all pending commands are cleared */

	rc = ecore_config_mcast(sc, &rparam, ECORE_MCAST_CMD_CONT);
	while (rc != 0) {
		if (rc < 0) {
			PMD_DRV_LOG(NOTICE, sc,
				    "Failed to clean MCAST object (%d)", rc);
			return;
		}

		rc = ecore_config_mcast(sc, &rparam, ECORE_MCAST_CMD_CONT);
	}
}

/* stop the controller */
__rte_noinline
int
bnx2x_nic_unload(struct bnx2x_softc *sc, uint32_t unload_mode, uint8_t keep_link)
{
	uint8_t global = FALSE;
	uint32_t val;

	PMD_INIT_FUNC_TRACE(sc);

	PMD_DRV_LOG(DEBUG, sc, "Starting NIC unload...");

	/* mark driver as unloaded in shmem2 */
	if (IS_PF(sc) && SHMEM2_HAS(sc, drv_capabilities_flag)) {
		val = SHMEM2_RD(sc, drv_capabilities_flag[SC_FW_MB_IDX(sc)]);
		SHMEM2_WR(sc, drv_capabilities_flag[SC_FW_MB_IDX(sc)],
			  val & ~DRV_FLAGS_CAPABILITIES_LOADED_L2);
	}

	if (IS_PF(sc) && sc->recovery_state != BNX2X_RECOVERY_DONE &&
	    (sc->state == BNX2X_STATE_CLOSED || sc->state == BNX2X_STATE_ERROR)) {
		/*
		 * We can get here if the driver has been unloaded
		 * during parity error recovery and is either waiting for a
		 * leader to complete or for other functions to unload and
		 * then ifconfig down has been issued. In this case we want to
		 * unload and let other functions to complete a recovery
		 * process.
		 */
		sc->recovery_state = BNX2X_RECOVERY_DONE;
		sc->is_leader = 0;
		bnx2x_release_leader_lock(sc);
		mb();

		PMD_DRV_LOG(NOTICE, sc, "Can't unload in closed or error state");
		return -1;
	}

	/*
	 * Nothing to do during unload if previous bnx2x_nic_load()
	 * did not completed successfully - all resourses are released.
	 */
	if ((sc->state == BNX2X_STATE_CLOSED) || (sc->state == BNX2X_STATE_ERROR)) {
		return 0;
	}

	sc->state = BNX2X_STATE_CLOSING_WAITING_HALT;
	mb();

	sc->rx_mode = BNX2X_RX_MODE_NONE;
	bnx2x_set_rx_mode(sc);
	mb();

	if (IS_PF(sc)) {
		/* set ALWAYS_ALIVE bit in shmem */
		sc->fw_drv_pulse_wr_seq |= DRV_PULSE_ALWAYS_ALIVE;

		bnx2x_drv_pulse(sc);

		bnx2x_stats_handle(sc, STATS_EVENT_STOP);
		bnx2x_save_statistics(sc);
	}

	/* wait till consumers catch up with producers in all queues */
	bnx2x_drain_tx_queues(sc);

	/* if VF indicate to PF this function is going down (PF will delete sp
	 * elements and clear initializations
	 */
	if (IS_VF(sc)) {
		bnx2x_vf_unload(sc);
	} else if (unload_mode != UNLOAD_RECOVERY) {
		/* if this is a normal/close unload need to clean up chip */
		bnx2x_chip_cleanup(sc, unload_mode, keep_link);
	} else {
		/* Send the UNLOAD_REQUEST to the MCP */
		bnx2x_send_unload_req(sc, unload_mode);

		/*
		 * Prevent transactions to host from the functions on the
		 * engine that doesn't reset global blocks in case of global
		 * attention once gloabl blocks are reset and gates are opened
		 * (the engine which leader will perform the recovery
		 * last).
		 */
		if (!CHIP_IS_E1x(sc)) {
			bnx2x_pf_disable(sc);
		}

		/* disable HW interrupts */
		bnx2x_int_disable_sync(sc, TRUE);

		/* Report UNLOAD_DONE to MCP */
		bnx2x_send_unload_done(sc, FALSE);
	}

	/*
	 * At this stage no more interrupts will arrive so we may safely clean
	 * the queue'able objects here in case they failed to get cleaned so far.
	 */
	if (IS_PF(sc)) {
		bnx2x_squeeze_objects(sc);
	}

	/* There should be no more pending SP commands at this stage */
	sc->sp_state = 0;

	sc->port.pmf = 0;

	if (IS_PF(sc)) {
		bnx2x_free_mem(sc);
	}

	/* free the host hardware/software hsi structures */
	bnx2x_free_hsi_mem(sc);

	bnx2x_free_fw_stats_mem(sc);

	sc->state = BNX2X_STATE_CLOSED;

	/*
	 * Check if there are pending parity attentions. If there are - set
	 * RECOVERY_IN_PROGRESS.
	 */
	if (IS_PF(sc) && bnx2x_chk_parity_attn(sc, &global, FALSE)) {
		bnx2x_set_reset_in_progress(sc);

		/* Set RESET_IS_GLOBAL if needed */
		if (global) {
			bnx2x_set_reset_global(sc);
		}
	}

	/*
	 * The last driver must disable a "close the gate" if there is no
	 * parity attention or "process kill" pending.
	 */
	if (IS_PF(sc) && !bnx2x_clear_pf_load(sc) &&
	    bnx2x_reset_is_done(sc, SC_PATH(sc))) {
		bnx2x_disable_close_the_gate(sc);
	}

	PMD_DRV_LOG(DEBUG, sc, "Ended NIC unload");

	return 0;
}

/*
 * Encapsulte an mbuf cluster into the tx bd chain and makes the memory
 * visible to the controller.
 *
 * If an mbuf is submitted to this routine and cannot be given to the
 * controller (e.g. it has too many fragments) then the function may free
 * the mbuf and return to the caller.
 *
 * Returns:
 *     int: Number of TX BDs used for the mbuf
 *
 *   Note the side effect that an mbuf may be freed if it causes a problem.
 */
int bnx2x_tx_encap(struct bnx2x_tx_queue *txq, struct rte_mbuf *m0)
{
	struct eth_tx_start_bd *tx_start_bd;
	uint16_t bd_prod, pkt_prod;
	struct bnx2x_softc *sc;
	uint32_t nbds = 0;

	sc = txq->sc;
	bd_prod = txq->tx_bd_tail;
	pkt_prod = txq->tx_pkt_tail;

	txq->sw_ring[TX_BD(pkt_prod, txq)] = m0;

	tx_start_bd = &txq->tx_ring[TX_BD(bd_prod, txq)].start_bd;

	tx_start_bd->addr_lo =
	    rte_cpu_to_le_32(U64_LO(rte_mbuf_data_iova(m0)));
	tx_start_bd->addr_hi =
	    rte_cpu_to_le_32(U64_HI(rte_mbuf_data_iova(m0)));
	tx_start_bd->nbytes = rte_cpu_to_le_16(m0->data_len);
	tx_start_bd->bd_flags.as_bitfield = ETH_TX_BD_FLAGS_START_BD;
	tx_start_bd->general_data =
	    (1 << ETH_TX_START_BD_HDR_NBDS_SHIFT);

	tx_start_bd->nbd = rte_cpu_to_le_16(2);

	if (m0->ol_flags & PKT_TX_VLAN_PKT) {
		tx_start_bd->vlan_or_ethertype =
		    rte_cpu_to_le_16(m0->vlan_tci);
		tx_start_bd->bd_flags.as_bitfield |=
		    (X_ETH_OUTBAND_VLAN <<
		     ETH_TX_BD_FLAGS_VLAN_MODE_SHIFT);
	} else {
		if (IS_PF(sc))
			tx_start_bd->vlan_or_ethertype =
			    rte_cpu_to_le_16(pkt_prod);
		else {
			/* when transmitting in a vf, start bd
			 * must hold the ethertype for fw to enforce it
			 */
			struct rte_ether_hdr *eh =
			    rte_pktmbuf_mtod(m0, struct rte_ether_hdr *);

			/* Still need to consider inband vlan for enforced */
			if (eh->ether_type ==
					rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN)) {
				struct rte_vlan_hdr *vh =
					(struct rte_vlan_hdr *)(eh + 1);
				tx_start_bd->bd_flags.as_bitfield |=
					(X_ETH_INBAND_VLAN <<
					ETH_TX_BD_FLAGS_VLAN_MODE_SHIFT);
				tx_start_bd->vlan_or_ethertype =
					rte_cpu_to_le_16(ntohs(vh->vlan_tci));
			} else {
				tx_start_bd->vlan_or_ethertype =
					(rte_cpu_to_le_16
					(rte_be_to_cpu_16(eh->ether_type)));
			}
		}
	}

	bd_prod = NEXT_TX_BD(bd_prod);
	if (IS_VF(sc)) {
		struct eth_tx_parse_bd_e2 *tx_parse_bd;
		const struct rte_ether_hdr *eh =
		    rte_pktmbuf_mtod(m0, struct rte_ether_hdr *);
		uint8_t mac_type = UNICAST_ADDRESS;

		tx_parse_bd =
		    &txq->tx_ring[TX_BD(bd_prod, txq)].parse_bd_e2;
		if (rte_is_multicast_ether_addr(&eh->d_addr)) {
			if (rte_is_broadcast_ether_addr(&eh->d_addr))
				mac_type = BROADCAST_ADDRESS;
			else
				mac_type = MULTICAST_ADDRESS;
		}
		tx_parse_bd->parsing_data =
		    (mac_type << ETH_TX_PARSE_BD_E2_ETH_ADDR_TYPE_SHIFT);

		rte_memcpy(&tx_parse_bd->data.mac_addr.dst_hi,
			   &eh->d_addr.addr_bytes[0], 2);
		rte_memcpy(&tx_parse_bd->data.mac_addr.dst_mid,
			   &eh->d_addr.addr_bytes[2], 2);
		rte_memcpy(&tx_parse_bd->data.mac_addr.dst_lo,
			   &eh->d_addr.addr_bytes[4], 2);
		rte_memcpy(&tx_parse_bd->data.mac_addr.src_hi,
			   &eh->s_addr.addr_bytes[0], 2);
		rte_memcpy(&tx_parse_bd->data.mac_addr.src_mid,
			   &eh->s_addr.addr_bytes[2], 2);
		rte_memcpy(&tx_parse_bd->data.mac_addr.src_lo,
			   &eh->s_addr.addr_bytes[4], 2);

		tx_parse_bd->data.mac_addr.dst_hi =
		    rte_cpu_to_be_16(tx_parse_bd->data.mac_addr.dst_hi);
		tx_parse_bd->data.mac_addr.dst_mid =
		    rte_cpu_to_be_16(tx_parse_bd->data.
				     mac_addr.dst_mid);
		tx_parse_bd->data.mac_addr.dst_lo =
		    rte_cpu_to_be_16(tx_parse_bd->data.mac_addr.dst_lo);
		tx_parse_bd->data.mac_addr.src_hi =
		    rte_cpu_to_be_16(tx_parse_bd->data.mac_addr.src_hi);
		tx_parse_bd->data.mac_addr.src_mid =
		    rte_cpu_to_be_16(tx_parse_bd->data.
				     mac_addr.src_mid);
		tx_parse_bd->data.mac_addr.src_lo =
		    rte_cpu_to_be_16(tx_parse_bd->data.mac_addr.src_lo);

		PMD_TX_LOG(DEBUG,
			   "PBD dst %x %x %x src %x %x %x p_data %x",
			   tx_parse_bd->data.mac_addr.dst_hi,
			   tx_parse_bd->data.mac_addr.dst_mid,
			   tx_parse_bd->data.mac_addr.dst_lo,
			   tx_parse_bd->data.mac_addr.src_hi,
			   tx_parse_bd->data.mac_addr.src_mid,
			   tx_parse_bd->data.mac_addr.src_lo,
			   tx_parse_bd->parsing_data);
	}

	PMD_TX_LOG(DEBUG,
		   "start bd: nbytes %d flags %x vlan %x",
		   tx_start_bd->nbytes,
		   tx_start_bd->bd_flags.as_bitfield,
		   tx_start_bd->vlan_or_ethertype);

	bd_prod = NEXT_TX_BD(bd_prod);
	pkt_prod++;

	if (TX_IDX(bd_prod) < 2)
		nbds++;

	txq->nb_tx_avail -= 2;
	txq->tx_bd_tail = bd_prod;
	txq->tx_pkt_tail = pkt_prod;

	return nbds + 2;
}

static uint16_t bnx2x_cid_ilt_lines(struct bnx2x_softc *sc)
{
	return L2_ILT_LINES(sc);
}

static void bnx2x_ilt_set_info(struct bnx2x_softc *sc)
{
	struct ilt_client_info *ilt_client;
	struct ecore_ilt *ilt = sc->ilt;
	uint16_t line = 0;

	PMD_INIT_FUNC_TRACE(sc);

	ilt->start_line = FUNC_ILT_BASE(SC_FUNC(sc));

	/* CDU */
	ilt_client = &ilt->clients[ILT_CLIENT_CDU];
	ilt_client->client_num = ILT_CLIENT_CDU;
	ilt_client->page_size = CDU_ILT_PAGE_SZ;
	ilt_client->flags = ILT_CLIENT_SKIP_MEM;
	ilt_client->start = line;
	line += bnx2x_cid_ilt_lines(sc);

	if (CNIC_SUPPORT(sc)) {
		line += CNIC_ILT_LINES;
	}

	ilt_client->end = (line - 1);

	/* QM */
	if (QM_INIT(sc->qm_cid_count)) {
		ilt_client = &ilt->clients[ILT_CLIENT_QM];
		ilt_client->client_num = ILT_CLIENT_QM;
		ilt_client->page_size = QM_ILT_PAGE_SZ;
		ilt_client->flags = 0;
		ilt_client->start = line;

		/* 4 bytes for each cid */
		line += DIV_ROUND_UP(sc->qm_cid_count * QM_QUEUES_PER_FUNC * 4,
				     QM_ILT_PAGE_SZ);

		ilt_client->end = (line - 1);
	}

	if (CNIC_SUPPORT(sc)) {
		/* SRC */
		ilt_client = &ilt->clients[ILT_CLIENT_SRC];
		ilt_client->client_num = ILT_CLIENT_SRC;
		ilt_client->page_size = SRC_ILT_PAGE_SZ;
		ilt_client->flags = 0;
		ilt_client->start = line;
		line += SRC_ILT_LINES;
		ilt_client->end = (line - 1);

		/* TM */
		ilt_client = &ilt->clients[ILT_CLIENT_TM];
		ilt_client->client_num = ILT_CLIENT_TM;
		ilt_client->page_size = TM_ILT_PAGE_SZ;
		ilt_client->flags = 0;
		ilt_client->start = line;
		line += TM_ILT_LINES;
		ilt_client->end = (line - 1);
	}

	assert((line <= ILT_MAX_LINES));
}

static void bnx2x_set_fp_rx_buf_size(struct bnx2x_softc *sc)
{
	int i;

	for (i = 0; i < sc->num_queues; i++) {
		/* get the Rx buffer size for RX frames */
		sc->fp[i].rx_buf_size =
		    (IP_HEADER_ALIGNMENT_PADDING + ETH_OVERHEAD + sc->mtu);
	}
}

int bnx2x_alloc_ilt_mem(struct bnx2x_softc *sc)
{

	sc->ilt = rte_malloc("", sizeof(struct ecore_ilt), RTE_CACHE_LINE_SIZE);

	return sc->ilt == NULL;
}

static int bnx2x_alloc_ilt_lines_mem(struct bnx2x_softc *sc)
{
	sc->ilt->lines = rte_calloc("",
				    sizeof(struct ilt_line), ILT_MAX_LINES,
				    RTE_CACHE_LINE_SIZE);
	return sc->ilt->lines == NULL;
}

void bnx2x_free_ilt_mem(struct bnx2x_softc *sc)
{
	rte_free(sc->ilt);
	sc->ilt = NULL;
}

static void bnx2x_free_ilt_lines_mem(struct bnx2x_softc *sc)
{
	if (sc->ilt->lines != NULL) {
		rte_free(sc->ilt->lines);
		sc->ilt->lines = NULL;
	}
}

static void bnx2x_free_mem(struct bnx2x_softc *sc)
{
	uint32_t i;

	for (i = 0; i < L2_ILT_LINES(sc); i++) {
		sc->context[i].vcxt = NULL;
		sc->context[i].size = 0;
	}

	ecore_ilt_mem_op(sc, ILT_MEMOP_FREE);

	bnx2x_free_ilt_lines_mem(sc);
}

static int bnx2x_alloc_mem(struct bnx2x_softc *sc)
{
	int context_size;
	int allocated;
	int i;
	char cdu_name[RTE_MEMZONE_NAMESIZE];

	/*
	 * Allocate memory for CDU context:
	 * This memory is allocated separately and not in the generic ILT
	 * functions because CDU differs in few aspects:
	 * 1. There can be multiple entities allocating memory for context -
	 * regular L2, CNIC, and SRIOV drivers. Each separately controls
	 * its own ILT lines.
	 * 2. Since CDU page-size is not a single 4KB page (which is the case
	 * for the other ILT clients), to be efficient we want to support
	 * allocation of sub-page-size in the last entry.
	 * 3. Context pointers are used by the driver to pass to FW / update
	 * the context (for the other ILT clients the pointers are used just to
	 * free the memory during unload).
	 */
	context_size = (sizeof(union cdu_context) * BNX2X_L2_CID_COUNT(sc));
	for (i = 0, allocated = 0; allocated < context_size; i++) {
		sc->context[i].size = min(CDU_ILT_PAGE_SZ,
					  (context_size - allocated));

		snprintf(cdu_name, sizeof(cdu_name), "cdu_%d", i);
		if (bnx2x_dma_alloc(sc, sc->context[i].size,
				  &sc->context[i].vcxt_dma,
				  cdu_name, BNX2X_PAGE_SIZE) != 0) {
			bnx2x_free_mem(sc);
			return -1;
		}

		sc->context[i].vcxt =
		    (union cdu_context *)sc->context[i].vcxt_dma.vaddr;

		allocated += sc->context[i].size;
	}

	bnx2x_alloc_ilt_lines_mem(sc);

	if (ecore_ilt_mem_op(sc, ILT_MEMOP_ALLOC)) {
		PMD_DRV_LOG(NOTICE, sc, "ecore_ilt_mem_op ILT_MEMOP_ALLOC failed");
		bnx2x_free_mem(sc);
		return -1;
	}

	return 0;
}

static void bnx2x_free_fw_stats_mem(struct bnx2x_softc *sc)
{
	bnx2x_dma_free(&sc->fw_stats_dma);
	sc->fw_stats_num = 0;

	sc->fw_stats_req_size = 0;
	sc->fw_stats_req = NULL;
	sc->fw_stats_req_mapping = 0;

	sc->fw_stats_data_size = 0;
	sc->fw_stats_data = NULL;
	sc->fw_stats_data_mapping = 0;
}

static int bnx2x_alloc_fw_stats_mem(struct bnx2x_softc *sc)
{
	uint8_t num_queue_stats;
	int num_groups, vf_headroom = 0;

	/* number of queues for statistics is number of eth queues */
	num_queue_stats = BNX2X_NUM_ETH_QUEUES(sc);

	/*
	 * Total number of FW statistics requests =
	 *   1 for port stats + 1 for PF stats + num of queues
	 */
	sc->fw_stats_num = (2 + num_queue_stats);

	/*
	 * Request is built from stats_query_header and an array of
	 * stats_query_cmd_group each of which contains STATS_QUERY_CMD_COUNT
	 * rules. The real number or requests is configured in the
	 * stats_query_header.
	 */
	num_groups = (sc->fw_stats_num + vf_headroom) / STATS_QUERY_CMD_COUNT;
	if ((sc->fw_stats_num + vf_headroom) % STATS_QUERY_CMD_COUNT)
		num_groups++;

	sc->fw_stats_req_size =
	    (sizeof(struct stats_query_header) +
	     (num_groups * sizeof(struct stats_query_cmd_group)));

	/*
	 * Data for statistics requests + stats_counter.
	 * stats_counter holds per-STORM counters that are incremented when
	 * STORM has finished with the current request. Memory for FCoE
	 * offloaded statistics are counted anyway, even if they will not be sent.
	 * VF stats are not accounted for here as the data of VF stats is stored
	 * in memory allocated by the VF, not here.
	 */
	sc->fw_stats_data_size =
	    (sizeof(struct stats_counter) +
	     sizeof(struct per_port_stats) + sizeof(struct per_pf_stats) +
	     /* sizeof(struct fcoe_statistics_params) + */
	     (sizeof(struct per_queue_stats) * num_queue_stats));

	if (bnx2x_dma_alloc(sc, (sc->fw_stats_req_size + sc->fw_stats_data_size),
			  &sc->fw_stats_dma, "fw_stats",
			  RTE_CACHE_LINE_SIZE) != 0) {
		bnx2x_free_fw_stats_mem(sc);
		return -1;
	}

	/* set up the shortcuts */

	sc->fw_stats_req = (struct bnx2x_fw_stats_req *)sc->fw_stats_dma.vaddr;
	sc->fw_stats_req_mapping = sc->fw_stats_dma.paddr;

	sc->fw_stats_data =
	    (struct bnx2x_fw_stats_data *)((uint8_t *) sc->fw_stats_dma.vaddr +
					 sc->fw_stats_req_size);
	sc->fw_stats_data_mapping = (sc->fw_stats_dma.paddr +
				     sc->fw_stats_req_size);

	return 0;
}

/*
 * Bits map:
 * 0-7  - Engine0 load counter.
 * 8-15 - Engine1 load counter.
 * 16   - Engine0 RESET_IN_PROGRESS bit.
 * 17   - Engine1 RESET_IN_PROGRESS bit.
 * 18   - Engine0 ONE_IS_LOADED. Set when there is at least one active
 *        function on the engine
 * 19   - Engine1 ONE_IS_LOADED.
 * 20   - Chip reset flow bit. When set none-leader must wait for both engines
 *        leader to complete (check for both RESET_IN_PROGRESS bits and not
 *        for just the one belonging to its engine).
 */
#define BNX2X_RECOVERY_GLOB_REG     MISC_REG_GENERIC_POR_1
#define BNX2X_PATH0_LOAD_CNT_MASK   0x000000ff
#define BNX2X_PATH0_LOAD_CNT_SHIFT  0
#define BNX2X_PATH1_LOAD_CNT_MASK   0x0000ff00
#define BNX2X_PATH1_LOAD_CNT_SHIFT  8
#define BNX2X_PATH0_RST_IN_PROG_BIT 0x00010000
#define BNX2X_PATH1_RST_IN_PROG_BIT 0x00020000
#define BNX2X_GLOBAL_RESET_BIT      0x00040000

/* set the GLOBAL_RESET bit, should be run under rtnl lock */
static void bnx2x_set_reset_global(struct bnx2x_softc *sc)
{
	uint32_t val;
	bnx2x_acquire_hw_lock(sc, HW_LOCK_RESOURCE_RECOVERY_REG);
	val = REG_RD(sc, BNX2X_RECOVERY_GLOB_REG);
	REG_WR(sc, BNX2X_RECOVERY_GLOB_REG, val | BNX2X_GLOBAL_RESET_BIT);
	bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_RECOVERY_REG);
}

/* clear the GLOBAL_RESET bit, should be run under rtnl lock */
static void bnx2x_clear_reset_global(struct bnx2x_softc *sc)
{
	uint32_t val;
	bnx2x_acquire_hw_lock(sc, HW_LOCK_RESOURCE_RECOVERY_REG);
	val = REG_RD(sc, BNX2X_RECOVERY_GLOB_REG);
	REG_WR(sc, BNX2X_RECOVERY_GLOB_REG, val & (~BNX2X_GLOBAL_RESET_BIT));
	bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_RECOVERY_REG);
}

/* checks the GLOBAL_RESET bit, should be run under rtnl lock */
static uint8_t bnx2x_reset_is_global(struct bnx2x_softc *sc)
{
	return REG_RD(sc, BNX2X_RECOVERY_GLOB_REG) & BNX2X_GLOBAL_RESET_BIT;
}

/* clear RESET_IN_PROGRESS bit for the engine, should be run under rtnl lock */
static void bnx2x_set_reset_done(struct bnx2x_softc *sc)
{
	uint32_t val;
	uint32_t bit = SC_PATH(sc) ? BNX2X_PATH1_RST_IN_PROG_BIT :
	    BNX2X_PATH0_RST_IN_PROG_BIT;

	bnx2x_acquire_hw_lock(sc, HW_LOCK_RESOURCE_RECOVERY_REG);

	val = REG_RD(sc, BNX2X_RECOVERY_GLOB_REG);
	/* Clear the bit */
	val &= ~bit;
	REG_WR(sc, BNX2X_RECOVERY_GLOB_REG, val);

	bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_RECOVERY_REG);
}

/* set RESET_IN_PROGRESS for the engine, should be run under rtnl lock */
static void bnx2x_set_reset_in_progress(struct bnx2x_softc *sc)
{
	uint32_t val;
	uint32_t bit = SC_PATH(sc) ? BNX2X_PATH1_RST_IN_PROG_BIT :
	    BNX2X_PATH0_RST_IN_PROG_BIT;

	bnx2x_acquire_hw_lock(sc, HW_LOCK_RESOURCE_RECOVERY_REG);

	val = REG_RD(sc, BNX2X_RECOVERY_GLOB_REG);
	/* Set the bit */
	val |= bit;
	REG_WR(sc, BNX2X_RECOVERY_GLOB_REG, val);

	bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_RECOVERY_REG);
}

/* check RESET_IN_PROGRESS bit for an engine, should be run under rtnl lock */
static uint8_t bnx2x_reset_is_done(struct bnx2x_softc *sc, int engine)
{
	uint32_t val = REG_RD(sc, BNX2X_RECOVERY_GLOB_REG);
	uint32_t bit = engine ? BNX2X_PATH1_RST_IN_PROG_BIT :
	    BNX2X_PATH0_RST_IN_PROG_BIT;

	/* return false if bit is set */
	return (val & bit) ? FALSE : TRUE;
}

/* get the load status for an engine, should be run under rtnl lock */
static uint8_t bnx2x_get_load_status(struct bnx2x_softc *sc, int engine)
{
	uint32_t mask = engine ? BNX2X_PATH1_LOAD_CNT_MASK :
	    BNX2X_PATH0_LOAD_CNT_MASK;
	uint32_t shift = engine ? BNX2X_PATH1_LOAD_CNT_SHIFT :
	    BNX2X_PATH0_LOAD_CNT_SHIFT;
	uint32_t val = REG_RD(sc, BNX2X_RECOVERY_GLOB_REG);

	val = ((val & mask) >> shift);

	return val != 0;
}

/* set pf load mark */
static void bnx2x_set_pf_load(struct bnx2x_softc *sc)
{
	uint32_t val;
	uint32_t val1;
	uint32_t mask = SC_PATH(sc) ? BNX2X_PATH1_LOAD_CNT_MASK :
	    BNX2X_PATH0_LOAD_CNT_MASK;
	uint32_t shift = SC_PATH(sc) ? BNX2X_PATH1_LOAD_CNT_SHIFT :
	    BNX2X_PATH0_LOAD_CNT_SHIFT;

	bnx2x_acquire_hw_lock(sc, HW_LOCK_RESOURCE_RECOVERY_REG);

	PMD_INIT_FUNC_TRACE(sc);

	val = REG_RD(sc, BNX2X_RECOVERY_GLOB_REG);

	/* get the current counter value */
	val1 = ((val & mask) >> shift);

	/* set bit of this PF */
	val1 |= (1 << SC_ABS_FUNC(sc));

	/* clear the old value */
	val &= ~mask;

	/* set the new one */
	val |= ((val1 << shift) & mask);

	REG_WR(sc, BNX2X_RECOVERY_GLOB_REG, val);

	bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_RECOVERY_REG);
}

/* clear pf load mark */
static uint8_t bnx2x_clear_pf_load(struct bnx2x_softc *sc)
{
	uint32_t val1, val;
	uint32_t mask = SC_PATH(sc) ? BNX2X_PATH1_LOAD_CNT_MASK :
	    BNX2X_PATH0_LOAD_CNT_MASK;
	uint32_t shift = SC_PATH(sc) ? BNX2X_PATH1_LOAD_CNT_SHIFT :
	    BNX2X_PATH0_LOAD_CNT_SHIFT;

	bnx2x_acquire_hw_lock(sc, HW_LOCK_RESOURCE_RECOVERY_REG);
	val = REG_RD(sc, BNX2X_RECOVERY_GLOB_REG);

	/* get the current counter value */
	val1 = (val & mask) >> shift;

	/* clear bit of that PF */
	val1 &= ~(1 << SC_ABS_FUNC(sc));

	/* clear the old value */
	val &= ~mask;

	/* set the new one */
	val |= ((val1 << shift) & mask);

	REG_WR(sc, BNX2X_RECOVERY_GLOB_REG, val);
	bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_RECOVERY_REG);
	return val1 != 0;
}

/* send load requrest to mcp and analyze response */
static int bnx2x_nic_load_request(struct bnx2x_softc *sc, uint32_t * load_code)
{
	PMD_INIT_FUNC_TRACE(sc);

	/* init fw_seq */
	sc->fw_seq =
	    (SHMEM_RD(sc, func_mb[SC_FW_MB_IDX(sc)].drv_mb_header) &
	     DRV_MSG_SEQ_NUMBER_MASK);

	PMD_DRV_LOG(DEBUG, sc, "initial fw_seq 0x%04x", sc->fw_seq);

#ifdef BNX2X_PULSE
	/* get the current FW pulse sequence */
	sc->fw_drv_pulse_wr_seq =
	    (SHMEM_RD(sc, func_mb[SC_FW_MB_IDX(sc)].drv_pulse_mb) &
	     DRV_PULSE_SEQ_MASK);
#else
	/* set ALWAYS_ALIVE bit in shmem */
	sc->fw_drv_pulse_wr_seq |= DRV_PULSE_ALWAYS_ALIVE;
	bnx2x_drv_pulse(sc);
#endif

	/* load request */
	(*load_code) = bnx2x_fw_command(sc, DRV_MSG_CODE_LOAD_REQ,
				      DRV_MSG_CODE_LOAD_REQ_WITH_LFA);

	/* if the MCP fails to respond we must abort */
	if (!(*load_code)) {
		PMD_DRV_LOG(NOTICE, sc, "MCP response failure!");
		return -1;
	}

	/* if MCP refused then must abort */
	if ((*load_code) == FW_MSG_CODE_DRV_LOAD_REFUSED) {
		PMD_DRV_LOG(NOTICE, sc, "MCP refused load request");
		return -1;
	}

	return 0;
}

/*
 * Check whether another PF has already loaded FW to chip. In virtualized
 * environments a pf from anoth VM may have already initialized the device
 * including loading FW.
 */
static int bnx2x_nic_load_analyze_req(struct bnx2x_softc *sc, uint32_t load_code)
{
	uint32_t my_fw, loaded_fw;

	/* is another pf loaded on this engine? */
	if ((load_code != FW_MSG_CODE_DRV_LOAD_COMMON_CHIP) &&
	    (load_code != FW_MSG_CODE_DRV_LOAD_COMMON)) {
		/* build my FW version dword */
		my_fw = (BNX2X_5710_FW_MAJOR_VERSION +
			 (BNX2X_5710_FW_MINOR_VERSION << 8) +
			 (BNX2X_5710_FW_REVISION_VERSION << 16) +
			 (BNX2X_5710_FW_ENGINEERING_VERSION << 24));

		/* read loaded FW from chip */
		loaded_fw = REG_RD(sc, XSEM_REG_PRAM);
		PMD_DRV_LOG(DEBUG, sc, "loaded FW 0x%08x / my FW 0x%08x",
			    loaded_fw, my_fw);

		/* abort nic load if version mismatch */
		if (my_fw != loaded_fw) {
			PMD_DRV_LOG(NOTICE, sc,
				    "FW 0x%08x already loaded (mine is 0x%08x)",
				    loaded_fw, my_fw);
			return -1;
		}
	}

	return 0;
}

/* mark PMF if applicable */
static void bnx2x_nic_load_pmf(struct bnx2x_softc *sc, uint32_t load_code)
{
	uint32_t ncsi_oem_data_addr;

	PMD_INIT_FUNC_TRACE(sc);

	if ((load_code == FW_MSG_CODE_DRV_LOAD_COMMON) ||
	    (load_code == FW_MSG_CODE_DRV_LOAD_COMMON_CHIP) ||
	    (load_code == FW_MSG_CODE_DRV_LOAD_PORT)) {
		/*
		 * Barrier here for ordering between the writing to sc->port.pmf here
		 * and reading it from the periodic task.
		 */
		sc->port.pmf = 1;
		mb();
	} else {
		sc->port.pmf = 0;
	}

	PMD_DRV_LOG(DEBUG, sc, "pmf %d", sc->port.pmf);

	if (load_code == FW_MSG_CODE_DRV_LOAD_COMMON_CHIP) {
		if (SHMEM2_HAS(sc, ncsi_oem_data_addr)) {
			ncsi_oem_data_addr = SHMEM2_RD(sc, ncsi_oem_data_addr);
			if (ncsi_oem_data_addr) {
				REG_WR(sc,
				       (ncsi_oem_data_addr +
					offsetof(struct glob_ncsi_oem_data,
						 driver_version)), 0);
			}
		}
	}
}

static void bnx2x_read_mf_cfg(struct bnx2x_softc *sc)
{
	int n = (CHIP_IS_MODE_4_PORT(sc) ? 2 : 1);
	int abs_func;
	int vn;

	if (BNX2X_NOMCP(sc)) {
		return;		/* what should be the default bvalue in this case */
	}

	/*
	 * The formula for computing the absolute function number is...
	 * For 2 port configuration (4 functions per port):
	 *   abs_func = 2 * vn + SC_PORT + SC_PATH
	 * For 4 port configuration (2 functions per port):
	 *   abs_func = 4 * vn + 2 * SC_PORT + SC_PATH
	 */
	for (vn = VN_0; vn < SC_MAX_VN_NUM(sc); vn++) {
		abs_func = (n * (2 * vn + SC_PORT(sc)) + SC_PATH(sc));
		if (abs_func >= E1H_FUNC_MAX) {
			break;
		}
		sc->devinfo.mf_info.mf_config[vn] =
		    MFCFG_RD(sc, func_mf_config[abs_func].config);
	}

	if (sc->devinfo.mf_info.mf_config[SC_VN(sc)] &
	    FUNC_MF_CFG_FUNC_DISABLED) {
		PMD_DRV_LOG(DEBUG, sc, "mf_cfg function disabled");
		sc->flags |= BNX2X_MF_FUNC_DIS;
	} else {
		PMD_DRV_LOG(DEBUG, sc, "mf_cfg function enabled");
		sc->flags &= ~BNX2X_MF_FUNC_DIS;
	}
}

/* acquire split MCP access lock register */
static int bnx2x_acquire_alr(struct bnx2x_softc *sc)
{
	uint32_t j, val;

	for (j = 0; j < 1000; j++) {
		val = (1UL << 31);
		REG_WR(sc, GRCBASE_MCP + 0x9c, val);
		val = REG_RD(sc, GRCBASE_MCP + 0x9c);
		if (val & (1L << 31))
			break;

		DELAY(5000);
	}

	if (!(val & (1L << 31))) {
		PMD_DRV_LOG(NOTICE, sc, "Cannot acquire MCP access lock register");
		return -1;
	}

	return 0;
}

/* release split MCP access lock register */
static void bnx2x_release_alr(struct bnx2x_softc *sc)
{
	REG_WR(sc, GRCBASE_MCP + 0x9c, 0);
}

static void bnx2x_fan_failure(struct bnx2x_softc *sc)
{
	int port = SC_PORT(sc);
	uint32_t ext_phy_config;

	/* mark the failure */
	ext_phy_config =
	    SHMEM_RD(sc, dev_info.port_hw_config[port].external_phy_config);

	ext_phy_config &= ~PORT_HW_CFG_XGXS_EXT_PHY_TYPE_MASK;
	ext_phy_config |= PORT_HW_CFG_XGXS_EXT_PHY_TYPE_FAILURE;
	SHMEM_WR(sc, dev_info.port_hw_config[port].external_phy_config,
		 ext_phy_config);

	/* log the failure */
	PMD_DRV_LOG(INFO, sc,
		    "Fan Failure has caused the driver to shutdown "
		    "the card to prevent permanent damage. "
		    "Please contact OEM Support for assistance");

	rte_panic("Schedule task to handle fan failure");
}

/* this function is called upon a link interrupt */
static void bnx2x_link_attn(struct bnx2x_softc *sc)
{
	uint32_t pause_enabled = 0;
	struct host_port_stats *pstats;
	int cmng_fns;

	/* Make sure that we are synced with the current statistics */
	bnx2x_stats_handle(sc, STATS_EVENT_STOP);

	elink_link_update(&sc->link_params, &sc->link_vars);

	if (sc->link_vars.link_up) {

		/* dropless flow control */
		if (sc->dropless_fc) {
			pause_enabled = 0;

			if (sc->link_vars.flow_ctrl & ELINK_FLOW_CTRL_TX) {
				pause_enabled = 1;
			}

			REG_WR(sc,
			       (BAR_USTRORM_INTMEM +
				USTORM_ETH_PAUSE_ENABLED_OFFSET(SC_PORT(sc))),
			       pause_enabled);
		}

		if (sc->link_vars.mac_type != ELINK_MAC_TYPE_EMAC) {
			pstats = BNX2X_SP(sc, port_stats);
			/* reset old mac stats */
			memset(&(pstats->mac_stx[0]), 0,
			       sizeof(struct mac_stx));
		}

		if (sc->state == BNX2X_STATE_OPEN) {
			bnx2x_stats_handle(sc, STATS_EVENT_LINK_UP);
		}
	}

	if (sc->link_vars.link_up && sc->link_vars.line_speed) {
		cmng_fns = bnx2x_get_cmng_fns_mode(sc);

		if (cmng_fns != CMNG_FNS_NONE) {
			bnx2x_cmng_fns_init(sc, FALSE, cmng_fns);
			storm_memset_cmng(sc, &sc->cmng, SC_PORT(sc));
		}
	}

	bnx2x_link_report_locked(sc);

	if (IS_MF(sc)) {
		bnx2x_link_sync_notify(sc);
	}
}

static void bnx2x_attn_int_asserted(struct bnx2x_softc *sc, uint32_t asserted)
{
	int port = SC_PORT(sc);
	uint32_t aeu_addr = port ? MISC_REG_AEU_MASK_ATTN_FUNC_1 :
	    MISC_REG_AEU_MASK_ATTN_FUNC_0;
	uint32_t nig_int_mask_addr = port ? NIG_REG_MASK_INTERRUPT_PORT1 :
	    NIG_REG_MASK_INTERRUPT_PORT0;
	uint32_t aeu_mask;
	uint32_t nig_mask = 0;
	uint32_t reg_addr;
	uint32_t igu_acked;
	uint32_t cnt;

	if (sc->attn_state & asserted) {
		PMD_DRV_LOG(ERR, sc, "IGU ERROR attn=0x%08x", asserted);
	}

	bnx2x_acquire_hw_lock(sc, HW_LOCK_RESOURCE_PORT0_ATT_MASK + port);

	aeu_mask = REG_RD(sc, aeu_addr);

	aeu_mask &= ~(asserted & 0x3ff);

	REG_WR(sc, aeu_addr, aeu_mask);

	bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_PORT0_ATT_MASK + port);

	sc->attn_state |= asserted;

	if (asserted & ATTN_HARD_WIRED_MASK) {
		if (asserted & ATTN_NIG_FOR_FUNC) {

			bnx2x_acquire_phy_lock(sc);
			/* save nig interrupt mask */
			nig_mask = REG_RD(sc, nig_int_mask_addr);

			/* If nig_mask is not set, no need to call the update function */
			if (nig_mask) {
				REG_WR(sc, nig_int_mask_addr, 0);

				bnx2x_link_attn(sc);
			}

			/* handle unicore attn? */
		}

		if (asserted & ATTN_SW_TIMER_4_FUNC) {
			PMD_DRV_LOG(DEBUG, sc, "ATTN_SW_TIMER_4_FUNC!");
		}

		if (asserted & GPIO_2_FUNC) {
			PMD_DRV_LOG(DEBUG, sc, "GPIO_2_FUNC!");
		}

		if (asserted & GPIO_3_FUNC) {
			PMD_DRV_LOG(DEBUG, sc, "GPIO_3_FUNC!");
		}

		if (asserted & GPIO_4_FUNC) {
			PMD_DRV_LOG(DEBUG, sc, "GPIO_4_FUNC!");
		}

		if (port == 0) {
			if (asserted & ATTN_GENERAL_ATTN_1) {
				PMD_DRV_LOG(DEBUG, sc, "ATTN_GENERAL_ATTN_1!");
				REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_1, 0x0);
			}
			if (asserted & ATTN_GENERAL_ATTN_2) {
				PMD_DRV_LOG(DEBUG, sc, "ATTN_GENERAL_ATTN_2!");
				REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_2, 0x0);
			}
			if (asserted & ATTN_GENERAL_ATTN_3) {
				PMD_DRV_LOG(DEBUG, sc, "ATTN_GENERAL_ATTN_3!");
				REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_3, 0x0);
			}
		} else {
			if (asserted & ATTN_GENERAL_ATTN_4) {
				PMD_DRV_LOG(DEBUG, sc, "ATTN_GENERAL_ATTN_4!");
				REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_4, 0x0);
			}
			if (asserted & ATTN_GENERAL_ATTN_5) {
				PMD_DRV_LOG(DEBUG, sc, "ATTN_GENERAL_ATTN_5!");
				REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_5, 0x0);
			}
			if (asserted & ATTN_GENERAL_ATTN_6) {
				PMD_DRV_LOG(DEBUG, sc, "ATTN_GENERAL_ATTN_6!");
				REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_6, 0x0);
			}
		}
	}
	/* hardwired */
	if (sc->devinfo.int_block == INT_BLOCK_HC) {
		reg_addr =
		    (HC_REG_COMMAND_REG + port * 32 +
		     COMMAND_REG_ATTN_BITS_SET);
	} else {
		reg_addr = (BAR_IGU_INTMEM + IGU_CMD_ATTN_BIT_SET_UPPER * 8);
	}

	PMD_DRV_LOG(DEBUG, sc, "about to mask 0x%08x at %s addr 0x%08x",
		    asserted,
		    (sc->devinfo.int_block == INT_BLOCK_HC) ? "HC" : "IGU",
		    reg_addr);
	REG_WR(sc, reg_addr, asserted);

	/* now set back the mask */
	if (asserted & ATTN_NIG_FOR_FUNC) {
		/*
		 * Verify that IGU ack through BAR was written before restoring
		 * NIG mask. This loop should exit after 2-3 iterations max.
		 */
		if (sc->devinfo.int_block != INT_BLOCK_HC) {
			cnt = 0;

			do {
				igu_acked =
				    REG_RD(sc, IGU_REG_ATTENTION_ACK_BITS);
			} while (((igu_acked & ATTN_NIG_FOR_FUNC) == 0)
				 && (++cnt < MAX_IGU_ATTN_ACK_TO));

			if (!igu_acked) {
				PMD_DRV_LOG(ERR, sc,
					    "Failed to verify IGU ack on time");
			}

			mb();
		}

		REG_WR(sc, nig_int_mask_addr, nig_mask);

		bnx2x_release_phy_lock(sc);
	}
}

static void
bnx2x_print_next_block(__rte_unused struct bnx2x_softc *sc, __rte_unused int idx,
		     __rte_unused const char *blk)
{
	PMD_DRV_LOG(INFO, sc, "%s%s", idx ? ", " : "", blk);
}

static int
bnx2x_check_blocks_with_parity0(struct bnx2x_softc *sc, uint32_t sig, int par_num,
			      uint8_t print)
{
	uint32_t cur_bit = 0;
	int i = 0;

	for (i = 0; sig; i++) {
		cur_bit = ((uint32_t) 0x1 << i);
		if (sig & cur_bit) {
			switch (cur_bit) {
			case AEU_INPUTS_ATTN_BITS_BRB_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "BRB");
				break;
			case AEU_INPUTS_ATTN_BITS_PARSER_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "PARSER");
				break;
			case AEU_INPUTS_ATTN_BITS_TSDM_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "TSDM");
				break;
			case AEU_INPUTS_ATTN_BITS_SEARCHER_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "SEARCHER");
				break;
			case AEU_INPUTS_ATTN_BITS_TCM_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "TCM");
				break;
			case AEU_INPUTS_ATTN_BITS_TSEMI_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "TSEMI");
				break;
			case AEU_INPUTS_ATTN_BITS_PBCLIENT_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "XPB");
				break;
			}

			/* Clear the bit */
			sig &= ~cur_bit;
		}
	}

	return par_num;
}

static int
bnx2x_check_blocks_with_parity1(struct bnx2x_softc *sc, uint32_t sig, int par_num,
			      uint8_t * global, uint8_t print)
{
	int i = 0;
	uint32_t cur_bit = 0;
	for (i = 0; sig; i++) {
		cur_bit = ((uint32_t) 0x1 << i);
		if (sig & cur_bit) {
			switch (cur_bit) {
			case AEU_INPUTS_ATTN_BITS_PBF_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "PBF");
				break;
			case AEU_INPUTS_ATTN_BITS_QM_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "QM");
				break;
			case AEU_INPUTS_ATTN_BITS_TIMERS_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "TM");
				break;
			case AEU_INPUTS_ATTN_BITS_XSDM_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "XSDM");
				break;
			case AEU_INPUTS_ATTN_BITS_XCM_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "XCM");
				break;
			case AEU_INPUTS_ATTN_BITS_XSEMI_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "XSEMI");
				break;
			case AEU_INPUTS_ATTN_BITS_DOORBELLQ_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "DOORBELLQ");
				break;
			case AEU_INPUTS_ATTN_BITS_NIG_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "NIG");
				break;
			case AEU_INPUTS_ATTN_BITS_VAUX_PCI_CORE_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "VAUX PCI CORE");
				*global = TRUE;
				break;
			case AEU_INPUTS_ATTN_BITS_DEBUG_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "DEBUG");
				break;
			case AEU_INPUTS_ATTN_BITS_USDM_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "USDM");
				break;
			case AEU_INPUTS_ATTN_BITS_UCM_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "UCM");
				break;
			case AEU_INPUTS_ATTN_BITS_USEMI_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "USEMI");
				break;
			case AEU_INPUTS_ATTN_BITS_UPB_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "UPB");
				break;
			case AEU_INPUTS_ATTN_BITS_CSDM_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "CSDM");
				break;
			case AEU_INPUTS_ATTN_BITS_CCM_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "CCM");
				break;
			}

			/* Clear the bit */
			sig &= ~cur_bit;
		}
	}

	return par_num;
}

static int
bnx2x_check_blocks_with_parity2(struct bnx2x_softc *sc, uint32_t sig, int par_num,
			      uint8_t print)
{
	uint32_t cur_bit = 0;
	int i = 0;

	for (i = 0; sig; i++) {
		cur_bit = ((uint32_t) 0x1 << i);
		if (sig & cur_bit) {
			switch (cur_bit) {
			case AEU_INPUTS_ATTN_BITS_CSEMI_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "CSEMI");
				break;
			case AEU_INPUTS_ATTN_BITS_PXP_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "PXP");
				break;
			case AEU_IN_ATTN_BITS_PXPPCICLOCKCLIENT_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "PXPPCICLOCKCLIENT");
				break;
			case AEU_INPUTS_ATTN_BITS_CFC_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "CFC");
				break;
			case AEU_INPUTS_ATTN_BITS_CDU_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "CDU");
				break;
			case AEU_INPUTS_ATTN_BITS_DMAE_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "DMAE");
				break;
			case AEU_INPUTS_ATTN_BITS_IGU_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "IGU");
				break;
			case AEU_INPUTS_ATTN_BITS_MISC_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "MISC");
				break;
			}

			/* Clear the bit */
			sig &= ~cur_bit;
		}
	}

	return par_num;
}

static int
bnx2x_check_blocks_with_parity3(struct bnx2x_softc *sc, uint32_t sig, int par_num,
			      uint8_t * global, uint8_t print)
{
	uint32_t cur_bit = 0;
	int i = 0;

	for (i = 0; sig; i++) {
		cur_bit = ((uint32_t) 0x1 << i);
		if (sig & cur_bit) {
			switch (cur_bit) {
			case AEU_INPUTS_ATTN_BITS_MCP_LATCHED_ROM_PARITY:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "MCP ROM");
				*global = TRUE;
				break;
			case AEU_INPUTS_ATTN_BITS_MCP_LATCHED_UMP_RX_PARITY:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "MCP UMP RX");
				*global = TRUE;
				break;
			case AEU_INPUTS_ATTN_BITS_MCP_LATCHED_UMP_TX_PARITY:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "MCP UMP TX");
				*global = TRUE;
				break;
			case AEU_INPUTS_ATTN_BITS_MCP_LATCHED_SCPAD_PARITY:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "MCP SCPAD");
				*global = TRUE;
				break;
			}

			/* Clear the bit */
			sig &= ~cur_bit;
		}
	}

	return par_num;
}

static int
bnx2x_check_blocks_with_parity4(struct bnx2x_softc *sc, uint32_t sig, int par_num,
			      uint8_t print)
{
	uint32_t cur_bit = 0;
	int i = 0;

	for (i = 0; sig; i++) {
		cur_bit = ((uint32_t) 0x1 << i);
		if (sig & cur_bit) {
			switch (cur_bit) {
			case AEU_INPUTS_ATTN_BITS_PGLUE_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "PGLUE_B");
				break;
			case AEU_INPUTS_ATTN_BITS_ATC_PARITY_ERROR:
				if (print)
					bnx2x_print_next_block(sc, par_num++,
							     "ATC");
				break;
			}

			/* Clear the bit */
			sig &= ~cur_bit;
		}
	}

	return par_num;
}

static uint8_t
bnx2x_parity_attn(struct bnx2x_softc *sc, uint8_t * global, uint8_t print,
		uint32_t * sig)
{
	int par_num = 0;

	if ((sig[0] & HW_PRTY_ASSERT_SET_0) ||
	    (sig[1] & HW_PRTY_ASSERT_SET_1) ||
	    (sig[2] & HW_PRTY_ASSERT_SET_2) ||
	    (sig[3] & HW_PRTY_ASSERT_SET_3) ||
	    (sig[4] & HW_PRTY_ASSERT_SET_4)) {
		PMD_DRV_LOG(ERR, sc,
			    "Parity error: HW block parity attention:"
			    "[0]:0x%08x [1]:0x%08x [2]:0x%08x [3]:0x%08x [4]:0x%08x",
			    (uint32_t) (sig[0] & HW_PRTY_ASSERT_SET_0),
			    (uint32_t) (sig[1] & HW_PRTY_ASSERT_SET_1),
			    (uint32_t) (sig[2] & HW_PRTY_ASSERT_SET_2),
			    (uint32_t) (sig[3] & HW_PRTY_ASSERT_SET_3),
			    (uint32_t) (sig[4] & HW_PRTY_ASSERT_SET_4));

		if (print)
			PMD_DRV_LOG(INFO, sc, "Parity errors detected in blocks: ");

		par_num =
		    bnx2x_check_blocks_with_parity0(sc, sig[0] &
						  HW_PRTY_ASSERT_SET_0,
						  par_num, print);
		par_num =
		    bnx2x_check_blocks_with_parity1(sc, sig[1] &
						  HW_PRTY_ASSERT_SET_1,
						  par_num, global, print);
		par_num =
		    bnx2x_check_blocks_with_parity2(sc, sig[2] &
						  HW_PRTY_ASSERT_SET_2,
						  par_num, print);
		par_num =
		    bnx2x_check_blocks_with_parity3(sc, sig[3] &
						  HW_PRTY_ASSERT_SET_3,
						  par_num, global, print);
		par_num =
		    bnx2x_check_blocks_with_parity4(sc, sig[4] &
						  HW_PRTY_ASSERT_SET_4,
						  par_num, print);

		if (print)
			PMD_DRV_LOG(INFO, sc, "");

		return TRUE;
	}

	return FALSE;
}

static uint8_t
bnx2x_chk_parity_attn(struct bnx2x_softc *sc, uint8_t * global, uint8_t print)
{
	struct attn_route attn = { {0} };
	int port = SC_PORT(sc);

	attn.sig[0] = REG_RD(sc, MISC_REG_AEU_AFTER_INVERT_1_FUNC_0 + port * 4);
	attn.sig[1] = REG_RD(sc, MISC_REG_AEU_AFTER_INVERT_2_FUNC_0 + port * 4);
	attn.sig[2] = REG_RD(sc, MISC_REG_AEU_AFTER_INVERT_3_FUNC_0 + port * 4);
	attn.sig[3] = REG_RD(sc, MISC_REG_AEU_AFTER_INVERT_4_FUNC_0 + port * 4);

	if (!CHIP_IS_E1x(sc))
		attn.sig[4] =
		    REG_RD(sc, MISC_REG_AEU_AFTER_INVERT_5_FUNC_0 + port * 4);

	return bnx2x_parity_attn(sc, global, print, attn.sig);
}

static void bnx2x_attn_int_deasserted4(struct bnx2x_softc *sc, uint32_t attn)
{
	uint32_t val;

	if (attn & AEU_INPUTS_ATTN_BITS_PGLUE_HW_INTERRUPT) {
		val = REG_RD(sc, PGLUE_B_REG_PGLUE_B_INT_STS_CLR);
		PMD_DRV_LOG(INFO, sc, "ERROR: PGLUE hw attention 0x%08x", val);
		if (val & PGLUE_B_PGLUE_B_INT_STS_REG_ADDRESS_ERROR)
			PMD_DRV_LOG(INFO, sc,
				    "ERROR: PGLUE_B_PGLUE_B_INT_STS_REG_ADDRESS_ERROR");
		if (val & PGLUE_B_PGLUE_B_INT_STS_REG_INCORRECT_RCV_BEHAVIOR)
			PMD_DRV_LOG(INFO, sc,
				    "ERROR: PGLUE_B_PGLUE_B_INT_STS_REG_INCORRECT_RCV_BEHAVIOR");
		if (val & PGLUE_B_PGLUE_B_INT_STS_REG_WAS_ERROR_ATTN)
			PMD_DRV_LOG(INFO, sc,
				    "ERROR: PGLUE_B_PGLUE_B_INT_STS_REG_WAS_ERROR_ATTN");
		if (val & PGLUE_B_PGLUE_B_INT_STS_REG_VF_LENGTH_VIOLATION_ATTN)
			PMD_DRV_LOG(INFO, sc,
				    "ERROR: PGLUE_B_PGLUE_B_INT_STS_REG_VF_LENGTH_VIOLATION_ATTN");
		if (val &
		    PGLUE_B_PGLUE_B_INT_STS_REG_VF_GRC_SPACE_VIOLATION_ATTN)
			PMD_DRV_LOG(INFO, sc,
				    "ERROR: PGLUE_B_PGLUE_B_INT_STS_REG_VF_GRC_SPACE_VIOLATION_ATTN");
		if (val &
		    PGLUE_B_PGLUE_B_INT_STS_REG_VF_MSIX_BAR_VIOLATION_ATTN)
			PMD_DRV_LOG(INFO, sc,
				    "ERROR: PGLUE_B_PGLUE_B_INT_STS_REG_VF_MSIX_BAR_VIOLATION_ATTN");
		if (val & PGLUE_B_PGLUE_B_INT_STS_REG_TCPL_ERROR_ATTN)
			PMD_DRV_LOG(INFO, sc,
				    "ERROR: PGLUE_B_PGLUE_B_INT_STS_REG_TCPL_ERROR_ATTN");
		if (val & PGLUE_B_PGLUE_B_INT_STS_REG_TCPL_IN_TWO_RCBS_ATTN)
			PMD_DRV_LOG(INFO, sc,
				    "ERROR: PGLUE_B_PGLUE_B_INT_STS_REG_TCPL_IN_TWO_RCBS_ATTN");
		if (val & PGLUE_B_PGLUE_B_INT_STS_REG_CSSNOOP_FIFO_OVERFLOW)
			PMD_DRV_LOG(INFO, sc,
				    "ERROR: PGLUE_B_PGLUE_B_INT_STS_REG_CSSNOOP_FIFO_OVERFLOW");
	}

	if (attn & AEU_INPUTS_ATTN_BITS_ATC_HW_INTERRUPT) {
		val = REG_RD(sc, ATC_REG_ATC_INT_STS_CLR);
		PMD_DRV_LOG(INFO, sc, "ERROR: ATC hw attention 0x%08x", val);
		if (val & ATC_ATC_INT_STS_REG_ADDRESS_ERROR)
			PMD_DRV_LOG(INFO, sc,
				    "ERROR: ATC_ATC_INT_STS_REG_ADDRESS_ERROR");
		if (val & ATC_ATC_INT_STS_REG_ATC_TCPL_TO_NOT_PEND)
			PMD_DRV_LOG(INFO, sc,
				    "ERROR: ATC_ATC_INT_STS_REG_ATC_TCPL_TO_NOT_PEND");
		if (val & ATC_ATC_INT_STS_REG_ATC_GPA_MULTIPLE_HITS)
			PMD_DRV_LOG(INFO, sc,
				    "ERROR: ATC_ATC_INT_STS_REG_ATC_GPA_MULTIPLE_HITS");
		if (val & ATC_ATC_INT_STS_REG_ATC_RCPL_TO_EMPTY_CNT)
			PMD_DRV_LOG(INFO, sc,
				    "ERROR: ATC_ATC_INT_STS_REG_ATC_RCPL_TO_EMPTY_CNT");
		if (val & ATC_ATC_INT_STS_REG_ATC_TCPL_ERROR)
			PMD_DRV_LOG(INFO, sc,
				    "ERROR: ATC_ATC_INT_STS_REG_ATC_TCPL_ERROR");
		if (val & ATC_ATC_INT_STS_REG_ATC_IREQ_LESS_THAN_STU)
			PMD_DRV_LOG(INFO, sc,
				    "ERROR: ATC_ATC_INT_STS_REG_ATC_IREQ_LESS_THAN_STU");
	}

	if (attn & (AEU_INPUTS_ATTN_BITS_PGLUE_PARITY_ERROR |
		    AEU_INPUTS_ATTN_BITS_ATC_PARITY_ERROR)) {
		PMD_DRV_LOG(INFO, sc,
			    "ERROR: FATAL parity attention set4 0x%08x",
			    (uint32_t) (attn &
					(AEU_INPUTS_ATTN_BITS_PGLUE_PARITY_ERROR
					 |
					 AEU_INPUTS_ATTN_BITS_ATC_PARITY_ERROR)));
	}
}

static void bnx2x_e1h_disable(struct bnx2x_softc *sc)
{
	int port = SC_PORT(sc);

	REG_WR(sc, NIG_REG_LLH0_FUNC_EN + port * 8, 0);
}

static void bnx2x_e1h_enable(struct bnx2x_softc *sc)
{
	int port = SC_PORT(sc);

	REG_WR(sc, NIG_REG_LLH0_FUNC_EN + port * 8, 1);
}

/*
 * called due to MCP event (on pmf):
 *   reread new bandwidth configuration
 *   configure FW
 *   notify others function about the change
 */
static void bnx2x_config_mf_bw(struct bnx2x_softc *sc)
{
	if (sc->link_vars.link_up) {
		bnx2x_cmng_fns_init(sc, TRUE, CMNG_FNS_MINMAX);
		bnx2x_link_sync_notify(sc);
	}

	storm_memset_cmng(sc, &sc->cmng, SC_PORT(sc));
}

static void bnx2x_set_mf_bw(struct bnx2x_softc *sc)
{
	bnx2x_config_mf_bw(sc);
	bnx2x_fw_command(sc, DRV_MSG_CODE_SET_MF_BW_ACK, 0);
}

static void bnx2x_handle_eee_event(struct bnx2x_softc *sc)
{
	bnx2x_fw_command(sc, DRV_MSG_CODE_EEE_RESULTS_ACK, 0);
}

#define DRV_INFO_ETH_STAT_NUM_MACS_REQUIRED 3

static void bnx2x_drv_info_ether_stat(struct bnx2x_softc *sc)
{
	struct eth_stats_info *ether_stat = &sc->sp->drv_info_to_mcp.ether_stat;

	strncpy(ether_stat->version, BNX2X_DRIVER_VERSION,
		ETH_STAT_INFO_VERSION_LEN);

	sc->sp_objs[0].mac_obj.get_n_elements(sc, &sc->sp_objs[0].mac_obj,
					      DRV_INFO_ETH_STAT_NUM_MACS_REQUIRED,
					      ether_stat->mac_local + MAC_PAD,
					      MAC_PAD, ETH_ALEN);

	ether_stat->mtu_size = sc->mtu;

	ether_stat->feature_flags |= FEATURE_ETH_CHKSUM_OFFLOAD_MASK;
	ether_stat->promiscuous_mode = 0;	// (flags & PROMISC) ? 1 : 0;

	ether_stat->txq_size = sc->tx_ring_size;
	ether_stat->rxq_size = sc->rx_ring_size;
}

static void bnx2x_handle_drv_info_req(struct bnx2x_softc *sc)
{
	enum drv_info_opcode op_code;
	uint32_t drv_info_ctl = SHMEM2_RD(sc, drv_info_control);

	/* if drv_info version supported by MFW doesn't match - send NACK */
	if ((drv_info_ctl & DRV_INFO_CONTROL_VER_MASK) != DRV_INFO_CUR_VER) {
		bnx2x_fw_command(sc, DRV_MSG_CODE_DRV_INFO_NACK, 0);
		return;
	}

	op_code = ((drv_info_ctl & DRV_INFO_CONTROL_OP_CODE_MASK) >>
		   DRV_INFO_CONTROL_OP_CODE_SHIFT);

	memset(&sc->sp->drv_info_to_mcp, 0, sizeof(union drv_info_to_mcp));

	switch (op_code) {
	case ETH_STATS_OPCODE:
		bnx2x_drv_info_ether_stat(sc);
		break;
	case FCOE_STATS_OPCODE:
	case ISCSI_STATS_OPCODE:
	default:
		/* if op code isn't supported - send NACK */
		bnx2x_fw_command(sc, DRV_MSG_CODE_DRV_INFO_NACK, 0);
		return;
	}

	/*
	 * If we got drv_info attn from MFW then these fields are defined in
	 * shmem2 for sure
	 */
	SHMEM2_WR(sc, drv_info_host_addr_lo,
		  U64_LO(BNX2X_SP_MAPPING(sc, drv_info_to_mcp)));
	SHMEM2_WR(sc, drv_info_host_addr_hi,
		  U64_HI(BNX2X_SP_MAPPING(sc, drv_info_to_mcp)));

	bnx2x_fw_command(sc, DRV_MSG_CODE_DRV_INFO_ACK, 0);
}

static void bnx2x_dcc_event(struct bnx2x_softc *sc, uint32_t dcc_event)
{
	if (dcc_event & DRV_STATUS_DCC_DISABLE_ENABLE_PF) {
/*
 * This is the only place besides the function initialization
 * where the sc->flags can change so it is done without any
 * locks
 */
		if (sc->devinfo.
		    mf_info.mf_config[SC_VN(sc)] & FUNC_MF_CFG_FUNC_DISABLED) {
			PMD_DRV_LOG(DEBUG, sc, "mf_cfg function disabled");
			sc->flags |= BNX2X_MF_FUNC_DIS;
			bnx2x_e1h_disable(sc);
		} else {
			PMD_DRV_LOG(DEBUG, sc, "mf_cfg function enabled");
			sc->flags &= ~BNX2X_MF_FUNC_DIS;
			bnx2x_e1h_enable(sc);
		}
		dcc_event &= ~DRV_STATUS_DCC_DISABLE_ENABLE_PF;
	}

	if (dcc_event & DRV_STATUS_DCC_BANDWIDTH_ALLOCATION) {
		bnx2x_config_mf_bw(sc);
		dcc_event &= ~DRV_STATUS_DCC_BANDWIDTH_ALLOCATION;
	}

	/* Report results to MCP */
	if (dcc_event)
		bnx2x_fw_command(sc, DRV_MSG_CODE_DCC_FAILURE, 0);
	else
		bnx2x_fw_command(sc, DRV_MSG_CODE_DCC_OK, 0);
}

static void bnx2x_pmf_update(struct bnx2x_softc *sc)
{
	int port = SC_PORT(sc);
	uint32_t val;

	sc->port.pmf = 1;

	/*
	 * We need the mb() to ensure the ordering between the writing to
	 * sc->port.pmf here and reading it from the bnx2x_periodic_task().
	 */
	mb();

	/* enable nig attention */
	val = (0xff0f | (1 << (SC_VN(sc) + 4)));
	if (sc->devinfo.int_block == INT_BLOCK_HC) {
		REG_WR(sc, HC_REG_TRAILING_EDGE_0 + port * 8, val);
		REG_WR(sc, HC_REG_LEADING_EDGE_0 + port * 8, val);
	} else if (!CHIP_IS_E1x(sc)) {
		REG_WR(sc, IGU_REG_TRAILING_EDGE_LATCH, val);
		REG_WR(sc, IGU_REG_LEADING_EDGE_LATCH, val);
	}

	bnx2x_stats_handle(sc, STATS_EVENT_PMF);
}

static int bnx2x_mc_assert(struct bnx2x_softc *sc)
{
	char last_idx;
	int i, rc = 0;
	__rte_unused uint32_t row0, row1, row2, row3;

	/* XSTORM */
	last_idx =
	    REG_RD8(sc, BAR_XSTRORM_INTMEM + XSTORM_ASSERT_LIST_INDEX_OFFSET);
	if (last_idx)
		PMD_DRV_LOG(ERR, sc, "XSTORM_ASSERT_LIST_INDEX 0x%x", last_idx);

	/* print the asserts */
	for (i = 0; i < STORM_ASSERT_ARRAY_SIZE; i++) {

		row0 =
		    REG_RD(sc,
			   BAR_XSTRORM_INTMEM + XSTORM_ASSERT_LIST_OFFSET(i));
		row1 =
		    REG_RD(sc,
			   BAR_XSTRORM_INTMEM + XSTORM_ASSERT_LIST_OFFSET(i) +
			   4);
		row2 =
		    REG_RD(sc,
			   BAR_XSTRORM_INTMEM + XSTORM_ASSERT_LIST_OFFSET(i) +
			   8);
		row3 =
		    REG_RD(sc,
			   BAR_XSTRORM_INTMEM + XSTORM_ASSERT_LIST_OFFSET(i) +
			   12);

		if (row0 != COMMON_ASM_INVALID_ASSERT_OPCODE) {
			PMD_DRV_LOG(ERR, sc,
				    "XSTORM_ASSERT_INDEX 0x%x = 0x%08x 0x%08x 0x%08x 0x%08x",
				    i, row3, row2, row1, row0);
			rc++;
		} else {
			break;
		}
	}

	/* TSTORM */
	last_idx =
	    REG_RD8(sc, BAR_TSTRORM_INTMEM + TSTORM_ASSERT_LIST_INDEX_OFFSET);
	if (last_idx) {
		PMD_DRV_LOG(ERR, sc, "TSTORM_ASSERT_LIST_INDEX 0x%x", last_idx);
	}

	/* print the asserts */
	for (i = 0; i < STORM_ASSERT_ARRAY_SIZE; i++) {

		row0 =
		    REG_RD(sc,
			   BAR_TSTRORM_INTMEM + TSTORM_ASSERT_LIST_OFFSET(i));
		row1 =
		    REG_RD(sc,
			   BAR_TSTRORM_INTMEM + TSTORM_ASSERT_LIST_OFFSET(i) +
			   4);
		row2 =
		    REG_RD(sc,
			   BAR_TSTRORM_INTMEM + TSTORM_ASSERT_LIST_OFFSET(i) +
			   8);
		row3 =
		    REG_RD(sc,
			   BAR_TSTRORM_INTMEM + TSTORM_ASSERT_LIST_OFFSET(i) +
			   12);

		if (row0 != COMMON_ASM_INVALID_ASSERT_OPCODE) {
			PMD_DRV_LOG(ERR, sc,
				    "TSTORM_ASSERT_INDEX 0x%x = 0x%08x 0x%08x 0x%08x 0x%08x",
				    i, row3, row2, row1, row0);
			rc++;
		} else {
			break;
		}
	}

	/* CSTORM */
	last_idx =
	    REG_RD8(sc, BAR_CSTRORM_INTMEM + CSTORM_ASSERT_LIST_INDEX_OFFSET);
	if (last_idx) {
		PMD_DRV_LOG(ERR, sc, "CSTORM_ASSERT_LIST_INDEX 0x%x", last_idx);
	}

	/* print the asserts */
	for (i = 0; i < STORM_ASSERT_ARRAY_SIZE; i++) {

		row0 =
		    REG_RD(sc,
			   BAR_CSTRORM_INTMEM + CSTORM_ASSERT_LIST_OFFSET(i));
		row1 =
		    REG_RD(sc,
			   BAR_CSTRORM_INTMEM + CSTORM_ASSERT_LIST_OFFSET(i) +
			   4);
		row2 =
		    REG_RD(sc,
			   BAR_CSTRORM_INTMEM + CSTORM_ASSERT_LIST_OFFSET(i) +
			   8);
		row3 =
		    REG_RD(sc,
			   BAR_CSTRORM_INTMEM + CSTORM_ASSERT_LIST_OFFSET(i) +
			   12);

		if (row0 != COMMON_ASM_INVALID_ASSERT_OPCODE) {
			PMD_DRV_LOG(ERR, sc,
				    "CSTORM_ASSERT_INDEX 0x%x = 0x%08x 0x%08x 0x%08x 0x%08x",
				    i, row3, row2, row1, row0);
			rc++;
		} else {
			break;
		}
	}

	/* USTORM */
	last_idx =
	    REG_RD8(sc, BAR_USTRORM_INTMEM + USTORM_ASSERT_LIST_INDEX_OFFSET);
	if (last_idx) {
		PMD_DRV_LOG(ERR, sc, "USTORM_ASSERT_LIST_INDEX 0x%x", last_idx);
	}

	/* print the asserts */
	for (i = 0; i < STORM_ASSERT_ARRAY_SIZE; i++) {

		row0 =
		    REG_RD(sc,
			   BAR_USTRORM_INTMEM + USTORM_ASSERT_LIST_OFFSET(i));
		row1 =
		    REG_RD(sc,
			   BAR_USTRORM_INTMEM + USTORM_ASSERT_LIST_OFFSET(i) +
			   4);
		row2 =
		    REG_RD(sc,
			   BAR_USTRORM_INTMEM + USTORM_ASSERT_LIST_OFFSET(i) +
			   8);
		row3 =
		    REG_RD(sc,
			   BAR_USTRORM_INTMEM + USTORM_ASSERT_LIST_OFFSET(i) +
			   12);

		if (row0 != COMMON_ASM_INVALID_ASSERT_OPCODE) {
			PMD_DRV_LOG(ERR, sc,
				    "USTORM_ASSERT_INDEX 0x%x = 0x%08x 0x%08x 0x%08x 0x%08x",
				    i, row3, row2, row1, row0);
			rc++;
		} else {
			break;
		}
	}

	return rc;
}

static void bnx2x_attn_int_deasserted3(struct bnx2x_softc *sc, uint32_t attn)
{
	int func = SC_FUNC(sc);
	uint32_t val;

	if (attn & EVEREST_GEN_ATTN_IN_USE_MASK) {

		if (attn & BNX2X_PMF_LINK_ASSERT(sc)) {

			REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_12 + func * 4, 0);
			bnx2x_read_mf_cfg(sc);
			sc->devinfo.mf_info.mf_config[SC_VN(sc)] =
			    MFCFG_RD(sc,
				     func_mf_config[SC_ABS_FUNC(sc)].config);
			val =
			    SHMEM_RD(sc, func_mb[SC_FW_MB_IDX(sc)].drv_status);

			if (val & DRV_STATUS_DCC_EVENT_MASK)
				bnx2x_dcc_event(sc,
					      (val &
					       DRV_STATUS_DCC_EVENT_MASK));

			if (val & DRV_STATUS_SET_MF_BW)
				bnx2x_set_mf_bw(sc);

			if (val & DRV_STATUS_DRV_INFO_REQ)
				bnx2x_handle_drv_info_req(sc);

			if ((sc->port.pmf == 0) && (val & DRV_STATUS_PMF))
				bnx2x_pmf_update(sc);

			if (val & DRV_STATUS_EEE_NEGOTIATION_RESULTS)
				bnx2x_handle_eee_event(sc);

			if (sc->link_vars.periodic_flags &
			    ELINK_PERIODIC_FLAGS_LINK_EVENT) {
				/* sync with link */
				bnx2x_acquire_phy_lock(sc);
				sc->link_vars.periodic_flags &=
				    ~ELINK_PERIODIC_FLAGS_LINK_EVENT;
				bnx2x_release_phy_lock(sc);
				if (IS_MF(sc)) {
					bnx2x_link_sync_notify(sc);
				}
				bnx2x_link_report(sc);
			}

			/*
			 * Always call it here: bnx2x_link_report() will
			 * prevent the link indication duplication.
			 */
			bnx2x_link_status_update(sc);

		} else if (attn & BNX2X_MC_ASSERT_BITS) {

			PMD_DRV_LOG(ERR, sc, "MC assert!");
			bnx2x_mc_assert(sc);
			REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_10, 0);
			REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_9, 0);
			REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_8, 0);
			REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_7, 0);
			rte_panic("MC assert!");

		} else if (attn & BNX2X_MCP_ASSERT) {

			PMD_DRV_LOG(ERR, sc, "MCP assert!");
			REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_11, 0);

		} else {
			PMD_DRV_LOG(ERR, sc,
				    "Unknown HW assert! (attn 0x%08x)", attn);
		}
	}

	if (attn & EVEREST_LATCHED_ATTN_IN_USE_MASK) {
		PMD_DRV_LOG(ERR, sc, "LATCHED attention 0x%08x (masked)", attn);
		if (attn & BNX2X_GRC_TIMEOUT) {
			val = REG_RD(sc, MISC_REG_GRC_TIMEOUT_ATTN);
			PMD_DRV_LOG(ERR, sc, "GRC time-out 0x%08x", val);
		}
		if (attn & BNX2X_GRC_RSV) {
			val = REG_RD(sc, MISC_REG_GRC_RSV_ATTN);
			PMD_DRV_LOG(ERR, sc, "GRC reserved 0x%08x", val);
		}
		REG_WR(sc, MISC_REG_AEU_CLR_LATCH_SIGNAL, 0x7ff);
	}
}

static void bnx2x_attn_int_deasserted2(struct bnx2x_softc *sc, uint32_t attn)
{
	int port = SC_PORT(sc);
	int reg_offset;
	uint32_t val0, mask0, val1, mask1;
	uint32_t val;

	if (attn & AEU_INPUTS_ATTN_BITS_CFC_HW_INTERRUPT) {
		val = REG_RD(sc, CFC_REG_CFC_INT_STS_CLR);
		PMD_DRV_LOG(ERR, sc, "CFC hw attention 0x%08x", val);
/* CFC error attention */
		if (val & 0x2) {
			PMD_DRV_LOG(ERR, sc, "FATAL error from CFC");
		}
	}

	if (attn & AEU_INPUTS_ATTN_BITS_PXP_HW_INTERRUPT) {
		val = REG_RD(sc, PXP_REG_PXP_INT_STS_CLR_0);
		PMD_DRV_LOG(ERR, sc, "PXP hw attention-0 0x%08x", val);
/* RQ_USDMDP_FIFO_OVERFLOW */
		if (val & 0x18000) {
			PMD_DRV_LOG(ERR, sc, "FATAL error from PXP");
		}

		if (!CHIP_IS_E1x(sc)) {
			val = REG_RD(sc, PXP_REG_PXP_INT_STS_CLR_1);
			PMD_DRV_LOG(ERR, sc, "PXP hw attention-1 0x%08x", val);
		}
	}
#define PXP2_EOP_ERROR_BIT  PXP2_PXP2_INT_STS_CLR_0_REG_WR_PGLUE_EOP_ERROR
#define AEU_PXP2_HW_INT_BIT AEU_INPUTS_ATTN_BITS_PXPPCICLOCKCLIENT_HW_INTERRUPT

	if (attn & AEU_PXP2_HW_INT_BIT) {
/*  CQ47854 workaround do not panic on
 *  PXP2_PXP2_INT_STS_0_REG_WR_PGLUE_EOP_ERROR
 */
		if (!CHIP_IS_E1x(sc)) {
			mask0 = REG_RD(sc, PXP2_REG_PXP2_INT_MASK_0);
			val1 = REG_RD(sc, PXP2_REG_PXP2_INT_STS_1);
			mask1 = REG_RD(sc, PXP2_REG_PXP2_INT_MASK_1);
			val0 = REG_RD(sc, PXP2_REG_PXP2_INT_STS_0);
			/*
			 * If the only PXP2_EOP_ERROR_BIT is set in
			 * STS0 and STS1 - clear it
			 *
			 * probably we lose additional attentions between
			 * STS0 and STS_CLR0, in this case user will not
			 * be notified about them
			 */
			if (val0 & mask0 & PXP2_EOP_ERROR_BIT &&
			    !(val1 & mask1))
				val0 = REG_RD(sc, PXP2_REG_PXP2_INT_STS_CLR_0);

			/* print the register, since no one can restore it */
			PMD_DRV_LOG(ERR, sc,
				    "PXP2_REG_PXP2_INT_STS_CLR_0 0x%08x", val0);

			/*
			 * if PXP2_PXP2_INT_STS_0_REG_WR_PGLUE_EOP_ERROR
			 * then notify
			 */
			if (val0 & PXP2_EOP_ERROR_BIT) {
				PMD_DRV_LOG(ERR, sc, "PXP2_WR_PGLUE_EOP_ERROR");

				/*
				 * if only PXP2_PXP2_INT_STS_0_REG_WR_PGLUE_EOP_ERROR is
				 * set then clear attention from PXP2 block without panic
				 */
				if (((val0 & mask0) == PXP2_EOP_ERROR_BIT) &&
				    ((val1 & mask1) == 0))
					attn &= ~AEU_PXP2_HW_INT_BIT;
			}
		}
	}

	if (attn & HW_INTERRUT_ASSERT_SET_2) {
		reg_offset = (port ? MISC_REG_AEU_ENABLE1_FUNC_1_OUT_2 :
			      MISC_REG_AEU_ENABLE1_FUNC_0_OUT_2);

		val = REG_RD(sc, reg_offset);
		val &= ~(attn & HW_INTERRUT_ASSERT_SET_2);
		REG_WR(sc, reg_offset, val);

		PMD_DRV_LOG(ERR, sc,
			    "FATAL HW block attention set2 0x%x",
			    (uint32_t) (attn & HW_INTERRUT_ASSERT_SET_2));
		rte_panic("HW block attention set2");
	}
}

static void bnx2x_attn_int_deasserted1(struct bnx2x_softc *sc, uint32_t attn)
{
	int port = SC_PORT(sc);
	int reg_offset;
	uint32_t val;

	if (attn & AEU_INPUTS_ATTN_BITS_DOORBELLQ_HW_INTERRUPT) {
		val = REG_RD(sc, DORQ_REG_DORQ_INT_STS_CLR);
		PMD_DRV_LOG(ERR, sc, "DB hw attention 0x%08x", val);
/* DORQ discard attention */
		if (val & 0x2) {
			PMD_DRV_LOG(ERR, sc, "FATAL error from DORQ");
		}
	}

	if (attn & HW_INTERRUT_ASSERT_SET_1) {
		reg_offset = (port ? MISC_REG_AEU_ENABLE1_FUNC_1_OUT_1 :
			      MISC_REG_AEU_ENABLE1_FUNC_0_OUT_1);

		val = REG_RD(sc, reg_offset);
		val &= ~(attn & HW_INTERRUT_ASSERT_SET_1);
		REG_WR(sc, reg_offset, val);

		PMD_DRV_LOG(ERR, sc,
			    "FATAL HW block attention set1 0x%08x",
			    (uint32_t) (attn & HW_INTERRUT_ASSERT_SET_1));
		rte_panic("HW block attention set1");
	}
}

static void bnx2x_attn_int_deasserted0(struct bnx2x_softc *sc, uint32_t attn)
{
	int port = SC_PORT(sc);
	int reg_offset;
	uint32_t val;

	reg_offset = (port) ? MISC_REG_AEU_ENABLE1_FUNC_1_OUT_0 :
	    MISC_REG_AEU_ENABLE1_FUNC_0_OUT_0;

	if (attn & AEU_INPUTS_ATTN_BITS_SPIO5) {
		val = REG_RD(sc, reg_offset);
		val &= ~AEU_INPUTS_ATTN_BITS_SPIO5;
		REG_WR(sc, reg_offset, val);

		PMD_DRV_LOG(WARNING, sc, "SPIO5 hw attention");

/* Fan failure attention */
		elink_hw_reset_phy(&sc->link_params);
		bnx2x_fan_failure(sc);
	}

	if ((attn & sc->link_vars.aeu_int_mask) && sc->port.pmf) {
		bnx2x_acquire_phy_lock(sc);
		elink_handle_module_detect_int(&sc->link_params);
		bnx2x_release_phy_lock(sc);
	}

	if (attn & HW_INTERRUT_ASSERT_SET_0) {
		val = REG_RD(sc, reg_offset);
		val &= ~(attn & HW_INTERRUT_ASSERT_SET_0);
		REG_WR(sc, reg_offset, val);

		rte_panic("FATAL HW block attention set0 0x%lx",
			  (attn & (unsigned long)HW_INTERRUT_ASSERT_SET_0));
	}
}

static void bnx2x_attn_int_deasserted(struct bnx2x_softc *sc, uint32_t deasserted)
{
	struct attn_route attn;
	struct attn_route *group_mask;
	int port = SC_PORT(sc);
	int index;
	uint32_t reg_addr;
	uint32_t val;
	uint32_t aeu_mask;
	uint8_t global = FALSE;

	/*
	 * Need to take HW lock because MCP or other port might also
	 * try to handle this event.
	 */
	bnx2x_acquire_alr(sc);

	if (bnx2x_chk_parity_attn(sc, &global, TRUE)) {
		sc->recovery_state = BNX2X_RECOVERY_INIT;

/* disable HW interrupts */
		bnx2x_int_disable(sc);
		bnx2x_release_alr(sc);
		return;
	}

	attn.sig[0] = REG_RD(sc, MISC_REG_AEU_AFTER_INVERT_1_FUNC_0 + port * 4);
	attn.sig[1] = REG_RD(sc, MISC_REG_AEU_AFTER_INVERT_2_FUNC_0 + port * 4);
	attn.sig[2] = REG_RD(sc, MISC_REG_AEU_AFTER_INVERT_3_FUNC_0 + port * 4);
	attn.sig[3] = REG_RD(sc, MISC_REG_AEU_AFTER_INVERT_4_FUNC_0 + port * 4);
	if (!CHIP_IS_E1x(sc)) {
		attn.sig[4] =
		    REG_RD(sc, MISC_REG_AEU_AFTER_INVERT_5_FUNC_0 + port * 4);
	} else {
		attn.sig[4] = 0;
	}

	for (index = 0; index < MAX_DYNAMIC_ATTN_GRPS; index++) {
		if (deasserted & (1 << index)) {
			group_mask = &sc->attn_group[index];

			bnx2x_attn_int_deasserted4(sc,
						 attn.
						 sig[4] & group_mask->sig[4]);
			bnx2x_attn_int_deasserted3(sc,
						 attn.
						 sig[3] & group_mask->sig[3]);
			bnx2x_attn_int_deasserted1(sc,
						 attn.
						 sig[1] & group_mask->sig[1]);
			bnx2x_attn_int_deasserted2(sc,
						 attn.
						 sig[2] & group_mask->sig[2]);
			bnx2x_attn_int_deasserted0(sc,
						 attn.
						 sig[0] & group_mask->sig[0]);
		}
	}

	bnx2x_release_alr(sc);

	if (sc->devinfo.int_block == INT_BLOCK_HC) {
		reg_addr = (HC_REG_COMMAND_REG + port * 32 +
			    COMMAND_REG_ATTN_BITS_CLR);
	} else {
		reg_addr = (BAR_IGU_INTMEM + IGU_CMD_ATTN_BIT_CLR_UPPER * 8);
	}

	val = ~deasserted;
	PMD_DRV_LOG(DEBUG, sc,
		    "about to mask 0x%08x at %s addr 0x%08x", val,
		    (sc->devinfo.int_block == INT_BLOCK_HC) ? "HC" : "IGU",
		    reg_addr);
	REG_WR(sc, reg_addr, val);

	if (~sc->attn_state & deasserted) {
		PMD_DRV_LOG(ERR, sc, "IGU error");
	}

	reg_addr = port ? MISC_REG_AEU_MASK_ATTN_FUNC_1 :
	    MISC_REG_AEU_MASK_ATTN_FUNC_0;

	bnx2x_acquire_hw_lock(sc, HW_LOCK_RESOURCE_PORT0_ATT_MASK + port);

	aeu_mask = REG_RD(sc, reg_addr);

	aeu_mask |= (deasserted & 0x3ff);

	REG_WR(sc, reg_addr, aeu_mask);
	bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_PORT0_ATT_MASK + port);

	sc->attn_state &= ~deasserted;
}

static void bnx2x_attn_int(struct bnx2x_softc *sc)
{
	/* read local copy of bits */
	uint32_t attn_bits = le32toh(sc->def_sb->atten_status_block.attn_bits);
	uint32_t attn_ack =
	    le32toh(sc->def_sb->atten_status_block.attn_bits_ack);
	uint32_t attn_state = sc->attn_state;

	/* look for changed bits */
	uint32_t asserted = attn_bits & ~attn_ack & ~attn_state;
	uint32_t deasserted = ~attn_bits & attn_ack & attn_state;

	PMD_DRV_LOG(DEBUG, sc,
		    "attn_bits 0x%08x attn_ack 0x%08x asserted 0x%08x deasserted 0x%08x",
		    attn_bits, attn_ack, asserted, deasserted);

	if (~(attn_bits ^ attn_ack) & (attn_bits ^ attn_state)) {
		PMD_DRV_LOG(ERR, sc, "BAD attention state");
	}

	/* handle bits that were raised */
	if (asserted) {
		bnx2x_attn_int_asserted(sc, asserted);
	}

	if (deasserted) {
		bnx2x_attn_int_deasserted(sc, deasserted);
	}
}

static uint16_t bnx2x_update_dsb_idx(struct bnx2x_softc *sc)
{
	struct host_sp_status_block *def_sb = sc->def_sb;
	uint16_t rc = 0;

	if (!def_sb)
		return 0;

	mb();			/* status block is written to by the chip */

	if (sc->def_att_idx != def_sb->atten_status_block.attn_bits_index) {
		sc->def_att_idx = def_sb->atten_status_block.attn_bits_index;
		rc |= BNX2X_DEF_SB_ATT_IDX;
	}

	if (sc->def_idx != def_sb->sp_sb.running_index) {
		sc->def_idx = def_sb->sp_sb.running_index;
		rc |= BNX2X_DEF_SB_IDX;
	}

	mb();

	return rc;
}

static struct ecore_queue_sp_obj *bnx2x_cid_to_q_obj(struct bnx2x_softc *sc,
							  uint32_t cid)
{
	return &sc->sp_objs[CID_TO_FP(cid, sc)].q_obj;
}

static void bnx2x_handle_mcast_eqe(struct bnx2x_softc *sc)
{
	struct ecore_mcast_ramrod_params rparam;
	int rc;

	memset(&rparam, 0, sizeof(rparam));

	rparam.mcast_obj = &sc->mcast_obj;

	/* clear pending state for the last command */
	sc->mcast_obj.raw.clear_pending(&sc->mcast_obj.raw);

	/* if there are pending mcast commands - send them */
	if (sc->mcast_obj.check_pending(&sc->mcast_obj)) {
		rc = ecore_config_mcast(sc, &rparam, ECORE_MCAST_CMD_CONT);
		if (rc < 0) {
			PMD_DRV_LOG(INFO, sc,
				    "Failed to send pending mcast commands (%d)",
				    rc);
		}
	}
}

static void
bnx2x_handle_classification_eqe(struct bnx2x_softc *sc, union event_ring_elem *elem)
{
	uint32_t ramrod_flags = 0;
	int rc = 0;
	uint32_t cid = elem->message.data.eth_event.echo & BNX2X_SWCID_MASK;
	struct ecore_vlan_mac_obj *vlan_mac_obj;

	/* always push next commands out, don't wait here */
	rte_bit_relaxed_set32(RAMROD_CONT, &ramrod_flags);

	switch (le32toh(elem->message.data.eth_event.echo) >> BNX2X_SWCID_SHIFT) {
	case ECORE_FILTER_MAC_PENDING:
		PMD_DRV_LOG(DEBUG, sc, "Got SETUP_MAC completions");
		vlan_mac_obj = &sc->sp_objs[cid].mac_obj;
		break;

	case ECORE_FILTER_MCAST_PENDING:
		PMD_DRV_LOG(DEBUG, sc, "Got SETUP_MCAST completions");
		bnx2x_handle_mcast_eqe(sc);
		return;

	default:
		PMD_DRV_LOG(NOTICE, sc, "Unsupported classification command: %d",
			    elem->message.data.eth_event.echo);
		return;
	}

	rc = vlan_mac_obj->complete(sc, vlan_mac_obj, elem, &ramrod_flags);

	if (rc < 0) {
		PMD_DRV_LOG(NOTICE, sc,
			    "Failed to schedule new commands (%d)", rc);
	} else if (rc > 0) {
		PMD_DRV_LOG(DEBUG, sc, "Scheduled next pending commands...");
	}
}

static void bnx2x_handle_rx_mode_eqe(struct bnx2x_softc *sc)
{
	rte_bit_relaxed_clear32(ECORE_FILTER_RX_MODE_PENDING, &sc->sp_state);

	/* send rx_mode command again if was requested */
	if (rte_bit_relaxed_test_and_clear32(ECORE_FILTER_RX_MODE_SCHED,
						&sc->sp_state))
		bnx2x_set_storm_rx_mode(sc);
}

static void bnx2x_update_eq_prod(struct bnx2x_softc *sc, uint16_t prod)
{
	storm_memset_eq_prod(sc, prod, SC_FUNC(sc));
	wmb();			/* keep prod updates ordered */
}

static void bnx2x_eq_int(struct bnx2x_softc *sc)
{
	uint16_t hw_cons, sw_cons, sw_prod;
	union event_ring_elem *elem;
	uint8_t echo;
	uint32_t cid;
	uint8_t opcode;
	int spqe_cnt = 0;
	struct ecore_queue_sp_obj *q_obj;
	struct ecore_func_sp_obj *f_obj = &sc->func_obj;
	struct ecore_raw_obj *rss_raw = &sc->rss_conf_obj.raw;

	hw_cons = le16toh(*sc->eq_cons_sb);

	/*
	 * The hw_cons range is 1-255, 257 - the sw_cons range is 0-254, 256.
	 * when we get to the next-page we need to adjust so the loop
	 * condition below will be met. The next element is the size of a
	 * regular element and hence incrementing by 1
	 */
	if ((hw_cons & EQ_DESC_MAX_PAGE) == EQ_DESC_MAX_PAGE) {
		hw_cons++;
	}

	/*
	 * This function may never run in parallel with itself for a
	 * specific sc and no need for a read memory barrier here.
	 */
	sw_cons = sc->eq_cons;
	sw_prod = sc->eq_prod;

	for (;
	     sw_cons != hw_cons;
	     sw_prod = NEXT_EQ_IDX(sw_prod), sw_cons = NEXT_EQ_IDX(sw_cons)) {

		elem = &sc->eq[EQ_DESC(sw_cons)];

/* elem CID originates from FW, actually LE */
		cid = SW_CID(elem->message.data.cfc_del_event.cid);
		opcode = elem->message.opcode;

/* handle eq element */
		switch (opcode) {
		case EVENT_RING_OPCODE_STAT_QUERY:
			PMD_DEBUG_PERIODIC_LOG(DEBUG, sc, "got statistics completion event %d",
				    sc->stats_comp++);
			/* nothing to do with stats comp */
			goto next_spqe;

		case EVENT_RING_OPCODE_CFC_DEL:
			/* handle according to cid range */
			/* we may want to verify here that the sc state is HALTING */
			PMD_DRV_LOG(DEBUG, sc, "got delete ramrod for MULTI[%d]",
				    cid);
			q_obj = bnx2x_cid_to_q_obj(sc, cid);
			if (q_obj->complete_cmd(sc, q_obj, ECORE_Q_CMD_CFC_DEL)) {
				break;
			}
			goto next_spqe;

		case EVENT_RING_OPCODE_STOP_TRAFFIC:
			PMD_DRV_LOG(DEBUG, sc, "got STOP TRAFFIC");
			if (f_obj->complete_cmd(sc, f_obj, ECORE_F_CMD_TX_STOP)) {
				break;
			}
			goto next_spqe;

		case EVENT_RING_OPCODE_START_TRAFFIC:
			PMD_DRV_LOG(DEBUG, sc, "got START TRAFFIC");
			if (f_obj->complete_cmd
			    (sc, f_obj, ECORE_F_CMD_TX_START)) {
				break;
			}
			goto next_spqe;

		case EVENT_RING_OPCODE_FUNCTION_UPDATE:
			echo = elem->message.data.function_update_event.echo;
			if (echo == SWITCH_UPDATE) {
				PMD_DRV_LOG(DEBUG, sc,
					    "got FUNC_SWITCH_UPDATE ramrod");
				if (f_obj->complete_cmd(sc, f_obj,
							ECORE_F_CMD_SWITCH_UPDATE))
				{
					break;
				}
			} else {
				PMD_DRV_LOG(DEBUG, sc,
					    "AFEX: ramrod completed FUNCTION_UPDATE");
				f_obj->complete_cmd(sc, f_obj,
						    ECORE_F_CMD_AFEX_UPDATE);
			}
			goto next_spqe;

		case EVENT_RING_OPCODE_FORWARD_SETUP:
			q_obj = &bnx2x_fwd_sp_obj(sc, q_obj);
			if (q_obj->complete_cmd(sc, q_obj,
						ECORE_Q_CMD_SETUP_TX_ONLY)) {
				break;
			}
			goto next_spqe;

		case EVENT_RING_OPCODE_FUNCTION_START:
			PMD_DRV_LOG(DEBUG, sc, "got FUNC_START ramrod");
			if (f_obj->complete_cmd(sc, f_obj, ECORE_F_CMD_START)) {
				break;
			}
			goto next_spqe;

		case EVENT_RING_OPCODE_FUNCTION_STOP:
			PMD_DRV_LOG(DEBUG, sc, "got FUNC_STOP ramrod");
			if (f_obj->complete_cmd(sc, f_obj, ECORE_F_CMD_STOP)) {
				break;
			}
			goto next_spqe;
		}

		switch (opcode | sc->state) {
		case (EVENT_RING_OPCODE_RSS_UPDATE_RULES | BNX2X_STATE_OPEN):
		case (EVENT_RING_OPCODE_RSS_UPDATE_RULES | BNX2X_STATE_OPENING_WAITING_PORT):
			cid =
			    elem->message.data.eth_event.echo & BNX2X_SWCID_MASK;
			PMD_DRV_LOG(DEBUG, sc, "got RSS_UPDATE ramrod. CID %d",
				    cid);
			rss_raw->clear_pending(rss_raw);
			break;

		case (EVENT_RING_OPCODE_SET_MAC | BNX2X_STATE_OPEN):
		case (EVENT_RING_OPCODE_SET_MAC | BNX2X_STATE_DIAG):
		case (EVENT_RING_OPCODE_SET_MAC | BNX2X_STATE_CLOSING_WAITING_HALT):
		case (EVENT_RING_OPCODE_CLASSIFICATION_RULES | BNX2X_STATE_OPEN):
		case (EVENT_RING_OPCODE_CLASSIFICATION_RULES | BNX2X_STATE_DIAG):
		case (EVENT_RING_OPCODE_CLASSIFICATION_RULES | BNX2X_STATE_CLOSING_WAITING_HALT):
			PMD_DRV_LOG(DEBUG, sc,
				    "got (un)set mac ramrod");
			bnx2x_handle_classification_eqe(sc, elem);
			break;

		case (EVENT_RING_OPCODE_MULTICAST_RULES | BNX2X_STATE_OPEN):
		case (EVENT_RING_OPCODE_MULTICAST_RULES | BNX2X_STATE_DIAG):
		case (EVENT_RING_OPCODE_MULTICAST_RULES | BNX2X_STATE_CLOSING_WAITING_HALT):
			PMD_DRV_LOG(DEBUG, sc,
				    "got mcast ramrod");
			bnx2x_handle_mcast_eqe(sc);
			break;

		case (EVENT_RING_OPCODE_FILTERS_RULES | BNX2X_STATE_OPEN):
		case (EVENT_RING_OPCODE_FILTERS_RULES | BNX2X_STATE_DIAG):
		case (EVENT_RING_OPCODE_FILTERS_RULES | BNX2X_STATE_CLOSING_WAITING_HALT):
			PMD_DRV_LOG(DEBUG, sc,
				    "got rx_mode ramrod");
			bnx2x_handle_rx_mode_eqe(sc);
			break;

		default:
			/* unknown event log error and continue */
			PMD_DRV_LOG(INFO, sc, "Unknown EQ event %d, sc->state 0x%x",
				    elem->message.opcode, sc->state);
		}

next_spqe:
		spqe_cnt++;
	}			/* for */

	mb();
	atomic_add_acq_long(&sc->eq_spq_left, spqe_cnt);

	sc->eq_cons = sw_cons;
	sc->eq_prod = sw_prod;

	/* make sure that above mem writes were issued towards the memory */
	wmb();

	/* update producer */
	bnx2x_update_eq_prod(sc, sc->eq_prod);
}

static int bnx2x_handle_sp_tq(struct bnx2x_softc *sc)
{
	uint16_t status;
	int rc = 0;

	PMD_DRV_LOG(DEBUG, sc, "---> SP TASK <---");

	/* what work needs to be performed? */
	status = bnx2x_update_dsb_idx(sc);

	PMD_DRV_LOG(DEBUG, sc, "dsb status 0x%04x", status);

	/* HW attentions */
	if (status & BNX2X_DEF_SB_ATT_IDX) {
		PMD_DRV_LOG(DEBUG, sc, "---> ATTN INTR <---");
		bnx2x_attn_int(sc);
		status &= ~BNX2X_DEF_SB_ATT_IDX;
		rc = 1;
	}

	/* SP events: STAT_QUERY and others */
	if (status & BNX2X_DEF_SB_IDX) {
/* handle EQ completions */
		PMD_DRV_LOG(DEBUG, sc, "---> EQ INTR <---");
		bnx2x_eq_int(sc);
		bnx2x_ack_sb(sc, sc->igu_dsb_id, USTORM_ID,
			   le16toh(sc->def_idx), IGU_INT_NOP, 1);
		status &= ~BNX2X_DEF_SB_IDX;
	}

	/* if status is non zero then something went wrong */
	if (unlikely(status)) {
		PMD_DRV_LOG(INFO, sc,
			    "Got an unknown SP interrupt! (0x%04x)", status);
	}

	/* ack status block only if something was actually handled */
	bnx2x_ack_sb(sc, sc->igu_dsb_id, ATTENTION_ID,
		   le16toh(sc->def_att_idx), IGU_INT_ENABLE, 1);

	return rc;
}

static void bnx2x_handle_fp_tq(struct bnx2x_fastpath *fp)
{
	struct bnx2x_softc *sc = fp->sc;
	uint8_t more_rx = FALSE;

	/* Make sure FP is initialized */
	if (!fp->sb_running_index)
		return;

	PMD_DEBUG_PERIODIC_LOG(DEBUG, sc,
			       "---> FP TASK QUEUE (%d) <--", fp->index);

	/* update the fastpath index */
	bnx2x_update_fp_sb_idx(fp);

	if (rte_atomic32_read(&sc->scan_fp) == 1) {
		if (bnx2x_has_rx_work(fp)) {
			more_rx = bnx2x_rxeof(sc, fp);
		}

		if (more_rx) {
			/* still more work to do */
			bnx2x_handle_fp_tq(fp);
			return;
		}
		/* We have completed slow path completion, clear the flag */
		rte_atomic32_set(&sc->scan_fp, 0);
	}

	bnx2x_ack_sb(sc, fp->igu_sb_id, USTORM_ID,
		   le16toh(fp->fp_hc_idx), IGU_INT_ENABLE, 1);
}

/*
 * Legacy interrupt entry point.
 *
 * Verifies that the controller generated the interrupt and
 * then calls a separate routine to handle the various
 * interrupt causes: link, RX, and TX.
 */
int bnx2x_intr_legacy(struct bnx2x_softc *sc)
{
	struct bnx2x_fastpath *fp;
	uint32_t status, mask;
	int i, rc = 0;

	/*
	 * 0 for ustorm, 1 for cstorm
	 * the bits returned from ack_int() are 0-15
	 * bit 0 = attention status block
	 * bit 1 = fast path status block
	 * a mask of 0x2 or more = tx/rx event
	 * a mask of 1 = slow path event
	 */

	status = bnx2x_ack_int(sc);

	/* the interrupt is not for us */
	if (unlikely(status == 0)) {
		return 0;
	}

	PMD_DEBUG_PERIODIC_LOG(DEBUG, sc, "Interrupt status 0x%04x", status);
	//bnx2x_dump_status_block(sc);

	FOR_EACH_ETH_QUEUE(sc, i) {
		fp = &sc->fp[i];
		mask = (0x2 << (fp->index + CNIC_SUPPORT(sc)));
		if (status & mask) {
		/* acknowledge and disable further fastpath interrupts */
			bnx2x_ack_sb(sc, fp->igu_sb_id, USTORM_ID,
				     0, IGU_INT_DISABLE, 0);
			bnx2x_handle_fp_tq(fp);
			status &= ~mask;
		}
	}

	if (unlikely(status & 0x1)) {
		/* acknowledge and disable further slowpath interrupts */
		bnx2x_ack_sb(sc, sc->igu_dsb_id, USTORM_ID,
			     0, IGU_INT_DISABLE, 0);
		rc = bnx2x_handle_sp_tq(sc);
		status &= ~0x1;
	}

	if (unlikely(status)) {
		PMD_DRV_LOG(WARNING, sc,
			    "Unexpected fastpath status (0x%08x)!", status);
	}

	return rc;
}

static int bnx2x_init_hw_common_chip(struct bnx2x_softc *sc);
static int bnx2x_init_hw_common(struct bnx2x_softc *sc);
static int bnx2x_init_hw_port(struct bnx2x_softc *sc);
static int bnx2x_init_hw_func(struct bnx2x_softc *sc);
static void bnx2x_reset_common(struct bnx2x_softc *sc);
static void bnx2x_reset_port(struct bnx2x_softc *sc);
static void bnx2x_reset_func(struct bnx2x_softc *sc);
static int bnx2x_init_firmware(struct bnx2x_softc *sc);
static void bnx2x_release_firmware(struct bnx2x_softc *sc);

static struct
ecore_func_sp_drv_ops bnx2x_func_sp_drv = {
	.init_hw_cmn_chip = bnx2x_init_hw_common_chip,
	.init_hw_cmn = bnx2x_init_hw_common,
	.init_hw_port = bnx2x_init_hw_port,
	.init_hw_func = bnx2x_init_hw_func,

	.reset_hw_cmn = bnx2x_reset_common,
	.reset_hw_port = bnx2x_reset_port,
	.reset_hw_func = bnx2x_reset_func,

	.init_fw = bnx2x_init_firmware,
	.release_fw = bnx2x_release_firmware,
};

static void bnx2x_init_func_obj(struct bnx2x_softc *sc)
{
	sc->dmae_ready = 0;

	PMD_INIT_FUNC_TRACE(sc);

	ecore_init_func_obj(sc,
			    &sc->func_obj,
			    BNX2X_SP(sc, func_rdata),
			    (rte_iova_t)BNX2X_SP_MAPPING(sc, func_rdata),
			    BNX2X_SP(sc, func_afex_rdata),
			    (rte_iova_t)BNX2X_SP_MAPPING(sc, func_afex_rdata),
			    &bnx2x_func_sp_drv);
}

static int bnx2x_init_hw(struct bnx2x_softc *sc, uint32_t load_code)
{
	struct ecore_func_state_params func_params = { NULL };
	int rc;

	PMD_INIT_FUNC_TRACE(sc);

	/* prepare the parameters for function state transitions */
	rte_bit_relaxed_set32(RAMROD_COMP_WAIT, &func_params.ramrod_flags);

	func_params.f_obj = &sc->func_obj;
	func_params.cmd = ECORE_F_CMD_HW_INIT;

	func_params.params.hw_init.load_phase = load_code;

	/*
	 * Via a plethora of function pointers, we will eventually reach
	 * bnx2x_init_hw_common(), bnx2x_init_hw_port(), or bnx2x_init_hw_func().
	 */
	rc = ecore_func_state_change(sc, &func_params);

	return rc;
}

static void
bnx2x_fill(struct bnx2x_softc *sc, uint32_t addr, int fill, uint32_t len)
{
	uint32_t i;

	if (!(len % 4) && !(addr % 4)) {
		for (i = 0; i < len; i += 4) {
			REG_WR(sc, (addr + i), fill);
		}
	} else {
		for (i = 0; i < len; i++) {
			REG_WR8(sc, (addr + i), fill);
		}
	}
}

/* writes FP SP data to FW - data_size in dwords */
static void
bnx2x_wr_fp_sb_data(struct bnx2x_softc *sc, int fw_sb_id, uint32_t * sb_data_p,
		  uint32_t data_size)
{
	uint32_t index;

	for (index = 0; index < data_size; index++) {
		REG_WR(sc,
		       (BAR_CSTRORM_INTMEM +
			CSTORM_STATUS_BLOCK_DATA_OFFSET(fw_sb_id) +
			(sizeof(uint32_t) * index)), *(sb_data_p + index));
	}
}

static void bnx2x_zero_fp_sb(struct bnx2x_softc *sc, int fw_sb_id)
{
	struct hc_status_block_data_e2 sb_data_e2;
	struct hc_status_block_data_e1x sb_data_e1x;
	uint32_t *sb_data_p;
	uint32_t data_size = 0;

	if (!CHIP_IS_E1x(sc)) {
		memset(&sb_data_e2, 0, sizeof(struct hc_status_block_data_e2));
		sb_data_e2.common.state = SB_DISABLED;
		sb_data_e2.common.p_func.vf_valid = FALSE;
		sb_data_p = (uint32_t *) & sb_data_e2;
		data_size = (sizeof(struct hc_status_block_data_e2) /
			     sizeof(uint32_t));
	} else {
		memset(&sb_data_e1x, 0,
		       sizeof(struct hc_status_block_data_e1x));
		sb_data_e1x.common.state = SB_DISABLED;
		sb_data_e1x.common.p_func.vf_valid = FALSE;
		sb_data_p = (uint32_t *) & sb_data_e1x;
		data_size = (sizeof(struct hc_status_block_data_e1x) /
			     sizeof(uint32_t));
	}

	bnx2x_wr_fp_sb_data(sc, fw_sb_id, sb_data_p, data_size);

	bnx2x_fill(sc,
		 (BAR_CSTRORM_INTMEM + CSTORM_STATUS_BLOCK_OFFSET(fw_sb_id)), 0,
		 CSTORM_STATUS_BLOCK_SIZE);
	bnx2x_fill(sc, (BAR_CSTRORM_INTMEM + CSTORM_SYNC_BLOCK_OFFSET(fw_sb_id)),
		 0, CSTORM_SYNC_BLOCK_SIZE);
}

static void
bnx2x_wr_sp_sb_data(struct bnx2x_softc *sc,
		  struct hc_sp_status_block_data *sp_sb_data)
{
	uint32_t i;

	for (i = 0;
	     i < (sizeof(struct hc_sp_status_block_data) / sizeof(uint32_t));
	     i++) {
		REG_WR(sc,
		       (BAR_CSTRORM_INTMEM +
			CSTORM_SP_STATUS_BLOCK_DATA_OFFSET(SC_FUNC(sc)) +
			(i * sizeof(uint32_t))),
		       *((uint32_t *) sp_sb_data + i));
	}
}

static void bnx2x_zero_sp_sb(struct bnx2x_softc *sc)
{
	struct hc_sp_status_block_data sp_sb_data;

	memset(&sp_sb_data, 0, sizeof(struct hc_sp_status_block_data));

	sp_sb_data.state = SB_DISABLED;
	sp_sb_data.p_func.vf_valid = FALSE;

	bnx2x_wr_sp_sb_data(sc, &sp_sb_data);

	bnx2x_fill(sc,
		 (BAR_CSTRORM_INTMEM +
		  CSTORM_SP_STATUS_BLOCK_OFFSET(SC_FUNC(sc))),
		 0, CSTORM_SP_STATUS_BLOCK_SIZE);
	bnx2x_fill(sc,
		 (BAR_CSTRORM_INTMEM +
		  CSTORM_SP_SYNC_BLOCK_OFFSET(SC_FUNC(sc))),
		 0, CSTORM_SP_SYNC_BLOCK_SIZE);
}

static void
bnx2x_setup_ndsb_state_machine(struct hc_status_block_sm *hc_sm, int igu_sb_id,
			     int igu_seg_id)
{
	hc_sm->igu_sb_id = igu_sb_id;
	hc_sm->igu_seg_id = igu_seg_id;
	hc_sm->timer_value = 0xFF;
	hc_sm->time_to_expire = 0xFFFFFFFF;
}

static void bnx2x_map_sb_state_machines(struct hc_index_data *index_data)
{
	/* zero out state machine indices */

	/* rx indices */
	index_data[HC_INDEX_ETH_RX_CQ_CONS].flags &= ~HC_INDEX_DATA_SM_ID;

	/* tx indices */
	index_data[HC_INDEX_OOO_TX_CQ_CONS].flags &= ~HC_INDEX_DATA_SM_ID;
	index_data[HC_INDEX_ETH_TX_CQ_CONS_COS0].flags &= ~HC_INDEX_DATA_SM_ID;
	index_data[HC_INDEX_ETH_TX_CQ_CONS_COS1].flags &= ~HC_INDEX_DATA_SM_ID;
	index_data[HC_INDEX_ETH_TX_CQ_CONS_COS2].flags &= ~HC_INDEX_DATA_SM_ID;

	/* map indices */

	/* rx indices */
	index_data[HC_INDEX_ETH_RX_CQ_CONS].flags |=
	    (SM_RX_ID << HC_INDEX_DATA_SM_ID_SHIFT);

	/* tx indices */
	index_data[HC_INDEX_OOO_TX_CQ_CONS].flags |=
	    (SM_TX_ID << HC_INDEX_DATA_SM_ID_SHIFT);
	index_data[HC_INDEX_ETH_TX_CQ_CONS_COS0].flags |=
	    (SM_TX_ID << HC_INDEX_DATA_SM_ID_SHIFT);
	index_data[HC_INDEX_ETH_TX_CQ_CONS_COS1].flags |=
	    (SM_TX_ID << HC_INDEX_DATA_SM_ID_SHIFT);
	index_data[HC_INDEX_ETH_TX_CQ_CONS_COS2].flags |=
	    (SM_TX_ID << HC_INDEX_DATA_SM_ID_SHIFT);
}

static void
bnx2x_init_sb(struct bnx2x_softc *sc, rte_iova_t busaddr, int vfid,
	    uint8_t vf_valid, int fw_sb_id, int igu_sb_id)
{
	struct hc_status_block_data_e2 sb_data_e2;
	struct hc_status_block_data_e1x sb_data_e1x;
	struct hc_status_block_sm *hc_sm_p;
	uint32_t *sb_data_p;
	int igu_seg_id;
	int data_size;

	if (CHIP_INT_MODE_IS_BC(sc)) {
		igu_seg_id = HC_SEG_ACCESS_NORM;
	} else {
		igu_seg_id = IGU_SEG_ACCESS_NORM;
	}

	bnx2x_zero_fp_sb(sc, fw_sb_id);

	if (!CHIP_IS_E1x(sc)) {
		memset(&sb_data_e2, 0, sizeof(struct hc_status_block_data_e2));
		sb_data_e2.common.state = SB_ENABLED;
		sb_data_e2.common.p_func.pf_id = SC_FUNC(sc);
		sb_data_e2.common.p_func.vf_id = vfid;
		sb_data_e2.common.p_func.vf_valid = vf_valid;
		sb_data_e2.common.p_func.vnic_id = SC_VN(sc);
		sb_data_e2.common.same_igu_sb_1b = TRUE;
		sb_data_e2.common.host_sb_addr.hi = U64_HI(busaddr);
		sb_data_e2.common.host_sb_addr.lo = U64_LO(busaddr);
		hc_sm_p = sb_data_e2.common.state_machine;
		sb_data_p = (uint32_t *) & sb_data_e2;
		data_size = (sizeof(struct hc_status_block_data_e2) /
			     sizeof(uint32_t));
		bnx2x_map_sb_state_machines(sb_data_e2.index_data);
	} else {
		memset(&sb_data_e1x, 0,
		       sizeof(struct hc_status_block_data_e1x));
		sb_data_e1x.common.state = SB_ENABLED;
		sb_data_e1x.common.p_func.pf_id = SC_FUNC(sc);
		sb_data_e1x.common.p_func.vf_id = 0xff;
		sb_data_e1x.common.p_func.vf_valid = FALSE;
		sb_data_e1x.common.p_func.vnic_id = SC_VN(sc);
		sb_data_e1x.common.same_igu_sb_1b = TRUE;
		sb_data_e1x.common.host_sb_addr.hi = U64_HI(busaddr);
		sb_data_e1x.common.host_sb_addr.lo = U64_LO(busaddr);
		hc_sm_p = sb_data_e1x.common.state_machine;
		sb_data_p = (uint32_t *) & sb_data_e1x;
		data_size = (sizeof(struct hc_status_block_data_e1x) /
			     sizeof(uint32_t));
		bnx2x_map_sb_state_machines(sb_data_e1x.index_data);
	}

	bnx2x_setup_ndsb_state_machine(&hc_sm_p[SM_RX_ID], igu_sb_id, igu_seg_id);
	bnx2x_setup_ndsb_state_machine(&hc_sm_p[SM_TX_ID], igu_sb_id, igu_seg_id);

	/* write indices to HW - PCI guarantees endianity of regpairs */
	bnx2x_wr_fp_sb_data(sc, fw_sb_id, sb_data_p, data_size);
}

static uint8_t bnx2x_fp_qzone_id(struct bnx2x_fastpath *fp)
{
	if (CHIP_IS_E1x(fp->sc)) {
		return fp->cl_id + SC_PORT(fp->sc) * ETH_MAX_RX_CLIENTS_E1H;
	} else {
		return fp->cl_id;
	}
}

static uint32_t
bnx2x_rx_ustorm_prods_offset(struct bnx2x_softc *sc, struct bnx2x_fastpath *fp)
{
	uint32_t offset = BAR_USTRORM_INTMEM;

	if (IS_VF(sc)) {
		return PXP_VF_ADDR_USDM_QUEUES_START +
			(sc->acquire_resp.resc.hw_qid[fp->index] *
			 sizeof(struct ustorm_queue_zone_data));
	} else if (!CHIP_IS_E1x(sc)) {
		offset += USTORM_RX_PRODS_E2_OFFSET(fp->cl_qzone_id);
	} else {
		offset += USTORM_RX_PRODS_E1X_OFFSET(SC_PORT(sc), fp->cl_id);
	}

	return offset;
}

static void bnx2x_init_eth_fp(struct bnx2x_softc *sc, int idx)
{
	struct bnx2x_fastpath *fp = &sc->fp[idx];
	uint32_t cids[ECORE_MULTI_TX_COS] = { 0 };
	uint32_t q_type = 0;
	int cos;

	fp->sc = sc;
	fp->index = idx;

	fp->igu_sb_id = (sc->igu_base_sb + idx + CNIC_SUPPORT(sc));
	fp->fw_sb_id = (sc->base_fw_ndsb + idx + CNIC_SUPPORT(sc));

	if (CHIP_IS_E1x(sc))
		fp->cl_id = SC_L_ID(sc) + idx;
	else
/* want client ID same as IGU SB ID for non-E1 */
		fp->cl_id = fp->igu_sb_id;
	fp->cl_qzone_id = bnx2x_fp_qzone_id(fp);

	/* setup sb indices */
	if (!CHIP_IS_E1x(sc)) {
		fp->sb_index_values = fp->status_block.e2_sb->sb.index_values;
		fp->sb_running_index = fp->status_block.e2_sb->sb.running_index;
	} else {
		fp->sb_index_values = fp->status_block.e1x_sb->sb.index_values;
		fp->sb_running_index =
		    fp->status_block.e1x_sb->sb.running_index;
	}

	/* init shortcut */
	fp->ustorm_rx_prods_offset = bnx2x_rx_ustorm_prods_offset(sc, fp);

	fp->rx_cq_cons_sb = &fp->sb_index_values[HC_INDEX_ETH_RX_CQ_CONS];

	for (cos = 0; cos < sc->max_cos; cos++) {
		cids[cos] = idx;
	}
	fp->tx_cons_sb = &fp->sb_index_values[HC_INDEX_ETH_TX_CQ_CONS_COS0];

	/* nothing more for a VF to do */
	if (IS_VF(sc)) {
		return;
	}

	bnx2x_init_sb(sc, fp->sb_dma.paddr, BNX2X_VF_ID_INVALID, FALSE,
		    fp->fw_sb_id, fp->igu_sb_id);

	bnx2x_update_fp_sb_idx(fp);

	/* Configure Queue State object */
	rte_bit_relaxed_set32(ECORE_Q_TYPE_HAS_RX, &q_type);
	rte_bit_relaxed_set32(ECORE_Q_TYPE_HAS_TX, &q_type);

	ecore_init_queue_obj(sc,
			     &sc->sp_objs[idx].q_obj,
			     fp->cl_id,
			     cids,
			     sc->max_cos,
			     SC_FUNC(sc),
			     BNX2X_SP(sc, q_rdata),
			     (rte_iova_t)BNX2X_SP_MAPPING(sc, q_rdata),
			     q_type);

	/* configure classification DBs */
	ecore_init_mac_obj(sc,
			   &sc->sp_objs[idx].mac_obj,
			   fp->cl_id,
			   idx,
			   SC_FUNC(sc),
			   BNX2X_SP(sc, mac_rdata),
			   (rte_iova_t)BNX2X_SP_MAPPING(sc, mac_rdata),
			   ECORE_FILTER_MAC_PENDING, &sc->sp_state,
			   ECORE_OBJ_TYPE_RX_TX, &sc->macs_pool);
}

static void
bnx2x_update_rx_prod(struct bnx2x_softc *sc, struct bnx2x_fastpath *fp,
		   uint16_t rx_bd_prod, uint16_t rx_cq_prod)
{
	struct ustorm_eth_rx_producers rx_prods;
	uint32_t i;

	memset(&rx_prods, 0, sizeof(rx_prods));

	/* update producers */
	rx_prods.bd_prod = rx_bd_prod;
	rx_prods.cqe_prod = rx_cq_prod;

	/*
	 * Make sure that the BD and SGE data is updated before updating the
	 * producers since FW might read the BD/SGE right after the producer
	 * is updated.
	 * This is only applicable for weak-ordered memory model archs such
	 * as IA-64. The following barrier is also mandatory since FW will
	 * assumes BDs must have buffers.
	 */
	wmb();

	for (i = 0; i < (sizeof(rx_prods) / 4); i++) {
		REG_WR(sc, (fp->ustorm_rx_prods_offset + (i * 4)),
		       ((uint32_t *)&rx_prods)[i]);
	}

	wmb();			/* keep prod updates ordered */
}

static void bnx2x_init_rx_rings(struct bnx2x_softc *sc)
{
	struct bnx2x_fastpath *fp;
	int i;
	struct bnx2x_rx_queue *rxq;

	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];
		rxq = sc->rx_queues[fp->index];
		if (!rxq) {
			PMD_RX_LOG(ERR, "RX queue is NULL");
			return;
		}

		rxq->rx_bd_head = 0;
		rxq->rx_bd_tail = rxq->nb_rx_desc;
		rxq->rx_cq_head = 0;
		rxq->rx_cq_tail = TOTAL_RCQ_ENTRIES(rxq);
		*fp->rx_cq_cons_sb = 0;

		/*
		 * Activate the BD ring...
		 * Warning, this will generate an interrupt (to the TSTORM)
		 * so this can only be done after the chip is initialized
		 */
		bnx2x_update_rx_prod(sc, fp, rxq->rx_bd_tail, rxq->rx_cq_tail);

		if (i != 0) {
			continue;
		}
	}
}

static void bnx2x_init_tx_ring_one(struct bnx2x_fastpath *fp)
{
	struct bnx2x_tx_queue *txq = fp->sc->tx_queues[fp->index];

	fp->tx_db.data.header.header = 1 << DOORBELL_HDR_DB_TYPE_SHIFT;
	fp->tx_db.data.zero_fill1 = 0;
	fp->tx_db.data.prod = 0;

	if (!txq) {
		PMD_TX_LOG(ERR, "ERROR: TX queue is NULL");
		return;
	}

	txq->tx_pkt_tail = 0;
	txq->tx_pkt_head = 0;
	txq->tx_bd_tail = 0;
	txq->tx_bd_head = 0;
}

static void bnx2x_init_tx_rings(struct bnx2x_softc *sc)
{
	int i;

	for (i = 0; i < sc->num_queues; i++) {
		bnx2x_init_tx_ring_one(&sc->fp[i]);
	}
}

static void bnx2x_init_def_sb(struct bnx2x_softc *sc)
{
	struct host_sp_status_block *def_sb = sc->def_sb;
	rte_iova_t mapping = sc->def_sb_dma.paddr;
	int igu_sp_sb_index;
	int igu_seg_id;
	int port = SC_PORT(sc);
	int func = SC_FUNC(sc);
	int reg_offset, reg_offset_en5;
	uint64_t section;
	int index, sindex;
	struct hc_sp_status_block_data sp_sb_data;

	memset(&sp_sb_data, 0, sizeof(struct hc_sp_status_block_data));

	if (CHIP_INT_MODE_IS_BC(sc)) {
		igu_sp_sb_index = DEF_SB_IGU_ID;
		igu_seg_id = HC_SEG_ACCESS_DEF;
	} else {
		igu_sp_sb_index = sc->igu_dsb_id;
		igu_seg_id = IGU_SEG_ACCESS_DEF;
	}

	/* attentions */
	section = ((uint64_t) mapping +
		   offsetof(struct host_sp_status_block, atten_status_block));
	def_sb->atten_status_block.status_block_id = igu_sp_sb_index;
	sc->attn_state = 0;

	reg_offset = (port) ? MISC_REG_AEU_ENABLE1_FUNC_1_OUT_0 :
	    MISC_REG_AEU_ENABLE1_FUNC_0_OUT_0;

	reg_offset_en5 = (port) ? MISC_REG_AEU_ENABLE5_FUNC_1_OUT_0 :
	    MISC_REG_AEU_ENABLE5_FUNC_0_OUT_0;

	for (index = 0; index < MAX_DYNAMIC_ATTN_GRPS; index++) {
/* take care of sig[0]..sig[4] */
		for (sindex = 0; sindex < 4; sindex++) {
			sc->attn_group[index].sig[sindex] =
			    REG_RD(sc,
				   (reg_offset + (sindex * 0x4) +
				    (0x10 * index)));
		}

		if (!CHIP_IS_E1x(sc)) {
			/*
			 * enable5 is separate from the rest of the registers,
			 * and the address skip is 4 and not 16 between the
			 * different groups
			 */
			sc->attn_group[index].sig[4] =
			    REG_RD(sc, (reg_offset_en5 + (0x4 * index)));
		} else {
			sc->attn_group[index].sig[4] = 0;
		}
	}

	if (sc->devinfo.int_block == INT_BLOCK_HC) {
		reg_offset =
		    port ? HC_REG_ATTN_MSG1_ADDR_L : HC_REG_ATTN_MSG0_ADDR_L;
		REG_WR(sc, reg_offset, U64_LO(section));
		REG_WR(sc, (reg_offset + 4), U64_HI(section));
	} else if (!CHIP_IS_E1x(sc)) {
		REG_WR(sc, IGU_REG_ATTN_MSG_ADDR_L, U64_LO(section));
		REG_WR(sc, IGU_REG_ATTN_MSG_ADDR_H, U64_HI(section));
	}

	section = ((uint64_t) mapping +
		   offsetof(struct host_sp_status_block, sp_sb));

	bnx2x_zero_sp_sb(sc);

	/* PCI guarantees endianity of regpair */
	sp_sb_data.state = SB_ENABLED;
	sp_sb_data.host_sb_addr.lo = U64_LO(section);
	sp_sb_data.host_sb_addr.hi = U64_HI(section);
	sp_sb_data.igu_sb_id = igu_sp_sb_index;
	sp_sb_data.igu_seg_id = igu_seg_id;
	sp_sb_data.p_func.pf_id = func;
	sp_sb_data.p_func.vnic_id = SC_VN(sc);
	sp_sb_data.p_func.vf_id = 0xff;

	bnx2x_wr_sp_sb_data(sc, &sp_sb_data);

	bnx2x_ack_sb(sc, sc->igu_dsb_id, USTORM_ID, 0, IGU_INT_ENABLE, 0);
}

static void bnx2x_init_sp_ring(struct bnx2x_softc *sc)
{
	atomic_store_rel_long(&sc->cq_spq_left, MAX_SPQ_PENDING);
	sc->spq_prod_idx = 0;
	sc->dsb_sp_prod =
	    &sc->def_sb->sp_sb.index_values[HC_SP_INDEX_ETH_DEF_CONS];
	sc->spq_prod_bd = sc->spq;
	sc->spq_last_bd = (sc->spq_prod_bd + MAX_SP_DESC_CNT);
}

static void bnx2x_init_eq_ring(struct bnx2x_softc *sc)
{
	union event_ring_elem *elem;
	int i;

	for (i = 1; i <= NUM_EQ_PAGES; i++) {
		elem = &sc->eq[EQ_DESC_CNT_PAGE * i - 1];

		elem->next_page.addr.hi = htole32(U64_HI(sc->eq_dma.paddr +
							 BNX2X_PAGE_SIZE *
							 (i % NUM_EQ_PAGES)));
		elem->next_page.addr.lo = htole32(U64_LO(sc->eq_dma.paddr +
							 BNX2X_PAGE_SIZE *
							 (i % NUM_EQ_PAGES)));
	}

	sc->eq_cons = 0;
	sc->eq_prod = NUM_EQ_DESC;
	sc->eq_cons_sb = &sc->def_sb->sp_sb.index_values[HC_SP_INDEX_EQ_CONS];

	atomic_store_rel_long(&sc->eq_spq_left,
			      (min((MAX_SP_DESC_CNT - MAX_SPQ_PENDING),
				   NUM_EQ_DESC) - 1));
}

static void bnx2x_init_internal_common(struct bnx2x_softc *sc)
{
	int i;

	/*
	 * Zero this manually as its initialization is currently missing
	 * in the initTool.
	 */
	for (i = 0; i < (USTORM_AGG_DATA_SIZE >> 2); i++) {
		REG_WR(sc,
		       (BAR_USTRORM_INTMEM + USTORM_AGG_DATA_OFFSET + (i * 4)),
		       0);
	}

	if (!CHIP_IS_E1x(sc)) {
		REG_WR8(sc, (BAR_CSTRORM_INTMEM + CSTORM_IGU_MODE_OFFSET),
			CHIP_INT_MODE_IS_BC(sc) ? HC_IGU_BC_MODE :
			HC_IGU_NBC_MODE);
	}
}

static void bnx2x_init_internal(struct bnx2x_softc *sc, uint32_t load_code)
{
	switch (load_code) {
	case FW_MSG_CODE_DRV_LOAD_COMMON:
	case FW_MSG_CODE_DRV_LOAD_COMMON_CHIP:
		bnx2x_init_internal_common(sc);
		/* no break */

	case FW_MSG_CODE_DRV_LOAD_PORT:
		/* nothing to do */
		/* no break */

	case FW_MSG_CODE_DRV_LOAD_FUNCTION:
		/* internal memory per function is initialized inside bnx2x_pf_init */
		break;

	default:
		PMD_DRV_LOG(NOTICE, sc, "Unknown load_code (0x%x) from MCP",
			    load_code);
		break;
	}
}

static void
storm_memset_func_cfg(struct bnx2x_softc *sc,
		      struct tstorm_eth_function_common_config *tcfg,
		      uint16_t abs_fid)
{
	uint32_t addr;
	size_t size;

	addr = (BAR_TSTRORM_INTMEM +
		TSTORM_FUNCTION_COMMON_CONFIG_OFFSET(abs_fid));
	size = sizeof(struct tstorm_eth_function_common_config);
	ecore_storm_memset_struct(sc, addr, size, (uint32_t *) tcfg);
}

static void bnx2x_func_init(struct bnx2x_softc *sc, struct bnx2x_func_init_params *p)
{
	struct tstorm_eth_function_common_config tcfg = { 0 };

	if (CHIP_IS_E1x(sc)) {
		storm_memset_func_cfg(sc, &tcfg, p->func_id);
	}

	/* Enable the function in the FW */
	storm_memset_vf_to_pf(sc, p->func_id, p->pf_id);
	storm_memset_func_en(sc, p->func_id, 1);

	/* spq */
	if (p->func_flgs & FUNC_FLG_SPQ) {
		storm_memset_spq_addr(sc, p->spq_map, p->func_id);
		REG_WR(sc,
		       (XSEM_REG_FAST_MEMORY +
			XSTORM_SPQ_PROD_OFFSET(p->func_id)), p->spq_prod);
	}
}

/*
 * Calculates the sum of vn_min_rates.
 * It's needed for further normalizing of the min_rates.
 * Returns:
 *   sum of vn_min_rates.
 *     or
 *   0 - if all the min_rates are 0.
 * In the later case fainess algorithm should be deactivated.
 * If all min rates are not zero then those that are zeroes will be set to 1.
 */
static void bnx2x_calc_vn_min(struct bnx2x_softc *sc, struct cmng_init_input *input)
{
	uint32_t vn_cfg;
	uint32_t vn_min_rate;
	int all_zero = 1;
	int vn;

	for (vn = VN_0; vn < SC_MAX_VN_NUM(sc); vn++) {
		vn_cfg = sc->devinfo.mf_info.mf_config[vn];
		vn_min_rate = (((vn_cfg & FUNC_MF_CFG_MIN_BW_MASK) >>
				FUNC_MF_CFG_MIN_BW_SHIFT) * 100);

		if (vn_cfg & FUNC_MF_CFG_FUNC_HIDE) {
			/* skip hidden VNs */
			vn_min_rate = 0;
		} else if (!vn_min_rate) {
			/* If min rate is zero - set it to 100 */
			vn_min_rate = DEF_MIN_RATE;
		} else {
			all_zero = 0;
		}

		input->vnic_min_rate[vn] = vn_min_rate;
	}

	/* if ETS or all min rates are zeros - disable fairness */
	if (all_zero) {
		input->flags.cmng_enables &= ~CMNG_FLAGS_PER_PORT_FAIRNESS_VN;
	} else {
		input->flags.cmng_enables |= CMNG_FLAGS_PER_PORT_FAIRNESS_VN;
	}
}

static uint16_t
bnx2x_extract_max_cfg(__rte_unused struct bnx2x_softc *sc, uint32_t mf_cfg)
{
	uint16_t max_cfg = ((mf_cfg & FUNC_MF_CFG_MAX_BW_MASK) >>
			    FUNC_MF_CFG_MAX_BW_SHIFT);

	if (!max_cfg) {
		PMD_DRV_LOG(DEBUG, sc,
			    "Max BW configured to 0 - using 100 instead");
		max_cfg = 100;
	}

	return max_cfg;
}

static void
bnx2x_calc_vn_max(struct bnx2x_softc *sc, int vn, struct cmng_init_input *input)
{
	uint16_t vn_max_rate;
	uint32_t vn_cfg = sc->devinfo.mf_info.mf_config[vn];
	uint32_t max_cfg;

	if (vn_cfg & FUNC_MF_CFG_FUNC_HIDE) {
		vn_max_rate = 0;
	} else {
		max_cfg = bnx2x_extract_max_cfg(sc, vn_cfg);

		if (IS_MF_SI(sc)) {
			/* max_cfg in percents of linkspeed */
			vn_max_rate =
			    ((sc->link_vars.line_speed * max_cfg) / 100);
		} else {	/* SD modes */
			/* max_cfg is absolute in 100Mb units */
			vn_max_rate = (max_cfg * 100);
		}
	}

	input->vnic_max_rate[vn] = vn_max_rate;
}

static void
bnx2x_cmng_fns_init(struct bnx2x_softc *sc, uint8_t read_cfg, uint8_t cmng_type)
{
	struct cmng_init_input input;
	int vn;

	memset(&input, 0, sizeof(struct cmng_init_input));

	input.port_rate = sc->link_vars.line_speed;

	if (cmng_type == CMNG_FNS_MINMAX) {
/* read mf conf from shmem */
		if (read_cfg) {
			bnx2x_read_mf_cfg(sc);
		}

/* get VN min rate and enable fairness if not 0 */
		bnx2x_calc_vn_min(sc, &input);

/* get VN max rate */
		if (sc->port.pmf) {
			for (vn = VN_0; vn < SC_MAX_VN_NUM(sc); vn++) {
				bnx2x_calc_vn_max(sc, vn, &input);
			}
		}

/* always enable rate shaping and fairness */
		input.flags.cmng_enables |= CMNG_FLAGS_PER_PORT_RATE_SHAPING_VN;

		ecore_init_cmng(&input, &sc->cmng);
		return;
	}
}

static int bnx2x_get_cmng_fns_mode(struct bnx2x_softc *sc)
{
	if (CHIP_REV_IS_SLOW(sc)) {
		return CMNG_FNS_NONE;
	}

	if (IS_MF(sc)) {
		return CMNG_FNS_MINMAX;
	}

	return CMNG_FNS_NONE;
}

static void
storm_memset_cmng(struct bnx2x_softc *sc, struct cmng_init *cmng, uint8_t port)
{
	int vn;
	int func;
	uint32_t addr;
	size_t size;

	addr = (BAR_XSTRORM_INTMEM + XSTORM_CMNG_PER_PORT_VARS_OFFSET(port));
	size = sizeof(struct cmng_struct_per_port);
	ecore_storm_memset_struct(sc, addr, size, (uint32_t *) & cmng->port);

	for (vn = VN_0; vn < SC_MAX_VN_NUM(sc); vn++) {
		func = func_by_vn(sc, vn);

		addr = (BAR_XSTRORM_INTMEM +
			XSTORM_RATE_SHAPING_PER_VN_VARS_OFFSET(func));
		size = sizeof(struct rate_shaping_vars_per_vn);
		ecore_storm_memset_struct(sc, addr, size,
					  (uint32_t *) & cmng->
					  vnic.vnic_max_rate[vn]);

		addr = (BAR_XSTRORM_INTMEM +
			XSTORM_FAIRNESS_PER_VN_VARS_OFFSET(func));
		size = sizeof(struct fairness_vars_per_vn);
		ecore_storm_memset_struct(sc, addr, size,
					  (uint32_t *) & cmng->
					  vnic.vnic_min_rate[vn]);
	}
}

static void bnx2x_pf_init(struct bnx2x_softc *sc)
{
	struct bnx2x_func_init_params func_init;
	struct event_ring_data eq_data;
	uint16_t flags;

	memset(&eq_data, 0, sizeof(struct event_ring_data));
	memset(&func_init, 0, sizeof(struct bnx2x_func_init_params));

	if (!CHIP_IS_E1x(sc)) {
/* reset IGU PF statistics: MSIX + ATTN */
/* PF */
		REG_WR(sc,
		       (IGU_REG_STATISTIC_NUM_MESSAGE_SENT +
			(BNX2X_IGU_STAS_MSG_VF_CNT * 4) +
			((CHIP_IS_MODE_4_PORT(sc) ? SC_FUNC(sc) : SC_VN(sc)) *
			 4)), 0);
/* ATTN */
		REG_WR(sc,
		       (IGU_REG_STATISTIC_NUM_MESSAGE_SENT +
			(BNX2X_IGU_STAS_MSG_VF_CNT * 4) +
			(BNX2X_IGU_STAS_MSG_PF_CNT * 4) +
			((CHIP_IS_MODE_4_PORT(sc) ? SC_FUNC(sc) : SC_VN(sc)) *
			 4)), 0);
	}

	/* function setup flags */
	flags = (FUNC_FLG_STATS | FUNC_FLG_LEADING | FUNC_FLG_SPQ);

	func_init.func_flgs = flags;
	func_init.pf_id = SC_FUNC(sc);
	func_init.func_id = SC_FUNC(sc);
	func_init.spq_map = sc->spq_dma.paddr;
	func_init.spq_prod = sc->spq_prod_idx;

	bnx2x_func_init(sc, &func_init);

	memset(&sc->cmng, 0, sizeof(struct cmng_struct_per_port));

	/*
	 * Congestion management values depend on the link rate.
	 * There is no active link so initial link rate is set to 10Gbps.
	 * When the link comes up the congestion management values are
	 * re-calculated according to the actual link rate.
	 */
	sc->link_vars.line_speed = SPEED_10000;
	bnx2x_cmng_fns_init(sc, TRUE, bnx2x_get_cmng_fns_mode(sc));

	/* Only the PMF sets the HW */
	if (sc->port.pmf) {
		storm_memset_cmng(sc, &sc->cmng, SC_PORT(sc));
	}

	/* init Event Queue - PCI bus guarantees correct endainity */
	eq_data.base_addr.hi = U64_HI(sc->eq_dma.paddr);
	eq_data.base_addr.lo = U64_LO(sc->eq_dma.paddr);
	eq_data.producer = sc->eq_prod;
	eq_data.index_id = HC_SP_INDEX_EQ_CONS;
	eq_data.sb_id = DEF_SB_ID;
	storm_memset_eq_data(sc, &eq_data, SC_FUNC(sc));
}

static void bnx2x_hc_int_enable(struct bnx2x_softc *sc)
{
	int port = SC_PORT(sc);
	uint32_t addr = (port) ? HC_REG_CONFIG_1 : HC_REG_CONFIG_0;
	uint32_t val = REG_RD(sc, addr);
	uint8_t msix = (sc->interrupt_mode == INTR_MODE_MSIX)
	    || (sc->interrupt_mode == INTR_MODE_SINGLE_MSIX);
	uint8_t single_msix = (sc->interrupt_mode == INTR_MODE_SINGLE_MSIX);
	uint8_t msi = (sc->interrupt_mode == INTR_MODE_MSI);

	if (msix) {
		val &= ~(HC_CONFIG_0_REG_SINGLE_ISR_EN_0 |
			 HC_CONFIG_0_REG_INT_LINE_EN_0);
		val |= (HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0 |
			HC_CONFIG_0_REG_ATTN_BIT_EN_0);
		if (single_msix) {
			val |= HC_CONFIG_0_REG_SINGLE_ISR_EN_0;
		}
	} else if (msi) {
		val &= ~HC_CONFIG_0_REG_INT_LINE_EN_0;
		val |= (HC_CONFIG_0_REG_SINGLE_ISR_EN_0 |
			HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0 |
			HC_CONFIG_0_REG_ATTN_BIT_EN_0);
	} else {
		val |= (HC_CONFIG_0_REG_SINGLE_ISR_EN_0 |
			HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0 |
			HC_CONFIG_0_REG_INT_LINE_EN_0 |
			HC_CONFIG_0_REG_ATTN_BIT_EN_0);

		REG_WR(sc, addr, val);

		val &= ~HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0;
	}

	REG_WR(sc, addr, val);

	/* ensure that HC_CONFIG is written before leading/trailing edge config */
	mb();

	/* init leading/trailing edge */
	if (IS_MF(sc)) {
		val = (0xee0f | (1 << (SC_VN(sc) + 4)));
		if (sc->port.pmf) {
			/* enable nig and gpio3 attention */
			val |= 0x1100;
		}
	} else {
		val = 0xffff;
	}

	REG_WR(sc, (HC_REG_TRAILING_EDGE_0 + port * 8), val);
	REG_WR(sc, (HC_REG_LEADING_EDGE_0 + port * 8), val);

	/* make sure that interrupts are indeed enabled from here on */
	mb();
}

static void bnx2x_igu_int_enable(struct bnx2x_softc *sc)
{
	uint32_t val;
	uint8_t msix = (sc->interrupt_mode == INTR_MODE_MSIX)
	    || (sc->interrupt_mode == INTR_MODE_SINGLE_MSIX);
	uint8_t single_msix = (sc->interrupt_mode == INTR_MODE_SINGLE_MSIX);
	uint8_t msi = (sc->interrupt_mode == INTR_MODE_MSI);

	val = REG_RD(sc, IGU_REG_PF_CONFIGURATION);

	if (msix) {
		val &= ~(IGU_PF_CONF_INT_LINE_EN | IGU_PF_CONF_SINGLE_ISR_EN);
		val |= (IGU_PF_CONF_MSI_MSIX_EN | IGU_PF_CONF_ATTN_BIT_EN);
		if (single_msix) {
			val |= IGU_PF_CONF_SINGLE_ISR_EN;
		}
	} else if (msi) {
		val &= ~IGU_PF_CONF_INT_LINE_EN;
		val |= (IGU_PF_CONF_MSI_MSIX_EN |
			IGU_PF_CONF_ATTN_BIT_EN | IGU_PF_CONF_SINGLE_ISR_EN);
	} else {
		val &= ~IGU_PF_CONF_MSI_MSIX_EN;
		val |= (IGU_PF_CONF_INT_LINE_EN |
			IGU_PF_CONF_ATTN_BIT_EN | IGU_PF_CONF_SINGLE_ISR_EN);
	}

	/* clean previous status - need to configure igu prior to ack */
	if ((!msix) || single_msix) {
		REG_WR(sc, IGU_REG_PF_CONFIGURATION, val);
		bnx2x_ack_int(sc);
	}

	val |= IGU_PF_CONF_FUNC_EN;

	PMD_DRV_LOG(DEBUG, sc, "write 0x%x to IGU mode %s",
		    val, ((msix) ? "MSI-X" : ((msi) ? "MSI" : "INTx")));

	REG_WR(sc, IGU_REG_PF_CONFIGURATION, val);

	mb();

	/* init leading/trailing edge */
	if (IS_MF(sc)) {
		val = (0xee0f | (1 << (SC_VN(sc) + 4)));
		if (sc->port.pmf) {
			/* enable nig and gpio3 attention */
			val |= 0x1100;
		}
	} else {
		val = 0xffff;
	}

	REG_WR(sc, IGU_REG_TRAILING_EDGE_LATCH, val);
	REG_WR(sc, IGU_REG_LEADING_EDGE_LATCH, val);

	/* make sure that interrupts are indeed enabled from here on */
	mb();
}

static void bnx2x_int_enable(struct bnx2x_softc *sc)
{
	if (sc->devinfo.int_block == INT_BLOCK_HC) {
		bnx2x_hc_int_enable(sc);
	} else {
		bnx2x_igu_int_enable(sc);
	}
}

static void bnx2x_hc_int_disable(struct bnx2x_softc *sc)
{
	int port = SC_PORT(sc);
	uint32_t addr = (port) ? HC_REG_CONFIG_1 : HC_REG_CONFIG_0;
	uint32_t val = REG_RD(sc, addr);

	val &= ~(HC_CONFIG_0_REG_SINGLE_ISR_EN_0 |
		 HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0 |
		 HC_CONFIG_0_REG_INT_LINE_EN_0 | HC_CONFIG_0_REG_ATTN_BIT_EN_0);
	/* flush all outstanding writes */
	mb();

	REG_WR(sc, addr, val);
	if (REG_RD(sc, addr) != val) {
		PMD_DRV_LOG(ERR, sc, "proper val not read from HC IGU!");
	}
}

static void bnx2x_igu_int_disable(struct bnx2x_softc *sc)
{
	uint32_t val = REG_RD(sc, IGU_REG_PF_CONFIGURATION);

	val &= ~(IGU_PF_CONF_MSI_MSIX_EN |
		 IGU_PF_CONF_INT_LINE_EN | IGU_PF_CONF_ATTN_BIT_EN);

	PMD_DRV_LOG(DEBUG, sc, "write %x to IGU", val);

	/* flush all outstanding writes */
	mb();

	REG_WR(sc, IGU_REG_PF_CONFIGURATION, val);
	if (REG_RD(sc, IGU_REG_PF_CONFIGURATION) != val) {
		PMD_DRV_LOG(ERR, sc, "proper val not read from IGU!");
	}
}

static void bnx2x_int_disable(struct bnx2x_softc *sc)
{
	if (sc->devinfo.int_block == INT_BLOCK_HC) {
		bnx2x_hc_int_disable(sc);
	} else {
		bnx2x_igu_int_disable(sc);
	}
}

static void bnx2x_nic_init(struct bnx2x_softc *sc, int load_code)
{
	int i;

	PMD_INIT_FUNC_TRACE(sc);

	for (i = 0; i < sc->num_queues; i++) {
		bnx2x_init_eth_fp(sc, i);
	}

	rmb();			/* ensure status block indices were read */

	bnx2x_init_rx_rings(sc);
	bnx2x_init_tx_rings(sc);

	if (IS_VF(sc)) {
		bnx2x_memset_stats(sc);
		return;
	}

	/* initialize MOD_ABS interrupts */
	elink_init_mod_abs_int(sc, &sc->link_vars,
			       sc->devinfo.chip_id,
			       sc->devinfo.shmem_base,
			       sc->devinfo.shmem2_base, SC_PORT(sc));

	bnx2x_init_def_sb(sc);
	bnx2x_update_dsb_idx(sc);
	bnx2x_init_sp_ring(sc);
	bnx2x_init_eq_ring(sc);
	bnx2x_init_internal(sc, load_code);
	bnx2x_pf_init(sc);
	bnx2x_stats_init(sc);

	/* flush all before enabling interrupts */
	mb();

	bnx2x_int_enable(sc);

	/* check for SPIO5 */
	bnx2x_attn_int_deasserted0(sc,
				 REG_RD(sc,
					(MISC_REG_AEU_AFTER_INVERT_1_FUNC_0 +
					 SC_PORT(sc) * 4)) &
				 AEU_INPUTS_ATTN_BITS_SPIO5);
}

static void bnx2x_init_objs(struct bnx2x_softc *sc)
{
	/* mcast rules must be added to tx if tx switching is enabled */
	ecore_obj_type o_type;
	if (sc->flags & BNX2X_TX_SWITCHING)
		o_type = ECORE_OBJ_TYPE_RX_TX;
	else
		o_type = ECORE_OBJ_TYPE_RX;

	/* RX_MODE controlling object */
	ecore_init_rx_mode_obj(sc, &sc->rx_mode_obj);

	/* multicast configuration controlling object */
	ecore_init_mcast_obj(sc,
			     &sc->mcast_obj,
			     sc->fp[0].cl_id,
			     sc->fp[0].index,
			     SC_FUNC(sc),
			     SC_FUNC(sc),
			     BNX2X_SP(sc, mcast_rdata),
			     (rte_iova_t)BNX2X_SP_MAPPING(sc, mcast_rdata),
			     ECORE_FILTER_MCAST_PENDING,
			     &sc->sp_state, o_type);

	/* Setup CAM credit pools */
	ecore_init_mac_credit_pool(sc,
				   &sc->macs_pool,
				   SC_FUNC(sc),
				   CHIP_IS_E1x(sc) ? VNICS_PER_PORT(sc) :
				   VNICS_PER_PATH(sc));

	ecore_init_vlan_credit_pool(sc,
				    &sc->vlans_pool,
				    SC_ABS_FUNC(sc) >> 1,
				    CHIP_IS_E1x(sc) ? VNICS_PER_PORT(sc) :
				    VNICS_PER_PATH(sc));

	/* RSS configuration object */
	ecore_init_rss_config_obj(sc, &sc->rss_conf_obj, sc->fp->cl_id,
				  sc->fp->index, SC_FUNC(sc), SC_FUNC(sc),
				  BNX2X_SP(sc, rss_rdata),
				  (rte_iova_t)BNX2X_SP_MAPPING(sc, rss_rdata),
				  ECORE_FILTER_RSS_CONF_PENDING, &sc->sp_state,
				  ECORE_OBJ_TYPE_RX);
}

/*
 * Initialize the function. This must be called before sending CLIENT_SETUP
 * for the first client.
 */
static int bnx2x_func_start(struct bnx2x_softc *sc)
{
	struct ecore_func_state_params func_params = { NULL };
	struct ecore_func_start_params *start_params =
	    &func_params.params.start;

	/* Prepare parameters for function state transitions */
	rte_bit_relaxed_set32(RAMROD_COMP_WAIT, &func_params.ramrod_flags);

	func_params.f_obj = &sc->func_obj;
	func_params.cmd = ECORE_F_CMD_START;

	/* Function parameters */
	start_params->mf_mode = sc->devinfo.mf_info.mf_mode;
	start_params->sd_vlan_tag = OVLAN(sc);

	if (CHIP_IS_E2(sc) || CHIP_IS_E3(sc)) {
		start_params->network_cos_mode = STATIC_COS;
	} else {		/* CHIP_IS_E1X */
		start_params->network_cos_mode = FW_WRR;
	}

	return ecore_func_state_change(sc, &func_params);
}

static int bnx2x_set_power_state(struct bnx2x_softc *sc, uint8_t state)
{
	uint16_t pmcsr;

	/* If there is no power capability, silently succeed */
	if (!(sc->devinfo.pcie_cap_flags & BNX2X_PM_CAPABLE_FLAG)) {
		PMD_DRV_LOG(INFO, sc, "No power capability");
		return 0;
	}

	pci_read(sc, (sc->devinfo.pcie_pm_cap_reg + PCIR_POWER_STATUS), &pmcsr,
		 2);

	switch (state) {
	case PCI_PM_D0:
		pci_write_word(sc,
			       (sc->devinfo.pcie_pm_cap_reg +
				PCIR_POWER_STATUS),
			       ((pmcsr & ~PCIM_PSTAT_DMASK) | PCIM_PSTAT_PME));

		if (pmcsr & PCIM_PSTAT_DMASK) {
			/* delay required during transition out of D3hot */
			DELAY(20000);
		}

		break;

	case PCI_PM_D3hot:
		/* don't shut down the power for emulation and FPGA */
		if (CHIP_REV_IS_SLOW(sc)) {
			return 0;
		}

		pmcsr &= ~PCIM_PSTAT_DMASK;
		pmcsr |= PCIM_PSTAT_D3;

		if (sc->wol) {
			pmcsr |= PCIM_PSTAT_PMEENABLE;
		}

		pci_write_long(sc,
			       (sc->devinfo.pcie_pm_cap_reg +
				PCIR_POWER_STATUS), pmcsr);

		/*
		 * No more memory access after this point until device is brought back
		 * to D0 state.
		 */
		break;

	default:
		PMD_DRV_LOG(NOTICE, sc, "Can't support PCI power state = %d",
			    state);
		return -1;
	}

	return 0;
}

/* return true if succeeded to acquire the lock */
static uint8_t bnx2x_trylock_hw_lock(struct bnx2x_softc *sc, uint32_t resource)
{
	uint32_t lock_status;
	uint32_t resource_bit = (1 << resource);
	int func = SC_FUNC(sc);
	uint32_t hw_lock_control_reg;

	/* Validating that the resource is within range */
	if (resource > HW_LOCK_MAX_RESOURCE_VALUE) {
		PMD_DRV_LOG(INFO, sc,
			    "resource(0x%x) > HW_LOCK_MAX_RESOURCE_VALUE(0x%x)",
			    resource, HW_LOCK_MAX_RESOURCE_VALUE);
		return FALSE;
	}

	if (func <= 5) {
		hw_lock_control_reg = (MISC_REG_DRIVER_CONTROL_1 + func * 8);
	} else {
		hw_lock_control_reg =
		    (MISC_REG_DRIVER_CONTROL_7 + (func - 6) * 8);
	}

	/* try to acquire the lock */
	REG_WR(sc, hw_lock_control_reg + 4, resource_bit);
	lock_status = REG_RD(sc, hw_lock_control_reg);
	if (lock_status & resource_bit) {
		return TRUE;
	}

	PMD_DRV_LOG(NOTICE, sc, "Failed to get a resource lock 0x%x", resource);

	return FALSE;
}

/*
 * Get the recovery leader resource id according to the engine this function
 * belongs to. Currently only only 2 engines is supported.
 */
static int bnx2x_get_leader_lock_resource(struct bnx2x_softc *sc)
{
	if (SC_PATH(sc)) {
		return HW_LOCK_RESOURCE_RECOVERY_LEADER_1;
	} else {
		return HW_LOCK_RESOURCE_RECOVERY_LEADER_0;
	}
}

/* try to acquire a leader lock for current engine */
static uint8_t bnx2x_trylock_leader_lock(struct bnx2x_softc *sc)
{
	return bnx2x_trylock_hw_lock(sc, bnx2x_get_leader_lock_resource(sc));
}

static int bnx2x_release_leader_lock(struct bnx2x_softc *sc)
{
	return bnx2x_release_hw_lock(sc, bnx2x_get_leader_lock_resource(sc));
}

/* close gates #2, #3 and #4 */
static void bnx2x_set_234_gates(struct bnx2x_softc *sc, uint8_t close)
{
	uint32_t val;

	/* gates #2 and #4a are closed/opened */
	/* #4 */
	REG_WR(sc, PXP_REG_HST_DISCARD_DOORBELLS, ! !close);
	/* #2 */
	REG_WR(sc, PXP_REG_HST_DISCARD_INTERNAL_WRITES, ! !close);

	/* #3 */
	if (CHIP_IS_E1x(sc)) {
/* prevent interrupts from HC on both ports */
		val = REG_RD(sc, HC_REG_CONFIG_1);
		if (close)
			REG_WR(sc, HC_REG_CONFIG_1, (val & ~(uint32_t)
						     HC_CONFIG_1_REG_BLOCK_DISABLE_1));
		else
			REG_WR(sc, HC_REG_CONFIG_1,
			       (val | HC_CONFIG_1_REG_BLOCK_DISABLE_1));

		val = REG_RD(sc, HC_REG_CONFIG_0);
		if (close)
			REG_WR(sc, HC_REG_CONFIG_0, (val & ~(uint32_t)
						     HC_CONFIG_0_REG_BLOCK_DISABLE_0));
		else
			REG_WR(sc, HC_REG_CONFIG_0,
			       (val | HC_CONFIG_0_REG_BLOCK_DISABLE_0));

	} else {
/* Prevent incoming interrupts in IGU */
		val = REG_RD(sc, IGU_REG_BLOCK_CONFIGURATION);

		if (close)
			REG_WR(sc, IGU_REG_BLOCK_CONFIGURATION,
			       (val & ~(uint32_t)
				IGU_BLOCK_CONFIGURATION_REG_BLOCK_ENABLE));
		else
			REG_WR(sc, IGU_REG_BLOCK_CONFIGURATION,
			       (val |
				IGU_BLOCK_CONFIGURATION_REG_BLOCK_ENABLE));
	}

	wmb();
}

/* poll for pending writes bit, it should get cleared in no more than 1s */
static int bnx2x_er_poll_igu_vq(struct bnx2x_softc *sc)
{
	uint32_t cnt = 1000;
	uint32_t pend_bits = 0;

	do {
		pend_bits = REG_RD(sc, IGU_REG_PENDING_BITS_STATUS);

		if (pend_bits == 0) {
			break;
		}

		DELAY(1000);
	} while (cnt-- > 0);

	if (cnt <= 0) {
		PMD_DRV_LOG(NOTICE, sc, "Still pending IGU requests bits=0x%08x!",
			    pend_bits);
		return -1;
	}

	return 0;
}

#define SHARED_MF_CLP_MAGIC  0x80000000	/* 'magic' bit */

static void bnx2x_clp_reset_prep(struct bnx2x_softc *sc, uint32_t * magic_val)
{
	/* Do some magic... */
	uint32_t val = MFCFG_RD(sc, shared_mf_config.clp_mb);
	*magic_val = val & SHARED_MF_CLP_MAGIC;
	MFCFG_WR(sc, shared_mf_config.clp_mb, val | SHARED_MF_CLP_MAGIC);
}

/* restore the value of the 'magic' bit */
static void bnx2x_clp_reset_done(struct bnx2x_softc *sc, uint32_t magic_val)
{
	/* Restore the 'magic' bit value... */
	uint32_t val = MFCFG_RD(sc, shared_mf_config.clp_mb);
	MFCFG_WR(sc, shared_mf_config.clp_mb,
		 (val & (~SHARED_MF_CLP_MAGIC)) | magic_val);
}

/* prepare for MCP reset, takes care of CLP configurations */
static void bnx2x_reset_mcp_prep(struct bnx2x_softc *sc, uint32_t * magic_val)
{
	uint32_t shmem;
	uint32_t validity_offset;

	/* set `magic' bit in order to save MF config */
	bnx2x_clp_reset_prep(sc, magic_val);

	/* get shmem offset */
	shmem = REG_RD(sc, MISC_REG_SHARED_MEM_ADDR);
	validity_offset =
	    offsetof(struct shmem_region, validity_map[SC_PORT(sc)]);

	/* Clear validity map flags */
	if (shmem > 0) {
		REG_WR(sc, shmem + validity_offset, 0);
	}
}

#define MCP_TIMEOUT      5000	/* 5 seconds (in ms) */
#define MCP_ONE_TIMEOUT  100	/* 100 ms */

static void bnx2x_mcp_wait_one(struct bnx2x_softc *sc)
{
	/* special handling for emulation and FPGA (10 times longer) */
	if (CHIP_REV_IS_SLOW(sc)) {
		DELAY((MCP_ONE_TIMEOUT * 10) * 1000);
	} else {
		DELAY((MCP_ONE_TIMEOUT) * 1000);
	}
}

/* initialize shmem_base and waits for validity signature to appear */
static int bnx2x_init_shmem(struct bnx2x_softc *sc)
{
	int cnt = 0;
	uint32_t val = 0;

	do {
		sc->devinfo.shmem_base =
		    sc->link_params.shmem_base =
		    REG_RD(sc, MISC_REG_SHARED_MEM_ADDR);

		if (sc->devinfo.shmem_base) {
			val = SHMEM_RD(sc, validity_map[SC_PORT(sc)]);
			if (val & SHR_MEM_VALIDITY_MB)
				return 0;
		}

		bnx2x_mcp_wait_one(sc);

	} while (cnt++ < (MCP_TIMEOUT / MCP_ONE_TIMEOUT));

	PMD_DRV_LOG(NOTICE, sc, "BAD MCP validity signature");

	return -1;
}

static int bnx2x_reset_mcp_comp(struct bnx2x_softc *sc, uint32_t magic_val)
{
	int rc = bnx2x_init_shmem(sc);

	/* Restore the `magic' bit value */
	bnx2x_clp_reset_done(sc, magic_val);

	return rc;
}

static void bnx2x_pxp_prep(struct bnx2x_softc *sc)
{
	REG_WR(sc, PXP2_REG_RD_START_INIT, 0);
	REG_WR(sc, PXP2_REG_RQ_RBC_DONE, 0);
	wmb();
}

/*
 * Reset the whole chip except for:
 *      - PCIE core
 *      - PCI Glue, PSWHST, PXP/PXP2 RF (all controlled by one reset bit)
 *      - IGU
 *      - MISC (including AEU)
 *      - GRC
 *      - RBCN, RBCP
 */
static void bnx2x_process_kill_chip_reset(struct bnx2x_softc *sc, uint8_t global)
{
	uint32_t not_reset_mask1, reset_mask1, not_reset_mask2, reset_mask2;
	uint32_t global_bits2, stay_reset2;

	/*
	 * Bits that have to be set in reset_mask2 if we want to reset 'global'
	 * (per chip) blocks.
	 */
	global_bits2 =
	    MISC_REGISTERS_RESET_REG_2_RST_MCP_N_RESET_CMN_CPU |
	    MISC_REGISTERS_RESET_REG_2_RST_MCP_N_RESET_CMN_CORE;

	/*
	 * Don't reset the following blocks.
	 * Important: per port blocks (such as EMAC, BMAC, UMAC) can't be
	 *            reset, as in 4 port device they might still be owned
	 *            by the MCP (there is only one leader per path).
	 */
	not_reset_mask1 =
	    MISC_REGISTERS_RESET_REG_1_RST_HC |
	    MISC_REGISTERS_RESET_REG_1_RST_PXPV |
	    MISC_REGISTERS_RESET_REG_1_RST_PXP;

	not_reset_mask2 =
	    MISC_REGISTERS_RESET_REG_2_RST_PCI_MDIO |
	    MISC_REGISTERS_RESET_REG_2_RST_EMAC0_HARD_CORE |
	    MISC_REGISTERS_RESET_REG_2_RST_EMAC1_HARD_CORE |
	    MISC_REGISTERS_RESET_REG_2_RST_MISC_CORE |
	    MISC_REGISTERS_RESET_REG_2_RST_RBCN |
	    MISC_REGISTERS_RESET_REG_2_RST_GRC |
	    MISC_REGISTERS_RESET_REG_2_RST_MCP_N_RESET_REG_HARD_CORE |
	    MISC_REGISTERS_RESET_REG_2_RST_MCP_N_HARD_CORE_RST_B |
	    MISC_REGISTERS_RESET_REG_2_RST_ATC |
	    MISC_REGISTERS_RESET_REG_2_PGLC |
	    MISC_REGISTERS_RESET_REG_2_RST_BMAC0 |
	    MISC_REGISTERS_RESET_REG_2_RST_BMAC1 |
	    MISC_REGISTERS_RESET_REG_2_RST_EMAC0 |
	    MISC_REGISTERS_RESET_REG_2_RST_EMAC1 |
	    MISC_REGISTERS_RESET_REG_2_UMAC0 | MISC_REGISTERS_RESET_REG_2_UMAC1;

	/*
	 * Keep the following blocks in reset:
	 *  - all xxMACs are handled by the elink code.
	 */
	stay_reset2 =
	    MISC_REGISTERS_RESET_REG_2_XMAC |
	    MISC_REGISTERS_RESET_REG_2_XMAC_SOFT;

	/* Full reset masks according to the chip */
	reset_mask1 = 0xffffffff;

	if (CHIP_IS_E1H(sc))
		reset_mask2 = 0x1ffff;
	else if (CHIP_IS_E2(sc))
		reset_mask2 = 0xfffff;
	else			/* CHIP_IS_E3 */
		reset_mask2 = 0x3ffffff;

	/* Don't reset global blocks unless we need to */
	if (!global)
		reset_mask2 &= ~global_bits2;

	/*
	 * In case of attention in the QM, we need to reset PXP
	 * (MISC_REGISTERS_RESET_REG_2_RST_PXP_RQ_RD_WR) before QM
	 * because otherwise QM reset would release 'close the gates' shortly
	 * before resetting the PXP, then the PSWRQ would send a write
	 * request to PGLUE. Then when PXP is reset, PGLUE would try to
	 * read the payload data from PSWWR, but PSWWR would not
	 * respond. The write queue in PGLUE would stuck, dmae commands
	 * would not return. Therefore it's important to reset the second
	 * reset register (containing the
	 * MISC_REGISTERS_RESET_REG_2_RST_PXP_RQ_RD_WR bit) before the
	 * first one (containing the MISC_REGISTERS_RESET_REG_1_RST_QM
	 * bit).
	 */
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_CLEAR,
	       reset_mask2 & (~not_reset_mask2));

	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_1_CLEAR,
	       reset_mask1 & (~not_reset_mask1));

	mb();
	wmb();

	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_SET,
	       reset_mask2 & (~stay_reset2));

	mb();
	wmb();

	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_1_SET, reset_mask1);
	wmb();
}

static int bnx2x_process_kill(struct bnx2x_softc *sc, uint8_t global)
{
	int cnt = 1000;
	uint32_t val = 0;
	uint32_t sr_cnt, blk_cnt, port_is_idle_0, port_is_idle_1, pgl_exp_rom2;
	uint32_t tags_63_32 = 0;

	/* Empty the Tetris buffer, wait for 1s */
	do {
		sr_cnt = REG_RD(sc, PXP2_REG_RD_SR_CNT);
		blk_cnt = REG_RD(sc, PXP2_REG_RD_BLK_CNT);
		port_is_idle_0 = REG_RD(sc, PXP2_REG_RD_PORT_IS_IDLE_0);
		port_is_idle_1 = REG_RD(sc, PXP2_REG_RD_PORT_IS_IDLE_1);
		pgl_exp_rom2 = REG_RD(sc, PXP2_REG_PGL_EXP_ROM2);
		if (CHIP_IS_E3(sc)) {
			tags_63_32 = REG_RD(sc, PGLUE_B_REG_TAGS_63_32);
		}

		if ((sr_cnt == 0x7e) && (blk_cnt == 0xa0) &&
		    ((port_is_idle_0 & 0x1) == 0x1) &&
		    ((port_is_idle_1 & 0x1) == 0x1) &&
		    (pgl_exp_rom2 == 0xffffffff) &&
		    (!CHIP_IS_E3(sc) || (tags_63_32 == 0xffffffff)))
			break;
		DELAY(1000);
	} while (cnt-- > 0);

	if (cnt <= 0) {
		PMD_DRV_LOG(NOTICE, sc,
			    "ERROR: Tetris buffer didn't get empty or there "
			    "are still outstanding read requests after 1s! "
			    "sr_cnt=0x%08x, blk_cnt=0x%08x, port_is_idle_0=0x%08x, "
			    "port_is_idle_1=0x%08x, pgl_exp_rom2=0x%08x",
			    sr_cnt, blk_cnt, port_is_idle_0, port_is_idle_1,
			    pgl_exp_rom2);
		return -1;
	}

	mb();

	/* Close gates #2, #3 and #4 */
	bnx2x_set_234_gates(sc, TRUE);

	/* Poll for IGU VQs for 57712 and newer chips */
	if (!CHIP_IS_E1x(sc) && bnx2x_er_poll_igu_vq(sc)) {
		return -1;
	}

	/* clear "unprepared" bit */
	REG_WR(sc, MISC_REG_UNPREPARED, 0);
	mb();

	/* Make sure all is written to the chip before the reset */
	wmb();

	/*
	 * Wait for 1ms to empty GLUE and PCI-E core queues,
	 * PSWHST, GRC and PSWRD Tetris buffer.
	 */
	DELAY(1000);

	/* Prepare to chip reset: */
	/* MCP */
	if (global) {
		bnx2x_reset_mcp_prep(sc, &val);
	}

	/* PXP */
	bnx2x_pxp_prep(sc);
	mb();

	/* reset the chip */
	bnx2x_process_kill_chip_reset(sc, global);
	mb();

	/* Recover after reset: */
	/* MCP */
	if (global && bnx2x_reset_mcp_comp(sc, val)) {
		return -1;
	}

	/* Open the gates #2, #3 and #4 */
	bnx2x_set_234_gates(sc, FALSE);

	return 0;
}

static int bnx2x_leader_reset(struct bnx2x_softc *sc)
{
	int rc = 0;
	uint8_t global = bnx2x_reset_is_global(sc);
	uint32_t load_code;

	/*
	 * If not going to reset MCP, load "fake" driver to reset HW while
	 * driver is owner of the HW.
	 */
	if (!global && !BNX2X_NOMCP(sc)) {
		load_code = bnx2x_fw_command(sc, DRV_MSG_CODE_LOAD_REQ,
					   DRV_MSG_CODE_LOAD_REQ_WITH_LFA);
		if (!load_code) {
			PMD_DRV_LOG(NOTICE, sc, "MCP response failure, aborting");
			rc = -1;
			goto exit_leader_reset;
		}

		if ((load_code != FW_MSG_CODE_DRV_LOAD_COMMON_CHIP) &&
		    (load_code != FW_MSG_CODE_DRV_LOAD_COMMON)) {
			PMD_DRV_LOG(NOTICE, sc,
				    "MCP unexpected response, aborting");
			rc = -1;
			goto exit_leader_reset2;
		}

		load_code = bnx2x_fw_command(sc, DRV_MSG_CODE_LOAD_DONE, 0);
		if (!load_code) {
			PMD_DRV_LOG(NOTICE, sc, "MCP response failure, aborting");
			rc = -1;
			goto exit_leader_reset2;
		}
	}

	/* try to recover after the failure */
	if (bnx2x_process_kill(sc, global)) {
		PMD_DRV_LOG(NOTICE, sc, "Something bad occurred on engine %d!",
			    SC_PATH(sc));
		rc = -1;
		goto exit_leader_reset2;
	}

	/*
	 * Clear the RESET_IN_PROGRESS and RESET_GLOBAL bits and update the driver
	 * state.
	 */
	bnx2x_set_reset_done(sc);
	if (global) {
		bnx2x_clear_reset_global(sc);
	}

exit_leader_reset2:

	/* unload "fake driver" if it was loaded */
	if (!global &&!BNX2X_NOMCP(sc)) {
		bnx2x_fw_command(sc, DRV_MSG_CODE_UNLOAD_REQ_WOL_MCP, 0);
		bnx2x_fw_command(sc, DRV_MSG_CODE_UNLOAD_DONE, 0);
	}

exit_leader_reset:

	sc->is_leader = 0;
	bnx2x_release_leader_lock(sc);

	mb();
	return rc;
}

/*
 * prepare INIT transition, parameters configured:
 *   - HC configuration
 *   - Queue's CDU context
 */
static void
bnx2x_pf_q_prep_init(struct bnx2x_softc *sc, struct bnx2x_fastpath *fp,
		   struct ecore_queue_init_params *init_params)
{
	uint8_t cos;
	int cxt_index, cxt_offset;

	rte_bit_relaxed_set32(ECORE_Q_FLG_HC, &init_params->rx.flags);
	rte_bit_relaxed_set32(ECORE_Q_FLG_HC, &init_params->tx.flags);

	rte_bit_relaxed_set32(ECORE_Q_FLG_HC_EN, &init_params->rx.flags);
	rte_bit_relaxed_set32(ECORE_Q_FLG_HC_EN, &init_params->tx.flags);

	/* HC rate */
	init_params->rx.hc_rate =
	    sc->hc_rx_ticks ? (1000000 / sc->hc_rx_ticks) : 0;
	init_params->tx.hc_rate =
	    sc->hc_tx_ticks ? (1000000 / sc->hc_tx_ticks) : 0;

	/* FW SB ID */
	init_params->rx.fw_sb_id = init_params->tx.fw_sb_id = fp->fw_sb_id;

	/* CQ index among the SB indices */
	init_params->rx.sb_cq_index = HC_INDEX_ETH_RX_CQ_CONS;
	init_params->tx.sb_cq_index = HC_INDEX_ETH_FIRST_TX_CQ_CONS;

	/* set maximum number of COSs supported by this queue */
	init_params->max_cos = sc->max_cos;

	/* set the context pointers queue object */
	for (cos = FIRST_TX_COS_INDEX; cos < init_params->max_cos; cos++) {
		cxt_index = fp->index / ILT_PAGE_CIDS;
		cxt_offset = fp->index - (cxt_index * ILT_PAGE_CIDS);
		init_params->cxts[cos] =
		    &sc->context[cxt_index].vcxt[cxt_offset].eth;
	}
}

/* set flags that are common for the Tx-only and not normal connections */
static unsigned long
bnx2x_get_common_flags(struct bnx2x_softc *sc, uint8_t zero_stats)
{
	uint32_t flags = 0;

	/* PF driver will always initialize the Queue to an ACTIVE state */
	rte_bit_relaxed_set32(ECORE_Q_FLG_ACTIVE, &flags);

	/*
	 * tx only connections collect statistics (on the same index as the
	 * parent connection). The statistics are zeroed when the parent
	 * connection is initialized.
	 */

	rte_bit_relaxed_set32(ECORE_Q_FLG_STATS, &flags);
	if (zero_stats) {
		rte_bit_relaxed_set32(ECORE_Q_FLG_ZERO_STATS, &flags);
	}

	/*
	 * tx only connections can support tx-switching, though their
	 * CoS-ness doesn't survive the loopback
	 */
	if (sc->flags & BNX2X_TX_SWITCHING) {
		rte_bit_relaxed_set32(ECORE_Q_FLG_TX_SWITCH, &flags);
	}

	rte_bit_relaxed_set32(ECORE_Q_FLG_PCSUM_ON_PKT, &flags);

	return flags;
}

static unsigned long bnx2x_get_q_flags(struct bnx2x_softc *sc, uint8_t leading)
{
	uint32_t flags = 0;

	if (IS_MF_SD(sc)) {
		rte_bit_relaxed_set32(ECORE_Q_FLG_OV, &flags);
	}

	if (leading) {
		rte_bit_relaxed_set32(ECORE_Q_FLG_LEADING_RSS, &flags);
		rte_bit_relaxed_set32(ECORE_Q_FLG_MCAST, &flags);
	}

	rte_bit_relaxed_set32(ECORE_Q_FLG_VLAN, &flags);

	/* merge with common flags */
	return flags | bnx2x_get_common_flags(sc, TRUE);
}

static void
bnx2x_pf_q_prep_general(struct bnx2x_softc *sc, struct bnx2x_fastpath *fp,
		      struct ecore_general_setup_params *gen_init, uint8_t cos)
{
	gen_init->stat_id = bnx2x_stats_id(fp);
	gen_init->spcl_id = fp->cl_id;
	gen_init->mtu = sc->mtu;
	gen_init->cos = cos;
}

static void
bnx2x_pf_rx_q_prep(struct bnx2x_softc *sc, struct bnx2x_fastpath *fp,
		 struct rxq_pause_params *pause,
		 struct ecore_rxq_setup_params *rxq_init)
{
	struct bnx2x_rx_queue *rxq;

	rxq = sc->rx_queues[fp->index];
	if (!rxq) {
		PMD_RX_LOG(ERR, "RX queue is NULL");
		return;
	}
	/* pause */
	pause->bd_th_lo = BD_TH_LO(sc);
	pause->bd_th_hi = BD_TH_HI(sc);

	pause->rcq_th_lo = RCQ_TH_LO(sc);
	pause->rcq_th_hi = RCQ_TH_HI(sc);

	/* validate rings have enough entries to cross high thresholds */
	if (sc->dropless_fc &&
	    pause->bd_th_hi + FW_PREFETCH_CNT > sc->rx_ring_size) {
		PMD_DRV_LOG(WARNING, sc, "rx bd ring threshold limit");
	}

	if (sc->dropless_fc &&
	    pause->rcq_th_hi + FW_PREFETCH_CNT > USABLE_RCQ_ENTRIES(rxq)) {
		PMD_DRV_LOG(WARNING, sc, "rcq ring threshold limit");
	}

	pause->pri_map = 1;

	/* rxq setup */
	rxq_init->dscr_map = (rte_iova_t)rxq->rx_ring_phys_addr;
	rxq_init->rcq_map = (rte_iova_t)rxq->cq_ring_phys_addr;
	rxq_init->rcq_np_map = (rte_iova_t)(rxq->cq_ring_phys_addr +
					      BNX2X_PAGE_SIZE);

	/*
	 * This should be a maximum number of data bytes that may be
	 * placed on the BD (not including paddings).
	 */
	rxq_init->buf_sz = (fp->rx_buf_size - IP_HEADER_ALIGNMENT_PADDING);

	rxq_init->cl_qzone_id = fp->cl_qzone_id;
	rxq_init->rss_engine_id = SC_FUNC(sc);
	rxq_init->mcast_engine_id = SC_FUNC(sc);

	rxq_init->cache_line_log = BNX2X_RX_ALIGN_SHIFT;
	rxq_init->fw_sb_id = fp->fw_sb_id;

	rxq_init->sb_cq_index = HC_INDEX_ETH_RX_CQ_CONS;

	/*
	 * configure silent vlan removal
	 * if multi function mode is afex, then mask default vlan
	 */
	if (IS_MF_AFEX(sc)) {
		rxq_init->silent_removal_value =
		    sc->devinfo.mf_info.afex_def_vlan_tag;
		rxq_init->silent_removal_mask = EVL_VLID_MASK;
	}
}

static void
bnx2x_pf_tx_q_prep(struct bnx2x_softc *sc, struct bnx2x_fastpath *fp,
		 struct ecore_txq_setup_params *txq_init, uint8_t cos)
{
	struct bnx2x_tx_queue *txq = fp->sc->tx_queues[fp->index];

	if (!txq) {
		PMD_TX_LOG(ERR, "ERROR: TX queue is NULL");
		return;
	}
	txq_init->dscr_map = (rte_iova_t)txq->tx_ring_phys_addr;
	txq_init->sb_cq_index = HC_INDEX_ETH_FIRST_TX_CQ_CONS + cos;
	txq_init->traffic_type = LLFC_TRAFFIC_TYPE_NW;
	txq_init->fw_sb_id = fp->fw_sb_id;

	/*
	 * set the TSS leading client id for TX classfication to the
	 * leading RSS client id
	 */
	txq_init->tss_leading_cl_id = BNX2X_FP(sc, 0, cl_id);
}

/*
 * This function performs 2 steps in a queue state machine:
 *   1) RESET->INIT
 *   2) INIT->SETUP
 */
static int
bnx2x_setup_queue(struct bnx2x_softc *sc, struct bnx2x_fastpath *fp, uint8_t leading)
{
	struct ecore_queue_state_params q_params = { NULL };
	struct ecore_queue_setup_params *setup_params = &q_params.params.setup;
	int rc;

	PMD_DRV_LOG(DEBUG, sc, "setting up queue %d", fp->index);

	bnx2x_ack_sb(sc, fp->igu_sb_id, USTORM_ID, 0, IGU_INT_ENABLE, 0);

	q_params.q_obj = &BNX2X_SP_OBJ(sc, fp).q_obj;

	/* we want to wait for completion in this context */
	rte_bit_relaxed_set32(RAMROD_COMP_WAIT, &q_params.ramrod_flags);

	/* prepare the INIT parameters */
	bnx2x_pf_q_prep_init(sc, fp, &q_params.params.init);

	/* Set the command */
	q_params.cmd = ECORE_Q_CMD_INIT;

	/* Change the state to INIT */
	rc = ecore_queue_state_change(sc, &q_params);
	if (rc) {
		PMD_DRV_LOG(NOTICE, sc, "Queue(%d) INIT failed", fp->index);
		return rc;
	}

	PMD_DRV_LOG(DEBUG, sc, "init complete");

	/* now move the Queue to the SETUP state */
	memset(setup_params, 0, sizeof(*setup_params));

	/* set Queue flags */
	setup_params->flags = bnx2x_get_q_flags(sc, leading);

	/* set general SETUP parameters */
	bnx2x_pf_q_prep_general(sc, fp, &setup_params->gen_params,
			      FIRST_TX_COS_INDEX);

	bnx2x_pf_rx_q_prep(sc, fp,
			 &setup_params->pause_params,
			 &setup_params->rxq_params);

	bnx2x_pf_tx_q_prep(sc, fp, &setup_params->txq_params, FIRST_TX_COS_INDEX);

	/* Set the command */
	q_params.cmd = ECORE_Q_CMD_SETUP;

	/* change the state to SETUP */
	rc = ecore_queue_state_change(sc, &q_params);
	if (rc) {
		PMD_DRV_LOG(NOTICE, sc, "Queue(%d) SETUP failed", fp->index);
		return rc;
	}

	return rc;
}

static int bnx2x_setup_leading(struct bnx2x_softc *sc)
{
	if (IS_PF(sc))
		return bnx2x_setup_queue(sc, &sc->fp[0], TRUE);
	else			/* VF */
		return bnx2x_vf_setup_queue(sc, &sc->fp[0], TRUE);
}

static int
bnx2x_config_rss_pf(struct bnx2x_softc *sc, struct ecore_rss_config_obj *rss_obj,
		  uint8_t config_hash)
{
	struct ecore_config_rss_params params = { NULL };
	uint32_t i;

	/*
	 * Although RSS is meaningless when there is a single HW queue we
	 * still need it enabled in order to have HW Rx hash generated.
	 */

	params.rss_obj = rss_obj;

	rte_bit_relaxed_set32(RAMROD_COMP_WAIT, &params.ramrod_flags);

	rte_bit_relaxed_set32(ECORE_RSS_MODE_REGULAR, &params.rss_flags);

	/* RSS configuration */
	rte_bit_relaxed_set32(ECORE_RSS_IPV4, &params.rss_flags);
	rte_bit_relaxed_set32(ECORE_RSS_IPV4_TCP, &params.rss_flags);
	rte_bit_relaxed_set32(ECORE_RSS_IPV6, &params.rss_flags);
	rte_bit_relaxed_set32(ECORE_RSS_IPV6_TCP, &params.rss_flags);
	if (rss_obj->udp_rss_v4) {
		rte_bit_relaxed_set32(ECORE_RSS_IPV4_UDP, &params.rss_flags);
	}
	if (rss_obj->udp_rss_v6) {
		rte_bit_relaxed_set32(ECORE_RSS_IPV6_UDP, &params.rss_flags);
	}

	/* Hash bits */
	params.rss_result_mask = MULTI_MASK;

	rte_memcpy(params.ind_table, rss_obj->ind_table,
			 sizeof(params.ind_table));

	if (config_hash) {
/* RSS keys */
		for (i = 0; i < sizeof(params.rss_key) / 4; i++) {
			params.rss_key[i] = (uint32_t) rte_rand();
		}

		rte_bit_relaxed_set32(ECORE_RSS_SET_SRCH, &params.rss_flags);
	}

	if (IS_PF(sc))
		return ecore_config_rss(sc, &params);
	else
		return bnx2x_vf_config_rss(sc, &params);
}

static int bnx2x_config_rss_eth(struct bnx2x_softc *sc, uint8_t config_hash)
{
	return bnx2x_config_rss_pf(sc, &sc->rss_conf_obj, config_hash);
}

static int bnx2x_init_rss_pf(struct bnx2x_softc *sc)
{
	uint8_t num_eth_queues = BNX2X_NUM_ETH_QUEUES(sc);
	uint32_t i;

	/*
	 * Prepare the initial contents of the indirection table if
	 * RSS is enabled
	 */
	for (i = 0; i < sizeof(sc->rss_conf_obj.ind_table); i++) {
		sc->rss_conf_obj.ind_table[i] =
		    (sc->fp->cl_id + (i % num_eth_queues));
	}

	if (sc->udp_rss) {
		sc->rss_conf_obj.udp_rss_v4 = sc->rss_conf_obj.udp_rss_v6 = 1;
	}

	/*
	 * For 57711 SEARCHER configuration (rss_keys) is
	 * per-port, so if explicit configuration is needed, do it only
	 * for a PMF.
	 *
	 * For 57712 and newer it's a per-function configuration.
	 */
	return bnx2x_config_rss_eth(sc, sc->port.pmf || !CHIP_IS_E1x(sc));
}

static int
bnx2x_set_mac_one(struct bnx2x_softc *sc, uint8_t * mac,
		struct ecore_vlan_mac_obj *obj, uint8_t set, int mac_type,
		uint32_t *ramrod_flags)
{
	struct ecore_vlan_mac_ramrod_params ramrod_param;
	int rc;

	memset(&ramrod_param, 0, sizeof(ramrod_param));

	/* fill in general parameters */
	ramrod_param.vlan_mac_obj = obj;
	ramrod_param.ramrod_flags = *ramrod_flags;

	/* fill a user request section if needed */
	if (!rte_bit_relaxed_get32(RAMROD_CONT, ramrod_flags)) {
		rte_memcpy(ramrod_param.user_req.u.mac.mac, mac,
				 ETH_ALEN);

		rte_bit_relaxed_set32(mac_type,
				      &ramrod_param.user_req.vlan_mac_flags);

/* Set the command: ADD or DEL */
		ramrod_param.user_req.cmd = (set) ? ECORE_VLAN_MAC_ADD :
		    ECORE_VLAN_MAC_DEL;
	}

	rc = ecore_config_vlan_mac(sc, &ramrod_param);

	if (rc == ECORE_EXISTS) {
		PMD_DRV_LOG(INFO, sc, "Failed to schedule ADD operations (EEXIST)");
/* do not treat adding same MAC as error */
		rc = 0;
	} else if (rc < 0) {
		PMD_DRV_LOG(ERR, sc,
			    "%s MAC failed (%d)", (set ? "Set" : "Delete"), rc);
	}

	return rc;
}

static int bnx2x_set_eth_mac(struct bnx2x_softc *sc, uint8_t set)
{
	uint32_t ramrod_flags = 0;

	PMD_DRV_LOG(DEBUG, sc, "Adding Ethernet MAC");

	rte_bit_relaxed_set32(RAMROD_COMP_WAIT, &ramrod_flags);

	/* Eth MAC is set on RSS leading client (fp[0]) */
	return bnx2x_set_mac_one(sc, sc->link_params.mac_addr,
			       &sc->sp_objs->mac_obj,
			       set, ECORE_ETH_MAC, &ramrod_flags);
}

static int bnx2x_get_cur_phy_idx(struct bnx2x_softc *sc)
{
	uint32_t sel_phy_idx = 0;

	if (sc->link_params.num_phys <= 1) {
		return ELINK_INT_PHY;
	}

	if (sc->link_vars.link_up) {
		sel_phy_idx = ELINK_EXT_PHY1;
/* In case link is SERDES, check if the ELINK_EXT_PHY2 is the one */
		if ((sc->link_vars.link_status & LINK_STATUS_SERDES_LINK) &&
		    (sc->link_params.phy[ELINK_EXT_PHY2].supported &
		     ELINK_SUPPORTED_FIBRE))
			sel_phy_idx = ELINK_EXT_PHY2;
	} else {
		switch (elink_phy_selection(&sc->link_params)) {
		case PORT_HW_CFG_PHY_SELECTION_HARDWARE_DEFAULT:
		case PORT_HW_CFG_PHY_SELECTION_FIRST_PHY:
		case PORT_HW_CFG_PHY_SELECTION_FIRST_PHY_PRIORITY:
			sel_phy_idx = ELINK_EXT_PHY1;
			break;
		case PORT_HW_CFG_PHY_SELECTION_SECOND_PHY:
		case PORT_HW_CFG_PHY_SELECTION_SECOND_PHY_PRIORITY:
			sel_phy_idx = ELINK_EXT_PHY2;
			break;
		}
	}

	return sel_phy_idx;
}

static int bnx2x_get_link_cfg_idx(struct bnx2x_softc *sc)
{
	uint32_t sel_phy_idx = bnx2x_get_cur_phy_idx(sc);

	/*
	 * The selected activated PHY is always after swapping (in case PHY
	 * swapping is enabled). So when swapping is enabled, we need to reverse
	 * the configuration
	 */

	if (sc->link_params.multi_phy_config & PORT_HW_CFG_PHY_SWAPPED_ENABLED) {
		if (sel_phy_idx == ELINK_EXT_PHY1)
			sel_phy_idx = ELINK_EXT_PHY2;
		else if (sel_phy_idx == ELINK_EXT_PHY2)
			sel_phy_idx = ELINK_EXT_PHY1;
	}

	return ELINK_LINK_CONFIG_IDX(sel_phy_idx);
}

static void bnx2x_set_requested_fc(struct bnx2x_softc *sc)
{
	/*
	 * Initialize link parameters structure variables
	 * It is recommended to turn off RX FC for jumbo frames
	 * for better performance
	 */
	if (CHIP_IS_E1x(sc) && (sc->mtu > 5000)) {
		sc->link_params.req_fc_auto_adv = ELINK_FLOW_CTRL_TX;
	} else {
		sc->link_params.req_fc_auto_adv = ELINK_FLOW_CTRL_BOTH;
	}
}

static void bnx2x_calc_fc_adv(struct bnx2x_softc *sc)
{
	uint8_t cfg_idx = bnx2x_get_link_cfg_idx(sc);
	switch (sc->link_vars.ieee_fc &
		MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_MASK) {
	case MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_NONE:
	default:
		sc->port.advertising[cfg_idx] &= ~(ADVERTISED_Asym_Pause |
						   ADVERTISED_Pause);
		break;

	case MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH:
		sc->port.advertising[cfg_idx] |= (ADVERTISED_Asym_Pause |
						  ADVERTISED_Pause);
		break;

	case MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC:
		sc->port.advertising[cfg_idx] |= ADVERTISED_Asym_Pause;
		break;
	}
}

static uint16_t bnx2x_get_mf_speed(struct bnx2x_softc *sc)
{
	uint16_t line_speed = sc->link_vars.line_speed;
	if (IS_MF(sc)) {
		uint16_t maxCfg = bnx2x_extract_max_cfg(sc,
						      sc->devinfo.
						      mf_info.mf_config[SC_VN
									(sc)]);

/* calculate the current MAX line speed limit for the MF devices */
		if (IS_MF_SI(sc)) {
			line_speed = (line_speed * maxCfg) / 100;
		} else {	/* SD mode */
			uint16_t vn_max_rate = maxCfg * 100;

			if (vn_max_rate < line_speed) {
				line_speed = vn_max_rate;
			}
		}
	}

	return line_speed;
}

static void
bnx2x_fill_report_data(struct bnx2x_softc *sc, struct bnx2x_link_report_data *data)
{
	uint16_t line_speed = bnx2x_get_mf_speed(sc);

	memset(data, 0, sizeof(*data));

	/* fill the report data with the effective line speed */
	data->line_speed = line_speed;

	/* Link is down */
	if (!sc->link_vars.link_up || (sc->flags & BNX2X_MF_FUNC_DIS)) {
		rte_bit_relaxed_set32(BNX2X_LINK_REPORT_LINK_DOWN,
			    &data->link_report_flags);
	}

	/* Full DUPLEX */
	if (sc->link_vars.duplex == DUPLEX_FULL) {
		rte_bit_relaxed_set32(BNX2X_LINK_REPORT_FULL_DUPLEX,
			    &data->link_report_flags);
	}

	/* Rx Flow Control is ON */
	if (sc->link_vars.flow_ctrl & ELINK_FLOW_CTRL_RX) {
		rte_bit_relaxed_set32(BNX2X_LINK_REPORT_RX_FC_ON,
				&data->link_report_flags);
	}

	/* Tx Flow Control is ON */
	if (sc->link_vars.flow_ctrl & ELINK_FLOW_CTRL_TX) {
		rte_bit_relaxed_set32(BNX2X_LINK_REPORT_TX_FC_ON,
				&data->link_report_flags);
	}
}

/* report link status to OS, should be called under phy_lock */
static void bnx2x_link_report_locked(struct bnx2x_softc *sc)
{
	struct bnx2x_link_report_data cur_data;

	/* reread mf_cfg */
	if (IS_PF(sc)) {
		bnx2x_read_mf_cfg(sc);
	}

	/* Read the current link report info */
	bnx2x_fill_report_data(sc, &cur_data);

	/* Don't report link down or exactly the same link status twice */
	if (!memcmp(&cur_data, &sc->last_reported_link, sizeof(cur_data)) ||
	    (rte_bit_relaxed_get32(BNX2X_LINK_REPORT_LINK_DOWN,
			  &sc->last_reported_link.link_report_flags) &&
	     rte_bit_relaxed_get32(BNX2X_LINK_REPORT_LINK_DOWN,
			  &cur_data.link_report_flags))) {
		return;
	}

	ELINK_DEBUG_P2(sc, "Change in link status : cur_data = %x, last_reported_link = %x",
		       cur_data.link_report_flags,
		       sc->last_reported_link.link_report_flags);

	sc->link_cnt++;

	ELINK_DEBUG_P1(sc, "link status change count = %x", sc->link_cnt);
	/* report new link params and remember the state for the next time */
	rte_memcpy(&sc->last_reported_link, &cur_data, sizeof(cur_data));

	if (rte_bit_relaxed_get32(BNX2X_LINK_REPORT_LINK_DOWN,
			 &cur_data.link_report_flags)) {
		ELINK_DEBUG_P0(sc, "NIC Link is Down");
	} else {
		__rte_unused const char *duplex;
		__rte_unused const char *flow;

		if (rte_bit_relaxed_test_and_clear32
					(BNX2X_LINK_REPORT_FULL_DUPLEX,
					 &cur_data.link_report_flags)) {
			duplex = "full";
				ELINK_DEBUG_P0(sc, "link set to full duplex");
		} else {
			duplex = "half";
				ELINK_DEBUG_P0(sc, "link set to half duplex");
		}

/*
 * Handle the FC at the end so that only these flags would be
 * possibly set. This way we may easily check if there is no FC
 * enabled.
 */
		if (cur_data.link_report_flags) {
			if (rte_bit_relaxed_get32
					(BNX2X_LINK_REPORT_RX_FC_ON,
					 &cur_data.link_report_flags) &&
			    rte_bit_relaxed_get32(BNX2X_LINK_REPORT_TX_FC_ON,
					 &cur_data.link_report_flags)) {
				flow = "ON - receive & transmit";
			} else if (rte_bit_relaxed_get32
						(BNX2X_LINK_REPORT_RX_FC_ON,
						 &cur_data.link_report_flags) &&
				   !rte_bit_relaxed_get32
						(BNX2X_LINK_REPORT_TX_FC_ON,
						 &cur_data.link_report_flags)) {
				flow = "ON - receive";
			} else if (!rte_bit_relaxed_get32
						(BNX2X_LINK_REPORT_RX_FC_ON,
						 &cur_data.link_report_flags) &&
				   rte_bit_relaxed_get32
						(BNX2X_LINK_REPORT_TX_FC_ON,
						 &cur_data.link_report_flags)) {
				flow = "ON - transmit";
			} else {
				flow = "none";	/* possible? */
			}
		} else {
			flow = "none";
		}

		PMD_DRV_LOG(INFO, sc,
			    "NIC Link is Up, %d Mbps %s duplex, Flow control: %s",
			    cur_data.line_speed, duplex, flow);
	}
}

static void
bnx2x_link_report(struct bnx2x_softc *sc)
{
	bnx2x_acquire_phy_lock(sc);
	bnx2x_link_report_locked(sc);
	bnx2x_release_phy_lock(sc);
}

void bnx2x_link_status_update(struct bnx2x_softc *sc)
{
	if (sc->state != BNX2X_STATE_OPEN) {
		return;
	}

	if (IS_PF(sc) && !CHIP_REV_IS_SLOW(sc)) {
		elink_link_status_update(&sc->link_params, &sc->link_vars);
	} else {
		sc->port.supported[0] |= (ELINK_SUPPORTED_10baseT_Half |
					  ELINK_SUPPORTED_10baseT_Full |
					  ELINK_SUPPORTED_100baseT_Half |
					  ELINK_SUPPORTED_100baseT_Full |
					  ELINK_SUPPORTED_1000baseT_Full |
					  ELINK_SUPPORTED_2500baseX_Full |
					  ELINK_SUPPORTED_10000baseT_Full |
					  ELINK_SUPPORTED_TP |
					  ELINK_SUPPORTED_FIBRE |
					  ELINK_SUPPORTED_Autoneg |
					  ELINK_SUPPORTED_Pause |
					  ELINK_SUPPORTED_Asym_Pause);
		sc->port.advertising[0] = sc->port.supported[0];

		sc->link_params.sc = sc;
		sc->link_params.port = SC_PORT(sc);
		sc->link_params.req_duplex[0] = DUPLEX_FULL;
		sc->link_params.req_flow_ctrl[0] = ELINK_FLOW_CTRL_NONE;
		sc->link_params.req_line_speed[0] = SPEED_10000;
		sc->link_params.speed_cap_mask[0] = 0x7f0000;
		sc->link_params.switch_cfg = ELINK_SWITCH_CFG_10G;

		if (CHIP_REV_IS_FPGA(sc)) {
			sc->link_vars.mac_type = ELINK_MAC_TYPE_EMAC;
			sc->link_vars.line_speed = ELINK_SPEED_1000;
			sc->link_vars.link_status = (LINK_STATUS_LINK_UP |
						     LINK_STATUS_SPEED_AND_DUPLEX_1000TFD);
		} else {
			sc->link_vars.mac_type = ELINK_MAC_TYPE_BMAC;
			sc->link_vars.line_speed = ELINK_SPEED_10000;
			sc->link_vars.link_status = (LINK_STATUS_LINK_UP |
						     LINK_STATUS_SPEED_AND_DUPLEX_10GTFD);
		}

		sc->link_vars.link_up = 1;

		sc->link_vars.duplex = DUPLEX_FULL;
		sc->link_vars.flow_ctrl = ELINK_FLOW_CTRL_NONE;

		if (IS_PF(sc)) {
			REG_WR(sc,
			       NIG_REG_EGRESS_DRAIN0_MODE +
			       sc->link_params.port * 4, 0);
			bnx2x_stats_handle(sc, STATS_EVENT_LINK_UP);
			bnx2x_link_report(sc);
		}
	}

	if (IS_PF(sc)) {
		if (sc->link_vars.link_up) {
			bnx2x_stats_handle(sc, STATS_EVENT_LINK_UP);
		} else {
			bnx2x_stats_handle(sc, STATS_EVENT_STOP);
		}
		bnx2x_link_report(sc);
	} else {
		bnx2x_link_report_locked(sc);
		bnx2x_stats_handle(sc, STATS_EVENT_LINK_UP);
	}
}

static int bnx2x_initial_phy_init(struct bnx2x_softc *sc, int load_mode)
{
	int rc, cfg_idx = bnx2x_get_link_cfg_idx(sc);
	uint16_t req_line_speed = sc->link_params.req_line_speed[cfg_idx];
	struct elink_params *lp = &sc->link_params;

	bnx2x_set_requested_fc(sc);

	bnx2x_acquire_phy_lock(sc);

	if (load_mode == LOAD_DIAG) {
		lp->loopback_mode = ELINK_LOOPBACK_XGXS;
/* Prefer doing PHY loopback at 10G speed, if possible */
		if (lp->req_line_speed[cfg_idx] < ELINK_SPEED_10000) {
			if (lp->speed_cap_mask[cfg_idx] &
			    PORT_HW_CFG_SPEED_CAPABILITY_D0_10G) {
				lp->req_line_speed[cfg_idx] = ELINK_SPEED_10000;
			} else {
				lp->req_line_speed[cfg_idx] = ELINK_SPEED_1000;
			}
		}
	}

	if (load_mode == LOAD_LOOPBACK_EXT) {
		lp->loopback_mode = ELINK_LOOPBACK_EXT;
	}

	rc = elink_phy_init(&sc->link_params, &sc->link_vars);

	bnx2x_release_phy_lock(sc);

	bnx2x_calc_fc_adv(sc);

	if (sc->link_vars.link_up) {
		bnx2x_stats_handle(sc, STATS_EVENT_LINK_UP);
		bnx2x_link_report(sc);
	}

	sc->link_params.req_line_speed[cfg_idx] = req_line_speed;
	return rc;
}

/* update flags in shmem */
static void
bnx2x_update_drv_flags(struct bnx2x_softc *sc, uint32_t flags, uint32_t set)
{
	uint32_t drv_flags;

	if (SHMEM2_HAS(sc, drv_flags)) {
		bnx2x_acquire_hw_lock(sc, HW_LOCK_RESOURCE_DRV_FLAGS);
		drv_flags = SHMEM2_RD(sc, drv_flags);

		if (set) {
			drv_flags |= flags;
		} else {
			drv_flags &= ~flags;
		}

		SHMEM2_WR(sc, drv_flags, drv_flags);

		bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_DRV_FLAGS);
	}
}

/* periodic timer callout routine, only runs when the interface is up */
void bnx2x_periodic_callout(struct bnx2x_softc *sc)
{
	if ((sc->state != BNX2X_STATE_OPEN) ||
	    (atomic_load_acq_long(&sc->periodic_flags) == PERIODIC_STOP)) {
		PMD_DRV_LOG(DEBUG, sc, "periodic callout exit (state=0x%x)",
			    sc->state);
		return;
	}
	if (!CHIP_REV_IS_SLOW(sc)) {
/*
 * This barrier is needed to ensure the ordering between the writing
 * to the sc->port.pmf in the bnx2x_nic_load() or bnx2x_pmf_update() and
 * the reading here.
 */
		mb();
		if (sc->port.pmf) {
			bnx2x_acquire_phy_lock(sc);
			elink_period_func(&sc->link_params, &sc->link_vars);
			bnx2x_release_phy_lock(sc);
		}
	}
#ifdef BNX2X_PULSE
	if (IS_PF(sc) && !BNX2X_NOMCP(sc)) {
		int mb_idx = SC_FW_MB_IDX(sc);
		uint32_t drv_pulse;
		uint32_t mcp_pulse;

		++sc->fw_drv_pulse_wr_seq;
		sc->fw_drv_pulse_wr_seq &= DRV_PULSE_SEQ_MASK;

		drv_pulse = sc->fw_drv_pulse_wr_seq;
		bnx2x_drv_pulse(sc);

		mcp_pulse = (SHMEM_RD(sc, func_mb[mb_idx].mcp_pulse_mb) &
			     MCP_PULSE_SEQ_MASK);

/*
 * The delta between driver pulse and mcp response should
 * be 1 (before mcp response) or 0 (after mcp response).
 */
		if ((drv_pulse != mcp_pulse) &&
		    (drv_pulse != ((mcp_pulse + 1) & MCP_PULSE_SEQ_MASK))) {
			/* someone lost a heartbeat... */
			PMD_DRV_LOG(ERR, sc,
				    "drv_pulse (0x%x) != mcp_pulse (0x%x)",
				    drv_pulse, mcp_pulse);
		}
	}
#endif
}

/* start the controller */
static __rte_noinline
int bnx2x_nic_load(struct bnx2x_softc *sc)
{
	uint32_t val;
	uint32_t load_code = 0;
	int i, rc = 0;

	PMD_INIT_FUNC_TRACE(sc);

	sc->state = BNX2X_STATE_OPENING_WAITING_LOAD;

	if (IS_PF(sc)) {
/* must be called before memory allocation and HW init */
		bnx2x_ilt_set_info(sc);
	}

	bnx2x_set_fp_rx_buf_size(sc);

	if (IS_PF(sc)) {
		if (bnx2x_alloc_mem(sc) != 0) {
			sc->state = BNX2X_STATE_CLOSED;
			rc = -ENOMEM;
			goto bnx2x_nic_load_error0;
		}
	}

	/* allocate the host hardware/software hsi structures */
	if (bnx2x_alloc_hsi_mem(sc) != 0) {
		PMD_DRV_LOG(ERR, sc, "bnx2x_alloc_hsi_mem was failed");
		sc->state = BNX2X_STATE_CLOSED;
		rc = -ENOMEM;
		goto bnx2x_nic_load_error0;
	}

	if (bnx2x_alloc_fw_stats_mem(sc) != 0) {
		sc->state = BNX2X_STATE_CLOSED;
		rc = -ENOMEM;
		goto bnx2x_nic_load_error0;
	}

	if (IS_VF(sc)) {
		rc = bnx2x_vf_init(sc);
		if (rc) {
			sc->state = BNX2X_STATE_ERROR;
			goto bnx2x_nic_load_error0;
		}
	}

	if (IS_PF(sc)) {
/* set pf load just before approaching the MCP */
		bnx2x_set_pf_load(sc);

/* if MCP exists send load request and analyze response */
		if (!BNX2X_NOMCP(sc)) {
			/* attempt to load pf */
			if (bnx2x_nic_load_request(sc, &load_code) != 0) {
				sc->state = BNX2X_STATE_CLOSED;
				rc = -ENXIO;
				goto bnx2x_nic_load_error1;
			}

			/* what did the MCP say? */
			if (bnx2x_nic_load_analyze_req(sc, load_code) != 0) {
				bnx2x_fw_command(sc, DRV_MSG_CODE_LOAD_DONE, 0);
				sc->state = BNX2X_STATE_CLOSED;
				rc = -ENXIO;
				goto bnx2x_nic_load_error2;
			}
		} else {
			PMD_DRV_LOG(INFO, sc, "Device has no MCP!");
			load_code = bnx2x_nic_load_no_mcp(sc);
		}

/* mark PMF if applicable */
		bnx2x_nic_load_pmf(sc, load_code);

/* Init Function state controlling object */
		bnx2x_init_func_obj(sc);

/* Initialize HW */
		if (bnx2x_init_hw(sc, load_code) != 0) {
			PMD_DRV_LOG(NOTICE, sc, "HW init failed");
			bnx2x_fw_command(sc, DRV_MSG_CODE_LOAD_DONE, 0);
			sc->state = BNX2X_STATE_CLOSED;
			rc = -ENXIO;
			goto bnx2x_nic_load_error2;
		}
	}

	bnx2x_nic_init(sc, load_code);

	/* Init per-function objects */
	if (IS_PF(sc)) {
		bnx2x_init_objs(sc);

/* set AFEX default VLAN tag to an invalid value */
		sc->devinfo.mf_info.afex_def_vlan_tag = -1;

		sc->state = BNX2X_STATE_OPENING_WAITING_PORT;
		rc = bnx2x_func_start(sc);
		if (rc) {
			PMD_DRV_LOG(NOTICE, sc, "Function start failed!");
			bnx2x_fw_command(sc, DRV_MSG_CODE_LOAD_DONE, 0);
			sc->state = BNX2X_STATE_ERROR;
			goto bnx2x_nic_load_error3;
		}

/* send LOAD_DONE command to MCP */
		if (!BNX2X_NOMCP(sc)) {
			load_code =
			    bnx2x_fw_command(sc, DRV_MSG_CODE_LOAD_DONE, 0);
			if (!load_code) {
				PMD_DRV_LOG(NOTICE, sc,
					    "MCP response failure, aborting");
				sc->state = BNX2X_STATE_ERROR;
				rc = -ENXIO;
				goto bnx2x_nic_load_error3;
			}
		}
	}

	rc = bnx2x_setup_leading(sc);
	if (rc) {
		PMD_DRV_LOG(NOTICE, sc, "Setup leading failed!");
		sc->state = BNX2X_STATE_ERROR;
		goto bnx2x_nic_load_error3;
	}

	FOR_EACH_NONDEFAULT_ETH_QUEUE(sc, i) {
		if (IS_PF(sc))
			rc = bnx2x_setup_queue(sc, &sc->fp[i], FALSE);
		else		/* IS_VF(sc) */
			rc = bnx2x_vf_setup_queue(sc, &sc->fp[i], FALSE);

		if (rc) {
			PMD_DRV_LOG(NOTICE, sc, "Queue(%d) setup failed", i);
			sc->state = BNX2X_STATE_ERROR;
			goto bnx2x_nic_load_error3;
		}
	}

	rc = bnx2x_init_rss_pf(sc);
	if (rc) {
		PMD_DRV_LOG(NOTICE, sc, "PF RSS init failed");
		sc->state = BNX2X_STATE_ERROR;
		goto bnx2x_nic_load_error3;
	}

	/* now when Clients are configured we are ready to work */
	sc->state = BNX2X_STATE_OPEN;

	/* Configure a ucast MAC */
	if (IS_PF(sc)) {
		rc = bnx2x_set_eth_mac(sc, TRUE);
	} else {		/* IS_VF(sc) */
		rc = bnx2x_vf_set_mac(sc, TRUE);
	}

	if (rc) {
		PMD_DRV_LOG(NOTICE, sc, "Setting Ethernet MAC failed");
		sc->state = BNX2X_STATE_ERROR;
		goto bnx2x_nic_load_error3;
	}

	if (sc->port.pmf) {
		rc = bnx2x_initial_phy_init(sc, LOAD_OPEN);
		if (rc) {
			sc->state = BNX2X_STATE_ERROR;
			goto bnx2x_nic_load_error3;
		}
	}

	sc->link_params.feature_config_flags &=
	    ~ELINK_FEATURE_CONFIG_BOOT_FROM_SAN;

	/* start the Tx */
	switch (LOAD_OPEN) {
	case LOAD_NORMAL:
	case LOAD_OPEN:
		break;

	case LOAD_DIAG:
	case LOAD_LOOPBACK_EXT:
		sc->state = BNX2X_STATE_DIAG;
		break;

	default:
		break;
	}

	if (sc->port.pmf) {
		bnx2x_update_drv_flags(sc, 1 << DRV_FLAGS_PORT_MASK, 0);
	} else {
		bnx2x_link_status_update(sc);
	}

	if (IS_PF(sc) && SHMEM2_HAS(sc, drv_capabilities_flag)) {
/* mark driver is loaded in shmem2 */
		val = SHMEM2_RD(sc, drv_capabilities_flag[SC_FW_MB_IDX(sc)]);
		SHMEM2_WR(sc, drv_capabilities_flag[SC_FW_MB_IDX(sc)],
			  (val |
			   DRV_FLAGS_CAPABILITIES_LOADED_SUPPORTED |
			   DRV_FLAGS_CAPABILITIES_LOADED_L2));
	}

	/* start fast path */
	/* Initialize Rx filter */
	bnx2x_set_rx_mode(sc);

	/* wait for all pending SP commands to complete */
	if (IS_PF(sc) && !bnx2x_wait_sp_comp(sc, ~0x0U)) {
		PMD_DRV_LOG(NOTICE, sc, "Timeout waiting for all SPs to complete!");
		bnx2x_periodic_stop(sc);
		bnx2x_nic_unload(sc, UNLOAD_CLOSE, FALSE);
		return -ENXIO;
	}

	PMD_DRV_LOG(DEBUG, sc, "NIC successfully loaded");

	return 0;

bnx2x_nic_load_error3:

	if (IS_PF(sc)) {
		bnx2x_int_disable_sync(sc, 1);

/* clean out queued objects */
		bnx2x_squeeze_objects(sc);
	}

bnx2x_nic_load_error2:

	if (IS_PF(sc) && !BNX2X_NOMCP(sc)) {
		bnx2x_fw_command(sc, DRV_MSG_CODE_UNLOAD_REQ_WOL_MCP, 0);
		bnx2x_fw_command(sc, DRV_MSG_CODE_UNLOAD_DONE, 0);
	}

	sc->port.pmf = 0;

bnx2x_nic_load_error1:

	/* clear pf_load status, as it was already set */
	if (IS_PF(sc)) {
		bnx2x_clear_pf_load(sc);
	}

bnx2x_nic_load_error0:

	bnx2x_free_fw_stats_mem(sc);
	bnx2x_free_hsi_mem(sc);
	bnx2x_free_mem(sc);

	return rc;
}

/*
* Handles controller initialization.
*/
int bnx2x_init(struct bnx2x_softc *sc)
{
	int other_engine = SC_PATH(sc) ? 0 : 1;
	uint8_t other_load_status, load_status;
	uint8_t global = FALSE;
	int rc;

	/* Check if the driver is still running and bail out if it is. */
	if (sc->state != BNX2X_STATE_CLOSED) {
		PMD_DRV_LOG(DEBUG, sc, "Init called while driver is running!");
		rc = 0;
		goto bnx2x_init_done;
	}

	bnx2x_set_power_state(sc, PCI_PM_D0);

	/*
	 * If parity occurred during the unload, then attentions and/or
	 * RECOVERY_IN_PROGRESS may still be set. If so we want the first function
	 * loaded on the current engine to complete the recovery. Parity recovery
	 * is only relevant for PF driver.
	 */
	if (IS_PF(sc)) {
		other_load_status = bnx2x_get_load_status(sc, other_engine);
		load_status = bnx2x_get_load_status(sc, SC_PATH(sc));

		if (!bnx2x_reset_is_done(sc, SC_PATH(sc)) ||
		    bnx2x_chk_parity_attn(sc, &global, TRUE)) {
			do {
				/*
				 * If there are attentions and they are in global blocks, set
				 * the GLOBAL_RESET bit regardless whether it will be this
				 * function that will complete the recovery or not.
				 */
				if (global) {
					bnx2x_set_reset_global(sc);
				}

				/*
				 * Only the first function on the current engine should try
				 * to recover in open. In case of attentions in global blocks
				 * only the first in the chip should try to recover.
				 */
				if ((!load_status
				     && (!global ||!other_load_status))
				    && bnx2x_trylock_leader_lock(sc)
				    && !bnx2x_leader_reset(sc)) {
					PMD_DRV_LOG(INFO, sc,
						    "Recovered during init");
					break;
				}

				/* recovery has failed... */
				bnx2x_set_power_state(sc, PCI_PM_D3hot);

				sc->recovery_state = BNX2X_RECOVERY_FAILED;

				PMD_DRV_LOG(NOTICE, sc,
					    "Recovery flow hasn't properly "
					    "completed yet, try again later. "
					    "If you still see this message after a "
					    "few retries then power cycle is required.");

				rc = -ENXIO;
				goto bnx2x_init_done;
			} while (0);
		}
	}

	sc->recovery_state = BNX2X_RECOVERY_DONE;

	rc = bnx2x_nic_load(sc);

bnx2x_init_done:

	if (rc) {
		PMD_DRV_LOG(NOTICE, sc, "Initialization failed, "
			    "stack notified driver is NOT running!");
	}

	return rc;
}

static void bnx2x_get_function_num(struct bnx2x_softc *sc)
{
	uint32_t val = 0;

	/*
	 * Read the ME register to get the function number. The ME register
	 * holds the relative-function number and absolute-function number. The
	 * absolute-function number appears only in E2 and above. Before that
	 * these bits always contained zero, therefore we cannot blindly use them.
	 */

	val = REG_RD(sc, BAR_ME_REGISTER);

	sc->pfunc_rel =
	    (uint8_t) ((val & ME_REG_PF_NUM) >> ME_REG_PF_NUM_SHIFT);
	sc->path_id =
	    (uint8_t) ((val & ME_REG_ABS_PF_NUM) >> ME_REG_ABS_PF_NUM_SHIFT) &
	    1;

	if (CHIP_PORT_MODE(sc) == CHIP_4_PORT_MODE) {
		sc->pfunc_abs = ((sc->pfunc_rel << 1) | sc->path_id);
	} else {
		sc->pfunc_abs = (sc->pfunc_rel | sc->path_id);
	}

	PMD_DRV_LOG(DEBUG, sc,
		    "Relative function %d, Absolute function %d, Path %d",
		    sc->pfunc_rel, sc->pfunc_abs, sc->path_id);
}

static uint32_t bnx2x_get_shmem_mf_cfg_base(struct bnx2x_softc *sc)
{
	uint32_t shmem2_size;
	uint32_t offset;
	uint32_t mf_cfg_offset_value;

	/* Non 57712 */
	offset = (SHMEM_ADDR(sc, func_mb) +
		  (MAX_FUNC_NUM * sizeof(struct drv_func_mb)));

	/* 57712 plus */
	if (sc->devinfo.shmem2_base != 0) {
		shmem2_size = SHMEM2_RD(sc, size);
		if (shmem2_size > offsetof(struct shmem2_region, mf_cfg_addr)) {
			mf_cfg_offset_value = SHMEM2_RD(sc, mf_cfg_addr);
			if (SHMEM_MF_CFG_ADDR_NONE != mf_cfg_offset_value) {
				offset = mf_cfg_offset_value;
			}
		}
	}

	return offset;
}

static uint32_t bnx2x_pcie_capability_read(struct bnx2x_softc *sc, int reg)
{
	uint32_t ret;
	struct bnx2x_pci_cap *caps;

	/* ensure PCIe capability is enabled */
	caps = pci_find_cap(sc, PCIY_EXPRESS, BNX2X_PCI_CAP);
	if (NULL != caps) {
		PMD_DRV_LOG(DEBUG, sc, "Found PCIe capability: "
			    "id=0x%04X type=0x%04X addr=0x%08X",
			    caps->id, caps->type, caps->addr);
		pci_read(sc, (caps->addr + reg), &ret, 2);
		return ret;
	}

	PMD_DRV_LOG(WARNING, sc, "PCIe capability NOT FOUND!!!");

	return 0;
}

static uint8_t bnx2x_is_pcie_pending(struct bnx2x_softc *sc)
{
	return bnx2x_pcie_capability_read(sc, PCIR_EXPRESS_DEVICE_STA) &
		PCIM_EXP_STA_TRANSACTION_PND;
}

/*
* Walk the PCI capabiites list for the device to find what features are
* supported. These capabilites may be enabled/disabled by firmware so it's
* best to walk the list rather than make assumptions.
*/
static void bnx2x_probe_pci_caps(struct bnx2x_softc *sc)
{
	PMD_INIT_FUNC_TRACE(sc);

	struct bnx2x_pci_cap *caps;
	uint16_t link_status;
	int reg = 0;

	/* check if PCI Power Management is enabled */
	caps = pci_find_cap(sc, PCIY_PMG, BNX2X_PCI_CAP);
	if (NULL != caps) {
		PMD_DRV_LOG(DEBUG, sc, "Found PM capability: "
			    "id=0x%04X type=0x%04X addr=0x%08X",
			    caps->id, caps->type, caps->addr);

		sc->devinfo.pcie_cap_flags |= BNX2X_PM_CAPABLE_FLAG;
		sc->devinfo.pcie_pm_cap_reg = caps->addr;
	}

	link_status = bnx2x_pcie_capability_read(sc, PCIR_EXPRESS_LINK_STA);

	sc->devinfo.pcie_link_speed = (link_status & PCIM_LINK_STA_SPEED);
	sc->devinfo.pcie_link_width =
	    ((link_status & PCIM_LINK_STA_WIDTH) >> 4);

	PMD_DRV_LOG(DEBUG, sc, "PCIe link speed=%d width=%d",
		    sc->devinfo.pcie_link_speed, sc->devinfo.pcie_link_width);

	sc->devinfo.pcie_cap_flags |= BNX2X_PCIE_CAPABLE_FLAG;

	/* check if MSI capability is enabled */
	caps = pci_find_cap(sc, PCIY_MSI, BNX2X_PCI_CAP);
	if (NULL != caps) {
		PMD_DRV_LOG(DEBUG, sc, "Found MSI capability at 0x%04x", reg);

		sc->devinfo.pcie_cap_flags |= BNX2X_MSI_CAPABLE_FLAG;
		sc->devinfo.pcie_msi_cap_reg = caps->addr;
	}

	/* check if MSI-X capability is enabled */
	caps = pci_find_cap(sc, PCIY_MSIX, BNX2X_PCI_CAP);
	if (NULL != caps) {
		PMD_DRV_LOG(DEBUG, sc, "Found MSI-X capability at 0x%04x", reg);

		sc->devinfo.pcie_cap_flags |= BNX2X_MSIX_CAPABLE_FLAG;
		sc->devinfo.pcie_msix_cap_reg = caps->addr;
	}
}

static int bnx2x_get_shmem_mf_cfg_info_sd(struct bnx2x_softc *sc)
{
	struct bnx2x_mf_info *mf_info = &sc->devinfo.mf_info;
	uint32_t val;

	/* get the outer vlan if we're in switch-dependent mode */

	val = MFCFG_RD(sc, func_mf_config[SC_ABS_FUNC(sc)].e1hov_tag);
	mf_info->ext_id = (uint16_t) val;

	mf_info->multi_vnics_mode = 1;

	if (!VALID_OVLAN(mf_info->ext_id)) {
		PMD_DRV_LOG(NOTICE, sc, "Invalid VLAN (%d)", mf_info->ext_id);
		return 1;
	}

	/* get the capabilities */
	if ((mf_info->mf_config[SC_VN(sc)] & FUNC_MF_CFG_PROTOCOL_MASK) ==
	    FUNC_MF_CFG_PROTOCOL_ISCSI) {
		mf_info->mf_protos_supported |= MF_PROTO_SUPPORT_ISCSI;
	} else if ((mf_info->mf_config[SC_VN(sc)] & FUNC_MF_CFG_PROTOCOL_MASK)
		   == FUNC_MF_CFG_PROTOCOL_FCOE) {
		mf_info->mf_protos_supported |= MF_PROTO_SUPPORT_FCOE;
	} else {
		mf_info->mf_protos_supported |= MF_PROTO_SUPPORT_ETHERNET;
	}

	mf_info->vnics_per_port =
	    (CHIP_PORT_MODE(sc) == CHIP_4_PORT_MODE) ? 2 : 4;

	return 0;
}

static uint32_t bnx2x_get_shmem_ext_proto_support_flags(struct bnx2x_softc *sc)
{
	uint32_t retval = 0;
	uint32_t val;

	val = MFCFG_RD(sc, func_ext_config[SC_ABS_FUNC(sc)].func_cfg);

	if (val & MACP_FUNC_CFG_FLAGS_ENABLED) {
		if (val & MACP_FUNC_CFG_FLAGS_ETHERNET) {
			retval |= MF_PROTO_SUPPORT_ETHERNET;
		}
		if (val & MACP_FUNC_CFG_FLAGS_ISCSI_OFFLOAD) {
			retval |= MF_PROTO_SUPPORT_ISCSI;
		}
		if (val & MACP_FUNC_CFG_FLAGS_FCOE_OFFLOAD) {
			retval |= MF_PROTO_SUPPORT_FCOE;
		}
	}

	return retval;
}

static int bnx2x_get_shmem_mf_cfg_info_si(struct bnx2x_softc *sc)
{
	struct bnx2x_mf_info *mf_info = &sc->devinfo.mf_info;
	uint32_t val;

	/*
	 * There is no outer vlan if we're in switch-independent mode.
	 * If the mac is valid then assume multi-function.
	 */

	val = MFCFG_RD(sc, func_ext_config[SC_ABS_FUNC(sc)].func_cfg);

	mf_info->multi_vnics_mode = ((val & MACP_FUNC_CFG_FLAGS_MASK) != 0);

	mf_info->mf_protos_supported =
	    bnx2x_get_shmem_ext_proto_support_flags(sc);

	mf_info->vnics_per_port =
	    (CHIP_PORT_MODE(sc) == CHIP_4_PORT_MODE) ? 2 : 4;

	return 0;
}

static int bnx2x_get_shmem_mf_cfg_info_niv(struct bnx2x_softc *sc)
{
	struct bnx2x_mf_info *mf_info = &sc->devinfo.mf_info;
	uint32_t e1hov_tag;
	uint32_t func_config;
	uint32_t niv_config;

	mf_info->multi_vnics_mode = 1;

	e1hov_tag = MFCFG_RD(sc, func_mf_config[SC_ABS_FUNC(sc)].e1hov_tag);
	func_config = MFCFG_RD(sc, func_mf_config[SC_ABS_FUNC(sc)].config);
	niv_config = MFCFG_RD(sc, func_mf_config[SC_ABS_FUNC(sc)].afex_config);

	mf_info->ext_id =
	    (uint16_t) ((e1hov_tag & FUNC_MF_CFG_E1HOV_TAG_MASK) >>
			FUNC_MF_CFG_E1HOV_TAG_SHIFT);

	mf_info->default_vlan =
	    (uint16_t) ((e1hov_tag & FUNC_MF_CFG_AFEX_VLAN_MASK) >>
			FUNC_MF_CFG_AFEX_VLAN_SHIFT);

	mf_info->niv_allowed_priorities =
	    (uint8_t) ((niv_config & FUNC_MF_CFG_AFEX_COS_FILTER_MASK) >>
		       FUNC_MF_CFG_AFEX_COS_FILTER_SHIFT);

	mf_info->niv_default_cos =
	    (uint8_t) ((func_config & FUNC_MF_CFG_TRANSMIT_PRIORITY_MASK) >>
		       FUNC_MF_CFG_TRANSMIT_PRIORITY_SHIFT);

	mf_info->afex_vlan_mode =
	    ((niv_config & FUNC_MF_CFG_AFEX_VLAN_MODE_MASK) >>
	     FUNC_MF_CFG_AFEX_VLAN_MODE_SHIFT);

	mf_info->niv_mba_enabled =
	    ((niv_config & FUNC_MF_CFG_AFEX_MBA_ENABLED_MASK) >>
	     FUNC_MF_CFG_AFEX_MBA_ENABLED_SHIFT);

	mf_info->mf_protos_supported =
	    bnx2x_get_shmem_ext_proto_support_flags(sc);

	mf_info->vnics_per_port =
	    (CHIP_PORT_MODE(sc) == CHIP_4_PORT_MODE) ? 2 : 4;

	return 0;
}

static int bnx2x_check_valid_mf_cfg(struct bnx2x_softc *sc)
{
	struct bnx2x_mf_info *mf_info = &sc->devinfo.mf_info;
	uint32_t mf_cfg1;
	uint32_t mf_cfg2;
	uint32_t ovlan1;
	uint32_t ovlan2;
	uint8_t i, j;

	/* various MF mode sanity checks... */

	if (mf_info->mf_config[SC_VN(sc)] & FUNC_MF_CFG_FUNC_HIDE) {
		PMD_DRV_LOG(NOTICE, sc,
			    "Enumerated function %d is marked as hidden",
			    SC_PORT(sc));
		return 1;
	}

	if ((mf_info->vnics_per_port > 1) && !mf_info->multi_vnics_mode) {
		PMD_DRV_LOG(NOTICE, sc, "vnics_per_port=%d multi_vnics_mode=%d",
			    mf_info->vnics_per_port, mf_info->multi_vnics_mode);
		return 1;
	}

	if (mf_info->mf_mode == MULTI_FUNCTION_SD) {
/* vnic id > 0 must have valid ovlan in switch-dependent mode */
		if ((SC_VN(sc) > 0) && !VALID_OVLAN(OVLAN(sc))) {
			PMD_DRV_LOG(NOTICE, sc, "mf_mode=SD vnic_id=%d ovlan=%d",
				    SC_VN(sc), OVLAN(sc));
			return 1;
		}

		if (!VALID_OVLAN(OVLAN(sc)) && mf_info->multi_vnics_mode) {
			PMD_DRV_LOG(NOTICE, sc,
				    "mf_mode=SD multi_vnics_mode=%d ovlan=%d",
				    mf_info->multi_vnics_mode, OVLAN(sc));
			return 1;
		}

/*
 * Verify all functions are either MF or SF mode. If MF, make sure
 * sure that all non-hidden functions have a valid ovlan. If SF,
 * make sure that all non-hidden functions have an invalid ovlan.
 */
		FOREACH_ABS_FUNC_IN_PORT(sc, i) {
			mf_cfg1 = MFCFG_RD(sc, func_mf_config[i].config);
			ovlan1 = MFCFG_RD(sc, func_mf_config[i].e1hov_tag);
			if (!(mf_cfg1 & FUNC_MF_CFG_FUNC_HIDE) &&
			    (((mf_info->multi_vnics_mode)
			      && !VALID_OVLAN(ovlan1))
			     || ((!mf_info->multi_vnics_mode)
				 && VALID_OVLAN(ovlan1)))) {
				PMD_DRV_LOG(NOTICE, sc,
					    "mf_mode=SD function %d MF config "
					    "mismatch, multi_vnics_mode=%d ovlan=%d",
					    i, mf_info->multi_vnics_mode,
					    ovlan1);
				return 1;
			}
		}

/* Verify all funcs on the same port each have a different ovlan. */
		FOREACH_ABS_FUNC_IN_PORT(sc, i) {
			mf_cfg1 = MFCFG_RD(sc, func_mf_config[i].config);
			ovlan1 = MFCFG_RD(sc, func_mf_config[i].e1hov_tag);
			/* iterate from the next function on the port to the max func */
			for (j = i + 2; j < MAX_FUNC_NUM; j += 2) {
				mf_cfg2 =
				    MFCFG_RD(sc, func_mf_config[j].config);
				ovlan2 =
				    MFCFG_RD(sc, func_mf_config[j].e1hov_tag);
				if (!(mf_cfg1 & FUNC_MF_CFG_FUNC_HIDE)
				    && VALID_OVLAN(ovlan1)
				    && !(mf_cfg2 & FUNC_MF_CFG_FUNC_HIDE)
				    && VALID_OVLAN(ovlan2)
				    && (ovlan1 == ovlan2)) {
					PMD_DRV_LOG(NOTICE, sc,
						    "mf_mode=SD functions %d and %d "
						    "have the same ovlan (%d)",
						    i, j, ovlan1);
					return 1;
				}
			}
		}
	}
	/* MULTI_FUNCTION_SD */
	return 0;
}

static int bnx2x_get_mf_cfg_info(struct bnx2x_softc *sc)
{
	struct bnx2x_mf_info *mf_info = &sc->devinfo.mf_info;
	uint32_t val, mac_upper;
	uint8_t i, vnic;

	/* initialize mf_info defaults */
	mf_info->vnics_per_port = 1;
	mf_info->multi_vnics_mode = FALSE;
	mf_info->path_has_ovlan = FALSE;
	mf_info->mf_mode = SINGLE_FUNCTION;

	if (!CHIP_IS_MF_CAP(sc)) {
		return 0;
	}

	if (sc->devinfo.mf_cfg_base == SHMEM_MF_CFG_ADDR_NONE) {
		PMD_DRV_LOG(NOTICE, sc, "Invalid mf_cfg_base!");
		return 1;
	}

	/* get the MF mode (switch dependent / independent / single-function) */

	val = SHMEM_RD(sc, dev_info.shared_feature_config.config);

	switch (val & SHARED_FEAT_CFG_FORCE_SF_MODE_MASK) {
	case SHARED_FEAT_CFG_FORCE_SF_MODE_SWITCH_INDEPT:

		mac_upper =
		    MFCFG_RD(sc, func_mf_config[SC_ABS_FUNC(sc)].mac_upper);

		/* check for legal upper mac bytes */
		if (mac_upper != FUNC_MF_CFG_UPPERMAC_DEFAULT) {
			mf_info->mf_mode = MULTI_FUNCTION_SI;
		} else {
			PMD_DRV_LOG(NOTICE, sc,
				    "Invalid config for Switch Independent mode");
		}

		break;

	case SHARED_FEAT_CFG_FORCE_SF_MODE_MF_ALLOWED:
	case SHARED_FEAT_CFG_FORCE_SF_MODE_SPIO4:

		/* get outer vlan configuration */
		val = MFCFG_RD(sc, func_mf_config[SC_ABS_FUNC(sc)].e1hov_tag);

		if ((val & FUNC_MF_CFG_E1HOV_TAG_MASK) !=
		    FUNC_MF_CFG_E1HOV_TAG_DEFAULT) {
			mf_info->mf_mode = MULTI_FUNCTION_SD;
		} else {
			PMD_DRV_LOG(NOTICE, sc,
				    "Invalid config for Switch Dependent mode");
		}

		break;

	case SHARED_FEAT_CFG_FORCE_SF_MODE_FORCED_SF:

		/* not in MF mode, vnics_per_port=1 and multi_vnics_mode=FALSE */
		return 0;

	case SHARED_FEAT_CFG_FORCE_SF_MODE_AFEX_MODE:

		/*
		 * Mark MF mode as NIV if MCP version includes NPAR-SD support
		 * and the MAC address is valid.
		 */
		mac_upper =
		    MFCFG_RD(sc, func_mf_config[SC_ABS_FUNC(sc)].mac_upper);

		if ((SHMEM2_HAS(sc, afex_driver_support)) &&
		    (mac_upper != FUNC_MF_CFG_UPPERMAC_DEFAULT)) {
			mf_info->mf_mode = MULTI_FUNCTION_AFEX;
		} else {
			PMD_DRV_LOG(NOTICE, sc, "Invalid config for AFEX mode");
		}

		break;

	default:

		PMD_DRV_LOG(NOTICE, sc, "Unknown MF mode (0x%08x)",
			    (val & SHARED_FEAT_CFG_FORCE_SF_MODE_MASK));

		return 1;
	}

	/* set path mf_mode (which could be different than function mf_mode) */
	if (mf_info->mf_mode == MULTI_FUNCTION_SD) {
		mf_info->path_has_ovlan = TRUE;
	} else if (mf_info->mf_mode == SINGLE_FUNCTION) {
/*
 * Decide on path multi vnics mode. If we're not in MF mode and in
 * 4-port mode, this is good enough to check vnic-0 of the other port
 * on the same path
 */
		if (CHIP_PORT_MODE(sc) == CHIP_4_PORT_MODE) {
			uint8_t other_port = !(PORT_ID(sc) & 1);
			uint8_t abs_func_other_port =
			    (SC_PATH(sc) + (2 * other_port));

			val =
			    MFCFG_RD(sc,
				     func_mf_config
				     [abs_func_other_port].e1hov_tag);

			mf_info->path_has_ovlan = VALID_OVLAN((uint16_t) val);
		}
	}

	if (mf_info->mf_mode == SINGLE_FUNCTION) {
/* invalid MF config */
		if (SC_VN(sc) >= 1) {
			PMD_DRV_LOG(NOTICE, sc, "VNIC ID >= 1 in SF mode");
			return 1;
		}

		return 0;
	}

	/* get the MF configuration */
	mf_info->mf_config[SC_VN(sc)] =
	    MFCFG_RD(sc, func_mf_config[SC_ABS_FUNC(sc)].config);

	switch (mf_info->mf_mode) {
	case MULTI_FUNCTION_SD:

		bnx2x_get_shmem_mf_cfg_info_sd(sc);
		break;

	case MULTI_FUNCTION_SI:

		bnx2x_get_shmem_mf_cfg_info_si(sc);
		break;

	case MULTI_FUNCTION_AFEX:

		bnx2x_get_shmem_mf_cfg_info_niv(sc);
		break;

	default:

		PMD_DRV_LOG(NOTICE, sc, "Get MF config failed (mf_mode=0x%08x)",
			    mf_info->mf_mode);
		return 1;
	}

	/* get the congestion management parameters */

	vnic = 0;
	FOREACH_ABS_FUNC_IN_PORT(sc, i) {
/* get min/max bw */
		val = MFCFG_RD(sc, func_mf_config[i].config);
		mf_info->min_bw[vnic] =
		    ((val & FUNC_MF_CFG_MIN_BW_MASK) >>
		     FUNC_MF_CFG_MIN_BW_SHIFT);
		mf_info->max_bw[vnic] =
		    ((val & FUNC_MF_CFG_MAX_BW_MASK) >>
		     FUNC_MF_CFG_MAX_BW_SHIFT);
		vnic++;
	}

	return bnx2x_check_valid_mf_cfg(sc);
}

static int bnx2x_get_shmem_info(struct bnx2x_softc *sc)
{
	int port;
	uint32_t mac_hi, mac_lo, val;

	PMD_INIT_FUNC_TRACE(sc);

	port = SC_PORT(sc);
	mac_hi = mac_lo = 0;

	sc->link_params.sc = sc;
	sc->link_params.port = port;

	/* get the hardware config info */
	sc->devinfo.hw_config = SHMEM_RD(sc, dev_info.shared_hw_config.config);
	sc->devinfo.hw_config2 =
	    SHMEM_RD(sc, dev_info.shared_hw_config.config2);

	sc->link_params.hw_led_mode =
	    ((sc->devinfo.hw_config & SHARED_HW_CFG_LED_MODE_MASK) >>
	     SHARED_HW_CFG_LED_MODE_SHIFT);

	/* get the port feature config */
	sc->port.config =
	    SHMEM_RD(sc, dev_info.port_feature_config[port].config);

	/* get the link params */
	sc->link_params.speed_cap_mask[ELINK_INT_PHY] =
	    SHMEM_RD(sc, dev_info.port_hw_config[port].speed_capability_mask)
	    & PORT_HW_CFG_SPEED_CAPABILITY_D0_MASK;
	sc->link_params.speed_cap_mask[ELINK_EXT_PHY1] =
	    SHMEM_RD(sc, dev_info.port_hw_config[port].speed_capability_mask2)
	    & PORT_HW_CFG_SPEED_CAPABILITY_D0_MASK;

	/* get the lane config */
	sc->link_params.lane_config =
	    SHMEM_RD(sc, dev_info.port_hw_config[port].lane_config);

	/* get the link config */
	val = SHMEM_RD(sc, dev_info.port_feature_config[port].link_config);
	sc->port.link_config[ELINK_INT_PHY] = val;
	sc->link_params.switch_cfg = (val & PORT_FEATURE_CONNECTED_SWITCH_MASK);
	sc->port.link_config[ELINK_EXT_PHY1] =
	    SHMEM_RD(sc, dev_info.port_feature_config[port].link_config2);

	/* get the override preemphasis flag and enable it or turn it off */
	val = SHMEM_RD(sc, dev_info.shared_feature_config.config);
	if (val & SHARED_FEAT_CFG_OVERRIDE_PREEMPHASIS_CFG_ENABLED) {
		sc->link_params.feature_config_flags |=
		    ELINK_FEATURE_CONFIG_OVERRIDE_PREEMPHASIS_ENABLED;
	} else {
		sc->link_params.feature_config_flags &=
		    ~ELINK_FEATURE_CONFIG_OVERRIDE_PREEMPHASIS_ENABLED;
	}

	val = sc->devinfo.bc_ver >> 8;
	if (val < BNX2X_BC_VER) {
		/* for now only warn later we might need to enforce this */
		PMD_DRV_LOG(NOTICE, sc, "This driver needs bc_ver %X but found %X, please upgrade BC\n",
			    BNX2X_BC_VER, val);
	}
	sc->link_params.feature_config_flags |=
				(val >= REQ_BC_VER_4_VRFY_FIRST_PHY_OPT_MDL) ?
				ELINK_FEATURE_CONFIG_BC_SUPPORTS_OPT_MDL_VRFY :
				0;

	sc->link_params.feature_config_flags |=
		(val >= REQ_BC_VER_4_VRFY_SPECIFIC_PHY_OPT_MDL) ?
		ELINK_FEATURE_CONFIG_BC_SUPPORTS_DUAL_PHY_OPT_MDL_VRFY : 0;
	sc->link_params.feature_config_flags |=
		(val >= REQ_BC_VER_4_VRFY_AFEX_SUPPORTED) ?
		ELINK_FEATURE_CONFIG_BC_SUPPORTS_AFEX : 0;
	sc->link_params.feature_config_flags |=
		(val >= REQ_BC_VER_4_SFP_TX_DISABLE_SUPPORTED) ?
		ELINK_FEATURE_CONFIG_BC_SUPPORTS_SFP_TX_DISABLED : 0;

	/* get the initial value of the link params */
	sc->link_params.multi_phy_config =
	    SHMEM_RD(sc, dev_info.port_hw_config[port].multi_phy_config);

	/* get external phy info */
	sc->port.ext_phy_config =
	    SHMEM_RD(sc, dev_info.port_hw_config[port].external_phy_config);

	/* get the multifunction configuration */
	bnx2x_get_mf_cfg_info(sc);

	/* get the mac address */
	if (IS_MF(sc)) {
		mac_hi =
		    MFCFG_RD(sc, func_mf_config[SC_ABS_FUNC(sc)].mac_upper);
		mac_lo =
		    MFCFG_RD(sc, func_mf_config[SC_ABS_FUNC(sc)].mac_lower);
	} else {
		mac_hi = SHMEM_RD(sc, dev_info.port_hw_config[port].mac_upper);
		mac_lo = SHMEM_RD(sc, dev_info.port_hw_config[port].mac_lower);
	}

	if ((mac_lo == 0) && (mac_hi == 0)) {
		*sc->mac_addr_str = 0;
		PMD_DRV_LOG(NOTICE, sc, "No Ethernet address programmed!");
	} else {
		sc->link_params.mac_addr[0] = (uint8_t) (mac_hi >> 8);
		sc->link_params.mac_addr[1] = (uint8_t) (mac_hi);
		sc->link_params.mac_addr[2] = (uint8_t) (mac_lo >> 24);
		sc->link_params.mac_addr[3] = (uint8_t) (mac_lo >> 16);
		sc->link_params.mac_addr[4] = (uint8_t) (mac_lo >> 8);
		sc->link_params.mac_addr[5] = (uint8_t) (mac_lo);
		snprintf(sc->mac_addr_str, sizeof(sc->mac_addr_str),
			 RTE_ETHER_ADDR_PRT_FMT,
			 sc->link_params.mac_addr[0],
			 sc->link_params.mac_addr[1],
			 sc->link_params.mac_addr[2],
			 sc->link_params.mac_addr[3],
			 sc->link_params.mac_addr[4],
			 sc->link_params.mac_addr[5]);
		PMD_DRV_LOG(DEBUG, sc,
			    "Ethernet address: %s", sc->mac_addr_str);
	}

	return 0;
}

static void bnx2x_media_detect(struct bnx2x_softc *sc)
{
	uint32_t phy_idx = bnx2x_get_cur_phy_idx(sc);
	switch (sc->link_params.phy[phy_idx].media_type) {
	case ELINK_ETH_PHY_SFPP_10G_FIBER:
	case ELINK_ETH_PHY_SFP_1G_FIBER:
	case ELINK_ETH_PHY_XFP_FIBER:
	case ELINK_ETH_PHY_KR:
	case ELINK_ETH_PHY_CX4:
		PMD_DRV_LOG(INFO, sc, "Found 10GBase-CX4 media.");
		sc->media = IFM_10G_CX4;
		break;
	case ELINK_ETH_PHY_DA_TWINAX:
		PMD_DRV_LOG(INFO, sc, "Found 10Gb Twinax media.");
		sc->media = IFM_10G_TWINAX;
		break;
	case ELINK_ETH_PHY_BASE_T:
		PMD_DRV_LOG(INFO, sc, "Found 10GBase-T media.");
		sc->media = IFM_10G_T;
		break;
	case ELINK_ETH_PHY_NOT_PRESENT:
		PMD_DRV_LOG(INFO, sc, "Media not present.");
		sc->media = 0;
		break;
	case ELINK_ETH_PHY_UNSPECIFIED:
	default:
		PMD_DRV_LOG(INFO, sc, "Unknown media!");
		sc->media = 0;
		break;
	}
}

#define GET_FIELD(value, fname)                     \
(((value) & (fname##_MASK)) >> (fname##_SHIFT))
#define IGU_FID(val) GET_FIELD((val), IGU_REG_MAPPING_MEMORY_FID)
#define IGU_VEC(val) GET_FIELD((val), IGU_REG_MAPPING_MEMORY_VECTOR)

static int bnx2x_get_igu_cam_info(struct bnx2x_softc *sc)
{
	int pfid = SC_FUNC(sc);
	int igu_sb_id;
	uint32_t val;
	uint8_t fid, igu_sb_cnt = 0;

	sc->igu_base_sb = 0xff;

	if (CHIP_INT_MODE_IS_BC(sc)) {
		int vn = SC_VN(sc);
		igu_sb_cnt = sc->igu_sb_cnt;
		sc->igu_base_sb = ((CHIP_IS_MODE_4_PORT(sc) ? pfid : vn) *
				   FP_SB_MAX_E1x);
		sc->igu_dsb_id = (E1HVN_MAX * FP_SB_MAX_E1x +
				  (CHIP_IS_MODE_4_PORT(sc) ? pfid : vn));
		return 0;
	}

	/* IGU in normal mode - read CAM */
	for (igu_sb_id = 0;
	     igu_sb_id < IGU_REG_MAPPING_MEMORY_SIZE; igu_sb_id++) {
		val = REG_RD(sc, IGU_REG_MAPPING_MEMORY + igu_sb_id * 4);
		if (!(val & IGU_REG_MAPPING_MEMORY_VALID)) {
			continue;
		}
		fid = IGU_FID(val);
		if (fid & IGU_FID_ENCODE_IS_PF) {
			if ((fid & IGU_FID_PF_NUM_MASK) != pfid) {
				continue;
			}
			if (IGU_VEC(val) == 0) {
				/* default status block */
				sc->igu_dsb_id = igu_sb_id;
			} else {
				if (sc->igu_base_sb == 0xff) {
					sc->igu_base_sb = igu_sb_id;
				}
				igu_sb_cnt++;
			}
		}
	}

	/*
	 * Due to new PF resource allocation by MFW T7.4 and above, it's optional
	 * that number of CAM entries will not be equal to the value advertised in
	 * PCI. Driver should use the minimal value of both as the actual status
	 * block count
	 */
	sc->igu_sb_cnt = min(sc->igu_sb_cnt, igu_sb_cnt);

	if (igu_sb_cnt == 0) {
		PMD_DRV_LOG(ERR, sc, "CAM configuration error");
		return -1;
	}

	return 0;
}

/*
* Gather various information from the device config space, the device itself,
* shmem, and the user input.
*/
static int bnx2x_get_device_info(struct bnx2x_softc *sc)
{
	uint32_t val;
	int rc;

	/* get the chip revision (chip metal comes from pci config space) */
	sc->devinfo.chip_id = sc->link_params.chip_id =
	    (((REG_RD(sc, MISC_REG_CHIP_NUM) & 0xffff) << 16) |
	     ((REG_RD(sc, MISC_REG_CHIP_REV) & 0xf) << 12) |
	     (((REG_RD(sc, PCICFG_OFFSET + PCI_ID_VAL3) >> 24) & 0xf) << 4) |
	     ((REG_RD(sc, MISC_REG_BOND_ID) & 0xf) << 0));

	/* force 57811 according to MISC register */
	if (REG_RD(sc, MISC_REG_CHIP_TYPE) & MISC_REG_CHIP_TYPE_57811_MASK) {
		if (CHIP_IS_57810(sc)) {
			sc->devinfo.chip_id = ((CHIP_NUM_57811 << 16) |
					       (sc->
						devinfo.chip_id & 0x0000ffff));
		} else if (CHIP_IS_57810_MF(sc)) {
			sc->devinfo.chip_id = ((CHIP_NUM_57811_MF << 16) |
					       (sc->
						devinfo.chip_id & 0x0000ffff));
		}
		sc->devinfo.chip_id |= 0x1;
	}

	PMD_DRV_LOG(DEBUG, sc,
		    "chip_id=0x%08x (num=0x%04x rev=0x%01x metal=0x%02x bond=0x%01x)",
		    sc->devinfo.chip_id,
		    ((sc->devinfo.chip_id >> 16) & 0xffff),
		    ((sc->devinfo.chip_id >> 12) & 0xf),
		    ((sc->devinfo.chip_id >> 4) & 0xff),
		    ((sc->devinfo.chip_id >> 0) & 0xf));

	val = (REG_RD(sc, 0x2874) & 0x55);
	if ((sc->devinfo.chip_id & 0x1) || (CHIP_IS_E1H(sc) && (val == 0x55))) {
		sc->flags |= BNX2X_ONE_PORT_FLAG;
		PMD_DRV_LOG(DEBUG, sc, "single port device");
	}

	/* set the doorbell size */
	sc->doorbell_size = (1 << BNX2X_DB_SHIFT);

	/* determine whether the device is in 2 port or 4 port mode */
	sc->devinfo.chip_port_mode = CHIP_PORT_MODE_NONE;	/* E1h */
	if (CHIP_IS_E2E3(sc)) {
/*
 * Read port4mode_en_ovwr[0]:
 *   If 1, four port mode is in port4mode_en_ovwr[1].
 *   If 0, four port mode is in port4mode_en[0].
 */
		val = REG_RD(sc, MISC_REG_PORT4MODE_EN_OVWR);
		if (val & 1) {
			val = ((val >> 1) & 1);
		} else {
			val = REG_RD(sc, MISC_REG_PORT4MODE_EN);
		}

		sc->devinfo.chip_port_mode =
		    (val) ? CHIP_4_PORT_MODE : CHIP_2_PORT_MODE;

		PMD_DRV_LOG(DEBUG, sc, "Port mode = %s", (val) ? "4" : "2");
	}

	/* get the function and path info for the device */
	bnx2x_get_function_num(sc);

	/* get the shared memory base address */
	sc->devinfo.shmem_base =
	    sc->link_params.shmem_base = REG_RD(sc, MISC_REG_SHARED_MEM_ADDR);
	sc->devinfo.shmem2_base =
	    REG_RD(sc, (SC_PATH(sc) ? MISC_REG_GENERIC_CR_1 :
			MISC_REG_GENERIC_CR_0));

	if (!sc->devinfo.shmem_base) {
/* this should ONLY prevent upcoming shmem reads */
		PMD_DRV_LOG(INFO, sc, "MCP not active");
		sc->flags |= BNX2X_NO_MCP_FLAG;
		return 0;
	}

	/* make sure the shared memory contents are valid */
	val = SHMEM_RD(sc, validity_map[SC_PORT(sc)]);
	if ((val & (SHR_MEM_VALIDITY_DEV_INFO | SHR_MEM_VALIDITY_MB)) !=
	    (SHR_MEM_VALIDITY_DEV_INFO | SHR_MEM_VALIDITY_MB)) {
		PMD_DRV_LOG(NOTICE, sc, "Invalid SHMEM validity signature: 0x%08x",
			    val);
		return 0;
	}

	/* get the bootcode version */
	sc->devinfo.bc_ver = SHMEM_RD(sc, dev_info.bc_rev);
	snprintf(sc->devinfo.bc_ver_str,
		 sizeof(sc->devinfo.bc_ver_str),
		 "%d.%d.%d",
		 ((sc->devinfo.bc_ver >> 24) & 0xff),
		 ((sc->devinfo.bc_ver >> 16) & 0xff),
		 ((sc->devinfo.bc_ver >> 8) & 0xff));
	PMD_DRV_LOG(DEBUG, sc, "Bootcode version: %s", sc->devinfo.bc_ver_str);

	/* get the bootcode shmem address */
	sc->devinfo.mf_cfg_base = bnx2x_get_shmem_mf_cfg_base(sc);

	/* clean indirect addresses as they're not used */
	pci_write_long(sc, PCICFG_GRC_ADDRESS, 0);
	if (IS_PF(sc)) {
		REG_WR(sc, PXP2_REG_PGL_ADDR_88_F0, 0);
		REG_WR(sc, PXP2_REG_PGL_ADDR_8C_F0, 0);
		REG_WR(sc, PXP2_REG_PGL_ADDR_90_F0, 0);
		REG_WR(sc, PXP2_REG_PGL_ADDR_94_F0, 0);
		if (CHIP_IS_E1x(sc)) {
			REG_WR(sc, PXP2_REG_PGL_ADDR_88_F1, 0);
			REG_WR(sc, PXP2_REG_PGL_ADDR_8C_F1, 0);
			REG_WR(sc, PXP2_REG_PGL_ADDR_90_F1, 0);
			REG_WR(sc, PXP2_REG_PGL_ADDR_94_F1, 0);
		}
	}

	/* get the nvram size */
	val = REG_RD(sc, MCP_REG_MCPR_NVM_CFG4);
	sc->devinfo.flash_size =
	    (NVRAM_1MB_SIZE << (val & MCPR_NVM_CFG4_FLASH_SIZE));

	bnx2x_set_power_state(sc, PCI_PM_D0);
	/* get various configuration parameters from shmem */
	bnx2x_get_shmem_info(sc);

	/* initialize IGU parameters */
	if (CHIP_IS_E1x(sc)) {
		sc->devinfo.int_block = INT_BLOCK_HC;
		sc->igu_dsb_id = DEF_SB_IGU_ID;
		sc->igu_base_sb = 0;
	} else {
		sc->devinfo.int_block = INT_BLOCK_IGU;

/* do not allow device reset during IGU info preocessing */
		bnx2x_acquire_hw_lock(sc, HW_LOCK_RESOURCE_RESET);

		val = REG_RD(sc, IGU_REG_BLOCK_CONFIGURATION);

		if (val & IGU_BLOCK_CONFIGURATION_REG_BACKWARD_COMP_EN) {
			int tout = 5000;

			val &= ~(IGU_BLOCK_CONFIGURATION_REG_BACKWARD_COMP_EN);
			REG_WR(sc, IGU_REG_BLOCK_CONFIGURATION, val);
			REG_WR(sc, IGU_REG_RESET_MEMORIES, 0x7f);

			while (tout && REG_RD(sc, IGU_REG_RESET_MEMORIES)) {
				tout--;
				DELAY(1000);
			}

			if (REG_RD(sc, IGU_REG_RESET_MEMORIES)) {
				PMD_DRV_LOG(NOTICE, sc,
					    "FORCING IGU Normal Mode failed!!!");
				bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_RESET);
				return -1;
			}
		}

		if (val & IGU_BLOCK_CONFIGURATION_REG_BACKWARD_COMP_EN) {
			PMD_DRV_LOG(DEBUG, sc, "IGU Backward Compatible Mode");
			sc->devinfo.int_block |= INT_BLOCK_MODE_BW_COMP;
		} else {
			PMD_DRV_LOG(DEBUG, sc, "IGU Normal Mode");
		}

		rc = bnx2x_get_igu_cam_info(sc);

		bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_RESET);

		if (rc) {
			return rc;
		}
	}

	/*
	 * Get base FW non-default (fast path) status block ID. This value is
	 * used to initialize the fw_sb_id saved on the fp/queue structure to
	 * determine the id used by the FW.
	 */
	if (CHIP_IS_E1x(sc)) {
		sc->base_fw_ndsb =
		    ((SC_PORT(sc) * FP_SB_MAX_E1x) + SC_L_ID(sc));
	} else {
/*
 * 57712+ - We currently use one FW SB per IGU SB (Rx and Tx of
 * the same queue are indicated on the same IGU SB). So we prefer
 * FW and IGU SBs to be the same value.
 */
		sc->base_fw_ndsb = sc->igu_base_sb;
	}

	elink_phy_probe(&sc->link_params);

	return 0;
}

static void
bnx2x_link_settings_supported(struct bnx2x_softc *sc, uint32_t switch_cfg)
{
	uint32_t cfg_size = 0;
	uint32_t idx;
	uint8_t port = SC_PORT(sc);

	/* aggregation of supported attributes of all external phys */
	sc->port.supported[0] = 0;
	sc->port.supported[1] = 0;

	switch (sc->link_params.num_phys) {
	case 1:
		sc->port.supported[0] =
		    sc->link_params.phy[ELINK_INT_PHY].supported;
		cfg_size = 1;
		break;
	case 2:
		sc->port.supported[0] =
		    sc->link_params.phy[ELINK_EXT_PHY1].supported;
		cfg_size = 1;
		break;
	case 3:
		if (sc->link_params.multi_phy_config &
		    PORT_HW_CFG_PHY_SWAPPED_ENABLED) {
			sc->port.supported[1] =
			    sc->link_params.phy[ELINK_EXT_PHY1].supported;
			sc->port.supported[0] =
			    sc->link_params.phy[ELINK_EXT_PHY2].supported;
		} else {
			sc->port.supported[0] =
			    sc->link_params.phy[ELINK_EXT_PHY1].supported;
			sc->port.supported[1] =
			    sc->link_params.phy[ELINK_EXT_PHY2].supported;
		}
		cfg_size = 2;
		break;
	}

	if (!(sc->port.supported[0] || sc->port.supported[1])) {
		PMD_DRV_LOG(ERR, sc,
			    "Invalid phy config in NVRAM (PHY1=0x%08x PHY2=0x%08x)",
			    SHMEM_RD(sc,
				     dev_info.port_hw_config
				     [port].external_phy_config),
			    SHMEM_RD(sc,
				     dev_info.port_hw_config
				     [port].external_phy_config2));
		return;
	}

	if (CHIP_IS_E3(sc))
		sc->port.phy_addr = REG_RD(sc, MISC_REG_WC0_CTRL_PHY_ADDR);
	else {
		switch (switch_cfg) {
		case ELINK_SWITCH_CFG_1G:
			sc->port.phy_addr =
			    REG_RD(sc,
				   NIG_REG_SERDES0_CTRL_PHY_ADDR + port * 0x10);
			break;
		case ELINK_SWITCH_CFG_10G:
			sc->port.phy_addr =
			    REG_RD(sc,
				   NIG_REG_XGXS0_CTRL_PHY_ADDR + port * 0x18);
			break;
		default:
			PMD_DRV_LOG(ERR, sc,
				    "Invalid switch config in"
				    "link_config=0x%08x",
				    sc->port.link_config[0]);
			return;
		}
	}

	PMD_DRV_LOG(INFO, sc, "PHY addr 0x%08x", sc->port.phy_addr);

	/* mask what we support according to speed_cap_mask per configuration */
	for (idx = 0; idx < cfg_size; idx++) {
		if (!(sc->link_params.speed_cap_mask[idx] &
		      PORT_HW_CFG_SPEED_CAPABILITY_D0_10M_HALF)) {
			sc->port.supported[idx] &=
			    ~ELINK_SUPPORTED_10baseT_Half;
		}

		if (!(sc->link_params.speed_cap_mask[idx] &
		      PORT_HW_CFG_SPEED_CAPABILITY_D0_10M_FULL)) {
			sc->port.supported[idx] &=
			    ~ELINK_SUPPORTED_10baseT_Full;
		}

		if (!(sc->link_params.speed_cap_mask[idx] &
		      PORT_HW_CFG_SPEED_CAPABILITY_D0_100M_HALF)) {
			sc->port.supported[idx] &=
			    ~ELINK_SUPPORTED_100baseT_Half;
		}

		if (!(sc->link_params.speed_cap_mask[idx] &
		      PORT_HW_CFG_SPEED_CAPABILITY_D0_100M_FULL)) {
			sc->port.supported[idx] &=
			    ~ELINK_SUPPORTED_100baseT_Full;
		}

		if (!(sc->link_params.speed_cap_mask[idx] &
		      PORT_HW_CFG_SPEED_CAPABILITY_D0_1G)) {
			sc->port.supported[idx] &=
			    ~ELINK_SUPPORTED_1000baseT_Full;
		}

		if (!(sc->link_params.speed_cap_mask[idx] &
		      PORT_HW_CFG_SPEED_CAPABILITY_D0_2_5G)) {
			sc->port.supported[idx] &=
			    ~ELINK_SUPPORTED_2500baseX_Full;
		}

		if (!(sc->link_params.speed_cap_mask[idx] &
		      PORT_HW_CFG_SPEED_CAPABILITY_D0_10G)) {
			sc->port.supported[idx] &=
			    ~ELINK_SUPPORTED_10000baseT_Full;
		}

		if (!(sc->link_params.speed_cap_mask[idx] &
		      PORT_HW_CFG_SPEED_CAPABILITY_D0_20G)) {
			sc->port.supported[idx] &=
			    ~ELINK_SUPPORTED_20000baseKR2_Full;
		}
	}

	PMD_DRV_LOG(INFO, sc, "PHY supported 0=0x%08x 1=0x%08x",
		    sc->port.supported[0], sc->port.supported[1]);
}

static void bnx2x_link_settings_requested(struct bnx2x_softc *sc)
{
	uint32_t link_config;
	uint32_t idx;
	uint32_t cfg_size = 0;

	sc->port.advertising[0] = 0;
	sc->port.advertising[1] = 0;

	switch (sc->link_params.num_phys) {
	case 1:
	case 2:
		cfg_size = 1;
		break;
	case 3:
		cfg_size = 2;
		break;
	}

	for (idx = 0; idx < cfg_size; idx++) {
		sc->link_params.req_duplex[idx] = DUPLEX_FULL;
		link_config = sc->port.link_config[idx];

		switch (link_config & PORT_FEATURE_LINK_SPEED_MASK) {
		case PORT_FEATURE_LINK_SPEED_AUTO:
			if (sc->port.supported[idx] & ELINK_SUPPORTED_Autoneg) {
				sc->link_params.req_line_speed[idx] =
				    ELINK_SPEED_AUTO_NEG;
				sc->port.advertising[idx] |=
				    sc->port.supported[idx];
				if (sc->link_params.phy[ELINK_EXT_PHY1].type ==
				    PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BNX2X84833)
					sc->port.advertising[idx] |=
					    (ELINK_SUPPORTED_100baseT_Half |
					     ELINK_SUPPORTED_100baseT_Full);
			} else {
				/* force 10G, no AN */
				sc->link_params.req_line_speed[idx] =
				    ELINK_SPEED_10000;
				sc->port.advertising[idx] |=
				    (ADVERTISED_10000baseT_Full |
				     ADVERTISED_FIBRE);
				continue;
			}
			break;

		case PORT_FEATURE_LINK_SPEED_10M_FULL:
			if (sc->
			    port.supported[idx] & ELINK_SUPPORTED_10baseT_Full)
			{
				sc->link_params.req_line_speed[idx] =
				    ELINK_SPEED_10;
				sc->port.advertising[idx] |=
				    (ADVERTISED_10baseT_Full | ADVERTISED_TP);
			} else {
				PMD_DRV_LOG(ERR, sc,
					    "Invalid NVRAM config link_config=0x%08x "
					    "speed_cap_mask=0x%08x",
					    link_config,
					    sc->
					    link_params.speed_cap_mask[idx]);
				return;
			}
			break;

		case PORT_FEATURE_LINK_SPEED_10M_HALF:
			if (sc->
			    port.supported[idx] & ELINK_SUPPORTED_10baseT_Half)
			{
				sc->link_params.req_line_speed[idx] =
				    ELINK_SPEED_10;
				sc->link_params.req_duplex[idx] = DUPLEX_HALF;
				sc->port.advertising[idx] |=
				    (ADVERTISED_10baseT_Half | ADVERTISED_TP);
			} else {
				PMD_DRV_LOG(ERR, sc,
					    "Invalid NVRAM config link_config=0x%08x "
					    "speed_cap_mask=0x%08x",
					    link_config,
					    sc->
					    link_params.speed_cap_mask[idx]);
				return;
			}
			break;

		case PORT_FEATURE_LINK_SPEED_100M_FULL:
			if (sc->
			    port.supported[idx] & ELINK_SUPPORTED_100baseT_Full)
			{
				sc->link_params.req_line_speed[idx] =
				    ELINK_SPEED_100;
				sc->port.advertising[idx] |=
				    (ADVERTISED_100baseT_Full | ADVERTISED_TP);
			} else {
				PMD_DRV_LOG(ERR, sc,
					    "Invalid NVRAM config link_config=0x%08x "
					    "speed_cap_mask=0x%08x",
					    link_config,
					    sc->
					    link_params.speed_cap_mask[idx]);
				return;
			}
			break;

		case PORT_FEATURE_LINK_SPEED_100M_HALF:
			if (sc->
			    port.supported[idx] & ELINK_SUPPORTED_100baseT_Half)
			{
				sc->link_params.req_line_speed[idx] =
				    ELINK_SPEED_100;
				sc->link_params.req_duplex[idx] = DUPLEX_HALF;
				sc->port.advertising[idx] |=
				    (ADVERTISED_100baseT_Half | ADVERTISED_TP);
			} else {
				PMD_DRV_LOG(ERR, sc,
					    "Invalid NVRAM config link_config=0x%08x "
					    "speed_cap_mask=0x%08x",
					    link_config,
					    sc->
					    link_params.speed_cap_mask[idx]);
				return;
			}
			break;

		case PORT_FEATURE_LINK_SPEED_1G:
			if (sc->port.supported[idx] &
			    ELINK_SUPPORTED_1000baseT_Full) {
				sc->link_params.req_line_speed[idx] =
				    ELINK_SPEED_1000;
				sc->port.advertising[idx] |=
				    (ADVERTISED_1000baseT_Full | ADVERTISED_TP);
			} else {
				PMD_DRV_LOG(ERR, sc,
					    "Invalid NVRAM config link_config=0x%08x "
					    "speed_cap_mask=0x%08x",
					    link_config,
					    sc->
					    link_params.speed_cap_mask[idx]);
				return;
			}
			break;

		case PORT_FEATURE_LINK_SPEED_2_5G:
			if (sc->port.supported[idx] &
			    ELINK_SUPPORTED_2500baseX_Full) {
				sc->link_params.req_line_speed[idx] =
				    ELINK_SPEED_2500;
				sc->port.advertising[idx] |=
				    (ADVERTISED_2500baseX_Full | ADVERTISED_TP);
			} else {
				PMD_DRV_LOG(ERR, sc,
					    "Invalid NVRAM config link_config=0x%08x "
					    "speed_cap_mask=0x%08x",
					    link_config,
					    sc->
					    link_params.speed_cap_mask[idx]);
				return;
			}
			break;

		case PORT_FEATURE_LINK_SPEED_10G_CX4:
			if (sc->port.supported[idx] &
			    ELINK_SUPPORTED_10000baseT_Full) {
				sc->link_params.req_line_speed[idx] =
				    ELINK_SPEED_10000;
				sc->port.advertising[idx] |=
				    (ADVERTISED_10000baseT_Full |
				     ADVERTISED_FIBRE);
			} else {
				PMD_DRV_LOG(ERR, sc,
					    "Invalid NVRAM config link_config=0x%08x "
					    "speed_cap_mask=0x%08x",
					    link_config,
					    sc->
					    link_params.speed_cap_mask[idx]);
				return;
			}
			break;

		case PORT_FEATURE_LINK_SPEED_20G:
			sc->link_params.req_line_speed[idx] = ELINK_SPEED_20000;
			break;

		default:
			PMD_DRV_LOG(ERR, sc,
				    "Invalid NVRAM config link_config=0x%08x "
				    "speed_cap_mask=0x%08x", link_config,
				    sc->link_params.speed_cap_mask[idx]);
			sc->link_params.req_line_speed[idx] =
			    ELINK_SPEED_AUTO_NEG;
			sc->port.advertising[idx] = sc->port.supported[idx];
			break;
		}

		sc->link_params.req_flow_ctrl[idx] =
		    (link_config & PORT_FEATURE_FLOW_CONTROL_MASK);

		if (sc->link_params.req_flow_ctrl[idx] == ELINK_FLOW_CTRL_AUTO) {
			if (!
			    (sc->
			     port.supported[idx] & ELINK_SUPPORTED_Autoneg)) {
				sc->link_params.req_flow_ctrl[idx] =
				    ELINK_FLOW_CTRL_NONE;
			} else {
				bnx2x_set_requested_fc(sc);
			}
		}
	}
}

static void bnx2x_get_phy_info(struct bnx2x_softc *sc)
{
	uint8_t port = SC_PORT(sc);
	uint32_t eee_mode;

	PMD_INIT_FUNC_TRACE(sc);

	/* shmem data already read in bnx2x_get_shmem_info() */

	bnx2x_link_settings_supported(sc, sc->link_params.switch_cfg);
	bnx2x_link_settings_requested(sc);

	/* configure link feature according to nvram value */
	eee_mode =
	    (((SHMEM_RD(sc, dev_info.port_feature_config[port].eee_power_mode))
	      & PORT_FEAT_CFG_EEE_POWER_MODE_MASK) >>
	     PORT_FEAT_CFG_EEE_POWER_MODE_SHIFT);
	if (eee_mode != PORT_FEAT_CFG_EEE_POWER_MODE_DISABLED) {
		sc->link_params.eee_mode = (ELINK_EEE_MODE_ADV_LPI |
					    ELINK_EEE_MODE_ENABLE_LPI |
					    ELINK_EEE_MODE_OUTPUT_TIME);
	} else {
		sc->link_params.eee_mode = 0;
	}

	/* get the media type */
	bnx2x_media_detect(sc);
}

static void bnx2x_set_modes_bitmap(struct bnx2x_softc *sc)
{
	uint32_t flags = MODE_ASIC | MODE_PORT2;

	if (CHIP_IS_E2(sc)) {
		flags |= MODE_E2;
	} else if (CHIP_IS_E3(sc)) {
		flags |= MODE_E3;
		if (CHIP_REV(sc) == CHIP_REV_Ax) {
			flags |= MODE_E3_A0;
		} else {	/*if (CHIP_REV(sc) == CHIP_REV_Bx) */

			flags |= MODE_E3_B0 | MODE_COS3;
		}
	}

	if (IS_MF(sc)) {
		flags |= MODE_MF;
		switch (sc->devinfo.mf_info.mf_mode) {
		case MULTI_FUNCTION_SD:
			flags |= MODE_MF_SD;
			break;
		case MULTI_FUNCTION_SI:
			flags |= MODE_MF_SI;
			break;
		case MULTI_FUNCTION_AFEX:
			flags |= MODE_MF_AFEX;
			break;
		}
	} else {
		flags |= MODE_SF;
	}

#if defined(__LITTLE_ENDIAN)
	flags |= MODE_LITTLE_ENDIAN;
#else /* __BIG_ENDIAN */
	flags |= MODE_BIG_ENDIAN;
#endif

	INIT_MODE_FLAGS(sc) = flags;
}

int bnx2x_alloc_hsi_mem(struct bnx2x_softc *sc)
{
	struct bnx2x_fastpath *fp;
	char buf[32];
	uint32_t i;

	if (IS_PF(sc)) {
		/************************/
		/* DEFAULT STATUS BLOCK */
		/************************/

		if (bnx2x_dma_alloc(sc, sizeof(struct host_sp_status_block),
				  &sc->def_sb_dma, "def_sb",
				  RTE_CACHE_LINE_SIZE) != 0) {
			return -1;
		}

		sc->def_sb =
		    (struct host_sp_status_block *)sc->def_sb_dma.vaddr;
		/***************/
		/* EVENT QUEUE */
		/***************/

		if (bnx2x_dma_alloc(sc, BNX2X_PAGE_SIZE,
				  &sc->eq_dma, "ev_queue",
				  RTE_CACHE_LINE_SIZE) != 0) {
			sc->def_sb = NULL;
			return -1;
		}

		sc->eq = (union event_ring_elem *)sc->eq_dma.vaddr;

		/*************/
		/* SLOW PATH */
		/*************/

		if (bnx2x_dma_alloc(sc, sizeof(struct bnx2x_slowpath),
				  &sc->sp_dma, "sp",
				  RTE_CACHE_LINE_SIZE) != 0) {
			sc->eq = NULL;
			sc->def_sb = NULL;
			return -1;
		}

		sc->sp = (struct bnx2x_slowpath *)sc->sp_dma.vaddr;

		/*******************/
		/* SLOW PATH QUEUE */
		/*******************/

		if (bnx2x_dma_alloc(sc, BNX2X_PAGE_SIZE,
				  &sc->spq_dma, "sp_queue",
				  RTE_CACHE_LINE_SIZE) != 0) {
			sc->sp = NULL;
			sc->eq = NULL;
			sc->def_sb = NULL;
			return -1;
		}

		sc->spq = (struct eth_spe *)sc->spq_dma.vaddr;

		/***************************/
		/* FW DECOMPRESSION BUFFER */
		/***************************/

		if (bnx2x_dma_alloc(sc, FW_BUF_SIZE, &sc->gz_buf_dma,
				  "fw_buf", RTE_CACHE_LINE_SIZE) != 0) {
			sc->spq = NULL;
			sc->sp = NULL;
			sc->eq = NULL;
			sc->def_sb = NULL;
			return -1;
		}

		sc->gz_buf = (void *)sc->gz_buf_dma.vaddr;
	}

	/*************/
	/* FASTPATHS */
	/*************/

	/* allocate DMA memory for each fastpath structure */
	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];
		fp->sc = sc;
		fp->index = i;

		/*******************/
		/* FP STATUS BLOCK */
		/*******************/

		snprintf(buf, sizeof(buf), "fp_%d_sb", i);
		if (bnx2x_dma_alloc(sc, sizeof(union bnx2x_host_hc_status_block),
				  &fp->sb_dma, buf, RTE_CACHE_LINE_SIZE) != 0) {
			PMD_DRV_LOG(NOTICE, sc, "Failed to alloc %s", buf);
			return -1;
		} else {
			if (CHIP_IS_E2E3(sc)) {
				fp->status_block.e2_sb =
				    (struct host_hc_status_block_e2 *)
				    fp->sb_dma.vaddr;
			} else {
				fp->status_block.e1x_sb =
				    (struct host_hc_status_block_e1x *)
				    fp->sb_dma.vaddr;
			}
		}
	}

	return 0;
}

void bnx2x_free_hsi_mem(struct bnx2x_softc *sc)
{
	struct bnx2x_fastpath *fp;
	int i;

	for (i = 0; i < sc->num_queues; i++) {
		fp = &sc->fp[i];

		/*******************/
		/* FP STATUS BLOCK */
		/*******************/

		memset(&fp->status_block, 0, sizeof(fp->status_block));
		bnx2x_dma_free(&fp->sb_dma);
	}

	if (IS_PF(sc)) {
		/***************************/
		/* FW DECOMPRESSION BUFFER */
		/***************************/

		bnx2x_dma_free(&sc->gz_buf_dma);
		sc->gz_buf = NULL;

		/*******************/
		/* SLOW PATH QUEUE */
		/*******************/

		bnx2x_dma_free(&sc->spq_dma);
		sc->spq = NULL;

		/*************/
		/* SLOW PATH */
		/*************/

		bnx2x_dma_free(&sc->sp_dma);
		sc->sp = NULL;

		/***************/
		/* EVENT QUEUE */
		/***************/

		bnx2x_dma_free(&sc->eq_dma);
		sc->eq = NULL;

		/************************/
		/* DEFAULT STATUS BLOCK */
		/************************/

		bnx2x_dma_free(&sc->def_sb_dma);
		sc->def_sb = NULL;
	}
}

/*
* Previous driver DMAE transaction may have occurred when pre-boot stage
* ended and boot began. This would invalidate the addresses of the
* transaction, resulting in was-error bit set in the PCI causing all
* hw-to-host PCIe transactions to timeout. If this happened we want to clear
* the interrupt which detected this from the pglueb and the was-done bit
*/
static void bnx2x_prev_interrupted_dmae(struct bnx2x_softc *sc)
{
	uint32_t val;

	if (!CHIP_IS_E1x(sc)) {
		val = REG_RD(sc, PGLUE_B_REG_PGLUE_B_INT_STS);
		if (val & PGLUE_B_PGLUE_B_INT_STS_REG_WAS_ERROR_ATTN) {
			REG_WR(sc, PGLUE_B_REG_WAS_ERROR_PF_7_0_CLR,
			       1 << SC_FUNC(sc));
		}
	}
}

static int bnx2x_prev_mcp_done(struct bnx2x_softc *sc)
{
	uint32_t rc = bnx2x_fw_command(sc, DRV_MSG_CODE_UNLOAD_DONE,
				     DRV_MSG_CODE_UNLOAD_SKIP_LINK_RESET);
	if (!rc) {
		PMD_DRV_LOG(NOTICE, sc, "MCP response failure, aborting");
		return -1;
	}

	return 0;
}

static struct bnx2x_prev_list_node *bnx2x_prev_path_get_entry(struct bnx2x_softc *sc)
{
	struct bnx2x_prev_list_node *tmp;

	LIST_FOREACH(tmp, &bnx2x_prev_list, node) {
		if ((sc->pcie_bus == tmp->bus) &&
		    (sc->pcie_device == tmp->slot) &&
		    (SC_PATH(sc) == tmp->path)) {
			return tmp;
		}
	}

	return NULL;
}

static uint8_t bnx2x_prev_is_path_marked(struct bnx2x_softc *sc)
{
	struct bnx2x_prev_list_node *tmp;
	int rc = FALSE;

	rte_spinlock_lock(&bnx2x_prev_mtx);

	tmp = bnx2x_prev_path_get_entry(sc);
	if (tmp) {
		if (tmp->aer) {
			PMD_DRV_LOG(DEBUG, sc,
				    "Path %d/%d/%d was marked by AER",
				    sc->pcie_bus, sc->pcie_device, SC_PATH(sc));
		} else {
			rc = TRUE;
			PMD_DRV_LOG(DEBUG, sc,
				    "Path %d/%d/%d was already cleaned from previous drivers",
				    sc->pcie_bus, sc->pcie_device, SC_PATH(sc));
		}
	}

	rte_spinlock_unlock(&bnx2x_prev_mtx);

	return rc;
}

static int bnx2x_prev_mark_path(struct bnx2x_softc *sc, uint8_t after_undi)
{
	struct bnx2x_prev_list_node *tmp;

	rte_spinlock_lock(&bnx2x_prev_mtx);

	/* Check whether the entry for this path already exists */
	tmp = bnx2x_prev_path_get_entry(sc);
	if (tmp) {
		if (!tmp->aer) {
			PMD_DRV_LOG(DEBUG, sc,
				    "Re-marking AER in path %d/%d/%d",
				    sc->pcie_bus, sc->pcie_device, SC_PATH(sc));
		} else {
			PMD_DRV_LOG(DEBUG, sc,
				    "Removing AER indication from path %d/%d/%d",
				    sc->pcie_bus, sc->pcie_device, SC_PATH(sc));
			tmp->aer = 0;
		}

		rte_spinlock_unlock(&bnx2x_prev_mtx);
		return 0;
	}

	rte_spinlock_unlock(&bnx2x_prev_mtx);

	/* Create an entry for this path and add it */
	tmp = rte_malloc("", sizeof(struct bnx2x_prev_list_node),
			 RTE_CACHE_LINE_SIZE);
	if (!tmp) {
		PMD_DRV_LOG(NOTICE, sc, "Failed to allocate 'bnx2x_prev_list_node'");
		return -1;
	}

	tmp->bus = sc->pcie_bus;
	tmp->slot = sc->pcie_device;
	tmp->path = SC_PATH(sc);
	tmp->aer = 0;
	tmp->undi = after_undi ? (1 << SC_PORT(sc)) : 0;

	rte_spinlock_lock(&bnx2x_prev_mtx);

	LIST_INSERT_HEAD(&bnx2x_prev_list, tmp, node);

	rte_spinlock_unlock(&bnx2x_prev_mtx);

	return 0;
}

static int bnx2x_do_flr(struct bnx2x_softc *sc)
{
	int i;

	/* only E2 and onwards support FLR */
	if (CHIP_IS_E1x(sc)) {
		PMD_DRV_LOG(WARNING, sc, "FLR not supported in E1H");
		return -1;
	}

	/* only bootcode REQ_BC_VER_4_INITIATE_FLR and onwards support flr */
	if (sc->devinfo.bc_ver < REQ_BC_VER_4_INITIATE_FLR) {
		PMD_DRV_LOG(WARNING, sc,
			    "FLR not supported by BC_VER: 0x%08x",
			    sc->devinfo.bc_ver);
		return -1;
	}

	/* Wait for Transaction Pending bit clean */
	for (i = 0; i < 4; i++) {
		if (i) {
			DELAY(((1 << (i - 1)) * 100) * 1000);
		}

		if (!bnx2x_is_pcie_pending(sc)) {
			goto clear;
		}
	}

	PMD_DRV_LOG(NOTICE, sc, "PCIE transaction is not cleared, "
		    "proceeding with reset anyway");

clear:
	bnx2x_fw_command(sc, DRV_MSG_CODE_INITIATE_FLR, 0);

	return 0;
}

struct bnx2x_mac_vals {
	uint32_t xmac_addr;
	uint32_t xmac_val;
	uint32_t emac_addr;
	uint32_t emac_val;
	uint32_t umac_addr;
	uint32_t umac_val;
	uint32_t bmac_addr;
	uint32_t bmac_val[2];
};

static void
bnx2x_prev_unload_close_mac(struct bnx2x_softc *sc, struct bnx2x_mac_vals *vals)
{
	uint32_t val, base_addr, offset, mask, reset_reg;
	uint8_t mac_stopped = FALSE;
	uint8_t port = SC_PORT(sc);
	uint32_t wb_data[2];

	/* reset addresses as they also mark which values were changed */
	vals->bmac_addr = 0;
	vals->umac_addr = 0;
	vals->xmac_addr = 0;
	vals->emac_addr = 0;

	reset_reg = REG_RD(sc, MISC_REG_RESET_REG_2);

	if (!CHIP_IS_E3(sc)) {
		val = REG_RD(sc, NIG_REG_BMAC0_REGS_OUT_EN + port * 4);
		mask = MISC_REGISTERS_RESET_REG_2_RST_BMAC0 << port;
		if ((mask & reset_reg) && val) {
			base_addr = SC_PORT(sc) ? NIG_REG_INGRESS_BMAC1_MEM
			    : NIG_REG_INGRESS_BMAC0_MEM;
			offset = CHIP_IS_E2(sc) ? BIGMAC2_REGISTER_BMAC_CONTROL
			    : BIGMAC_REGISTER_BMAC_CONTROL;

			/*
			 * use rd/wr since we cannot use dmae. This is safe
			 * since MCP won't access the bus due to the request
			 * to unload, and no function on the path can be
			 * loaded at this time.
			 */
			wb_data[0] = REG_RD(sc, base_addr + offset);
			wb_data[1] = REG_RD(sc, base_addr + offset + 0x4);
			vals->bmac_addr = base_addr + offset;
			vals->bmac_val[0] = wb_data[0];
			vals->bmac_val[1] = wb_data[1];
			wb_data[0] &= ~ELINK_BMAC_CONTROL_RX_ENABLE;
			REG_WR(sc, vals->bmac_addr, wb_data[0]);
			REG_WR(sc, vals->bmac_addr + 0x4, wb_data[1]);
		}

		vals->emac_addr = NIG_REG_NIG_EMAC0_EN + SC_PORT(sc) * 4;
		vals->emac_val = REG_RD(sc, vals->emac_addr);
		REG_WR(sc, vals->emac_addr, 0);
		mac_stopped = TRUE;
	} else {
		if (reset_reg & MISC_REGISTERS_RESET_REG_2_XMAC) {
			base_addr = SC_PORT(sc) ? GRCBASE_XMAC1 : GRCBASE_XMAC0;
			val = REG_RD(sc, base_addr + XMAC_REG_PFC_CTRL_HI);
			REG_WR(sc, base_addr + XMAC_REG_PFC_CTRL_HI,
			       val & ~(1 << 1));
			REG_WR(sc, base_addr + XMAC_REG_PFC_CTRL_HI,
			       val | (1 << 1));
			vals->xmac_addr = base_addr + XMAC_REG_CTRL;
			vals->xmac_val = REG_RD(sc, vals->xmac_addr);
			REG_WR(sc, vals->xmac_addr, 0);
			mac_stopped = TRUE;
		}

		mask = MISC_REGISTERS_RESET_REG_2_UMAC0 << port;
		if (mask & reset_reg) {
			base_addr = SC_PORT(sc) ? GRCBASE_UMAC1 : GRCBASE_UMAC0;
			vals->umac_addr = base_addr + UMAC_REG_COMMAND_CONFIG;
			vals->umac_val = REG_RD(sc, vals->umac_addr);
			REG_WR(sc, vals->umac_addr, 0);
			mac_stopped = TRUE;
		}
	}

	if (mac_stopped) {
		DELAY(20000);
	}
}

#define BNX2X_PREV_UNDI_PROD_ADDR(p)  (BAR_TSTRORM_INTMEM + 0x1508 + ((p) << 4))
#define BNX2X_PREV_UNDI_RCQ(val)      ((val) & 0xffff)
#define BNX2X_PREV_UNDI_BD(val)       ((val) >> 16 & 0xffff)
#define BNX2X_PREV_UNDI_PROD(rcq, bd) ((bd) << 16 | (rcq))

static void
bnx2x_prev_unload_undi_inc(struct bnx2x_softc *sc, uint8_t port, uint8_t inc)
{
	uint16_t rcq, bd;
	uint32_t tmp_reg = REG_RD(sc, BNX2X_PREV_UNDI_PROD_ADDR(port));

	rcq = BNX2X_PREV_UNDI_RCQ(tmp_reg) + inc;
	bd = BNX2X_PREV_UNDI_BD(tmp_reg) + inc;

	tmp_reg = BNX2X_PREV_UNDI_PROD(rcq, bd);
	REG_WR(sc, BNX2X_PREV_UNDI_PROD_ADDR(port), tmp_reg);
}

static int bnx2x_prev_unload_common(struct bnx2x_softc *sc)
{
	uint32_t reset_reg, tmp_reg = 0, rc;
	uint8_t prev_undi = FALSE;
	struct bnx2x_mac_vals mac_vals;
	uint32_t timer_count = 1000;
	uint32_t prev_brb;

	/*
	 * It is possible a previous function received 'common' answer,
	 * but hasn't loaded yet, therefore creating a scenario of
	 * multiple functions receiving 'common' on the same path.
	 */
	memset(&mac_vals, 0, sizeof(mac_vals));

	if (bnx2x_prev_is_path_marked(sc)) {
		return bnx2x_prev_mcp_done(sc);
	}

	reset_reg = REG_RD(sc, MISC_REG_RESET_REG_1);

	/* Reset should be performed after BRB is emptied */
	if (reset_reg & MISC_REGISTERS_RESET_REG_1_RST_BRB1) {
		/* Close the MAC Rx to prevent BRB from filling up */
		bnx2x_prev_unload_close_mac(sc, &mac_vals);

		/* close LLH filters towards the BRB */
		elink_set_rx_filter(&sc->link_params, 0);

		/*
		 * Check if the UNDI driver was previously loaded.
		 * UNDI driver initializes CID offset for normal bell to 0x7
		 */
		if (reset_reg & MISC_REGISTERS_RESET_REG_1_RST_DORQ) {
			tmp_reg = REG_RD(sc, DORQ_REG_NORM_CID_OFST);
			if (tmp_reg == 0x7) {
				PMD_DRV_LOG(DEBUG, sc, "UNDI previously loaded");
				prev_undi = TRUE;
				/* clear the UNDI indication */
				REG_WR(sc, DORQ_REG_NORM_CID_OFST, 0);
				/* clear possible idle check errors */
				REG_RD(sc, NIG_REG_NIG_INT_STS_CLR_0);
			}
		}

		/* wait until BRB is empty */
		tmp_reg = REG_RD(sc, BRB1_REG_NUM_OF_FULL_BLOCKS);
		while (timer_count) {
			prev_brb = tmp_reg;

			tmp_reg = REG_RD(sc, BRB1_REG_NUM_OF_FULL_BLOCKS);
			if (!tmp_reg) {
				break;
			}

			PMD_DRV_LOG(DEBUG, sc, "BRB still has 0x%08x", tmp_reg);

			/* reset timer as long as BRB actually gets emptied */
			if (prev_brb > tmp_reg) {
				timer_count = 1000;
			} else {
				timer_count--;
			}

			/* If UNDI resides in memory, manually increment it */
			if (prev_undi) {
				bnx2x_prev_unload_undi_inc(sc, SC_PORT(sc), 1);
			}

			DELAY(10);
		}

		if (!timer_count) {
			PMD_DRV_LOG(NOTICE, sc, "Failed to empty BRB");
		}
	}

	/* No packets are in the pipeline, path is ready for reset */
	bnx2x_reset_common(sc);

	if (mac_vals.xmac_addr) {
		REG_WR(sc, mac_vals.xmac_addr, mac_vals.xmac_val);
	}
	if (mac_vals.umac_addr) {
		REG_WR(sc, mac_vals.umac_addr, mac_vals.umac_val);
	}
	if (mac_vals.emac_addr) {
		REG_WR(sc, mac_vals.emac_addr, mac_vals.emac_val);
	}
	if (mac_vals.bmac_addr) {
		REG_WR(sc, mac_vals.bmac_addr, mac_vals.bmac_val[0]);
		REG_WR(sc, mac_vals.bmac_addr + 4, mac_vals.bmac_val[1]);
	}

	rc = bnx2x_prev_mark_path(sc, prev_undi);
	if (rc) {
		bnx2x_prev_mcp_done(sc);
		return rc;
	}

	return bnx2x_prev_mcp_done(sc);
}

static int bnx2x_prev_unload_uncommon(struct bnx2x_softc *sc)
{
	int rc;

	/* Test if previous unload process was already finished for this path */
	if (bnx2x_prev_is_path_marked(sc)) {
		return bnx2x_prev_mcp_done(sc);
	}

	/*
	 * If function has FLR capabilities, and existing FW version matches
	 * the one required, then FLR will be sufficient to clean any residue
	 * left by previous driver
	 */
	rc = bnx2x_nic_load_analyze_req(sc, FW_MSG_CODE_DRV_LOAD_FUNCTION);
	if (!rc) {
		/* fw version is good */
		rc = bnx2x_do_flr(sc);
	}

	if (!rc) {
		/* FLR was performed */
		return 0;
	}

	PMD_DRV_LOG(INFO, sc, "Could not FLR");

	/* Close the MCP request, return failure */
	rc = bnx2x_prev_mcp_done(sc);
	if (!rc) {
		rc = BNX2X_PREV_WAIT_NEEDED;
	}

	return rc;
}

static int bnx2x_prev_unload(struct bnx2x_softc *sc)
{
	int time_counter = 10;
	uint32_t fw, hw_lock_reg, hw_lock_val;
	uint32_t rc = 0;

	PMD_INIT_FUNC_TRACE(sc);

	/*
	 * Clear HW from errors which may have resulted from an interrupted
	 * DMAE transaction.
	 */
	bnx2x_prev_interrupted_dmae(sc);

	/* Release previously held locks */
	hw_lock_reg = (SC_FUNC(sc) <= 5) ?
			(MISC_REG_DRIVER_CONTROL_1 + SC_FUNC(sc) * 8) :
			(MISC_REG_DRIVER_CONTROL_7 + (SC_FUNC(sc) - 6) * 8);

	hw_lock_val = (REG_RD(sc, hw_lock_reg));
	if (hw_lock_val) {
		if (hw_lock_val & HW_LOCK_RESOURCE_NVRAM) {
			PMD_DRV_LOG(DEBUG, sc, "Releasing previously held NVRAM lock\n");
			REG_WR(sc, MCP_REG_MCPR_NVM_SW_ARB,
			       (MCPR_NVM_SW_ARB_ARB_REQ_CLR1 << SC_PORT(sc)));
		}
		PMD_DRV_LOG(DEBUG, sc, "Releasing previously held HW lock\n");
		REG_WR(sc, hw_lock_reg, 0xffffffff);
	}

	if (MCPR_ACCESS_LOCK_LOCK & REG_RD(sc, MCP_REG_MCPR_ACCESS_LOCK)) {
		PMD_DRV_LOG(DEBUG, sc, "Releasing previously held ALR\n");
		REG_WR(sc, MCP_REG_MCPR_ACCESS_LOCK, 0);
	}

	do {
		/* Lock MCP using an unload request */
		fw = bnx2x_fw_command(sc, DRV_MSG_CODE_UNLOAD_REQ_WOL_DIS, 0);
		if (!fw) {
			PMD_DRV_LOG(NOTICE, sc, "MCP response failure, aborting");
			rc = -1;
			break;
		}

		if (fw == FW_MSG_CODE_DRV_UNLOAD_COMMON) {
			rc = bnx2x_prev_unload_common(sc);
			break;
		}

		/* non-common reply from MCP might require looping */
		rc = bnx2x_prev_unload_uncommon(sc);
		if (rc != BNX2X_PREV_WAIT_NEEDED) {
			break;
		}

		DELAY(20000);
	} while (--time_counter);

	if (!time_counter || rc) {
		PMD_DRV_LOG(NOTICE, sc, "Failed to unload previous driver!");
		rc = -1;
	}

	return rc;
}

static void
bnx2x_dcbx_set_state(struct bnx2x_softc *sc, uint8_t dcb_on, uint32_t dcbx_enabled)
{
	if (!CHIP_IS_E1x(sc)) {
		sc->dcb_state = dcb_on;
		sc->dcbx_enabled = dcbx_enabled;
	} else {
		sc->dcb_state = FALSE;
		sc->dcbx_enabled = BNX2X_DCBX_ENABLED_INVALID;
	}
	PMD_DRV_LOG(DEBUG, sc,
		    "DCB state [%s:%s]",
		    dcb_on ? "ON" : "OFF",
		    (dcbx_enabled == BNX2X_DCBX_ENABLED_OFF) ? "user-mode" :
		    (dcbx_enabled ==
		     BNX2X_DCBX_ENABLED_ON_NEG_OFF) ? "on-chip static"
		    : (dcbx_enabled ==
		       BNX2X_DCBX_ENABLED_ON_NEG_ON) ?
		    "on-chip with negotiation" : "invalid");
}

static int bnx2x_set_qm_cid_count(struct bnx2x_softc *sc)
{
	int cid_count = BNX2X_L2_MAX_CID(sc);

	if (CNIC_SUPPORT(sc)) {
		cid_count += CNIC_CID_MAX;
	}

	return roundup(cid_count, QM_CID_ROUND);
}

static void bnx2x_init_multi_cos(struct bnx2x_softc *sc)
{
	int pri, cos;

	uint32_t pri_map = 0;

	for (pri = 0; pri < BNX2X_MAX_PRIORITY; pri++) {
		cos = ((pri_map & (0xf << (pri * 4))) >> (pri * 4));
		if (cos < sc->max_cos) {
			sc->prio_to_cos[pri] = cos;
		} else {
			PMD_DRV_LOG(WARNING, sc,
				    "Invalid COS %d for priority %d "
				    "(max COS is %d), setting to 0", cos, pri,
				    (sc->max_cos - 1));
			sc->prio_to_cos[pri] = 0;
		}
	}
}

static int bnx2x_pci_get_caps(struct bnx2x_softc *sc)
{
	struct {
		uint8_t id;
		uint8_t next;
	} pci_cap;
	uint16_t status;
	struct bnx2x_pci_cap *cap;

	cap = sc->pci_caps = rte_zmalloc("caps", sizeof(struct bnx2x_pci_cap),
					 RTE_CACHE_LINE_SIZE);
	if (!cap) {
		PMD_DRV_LOG(NOTICE, sc, "Failed to allocate memory");
		return -ENOMEM;
	}

#ifndef RTE_EXEC_ENV_FREEBSD
	pci_read(sc, PCI_STATUS, &status, 2);
	if (!(status & PCI_STATUS_CAP_LIST)) {
#else
	pci_read(sc, PCIR_STATUS, &status, 2);
	if (!(status & PCIM_STATUS_CAPPRESENT)) {
#endif
		PMD_DRV_LOG(NOTICE, sc, "PCIe capability reading failed");
		return -1;
	}

#ifndef RTE_EXEC_ENV_FREEBSD
	pci_read(sc, PCI_CAPABILITY_LIST, &pci_cap.next, 1);
#else
	pci_read(sc, PCIR_CAP_PTR, &pci_cap.next, 1);
#endif
	while (pci_cap.next) {
		cap->addr = pci_cap.next & ~3;
		pci_read(sc, pci_cap.next & ~3, &pci_cap, 2);
		if (pci_cap.id == 0xff)
			break;
		cap->id = pci_cap.id;
		cap->type = BNX2X_PCI_CAP;
		cap->next = rte_zmalloc("pci_cap",
					sizeof(struct bnx2x_pci_cap),
					RTE_CACHE_LINE_SIZE);
		if (!cap->next) {
			PMD_DRV_LOG(NOTICE, sc, "Failed to allocate memory");
			return -ENOMEM;
		}
		cap = cap->next;
	}

	return 0;
}

static void bnx2x_init_rte(struct bnx2x_softc *sc)
{
	if (IS_VF(sc)) {
		sc->max_tx_queues = min(BNX2X_VF_MAX_QUEUES_PER_VF,
					sc->igu_sb_cnt);
		sc->max_rx_queues = min(BNX2X_VF_MAX_QUEUES_PER_VF,
					sc->igu_sb_cnt);
	} else {
		sc->max_rx_queues = BNX2X_MAX_RSS_COUNT(sc);
		sc->max_tx_queues = sc->max_rx_queues;
	}
}

#define FW_HEADER_LEN 104
#define FW_NAME_57711 "/lib/firmware/bnx2x/bnx2x-e1h-7.13.11.0.fw"
#define FW_NAME_57810 "/lib/firmware/bnx2x/bnx2x-e2-7.13.11.0.fw"

void bnx2x_load_firmware(struct bnx2x_softc *sc)
{
	const char *fwname;
	void *buf;
	size_t bufsz;

	fwname = sc->devinfo.device_id == CHIP_NUM_57711
		? FW_NAME_57711 : FW_NAME_57810;
	if (rte_firmware_read(fwname, &buf, &bufsz) != 0) {
		PMD_DRV_LOG(NOTICE, sc, "Can't open firmware file");
		return;
	}

	sc->firmware = rte_zmalloc("bnx2x_fw", bufsz, RTE_CACHE_LINE_SIZE);
	if (!sc->firmware) {
		PMD_DRV_LOG(NOTICE, sc, "Can't allocate memory for firmware");
		goto out;
	}

	sc->fw_len = bufsz;
	if (sc->fw_len < FW_HEADER_LEN) {
		PMD_DRV_LOG(NOTICE, sc,
			    "Invalid fw size: %" PRIu64, sc->fw_len);
		goto out;
	}

	memcpy(sc->firmware, buf, sc->fw_len);
	PMD_DRV_LOG(DEBUG, sc, "fw_len = %" PRIu64, sc->fw_len);
out:
	free(buf);
}

static void
bnx2x_data_to_init_ops(uint8_t * data, struct raw_op *dst, uint32_t len)
{
	uint32_t *src = (uint32_t *) data;
	uint32_t i, j, tmp;

	for (i = 0, j = 0; i < len / 8; ++i, j += 2) {
		tmp = rte_be_to_cpu_32(src[j]);
		dst[i].op = (tmp >> 24) & 0xFF;
		dst[i].offset = tmp & 0xFFFFFF;
		dst[i].raw_data = rte_be_to_cpu_32(src[j + 1]);
	}
}

static void
bnx2x_data_to_init_offsets(uint8_t * data, uint16_t * dst, uint32_t len)
{
	uint16_t *src = (uint16_t *) data;
	uint32_t i;

	for (i = 0; i < len / 2; ++i)
		dst[i] = rte_be_to_cpu_16(src[i]);
}

static void bnx2x_data_to_init_data(uint8_t * data, uint32_t * dst, uint32_t len)
{
	uint32_t *src = (uint32_t *) data;
	uint32_t i;

	for (i = 0; i < len / 4; ++i)
		dst[i] = rte_be_to_cpu_32(src[i]);
}

static void bnx2x_data_to_iro_array(uint8_t * data, struct iro *dst, uint32_t len)
{
	uint32_t *src = (uint32_t *) data;
	uint32_t i, j, tmp;

	for (i = 0, j = 0; i < len / sizeof(struct iro); ++i, ++j) {
		dst[i].base = rte_be_to_cpu_32(src[j++]);
		tmp = rte_be_to_cpu_32(src[j]);
		dst[i].m1 = (tmp >> 16) & 0xFFFF;
		dst[i].m2 = tmp & 0xFFFF;
		++j;
		tmp = rte_be_to_cpu_32(src[j]);
		dst[i].m3 = (tmp >> 16) & 0xFFFF;
		dst[i].size = tmp & 0xFFFF;
	}
}

/*
* Device attach function.
*
* Allocates device resources, performs secondary chip identification, and
* initializes driver instance variables. This function is called from driver
* load after a successful probe.
*
* Returns:
*   0 = Success, >0 = Failure
*/
int bnx2x_attach(struct bnx2x_softc *sc)
{
	int rc;

	PMD_DRV_LOG(DEBUG, sc, "Starting attach...");

	rc = bnx2x_pci_get_caps(sc);
	if (rc) {
		PMD_DRV_LOG(NOTICE, sc, "PCIe caps reading was failed");
		return rc;
	}

	sc->state = BNX2X_STATE_CLOSED;

	pci_write_long(sc, PCICFG_GRC_ADDRESS, PCICFG_VENDOR_ID_OFFSET);

	sc->igu_base_addr = IS_VF(sc) ? PXP_VF_ADDR_IGU_START : BAR_IGU_INTMEM;

	/* get PCI capabilites */
	bnx2x_probe_pci_caps(sc);

	if (sc->devinfo.pcie_msix_cap_reg != 0) {
		uint32_t val;
		pci_read(sc,
			 (sc->devinfo.pcie_msix_cap_reg + PCIR_MSIX_CTRL), &val,
			 2);
		sc->igu_sb_cnt = (val & PCIM_MSIXCTRL_TABLE_SIZE) + 1;
	} else {
		sc->igu_sb_cnt = 1;
	}

	/* Init RTE stuff */
	bnx2x_init_rte(sc);

	if (IS_PF(sc)) {
		/* Enable internal target-read (in case we are probed after PF
		 * FLR). Must be done prior to any BAR read access. Only for
		 * 57712 and up
		 */
		if (!CHIP_IS_E1x(sc)) {
			REG_WR(sc, PGLUE_B_REG_INTERNAL_PFID_ENABLE_TARGET_READ,
			       1);
			DELAY(200000);
		}

		/* get device info and set params */
		if (bnx2x_get_device_info(sc) != 0) {
			PMD_DRV_LOG(NOTICE, sc, "getting device info");
			return -ENXIO;
		}

/* get phy settings from shmem and 'and' against admin settings */
		bnx2x_get_phy_info(sc);
	} else {
		/* Left mac of VF unfilled, PF should set it for VF */
		memset(sc->link_params.mac_addr, 0, RTE_ETHER_ADDR_LEN);
	}

	sc->wol = 0;

	/* set the default MTU (changed via ifconfig) */
	sc->mtu = RTE_ETHER_MTU;

	bnx2x_set_modes_bitmap(sc);

	/* need to reset chip if UNDI was active */
	if (IS_PF(sc) && !BNX2X_NOMCP(sc)) {
/* init fw_seq */
		sc->fw_seq =
		    (SHMEM_RD(sc, func_mb[SC_FW_MB_IDX(sc)].drv_mb_header) &
		     DRV_MSG_SEQ_NUMBER_MASK);
		PMD_DRV_LOG(DEBUG, sc, "prev unload fw_seq 0x%04x",
			    sc->fw_seq);
		bnx2x_prev_unload(sc);
	}

	bnx2x_dcbx_set_state(sc, FALSE, BNX2X_DCBX_ENABLED_OFF);

	/* calculate qm_cid_count */
	sc->qm_cid_count = bnx2x_set_qm_cid_count(sc);

	sc->max_cos = 1;
	bnx2x_init_multi_cos(sc);

	return 0;
}

static void
bnx2x_igu_ack_sb(struct bnx2x_softc *sc, uint8_t igu_sb_id, uint8_t segment,
	       uint16_t index, uint8_t op, uint8_t update)
{
	uint32_t igu_addr = sc->igu_base_addr;
	igu_addr += (IGU_CMD_INT_ACK_BASE + igu_sb_id) * 8;
	bnx2x_igu_ack_sb_gen(sc, segment, index, op, update, igu_addr);
}

static void
bnx2x_ack_sb(struct bnx2x_softc *sc, uint8_t igu_sb_id, uint8_t storm,
	   uint16_t index, uint8_t op, uint8_t update)
{
	if (unlikely(sc->devinfo.int_block == INT_BLOCK_HC))
		bnx2x_hc_ack_sb(sc, igu_sb_id, storm, index, op, update);
	else {
		uint8_t segment;
		if (CHIP_INT_MODE_IS_BC(sc)) {
			segment = storm;
		} else if (igu_sb_id != sc->igu_dsb_id) {
			segment = IGU_SEG_ACCESS_DEF;
		} else if (storm == ATTENTION_ID) {
			segment = IGU_SEG_ACCESS_ATTN;
		} else {
			segment = IGU_SEG_ACCESS_DEF;
		}
		bnx2x_igu_ack_sb(sc, igu_sb_id, segment, index, op, update);
	}
}

static void
bnx2x_igu_clear_sb_gen(struct bnx2x_softc *sc, uint8_t func, uint8_t idu_sb_id,
		     uint8_t is_pf)
{
	uint32_t data, ctl, cnt = 100;
	uint32_t igu_addr_data = IGU_REG_COMMAND_REG_32LSB_DATA;
	uint32_t igu_addr_ctl = IGU_REG_COMMAND_REG_CTRL;
	uint32_t igu_addr_ack = IGU_REG_CSTORM_TYPE_0_SB_CLEANUP +
	    (idu_sb_id / 32) * 4;
	uint32_t sb_bit = 1 << (idu_sb_id % 32);
	uint32_t func_encode = func |
	    (is_pf ? 1 : 0) << IGU_FID_ENCODE_IS_PF_SHIFT;
	uint32_t addr_encode = IGU_CMD_E2_PROD_UPD_BASE + idu_sb_id;

	/* Not supported in BC mode */
	if (CHIP_INT_MODE_IS_BC(sc)) {
		return;
	}

	data = ((IGU_USE_REGISTER_cstorm_type_0_sb_cleanup <<
		 IGU_REGULAR_CLEANUP_TYPE_SHIFT) |
		IGU_REGULAR_CLEANUP_SET | IGU_REGULAR_BCLEANUP);

	ctl = ((addr_encode << IGU_CTRL_REG_ADDRESS_SHIFT) |
	       (func_encode << IGU_CTRL_REG_FID_SHIFT) |
	       (IGU_CTRL_CMD_TYPE_WR << IGU_CTRL_REG_TYPE_SHIFT));

	REG_WR(sc, igu_addr_data, data);

	mb();

	PMD_DRV_LOG(DEBUG, sc, "write 0x%08x to IGU(via GRC) addr 0x%x",
		    ctl, igu_addr_ctl);
	REG_WR(sc, igu_addr_ctl, ctl);

	mb();

	/* wait for clean up to finish */
	while (!(REG_RD(sc, igu_addr_ack) & sb_bit) && --cnt) {
		DELAY(20000);
	}

	if (!(REG_RD(sc, igu_addr_ack) & sb_bit)) {
		PMD_DRV_LOG(DEBUG, sc,
			    "Unable to finish IGU cleanup: "
			    "idu_sb_id %d offset %d bit %d (cnt %d)",
			    idu_sb_id, idu_sb_id / 32, idu_sb_id % 32, cnt);
	}
}

static void bnx2x_igu_clear_sb(struct bnx2x_softc *sc, uint8_t idu_sb_id)
{
	bnx2x_igu_clear_sb_gen(sc, SC_FUNC(sc), idu_sb_id, TRUE /*PF*/);
}

/*******************/
/* ECORE CALLBACKS */
/*******************/

static void bnx2x_reset_common(struct bnx2x_softc *sc)
{
	uint32_t val = 0x1400;

	PMD_INIT_FUNC_TRACE(sc);

	/* reset_common */
	REG_WR(sc, (GRCBASE_MISC + MISC_REGISTERS_RESET_REG_1_CLEAR),
	       0xd3ffff7f);

	if (CHIP_IS_E3(sc)) {
		val |= MISC_REGISTERS_RESET_REG_2_MSTAT0;
		val |= MISC_REGISTERS_RESET_REG_2_MSTAT1;
	}

	REG_WR(sc, (GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_CLEAR), val);
}

static void bnx2x_common_init_phy(struct bnx2x_softc *sc)
{
	uint32_t shmem_base[2];
	uint32_t shmem2_base[2];

	/* Avoid common init in case MFW supports LFA */
	if (SHMEM2_RD(sc, size) >
	    (uint32_t) offsetof(struct shmem2_region,
				lfa_host_addr[SC_PORT(sc)])) {
		return;
	}

	shmem_base[0] = sc->devinfo.shmem_base;
	shmem2_base[0] = sc->devinfo.shmem2_base;

	if (!CHIP_IS_E1x(sc)) {
		shmem_base[1] = SHMEM2_RD(sc, other_shmem_base_addr);
		shmem2_base[1] = SHMEM2_RD(sc, other_shmem2_base_addr);
	}

	bnx2x_acquire_phy_lock(sc);
	elink_common_init_phy(sc, shmem_base, shmem2_base,
			      sc->devinfo.chip_id, 0);
	bnx2x_release_phy_lock(sc);
}

static void bnx2x_pf_disable(struct bnx2x_softc *sc)
{
	uint32_t val = REG_RD(sc, IGU_REG_PF_CONFIGURATION);

	val &= ~IGU_PF_CONF_FUNC_EN;

	REG_WR(sc, IGU_REG_PF_CONFIGURATION, val);
	REG_WR(sc, PGLUE_B_REG_INTERNAL_PFID_ENABLE_MASTER, 0);
	REG_WR(sc, CFC_REG_WEAK_ENABLE_PF, 0);
}

static void bnx2x_init_pxp(struct bnx2x_softc *sc)
{
	uint16_t devctl;
	int r_order, w_order;

	devctl = bnx2x_pcie_capability_read(sc, PCIR_EXPRESS_DEVICE_CTL);

	w_order = ((devctl & PCIM_EXP_CTL_MAX_PAYLOAD) >> 5);
	r_order = ((devctl & PCIM_EXP_CTL_MAX_READ_REQUEST) >> 12);

	ecore_init_pxp_arb(sc, r_order, w_order);
}

static uint32_t bnx2x_get_pretend_reg(struct bnx2x_softc *sc)
{
	uint32_t base = PXP2_REG_PGL_PRETEND_FUNC_F0;
	uint32_t stride = (PXP2_REG_PGL_PRETEND_FUNC_F1 - base);
	return base + (SC_ABS_FUNC(sc)) * stride;
}

/*
 * Called only on E1H or E2.
 * When pretending to be PF, the pretend value is the function number 0..7.
 * When pretending to be VF, the pretend val is the PF-num:VF-valid:ABS-VFID
 * combination.
 */
static int bnx2x_pretend_func(struct bnx2x_softc *sc, uint16_t pretend_func_val)
{
	uint32_t pretend_reg;

	if (CHIP_IS_E1H(sc) && (pretend_func_val > E1H_FUNC_MAX))
		return -1;

	/* get my own pretend register */
	pretend_reg = bnx2x_get_pretend_reg(sc);
	REG_WR(sc, pretend_reg, pretend_func_val);
	REG_RD(sc, pretend_reg);
	return 0;
}

static void bnx2x_setup_fan_failure_detection(struct bnx2x_softc *sc)
{
	int is_required;
	uint32_t val;
	int port;

	is_required = 0;
	val = (SHMEM_RD(sc, dev_info.shared_hw_config.config2) &
	       SHARED_HW_CFG_FAN_FAILURE_MASK);

	if (val == SHARED_HW_CFG_FAN_FAILURE_ENABLED) {
		is_required = 1;
	}
	/*
	 * The fan failure mechanism is usually related to the PHY type since
	 * the power consumption of the board is affected by the PHY. Currently,
	 * fan is required for most designs with SFX7101, BNX2X8727 and BNX2X8481.
	 */
	else if (val == SHARED_HW_CFG_FAN_FAILURE_PHY_TYPE) {
		for (port = PORT_0; port < PORT_MAX; port++) {
			is_required |= elink_fan_failure_det_req(sc,
								 sc->
								 devinfo.shmem_base,
								 sc->
								 devinfo.shmem2_base,
								 port);
		}
	}

	if (is_required == 0) {
		return;
	}

	/* Fan failure is indicated by SPIO 5 */
	bnx2x_set_spio(sc, MISC_SPIO_SPIO5, MISC_SPIO_INPUT_HI_Z);

	/* set to active low mode */
	val = REG_RD(sc, MISC_REG_SPIO_INT);
	val |= (MISC_SPIO_SPIO5 << MISC_SPIO_INT_OLD_SET_POS);
	REG_WR(sc, MISC_REG_SPIO_INT, val);

	/* enable interrupt to signal the IGU */
	val = REG_RD(sc, MISC_REG_SPIO_EVENT_EN);
	val |= MISC_SPIO_SPIO5;
	REG_WR(sc, MISC_REG_SPIO_EVENT_EN, val);
}

static void bnx2x_enable_blocks_attention(struct bnx2x_softc *sc)
{
	uint32_t val;

	REG_WR(sc, PXP_REG_PXP_INT_MASK_0, 0);
	if (!CHIP_IS_E1x(sc)) {
		REG_WR(sc, PXP_REG_PXP_INT_MASK_1, 0x40);
	} else {
		REG_WR(sc, PXP_REG_PXP_INT_MASK_1, 0);
	}
	REG_WR(sc, DORQ_REG_DORQ_INT_MASK, 0);
	REG_WR(sc, CFC_REG_CFC_INT_MASK, 0);
	/*
	 * mask read length error interrupts in brb for parser
	 * (parsing unit and 'checksum and crc' unit)
	 * these errors are legal (PU reads fixed length and CAC can cause
	 * read length error on truncated packets)
	 */
	REG_WR(sc, BRB1_REG_BRB1_INT_MASK, 0xFC00);
	REG_WR(sc, QM_REG_QM_INT_MASK, 0);
	REG_WR(sc, TM_REG_TM_INT_MASK, 0);
	REG_WR(sc, XSDM_REG_XSDM_INT_MASK_0, 0);
	REG_WR(sc, XSDM_REG_XSDM_INT_MASK_1, 0);
	REG_WR(sc, XCM_REG_XCM_INT_MASK, 0);
	/*      REG_WR(sc, XSEM_REG_XSEM_INT_MASK_0, 0); */
	/*      REG_WR(sc, XSEM_REG_XSEM_INT_MASK_1, 0); */
	REG_WR(sc, USDM_REG_USDM_INT_MASK_0, 0);
	REG_WR(sc, USDM_REG_USDM_INT_MASK_1, 0);
	REG_WR(sc, UCM_REG_UCM_INT_MASK, 0);
	/*      REG_WR(sc, USEM_REG_USEM_INT_MASK_0, 0); */
	/*      REG_WR(sc, USEM_REG_USEM_INT_MASK_1, 0); */
	REG_WR(sc, GRCBASE_UPB + PB_REG_PB_INT_MASK, 0);
	REG_WR(sc, CSDM_REG_CSDM_INT_MASK_0, 0);
	REG_WR(sc, CSDM_REG_CSDM_INT_MASK_1, 0);
	REG_WR(sc, CCM_REG_CCM_INT_MASK, 0);
	/*      REG_WR(sc, CSEM_REG_CSEM_INT_MASK_0, 0); */
	/*      REG_WR(sc, CSEM_REG_CSEM_INT_MASK_1, 0); */

	val = (PXP2_PXP2_INT_MASK_0_REG_PGL_CPL_AFT |
	       PXP2_PXP2_INT_MASK_0_REG_PGL_CPL_OF |
	       PXP2_PXP2_INT_MASK_0_REG_PGL_PCIE_ATTN);
	if (!CHIP_IS_E1x(sc)) {
		val |= (PXP2_PXP2_INT_MASK_0_REG_PGL_READ_BLOCKED |
			PXP2_PXP2_INT_MASK_0_REG_PGL_WRITE_BLOCKED);
	}
	REG_WR(sc, PXP2_REG_PXP2_INT_MASK_0, val);

	REG_WR(sc, TSDM_REG_TSDM_INT_MASK_0, 0);
	REG_WR(sc, TSDM_REG_TSDM_INT_MASK_1, 0);
	REG_WR(sc, TCM_REG_TCM_INT_MASK, 0);
	/*      REG_WR(sc, TSEM_REG_TSEM_INT_MASK_0, 0); */

	if (!CHIP_IS_E1x(sc)) {
/* enable VFC attentions: bits 11 and 12, bits 31:13 reserved */
		REG_WR(sc, TSEM_REG_TSEM_INT_MASK_1, 0x07ff);
	}

	REG_WR(sc, CDU_REG_CDU_INT_MASK, 0);
	REG_WR(sc, DMAE_REG_DMAE_INT_MASK, 0);
	/*      REG_WR(sc, MISC_REG_MISC_INT_MASK, 0); */
	REG_WR(sc, PBF_REG_PBF_INT_MASK, 0x18);	/* bit 3,4 masked */
}

/**
 * bnx2x_init_hw_common - initialize the HW at the COMMON phase.
 *
 * @sc:     driver handle
 */
static int bnx2x_init_hw_common(struct bnx2x_softc *sc)
{
	uint8_t abs_func_id;
	uint32_t val;

	PMD_DRV_LOG(DEBUG, sc,
		    "starting common init for func %d", SC_ABS_FUNC(sc));

	/*
	 * take the RESET lock to protect undi_unload flow from accessing
	 * registers while we are resetting the chip
	 */
	bnx2x_acquire_hw_lock(sc, HW_LOCK_RESOURCE_RESET);

	bnx2x_reset_common(sc);

	REG_WR(sc, (GRCBASE_MISC + MISC_REGISTERS_RESET_REG_1_SET), 0xffffffff);

	val = 0xfffc;
	if (CHIP_IS_E3(sc)) {
		val |= MISC_REGISTERS_RESET_REG_2_MSTAT0;
		val |= MISC_REGISTERS_RESET_REG_2_MSTAT1;
	}

	REG_WR(sc, (GRCBASE_MISC + MISC_REGISTERS_RESET_REG_2_SET), val);

	bnx2x_release_hw_lock(sc, HW_LOCK_RESOURCE_RESET);

	ecore_init_block(sc, BLOCK_MISC, PHASE_COMMON);

	if (!CHIP_IS_E1x(sc)) {
/*
 * 4-port mode or 2-port mode we need to turn off master-enable for
 * everyone. After that we turn it back on for self. So, we disregard
 * multi-function, and always disable all functions on the given path,
 * this means 0,2,4,6 for path 0 and 1,3,5,7 for path 1
 */
		for (abs_func_id = SC_PATH(sc);
		     abs_func_id < (E2_FUNC_MAX * 2); abs_func_id += 2) {
			if (abs_func_id == SC_ABS_FUNC(sc)) {
				REG_WR(sc,
				       PGLUE_B_REG_INTERNAL_PFID_ENABLE_MASTER,
				       1);
				continue;
			}

			bnx2x_pretend_func(sc, abs_func_id);

			/* clear pf enable */
			bnx2x_pf_disable(sc);

			bnx2x_pretend_func(sc, SC_ABS_FUNC(sc));
		}
	}

	ecore_init_block(sc, BLOCK_PXP, PHASE_COMMON);

	ecore_init_block(sc, BLOCK_PXP2, PHASE_COMMON);
	bnx2x_init_pxp(sc);

#ifdef __BIG_ENDIAN
	REG_WR(sc, PXP2_REG_RQ_QM_ENDIAN_M, 1);
	REG_WR(sc, PXP2_REG_RQ_TM_ENDIAN_M, 1);
	REG_WR(sc, PXP2_REG_RQ_SRC_ENDIAN_M, 1);
	REG_WR(sc, PXP2_REG_RQ_CDU_ENDIAN_M, 1);
	REG_WR(sc, PXP2_REG_RQ_DBG_ENDIAN_M, 1);
	/* make sure this value is 0 */
	REG_WR(sc, PXP2_REG_RQ_HC_ENDIAN_M, 0);

	//REG_WR(sc, PXP2_REG_RD_PBF_SWAP_MODE, 1);
	REG_WR(sc, PXP2_REG_RD_QM_SWAP_MODE, 1);
	REG_WR(sc, PXP2_REG_RD_TM_SWAP_MODE, 1);
	REG_WR(sc, PXP2_REG_RD_SRC_SWAP_MODE, 1);
	REG_WR(sc, PXP2_REG_RD_CDURD_SWAP_MODE, 1);
#endif

	ecore_ilt_init_page_size(sc, INITOP_SET);

	if (CHIP_REV_IS_FPGA(sc) && CHIP_IS_E1H(sc)) {
		REG_WR(sc, PXP2_REG_PGL_TAGS_LIMIT, 0x1);
	}

	/* let the HW do it's magic... */
	DELAY(100000);

	/* finish PXP init */

	val = REG_RD(sc, PXP2_REG_RQ_CFG_DONE);
	if (val != 1) {
		PMD_DRV_LOG(NOTICE, sc, "PXP2 CFG failed");
		return -1;
	}
	val = REG_RD(sc, PXP2_REG_RD_INIT_DONE);
	if (val != 1) {
		PMD_DRV_LOG(NOTICE, sc, "PXP2 RD_INIT failed");
		return -1;
	}

	/*
	 * Timer bug workaround for E2 only. We need to set the entire ILT to have
	 * entries with value "0" and valid bit on. This needs to be done by the
	 * first PF that is loaded in a path (i.e. common phase)
	 */
	if (!CHIP_IS_E1x(sc)) {
/*
 * In E2 there is a bug in the timers block that can cause function 6 / 7
 * (i.e. vnic3) to start even if it is marked as "scan-off".
 * This occurs when a different function (func2,3) is being marked
 * as "scan-off". Real-life scenario for example: if a driver is being
 * load-unloaded while func6,7 are down. This will cause the timer to access
 * the ilt, translate to a logical address and send a request to read/write.
 * Since the ilt for the function that is down is not valid, this will cause
 * a translation error which is unrecoverable.
 * The Workaround is intended to make sure that when this happens nothing
 * fatal will occur. The workaround:
 *  1.  First PF driver which loads on a path will:
 *      a.  After taking the chip out of reset, by using pretend,
 *          it will write "0" to the following registers of
 *          the other vnics.
 *          REG_WR(pdev, PGLUE_B_REG_INTERNAL_PFID_ENABLE_MASTER, 0);
 *          REG_WR(pdev, CFC_REG_WEAK_ENABLE_PF,0);
 *          REG_WR(pdev, CFC_REG_STRONG_ENABLE_PF,0);
 *          And for itself it will write '1' to
 *          PGLUE_B_REG_INTERNAL_PFID_ENABLE_MASTER to enable
 *          dmae-operations (writing to pram for example.)
 *          note: can be done for only function 6,7 but cleaner this
 *            way.
 *      b.  Write zero+valid to the entire ILT.
 *      c.  Init the first_timers_ilt_entry, last_timers_ilt_entry of
 *          VNIC3 (of that port). The range allocated will be the
 *          entire ILT. This is needed to prevent  ILT range error.
 *  2.  Any PF driver load flow:
 *      a.  ILT update with the physical addresses of the allocated
 *          logical pages.
 *      b.  Wait 20msec. - note that this timeout is needed to make
 *          sure there are no requests in one of the PXP internal
 *          queues with "old" ILT addresses.
 *      c.  PF enable in the PGLC.
 *      d.  Clear the was_error of the PF in the PGLC. (could have
 *          occurred while driver was down)
 *      e.  PF enable in the CFC (WEAK + STRONG)
 *      f.  Timers scan enable
 *  3.  PF driver unload flow:
 *      a.  Clear the Timers scan_en.
 *      b.  Polling for scan_on=0 for that PF.
 *      c.  Clear the PF enable bit in the PXP.
 *      d.  Clear the PF enable in the CFC (WEAK + STRONG)
 *      e.  Write zero+valid to all ILT entries (The valid bit must
 *          stay set)
 *      f.  If this is VNIC 3 of a port then also init
 *          first_timers_ilt_entry to zero and last_timers_ilt_entry
 *          to the last enrty in the ILT.
 *
 *      Notes:
 *      Currently the PF error in the PGLC is non recoverable.
 *      In the future the there will be a recovery routine for this error.
 *      Currently attention is masked.
 *      Having an MCP lock on the load/unload process does not guarantee that
 *      there is no Timer disable during Func6/7 enable. This is because the
 *      Timers scan is currently being cleared by the MCP on FLR.
 *      Step 2.d can be done only for PF6/7 and the driver can also check if
 *      there is error before clearing it. But the flow above is simpler and
 *      more general.
 *      All ILT entries are written by zero+valid and not just PF6/7
 *      ILT entries since in the future the ILT entries allocation for
 *      PF-s might be dynamic.
 */
		struct ilt_client_info ilt_cli;
		struct ecore_ilt ilt;

		memset(&ilt_cli, 0, sizeof(struct ilt_client_info));
		memset(&ilt, 0, sizeof(struct ecore_ilt));

/* initialize dummy TM client */
		ilt_cli.start = 0;
		ilt_cli.end = ILT_NUM_PAGE_ENTRIES - 1;
		ilt_cli.client_num = ILT_CLIENT_TM;

/*
 * Step 1: set zeroes to all ilt page entries with valid bit on
 * Step 2: set the timers first/last ilt entry to point
 * to the entire range to prevent ILT range error for 3rd/4th
 * vnic (this code assumes existence of the vnic)
 *
 * both steps performed by call to ecore_ilt_client_init_op()
 * with dummy TM client
 *
 * we must use pretend since PXP2_REG_RQ_##blk##_FIRST_ILT
 * and his brother are split registers
 */

		bnx2x_pretend_func(sc, (SC_PATH(sc) + 6));
		ecore_ilt_client_init_op_ilt(sc, &ilt, &ilt_cli, INITOP_CLEAR);
		bnx2x_pretend_func(sc, SC_ABS_FUNC(sc));

		REG_WR(sc, PXP2_REG_RQ_DRAM_ALIGN, BNX2X_PXP_DRAM_ALIGN);
		REG_WR(sc, PXP2_REG_RQ_DRAM_ALIGN_RD, BNX2X_PXP_DRAM_ALIGN);
		REG_WR(sc, PXP2_REG_RQ_DRAM_ALIGN_SEL, 1);
	}

	REG_WR(sc, PXP2_REG_RQ_DISABLE_INPUTS, 0);
	REG_WR(sc, PXP2_REG_RD_DISABLE_INPUTS, 0);

	if (!CHIP_IS_E1x(sc)) {
		int factor = 0;

		ecore_init_block(sc, BLOCK_PGLUE_B, PHASE_COMMON);
		ecore_init_block(sc, BLOCK_ATC, PHASE_COMMON);

/* let the HW do it's magic... */
		do {
			DELAY(200000);
			val = REG_RD(sc, ATC_REG_ATC_INIT_DONE);
		} while (factor-- && (val != 1));

		if (val != 1) {
			PMD_DRV_LOG(NOTICE, sc, "ATC_INIT failed");
			return -1;
		}
	}

	ecore_init_block(sc, BLOCK_DMAE, PHASE_COMMON);

	/* clean the DMAE memory */
	sc->dmae_ready = 1;
	ecore_init_fill(sc, TSEM_REG_PRAM, 0, 8, 1);

	ecore_init_block(sc, BLOCK_TCM, PHASE_COMMON);

	ecore_init_block(sc, BLOCK_UCM, PHASE_COMMON);

	ecore_init_block(sc, BLOCK_CCM, PHASE_COMMON);

	ecore_init_block(sc, BLOCK_XCM, PHASE_COMMON);

	bnx2x_read_dmae(sc, XSEM_REG_PASSIVE_BUFFER, 3);
	bnx2x_read_dmae(sc, CSEM_REG_PASSIVE_BUFFER, 3);
	bnx2x_read_dmae(sc, TSEM_REG_PASSIVE_BUFFER, 3);
	bnx2x_read_dmae(sc, USEM_REG_PASSIVE_BUFFER, 3);

	ecore_init_block(sc, BLOCK_QM, PHASE_COMMON);

	/* QM queues pointers table */
	ecore_qm_init_ptr_table(sc, sc->qm_cid_count, INITOP_SET);

	/* soft reset pulse */
	REG_WR(sc, QM_REG_SOFT_RESET, 1);
	REG_WR(sc, QM_REG_SOFT_RESET, 0);

	if (CNIC_SUPPORT(sc))
		ecore_init_block(sc, BLOCK_TM, PHASE_COMMON);

	ecore_init_block(sc, BLOCK_DORQ, PHASE_COMMON);

	if (!CHIP_REV_IS_SLOW(sc)) {
/* enable hw interrupt from doorbell Q */
		REG_WR(sc, DORQ_REG_DORQ_INT_MASK, 0);
	}

	ecore_init_block(sc, BLOCK_BRB1, PHASE_COMMON);

	ecore_init_block(sc, BLOCK_PRS, PHASE_COMMON);
	REG_WR(sc, PRS_REG_A_PRSU_20, 0xf);
	REG_WR(sc, PRS_REG_E1HOV_MODE, sc->devinfo.mf_info.path_has_ovlan);

	if (!CHIP_IS_E1x(sc) && !CHIP_IS_E3B0(sc)) {
		if (IS_MF_AFEX(sc)) {
			/*
			 * configure that AFEX and VLAN headers must be
			 * received in AFEX mode
			 */
			REG_WR(sc, PRS_REG_HDRS_AFTER_BASIC, 0xE);
			REG_WR(sc, PRS_REG_MUST_HAVE_HDRS, 0xA);
			REG_WR(sc, PRS_REG_HDRS_AFTER_TAG_0, 0x6);
			REG_WR(sc, PRS_REG_TAG_ETHERTYPE_0, 0x8926);
			REG_WR(sc, PRS_REG_TAG_LEN_0, 0x4);
		} else {
			/*
			 * Bit-map indicating which L2 hdrs may appear
			 * after the basic Ethernet header
			 */
			REG_WR(sc, PRS_REG_HDRS_AFTER_BASIC,
			       sc->devinfo.mf_info.path_has_ovlan ? 7 : 6);
		}
	}

	ecore_init_block(sc, BLOCK_TSDM, PHASE_COMMON);
	ecore_init_block(sc, BLOCK_CSDM, PHASE_COMMON);
	ecore_init_block(sc, BLOCK_USDM, PHASE_COMMON);
	ecore_init_block(sc, BLOCK_XSDM, PHASE_COMMON);

	if (!CHIP_IS_E1x(sc)) {
/* reset VFC memories */
		REG_WR(sc, TSEM_REG_FAST_MEMORY + VFC_REG_MEMORIES_RST,
		       VFC_MEMORIES_RST_REG_CAM_RST |
		       VFC_MEMORIES_RST_REG_RAM_RST);
		REG_WR(sc, XSEM_REG_FAST_MEMORY + VFC_REG_MEMORIES_RST,
		       VFC_MEMORIES_RST_REG_CAM_RST |
		       VFC_MEMORIES_RST_REG_RAM_RST);

		DELAY(20000);
	}

	ecore_init_block(sc, BLOCK_TSEM, PHASE_COMMON);
	ecore_init_block(sc, BLOCK_USEM, PHASE_COMMON);
	ecore_init_block(sc, BLOCK_CSEM, PHASE_COMMON);
	ecore_init_block(sc, BLOCK_XSEM, PHASE_COMMON);

	/* sync semi rtc */
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_1_CLEAR, 0x80000000);
	REG_WR(sc, GRCBASE_MISC + MISC_REGISTERS_RESET_REG_1_SET, 0x80000000);

	ecore_init_block(sc, BLOCK_UPB, PHASE_COMMON);
	ecore_init_block(sc, BLOCK_XPB, PHASE_COMMON);
	ecore_init_block(sc, BLOCK_PBF, PHASE_COMMON);

	if (!CHIP_IS_E1x(sc)) {
		if (IS_MF_AFEX(sc)) {
			/*
			 * configure that AFEX and VLAN headers must be
			 * sent in AFEX mode
			 */
			REG_WR(sc, PBF_REG_HDRS_AFTER_BASIC, 0xE);
			REG_WR(sc, PBF_REG_MUST_HAVE_HDRS, 0xA);
			REG_WR(sc, PBF_REG_HDRS_AFTER_TAG_0, 0x6);
			REG_WR(sc, PBF_REG_TAG_ETHERTYPE_0, 0x8926);
			REG_WR(sc, PBF_REG_TAG_LEN_0, 0x4);
		} else {
			REG_WR(sc, PBF_REG_HDRS_AFTER_BASIC,
			       sc->devinfo.mf_info.path_has_ovlan ? 7 : 6);
		}
	}

	REG_WR(sc, SRC_REG_SOFT_RST, 1);

	ecore_init_block(sc, BLOCK_SRC, PHASE_COMMON);

	if (CNIC_SUPPORT(sc)) {
		REG_WR(sc, SRC_REG_KEYSEARCH_0, 0x63285672);
		REG_WR(sc, SRC_REG_KEYSEARCH_1, 0x24b8f2cc);
		REG_WR(sc, SRC_REG_KEYSEARCH_2, 0x223aef9b);
		REG_WR(sc, SRC_REG_KEYSEARCH_3, 0x26001e3a);
		REG_WR(sc, SRC_REG_KEYSEARCH_4, 0x7ae91116);
		REG_WR(sc, SRC_REG_KEYSEARCH_5, 0x5ce5230b);
		REG_WR(sc, SRC_REG_KEYSEARCH_6, 0x298d8adf);
		REG_WR(sc, SRC_REG_KEYSEARCH_7, 0x6eb0ff09);
		REG_WR(sc, SRC_REG_KEYSEARCH_8, 0x1830f82f);
		REG_WR(sc, SRC_REG_KEYSEARCH_9, 0x01e46be7);
	}
	REG_WR(sc, SRC_REG_SOFT_RST, 0);

	if (sizeof(union cdu_context) != 1024) {
/* we currently assume that a context is 1024 bytes */
		PMD_DRV_LOG(NOTICE, sc,
			    "please adjust the size of cdu_context(%ld)",
			    (long)sizeof(union cdu_context));
	}

	ecore_init_block(sc, BLOCK_CDU, PHASE_COMMON);
	val = (4 << 24) + (0 << 12) + 1024;
	REG_WR(sc, CDU_REG_CDU_GLOBAL_PARAMS, val);

	ecore_init_block(sc, BLOCK_CFC, PHASE_COMMON);

	REG_WR(sc, CFC_REG_INIT_REG, 0x7FF);
	/* enable context validation interrupt from CFC */
	REG_WR(sc, CFC_REG_CFC_INT_MASK, 0);

	/* set the thresholds to prevent CFC/CDU race */
	REG_WR(sc, CFC_REG_DEBUG0, 0x20020000);
	ecore_init_block(sc, BLOCK_HC, PHASE_COMMON);

	if (!CHIP_IS_E1x(sc) && BNX2X_NOMCP(sc)) {
		REG_WR(sc, IGU_REG_RESET_MEMORIES, 0x36);
	}

	ecore_init_block(sc, BLOCK_IGU, PHASE_COMMON);
	ecore_init_block(sc, BLOCK_MISC_AEU, PHASE_COMMON);

	/* Reset PCIE errors for debug */
	REG_WR(sc, 0x2814, 0xffffffff);
	REG_WR(sc, 0x3820, 0xffffffff);

	if (!CHIP_IS_E1x(sc)) {
		REG_WR(sc, PCICFG_OFFSET + PXPCS_TL_CONTROL_5,
		       (PXPCS_TL_CONTROL_5_ERR_UNSPPORT1 |
			PXPCS_TL_CONTROL_5_ERR_UNSPPORT));
		REG_WR(sc, PCICFG_OFFSET + PXPCS_TL_FUNC345_STAT,
		       (PXPCS_TL_FUNC345_STAT_ERR_UNSPPORT4 |
			PXPCS_TL_FUNC345_STAT_ERR_UNSPPORT3 |
			PXPCS_TL_FUNC345_STAT_ERR_UNSPPORT2));
		REG_WR(sc, PCICFG_OFFSET + PXPCS_TL_FUNC678_STAT,
		       (PXPCS_TL_FUNC678_STAT_ERR_UNSPPORT7 |
			PXPCS_TL_FUNC678_STAT_ERR_UNSPPORT6 |
			PXPCS_TL_FUNC678_STAT_ERR_UNSPPORT5));
	}

	ecore_init_block(sc, BLOCK_NIG, PHASE_COMMON);

	/* in E3 this done in per-port section */
	if (!CHIP_IS_E3(sc))
		REG_WR(sc, NIG_REG_LLH_MF_MODE, IS_MF(sc));

	if (CHIP_IS_E1H(sc)) {
/* not applicable for E2 (and above ...) */
		REG_WR(sc, NIG_REG_LLH_E1HOV_MODE, IS_MF_SD(sc));
	}

	if (CHIP_REV_IS_SLOW(sc)) {
		DELAY(200000);
	}

	/* finish CFC init */
	val = reg_poll(sc, CFC_REG_LL_INIT_DONE, 1, 100, 10);
	if (val != 1) {
		PMD_DRV_LOG(NOTICE, sc, "CFC LL_INIT failed");
		return -1;
	}
	val = reg_poll(sc, CFC_REG_AC_INIT_DONE, 1, 100, 10);
	if (val != 1) {
		PMD_DRV_LOG(NOTICE, sc, "CFC AC_INIT failed");
		return -1;
	}
	val = reg_poll(sc, CFC_REG_CAM_INIT_DONE, 1, 100, 10);
	if (val != 1) {
		PMD_DRV_LOG(NOTICE, sc, "CFC CAM_INIT failed");
		return -1;
	}
	REG_WR(sc, CFC_REG_DEBUG0, 0);

	bnx2x_setup_fan_failure_detection(sc);

	/* clear PXP2 attentions */
	REG_RD(sc, PXP2_REG_PXP2_INT_STS_CLR_0);

	bnx2x_enable_blocks_attention(sc);

	if (!CHIP_REV_IS_SLOW(sc)) {
		ecore_enable_blocks_parity(sc);
	}

	if (!BNX2X_NOMCP(sc)) {
		if (CHIP_IS_E1x(sc)) {
			bnx2x_common_init_phy(sc);
		}
	}

	return 0;
}

/**
 * bnx2x_init_hw_common_chip - init HW at the COMMON_CHIP phase.
 *
 * @sc:     driver handle
 */
static int bnx2x_init_hw_common_chip(struct bnx2x_softc *sc)
{
	int rc = bnx2x_init_hw_common(sc);

	if (rc) {
		return rc;
	}

	/* In E2 2-PORT mode, same ext phy is used for the two paths */
	if (!BNX2X_NOMCP(sc)) {
		bnx2x_common_init_phy(sc);
	}

	return 0;
}

static int bnx2x_init_hw_port(struct bnx2x_softc *sc)
{
	int port = SC_PORT(sc);
	int init_phase = port ? PHASE_PORT1 : PHASE_PORT0;
	uint32_t low, high;
	uint32_t val;

	PMD_DRV_LOG(DEBUG, sc, "starting port init for port %d", port);

	REG_WR(sc, NIG_REG_MASK_INTERRUPT_PORT0 + port * 4, 0);

	ecore_init_block(sc, BLOCK_MISC, init_phase);
	ecore_init_block(sc, BLOCK_PXP, init_phase);
	ecore_init_block(sc, BLOCK_PXP2, init_phase);

	/*
	 * Timers bug workaround: disables the pf_master bit in pglue at
	 * common phase, we need to enable it here before any dmae access are
	 * attempted. Therefore we manually added the enable-master to the
	 * port phase (it also happens in the function phase)
	 */
	if (!CHIP_IS_E1x(sc)) {
		REG_WR(sc, PGLUE_B_REG_INTERNAL_PFID_ENABLE_MASTER, 1);
	}

	ecore_init_block(sc, BLOCK_ATC, init_phase);
	ecore_init_block(sc, BLOCK_DMAE, init_phase);
	ecore_init_block(sc, BLOCK_PGLUE_B, init_phase);
	ecore_init_block(sc, BLOCK_QM, init_phase);

	ecore_init_block(sc, BLOCK_TCM, init_phase);
	ecore_init_block(sc, BLOCK_UCM, init_phase);
	ecore_init_block(sc, BLOCK_CCM, init_phase);
	ecore_init_block(sc, BLOCK_XCM, init_phase);

	/* QM cid (connection) count */
	ecore_qm_init_cid_count(sc, sc->qm_cid_count, INITOP_SET);

	if (CNIC_SUPPORT(sc)) {
		ecore_init_block(sc, BLOCK_TM, init_phase);
		REG_WR(sc, TM_REG_LIN0_SCAN_TIME + port * 4, 20);
		REG_WR(sc, TM_REG_LIN0_MAX_ACTIVE_CID + port * 4, 31);
	}

	ecore_init_block(sc, BLOCK_DORQ, init_phase);

	ecore_init_block(sc, BLOCK_BRB1, init_phase);

	if (CHIP_IS_E1H(sc)) {
		if (IS_MF(sc)) {
			low = (BNX2X_ONE_PORT(sc) ? 160 : 246);
		} else if (sc->mtu > 4096) {
			if (BNX2X_ONE_PORT(sc)) {
				low = 160;
			} else {
				val = sc->mtu;
				/* (24*1024 + val*4)/256 */
				low = (96 + (val / 64) + ((val % 64) ? 1 : 0));
			}
		} else {
			low = (BNX2X_ONE_PORT(sc) ? 80 : 160);
		}
		high = (low + 56);	/* 14*1024/256 */
		REG_WR(sc, BRB1_REG_PAUSE_LOW_THRESHOLD_0 + port * 4, low);
		REG_WR(sc, BRB1_REG_PAUSE_HIGH_THRESHOLD_0 + port * 4, high);
	}

	if (CHIP_IS_MODE_4_PORT(sc)) {
		REG_WR(sc, SC_PORT(sc) ?
		       BRB1_REG_MAC_GUARANTIED_1 :
		       BRB1_REG_MAC_GUARANTIED_0, 40);
	}

	ecore_init_block(sc, BLOCK_PRS, init_phase);
	if (CHIP_IS_E3B0(sc)) {
		if (IS_MF_AFEX(sc)) {
			/* configure headers for AFEX mode */
			if (SC_PORT(sc)) {
				REG_WR(sc, PRS_REG_HDRS_AFTER_BASIC_PORT_1,
				       0xE);
				REG_WR(sc, PRS_REG_HDRS_AFTER_TAG_0_PORT_1,
				       0x6);
				REG_WR(sc, PRS_REG_MUST_HAVE_HDRS_PORT_1, 0xA);
			} else {
				REG_WR(sc, PRS_REG_HDRS_AFTER_BASIC_PORT_0,
				       0xE);
				REG_WR(sc, PRS_REG_HDRS_AFTER_TAG_0_PORT_0,
				       0x6);
				REG_WR(sc, PRS_REG_MUST_HAVE_HDRS_PORT_0, 0xA);
			}
		} else {
			/* Ovlan exists only if we are in multi-function +
			 * switch-dependent mode, in switch-independent there
			 * is no ovlan headers
			 */
			REG_WR(sc, SC_PORT(sc) ?
			       PRS_REG_HDRS_AFTER_BASIC_PORT_1 :
			       PRS_REG_HDRS_AFTER_BASIC_PORT_0,
			       (sc->devinfo.mf_info.path_has_ovlan ? 7 : 6));
		}
	}

	ecore_init_block(sc, BLOCK_TSDM, init_phase);
	ecore_init_block(sc, BLOCK_CSDM, init_phase);
	ecore_init_block(sc, BLOCK_USDM, init_phase);
	ecore_init_block(sc, BLOCK_XSDM, init_phase);

	ecore_init_block(sc, BLOCK_TSEM, init_phase);
	ecore_init_block(sc, BLOCK_USEM, init_phase);
	ecore_init_block(sc, BLOCK_CSEM, init_phase);
	ecore_init_block(sc, BLOCK_XSEM, init_phase);

	ecore_init_block(sc, BLOCK_UPB, init_phase);
	ecore_init_block(sc, BLOCK_XPB, init_phase);

	ecore_init_block(sc, BLOCK_PBF, init_phase);

	if (CHIP_IS_E1x(sc)) {
/* configure PBF to work without PAUSE mtu 9000 */
		REG_WR(sc, PBF_REG_P0_PAUSE_ENABLE + port * 4, 0);

/* update threshold */
		REG_WR(sc, PBF_REG_P0_ARB_THRSH + port * 4, (9040 / 16));
/* update init credit */
		REG_WR(sc, PBF_REG_P0_INIT_CRD + port * 4,
		       (9040 / 16) + 553 - 22);

/* probe changes */
		REG_WR(sc, PBF_REG_INIT_P0 + port * 4, 1);
		DELAY(50);
		REG_WR(sc, PBF_REG_INIT_P0 + port * 4, 0);
	}

	if (CNIC_SUPPORT(sc)) {
		ecore_init_block(sc, BLOCK_SRC, init_phase);
	}

	ecore_init_block(sc, BLOCK_CDU, init_phase);
	ecore_init_block(sc, BLOCK_CFC, init_phase);
	ecore_init_block(sc, BLOCK_HC, init_phase);
	ecore_init_block(sc, BLOCK_IGU, init_phase);
	ecore_init_block(sc, BLOCK_MISC_AEU, init_phase);
	/* init aeu_mask_attn_func_0/1:
	 *  - SF mode: bits 3-7 are masked. only bits 0-2 are in use
	 *  - MF mode: bit 3 is masked. bits 0-2 are in use as in SF
	 *             bits 4-7 are used for "per vn group attention" */
	val = IS_MF(sc) ? 0xF7 : 0x7;
	val |= 0x10;
	REG_WR(sc, MISC_REG_AEU_MASK_ATTN_FUNC_0 + port * 4, val);

	ecore_init_block(sc, BLOCK_NIG, init_phase);

	if (!CHIP_IS_E1x(sc)) {
/* Bit-map indicating which L2 hdrs may appear after the
 * basic Ethernet header
 */
		if (IS_MF_AFEX(sc)) {
			REG_WR(sc, SC_PORT(sc) ?
			       NIG_REG_P1_HDRS_AFTER_BASIC :
			       NIG_REG_P0_HDRS_AFTER_BASIC, 0xE);
		} else {
			REG_WR(sc, SC_PORT(sc) ?
			       NIG_REG_P1_HDRS_AFTER_BASIC :
			       NIG_REG_P0_HDRS_AFTER_BASIC,
			       IS_MF_SD(sc) ? 7 : 6);
		}

		if (CHIP_IS_E3(sc)) {
			REG_WR(sc, SC_PORT(sc) ?
			       NIG_REG_LLH1_MF_MODE :
			       NIG_REG_LLH_MF_MODE, IS_MF(sc));
		}
	}
	if (!CHIP_IS_E3(sc)) {
		REG_WR(sc, NIG_REG_XGXS_SERDES0_MODE_SEL + port * 4, 1);
	}

	/* 0x2 disable mf_ov, 0x1 enable */
	REG_WR(sc, NIG_REG_LLH0_BRB1_DRV_MASK_MF + port * 4,
	       (IS_MF_SD(sc) ? 0x1 : 0x2));

	if (!CHIP_IS_E1x(sc)) {
		val = 0;
		switch (sc->devinfo.mf_info.mf_mode) {
		case MULTI_FUNCTION_SD:
			val = 1;
			break;
		case MULTI_FUNCTION_SI:
		case MULTI_FUNCTION_AFEX:
			val = 2;
			break;
		}

		REG_WR(sc, (SC_PORT(sc) ? NIG_REG_LLH1_CLS_TYPE :
			    NIG_REG_LLH0_CLS_TYPE), val);
	}
	REG_WR(sc, NIG_REG_LLFC_ENABLE_0 + port * 4, 0);
	REG_WR(sc, NIG_REG_LLFC_OUT_EN_0 + port * 4, 0);
	REG_WR(sc, NIG_REG_PAUSE_ENABLE_0 + port * 4, 1);

	/* If SPIO5 is set to generate interrupts, enable it for this port */
	val = REG_RD(sc, MISC_REG_SPIO_EVENT_EN);
	if (val & MISC_SPIO_SPIO5) {
		uint32_t reg_addr = (port ? MISC_REG_AEU_ENABLE1_FUNC_1_OUT_0 :
				     MISC_REG_AEU_ENABLE1_FUNC_0_OUT_0);
		val = REG_RD(sc, reg_addr);
		val |= AEU_INPUTS_ATTN_BITS_SPIO5;
		REG_WR(sc, reg_addr, val);
	}

	return 0;
}

static uint32_t
bnx2x_flr_clnup_reg_poll(struct bnx2x_softc *sc, uint32_t reg,
		       uint32_t expected, uint32_t poll_count)
{
	uint32_t cur_cnt = poll_count;
	uint32_t val;

	while ((val = REG_RD(sc, reg)) != expected && cur_cnt--) {
		DELAY(FLR_WAIT_INTERVAL);
	}

	return val;
}

static int
bnx2x_flr_clnup_poll_hw_counter(struct bnx2x_softc *sc, uint32_t reg,
			      __rte_unused const char *msg, uint32_t poll_cnt)
{
	uint32_t val = bnx2x_flr_clnup_reg_poll(sc, reg, 0, poll_cnt);

	if (val != 0) {
		PMD_DRV_LOG(NOTICE, sc, "%s usage count=%d", msg, val);
		return -1;
	}

	return 0;
}

/* Common routines with VF FLR cleanup */
static uint32_t bnx2x_flr_clnup_poll_count(struct bnx2x_softc *sc)
{
	/* adjust polling timeout */
	if (CHIP_REV_IS_EMUL(sc)) {
		return FLR_POLL_CNT * 2000;
	}

	if (CHIP_REV_IS_FPGA(sc)) {
		return FLR_POLL_CNT * 120;
	}

	return FLR_POLL_CNT;
}

static int bnx2x_poll_hw_usage_counters(struct bnx2x_softc *sc, uint32_t poll_cnt)
{
	/* wait for CFC PF usage-counter to zero (includes all the VFs) */
	if (bnx2x_flr_clnup_poll_hw_counter(sc,
					  CFC_REG_NUM_LCIDS_INSIDE_PF,
					  "CFC PF usage counter timed out",
					  poll_cnt)) {
		return -1;
	}

	/* Wait for DQ PF usage-counter to zero (until DQ cleanup) */
	if (bnx2x_flr_clnup_poll_hw_counter(sc,
					  DORQ_REG_PF_USAGE_CNT,
					  "DQ PF usage counter timed out",
					  poll_cnt)) {
		return -1;
	}

	/* Wait for QM PF usage-counter to zero (until DQ cleanup) */
	if (bnx2x_flr_clnup_poll_hw_counter(sc,
					  QM_REG_PF_USG_CNT_0 + 4 * SC_FUNC(sc),
					  "QM PF usage counter timed out",
					  poll_cnt)) {
		return -1;
	}

	/* Wait for Timer PF usage-counters to zero (until DQ cleanup) */
	if (bnx2x_flr_clnup_poll_hw_counter(sc,
					  TM_REG_LIN0_VNIC_UC + 4 * SC_PORT(sc),
					  "Timers VNIC usage counter timed out",
					  poll_cnt)) {
		return -1;
	}

	if (bnx2x_flr_clnup_poll_hw_counter(sc,
					  TM_REG_LIN0_NUM_SCANS +
					  4 * SC_PORT(sc),
					  "Timers NUM_SCANS usage counter timed out",
					  poll_cnt)) {
		return -1;
	}

	/* Wait DMAE PF usage counter to zero */
	if (bnx2x_flr_clnup_poll_hw_counter(sc,
					  dmae_reg_go_c[INIT_DMAE_C(sc)],
					  "DMAE dommand register timed out",
					  poll_cnt)) {
		return -1;
	}

	return 0;
}

#define OP_GEN_PARAM(param)                                            \
	(((param) << SDM_OP_GEN_COMP_PARAM_SHIFT) & SDM_OP_GEN_COMP_PARAM)
#define OP_GEN_TYPE(type)                                           \
	(((type) << SDM_OP_GEN_COMP_TYPE_SHIFT) & SDM_OP_GEN_COMP_TYPE)
#define OP_GEN_AGG_VECT(index)                                             \
	(((index) << SDM_OP_GEN_AGG_VECT_IDX_SHIFT) & SDM_OP_GEN_AGG_VECT_IDX)

static int
bnx2x_send_final_clnup(struct bnx2x_softc *sc, uint8_t clnup_func,
		     uint32_t poll_cnt)
{
	uint32_t op_gen_command = 0;
	uint32_t comp_addr = (BAR_CSTRORM_INTMEM +
			      CSTORM_FINAL_CLEANUP_COMPLETE_OFFSET(clnup_func));
	int ret = 0;

	if (REG_RD(sc, comp_addr)) {
		PMD_DRV_LOG(NOTICE, sc,
			    "Cleanup complete was not 0 before sending");
		return -1;
	}

	op_gen_command |= OP_GEN_PARAM(XSTORM_AGG_INT_FINAL_CLEANUP_INDEX);
	op_gen_command |= OP_GEN_TYPE(XSTORM_AGG_INT_FINAL_CLEANUP_COMP_TYPE);
	op_gen_command |= OP_GEN_AGG_VECT(clnup_func);
	op_gen_command |= 1 << SDM_OP_GEN_AGG_VECT_IDX_VALID_SHIFT;

	REG_WR(sc, XSDM_REG_OPERATION_GEN, op_gen_command);

	if (bnx2x_flr_clnup_reg_poll(sc, comp_addr, 1, poll_cnt) != 1) {
		PMD_DRV_LOG(NOTICE, sc, "FW final cleanup did not succeed");
		PMD_DRV_LOG(DEBUG, sc, "At timeout completion address contained %x",
			    (REG_RD(sc, comp_addr)));
		rte_panic("FLR cleanup failed");
		return -1;
	}

	/* Zero completion for nxt FLR */
	REG_WR(sc, comp_addr, 0);

	return ret;
}

static void
bnx2x_pbf_pN_buf_flushed(struct bnx2x_softc *sc, struct pbf_pN_buf_regs *regs,
		       uint32_t poll_count)
{
	uint32_t init_crd, crd, crd_start, crd_freed, crd_freed_start;
	uint32_t cur_cnt = poll_count;

	crd_freed = crd_freed_start = REG_RD(sc, regs->crd_freed);
	crd = crd_start = REG_RD(sc, regs->crd);
	init_crd = REG_RD(sc, regs->init_crd);

	while ((crd != init_crd) &&
	       ((uint32_t) ((int32_t) crd_freed - (int32_t) crd_freed_start) <
		(init_crd - crd_start))) {
		if (cur_cnt--) {
			DELAY(FLR_WAIT_INTERVAL);
			crd = REG_RD(sc, regs->crd);
			crd_freed = REG_RD(sc, regs->crd_freed);
		} else {
			break;
		}
	}
}

static void
bnx2x_pbf_pN_cmd_flushed(struct bnx2x_softc *sc, struct pbf_pN_cmd_regs *regs,
		       uint32_t poll_count)
{
	uint32_t occup, to_free, freed, freed_start;
	uint32_t cur_cnt = poll_count;

	occup = to_free = REG_RD(sc, regs->lines_occup);
	freed = freed_start = REG_RD(sc, regs->lines_freed);

	while (occup &&
	       ((uint32_t) ((int32_t) freed - (int32_t) freed_start) <
		to_free)) {
		if (cur_cnt--) {
			DELAY(FLR_WAIT_INTERVAL);
			occup = REG_RD(sc, regs->lines_occup);
			freed = REG_RD(sc, regs->lines_freed);
		} else {
			break;
		}
	}
}

static void bnx2x_tx_hw_flushed(struct bnx2x_softc *sc, uint32_t poll_count)
{
	struct pbf_pN_cmd_regs cmd_regs[] = {
		{0, (CHIP_IS_E3B0(sc)) ?
		 PBF_REG_TQ_OCCUPANCY_Q0 : PBF_REG_P0_TQ_OCCUPANCY,
		 (CHIP_IS_E3B0(sc)) ?
		 PBF_REG_TQ_LINES_FREED_CNT_Q0 : PBF_REG_P0_TQ_LINES_FREED_CNT},
		{1, (CHIP_IS_E3B0(sc)) ?
		 PBF_REG_TQ_OCCUPANCY_Q1 : PBF_REG_P1_TQ_OCCUPANCY,
		 (CHIP_IS_E3B0(sc)) ?
		 PBF_REG_TQ_LINES_FREED_CNT_Q1 : PBF_REG_P1_TQ_LINES_FREED_CNT},
		{4, (CHIP_IS_E3B0(sc)) ?
		 PBF_REG_TQ_OCCUPANCY_LB_Q : PBF_REG_P4_TQ_OCCUPANCY,
		 (CHIP_IS_E3B0(sc)) ?
		 PBF_REG_TQ_LINES_FREED_CNT_LB_Q :
		 PBF_REG_P4_TQ_LINES_FREED_CNT}
	};

	struct pbf_pN_buf_regs buf_regs[] = {
		{0, (CHIP_IS_E3B0(sc)) ?
		 PBF_REG_INIT_CRD_Q0 : PBF_REG_P0_INIT_CRD,
		 (CHIP_IS_E3B0(sc)) ? PBF_REG_CREDIT_Q0 : PBF_REG_P0_CREDIT,
		 (CHIP_IS_E3B0(sc)) ?
		 PBF_REG_INTERNAL_CRD_FREED_CNT_Q0 :
		 PBF_REG_P0_INTERNAL_CRD_FREED_CNT},
		{1, (CHIP_IS_E3B0(sc)) ?
		 PBF_REG_INIT_CRD_Q1 : PBF_REG_P1_INIT_CRD,
		 (CHIP_IS_E3B0(sc)) ? PBF_REG_CREDIT_Q1 : PBF_REG_P1_CREDIT,
		 (CHIP_IS_E3B0(sc)) ?
		 PBF_REG_INTERNAL_CRD_FREED_CNT_Q1 :
		 PBF_REG_P1_INTERNAL_CRD_FREED_CNT},
		{4, (CHIP_IS_E3B0(sc)) ?
		 PBF_REG_INIT_CRD_LB_Q : PBF_REG_P4_INIT_CRD,
		 (CHIP_IS_E3B0(sc)) ? PBF_REG_CREDIT_LB_Q : PBF_REG_P4_CREDIT,
		 (CHIP_IS_E3B0(sc)) ?
		 PBF_REG_INTERNAL_CRD_FREED_CNT_LB_Q :
		 PBF_REG_P4_INTERNAL_CRD_FREED_CNT},
	};

	uint32_t i;

	/* Verify the command queues are flushed P0, P1, P4 */
	for (i = 0; i < ARRAY_SIZE(cmd_regs); i++) {
		bnx2x_pbf_pN_cmd_flushed(sc, &cmd_regs[i], poll_count);
	}

	/* Verify the transmission buffers are flushed P0, P1, P4 */
	for (i = 0; i < ARRAY_SIZE(buf_regs); i++) {
		bnx2x_pbf_pN_buf_flushed(sc, &buf_regs[i], poll_count);
	}
}

static void bnx2x_hw_enable_status(struct bnx2x_softc *sc)
{
	__rte_unused uint32_t val;

	val = REG_RD(sc, CFC_REG_WEAK_ENABLE_PF);
	PMD_DRV_LOG(DEBUG, sc, "CFC_REG_WEAK_ENABLE_PF is 0x%x", val);

	val = REG_RD(sc, PBF_REG_DISABLE_PF);
	PMD_DRV_LOG(DEBUG, sc, "PBF_REG_DISABLE_PF is 0x%x", val);

	val = REG_RD(sc, IGU_REG_PCI_PF_MSI_EN);
	PMD_DRV_LOG(DEBUG, sc, "IGU_REG_PCI_PF_MSI_EN is 0x%x", val);

	val = REG_RD(sc, IGU_REG_PCI_PF_MSIX_EN);
	PMD_DRV_LOG(DEBUG, sc, "IGU_REG_PCI_PF_MSIX_EN is 0x%x", val);

	val = REG_RD(sc, IGU_REG_PCI_PF_MSIX_FUNC_MASK);
	PMD_DRV_LOG(DEBUG, sc, "IGU_REG_PCI_PF_MSIX_FUNC_MASK is 0x%x", val);

	val = REG_RD(sc, PGLUE_B_REG_SHADOW_BME_PF_7_0_CLR);
	PMD_DRV_LOG(DEBUG, sc,
		    "PGLUE_B_REG_SHADOW_BME_PF_7_0_CLR is 0x%x", val);

	val = REG_RD(sc, PGLUE_B_REG_FLR_REQUEST_PF_7_0_CLR);
	PMD_DRV_LOG(DEBUG, sc,
		    "PGLUE_B_REG_FLR_REQUEST_PF_7_0_CLR is 0x%x", val);

	val = REG_RD(sc, PGLUE_B_REG_INTERNAL_PFID_ENABLE_MASTER);
	PMD_DRV_LOG(DEBUG, sc, "PGLUE_B_REG_INTERNAL_PFID_ENABLE_MASTER is 0x%x",
		    val);
}

/**
 *	bnx2x_pf_flr_clnup
 *	a. re-enable target read on the PF
 *	b. poll cfc per function usgae counter
 *	c. poll the qm perfunction usage counter
 *	d. poll the tm per function usage counter
 *	e. poll the tm per function scan-done indication
 *	f. clear the dmae channel associated wit hthe PF
 *	g. zero the igu 'trailing edge' and 'leading edge' regs (attentions)
 *	h. call the common flr cleanup code with -1 (pf indication)
 */
static int bnx2x_pf_flr_clnup(struct bnx2x_softc *sc)
{
	uint32_t poll_cnt = bnx2x_flr_clnup_poll_count(sc);

	/* Re-enable PF target read access */
	REG_WR(sc, PGLUE_B_REG_INTERNAL_PFID_ENABLE_TARGET_READ, 1);

	/* Poll HW usage counters */
	if (bnx2x_poll_hw_usage_counters(sc, poll_cnt)) {
		return -1;
	}

	/* Zero the igu 'trailing edge' and 'leading edge' */

	/* Send the FW cleanup command */
	if (bnx2x_send_final_clnup(sc, (uint8_t) SC_FUNC(sc), poll_cnt)) {
		return -1;
	}

	/* ATC cleanup */

	/* Verify TX hw is flushed */
	bnx2x_tx_hw_flushed(sc, poll_cnt);

	/* Wait 100ms (not adjusted according to platform) */
	DELAY(100000);

	/* Verify no pending pci transactions */
	if (bnx2x_is_pcie_pending(sc)) {
		PMD_DRV_LOG(NOTICE, sc, "PCIE Transactions still pending");
	}

	/* Debug */
	bnx2x_hw_enable_status(sc);

	/*
	 * Master enable - Due to WB DMAE writes performed before this
	 * register is re-initialized as part of the regular function init
	 */
	REG_WR(sc, PGLUE_B_REG_INTERNAL_PFID_ENABLE_MASTER, 1);

	return 0;
}

static int bnx2x_init_hw_func(struct bnx2x_softc *sc)
{
	int port = SC_PORT(sc);
	int func = SC_FUNC(sc);
	int init_phase = PHASE_PF0 + func;
	struct ecore_ilt *ilt = sc->ilt;
	uint16_t cdu_ilt_start;
	uint32_t addr, val;
	uint32_t main_mem_base, main_mem_size, main_mem_prty_clr;
	int main_mem_width, rc;
	uint32_t i;

	PMD_DRV_LOG(DEBUG, sc, "starting func init for func %d", func);

	/* FLR cleanup */
	if (!CHIP_IS_E1x(sc)) {
		rc = bnx2x_pf_flr_clnup(sc);
		if (rc) {
			PMD_DRV_LOG(NOTICE, sc, "FLR cleanup failed!");
			return rc;
		}
	}

	/* set MSI reconfigure capability */
	if (sc->devinfo.int_block == INT_BLOCK_HC) {
		addr = (port ? HC_REG_CONFIG_1 : HC_REG_CONFIG_0);
		val = REG_RD(sc, addr);
		val |= HC_CONFIG_0_REG_MSI_ATTN_EN_0;
		REG_WR(sc, addr, val);
	}

	ecore_init_block(sc, BLOCK_PXP, init_phase);
	ecore_init_block(sc, BLOCK_PXP2, init_phase);

	ilt = sc->ilt;
	cdu_ilt_start = ilt->clients[ILT_CLIENT_CDU].start;

	for (i = 0; i < L2_ILT_LINES(sc); i++) {
		ilt->lines[cdu_ilt_start + i].page = sc->context[i].vcxt;
		ilt->lines[cdu_ilt_start + i].page_mapping =
		    (rte_iova_t)sc->context[i].vcxt_dma.paddr;
		ilt->lines[cdu_ilt_start + i].size = sc->context[i].size;
	}
	ecore_ilt_init_op(sc, INITOP_SET);

	REG_WR(sc, PRS_REG_NIC_MODE, 1);

	if (!CHIP_IS_E1x(sc)) {
		uint32_t pf_conf = IGU_PF_CONF_FUNC_EN;

/* Turn on a single ISR mode in IGU if driver is going to use
 * INT#x or MSI
 */
		if ((sc->interrupt_mode != INTR_MODE_MSIX)
		    || (sc->interrupt_mode != INTR_MODE_SINGLE_MSIX)) {
			pf_conf |= IGU_PF_CONF_SINGLE_ISR_EN;
		}

/*
 * Timers workaround bug: function init part.
 * Need to wait 20msec after initializing ILT,
 * needed to make sure there are no requests in
 * one of the PXP internal queues with "old" ILT addresses
 */
		DELAY(20000);

/*
 * Master enable - Due to WB DMAE writes performed before this
 * register is re-initialized as part of the regular function
 * init
 */
		REG_WR(sc, PGLUE_B_REG_INTERNAL_PFID_ENABLE_MASTER, 1);
/* Enable the function in IGU */
		REG_WR(sc, IGU_REG_PF_CONFIGURATION, pf_conf);
	}

	sc->dmae_ready = 1;

	ecore_init_block(sc, BLOCK_PGLUE_B, init_phase);

	if (!CHIP_IS_E1x(sc))
		REG_WR(sc, PGLUE_B_REG_WAS_ERROR_PF_7_0_CLR, func);

	ecore_init_block(sc, BLOCK_ATC, init_phase);
	ecore_init_block(sc, BLOCK_DMAE, init_phase);
	ecore_init_block(sc, BLOCK_NIG, init_phase);
	ecore_init_block(sc, BLOCK_SRC, init_phase);
	ecore_init_block(sc, BLOCK_MISC, init_phase);
	ecore_init_block(sc, BLOCK_TCM, init_phase);
	ecore_init_block(sc, BLOCK_UCM, init_phase);
	ecore_init_block(sc, BLOCK_CCM, init_phase);
	ecore_init_block(sc, BLOCK_XCM, init_phase);
	ecore_init_block(sc, BLOCK_TSEM, init_phase);
	ecore_init_block(sc, BLOCK_USEM, init_phase);
	ecore_init_block(sc, BLOCK_CSEM, init_phase);
	ecore_init_block(sc, BLOCK_XSEM, init_phase);

	if (!CHIP_IS_E1x(sc))
		REG_WR(sc, QM_REG_PF_EN, 1);

	if (!CHIP_IS_E1x(sc)) {
		REG_WR(sc, TSEM_REG_VFPF_ERR_NUM, BNX2X_MAX_NUM_OF_VFS + func);
		REG_WR(sc, USEM_REG_VFPF_ERR_NUM, BNX2X_MAX_NUM_OF_VFS + func);
		REG_WR(sc, CSEM_REG_VFPF_ERR_NUM, BNX2X_MAX_NUM_OF_VFS + func);
		REG_WR(sc, XSEM_REG_VFPF_ERR_NUM, BNX2X_MAX_NUM_OF_VFS + func);
	}
	ecore_init_block(sc, BLOCK_QM, init_phase);

	ecore_init_block(sc, BLOCK_TM, init_phase);
	ecore_init_block(sc, BLOCK_DORQ, init_phase);

	ecore_init_block(sc, BLOCK_BRB1, init_phase);
	ecore_init_block(sc, BLOCK_PRS, init_phase);
	ecore_init_block(sc, BLOCK_TSDM, init_phase);
	ecore_init_block(sc, BLOCK_CSDM, init_phase);
	ecore_init_block(sc, BLOCK_USDM, init_phase);
	ecore_init_block(sc, BLOCK_XSDM, init_phase);
	ecore_init_block(sc, BLOCK_UPB, init_phase);
	ecore_init_block(sc, BLOCK_XPB, init_phase);
	ecore_init_block(sc, BLOCK_PBF, init_phase);
	if (!CHIP_IS_E1x(sc))
		REG_WR(sc, PBF_REG_DISABLE_PF, 0);

	ecore_init_block(sc, BLOCK_CDU, init_phase);

	ecore_init_block(sc, BLOCK_CFC, init_phase);

	if (!CHIP_IS_E1x(sc))
		REG_WR(sc, CFC_REG_WEAK_ENABLE_PF, 1);

	if (IS_MF(sc)) {
		REG_WR(sc, NIG_REG_LLH0_FUNC_EN + port * 8, 1);
		REG_WR(sc, NIG_REG_LLH0_FUNC_VLAN_ID + port * 8, OVLAN(sc));
	}

	ecore_init_block(sc, BLOCK_MISC_AEU, init_phase);

	/* HC init per function */
	if (sc->devinfo.int_block == INT_BLOCK_HC) {
		if (CHIP_IS_E1H(sc)) {
			REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_12 + func * 4, 0);

			REG_WR(sc, HC_REG_LEADING_EDGE_0 + port * 8, 0);
			REG_WR(sc, HC_REG_TRAILING_EDGE_0 + port * 8, 0);
		}
		ecore_init_block(sc, BLOCK_HC, init_phase);

	} else {
		uint32_t num_segs, sb_idx, prod_offset;

		REG_WR(sc, MISC_REG_AEU_GENERAL_ATTN_12 + func * 4, 0);

		if (!CHIP_IS_E1x(sc)) {
			REG_WR(sc, IGU_REG_LEADING_EDGE_LATCH, 0);
			REG_WR(sc, IGU_REG_TRAILING_EDGE_LATCH, 0);
		}

		ecore_init_block(sc, BLOCK_IGU, init_phase);

		if (!CHIP_IS_E1x(sc)) {
			int dsb_idx = 0;
	/**
	 * Producer memory:
	 * E2 mode: address 0-135 match to the mapping memory;
	 * 136 - PF0 default prod; 137 - PF1 default prod;
	 * 138 - PF2 default prod; 139 - PF3 default prod;
	 * 140 - PF0 attn prod;    141 - PF1 attn prod;
	 * 142 - PF2 attn prod;    143 - PF3 attn prod;
	 * 144-147 reserved.
	 *
	 * E1.5 mode - In backward compatible mode;
	 * for non default SB; each even line in the memory
	 * holds the U producer and each odd line hold
	 * the C producer. The first 128 producers are for
	 * NDSB (PF0 - 0-31; PF1 - 32-63 and so on). The last 20
	 * producers are for the DSB for each PF.
	 * Each PF has five segments: (the order inside each
	 * segment is PF0; PF1; PF2; PF3) - 128-131 U prods;
	 * 132-135 C prods; 136-139 X prods; 140-143 T prods;
	 * 144-147 attn prods;
	 */
			/* non-default-status-blocks */
			num_segs = CHIP_INT_MODE_IS_BC(sc) ?
			    IGU_BC_NDSB_NUM_SEGS : IGU_NORM_NDSB_NUM_SEGS;
			for (sb_idx = 0; sb_idx < sc->igu_sb_cnt; sb_idx++) {
				prod_offset = (sc->igu_base_sb + sb_idx) *
				    num_segs;

				for (i = 0; i < num_segs; i++) {
					addr = IGU_REG_PROD_CONS_MEMORY +
					    (prod_offset + i) * 4;
					REG_WR(sc, addr, 0);
				}
				/* send consumer update with value 0 */
				bnx2x_ack_sb(sc, sc->igu_base_sb + sb_idx,
					   USTORM_ID, 0, IGU_INT_NOP, 1);
				bnx2x_igu_clear_sb(sc, sc->igu_base_sb + sb_idx);
			}

			/* default-status-blocks */
			num_segs = CHIP_INT_MODE_IS_BC(sc) ?
			    IGU_BC_DSB_NUM_SEGS : IGU_NORM_DSB_NUM_SEGS;

			if (CHIP_IS_MODE_4_PORT(sc))
				dsb_idx = SC_FUNC(sc);
			else
				dsb_idx = SC_VN(sc);

			prod_offset = (CHIP_INT_MODE_IS_BC(sc) ?
				       IGU_BC_BASE_DSB_PROD + dsb_idx :
				       IGU_NORM_BASE_DSB_PROD + dsb_idx);

			/*
			 * igu prods come in chunks of E1HVN_MAX (4) -
			 * does not matters what is the current chip mode
			 */
			for (i = 0; i < (num_segs * E1HVN_MAX); i += E1HVN_MAX) {
				addr = IGU_REG_PROD_CONS_MEMORY +
				    (prod_offset + i) * 4;
				REG_WR(sc, addr, 0);
			}
			/* send consumer update with 0 */
			if (CHIP_INT_MODE_IS_BC(sc)) {
				bnx2x_ack_sb(sc, sc->igu_dsb_id,
					   USTORM_ID, 0, IGU_INT_NOP, 1);
				bnx2x_ack_sb(sc, sc->igu_dsb_id,
					   CSTORM_ID, 0, IGU_INT_NOP, 1);
				bnx2x_ack_sb(sc, sc->igu_dsb_id,
					   XSTORM_ID, 0, IGU_INT_NOP, 1);
				bnx2x_ack_sb(sc, sc->igu_dsb_id,
					   TSTORM_ID, 0, IGU_INT_NOP, 1);
				bnx2x_ack_sb(sc, sc->igu_dsb_id,
					   ATTENTION_ID, 0, IGU_INT_NOP, 1);
			} else {
				bnx2x_ack_sb(sc, sc->igu_dsb_id,
					   USTORM_ID, 0, IGU_INT_NOP, 1);
				bnx2x_ack_sb(sc, sc->igu_dsb_id,
					   ATTENTION_ID, 0, IGU_INT_NOP, 1);
			}
			bnx2x_igu_clear_sb(sc, sc->igu_dsb_id);

			/* !!! these should become driver const once
			   rf-tool supports split-68 const */
			REG_WR(sc, IGU_REG_SB_INT_BEFORE_MASK_LSB, 0);
			REG_WR(sc, IGU_REG_SB_INT_BEFORE_MASK_MSB, 0);
			REG_WR(sc, IGU_REG_SB_MASK_LSB, 0);
			REG_WR(sc, IGU_REG_SB_MASK_MSB, 0);
			REG_WR(sc, IGU_REG_PBA_STATUS_LSB, 0);
			REG_WR(sc, IGU_REG_PBA_STATUS_MSB, 0);
		}
	}

	/* Reset PCIE errors for debug */
	REG_WR(sc, 0x2114, 0xffffffff);
	REG_WR(sc, 0x2120, 0xffffffff);

	if (CHIP_IS_E1x(sc)) {
		main_mem_size = HC_REG_MAIN_MEMORY_SIZE / 2;	/*dwords */
		main_mem_base = HC_REG_MAIN_MEMORY +
		    SC_PORT(sc) * (main_mem_size * 4);
		main_mem_prty_clr = HC_REG_HC_PRTY_STS_CLR;
		main_mem_width = 8;

		val = REG_RD(sc, main_mem_prty_clr);
		if (val) {
			PMD_DRV_LOG(DEBUG, sc,
				    "Parity errors in HC block during function init (0x%x)!",
				    val);
		}

/* Clear "false" parity errors in MSI-X table */
		for (i = main_mem_base;
		     i < main_mem_base + main_mem_size * 4;
		     i += main_mem_width) {
			bnx2x_read_dmae(sc, i, main_mem_width / 4);
			bnx2x_write_dmae(sc, BNX2X_SP_MAPPING(sc, wb_data),
				       i, main_mem_width / 4);
		}
/* Clear HC parity attention */
		REG_RD(sc, main_mem_prty_clr);
	}

	/* Enable STORMs SP logging */
	REG_WR8(sc, BAR_USTRORM_INTMEM +
		USTORM_RECORD_SLOW_PATH_OFFSET(SC_FUNC(sc)), 1);
	REG_WR8(sc, BAR_TSTRORM_INTMEM +
		TSTORM_RECORD_SLOW_PATH_OFFSET(SC_FUNC(sc)), 1);
	REG_WR8(sc, BAR_CSTRORM_INTMEM +
		CSTORM_RECORD_SLOW_PATH_OFFSET(SC_FUNC(sc)), 1);
	REG_WR8(sc, BAR_XSTRORM_INTMEM +
		XSTORM_RECORD_SLOW_PATH_OFFSET(SC_FUNC(sc)), 1);

	elink_phy_probe(&sc->link_params);

	return 0;
}

static void bnx2x_link_reset(struct bnx2x_softc *sc)
{
	if (!BNX2X_NOMCP(sc)) {
		bnx2x_acquire_phy_lock(sc);
		elink_lfa_reset(&sc->link_params, &sc->link_vars);
		bnx2x_release_phy_lock(sc);
	} else {
		if (!CHIP_REV_IS_SLOW(sc)) {
			PMD_DRV_LOG(WARNING, sc,
				    "Bootcode is missing - cannot reset link");
		}
	}
}

static void bnx2x_reset_port(struct bnx2x_softc *sc)
{
	int port = SC_PORT(sc);
	uint32_t val;

	/* reset physical Link */
	bnx2x_link_reset(sc);

	REG_WR(sc, NIG_REG_MASK_INTERRUPT_PORT0 + port * 4, 0);

	/* Do not rcv packets to BRB */
	REG_WR(sc, NIG_REG_LLH0_BRB1_DRV_MASK + port * 4, 0x0);
	/* Do not direct rcv packets that are not for MCP to the BRB */
	REG_WR(sc, (port ? NIG_REG_LLH1_BRB1_NOT_MCP :
		    NIG_REG_LLH0_BRB1_NOT_MCP), 0x0);

	/* Configure AEU */
	REG_WR(sc, MISC_REG_AEU_MASK_ATTN_FUNC_0 + port * 4, 0);

	DELAY(100000);

	/* Check for BRB port occupancy */
	val = REG_RD(sc, BRB1_REG_PORT_NUM_OCC_BLOCKS_0 + port * 4);
	if (val) {
		PMD_DRV_LOG(DEBUG, sc,
			    "BRB1 is not empty, %d blocks are occupied", val);
	}
}

static void bnx2x_ilt_wr(struct bnx2x_softc *sc, uint32_t index, rte_iova_t addr)
{
	int reg;
	uint32_t wb_write[2];

	reg = PXP2_REG_RQ_ONCHIP_AT_B0 + index * 8;

	wb_write[0] = ONCHIP_ADDR1(addr);
	wb_write[1] = ONCHIP_ADDR2(addr);
	REG_WR_DMAE(sc, reg, wb_write, 2);
}

static void bnx2x_clear_func_ilt(struct bnx2x_softc *sc, uint32_t func)
{
	uint32_t i, base = FUNC_ILT_BASE(func);
	for (i = base; i < base + ILT_PER_FUNC; i++) {
		bnx2x_ilt_wr(sc, i, 0);
	}
}

static void bnx2x_reset_func(struct bnx2x_softc *sc)
{
	struct bnx2x_fastpath *fp;
	int port = SC_PORT(sc);
	int func = SC_FUNC(sc);
	int i;

	/* Disable the function in the FW */
	REG_WR8(sc, BAR_XSTRORM_INTMEM + XSTORM_FUNC_EN_OFFSET(func), 0);
	REG_WR8(sc, BAR_CSTRORM_INTMEM + CSTORM_FUNC_EN_OFFSET(func), 0);
	REG_WR8(sc, BAR_TSTRORM_INTMEM + TSTORM_FUNC_EN_OFFSET(func), 0);
	REG_WR8(sc, BAR_USTRORM_INTMEM + USTORM_FUNC_EN_OFFSET(func), 0);

	/* FP SBs */
	FOR_EACH_ETH_QUEUE(sc, i) {
		fp = &sc->fp[i];
		REG_WR8(sc, BAR_CSTRORM_INTMEM +
			CSTORM_STATUS_BLOCK_DATA_STATE_OFFSET(fp->fw_sb_id),
			SB_DISABLED);
	}

	/* SP SB */
	REG_WR8(sc, BAR_CSTRORM_INTMEM +
		CSTORM_SP_STATUS_BLOCK_DATA_STATE_OFFSET(func), SB_DISABLED);

	for (i = 0; i < XSTORM_SPQ_DATA_SIZE / 4; i++) {
		REG_WR(sc, BAR_XSTRORM_INTMEM + XSTORM_SPQ_DATA_OFFSET(func),
		       0);
	}

	/* Configure IGU */
	if (sc->devinfo.int_block == INT_BLOCK_HC) {
		REG_WR(sc, HC_REG_LEADING_EDGE_0 + port * 8, 0);
		REG_WR(sc, HC_REG_TRAILING_EDGE_0 + port * 8, 0);
	} else {
		REG_WR(sc, IGU_REG_LEADING_EDGE_LATCH, 0);
		REG_WR(sc, IGU_REG_TRAILING_EDGE_LATCH, 0);
	}

	if (CNIC_LOADED(sc)) {
/* Disable Timer scan */
		REG_WR(sc, TM_REG_EN_LINEAR0_TIMER + port * 4, 0);
/*
 * Wait for at least 10ms and up to 2 second for the timers
 * scan to complete
 */
		for (i = 0; i < 200; i++) {
			DELAY(10000);
			if (!REG_RD(sc, TM_REG_LIN0_SCAN_ON + port * 4))
				break;
		}
	}

	/* Clear ILT */
	bnx2x_clear_func_ilt(sc, func);

	/*
	 * Timers workaround bug for E2: if this is vnic-3,
	 * we need to set the entire ilt range for this timers.
	 */
	if (!CHIP_IS_E1x(sc) && SC_VN(sc) == 3) {
		struct ilt_client_info ilt_cli;
/* use dummy TM client */
		memset(&ilt_cli, 0, sizeof(struct ilt_client_info));
		ilt_cli.start = 0;
		ilt_cli.end = ILT_NUM_PAGE_ENTRIES - 1;
		ilt_cli.client_num = ILT_CLIENT_TM;

		ecore_ilt_boundary_init_op(sc, &ilt_cli, 0, INITOP_CLEAR);
	}

	/* this assumes that reset_port() called before reset_func() */
	if (!CHIP_IS_E1x(sc)) {
		bnx2x_pf_disable(sc);
	}

	sc->dmae_ready = 0;
}

static void bnx2x_release_firmware(struct bnx2x_softc *sc)
{
	rte_free(sc->init_ops);
	rte_free(sc->init_ops_offsets);
	rte_free(sc->init_data);
	rte_free(sc->iro_array);
}

static int bnx2x_init_firmware(struct bnx2x_softc *sc)
{
	uint32_t len, i;
	uint8_t *p = sc->firmware;
	uint32_t off[24];

	for (i = 0; i < 24; ++i)
		off[i] = rte_be_to_cpu_32(*((uint32_t *) sc->firmware + i));

	len = off[0];
	sc->init_ops = rte_zmalloc("", len, RTE_CACHE_LINE_SIZE);
	if (!sc->init_ops)
		goto alloc_failed;
	bnx2x_data_to_init_ops(p + off[1], sc->init_ops, len);

	len = off[2];
	sc->init_ops_offsets = rte_zmalloc("", len, RTE_CACHE_LINE_SIZE);
	if (!sc->init_ops_offsets)
		goto alloc_failed;
	bnx2x_data_to_init_offsets(p + off[3], sc->init_ops_offsets, len);

	len = off[4];
	sc->init_data = rte_zmalloc("", len, RTE_CACHE_LINE_SIZE);
	if (!sc->init_data)
		goto alloc_failed;
	bnx2x_data_to_init_data(p + off[5], sc->init_data, len);

	sc->tsem_int_table_data = p + off[7];
	sc->tsem_pram_data = p + off[9];
	sc->usem_int_table_data = p + off[11];
	sc->usem_pram_data = p + off[13];
	sc->csem_int_table_data = p + off[15];
	sc->csem_pram_data = p + off[17];
	sc->xsem_int_table_data = p + off[19];
	sc->xsem_pram_data = p + off[21];

	len = off[22];
	sc->iro_array = rte_zmalloc("", len, RTE_CACHE_LINE_SIZE);
	if (!sc->iro_array)
		goto alloc_failed;
	bnx2x_data_to_iro_array(p + off[23], sc->iro_array, len);

	return 0;

alloc_failed:
	bnx2x_release_firmware(sc);
	return -1;
}

static int cut_gzip_prefix(const uint8_t * zbuf, int len)
{
#define MIN_PREFIX_SIZE (10)

	int n = MIN_PREFIX_SIZE;
	uint16_t xlen;

	if (!(zbuf[0] == 0x1f && zbuf[1] == 0x8b && zbuf[2] == Z_DEFLATED) ||
	    len <= MIN_PREFIX_SIZE) {
		return -1;
	}

	/* optional extra fields are present */
	if (zbuf[3] & 0x4) {
		xlen = zbuf[13];
		xlen <<= 8;
		xlen += zbuf[12];

		n += xlen;
	}
	/* file name is present */
	if (zbuf[3] & 0x8) {
		while ((zbuf[n++] != 0) && (n < len)) ;
	}

	return n;
}

static int ecore_gunzip(struct bnx2x_softc *sc, const uint8_t * zbuf, int len)
{
	int ret;
	int data_begin = cut_gzip_prefix(zbuf, len);

	PMD_DRV_LOG(DEBUG, sc, "ecore_gunzip %d", len);

	if (data_begin <= 0) {
		PMD_DRV_LOG(NOTICE, sc, "bad gzip prefix");
		return -1;
	}

	memset(&zlib_stream, 0, sizeof(zlib_stream));
	zlib_stream.next_in = zbuf + data_begin;
	zlib_stream.avail_in = len - data_begin;
	zlib_stream.next_out = sc->gz_buf;
	zlib_stream.avail_out = FW_BUF_SIZE;

	ret = inflateInit2(&zlib_stream, -MAX_WBITS);
	if (ret != Z_OK) {
		PMD_DRV_LOG(NOTICE, sc, "zlib inflateInit2 error");
		return ret;
	}

	ret = inflate(&zlib_stream, Z_FINISH);
	if ((ret != Z_STREAM_END) && (ret != Z_OK)) {
		PMD_DRV_LOG(NOTICE, sc, "zlib inflate error: %d %s", ret,
			    zlib_stream.msg);
	}

	sc->gz_outlen = zlib_stream.total_out;
	if (sc->gz_outlen & 0x3) {
		PMD_DRV_LOG(NOTICE, sc, "firmware is not aligned. gz_outlen == %d",
			    sc->gz_outlen);
	}
	sc->gz_outlen >>= 2;

	inflateEnd(&zlib_stream);

	if (ret == Z_STREAM_END)
		return 0;

	return ret;
}

static void
ecore_write_dmae_phys_len(struct bnx2x_softc *sc, rte_iova_t phys_addr,
			  uint32_t addr, uint32_t len)
{
	bnx2x_write_dmae_phys_len(sc, phys_addr, addr, len);
}

void
ecore_storm_memset_struct(struct bnx2x_softc *sc, uint32_t addr, size_t size,
			  uint32_t * data)
{
	uint8_t i;
	for (i = 0; i < size / 4; i++) {
		REG_WR(sc, addr + (i * 4), data[i]);
	}
}

static const char *get_ext_phy_type(uint32_t ext_phy_type)
{
	uint32_t phy_type_idx = ext_phy_type >> 8;
	static const char *types[] =
	    { "DIRECT", "BNX2X-8071", "BNX2X-8072", "BNX2X-8073",
		"BNX2X-8705", "BNX2X-8706", "BNX2X-8726", "BNX2X-8481", "SFX-7101",
		"BNX2X-8727",
		"BNX2X-8727-NOC", "BNX2X-84823", "NOT_CONN", "FAILURE"
	};

	if (phy_type_idx < 12)
		return types[phy_type_idx];
	else if (PORT_HW_CFG_XGXS_EXT_PHY_TYPE_NOT_CONN == ext_phy_type)
		return types[12];
	else
		return types[13];
}

static const char *get_state(uint32_t state)
{
	uint32_t state_idx = state >> 12;
	static const char *states[] = { "CLOSED", "OPENING_WAIT4_LOAD",
		"OPENING_WAIT4_PORT", "OPEN", "CLOSING_WAIT4_HALT",
		"CLOSING_WAIT4_DELETE", "CLOSING_WAIT4_UNLOAD",
		"UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN",
		"UNKNOWN", "DISABLED", "DIAG", "ERROR", "UNDEFINED"
	};

	if (state_idx <= 0xF)
		return states[state_idx];
	else
		return states[0x10];
}

static const char *get_recovery_state(uint32_t state)
{
	static const char *states[] = { "NONE", "DONE", "INIT",
		"WAIT", "FAILED", "NIC_LOADING"
	};
	return states[state];
}

static const char *get_rx_mode(uint32_t mode)
{
	static const char *modes[] = { "NONE", "NORMAL", "ALLMULTI",
		"PROMISC", "MAX_MULTICAST", "ERROR"
	};

	if (mode < 0x4)
		return modes[mode];
	else if (BNX2X_MAX_MULTICAST == mode)
		return modes[4];
	else
		return modes[5];
}

#define BNX2X_INFO_STR_MAX 256
static const char *get_bnx2x_flags(uint32_t flags)
{
	int i;
	static const char *flag[] = { "ONE_PORT ", "NO_ISCSI ",
		"NO_FCOE ", "NO_WOL ", "USING_DAC ", "USING_MSIX ",
		"USING_MSI ", "DISABLE_MSI ", "UNKNOWN ", "NO_MCP ",
		"SAFC_TX_FLAG ", "MF_FUNC_DIS ", "TX_SWITCHING "
	};
	static char flag_str[BNX2X_INFO_STR_MAX];
	memset(flag_str, 0, BNX2X_INFO_STR_MAX);

	for (i = 0; i < 5; i++)
		if (flags & (1 << i)) {
			strlcat(flag_str, flag[i], sizeof(flag_str));
			flags ^= (1 << i);
		}
	if (flags) {
		static char unknown[BNX2X_INFO_STR_MAX];
		snprintf(unknown, 32, "Unknown flag mask %x", flags);
		strlcat(flag_str, unknown, sizeof(flag_str));
	}
	return flag_str;
}

/* Prints useful adapter info. */
void bnx2x_print_adapter_info(struct bnx2x_softc *sc)
{
	int i = 0;

	PMD_DRV_LOG(INFO, sc, "========================================");
	/* DPDK and Driver versions */
	PMD_DRV_LOG(INFO, sc, "%12s : %s", "DPDK",
			rte_version());
	PMD_DRV_LOG(INFO, sc, "%12s : %s", "Driver",
			bnx2x_pmd_version());
	/* Firmware versions. */
	PMD_DRV_LOG(INFO, sc, "%12s : %d.%d.%d",
		     "Firmware",
		     BNX2X_5710_FW_MAJOR_VERSION,
		     BNX2X_5710_FW_MINOR_VERSION,
		     BNX2X_5710_FW_REVISION_VERSION);
	PMD_DRV_LOG(INFO, sc, "%12s : %s",
		     "Bootcode", sc->devinfo.bc_ver_str);
	/* Hardware chip info. */
	PMD_DRV_LOG(INFO, sc, "%12s : %#08x", "ASIC", sc->devinfo.chip_id);
	PMD_DRV_LOG(INFO, sc, "%12s : %c%d", "Rev", (CHIP_REV(sc) >> 12) + 'A',
		     (CHIP_METAL(sc) >> 4));
	/* Bus PCIe info. */
	PMD_DRV_LOG(INFO, sc, "%12s : 0x%x", "Vendor Id",
		    sc->devinfo.vendor_id);
	PMD_DRV_LOG(INFO, sc, "%12s : 0x%x", "Device Id",
		    sc->devinfo.device_id);
	PMD_DRV_LOG(INFO, sc, "%12s : width x%d, ", "Bus PCIe",
		    sc->devinfo.pcie_link_width);
	switch (sc->devinfo.pcie_link_speed) {
	case 1:
		PMD_DRV_LOG(INFO, sc, "%23s", "2.5 Gbps");
		break;
	case 2:
		PMD_DRV_LOG(INFO, sc, "%21s", "5 Gbps");
		break;
	case 4:
		PMD_DRV_LOG(INFO, sc, "%21s", "8 Gbps");
		break;
	default:
		PMD_DRV_LOG(INFO, sc, "%33s", "Unknown link speed");
	}
	/* Device features. */
	PMD_DRV_LOG(INFO, sc, "%12s : ", "Flags");
	/* Miscellaneous flags. */
	if (sc->devinfo.pcie_cap_flags & BNX2X_MSI_CAPABLE_FLAG) {
		PMD_DRV_LOG(INFO, sc, "%18s", "MSI");
		i++;
	}
	if (sc->devinfo.pcie_cap_flags & BNX2X_MSIX_CAPABLE_FLAG) {
		if (i > 0)
			PMD_DRV_LOG(INFO, sc, "|");
		PMD_DRV_LOG(INFO, sc, "%20s", "MSI-X");
		i++;
	}
	PMD_DRV_LOG(INFO, sc, "%12s : %s", "OVLAN", (OVLAN(sc) ? "YES" : "NO"));
	PMD_DRV_LOG(INFO, sc, "%12s : %s", "MF", (IS_MF(sc) ? "YES" : "NO"));
	PMD_DRV_LOG(INFO, sc, "========================================");
}

/* Prints useful device info. */
void bnx2x_print_device_info(struct bnx2x_softc *sc)
{
	__rte_unused uint32_t ext_phy_type;
	uint32_t offset, reg_val;

	PMD_INIT_FUNC_TRACE(sc);
	offset = offsetof(struct shmem_region,
			  dev_info.port_hw_config[0].external_phy_config);
	reg_val = REG_RD(sc, sc->devinfo.shmem_base + offset);
	if (sc->link_vars.phy_flags & PHY_XGXS_FLAG)
		ext_phy_type = ELINK_XGXS_EXT_PHY_TYPE(reg_val);
	else
		ext_phy_type = ELINK_SERDES_EXT_PHY_TYPE(reg_val);

	/* Device features. */
	PMD_DRV_LOG(INFO, sc, "%12s : %u", "Bnx2x Func", sc->pcie_func);
	PMD_DRV_LOG(INFO, sc,
		    "%12s : %s", "Bnx2x Flags", get_bnx2x_flags(sc->flags));
	PMD_DRV_LOG(INFO, sc, "%12s : %s", "DMAE Is",
		     (sc->dmae_ready ? "Ready" : "Not Ready"));
	PMD_DRV_LOG(INFO, sc, "%12s : %u", "MTU", sc->mtu);
	PMD_DRV_LOG(INFO, sc,
		    "%12s : %s", "PHY Type", get_ext_phy_type(ext_phy_type));
	PMD_DRV_LOG(INFO, sc, "%12s : " RTE_ETHER_ADDR_PRT_FMT, "MAC Addr",
			sc->link_params.mac_addr[0],
			sc->link_params.mac_addr[1],
			sc->link_params.mac_addr[2],
			sc->link_params.mac_addr[3],
			sc->link_params.mac_addr[4],
			sc->link_params.mac_addr[5]);
	PMD_DRV_LOG(INFO, sc, "%12s : %s", "RX Mode", get_rx_mode(sc->rx_mode));
	PMD_DRV_LOG(INFO, sc, "%12s : %s", "State", get_state(sc->state));
	if (sc->recovery_state)
		PMD_DRV_LOG(INFO, sc, "%12s : %s", "Recovery",
			     get_recovery_state(sc->recovery_state));
	/* Queue info. */
	if (IS_PF(sc)) {
		switch (sc->sp->rss_rdata.rss_mode) {
		case ETH_RSS_MODE_DISABLED:
			PMD_DRV_LOG(INFO, sc, "%12s : %s", "Queues", "RSS mode - None");
			break;
		case ETH_RSS_MODE_REGULAR:
			PMD_DRV_LOG(INFO, sc, "%12s : %s,", "Queues", "RSS mode - Regular");
			PMD_DRV_LOG(INFO, sc, "%16d", sc->num_queues);
			break;
		default:
			PMD_DRV_LOG(INFO, sc, "%12s : %s", "Queues", "RSS mode - Unknown");
			break;
		}
	}
	PMD_DRV_LOG(INFO, sc, "%12s : CQ = %lx,  EQ = %lx", "SPQ Left",
		     sc->cq_spq_left, sc->eq_spq_left);

	PMD_DRV_LOG(INFO, sc,
		    "%12s : %x", "Switch", sc->link_params.switch_cfg);
	PMD_DRV_LOG(INFO, sc, "pcie_bus=%d, pcie_device=%d",
			sc->pcie_bus, sc->pcie_device);
	PMD_DRV_LOG(INFO, sc, "bar0.addr=%p, bar1.addr=%p",
			sc->bar[BAR0].base_addr, sc->bar[BAR1].base_addr);
	PMD_DRV_LOG(INFO, sc, "port=%d, path=%d, vnic=%d, func=%d",
			PORT_ID(sc), PATH_ID(sc), VNIC_ID(sc), FUNC_ID(sc));
}
