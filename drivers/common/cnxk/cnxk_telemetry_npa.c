/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <ctype.h>
#include "cnxk_telemetry.h"
#include "roc_api.h"
#include "roc_priv.h"

static int
cnxk_tel_npa(struct plt_tel_data *d)
{
	struct npa_lf *lf;
	int aura_cnt = 0;
	uint32_t i;

	lf = idev_npa_obj_get();
	if (lf == NULL)
		return NPA_ERR_DEVICE_NOT_BOUNDED;

	for (i = 0; i < lf->nr_pools; i++) {
		if (plt_bitmap_get(lf->npa_bmp, i))
			continue;
		aura_cnt++;
	}

	plt_tel_data_add_dict_ptr(d, "npa", lf);
	plt_tel_data_add_dict_int(d, "pf", dev_get_pf(lf->pf_func));
	plt_tel_data_add_dict_int(d, "vf", dev_get_vf(lf->pf_func));
	plt_tel_data_add_dict_int(d, "aura_cnt", aura_cnt);

	CNXK_TEL_DICT_STR(d, lf->pci_dev, name, pcidev_);
	CNXK_TEL_DICT_PTR(d, lf, npa_bmp);
	CNXK_TEL_DICT_PTR(d, lf, npa_bmp_mem);
	CNXK_TEL_DICT_PTR(d, lf, npa_qint_mem);
	CNXK_TEL_DICT_PTR(d, lf, mbox);
	CNXK_TEL_DICT_PTR(d, lf, base);
	CNXK_TEL_DICT_INT(d, lf, stack_pg_ptrs);
	CNXK_TEL_DICT_INT(d, lf, stack_pg_bytes);
	CNXK_TEL_DICT_INT(d, lf, npa_msixoff);
	CNXK_TEL_DICT_INT(d, lf, nr_pools);
	CNXK_TEL_DICT_INT(d, lf, pf_func);
	CNXK_TEL_DICT_INT(d, lf, aura_sz);
	CNXK_TEL_DICT_INT(d, lf, qints);

	return 0;
}

static int
cnxk_tel_npa_aura(int aura_id, struct plt_tel_data *d)
{
	__io struct npa_aura_s *aura;
	struct npa_aq_enq_req *req;
	struct npa_aq_enq_rsp *rsp;
	struct npa_lf *lf;
	int rc;

	lf = idev_npa_obj_get();
	if (lf == NULL)
		return NPA_ERR_DEVICE_NOT_BOUNDED;

	if (plt_bitmap_get(lf->npa_bmp, aura_id))
		return -1;

	req = mbox_alloc_msg_npa_aq_enq(mbox_get(lf->mbox));
	if (!req) {
		plt_err("Failed to alloc aq enq for npa");
		rc = -1;
		goto exit;
	}

	req->aura_id = aura_id;
	req->ctype = NPA_AQ_CTYPE_AURA;
	req->op = NPA_AQ_INSTOP_READ;

	rc = mbox_process_msg(lf->mbox, (void *)&rsp);
	if (rc) {
		plt_err("Failed to get pool(%d) context", aura_id);
		goto exit;
	}

	aura = &rsp->aura;
	CNXK_TEL_DICT_PTR(d, aura, pool_addr, w0_);
	CNXK_TEL_DICT_INT(d, aura, ena, w1_);
	CNXK_TEL_DICT_INT(d, aura, pool_caching, w1_);
	CNXK_TEL_DICT_INT(d, aura, pool_way_mask, w1_);
	CNXK_TEL_DICT_INT(d, aura, avg_con, w1_);
	CNXK_TEL_DICT_INT(d, aura, pool_drop_ena, w1_);
	CNXK_TEL_DICT_INT(d, aura, aura_drop_ena, w1_);
	CNXK_TEL_DICT_INT(d, aura, bp_ena, w1_);
	CNXK_TEL_DICT_INT(d, aura, aura_drop, w1_);
	CNXK_TEL_DICT_INT(d, aura, avg_level, w1_);
	CNXK_TEL_DICT_U64(d, aura, count, w2_);
	CNXK_TEL_DICT_INT(d, aura, nix0_bpid, w2_);
	CNXK_TEL_DICT_INT(d, aura, nix1_bpid, w2_);
	CNXK_TEL_DICT_U64(d, aura, limit, w3_);
	CNXK_TEL_DICT_INT(d, aura, bp, w3_);
	CNXK_TEL_DICT_INT(d, aura, fc_ena, w3_);
	CNXK_TEL_DICT_INT(d, aura, fc_up_crossing, w3_);
	CNXK_TEL_DICT_INT(d, aura, fc_stype, w3_);
	CNXK_TEL_DICT_INT(d, aura, fc_hyst_bits, w3_);
	CNXK_TEL_DICT_INT(d, aura, fc_addr, w4_);
	CNXK_TEL_DICT_INT(d, aura, pool_drop, w5_);
	CNXK_TEL_DICT_INT(d, aura, update_time, w5_);
	CNXK_TEL_DICT_INT(d, aura, err_int, w5_);
	CNXK_TEL_DICT_INT(d, aura, err_int_ena, w5_);
	CNXK_TEL_DICT_INT(d, aura, thresh_int, w5_);
	CNXK_TEL_DICT_INT(d, aura, thresh_int_ena, w5_);
	CNXK_TEL_DICT_INT(d, aura, thresh_up, w5_);
	CNXK_TEL_DICT_INT(d, aura, thresh_qint_idx, w5_);
	CNXK_TEL_DICT_INT(d, aura, err_qint_idx, w5_);
	CNXK_TEL_DICT_U64(d, aura, thresh, w6_);

	rc = 0;
exit:
	mbox_put(lf->mbox);
	return rc;
}

