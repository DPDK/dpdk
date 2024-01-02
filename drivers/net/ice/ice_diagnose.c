/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include <rte_string_fns.h>
#include <rte_malloc.h>
#include <rte_tailq.h>

#include "ice_ethdev.h"
#include "ice_rxtx.h"

#define ICE_BLK_MAX_COUNT          512
#define ICE_BUFF_SEG_HEADER_FLAG   0x1
#define ICE_PKG_HDR_HEADR_PART1    1
#define ICE_PKG_HDR_HEADR_PART2    2
#define ICE_PKG_HDR_GM_SEG_OFFSET  16
#define ICE_PKG_HDR_ICE_SEG_OFFSET 100
#define ICE_PKG_GM_SEG_TYPE        1
#define ICE_PKG_MAJOR_VERSION      1
#define ICE_PKG_GM_SEG_SIZE        84
#define ICE_PKG_ICE_SEG_TYPE       0x10
#define ICE_PKG_ICE_SEG_SIZE_BASE  56
#define SPACE_CHAR                 0x20

#define ICE_PKG_COPY_STRING(dst, src)	\
	do {\
		char *_dst = (dst); \
		const char *_src = (src); \
		memset(_dst, SPACE_CHAR, ICE_PKG_NAME_SIZE); \
		strlcpy(_dst, _src, strlen(_dst)); \
	} while (0)

/* Package header */
struct ice_package_header {
	struct __hdr {
		uint32_t h1; /* header part 1 */
		uint32_t h2; /* header part 2 */
	} header;
	uint32_t gm_seg_offset;	 /* Global Metadata segment: 16 */
	uint32_t ice_seg_offset; /* ICE segment: 100 */
	struct ice_global_metadata_seg gm_seg;
	struct __ice_seg {
		struct ice_generic_seg_hdr hdr;
		uint32_t devid_count;
		struct ice_pkg_ver nvm_ver;
	} ice_seg;

	uint32_t buff_count;
};

struct ice_buff_seg_header {
	__le16 flag;
	__le16 length;
	__le16 type;
	__le16 reserve;		/* 0 */
	__le16 header_len;	/* 0x0C */
	__le16 data_size;	/* length - header_len */
};

struct ice_buff_seg_simple {
	struct ice_buff_seg_header header;
	__le16 seg_end;
};

struct ice_buff_seg_simple_data {
	__le16 type;
	__le32 addr;
	__le16 len;
	__le16 seg_end;
};

struct ice_buff_seg_series {
	struct ice_buff_seg_header header;
	uint16_t offset_delta;
	uint16_t offset[2];
};

struct ice_buff_seg_series_data {
	__le16 type;
	__le32 begin_addr;
	__le16 len;
	__le32 end_addr;
	__le16 last_len;
	__le16 offset_delta;
	__le16 seg_end;
	uint8_t padding;
};

struct ice_buff_seg_series_with_sub {
	struct ice_buff_seg_header header;
	uint16_t sub_block_num;
};

struct ice_buff_seg_series_with_sub_data {
	__le16 type;
	__le32 begin_addr;
	__le16 len;
	__le32 end_addr;
	__le16 last_len;
	__le16 sblk_size;
};


static const
uint16_t ice_buff_seg_header_size = sizeof(struct ice_buff_seg_header);

