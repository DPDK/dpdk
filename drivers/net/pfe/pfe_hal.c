/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#include <arpa/inet.h>

#include "pfe_logs.h"
#include "pfe_mod.h"

#define PFE_MTU_RESET_MASK	0xC000FFFF

void *cbus_base_addr;
void *ddr_base_addr;
unsigned long ddr_phys_base_addr;
unsigned int ddr_size;
static struct pe_info pe[MAX_PE];

/* Initializes the PFE library.
 * Must be called before using any of the library functions.
 *
 * @param[in] cbus_base		CBUS virtual base address (as mapped in
 * the host CPU address space)
 * @param[in] ddr_base		PFE DDR range virtual base address (as
 * mapped in the host CPU address space)
 * @param[in] ddr_phys_base	PFE DDR range physical base address (as
 * mapped in platform)
 * @param[in] size		PFE DDR range size (as defined by the host
 * software)
 */
void
pfe_lib_init(void *cbus_base, void *ddr_base, unsigned long ddr_phys_base,
		  unsigned int size)
{
	cbus_base_addr = cbus_base;
	ddr_base_addr = ddr_base;
	ddr_phys_base_addr = ddr_phys_base;
	ddr_size = size;

	pe[CLASS0_ID].dmem_base_addr = CLASS_DMEM_BASE_ADDR(0);
	pe[CLASS0_ID].pmem_base_addr = CLASS_IMEM_BASE_ADDR(0);
	pe[CLASS0_ID].pmem_size = CLASS_IMEM_SIZE;
	pe[CLASS0_ID].mem_access_wdata = CLASS_MEM_ACCESS_WDATA;
	pe[CLASS0_ID].mem_access_addr = CLASS_MEM_ACCESS_ADDR;
	pe[CLASS0_ID].mem_access_rdata = CLASS_MEM_ACCESS_RDATA;

	pe[CLASS1_ID].dmem_base_addr = CLASS_DMEM_BASE_ADDR(1);
	pe[CLASS1_ID].pmem_base_addr = CLASS_IMEM_BASE_ADDR(1);
	pe[CLASS1_ID].pmem_size = CLASS_IMEM_SIZE;
	pe[CLASS1_ID].mem_access_wdata = CLASS_MEM_ACCESS_WDATA;
	pe[CLASS1_ID].mem_access_addr = CLASS_MEM_ACCESS_ADDR;
	pe[CLASS1_ID].mem_access_rdata = CLASS_MEM_ACCESS_RDATA;

	pe[CLASS2_ID].dmem_base_addr = CLASS_DMEM_BASE_ADDR(2);
	pe[CLASS2_ID].pmem_base_addr = CLASS_IMEM_BASE_ADDR(2);
	pe[CLASS2_ID].pmem_size = CLASS_IMEM_SIZE;
	pe[CLASS2_ID].mem_access_wdata = CLASS_MEM_ACCESS_WDATA;
	pe[CLASS2_ID].mem_access_addr = CLASS_MEM_ACCESS_ADDR;
	pe[CLASS2_ID].mem_access_rdata = CLASS_MEM_ACCESS_RDATA;

	pe[CLASS3_ID].dmem_base_addr = CLASS_DMEM_BASE_ADDR(3);
	pe[CLASS3_ID].pmem_base_addr = CLASS_IMEM_BASE_ADDR(3);
	pe[CLASS3_ID].pmem_size = CLASS_IMEM_SIZE;
	pe[CLASS3_ID].mem_access_wdata = CLASS_MEM_ACCESS_WDATA;
	pe[CLASS3_ID].mem_access_addr = CLASS_MEM_ACCESS_ADDR;
	pe[CLASS3_ID].mem_access_rdata = CLASS_MEM_ACCESS_RDATA;

	pe[CLASS4_ID].dmem_base_addr = CLASS_DMEM_BASE_ADDR(4);
	pe[CLASS4_ID].pmem_base_addr = CLASS_IMEM_BASE_ADDR(4);
	pe[CLASS4_ID].pmem_size = CLASS_IMEM_SIZE;
	pe[CLASS4_ID].mem_access_wdata = CLASS_MEM_ACCESS_WDATA;
	pe[CLASS4_ID].mem_access_addr = CLASS_MEM_ACCESS_ADDR;
	pe[CLASS4_ID].mem_access_rdata = CLASS_MEM_ACCESS_RDATA;

	pe[CLASS5_ID].dmem_base_addr = CLASS_DMEM_BASE_ADDR(5);
	pe[CLASS5_ID].pmem_base_addr = CLASS_IMEM_BASE_ADDR(5);
	pe[CLASS5_ID].pmem_size = CLASS_IMEM_SIZE;
	pe[CLASS5_ID].mem_access_wdata = CLASS_MEM_ACCESS_WDATA;
	pe[CLASS5_ID].mem_access_addr = CLASS_MEM_ACCESS_ADDR;
	pe[CLASS5_ID].mem_access_rdata = CLASS_MEM_ACCESS_RDATA;

	pe[TMU0_ID].dmem_base_addr = TMU_DMEM_BASE_ADDR(0);
	pe[TMU0_ID].pmem_base_addr = TMU_IMEM_BASE_ADDR(0);
	pe[TMU0_ID].pmem_size = TMU_IMEM_SIZE;
	pe[TMU0_ID].mem_access_wdata = TMU_MEM_ACCESS_WDATA;
	pe[TMU0_ID].mem_access_addr = TMU_MEM_ACCESS_ADDR;
	pe[TMU0_ID].mem_access_rdata = TMU_MEM_ACCESS_RDATA;

	pe[TMU1_ID].dmem_base_addr = TMU_DMEM_BASE_ADDR(1);
	pe[TMU1_ID].pmem_base_addr = TMU_IMEM_BASE_ADDR(1);
	pe[TMU1_ID].pmem_size = TMU_IMEM_SIZE;
	pe[TMU1_ID].mem_access_wdata = TMU_MEM_ACCESS_WDATA;
	pe[TMU1_ID].mem_access_addr = TMU_MEM_ACCESS_ADDR;
	pe[TMU1_ID].mem_access_rdata = TMU_MEM_ACCESS_RDATA;

	pe[TMU3_ID].dmem_base_addr = TMU_DMEM_BASE_ADDR(3);
	pe[TMU3_ID].pmem_base_addr = TMU_IMEM_BASE_ADDR(3);
	pe[TMU3_ID].pmem_size = TMU_IMEM_SIZE;
	pe[TMU3_ID].mem_access_wdata = TMU_MEM_ACCESS_WDATA;
	pe[TMU3_ID].mem_access_addr = TMU_MEM_ACCESS_ADDR;
	pe[TMU3_ID].mem_access_rdata = TMU_MEM_ACCESS_RDATA;

#if !defined(CONFIG_FSL_PFE_UTIL_DISABLED)
	pe[UTIL_ID].dmem_base_addr = UTIL_DMEM_BASE_ADDR;
	pe[UTIL_ID].mem_access_wdata = UTIL_MEM_ACCESS_WDATA;
	pe[UTIL_ID].mem_access_addr = UTIL_MEM_ACCESS_ADDR;
	pe[UTIL_ID].mem_access_rdata = UTIL_MEM_ACCESS_RDATA;
#endif
}

