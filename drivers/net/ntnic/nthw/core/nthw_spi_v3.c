/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_fpga.h"

#include "nthw_spi_v3.h"

#include <arpa/inet.h>

#undef SPI_V3_DEBUG_PRINT

nthw_spi_v3_t *nthw_spi_v3_new(void)
{
	nthw_spi_v3_t *p = malloc(sizeof(nthw_spi_v3_t));

	if (p)
		memset(p, 0, sizeof(nthw_spi_v3_t));

	return p;
}

static int nthw_spi_v3_set_timeout(nthw_spi_v3_t *p, int time_out)
{
	p->m_time_out = time_out;
	return 0;
}

/*
 * Wait until Tx data have been sent after they have been placed in the Tx FIFO.
 */
static int wait_for_tx_data_sent(nthw_spim_t *p_spim_mod, uint64_t time_out)
{
	int result;
	bool empty;
	uint64_t start_time;
	uint64_t cur_time;
	start_time = nthw_os_get_time_monotonic_counter();

	while (true) {
		nthw_os_wait_usec(1000);	/* Every 1ms */

		result = nthw_spim_get_tx_fifo_empty(p_spim_mod, &empty);

		if (result != 0) {
			NT_LOG(WRN, NTHW, "nthw_spim_get_tx_fifo_empty failed");
			return result;
		}

		if (empty)
			break;

		cur_time = nthw_os_get_time_monotonic_counter();

		if ((cur_time - start_time) > time_out) {
			NT_LOG(WRN, NTHW, "%s: Timed out", __func__);
			return -1;
		}
	}

	return 0;
}

/*
 * Wait until Rx data have been received.
 */
static int wait_for_rx_data_ready(nthw_spis_t *p_spis_mod, uint64_t time_out)
{
	int result;
	bool empty;
	uint64_t start_time;
	uint64_t cur_time;
	start_time = nthw_os_get_time_monotonic_counter();

	/* Wait for data to become ready in the Rx FIFO */
	while (true) {
		nthw_os_wait_usec(10000);	/* Every 10ms */

		result = nthw_spis_get_rx_fifo_empty(p_spis_mod, &empty);

		if (result != 0) {
			NT_LOG(WRN, NTHW, "nthw_spis_get_rx_empty failed");
			return result;
		}

		if (!empty)
			break;

		cur_time = nthw_os_get_time_monotonic_counter();

		if ((cur_time - start_time) > time_out) {
			NT_LOG(WRN, NTHW, "%s: Timed out", __func__);
			return -1;
		}
	}

	return 0;
}

#ifdef SPI_V3_DEBUG_PRINT
static void dump_hex(uint8_t *p_data, uint16_t count)
{
	int i;
	int j = 0;
	char tmp_str[128];

	for (i = 0; i < count; i++) {
		sprintf(&tmp_str[j * 3], "%02X ", *(p_data++));
		j++;

		if (j == 16 || i == count - 1) {
			tmp_str[j * 3 - 1] = '\0';
			NT_LOG(DBG, NTHW, "    %s", tmp_str);
			j = 0;
		}
	}
}

#endif

int nthw_spi_v3_init(nthw_spi_v3_t *p, nthw_fpga_t *p_fpga, int n_instance_no)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	int result;

	p->mn_instance_no = n_instance_no;

	nthw_spi_v3_set_timeout(p, 1);

	/* Initialize SPIM module */
	p->mp_spim_mod = nthw_spim_new();

	result = nthw_spim_init(p->mp_spim_mod, p_fpga, n_instance_no);

	if (result != 0)
		NT_LOG(ERR, NTHW, "%s: nthw_spis_init failed: %d", p_adapter_id_str, result);

	/* Initialize SPIS module */
	p->mp_spis_mod = nthw_spis_new();

	result = nthw_spis_init(p->mp_spis_mod, p_fpga, n_instance_no);

	if (result != 0)
		NT_LOG(ERR, NTHW, "%s: nthw_spim_init failed: %d", p_adapter_id_str, result);

	/* Reset SPIM and SPIS modules */
	result = nthw_spim_reset(p->mp_spim_mod);

	if (result != 0)
		NT_LOG(ERR, NTHW, "%s: nthw_spim_reset failed: %d", p_adapter_id_str, result);

	result = nthw_spis_reset(p->mp_spis_mod);

	if (result != 0)
		NT_LOG(ERR, NTHW, "%s: nthw_spis_reset failed: %d", p_adapter_id_str, result);

	return result;
}

/*
 * Send Tx data using the SPIM module and receive any data using the SPIS module.
 * The data are sent and received being wrapped into a SPI v3 container.
 */
int nthw_spi_v3_transfer(nthw_spi_v3_t *p, uint16_t opcode, struct tx_rx_buf *tx_buf,
	struct tx_rx_buf *rx_buf)
{
	const uint16_t max_payload_rx_size = rx_buf->size;
	int result = 0;

	union __rte_packed_begin {
		uint32_t raw;

		struct {
			uint16_t opcode;
			uint16_t size;
		};
	} __rte_packed_end spi_tx_hdr;

	union __rte_packed_begin {
		uint32_t raw;

		struct {
			uint16_t error_code;
			uint16_t size;
		};
	} __rte_packed_end spi_rx_hdr;

#ifdef SPI_V3_DEBUG_PRINT
	NT_LOG_DBG(DBG, NTHW, "Started");
#endif

