/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include "ifpga_feature_dev.h"

#define PERF_OBJ_ROOT_ID	0xff

static int fme_iperf_get_clock(struct ifpga_fme_hw *fme, u64 *clock)
{
	struct feature_fme_iperf *iperf;
	struct feature_fme_ifpmon_clk_ctr clk;

	iperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_IPERF);
	clk.afu_interf_clock = readq(&iperf->clk);

	*clock = clk.afu_interf_clock;
	return 0;
}

static int fme_iperf_get_revision(struct ifpga_fme_hw *fme, u64 *revision)
{
	struct feature_fme_iperf *iperf;
	struct feature_header header;

	iperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_IPERF);
	header.csr = readq(&iperf->header);
	*revision = header.revision;

	return 0;
}

static int fme_iperf_get_cache_freeze(struct ifpga_fme_hw *fme, u64 *freeze)
{
	struct feature_fme_iperf *iperf;
	struct feature_fme_ifpmon_ch_ctl ctl;

	iperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_IPERF);
	ctl.csr = readq(&iperf->ch_ctl);
	*freeze = (u64)ctl.freeze;
	return 0;
}

static int fme_iperf_set_cache_freeze(struct ifpga_fme_hw *fme, u64 freeze)
{
	struct feature_fme_iperf *iperf;
	struct feature_fme_ifpmon_ch_ctl ctl;
	bool state;

	state = !!freeze;

	spinlock_lock(&fme->lock);
	iperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_IPERF);
	ctl.csr = readq(&iperf->ch_ctl);
	ctl.freeze = state;
	writeq(ctl.csr, &iperf->ch_ctl);
	spinlock_unlock(&fme->lock);

	return 0;
}

#define IPERF_TIMEOUT	30

static u64 read_cache_counter(struct ifpga_fme_hw *fme,
			      u8 channel, enum iperf_cache_events event)
{
	struct feature_fme_iperf *iperf;
	struct feature_fme_ifpmon_ch_ctl ctl;
	struct feature_fme_ifpmon_ch_ctr ctr0, ctr1;
	u64 counter;

	spinlock_lock(&fme->lock);
	iperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_IPERF);

	/* set channel access type and cache event code. */
	ctl.csr = readq(&iperf->ch_ctl);
	ctl.cci_chsel = channel;
	ctl.cache_event = event;
	writeq(ctl.csr, &iperf->ch_ctl);

	/* check the event type in the counter registers */
	ctr0.event_code = event;

	if (fpga_wait_register_field(event_code, ctr0,
				     &iperf->ch_ctr0, IPERF_TIMEOUT, 1)) {
		dev_err(fme, "timeout, unmatched cache event type in counter registers.\n");
		spinlock_unlock(&fme->lock);
		return -ETIMEDOUT;
	}

	ctr0.csr = readq(&iperf->ch_ctr0);
	ctr1.csr = readq(&iperf->ch_ctr1);
	counter = ctr0.cache_counter + ctr1.cache_counter;
	spinlock_unlock(&fme->lock);

	return counter;
}

#define CACHE_SHOW(name, type, event)					\
static int fme_iperf_get_cache_##name(struct ifpga_fme_hw *fme,		\
					u64 *counter)			\
{									\
	*counter = read_cache_counter(fme, type, event);		\
	return 0;							\
}

CACHE_SHOW(read_hit, CACHE_CHANNEL_RD, IPERF_CACHE_RD_HIT);
CACHE_SHOW(read_miss, CACHE_CHANNEL_RD, IPERF_CACHE_RD_MISS);
CACHE_SHOW(write_hit, CACHE_CHANNEL_WR, IPERF_CACHE_WR_HIT);
CACHE_SHOW(write_miss, CACHE_CHANNEL_WR, IPERF_CACHE_WR_MISS);
CACHE_SHOW(hold_request, CACHE_CHANNEL_RD, IPERF_CACHE_HOLD_REQ);
CACHE_SHOW(tx_req_stall, CACHE_CHANNEL_RD, IPERF_CACHE_TX_REQ_STALL);
CACHE_SHOW(rx_req_stall, CACHE_CHANNEL_RD, IPERF_CACHE_RX_REQ_STALL);
CACHE_SHOW(rx_eviction, CACHE_CHANNEL_RD, IPERF_CACHE_EVICTIONS);
CACHE_SHOW(data_write_port_contention, CACHE_CHANNEL_WR,
	   IPERF_CACHE_DATA_WR_PORT_CONTEN);
