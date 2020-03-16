/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef	__OCTEONTX_PKO_H__
#define	__OCTEONTX_PKO_H__

#include <octeontx_mbox.h>

/* PKO maximum constants */
#define	PKO_VF_MAX			(32)
#define	PKO_VF_NUM_DQ			(8)
#define PKO_MAX_NUM_DQ			(8)
#define	PKO_DQ_DRAIN_TO			(1000)

#define PKO_DQ_FC_SKID			(4)
#define PKO_DQ_FC_DEPTH_PAGES		(2048)
#define PKO_DQ_FC_STRIDE_16		(16)
#define PKO_DQ_FC_STRIDE_128		(128)
#define PKO_DQ_FC_STRIDE		PKO_DQ_FC_STRIDE_16

#define PKO_DQ_KIND_BIT			49
#define PKO_DQ_STATUS_BIT		60
#define PKO_DQ_OP_BIT			48

/* PKO VF register offsets from VF_BAR0 */
#define	PKO_VF_DQ_SW_XOFF(gdq)		(0x000100 | (gdq) << 17)
#define	PKO_VF_DQ_WM_CTL(gdq)		(0x000130 | (gdq) << 17)
#define	PKO_VF_DQ_WM_CNT(gdq)		(0x000150 | (gdq) << 17)
#define	PKO_VF_DQ_FC_CONFIG		(0x000160)
#define	PKO_VF_DQ_FC_STATUS(gdq)	(0x000168 | (gdq) << 17)
#define	PKO_VF_DQ_OP_SEND(gdq, op)	(0x001000 | (gdq) << 17 | (op) << 3)
#define	PKO_VF_DQ_OP_OPEN(gdq)		(0x001100 | (gdq) << 17)
#define	PKO_VF_DQ_OP_CLOSE(gdq)		(0x001200 | (gdq) << 17)
#define	PKO_VF_DQ_OP_QUERY(gdq)		(0x001300 | (gdq) << 17)

/* pko_send_hdr_s + pko_send_link */
#define PKO_CMD_SZ			(2 << 1)
#define PKO_SEND_BUFLINK_SUBDC		(0x0ull << 60)
#define PKO_SEND_BUFLINK_LDTYPE(x)	((x) << 58)
#define PKO_SEND_BUFLINK_GAUAR(x)	((x) << 24)
#define PKO_SEND_GATHER_SUBDC		(0x2ull << 60)
#define PKO_SEND_GATHER_LDTYPE(x)	((x) << 58)
#define PKO_SEND_GATHER_GAUAR(x)	((x) << 24)

#define OCTEONTX_PKO_COPROC                     4
#define MBOX_PKO_MTU_CONFIG			1

typedef struct mbox_pko_mtu_cfg {
	uint32_t mtu;
} mbox_pko_mtu_cfg_t;

typedef struct octeontx_dq_s {
	void *lmtline_va;
	void *ioreg_va;
	void *fc_status_va;
} octeontx_dq_t;

/**
 * Function for extracting information out of a given DQ.
 *
 * It is intended to be used in slow path (configuration) in
 * octeontx_pko_channel_query().
 *
 * @param dq The DQ to extract information from.
 * @param out Pointer to the user's structure he wants to fill.
 */
typedef void (*octeontx_pko_dq_getter_t)(octeontx_dq_t *dq, void *out);

int
octeontx_pko_channel_query_dqs(int chanid, void *out, size_t out_elem_size,
			       size_t dq_num, octeontx_pko_dq_getter_t getter);
int octeontx_pko_channel_open(int dq_base, int dq_num, int chanid);
int octeontx_pko_channel_close(int chanid);
int octeontx_pko_channel_start(int chanid);
int octeontx_pko_channel_stop(int chanid);
int octeontx_pko_vf_count(void);
size_t octeontx_pko_get_vfid(void);
int octeontx_pko_init_fc(const size_t pko_vf_count);
void octeontx_pko_fc_free(void);
int octeontx_pko_send_mtu(int port, int mtu);

#endif /* __OCTEONTX_PKO_H__ */
