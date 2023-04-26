/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */
#ifndef _SIOV_REGS_H_
#define _SIOV_REGS_H_
#define VDEV_MBX_START			0x20000 /* Begin at 128KB */
#define VDEV_MBX_ATQBAL			(VDEV_MBX_START + 0x0000)
#define VDEV_MBX_ATQBAH			(VDEV_MBX_START + 0x0004)
#define VDEV_MBX_ATQLEN			(VDEV_MBX_START + 0x0008)
#define VDEV_MBX_ATQH			(VDEV_MBX_START + 0x000C)
#define VDEV_MBX_ATQT			(VDEV_MBX_START + 0x0010)
#define VDEV_MBX_ARQBAL			(VDEV_MBX_START + 0x0014)
#define VDEV_MBX_ARQBAH			(VDEV_MBX_START + 0x0018)
#define VDEV_MBX_ARQLEN			(VDEV_MBX_START + 0x001C)
#define VDEV_MBX_ARQH			(VDEV_MBX_START + 0x0020)
#define VDEV_MBX_ARQT			(VDEV_MBX_START + 0x0024)
#define VDEV_GET_RSTAT			0x21000 /* 132KB for RSTAT */

/* Begin at offset after 1MB (after 256 4k pages) */
#define VDEV_QRX_TAIL_START		0x100000
#define VDEV_QRX_TAIL(_i)		(VDEV_QRX_TAIL_START + ((_i) * 0x1000)) /* 2k Rx queues */

/* Begin at offset of 9MB for Rx buffer queue tail register pages */
#define VDEV_QRX_BUFQ_TAIL_START	0x900000
/* 2k Rx buffer queues */
#define VDEV_QRX_BUFQ_TAIL(_i)		(VDEV_QRX_BUFQ_TAIL_START + ((_i) * 0x1000))

/* Begin at offset of 17MB for 2k Tx queues */
#define VDEV_QTX_TAIL_START		0x1100000
#define VDEV_QTX_TAIL(_i)		(VDEV_QTX_TAIL_START + ((_i) * 0x1000)) /* 2k Tx queues */

/* Begin at offset of 25MB for 2k Tx completion queues */
#define VDEV_QTX_COMPL_TAIL_START	0x1900000
/* 2k Tx completion queues */
#define VDEV_QTX_COMPL_TAIL(_i)		(VDEV_QTX_COMPL_TAIL_START + ((_i) * 0x1000))

#define VDEV_INT_DYN_CTL01		0x2100000 /* Begin at offset 33MB */

/* Begin at offset of 33MB + 4k to accommodate CTL01 register */
#define VDEV_INT_DYN_START		(VDEV_INT_DYN_CTL01 + 0x1000)
#define VDEV_INT_DYN_CTL(_i)		(VDEV_INT_DYN_START + ((_i) * 0x1000))
#define VDEV_INT_ITR_0(_i)		(VDEV_INT_DYN_START + ((_i) * 0x1000) + 0x04)
#define VDEV_INT_ITR_1(_i)		(VDEV_INT_DYN_START + ((_i) * 0x1000) + 0x08)
#define VDEV_INT_ITR_2(_i)		(VDEV_INT_DYN_START + ((_i) * 0x1000) + 0x0C)

/* Next offset to begin at 42MB (0x2A00000) */
#endif /* _SIOV_REGS_H_ */