CACHE_SHOW(tag_write_port_contention, CACHE_CHANNEL_WR,
	   IPERF_CACHE_TAG_WR_PORT_CONTEN);

static int fme_iperf_get_vtd_freeze(struct ifpga_fme_hw *fme, u64 *freeze)
{
	struct feature_fme_ifpmon_vtd_ctl ctl;
	struct feature_fme_iperf *iperf;

	iperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_IPERF);
	ctl.csr = readq(&iperf->vtd_ctl);
	*freeze = (u64)ctl.freeze;

	return 0;
}

static int fme_iperf_set_vtd_freeze(struct ifpga_fme_hw *fme, u64 freeze)
{
	struct feature_fme_ifpmon_vtd_ctl ctl;
	struct feature_fme_iperf *iperf;
	bool state;

	state = !!freeze;

	spinlock_lock(&fme->lock);
	iperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_IPERF);
	ctl.csr = readq(&iperf->vtd_ctl);
	ctl.freeze = state;
	writeq(ctl.csr, &iperf->vtd_ctl);
	spinlock_unlock(&fme->lock);

	return 0;
}

static u64 read_iommu_sip_counter(struct ifpga_fme_hw *fme,
				  enum iperf_vtd_sip_events event)
{
	struct feature_fme_ifpmon_vtd_sip_ctl sip_ctl;
	struct feature_fme_ifpmon_vtd_sip_ctr sip_ctr;
	struct feature_fme_iperf *iperf;
	u64 counter;

	spinlock_lock(&fme->lock);
	iperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_IPERF);
	sip_ctl.csr = readq(&iperf->vtd_sip_ctl);
	sip_ctl.vtd_evtcode = event;
	writeq(sip_ctl.csr, &iperf->vtd_sip_ctl);

	sip_ctr.event_code = event;

	if (fpga_wait_register_field(event_code, sip_ctr,
				     &iperf->vtd_sip_ctr, IPERF_TIMEOUT, 1)) {
		dev_err(fme, "timeout, unmatched VTd SIP event type in counter registers\n");
		spinlock_unlock(&fme->lock);
		return -ETIMEDOUT;
	}

	sip_ctr.csr = readq(&iperf->vtd_sip_ctr);
	counter = sip_ctr.vtd_counter;
	spinlock_unlock(&fme->lock);

	return counter;
}

#define VTD_SIP_SHOW(name, event)					\
static int fme_iperf_get_vtd_sip_##name(struct ifpga_fme_hw *fme,	\
						u64 *counter)		\
{									\
	*counter = read_iommu_sip_counter(fme, event);			\
	return 0;							\
}

VTD_SIP_SHOW(iotlb_4k_hit, IPERF_VTD_SIP_IOTLB_4K_HIT);
VTD_SIP_SHOW(iotlb_2m_hit, IPERF_VTD_SIP_IOTLB_2M_HIT);
VTD_SIP_SHOW(iotlb_1g_hit, IPERF_VTD_SIP_IOTLB_1G_HIT);
VTD_SIP_SHOW(slpwc_l3_hit, IPERF_VTD_SIP_SLPWC_L3_HIT);
VTD_SIP_SHOW(slpwc_l4_hit, IPERF_VTD_SIP_SLPWC_L4_HIT);
VTD_SIP_SHOW(rcc_hit, IPERF_VTD_SIP_RCC_HIT);
VTD_SIP_SHOW(iotlb_4k_miss, IPERF_VTD_SIP_IOTLB_4K_MISS);
VTD_SIP_SHOW(iotlb_2m_miss, IPERF_VTD_SIP_IOTLB_2M_MISS);
VTD_SIP_SHOW(iotlb_1g_miss, IPERF_VTD_SIP_IOTLB_1G_MISS);
VTD_SIP_SHOW(slpwc_l3_miss, IPERF_VTD_SIP_SLPWC_L3_MISS);
VTD_SIP_SHOW(slpwc_l4_miss, IPERF_VTD_SIP_SLPWC_L4_MISS);
VTD_SIP_SHOW(rcc_miss, IPERF_VTD_SIP_RCC_MISS);

