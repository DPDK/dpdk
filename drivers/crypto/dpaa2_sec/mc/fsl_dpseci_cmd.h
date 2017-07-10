/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2016 NXP.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *   GPL LICENSE SUMMARY
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _FSL_DPSECI_CMD_H
#define _FSL_DPSECI_CMD_H

/* DPSECI Version */
#define DPSECI_VER_MAJOR				5
#define DPSECI_VER_MINOR				0

/* Command IDs */
#define DPSECI_CMDID_CLOSE                              ((0x800 << 4) | (0x1))
#define DPSECI_CMDID_OPEN                               ((0x809 << 4) | (0x1))
#define DPSECI_CMDID_CREATE                             ((0x909 << 4) | (0x1))
#define DPSECI_CMDID_DESTROY                            ((0x989 << 4) | (0x1))
#define DPSECI_CMDID_GET_API_VERSION                    ((0xa09 << 4) | (0x1))

#define DPSECI_CMDID_ENABLE                             ((0x002 << 4) | (0x1))
#define DPSECI_CMDID_DISABLE                            ((0x003 << 4) | (0x1))
#define DPSECI_CMDID_GET_ATTR                           ((0x004 << 4) | (0x1))
#define DPSECI_CMDID_RESET                              ((0x005 << 4) | (0x1))
#define DPSECI_CMDID_IS_ENABLED                         ((0x006 << 4) | (0x1))

#define DPSECI_CMDID_SET_IRQ                            ((0x010 << 4) | (0x1))
#define DPSECI_CMDID_GET_IRQ                            ((0x011 << 4) | (0x1))
#define DPSECI_CMDID_SET_IRQ_ENABLE                     ((0x012 << 4) | (0x1))
#define DPSECI_CMDID_GET_IRQ_ENABLE                     ((0x013 << 4) | (0x1))
#define DPSECI_CMDID_SET_IRQ_MASK                       ((0x014 << 4) | (0x1))
#define DPSECI_CMDID_GET_IRQ_MASK                       ((0x015 << 4) | (0x1))
#define DPSECI_CMDID_GET_IRQ_STATUS                     ((0x016 << 4) | (0x1))
#define DPSECI_CMDID_CLEAR_IRQ_STATUS                   ((0x017 << 4) | (0x1))

#define DPSECI_CMDID_SET_RX_QUEUE                       ((0x194 << 4) | (0x1))
#define DPSECI_CMDID_GET_RX_QUEUE                       ((0x196 << 4) | (0x1))
#define DPSECI_CMDID_GET_TX_QUEUE                       ((0x197 << 4) | (0x1))
#define DPSECI_CMDID_GET_SEC_ATTR                       ((0x198 << 4) | (0x1))
#define DPSECI_CMDID_GET_SEC_COUNTERS                   ((0x199 << 4) | (0x1))

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_CMD_OPEN(cmd, dpseci_id) \
	MC_CMD_OP(cmd, 0, 0,  32, int,      dpseci_id)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_CMD_CREATE(cmd, cfg) \
