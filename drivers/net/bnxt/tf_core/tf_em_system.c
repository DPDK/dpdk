/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <math.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_log.h>

#include "tf_core.h"
#include "tf_util.h"
#include "tf_common.h"
#include "tf_em.h"
#include "tf_em_common.h"
#include "tf_msg.h"
#include "tfp.h"
#include "lookup3.h"
#include "tf_ext_flow_handle.h"

#include "bnxt.h"

enum tf_em_req_type {
	TF_EM_BNXT_LFC_CFA_EEM_DMABUF_EXPORT_REQ = 5,
};

struct tf_em_bnxt_lfc_req_hdr {
	uint32_t ver;
	uint32_t bus;
	uint32_t devfn;
	enum tf_em_req_type req_type;
};

struct tf_em_bnxt_lfc_cfa_eem_std_hdr {
	uint16_t version;
	uint16_t size;
	uint32_t flags;
	#define TF_EM_BNXT_LFC_EEM_CFG_PRIMARY_FUNC     (1 << 0)
};

struct tf_em_bnxt_lfc_dmabuf_fd {
	int fd[TF_DIR_MAX][TF_MAX_TABLE];
};

#ifndef __user
#define __user
#endif

struct tf_em_bnxt_lfc_cfa_eem_dmabuf_export_req {
	struct tf_em_bnxt_lfc_cfa_eem_std_hdr std;
	uint8_t dir;
	uint32_t flags;
	void __user *dma_fd;
};

struct tf_em_bnxt_lfc_req {
	struct tf_em_bnxt_lfc_req_hdr hdr;
	union {
		struct tf_em_bnxt_lfc_cfa_eem_dmabuf_export_req
		       eem_dmabuf_export_req;
		uint64_t hreq;
	} req;
};

#define TF_EEM_BNXT_LFC_IOCTL_MAGIC     0x98
#define BNXT_LFC_REQ    \
	_IOW(TF_EEM_BNXT_LFC_IOCTL_MAGIC, 1, struct tf_em_bnxt_lfc_req)

/**
 * EM DBs.
 */
extern void *eem_db[TF_DIR_MAX];

extern struct tf_tbl_scope_cb tbl_scopes[TF_NUM_TBL_SCOPE];

static void
tf_em_dmabuf_mem_unmap(struct hcapi_cfa_em_table *tbl)
{
	struct hcapi_cfa_em_page_tbl *tp;
	int level;
	uint32_t page_no, pg_count;

	for (level = (tbl->num_lvl - 1); level < tbl->num_lvl; level++) {
		tp = &tbl->pg_tbl[level];

		pg_count = tbl->page_cnt[level];
		for (page_no = 0; page_no < pg_count; page_no++) {
			if (tp->pg_va_tbl != NULL &&
			    tp->pg_va_tbl[page_no] != NULL &&
			    tp->pg_size != 0) {
				(void)munmap(tp->pg_va_tbl[page_no],
					     tp->pg_size);
			}
		}

		tfp_free((void *)tp->pg_va_tbl);
		tfp_free((void *)tp->pg_pa_tbl);
	}
}

/**
 * Unregisters EM Ctx in Firmware
 *
 * [in] tfp
 *   Pointer to a TruFlow handle
 *
 * [in] tbl_scope_cb
 *   Pointer to a table scope control block
 *
 * [in] dir
 *   Receive or transmit direction
 */
static void
tf_em_ctx_unreg(struct tf_tbl_scope_cb *tbl_scope_cb,
		int dir)
{
	struct hcapi_cfa_em_ctx_mem_info *ctxp =
		&tbl_scope_cb->em_ctx_info[dir];
	struct hcapi_cfa_em_table *tbl;
	int i;

	for (i = TF_KEY0_TABLE; i < TF_MAX_TABLE; i++) {
		tbl = &ctxp->em_tables[i];
			tf_em_dmabuf_mem_unmap(tbl);
	}
}

