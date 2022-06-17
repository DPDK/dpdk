/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include "ifpga_feature_dev.h"

static int fme_err_get_errors(struct ifpga_fme_hw *fme, u64 *val)
{
	struct feature_fme_err *fme_err
		= get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_GLOBAL_ERR);
	struct feature_fme_error0 fme_error0;

	fme_error0.csr = readq(&fme_err->fme_err);
	*val = fme_error0.csr;

	return 0;
}

static int fme_err_get_first_error(struct ifpga_fme_hw *fme, u64 *val)
{
	struct feature_fme_err *fme_err
		= get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_GLOBAL_ERR);
	struct feature_fme_first_error fme_first_err;

	fme_first_err.csr = readq(&fme_err->fme_first_err);
	*val = fme_first_err.err_reg_status;

	return 0;
}

static int fme_err_get_next_error(struct ifpga_fme_hw *fme, u64 *val)
{
	struct feature_fme_err *fme_err
		= get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_GLOBAL_ERR);
	struct feature_fme_next_error fme_next_err;

	fme_next_err.csr = readq(&fme_err->fme_next_err);
	*val = fme_next_err.err_reg_status;

	return 0;
}

static int fme_err_set_clear(struct ifpga_fme_hw *fme, u64 val)
{
	struct feature_fme_err *fme_err
		= get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_GLOBAL_ERR);

	spinlock_lock(&fme->lock);

	writeq(val, &fme_err->fme_err);

	spinlock_unlock(&fme->lock);

	return 0;
}

static int fme_err_get_revision(struct ifpga_fme_hw *fme, u64 *val)
{
	struct feature_fme_err *fme_err
		= get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_GLOBAL_ERR);
	struct feature_header header;

	header.csr = readq(&fme_err->header);
	*val = header.revision;

	return 0;
}

static int fme_err_get_pcie0_errors(struct ifpga_fme_hw *fme, u64 *val)
{
	struct feature_fme_err *fme_err
		= get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_GLOBAL_ERR);
	struct feature_fme_pcie0_error pcie0_err;

	pcie0_err.csr = readq(&fme_err->pcie0_err);
	*val = pcie0_err.csr;

	return 0;
}

static int fme_err_set_pcie0_errors(struct ifpga_fme_hw *fme, u64 val)
{
	struct feature_fme_err *fme_err
		= get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_GLOBAL_ERR);
	struct feature_fme_pcie0_error pcie0_err;
	int ret = 0;

	spinlock_lock(&fme->lock);
	writeq(FME_PCIE0_ERROR_MASK, &fme_err->pcie0_err_mask);

	pcie0_err.csr = readq(&fme_err->pcie0_err);
	if (val != pcie0_err.csr)
		ret = -EBUSY;
	else
		writeq(pcie0_err.csr & FME_PCIE0_ERROR_MASK,
		       &fme_err->pcie0_err);

	writeq(0UL, &fme_err->pcie0_err_mask);
	spinlock_unlock(&fme->lock);

	return ret;
}

static int fme_err_get_pcie1_errors(struct ifpga_fme_hw *fme, u64 *val)
{
	struct feature_fme_err *fme_err
		= get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_GLOBAL_ERR);
	struct feature_fme_pcie1_error pcie1_err;

	pcie1_err.csr = readq(&fme_err->pcie1_err);
	*val = pcie1_err.csr;

	return 0;
}

static int fme_err_set_pcie1_errors(struct ifpga_fme_hw *fme, u64 val)
{
	struct feature_fme_err *fme_err
		= get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_GLOBAL_ERR);
	struct feature_fme_pcie1_error pcie1_err;
	int ret = 0;

	spinlock_lock(&fme->lock);
	writeq(FME_PCIE1_ERROR_MASK, &fme_err->pcie1_err_mask);

	pcie1_err.csr = readq(&fme_err->pcie1_err);
	if (val != pcie1_err.csr)
		ret = -EBUSY;
	else
		writeq(pcie1_err.csr & FME_PCIE1_ERROR_MASK,
		       &fme_err->pcie1_err);

	writeq(0UL, &fme_err->pcie1_err_mask);
	spinlock_unlock(&fme->lock);

	return ret;
}

