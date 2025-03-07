/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <unistd.h>

#include <eal_export.h>
#include <rte_common.h>
#include <rte_log.h>
#include <dev_driver.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_errno.h>
#include <rte_pci.h>
#include <bus_pci_driver.h>
#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_random.h>

#include <rte_bbdev.h>
#include <rte_bbdev_pmd.h>

#include "rte_pmd_fpga_5gnr_fec.h"
#include "fpga_5gnr_fec.h"

#ifdef RTE_LIBRTE_BBDEV_DEBUG
RTE_LOG_REGISTER_DEFAULT(fpga_5gnr_fec_logtype, DEBUG);
#else
RTE_LOG_REGISTER_DEFAULT(fpga_5gnr_fec_logtype, NOTICE);
#endif

#ifdef RTE_LIBRTE_BBDEV_DEBUG

/* Read Ring Control Register of FPGA 5GNR FEC device */
static inline void
print_ring_reg_debug_info(void *mmio_base, uint32_t offset)
{
	rte_bbdev_log_debug(
		"FPGA 5GNR MMIO base address @ %p | Ring Control Register @ offset = 0x%08"
		PRIx32, mmio_base, offset);
	rte_bbdev_log_debug(
		"RING_BASE_ADDR = 0x%016"PRIx64,
		fpga_5gnr_reg_read_64(mmio_base, offset));
	rte_bbdev_log_debug(
		"RING_HEAD_ADDR = 0x%016"PRIx64,
		fpga_5gnr_reg_read_64(mmio_base, offset +
				FPGA_5GNR_FEC_RING_HEAD_ADDR));
	rte_bbdev_log_debug(
		"RING_SIZE = 0x%04"PRIx16,
		fpga_5gnr_reg_read_16(mmio_base, offset +
				FPGA_5GNR_FEC_RING_SIZE));
	rte_bbdev_log_debug(
		"RING_MISC = 0x%02"PRIx8,
		fpga_5gnr_reg_read_8(mmio_base, offset +
				FPGA_5GNR_FEC_RING_MISC));
	rte_bbdev_log_debug(
		"RING_ENABLE = 0x%02"PRIx8,
		fpga_5gnr_reg_read_8(mmio_base, offset +
				FPGA_5GNR_FEC_RING_ENABLE));
	rte_bbdev_log_debug(
		"RING_FLUSH_QUEUE_EN = 0x%02"PRIx8,
		fpga_5gnr_reg_read_8(mmio_base, offset +
				FPGA_5GNR_FEC_RING_FLUSH_QUEUE_EN));
	rte_bbdev_log_debug(
		"RING_SHADOW_TAIL = 0x%04"PRIx16,
		fpga_5gnr_reg_read_16(mmio_base, offset +
				FPGA_5GNR_FEC_RING_SHADOW_TAIL));
	rte_bbdev_log_debug(
		"RING_HEAD_POINT = 0x%04"PRIx16,
		fpga_5gnr_reg_read_16(mmio_base, offset +
				FPGA_5GNR_FEC_RING_HEAD_POINT));
}

/* Read Static Register of Vista Creek device. */
static inline void
print_static_reg_debug_info(void *mmio_base, uint8_t fpga_variant)
{
	uint16_t config;
	uint8_t qmap_done = fpga_5gnr_reg_read_8(mmio_base, FPGA_5GNR_FEC_QUEUE_PF_VF_MAP_DONE);
	uint16_t lb_factor = fpga_5gnr_reg_read_16(mmio_base, FPGA_5GNR_FEC_LOAD_BALANCE_FACTOR);
	uint16_t ring_desc_len = fpga_5gnr_reg_read_16(mmio_base, FPGA_5GNR_FEC_RING_DESC_LEN);
	if (fpga_variant == VC_5GNR_FPGA_VARIANT)
		config = fpga_5gnr_reg_read_16(mmio_base, VC_5GNR_CONFIGURATION);

	if (fpga_variant == VC_5GNR_FPGA_VARIANT)
		rte_bbdev_log_debug("UL.DL Weights = %u.%u",
				((uint8_t)config), ((uint8_t)(config >> 8)));
	rte_bbdev_log_debug("UL.DL Load Balance = %u.%u",
			((uint8_t)lb_factor), ((uint8_t)(lb_factor >> 8)));
	rte_bbdev_log_debug("Queue-PF/VF Mapping Table = %s",
			(qmap_done > 0) ? "READY" : "NOT-READY");
	if (fpga_variant == VC_5GNR_FPGA_VARIANT)
		rte_bbdev_log_debug("Ring Descriptor Size = %u bytes",
				ring_desc_len * VC_5GNR_RING_DESC_LEN_UNIT_BYTES);
	else
		rte_bbdev_log_debug("Ring Descriptor Size = %u bytes",
				ring_desc_len * AGX100_RING_DESC_LEN_UNIT_BYTES);
}

/* Print decode DMA Descriptor of Vista Creek Decoder device. */
static void
vc_5gnr_print_dma_dec_desc_debug_info(union vc_5gnr_dma_desc *desc)
{
	rte_bbdev_log_debug("DMA response desc %p",
			desc);
	rte_bbdev_log_debug("\t-- done(%"PRIu32") | iter(%"PRIu32") | et_pass(%"PRIu32")"
			" | crcb_pass (%"PRIu32") | error(%"PRIu32")",
			(uint32_t)desc->dec_req.done,
			(uint32_t)desc->dec_req.iter,
			(uint32_t)desc->dec_req.et_pass,
			(uint32_t)desc->dec_req.crcb_pass,
			(uint32_t)desc->dec_req.error);
	rte_bbdev_log_debug("\t-- qm_idx(%"PRIu32") | max_iter(%"PRIu32") | "
			"bg_idx (%"PRIu32") | harqin_en(%"PRIu32") | zc(%"PRIu32")",
			(uint32_t)desc->dec_req.qm_idx,
			(uint32_t)desc->dec_req.max_iter,
			(uint32_t)desc->dec_req.bg_idx,
			(uint32_t)desc->dec_req.harqin_en,
			(uint32_t)desc->dec_req.zc);
	rte_bbdev_log_debug("\t-- hbstroe_offset(%"PRIu32") | num_null (%"PRIu32") "
			"| irq_en(%"PRIu32")",
			(uint32_t)desc->dec_req.hbstroe_offset,
			(uint32_t)desc->dec_req.num_null,
			(uint32_t)desc->dec_req.irq_en);
	rte_bbdev_log_debug("\t-- ncb(%"PRIu32") | desc_idx (%"PRIu32") | "
			"drop_crc24b(%"PRIu32") | RV (%"PRIu32")",
			(uint32_t)desc->dec_req.ncb,
			(uint32_t)desc->dec_req.desc_idx,
			(uint32_t)desc->dec_req.drop_crc24b,
			(uint32_t)desc->dec_req.rv);
	rte_bbdev_log_debug("\t-- crc24b_ind(%"PRIu32") | et_dis (%"PRIu32")",
			(uint32_t)desc->dec_req.crc24b_ind,
			(uint32_t)desc->dec_req.et_dis);
	rte_bbdev_log_debug("\t-- harq_input_length(%"PRIu32") | rm_e(%"PRIu32")",
			(uint32_t)desc->dec_req.harq_input_length,
			(uint32_t)desc->dec_req.rm_e);
	rte_bbdev_log_debug("\t-- cbs_in_op(%"PRIu32") | in_add (0x%08"PRIx32"%08"PRIx32")"
			"| out_add (0x%08"PRIx32"%08"PRIx32")",
			(uint32_t)desc->dec_req.cbs_in_op,
			(uint32_t)desc->dec_req.in_addr_hi,
			(uint32_t)desc->dec_req.in_addr_lw,
			(uint32_t)desc->dec_req.out_addr_hi,
			(uint32_t)desc->dec_req.out_addr_lw);
	uint32_t *word = (uint32_t *) desc;
	rte_bbdev_log_debug("%08"PRIx32", %08"PRIx32", %08"PRIx32", %08"PRIx32", "
			"%08"PRIx32", %08"PRIx32", %08"PRIx32", %08"PRIx32,
			word[0], word[1], word[2], word[3],
			word[4], word[5], word[6], word[7]);
}

/* Print decode DMA Descriptor of AGX100 Decoder device. */
static void
agx100_print_dma_dec_desc_debug_info(union agx100_dma_desc *desc)
{
	rte_bbdev_log_debug("DMA response desc %p",
			desc);
	rte_bbdev_log_debug("\t-- done(%"PRIu32") | tb_crc_pass(%"PRIu32") | cb_crc_all_pass(%"PRIu32")"
			" | cb_all_et_pass(%"PRIu32") | max_iter_ret(%"PRIu32") |"
			"cgb_crc_bitmap(%"PRIu32") | error_msg(%"PRIu32") | error_code(%"PRIu32") |"
			"et_dis (%"PRIu32") | harq_in_en(%"PRIu32") | max_iter(%"PRIu32")",
			(uint32_t)desc->dec_req.done,
			(uint32_t)desc->dec_req.tb_crc_pass,
			(uint32_t)desc->dec_req.cb_crc_all_pass,
			(uint32_t)desc->dec_req.cb_all_et_pass,
			(uint32_t)desc->dec_req.max_iter_ret,
			(uint32_t)desc->dec_req.cgb_crc_bitmap,
			(uint32_t)desc->dec_req.error_msg,
			(uint32_t)desc->dec_req.error_code,
			(uint32_t)desc->dec_req.et_dis,
			(uint32_t)desc->dec_req.harq_in_en,
			(uint32_t)desc->dec_req.max_iter);
	rte_bbdev_log_debug("\t-- ncb(%"PRIu32") | bg_idx (%"PRIu32") | qm_idx (%"PRIu32")"
			"| zc(%"PRIu32") | rv(%"PRIu32") | int_en(%"PRIu32")",
			(uint32_t)desc->dec_req.ncb,
			(uint32_t)desc->dec_req.bg_idx,
			(uint32_t)desc->dec_req.qm_idx,
			(uint32_t)desc->dec_req.zc,
			(uint32_t)desc->dec_req.rv,
			(uint32_t)desc->dec_req.int_en);
	rte_bbdev_log_debug("\t-- max_cbg(%"PRIu32") | cbgti(%"PRIu32") | cbgfi(%"PRIu32") |"
			"cbgs(%"PRIu32") | desc_idx(%"PRIu32")",
			(uint32_t)desc->dec_req.max_cbg,
			(uint32_t)desc->dec_req.cbgti,
			(uint32_t)desc->dec_req.cbgfi,
			(uint32_t)desc->dec_req.cbgs,
			(uint32_t)desc->dec_req.desc_idx);
	rte_bbdev_log_debug("\t-- ca(%"PRIu32") | c(%"PRIu32") | llr_pckg(%"PRIu32") |"
			"syndrome_check_mode(%"PRIu32") | num_null(%"PRIu32")",
			(uint32_t)desc->dec_req.ca,
			(uint32_t)desc->dec_req.c,
			(uint32_t)desc->dec_req.llr_pckg,
			(uint32_t)desc->dec_req.syndrome_check_mode,
			(uint32_t)desc->dec_req.num_null);
	rte_bbdev_log_debug("\t-- ea(%"PRIu32") | eba(%"PRIu32")",
			(uint32_t)desc->dec_req.ea,
			(uint32_t)desc->dec_req.eba);
	rte_bbdev_log_debug("\t-- hbstore_offset_out(%"PRIu32")",
			(uint32_t)desc->dec_req.hbstore_offset_out);
	rte_bbdev_log_debug("\t-- hbstore_offset_in(%"PRIu32") | en_slice_ts(%"PRIu32") |"
			"en_host_ts(%"PRIu32") | en_cb_wr_status(%"PRIu32")"
			" | en_output_sg(%"PRIu32") | en_input_sg(%"PRIu32") | tb_cb(%"PRIu32")"
			" | crc24b_ind(%"PRIu32")| drop_crc24b(%"PRIu32")",
			(uint32_t)desc->dec_req.hbstore_offset_in,
			(uint32_t)desc->dec_req.en_slice_ts,
			(uint32_t)desc->dec_req.en_host_ts,
			(uint32_t)desc->dec_req.en_cb_wr_status,
			(uint32_t)desc->dec_req.en_output_sg,
			(uint32_t)desc->dec_req.en_input_sg,
			(uint32_t)desc->dec_req.tb_cb,
			(uint32_t)desc->dec_req.crc24b_ind,
			(uint32_t)desc->dec_req.drop_crc24b);
	rte_bbdev_log_debug("\t-- harq_input_length_a(%"PRIu32") | harq_input_length_b(%"PRIu32")",
			(uint32_t)desc->dec_req.harq_input_length_a,
			(uint32_t)desc->dec_req.harq_input_length_b);
	rte_bbdev_log_debug("\t-- input_slice_table_addr_lo(%"PRIu32")"
			" | input_start_addr_lo(%"PRIu32")",
			(uint32_t)desc->dec_req.input_slice_table_addr_lo,
			(uint32_t)desc->dec_req.input_start_addr_lo);
	rte_bbdev_log_debug("\t-- input_slice_table_addr_hi(%"PRIu32")"
			" | input_start_addr_hi(%"PRIu32")",
			(uint32_t)desc->dec_req.input_slice_table_addr_hi,
			(uint32_t)desc->dec_req.input_start_addr_hi);
	rte_bbdev_log_debug("\t-- input_slice_num(%"PRIu32") | input_length(%"PRIu32")",
			(uint32_t)desc->dec_req.input_slice_num,
			(uint32_t)desc->dec_req.input_length);
	rte_bbdev_log_debug("\t-- output_slice_table_addr_lo(%"PRIu32")"
			" | output_start_addr_lo(%"PRIu32")",
			(uint32_t)desc->dec_req.output_slice_table_addr_lo,
			(uint32_t)desc->dec_req.output_start_addr_lo);
	rte_bbdev_log_debug("\t-- output_slice_table_addr_hi(%"PRIu32")"
			" | output_start_addr_hi(%"PRIu32")",
			(uint32_t)desc->dec_req.output_slice_table_addr_hi,
			(uint32_t)desc->dec_req.output_start_addr_hi);
	rte_bbdev_log_debug("\t-- output_slice_num(%"PRIu32") | output_length(%"PRIu32")",
			(uint32_t)desc->dec_req.output_slice_num,
			(uint32_t)desc->dec_req.output_length);
	rte_bbdev_log_debug("\t-- enqueue_timestamp(%"PRIu32")",
			(uint32_t)desc->dec_req.enqueue_timestamp);
	rte_bbdev_log_debug("\t-- completion_timestamp(%"PRIu32")",
			(uint32_t)desc->dec_req.completion_timestamp);

	uint32_t *word = (uint32_t *) desc;
	rte_bbdev_log_debug("%08"PRIx32", %08"PRIx32", %08"PRIx32", %08"PRIx32", "
			"%08"PRIx32", %08"PRIx32", %08"PRIx32", %08"PRIx32", "
			"%08"PRIx32", %08"PRIx32", %08"PRIx32", %08"PRIx32", "
			"%08"PRIx32", %08"PRIx32", %08"PRIx32", %08"PRIx32,
			word[0], word[1], word[2], word[3],
			word[4], word[5], word[6], word[7],
			word[8], word[9], word[10], word[11],
			word[12], word[13], word[14], word[15]);
}

/* Print decode DMA Descriptor of Vista Creek encoder device. */
static void
vc_5gnr_print_dma_enc_desc_debug_info(union vc_5gnr_dma_desc *desc)
{
	rte_bbdev_log_debug("DMA response desc %p",
			desc);
	rte_bbdev_log_debug("%"PRIu32" %"PRIu32,
			(uint32_t)desc->enc_req.done,
			(uint32_t)desc->enc_req.error);
	rte_bbdev_log_debug("K' %"PRIu32" E %"PRIu32" desc %"PRIu32" Z %"PRIu32,
			(uint32_t)desc->enc_req.k_,
			(uint32_t)desc->enc_req.rm_e,
			(uint32_t)desc->enc_req.desc_idx,
			(uint32_t)desc->enc_req.zc);
	rte_bbdev_log_debug("BG %"PRIu32" Qm %"PRIu32" CRC %"PRIu32" IRQ %"PRIu32,
			(uint32_t)desc->enc_req.bg_idx,
			(uint32_t)desc->enc_req.qm_idx,
			(uint32_t)desc->enc_req.crc_en,
			(uint32_t)desc->enc_req.irq_en);
	rte_bbdev_log_debug("k0 %"PRIu32" Ncb %"PRIu32" F %"PRIu32,
			(uint32_t)desc->enc_req.k0,
			(uint32_t)desc->enc_req.ncb,
			(uint32_t)desc->enc_req.num_null);
	uint32_t *word = (uint32_t *) desc;
	rte_bbdev_log_debug("%08"PRIx32", %08"PRIx32", %08"PRIx32", %08"PRIx32", "
			"%08"PRIx32", %08"PRIx32", %08"PRIx32", %08"PRIx32,
			word[0], word[1], word[2], word[3],
			word[4], word[5], word[6], word[7]);
}

