/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __ROC_MBOX_PRIV_H__
#define __ROC_MBOX_PRIV_H__

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#define SZ_64K	  (64ULL * 1024ULL)
#define SZ_1K	  (1ULL * 1024ULL)
#define MBOX_SIZE SZ_64K

/* AF/PF: PF initiated, PF/VF VF initiated */
#define MBOX_DOWN_RX_START 0
#define MBOX_DOWN_RX_SIZE  (46 * SZ_1K)
#define MBOX_DOWN_TX_START (MBOX_DOWN_RX_START + MBOX_DOWN_RX_SIZE)
#define MBOX_DOWN_TX_SIZE  (16 * SZ_1K)
/* AF/PF: AF initiated, PF/VF PF initiated */
#define MBOX_UP_RX_START (MBOX_DOWN_TX_START + MBOX_DOWN_TX_SIZE)
#define MBOX_UP_RX_SIZE	 SZ_1K
#define MBOX_UP_TX_START (MBOX_UP_RX_START + MBOX_UP_RX_SIZE)
#define MBOX_UP_TX_SIZE	 SZ_1K

#if MBOX_UP_TX_SIZE + MBOX_UP_TX_START != MBOX_SIZE
#error "Incorrect mailbox area sizes"
#endif

#define INTR_MASK(pfvfs) ((pfvfs < 64) ? (BIT_ULL(pfvfs) - 1) : (~0ull))

#define MBOX_RSP_TIMEOUT 3000 /* Time to wait for mbox response in ms */

#define MBOX_MSG_ALIGN 16 /* Align mbox msg start to 16bytes */

/* Mailbox directions */
#define MBOX_DIR_AFPF	 0 /* AF replies to PF */
#define MBOX_DIR_PFAF	 1 /* PF sends messages to AF */
#define MBOX_DIR_PFVF	 2 /* PF replies to VF */
#define MBOX_DIR_VFPF	 3 /* VF sends messages to PF */
#define MBOX_DIR_AFPF_UP 4 /* AF sends messages to PF */
#define MBOX_DIR_PFAF_UP 5 /* PF replies to AF */
#define MBOX_DIR_PFVF_UP 6 /* PF sends messages to VF */
#define MBOX_DIR_VFPF_UP 7 /* VF replies to PF */

struct mbox_dev {
	void *mbase; /* This dev's mbox region */
	plt_spinlock_t mbox_lock;
	uint16_t msg_size;   /* Total msg size to be sent */
	uint16_t rsp_size;   /* Total rsp size to be sure the reply is ok */
	uint16_t num_msgs;   /* No of msgs sent or waiting for response */
	uint16_t msgs_acked; /* No of msgs for which response is received */
};

struct mbox {
	uintptr_t hwbase;   /* Mbox region advertised by HW */
	uintptr_t reg_base; /* CSR base for this dev */
	uint64_t trigger;   /* Trigger mbox notification */
	uint16_t tr_shift;  /* Mbox trigger shift */
	uint64_t rx_start;  /* Offset of Rx region in mbox memory */
	uint64_t tx_start;  /* Offset of Tx region in mbox memory */
	uint16_t rx_size;   /* Size of Rx region */
	uint16_t tx_size;   /* Size of Tx region */
	uint16_t ndevs;	    /* The number of peers */
	struct mbox_dev *dev;
	uint64_t intr_offset; /* Offset to interrupt register */
	uint32_t rsp_tmo;
};

const char *mbox_id2name(uint16_t id);
int mbox_id2size(uint16_t id);
void mbox_reset(struct mbox *mbox, int devid);
int mbox_init(struct mbox *mbox, uintptr_t hwbase, uintptr_t reg_base, int direction, int ndevsi,
	      uint64_t intr_offset);
void mbox_fini(struct mbox *mbox);
void mbox_msg_send(struct mbox *mbox, int devid);
void mbox_msg_send_up(struct mbox *mbox, int devid);
bool mbox_wait_for_zero(struct mbox *mbox, int devid);
int mbox_wait_for_rsp(struct mbox *mbox, int devid);
int mbox_wait_for_rsp_tmo(struct mbox *mbox, int devid, uint32_t tmo);
int mbox_get_rsp(struct mbox *mbox, int devid, void **msg);
int mbox_get_rsp_tmo(struct mbox *mbox, int devid, void **msg, uint32_t tmo);
int mbox_get_availmem(struct mbox *mbox, int devid);
struct mbox_msghdr *mbox_alloc_msg_rsp(struct mbox *mbox, int devid, int size,
				       int size_rsp);