/**************************** MTIP GEMAC ***************************/

/* Enable Rx Checksum Engine. With this enabled, Frame with bad IP,
 *   TCP or UDP checksums are discarded
 *
 * @param[in] base	GEMAC base address.
 */
void
gemac_enable_rx_checksum_offload(__rte_unused void *base)
{
	/*Do not find configuration to do this */
}

/* Disable Rx Checksum Engine.
 *
 * @param[in] base	GEMAC base address.
 */
void
gemac_disable_rx_checksum_offload(__rte_unused void *base)
{
	/*Do not find configuration to do this */
}

/* GEMAC set speed.
 * @param[in] base	GEMAC base address
 * @param[in] speed	GEMAC speed (10, 100 or 1000 Mbps)
 */
void
gemac_set_speed(void *base, enum mac_speed gem_speed)
{
	u32 ecr = readl(base + EMAC_ECNTRL_REG) & ~EMAC_ECNTRL_SPEED;
	u32 rcr = readl(base + EMAC_RCNTRL_REG) & ~EMAC_RCNTRL_RMII_10T;

	switch (gem_speed) {
	case SPEED_10M:
			rcr |= EMAC_RCNTRL_RMII_10T;
			break;

	case SPEED_1000M:
			ecr |= EMAC_ECNTRL_SPEED;
			break;

	case SPEED_100M:
	default:
			/*It is in 100M mode */
			break;
	}
	writel(ecr, (base + EMAC_ECNTRL_REG));
	writel(rcr, (base + EMAC_RCNTRL_REG));
}