static int tf_export_tbl_scope(int lfc_fd,
			       int *fd,
			       int bus,
			       int devfn)
{
	struct tf_em_bnxt_lfc_req tf_lfc_req;
	struct tf_em_bnxt_lfc_dmabuf_fd *dma_fd;
	struct tfp_calloc_parms  mparms;
	int rc;

	memset(&tf_lfc_req, 0, sizeof(struct tf_em_bnxt_lfc_req));
	tf_lfc_req.hdr.ver = 1;
	tf_lfc_req.hdr.bus = bus;
	tf_lfc_req.hdr.devfn = devfn;
	tf_lfc_req.hdr.req_type = TF_EM_BNXT_LFC_CFA_EEM_DMABUF_EXPORT_REQ;
	tf_lfc_req.req.eem_dmabuf_export_req.flags = O_ACCMODE;
	tf_lfc_req.req.eem_dmabuf_export_req.std.version = 1;

	mparms.nitems = 1;
	mparms.size = sizeof(struct tf_em_bnxt_lfc_dmabuf_fd);
	mparms.alignment = 0;
	tfp_calloc(&mparms);
	dma_fd = (struct tf_em_bnxt_lfc_dmabuf_fd *)mparms.mem_va;
	tf_lfc_req.req.eem_dmabuf_export_req.dma_fd = dma_fd;

	rc = ioctl(lfc_fd, BNXT_LFC_REQ, &tf_lfc_req);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "EXT EEM export chanel_fd %d, rc=%d\n",
			    lfc_fd,
			    rc);
		tfp_free(dma_fd);
		return rc;
	}

	memcpy(fd, dma_fd->fd, sizeof(dma_fd->fd));
	tfp_free(dma_fd);

	return rc;
}

static int
tf_em_dmabuf_mem_map(struct hcapi_cfa_em_table *tbl,
		     int dmabuf_fd)
{
	struct hcapi_cfa_em_page_tbl *tp;
	int level;
	uint32_t page_no;
	uint32_t pg_count;
	uint32_t offset;
	struct tfp_calloc_parms parms;

	for (level = (tbl->num_lvl - 1); level < tbl->num_lvl; level++) {
		tp = &tbl->pg_tbl[level];

		pg_count = tbl->page_cnt[level];
		offset = 0;

		parms.nitems = pg_count;
		parms.size = sizeof(void *);
		parms.alignment = 0;

		if ((tfp_calloc(&parms)) != 0)
			return -ENOMEM;

		tp->pg_va_tbl = parms.mem_va;
		parms.nitems = pg_count;
		parms.size = sizeof(void *);
		parms.alignment = 0;

		if ((tfp_calloc(&parms)) != 0) {
			tfp_free((void *)tp->pg_va_tbl);
			return -ENOMEM;
		}

		tp->pg_pa_tbl = parms.mem_va;
		tp->pg_count = 0;
		tp->pg_size =  TF_EM_PAGE_SIZE;

		for (page_no = 0; page_no < pg_count; page_no++) {
			tp->pg_va_tbl[page_no] = mmap(NULL,
						      TF_EM_PAGE_SIZE,
						      PROT_READ | PROT_WRITE,
						      MAP_SHARED,
						      dmabuf_fd,
						      offset);
			if (tp->pg_va_tbl[page_no] == (void *)-1) {
				TFP_DRV_LOG(ERR,
		"MMap memory error. level:%d page:%d pg_count:%d - %s\n",
					    level,
				     page_no,
					    pg_count,
					    strerror(errno));
				return -ENOMEM;
			}
			offset += tp->pg_size;
			tp->pg_count++;
		}
	}

	return 0;
}

static int tf_mmap_tbl_scope(struct tf_tbl_scope_cb *tbl_scope_cb,
			     enum tf_dir dir,
			     int tbl_type,
			     int dmabuf_fd)
{
	struct hcapi_cfa_em_table *tbl;

	if (tbl_type == TF_EFC_TABLE)
		return 0;

	tbl = &tbl_scope_cb->em_ctx_info[dir].em_tables[tbl_type];
	return tf_em_dmabuf_mem_map(tbl, dmabuf_fd);
}

#define TF_LFC_DEVICE "/dev/bnxt_lfc"

