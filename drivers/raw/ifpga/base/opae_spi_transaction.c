/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#include "opae_spi.h"
#include "ifpga_compat.h"

/*transaction opcodes*/
#define SPI_TRAN_SEQ_WRITE 0x04 /* SPI transaction sequential write */
#define SPI_TRAN_SEQ_READ  0x14 /* SPI transaction sequential read */
#define SPI_TRAN_NON_SEQ_WRITE 0x00 /* SPI transaction non-sequential write */
#define SPI_TRAN_NON_SEQ_READ  0x10 /* SPI transaction non-sequential read*/

/*specail packet characters*/
#define SPI_PACKET_SOP     0x7a
#define SPI_PACKET_EOP     0x7b
#define SPI_PACKET_CHANNEL 0x7c
#define SPI_PACKET_ESC     0x7d

/*special byte characters*/
#define SPI_BYTE_IDLE 0x4a
#define SPI_BYTE_ESC  0x4d

#define SPI_REG_BYTES 4

#define INIT_SPI_TRAN_HEADER(trans_type, size, address) \
({ \
	header.trans_type = trans_type; \
	header.reserve = 0; \
	header.size = cpu_to_be16(size); \
	header.addr = cpu_to_be32(addr); \
})

#ifdef OPAE_SPI_DEBUG
static void print_buffer(const char *string, void *buffer, int len)
{
	int i;
	unsigned char *p = buffer;

	printf("%s print buffer, len=%d\n", string, len);

	for (i = 0; i < len; i++)
		printf("%02x ", *(p+i));
	printf("\n");
}
#else
static void print_buffer(const char *string, void *buffer, int len)
{
	UNUSED(string);
	UNUSED(buffer);
	UNUSED(len);
}
#endif

static unsigned char xor_20(unsigned char val)
{
	return val^0x20;
}

static void reorder_phy_data(u8 bits_per_word,
		void *buf, unsigned int len)
{
	unsigned int count = len / (bits_per_word/8);
	u32 *p;

	if (bits_per_word == 32) {
		p = (u32 *)buf;
		while (count--) {
			*p = cpu_to_be32(*p);
			p++;
		}
	}
}

static void phy_tx_pad(unsigned char *phy_buf, unsigned int phy_buf_len,
		unsigned int *aligned_len)
{
	unsigned char *p = &phy_buf[phy_buf_len - 1], *dst_p;

	*aligned_len = IFPGA_ALIGN(phy_buf_len, 4);

	if (*aligned_len == phy_buf_len)
		return;

	dst_p = &phy_buf[*aligned_len - 1];

	/* move EOP and bytes after EOP to the end of aligned size */
	while (p > phy_buf) {
		*dst_p = *p;

		if (*p == SPI_PACKET_EOP)
			break;

		p--;
		dst_p--;
	}

	/* fill the hole with PHY_IDLE */
	while (p < dst_p)
		*p++ = SPI_BYTE_IDLE;
}

#define RX_ALL_IDLE_DATA (SPI_BYTE_IDLE << 24 | SPI_BYTE_IDLE << 16 |	\
			 SPI_BYTE_IDLE << 8 | SPI_BYTE_IDLE)

static bool all_idle_data(u8 *rxbuf)
{
	return *(u32 *)rxbuf == RX_ALL_IDLE_DATA;
}

static unsigned char *find_eop(u8 *rxbuf, u32 BPW)
{
	return memchr(rxbuf, SPI_PACKET_EOP, BPW);
}