/* GEMAC set duplex.
 * @param[in] base	GEMAC base address
 * @param[in] duplex	GEMAC duplex mode (Full, Half)
 */
void
gemac_set_duplex(void *base, int duplex)
{
	if (duplex == DUPLEX_HALF) {
		writel(readl(base + EMAC_TCNTRL_REG) & ~EMAC_TCNTRL_FDEN, base
			+ EMAC_TCNTRL_REG);
		writel(readl(base + EMAC_RCNTRL_REG) | EMAC_RCNTRL_DRT, (base
			+ EMAC_RCNTRL_REG));
	} else {
		writel(readl(base + EMAC_TCNTRL_REG) | EMAC_TCNTRL_FDEN, base
			+ EMAC_TCNTRL_REG);
		writel(readl(base + EMAC_RCNTRL_REG) & ~EMAC_RCNTRL_DRT, (base
			+ EMAC_RCNTRL_REG));
	}
}

/* GEMAC set mode.
 * @param[in] base	GEMAC base address
 * @param[in] mode	GEMAC operation mode (MII, RMII, RGMII, SGMII)
 */
void
gemac_set_mode(void *base, __rte_unused int mode)
{
	u32 val = readl(base + EMAC_RCNTRL_REG);

	/* Remove loopback */
	val &= ~EMAC_RCNTRL_LOOP;

	/*Enable flow control and MII mode*/
	val |= (EMAC_RCNTRL_FCE | EMAC_RCNTRL_MII_MODE | EMAC_RCNTRL_CRC_FWD);

	writel(val, base + EMAC_RCNTRL_REG);
}

/* GEMAC enable function.
 * @param[in] base	GEMAC base address
 */
void
gemac_enable(void *base)
{
	writel(readl(base + EMAC_ECNTRL_REG) | EMAC_ECNTRL_ETHER_EN, base +
		EMAC_ECNTRL_REG);
}

/* GEMAC disable function.
 * @param[in] base	GEMAC base address
 */
void
gemac_disable(void *base)
{
	writel(readl(base + EMAC_ECNTRL_REG) & ~EMAC_ECNTRL_ETHER_EN, base +
		EMAC_ECNTRL_REG);
}

/* GEMAC TX disable function.
 * @param[in] base	GEMAC base address
 */
void
gemac_tx_disable(void *base)
{
	writel(readl(base + EMAC_TCNTRL_REG) | EMAC_TCNTRL_GTS, base +
		EMAC_TCNTRL_REG);
}

void
gemac_tx_enable(void *base)
{
	writel(readl(base + EMAC_TCNTRL_REG) & ~EMAC_TCNTRL_GTS, base +
			EMAC_TCNTRL_REG);
}

/* Sets the hash register of the MAC.
 * This register is used for matching unicast and multicast frames.
 *
 * @param[in] base	GEMAC base address.
 * @param[in] hash	64-bit hash to be configured.
 */
void
gemac_set_hash(void *base, struct pfe_mac_addr *hash)
{
	writel(hash->bottom,  base + EMAC_GALR);
	writel(hash->top, base + EMAC_GAUR);
}

void
gemac_set_laddrN(void *base, struct pfe_mac_addr *address,
		      unsigned int entry_index)
{
	if (entry_index < 1 || entry_index > EMAC_SPEC_ADDR_MAX)
		return;

	entry_index = entry_index - 1;
	if (entry_index < 1) {
		writel(htonl(address->bottom),  base + EMAC_PHY_ADDR_LOW);
		writel((htonl(address->top) | 0x8808), base +
			EMAC_PHY_ADDR_HIGH);
	} else {
		writel(htonl(address->bottom),  base + ((entry_index - 1) * 8)
			+ EMAC_SMAC_0_0);
		writel((htonl(address->top) | 0x8808), base + ((entry_index -
			1) * 8) + EMAC_SMAC_0_1);
	}
}