static int
tf_prepare_dmabuf_bnxt_lfc_device(struct tf_tbl_scope_cb *tbl_scope_cb)
{
	int lfc_fd;

	lfc_fd = open(TF_LFC_DEVICE, O_RDWR);
	if (!lfc_fd) {
		TFP_DRV_LOG(ERR,
			    "EEM: open %s device error\n",
			    TF_LFC_DEVICE);
		return -ENOENT;
	}

	tbl_scope_cb->lfc_fd = lfc_fd;

	return 0;
}

static int
offload_system_mmap(struct tf_tbl_scope_cb *tbl_scope_cb)
{
	int rc;
	int dmabuf_fd;
	enum tf_dir dir;
	enum hcapi_cfa_em_table_type tbl_type;

	rc = tf_prepare_dmabuf_bnxt_lfc_device(tbl_scope_cb);
	if (rc) {
		TFP_DRV_LOG(ERR, "EEM: Prepare bnxt_lfc channel failed\n");
		return rc;
	}

	rc = tf_export_tbl_scope(tbl_scope_cb->lfc_fd,
				 (int *)tbl_scope_cb->fd,
				 tbl_scope_cb->bus,
				 tbl_scope_cb->devfn);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "export dmabuf fd failed\n");
		return rc;
	}

	tbl_scope_cb->valid = true;

	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		for (tbl_type = TF_KEY0_TABLE; tbl_type <
			     TF_MAX_TABLE; tbl_type++) {
			if (tbl_type == TF_EFC_TABLE)
				continue;

			dmabuf_fd = tbl_scope_cb->fd[(dir ? 0 : 1)][tbl_type];
			rc = tf_mmap_tbl_scope(tbl_scope_cb,
					       dir,
					       tbl_type,
					       dmabuf_fd);
			if (rc) {
				TFP_DRV_LOG(ERR,
					    "dir:%d tbl:%d mmap failed rc %d\n",
					    dir,
					    tbl_type,
					    rc);
				break;
			}
		}
	}
	return 0;
}

static int
tf_destroy_dmabuf_bnxt_lfc_device(struct tf_tbl_scope_cb *tbl_scope_cb)
{
	close(tbl_scope_cb->lfc_fd);

	return 0;
}

static int
tf_dmabuf_alloc(struct tf *tfp, struct tf_tbl_scope_cb *tbl_scope_cb)
{
	int rc;

	rc = tfp_msg_hwrm_oem_cmd(tfp,
		tbl_scope_cb->em_ctx_info[TF_DIR_RX].em_tables[TF_KEY0_TABLE].num_entries);
	if (rc)
		PMD_DRV_LOG(ERR, "EEM: Failed to prepare system memory rc:%d\n",
			    rc);

	return 0;
}

static int
tf_dmabuf_free(struct tf *tfp, struct tf_tbl_scope_cb *tbl_scope_cb)
{
	int rc;

	rc = tfp_msg_hwrm_oem_cmd(tfp, 0);
	if (rc)
		TFP_DRV_LOG(ERR, "EEM: Failed to cleanup system memory\n");

	tf_destroy_dmabuf_bnxt_lfc_device(tbl_scope_cb);

	return 0;
}