/* Print decode DMA Descriptor of AGX100 encoder device. */
static void
agx100_print_dma_enc_desc_debug_info(union agx100_dma_desc *desc)
{
	rte_bbdev_log_debug("DMA response desc %p",
			desc);
	rte_bbdev_log_debug("\t-- done(%"PRIu32") | error_msg(%"PRIu32") | error_code(%"PRIu32")",
			(uint32_t)desc->enc_req.done,
			(uint32_t)desc->enc_req.error_msg,
			(uint32_t)desc->enc_req.error_code);
	rte_bbdev_log_debug("\t-- ncb(%"PRIu32") | bg_idx (%"PRIu32") | qm_idx (%"PRIu32")"
			"| zc(%"PRIu32") | rv(%"PRIu32") | int_en(%"PRIu32")",
			(uint32_t)desc->enc_req.ncb,
			(uint32_t)desc->enc_req.bg_idx,
			(uint32_t)desc->enc_req.qm_idx,
			(uint32_t)desc->enc_req.zc,
			(uint32_t)desc->enc_req.rv,
			(uint32_t)desc->enc_req.int_en);
	rte_bbdev_log_debug("\t-- max_cbg(%"PRIu32") | cbgti(%"PRIu32") | cbgs(%"PRIu32") | "
			"desc_idx(%"PRIu32")",
			(uint32_t)desc->enc_req.max_cbg,
			(uint32_t)desc->enc_req.cbgti,
			(uint32_t)desc->enc_req.cbgs,
			(uint32_t)desc->enc_req.desc_idx);
	rte_bbdev_log_debug("\t-- ca(%"PRIu32") | c(%"PRIu32") | num_null(%"PRIu32")",
			(uint32_t)desc->enc_req.ca,
			(uint32_t)desc->enc_req.c,
			(uint32_t)desc->enc_req.num_null);
	rte_bbdev_log_debug("\t-- ea(%"PRIu32")",
			(uint32_t)desc->enc_req.ea);
	rte_bbdev_log_debug("\t-- eb(%"PRIu32")",
			(uint32_t)desc->enc_req.eb);
	rte_bbdev_log_debug("\t-- k_(%"PRIu32") | en_slice_ts(%"PRIu32") | en_host_ts(%"PRIu32") | "
			"en_cb_wr_status(%"PRIu32") | en_output_sg(%"PRIu32") | "
			"en_input_sg(%"PRIu32") | tb_cb(%"PRIu32") | crc_en(%"PRIu32")",
			(uint32_t)desc->enc_req.k_,
			(uint32_t)desc->enc_req.en_slice_ts,
			(uint32_t)desc->enc_req.en_host_ts,
			(uint32_t)desc->enc_req.en_cb_wr_status,
			(uint32_t)desc->enc_req.en_output_sg,
			(uint32_t)desc->enc_req.en_input_sg,
			(uint32_t)desc->enc_req.tb_cb,
			(uint32_t)desc->enc_req.crc_en);
	rte_bbdev_log_debug("\t-- input_slice_table_addr_lo(%"PRIu32")"
			" | input_start_addr_lo(%"PRIu32")",
			(uint32_t)desc->enc_req.input_slice_table_addr_lo,
			(uint32_t)desc->enc_req.input_start_addr_lo);
	rte_bbdev_log_debug("\t-- input_slice_table_addr_hi(%"PRIu32")"
			" | input_start_addr_hi(%"PRIu32")",
			(uint32_t)desc->enc_req.input_slice_table_addr_hi,
			(uint32_t)desc->enc_req.input_start_addr_hi);
	rte_bbdev_log_debug("\t-- input_slice_num(%"PRIu32") | input_length(%"PRIu32")",
			(uint32_t)desc->enc_req.input_slice_num,
			(uint32_t)desc->enc_req.input_length);
	rte_bbdev_log_debug("\t-- output_slice_table_addr_lo(%"PRIu32")"
			" | output_start_addr_lo(%"PRIu32")",
			(uint32_t)desc->enc_req.output_slice_table_addr_lo,
			(uint32_t)desc->enc_req.output_start_addr_lo);
	rte_bbdev_log_debug("\t-- output_slice_table_addr_hi(%"PRIu32")"
			" | output_start_addr_hi(%"PRIu32")",
			(uint32_t)desc->enc_req.output_slice_table_addr_hi,
			(uint32_t)desc->enc_req.output_start_addr_hi);
	rte_bbdev_log_debug("\t-- output_slice_num(%"PRIu32") | output_length(%"PRIu32")",
			(uint32_t)desc->enc_req.output_slice_num,
			(uint32_t)desc->enc_req.output_length);
	rte_bbdev_log_debug("\t-- enqueue_timestamp(%"PRIu32")",
			(uint32_t)desc->enc_req.enqueue_timestamp);
	rte_bbdev_log_debug("\t-- completion_timestamp(%"PRIu32")",
			(uint32_t)desc->enc_req.completion_timestamp);

	uint32_t *word = (uint32_t *) desc;
	rte_bbdev_log_debug("%08"PRIx32", %08"PRIx32", %08"PRIx32", %08"PRIx32", "
			"%08"PRIx32", %08"PRIx32", %08"PRIx32", %08"PRIx32", "
			"%08"PRIx32", %08"PRIx32", %08"PRIx32", %08"PRIx32", "
			"%08"PRIx32", %08"PRIx32", %08"PRIx32", %08"PRIx32,
			word[0], word[1], word[2], word[3],
			word[4], word[5], word[6], word[7],
			word[8], word[9], word[10], word[11],
			word[12], word[13], word[14], word[15]);
}

#endif

/**
 * Helper function that returns queue ID if queue is valid
 * or FPGA_5GNR_INVALID_HW_QUEUE_ID otherwise.
 */
static inline uint32_t
fpga_5gnr_get_queue_map(struct fpga_5gnr_fec_device *d, uint32_t q_id)
{
	if (d->fpga_variant == VC_5GNR_FPGA_VARIANT)
		return fpga_5gnr_reg_read_32(d->mmio_base, VC_5GNR_QUEUE_MAP + (q_id << 2));
	else
		return fpga_5gnr_reg_read_32(d->mmio_base, AGX100_QUEUE_MAP + (q_id << 2));
}

static int
fpga_5gnr_setup_queues(struct rte_bbdev *dev, uint16_t num_queues, int socket_id)
{
	/* Number of queues bound to a PF/VF */
	uint32_t hw_q_num = 0;
	uint32_t ring_size, payload, address, q_id, offset;
	rte_iova_t phys_addr;
	struct fpga_5gnr_ring_ctrl_reg ring_reg;
	struct fpga_5gnr_fec_device *d = dev->data->dev_private;

	address = FPGA_5GNR_FEC_QUEUE_PF_VF_MAP_DONE;
	if (!(fpga_5gnr_reg_read_32(d->mmio_base, address) & 0x1)) {
		rte_bbdev_log(ERR,
				"Queue-PF/VF mapping is not set! Was PF configured for device (%s) ?",
				dev->data->name);
		return -EPERM;
	}

	/* Clear queue registers structure */
	memset(&ring_reg, 0, sizeof(struct fpga_5gnr_ring_ctrl_reg));

	/* Scan queue map.
	 * If a queue is valid and mapped to a calling PF/VF the read value is
	 * replaced with a queue ID and if it's not then
	 * FPGA_5GNR_INVALID_HW_QUEUE_ID is returned.
	 */
	for (q_id = 0; q_id < d->total_num_queues; ++q_id) {
		uint32_t hw_q_id = fpga_5gnr_get_queue_map(d, q_id);

		rte_bbdev_log_debug("%s: queue ID: %u, registry queue ID: %u",
				dev->device->name, q_id, hw_q_id);

		if (hw_q_id != FPGA_5GNR_INVALID_HW_QUEUE_ID) {
			d->q_bound_bit_map |= (1ULL << q_id);
			/* Clear queue register of found queue */
			offset = FPGA_5GNR_FEC_RING_CTRL_REGS +
				(sizeof(struct fpga_5gnr_ring_ctrl_reg) * q_id);
			fpga_ring_reg_write(d->mmio_base, offset, ring_reg);
			++hw_q_num;
		}
	}
	if (hw_q_num == 0) {
		rte_bbdev_log(ERR,
			"No HW queues assigned to this device. Probably this is a VF configured for PF mode. Check device configuration!");
		return -ENODEV;
	}

	if (num_queues > hw_q_num) {
		rte_bbdev_log(ERR,
			"Not enough queues for device %s! Requested: %u, available: %u",
			dev->device->name, num_queues, hw_q_num);
		return -EINVAL;
	}
	if (d->fpga_variant == VC_5GNR_FPGA_VARIANT)
		ring_size = FPGA_5GNR_RING_MAX_SIZE * sizeof(struct vc_5gnr_dma_dec_desc);
	else
		ring_size = FPGA_5GNR_RING_MAX_SIZE * sizeof(struct agx100_dma_dec_desc);

	/* Enforce 32 byte alignment */
	RTE_BUILD_BUG_ON((RTE_CACHE_LINE_SIZE % 32) != 0);

	/* Allocate memory for SW descriptor rings */
	d->sw_rings = rte_zmalloc_socket(dev->device->driver->name,
			num_queues * ring_size, RTE_CACHE_LINE_SIZE,
			socket_id);
	if (d->sw_rings == NULL) {
		rte_bbdev_log(ERR,
				"Failed to allocate memory for %s:%u sw_rings",
				dev->device->driver->name, dev->data->dev_id);
		return -ENOMEM;
	}

	d->sw_rings_phys = rte_malloc_virt2iova(d->sw_rings);
	d->sw_ring_size = ring_size;
	d->sw_ring_max_depth = FPGA_5GNR_RING_MAX_SIZE;

	/* Allocate memory for ring flush status */
	d->flush_queue_status = rte_zmalloc_socket(NULL,
			sizeof(uint64_t), RTE_CACHE_LINE_SIZE, socket_id);
	if (d->flush_queue_status == NULL) {
		rte_bbdev_log(ERR,
				"Failed to allocate memory for %s:%u flush_queue_status",
				dev->device->driver->name, dev->data->dev_id);
		return -ENOMEM;
	}

	/* Set the flush status address registers */
	phys_addr = rte_malloc_virt2iova(d->flush_queue_status);

	address = FPGA_5GNR_FEC_VFQ_FLUSH_STATUS_LW;
	payload = (uint32_t)(phys_addr);
	fpga_5gnr_reg_write_32(d->mmio_base, address, payload);

	address = FPGA_5GNR_FEC_VFQ_FLUSH_STATUS_HI;
	payload = (uint32_t)(phys_addr >> 32);
	fpga_5gnr_reg_write_32(d->mmio_base, address, payload);

	return 0;
}

static int
fpga_5gnr_dev_close(struct rte_bbdev *dev)
{
	struct fpga_5gnr_fec_device *fpga_5gnr_dev = dev->data->dev_private;

	rte_free(fpga_5gnr_dev->sw_rings);
	rte_free(fpga_5gnr_dev->flush_queue_status);

	return 0;
}

static void
fpga_5gnr_dev_info_get(struct rte_bbdev *dev, struct rte_bbdev_driver_info *dev_info)
{
	struct fpga_5gnr_fec_device *d = dev->data->dev_private;
	uint32_t q_id = 0;

	static const struct rte_bbdev_op_cap vc_5gnr_bbdev_capabilities[] = {
		{
			.type   = RTE_BBDEV_OP_LDPC_ENC,
			.cap.ldpc_enc = {
				.capability_flags =
						RTE_BBDEV_LDPC_RATE_MATCH |
						RTE_BBDEV_LDPC_ENC_INTERRUPTS |
						RTE_BBDEV_LDPC_CRC_24B_ATTACH,
				.num_buffers_src =
						RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
				.num_buffers_dst =
						RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
			}
		},
		{
		.type   = RTE_BBDEV_OP_LDPC_DEC,
		.cap.ldpc_dec = {
			.capability_flags =
				RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK |
				RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP |
				RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE |
				RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE |
				RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE |
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE |
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE |
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK |
				RTE_BBDEV_LDPC_DEC_INTERRUPTS |
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_FILLERS,
			.llr_size = 6,
			.llr_decimals = 2,
			.num_buffers_src =
					RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
			.num_buffers_hard_out =
					RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
			.num_buffers_soft_out = 0,
		}
		},
		RTE_BBDEV_END_OF_CAPABILITIES_LIST()
	};

	static const struct rte_bbdev_op_cap agx100_bbdev_capabilities[] = {
		{
			.type   = RTE_BBDEV_OP_LDPC_ENC,
			.cap.ldpc_enc = {
				.capability_flags =
						RTE_BBDEV_LDPC_RATE_MATCH |
						RTE_BBDEV_LDPC_CRC_24B_ATTACH,
				.num_buffers_src =
						RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
				.num_buffers_dst =
						RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
			}
		},
		{
		.type   = RTE_BBDEV_OP_LDPC_DEC,
		.cap.ldpc_dec = {
			.capability_flags =
					RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK |
					RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP |
					RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE |
					RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE |
					RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE |
					RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE |
					RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE |
					RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK |
					RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_FILLERS,
			.llr_size = 6,
			.llr_decimals = 2,
			.num_buffers_src =
					RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
			.num_buffers_hard_out =
					RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
			.num_buffers_soft_out = 0,
		}
		},
		RTE_BBDEV_END_OF_CAPABILITIES_LIST()
	};

	/* Check the HARQ DDR size available */
	uint8_t timeout_counter = 0;
	uint32_t harq_buf_ready = fpga_5gnr_reg_read_32(d->mmio_base,
			FPGA_5GNR_FEC_HARQ_BUF_SIZE_RDY_REGS);
	while (harq_buf_ready != 1) {
		usleep(FPGA_5GNR_TIMEOUT_CHECK_INTERVAL);
		timeout_counter++;
		harq_buf_ready = fpga_5gnr_reg_read_32(d->mmio_base,
				FPGA_5GNR_FEC_HARQ_BUF_SIZE_RDY_REGS);
		if (timeout_counter > FPGA_5GNR_HARQ_RDY_TIMEOUT) {
			rte_bbdev_log(ERR, "HARQ Buffer not ready %d", harq_buf_ready);
			harq_buf_ready = 1;
		}
	}
	uint32_t harq_buf_size = fpga_5gnr_reg_read_32(d->mmio_base,
			FPGA_5GNR_FEC_HARQ_BUF_SIZE_REGS);

	static struct rte_bbdev_queue_conf default_queue_conf;
	default_queue_conf.socket = dev->data->socket_id;
	default_queue_conf.queue_size = FPGA_5GNR_RING_MAX_SIZE;

	dev_info->driver_name = dev->device->driver->name;
	dev_info->queue_size_lim = FPGA_5GNR_RING_MAX_SIZE;
	dev_info->hardware_accelerated = true;
	dev_info->min_alignment = 1;
	if (d->fpga_variant == VC_5GNR_FPGA_VARIANT)
		dev_info->harq_buffer_size = (harq_buf_size >> 10) + 1;
	else
		dev_info->harq_buffer_size = harq_buf_size << 10;
	dev_info->default_queue_conf = default_queue_conf;
	if (d->fpga_variant == VC_5GNR_FPGA_VARIANT)
		dev_info->capabilities = vc_5gnr_bbdev_capabilities;
	else
		dev_info->capabilities = agx100_bbdev_capabilities;
	dev_info->cpu_flag_reqs = NULL;
	dev_info->data_endianness = RTE_LITTLE_ENDIAN;
	dev_info->device_status = RTE_BBDEV_DEV_NOT_SUPPORTED;

	/* Calculates number of queues assigned to device */
	dev_info->max_num_queues = 0;
	for (q_id = 0; q_id < d->total_num_queues; ++q_id) {
		uint32_t hw_q_id = fpga_5gnr_get_queue_map(d, q_id);

		if (hw_q_id != FPGA_5GNR_INVALID_HW_QUEUE_ID)
			dev_info->max_num_queues++;
	}
	/* Expose number of queue per operation type */
	dev_info->num_queues[RTE_BBDEV_OP_NONE] = 0;
	dev_info->num_queues[RTE_BBDEV_OP_TURBO_DEC] = 0;
	dev_info->num_queues[RTE_BBDEV_OP_TURBO_ENC] = 0;
	dev_info->num_queues[RTE_BBDEV_OP_LDPC_DEC] = dev_info->max_num_queues / 2;
	dev_info->num_queues[RTE_BBDEV_OP_LDPC_ENC] = dev_info->max_num_queues / 2;
	dev_info->num_queues[RTE_BBDEV_OP_FFT] = 0;
	dev_info->num_queues[RTE_BBDEV_OP_MLDTS] = 0;
	dev_info->queue_priority[RTE_BBDEV_OP_LDPC_DEC] = 1;
	dev_info->queue_priority[RTE_BBDEV_OP_LDPC_ENC] = 1;
}

/**
 * Find index of queue bound to current PF/VF which is unassigned. Return -1
 * when there is no available queue
 */
static inline int
fpga_5gnr_find_free_queue_idx(struct rte_bbdev *dev,
		const struct rte_bbdev_queue_conf *conf)
{
	struct fpga_5gnr_fec_device *d = dev->data->dev_private;
	uint64_t q_idx;
	uint8_t i = 0;
	uint8_t range = d->total_num_queues >> 1;

	if (conf->op_type == RTE_BBDEV_OP_LDPC_ENC) {
		i = d->total_num_queues >> 1;
		range = d->total_num_queues;
	}

	for (; i < range; ++i) {
		q_idx = 1ULL << i;
		/* Check if index of queue is bound to current PF/VF */
		if (d->q_bound_bit_map & q_idx)
			/* Check if found queue was not already assigned */
			if (!(d->q_assigned_bit_map & q_idx)) {
				d->q_assigned_bit_map |= q_idx;
				return i;
			}
	}

	rte_bbdev_log(INFO, "Failed to find free queue on %s", dev->data->name);

	return -1;
}