void
gemac_clear_laddrN(void *base, unsigned int entry_index)
{
	if (entry_index < 1 || entry_index > EMAC_SPEC_ADDR_MAX)
		return;

	entry_index = entry_index - 1;
	if (entry_index < 1) {
		writel(0, base + EMAC_PHY_ADDR_LOW);
		writel(0, base + EMAC_PHY_ADDR_HIGH);
	} else {
		writel(0,  base + ((entry_index - 1) * 8) + EMAC_SMAC_0_0);
		writel(0, base + ((entry_index - 1) * 8) + EMAC_SMAC_0_1);
	}
}

/* Set the loopback mode of the MAC.  This can be either no loopback for
 * normal operation, local loopback through MAC internal loopback module or PHY
 *   loopback for external loopback through a PHY.  This asserts the external
 * loop pin.
 *
 * @param[in] base	GEMAC base address.
 * @param[in] gem_loop	Loopback mode to be enabled. LB_LOCAL - MAC
 * Loopback,
 *			LB_EXT - PHY Loopback.
 */
void
gemac_set_loop(void *base, __rte_unused enum mac_loop gem_loop)
{
	pr_info("%s()\n", __func__);
	writel(readl(base + EMAC_RCNTRL_REG) | EMAC_RCNTRL_LOOP, (base +
		EMAC_RCNTRL_REG));
}

/* GEMAC allow frames
 * @param[in] base	GEMAC base address
 */
void
gemac_enable_copy_all(void *base)
{
	writel(readl(base + EMAC_RCNTRL_REG) | EMAC_RCNTRL_PROM, (base +
		EMAC_RCNTRL_REG));
}

/* GEMAC do not allow frames
 * @param[in] base	GEMAC base address
 */
void
gemac_disable_copy_all(void *base)
{
	writel(readl(base + EMAC_RCNTRL_REG) & ~EMAC_RCNTRL_PROM, (base +
		EMAC_RCNTRL_REG));
}

/* GEMAC allow broadcast function.
 * @param[in] base	GEMAC base address
 */
void
gemac_allow_broadcast(void *base)
{
	writel(readl(base + EMAC_RCNTRL_REG) & ~EMAC_RCNTRL_BC_REJ, base +
		EMAC_RCNTRL_REG);
}

/* GEMAC no broadcast function.
 * @param[in] base	GEMAC base address
 */
void
gemac_no_broadcast(void *base)
{
	writel(readl(base + EMAC_RCNTRL_REG) | EMAC_RCNTRL_BC_REJ, base +
		EMAC_RCNTRL_REG);
}

/* GEMAC enable 1536 rx function.
 * @param[in]	base	GEMAC base address
 */
void
gemac_enable_1536_rx(void *base)
{
	/* Set 1536 as Maximum frame length */
	writel((readl(base + EMAC_RCNTRL_REG) & PFE_MTU_RESET_MASK)
			| (1536 << 16),
			base + EMAC_RCNTRL_REG);
}

/* GEMAC set Max rx function.
 * @param[in]	base	GEMAC base address
 */
int
gemac_set_rx(void *base, int mtu)
{
	if (mtu < HIF_RX_PKT_MIN_SIZE || mtu > JUMBO_FRAME_SIZE) {
		PFE_PMD_ERR("Invalid or not support MTU size");
		return -1;
	}

	if (pfe_svr == SVR_LS1012A_REV1 &&
	    mtu > (MAX_MTU_ON_REV1 + PFE_ETH_OVERHEAD)) {
		PFE_PMD_ERR("Max supported MTU on Rev1 is %d", MAX_MTU_ON_REV1);
		return -1;
	}

	writel((readl(base + EMAC_RCNTRL_REG) & PFE_MTU_RESET_MASK)
			| (mtu << 16),
			base + EMAC_RCNTRL_REG);
	return 0;
}

/* GEMAC enable jumbo function.
 * @param[in]	base	GEMAC base address
 */