static int do_spi_txrx(struct spi_transaction_dev *dev,
		unsigned char *tx_buffer,
		unsigned int tx_len, unsigned char *rx_buffer,
		unsigned int rx_len,
		unsigned int *actual_rx)
{
	unsigned int rx_cnt = 0;
	int ret = 0;
	unsigned int BPW = 4;
	bool eop_found = false;
	unsigned char *eop;
	unsigned char *ptr;
	unsigned char *rxbuf = rx_buffer;
	int add_byte = 0;
	unsigned long ticks;
	unsigned long timeout;

	/* send command */
	ret = spi_write(dev->dev, dev->chipselect, tx_len, tx_buffer);
	if (ret)
		return -EBUSY;

	timeout = rte_get_timer_cycles() +
				msecs_to_timer_cycles(2000);

	/* read out data */
	while (rx_cnt < rx_len) {
		ret = spi_read(dev->dev, dev->chipselect, BPW, rxbuf);
		if (ret)
			return -EBUSY;

		/* skip all of invalid data */
		if (!eop_found && all_idle_data(rxbuf)) {
			ticks = rte_get_timer_cycles();
			if (!time_after(ticks, timeout)) {
				continue;
			} else {
				dev_err(dev, "read spi data timeout\n");
				return -ETIMEDOUT;
			}
		}

		rx_cnt += BPW;
		if (!eop_found) {
			/* EOP is found, we read 2 more bytes and exit. */
			eop = find_eop(rxbuf, BPW);
			if (eop) {
				if ((BPW + rxbuf - eop) > 2) {
					/*
					 * check if the last 2 bytes are already
					 * received in current word.
					 */
					break;
				} else if ((BPW + rxbuf - eop) == 2) {
					/*
					 * skip if last byte is not SPI_BYTE_ESC
					 * or SPI_PACKET_ESC. this is the valid
					 * end of a response too.
					 */
					ptr = eop + 1;

					if (*ptr != SPI_BYTE_ESC &&
							*ptr != SPI_PACKET_ESC)
						break;

					add_byte = 1;
				} else {
					add_byte = 2;
				}

				rx_len = min(rx_len,
						IFPGA_ALIGN(rx_cnt +
							add_byte, BPW));
				eop_found = true;
			}
		}
		rxbuf += BPW;
	}

	*actual_rx = rx_cnt;
	print_buffer("found valid data:", rx_buffer, rx_cnt);

	return ret;
}

static int byte_to_core_convert(struct spi_transaction_dev *dev,
		unsigned int send_len, unsigned char *send_data,
		unsigned int resp_len, unsigned char *resp_data,
		unsigned int *valid_resp_len)
{
	unsigned int i;
	int ret = 0;
	unsigned char *send_packet = dev->buffer->bytes_send;
	unsigned char *resp_packet = dev->buffer->bytes_resp;
	unsigned char *p;
	unsigned char current_byte;
	unsigned int tx_len = 0;
	unsigned int resp_max_len = 2 * resp_len;
	unsigned int actual_rx;

	print_buffer("before bytes:", send_data, send_len);

	p = send_packet;

	for (i = 0; i < send_len; i++) {
		current_byte = send_data[i];
		switch (current_byte) {
		case SPI_BYTE_IDLE:
			*p++ = SPI_BYTE_ESC;
			*p++ = xor_20(current_byte);
			break;
		case SPI_BYTE_ESC:
			*p++ = SPI_BYTE_ESC;
			*p++ = xor_20(current_byte);
			break;
		default:
			*p++ = current_byte;
			break;
		}
	}

	tx_len = p - send_packet;

	print_buffer("before spi:", send_packet, tx_len);

	phy_tx_pad(send_packet, tx_len, &tx_len);
	print_buffer("after pad:", send_packet, tx_len);

	reorder_phy_data(32, send_packet, tx_len);

	print_buffer("after order to spi:", send_packet, tx_len);

	ret = do_spi_txrx(dev, send_packet, tx_len, resp_packet,
			resp_max_len, &actual_rx);
	if (ret)
		return ret;

	/* analyze response packet */
	i = 0;
	p = resp_data;
	while (i < actual_rx) {
		current_byte = resp_packet[i];
		switch (current_byte) {
		case SPI_BYTE_IDLE:
			i++;
			break;
		case SPI_BYTE_ESC:
			i++;
			current_byte = resp_packet[i];
			*p++ = xor_20(current_byte);
			i++;
			break;
		default:
			*p++ = current_byte;
			i++;
			break;
		}
	}

	/* receive "4a" means the SPI is idle, not valid data */
	*valid_resp_len = p - resp_data;
	if (*valid_resp_len == 0) {
		dev_err(NULL, "error: repond package without valid data\n");
		return -EINVAL;
	}

	return 0;
}