static u64 read_iommu_counter(struct ifpga_fme_hw *fme, u8 port_id,
			      enum iperf_vtd_events base_event)
{
	struct feature_fme_ifpmon_vtd_ctl ctl;
	struct feature_fme_ifpmon_vtd_ctr ctr;
	struct feature_fme_iperf *iperf;
	enum iperf_vtd_events event = base_event + port_id;
	u64 counter;

	spinlock_lock(&fme->lock);
	iperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_IPERF);
	ctl.csr = readq(&iperf->vtd_ctl);
	ctl.vtd_evtcode = event;
	writeq(ctl.csr, &iperf->vtd_ctl);

	ctr.event_code = event;

	if (fpga_wait_register_field(event_code, ctr,
				     &iperf->vtd_ctr, IPERF_TIMEOUT, 1)) {
		dev_err(fme, "timeout, unmatched VTd event type in counter registers.\n");
		spinlock_unlock(&fme->lock);
		return -ETIMEDOUT;
	}

	ctr.csr = readq(&iperf->vtd_ctr);
	counter = ctr.vtd_counter;
	spinlock_unlock(&fme->lock);

	return counter;
}

#define VTD_PORT_SHOW(name, base_event)					\
static int fme_iperf_get_vtd_port_##name(struct ifpga_fme_hw *fme,	\
				u8 port_id, u64 *counter)		\
{									\
	*counter = read_iommu_counter(fme, port_id, base_event);	\
	return 0;							\
}

VTD_PORT_SHOW(read_transaction, IPERF_VTD_AFU_MEM_RD_TRANS);
VTD_PORT_SHOW(write_transaction, IPERF_VTD_AFU_MEM_WR_TRANS);
VTD_PORT_SHOW(devtlb_read_hit, IPERF_VTD_AFU_DEVTLB_RD_HIT);
VTD_PORT_SHOW(devtlb_write_hit, IPERF_VTD_AFU_DEVTLB_WR_HIT);
VTD_PORT_SHOW(devtlb_4k_fill, IPERF_VTD_DEVTLB_4K_FILL);
VTD_PORT_SHOW(devtlb_2m_fill, IPERF_VTD_DEVTLB_2M_FILL);
VTD_PORT_SHOW(devtlb_1g_fill, IPERF_VTD_DEVTLB_1G_FILL);

static bool fabric_pobj_is_enabled(u8 port_id, struct feature_fme_iperf *iperf)
{
	struct feature_fme_ifpmon_fab_ctl ctl;

	ctl.csr = readq(&iperf->fab_ctl);

	if (ctl.port_filter == FAB_DISABLE_FILTER)
		return port_id == PERF_OBJ_ROOT_ID;

	return port_id == ctl.port_id;
}

static u64 read_fabric_counter(struct ifpga_fme_hw *fme, u8 port_id,
			       enum iperf_fab_events fab_event)
{
	struct feature_fme_ifpmon_fab_ctl ctl;
	struct feature_fme_ifpmon_fab_ctr ctr;
	struct feature_fme_iperf *iperf;
	u64 counter = 0;

	spinlock_lock(&fme->lock);
	iperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_IPERF);

	/* if it is disabled, force the counter to return zero. */
	if (!fabric_pobj_is_enabled(port_id, iperf))
		goto exit;

	ctl.csr = readq(&iperf->fab_ctl);
	ctl.fab_evtcode = fab_event;
	writeq(ctl.csr, &iperf->fab_ctl);

	ctr.event_code = fab_event;

	if (fpga_wait_register_field(event_code, ctr,
				     &iperf->fab_ctr, IPERF_TIMEOUT, 1)) {
		dev_err(fme, "timeout, unmatched VTd event type in counter registers.\n");
		spinlock_unlock(&fme->lock);
		return -ETIMEDOUT;
	}

	ctr.csr = readq(&iperf->fab_ctr);
	counter = ctr.fab_cnt;
