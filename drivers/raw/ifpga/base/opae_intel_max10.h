/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _OPAE_INTEL_MAX10_H_
#define _OPAE_INTEL_MAX10_H_

#include "opae_osdep.h"
#include "opae_spi.h"
#include "ifpga_compat.h"

struct intel_max10_device;

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

/* Supported MAX10 BMC types */
enum m10bmc_type {
	M10_N3000,
	M10_N6000
};

struct regmap_range {
	unsigned int min;
	unsigned int max;
};

struct m10bmc_regmap {
	int (*reg_write)(struct intel_max10_device *dev,
			unsigned int reg, unsigned int val);
	int (*reg_read)(struct intel_max10_device *dev,
			unsigned int reg, unsigned int *val);
	const struct regmap_range *range;
	int num_ranges;
};

struct m10bmc_csr {
	unsigned int base;
	unsigned int build_version;
	unsigned int fw_version;
	unsigned int fpga_page_info;
	unsigned int doorbell;
	unsigned int auth_result;
};

/**
 * struct flash_raw_blk_ops - device specific operations for flash R/W
 * @write_blk: write a block of data to flash
 * @read_blk: read a block of data from flash
 */
struct flash_raw_blk_ops {
	int (*write_blk)(struct intel_max10_device *dev, uint32_t addr,
			void *buf, uint32_t size);
	int (*read_blk)(struct intel_max10_device *dev, uint32_t addr,
			void *buf, uint32_t size);
};

/**
 * struct m10bmc_ops - device specific operations
 * @lock: prevent concurrent flash read/write
 * @mutex: prevent concurrent bmc read/write
 * @check_flash_range: validate flash address
 * @flash_read: read a block of data from flash
 * @flash_write: write a block of data to flash
 */
struct m10bmc_ops {
	pthread_mutex_t lock;
	pthread_mutex_t *mutex;
	int (*check_flash_range)(u32 start, u32 end);
	int (*flash_read)(struct intel_max10_device *dev, u32 addr,
			void *buf, u32 size);
	int (*flash_write)(struct intel_max10_device *dev, u32 addr,
			void *buf, u32 size);
};

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
	enum m10bmc_type type;
	const struct m10bmc_regmap *ops;
	const struct m10bmc_csr *csr;
	struct flash_raw_blk_ops raw_blk_ops;
	struct m10bmc_ops bmc_ops;
	u8 *mmio; /* mmio address for PMCI */
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
#define   SEC_STATUS_PMCI_SS_FAIL           0x9
#define   SEC_STATUS_FLASH_CMD              0xa
#define   SEC_STATUS_FACTORY_UNVERITY       0xb
#define   SEC_STATUS_FACTORY_ACTIVE         0xc
#define   SEC_STATUS_POWER_DOWN             0xd
#define   SEC_STATUS_CANCELLATION           0xe
#define   SEC_STATUS_HASH                   0xf
#define   SEC_STATUS_FLASH_ACCESS           0x10
#define   SEC_STATUS_SDM_PR_CERT            0x20
#define   SEC_STATUS_SDM_PR_NIOS_BUSY       0x21
#define   SEC_STATUS_SDM_PR_TIMEOUT         0x22
#define   SEC_STATUS_SDM_PR_FAILED          0x23
#define   SEC_STATUS_SDM_PR_MISMATCH        0x24
#define   SEC_STATUS_SDM_PR_FLUSH           0x25
#define   SEC_STATUS_SDM_SR_CERT            0x30
#define   SEC_STATUS_SDM_SR_NIOS_BUSY       0x31
#define   SEC_STATUS_SDM_SR_TIMEOUT         0x32
#define   SEC_STATUS_SDM_SR_FAILED          0x33
#define   SEC_STATUS_SDM_SR_MISMATCH        0x34
#define   SEC_STATUS_SDM_SR_FLUSH           0x35
#define   SEC_STATUS_SDM_KEY_CERT           0x40
#define   SEC_STATUS_SDM_KEY_NIOS_BUSY      0x41
#define   SEC_STATUS_SDM_KEY_TIMEOUT        0x42
#define   SEC_STATUS_SDM_KEY_FAILED         0x43
#define   SEC_STATUS_SDM_KEY_MISMATCH       0x44
#define   SEC_STATUS_SDM_KEY_FLUSH          0x45
#define   SEC_STATUS_NIOS_OK		0x80
#define   SEC_STATUS_USER_OK		0x81
#define   SEC_STATUS_FACTORY_OK		0x82
#define   SEC_STATUS_USER_FAIL		0x83
#define   SEC_STATUS_FACTORY_FAIL	0x84
#define   SEC_STATUS_NIOS_FLASH_ERR	0x85
#define   SEC_STATUS_FPGA_FLASH_ERR	0x86
#define   SEC_STATUS_MAX   SEC_STATUS_FPGA_FLASH_ERR

