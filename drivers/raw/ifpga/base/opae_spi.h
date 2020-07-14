/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#ifndef _OPAE_SPI_H
#define _OPAE_SPI_H

#include "opae_osdep.h"

#define ALTERA_SPI_RXDATA	0
#define ALTERA_SPI_TXDATA	4
#define ALTERA_SPI_STATUS	8
#define ALTERA_SPI_CONTROL	12
#define ALTERA_SPI_SLAVE_SEL	20

#define ALTERA_SPI_STATUS_ROE_MSK	0x8
#define ALTERA_SPI_STATUS_TOE_MSK	0x10
#define ALTERA_SPI_STATUS_TMT_MSK	0x20
#define ALTERA_SPI_STATUS_TRDY_MSK	0x40
#define ALTERA_SPI_STATUS_RRDY_MSK	0x80
#define ALTERA_SPI_STATUS_E_MSK		0x100

#define ALTERA_SPI_CONTROL_IROE_MSK	0x8
#define ALTERA_SPI_CONTROL_ITOE_MSK	0x10
#define ALTERA_SPI_CONTROL_ITRDY_MSK	0x40
#define ALTERA_SPI_CONTROL_IRRDY_MSK	0x80
#define ALTERA_SPI_CONTROL_IE_MSK	0x100
#define ALTERA_SPI_CONTROL_SSO_MSK	0x400

#define SPI_CORE_PARAM 0x8
#define SPI_CTRL 0x10
#define CTRL_R    BIT_ULL(9)
#define CTRL_W    BIT_ULL(8)
#define CTRL_ADDR_MASK GENMASK_ULL(2, 0)
#define SPI_READ 0x18
#define READ_DATA_VALID BIT_ULL(32)
#define READ_DATA_MASK GENMASK_ULL(31, 0)
#define SPI_WRITE 0x20
#define WRITE_DATA_MASK GENMASK_ULL(31, 0)

#define SPI_MAX_RETRY 1000000

#define TYPE_SPI 0
#define TYPE_NIOS_SPI 1

struct spi_core_param {
	union {
		u64 info;
		struct {
			u8 type:1;
			u8 endian:1;
			u8 data_width:6;
			u8 num_chipselect:6;
			u8 clock_polarity:1;
			u8 clock_phase:1;
			u8 stages:2;
			u8 resvd:4;
			u16 clock:10;
			u16 peripheral_id:16;
			u8 controller_type:1;
			u16 resvd1:15;
		};
	};
};

struct altera_spi_device {
	u8 *regs;
	struct spi_core_param spi_param;
	int data_width; /* how many bytes for data width */
	int endian;
	#define SPI_BIG_ENDIAN  0
	#define SPI_LITTLE_ENDIAN 1
	int num_chipselect;
	unsigned char *rxbuf;
	unsigned char *txbuf;
	unsigned int len;
	int (*reg_read)(struct altera_spi_device *dev, u32 reg, u32 *val);
	int (*reg_write)(struct altera_spi_device *dev, u32 reg,
			u32 value);
};

#define HEADER_LEN 8
#define RESPONSE_LEN 4
#define SPI_TRANSACTION_MAX_LEN 1024
#define TRAN_SEND_MAX_LEN (SPI_TRANSACTION_MAX_LEN + HEADER_LEN)
#define TRAN_RESP_MAX_LEN SPI_TRANSACTION_MAX_LEN
#define PACKET_SEND_MAX_LEN (2*TRAN_SEND_MAX_LEN + 4)
#define PACKET_RESP_MAX_LEN (2*TRAN_RESP_MAX_LEN + 4)
#define BYTES_SEND_MAX_LEN  (2*PACKET_SEND_MAX_LEN)
#define BYTES_RESP_MAX_LEN (2*PACKET_RESP_MAX_LEN)