exit:
	spinlock_unlock(&fme->lock);
	return counter;
}

#define FAB_PORT_SHOW(name, event)					\
static int fme_iperf_get_fab_port_##name(struct ifpga_fme_hw *fme,	\
				u8 port_id, u64 *counter)		\
{									\
	*counter = read_fabric_counter(fme, port_id, event);		\
	return 0;							\
}

FAB_PORT_SHOW(pcie0_read, IPERF_FAB_PCIE0_RD);
FAB_PORT_SHOW(pcie0_write, IPERF_FAB_PCIE0_WR);
FAB_PORT_SHOW(pcie1_read, IPERF_FAB_PCIE1_RD);
FAB_PORT_SHOW(pcie1_write, IPERF_FAB_PCIE1_WR);
FAB_PORT_SHOW(upi_read, IPERF_FAB_UPI_RD);
FAB_PORT_SHOW(upi_write, IPERF_FAB_UPI_WR);
FAB_PORT_SHOW(mmio_read, IPERF_FAB_MMIO_RD);
FAB_PORT_SHOW(mmio_write, IPERF_FAB_MMIO_WR);

static int fme_iperf_get_fab_port_enable(struct ifpga_fme_hw *fme,
					 u8 port_id, u64 *enable)
{
	struct feature_fme_iperf *iperf;
	int status;

	iperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_IPERF);

	status = fabric_pobj_is_enabled(port_id, iperf);
	*enable = (u64)status;

	return 0;
}

/*
 * If enable one port or all port event counter in fabric, other
 * fabric event counter originally enabled will be disable automatically.
 */
static int fme_iperf_set_fab_port_enable(struct ifpga_fme_hw *fme,
					 u8 port_id, u64 enable)
{
	struct feature_fme_ifpmon_fab_ctl ctl;
	struct feature_fme_iperf *iperf;
	bool state;

	state = !!enable;

	if (!state)
		return -EINVAL;

	iperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_IPERF);

	/* if it is already enabled. */
	if (fabric_pobj_is_enabled(port_id, iperf))
		return 0;

	spinlock_lock(&fme->lock);
	ctl.csr = readq(&iperf->fab_ctl);
	if (port_id == PERF_OBJ_ROOT_ID) {
		ctl.port_filter = FAB_DISABLE_FILTER;
	} else {
		ctl.port_filter = FAB_ENABLE_FILTER;
		ctl.port_id = port_id;
	}

	writeq(ctl.csr, &iperf->fab_ctl);
	spinlock_unlock(&fme->lock);

	return 0;
}

static int fme_iperf_get_fab_freeze(struct ifpga_fme_hw *fme, u64 *freeze)
{
	struct feature_fme_iperf *iperf;
	struct feature_fme_ifpmon_fab_ctl ctl;

	iperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_IPERF);
	ctl.csr = readq(&iperf->fab_ctl);
	*freeze = (u64)ctl.freeze;

	return 0;
}

static int fme_iperf_set_fab_freeze(struct ifpga_fme_hw *fme, u64 freeze)
{
	struct feature_fme_iperf *iperf;
	struct feature_fme_ifpmon_fab_ctl ctl;
	bool state;

	state = !!freeze;

	spinlock_lock(&fme->lock);
	iperf = get_fme_feature_ioaddr_by_index(fme,
						FME_FEATURE_ID_GLOBAL_IPERF);
	ctl.csr = readq(&iperf->fab_ctl);
	ctl.freeze = state;
	writeq(ctl.csr, &iperf->fab_ctl);
	spinlock_unlock(&fme->lock);

	return 0;
}

#define PERF_MAX_PORT_NUM	1
#define FME_IPERF_CAP_IOMMU	0x1

