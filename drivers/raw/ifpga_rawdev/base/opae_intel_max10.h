/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _OPAE_INTEL_MAX10_H_
#define _OPAE_INTEL_MAX10_H_

#include "opae_osdep.h"
#include "opae_spi.h"

#define INTEL_MAX10_MAX_MDIO_DEVS 2
#define PKVL_NUMBER_PORTS  4

/* max10 capability flags */
#define MAX10_FLAGS_NO_I2C2		BIT(0)
#define MAX10_FLAGS_NO_BMCIMG_FLASH	BIT(1)
#define MAX10_FLAGS_DEVICE_TABLE        BIT(2)
#define MAX10_FLAGS_SPI                 BIT(3)
#define MAX10_FLGAS_NIOS_SPI            BIT(4)
#define MAX10_FLAGS_PKVL                BIT(5)

struct intel_max10_device {
	unsigned int flags; /*max10 hardware capability*/
	struct altera_spi_device *spi_master;
	struct spi_transaction_dev *spi_tran_dev;
};

#define FLASH_BASE 0x10000000
#define FLASH_OPTION_BITS 0x10000

#define NIOS2_FW_VERSION_OFF   0x300400
#define RSU_REG_OFF            0x30042c
#define FPGA_RP_LOAD		BIT(3)
#define NIOS2_PRERESET		BIT(4)
#define NIOS2_HANG		BIT(5)
#define RSU_ENABLE		BIT(6)
#define NIOS2_RESET		BIT(7)
#define NIOS2_I2C2_POLL_STOP	BIT(13)
#define FPGA_RECONF_REG_OFF	0x300430
#define COUNTDOWN_START		BIT(18)
#define MAX10_BUILD_VER_OFF	0x300468
#define PCB_INFO		GENMASK(31, 24)
#define MAX10_BUILD_VERION	GENMASK(23, 0)
#define FPGA_PAGE_INFO_OFF	0x30046c
#define DT_AVAIL_REG_OFF	0x300490
#define DT_AVAIL		BIT(0)
#define DT_BASE_ADDR_REG_OFF	0x300494
#define PKVL_POLLING_CTRL       0x300480
#define PKVL_LINK_STATUS        0x300564

#define DFT_MAX_SIZE		0x7e0000

int max10_reg_read(unsigned int reg, unsigned int *val);
int max10_reg_write(unsigned int reg, unsigned int val);
struct intel_max10_device *
intel_max10_device_probe(struct altera_spi_device *spi,
		int chipselect);
int intel_max10_device_remove(struct intel_max10_device *dev);

#endif