/* Authentication status */
#define SEC_AUTH_G(v)	((v) & 0xff)
#define AUTH_STAT_PASS    0x0
#define AUTH_STAT_B0_MAGIC   0x1
#define AUTH_STAT_CONLEN  0x2
#define AUTH_STAT_CONTYPE 0x3
#define AUTH_STAT_B1_MAGIC 0x4
#define AUTH_STAT_ROOT_MAGIC 0x5
#define AUTH_STAT_CURVE_MAGIC 0x6
#define AUTH_STAT_PERMISSION 0x7
#define AUTH_STAT_KEY_ID    0x8
#define AUTH_STAT_CSK_MAGIC 0x9
#define AUTH_STAT_CSK_CURVE 0xa
#define AUTH_STAT_CSK_PERMISSION 0xb
#define AUTH_STAT_CSK_ID    0xc
#define AUTH_STAT_CSK_SM 0xd
#define AUTH_STAT_B0_E_MAGIC 0xe
#define AUTH_STAT_B0_E_SIGN 0xf
#define AUTH_STAT_RK_P      0x10
#define AUTH_STAT_RE_SHA    0x11
#define AUTH_STAT_CSK_SHA   0x12
#define AUTH_STAT_B0_SHA    0x13
#define AUTH_STAT_KEY_INV   0x14
#define AUTH_STAT_KEY_CAN   0x15
#define AUTH_STAT_UP_SHA    0x16
#define AUTH_STAT_CAN_SHA   0x17
#define AUTH_STAT_HASH      0x18
#define AUTH_STAT_INV_ID    0x19
#define AUTH_STAT_KEY_PROG  0x1a
#define AUTH_STAT_INV_BC    0x1b
#define AUTH_STAT_INV_SLOT  0x1c
#define AUTH_STAT_IN_OP     0x1d
#define AUTH_STAT_TIME_OUT  0X1e
#define AUTH_STAT_SHA_TO    0x1f
#define AUTH_STAT_CSK_TO    0x20
#define AUTH_STAT_B0_TO     0x21
#define AUTH_STAT_UP_TO     0x22
#define AUTH_STAT_CAN_TO    0x23
#define AUTH_STAT_HASH_TO   0x24
#define AUTH_STAT_AUTH_IDLE 0xfe
#define AUTH_STAT_GA_FAIL   0xff
#define AUTH_STAT_S_ERR     0x8000
#define AUTH_STAT_S_MN      0x8001
#define AUTH_STAT_SH_CRC     0x8002
#define AUTH_STAT_SD_CRC    0x8003
#define AUTH_STAT_SD_LEN    0x8004
#define AUTH_STAT_S_ID      0x8005
#define AUTH_STAT_S_THR    0x8006
#define AUTH_STAT_S_TO      0x8007
#define AUTH_STAT_S_EN     0x8008
#define AUTH_STAT_SF       0x8009
#define AUTH_STAT_MAX    AUTH_STAT_SF