struct spi_tran_buffer {
	unsigned char tran_send[TRAN_SEND_MAX_LEN];
	unsigned char tran_resp[TRAN_RESP_MAX_LEN];
	unsigned char packet_send[PACKET_SEND_MAX_LEN];
	unsigned char packet_resp[PACKET_RESP_MAX_LEN];
	unsigned char bytes_send[BYTES_SEND_MAX_LEN];
	unsigned char bytes_resp[2*BYTES_RESP_MAX_LEN];
};

struct spi_transaction_dev {
	struct altera_spi_device *dev;
	int chipselect;
	struct spi_tran_buffer *buffer;
	pthread_mutex_t lock;
};

struct spi_tran_header {
	u8 trans_type;
	u8 reserve;
	u16 size;
	u32 addr;
};

int spi_command(struct altera_spi_device *dev, unsigned int chip_select,
		unsigned int wlen, void *wdata, unsigned int rlen, void *rdata);
void spi_cs_deactivate(struct altera_spi_device *dev);
void spi_cs_activate(struct altera_spi_device *dev, unsigned int chip_select);
struct altera_spi_device *altera_spi_alloc(void *base, int type);
void altera_spi_init(struct altera_spi_device *dev);
void altera_spi_release(struct altera_spi_device *dev);
int spi_transaction_read(struct spi_transaction_dev *dev, unsigned int addr,
		unsigned int size, unsigned char *data);
int spi_transaction_write(struct spi_transaction_dev *dev, unsigned int addr,
		unsigned int size, unsigned char *data);
struct spi_transaction_dev *spi_transaction_init(struct altera_spi_device *dev,
		int chipselect);
void spi_transaction_remove(struct spi_transaction_dev *dev);
int spi_reg_write(struct altera_spi_device *dev, u32 reg,
		u32 value);
int spi_reg_read(struct altera_spi_device *dev, u32 reg, u32 *val);

#define NIOS_SPI_PARAM 0x8
#define CONTROL_TYPE BIT_ULL(48)
#define PERI_ID GENMASK_ULL(47, 32)
#define SPI_CLK GENMASK_ULL(31, 22)
#define SYNC_STAGES GENMASK_ULL(17, 16)
#define CLOCK_PHASE BIT_ULL(15)
#define CLOCK_POLARITY BIT_ULL(14)
#define NUM_SELECT  GENMASK_ULL(13, 8)
#define DATA_WIDTH GENMASK_ULL(7, 2)
#define SHIFT_DIRECTION BIT_ULL(1)
#define SPI_TYPE  BIT_ULL(0)
#define NIOS_SPI_CTRL 0x10
#define NIOS_SPI_RD (0x1ULL << 62)
#define NIOS_SPI_WR (0x2ULL << 62)
#define NIOS_SPI_COMMAND GENMASK_ULL(63, 62)
#define NIOS_SPI_ADDR  GENMASK_ULL(44, 32)
#define NIOS_SPI_WRITE_DATA  GENMASK_ULL(31, 0)
#define NIOS_SPI_STAT 0x18
#define NIOS_SPI_VALID BIT_ULL(32)
#define NIOS_SPI_READ_DATA GENMASK_ULL(31, 0)

#define NIOS_INIT		0x1000
#define REQ_FEC_MODE		GENMASK(23, 8)
#define REQ_FEC_MODE_SHIFT      8
#define FEC_MODE_NO		0x0
#define FEC_MODE_KR		0x5555
#define FEC_MODE_RS		0xaaaa
#define NIOS_INIT_START		BIT(1)
#define NIOS_INIT_DONE		BIT(0)
#define NIOS_VERSION		0x1004
#define NIOS_VERSION_MAJOR_SHIFT 28
#define NIOS_VERSION_MAJOR	GENMASK(31, 28)
#define NIOS_VERSION_MINOR	GENMASK(27, 24)
#define NIOS_VERSION_PATCH	GENMASK(23, 20)
#define PKVL_A_MODE_STS		0x1020
#define PKVL_B_MODE_STS		0x1024
#endif
