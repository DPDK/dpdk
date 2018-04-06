/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 *   Copyright(c) 2018 Synopsys, Inc. All rights reserved.
 */

#include "axgbe_ethdev.h"
#include "axgbe_common.h"
#include "axgbe_phy.h"

#define AXGBE_PHY_PORT_SPEED_100	BIT(0)
#define AXGBE_PHY_PORT_SPEED_1000	BIT(1)
#define AXGBE_PHY_PORT_SPEED_2500	BIT(2)
#define AXGBE_PHY_PORT_SPEED_10000	BIT(3)

#define AXGBE_MUTEX_RELEASE		0x80000000

#define AXGBE_SFP_DIRECT		7

/* I2C target addresses */
#define AXGBE_SFP_SERIAL_ID_ADDRESS	0x50
#define AXGBE_SFP_DIAG_INFO_ADDRESS	0x51
#define AXGBE_SFP_PHY_ADDRESS		0x56
#define AXGBE_GPIO_ADDRESS_PCA9555	0x20

/* SFP sideband signal indicators */
#define AXGBE_GPIO_NO_TX_FAULT		BIT(0)
#define AXGBE_GPIO_NO_RATE_SELECT	BIT(1)
#define AXGBE_GPIO_NO_MOD_ABSENT	BIT(2)
#define AXGBE_GPIO_NO_RX_LOS		BIT(3)

/* Rate-change complete wait/retry count */
#define AXGBE_RATECHANGE_COUNT		500

enum axgbe_port_mode {
	AXGBE_PORT_MODE_RSVD = 0,
	AXGBE_PORT_MODE_BACKPLANE,
	AXGBE_PORT_MODE_BACKPLANE_2500,
	AXGBE_PORT_MODE_1000BASE_T,
	AXGBE_PORT_MODE_1000BASE_X,
	AXGBE_PORT_MODE_NBASE_T,
	AXGBE_PORT_MODE_10GBASE_T,
	AXGBE_PORT_MODE_10GBASE_R,
	AXGBE_PORT_MODE_SFP,
	AXGBE_PORT_MODE_MAX,
};

enum axgbe_conn_type {
	AXGBE_CONN_TYPE_NONE = 0,
	AXGBE_CONN_TYPE_SFP,
	AXGBE_CONN_TYPE_MDIO,
	AXGBE_CONN_TYPE_RSVD1,
	AXGBE_CONN_TYPE_BACKPLANE,
	AXGBE_CONN_TYPE_MAX,
};

/* SFP/SFP+ related definitions */
enum axgbe_sfp_comm {
	AXGBE_SFP_COMM_DIRECT = 0,
	AXGBE_SFP_COMM_PCA9545,
};

enum axgbe_sfp_cable {
	AXGBE_SFP_CABLE_UNKNOWN = 0,
	AXGBE_SFP_CABLE_ACTIVE,
	AXGBE_SFP_CABLE_PASSIVE,
};

enum axgbe_sfp_base {
	AXGBE_SFP_BASE_UNKNOWN = 0,
	AXGBE_SFP_BASE_1000_T,
	AXGBE_SFP_BASE_1000_SX,
	AXGBE_SFP_BASE_1000_LX,
	AXGBE_SFP_BASE_1000_CX,
	AXGBE_SFP_BASE_10000_SR,
	AXGBE_SFP_BASE_10000_LR,
	AXGBE_SFP_BASE_10000_LRM,
	AXGBE_SFP_BASE_10000_ER,
	AXGBE_SFP_BASE_10000_CR,
};

enum axgbe_sfp_speed {
	AXGBE_SFP_SPEED_UNKNOWN = 0,
	AXGBE_SFP_SPEED_100_1000,
	AXGBE_SFP_SPEED_1000,
	AXGBE_SFP_SPEED_10000,
};

/* SFP Serial ID Base ID values relative to an offset of 0 */
#define AXGBE_SFP_BASE_ID			0
#define AXGBE_SFP_ID_SFP			0x03

#define AXGBE_SFP_BASE_EXT_ID			1
#define AXGBE_SFP_EXT_ID_SFP			0x04