	/* Disable transmission from Tx FIFO */
	result = nthw_spim_enable(p->mp_spim_mod, false);

	if (result != 0) {
		NT_LOG(WRN, NTHW, "nthw_spim_enable failed");
		return result;
	}

	/* Enable SPIS module */
	result = nthw_spis_enable(p->mp_spis_mod, true);

	if (result != 0) {
		NT_LOG(WRN, NTHW, "nthw_spis_enable failed");
		return result;
	}

	/* Put data into Tx FIFO */
	spi_tx_hdr.opcode = opcode;
	spi_tx_hdr.size = tx_buf->size;

#ifdef SPI_V3_DEBUG_PRINT
	NT_LOG(DBG, NTHW, "Opcode=0x%04X tx_bufSize=0x%04X rx_bufSize=0x%04X", opcode,
		tx_buf->size, rx_buf->size);

#endif	/* SPI_V3_DEBUG_PRINT */

	result = nthw_spim_write_tx_fifo(p->mp_spim_mod, htonl(spi_tx_hdr.raw));

	if (result != 0) {
		NT_LOG(WRN, NTHW, "nthw_spim_write_tx_fifo failed");
		return result;
	}

	{
		uint8_t *tx_data = (uint8_t *)tx_buf->p_buf;
		uint16_t tx_size = tx_buf->size;
		uint16_t count;
		uint32_t value;

		while (tx_size > 0) {
			if (tx_size > 4) {
				count = 4;

			} else {
				count = tx_size;
				value = 0;
			}

			memcpy(&value, tx_data, count);

			result = nthw_spim_write_tx_fifo(p->mp_spim_mod, htonl(value));

			if (result != 0) {
				NT_LOG(WRN, NTHW, "nthw_spim_write_tx_fifo failed");
				return result;
			}

			tx_size = (uint16_t)(tx_size - count);
			tx_data += count;
		}
	}

	/* Enable Tx FIFO */
	result = nthw_spim_enable(p->mp_spim_mod, true);

	if (result != 0) {
		NT_LOG(WRN, NTHW, "nthw_spim_enable failed");
		return result;
	}

	result = wait_for_tx_data_sent(p->mp_spim_mod, p->m_time_out);

	if (result != 0)
		return result;

#ifdef SPI_V3_DEBUG_PRINT
	NT_LOG(DBG, NTHW, "%s: SPI header and payload data have been sent", __func__);
#endif

	{
		/* Start receiving data */
		uint16_t rx_size =
			sizeof(spi_rx_hdr.raw);	/* The first data to read is the header */
		uint8_t *rx_data = (uint8_t *)rx_buf->p_buf;
		bool rx_hdr_read = false;

		rx_buf->size = 0;

		while (true) {
			uint16_t count;
			uint32_t value;

			if (!rx_hdr_read) {	/* Read the header */
				result = wait_for_rx_data_ready(p->mp_spis_mod, p->m_time_out);

				if (result != 0)
					return result;

				typeof(spi_rx_hdr.raw) raw;
				result = nthw_spis_read_rx_fifo(p->mp_spis_mod, &raw);
				spi_rx_hdr.raw = raw;

				if (result != 0) {
					NT_LOG(WRN, NTHW, "nthw_spis_read_rx_fifo failed");
					return result;
				}

				spi_rx_hdr.raw = ntohl(spi_rx_hdr.raw);
				rx_size = spi_rx_hdr.size;
				rx_hdr_read = true;	/* Next time read payload */

#ifdef SPI_V3_DEBUG_PRINT
				NT_LOG(DBG, NTHW,
					"  spi_rx_hdr.error_code = 0x%04X, spi_rx_hdr.size = 0x%04X",
					spi_rx_hdr.error_code, spi_rx_hdr.size);
#endif

				if (spi_rx_hdr.error_code != 0) {
					result = -1;	/* NT_ERROR_AVR_OPCODE_RETURNED_ERROR; */
					break;
				}

				if (rx_size > max_payload_rx_size) {
					result = 1;	/* NT_ERROR_AVR_RX_BUFFER_TOO_SMALL; */
					break;
				}

			} else {/* Read the payload */
				count = (uint16_t)(rx_size < 4U ? rx_size : 4U);

				if (count == 0)
					break;

				result = wait_for_rx_data_ready(p->mp_spis_mod, p->m_time_out);

				if (result != 0)
					return result;

				result = nthw_spis_read_rx_fifo(p->mp_spis_mod, &value);

				if (result != 0) {
					NT_LOG(WRN, NTHW, "nthw_spis_read_rx_fifo failed");
					return result;
				}

				value = ntohl(value);	/* Convert to host endian */
				memcpy(rx_data, &value, count);
				rx_buf->size = (uint16_t)(rx_buf->size + count);
				rx_size = (uint16_t)(rx_size - count);
				rx_data += count;
			}
		}
	}

#ifdef SPI_V3_DEBUG_PRINT
	NT_LOG(DBG, NTHW, "  RxData: %d", rx_buf->size);
	dump_hex(rx_buf->p_buf, rx_buf->size);
	NT_LOG(DBG, NTHW, "%s:  Ended: %d", __func__, result);
#endif

	return result;
}