static int
fpga_5gnr_queue_setup(struct rte_bbdev *dev, uint16_t queue_id,
		const struct rte_bbdev_queue_conf *conf)
{
	uint32_t address, ring_offset;
	struct fpga_5gnr_fec_device *d = dev->data->dev_private;
	struct fpga_5gnr_queue *q;
	int8_t q_idx;

	/* Check if there is a free queue to assign */
	q_idx = fpga_5gnr_find_free_queue_idx(dev, conf);
	if (q_idx == -1)
		return -1;

	/* Allocate the queue data structure. */
	q = rte_zmalloc_socket(dev->device->driver->name, sizeof(*q),
			RTE_CACHE_LINE_SIZE, conf->socket);
	if (q == NULL) {
		/* Mark queue as un-assigned */
		d->q_assigned_bit_map &= (0xFFFFFFFF - (1ULL << q_idx));
		rte_bbdev_log(ERR, "Failed to allocate queue memory");
		return -ENOMEM;
	}

	q->d = d;
	q->q_idx = q_idx;

	/* Set ring_base_addr */
	if (d->fpga_variant == VC_5GNR_FPGA_VARIANT)
		q->vc_5gnr_ring_addr = RTE_PTR_ADD(d->sw_rings, (d->sw_ring_size * queue_id));
	else
		q->agx100_ring_addr = RTE_PTR_ADD(d->sw_rings, (d->sw_ring_size * queue_id));

	q->ring_ctrl_reg.ring_base_addr = d->sw_rings_phys + (d->sw_ring_size * queue_id);

	/* Allocate memory for Completion Head variable*/
	q->ring_head_addr = rte_zmalloc_socket(dev->device->driver->name,
			sizeof(uint64_t), RTE_CACHE_LINE_SIZE, conf->socket);
	if (q->ring_head_addr == NULL) {
		/* Mark queue as un-assigned */
		d->q_assigned_bit_map &= (0xFFFFFFFF - (1ULL << q_idx));
		rte_free(q);
		rte_bbdev_log(ERR,
				"Failed to allocate memory for %s:%u completion_head",
				dev->device->driver->name, dev->data->dev_id);
		return -ENOMEM;
	}
	/* Set ring_head_addr */
	q->ring_ctrl_reg.ring_head_addr = rte_malloc_virt2iova(q->ring_head_addr);

	/* Clear shadow_completion_head */
	q->shadow_completion_head = 0;

	/* Set ring_size */
	if (conf->queue_size > FPGA_5GNR_RING_MAX_SIZE) {
		/* Mark queue as un-assigned */
		d->q_assigned_bit_map &= (0xFFFFFFFF - (1ULL << q_idx));
		rte_free(q->ring_head_addr);
		rte_free(q);
		rte_bbdev_log(ERR,
				"Size of queue is too big %d (MAX: %d ) for %s:%u",
				conf->queue_size, FPGA_5GNR_RING_MAX_SIZE,
				dev->device->driver->name, dev->data->dev_id);
		return -EINVAL;
	}
	q->ring_ctrl_reg.ring_size = conf->queue_size;

	/* Set Miscellaneous FPGA 5GNR register. */
	/* Max iteration number for TTI mitigation - todo */
	q->ring_ctrl_reg.max_ul_dec = 0;
	/* Enable max iteration number for TTI - todo */
	q->ring_ctrl_reg.max_ul_dec_en = 0;

	/* Enable the ring */
	q->ring_ctrl_reg.enable = 1;

	/* Set FPGA 5GNR head_point and tail registers */
	q->ring_ctrl_reg.head_point = q->tail = 0;

	/* Set FPGA 5GNR shadow_tail register */
	q->ring_ctrl_reg.shadow_tail = q->tail;

	/* Calculates the ring offset for found queue */
	ring_offset = FPGA_5GNR_FEC_RING_CTRL_REGS +
			(sizeof(struct fpga_5gnr_ring_ctrl_reg) * q_idx);

	/* Set FPGA 5GNR Ring Control Registers */
	fpga_ring_reg_write(d->mmio_base, ring_offset, q->ring_ctrl_reg);

	/* Store MMIO register of shadow_tail */
	address = ring_offset + FPGA_5GNR_FEC_RING_SHADOW_TAIL;
	q->shadow_tail_addr = RTE_PTR_ADD(d->mmio_base, address);

	q->head_free_desc = q->tail;

	/* Set wrap mask */
	q->sw_ring_wrap_mask = conf->queue_size - 1;

	rte_bbdev_log_debug("Setup dev%u q%u: queue_idx=%u",
			dev->data->dev_id, queue_id, q->q_idx);

	dev->data->queues[queue_id].queue_private = q;

	rte_bbdev_log_debug("BBDEV queue[%d] set up for FPGA 5GNR queue[%d]", queue_id, q_idx);

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	/* Read FPGA Ring Control Registers after configuration*/
	print_ring_reg_debug_info(d->mmio_base, ring_offset);
#endif
	return 0;
}

static int
fpga_5gnr_queue_release(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct fpga_5gnr_fec_device *d = dev->data->dev_private;
	struct fpga_5gnr_queue *q = dev->data->queues[queue_id].queue_private;
	struct fpga_5gnr_ring_ctrl_reg ring_reg;
	uint32_t offset;

	rte_bbdev_log_debug("FPGA 5GNR Queue[%d] released", queue_id);

	if (q != NULL) {
		memset(&ring_reg, 0, sizeof(struct fpga_5gnr_ring_ctrl_reg));
		offset = FPGA_5GNR_FEC_RING_CTRL_REGS +
			(sizeof(struct fpga_5gnr_ring_ctrl_reg) * q->q_idx);
		/* Disable queue */
		fpga_5gnr_reg_write_8(d->mmio_base,
				offset + FPGA_5GNR_FEC_RING_ENABLE, 0x00);
		/* Clear queue registers */
		fpga_ring_reg_write(d->mmio_base, offset, ring_reg);

		/* Mark the Queue as un-assigned */
		d->q_assigned_bit_map &= (0xFFFFFFFF - (1ULL << q->q_idx));
		rte_free(q->ring_head_addr);
		rte_free(q);
		dev->data->queues[queue_id].queue_private = NULL;
	}

	return 0;
}

/* Function starts a device queue. */
static int
fpga_5gnr_queue_start(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct fpga_5gnr_fec_device *d = dev->data->dev_private;
	struct fpga_5gnr_queue *q = dev->data->queues[queue_id].queue_private;
	uint32_t offset = FPGA_5GNR_FEC_RING_CTRL_REGS +
			(sizeof(struct fpga_5gnr_ring_ctrl_reg) * q->q_idx);
	uint8_t enable = 0x01;
	uint16_t zero = 0x0000;
#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (d == NULL) {
		rte_bbdev_log(ERR, "Invalid device pointer");
		return -1;
	}
#endif
	if (dev->data->queues[queue_id].queue_private == NULL) {
		rte_bbdev_log(ERR, "Cannot start invalid queue %d", queue_id);
		return -1;
	}

	/* Clear queue head and tail variables */
	q->tail = q->head_free_desc = 0;

	/* Clear FPGA 5GNR head_point and tail registers */
	fpga_5gnr_reg_write_16(d->mmio_base, offset + FPGA_5GNR_FEC_RING_HEAD_POINT, zero);
	fpga_5gnr_reg_write_16(d->mmio_base, offset + FPGA_5GNR_FEC_RING_SHADOW_TAIL, zero);

	/* Enable queue */
	fpga_5gnr_reg_write_8(d->mmio_base, offset + FPGA_5GNR_FEC_RING_ENABLE, enable);

	rte_bbdev_log_debug("FPGA 5GNR Queue[%d] started", queue_id);
	return 0;
}

/* Function stops a device queue. */
static int
fpga_5gnr_queue_stop(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct fpga_5gnr_fec_device *d = dev->data->dev_private;
#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (d == NULL) {
		rte_bbdev_log(ERR, "Invalid device pointer");
		return -1;
	}
#endif
	struct fpga_5gnr_queue *q = dev->data->queues[queue_id].queue_private;
	uint32_t offset = FPGA_5GNR_FEC_RING_CTRL_REGS +
			(sizeof(struct fpga_5gnr_ring_ctrl_reg) * q->q_idx);
	uint8_t payload = 0x01;
	uint8_t counter = 0;
	uint8_t timeout = FPGA_5GNR_QUEUE_FLUSH_TIMEOUT_US / FPGA_5GNR_TIMEOUT_CHECK_INTERVAL;

	/* Set flush_queue_en bit to trigger queue flushing */
	fpga_5gnr_reg_write_8(d->mmio_base,
			offset + FPGA_5GNR_FEC_RING_FLUSH_QUEUE_EN, payload);

	/** Check if queue flush is completed.
	 * FPGA 5GNR will update the completion flag after queue flushing is
	 * completed. If completion flag is not updated within 1ms it is
	 * considered as a failure.
	 */
	while (!(*((volatile uint8_t *)d->flush_queue_status + q->q_idx) & payload)) {
		if (counter > timeout) {
			rte_bbdev_log(ERR, "FPGA 5GNR Queue Flush failed for queue %d", queue_id);
			return -1;
		}
		usleep(FPGA_5GNR_TIMEOUT_CHECK_INTERVAL);
		counter++;
	}

	/* Disable queue */
	payload = 0x00;
	fpga_5gnr_reg_write_8(d->mmio_base, offset + FPGA_5GNR_FEC_RING_ENABLE, payload);

	rte_bbdev_log_debug("FPGA 5GNR Queue[%d] stopped", queue_id);
	return 0;
}

static inline uint16_t
get_queue_id(struct rte_bbdev_data *data, uint8_t q_idx)
{
	uint16_t queue_id;

	for (queue_id = 0; queue_id < data->num_queues; ++queue_id) {
		struct fpga_5gnr_queue *q = data->queues[queue_id].queue_private;
		if (q != NULL && q->q_idx == q_idx)
			return queue_id;
	}

	return -1;
}

/* Interrupt handler triggered by FPGA 5GNR dev for handling specific interrupt. */
static void
fpga_5gnr_dev_interrupt_handler(void *cb_arg)
{
	struct rte_bbdev *dev = cb_arg;
	struct fpga_5gnr_fec_device *d = dev->data->dev_private;
	struct fpga_5gnr_queue *q;
	uint64_t ring_head;
	uint64_t q_idx;
	uint16_t queue_id;
	uint8_t i;

	/* Scan queue assigned to this device */
	for (i = 0; i < d->total_num_queues; ++i) {
		q_idx = 1ULL << i;
		if (d->q_bound_bit_map & q_idx) {
			queue_id = get_queue_id(dev->data, i);
			if (queue_id == (uint16_t) -1)
				continue;

			/* Check if completion head was changed */
			q = dev->data->queues[queue_id].queue_private;
			ring_head = *q->ring_head_addr;
			if (q->shadow_completion_head != ring_head &&
				q->irq_enable == 1) {
				q->shadow_completion_head = ring_head;
				rte_bbdev_pmd_callback_process(
						dev,
						RTE_BBDEV_EVENT_DEQUEUE,
						&queue_id);
			}
		}
	}
}

static int
fpga_5gnr_queue_intr_enable(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct fpga_5gnr_queue *q = dev->data->queues[queue_id].queue_private;

	if (!rte_intr_cap_multiple(dev->intr_handle))
		return -ENOTSUP;

	q->irq_enable = 1;

	return 0;
}

static int
fpga_5gnr_queue_intr_disable(struct rte_bbdev *dev, uint16_t queue_id)
{
	struct fpga_5gnr_queue *q = dev->data->queues[queue_id].queue_private;
	q->irq_enable = 0;

	return 0;
}

static int
fpga_5gnr_intr_enable(struct rte_bbdev *dev)
{
	int ret;
	uint8_t i;
	struct fpga_5gnr_fec_device *d = dev->data->dev_private;
	uint8_t num_intr_vec;

	num_intr_vec = d->total_num_queues - RTE_INTR_VEC_RXTX_OFFSET;
	if (!rte_intr_cap_multiple(dev->intr_handle)) {
		rte_bbdev_log(ERR, "Multiple intr vector is not supported by FPGA (%s)",
				dev->data->name);
		return -ENOTSUP;
	}

	/* Create event file descriptors for each of the supported queues (Maximum 64).
	 * Event fds will be mapped to FPGA IRQs in rte_intr_enable().
	 * This is a 1:1 mapping where the IRQ number is a direct translation to the queue number.
	 *
	 * num_intr_vec event fds are created as rte_intr_enable()
	 * mapped the first IRQ to already created interrupt event file
	 * descriptor (intr_handle->fd).
	 */
	if (rte_intr_efd_enable(dev->intr_handle, num_intr_vec)) {
		rte_bbdev_log(ERR, "Failed to create fds for %u queues", dev->data->num_queues);
		return -1;
	}

	/* TODO Each event file descriptor is overwritten by interrupt event
	 * file descriptor. That descriptor is added to epoll observed list.
	 * It ensures that callback function assigned to that descriptor will
	 * invoked when any FPGA queue issues interrupt.
	 */
	for (i = 0; i < num_intr_vec; ++i) {
		if (rte_intr_efds_index_set(dev->intr_handle, i,
				rte_intr_fd_get(dev->intr_handle)))
			return -rte_errno;
	}

	if (rte_intr_vec_list_alloc(dev->intr_handle, "intr_vec", dev->data->num_queues)) {
		rte_bbdev_log(ERR, "Failed to allocate %u vectors", dev->data->num_queues);
		return -ENOMEM;
	}

	ret = rte_intr_enable(dev->intr_handle);
	if (ret < 0) {
		rte_bbdev_log(ERR,
				"Couldn't enable interrupts for device: %s",
				dev->data->name);
		return ret;
	}

	ret = rte_intr_callback_register(dev->intr_handle, fpga_5gnr_dev_interrupt_handler, dev);
	if (ret < 0) {
		rte_bbdev_log(ERR,
				"Couldn't register interrupt callback for device: %s",
				dev->data->name);
		return ret;
	}

	return 0;
}

static const struct rte_bbdev_ops fpga_5gnr_ops = {
	.setup_queues = fpga_5gnr_setup_queues,
	.intr_enable = fpga_5gnr_intr_enable,
	.close = fpga_5gnr_dev_close,
	.info_get = fpga_5gnr_dev_info_get,
	.queue_setup = fpga_5gnr_queue_setup,
	.queue_stop = fpga_5gnr_queue_stop,
	.queue_start = fpga_5gnr_queue_start,
	.queue_release = fpga_5gnr_queue_release,
	.queue_intr_enable = fpga_5gnr_queue_intr_enable,
	.queue_intr_disable = fpga_5gnr_queue_intr_disable
};

/* Provide the descriptor index on a given queue */
static inline uint16_t
fpga_5gnr_desc_idx(struct fpga_5gnr_queue *q, uint16_t offset)
{
	return (q->head_free_desc + offset) & q->sw_ring_wrap_mask;
}

/* Provide the VC 5GNR descriptor pointer on a given queue */
static inline union vc_5gnr_dma_desc*
vc_5gnr_get_desc(struct fpga_5gnr_queue *q, uint16_t offset)
{
	return q->vc_5gnr_ring_addr + fpga_5gnr_desc_idx(q, offset);
}

/* Provide the AGX100 descriptor pointer on a given queue */
static inline union agx100_dma_desc*
agx100_get_desc(struct fpga_5gnr_queue *q, uint16_t offset)
{
	return q->agx100_ring_addr + fpga_5gnr_desc_idx(q, offset);
}

/* Provide the descriptor index for the tail of a given queue */
static inline uint16_t
fpga_5gnr_desc_idx_tail(struct fpga_5gnr_queue *q, uint16_t offset)
{
	return (q->tail + offset) & q->sw_ring_wrap_mask;
}

/* Provide the descriptor tail pointer on a given queue */
static inline union vc_5gnr_dma_desc*
vc_5gnr_get_desc_tail(struct fpga_5gnr_queue *q, uint16_t offset)
{
	return q->vc_5gnr_ring_addr + fpga_5gnr_desc_idx_tail(q, offset);
}

/* Provide the descriptor tail pointer on a given queue */
static inline union agx100_dma_desc*
agx100_get_desc_tail(struct fpga_5gnr_queue *q, uint16_t offset)
{
	return q->agx100_ring_addr + fpga_5gnr_desc_idx_tail(q, offset);
}

static inline void
fpga_5gnr_dma_enqueue(struct fpga_5gnr_queue *q, uint16_t num_desc,
		struct rte_bbdev_stats *queue_stats)
{
	uint64_t start_time = 0;
	queue_stats->acc_offload_cycles = 0;

	/* Update tail and shadow_tail register */
	q->tail = fpga_5gnr_desc_idx_tail(q, num_desc);

	rte_wmb();

	/* Start time measurement for enqueue function offload. */
	start_time = rte_rdtsc_precise();
	mmio_write_16(q->shadow_tail_addr, q->tail);

	rte_wmb();
	queue_stats->acc_offload_cycles += rte_rdtsc_precise() - start_time;
}

/* Read flag value 0/1/ from bitmap */
static inline bool
check_bit(uint32_t bitmap, uint32_t bitmask)
{
	return bitmap & bitmask;
}

/* Vista Creek 5GNR FPGA descriptor errors.
 * Print an error if a descriptor error has occurred.
 * Return 0 on success, 1 on failure.
 */
static inline int
vc_5gnr_check_desc_error(uint32_t error_code) {
	switch (error_code) {
	case VC_5GNR_DESC_ERR_NO_ERR:
		return 0;
	case VC_5GNR_DESC_ERR_K_P_OUT_OF_RANGE:
		rte_bbdev_log(ERR, "Encode block size K' is out of range");
		break;
	case VC_5GNR_DESC_ERR_Z_C_NOT_LEGAL:
		rte_bbdev_log(ERR, "Zc is illegal");
		break;
	case VC_5GNR_DESC_ERR_DESC_OFFSET_ERR:
		rte_bbdev_log(ERR,
				"Queue offset does not meet the expectation in the FPGA"
				);
		break;
	case VC_5GNR_DESC_ERR_DESC_READ_FAIL:
		rte_bbdev_log(ERR, "Unsuccessful completion for descriptor read");
		break;
	case VC_5GNR_DESC_ERR_DESC_READ_TIMEOUT:
		rte_bbdev_log(ERR, "Descriptor read time-out");
		break;
	case VC_5GNR_DESC_ERR_DESC_READ_TLP_POISONED:
		rte_bbdev_log(ERR, "Descriptor read TLP poisoned");
		break;
	case VC_5GNR_DESC_ERR_HARQ_INPUT_LEN:
		rte_bbdev_log(ERR, "HARQ input length is invalid");
		break;
	case VC_5GNR_DESC_ERR_CB_READ_FAIL:
		rte_bbdev_log(ERR, "Unsuccessful completion for code block");
		break;
	case VC_5GNR_DESC_ERR_CB_READ_TIMEOUT:
		rte_bbdev_log(ERR, "Code block read time-out");
		break;
	case VC_5GNR_DESC_ERR_CB_READ_TLP_POISONED:
		rte_bbdev_log(ERR, "Code block read TLP poisoned");
		break;
	case VC_5GNR_DESC_ERR_HBSTORE_ERR:
		rte_bbdev_log(ERR, "Hbstroe exceeds HARQ buffer size.");
		break;
	default:
		rte_bbdev_log(ERR, "Descriptor error unknown error code %u", error_code);
		break;
	}
	return 1;
}

/* AGX100 FPGA descriptor errors
 * Print an error if a descriptor error has occurred.
 * Return 0 on success, 1 on failure
 */
