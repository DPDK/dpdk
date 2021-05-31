/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _OPAE_INTEL_MAX10_H_
#define _OPAE_INTEL_MAX10_H_

#include "opae_osdep.h"
#include "opae_spi.h"

struct max10_compatible_id {
	char compatible[128];
};

#define MAX10_PAC	"intel,max10"
#define MAX10_PAC_N3000	"intel,max10-pac-n3000"
#define MAX10_PAC_END    "intel,end"

/* max10 capability flags */
#define MAX10_FLAGS_NO_I2C2		BIT(0)
#define MAX10_FLAGS_NO_BMCIMG_FLASH	BIT(1)
#define MAX10_FLAGS_DEVICE_TABLE        BIT(2)
#define MAX10_FLAGS_SPI                 BIT(3)
#define MAX10_FLGAS_NIOS_SPI            BIT(4)
#define MAX10_FLAGS_PKVL                BIT(5)
#define MAX10_FLAGS_SECURE		BIT(6)
#define MAX10_FLAGS_MAC_CACHE		BIT(7)

/** List of opae sensors */
TAILQ_HEAD(opae_sensor_list, opae_sensor_info);

struct intel_max10_device {
	unsigned int flags; /*max10 hardware capability*/
	struct altera_spi_device *spi_master;
	struct spi_transaction_dev *spi_tran_dev;
	struct max10_compatible_id *id; /*max10 compatible*/
	char *fdt_root;
	unsigned int base; /* max10 base address */
	u16 bus;
	struct opae_sensor_list opae_sensor_list;
	u32 staging_area_base;
	u32 staging_area_size;
};

/* retimer speed */
enum retimer_speed {
	MXD_1GB = 1,
	MXD_2_5GB = 2,
	MXD_5GB = 5,
	MXD_10GB = 10,
	MXD_25GB = 25,
	MXD_40GB = 40,
	MXD_100GB = 100,
	MXD_SPEED_UNKNOWN,
};

/* retimer info */
struct opae_retimer_info {
	unsigned int nums_retimer;
	unsigned int ports_per_retimer;
	unsigned int nums_fvl;
	unsigned int ports_per_fvl;
	enum retimer_speed support_speed;
};

/* retimer status*/
struct opae_retimer_status {
	enum retimer_speed speed;
	/*
	 * retimer line link status bitmap:
	 * bit 0: Retimer0 Port0 link status
	 * bit 1: Retimer0 Port1 link status
	 * bit 2: Retimer0 Port2 link status
	 * bit 3: Retimer0 Port3 link status
	 *
	 * bit 4: Retimer1 Port0 link status
	 * bit 5: Retimer1 Port1 link status
	 * bit 6: Retimer1 Port2 link status
	 * bit 7: Retimer1 Port3 link status
	 */
	unsigned int line_link_bitmap;
};

#define FLASH_BASE 0x10000000
#define FLASH_OPTION_BITS 0x10000

/* System Registers */
#define MAX10_BASE_ADDR		0x300400
#define MAX10_SEC_BASE_ADDR	0x300800
/* Register offset of system registers */
#define NIOS2_FW_VERSION	0x0
#define MAX10_MACADDR1		0x10
#define   MAX10_MAC_BYTE4	GENMASK(7, 0)
#define   MAX10_MAC_BYTE3	GENMASK(15, 8)
#define   MAX10_MAC_BYTE2	GENMASK(23, 16)
#define   MAX10_MAC_BYTE1	GENMASK(31, 24)
#define MAX10_MACADDR2		0x14
#define   MAX10_MAC_BYTE6	GENMASK(7, 0)
#define   MAX10_MAC_BYTE5	GENMASK(15, 8)
#define   MAX10_MAC_COUNT	GENMASK(23, 16)
#define RSU_REG			0x2c
#define   FPGA_RECONF_PAGE	GENMASK(2, 0)
#define   FPGA_PAGE(p)		((p) & 0x1)
#define   FPGA_RP_LOAD		BIT(3)
#define   NIOS2_PRERESET	BIT(4)
#define   NIOS2_HANG		BIT(5)
#define   RSU_ENABLE		BIT(6)
#define   NIOS2_RESET		BIT(7)
#define   NIOS2_I2C2_POLL_STOP	BIT(13)
#define   PKVL_EEPROM_LOAD	BIT(31)
#define FPGA_RECONF_REG		0x30
#define   SFPGA_RECONF_PAGE	GENMASK(22, 20)
#define   SFPGA_PAGE(p)		(((p) & 0x1) << 20)
#define   SFPGA_RP_LOAD		BIT(23)
#define MAX10_TEST_REG		0x3c
#define   COUNTDOWN_START	BIT(18)
#define MAX10_BUILD_VER		0x68
#define   MAX10_VERSION_MAJOR	GENMASK(23, 16)
#define   PCB_INFO		GENMASK(31, 24)
#define FPGA_PAGE_INFO		0x6c
#define DT_AVAIL_REG		0x90
#define   DT_AVAIL		BIT(0)
#define DT_BASE_ADDR_REG	0x94
#define MAX10_DOORBELL		0x400
#define   RSU_REQUEST		BIT(0)
#define   SEC_PROGRESS		GENMASK(7, 4)
#define   SEC_PROGRESS_G(v)	(((v) >> 4) & 0xf)
#define   SEC_PROGRESS_IDLE				0x0
#define   SEC_PROGRESS_PREPARE			0x1
#define   SEC_PROGRESS_SLEEP			0x2
#define   SEC_PROGRESS_READY			0x3
#define   SEC_PROGRESS_AUTHENTICATING	0x4
#define   SEC_PROGRESS_COPYING			0x5
#define   SEC_PROGRESS_UPDATE_CANCEL	0x6
#define   SEC_PROGRESS_PROGRAM_KEY_HASH	0x7
#define   SEC_PROGRESS_RSU_DONE			0x8
#define   SEC_PROGRESS_PKVL_PROM_DONE	0x9
#define   HOST_STATUS		GENMASK(11, 8)
#define   HOST_STATUS_S(v)	(((v) << 8) & 0xf00)
#define   HOST_STATUS_IDLE			0x0
#define   HOST_STATUS_WRITE_DONE	0x1
#define   HOST_STATUS_ABORT_RSU		0x2
#define   SEC_STATUS		GENMASK(23, 16)
#define   SEC_STATUS_G(v)	(((v) >> 16) & 0xff)
#define   SEC_STATUS_NORMAL			0x0
#define   SEC_STATUS_TIMEOUT		0x1
#define   SEC_STATUS_AUTH_FAIL		0x2
#define   SEC_STATUS_COPY_FAIL		0x3
#define   SEC_STATUS_FATAL			0x4
#define   SEC_STATUS_PKVL_REJECT	0x5
#define   SEC_STATUS_NON_INC		0x6
#define   SEC_STATUS_ERASE_FAIL		0x7
#define   SEC_STATUS_WEAROUT		0x8
#define   SEC_STATUS_NIOS_OK		0x80
#define   SEC_STATUS_USER_OK		0x81
#define   SEC_STATUS_FACTORY_OK		0x82
#define   SEC_STATUS_USER_FAIL		0x83
#define   SEC_STATUS_FACTORY_FAIL	0x84
#define   SEC_STATUS_NIOS_FLASH_ERR	0x85
#define   SEC_STATUS_FPGA_FLASH_ERR	0x86
#define   CONFIG_SEL		BIT(28)
#define   CONFIG_SEL_S(v)	(((v) & 0x1) << 28)
#define   REBOOT_REQ		BIT(29)
#define MAX10_AUTH_RESULT	0x404

