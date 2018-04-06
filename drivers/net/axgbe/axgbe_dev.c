/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 *   Copyright(c) 2018 Synopsys, Inc. All rights reserved.
 */

#include "axgbe_ethdev.h"
#include "axgbe_common.h"
#include "axgbe_phy.h"

/* query busy bit */
static int mdio_complete(struct axgbe_port *pdata)
{
	if (!AXGMAC_IOREAD_BITS(pdata, MAC_MDIOSCCDR, BUSY))
		return 1;

	return 0;
}

static int axgbe_write_ext_mii_regs(struct axgbe_port *pdata, int addr,
				    int reg, u16 val)
{
	unsigned int mdio_sca, mdio_sccd;
	uint64_t timeout;

	mdio_sca = 0;
	AXGMAC_SET_BITS(mdio_sca, MAC_MDIOSCAR, REG, reg);
	AXGMAC_SET_BITS(mdio_sca, MAC_MDIOSCAR, DA, addr);
	AXGMAC_IOWRITE(pdata, MAC_MDIOSCAR, mdio_sca);

	mdio_sccd = 0;
	AXGMAC_SET_BITS(mdio_sccd, MAC_MDIOSCCDR, DATA, val);
	AXGMAC_SET_BITS(mdio_sccd, MAC_MDIOSCCDR, CMD, 1);
	AXGMAC_SET_BITS(mdio_sccd, MAC_MDIOSCCDR, BUSY, 1);
	AXGMAC_IOWRITE(pdata, MAC_MDIOSCCDR, mdio_sccd);

	timeout = rte_get_timer_cycles() + rte_get_timer_hz();
	while (time_before(rte_get_timer_cycles(), timeout)) {
		rte_delay_us(100);
		if (mdio_complete(pdata))
			return 0;
	}

	PMD_DRV_LOG(ERR, "Mdio write operation timed out\n");
	return -ETIMEDOUT;
}

static int axgbe_read_ext_mii_regs(struct axgbe_port *pdata, int addr,
				   int reg)
{
	unsigned int mdio_sca, mdio_sccd;
	uint64_t timeout;

	mdio_sca = 0;
	AXGMAC_SET_BITS(mdio_sca, MAC_MDIOSCAR, REG, reg);
	AXGMAC_SET_BITS(mdio_sca, MAC_MDIOSCAR, DA, addr);
	AXGMAC_IOWRITE(pdata, MAC_MDIOSCAR, mdio_sca);

	mdio_sccd = 0;
	AXGMAC_SET_BITS(mdio_sccd, MAC_MDIOSCCDR, CMD, 3);
	AXGMAC_SET_BITS(mdio_sccd, MAC_MDIOSCCDR, BUSY, 1);
	AXGMAC_IOWRITE(pdata, MAC_MDIOSCCDR, mdio_sccd);

	timeout = rte_get_timer_cycles() + rte_get_timer_hz();

	while (time_before(rte_get_timer_cycles(), timeout)) {
		rte_delay_us(100);
		if (mdio_complete(pdata))
			goto success;
	}

	PMD_DRV_LOG(ERR, "Mdio read operation timed out\n");
	return -ETIMEDOUT;

success:
	return AXGMAC_IOREAD_BITS(pdata, MAC_MDIOSCCDR, DATA);
}

static int axgbe_set_ext_mii_mode(struct axgbe_port *pdata, unsigned int port,
				  enum axgbe_mdio_mode mode)
{
	unsigned int reg_val = 0;

	switch (mode) {
	case AXGBE_MDIO_MODE_CL22:
		if (port > AXGMAC_MAX_C22_PORT)
			return -EINVAL;
		reg_val |= (1 << port);
		break;
	case AXGBE_MDIO_MODE_CL45:
		break;
	default:
		return -EINVAL;
	}
	AXGMAC_IOWRITE(pdata, MAC_MDIOCL22R, reg_val);

	return 0;
}

static int axgbe_read_mmd_regs_v2(struct axgbe_port *pdata,
				  int prtad __rte_unused, int mmd_reg)
{
	unsigned int mmd_address, index, offset;
	int mmd_data;

	if (mmd_reg & MII_ADDR_C45)
		mmd_address = mmd_reg & ~MII_ADDR_C45;
	else
		mmd_address = (pdata->mdio_mmd << 16) | (mmd_reg & 0xffff);

	/* The PCS registers are accessed using mmio. The underlying
	 * management interface uses indirect addressing to access the MMD
	 * register sets. This requires accessing of the PCS register in two
	 * phases, an address phase and a data phase.
	 *
	 * The mmio interface is based on 16-bit offsets and values. All
	 * register offsets must therefore be adjusted by left shifting the
	 * offset 1 bit and reading 16 bits of data.
	 */
	mmd_address <<= 1;
	index = mmd_address & ~pdata->xpcs_window_mask;
	offset = pdata->xpcs_window + (mmd_address & pdata->xpcs_window_mask);

	pthread_mutex_lock(&pdata->xpcs_mutex);

	XPCS32_IOWRITE(pdata, pdata->xpcs_window_sel_reg, index);
	mmd_data = XPCS16_IOREAD(pdata, offset);

	pthread_mutex_unlock(&pdata->xpcs_mutex);

	return mmd_data;
}