static inline int
agx100_check_desc_error(uint32_t error_code, uint32_t error_msg) {
	uint8_t error = error_code << 4 | error_msg;
	switch (error) {
	case AGX100_DESC_ERR_NO_ERR:
		return 0;
	case AGX100_DESC_ERR_E_NOT_LEGAL:
		rte_bbdev_log(ERR, "Invalid output length of rate matcher E");
		break;
	case AGX100_DESC_ERR_K_P_OUT_OF_RANGE:
		rte_bbdev_log(ERR, "Encode block size K' is out of range");
		break;
	case AGX100_DESC_ERR_NCB_OUT_OF_RANGE:
		rte_bbdev_log(ERR, "Ncb circular buffer size is out of range");
		break;
	case AGX100_DESC_ERR_Z_C_NOT_LEGAL:
		rte_bbdev_log(ERR, "Zc is illegal");
		break;
	case AGX100_DESC_ERR_DESC_INDEX_ERR:
		rte_bbdev_log(ERR,
				"Desc_index received does not meet the expectation in the AGX100"
				);
		break;
	case AGX100_DESC_ERR_HARQ_INPUT_LEN_A:
		rte_bbdev_log(ERR, "HARQ input length A is invalid.");
		break;
	case AGX100_DESC_ERR_HARQ_INPUT_LEN_B:
		rte_bbdev_log(ERR, "HARQ input length B is invalid.");
		break;
	case AGX100_DESC_ERR_HBSTORE_OFFSET_ERR:
		rte_bbdev_log(ERR, "Hbstore exceeds HARQ buffer size.");
		break;
	case AGX100_DESC_ERR_TB_CBG_ERR:
		rte_bbdev_log(ERR, "Total CB number C=0 or CB number with Ea Ca=0 or Ca>C.");
		break;
	case AGX100_DESC_ERR_CBG_OUT_OF_RANGE:
		rte_bbdev_log(ERR, "Cbgti or max_cbg is out of range");
		break;
	case AGX100_DESC_ERR_CW_RM_NOT_LEGAL:
		rte_bbdev_log(ERR, "Cw_rm is illegal");
		break;
	case AGX100_DESC_ERR_UNSUPPORTED_REQ:
		rte_bbdev_log(ERR, "Unsupported request for descriptor");
		break;
	case AGX100_DESC_ERR_RESERVED:
		rte_bbdev_log(ERR, "Reserved");
		break;
	case AGX100_DESC_ERR_DESC_ABORT:
		rte_bbdev_log(ERR, "Completed abort for descriptor");
		break;
	case AGX100_DESC_ERR_DESC_READ_TLP_POISONED:
		rte_bbdev_log(ERR, "Descriptor read TLP poisoned");
		break;
	default:
		rte_bbdev_log(ERR,
				"Descriptor error unknown error code %u error msg %u",
				error_code, error_msg);
		break;
	}
	return 1;
}

/* Compute value of k0.
 * Based on 3GPP 38.212 Table 5.4.2.1-2
 * Starting position of different redundancy versions, k0
 */
static inline uint16_t
get_k0(uint16_t n_cb, uint16_t z_c, uint8_t bg, uint8_t rv_index)
{
	uint16_t n = (bg == 1 ? N_ZC_1 : N_ZC_2) * z_c;
	if (rv_index == 0)
		return 0;
	if (z_c == 0)
		return 0;
	if (n_cb == n) {
		if (rv_index == 1)
			return (bg == 1 ? K0_1_1 : K0_1_2) * z_c;
		else if (rv_index == 2)
			return (bg == 1 ? K0_2_1 : K0_2_2) * z_c;
		else
			return (bg == 1 ? K0_3_1 : K0_3_2) * z_c;
	}
	/* LBRM case - includes a division by N */
	if (rv_index == 1)
		return (((bg == 1 ? K0_1_1 : K0_1_2) * n_cb)
				/ n) * z_c;
	else if (rv_index == 2)
		return (((bg == 1 ? K0_2_1 : K0_2_2) * n_cb)
				/ n) * z_c;
	else
		return (((bg == 1 ? K0_3_1 : K0_3_2) * n_cb)
				/ n) * z_c;
}

/**
 * Vista Creek 5GNR FPGA
 * Set DMA descriptor for encode operation (1 Code Block)
 *
 * @param op
 *   Pointer to a single encode operation.
 * @param desc
 *   Pointer to DMA descriptor.
 * @param input
 *   Pointer to pointer to input data which will be decoded.
 * @param e
 *   E value (length of output in bits).
 * @param ncb
 *   Ncb value (size of the soft buffer).
 * @param out_length
 *   Length of output buffer
 * @param in_offset
 *   Input offset in rte_mbuf structure. It is used for calculating the point
 *   where data is starting.
 * @param out_offset
 *   Output offset in rte_mbuf structure. It is used for calculating the point
 *   where hard output data will be stored.
 * @param cbs_in_op
 *   Number of CBs contained in one operation.
 */
static inline int
vc_5gnr_dma_desc_te_fill(struct rte_bbdev_enc_op *op,
		struct vc_5gnr_dma_enc_desc *desc, struct rte_mbuf *input,
		struct rte_mbuf *output, uint16_t k_,  uint16_t e,
		uint32_t in_offset, uint32_t out_offset, uint16_t desc_offset,
		uint8_t cbs_in_op)
{
	/* reset */
	desc->done = 0;
	desc->error = 0;
	desc->k_ = k_;
	desc->rm_e = e;
	desc->desc_idx = desc_offset;
	desc->zc = op->ldpc_enc.z_c;
	desc->bg_idx = op->ldpc_enc.basegraph - 1;
	desc->qm_idx = op->ldpc_enc.q_m / 2;
	desc->crc_en = check_bit(op->ldpc_enc.op_flags,
			RTE_BBDEV_LDPC_CRC_24B_ATTACH);
	desc->irq_en = 0;
	desc->k0 = get_k0(op->ldpc_enc.n_cb, op->ldpc_enc.z_c,
			op->ldpc_enc.basegraph, op->ldpc_enc.rv_index);
	desc->ncb = op->ldpc_enc.n_cb;
	desc->num_null = op->ldpc_enc.n_filler;
	/* Set inbound data buffer address */
	desc->in_addr_hi = (uint32_t)(
			rte_pktmbuf_iova_offset(input, in_offset) >> 32);
	desc->in_addr_lw = (uint32_t)(
			rte_pktmbuf_iova_offset(input, in_offset));

	desc->out_addr_hi = (uint32_t)(
			rte_pktmbuf_iova_offset(output, out_offset) >> 32);
	desc->out_addr_lw = (uint32_t)(
			rte_pktmbuf_iova_offset(output, out_offset));
	/* Save software context needed for dequeue */
	desc->op_addr = op;
	/* Set total number of CBs in an op */
	desc->cbs_in_op = cbs_in_op;
	return 0;
}

/**
 * AGX100 FPGA
 * Set DMA descriptor for encode operation (1 Code Block)
 *
 * @param op
 *   Pointer to a single encode operation.
 * @param desc
 *   Pointer to DMA descriptor.
 * @param input
 *   Pointer to pointer to input data which will be decoded.
 * @param e
 *   E value (length of output in bits).
 * @param ncb
 *   Ncb value (size of the soft buffer).
 * @param out_length
 *   Length of output buffer
 * @param in_offset
 *   Input offset in rte_mbuf structure. It is used for calculating the point
 *   where data is starting.
 * @param out_offset
 *   Output offset in rte_mbuf structure. It is used for calculating the point
 *   where hard output data will be stored.
 * @param cbs_in_op
 *   Number of CBs contained in one operation.
 */
static inline int
agx100_dma_desc_le_fill(struct rte_bbdev_enc_op *op,
		struct agx100_dma_enc_desc *desc, struct rte_mbuf *input,
		struct rte_mbuf *output, uint16_t k_,  uint32_t e,
		uint32_t in_offset, uint32_t out_offset, uint16_t desc_offset,
		uint8_t cbs_in_op)
{
	/* reset. */
	desc->done = 0;
	desc->error_msg = 0;
	desc->error_code = 0;
	desc->ncb = op->ldpc_enc.n_cb;
	desc->bg_idx = op->ldpc_enc.basegraph - 1;
	desc->qm_idx = op->ldpc_enc.q_m >> 1;
	desc->zc = op->ldpc_enc.z_c;
	desc->rv = op->ldpc_enc.rv_index;
	desc->int_en = 0;	/**< Set by device externally. */
	desc->max_cbg = 0;	/**< TODO: CBG specific. */
	desc->cbgti = 0;	/**< TODO: CBG specific. */
	desc->cbgs = 0;		/**< TODO: CBG specific. */
	desc->desc_idx = desc_offset;
	desc->ca = 0;	/**< TODO: CBG specific. */
	desc->c = 0;	/**< TODO: CBG specific. */
	desc->num_null = op->ldpc_enc.n_filler;
	desc->ea = e;
	desc->eb = e;	/**< TODO: TB/CBG specific. */
	desc->k_ = k_;
	desc->en_slice_ts = 0;	/**< TODO: Slice specific. */
	desc->en_host_ts = 0;	/**< TODO: Slice specific. */
	desc->en_cb_wr_status = 0;	/**< TODO: Event Queue specific. */
	desc->en_output_sg = 0;	/**< TODO: Slice specific. */
	desc->en_input_sg = 0;	/**< TODO: Slice specific. */
	desc->tb_cb = 0;	/**< Descriptor for CB. TODO: Add TB and CBG logic. */
	desc->crc_en = check_bit(op->ldpc_enc.op_flags,
			RTE_BBDEV_LDPC_CRC_24B_ATTACH);

	/* Set inbound/outbound data buffer address. */
	/* TODO: add logic for input_slice. */
	desc->output_start_addr_hi = (uint32_t)(
			rte_pktmbuf_iova_offset(output, out_offset) >> 32);
	desc->output_start_addr_lo = (uint32_t)(
			rte_pktmbuf_iova_offset(output, out_offset));
	desc->input_start_addr_hi = (uint32_t)(
			rte_pktmbuf_iova_offset(input, in_offset) >> 32);
	desc->input_start_addr_lo = (uint32_t)(
			rte_pktmbuf_iova_offset(input, in_offset));
	desc->output_length = (e + 7) >> 3; /* in bytes. */
	desc->input_length = input->data_len;
	desc->enqueue_timestamp = 0;
	desc->completion_timestamp = 0;
	/* Save software context needed for dequeue. */
	desc->op_addr = op;
	/* Set total number of CBs in an op. */
	desc->cbs_in_op = cbs_in_op;
	return 0;
}

/**
 * Vista Creek 5GNR FPGA
 * Set DMA descriptor for decode operation (1 Code Block)
 *
 * @param op
 *   Pointer to a single encode operation.
 * @param desc
 *   Pointer to DMA descriptor.
 * @param input
 *   Pointer to pointer to input data which will be decoded.
 * @param in_offset
 *   Input offset in rte_mbuf structure. It is used for calculating the point
 *   where data is starting.
 * @param out_offset
 *   Output offset in rte_mbuf structure. It is used for calculating the point
 *   where hard output data will be stored.
 * @param cbs_in_op
 *   Number of CBs contained in one operation.
 */
static inline int
vc_5gnr_dma_desc_ld_fill(struct rte_bbdev_dec_op *op,
		struct vc_5gnr_dma_dec_desc *desc,
		struct rte_mbuf *input,	struct rte_mbuf *output,
		uint16_t harq_in_length,
		uint32_t in_offset, uint32_t out_offset,
		uint32_t harq_offset,
		uint16_t desc_offset,
		uint8_t cbs_in_op)
{
	/* reset */
	desc->done = 0;
	desc->error = 0;
	/* Set inbound data buffer address */
	desc->in_addr_hi = (uint32_t)(
			rte_pktmbuf_iova_offset(input, in_offset) >> 32);
	desc->in_addr_lw = (uint32_t)(
			rte_pktmbuf_iova_offset(input, in_offset));
	desc->rm_e = op->ldpc_dec.cb_params.e;
	desc->harq_input_length = harq_in_length;
	desc->et_dis = !check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE);
	desc->rv = op->ldpc_dec.rv_index;
	desc->crc24b_ind = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK);
	desc->drop_crc24b = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP);
	desc->desc_idx = desc_offset;
	desc->ncb = op->ldpc_dec.n_cb;
	desc->num_null = op->ldpc_dec.n_filler;
	desc->hbstroe_offset = harq_offset >> 10;
	desc->zc = op->ldpc_dec.z_c;
	desc->harqin_en = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE);
	desc->bg_idx = op->ldpc_dec.basegraph - 1;
	desc->max_iter = op->ldpc_dec.iter_max;
	desc->qm_idx = op->ldpc_dec.q_m / 2;
	desc->out_addr_hi = (uint32_t)(
			rte_pktmbuf_iova_offset(output, out_offset) >> 32);
	desc->out_addr_lw = (uint32_t)(
			rte_pktmbuf_iova_offset(output, out_offset));
	/* Save software context needed for dequeue */
	desc->op_addr = op;
	/* Set total number of CBs in an op */
	desc->cbs_in_op = cbs_in_op;

	return 0;
}

/**
 * AGX100 FPGA
 * Set DMA descriptor for decode operation (1 Code Block)
 *
 * @param op
 *   Pointer to a single encode operation.
 * @param desc
 *   Pointer to DMA descriptor.
 * @param input
 *   Pointer to pointer to input data which will be decoded.
 * @param in_offset
 *   Input offset in rte_mbuf structure. It is used for calculating the point
 *   where data is starting.
 * @param out_offset
 *   Output offset in rte_mbuf structure. It is used for calculating the point
 *   where hard output data will be stored.
 * @param cbs_in_op
 *   Number of CBs contained in one operation.
 */
static inline int
agx100_dma_desc_ld_fill(struct rte_bbdev_dec_op *op,
		struct agx100_dma_dec_desc *desc,
		struct rte_mbuf *input,	struct rte_mbuf *output,
		uint16_t harq_in_length,
		uint32_t in_offset, uint32_t out_offset,
		uint32_t harq_in_offset,
		uint32_t harq_out_offset,
		uint16_t desc_offset,
		uint8_t cbs_in_op)
{
	/* reset. */
	desc->done = 0;
	desc->tb_crc_pass = 0;
	desc->cb_crc_all_pass = 0;
	desc->cb_all_et_pass = 0;
	desc->max_iter_ret = 0;
	desc->cgb_crc_bitmap = 0;	/**< TODO: CBG specific. */
	desc->error_msg = 0;
	desc->error_code = 0;
	desc->et_dis = !check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE);
	desc->harq_in_en = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE);
	desc->max_iter = op->ldpc_dec.iter_max;
	desc->ncb = op->ldpc_dec.n_cb;
	desc->bg_idx = op->ldpc_dec.basegraph - 1;
	desc->qm_idx = op->ldpc_dec.q_m >> 1;
	desc->zc = op->ldpc_dec.z_c;
	desc->rv = op->ldpc_dec.rv_index;
	desc->int_en = 0;	/**< Set by device externally. */
	desc->max_cbg = 0;	/**< TODO: CBG specific. */
	desc->cbgti = 0;	/**< TODO: CBG specific. */
	desc->cbgfi = 0;	/**< TODO: CBG specific. */
	desc->cbgs = 0;		/**< TODO: CBG specific. */
	desc->desc_idx = desc_offset;
	desc->ca = 0;	/**< TODO: CBG specific. */
	desc->c = 0;		/**< TODO: CBG specific. */
	desc->llr_pckg = 0;		/**< TODO: Not implemented yet. */
	desc->syndrome_check_mode = 1;	/**< TODO: Make it configurable. */
	desc->num_null = op->ldpc_dec.n_filler;
	desc->ea = op->ldpc_dec.cb_params.e;	/**< TODO: TB/CBG specific. */
	desc->eba = 0;	/**< TODO: TB/CBG specific. */
	desc->hbstore_offset_out = harq_out_offset >> 10;
	desc->hbstore_offset_in = harq_in_offset >> 10;
	desc->en_slice_ts = 0;	/**< TODO: Slice specific. */
	desc->en_host_ts = 0;	/**< TODO: Slice specific. */
	desc->en_cb_wr_status = 0;	/**< TODO: Event Queue specific. */
	desc->en_output_sg = 0;	/**< TODO: Slice specific. */
	desc->en_input_sg = 0;	/**< TODO: Slice specific. */
	desc->tb_cb = 0; /**< Descriptor for CB. TODO: Add TB and CBG logic. */
	desc->crc24b_ind = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK);
	desc->drop_crc24b = check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP);
	desc->harq_input_length_a =
			harq_in_length; /**< Descriptor for CB. TODO: Add TB and CBG logic. */
	desc->harq_input_length_b = 0; /**< Descriptor for CB. TODO: Add TB and CBG logic. */
	/* Set inbound/outbound data buffer address. */
	/* TODO: add logic for input_slice. */
	desc->output_start_addr_hi = (uint32_t)(
			rte_pktmbuf_iova_offset(output, out_offset) >> 32);
	desc->output_start_addr_lo = (uint32_t)(
			rte_pktmbuf_iova_offset(output, out_offset));
	desc->input_start_addr_hi = (uint32_t)(
			rte_pktmbuf_iova_offset(input, in_offset) >> 32);
	desc->input_start_addr_lo = (uint32_t)(
			rte_pktmbuf_iova_offset(input, in_offset));
	desc->output_length = (((op->ldpc_dec.basegraph == 1) ? 22 : 10) * op->ldpc_dec.z_c
			- op->ldpc_dec.n_filler - desc->drop_crc24b * 24) >> 3;
	desc->input_length = op->ldpc_dec.cb_params.e;	/**< TODO: TB/CBG specific. */
	desc->enqueue_timestamp = 0;
	desc->completion_timestamp = 0;
	/* Save software context needed for dequeue. */
	desc->op_addr = op;
	/* Set total number of CBs in an op. */
	desc->cbs_in_op = cbs_in_op;
	return 0;
}