static int fme_global_iperf_init(struct feature *feature)
{
	struct ifpga_fme_hw *fme;
	struct feature_fme_header *fme_hdr;
	struct feature_fme_capability fme_capability;

	dev_info(NULL, "FME global_iperf Init.\n");

	fme = (struct ifpga_fme_hw *)feature->parent;
	fme_hdr = get_fme_feature_ioaddr_by_index(fme, FME_FEATURE_ID_HEADER);

	/* check if iommu is not supported on this device. */
	fme_capability.csr = readq(&fme_hdr->capability);
	dev_info(NULL, "FME HEAD fme_capability %llx.\n",
		 (unsigned long long)fme_hdr->capability.csr);

	if (fme_capability.iommu_support)
		feature->cap |= FME_IPERF_CAP_IOMMU;

	return 0;
}

static void fme_global_iperf_uinit(struct feature *feature)
{
	UNUSED(feature);

	dev_info(NULL, "FME global_iperf UInit.\n");
}

static int fme_iperf_root_get_prop(struct feature *feature,
				   struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;
	u8 sub = GET_FIELD(PROP_SUB, prop->prop_id);
	u16 id = GET_FIELD(PROP_ID, prop->prop_id);

	if (sub != PERF_PROP_SUB_UNUSED)
		return -ENOENT;

	switch (id) {
	case 0x1: /* CLOCK */
		return fme_iperf_get_clock(fme, &prop->data);
	case 0x2: /* REVISION */
		return fme_iperf_get_revision(fme, &prop->data);
	}

	return -ENOENT;
}

static int fme_iperf_cache_get_prop(struct feature *feature,
				    struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;
	u8 sub = GET_FIELD(PROP_SUB, prop->prop_id);
	u16 id = GET_FIELD(PROP_ID, prop->prop_id);

	if (sub != PERF_PROP_SUB_UNUSED)
		return -ENOENT;

	switch (id) {
	case 0x1: /* FREEZE */
		return fme_iperf_get_cache_freeze(fme, &prop->data);
	case 0x2: /* READ_HIT */
		return fme_iperf_get_cache_read_hit(fme, &prop->data);
	case 0x3: /* READ_MISS */
		return fme_iperf_get_cache_read_miss(fme, &prop->data);
	case 0x4: /* WRITE_HIT */
		return fme_iperf_get_cache_write_hit(fme, &prop->data);
	case 0x5: /* WRITE_MISS */
		return fme_iperf_get_cache_write_miss(fme, &prop->data);
	case 0x6: /* HOLD_REQUEST */
		return fme_iperf_get_cache_hold_request(fme, &prop->data);
	case 0x7: /* TX_REQ_STALL */
		return fme_iperf_get_cache_tx_req_stall(fme, &prop->data);
	case 0x8: /* RX_REQ_STALL */
		return fme_iperf_get_cache_rx_req_stall(fme, &prop->data);
	case 0x9: /* RX_EVICTION */
		return fme_iperf_get_cache_rx_eviction(fme, &prop->data);
	case 0xa: /* DATA_WRITE_PORT_CONTENTION */
		return fme_iperf_get_cache_data_write_port_contention(fme,
							&prop->data);
	case 0xb: /* TAG_WRITE_PORT_CONTENTION */
		return fme_iperf_get_cache_tag_write_port_contention(fme,
							&prop->data);
	}

	return -ENOENT;
}