static void axgbe_write_mmd_regs_v2(struct axgbe_port *pdata,
				    int prtad __rte_unused,
				    int mmd_reg, int mmd_data)
{
	unsigned int mmd_address, index, offset;

	if (mmd_reg & MII_ADDR_C45)
		mmd_address = mmd_reg & ~MII_ADDR_C45;
	else
		mmd_address = (pdata->mdio_mmd << 16) | (mmd_reg & 0xffff);

	/* The PCS registers are accessed using mmio. The underlying
	 * management interface uses indirect addressing to access the MMD
	 * register sets. This requires accessing of the PCS register in two
	 * phases, an address phase and a data phase.
	 *
	 * The mmio interface is based on 16-bit offsets and values. All
	 * register offsets must therefore be adjusted by left shifting the
	 * offset 1 bit and writing 16 bits of data.
	 */
	mmd_address <<= 1;
	index = mmd_address & ~pdata->xpcs_window_mask;
	offset = pdata->xpcs_window + (mmd_address & pdata->xpcs_window_mask);

	pthread_mutex_lock(&pdata->xpcs_mutex);

	XPCS32_IOWRITE(pdata, pdata->xpcs_window_sel_reg, index);
	XPCS16_IOWRITE(pdata, offset, mmd_data);

	pthread_mutex_unlock(&pdata->xpcs_mutex);
}

static int axgbe_read_mmd_regs(struct axgbe_port *pdata, int prtad,
			       int mmd_reg)
{
	switch (pdata->vdata->xpcs_access) {
	case AXGBE_XPCS_ACCESS_V1:
		PMD_DRV_LOG(ERR, "PHY_Version 1 is not supported\n");
		return -1;
	case AXGBE_XPCS_ACCESS_V2:
	default:
		return axgbe_read_mmd_regs_v2(pdata, prtad, mmd_reg);
	}
}

static void axgbe_write_mmd_regs(struct axgbe_port *pdata, int prtad,
				 int mmd_reg, int mmd_data)
{
	switch (pdata->vdata->xpcs_access) {
	case AXGBE_XPCS_ACCESS_V1:
		PMD_DRV_LOG(ERR, "PHY_Version 1 is not supported\n");
		return;
	case AXGBE_XPCS_ACCESS_V2:
	default:
		return axgbe_write_mmd_regs_v2(pdata, prtad, mmd_reg, mmd_data);
	}
}

static int axgbe_set_speed(struct axgbe_port *pdata, int speed)
{
	unsigned int ss;

	switch (speed) {
	case SPEED_1000:
		ss = 0x03;
		break;
	case SPEED_2500:
		ss = 0x02;
		break;
	case SPEED_10000:
		ss = 0x00;
		break;
	default:
		return -EINVAL;
	}

	if (AXGMAC_IOREAD_BITS(pdata, MAC_TCR, SS) != ss)
		AXGMAC_IOWRITE_BITS(pdata, MAC_TCR, SS, ss);

	return 0;
}

static int __axgbe_exit(struct axgbe_port *pdata)
{
	unsigned int count = 2000;

	/* Issue a software reset */
	AXGMAC_IOWRITE_BITS(pdata, DMA_MR, SWR, 1);
	rte_delay_us(10);

	/* Poll Until Poll Condition */
	while (--count && AXGMAC_IOREAD_BITS(pdata, DMA_MR, SWR))
		rte_delay_us(500);

	if (!count)
		return -EBUSY;

	return 0;
}

static int axgbe_exit(struct axgbe_port *pdata)
{
	int ret;

	/* To guard against possible incorrectly generated interrupts,
	 * issue the software reset twice.
	 */
	ret = __axgbe_exit(pdata);
	if (ret)
		return ret;

	return __axgbe_exit(pdata);
}

void axgbe_init_function_ptrs_dev(struct axgbe_hw_if *hw_if)
{
	hw_if->exit = axgbe_exit;


	hw_if->read_mmd_regs = axgbe_read_mmd_regs;
	hw_if->write_mmd_regs = axgbe_write_mmd_regs;

	hw_if->set_speed = axgbe_set_speed;

	hw_if->set_ext_mii_mode = axgbe_set_ext_mii_mode;
	hw_if->read_ext_mii_regs = axgbe_read_ext_mii_regs;
	hw_if->write_ext_mii_regs = axgbe_write_ext_mii_regs;
}