/* Validates LDPC encoder parameters for VC 5GNR FPGA. */
static inline int
vc_5gnr_validate_ldpc_enc_op(struct rte_bbdev_enc_op *op)
{
	struct rte_bbdev_op_ldpc_enc *ldpc_enc = &op->ldpc_enc;
	int z_c, n_filler, K, Kp, q_m, n_cb, N, k0, crc24;
	int32_t L, Lcb, cw, cw_rm, e;

	if (ldpc_enc->input.data == NULL) {
		rte_bbdev_log(ERR, "Invalid input pointer");
		return -1;
	}
	if (ldpc_enc->output.data == NULL) {
		rte_bbdev_log(ERR, "Invalid output pointer");
		return -1;
	}
	if (ldpc_enc->input.length == 0) {
		rte_bbdev_log(ERR, "CB size (%u) is null",
				ldpc_enc->input.length);
		return -1;
	}
	if ((ldpc_enc->basegraph > 2) || (ldpc_enc->basegraph == 0)) {
		rte_bbdev_log(ERR,
				"BG (%u) is out of range 1 <= value <= 2",
				ldpc_enc->basegraph);
		return -1;
	}
	if (ldpc_enc->rv_index > 3) {
		rte_bbdev_log(ERR,
				"rv_index (%u) is out of range 0 <= value <= 3",
				ldpc_enc->rv_index);
		return -1;
	}
	if (ldpc_enc->code_block_mode > RTE_BBDEV_CODE_BLOCK) {
		rte_bbdev_log(ERR,
				"code_block_mode (%u) is out of range 0 <= value <= 1",
				ldpc_enc->code_block_mode);
		return -1;
	}

	if (ldpc_enc->input.length >
		RTE_BBDEV_LDPC_MAX_CB_SIZE >> 3) {
		rte_bbdev_log(ERR, "CB size (%u) is too big, max: %d",
				ldpc_enc->input.length,
				RTE_BBDEV_LDPC_MAX_CB_SIZE);
		return -1;
	}

	z_c = ldpc_enc->z_c;
	/* Check Zc is valid value */
	if ((z_c > 384) || (z_c < 4)) {
		rte_bbdev_log(ERR, "Zc (%u) is out of range", z_c);
		return -1;
	}
	if (z_c > 256) {
		if ((z_c % 32) != 0) {
			rte_bbdev_log(ERR, "Invalid Zc %d", z_c);
			return -1;
		}
	} else if (z_c > 128) {
		if ((z_c % 16) != 0) {
			rte_bbdev_log(ERR, "Invalid Zc %d", z_c);
			return -1;
		}
	} else if (z_c > 64) {
		if ((z_c % 8) != 0) {
			rte_bbdev_log(ERR, "Invalid Zc %d", z_c);
			return -1;
		}
	} else if (z_c > 32) {
		if ((z_c % 4) != 0) {
			rte_bbdev_log(ERR, "Invalid Zc %d", z_c);
			return -1;
		}
	} else if (z_c > 16) {
		if ((z_c % 2) != 0) {
			rte_bbdev_log(ERR, "Invalid Zc %d", z_c);
			return -1;
		}
	}

	n_filler = ldpc_enc->n_filler;
	K = (ldpc_enc->basegraph == 1 ? 22 : 10) * ldpc_enc->z_c;
	Kp = K - n_filler;
	q_m = ldpc_enc->q_m;
	n_cb = ldpc_enc->n_cb;
	N = (ldpc_enc->basegraph == 1 ? N_ZC_1 : N_ZC_2) * z_c;
	k0 = get_k0(n_cb, z_c, ldpc_enc->basegraph, ldpc_enc->rv_index);
	crc24 = 0;
	e = ldpc_enc->cb_params.e;

	if (check_bit(op->ldpc_enc.op_flags, RTE_BBDEV_LDPC_CRC_24B_ATTACH))
		crc24 = 24;

	if (K < (int) (ldpc_enc->input.length * 8 + n_filler) + crc24) {
		rte_bbdev_log(ERR, "K and F not matching input size %u %u %u",
				K, n_filler, ldpc_enc->input.length);
		return -1;
	}
	if (ldpc_enc->code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK) {
		rte_bbdev_log(ERR, "TB mode not supported");
		return -1;

	}

	/* K' range check */
	if (Kp % 8 > 0) {
		rte_bbdev_log(ERR, "K' not byte aligned %u", Kp);
		return -1;
	}
	if ((crc24 > 0) && (Kp < 292)) {
		rte_bbdev_log(ERR, "Invalid CRC24 for small block %u", Kp);
		return -1;
	}
	if (Kp < 24) {
		rte_bbdev_log(ERR, "K' too small %u", Kp);
		return -1;
	}
	if (n_filler >= (K - 2 * z_c)) {
		rte_bbdev_log(ERR, "K - F invalid %u %u", K, n_filler);
		return -1;
	}
	/* Ncb range check */
	if ((n_cb > N) || (n_cb < 32) || (n_cb <= (Kp - crc24))) {
		rte_bbdev_log(ERR, "Ncb (%u) is out of range K  %d N %d", n_cb, K, N);
		return -1;
	}
	/* Qm range check */
	if (!check_bit(op->ldpc_enc.op_flags, RTE_BBDEV_LDPC_INTERLEAVER_BYPASS) &&
			((q_m == 0) || ((q_m > 2) && ((q_m % 2) == 1)) || (q_m > 8))) {
		rte_bbdev_log(ERR, "Qm (%u) is out of range", q_m);
		return -1;
	}
	/* K0 range check */
	if (((k0 % z_c) > 0) || (k0 >= n_cb) || ((k0 >= (Kp - 2 * z_c)) && (k0 < (K - 2 * z_c)))) {
		rte_bbdev_log(ERR, "K0 (%u) is out of range", k0);
		return -1;
	}
	/* E range check */
	if (e <= RTE_MAX(32, z_c)) {
		rte_bbdev_log(ERR, "E is too small %"PRIu32"", e);
		return -1;
	}
	if ((e > 0xFFFF)) {
		rte_bbdev_log(ERR, "E is too large for N3000 %"PRIu32" > 64k", e);
		return -1;
	}
	if (q_m > 0) {
		if (e % q_m > 0) {
			rte_bbdev_log(ERR, "E %"PRIu32" not multiple of qm %d", e, q_m);
			return -1;
		}
	}
	/* Code word in RM range check */
	if (k0 > (Kp - 2 * z_c))
		L = k0 + e;
	else
		L = k0 + e + n_filler;
	Lcb = RTE_MIN(L, n_cb);
	if (ldpc_enc->basegraph == 1) {
		if (Lcb <= 25 * z_c)
			cw = 25 * z_c;
		else if (Lcb <= 27 * z_c)
			cw = 27 * z_c;
		else if (Lcb <= 30 * z_c)
			cw = 30 * z_c;
		else if (Lcb <= 33 * z_c)
			cw = 33 * z_c;
		else if (Lcb <= 44 * z_c)
			cw = 44 * z_c;
		else if (Lcb <= 55 * z_c)
			cw = 55 * z_c;
		else
			cw = 66 * z_c;
	} else {
		if (Lcb <= 15 * z_c)
			cw = 15 * z_c;
		else if (Lcb <= 20 * z_c)
			cw = 20 * z_c;
		else if (Lcb <= 25 * z_c)
			cw = 25 * z_c;
		else if (Lcb <= 30 * z_c)
			cw = 30 * z_c;
		else
			cw = 50 * z_c;
	}
	if (n_cb < Kp - 2 * z_c)
		cw_rm = n_cb;
	else if ((Kp - 2 * z_c <= n_cb) && (n_cb < K - 2 * z_c))
		cw_rm = Kp - 2 * z_c;
	else if ((K - 2 * z_c <= n_cb) && (n_cb < cw))
		cw_rm = n_cb - n_filler;
	else
		cw_rm = cw - n_filler;
	if (cw_rm <= 32) {
		rte_bbdev_log(ERR, "Invalid Ratematching");
		return -1;
	}
	return 0;
}

/* Validates LDPC decoder parameters for VC 5GNR FPGA. */
static inline int
vc_5gnr_validate_ldpc_dec_op(struct rte_bbdev_dec_op *op)
{
	struct rte_bbdev_op_ldpc_dec *ldpc_dec = &op->ldpc_dec;
	int z_c, n_filler, K, Kp, q_m, n_cb, N, k0, crc24;
	int32_t L, Lcb, cw, cw_rm, e;

	if (check_bit(ldpc_dec->op_flags, RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK))
		return 0;
	if (ldpc_dec->input.data == NULL) {
		rte_bbdev_log(ERR, "Invalid input pointer");
		return -1;
	}
	if (ldpc_dec->hard_output.data == NULL) {
		rte_bbdev_log(ERR, "Invalid output pointer");
		return -1;
	}
	if (ldpc_dec->input.length == 0) {
		rte_bbdev_log(ERR, "input is null");
		return -1;
	}
	if ((ldpc_dec->basegraph > 2) || (ldpc_dec->basegraph == 0)) {
		rte_bbdev_log(ERR,
				"BG (%u) is out of range 1 <= value <= 2",
				ldpc_dec->basegraph);
		return -1;
	}
	if (ldpc_dec->iter_max == 0) {
		rte_bbdev_log(ERR,
				"iter_max (%u) is equal to 0",
				ldpc_dec->iter_max);
		return -1;
	}
	if (ldpc_dec->rv_index > 3) {
		rte_bbdev_log(ERR,
				"rv_index (%u) is out of range 0 <= value <= 3",
				ldpc_dec->rv_index);
		return -1;
	}
	if (ldpc_dec->code_block_mode > RTE_BBDEV_CODE_BLOCK) {
		rte_bbdev_log(ERR,
				"code_block_mode (%u) is out of range 0 <= value <= 1",
				ldpc_dec->code_block_mode);
		return -1;
	}
	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_DECODE_BYPASS)) {
		rte_bbdev_log(ERR, "Avoid LDPC Decode bypass");
		return -1;
	}

	z_c = ldpc_dec->z_c;
	/* Check Zc is valid value */
	if ((z_c > 384) || (z_c < 4)) {
		rte_bbdev_log(ERR, "Zc (%u) is out of range", z_c);
		return -1;
	}
	if (z_c > 256) {
		if ((z_c % 32) != 0) {
			rte_bbdev_log(ERR, "Invalid Zc %d", z_c);
			return -1;
		}
	} else if (z_c > 128) {
		if ((z_c % 16) != 0) {
			rte_bbdev_log(ERR, "Invalid Zc %d", z_c);
			return -1;
		}
	} else if (z_c > 64) {
		if ((z_c % 8) != 0) {
			rte_bbdev_log(ERR, "Invalid Zc %d", z_c);
			return -1;
		}
	} else if (z_c > 32) {
		if ((z_c % 4) != 0) {
			rte_bbdev_log(ERR, "Invalid Zc %d", z_c);
			return -1;
		}
	} else if (z_c > 16) {
		if ((z_c % 2) != 0) {
			rte_bbdev_log(ERR, "Invalid Zc %d", z_c);
			return -1;
		}
	}

	n_filler = ldpc_dec->n_filler;
	K = (ldpc_dec->basegraph == 1 ? 22 : 10) * ldpc_dec->z_c;
	Kp = K - n_filler;
	q_m = ldpc_dec->q_m;
	n_cb = ldpc_dec->n_cb;
	N = (ldpc_dec->basegraph == 1 ? N_ZC_1 : N_ZC_2) * z_c;
	k0 = get_k0(n_cb, z_c, ldpc_dec->basegraph, ldpc_dec->rv_index);
	crc24 = 0;
	e = ldpc_dec->cb_params.e;

	if (check_bit(op->ldpc_dec.op_flags, RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK))
		crc24 = 24;

	if (ldpc_dec->code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK) {
		rte_bbdev_log(ERR, "TB mode not supported");
		return -1;
	}
	/* Enforce HARQ input length */
	ldpc_dec->harq_combined_input.length = RTE_MIN((uint32_t) n_cb,
			ldpc_dec->harq_combined_input.length);
	if ((ldpc_dec->harq_combined_input.length == 0) &&
			check_bit(ldpc_dec->op_flags,
			RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE)) {
		rte_bbdev_log(ERR,
				"HARQ input length (%u) should not be null",
				ldpc_dec->harq_combined_input.length);
		return -1;
	}
	if ((ldpc_dec->harq_combined_input.length > 0) &&
			!check_bit(ldpc_dec->op_flags,
			RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE)) {
		ldpc_dec->harq_combined_input.length = 0;
	}

	/* K' range check */
	if (Kp % 8 > 0) {
		rte_bbdev_log(ERR, "K' not byte aligned %u", Kp);
		return -1;
	}
	if ((crc24 > 0) && (Kp < 292)) {
		rte_bbdev_log(ERR, "Invalid CRC24 for small block %u", Kp);
		return -1;
	}
	if (Kp < 24) {
		rte_bbdev_log(ERR, "K' too small %u", Kp);
		return -1;
	}
	if (n_filler >= (K - 2 * z_c)) {
		rte_bbdev_log(ERR, "K - F invalid %u %u", K, n_filler);
		return -1;
	}
	/* Ncb range check */
	if (n_cb != N) {
		rte_bbdev_log(ERR, "Ncb (%u) is out of range K  %d N %d", n_cb, K, N);
		return -1;
	}
	/* Qm range check */
	if (!check_bit(op->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_INTERLEAVER_BYPASS) &&
			((q_m == 0) || ((q_m > 2) && ((q_m % 2) == 1))
			|| (q_m > 8))) {
		rte_bbdev_log(ERR, "Qm (%u) is out of range", q_m);
		return -1;
	}
	/* K0 range check */
	if (((k0 % z_c) > 0) || (k0 >= n_cb) || ((k0 >= (Kp - 2 * z_c)) && (k0 < (K - 2 * z_c)))) {
		rte_bbdev_log(ERR, "K0 (%u) is out of range", k0);
		return -1;
	}
	/* E range check */
	if (e <= RTE_MAX(32, z_c)) {
		rte_bbdev_log(ERR, "E is too small");
		return -1;
	}
	if ((e > 0xFFFF)) {
		rte_bbdev_log(ERR, "E is too large");
		return -1;
	}
	if (q_m > 0) {
		if (e % q_m > 0) {
			rte_bbdev_log(ERR, "E not multiple of qm %d", q_m);
			return -1;
		}
	}
	/* Code word in RM range check */
	if (k0 > (Kp - 2 * z_c))
		L = k0 + e;
	else
		L = k0 + e + n_filler;

	Lcb = RTE_MIN(n_cb, RTE_MAX(L, (int32_t) ldpc_dec->harq_combined_input.length));
	if (ldpc_dec->basegraph == 1) {
		if (Lcb <= 25 * z_c)
			cw = 25 * z_c;
		else if (Lcb <= 27 * z_c)
			cw = 27 * z_c;
		else if (Lcb <= 30 * z_c)
			cw = 30 * z_c;
		else if (Lcb <= 33 * z_c)
			cw = 33 * z_c;
		else if (Lcb <= 44 * z_c)
			cw = 44 * z_c;
		else if (Lcb <= 55 * z_c)
			cw = 55 * z_c;
		else
			cw = 66 * z_c;
	} else {
		if (Lcb <= 15 * z_c)
			cw = 15 * z_c;
		else if (Lcb <= 20 * z_c)
			cw = 20 * z_c;
		else if (Lcb <= 25 * z_c)
			cw = 25 * z_c;
		else if (Lcb <= 30 * z_c)
			cw = 30 * z_c;
		else
			cw = 50 * z_c;
	}
	cw_rm = cw - n_filler;
	if (cw_rm <= 32) {
		rte_bbdev_log(ERR, "Invalid Ratematching");
		return -1;
	}
	return 0;
}

static inline char *
mbuf_append(struct rte_mbuf *m_head, struct rte_mbuf *m, uint16_t len)
{
	if (unlikely(len > rte_pktmbuf_tailroom(m)))
		return NULL;

	char *tail = (char *)m->buf_addr + m->data_off + m->data_len;
	m->data_len = (uint16_t)(m->data_len + len);
	m_head->pkt_len  = (m_head->pkt_len + len);
	return tail;
}

static inline void
fpga_5gnr_mutex_acquisition(struct fpga_5gnr_queue *q)
{
	uint32_t mutex_ctrl, mutex_read, cnt = 0;
	/* Assign a unique id for the duration of the DDR access */
	q->ddr_mutex_uuid = rte_rand();
	/* Request and wait for acquisition of the mutex */
	mutex_ctrl = (q->ddr_mutex_uuid << 16) + 1;
	do {
		if (cnt > 0)
			usleep(FPGA_5GNR_TIMEOUT_CHECK_INTERVAL);
		rte_bbdev_log_debug("Acquiring Mutex for %x", q->ddr_mutex_uuid);
		fpga_5gnr_reg_write_32(q->d->mmio_base, FPGA_5GNR_FEC_MUTEX, mutex_ctrl);
		mutex_read = fpga_5gnr_reg_read_32(q->d->mmio_base, FPGA_5GNR_FEC_MUTEX);
		rte_bbdev_log_debug("Mutex %x cnt %d owner %x",
				mutex_read, cnt, q->ddr_mutex_uuid);
		cnt++;
	} while ((mutex_read >> 16) != q->ddr_mutex_uuid);
}

static inline void
fpga_5gnr_mutex_free(struct fpga_5gnr_queue *q)
{
	uint32_t mutex_ctrl = q->ddr_mutex_uuid << 16;
	fpga_5gnr_reg_write_32(q->d->mmio_base, FPGA_5GNR_FEC_MUTEX, mutex_ctrl);
}

static inline int
fpga_5gnr_harq_write_loopback(struct fpga_5gnr_queue *q,
		struct rte_mbuf *harq_input, uint16_t harq_in_length,
		uint32_t harq_in_offset, uint32_t harq_out_offset)
{
	fpga_5gnr_mutex_acquisition(q);
	uint32_t out_offset = harq_out_offset;
	uint32_t in_offset = harq_in_offset;
	uint32_t left_length = harq_in_length;
	uint32_t reg_32, increment = 0;
	uint64_t *input = NULL;
	uint32_t last_transaction = left_length % FPGA_5GNR_DDR_WR_DATA_LEN_IN_BYTES;
	uint64_t last_word;
	struct fpga_5gnr_fec_device *d = q->d;

	if (last_transaction > 0)
		left_length -= last_transaction;
	if (d->fpga_variant == VC_5GNR_FPGA_VARIANT) {
		/*
		 * Get HARQ buffer size for each VF/PF: When 0x00, there is no
		 * available DDR space for the corresponding VF/PF.
		 */
		reg_32 = fpga_5gnr_reg_read_32(q->d->mmio_base, FPGA_5GNR_FEC_HARQ_BUF_SIZE_REGS);
		if (reg_32 < harq_in_length) {
			left_length = reg_32;
			rte_bbdev_log(ERR, "HARQ in length > HARQ buffer size");
		}
	}

	input = rte_pktmbuf_mtod_offset(harq_input, uint64_t *, in_offset);

	while (left_length > 0) {
		if (fpga_5gnr_reg_read_8(q->d->mmio_base, FPGA_5GNR_FEC_DDR4_ADDR_RDY_REGS) ==  1) {
			if (d->fpga_variant == AGX100_FPGA_VARIANT) {
				fpga_5gnr_reg_write_32(q->d->mmio_base,
						FPGA_5GNR_FEC_DDR4_WR_ADDR_REGS,
						out_offset >> 3);
			} else {
				fpga_5gnr_reg_write_32(q->d->mmio_base,
						FPGA_5GNR_FEC_DDR4_WR_ADDR_REGS,
						out_offset);
			}
			fpga_5gnr_reg_write_64(q->d->mmio_base,
					FPGA_5GNR_FEC_DDR4_WR_DATA_REGS,
					input[increment]);
			left_length -= FPGA_5GNR_DDR_WR_DATA_LEN_IN_BYTES;
			out_offset += FPGA_5GNR_DDR_WR_DATA_LEN_IN_BYTES;
			increment++;
			fpga_5gnr_reg_write_8(q->d->mmio_base, FPGA_5GNR_FEC_DDR4_WR_DONE_REGS, 1);
		}
	}
	while (last_transaction > 0) {
		if (fpga_5gnr_reg_read_8(q->d->mmio_base, FPGA_5GNR_FEC_DDR4_ADDR_RDY_REGS) ==  1) {
			if (d->fpga_variant == AGX100_FPGA_VARIANT) {
				fpga_5gnr_reg_write_32(q->d->mmio_base,
						FPGA_5GNR_FEC_DDR4_WR_ADDR_REGS,
						out_offset >> 3);
			} else {
				fpga_5gnr_reg_write_32(q->d->mmio_base,
						FPGA_5GNR_FEC_DDR4_WR_ADDR_REGS,
						out_offset);
			}
			last_word = input[increment];
			last_word &= (uint64_t)(1ULL << (last_transaction * 4)) - 1;
			fpga_5gnr_reg_write_64(q->d->mmio_base,
					FPGA_5GNR_FEC_DDR4_WR_DATA_REGS,
					last_word);
			fpga_5gnr_reg_write_8(q->d->mmio_base, FPGA_5GNR_FEC_DDR4_WR_DONE_REGS, 1);
			last_transaction = 0;
		}
	}
	fpga_5gnr_mutex_free(q);
	return 1;
}