do { \
	MC_CMD_OP(cmd, 0, 0,  8,  uint8_t,  cfg->priorities[0]);\
	MC_CMD_OP(cmd, 0, 8,  8,  uint8_t,  cfg->priorities[1]);\
	MC_CMD_OP(cmd, 0, 16, 8,  uint8_t,  cfg->priorities[2]);\
	MC_CMD_OP(cmd, 0, 24, 8,  uint8_t,  cfg->priorities[3]);\
	MC_CMD_OP(cmd, 0, 32, 8,  uint8_t,  cfg->priorities[4]);\
	MC_CMD_OP(cmd, 0, 40, 8,  uint8_t,  cfg->priorities[5]);\
	MC_CMD_OP(cmd, 0, 48, 8,  uint8_t,  cfg->priorities[6]);\
	MC_CMD_OP(cmd, 0, 56, 8,  uint8_t,  cfg->priorities[7]);\
	MC_CMD_OP(cmd, 1, 0,  8,  uint8_t,  cfg->num_tx_queues);\
	MC_CMD_OP(cmd, 1, 8,  8,  uint8_t,  cfg->num_rx_queues);\
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_RSP_IS_ENABLED(cmd, en) \
	MC_RSP_OP(cmd, 0, 0,  1,  int,	    en)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_CMD_SET_IRQ(cmd, irq_index, irq_cfg) \
do { \
	MC_CMD_OP(cmd, 0, 0,  8,  uint8_t,  irq_index);\
	MC_CMD_OP(cmd, 0, 32, 32, uint32_t, irq_cfg->val);\
	MC_CMD_OP(cmd, 1, 0,  64, uint64_t, irq_cfg->addr);\
	MC_CMD_OP(cmd, 2, 0,  32, int,	    irq_cfg->irq_num); \
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_CMD_GET_IRQ(cmd, irq_index) \
	MC_CMD_OP(cmd, 0, 32, 8,  uint8_t,  irq_index)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_RSP_GET_IRQ(cmd, type, irq_cfg) \
do { \
	MC_RSP_OP(cmd, 0, 0,  32, uint32_t, irq_cfg->val); \
	MC_RSP_OP(cmd, 1, 0,  64, uint64_t, irq_cfg->addr);\
	MC_RSP_OP(cmd, 2, 0,  32, int,	    irq_cfg->irq_num); \
	MC_RSP_OP(cmd, 2, 32, 32, int,	    type); \
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_CMD_SET_IRQ_ENABLE(cmd, irq_index, enable_state) \
do { \
	MC_CMD_OP(cmd, 0, 0,  8,  uint8_t,  enable_state); \
	MC_CMD_OP(cmd, 0, 32, 8,  uint8_t,  irq_index); \
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_CMD_GET_IRQ_ENABLE(cmd, irq_index) \
	MC_CMD_OP(cmd, 0, 32, 8,  uint8_t,  irq_index)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_RSP_GET_IRQ_ENABLE(cmd, enable_state) \
	MC_RSP_OP(cmd, 0, 0,  8,  uint8_t,  enable_state)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_CMD_SET_IRQ_MASK(cmd, irq_index, mask) \
do { \
	MC_CMD_OP(cmd, 0, 0,  32, uint32_t, mask); \
	MC_CMD_OP(cmd, 0, 32, 8,  uint8_t,  irq_index); \
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_CMD_GET_IRQ_MASK(cmd, irq_index) \
	MC_CMD_OP(cmd, 0, 32, 8,  uint8_t,  irq_index)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_RSP_GET_IRQ_MASK(cmd, mask) \
	MC_RSP_OP(cmd, 0, 0,  32, uint32_t, mask)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_CMD_GET_IRQ_STATUS(cmd, irq_index, status) \
do { \
	MC_CMD_OP(cmd, 0, 0,  32, uint32_t, status);\
	MC_CMD_OP(cmd, 0, 32, 8,  uint8_t,  irq_index);\
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_RSP_GET_IRQ_STATUS(cmd, status) \
	MC_RSP_OP(cmd, 0, 0,  32, uint32_t,  status)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_CMD_CLEAR_IRQ_STATUS(cmd, irq_index, status) \
do { \
	MC_CMD_OP(cmd, 0, 0,  32, uint32_t, status); \
	MC_CMD_OP(cmd, 0, 32, 8,  uint8_t,  irq_index); \
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_RSP_GET_ATTR(cmd, attr) \
do { \
	MC_RSP_OP(cmd, 0, 0,  32, int,	    attr->id); \
	MC_RSP_OP(cmd, 1, 0,  8,  uint8_t,  attr->num_tx_queues); \
	MC_RSP_OP(cmd, 1, 8,  8,  uint8_t,  attr->num_rx_queues); \
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_CMD_SET_RX_QUEUE(cmd, queue, cfg) \
do { \
	MC_CMD_OP(cmd, 0, 0,  32, int,      cfg->dest_cfg.dest_id); \
	MC_CMD_OP(cmd, 0, 32, 8,  uint8_t,  cfg->dest_cfg.priority); \
	MC_CMD_OP(cmd, 0, 40, 8,  uint8_t,  queue); \
	MC_CMD_OP(cmd, 0, 48, 4,  enum dpseci_dest, cfg->dest_cfg.dest_type); \
	MC_CMD_OP(cmd, 1, 0,  64, uint64_t, cfg->user_ctx); \
	MC_CMD_OP(cmd, 2, 0,  32, uint32_t, cfg->options);\
	MC_CMD_OP(cmd, 2, 32, 1,  int,		cfg->order_preservation_en);\
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_CMD_GET_RX_QUEUE(cmd, queue) \
	MC_CMD_OP(cmd, 0, 40, 8,  uint8_t,  queue)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_RSP_GET_RX_QUEUE(cmd, attr) \
do { \
	MC_RSP_OP(cmd, 0, 0,  32, int,      attr->dest_cfg.dest_id);\
	MC_RSP_OP(cmd, 0, 32, 8,  uint8_t,  attr->dest_cfg.priority);\
	MC_RSP_OP(cmd, 0, 48, 4,  enum dpseci_dest, attr->dest_cfg.dest_type);\
	MC_RSP_OP(cmd, 1, 0,  8,  uint64_t,  attr->user_ctx);\
	MC_RSP_OP(cmd, 2, 0,  32, uint32_t,  attr->fqid);\
	MC_RSP_OP(cmd, 2, 32, 1,  int,		 attr->order_preservation_en);\
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_CMD_GET_TX_QUEUE(cmd, queue) \
	MC_CMD_OP(cmd, 0, 40, 8,  uint8_t,  queue)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_RSP_GET_TX_QUEUE(cmd, attr) \
do { \
	MC_RSP_OP(cmd, 0, 32, 32, uint32_t,  attr->fqid);\
	MC_RSP_OP(cmd, 1, 0,  8,  uint8_t,   attr->priority);\
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_RSP_GET_SEC_ATTR(cmd, attr) \
do { \
	MC_RSP_OP(cmd, 0,  0, 16, uint16_t,  attr->ip_id);\
	MC_RSP_OP(cmd, 0, 16,  8,  uint8_t,  attr->major_rev);\
	MC_RSP_OP(cmd, 0, 24,  8,  uint8_t,  attr->minor_rev);\
	MC_RSP_OP(cmd, 0, 32,  8,  uint8_t,  attr->era);\
	MC_RSP_OP(cmd, 1,  0,  8,  uint8_t,  attr->deco_num);\
	MC_RSP_OP(cmd, 1,  8,  8,  uint8_t,  attr->zuc_auth_acc_num);\
	MC_RSP_OP(cmd, 1, 16,  8,  uint8_t,  attr->zuc_enc_acc_num);\
	MC_RSP_OP(cmd, 1, 32,  8,  uint8_t,  attr->snow_f8_acc_num);\
	MC_RSP_OP(cmd, 1, 40,  8,  uint8_t,  attr->snow_f9_acc_num);\
	MC_RSP_OP(cmd, 1, 48,  8,  uint8_t,  attr->crc_acc_num);\
	MC_RSP_OP(cmd, 2,  0,  8,  uint8_t,  attr->pk_acc_num);\
	MC_RSP_OP(cmd, 2,  8,  8,  uint8_t,  attr->kasumi_acc_num);\
	MC_RSP_OP(cmd, 2, 16,  8,  uint8_t,  attr->rng_acc_num);\
	MC_RSP_OP(cmd, 2, 32,  8,  uint8_t,  attr->md_acc_num);\
	MC_RSP_OP(cmd, 2, 40,  8,  uint8_t,  attr->arc4_acc_num);\
	MC_RSP_OP(cmd, 2, 48,  8,  uint8_t,  attr->des_acc_num);\
	MC_RSP_OP(cmd, 2, 56,  8,  uint8_t,  attr->aes_acc_num);\
} while (0)

/*                cmd, param, offset, width, type, arg_name */
#define DPSECI_RSP_GET_SEC_COUNTERS(cmd, counters) \
do { \
	MC_RSP_OP(cmd, 0,  0, 64, uint64_t,  counters->dequeued_requests);\
	MC_RSP_OP(cmd, 1,  0, 64, uint64_t,  counters->ob_enc_requests);\
	MC_RSP_OP(cmd, 2,  0, 64, uint64_t,  counters->ib_dec_requests);\
	MC_RSP_OP(cmd, 3,  0, 64, uint64_t,  counters->ob_enc_bytes);\
	MC_RSP_OP(cmd, 4,  0, 64, uint64_t,  counters->ob_prot_bytes);\
	MC_RSP_OP(cmd, 5,  0, 64, uint64_t,  counters->ib_dec_bytes);\
	MC_RSP_OP(cmd, 6,  0, 64, uint64_t,  counters->ib_valid_bytes);\
} while (0)

/*                cmd, param, offset, width, type,      arg_name */
#define DPSECI_RSP_GET_API_VERSION(cmd, major, minor) \
do { \
	MC_RSP_OP(cmd, 0, 0,  16, uint16_t, major);\
	MC_RSP_OP(cmd, 0, 16, 16, uint16_t, minor);\
} while (0)

#endif /* _FSL_DPSECI_CMD_H */