#define AXGBE_SFP_BASE_10GBE_CC			3
#define AXGBE_SFP_BASE_10GBE_CC_SR		BIT(4)
#define AXGBE_SFP_BASE_10GBE_CC_LR		BIT(5)
#define AXGBE_SFP_BASE_10GBE_CC_LRM		BIT(6)
#define AXGBE_SFP_BASE_10GBE_CC_ER		BIT(7)

#define AXGBE_SFP_BASE_1GBE_CC			6
#define AXGBE_SFP_BASE_1GBE_CC_SX		BIT(0)
#define AXGBE_SFP_BASE_1GBE_CC_LX		BIT(1)
#define AXGBE_SFP_BASE_1GBE_CC_CX		BIT(2)
#define AXGBE_SFP_BASE_1GBE_CC_T		BIT(3)

#define AXGBE_SFP_BASE_CABLE			8
#define AXGBE_SFP_BASE_CABLE_PASSIVE		BIT(2)
#define AXGBE_SFP_BASE_CABLE_ACTIVE		BIT(3)

#define AXGBE_SFP_BASE_BR			12
#define AXGBE_SFP_BASE_BR_1GBE_MIN		0x0a
#define AXGBE_SFP_BASE_BR_1GBE_MAX		0x0d
#define AXGBE_SFP_BASE_BR_10GBE_MIN		0x64
#define AXGBE_SFP_BASE_BR_10GBE_MAX		0x68

#define AXGBE_SFP_BASE_CU_CABLE_LEN		18

#define AXGBE_SFP_BASE_VENDOR_NAME		20
#define AXGBE_SFP_BASE_VENDOR_NAME_LEN		16
#define AXGBE_SFP_BASE_VENDOR_PN		40
#define AXGBE_SFP_BASE_VENDOR_PN_LEN		16
#define AXGBE_SFP_BASE_VENDOR_REV		56
#define AXGBE_SFP_BASE_VENDOR_REV_LEN		4

#define AXGBE_SFP_BASE_CC			63

/* SFP Serial ID Extended ID values relative to an offset of 64 */
#define AXGBE_SFP_BASE_VENDOR_SN		4
#define AXGBE_SFP_BASE_VENDOR_SN_LEN		16

#define AXGBE_SFP_EXTD_DIAG			28
#define AXGBE_SFP_EXTD_DIAG_ADDR_CHANGE		BIT(2)

#define AXGBE_SFP_EXTD_SFF_8472			30

#define AXGBE_SFP_EXTD_CC			31

struct axgbe_sfp_eeprom {
	u8 base[64];
	u8 extd[32];
	u8 vendor[32];
};

#define AXGBE_BEL_FUSE_VENDOR	"BEL-FUSE"
#define AXGBE_BEL_FUSE_PARTNO	"1GBT-SFP06"

struct axgbe_sfp_ascii {
	union {
		char vendor[AXGBE_SFP_BASE_VENDOR_NAME_LEN + 1];
		char partno[AXGBE_SFP_BASE_VENDOR_PN_LEN + 1];
		char rev[AXGBE_SFP_BASE_VENDOR_REV_LEN + 1];
		char serno[AXGBE_SFP_BASE_VENDOR_SN_LEN + 1];
	} u;
};

/* MDIO PHY reset types */
enum axgbe_mdio_reset {
	AXGBE_MDIO_RESET_NONE = 0,
	AXGBE_MDIO_RESET_I2C_GPIO,
	AXGBE_MDIO_RESET_INT_GPIO,
	AXGBE_MDIO_RESET_MAX,
};

/* Re-driver related definitions */
enum axgbe_phy_redrv_if {
	AXGBE_PHY_REDRV_IF_MDIO = 0,
	AXGBE_PHY_REDRV_IF_I2C,
	AXGBE_PHY_REDRV_IF_MAX,
};

enum axgbe_phy_redrv_model {
	AXGBE_PHY_REDRV_MODEL_4223 = 0,
	AXGBE_PHY_REDRV_MODEL_4227,
	AXGBE_PHY_REDRV_MODEL_MAX,
};