static inline int
fpga_5gnr_harq_read_loopback(struct fpga_5gnr_queue *q,
		struct rte_mbuf *harq_output, uint16_t harq_in_length,
		uint32_t harq_in_offset, uint32_t harq_out_offset)
{
	fpga_5gnr_mutex_acquisition(q);
	uint32_t left_length, in_offset = harq_in_offset;
	uint64_t reg;
	uint32_t increment = 0;
	uint64_t *input = NULL;
	uint32_t last_transaction = harq_in_length % FPGA_5GNR_DDR_WR_DATA_LEN_IN_BYTES;
	struct fpga_5gnr_fec_device *d = q->d;

	if (last_transaction > 0)
		harq_in_length += (8 - last_transaction);

	if (d->fpga_variant == VC_5GNR_FPGA_VARIANT) {
		reg = fpga_5gnr_reg_read_32(q->d->mmio_base, FPGA_5GNR_FEC_HARQ_BUF_SIZE_REGS);
		if (reg < harq_in_length) {
			harq_in_length = reg;
			rte_bbdev_log(ERR, "HARQ in length > HARQ buffer size");
		}
	}

	if (!mbuf_append(harq_output, harq_output, harq_in_length)) {
		rte_bbdev_log(ERR, "HARQ output buffer warning %d %d",
				harq_output->buf_len - rte_pktmbuf_headroom(harq_output),
				harq_in_length);
		harq_in_length = harq_output->buf_len - rte_pktmbuf_headroom(harq_output);
		if (!mbuf_append(harq_output, harq_output, harq_in_length)) {
			rte_bbdev_log(ERR, "HARQ output buffer issue %d %d",
					harq_output->buf_len, harq_in_length);
			return -1;
		}
	}
	left_length = harq_in_length;

	input = rte_pktmbuf_mtod_offset(harq_output, uint64_t *, harq_out_offset);

	while (left_length > 0) {
		if (d->fpga_variant == AGX100_FPGA_VARIANT) {
			fpga_5gnr_reg_write_32(q->d->mmio_base,
					FPGA_5GNR_FEC_DDR4_RD_ADDR_REGS,
					in_offset >> 3);
		} else {
			fpga_5gnr_reg_write_32(q->d->mmio_base,
					FPGA_5GNR_FEC_DDR4_RD_ADDR_REGS,
					in_offset);
		}
		fpga_5gnr_reg_write_8(q->d->mmio_base, FPGA_5GNR_FEC_DDR4_RD_DONE_REGS, 1);
		reg = fpga_5gnr_reg_read_8(q->d->mmio_base, FPGA_5GNR_FEC_DDR4_RD_RDY_REGS);
		while (reg != 1) {
			reg = fpga_5gnr_reg_read_8(q->d->mmio_base, FPGA_5GNR_FEC_DDR4_RD_RDY_REGS);
			if (reg == FPGA_5GNR_DDR_OVERFLOW) {
				rte_bbdev_log(ERR, "Read address is overflow!");
				return -1;
			}
		}
		input[increment] = fpga_5gnr_reg_read_64(q->d->mmio_base,
			FPGA_5GNR_FEC_DDR4_RD_DATA_REGS);
		left_length -= FPGA_5GNR_DDR_RD_DATA_LEN_IN_BYTES;
		in_offset += FPGA_5GNR_DDR_WR_DATA_LEN_IN_BYTES;
		increment++;
		if (d->fpga_variant == AGX100_FPGA_VARIANT)
			fpga_5gnr_reg_write_8(q->d->mmio_base, FPGA_5GNR_FEC_DDR4_RD_RDY_REGS, 0);
		else
			fpga_5gnr_reg_write_8(q->d->mmio_base, FPGA_5GNR_FEC_DDR4_RD_DONE_REGS, 0);
	}
	fpga_5gnr_mutex_free(q);
	return 1;
}

static inline int
enqueue_ldpc_enc_one_op_cb(struct fpga_5gnr_queue *q, struct rte_bbdev_enc_op *op,
		uint16_t desc_offset)
{
	union vc_5gnr_dma_desc *vc_5gnr_desc;
	union agx100_dma_desc *agx100_desc;
	int ret;
	uint8_t c, crc24_bits = 0;
	struct rte_bbdev_op_ldpc_enc *enc = &op->ldpc_enc;
	uint16_t in_offset = enc->input.offset;
	uint16_t out_offset = enc->output.offset;
	struct rte_mbuf *m_in = enc->input.data;
	struct rte_mbuf *m_out = enc->output.data;
	struct rte_mbuf *m_out_head = enc->output.data;
	uint32_t in_length, out_length, e;
	uint16_t total_left = enc->input.length;
	uint16_t ring_offset;
	uint16_t K, k_;
	struct fpga_5gnr_fec_device *d = q->d;

	if (d->fpga_variant == VC_5GNR_FPGA_VARIANT) {
		if (vc_5gnr_validate_ldpc_enc_op(op) == -1) {
			rte_bbdev_log(ERR, "LDPC encoder validation rejected");
			return -EINVAL;
		}
	}

	/* Clear op status */
	op->status = 0;

	if (m_in == NULL || m_out == NULL) {
		rte_bbdev_log(ERR, "Invalid mbuf pointer");
		op->status = 1 << RTE_BBDEV_DATA_ERROR;
		return -EINVAL;
	}

	if (enc->op_flags & RTE_BBDEV_LDPC_CRC_24B_ATTACH)
		crc24_bits = 24;

	if (enc->code_block_mode == RTE_BBDEV_TRANSPORT_BLOCK) {
		/* TODO: For Transport Block mode. */
		rte_bbdev_log(ERR, "Transport Block not supported yet");
		return -1;
	}
	/* For Code Block mode. */
	c = 1;
	e = enc->cb_params.e;

	/* Update total_left */
	K = (enc->basegraph == 1 ? 22 : 10) * enc->z_c;
	k_ = K - enc->n_filler;
	in_length = (k_ - crc24_bits) >> 3;
	out_length = (e + 7) >> 3;

	total_left = rte_pktmbuf_data_len(m_in) - in_offset;

	/* Update offsets */
	if (total_left != in_length) {
		op->status |= 1 << RTE_BBDEV_DATA_ERROR;
		rte_bbdev_log(ERR,
				"Mismatch between mbuf length and included CBs sizes %d",
				total_left);
	}

	mbuf_append(m_out_head, m_out, out_length);

	/* Offset into the ring. */
	ring_offset = fpga_5gnr_desc_idx_tail(q, desc_offset);

	if (d->fpga_variant == VC_5GNR_FPGA_VARIANT) {
		/* Setup DMA Descriptor. */
		vc_5gnr_desc = vc_5gnr_get_desc_tail(q, desc_offset);
		ret = vc_5gnr_dma_desc_te_fill(op, &vc_5gnr_desc->enc_req, m_in, m_out,
				k_, e, in_offset, out_offset, ring_offset, c);
	} else {
		/* Setup DMA Descriptor. */
		agx100_desc = agx100_get_desc_tail(q, desc_offset);
		ret = agx100_dma_desc_le_fill(op, &agx100_desc->enc_req, m_in, m_out,
				k_, e, in_offset, out_offset, ring_offset, c);
	}

	if (unlikely(ret < 0))
		return ret;

	/* Update lengths */
	total_left -= in_length;
	op->ldpc_enc.output.length += out_length;

	if (total_left > 0) {
		rte_bbdev_log(ERR,
			"Mismatch between mbuf length and included CB sizes: mbuf len %u, cb len %u",
				total_left, in_length);
		return -1;
	}

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	if (d->fpga_variant == VC_5GNR_FPGA_VARIANT)
		vc_5gnr_print_dma_enc_desc_debug_info(vc_5gnr_desc);
	else
		agx100_print_dma_enc_desc_debug_info(agx100_desc);
#endif
	return 1;
}

static inline int
vc_5gnr_enqueue_ldpc_dec_one_op_cb(struct fpga_5gnr_queue *q, struct rte_bbdev_dec_op *op,
		uint16_t desc_offset)
{
	union vc_5gnr_dma_desc *desc;
	int ret;
	uint16_t ring_offset;
	uint8_t c;
	uint16_t e, in_length, out_length, k0, l, seg_total_left, sys_cols;
	uint16_t K, parity_offset, harq_in_length = 0, harq_out_length = 0;
	uint16_t crc24_overlap = 0;
	struct rte_bbdev_op_ldpc_dec *dec = &op->ldpc_dec;
	struct rte_mbuf *m_in = dec->input.data;
	struct rte_mbuf *m_out = dec->hard_output.data;
	struct rte_mbuf *m_out_head = dec->hard_output.data;
	uint16_t in_offset = dec->input.offset;
	uint16_t out_offset = dec->hard_output.offset;
	uint32_t harq_offset = 0;

	if (vc_5gnr_validate_ldpc_dec_op(op) == -1) {
		rte_bbdev_log(ERR, "LDPC decoder validation rejected");
		return -EINVAL;
	}

	/* Clear op status */
	op->status = 0;

	/* Setup DMA Descriptor */
	ring_offset = fpga_5gnr_desc_idx_tail(q, desc_offset);
	desc = vc_5gnr_get_desc_tail(q, desc_offset);

	if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK)) {
		struct rte_mbuf *harq_in = dec->harq_combined_input.data;
		struct rte_mbuf *harq_out = dec->harq_combined_output.data;
		harq_in_length = dec->harq_combined_input.length;
		uint32_t harq_in_offset = dec->harq_combined_input.offset;
		uint32_t harq_out_offset = dec->harq_combined_output.offset;

		if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE)) {
			ret = fpga_5gnr_harq_write_loopback(q, harq_in,
					harq_in_length, harq_in_offset,
					harq_out_offset);
		} else if (check_bit(dec->op_flags,
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE
				)) {
			ret = fpga_5gnr_harq_read_loopback(q, harq_out,
				harq_in_length, harq_in_offset,
				harq_out_offset);
			dec->harq_combined_output.length = harq_in_length;
		} else {
			rte_bbdev_log(ERR, "OP flag Err!");
			ret = -1;
		}

		/* Set descriptor for dequeue */
		desc->dec_req.done = 1;
		desc->dec_req.error = 0;
		desc->dec_req.op_addr = op;
		desc->dec_req.cbs_in_op = 1;

		/* Mark this dummy descriptor to be dropped by HW */
		desc->dec_req.desc_idx = (ring_offset + 1) & q->sw_ring_wrap_mask;

		return ret; /* Error or number of CB */
	}

	if (m_in == NULL || m_out == NULL) {
		rte_bbdev_log(ERR, "Invalid mbuf pointer");
		op->status = 1 << RTE_BBDEV_DATA_ERROR;
		return -1;
	}

	c = 1;
	e = dec->cb_params.e;

	if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP))
		crc24_overlap = 24;

	sys_cols = (dec->basegraph == 1) ? 22 : 10;
	K = sys_cols * dec->z_c;
	parity_offset = K - 2 * dec->z_c;

	out_length = ((K - crc24_overlap - dec->n_filler) >> 3);
	in_length = e;
	seg_total_left = dec->input.length;

	if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE))
		harq_in_length = RTE_MIN(dec->harq_combined_input.length, (uint32_t)dec->n_cb);

	if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE)) {
		k0 = get_k0(dec->n_cb, dec->z_c, dec->basegraph, dec->rv_index);
		if (k0 > parity_offset)
			l = k0 + e;
		else
			l = k0 + e + dec->n_filler;
		harq_out_length = RTE_MIN(RTE_MAX(harq_in_length, l), dec->n_cb);
		dec->harq_combined_output.length = harq_out_length;
	}

	mbuf_append(m_out_head, m_out, out_length);

	if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE))
		harq_offset = dec->harq_combined_input.offset;
	else if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE))
		harq_offset = dec->harq_combined_output.offset;

	if ((harq_offset & 0x3FF) > 0) {
		rte_bbdev_log(ERR, "Invalid HARQ offset %d", harq_offset);
		op->status = 1 << RTE_BBDEV_DATA_ERROR;
		return -1;
	}

	ret = vc_5gnr_dma_desc_ld_fill(op, &desc->dec_req, m_in, m_out,
		harq_in_length, in_offset, out_offset, harq_offset,
		ring_offset, c);

	if (unlikely(ret < 0))
		return ret;
	/* Update lengths */
	seg_total_left -= in_length;
	op->ldpc_dec.hard_output.length += out_length;
	if (seg_total_left > 0) {
		rte_bbdev_log(ERR,
				"Mismatch between mbuf length and included CB sizes: mbuf len %u, cb len %u",
				seg_total_left, in_length);
		return -1;
	}

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	vc_5gnr_print_dma_dec_desc_debug_info(desc);
#endif

	return 1;
}

static inline int
agx100_enqueue_ldpc_dec_one_op_cb(struct fpga_5gnr_queue *q, struct rte_bbdev_dec_op *op,
		uint16_t desc_offset)
{
	union agx100_dma_desc *desc;
	int ret;
	uint16_t ring_offset;
	uint8_t c;
	uint16_t e, in_length, out_length, k0, l, seg_total_left, sys_cols;
	uint16_t K, parity_offset, harq_in_length = 0, harq_out_length = 0;
	uint16_t crc24_overlap = 0;
	struct rte_bbdev_op_ldpc_dec *dec = &op->ldpc_dec;
	struct rte_mbuf *m_in = dec->input.data;
	struct rte_mbuf *m_out = dec->hard_output.data;
	struct rte_mbuf *m_out_head = dec->hard_output.data;
	uint16_t in_offset = dec->input.offset;
	uint16_t out_offset = dec->hard_output.offset;
	uint32_t harq_in_offset = 0;
	uint32_t harq_out_offset = 0;

	/* Clear op status. */
	op->status = 0;

	/* Setup DMA Descriptor. */
	ring_offset = fpga_5gnr_desc_idx_tail(q, desc_offset);
	desc = agx100_get_desc_tail(q, desc_offset);

	if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK)) {
		struct rte_mbuf *harq_in = dec->harq_combined_input.data;
		struct rte_mbuf *harq_out = dec->harq_combined_output.data;
		harq_in_length = dec->harq_combined_input.length;
		uint32_t harq_in_offset = dec->harq_combined_input.offset;
		uint32_t harq_out_offset = dec->harq_combined_output.offset;

		if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE)) {
			ret = fpga_5gnr_harq_write_loopback(q, harq_in,
					harq_in_length, harq_in_offset,
					harq_out_offset);
		} else if (check_bit(dec->op_flags,
				RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE)) {
			ret = fpga_5gnr_harq_read_loopback(q, harq_out,
					harq_in_length, harq_in_offset,
					harq_out_offset);
			dec->harq_combined_output.length = harq_in_length;
		} else {
			rte_bbdev_log(ERR, "OP flag Err!");
			ret = -1;
		}

		/* Set descriptor for dequeue. */
		desc->dec_req.done = 1;
		desc->dec_req.error_code = 0;
		desc->dec_req.error_msg = 0;
		desc->dec_req.op_addr = op;
		desc->dec_req.cbs_in_op = 1;

		/* Mark this dummy descriptor to be dropped by HW. */
		desc->dec_req.desc_idx = (ring_offset + 1) & q->sw_ring_wrap_mask;

		return ret; /* Error or number of CB. */
	}

	if (m_in == NULL || m_out == NULL) {
		rte_bbdev_log(ERR, "Invalid mbuf pointer");
		op->status = 1 << RTE_BBDEV_DATA_ERROR;
		return -1;
	}

	c = 1;
	e = dec->cb_params.e;

	if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP))
		crc24_overlap = 24;

	sys_cols = (dec->basegraph == 1) ? 22 : 10;
	K = sys_cols * dec->z_c;
	parity_offset = K - 2 * dec->z_c;

	out_length = ((K - crc24_overlap - dec->n_filler) >> 3);
	in_length = e;
	seg_total_left = dec->input.length;

	if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE))
		harq_in_length = RTE_MIN(dec->harq_combined_input.length, (uint32_t)dec->n_cb);

	if (check_bit(dec->op_flags, RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE)) {
		k0 = get_k0(dec->n_cb, dec->z_c, dec->basegraph, dec->rv_index);
		if (k0 > parity_offset)
			l = k0 + e;
		else
			l = k0 + e + dec->n_filler;
		harq_out_length = RTE_MIN(RTE_MAX(harq_in_length, l), dec->n_cb);
		dec->harq_combined_output.length = harq_out_length;
	}

	mbuf_append(m_out_head, m_out, out_length);
	harq_in_offset = dec->harq_combined_input.offset;
	harq_out_offset = dec->harq_combined_output.offset;

	ret = agx100_dma_desc_ld_fill(op, &desc->dec_req, m_in, m_out,
		harq_in_length, in_offset, out_offset, harq_in_offset,
		harq_out_offset, ring_offset, c);

	if (unlikely(ret < 0))
		return ret;
	/* Update lengths. */
	seg_total_left -= in_length;
	op->ldpc_dec.hard_output.length += out_length;
	if (seg_total_left > 0) {
		rte_bbdev_log(ERR,
				"Mismatch between mbuf length and included CB sizes: mbuf len %u, cb len %u",
				seg_total_left, in_length);
		return -1;
	}

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	agx100_print_dma_dec_desc_debug_info(desc);
#endif

	return 1;
}

