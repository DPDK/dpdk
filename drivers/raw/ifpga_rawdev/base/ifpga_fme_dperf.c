/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include "ifpga_feature_dev.h"

#define PERF_OBJ_ROOT_ID	0xff

static int fme_dperf_get_clock(struct ifpga_fme_hw *fme, u64 *clock)
{
	struct feature_fme_dperf *dperf;
	struct feature_fme_dfpmon_clk_ctr clk;

	dperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_DPERF);
	clk.afu_interf_clock = readq(&dperf->clk);

	*clock = clk.afu_interf_clock;
	return 0;
}

static int fme_dperf_get_revision(struct ifpga_fme_hw *fme, u64 *revision)
{
	struct feature_fme_dperf *dperf;
	struct feature_header header;

	dperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_DPERF);
	header.csr = readq(&dperf->header);
	*revision = header.revision;

	return 0;
}

#define DPERF_TIMEOUT	30

static bool fabric_pobj_is_enabled(int port_id,
				   struct feature_fme_dperf *dperf)
{
	struct feature_fme_dfpmon_fab_ctl ctl;

	ctl.csr = readq(&dperf->fab_ctl);

	if (ctl.port_filter == FAB_DISABLE_FILTER)
		return port_id == PERF_OBJ_ROOT_ID;

	return port_id == ctl.port_id;
}

static u64 read_fabric_counter(struct ifpga_fme_hw *fme, u8 port_id,
			       enum dperf_fab_events fab_event)
{
	struct feature_fme_dfpmon_fab_ctl ctl;
	struct feature_fme_dfpmon_fab_ctr ctr;
	struct feature_fme_dperf *dperf;
	u64 counter = 0;

	spinlock_lock(&fme->lock);
	dperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_DPERF);

	/* if it is disabled, force the counter to return zero. */
	if (!fabric_pobj_is_enabled(port_id, dperf))
		goto exit;

	ctl.csr = readq(&dperf->fab_ctl);
	ctl.fab_evtcode = fab_event;
	writeq(ctl.csr, &dperf->fab_ctl);

	ctr.event_code = fab_event;

	if (fpga_wait_register_field(event_code, ctr,
				     &dperf->fab_ctr, DPERF_TIMEOUT, 1)) {
		dev_err(fme, "timeout, unmatched VTd event type in counter registers.\n");
		spinlock_unlock(&fme->lock);
		return -ETIMEDOUT;
	}

	ctr.csr = readq(&dperf->fab_ctr);
	counter = ctr.fab_cnt;
exit:
	spinlock_unlock(&fme->lock);
	return counter;
}

#define FAB_PORT_SHOW(name, event)					\
static int fme_dperf_get_fab_port_##name(struct ifpga_fme_hw *fme,	\
					 u8 port_id, u64 *counter)	\
{									\
	*counter = read_fabric_counter(fme, port_id, event);		\
	return 0;							\
}

FAB_PORT_SHOW(pcie0_read, DPERF_FAB_PCIE0_RD);
FAB_PORT_SHOW(pcie0_write, DPERF_FAB_PCIE0_WR);
FAB_PORT_SHOW(mmio_read, DPERF_FAB_MMIO_RD);
FAB_PORT_SHOW(mmio_write, DPERF_FAB_MMIO_WR);

static int fme_dperf_get_fab_port_enable(struct ifpga_fme_hw *fme,
					 u8 port_id, u64 *enable)
{
	struct feature_fme_dperf *dperf;
	int status;

	dperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_DPERF);

	status = fabric_pobj_is_enabled(port_id, dperf);
	*enable = (u64)status;

	return 0;
}

/*
 * If enable one port or all port event counter in fabric, other
 * fabric event counter originally enabled will be disable automatically.
 */
static int fme_dperf_set_fab_port_enable(struct ifpga_fme_hw *fme,
					 u8 port_id, u64 enable)
{
	struct feature_fme_dfpmon_fab_ctl ctl;
	struct feature_fme_dperf *dperf;
	bool state;

	state = !!enable;

	if (!state)
		return -EINVAL;

	dperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_DPERF);

	/* if it is already enabled. */
	if (fabric_pobj_is_enabled(port_id, dperf))
		return 0;

	spinlock_lock(&fme->lock);
	ctl.csr = readq(&dperf->fab_ctl);
	if (port_id == PERF_OBJ_ROOT_ID) {
		ctl.port_filter = FAB_DISABLE_FILTER;
	} else {
		ctl.port_filter = FAB_ENABLE_FILTER;
		ctl.port_id = port_id;
	}

	writeq(ctl.csr, &dperf->fab_ctl);
	spinlock_unlock(&fme->lock);

	return 0;
}

