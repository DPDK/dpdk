/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright 2013-2015 Freescale Semiconductor Inc.
 * Copyright 2016-2021 NXP
 *
 */
#ifndef __FSL_DPKG_H_
#define __FSL_DPKG_H_

#include <fsl_net.h>

/* Data Path Key Generator API
 * Contains initialization APIs and runtime APIs for the Key Generator
 */

/** Key Generator properties */

/**
 * Number of masks per key extraction
 */
#define DPKG_NUM_OF_MASKS		4
/**
 * Number of extractions per key profile
 */
#define DPKG_MAX_NUM_OF_EXTRACTS	20

/**
 * enum dpkg_extract_from_hdr_type - Selecting extraction by header types
 * @DPKG_FROM_HDR: Extract selected bytes from header, by offset
 * @DPKG_FROM_FIELD: Extract selected bytes from header, by offset from field
 * @DPKG_FULL_FIELD: Extract a full field
 */
enum dpkg_extract_from_hdr_type {
	DPKG_FROM_HDR = 0,
	DPKG_FROM_FIELD = 1,
	DPKG_FULL_FIELD = 2
};

/**
 * enum dpkg_extract_type - Enumeration for selecting extraction type
 * @DPKG_EXTRACT_FROM_HDR: Extract from the header
 * @DPKG_EXTRACT_FROM_DATA: Extract from data not in specific header
 * @DPKG_EXTRACT_FROM_PARSE: Extract from parser-result;
 *	e.g. can be used to extract header existence;
 *	please refer to 'Parse Result definition' section in the parser BG
 */
enum dpkg_extract_type {
	DPKG_EXTRACT_FROM_HDR = 0,
	DPKG_EXTRACT_FROM_DATA = 1,
	DPKG_EXTRACT_FROM_PARSE = 3
};

/**
 * struct dpkg_mask - A structure for defining a single extraction mask
 * @mask: Byte mask for the extracted content
 * @offset: Offset within the extracted content
 */
struct dpkg_mask {
	uint8_t mask;
	uint8_t offset;
};

/* Macros for accessing command fields smaller than 1byte */
#define DPKG_MASK(field)	\
	GENMASK(DPKG_##field##_SHIFT + DPKG_##field##_SIZE - 1, \
		DPKG_##field##_SHIFT)
#define dpkg_set_field(var, field, val)	\
	((var) |= (((val) << DPKG_##field##_SHIFT) & DPKG_MASK(field)))
#define dpkg_get_field(var, field)	\
	(((var) & DPKG_MASK(field)) >> DPKG_##field##_SHIFT)

/**
 * struct dpkg_extract - A structure for defining a single extraction
 * @type: Determines how the union below is interpreted:
 *	DPKG_EXTRACT_FROM_HDR: selects 'from_hdr';
 *	DPKG_EXTRACT_FROM_DATA: selects 'from_data';
 *	DPKG_EXTRACT_FROM_PARSE: selects 'from_parse'
 * @extract: Selects extraction method
 * @extract.from_hdr: Used when 'type = DPKG_EXTRACT_FROM_HDR'
 * @extract.from_data: Used when 'type = DPKG_EXTRACT_FROM_DATA'
 * @extract.from_parse:  Used when 'type = DPKG_EXTRACT_FROM_PARSE'
 * @extract.from_hdr.prot: Any of the supported headers
 * @extract.from_hdr.type: Defines the type of header extraction:
 *	DPKG_FROM_HDR: use size & offset below;
 *	DPKG_FROM_FIELD: use field, size and offset below;
 *	DPKG_FULL_FIELD: use field below
 * @extract.from_hdr.field: One of the supported fields (NH_FLD_)
 * @extract.from_hdr.size: Size in bytes
 * @extract.from_hdr.offset: Byte offset
 * @extract.from_hdr.hdr_index: Clear for cases not listed below;
 *	Used for protocols that may have more than a single
 *	header, 0 indicates an outer header;
 *	Supported protocols (possible values):
 *	NET_PROT_VLAN (0, HDR_INDEX_LAST);
 *	NET_PROT_MPLS (0, 1, HDR_INDEX_LAST);
 *	NET_PROT_IP(0, HDR_INDEX_LAST);
 *	NET_PROT_IPv4(0, HDR_INDEX_LAST);
 *	NET_PROT_IPv6(0, HDR_INDEX_LAST);
 * @extract.from_data.size: Size in bytes
 * @extract.from_data.offset: Byte offset
 * @extract.from_parse.size: Size in bytes
 * @extract.from_parse.offset: Byte offset
 * @num_of_byte_masks: Defines the number of valid entries in the array below;
 *		This is	also the number of bytes to be used as masks
 * @masks: Masks parameters
 */
struct dpkg_extract {
	enum dpkg_extract_type type;
	union {
		struct {
			enum net_prot prot;
			enum dpkg_extract_from_hdr_type type;
			uint32_t field;
			uint8_t size;
			uint8_t offset;
			uint8_t hdr_index;
		} from_hdr;
		struct {
			uint8_t size;
			uint8_t offset;
		} from_data;
		struct {
			uint8_t size;
			uint8_t offset;
		} from_parse;
	} extract;

	uint8_t num_of_byte_masks;
	struct dpkg_mask masks[DPKG_NUM_OF_MASKS];
};

/**
 * struct dpkg_profile_cfg - A structure for defining a full Key Generation
 *				profile (rule)
 * @num_extracts: Defines the number of valid entries in the array below
 * @extracts: Array of required extractions
 */
struct dpkg_profile_cfg {
	uint8_t num_extracts;
	struct dpkg_extract extracts[DPKG_MAX_NUM_OF_EXTRACTS];
};

/* dpni_set_rx_tc_dist extension (structure of the DMA-able memory at
 * key_cfg_iova)
 */
struct dpni_mask_cfg {
	uint8_t mask;
	uint8_t offset;
};

#define DPKG_EFH_TYPE_SHIFT		0
#define DPKG_EFH_TYPE_SIZE		4
#define DPKG_EXTRACT_TYPE_SHIFT		0
#define DPKG_EXTRACT_TYPE_SIZE		4

struct dpni_dist_extract {
	/* word 0 */
	uint8_t prot;
	/* EFH type stored in the 4 least significant bits */
	uint8_t efh_type;
	uint8_t size;
	uint8_t offset;
	uint32_t field;
	/* word 1 */
	uint8_t hdr_index;
	uint8_t constant;
	uint8_t num_of_repeats;
	uint8_t num_of_byte_masks;
	/* Extraction type is stored in the 4 LSBs */
	uint8_t extract_type;
	uint8_t pad[3];
	/* word 2 */
	struct dpni_mask_cfg masks[4];
};

struct dpni_ext_set_rx_tc_dist {
	/* extension word 0 */
	uint8_t num_extracts;
	uint8_t pad[7];
	/* words 1..25 */
	struct dpni_dist_extract extracts[DPKG_MAX_NUM_OF_EXTRACTS];
};

int dpkg_prepare_key_cfg(const struct dpkg_profile_cfg *cfg,
			 uint8_t *key_cfg_buf);

#endif /* __FSL_DPKG_H_ */
