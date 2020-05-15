/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016-2017 NXP
 *
 */
#ifndef __FSL_MC_CMD_H
#define __FSL_MC_CMD_H

#include <stdint.h>

#include <rte_byteorder.h>
#include <rte_compat.h>

#define MC_CMD_NUM_OF_PARAMS	7

#define phys_addr_t	uint64_t

#define u64	uint64_t
#define u32	uint32_t
#define u16	uint16_t
#define u8	uint8_t

#define cpu_to_le64	rte_cpu_to_le_64
#define cpu_to_le32	rte_cpu_to_le_32
#define cpu_to_le16	rte_cpu_to_le_16

#define le64_to_cpu	rte_le_to_cpu_64
#define le32_to_cpu	rte_le_to_cpu_32
#define le16_to_cpu	rte_le_to_cpu_16

#define BITS_PER_LONG	(__SIZEOF_LONG__ * 8)
#define GENMASK(h, l) \
		(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

struct mc_cmd_header {
	union {
		struct {
			uint8_t src_id;
			uint8_t flags_hw;
			uint8_t status;
			uint8_t flags_sw;
			uint16_t token;
			uint16_t cmd_id;
		};
		uint32_t word[2];
	};
};

struct mc_command {
	uint64_t header;
	uint64_t params[MC_CMD_NUM_OF_PARAMS];
};

struct mc_rsp_create {
	uint32_t object_id;
};

enum mc_cmd_status {
	MC_CMD_STATUS_OK = 0x0, /* Completed successfully */
	MC_CMD_STATUS_READY = 0x1, /* Ready to be processed */
	MC_CMD_STATUS_AUTH_ERR = 0x3, /* Authentication error */
	MC_CMD_STATUS_NO_PRIVILEGE = 0x4, /* No privilege */
	MC_CMD_STATUS_DMA_ERR = 0x5, /* DMA or I/O error */
	MC_CMD_STATUS_CONFIG_ERR = 0x6, /* Configuration error */
	MC_CMD_STATUS_TIMEOUT = 0x7, /* Operation timed out */
	MC_CMD_STATUS_NO_RESOURCE = 0x8, /* No resources */
	MC_CMD_STATUS_NO_MEMORY = 0x9, /* No memory available */
	MC_CMD_STATUS_BUSY = 0xA, /* Device is busy */
	MC_CMD_STATUS_UNSUPPORTED_OP = 0xB, /* Unsupported operation */
	MC_CMD_STATUS_INVALID_STATE = 0xC /* Invalid state */
};

/*
 * MC command flags
 */

/* High priority flag */
#define MC_CMD_FLAG_PRI		0x80
/* Command completion flag */
#define MC_CMD_FLAG_INTR_DIS	0x01

#define MC_CMD_HDR_FLAGS_MASK	0xFF00FF00

__rte_internal
int mc_send_command(struct fsl_mc_io *mc_io, struct mc_command *cmd);

static inline uint64_t mc_encode_cmd_header(uint16_t cmd_id,
					    uint32_t cmd_flags,
					    uint16_t token)
{
	uint64_t header = 0;
	struct mc_cmd_header *hdr = (struct mc_cmd_header *)&header;

	hdr->cmd_id = cpu_to_le16(cmd_id);
	hdr->token = cpu_to_le16(token);
	hdr->status = MC_CMD_STATUS_READY;
	hdr->word[0] |= cpu_to_le32(cmd_flags & MC_CMD_HDR_FLAGS_MASK);

	return header;
}

static inline uint16_t mc_cmd_hdr_read_token(struct mc_command *cmd)
{
	struct mc_cmd_header *hdr = (struct mc_cmd_header *)&cmd->header;
	uint16_t token = le16_to_cpu(hdr->token);

	return token;
}

static inline uint32_t mc_cmd_read_object_id(struct mc_command *cmd)
{
	struct mc_rsp_create *rsp_params;

	rsp_params = (struct mc_rsp_create *)cmd->params;
	return le32_to_cpu(rsp_params->object_id);
}

static inline enum mc_cmd_status mc_cmd_read_status(struct mc_command *cmd)
{
	struct mc_cmd_header *hdr = (struct mc_cmd_header *)&cmd->header;
	uint8_t status = hdr->status;

	return (enum mc_cmd_status)status;
}

/**
 * mc_write_command - writes a command to a Management Complex (MC) portal
 *
 * @portal: pointer to an MC portal
 * @cmd: pointer to a filled command
 */
static inline void mc_write_command(struct mc_command __iomem *portal,
				    struct mc_command *cmd)
{
	struct mc_cmd_header *cmd_header = (struct mc_cmd_header *)&cmd->header;
	char *header = (char *)&portal->header;
	int i;

	/* copy command parameters into the portal */
	for (i = 0; i < MC_CMD_NUM_OF_PARAMS; i++)
		iowrite64(cmd->params[i], &portal->params[i]);

	/* submit the command by writing the header */
	iowrite32(le32_to_cpu(cmd_header->word[1]), (((uint32_t *)header) + 1));
	iowrite32(le32_to_cpu(cmd_header->word[0]), (uint32_t *)header);
}

/**
 * mc_read_response - reads the response for the last MC command from a
 * Management Complex (MC) portal
 *
 * @portal: pointer to an MC portal
 * @resp: pointer to command response buffer
 *
 * Returns MC_CMD_STATUS_OK on Success; Error code otherwise.
 */
static inline enum mc_cmd_status mc_read_response(
					struct mc_command __iomem *portal,
					struct mc_command *resp)
{
	int i;
	enum mc_cmd_status status;

	/* Copy command response header from MC portal: */
	resp->header = ioread64(&portal->header);
	status = mc_cmd_read_status(resp);
	if (status != MC_CMD_STATUS_OK)
		return status;

	/* Copy command response data from MC portal: */
	for (i = 0; i < MC_CMD_NUM_OF_PARAMS; i++)
		resp->params[i] = ioread64(&portal->params[i]);

	return status;
}

#endif /* __FSL_MC_CMD_H */