static void
write_buffer_simple(uint8_t **buff)
{
	uint16_t i;
	/* ICE ddp package simple segment template */
	const struct ice_buff_seg_simple_data buff_data[] = {
	    {0x0001, 0x00000, 0x0030, 0x0000},
	    {0x000a, 0x01000, 0x0810, 0x0800},
	    {0x000b, 0x02000, 0x00d8, 0x0000},
	    {0x000d, 0x06000, 0x0810, 0x0400},
	    {0x000f, 0x09000, 0x0110, 0x0100},
	    {0x0011, 0x17000, 0x001d, 0x0000},
	    {0x0012, 0x18000, 0x0014, 0x0000},
	    {0x0014, 0x19000, 0x0810, 0x0800},
	    {0x0015, 0x1a000, 0x00d8, 0x0000},
	    {0x0017, 0x1e000, 0x0810, 0x0400},
	    {0x0019, 0x21000, 0x0090, 0x0080},
	    {0x001b, 0x27000, 0x001d, 0x0000},
	    {0x001c, 0x28000, 0x0014, 0x0000},
	    {0x001e, 0x29000, 0x0810, 0x0800},
	    {0x001f, 0x2a000, 0x00d8, 0x0000},
	    {0x0021, 0x2e000, 0x0810, 0x0400},
	    {0x0023, 0x31000, 0x0090, 0x0080},
	    {0x0025, 0x36000, 0x001d, 0x0000},
	    {0x0026, 0x37000, 0x0014, 0x0000},
	    {0x0028, 0x38000, 0x0810, 0x0800},
	    {0x0029, 0x39000, 0x00d8, 0x0000},
	    {0x002b, 0x3d000, 0x0810, 0x0400},
	    {0x002d, 0x40000, 0x0090, 0x0080},
	    {0x002f, 0x45000, 0x001d, 0x0000},
	    {0x0030, 0x46000, 0x0014, 0x0000},
	    {0x0035, 0x57000, 0x0010, 0x0000},
	    {0x003a, 0x67000, 0x0190, 0x0010},
	    {0x003b, 0x68000, 0x0810, 0x0800},
	    {0x003f, 0x79000, 0x0010, 0x0000},
	    {0x0044, 0x89000, 0x0190, 0x0010},
	    {0x0045, 0x8a000, 0x0810, 0x0800},
	    {0x0046, 0x8b000, 0x001c, 0x0000},
	    {0x0047, 0x8c000, 0x001c, 0x0000},
	    {0x0048, 0x8d000, 0x0410, 0x0080},
	    {0x0049, 0x8e000, 0x0410, 0x0080},
	    {0x004a, 0x8f000, 0x0028, 0x0006},
	    {0x004b, 0x90000, 0x0028, 0x0006},
	    {0x004c, 0x91000, 0x0890, 0x0080},
	    {0x004d, 0x92000, 0x0890, 0x0080},
	    {0x004e, 0x93000, 0x0350, 0x0040},
	    {0x004f, 0x94000, 0x0350, 0x0040},
	    {0x0050, 0x95000, 0x0810, 0x0800},
	    {0x0051, 0x96000, 0x00d8, 0x0000},
	    {0x0053, 0x9a000, 0x0810, 0x0400},
	    {0x0055, 0x9c000, 0x0030, 0x0020},
	    {0x0057, 0x9f000, 0x001d, 0x0000},
	    {0x0058, 0xa0000, 0x0014, 0x0000},
	    {0x005a, 0xa1000, 0x0024, 0x0000},
	    {0x005b, 0xa2000, 0x0024, 0x0000},
	    {0x005d, 0xa4000, 0x0810, 0x0100},
	    {0x020d, 0xa8000, 0x0414, 0x0400},
	    {0x020e, 0xa9000, 0x0214, 0x0200},
	    {0x020f, 0xaa000, 0x0114, 0x0100},
	    {0x0210, 0xab000, 0x0114, 0x0100},
	    {0x0217, 0xaf000, 0x0414, 0x0400},
	    {0x0218, 0xb0000, 0x0214, 0x0200},
	    {0x0219, 0xb1000, 0x0094, 0x0080},
	    {0x021a, 0xb2000, 0x0094, 0x0080},
	    {0x0221, 0xb6000, 0x0414, 0x0400},
	    {0x0222, 0xb7000, 0x0214, 0x0200},
	    {0x0223, 0xb8000, 0x0094, 0x0080},
	    {0x0224, 0xb9000, 0x0094, 0x0080},
	    {0x022b, 0xbd000, 0x0414, 0x0400},
	    {0x022c, 0xbe000, 0x0214, 0x0200},
	    {0x022d, 0xbf000, 0x0094, 0x0080},
	    {0x022e, 0xc0000, 0x0094, 0x0080},
	    {0x0238, 0xc1000, 0x0114, 0x0100},
	    {0x0253, 0xc5000, 0x0414, 0x0400},
	    {0x0254, 0xc6000, 0x0054, 0x0040},
	    {0x0255, 0xc7000, 0x0034, 0x0020},
	    {0x0256, 0xc8000, 0x0034, 0x0020},
	};

	for (i = 0; i < ARRAY_SIZE(buff_data); i++) {
		const struct ice_buff_seg_simple_data *seg = &buff_data[i];
		struct ice_buff_seg_simple buff_seg;
		uint8_t *buffer = &(*buff)[seg->addr];

		memset(buffer, 0xFF, ICE_PKG_BUF_SIZE);
		buff_seg.header.flag = ICE_BUFF_SEG_HEADER_FLAG;
		buff_seg.header.length = seg->len;
		buff_seg.header.type = seg->type;
		buff_seg.header.reserve = 0x0;
		buff_seg.header.header_len =
			sizeof(struct ice_buff_seg_header);
		buff_seg.header.data_size =
			buff_seg.header.length - buff_seg.header.header_len;
		buff_seg.seg_end = seg->seg_end;

		memset(buffer, 0x00, buff_seg.header.length);
		memcpy(buffer, &buff_seg, sizeof(struct ice_buff_seg_simple));
	}
}