enum axgbe_phy_redrv_mode {
	AXGBE_PHY_REDRV_MODE_CX = 5,
	AXGBE_PHY_REDRV_MODE_SR = 9,
};

#define AXGBE_PHY_REDRV_MODE_REG	0x12b0

/* PHY related configuration information */
struct axgbe_phy_data {
	enum axgbe_port_mode port_mode;

	unsigned int port_id;

	unsigned int port_speeds;

	enum axgbe_conn_type conn_type;

	enum axgbe_mode cur_mode;
	enum axgbe_mode start_mode;

	unsigned int rrc_count;

	unsigned int mdio_addr;

	unsigned int comm_owned;

	/* SFP Support */
	enum axgbe_sfp_comm sfp_comm;
	unsigned int sfp_mux_address;
	unsigned int sfp_mux_channel;

	unsigned int sfp_gpio_address;
	unsigned int sfp_gpio_mask;
	unsigned int sfp_gpio_rx_los;
	unsigned int sfp_gpio_tx_fault;
	unsigned int sfp_gpio_mod_absent;
	unsigned int sfp_gpio_rate_select;

	unsigned int sfp_rx_los;
	unsigned int sfp_tx_fault;
	unsigned int sfp_mod_absent;
	unsigned int sfp_diags;
	unsigned int sfp_changed;
	unsigned int sfp_phy_avail;
	unsigned int sfp_cable_len;
	enum axgbe_sfp_base sfp_base;
	enum axgbe_sfp_cable sfp_cable;
	enum axgbe_sfp_speed sfp_speed;
	struct axgbe_sfp_eeprom sfp_eeprom;

	/* External PHY support */
	enum axgbe_mdio_mode phydev_mode;
	enum axgbe_mdio_reset mdio_reset;
	unsigned int mdio_reset_addr;
	unsigned int mdio_reset_gpio;

	/* Re-driver support */
	unsigned int redrv;
	unsigned int redrv_if;
	unsigned int redrv_addr;
	unsigned int redrv_lane;
	unsigned int redrv_model;
};

static void axgbe_phy_sfp_gpio_setup(struct axgbe_port *pdata)
{
	struct axgbe_phy_data *phy_data = pdata->phy_data;
	unsigned int reg;

	reg = XP_IOREAD(pdata, XP_PROP_3);

	phy_data->sfp_gpio_address = AXGBE_GPIO_ADDRESS_PCA9555 +
		XP_GET_BITS(reg, XP_PROP_3, GPIO_ADDR);

	phy_data->sfp_gpio_mask = XP_GET_BITS(reg, XP_PROP_3, GPIO_MASK);

	phy_data->sfp_gpio_rx_los = XP_GET_BITS(reg, XP_PROP_3,
						GPIO_RX_LOS);
	phy_data->sfp_gpio_tx_fault = XP_GET_BITS(reg, XP_PROP_3,
						  GPIO_TX_FAULT);
	phy_data->sfp_gpio_mod_absent = XP_GET_BITS(reg, XP_PROP_3,
						    GPIO_MOD_ABS);
	phy_data->sfp_gpio_rate_select = XP_GET_BITS(reg, XP_PROP_3,
						     GPIO_RATE_SELECT);
}

static void axgbe_phy_sfp_comm_setup(struct axgbe_port *pdata)
{
	struct axgbe_phy_data *phy_data = pdata->phy_data;
	unsigned int reg, mux_addr_hi, mux_addr_lo;

	reg = XP_IOREAD(pdata, XP_PROP_4);

	mux_addr_hi = XP_GET_BITS(reg, XP_PROP_4, MUX_ADDR_HI);
	mux_addr_lo = XP_GET_BITS(reg, XP_PROP_4, MUX_ADDR_LO);
	if (mux_addr_lo == AXGBE_SFP_DIRECT)
		return;

	phy_data->sfp_comm = AXGBE_SFP_COMM_PCA9545;
	phy_data->sfp_mux_address = (mux_addr_hi << 2) + mux_addr_lo;
	phy_data->sfp_mux_channel = XP_GET_BITS(reg, XP_PROP_4, MUX_CHAN);
}