static int fme_err_get_nonfatal_errors(struct ifpga_fme_hw *fme, u64 *val)
{
	struct feature_fme_err *fme_err
		= get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_GLOBAL_ERR);
	struct feature_fme_ras_nonfaterror ras_nonfaterr;

	ras_nonfaterr.csr = readq(&fme_err->ras_nonfaterr);
	*val = ras_nonfaterr.csr;

	return 0;
}

static int fme_err_get_catfatal_errors(struct ifpga_fme_hw *fme, u64 *val)
{
	struct feature_fme_err *fme_err
		= get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_GLOBAL_ERR);
	struct feature_fme_ras_catfaterror ras_catfaterr;

	ras_catfaterr.csr = readq(&fme_err->ras_catfaterr);
	*val = ras_catfaterr.csr;

	return 0;
}

static int fme_err_get_inject_errors(struct ifpga_fme_hw *fme, u64 *val)
{
	struct feature_fme_err *fme_err
		= get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_GLOBAL_ERR);
	struct feature_fme_ras_error_inj ras_error_inj;

	ras_error_inj.csr = readq(&fme_err->ras_error_inj);
	*val = ras_error_inj.csr & FME_RAS_ERROR_INJ_MASK;

	return 0;
}

static int fme_err_set_inject_errors(struct ifpga_fme_hw *fme, u64 val)
{
	struct feature_fme_err *fme_err
		= get_fme_feature_ioaddr_by_index(fme,
					      FME_FEATURE_ID_GLOBAL_ERR);
	struct feature_fme_ras_error_inj ras_error_inj;

	spinlock_lock(&fme->lock);
	ras_error_inj.csr = readq(&fme_err->ras_error_inj);

	if (val <= FME_RAS_ERROR_INJ_MASK) {
		ras_error_inj.csr = val;
	} else {
		spinlock_unlock(&fme->lock);
		return -EINVAL;
	}

	writeq(ras_error_inj.csr, &fme_err->ras_error_inj);
	spinlock_unlock(&fme->lock);

	return 0;
}

static void fme_error_enable(struct ifpga_fme_hw *fme)
{
	struct feature_fme_err *fme_err
		= get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_GLOBAL_ERR);

	writeq(FME_ERROR0_MASK_DEFAULT, &fme_err->fme_err_mask);
	writeq(0UL, &fme_err->pcie0_err_mask);
	writeq(0UL, &fme_err->pcie1_err_mask);
	writeq(0UL, &fme_err->ras_nonfat_mask);
	writeq(0UL, &fme_err->ras_catfat_mask);
}

static int fme_global_error_init(struct ifpga_feature *feature)
{
	struct ifpga_fme_hw *fme = feature->parent;

	dev_info(NULL, "FME error_module Init.\n");

	fme_error_enable(fme);

	if (feature->ctx_num)
		fme->capability |= FPGA_FME_CAP_ERR_IRQ;

	return 0;
}

static void fme_global_error_uinit(struct ifpga_feature *feature)
{
	UNUSED(feature);
}

static int fme_err_check_seu(struct feature_fme_err *fme_err)
{
	struct feature_fme_error_capability error_cap;

	error_cap.csr = readq(&fme_err->fme_err_capability);

	return error_cap.seu_support ? 1 : 0;
}

static int fme_err_get_seu_emr(struct ifpga_fme_hw *fme,
		u64 *val, bool high)
{
	struct feature_fme_err *fme_err
		= get_fme_feature_ioaddr_by_index(fme,
				FME_FEATURE_ID_GLOBAL_ERR);

	if (!fme_err_check_seu(fme_err))
		return -ENODEV;

	if (high)
		*val = readq(&fme_err->seu_emr_h);
	else
		*val = readq(&fme_err->seu_emr_l);

	return 0;
}

static int fme_err_fme_err_get_prop(struct ifpga_feature *feature,
				    struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;
	u16 id = GET_FIELD(PROP_ID, prop->prop_id);

	switch (id) {
	case 0x1: /* ERRORS */
		return fme_err_get_errors(fme, &prop->data);
	case 0x2: /* FIRST_ERROR */
		return fme_err_get_first_error(fme, &prop->data);
	case 0x3: /* NEXT_ERROR */
		return fme_err_get_next_error(fme, &prop->data);
	case 0x5: /* SEU EMR LOW */
		return fme_err_get_seu_emr(fme, &prop->data, 0);
	case 0x6: /* SEU EMR HIGH */
		return fme_err_get_seu_emr(fme, &prop->data, 1);
	}

	return -ENOENT;
}