static int fme_iperf_vtd_root_get_prop(struct feature *feature,
				       struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;
	u16 id = GET_FIELD(PROP_ID, prop->prop_id);

	switch (id) {
	case 0x1: /* FREEZE */
		return fme_iperf_get_vtd_freeze(fme, &prop->data);
	case 0x2: /* IOTLB_4K_HIT */
		return fme_iperf_get_vtd_sip_iotlb_4k_hit(fme, &prop->data);
	case 0x3: /* IOTLB_2M_HIT */
		return fme_iperf_get_vtd_sip_iotlb_2m_hit(fme, &prop->data);
	case 0x4: /* IOTLB_1G_HIT */
		return fme_iperf_get_vtd_sip_iotlb_1g_hit(fme, &prop->data);
	case 0x5: /* SLPWC_L3_HIT */
		return fme_iperf_get_vtd_sip_slpwc_l3_hit(fme, &prop->data);
	case 0x6: /* SLPWC_L4_HIT */
		return fme_iperf_get_vtd_sip_slpwc_l4_hit(fme, &prop->data);
	case 0x7: /* RCC_HIT */
		return fme_iperf_get_vtd_sip_rcc_hit(fme, &prop->data);
	case 0x8: /* IOTLB_4K_MISS */
		return fme_iperf_get_vtd_sip_iotlb_4k_miss(fme, &prop->data);
	case 0x9: /* IOTLB_2M_MISS */
		return fme_iperf_get_vtd_sip_iotlb_2m_miss(fme, &prop->data);
	case 0xa: /* IOTLB_1G_MISS */
		return fme_iperf_get_vtd_sip_iotlb_1g_miss(fme, &prop->data);
	case 0xb: /* SLPWC_L3_MISS */
		return fme_iperf_get_vtd_sip_slpwc_l3_miss(fme, &prop->data);
	case 0xc: /* SLPWC_L4_MISS */
		return fme_iperf_get_vtd_sip_slpwc_l4_miss(fme, &prop->data);
	case 0xd: /* RCC_MISS */
		return fme_iperf_get_vtd_sip_rcc_miss(fme, &prop->data);
	}

	return -ENOENT;
}

static int fme_iperf_vtd_sub_get_prop(struct feature *feature,
				      struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;
	u16 id = GET_FIELD(PROP_ID, prop->prop_id);
	u8 sub = GET_FIELD(PROP_SUB, prop->prop_id);

	if (sub > PERF_MAX_PORT_NUM)
		return -ENOENT;

	switch (id) {
	case 0xe: /* READ_TRANSACTION */
		return fme_iperf_get_vtd_port_read_transaction(fme, sub,
							       &prop->data);
	case 0xf: /* WRITE_TRANSACTION */
		return fme_iperf_get_vtd_port_write_transaction(fme, sub,
								&prop->data);
	case 0x10: /* DEVTLB_READ_HIT */
		return fme_iperf_get_vtd_port_devtlb_read_hit(fme, sub,
							      &prop->data);
	case 0x11: /* DEVTLB_WRITE_HIT */
		return fme_iperf_get_vtd_port_devtlb_write_hit(fme, sub,
							       &prop->data);
	case 0x12: /* DEVTLB_4K_FILL */
		return fme_iperf_get_vtd_port_devtlb_4k_fill(fme, sub,
							     &prop->data);
	case 0x13: /* DEVTLB_2M_FILL */
		return fme_iperf_get_vtd_port_devtlb_2m_fill(fme, sub,
							     &prop->data);
	case 0x14: /* DEVTLB_1G_FILL */
		return fme_iperf_get_vtd_port_devtlb_1g_fill(fme, sub,
							     &prop->data);
	}

	return -ENOENT;
}

static int fme_iperf_vtd_get_prop(struct feature *feature,
				  struct feature_prop *prop)
{
	u8 sub = GET_FIELD(PROP_SUB, prop->prop_id);

	if (sub == PERF_PROP_SUB_UNUSED)
		return fme_iperf_vtd_root_get_prop(feature, prop);

	return fme_iperf_vtd_sub_get_prop(feature, prop);
}