static void axgbe_phy_sfp_setup(struct axgbe_port *pdata)
{
	axgbe_phy_sfp_comm_setup(pdata);
	axgbe_phy_sfp_gpio_setup(pdata);
}

static bool axgbe_phy_redrv_error(struct axgbe_phy_data *phy_data)
{
	if (!phy_data->redrv)
		return false;

	if (phy_data->redrv_if >= AXGBE_PHY_REDRV_IF_MAX)
		return true;

	switch (phy_data->redrv_model) {
	case AXGBE_PHY_REDRV_MODEL_4223:
		if (phy_data->redrv_lane > 3)
			return true;
		break;
	case AXGBE_PHY_REDRV_MODEL_4227:
		if (phy_data->redrv_lane > 1)
			return true;
		break;
	default:
		return true;
	}

	return false;
}

static int axgbe_phy_mdio_reset_setup(struct axgbe_port *pdata)
{
	struct axgbe_phy_data *phy_data = pdata->phy_data;
	unsigned int reg;

	if (phy_data->conn_type != AXGBE_CONN_TYPE_MDIO)
		return 0;
	reg = XP_IOREAD(pdata, XP_PROP_3);
	phy_data->mdio_reset = XP_GET_BITS(reg, XP_PROP_3, MDIO_RESET);
	switch (phy_data->mdio_reset) {
	case AXGBE_MDIO_RESET_NONE:
	case AXGBE_MDIO_RESET_I2C_GPIO:
	case AXGBE_MDIO_RESET_INT_GPIO:
		break;
	default:
		PMD_DRV_LOG(ERR, "unsupported MDIO reset (%#x)\n",
			    phy_data->mdio_reset);
		return -EINVAL;
	}
	if (phy_data->mdio_reset == AXGBE_MDIO_RESET_I2C_GPIO) {
		phy_data->mdio_reset_addr = AXGBE_GPIO_ADDRESS_PCA9555 +
			XP_GET_BITS(reg, XP_PROP_3,
				    MDIO_RESET_I2C_ADDR);
		phy_data->mdio_reset_gpio = XP_GET_BITS(reg, XP_PROP_3,
							MDIO_RESET_I2C_GPIO);
	} else if (phy_data->mdio_reset == AXGBE_MDIO_RESET_INT_GPIO) {
		phy_data->mdio_reset_gpio = XP_GET_BITS(reg, XP_PROP_3,
							MDIO_RESET_INT_GPIO);
	}

	return 0;
}

static bool axgbe_phy_port_mode_mismatch(struct axgbe_port *pdata)
{
	struct axgbe_phy_data *phy_data = pdata->phy_data;

	switch (phy_data->port_mode) {
	case AXGBE_PORT_MODE_BACKPLANE:
		if ((phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_1000) ||
		    (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_10000))
			return false;
		break;
	case AXGBE_PORT_MODE_BACKPLANE_2500:
		if (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_2500)
			return false;
		break;
	case AXGBE_PORT_MODE_1000BASE_T:
		if ((phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_100) ||
		    (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_1000))
			return false;
		break;
	case AXGBE_PORT_MODE_1000BASE_X:
		if (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_1000)
			return false;
		break;
	case AXGBE_PORT_MODE_NBASE_T:
		if ((phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_100) ||
		    (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_1000) ||
		    (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_2500))
			return false;
		break;
	case AXGBE_PORT_MODE_10GBASE_T:
		if ((phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_100) ||
		    (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_1000) ||
		    (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_10000))
			return false;
		break;
	case AXGBE_PORT_MODE_10GBASE_R:
		if (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_10000)
			return false;
		break;
	case AXGBE_PORT_MODE_SFP:
		if ((phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_100) ||
		    (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_1000) ||
		    (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_10000))
			return false;
		break;
	default:
		break;
	}

	return true;
}