static int fme_err_root_get_prop(struct ifpga_feature *feature,
				 struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;
	u16 id = GET_FIELD(PROP_ID, prop->prop_id);

	switch (id) {
	case 0x5: /* REVISION */
		return fme_err_get_revision(fme, &prop->data);
	case 0x6: /* PCIE0_ERRORS */
		return fme_err_get_pcie0_errors(fme, &prop->data);
	case 0x7: /* PCIE1_ERRORS */
		return fme_err_get_pcie1_errors(fme, &prop->data);
	case 0x8: /* NONFATAL_ERRORS */
		return fme_err_get_nonfatal_errors(fme, &prop->data);
	case 0x9: /* CATFATAL_ERRORS */
		return fme_err_get_catfatal_errors(fme, &prop->data);
	case 0xa: /* INJECT_ERRORS */
		return fme_err_get_inject_errors(fme, &prop->data);
	case 0xb: /* REVISION*/
		return fme_err_get_revision(fme, &prop->data);
	}

	return -ENOENT;
}

static int fme_global_error_get_prop(struct ifpga_feature *feature,
				     struct feature_prop *prop)
{
	u8 top = GET_FIELD(PROP_TOP, prop->prop_id);
	u8 sub = GET_FIELD(PROP_SUB, prop->prop_id);

	/* PROP_SUB is never used */
	if (sub != PROP_SUB_UNUSED)
		return -ENOENT;

	switch (top) {
	case ERR_PROP_TOP_FME_ERR:
		return fme_err_fme_err_get_prop(feature, prop);
	case ERR_PROP_TOP_UNUSED:
		return fme_err_root_get_prop(feature, prop);
	}

	return -ENOENT;
}

static int fme_err_fme_err_set_prop(struct ifpga_feature *feature,
				    struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;
	u16 id = GET_FIELD(PROP_ID, prop->prop_id);

	switch (id) {
	case 0x4: /* CLEAR */
		return fme_err_set_clear(fme, prop->data);
	}

	return -ENOENT;
}

static int fme_err_root_set_prop(struct ifpga_feature *feature,
				 struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;
	u16 id = GET_FIELD(PROP_ID, prop->prop_id);

	switch (id) {
	case 0x6: /* PCIE0_ERRORS */
		return fme_err_set_pcie0_errors(fme, prop->data);
	case 0x7: /* PCIE1_ERRORS */
		return fme_err_set_pcie1_errors(fme, prop->data);
	case 0xa: /* INJECT_ERRORS */
		return fme_err_set_inject_errors(fme, prop->data);
	}

	return -ENOENT;
}

static int fme_global_error_set_prop(struct ifpga_feature *feature,
				     struct feature_prop *prop)
{
	u8 top = GET_FIELD(PROP_TOP, prop->prop_id);
	u8 sub = GET_FIELD(PROP_SUB, prop->prop_id);

	/* PROP_SUB is never used */
	if (sub != PROP_SUB_UNUSED)
		return -ENOENT;

	switch (top) {
	case ERR_PROP_TOP_FME_ERR:
		return fme_err_fme_err_set_prop(feature, prop);
	case ERR_PROP_TOP_UNUSED:
		return fme_err_root_set_prop(feature, prop);
	}

	return -ENOENT;
}

static int fme_global_err_set_irq(struct ifpga_feature *feature, void *irq_set)
{
	struct fpga_fme_err_irq_set *err_irq_set = irq_set;
	struct ifpga_fme_hw *fme;
	int ret;

	fme = (struct ifpga_fme_hw *)feature->parent;

	if (!(fme->capability & FPGA_FME_CAP_ERR_IRQ))
		return -ENODEV;

	spinlock_lock(&fme->lock);
	ret = fpga_msix_set_block(feature, 0, 1, &err_irq_set->evtfd);
	spinlock_unlock(&fme->lock);

	return ret;
}

struct ifpga_feature_ops fme_global_err_ops = {
	.init = fme_global_error_init,
	.uinit = fme_global_error_uinit,
	.get_prop = fme_global_error_get_prop,
	.set_prop = fme_global_error_set_prop,
	.set_irq = fme_global_err_set_irq,
};