/* PKVL related registers, in system register region */
#define PKVL_POLLING_CTRL		0x80
#define   POLLING_MODE			GENMASK(15, 0)
#define   PKVL_A_PRELOAD		BIT(16)
#define   PKVL_A_PRELOAD_TIMEOUT	BIT(17)
#define   PKVL_A_DATA_TOO_BIG		BIT(18)
#define   PKVL_A_HDR_CHECKSUM		BIT(20)
#define   PKVL_B_PRELOAD		BIT(24)
#define   PKVL_B_PRELOAD_TIMEOUT	BIT(25)
#define   PKVL_B_DATA_TOO_BIG		BIT(26)
#define   PKVL_B_HDR_CHECKSUM		BIT(28)
#define   PKVL_EEPROM_UPG_STATUS	GENMASK(31, 16)
#define PKVL_LINK_STATUS		0x164
#define PKVL_A_VERSION			0x254
#define PKVL_B_VERSION			0x258
#define   SERDES_VERSION		GENMASK(15, 0)
#define   SBUS_VERSION			GENMASK(31, 16)

#define DFT_MAX_SIZE		0x7e0000
#define MAX_STAGING_AREA_BASE	0xffffffff
#define MAX_STAGING_AREA_SIZE	0x3800000

int max10_reg_read(struct intel_max10_device *dev,
	unsigned int reg, unsigned int *val);
int max10_reg_write(struct intel_max10_device *dev,
	unsigned int reg, unsigned int val);
int max10_sys_read(struct intel_max10_device *dev,
	unsigned int offset, unsigned int *val);
int max10_sys_write(struct intel_max10_device *dev,
	unsigned int offset, unsigned int val);
int max10_sys_update_bits(struct intel_max10_device *dev,
	unsigned int offset, unsigned int msk, unsigned int val);
struct intel_max10_device *
intel_max10_device_probe(struct altera_spi_device *spi,
		int chipselect);
int intel_max10_device_remove(struct intel_max10_device *dev);


#define SENSOR_REG_VALUE 0x0
#define SENSOR_REG_HIGH_WARN 0x1
#define SENSOR_REG_HIGH_FATAL 0x2
#define SENSOR_REG_LOW_WARN 0x3
#define SENSOR_REG_LOW_FATAL 0x4
#define SENSOR_REG_HYSTERESIS 0x5
#define SENSOR_REG_MAX 0x6

static const char * const sensor_reg_name[] = {
	"value",
	"high_warn",
	"high_fatal",
	"low_warn",
	"low_fatal",
	"hysteresis",
};

struct sensor_reg {
	unsigned int regoff;
	size_t size;
};

struct raw_sensor_info {
	const char *name;
	const char *type;
	unsigned int id;
	unsigned int multiplier;
	struct sensor_reg regs[SENSOR_REG_MAX];
};

#define OPAE_SENSOR_VALID 0x1
#define OPAE_SENSOR_HIGH_WARN_VALID 0x2
#define OPAE_SENSOR_HIGH_FATAL_VALID 0x4
#define OPAE_SENSOR_LOW_WARN_VALID 0x8
#define OPAE_SENSOR_LOW_FATAL_VALID 0x10
#define OPAE_SENSOR_HYSTERESIS_VALID 0x20

struct opae_sensor_info {
	TAILQ_ENTRY(opae_sensor_info) node;
	const char *name;
	const char *type;
	unsigned int id;
	unsigned int high_fatal;
	unsigned int high_warn;
	unsigned int low_fatal;
	unsigned int low_warn;
	unsigned int hysteresis;
	unsigned int multiplier;
	unsigned int flags;
	unsigned int value;
	unsigned int value_reg;
};

#endif