static bool axgbe_phy_conn_type_mismatch(struct axgbe_port *pdata)
{
	struct axgbe_phy_data *phy_data = pdata->phy_data;

	switch (phy_data->port_mode) {
	case AXGBE_PORT_MODE_BACKPLANE:
	case AXGBE_PORT_MODE_BACKPLANE_2500:
		if (phy_data->conn_type == AXGBE_CONN_TYPE_BACKPLANE)
			return false;
		break;
	case AXGBE_PORT_MODE_1000BASE_T:
	case AXGBE_PORT_MODE_1000BASE_X:
	case AXGBE_PORT_MODE_NBASE_T:
	case AXGBE_PORT_MODE_10GBASE_T:
	case AXGBE_PORT_MODE_10GBASE_R:
		if (phy_data->conn_type == AXGBE_CONN_TYPE_MDIO)
			return false;
		break;
	case AXGBE_PORT_MODE_SFP:
		if (phy_data->conn_type == AXGBE_CONN_TYPE_SFP)
			return false;
		break;
	default:
		break;
	}

	return true;
}

static bool axgbe_phy_port_enabled(struct axgbe_port *pdata)
{
	unsigned int reg;

	reg = XP_IOREAD(pdata, XP_PROP_0);
	if (!XP_GET_BITS(reg, XP_PROP_0, PORT_SPEEDS))
		return false;
	if (!XP_GET_BITS(reg, XP_PROP_0, CONN_TYPE))
		return false;

	return true;
}