static void
write_buffer_block(uint8_t **buff)
{
	uint16_t i;
	/* ICE ddp package multiple segments template 1 */
	const struct ice_buff_seg_series_data buff_data[] = {
		{0x000c, 0x03000, 0x1000, 0x05000, 0x0030, 0x0ff0, 0x0020, 0},
		{0x0010, 0x0a000, 0x0fd0, 0x16000, 0x0310, 0x0015, 0x0004, 0},
		{0x0016, 0x1b000, 0x1000, 0x1d000, 0x0030, 0x0ff0, 0x0020, 0},
		{0x001a, 0x22000, 0x0f90, 0x26000, 0x0210, 0x001f, 0x0004, 0},
		{0x0020, 0x2b000, 0x1000, 0x2d000, 0x0030, 0x0ff0, 0x0020, 0},
		{0x0024, 0x32000, 0x0fd0, 0x35000, 0x00d0, 0x002a, 0x0002, 0},
		{0x002a, 0x3a000, 0x1000, 0x3c000, 0x0030, 0x0ff0, 0x0020, 0},
		{0x002e, 0x41000, 0x0fd0, 0x44000, 0x00d0, 0x002a, 0x0002, 0},
		{0x0032, 0x47000, 0x1000, 0x4f000, 0x0090, 0x00ff, 0x0008, 0},
		{0x0033, 0x50000, 0x1000, 0x53000, 0x0040, 0x0154, 0x0004, 0},
		{0x0034, 0x54000, 0x1000, 0x56000, 0x0430, 0x0055, 0x0016, 0},
		{0x0039, 0x65000, 0x1000, 0x66000, 0x0220, 0x00aa, 0x0016, 0},
		{0x003c, 0x69000, 0x1000, 0x71000, 0x0090, 0x00ff, 0x0008, 0},
		{0x003d, 0x72000, 0x1000, 0x75000, 0x0040, 0x0154, 0x0004, 0},
		{0x003e, 0x76000, 0x1000, 0x78000, 0x0430, 0x0055, 0x0016, 0},
		{0x0043, 0x87000, 0x1000, 0x88000, 0x0220, 0x00aa, 0x0016, 0},
		{0x0052, 0x97000, 0x1000, 0x99000, 0x0030, 0x0ff0, 0x0020, 0},
		{0x0056, 0x9d000, 0x0f90, 0x9e000, 0x0090, 0x001f, 0x0001, 0},
		{0x020c, 0xa5000, 0x1000, 0xa7000, 0x003c, 0x0fec, 0x0028, 1},
		{0x0216, 0xac000, 0x1000, 0xae000, 0x003c, 0x0fec, 0x0028, 1},
		{0x0220, 0xb3000, 0x1000, 0xb5000, 0x003c, 0x0fec, 0x0028, 1},
		{0x022a, 0xba000, 0x1000, 0xbc000, 0x003c, 0x0fec, 0x0028, 1},
		{0x0252, 0xc2000, 0x1000, 0xc4000, 0x003c, 0x0fec, 0x0028, 1},
	};

	for (i = 0; i < ARRAY_SIZE(buff_data); i++) {
		const struct ice_buff_seg_series_data *seg = &buff_data[i];
		struct ice_buff_seg_series buff_seg;
		const uint16_t buff_seg_size =
			sizeof(struct ice_buff_seg_series);
		uint32_t addr = seg->begin_addr;
		__le16 last_offset = 0;

		for (; addr <= seg->end_addr; addr += ICE_PKG_BUF_SIZE) {
			uint8_t *buffer = &(*buff)[addr];

			memset(buffer, 0xFF, ICE_PKG_BUF_SIZE);
			buff_seg.header.flag = ICE_BUFF_SEG_HEADER_FLAG;
			buff_seg.header.length = addr == seg->end_addr ?
						seg->last_len : seg->len;
			buff_seg.header.type = seg->type;
			buff_seg.header.reserve = 0x0;
			buff_seg.header.header_len = ice_buff_seg_header_size;
			buff_seg.header.data_size = buff_seg.header.length -
						buff_seg.header.header_len;
			buff_seg.offset_delta =  addr < seg->end_addr ?
				seg->offset_delta : seg->seg_end;
			buff_seg.offset[!seg->padding] = 0x0;
			buff_seg.offset[seg->padding] = last_offset;

			memset(buffer, 0x00, buff_seg.header.length);
			memcpy(buffer, &buff_seg, buff_seg_size);

			last_offset += seg->offset_delta;
		}
	}
}