static int fme_dperf_get_fab_freeze(struct ifpga_fme_hw *fme, u64 *freeze)
{
	struct feature_fme_dperf *dperf;
	struct feature_fme_dfpmon_fab_ctl ctl;

	dperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_DPERF);
	ctl.csr = readq(&dperf->fab_ctl);
	*freeze = (u64)ctl.freeze;

	return 0;
}

static int fme_dperf_set_fab_freeze(struct ifpga_fme_hw *fme, u64 freeze)
{
	struct feature_fme_dperf *dperf;
	struct feature_fme_dfpmon_fab_ctl ctl;
	bool state;

	state = !!freeze;

	spinlock_lock(&fme->lock);
	dperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_DPERF);
	ctl.csr = readq(&dperf->fab_ctl);
	ctl.freeze = state;
	writeq(ctl.csr, &dperf->fab_ctl);
	spinlock_unlock(&fme->lock);

	return 0;
}

#define PERF_MAX_PORT_NUM	1

static int fme_global_dperf_init(struct ifpga_feature *feature)
{
	UNUSED(feature);

	dev_info(NULL, "FME global_dperf Init.\n");

	return 0;
}

static void fme_global_dperf_uinit(struct ifpga_feature *feature)
{
	UNUSED(feature);

	dev_info(NULL, "FME global_dperf UInit.\n");
}

static int fme_dperf_fab_get_prop(struct ifpga_feature *feature,
				  struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;
	u8 sub = GET_FIELD(PROP_SUB, prop->prop_id);
	u16 id = GET_FIELD(PROP_ID, prop->prop_id);

	switch (id) {
	case 0x1: /* FREEZE */
		return fme_dperf_get_fab_freeze(fme, &prop->data);
	case 0x2: /* PCIE0_READ */
		return fme_dperf_get_fab_port_pcie0_read(fme, sub, &prop->data);
	case 0x3: /* PCIE0_WRITE */
		return fme_dperf_get_fab_port_pcie0_write(fme, sub,
							  &prop->data);
	case 0x4: /* MMIO_READ */
		return fme_dperf_get_fab_port_mmio_read(fme, sub, &prop->data);
	case 0x5: /* MMIO_WRITE */
		return fme_dperf_get_fab_port_mmio_write(fme, sub, &prop->data);
	case 0x6: /* ENABLE */
		return fme_dperf_get_fab_port_enable(fme, sub, &prop->data);
	}

	return -ENOENT;
}

static int fme_dperf_root_get_prop(struct ifpga_feature *feature,
				   struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;
	u8 sub = GET_FIELD(PROP_SUB, prop->prop_id);
	u16 id = GET_FIELD(PROP_ID, prop->prop_id);

	if (sub != PERF_PROP_SUB_UNUSED)
		return -ENOENT;

	switch (id) {
	case 0x1: /* CLOCK */
		return fme_dperf_get_clock(fme, &prop->data);
	case 0x2: /* REVISION */
		return fme_dperf_get_revision(fme, &prop->data);
	}

	return -ENOENT;
}

static int fme_global_dperf_get_prop(struct ifpga_feature *feature,
				     struct feature_prop *prop)
{
	u8 top = GET_FIELD(PROP_TOP, prop->prop_id);

	switch (top) {
	case PERF_PROP_TOP_FAB:
		return fme_dperf_fab_get_prop(feature, prop);
	case PERF_PROP_TOP_UNUSED:
		return fme_dperf_root_get_prop(feature, prop);
	}

	return -ENOENT;
}

static int fme_dperf_fab_set_prop(struct ifpga_feature *feature,
				  struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;
	u8 sub = GET_FIELD(PROP_SUB, prop->prop_id);
	u16 id = GET_FIELD(PROP_ID, prop->prop_id);

	switch (id) {
	case 0x1: /* FREEZE - fab root only prop */
		if (sub != PERF_PROP_SUB_UNUSED)
			return -ENOENT;
		return fme_dperf_set_fab_freeze(fme, prop->data);
	case 0x6: /* ENABLE - fab both root and sub */
		return fme_dperf_set_fab_port_enable(fme, sub, prop->data);
	}

	return -ENOENT;
}

static int fme_global_dperf_set_prop(struct ifpga_feature *feature,
				     struct feature_prop *prop)
{
	u8 top = GET_FIELD(PROP_TOP, prop->prop_id);

	switch (top) {
	case PERF_PROP_TOP_FAB:
		return fme_dperf_fab_set_prop(feature, prop);
	}

	return -ENOENT;
}

struct ifpga_feature_ops fme_global_dperf_ops = {
	.init = fme_global_dperf_init,
	.uinit = fme_global_dperf_uinit,
	.get_prop = fme_global_dperf_get_prop,
	.set_prop = fme_global_dperf_set_prop,

};