void
gemac_enable_rx_jmb(void *base)
{
	if (pfe_svr == SVR_LS1012A_REV1) {
		PFE_PMD_ERR("Jumbo not supported on Rev1");
		return;
	}

	writel((readl(base + EMAC_RCNTRL_REG) & PFE_MTU_RESET_MASK) |
			(JUMBO_FRAME_SIZE << 16), base + EMAC_RCNTRL_REG);
}

/* GEMAC enable stacked vlan function.
 * @param[in]	base	GEMAC base address
 */
void
gemac_enable_stacked_vlan(__rte_unused void *base)
{
	/* MTIP doesn't support stacked vlan */
}

/* GEMAC enable pause rx function.
 * @param[in] base	GEMAC base address
 */
void
gemac_enable_pause_rx(void *base)
{
	writel(readl(base + EMAC_RCNTRL_REG) | EMAC_RCNTRL_FCE,
	       base + EMAC_RCNTRL_REG);
}

/* GEMAC disable pause rx function.
 * @param[in] base	GEMAC base address
 */
void
gemac_disable_pause_rx(void *base)
{
	writel(readl(base + EMAC_RCNTRL_REG) & ~EMAC_RCNTRL_FCE,
	       base + EMAC_RCNTRL_REG);
}

/* GEMAC enable pause tx function.
 * @param[in] base GEMAC base address
 */
void
gemac_enable_pause_tx(void *base)
{
	writel(EMAC_RX_SECTION_EMPTY_V, base + EMAC_RX_SECTION_EMPTY);
}

/* GEMAC disable pause tx function.
 * @param[in] base GEMAC base address
 */
void
gemac_disable_pause_tx(void *base)
{
	writel(0x0, base + EMAC_RX_SECTION_EMPTY);
}

/* GEMAC wol configuration
 * @param[in] base	GEMAC base address
 * @param[in] wol_conf	WoL register configuration
 */
void
gemac_set_wol(void *base, u32 wol_conf)
{
	u32  val = readl(base + EMAC_ECNTRL_REG);

	if (wol_conf)
		val |= (EMAC_ECNTRL_MAGIC_ENA | EMAC_ECNTRL_SLEEP);
	else
		val &= ~(EMAC_ECNTRL_MAGIC_ENA | EMAC_ECNTRL_SLEEP);
	writel(val, base + EMAC_ECNTRL_REG);
}

/* Sets Gemac bus width to 64bit
 * @param[in] base       GEMAC base address
 * @param[in] width     gemac bus width to be set possible values are 32/64/128
 */
void
gemac_set_bus_width(__rte_unused void *base, __rte_unused int width)
{
}

/* Sets Gemac configuration.
 * @param[in] base	GEMAC base address
 * @param[in] cfg	GEMAC configuration
 */
void
gemac_set_config(void *base, struct gemac_cfg *cfg)
{
	/*GEMAC config taken from VLSI */
	writel(0x00000004, base + EMAC_TFWR_STR_FWD);
	writel(0x00000005, base + EMAC_RX_SECTION_FULL);

	if (pfe_svr == SVR_LS1012A_REV1)
		writel(0x00000768, base + EMAC_TRUNC_FL);
	else
		writel(0x00003fff, base + EMAC_TRUNC_FL);

	writel(0x00000030, base + EMAC_TX_SECTION_EMPTY);
	writel(0x00000000, base + EMAC_MIB_CTRL_STS_REG);

	gemac_set_mode(base, cfg->mode);

	gemac_set_speed(base, cfg->speed);

	gemac_set_duplex(base, cfg->duplex);
}

/**************************** GPI ***************************/

/* Initializes a GPI block.
 * @param[in] base	GPI base address
 * @param[in] cfg	GPI configuration
 */
void
gpi_init(void *base, struct gpi_cfg *cfg)
{
	gpi_reset(base);

	gpi_disable(base);

	gpi_set_config(base, cfg);
}

/* Resets a GPI block.
 * @param[in] base	GPI base address
 */
void
gpi_reset(void *base)
{
	writel(CORE_SW_RESET, base + GPI_CTRL);
}

/* Enables a GPI block.
 * @param[in] base	GPI base address
 */
void
gpi_enable(void *base)
{
	writel(CORE_ENABLE, base + GPI_CTRL);
}

/* Disables a GPI block.
 * @param[in] base	GPI base address
 */