static void
write_buffer_block2(uint8_t **buff)
{
	uint16_t i;
	/* ICE ddp package multiple segments template 2 */
	struct ice_buff_seg_series_with_sub_data buff_data[] = {
		{0x000e, 0x07000, 0x1000, 0x08000, 0x0a1c, 13},
		{0x0018, 0x1f000, 0x1000, 0x20000, 0x0a1c, 13},
		{0x0022, 0x2f000, 0x1000, 0x30000, 0x0a1c, 13},
		{0x002c, 0x3e000, 0x1000, 0x3f000, 0x0a1c, 13},
		{0x0037, 0x58000, 0x1000, 0x5e000, 0x0070, 24},
		{0x0038, 0x5f000, 0x0fe0, 0x64000, 0x0900, 88},
		{0x0041, 0x7a000, 0x1000, 0x80000, 0x0070, 24},
		{0x0042, 0x81000, 0x0fe0, 0x86000, 0x0900, 88},
		{0x0054, 0x9b000, 0x034e, 0x9b000, 0x034e, 13},
		{0x005c, 0xa3000, 0x0a10, 0xa3000, 0x0a10, 40},
	};

	for (i = 0; i < ARRAY_SIZE(buff_data); i++) {
		struct ice_buff_seg_series_with_sub_data *seg = &buff_data[i];
		struct ice_buff_seg_series_with_sub buff_seg;
		const uint16_t buff_seg_size =
			sizeof(struct ice_buff_seg_series_with_sub);
		uint32_t addr;
		uint16_t last_idx = 0;

		for (addr = seg->begin_addr;
		     addr <= seg->end_addr; addr += ICE_PKG_BUF_SIZE) {
			uint8_t *buffer = &(*buff)[addr];
			uint16_t total_sblk_size;
			uint16_t idx = 0;
			uint32_t pos = buff_seg_size;

			memset(buffer, 0xFF, ICE_PKG_BUF_SIZE);
			buff_seg.header.flag = ICE_BUFF_SEG_HEADER_FLAG;
			buff_seg.header.length =
				addr == seg->end_addr ?
					seg->last_len : seg->len;
			buff_seg.header.type = seg->type;
			buff_seg.header.reserve = 0x0;
			buff_seg.header.header_len = ice_buff_seg_header_size;
			buff_seg.header.data_size = buff_seg.header.length -
					buff_seg.header.header_len;

			total_sblk_size = buff_seg.header.data_size
					  - sizeof(buff_seg.sub_block_num);
			buff_seg.sub_block_num =
					total_sblk_size / seg->sblk_size;

			memset(buffer, 0x00, buff_seg.header.length);
			memcpy(buffer, &buff_seg, buff_seg_size);

			/* padding if needed */
			if (total_sblk_size % seg->sblk_size)
				pos += sizeof(uint16_t);

			for (idx = last_idx;
			     idx < last_idx + buff_seg.sub_block_num; idx++) {
				memcpy(buffer + pos, &idx, sizeof(uint16_t));
				pos += seg->sblk_size;
			}

			last_idx = idx;
		}
	}
}