#define   CONFIG_SEL		BIT(28)
#define   CONFIG_SEL_S(v)	(((v) & 0x1) << 28)
#define   REBOOT_REQ		BIT(29)
#define   REBOOT_DISABLED	BIT(30)
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

#define m10bmc_base(max10) ((max10)->csr->base)
#define doorbell_reg(max10) ((max10)->csr->doorbell)
#define auth_result_reg(max10) ((max10)->csr->auth_result)

int max10_sys_read(struct intel_max10_device *dev,
	unsigned int offset, unsigned int *val);
int max10_sys_write(struct intel_max10_device *dev,
	unsigned int offset, unsigned int val);
int max10_reg_read(struct intel_max10_device *dev,
	unsigned int offset, unsigned int *val);
int max10_reg_write(struct intel_max10_device *dev,
	unsigned int offset, unsigned int val);
int max10_sys_update_bits(struct intel_max10_device *dev,
	unsigned int offset, unsigned int msk, unsigned int val);
int max10_get_bmcfw_version(struct intel_max10_device *dev, unsigned int *val);
int max10_get_bmc_version(struct intel_max10_device *dev, unsigned int *val);
int max10_get_fpga_load_info(struct intel_max10_device *dev, unsigned int *val);
int intel_max10_device_init(struct intel_max10_device *dev);
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

#define SENSOR_INVALID 0xdeadbeef

struct max10_sensor_raw_data {
	unsigned int reg_input;
	unsigned int reg_high_warn;
	unsigned int reg_high_fatal;
	unsigned int reg_hyst;
	unsigned int reg_low_warn;
	unsigned int multiplier;
	const char *label;
};

struct max10_sensor_data {
	const char *type;
	unsigned int number;
	const struct max10_sensor_raw_data *table;
};

enum max10_sensor_types {
	sensor_temp,
	sensor_in,
	sensor_curr,
	sensor_power,
	sensor_max,
};

#define SENSOR_TMP_NAME "Temperature"
#define SENSOR_IN_NAME "Voltage"
#define SENSOR_CURR_NAME "Current"
#define SENSOR_POWER_NAME "Power"

struct max10_sensor_board_data {
	const struct max10_sensor_data *tables[sensor_max];
};

/* indirect access for PMCI */
#define PMCI_INDIRECT_BASE 0x400
#define INDIRECT_CMD_OFF   (PMCI_INDIRECT_BASE + 0x0)
#define INDIRECT_CMD_RD	BIT(0)
#define INDIRECT_CMD_WR	BIT(1)
#define INDIRECT_CMD_ACK	BIT(2)

#define INDIRECT_ADDR_OFF	 (PMCI_INDIRECT_BASE + 0x4)
#define INDIRECT_RD_OFF	         (PMCI_INDIRECT_BASE + 0x8)
#define INDIRECT_WR_OFF	 (PMCI_INDIRECT_BASE + 0xc)

#define INDIRECT_INT_US	1
#define INDIRECT_TIMEOUT_US	10000

#define M10BMC_PMCI_SYS_BASE 0x0
#define M10BMC_PMCI_SYS_END  0xfff

#define M10BMC_PMCI_BUILD_VER   0x0
#define NIOS2_PMCI_FW_VERSION   0x4

#define M10BMC_PMCI_PWR_STATE 0xb4
#define PMCI_PRIMARY_IMAGE_PAGE GENMASK(10, 8)

#define M10BMC_PMCI_DOORBELL 0x1c0
#define PMCI_DRBL_REBOOT_DISABLED BIT(1)
#define M10BMC_PMCI_AUTH_RESULT 0x1c4

#define M10BMC_PMCI_MAX10_RECONF 0xfc
#define PMCI_MAX10_REBOOT_REQ BIT(0)
#define PMCI_MAX10_REBOOT_PAGE BIT(1)