void
gpi_disable(void *base)
{
	writel(CORE_DISABLE, base + GPI_CTRL);
}

/* Sets the configuration of a GPI block.
 * @param[in] base	GPI base address
 * @param[in] cfg	GPI configuration
 */
void
gpi_set_config(void *base, struct gpi_cfg *cfg)
{
	writel(CBUS_VIRT_TO_PFE(BMU1_BASE_ADDR + BMU_ALLOC_CTRL),	base
		+ GPI_LMEM_ALLOC_ADDR);
	writel(CBUS_VIRT_TO_PFE(BMU1_BASE_ADDR + BMU_FREE_CTRL),	base
		+ GPI_LMEM_FREE_ADDR);
	writel(CBUS_VIRT_TO_PFE(BMU2_BASE_ADDR + BMU_ALLOC_CTRL),	base
		+ GPI_DDR_ALLOC_ADDR);
	writel(CBUS_VIRT_TO_PFE(BMU2_BASE_ADDR + BMU_FREE_CTRL),	base
		+ GPI_DDR_FREE_ADDR);
	writel(CBUS_VIRT_TO_PFE(CLASS_INQ_PKTPTR), base + GPI_CLASS_ADDR);
	writel(DDR_HDR_SIZE, base + GPI_DDR_DATA_OFFSET);
	writel(LMEM_HDR_SIZE, base + GPI_LMEM_DATA_OFFSET);
	writel(0, base + GPI_LMEM_SEC_BUF_DATA_OFFSET);
	writel(0, base + GPI_DDR_SEC_BUF_DATA_OFFSET);
	writel((DDR_HDR_SIZE << 16) |	LMEM_HDR_SIZE,	base + GPI_HDR_SIZE);
	writel((DDR_BUF_SIZE << 16) |	LMEM_BUF_SIZE,	base + GPI_BUF_SIZE);

	writel(((cfg->lmem_rtry_cnt << 16) | (GPI_DDR_BUF_EN << 1) |
		GPI_LMEM_BUF_EN), base + GPI_RX_CONFIG);
	writel(cfg->tmlf_txthres, base + GPI_TMLF_TX);
	writel(cfg->aseq_len,	base + GPI_DTX_ASEQ);
	writel(1, base + GPI_TOE_CHKSUM_EN);

	if (cfg->mtip_pause_reg) {
		writel(cfg->mtip_pause_reg, base + GPI_CSR_MTIP_PAUSE_REG);
		writel(EGPI_PAUSE_TIME, base + GPI_TX_PAUSE_TIME);
	}
}

/**************************** HIF ***************************/
/* Initializes HIF copy block.
 *
 */
void
hif_init(void)
{
	/*Initialize HIF registers*/
	writel((HIF_RX_POLL_CTRL_CYCLE << 16) | HIF_TX_POLL_CTRL_CYCLE,
	       HIF_POLL_CTRL);
}

/* Enable hif tx DMA and interrupt
 *
 */
void
hif_tx_enable(void)
{
	writel(HIF_CTRL_DMA_EN, HIF_TX_CTRL);
	writel((readl(HIF_INT_ENABLE) | HIF_INT_EN | HIF_TXPKT_INT_EN),
	       HIF_INT_ENABLE);
}

/* Disable hif tx DMA and interrupt
 *
 */
void
hif_tx_disable(void)
{
	u32	hif_int;

	writel(0, HIF_TX_CTRL);

	hif_int = readl(HIF_INT_ENABLE);
	hif_int &= HIF_TXPKT_INT_EN;
	writel(hif_int, HIF_INT_ENABLE);
}

/* Enable hif rx DMA and interrupt
 *
 */
void
hif_rx_enable(void)
{
	hif_rx_dma_start();
	writel((readl(HIF_INT_ENABLE) | HIF_INT_EN | HIF_RXPKT_INT_EN),
	       HIF_INT_ENABLE);
}

/* Disable hif rx DMA and interrupt
 *
 */
void
hif_rx_disable(void)
{
	u32	hif_int;

	writel(0, HIF_RX_CTRL);

	hif_int = readl(HIF_INT_ENABLE);
	hif_int &= HIF_RXPKT_INT_EN;
	writel(hif_int, HIF_INT_ENABLE);
}