static int packet_to_byte_conver(struct spi_transaction_dev *dev,
		unsigned int send_len, unsigned char *send_buf,
		unsigned int resp_len, unsigned char *resp_buf,
		unsigned int *valid)
{
	int ret = 0;
	unsigned int i;
	unsigned char current_byte;
	unsigned int resp_max_len;
	unsigned char *send_packet = dev->buffer->packet_send;
	unsigned char *resp_packet = dev->buffer->packet_resp;
	unsigned char *p;
	unsigned int valid_resp_len = 0;

	print_buffer("before packet:", send_buf, send_len);

	resp_max_len = 2 * resp_len + 4;

	p = send_packet;

	/* SOP header */
	*p++ = SPI_PACKET_SOP;

	*p++ = SPI_PACKET_CHANNEL;
	*p++ = 0;

	/* append the data into a packet */
	for (i = 0; i < send_len; i++) {
		current_byte = send_buf[i];

		/* EOP for last byte */
		if (i == send_len - 1)
			*p++ = SPI_PACKET_EOP;

		switch (current_byte) {
		case SPI_PACKET_SOP:
		case SPI_PACKET_EOP:
		case SPI_PACKET_CHANNEL:
		case SPI_PACKET_ESC:
			*p++ = SPI_PACKET_ESC;
			*p++ = xor_20(current_byte);
			break;
		default:
			*p++ = current_byte;
		}
	}

	ret = byte_to_core_convert(dev, p - send_packet,
			send_packet, resp_max_len, resp_packet,
			&valid_resp_len);
	if (ret)
		return -EBUSY;

	print_buffer("after byte conver:", resp_packet, valid_resp_len);

	/* analyze the response packet */
	p = resp_buf;

	/* look for SOP */
	for (i = 0; i < valid_resp_len; i++) {
		if (resp_packet[i] == SPI_PACKET_SOP)
			break;
	}

	if (i == valid_resp_len) {
		dev_err(NULL, "error on analyze response packet 0x%x\n",
				resp_packet[i]);
		return -EINVAL;
	}

	i++;

	/* continue parsing data after SOP */
	while (i < valid_resp_len) {
		current_byte = resp_packet[i];

		switch (current_byte) {
		case SPI_PACKET_SOP:
			dev_err(dev, "error on get SOP after SOP\n");
			return -EINVAL;
		case SPI_PACKET_CHANNEL:
			i += 2;
			break;
		case SPI_PACKET_ESC:
			i++;
			current_byte = resp_packet[i];
			*p++ = xor_20(current_byte);
			i++;
			break;
		case SPI_PACKET_EOP:
			i++;
			current_byte = resp_packet[i];
			switch (current_byte) {
			case SPI_PACKET_ESC:
				i++;
				current_byte = resp_packet[i];
				*p++ = xor_20(current_byte);
				break;
			case SPI_PACKET_CHANNEL:
			case SPI_PACKET_SOP:
			case SPI_PACKET_EOP:
				dev_err(dev, "error get SOP/EOP after EOP\n");
				return -EINVAL;
			default:
				*p++ = current_byte;
				break;
			}
			goto done;

		default:
			*p++ = current_byte;
			i++;
		}
	}

done:
	*valid = p - resp_buf;

	print_buffer("after packet:", resp_buf, *valid);

	return ret;
}