#define M10BMC_PMCI_FPGA_RECONF 0xb8
#define PMCI_FPGA_RECONF_PAGE  GENMASK(22, 20)
#define PMCI_FPGA_RP_LOAD      BIT(23)

#define PMCI_FLASH_CTRL 0x40
#define PMCI_FLASH_WR_MODE BIT(0)
#define PMCI_FLASH_RD_MODE BIT(1)
#define PMCI_FLASH_BUSY    BIT(2)
#define PMCI_FLASH_FIFO_SPACE GENMASK(13, 4)
#define PMCI_FLASH_READ_COUNT GENMASK(25, 16)

#define PMCI_FLASH_INT_US       1
#define PMCI_FLASH_TIMEOUT_US   10000

#define PMCI_FLASH_ADDR 0x44
#define PMCI_FLASH_FIFO 0x800
#define PMCI_READ_BLOCK_SIZE 0x800
#define PMCI_FIFO_MAX_BYTES 0x800
#define PMCI_FIFO_MAX_WORDS (PMCI_FIFO_MAX_BYTES / 4)

#define M10BMC_PMCI_FPGA_POC	0xb0
#define PMCI_FPGA_POC		BIT(0)
#define PMCI_NIOS_REQ_CLEAR	BIT(1)
#define PMCI_NIOS_STATUS	GENMASK(5, 4)
#define NIOS_STATUS_IDLE	0
#define NIOS_STATUS_SUCCESS	1
#define NIOS_STATUS_FAIL	2
#define PMCI_USER_IMAGE_PAGE	GENMASK(10, 8)
#define POC_USER_IMAGE_1	1
#define POC_USER_IMAGE_2	2
#define PMCI_FACTORY_IMAGE_SEL	BIT(31)

#define M10BMC_PMCI_FPGA_CONF_STS 0xa0
#define PMCI_FPGA_BOOT_PAGE  GENMASK(2, 0)
#define PMCI_FPGA_CONFIGURED  BIT(3)

#define M10BMC_PMCI_FLASH_CTRL 0x1d0
#define FLASH_MUX_SELECTION GENMASK(2, 0)
#define FLASH_MUX_IDLE 0
#define FLASH_MUX_NIOS 1
#define FLASH_MUX_HOST 2
#define FLASH_MUX_PFL  4
#define get_flash_mux(mux)  GET_FIELD(FLASH_MUX_SELECTION, mux)
#define FLASH_NIOS_REQUEST BIT(4)
#define FLASH_HOST_REQUEST BIT(5)

#define M10BMC_PMCI_SDM_CTRL_STS 0x230
#define PMCI_SDM_IMG_REQ	BIT(0)
#define PMCI_SDM_STAT GENMASK(23, 16)

#define SDM_STAT_DONE    0x0
#define SDM_STAT_PROV    0x1
#define SDM_STAT_BUSY    0x2
#define SDM_STAT_INV     0x3
#define SDM_STAT_FAIL    0x4
#define SDM_STAT_BMC_BUSY 0x5
#define SDM_STAT_TO      0x6
#define SDM_STAT_DB      0x7
#define SDM_STAT_CON_R    0x8
#define SDM_STAT_CON_E    0x9
#define SDM_STAT_WAIT     0xa
#define SDM_STAT_RTO      0xb
#define SDM_STAT_SB       0xc
#define SDM_STAT_RE       0xd
#define SDM_STAT_PDD     0xe
#define SDM_STAT_ISC     0xf
#define SDM_STAT_SIC     0x10
#define SDM_STAT_NO_PROV  0x11
#define SDM_STAT_CS_MIS   0x12
#define SDM_STAT_PR_MIS   0x13
#define SDM_STAT_MAX SDM_STAT_PR_MIS

#define PMCI_FLASH_START 0x10000
#define PMCI_FLASH_END 0xC7FFFFF

int opae_read_flash(struct intel_max10_device *dev, u32 addr,
		u32 size, void *buf);
#endif