static int
ice_dump_pkg(struct rte_eth_dev *dev, uint8_t **buff, uint32_t *size)
{
	struct ice_hw *hw;
	struct ice_buf pkg_buff;
	uint8_t *next_buff;
	uint16_t i = 0;
	uint16_t count;
	struct ice_package_header *cache;
	uint32_t cache_size;

	write_buffer_simple(buff);
	write_buffer_block(buff);
	write_buffer_block2(buff);

	hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (*size % ICE_PKG_BUF_SIZE)
		return -EINVAL;

	count = *size / ICE_PKG_BUF_SIZE;
	for (i = 0; i < count; i++) {
		next_buff = (uint8_t *)(*buff) + i * ICE_PKG_BUF_SIZE;
		rte_memcpy(pkg_buff.buf, next_buff, ICE_PKG_BUF_SIZE);
		if (ice_aq_upload_section(hw,
					  (struct ice_buf_hdr *)&pkg_buff.buf[0],
					  ICE_PKG_BUF_SIZE,
					  NULL))
			return -EINVAL;
		rte_memcpy(next_buff, pkg_buff.buf, ICE_PKG_BUF_SIZE);
	}

	cache_size = sizeof(struct ice_package_header) + *size;
	cache = (struct ice_package_header *)malloc(cache_size);
	if (!cache)
		return -ENOSPC;

	cache->header.h1 = ICE_PKG_HDR_HEADR_PART1;
	cache->header.h2 = ICE_PKG_HDR_HEADR_PART2;
	cache->gm_seg_offset = ICE_PKG_HDR_GM_SEG_OFFSET;
	cache->ice_seg_offset = ICE_PKG_HDR_ICE_SEG_OFFSET;
	cache->gm_seg.hdr.seg_type = ICE_PKG_GM_SEG_TYPE;
	cache->gm_seg.hdr.seg_format_ver.major = ICE_PKG_MAJOR_VERSION;
	cache->gm_seg.hdr.seg_size = ICE_PKG_GM_SEG_SIZE;
	ICE_PKG_COPY_STRING(cache->gm_seg.hdr.seg_id, "Global Metadata");

	cache->gm_seg.pkg_ver.major = ICE_PKG_MAJOR_VERSION;
	cache->gm_seg.rsvd = 1;
	ICE_PKG_COPY_STRING(cache->gm_seg.pkg_name, "DEFAULT");

	cache->ice_seg.hdr.seg_type = ICE_PKG_ICE_SEG_TYPE;
	cache->ice_seg.hdr.seg_format_ver.major = ICE_PKG_MAJOR_VERSION;
	cache->ice_seg.hdr.seg_size = ICE_PKG_ICE_SEG_SIZE_BASE + *size;
	cache->ice_seg.devid_count = 0;
	cache->ice_seg.nvm_ver.major = 0;
	ICE_PKG_COPY_STRING(cache->ice_seg.hdr.seg_id, "CPK Configuration Data");

	cache->buff_count = count;

	next_buff = (uint8_t *)cache;
	next_buff += sizeof(struct ice_package_header);
	memcpy(next_buff, *buff, *size);

	free(*buff);
	*buff = (uint8_t *)cache;
	*size = cache_size;

	return 0;
}

int rte_pmd_ice_dump_package(uint16_t port, uint8_t **buff, uint32_t *size)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	if (!is_ice_supported(dev))
		return -ENOTSUP;

	return ice_dump_pkg(dev, buff, size);
}

static uint16_t
covert_byte_to_hex(uint8_t **outbuf, const uint8_t *inbuf, uint32_t inbuf_size)
{
	uint32_t i;
	uint8_t *buffer = *outbuf;
	for (i = 0; i < inbuf_size; ++i)
		sprintf((char *)(buffer + i * 2), "%02X", inbuf[i]);

	return inbuf_size * 2;
}

static int
ice_dump_switch(struct rte_eth_dev *dev, uint8_t **buff2, uint32_t *size)
{
	struct ice_hw *hw;
	int i = 0;
	uint16_t tbl_id = 0;
	uint32_t tbl_idx = 0;
	uint8_t *buffer = *buff2;

	hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* table index string format: "0000:" */
	#define TBL_IDX_STR_SIZE 7
	for (i = 0; i < ICE_BLK_MAX_COUNT; i++) {
		int res;
		uint16_t buff_size;
		uint8_t *buff;
		uint32_t offset = 0;

		buff = malloc(ICE_PKG_BUF_SIZE);
		if (!buff)
			return ICE_ERR_NO_MEMORY;

		if (tbl_idx == 0) {
			char tbl_idx_str[TBL_IDX_STR_SIZE];
			memset(tbl_idx_str, 0, sizeof(tbl_idx_str));
			sprintf(tbl_idx_str, "%d:", tbl_id);
			memcpy(buffer, tbl_idx_str, strlen(tbl_idx_str));
			offset = strlen(tbl_idx_str);
			buffer += offset;
		}

		res = ice_aq_get_internal_data(hw,
			ICE_AQC_DBG_DUMP_CLUSTER_ID_SW,
			tbl_id, tbl_idx, buff,
			ICE_PKG_BUF_SIZE,
			&buff_size, &tbl_id, &tbl_idx, NULL);

		if (res) {
			free(buff);
			return res;
		}

		offset = covert_byte_to_hex(&buffer, buff, buff_size);
		buffer += offset;

		free(buff);

		if (tbl_idx == 0xffffffff) {
			tbl_idx = 0;
			memset(buffer, '\n', sizeof(char));
			buffer++;
			offset = 0;
		}

		if (tbl_id == 0xff)
			break;
	}

	*size = buffer - *buff2;
	return 0;
}