static int do_transaction(struct spi_transaction_dev *dev, unsigned int addr,
		unsigned int size, unsigned char *data,
		unsigned int trans_type)
{

	struct spi_tran_header header;
	unsigned char *transaction = dev->buffer->tran_send;
	unsigned char *response = dev->buffer->tran_resp;
	unsigned char *p;
	int ret = 0;
	unsigned int i;
	unsigned int valid_len = 0;

	/* make transacation header */
	INIT_SPI_TRAN_HEADER(trans_type, size, addr);

	/* fill the header */
	p = transaction;
	opae_memcpy(p, &header, sizeof(struct spi_tran_header));
	p = p + sizeof(struct spi_tran_header);

	switch (trans_type) {
	case SPI_TRAN_SEQ_WRITE:
	case SPI_TRAN_NON_SEQ_WRITE:
		for (i = 0; i < size; i++)
			*p++ = *data++;

		ret = packet_to_byte_conver(dev, size + HEADER_LEN,
				transaction, RESPONSE_LEN, response,
				&valid_len);
		if (ret)
			return -EBUSY;

		/* check the result */
		if (size != ((unsigned int)(response[2] & 0xff) << 8 |
			(unsigned int)(response[3] & 0xff)))
			ret = -EBUSY;

		break;
	case SPI_TRAN_SEQ_READ:
	case SPI_TRAN_NON_SEQ_READ:
		ret = packet_to_byte_conver(dev, HEADER_LEN,
				transaction, size, response,
				&valid_len);
		if (ret || valid_len != size)
			return -EBUSY;

		for (i = 0; i < size; i++)
			*data++ = *response++;

		ret = 0;
		break;
	}

	return ret;
}

int spi_transaction_read(struct spi_transaction_dev *dev, unsigned int addr,
		unsigned int size, unsigned char *data)
{
	int ret;

	pthread_mutex_lock(dev->mutex);
	ret = do_transaction(dev, addr, size, data,
			(size > SPI_REG_BYTES) ?
			SPI_TRAN_SEQ_READ : SPI_TRAN_NON_SEQ_READ);
	pthread_mutex_unlock(dev->mutex);

	return ret;
}

int spi_transaction_write(struct spi_transaction_dev *dev, unsigned int addr,
		unsigned int size, unsigned char *data)
{
	int ret;

	pthread_mutex_lock(dev->mutex);
	ret = do_transaction(dev, addr, size, data,
			(size > SPI_REG_BYTES) ?
			SPI_TRAN_SEQ_WRITE : SPI_TRAN_NON_SEQ_WRITE);
	pthread_mutex_unlock(dev->mutex);

	return ret;
}

struct spi_transaction_dev *spi_transaction_init(struct altera_spi_device *dev,
		int chipselect)
{
	struct spi_transaction_dev *spi_tran_dev;
	int ret;

	spi_tran_dev = opae_malloc(sizeof(struct spi_transaction_dev));
	if (!spi_tran_dev)
		return NULL;

	spi_tran_dev->dev = dev;
	spi_tran_dev->chipselect = chipselect;

	spi_tran_dev->buffer = opae_malloc(sizeof(struct spi_tran_buffer));
	if (!spi_tran_dev->buffer)
		goto err;

	ret = pthread_mutex_init(&spi_tran_dev->lock, NULL);
	if (ret) {
		dev_err(spi_tran_dev, "fail to init mutex lock\n");
		goto err;
	}
	if (dev->mutex) {
		dev_info(NULL, "use multi-process mutex in spi\n");
		spi_tran_dev->mutex = dev->mutex;
	} else {
		dev_info(NULL, "use multi-thread mutex in spi\n");
		spi_tran_dev->mutex = &spi_tran_dev->lock;
	}

	return spi_tran_dev;

err:
	opae_free(spi_tran_dev);
	return NULL;
}

void spi_transaction_remove(struct spi_transaction_dev *dev)
{
	if (dev && dev->buffer)
		opae_free(dev->buffer);
	if (dev) {
		pthread_mutex_destroy(&dev->lock);
		opae_free(dev);
	}
}