static int
cnxk_tel_npa_pool(int pool_id, struct plt_tel_data *d)
{
	__io struct npa_pool_s *pool;
	struct npa_aq_enq_req *req;
	struct npa_aq_enq_rsp *rsp;
	struct npa_lf *lf;
	int rc;

	lf = idev_npa_obj_get();
	if (lf == NULL)
		return NPA_ERR_DEVICE_NOT_BOUNDED;

	if (plt_bitmap_get(lf->npa_bmp, pool_id))
		return -1;

	req = mbox_alloc_msg_npa_aq_enq(mbox_get(lf->mbox));
	if (!req) {
		plt_err("Failed to alloc aq enq for npa");
		rc = -1;
		goto exit;
	}

	req->aura_id = pool_id;
	req->ctype = NPA_AQ_CTYPE_POOL;
	req->op = NPA_AQ_INSTOP_READ;

	rc = mbox_process_msg(lf->mbox, (void *)&rsp);
	if (rc) {
		plt_err("Failed to get pool(%d) context", pool_id);
		goto exit;
	}

	pool = &rsp->pool;
	CNXK_TEL_DICT_PTR(d, pool, stack_base, w0_);
	CNXK_TEL_DICT_INT(d, pool, ena, w1_);
	CNXK_TEL_DICT_INT(d, pool, nat_align, w1_);
	CNXK_TEL_DICT_INT(d, pool, stack_caching, w1_);
	CNXK_TEL_DICT_INT(d, pool, stack_way_mask, w1_);
	CNXK_TEL_DICT_INT(d, pool, buf_offset, w1_);
	CNXK_TEL_DICT_INT(d, pool, buf_size, w1_);
	CNXK_TEL_DICT_INT(d, pool, stack_max_pages, w2_);
	CNXK_TEL_DICT_INT(d, pool, stack_pages, w2_);
	CNXK_TEL_DICT_INT(d, pool, op_pc, w3_);
	CNXK_TEL_DICT_INT(d, pool, stack_offset, w4_);
	CNXK_TEL_DICT_INT(d, pool, shift, w4_);
	CNXK_TEL_DICT_INT(d, pool, avg_level, w4_);
	CNXK_TEL_DICT_INT(d, pool, avg_con, w4_);
	CNXK_TEL_DICT_INT(d, pool, fc_ena, w4_);
	CNXK_TEL_DICT_INT(d, pool, fc_stype, w4_);
	CNXK_TEL_DICT_INT(d, pool, fc_hyst_bits, w4_);
	CNXK_TEL_DICT_INT(d, pool, fc_up_crossing, w4_);
	CNXK_TEL_DICT_INT(d, pool, update_time, w4_);
	CNXK_TEL_DICT_PTR(d, pool, fc_addr, w5_);
	CNXK_TEL_DICT_PTR(d, pool, ptr_start, w6_);
	CNXK_TEL_DICT_PTR(d, pool, ptr_end, w7_);
	CNXK_TEL_DICT_INT(d, pool, err_int, w8_);
	CNXK_TEL_DICT_INT(d, pool, err_int_ena, w8_);
	CNXK_TEL_DICT_INT(d, pool, thresh_int, w8_);
	CNXK_TEL_DICT_INT(d, pool, thresh_int_ena, w8_);
	CNXK_TEL_DICT_INT(d, pool, thresh_up, w8_);
	CNXK_TEL_DICT_INT(d, pool, thresh_qint_idx, w8_);
	CNXK_TEL_DICT_INT(d, pool, err_qint_idx, w8_);

	rc = 0;
exit:
	mbox_put(lf->mbox);
	return rc;
}