static int fme_iperf_fab_get_prop(struct feature *feature,
				  struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;
	u8 sub = GET_FIELD(PROP_SUB, prop->prop_id);
	u16 id = GET_FIELD(PROP_ID, prop->prop_id);

	/* Other properties are present for both top and sub levels */
	switch (id) {
	case 0x1: /* FREEZE */
		if (sub != PERF_PROP_SUB_UNUSED)
			return -ENOENT;
		return fme_iperf_get_fab_freeze(fme, &prop->data);
	case 0x2: /* PCIE0_READ */
		return fme_iperf_get_fab_port_pcie0_read(fme, sub,
							 &prop->data);
	case 0x3: /* PCIE0_WRITE */
		return fme_iperf_get_fab_port_pcie0_write(fme, sub,
							  &prop->data);
	case 0x4: /* PCIE1_READ */
		return fme_iperf_get_fab_port_pcie1_read(fme, sub,
							 &prop->data);
	case 0x5: /* PCIE1_WRITE */
		return fme_iperf_get_fab_port_pcie1_write(fme, sub,
							  &prop->data);
	case 0x6: /* UPI_READ */
		return fme_iperf_get_fab_port_upi_read(fme, sub,
						       &prop->data);
	case 0x7: /* UPI_WRITE */
		return fme_iperf_get_fab_port_upi_write(fme, sub,
							&prop->data);
	case 0x8: /* MMIO_READ */
		return fme_iperf_get_fab_port_mmio_read(fme, sub,
							&prop->data);
	case 0x9: /* MMIO_WRITE */
		return fme_iperf_get_fab_port_mmio_write(fme, sub,
							 &prop->data);
	case 0xa: /* ENABLE */
		return fme_iperf_get_fab_port_enable(fme, sub, &prop->data);
	}

	return -ENOENT;
}

static int fme_global_iperf_get_prop(struct feature *feature,
				     struct feature_prop *prop)
{
	u8 top = GET_FIELD(PROP_TOP, prop->prop_id);

	switch (top) {
	case PERF_PROP_TOP_CACHE:
		return fme_iperf_cache_get_prop(feature, prop);
	case PERF_PROP_TOP_VTD:
		return fme_iperf_vtd_get_prop(feature, prop);
	case PERF_PROP_TOP_FAB:
		return fme_iperf_fab_get_prop(feature, prop);
	case PERF_PROP_TOP_UNUSED:
		return fme_iperf_root_get_prop(feature, prop);
	}

	return -ENOENT;
}

static int fme_iperf_cache_set_prop(struct feature *feature,
				    struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;
	u8 sub = GET_FIELD(PROP_SUB, prop->prop_id);
	u16 id = GET_FIELD(PROP_ID, prop->prop_id);

	if (sub == PERF_PROP_SUB_UNUSED && id == 0x1) /* FREEZE */
		return fme_iperf_set_cache_freeze(fme, prop->data);

	return -ENOENT;
}

static int fme_iperf_vtd_set_prop(struct feature *feature,
				  struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;
	u8 sub = GET_FIELD(PROP_SUB, prop->prop_id);
	u16 id = GET_FIELD(PROP_ID, prop->prop_id);

	if (sub == PERF_PROP_SUB_UNUSED && id == 0x1) /* FREEZE */
		return fme_iperf_set_vtd_freeze(fme, prop->data);

	return -ENOENT;
}

static int fme_iperf_fab_set_prop(struct feature *feature,
				  struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;
	u8 sub = GET_FIELD(PROP_SUB, prop->prop_id);
	u16 id = GET_FIELD(PROP_ID, prop->prop_id);

	switch (id) {
	case 0x1: /* FREEZE */
		if (sub != PERF_PROP_SUB_UNUSED)
			return -ENOENT;
		return fme_iperf_set_fab_freeze(fme, prop->data);
	case 0xa: /* ENABLE */
		return fme_iperf_set_fab_port_enable(fme, sub, prop->data);
	}

	return -ENOENT;
}

static int fme_global_iperf_set_prop(struct feature *feature,
				     struct feature_prop *prop)
{
	u8 top = GET_FIELD(PROP_TOP, prop->prop_id);

	switch (top) {
	case PERF_PROP_TOP_CACHE:
		return fme_iperf_cache_set_prop(feature, prop);
	case PERF_PROP_TOP_VTD:
		return fme_iperf_vtd_set_prop(feature, prop);
	case PERF_PROP_TOP_FAB:
		return fme_iperf_fab_set_prop(feature, prop);
	}

	return -ENOENT;
}

struct feature_ops fme_global_iperf_ops = {
	.init = fme_global_iperf_init,
	.uinit = fme_global_iperf_uinit,
	.get_prop = fme_global_iperf_get_prop,
	.set_prop = fme_global_iperf_set_prop,
};