int rte_pmd_ice_dump_switch(uint16_t port, uint8_t **buff, uint32_t *size)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	if (!is_ice_supported(dev))
		return -ENOTSUP;

	return ice_dump_switch(dev, buff, size);
}

static void print_rl_profile(struct ice_aqc_rl_profile_elem *prof,
			     FILE *stream)
{
	fprintf(stream, "\t\t\t\t\t<td>\n");
	fprintf(stream, "\t\t\t\t\t\t<table>\n");

	fprintf(stream, "\t\t\t\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t\t\t\t<td>id</td>\n");
	fprintf(stream, "\t\t\t\t\t\t\t\t<td>%d</td>\n", prof->profile_id);
	fprintf(stream, "\t\t\t\t\t\t\t</tr>\n");

	fprintf(stream, "\t\t\t\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t\t\t\t<td>max burst size</td>\n");
	fprintf(stream, "\t\t\t\t\t\t\t\t<td>%d</td>\n", prof->max_burst_size);
	fprintf(stream, "\t\t\t\t\t\t\t</tr>\n");

	fprintf(stream, "\t\t\t\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t\t\t\t<td>rate limit multiply</td>\n");
	fprintf(stream, "\t\t\t\t\t\t\t\t<td>%d</td>\n", prof->rl_multiply);
	fprintf(stream, "\t\t\t\t\t\t\t</tr>\n");

	fprintf(stream, "\t\t\t\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t\t\t\t<td>wake up calculation</td>\n");
	fprintf(stream, "\t\t\t\t\t\t\t\t<td>%d</td>\n", prof->wake_up_calc);
	fprintf(stream, "\t\t\t\t\t\t\t</tr>\n");

	fprintf(stream, "\t\t\t\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t\t\t\t<td>rate limit encode</td>\n");
	fprintf(stream, "\t\t\t\t\t\t\t\t<td>%d</td>\n", prof->rl_encode);
	fprintf(stream, "\t\t\t\t\t\t\t</tr>\n");

	fprintf(stream, "\t\t\t\t\t\t</table>\n");
	fprintf(stream, "\t\t\t\t\t</td>\n");
}

static
void print_elem_type(FILE *stream, u8 type)
{
	switch (type) {
	case 1:
		fprintf(stream, "root");
		break;
	case 2:
		fprintf(stream, "tc");
		break;
	case 3:
		fprintf(stream, "se_generic");
		break;
	case 4:
		fprintf(stream, "entry_point");
		break;
	case 5:
		fprintf(stream, "leaf");
		break;
	default:
		fprintf(stream, "%d", type);
		break;
	}
}

static
void print_valid_sections(FILE *stream, u8 vs)
{
	if ((vs & 0x1) != 0)
		fprintf(stream, "generic ");
	if ((vs & 0x2) != 0)
		fprintf(stream, "cir ");
	if ((vs & 0x4) != 0)
		fprintf(stream, "eir ");
	if ((vs & 0x8) != 0)
		fprintf(stream, "shared ");
}

static
void print_scheduling_mode(FILE *stream, bool flag)
{
	if (flag)
		fprintf(stream, "pps");
	else
		fprintf(stream, "bps");
}

static
void print_priority_mode(FILE *stream, bool flag)
{
	if (flag)
		fprintf(stream, "single priority node");
	else
		fprintf(stream, "wfq");
}

