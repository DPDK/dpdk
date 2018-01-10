/*******************************************************************************

Copyright (c) 2013 - 2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#ifndef _AVF_ADMINQ_H_
#define _AVF_ADMINQ_H_

#include "avf_osdep.h"
#include "avf_status.h"
#include "avf_adminq_cmd.h"

#define AVF_ADMINQ_DESC(R, i)   \
	(&(((struct avf_aq_desc *)((R).desc_buf.va))[i]))

#define AVF_ADMINQ_DESC_ALIGNMENT 4096

struct avf_adminq_ring {
	struct avf_virt_mem dma_head;	/* space for dma structures */
	struct avf_dma_mem desc_buf;	/* descriptor ring memory */
	struct avf_virt_mem cmd_buf;	/* command buffer memory */

	union {
		struct avf_dma_mem *asq_bi;
		struct avf_dma_mem *arq_bi;
	} r;

	u16 count;		/* Number of descriptors */
	u16 rx_buf_len;		/* Admin Receive Queue buffer length */

	/* used for interrupt processing */
	u16 next_to_use;
	u16 next_to_clean;

	/* used for queue tracking */
	u32 head;
	u32 tail;
	u32 len;
	u32 bah;
	u32 bal;
};

/* ASQ transaction details */
struct avf_asq_cmd_details {
	void *callback; /* cast from type AVF_ADMINQ_CALLBACK */
	u64 cookie;
	u16 flags_ena;
	u16 flags_dis;
	bool async;
	bool postpone;
	struct avf_aq_desc *wb_desc;
};

#define AVF_ADMINQ_DETAILS(R, i)   \
	(&(((struct avf_asq_cmd_details *)((R).cmd_buf.va))[i]))

/* ARQ event information */
struct avf_arq_event_info {
	struct avf_aq_desc desc;
	u16 msg_len;
	u16 buf_len;
	u8 *msg_buf;
};

/* Admin Queue information */
struct avf_adminq_info {
	struct avf_adminq_ring arq;    /* receive queue */
	struct avf_adminq_ring asq;    /* send queue */
	u32 asq_cmd_timeout;            /* send queue cmd write back timeout*/
	u16 num_arq_entries;            /* receive queue depth */
	u16 num_asq_entries;            /* send queue depth */
	u16 arq_buf_size;               /* receive queue buffer size */
	u16 asq_buf_size;               /* send queue buffer size */
	u16 fw_maj_ver;                 /* firmware major version */
	u16 fw_min_ver;                 /* firmware minor version */
	u32 fw_build;                   /* firmware build number */
	u16 api_maj_ver;                /* api major version */
	u16 api_min_ver;                /* api minor version */

	struct avf_spinlock asq_spinlock; /* Send queue spinlock */
	struct avf_spinlock arq_spinlock; /* Receive queue spinlock */

	/* last status values on send and receive queues */
	enum avf_admin_queue_err asq_last_status;
	enum avf_admin_queue_err arq_last_status;
};

/**
 * avf_aq_rc_to_posix - convert errors to user-land codes
 * aq_ret: AdminQ handler error code can override aq_rc
 * aq_rc: AdminQ firmware error code to convert
 **/
STATIC INLINE int avf_aq_rc_to_posix(int aq_ret, int aq_rc)
{
	int aq_to_posix[] = {
		0,           /* AVF_AQ_RC_OK */
		-EPERM,      /* AVF_AQ_RC_EPERM */
		-ENOENT,     /* AVF_AQ_RC_ENOENT */
		-ESRCH,      /* AVF_AQ_RC_ESRCH */
		-EINTR,      /* AVF_AQ_RC_EINTR */
		-EIO,        /* AVF_AQ_RC_EIO */
		-ENXIO,      /* AVF_AQ_RC_ENXIO */
		-E2BIG,      /* AVF_AQ_RC_E2BIG */
		-EAGAIN,     /* AVF_AQ_RC_EAGAIN */
		-ENOMEM,     /* AVF_AQ_RC_ENOMEM */
		-EACCES,     /* AVF_AQ_RC_EACCES */
		-EFAULT,     /* AVF_AQ_RC_EFAULT */
		-EBUSY,      /* AVF_AQ_RC_EBUSY */
		-EEXIST,     /* AVF_AQ_RC_EEXIST */
		-EINVAL,     /* AVF_AQ_RC_EINVAL */
		-ENOTTY,     /* AVF_AQ_RC_ENOTTY */
		-ENOSPC,     /* AVF_AQ_RC_ENOSPC */
		-ENOSYS,     /* AVF_AQ_RC_ENOSYS */
		-ERANGE,     /* AVF_AQ_RC_ERANGE */
		-EPIPE,      /* AVF_AQ_RC_EFLUSHED */
		-ESPIPE,     /* AVF_AQ_RC_BAD_ADDR */
		-EROFS,      /* AVF_AQ_RC_EMODE */
		-EFBIG,      /* AVF_AQ_RC_EFBIG */
	};

	/* aq_rc is invalid if AQ timed out */
	if (aq_ret == AVF_ERR_ADMIN_QUEUE_TIMEOUT)
		return -EAGAIN;

	if (!((u32)aq_rc < (sizeof(aq_to_posix) / sizeof((aq_to_posix)[0]))))
		return -ERANGE;

	return aq_to_posix[aq_rc];
}

/* general information */
#define AVF_AQ_LARGE_BUF	512
#define AVF_ASQ_CMD_TIMEOUT	250000  /* usecs */

void avf_fill_default_direct_cmd_desc(struct avf_aq_desc *desc,
				       u16 opcode);

#endif /* _AVF_ADMINQ_H_ */
