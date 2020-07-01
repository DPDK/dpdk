/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include <string.h>

#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_io.h>
#include <rte_spinlock.h>

#include "octeontx_mbox.h"

/* Mbox operation timeout in seconds */
#define MBOX_WAIT_TIME_SEC	3
#define MAX_RAM_MBOX_LEN	((SSOW_BAR4_LEN >> 1) - 8 /* Mbox header */)

/* Mbox channel state */
enum {
	MBOX_CHAN_STATE_REQ = 1,
	MBOX_CHAN_STATE_RES = 0,
};

/* Response messages */
enum {
	MBOX_RET_SUCCESS,
	MBOX_RET_INVALID,
	MBOX_RET_INTERNAL_ERR,
};

struct mbox {
	int init_once;
	uint8_t ready;
	uint8_t *ram_mbox_base; /* Base address of mbox message stored in ram */
	uint8_t *reg; /* Store to this register triggers PF mbox interrupt */
	uint16_t tag_own; /* Last tag which was written to own channel */
	uint16_t domain; /* Domain */
	rte_spinlock_t lock;
};

static struct mbox octeontx_mbox;

/*
 * Structure used for mbox synchronization
 * This structure sits at the begin of Mbox RAM and used as main
 * synchronization point for channel communication
 */
struct mbox_ram_hdr {
	union {
		uint64_t u64;
		struct {
			uint8_t chan_state : 1;
			uint8_t coproc : 7;
			uint8_t msg;
			uint8_t vfid;
			uint8_t res_code;
			uint16_t tag;
			uint16_t len;
		};
	};
};

/* MBOX interface version message */
struct mbox_intf_ver {
	uint32_t platform:12;
	uint32_t major:10;
	uint32_t minor:10;
};

RTE_LOG_REGISTER(octeontx_logtype_mbox, pmd.octeontx.mbox, NOTICE);

static inline void
mbox_msgcpy(volatile uint8_t *d, volatile const uint8_t *s, uint16_t size)
{
	uint16_t i;

	for (i = 0; i < size; i++)
		d[i] = s[i];
}

static inline void
mbox_send_request(struct mbox *m, struct octeontx_mbox_hdr *hdr,
			const void *txmsg, uint16_t txsize)
{
	struct mbox_ram_hdr old_hdr;
	struct mbox_ram_hdr new_hdr = { {0} };
	uint64_t *ram_mbox_hdr = (uint64_t *)m->ram_mbox_base;
	uint8_t *ram_mbox_msg = m->ram_mbox_base + sizeof(struct mbox_ram_hdr);

	/*
	 * Initialize the channel with the tag left by last send.
	 * On success full mbox send complete, PF increments the tag by one.
	 * The sender can validate integrity of PF message with this scheme
	 */
	old_hdr.u64 = rte_read64(ram_mbox_hdr);
	m->tag_own = (old_hdr.tag + 2) & (~0x1ul); /* next even number */

	/* Copy msg body */
	if (txmsg)
		mbox_msgcpy(ram_mbox_msg, txmsg, txsize);

	/* Prepare new hdr */
	new_hdr.chan_state = MBOX_CHAN_STATE_REQ;
	new_hdr.coproc = hdr->coproc;
	new_hdr.msg = hdr->msg;
	new_hdr.vfid = hdr->vfid;
	new_hdr.tag = m->tag_own;
	new_hdr.len = txsize;

	/* Write the msg header */
	rte_write64(new_hdr.u64, ram_mbox_hdr);
	rte_smp_wmb();
	/* Notify PF about the new msg - write to MBOX reg generates PF IRQ */
	rte_write64(0, m->reg);
}

static inline int
mbox_wait_response(struct mbox *m, struct octeontx_mbox_hdr *hdr,
			void *rxmsg, uint16_t rxsize)
{
	int res = 0, wait;
	uint16_t len;
	struct mbox_ram_hdr rx_hdr;
	uint64_t *ram_mbox_hdr = (uint64_t *)m->ram_mbox_base;
	uint8_t *ram_mbox_msg = m->ram_mbox_base + sizeof(struct mbox_ram_hdr);

	/* Wait for response */
	wait = MBOX_WAIT_TIME_SEC * 1000 * 10;
	while (wait > 0) {
		rte_delay_us(100);
		rx_hdr.u64 = rte_read64(ram_mbox_hdr);
		if (rx_hdr.chan_state == MBOX_CHAN_STATE_RES)
			break;
		--wait;
	}

	hdr->res_code = rx_hdr.res_code;
	m->tag_own++;

	/* Timeout */
	if (wait <= 0) {
		res = -ETIMEDOUT;
		goto error;
	}

	/* Tag mismatch */
	if (m->tag_own != rx_hdr.tag) {
		res = -EINVAL;
		goto error;
	}

	/* PF nacked the msg */
	if (rx_hdr.res_code != MBOX_RET_SUCCESS) {
		res = -EBADMSG;
		goto error;
	}

	len = RTE_MIN(rx_hdr.len, rxsize);
	if (rxmsg)
		mbox_msgcpy(rxmsg, ram_mbox_msg, len);

	return len;

error:
	mbox_log_err("Failed to send mbox(%d/%d) coproc=%d msg=%d ret=(%d,%d)",
			m->tag_own, rx_hdr.tag, hdr->coproc, hdr->msg, res,
			hdr->res_code);
	return res;
}