static
void print_node(struct ice_aqc_txsched_elem_data *data,
		struct ice_aqc_rl_profile_elem *cir_prof,
		struct ice_aqc_rl_profile_elem *eir_prof,
		struct ice_aqc_rl_profile_elem *shared_prof,
		bool detail, FILE *stream)
{
	fprintf(stream, "\tNODE_%d [\n", data->node_teid);
	fprintf(stream, "\t\tlabel=<\n");

	fprintf(stream, "\t\t\t<table>\n");

	fprintf(stream, "\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t<td> teid </td>\n");
	fprintf(stream, "\t\t\t\t\t<td> %d </td>\n", data->node_teid);
	fprintf(stream, "\t\t\t\t</tr>\n");

	fprintf(stream, "\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t<td> type </td>\n");
	fprintf(stream, "\t\t\t\t\t<td>");
	print_elem_type(stream, data->data.elem_type);
	fprintf(stream, "</td>\n");
	fprintf(stream, "\t\t\t\t</tr>\n");

	if (!detail)
		goto brief;

	fprintf(stream, "\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t<td> valid sections </td>\n");
	fprintf(stream, "\t\t\t\t\t<td>");
	print_valid_sections(stream, data->data.valid_sections);
	fprintf(stream,	"</td>\n");
	fprintf(stream, "\t\t\t\t</tr>\n");

	fprintf(stream, "\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t<td> scheduling mode </td>\n");
	fprintf(stream, "\t\t\t\t\t<td>");
	print_scheduling_mode(stream, (data->data.generic & 0x1) != 0);
	fprintf(stream, "</td>\n");
	fprintf(stream, "\t\t\t\t</tr>\n");

	fprintf(stream, "\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t<td> priority </td>\n");
	fprintf(stream, "\t\t\t\t\t<td> %d </td>\n", (data->data.generic >> 1) & 0x7);
	fprintf(stream, "\t\t\t\t</tr>\n");

	fprintf(stream, "\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t<td> priority mode</td>\n");
	fprintf(stream, "\t\t\t\t\t<td>");
	print_priority_mode(stream, ((data->data.generic >> 4) & 0x1) != 0);
	fprintf(stream,	"</td>\n");
	fprintf(stream, "\t\t\t\t</tr>\n");

	fprintf(stream, "\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t<td> adjustment value </td>\n");
	fprintf(stream, "\t\t\t\t\t<td> %d </td>\n", (data->data.generic >> 5) & 0x3);
	fprintf(stream, "\t\t\t\t</tr>\n");

	fprintf(stream, "\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t<td> suspended </td>\n");
	fprintf(stream, "\t\t\t\t\t<td> %d </td>\n", data->data.flags & 0x1);
	fprintf(stream, "\t\t\t\t</tr>\n");

	fprintf(stream, "\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t<td> cir bw profile </td>\n");
	if (cir_prof == NULL)
		fprintf(stream, "\t\t\t\t\t<td> default </td>\n");
	else
		print_rl_profile(cir_prof, stream);
	fprintf(stream, "\t\t\t\t</tr>\n");

	fprintf(stream, "\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t<td> cir bw weight </td>\n");
	fprintf(stream, "\t\t\t\t\t<td> %d </td>\n", data->data.cir_bw.bw_alloc);
	fprintf(stream, "\t\t\t\t</tr>\n");

	fprintf(stream, "\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t<td> eir bw profile </td>\n");
	if (eir_prof == NULL)
		fprintf(stream, "\t\t\t\t\t<td> default </td>\n");
	else
		print_rl_profile(eir_prof, stream);
	fprintf(stream, "\t\t\t\t</tr>\n");

	fprintf(stream, "\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t<td> eir bw weight </td>\n");
	fprintf(stream, "\t\t\t\t\t<td> %d </td>\n", data->data.eir_bw.bw_alloc);
	fprintf(stream, "\t\t\t\t</tr>\n");

	fprintf(stream, "\t\t\t\t<tr>\n");
	fprintf(stream, "\t\t\t\t\t<td> shared rl profile </td>\n");
	if (shared_prof == NULL)
		fprintf(stream, "\t\t\t\t\t<td> default </td>\n");
	else
		print_rl_profile(shared_prof, stream);
	fprintf(stream, "\t\t\t\t</tr>\n");

brief:
	fprintf(stream, "\t\t\t</table>\n");

	fprintf(stream, "\t\t>\n");
	fprintf(stream, "\t\tshape=plain\n");
	fprintf(stream, "\t]\n");

	if (data->parent_teid != 0xFFFFFFFF)
		fprintf(stream, "\tNODE_%d -> NODE_%d\n", data->parent_teid, data->node_teid);
}

static
int query_rl_profile(struct ice_hw *hw,
		     uint8_t level, uint8_t flags, uint16_t profile_id,
		     struct ice_aqc_rl_profile_elem *data)
{
	enum ice_status ice_status;

	data->level = level;
	data->flags = flags;
	data->profile_id = profile_id;

	ice_status = ice_aq_query_rl_profile(hw, 1, data,
					     sizeof(struct ice_aqc_rl_profile_elem), NULL);

