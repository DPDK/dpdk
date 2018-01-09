/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2012 Intel Corporation.

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _IXGBE_FCOE_H
#define _IXGBE_FCOE_H

#ifdef IXGBE_FCOE

#include <scsi/fc/fc_fs.h>
#include <scsi/fc/fc_fcoe.h>

/* shift bits within STAT fo FCSTAT */
#define IXGBE_RXDADV_FCSTAT_SHIFT	4

/* ddp user buffer */
#define IXGBE_BUFFCNT_MAX	256	/* 8 bits bufcnt */
#define IXGBE_FCPTR_ALIGN	16
#define IXGBE_FCPTR_MAX		(IXGBE_BUFFCNT_MAX * sizeof(dma_addr_t))
#define IXGBE_FCBUFF_4KB	0x0
#define IXGBE_FCBUFF_8KB	0x1
#define IXGBE_FCBUFF_16KB	0x2
#define IXGBE_FCBUFF_64KB	0x3
#define IXGBE_FCBUFF_MAX	65536	/* 64KB max */
#define IXGBE_FCBUFF_MIN	4096	/* 4KB min */
#define IXGBE_FCOE_DDP_MAX	512	/* 9 bits xid */

/* Default traffic class to use for FCoE */
#define IXGBE_FCOE_DEFTC	3

/* fcerr */
#define IXGBE_FCERR_BADCRC	0x00100000
#define IXGBE_FCERR_EOFSOF	0x00200000
#define IXGBE_FCERR_NOFIRST	0x00300000
#define IXGBE_FCERR_OOOSEQ	0x00400000
#define IXGBE_FCERR_NODMA	0x00500000
#define IXGBE_FCERR_PKTLOST	0x00600000

/* FCoE DDP for target mode */
#define __IXGBE_FCOE_TARGET	1

struct ixgbe_fcoe_ddp {
	int len;
	u32 err;
	unsigned int sgc;
	struct scatterlist *sgl;
	dma_addr_t udp;
	u64 *udl;
	struct pci_pool *pool;
};

struct ixgbe_fcoe {
	struct pci_pool **pool;
	atomic_t refcnt;
	spinlock_t lock;
	struct ixgbe_fcoe_ddp ddp[IXGBE_FCOE_DDP_MAX];
	unsigned char *extra_ddp_buffer;
	dma_addr_t extra_ddp_buffer_dma;
	u64 __percpu *pcpu_noddp;
	u64 __percpu *pcpu_noddp_ext_buff;
	unsigned long mode;
	u8 tc;
	u8 up;
	u8 up_set;
};
#endif /* IXGBE_FCOE */

#endif /* _IXGBE_FCOE_H */