static inline int
mbox_send(struct mbox *m, struct octeontx_mbox_hdr *hdr, const void *txmsg,
		uint16_t txsize, void *rxmsg, uint16_t rxsize)
{
	int res = -EINVAL;

	if (m->init_once == 0 || hdr == NULL ||
		txsize > MAX_RAM_MBOX_LEN || rxsize > MAX_RAM_MBOX_LEN) {
		mbox_log_err("Invalid init_once=%d hdr=%p txsz=%d rxsz=%d",
				m->init_once, hdr, txsize, rxsize);
		return res;
	}

	rte_spinlock_lock(&m->lock);

	mbox_send_request(m, hdr, txmsg, txsize);
	res = mbox_wait_response(m, hdr, rxmsg, rxsize);

	rte_spinlock_unlock(&m->lock);
	return res;
}

int
octeontx_mbox_set_ram_mbox_base(uint8_t *ram_mbox_base, uint16_t domain)
{
	struct mbox *m = &octeontx_mbox;

	if (m->init_once)
		return -EALREADY;

	if (ram_mbox_base == NULL) {
		mbox_log_err("Invalid ram_mbox_base=%p", ram_mbox_base);
		return -EINVAL;
	}

	m->ram_mbox_base = ram_mbox_base;

	if (m->reg != NULL) {
		rte_spinlock_init(&m->lock);
		m->init_once = 1;
		m->domain = domain;
	}

	return 0;
}

int
octeontx_mbox_set_reg(uint8_t *reg, uint16_t domain)
{
	struct mbox *m = &octeontx_mbox;

	if (m->init_once)
		return -EALREADY;

	if (reg == NULL) {
		mbox_log_err("Invalid reg=%p", reg);
		return -EINVAL;
	}

	m->reg = reg;

	if (m->ram_mbox_base != NULL) {
		rte_spinlock_init(&m->lock);
		m->init_once = 1;
		m->domain = domain;
	}

	return 0;
}

int
octeontx_mbox_send(struct octeontx_mbox_hdr *hdr, void *txdata,
				 uint16_t txlen, void *rxdata, uint16_t rxlen)
{
	struct mbox *m = &octeontx_mbox;

	RTE_BUILD_BUG_ON(sizeof(struct mbox_ram_hdr) != 8);
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EINVAL;

	return mbox_send(m, hdr, txdata, txlen, rxdata, rxlen);
}

static int
octeontx_start_domain(void)
{
	struct octeontx_mbox_hdr hdr = {0};
	int result = -EINVAL;

	hdr.coproc = NO_COPROC;
	hdr.msg = RM_START_APP;

	result = octeontx_mbox_send(&hdr, NULL, 0, NULL, 0);
	if (result != 0) {
		mbox_log_err("Could not start domain. Err=%d. FuncErr=%d\n",
			     result, hdr.res_code);
		result = -EINVAL;
	}

	return result;
}

static int
octeontx_check_mbox_version(struct mbox_intf_ver *app_intf_ver,
			    struct mbox_intf_ver *intf_ver)
{
	struct mbox_intf_ver kernel_intf_ver = {0};
	struct octeontx_mbox_hdr hdr = {0};
	int result = 0;


	hdr.coproc = NO_COPROC;
	hdr.msg = RM_INTERFACE_VERSION;

	result = octeontx_mbox_send(&hdr, app_intf_ver,
				    sizeof(struct mbox_intf_ver),
				    &kernel_intf_ver, sizeof(kernel_intf_ver));
	if (result != sizeof(kernel_intf_ver)) {
		mbox_log_err("Could not send interface version. Err=%d. FuncErr=%d\n",
			     result, hdr.res_code);
		result = -EINVAL;
	}

	if (intf_ver)
		*intf_ver = kernel_intf_ver;

	if (app_intf_ver->platform != kernel_intf_ver.platform ||
			app_intf_ver->major != kernel_intf_ver.major ||
			app_intf_ver->minor != kernel_intf_ver.minor)
		result = -EINVAL;

	return result;
}

int
octeontx_mbox_init(void)
{
	struct mbox_intf_ver MBOX_INTERFACE_VERSION = {
		.platform = 0x01,
		.major = 0x01,
		.minor = 0x03
	};
	struct mbox_intf_ver rm_intf_ver = {0};
	struct mbox *m = &octeontx_mbox;
	int ret;

	if (m->ready)
		return 0;

	ret = octeontx_start_domain();
	if (ret < 0) {
		m->init_once = 0;
		return ret;
	}

	ret = octeontx_check_mbox_version(&MBOX_INTERFACE_VERSION,
					  &rm_intf_ver);
	if (ret < 0) {
		mbox_log_err("MBOX version: Kernel(%d.%d.%d) != DPDK(%d.%d.%d)",
			     rm_intf_ver.platform, rm_intf_ver.major,
			     rm_intf_ver.minor, MBOX_INTERFACE_VERSION.platform,
			     MBOX_INTERFACE_VERSION.major,
			     MBOX_INTERFACE_VERSION.minor);
		m->init_once = 0;
		return -EINVAL;
	}

	m->ready = 1;
	rte_mb();

	return 0;
}

uint16_t
octeontx_get_global_domain(void)
{
	struct mbox *m = &octeontx_mbox;

	return m->domain;
}