static uint16_t
fpga_5gnr_enqueue_ldpc_enc(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	uint16_t i, total_enqueued_cbs = 0;
	int32_t avail;
	int enqueued_cbs;
	struct fpga_5gnr_queue *q = q_data->queue_private;
	union vc_5gnr_dma_desc *vc_5gnr_desc;
	union agx100_dma_desc *agx100_desc;
	struct fpga_5gnr_fec_device *d = q->d;

	/* Check if queue is not full */
	if (unlikely((fpga_5gnr_desc_idx_tail(q, 1)) == q->head_free_desc))
		return 0;

	/* Calculates available space */
	avail = (q->head_free_desc > q->tail) ?
		q->head_free_desc - q->tail - 1 :
		q->ring_ctrl_reg.ring_size + q->head_free_desc - q->tail - 1;

	for (i = 0; i < num; ++i) {
		/* Check if there is available space for further
		 * processing
		 */
		if (unlikely(avail - 1 < 0))
			break;
		avail -= 1;
		enqueued_cbs = enqueue_ldpc_enc_one_op_cb(q, ops[i], total_enqueued_cbs);

		if (enqueued_cbs < 0)
			break;

		total_enqueued_cbs += enqueued_cbs;

		rte_bbdev_log_debug("enqueuing enc ops [%d/%d] | head %d | tail %d",
				total_enqueued_cbs, num,
				q->head_free_desc, q->tail);
	}

	/* Set interrupt bit for last CB in enqueued ops. FPGA issues interrupt
	 * only when all previous CBs were already processed.
	 */
	if (d->fpga_variant == VC_5GNR_FPGA_VARIANT) {
		vc_5gnr_desc = vc_5gnr_get_desc_tail(q, total_enqueued_cbs - 1);
		vc_5gnr_desc->enc_req.irq_en = q->irq_enable;
	} else {
		agx100_desc = agx100_get_desc_tail(q, total_enqueued_cbs - 1);
		agx100_desc->enc_req.int_en = q->irq_enable;
	}

	fpga_5gnr_dma_enqueue(q, total_enqueued_cbs, &q_data->queue_stats);

	/* Update stats */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;

	return i;
}

static uint16_t
fpga_5gnr_enqueue_ldpc_dec(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	uint16_t i, total_enqueued_cbs = 0;
	int32_t avail;
	int enqueued_cbs;
	struct fpga_5gnr_queue *q = q_data->queue_private;
	union vc_5gnr_dma_desc *vc_5gnr_desc;
	union agx100_dma_desc *agx100_desc;
	struct fpga_5gnr_fec_device *d = q->d;

	/* Check if queue is not full */
	if (unlikely((fpga_5gnr_desc_idx_tail(q, 1)) == q->head_free_desc))
		return 0;

	/* Calculates available space */
	avail = (q->head_free_desc > q->tail) ?
		q->head_free_desc - q->tail - 1 :
		q->ring_ctrl_reg.ring_size + q->head_free_desc - q->tail - 1;

	for (i = 0; i < num; ++i) {

		/* Check if there is available space for further
		 * processing
		 */
		if (unlikely(avail - 1 < 0))
			break;
		avail -= 1;
		if (q->d->fpga_variant == VC_5GNR_FPGA_VARIANT) {
			enqueued_cbs = vc_5gnr_enqueue_ldpc_dec_one_op_cb(q, ops[i],
					total_enqueued_cbs);
		} else {
			enqueued_cbs = agx100_enqueue_ldpc_dec_one_op_cb(q, ops[i],
					total_enqueued_cbs);
		}

		if (enqueued_cbs < 0)
			break;

		total_enqueued_cbs += enqueued_cbs;

		rte_bbdev_log_debug("enqueuing dec ops [%d/%d] | head %d | tail %d",
				total_enqueued_cbs, num,
				q->head_free_desc, q->tail);
	}

	/* Update stats */
	q_data->queue_stats.enqueued_count += i;
	q_data->queue_stats.enqueue_err_count += num - i;

	/* Set interrupt bit for last CB in enqueued ops. FPGA issues interrupt
	 * only when all previous CBs were already processed.
	 */
	if (d->fpga_variant == VC_5GNR_FPGA_VARIANT) {
		vc_5gnr_desc = vc_5gnr_get_desc_tail(q, total_enqueued_cbs - 1);
		vc_5gnr_desc->enc_req.irq_en = q->irq_enable;
	} else {
		agx100_desc = agx100_get_desc_tail(q, total_enqueued_cbs - 1);
		agx100_desc->enc_req.int_en = q->irq_enable;
	}

	fpga_5gnr_dma_enqueue(q, total_enqueued_cbs, &q_data->queue_stats);
	return i;
}


static inline int
vc_5gnr_dequeue_ldpc_enc_one_op_cb(struct fpga_5gnr_queue *q, struct rte_bbdev_enc_op **op,
		uint16_t desc_offset)
{
	union vc_5gnr_dma_desc *desc;
	int desc_error;
	/* Set current desc */
	desc = vc_5gnr_get_desc(q, desc_offset);

	/*check if done */
	if (desc->enc_req.done == 0)
		return -1;

	/* make sure the response is read atomically */
	rte_smp_rmb();

	rte_bbdev_log_debug("DMA response desc %p", desc);

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	vc_5gnr_print_dma_enc_desc_debug_info(desc);
#endif
	*op = desc->enc_req.op_addr;
	/* Check the descriptor error field, return 1 on error */
	desc_error = vc_5gnr_check_desc_error(desc->enc_req.error);
	(*op)->status = desc_error << RTE_BBDEV_DATA_ERROR;

	return 1;
}

static inline int
agx100_dequeue_ldpc_enc_one_op_cb(struct fpga_5gnr_queue *q, struct rte_bbdev_enc_op **op,
		uint16_t desc_offset)
{
	union agx100_dma_desc *desc;
	int desc_error;

	/* Set current desc. */
	desc = agx100_get_desc(q, desc_offset);
	/*check if done */
	if (desc->enc_req.done == 0)
		return -1;

	/* make sure the response is read atomically. */
	rte_smp_rmb();

	rte_bbdev_log_debug("DMA response desc %p", desc);

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	agx100_print_dma_enc_desc_debug_info(desc);
#endif
	*op = desc->enc_req.op_addr;
	/* Check the descriptor error field, return 1 on error. */
	desc_error = agx100_check_desc_error(desc->enc_req.error_code,
			desc->enc_req.error_msg);

	(*op)->status = desc_error << RTE_BBDEV_DATA_ERROR;

	return 1;
}

static inline int
vc_5gnr_dequeue_ldpc_dec_one_op_cb(struct fpga_5gnr_queue *q, struct rte_bbdev_dec_op **op,
		uint16_t desc_offset)
{
	union vc_5gnr_dma_desc *desc;
	int desc_error;

	/* Set descriptor */
	desc = vc_5gnr_get_desc(q, desc_offset);

	/* Verify done bit is set */
	if (desc->dec_req.done == 0)
		return -1;

	/* make sure the response is read atomically */
	rte_smp_rmb();

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	vc_5gnr_print_dma_dec_desc_debug_info(desc);
#endif

	*op = desc->dec_req.op_addr;

	if (check_bit((*op)->ldpc_dec.op_flags,
			RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK)) {
		(*op)->status = 0;
		return 1;
	}

	/* FPGA reports iterations based on round-up minus 1 */
	(*op)->ldpc_dec.iter_count = desc->dec_req.iter + 1;

	/* CRC Check criteria */
	if (desc->dec_req.crc24b_ind && !(desc->dec_req.crcb_pass))
		(*op)->status = 1 << RTE_BBDEV_CRC_ERROR;

	/* et_pass = 0 when decoder fails */
	(*op)->status |= !(desc->dec_req.et_pass) << RTE_BBDEV_SYNDROME_ERROR;

	/* Check the descriptor error field, return 1 on error */
	desc_error = vc_5gnr_check_desc_error(desc->dec_req.error);

	(*op)->status |= desc_error << RTE_BBDEV_DATA_ERROR;

	return 1;
}

static inline int
agx100_dequeue_ldpc_dec_one_op_cb(struct fpga_5gnr_queue *q, struct rte_bbdev_dec_op **op,
		uint16_t desc_offset)
{
	union agx100_dma_desc *desc;
	int desc_error;

	/* Set descriptor. */
	desc = agx100_get_desc(q, desc_offset);
	/* Verify done bit is set. */
	if (desc->dec_req.done == 0)
		return -1;

	/* make sure the response is read atomically. */
	rte_smp_rmb();

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	agx100_print_dma_dec_desc_debug_info(desc);
#endif

	*op = desc->dec_req.op_addr;

	if (check_bit((*op)->ldpc_dec.op_flags, RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK)) {
		(*op)->status = 0;
		return 1;
	}

	/* FPGA reports iterations based on round-up minus 1. */
	(*op)->ldpc_dec.iter_count = desc->dec_req.max_iter_ret + 1;

	/* CRC Check criteria. */
	if (desc->dec_req.crc24b_ind && !(desc->dec_req.cb_crc_all_pass))
		(*op)->status = 1 << RTE_BBDEV_CRC_ERROR;

	/* et_pass = 0 when decoder fails. */
	(*op)->status |= !(desc->dec_req.cb_all_et_pass) << RTE_BBDEV_SYNDROME_ERROR;

	/* Check the descriptor error field, return 1 on error. */
	desc_error = agx100_check_desc_error(desc->dec_req.error_code,
			desc->dec_req.error_msg);

	(*op)->status |= desc_error << RTE_BBDEV_DATA_ERROR;
	return 1;
}

static uint16_t
fpga_5gnr_dequeue_ldpc_enc(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t num)
{
	struct fpga_5gnr_queue *q = q_data->queue_private;
	uint32_t avail = (q->tail - q->head_free_desc) & q->sw_ring_wrap_mask;
	uint16_t i;
	uint16_t dequeued_cbs = 0;
	int ret;

	for (i = 0; (i < num) && (dequeued_cbs < avail); ++i) {
		if (q->d->fpga_variant == VC_5GNR_FPGA_VARIANT)
			ret = vc_5gnr_dequeue_ldpc_enc_one_op_cb(q, &ops[i], dequeued_cbs);
		else
			ret = agx100_dequeue_ldpc_enc_one_op_cb(q, &ops[i], dequeued_cbs);

		if (ret < 0)
			break;

		dequeued_cbs += ret;

		rte_bbdev_log_debug("dequeuing enc ops [%d/%d] | head %d | tail %d",
				dequeued_cbs, num, q->head_free_desc, q->tail);
	}

	/* Update head */
	q->head_free_desc = fpga_5gnr_desc_idx(q, dequeued_cbs);

	/* Update stats */
	q_data->queue_stats.dequeued_count += i;

	return i;
}

static uint16_t
fpga_5gnr_dequeue_ldpc_dec(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t num)
{
	struct fpga_5gnr_queue *q = q_data->queue_private;
	uint32_t avail = (q->tail - q->head_free_desc) & q->sw_ring_wrap_mask;
	uint16_t i;
	uint16_t dequeued_cbs = 0;
	int ret;

	for (i = 0; (i < num) && (dequeued_cbs < avail); ++i) {
		if (q->d->fpga_variant == VC_5GNR_FPGA_VARIANT)
			ret = vc_5gnr_dequeue_ldpc_dec_one_op_cb(q, &ops[i], dequeued_cbs);
		else
			ret = agx100_dequeue_ldpc_dec_one_op_cb(q, &ops[i], dequeued_cbs);

		if (ret < 0)
			break;

		dequeued_cbs += ret;

		rte_bbdev_log_debug("dequeuing dec ops [%d/%d] | head %d | tail %d",
				dequeued_cbs, num, q->head_free_desc, q->tail);
	}

	/* Update head */
	q->head_free_desc = fpga_5gnr_desc_idx(q, dequeued_cbs);

	/* Update stats */
	q_data->queue_stats.dequeued_count += i;

	return i;
}


/* Initialization Function */
static void
fpga_5gnr_fec_init(struct rte_bbdev *dev, struct rte_pci_driver *drv)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);

	dev->dev_ops = &fpga_5gnr_ops;
	dev->enqueue_ldpc_enc_ops = fpga_5gnr_enqueue_ldpc_enc;
	dev->enqueue_ldpc_dec_ops = fpga_5gnr_enqueue_ldpc_dec;
	dev->dequeue_ldpc_enc_ops = fpga_5gnr_dequeue_ldpc_enc;
	dev->dequeue_ldpc_dec_ops = fpga_5gnr_dequeue_ldpc_dec;

	/* Device variant specific handling. */
	if ((pci_dev->id.device_id == AGX100_PF_DEVICE_ID) ||
			(pci_dev->id.device_id == AGX100_VF_DEVICE_ID)) {
		((struct fpga_5gnr_fec_device *) dev->data->dev_private)->fpga_variant =
				AGX100_FPGA_VARIANT;
		((struct fpga_5gnr_fec_device *) dev->data->dev_private)->pf_device =
				!strcmp(drv->driver.name, RTE_STR(FPGA_5GNR_FEC_PF_DRIVER_NAME));
		((struct fpga_5gnr_fec_device *) dev->data->dev_private)->mmio_base =
				pci_dev->mem_resource[0].addr;
		/* Maximum number of queues possible for this device. */
		((struct fpga_5gnr_fec_device *) dev->data->dev_private)->total_num_queues =
				fpga_5gnr_reg_read_32(pci_dev->mem_resource[0].addr,
				FPGA_5GNR_FEC_VERSION_ID) >> 24;
	} else {
		((struct fpga_5gnr_fec_device *) dev->data->dev_private)->fpga_variant =
				VC_5GNR_FPGA_VARIANT;
		((struct fpga_5gnr_fec_device *) dev->data->dev_private)->pf_device =
				!strcmp(drv->driver.name, RTE_STR(FPGA_5GNR_FEC_PF_DRIVER_NAME));
		((struct fpga_5gnr_fec_device *) dev->data->dev_private)->mmio_base =
				pci_dev->mem_resource[0].addr;
		((struct fpga_5gnr_fec_device *) dev->data->dev_private)->total_num_queues =
				VC_5GNR_TOTAL_NUM_QUEUES;
	}

	rte_bbdev_log_debug(
			"Init device %s [%s] @ virtaddr %p phyaddr %#"PRIx64,
			drv->driver.name, dev->data->name,
			(void *)pci_dev->mem_resource[0].addr,
			pci_dev->mem_resource[0].phys_addr);
}

static int
fpga_5gnr_fec_probe(struct rte_pci_driver *pci_drv,
	struct rte_pci_device *pci_dev)
{
	struct rte_bbdev *bbdev = NULL;
	char dev_name[RTE_BBDEV_NAME_MAX_LEN];
	struct fpga_5gnr_fec_device *d;

	if (pci_dev == NULL) {
		rte_bbdev_log(ERR, "NULL PCI device");
		return -EINVAL;
	}

	rte_pci_device_name(&pci_dev->addr, dev_name, sizeof(dev_name));

	/* Allocate memory to be used privately by drivers */
	bbdev = rte_bbdev_allocate(pci_dev->device.name);
	if (bbdev == NULL)
		return -ENODEV;

	/* allocate device private memory */
	bbdev->data->dev_private = rte_zmalloc_socket(dev_name,
			sizeof(struct fpga_5gnr_fec_device),
			RTE_CACHE_LINE_SIZE,
			pci_dev->device.numa_node);

	if (bbdev->data->dev_private == NULL) {
		rte_bbdev_log(CRIT,
				"Allocate of %zu bytes for device \"%s\" failed",
				sizeof(struct fpga_5gnr_fec_device), dev_name);
				rte_bbdev_release(bbdev);
			return -ENOMEM;
	}

	/* Fill HW specific part of device structure */
	bbdev->device = &pci_dev->device;
	bbdev->intr_handle = pci_dev->intr_handle;
	bbdev->data->socket_id = pci_dev->device.numa_node;

	/* Invoke FPGA 5GNR FEC device initialization function */
	fpga_5gnr_fec_init(bbdev, pci_drv);

	rte_bbdev_log_debug("bbdev id = %u [%s]",
			bbdev->data->dev_id, dev_name);

	d = bbdev->data->dev_private;
	if (d->fpga_variant == VC_5GNR_FPGA_VARIANT) {
		uint32_t version_id = fpga_5gnr_reg_read_32(d->mmio_base, FPGA_5GNR_FEC_VERSION_ID);
		rte_bbdev_log(INFO, "Vista Creek FPGA RTL v%u.%u",
				((uint16_t)(version_id >> 16)), ((uint16_t)version_id));
	} else {
		uint32_t version_num_queues = fpga_5gnr_reg_read_32(d->mmio_base,
				FPGA_5GNR_FEC_VERSION_ID);
		uint8_t major_version_id = version_num_queues >> 16;
		uint8_t minor_version_id = version_num_queues >> 8;
		uint8_t patch_id = version_num_queues;

		rte_bbdev_log(INFO, "AGX100 RTL v%u.%u.%u",
				major_version_id, minor_version_id, patch_id);
	}

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	print_static_reg_debug_info(d->mmio_base, d->fpga_variant);
#endif
	return 0;
}

static int
fpga_5gnr_fec_remove(struct rte_pci_device *pci_dev)
{
	struct rte_bbdev *bbdev;
	int ret;
	uint8_t dev_id;

	if (pci_dev == NULL)
		return -EINVAL;

	/* Find device */
	bbdev = rte_bbdev_get_named_dev(pci_dev->device.name);
	if (bbdev == NULL) {
		rte_bbdev_log(CRIT,
				"Couldn't find HW dev \"%s\" to uninitialise it",
				pci_dev->device.name);
		return -ENODEV;
	}
	dev_id = bbdev->data->dev_id;

	/* free device private memory before close */
	rte_free(bbdev->data->dev_private);

	/* Close device */
	ret = rte_bbdev_close(dev_id);
	if (ret < 0)
		rte_bbdev_log(ERR,
				"Device %i failed to close during uninit: %i",
				dev_id, ret);

	/* release bbdev from library */
	ret = rte_bbdev_release(bbdev);
	if (ret)
		rte_bbdev_log(ERR, "Device %i failed to uninit: %i", dev_id, ret);

	rte_bbdev_log_debug("Destroyed bbdev = %u", dev_id);

	return 0;
}

