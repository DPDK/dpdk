/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NT4GA_SPI_V3__
#define __NT4GA_SPI_V3__

#include "nthw_spis.h"
#include "nthw_spim.h"

/* Must include v1.x series. The first v1.0a only had 248 bytes of storage. v2.0x have 255 */
#define MAX_AVR_CONTAINER_SIZE (248)

enum avr_opcodes {
	__AVR_OP_NOP = 0,	/* v2 NOP command */
	/* version handlers */
	AVR_OP_VERSION = 1,
	AVR_OP_SPI_VERSION = 2,	/* v2.0+ command Get protocol version */
	AVR_OP_SYSINFO = 3,
	/* Ping handlers */
	AVR_OP_PING = 4,
	AVR_OP_PING_DELAY = 5,
	/* i2c handlers */
	AVR_OP_I2C_READ = 9,
	AVR_OP_I2C_WRITE = 10,
	AVR_OP_I2C_RANDOM_READ = 11,
	/* VPD handlers */
	AVR_OP_VPD_READ = 19,
	AVR_OP_VPD_WRITE = 20,
	/* SENSOR handlers */
	AVR_OP_SENSOR_FETCH = 28,
	/* The following command are only relevant to V3 */
	AVR_OP_SENSOR_MON_CONTROL = 42,
	AVR_OP_SENSOR_MON_SETUP = 43,
	/* special version handler */
	AVR_OP_SYSINFO_2 = 62,
};

#define GEN2_AVR_IDENT_SIZE (20)
#define GEN2_AVR_VERSION_SIZE (50)

#define GEN2_PN_SIZE (13)
#define GEN2_PBA_SIZE (16)
#define GEN2_SN_SIZE (10)
#define GEN2_BNAME_SIZE (14)
#define GEN2_PLATFORM_SIZE (72)
#define GEN2_VPD_SIZE_TOTAL                                                                       \
	(1 + GEN2_PN_SIZE + GEN2_PBA_SIZE + GEN2_SN_SIZE + GEN2_BNAME_SIZE + GEN2_PLATFORM_SIZE + \
	 2)

typedef struct vpd_eeprom_s {
	uint8_t psu_hw_version;	/* Hw revision - MUST NEVER ne overwritten. */
	/* Vital Product Data: P/N   (13bytes ascii 0-9) */
	uint8_t vpd_pn[GEN2_PN_SIZE];
	/* Vital Product Data: PBA   (16bytes ascii 0-9) */
	uint8_t vpd_pba[GEN2_PBA_SIZE];
	/* Vital Product Data: S/N   (10bytes ascii 0-9) */
	uint8_t vpd_sn[GEN2_SN_SIZE];
	/* Vital Product Data: Board Name (10bytes ascii) (e.g. "ntmainb1e2" or "ntfront20b1") */
	uint8_t vpd_board_name[GEN2_BNAME_SIZE];
	/*
	 * Vital Product Data: Other (72bytes of MAC addresses or other stuff.. (gives up to 12 mac
	 * addresses)
	 */
	uint8_t vpd_platform_section[GEN2_PLATFORM_SIZE];
	/* CRC16 checksum of all of above. This field is not included in the checksum */
	uint16_t crc16;
} vpd_eeprom_t;

typedef struct {
	uint8_t psu_hw_revision;
	char board_type[GEN2_BNAME_SIZE + 1];
	char product_id[GEN2_PN_SIZE + 1];
	char pba_id[GEN2_PBA_SIZE + 1];
	char serial_number[GEN2_SN_SIZE + 1];
	uint8_t product_family;
	uint32_t feature_mask;
	uint32_t invfeature_mask;
	uint8_t no_of_macs;
	uint8_t mac_address[6];
	uint16_t custom_id;
	uint8_t user_id[8];
} board_info_t;

struct tx_rx_buf {
	uint16_t size;
	void *p_buf;
};

struct nthw_spi_v3 {
	int m_time_out;
	int mn_instance_no;
	nthw_spim_t *mp_spim_mod;
	nthw_spis_t *mp_spis_mod;
};

typedef struct nthw_spi_v3 nthw_spi_v3_t;
typedef struct nthw_spi_v3 nthw_spi_v3;

nthw_spi_v3_t *nthw_spi_v3_new(void);
int nthw_spi_v3_init(nthw_spi_v3_t *p, nthw_fpga_t *p_fpga, int n_instance_no);

int nthw_spi_v3_transfer(nthw_spi_v3_t *p, uint16_t opcode, struct tx_rx_buf *tx_buf,
	struct tx_rx_buf *rx_buf);

#endif	/* __NT4GA_SPI_V3__ */