static int
cnxk_npa_tel_handle_info(const char *cmd __plt_unused,
			 const char *params __plt_unused,
			 struct plt_tel_data *d)
{
	plt_tel_data_start_dict(d);
	return cnxk_tel_npa(d);
}

static int
cnxk_npa_tel_handle_aura_list(const char *cmd __plt_unused,
			      const char *params __plt_unused,
			      struct plt_tel_data *d)
{
	struct npa_lf *lf;
	int i;

	lf = idev_npa_obj_get();
	if (lf == NULL)
		return NPA_ERR_DEVICE_NOT_BOUNDED;

	plt_tel_data_start_array(d, PLT_TEL_INT_VAL);

	for (i = 0; i < (int)lf->nr_pools; i++)
		if (!plt_bitmap_get(lf->npa_bmp, i))
			rte_tel_data_add_array_int(d, i);

	return 0;
}

static int
cnxk_npa_tel_handle_pool_list(const char *cmd, const char *params,
			      struct plt_tel_data *d)
{
	/* In current implementation, aura and pool ID mapped 1:1 */
	return cnxk_npa_tel_handle_aura_list(cmd, params, d);
}

static int
cnxk_npa_tel_handle_info_x(const char *cmd, const char *params,
			   struct plt_tel_data *d)
{
	int id, rc;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	id = strtol(params, NULL, 10);
	plt_tel_data_start_dict(d);

	if (strstr(cmd, "aura/info"))
		rc = cnxk_tel_npa_aura(id, d);
	else
		rc = cnxk_tel_npa_pool(id, d);

	return rc;
}

PLT_INIT(cnxk_telemetry_npa_init)
{
	plt_telemetry_register_cmd(
		"/cnxk/npa/info", cnxk_npa_tel_handle_info,
		"Returns npa information. Takes no parameters");

	plt_telemetry_register_cmd(
		"/cnxk/npa/aura/list", cnxk_npa_tel_handle_aura_list,
		"Returns list of npa aura id. Takes no parameters");

	plt_telemetry_register_cmd(
		"/cnxk/npa/aura/info", cnxk_npa_tel_handle_info_x,
		"Returns npa aura information. Parameters: aura_id");

	plt_telemetry_register_cmd(
		"/cnxk/npa/pool/list", cnxk_npa_tel_handle_pool_list,
		"Returns list of npa pool id. Takes no parameters");

	plt_telemetry_register_cmd(
		"/cnxk/npa/pool/info", cnxk_npa_tel_handle_info_x,
		"Returns npa pool information. Parameters: pool_id");
}