static inline struct mbox_msghdr *
mbox_alloc_msg(struct mbox *mbox, int devid, int size)
{
	return mbox_alloc_msg_rsp(mbox, devid, size, 0);
}

static inline void
mbox_req_init(uint16_t mbox_id, void *msghdr)
{
	struct mbox_msghdr *hdr = msghdr;

	hdr->sig = MBOX_REQ_SIG;
	hdr->ver = MBOX_VERSION;
	hdr->id = mbox_id;
	hdr->pcifunc = 0;
}

static inline void
mbox_rsp_init(uint16_t mbox_id, void *msghdr)
{
	struct mbox_msghdr *hdr = msghdr;

	hdr->sig = MBOX_RSP_SIG;
	hdr->rc = -ETIMEDOUT;
	hdr->id = mbox_id;
}

static inline bool
mbox_nonempty(struct mbox *mbox, int devid)
{
	struct mbox_dev *mdev = &mbox->dev[devid];
	bool ret;

	plt_spinlock_lock(&mdev->mbox_lock);
	ret = mdev->num_msgs != 0;
	plt_spinlock_unlock(&mdev->mbox_lock);

	return ret;
}

static inline int
mbox_process(struct mbox *mbox)
{
	mbox_msg_send(mbox, 0);
	return mbox_get_rsp(mbox, 0, NULL);
}

static inline int
mbox_process_msg(struct mbox *mbox, void **msg)
{
	mbox_msg_send(mbox, 0);
	return mbox_get_rsp(mbox, 0, msg);
}

static inline int
mbox_process_tmo(struct mbox *mbox, uint32_t tmo)
{
	mbox_msg_send(mbox, 0);
	return mbox_get_rsp_tmo(mbox, 0, NULL, tmo);
}

static inline int
mbox_process_msg_tmo(struct mbox *mbox, void **msg, uint32_t tmo)
{
	mbox_msg_send(mbox, 0);
	return mbox_get_rsp_tmo(mbox, 0, msg, tmo);
}

static inline struct mbox *
mbox_get(struct mbox *mbox)
{
	struct mbox_dev *mdev = &mbox->dev[0];
	plt_spinlock_lock(&mdev->mbox_lock);
	return mbox;
}

static inline void
mbox_put(struct mbox *mbox)
{
	struct mbox_dev *mdev = &mbox->dev[0];
	plt_spinlock_unlock(&mdev->mbox_lock);
}

int send_ready_msg(struct mbox *mbox, uint16_t *pf_func /* out */);
int reply_invalid_msg(struct mbox *mbox, int devid, uint16_t pf_func,
		      uint16_t id);

#define M(_name, _id, _fn_name, _req_type, _rsp_type)                          \
	static inline struct _req_type *mbox_alloc_msg_##_fn_name(             \
		struct mbox *mbox)                                             \
	{                                                                      \
		struct _req_type *req;                                         \
		req = (struct _req_type *)mbox_alloc_msg_rsp(                  \
			mbox, 0, sizeof(struct _req_type),                     \
			sizeof(struct _rsp_type));                             \
		if (!req)                                                      \
			return NULL;                                           \
		req->hdr.sig = MBOX_REQ_SIG;                                   \
		req->hdr.id = _id;                                             \
		plt_mbox_dbg("id=0x%x (%s)", req->hdr.id,                      \
			     mbox_id2name(req->hdr.id));                       \
		return req;                                                    \
	}

MBOX_MESSAGES
#undef M

/* This is required for copy operations from device memory which do not work on
 * addresses which are unaligned to 16B. This is because of specific
 * optimizations to libc memcpy.
 */
static inline volatile void *
mbox_memcpy(volatile void *d, const volatile void *s, size_t l)
{
	const volatile uint8_t *sb;
	volatile uint8_t *db;
	size_t i;

	if (!d || !s)
		return NULL;
	db = (volatile uint8_t *)d;
	sb = (const volatile uint8_t *)s;
	for (i = 0; i < l; i++)
		db[i] = sb[i];
	return d;
}

/* This is required for memory operations from device memory which do not
 * work on addresses which are unaligned to 16B. This is because of specific
 * optimizations to libc memset.
 */
static inline void
mbox_memset(volatile void *d, uint8_t val, size_t l)
{
	volatile uint8_t *db;
	size_t i = 0;

	if (!d || !l)
		return;
	db = (volatile uint8_t *)d;
	for (i = 0; i < l; i++)
		db[i] = val;
}

#endif /* __ROC_MBOX_PRIV_H__ */