	if (ice_status != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to query rl profile.");
		return -EINVAL;
	}

	return 0;
}

static
int query_node(struct ice_hw *hw, uint32_t child, uint32_t *parent,
	       uint8_t level, bool detail, FILE *stream)
{
	struct ice_aqc_txsched_elem_data data;
	enum ice_status status;
	struct ice_aqc_rl_profile_elem cir_prof;
	struct ice_aqc_rl_profile_elem eir_prof;
	struct ice_aqc_rl_profile_elem shared_prof;
	struct ice_aqc_rl_profile_elem *cp = NULL;
	struct ice_aqc_rl_profile_elem *ep = NULL;
	struct ice_aqc_rl_profile_elem *sp = NULL;
	int ret;

	status = ice_sched_query_elem(hw, child, &data);
	if (status != ICE_SUCCESS) {
		if (level == hw->num_tx_sched_layers) {
			/* ignore the error when a queue has been stopped. */
			PMD_DRV_LOG(WARNING, "Failed to query queue node %d.", child);
			*parent = 0xffffffff;
			return 0;
		}
		PMD_DRV_LOG(ERR, "Failed to query scheduling node %d.", child);
		return -EINVAL;
	}

	*parent = data.parent_teid;

	if (data.data.cir_bw.bw_profile_idx != 0) {
		ret = query_rl_profile(hw, level, 0, data.data.cir_bw.bw_profile_idx, &cir_prof);

		if (ret)
			return ret;
		cp = &cir_prof;
	}

	if (data.data.eir_bw.bw_profile_idx != 0) {
		ret = query_rl_profile(hw, level, 1, data.data.eir_bw.bw_profile_idx, &eir_prof);

		if (ret)
			return ret;
		ep = &eir_prof;
	}

	if (data.data.srl_id != 0) {
		ret = query_rl_profile(hw, level, 2, data.data.srl_id, &shared_prof);

		if (ret)
			return ret;
		sp = &shared_prof;
	}

	print_node(&data, cp, ep, sp, detail, stream);

	return 0;
}

static
int query_nodes(struct ice_hw *hw,
		uint32_t *children, int child_num,
		uint32_t *parents, int *parent_num,
		uint8_t level, bool detail,
		FILE *stream)
{
	uint32_t parent;
	int i;
	int j;

	*parent_num = 0;
	for (i = 0; i < child_num; i++) {
		bool exist = false;
		int ret;

		ret = query_node(hw, children[i], &parent, level, detail, stream);
		if (ret)
			return -EINVAL;

		for (j = 0; j < *parent_num; j++) {
			if (parents[j] == parent) {
				exist = true;
				break;
			}
		}

		if (!exist && parent != 0xFFFFFFFF)
			parents[(*parent_num)++] = parent;
	}

	return 0;
}

int rte_pmd_ice_dump_txsched(uint16_t port, bool detail, FILE *stream)
{
	struct rte_eth_dev *dev;
	struct ice_hw *hw;
	struct ice_pf *pf;
	struct ice_q_ctx *q_ctx;
	uint16_t q_num;
	uint16_t i;
	struct ice_tx_queue *txq;
	uint32_t buf1[256];
	uint32_t buf2[256];
	uint32_t *children = buf1;
	uint32_t *parents = buf2;
	int child_num = 0;
	int parent_num = 0;
	uint8_t level;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	if (!is_ice_supported(dev))
		return -ENOTSUP;

	dev = &rte_eth_devices[port];
	hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	level = hw->num_tx_sched_layers;

	q_num = dev->data->nb_tx_queues;

	/* main vsi */
	for (i = 0; i < q_num; i++) {
		txq = dev->data->tx_queues[i];
		q_ctx = ice_get_lan_q_ctx(hw, txq->vsi->idx, 0, i);
		children[child_num++] = q_ctx->q_teid;
	}

	/* fdir vsi */
	q_ctx = ice_get_lan_q_ctx(hw, pf->fdir.fdir_vsi->idx, 0, 0);
	children[child_num++] = q_ctx->q_teid;

	fprintf(stream, "digraph tx_sched {\n");
	while (child_num > 0) {
		int ret;
		ret = query_nodes(hw, children, child_num,
				  parents, &parent_num,
				  level, detail, stream);
		if (ret)
			return ret;

		children = parents;
		child_num = parent_num;
		level--;
	}
	fprintf(stream, "}\n");

	return 0;
}