int
tf_em_ext_alloc(struct tf *tfp,
		struct tf_alloc_tbl_scope_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_tbl_scope_cb *tbl_scope_cb;
	struct tf_rm_allocate_parms aparms = { 0 };
	struct tf_free_tbl_scope_parms free_parms;
	struct tf_rm_free_parms fparms = { 0 };
	int dir;
	int i;
	struct hcapi_cfa_em_table *em_tables;

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	aparms.rm_db = eem_db[TF_DIR_RX];
	aparms.db_index = TF_EM_TBL_TYPE_TBL_SCOPE;
	aparms.index = (uint32_t *)&parms->tbl_scope_id;
	rc = tf_rm_allocate(&aparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to allocate table scope\n");
		return rc;
	}

	tbl_scope_cb = &tbl_scopes[parms->tbl_scope_id];
	tbl_scope_cb->index = parms->tbl_scope_id;
	tbl_scope_cb->tbl_scope_id = parms->tbl_scope_id;
	tbl_scope_cb->bus = tfs->session_id.internal.bus;
	tbl_scope_cb->devfn = tfs->session_id.internal.device;

	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		rc = tf_msg_em_qcaps(tfp,
				     dir,
				     &tbl_scope_cb->em_caps[dir]);
		if (rc) {
			TFP_DRV_LOG(ERR,
				    "EEM: Unable to query for EEM capability,"
				    " rc:%s\n",
				    strerror(-rc));
			goto cleanup;
		}
	}

	/*
	 * Validate and setup table sizes
	 */
	if (tf_em_validate_num_entries(tbl_scope_cb, parms))
		goto cleanup;

	rc = tf_dmabuf_alloc(tfp, tbl_scope_cb);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "System DMA buff alloc failed\n");
		return -EIO;
	}

	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		for (i = TF_KEY0_TABLE; i < TF_MAX_TABLE; i++) {
			if (i == TF_EFC_TABLE)
				continue;

			em_tables =
				&tbl_scope_cb->em_ctx_info[dir].em_tables[i];

			rc = tf_em_size_table(em_tables, TF_EM_PAGE_SIZE);
			if (rc) {
				TFP_DRV_LOG(ERR, "Size table failed\n");
				goto cleanup;
			}
		}

		em_tables = tbl_scope_cb->em_ctx_info[dir].em_tables;
		rc = tf_create_tbl_pool_external(dir,
					tbl_scope_cb,
					em_tables[TF_RECORD_TABLE].num_entries,
					em_tables[TF_RECORD_TABLE].entry_size);

		if (rc) {
			TFP_DRV_LOG(ERR,
				    "%s TBL: Unable to allocate idx pools %s\n",
				    tf_dir_2_str(dir),
				    strerror(-rc));
			goto cleanup_full;
		}
	}

	rc = offload_system_mmap(tbl_scope_cb);

	if (rc) {
		TFP_DRV_LOG(ERR,
			    "System alloc mmap failed\n");
		goto cleanup_full;
	}

	return rc;

cleanup_full:
	free_parms.tbl_scope_id = parms->tbl_scope_id;
	tf_em_ext_free(tfp, &free_parms);
	return -EINVAL;

cleanup:
	/* Free Table control block */
	fparms.rm_db = eem_db[TF_DIR_RX];
	fparms.db_index = TF_EM_TBL_TYPE_TBL_SCOPE;
	fparms.index = parms->tbl_scope_id;
	tf_rm_free(&fparms);
	return -EINVAL;
}

int
tf_em_ext_free(struct tf *tfp,
	       struct tf_free_tbl_scope_parms *parms)
{
	int rc;
	struct tf_session *tfs;
	struct tf_tbl_scope_cb *tbl_scope_cb;
	int dir;
	struct tf_rm_free_parms aparms = { 0 };

	TF_CHECK_PARMS2(tfp, parms);

	/* Retrieve the session information */
	rc = tf_session_get_session(tfp, &tfs);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to lookup session, rc:%s\n",
			    strerror(-rc));
		return rc;
	}

	tbl_scope_cb = &tbl_scopes[parms->tbl_scope_id];

		/* Free Table control block */
	aparms.rm_db = eem_db[TF_DIR_RX];
	aparms.db_index = TF_EM_TBL_TYPE_TBL_SCOPE;
	aparms.index = parms->tbl_scope_id;
	rc = tf_rm_free(&aparms);
	if (rc) {
		TFP_DRV_LOG(ERR,
			    "Failed to free table scope\n");
	}

	for (dir = 0; dir < TF_DIR_MAX; dir++) {
		/* Free associated external pools
		 */
		tf_destroy_tbl_pool_external(dir,
					     tbl_scope_cb);

		/* Unmap memory */
		tf_em_ctx_unreg(tbl_scope_cb, dir);

		tf_msg_em_op(tfp,
			     dir,
			     HWRM_TF_EXT_EM_OP_INPUT_OP_EXT_EM_DISABLE);
	}

	tf_dmabuf_free(tfp, tbl_scope_cb);

	return rc;
}