static int axgbe_phy_init(struct axgbe_port *pdata)
{
	struct axgbe_phy_data *phy_data;
	unsigned int reg;
	int ret;

	/* Check if enabled */
	if (!axgbe_phy_port_enabled(pdata)) {
		PMD_DRV_LOG(ERR, "device is not enabled\n");
		return -ENODEV;
	}

	/* Initialize the I2C controller */
	ret = pdata->i2c_if.i2c_init(pdata);
	if (ret)
		return ret;

	phy_data = rte_zmalloc("phy_data memory", sizeof(*phy_data), 0);
	if (!phy_data) {
		PMD_DRV_LOG(ERR, "phy_data allocation failed\n");
		return -ENOMEM;
	}
	pdata->phy_data = phy_data;

	reg = XP_IOREAD(pdata, XP_PROP_0);
	phy_data->port_mode = XP_GET_BITS(reg, XP_PROP_0, PORT_MODE);
	phy_data->port_id = XP_GET_BITS(reg, XP_PROP_0, PORT_ID);
	phy_data->port_speeds = XP_GET_BITS(reg, XP_PROP_0, PORT_SPEEDS);
	phy_data->conn_type = XP_GET_BITS(reg, XP_PROP_0, CONN_TYPE);
	phy_data->mdio_addr = XP_GET_BITS(reg, XP_PROP_0, MDIO_ADDR);

	reg = XP_IOREAD(pdata, XP_PROP_4);
	phy_data->redrv = XP_GET_BITS(reg, XP_PROP_4, REDRV_PRESENT);
	phy_data->redrv_if = XP_GET_BITS(reg, XP_PROP_4, REDRV_IF);
	phy_data->redrv_addr = XP_GET_BITS(reg, XP_PROP_4, REDRV_ADDR);
	phy_data->redrv_lane = XP_GET_BITS(reg, XP_PROP_4, REDRV_LANE);
	phy_data->redrv_model = XP_GET_BITS(reg, XP_PROP_4, REDRV_MODEL);

	/* Validate the connection requested */
	if (axgbe_phy_conn_type_mismatch(pdata)) {
		PMD_DRV_LOG(ERR, "phy mode/connection mismatch (%#x/%#x)\n",
			    phy_data->port_mode, phy_data->conn_type);
		return -EINVAL;
	}

	/* Validate the mode requested */
	if (axgbe_phy_port_mode_mismatch(pdata)) {
		PMD_DRV_LOG(ERR, "phy mode/speed mismatch (%#x/%#x)\n",
			    phy_data->port_mode, phy_data->port_speeds);
		return -EINVAL;
	}

	/* Check for and validate MDIO reset support */
	ret = axgbe_phy_mdio_reset_setup(pdata);
	if (ret)
		return ret;

	/* Validate the re-driver information */
	if (axgbe_phy_redrv_error(phy_data)) {
		PMD_DRV_LOG(ERR, "phy re-driver settings error\n");
		return -EINVAL;
	}
	pdata->kr_redrv = phy_data->redrv;

	/* Indicate current mode is unknown */
	phy_data->cur_mode = AXGBE_MODE_UNKNOWN;

	/* Initialize supported features */
	pdata->phy.supported = 0;

	switch (phy_data->port_mode) {
		/* Backplane support */
	case AXGBE_PORT_MODE_BACKPLANE:
		pdata->phy.supported |= SUPPORTED_Autoneg;
		pdata->phy.supported |= SUPPORTED_Pause | SUPPORTED_Asym_Pause;
		pdata->phy.supported |= SUPPORTED_Backplane;
		if (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_1000) {
			pdata->phy.supported |= SUPPORTED_1000baseKX_Full;
			phy_data->start_mode = AXGBE_MODE_KX_1000;
		}
		if (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_10000) {
			pdata->phy.supported |= SUPPORTED_10000baseKR_Full;
			if (pdata->fec_ability & MDIO_PMA_10GBR_FECABLE_ABLE)
				pdata->phy.supported |=
					SUPPORTED_10000baseR_FEC;
			phy_data->start_mode = AXGBE_MODE_KR;
		}

		phy_data->phydev_mode = AXGBE_MDIO_MODE_NONE;
		break;
	case AXGBE_PORT_MODE_BACKPLANE_2500:
		pdata->phy.supported |= SUPPORTED_Pause | SUPPORTED_Asym_Pause;
		pdata->phy.supported |= SUPPORTED_Backplane;
		pdata->phy.supported |= SUPPORTED_2500baseX_Full;
		phy_data->start_mode = AXGBE_MODE_KX_2500;

		phy_data->phydev_mode = AXGBE_MDIO_MODE_NONE;
		break;

		/* MDIO 1GBase-T support */
	case AXGBE_PORT_MODE_1000BASE_T:
		pdata->phy.supported |= SUPPORTED_Autoneg;
		pdata->phy.supported |= SUPPORTED_Pause | SUPPORTED_Asym_Pause;
		pdata->phy.supported |= SUPPORTED_TP;
		if (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_100) {
			pdata->phy.supported |= SUPPORTED_100baseT_Full;
			phy_data->start_mode = AXGBE_MODE_SGMII_100;
		}
		if (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_1000) {
			pdata->phy.supported |= SUPPORTED_1000baseT_Full;
			phy_data->start_mode = AXGBE_MODE_SGMII_1000;
		}

		phy_data->phydev_mode = AXGBE_MDIO_MODE_CL22;
		break;

		/* MDIO Base-X support */
	case AXGBE_PORT_MODE_1000BASE_X:
		pdata->phy.supported |= SUPPORTED_Autoneg;
		pdata->phy.supported |= SUPPORTED_Pause | SUPPORTED_Asym_Pause;
		pdata->phy.supported |= SUPPORTED_FIBRE;
		pdata->phy.supported |= SUPPORTED_1000baseT_Full;
		phy_data->start_mode = AXGBE_MODE_X;

		phy_data->phydev_mode = AXGBE_MDIO_MODE_CL22;
		break;

		/* MDIO NBase-T support */
	case AXGBE_PORT_MODE_NBASE_T:
		pdata->phy.supported |= SUPPORTED_Autoneg;
		pdata->phy.supported |= SUPPORTED_Pause | SUPPORTED_Asym_Pause;
		pdata->phy.supported |= SUPPORTED_TP;
		if (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_100) {
			pdata->phy.supported |= SUPPORTED_100baseT_Full;
			phy_data->start_mode = AXGBE_MODE_SGMII_100;
		}
		if (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_1000) {
			pdata->phy.supported |= SUPPORTED_1000baseT_Full;
			phy_data->start_mode = AXGBE_MODE_SGMII_1000;
		}
		if (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_2500) {
			pdata->phy.supported |= SUPPORTED_2500baseX_Full;
			phy_data->start_mode = AXGBE_MODE_KX_2500;
		}

		phy_data->phydev_mode = AXGBE_MDIO_MODE_CL45;
		break;

		/* 10GBase-T support */
	case AXGBE_PORT_MODE_10GBASE_T:
		pdata->phy.supported |= SUPPORTED_Autoneg;
		pdata->phy.supported |= SUPPORTED_Pause | SUPPORTED_Asym_Pause;
		pdata->phy.supported |= SUPPORTED_TP;
		if (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_100) {
			pdata->phy.supported |= SUPPORTED_100baseT_Full;
			phy_data->start_mode = AXGBE_MODE_SGMII_100;
		}
		if (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_1000) {
			pdata->phy.supported |= SUPPORTED_1000baseT_Full;
			phy_data->start_mode = AXGBE_MODE_SGMII_1000;
		}
		if (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_10000) {
			pdata->phy.supported |= SUPPORTED_10000baseT_Full;
			phy_data->start_mode = AXGBE_MODE_KR;
		}

		phy_data->phydev_mode = AXGBE_MDIO_MODE_NONE;
		break;

		/* 10GBase-R support */
	case AXGBE_PORT_MODE_10GBASE_R:
		pdata->phy.supported |= SUPPORTED_Autoneg;
		pdata->phy.supported |= SUPPORTED_Pause | SUPPORTED_Asym_Pause;
		pdata->phy.supported |= SUPPORTED_TP;
		pdata->phy.supported |= SUPPORTED_10000baseT_Full;
		if (pdata->fec_ability & MDIO_PMA_10GBR_FECABLE_ABLE)
			pdata->phy.supported |= SUPPORTED_10000baseR_FEC;
		phy_data->start_mode = AXGBE_MODE_SFI;

		phy_data->phydev_mode = AXGBE_MDIO_MODE_NONE;
		break;

		/* SFP support */
	case AXGBE_PORT_MODE_SFP:
		pdata->phy.supported |= SUPPORTED_Autoneg;
		pdata->phy.supported |= SUPPORTED_Pause | SUPPORTED_Asym_Pause;
		pdata->phy.supported |= SUPPORTED_TP;
		pdata->phy.supported |= SUPPORTED_FIBRE;
		if (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_100) {
			pdata->phy.supported |= SUPPORTED_100baseT_Full;
			phy_data->start_mode = AXGBE_MODE_SGMII_100;
		}
		if (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_1000) {
			pdata->phy.supported |= SUPPORTED_1000baseT_Full;
			phy_data->start_mode = AXGBE_MODE_SGMII_1000;
		}
		if (phy_data->port_speeds & AXGBE_PHY_PORT_SPEED_10000) {
			pdata->phy.supported |= SUPPORTED_10000baseT_Full;
			phy_data->start_mode = AXGBE_MODE_SFI;
			if (pdata->fec_ability & MDIO_PMA_10GBR_FECABLE_ABLE)
				pdata->phy.supported |=
					SUPPORTED_10000baseR_FEC;
		}

		phy_data->phydev_mode = AXGBE_MDIO_MODE_CL22;

		axgbe_phy_sfp_setup(pdata);
		break;
	default:
		return -EINVAL;
	}

	if ((phy_data->conn_type & AXGBE_CONN_TYPE_MDIO) &&
	    (phy_data->phydev_mode != AXGBE_MDIO_MODE_NONE)) {
		ret = pdata->hw_if.set_ext_mii_mode(pdata, phy_data->mdio_addr,
						    phy_data->phydev_mode);
		if (ret) {
			PMD_DRV_LOG(ERR, "mdio port/clause not compatible (%d/%u)\n",
				    phy_data->mdio_addr, phy_data->phydev_mode);
			return -EINVAL;
		}
	}

	if (phy_data->redrv && !phy_data->redrv_if) {
		ret = pdata->hw_if.set_ext_mii_mode(pdata, phy_data->redrv_addr,
						    AXGBE_MDIO_MODE_CL22);
		if (ret) {
			PMD_DRV_LOG(ERR, "redriver mdio port not compatible (%u)\n",
				    phy_data->redrv_addr);
			return -EINVAL;
		}
	}
	return 0;
}
void axgbe_init_function_ptrs_phy_v2(struct axgbe_phy_if *phy_if)
{
	struct axgbe_phy_impl_if *phy_impl = &phy_if->phy_impl;

	phy_impl->init			= axgbe_phy_init;
}