static inline void
fpga_5gnr_set_default_conf(struct rte_fpga_5gnr_fec_conf *def_conf)
{
	/* clear default configuration before initialization */
	memset(def_conf, 0, sizeof(struct rte_fpga_5gnr_fec_conf));
	/* Set pf mode to true */
	def_conf->pf_mode_en = true;

	/* Set ratio between UL and DL to 1:1 (unit of weight is 3 CBs) */
	def_conf->ul_bandwidth = 3;
	def_conf->dl_bandwidth = 3;

	/* Set Load Balance Factor to 64 */
	def_conf->dl_load_balance = 64;
	def_conf->ul_load_balance = 64;
}

/* Initial configuration of Vista Creek device. */
static int vc_5gnr_configure(const char *dev_name, const struct rte_fpga_5gnr_fec_conf *conf)
{
	uint32_t payload_32, address;
	uint16_t payload_16;
	uint8_t payload_8;
	uint16_t q_id, vf_id, total_q_id, total_ul_q_id, total_dl_q_id;
	struct rte_bbdev *bbdev = rte_bbdev_get_named_dev(dev_name);
	struct rte_fpga_5gnr_fec_conf def_conf;

	if (bbdev == NULL) {
		rte_bbdev_log(ERR,
				"Invalid dev_name (%s), or device is not yet initialised",
				dev_name);
		return -ENODEV;
	}

	struct fpga_5gnr_fec_device *d = bbdev->data->dev_private;

	if (conf == NULL) {
		rte_bbdev_log(ERR, "VC FPGA Configuration was not provided.");
		rte_bbdev_log(ERR, "Default configuration will be loaded.");
		fpga_5gnr_set_default_conf(&def_conf);
		conf = &def_conf;
	}

	/*
	 * Configure UL:DL ratio.
	 * [7:0]: UL weight
	 * [15:8]: DL weight
	 */
	payload_16 = (conf->dl_bandwidth << 8) | conf->ul_bandwidth;
	address = VC_5GNR_CONFIGURATION;
	fpga_5gnr_reg_write_16(d->mmio_base, address, payload_16);

	/* Clear all queues registers */
	payload_32 = FPGA_5GNR_INVALID_HW_QUEUE_ID;
	for (q_id = 0; q_id < d->total_num_queues; ++q_id) {
		address = (q_id << 2) + VC_5GNR_QUEUE_MAP;
		fpga_5gnr_reg_write_32(d->mmio_base, address, payload_32);
	}

	/*
	 * If PF mode is enabled allocate all queues for PF only.
	 *
	 * For VF mode each VF can have different number of UL and DL queues.
	 * Total number of queues to configure cannot exceed VC FPGA
	 * capabilities - 64 queues - 32 queues for UL and 32 queues for DL.
	 * Queues mapping is done according to configuration:
	 *
	 * UL queues:
	 * |                Q_ID              | VF_ID |
	 * |                 0                |   0   |
	 * |                ...               |   0   |
	 * | conf->vf_dl_queues_number[0] - 1 |   0   |
	 * | conf->vf_dl_queues_number[0]     |   1   |
	 * |                ...               |   1   |
	 * | conf->vf_dl_queues_number[1] - 1 |   1   |
	 * |                ...               |  ...  |
	 * | conf->vf_dl_queues_number[7] - 1 |   7   |
	 *
	 * DL queues:
	 * |                Q_ID              | VF_ID |
	 * |                 32               |   0   |
	 * |                ...               |   0   |
	 * | conf->vf_ul_queues_number[0] - 1 |   0   |
	 * | conf->vf_ul_queues_number[0]     |   1   |
	 * |                ...               |   1   |
	 * | conf->vf_ul_queues_number[1] - 1 |   1   |
	 * |                ...               |  ...  |
	 * | conf->vf_ul_queues_number[7] - 1 |   7   |
	 *
	 * Example of configuration:
	 * conf->vf_ul_queues_number[0] = 4;  -> 4 UL queues for VF0
	 * conf->vf_dl_queues_number[0] = 4;  -> 4 DL queues for VF0
	 * conf->vf_ul_queues_number[1] = 2;  -> 2 UL queues for VF1
	 * conf->vf_dl_queues_number[1] = 2;  -> 2 DL queues for VF1
	 *
	 * UL:
	 * | Q_ID | VF_ID |
	 * |   0  |   0   |
	 * |   1  |   0   |
	 * |   2  |   0   |
	 * |   3  |   0   |
	 * |   4  |   1   |
	 * |   5  |   1   |
	 *
	 * DL:
	 * | Q_ID | VF_ID |
	 * |  32  |   0   |
	 * |  33  |   0   |
	 * |  34  |   0   |
	 * |  35  |   0   |
	 * |  36  |   1   |
	 * |  37  |   1   |
	 */
	if (conf->pf_mode_en) {
		payload_32 = 0x1;
		for (q_id = 0; q_id < d->total_num_queues; ++q_id) {
			address = (q_id << 2) + VC_5GNR_QUEUE_MAP;
			fpga_5gnr_reg_write_32(d->mmio_base, address, payload_32);
		}
	} else {
		/* Calculate total number of UL and DL queues to configure */
		total_ul_q_id = total_dl_q_id = 0;
		for (vf_id = 0; vf_id < FPGA_5GNR_FEC_NUM_VFS; ++vf_id) {
			total_ul_q_id += conf->vf_ul_queues_number[vf_id];
			total_dl_q_id += conf->vf_dl_queues_number[vf_id];
		}
		total_q_id = total_dl_q_id + total_ul_q_id;
		/*
		 * Check if total number of queues to configure does not exceed
		 * FPGA capabilities (64 queues - 32 UL and 32 DL queues)
		 */
		if ((total_ul_q_id > VC_5GNR_NUM_UL_QUEUES) ||
			(total_dl_q_id > VC_5GNR_NUM_DL_QUEUES) ||
			(total_q_id > d->total_num_queues)) {
			rte_bbdev_log(ERR,
					"VC 5GNR FPGA Configuration failed. Too many queues to configure: UL_Q %u, DL_Q %u, FPGA_Q %u",
					total_ul_q_id, total_dl_q_id,
					d->total_num_queues);
			return -EINVAL;
		}
		total_ul_q_id = 0;
		for (vf_id = 0; vf_id < FPGA_5GNR_FEC_NUM_VFS; ++vf_id) {
			for (q_id = 0; q_id < conf->vf_ul_queues_number[vf_id];
					++q_id, ++total_ul_q_id) {
				address = (total_ul_q_id << 2) + VC_5GNR_QUEUE_MAP;
				payload_32 = ((0x80 + vf_id) << 16) | 0x1;
				fpga_5gnr_reg_write_32(d->mmio_base, address,
						payload_32);
			}
		}
		total_dl_q_id = 0;
		for (vf_id = 0; vf_id < FPGA_5GNR_FEC_NUM_VFS; ++vf_id) {
			for (q_id = 0; q_id < conf->vf_dl_queues_number[vf_id];
					++q_id, ++total_dl_q_id) {
				address = ((total_dl_q_id + VC_5GNR_NUM_UL_QUEUES)
						<< 2) + VC_5GNR_QUEUE_MAP;
				payload_32 = ((0x80 + vf_id) << 16) | 0x1;
				fpga_5gnr_reg_write_32(d->mmio_base, address,
						payload_32);
			}
		}
	}

	/* Setting Load Balance Factor */
	payload_16 = (conf->dl_load_balance << 8) | (conf->ul_load_balance);
	address = FPGA_5GNR_FEC_LOAD_BALANCE_FACTOR;
	fpga_5gnr_reg_write_16(d->mmio_base, address, payload_16);

	/* Setting length of ring descriptor entry */
	payload_16 = FPGA_5GNR_RING_DESC_ENTRY_LENGTH;
	address = FPGA_5GNR_FEC_RING_DESC_LEN;
	fpga_5gnr_reg_write_16(d->mmio_base, address, payload_16);

	/* Queue PF/VF mapping table is ready */
	payload_8 = 0x1;
	address = FPGA_5GNR_FEC_QUEUE_PF_VF_MAP_DONE;
	fpga_5gnr_reg_write_8(d->mmio_base, address, payload_8);

	rte_bbdev_log_debug("PF Vista Creek 5GNR FPGA configuration complete for %s", dev_name);

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	print_static_reg_debug_info(d->mmio_base, d->fpga_variant);
#endif
	return 0;
}

/* Initial configuration of AGX100 device. */
static int agx100_configure(const char *dev_name, const struct rte_fpga_5gnr_fec_conf *conf)
{
	uint32_t payload_32, address;
	uint16_t payload_16;
	uint8_t payload_8;
	uint16_t q_id, vf_id, total_q_id, total_ul_q_id, total_dl_q_id;
	struct rte_bbdev *bbdev = rte_bbdev_get_named_dev(dev_name);
	struct rte_fpga_5gnr_fec_conf def_conf;

	if (bbdev == NULL) {
		rte_bbdev_log(ERR,
				"Invalid dev_name (%s), or device is not yet initialised",
				dev_name);
		return -ENODEV;
	}

	struct fpga_5gnr_fec_device *d = bbdev->data->dev_private;

	if (conf == NULL) {
		rte_bbdev_log(ERR, "AGX100 Configuration was not provided.");
		rte_bbdev_log(ERR, "Default configuration will be loaded.");
		fpga_5gnr_set_default_conf(&def_conf);
		conf = &def_conf;
	}

	uint8_t total_num_queues = d->total_num_queues;
	uint8_t num_ul_queues = total_num_queues >> 1;
	uint8_t num_dl_queues = total_num_queues >> 1;

	/* Clear all queues registers */
	payload_32 = FPGA_5GNR_INVALID_HW_QUEUE_ID;
	for (q_id = 0; q_id < total_num_queues; ++q_id) {
		address = (q_id << 2) + AGX100_QUEUE_MAP;
		fpga_5gnr_reg_write_32(d->mmio_base, address, payload_32);
	}

	/*
	 * If PF mode is enabled allocate all queues for PF only.
	 *
	 * For VF mode each VF can have different number of UL and DL queues.
	 * Total number of queues to configure cannot exceed AGX100
	 * capabilities - 64 queues - 32 queues for UL and 32 queues for DL.
	 * Queues mapping is done according to configuration:
	 *
	 * UL queues:
	 * |                Q_ID              | VF_ID |
	 * |                 0                |   0   |
	 * |                ...               |   0   |
	 * | conf->vf_dl_queues_number[0] - 1 |   0   |
	 * | conf->vf_dl_queues_number[0]     |   1   |
	 * |                ...               |   1   |
	 * | conf->vf_dl_queues_number[1] - 1 |   1   |
	 * |                ...               |  ...  |
	 * | conf->vf_dl_queues_number[7] - 1 |   7   |
	 *
	 * DL queues:
	 * |                Q_ID              | VF_ID |
	 * |                 32               |   0   |
	 * |                ...               |   0   |
	 * | conf->vf_ul_queues_number[0] - 1 |   0   |
	 * | conf->vf_ul_queues_number[0]     |   1   |
	 * |                ...               |   1   |
	 * | conf->vf_ul_queues_number[1] - 1 |   1   |
	 * |                ...               |  ...  |
	 * | conf->vf_ul_queues_number[7] - 1 |   7   |
	 *
	 * Example of configuration:
	 * conf->vf_ul_queues_number[0] = 4;  -> 4 UL queues for VF0
	 * conf->vf_dl_queues_number[0] = 4;  -> 4 DL queues for VF0
	 * conf->vf_ul_queues_number[1] = 2;  -> 2 UL queues for VF1
	 * conf->vf_dl_queues_number[1] = 2;  -> 2 DL queues for VF1
	 *
	 * UL:
	 * | Q_ID | VF_ID |
	 * |   0  |   0   |
	 * |   1  |   0   |
	 * |   2  |   0   |
	 * |   3  |   0   |
	 * |   4  |   1   |
	 * |   5  |   1   |
	 *
	 * DL:
	 * | Q_ID | VF_ID |
	 * |  32  |   0   |
	 * |  33  |   0   |
	 * |  34  |   0   |
	 * |  35  |   0   |
	 * |  36  |   1   |
	 * |  37  |   1   |
	 */
	if (conf->pf_mode_en) {
		payload_32 = 0x1;
		for (q_id = 0; q_id < total_num_queues; ++q_id) {
			address = (q_id << 2) + AGX100_QUEUE_MAP;
			fpga_5gnr_reg_write_32(d->mmio_base, address, payload_32);
		}
	} else {
		/* Calculate total number of UL and DL queues to configure. */
		total_ul_q_id = total_dl_q_id = 0;
		for (vf_id = 0; vf_id < FPGA_5GNR_FEC_NUM_VFS; ++vf_id) {
			total_ul_q_id += conf->vf_ul_queues_number[vf_id];
			total_dl_q_id += conf->vf_dl_queues_number[vf_id];
		}
		total_q_id = total_dl_q_id + total_ul_q_id;
		/*
		 * Check if total number of queues to configure does not exceed
		 * AGX100 capabilities (64 queues - 32 UL and 32 DL queues)
		 */
		if ((total_ul_q_id > num_ul_queues) ||
				(total_dl_q_id > num_dl_queues) ||
				(total_q_id > total_num_queues)) {
			rte_bbdev_log(ERR,
					"AGX100 Configuration failed. Too many queues to configure: UL_Q %u, DL_Q %u, AGX100_Q %u",
					total_ul_q_id, total_dl_q_id,
					total_num_queues);
			return -EINVAL;
		}
		total_ul_q_id = 0;
		for (vf_id = 0; vf_id < FPGA_5GNR_FEC_NUM_VFS; ++vf_id) {
			for (q_id = 0; q_id < conf->vf_ul_queues_number[vf_id];
					++q_id, ++total_ul_q_id) {
				address = (total_ul_q_id << 2) + AGX100_QUEUE_MAP;
				payload_32 = ((0x80 + vf_id) << 16) | 0x1;
				fpga_5gnr_reg_write_32(d->mmio_base, address, payload_32);
			}
		}
		total_dl_q_id = 0;
		for (vf_id = 0; vf_id < FPGA_5GNR_FEC_NUM_VFS; ++vf_id) {
			for (q_id = 0; q_id < conf->vf_dl_queues_number[vf_id];
					++q_id, ++total_dl_q_id) {
				address = ((total_dl_q_id + num_ul_queues)
						<< 2) + AGX100_QUEUE_MAP;
				payload_32 = ((0x80 + vf_id) << 16) | 0x1;
				fpga_5gnr_reg_write_32(d->mmio_base, address, payload_32);
			}
		}
	}

	/* Setting Load Balance Factor. */
	payload_16 = (conf->dl_load_balance << 8) | (conf->ul_load_balance);
	address = FPGA_5GNR_FEC_LOAD_BALANCE_FACTOR;
	fpga_5gnr_reg_write_16(d->mmio_base, address, payload_16);

	/* Setting length of ring descriptor entry. */
	payload_16 = FPGA_5GNR_RING_DESC_ENTRY_LENGTH;
	address = FPGA_5GNR_FEC_RING_DESC_LEN;
	fpga_5gnr_reg_write_16(d->mmio_base, address, payload_16);

	/* Queue PF/VF mapping table is ready. */
	payload_8 = 0x1;
	address = FPGA_5GNR_FEC_QUEUE_PF_VF_MAP_DONE;
	fpga_5gnr_reg_write_8(d->mmio_base, address, payload_8);

	rte_bbdev_log_debug("PF AGX100 configuration complete for %s", dev_name);

#ifdef RTE_LIBRTE_BBDEV_DEBUG
	print_static_reg_debug_info(d->mmio_base, d->fpga_variant);
#endif
	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_fpga_5gnr_fec_configure, 20.11)
int rte_fpga_5gnr_fec_configure(const char *dev_name, const struct rte_fpga_5gnr_fec_conf *conf)
{
	struct rte_bbdev *bbdev = rte_bbdev_get_named_dev(dev_name);
	if (bbdev == NULL) {
		rte_bbdev_log(ERR, "Invalid dev_name (%s), or device is not yet initialised",
				dev_name);
		return -ENODEV;
	}
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(bbdev->device);
	rte_bbdev_log(INFO, "Configure dev id %x", pci_dev->id.device_id);
	if (pci_dev->id.device_id == VC_5GNR_PF_DEVICE_ID)
		return vc_5gnr_configure(dev_name, conf);
	else if (pci_dev->id.device_id == AGX100_PF_DEVICE_ID)
		return agx100_configure(dev_name, conf);

	rte_bbdev_log(ERR, "Invalid device_id (%d)", pci_dev->id.device_id);
	return -ENODEV;
}

/* FPGA 5GNR FEC PCI PF address map */
static struct rte_pci_id pci_id_fpga_5gnr_fec_pf_map[] = {
	{
		RTE_PCI_DEVICE(AGX100_VENDOR_ID, AGX100_PF_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(VC_5GNR_VENDOR_ID, VC_5GNR_PF_DEVICE_ID)
	},
	{.device_id = 0},
};

static struct rte_pci_driver fpga_5gnr_fec_pci_pf_driver = {
	.probe = fpga_5gnr_fec_probe,
	.remove = fpga_5gnr_fec_remove,
	.id_table = pci_id_fpga_5gnr_fec_pf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};

/* FPGA 5GNR FEC PCI VF address map */
static struct rte_pci_id pci_id_fpga_5gnr_fec_vf_map[] = {
	{
		RTE_PCI_DEVICE(AGX100_VENDOR_ID, AGX100_VF_DEVICE_ID)
	},
	{
		RTE_PCI_DEVICE(VC_5GNR_VENDOR_ID, VC_5GNR_VF_DEVICE_ID)
	},
	{.device_id = 0},
};

static struct rte_pci_driver fpga_5gnr_fec_pci_vf_driver = {
	.probe = fpga_5gnr_fec_probe,
	.remove = fpga_5gnr_fec_remove,
	.id_table = pci_id_fpga_5gnr_fec_vf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};


RTE_PMD_REGISTER_PCI(FPGA_5GNR_FEC_PF_DRIVER_NAME, fpga_5gnr_fec_pci_pf_driver);
RTE_PMD_REGISTER_PCI_TABLE(FPGA_5GNR_FEC_PF_DRIVER_NAME, pci_id_fpga_5gnr_fec_pf_map);
RTE_PMD_REGISTER_PCI(FPGA_5GNR_FEC_VF_DRIVER_NAME, fpga_5gnr_fec_pci_vf_driver);
RTE_PMD_REGISTER_PCI_TABLE(FPGA_5GNR_FEC_VF_DRIVER_NAME, pci_id_fpga_5gnr_fec_vf_map);
