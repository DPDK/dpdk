/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include "ifpga_feature_dev.h"

#define PWR_THRESHOLD_MAX       0x7F

int fme_get_prop(struct ifpga_fme_hw *fme, struct feature_prop *prop)
{
	struct feature *feature;

	if (!fme)
		return -ENOENT;

	feature = get_fme_feature_by_id(fme, prop->feature_id);

	if (feature && feature->ops && feature->ops->get_prop)
		return feature->ops->get_prop(feature, prop);

	return -ENOENT;
}

int fme_set_prop(struct ifpga_fme_hw *fme, struct feature_prop *prop)
{
	struct feature *feature;

	if (!fme)
		return -ENOENT;

	feature = get_fme_feature_by_id(fme, prop->feature_id);

	if (feature && feature->ops && feature->ops->set_prop)
		return feature->ops->set_prop(feature, prop);

	return -ENOENT;
}

int fme_set_irq(struct ifpga_fme_hw *fme, u32 feature_id, void *irq_set)
{
	struct feature *feature;

	if (!fme)
		return -ENOENT;

	feature = get_fme_feature_by_id(fme, feature_id);

	if (feature && feature->ops && feature->ops->set_irq)
		return feature->ops->set_irq(feature, irq_set);

	return -ENOENT;
}

/* fme private feature head */
static int fme_hdr_init(struct feature *feature)
{
	struct feature_fme_header *fme_hdr;

	fme_hdr = (struct feature_fme_header *)feature->addr;

	dev_info(NULL, "FME HDR Init.\n");
	dev_info(NULL, "FME cap %llx.\n",
		 (unsigned long long)fme_hdr->capability.csr);

	return 0;
}

static void fme_hdr_uinit(struct feature *feature)
{
	UNUSED(feature);

	dev_info(NULL, "FME HDR UInit.\n");
}

static int fme_hdr_get_revision(struct ifpga_fme_hw *fme, u64 *revision)
{
	struct feature_fme_header *fme_hdr
		= get_fme_feature_ioaddr_by_index(fme, FME_FEATURE_ID_HEADER);
	struct feature_header header;

	header.csr = readq(&fme_hdr->header);
	*revision = header.revision;

	return 0;
}

static int fme_hdr_get_ports_num(struct ifpga_fme_hw *fme, u64 *ports_num)
{
	struct feature_fme_header *fme_hdr
		= get_fme_feature_ioaddr_by_index(fme, FME_FEATURE_ID_HEADER);
	struct feature_fme_capability fme_capability;

	fme_capability.csr = readq(&fme_hdr->capability);
	*ports_num = fme_capability.num_ports;

	return 0;
}

static int fme_hdr_get_cache_size(struct ifpga_fme_hw *fme, u64 *cache_size)
{
	struct feature_fme_header *fme_hdr
		= get_fme_feature_ioaddr_by_index(fme, FME_FEATURE_ID_HEADER);
	struct feature_fme_capability fme_capability;

	fme_capability.csr = readq(&fme_hdr->capability);
	*cache_size = fme_capability.cache_size;

	return 0;
}

static int fme_hdr_get_version(struct ifpga_fme_hw *fme, u64 *version)
{
	struct feature_fme_header *fme_hdr
		= get_fme_feature_ioaddr_by_index(fme, FME_FEATURE_ID_HEADER);
	struct feature_fme_capability fme_capability;

	fme_capability.csr = readq(&fme_hdr->capability);
	*version = fme_capability.fabric_verid;

	return 0;
}

static int fme_hdr_get_socket_id(struct ifpga_fme_hw *fme, u64 *socket_id)
{
	struct feature_fme_header *fme_hdr
		= get_fme_feature_ioaddr_by_index(fme, FME_FEATURE_ID_HEADER);
	struct feature_fme_capability fme_capability;

	fme_capability.csr = readq(&fme_hdr->capability);
	*socket_id = fme_capability.socket_id;

	return 0;
}

static int fme_hdr_get_bitstream_id(struct ifpga_fme_hw *fme,
				    u64 *bitstream_id)
{
	struct feature_fme_header *fme_hdr
		= get_fme_feature_ioaddr_by_index(fme, FME_FEATURE_ID_HEADER);

	*bitstream_id = readq(&fme_hdr->bitstream_id);

	return 0;
}

static int fme_hdr_get_bitstream_metadata(struct ifpga_fme_hw *fme,
					  u64 *bitstream_metadata)
{
	struct feature_fme_header *fme_hdr
		= get_fme_feature_ioaddr_by_index(fme, FME_FEATURE_ID_HEADER);

	*bitstream_metadata = readq(&fme_hdr->bitstream_md);

	return 0;
}

static int
fme_hdr_get_prop(struct feature *feature, struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;

	switch (prop->prop_id) {
	case FME_HDR_PROP_REVISION:
		return fme_hdr_get_revision(fme, &prop->data);
	case FME_HDR_PROP_PORTS_NUM:
		return fme_hdr_get_ports_num(fme, &prop->data);
	case FME_HDR_PROP_CACHE_SIZE:
		return fme_hdr_get_cache_size(fme, &prop->data);
	case FME_HDR_PROP_VERSION:
		return fme_hdr_get_version(fme, &prop->data);
	case FME_HDR_PROP_SOCKET_ID:
		return fme_hdr_get_socket_id(fme, &prop->data);
	case FME_HDR_PROP_BITSTREAM_ID:
		return fme_hdr_get_bitstream_id(fme, &prop->data);
	case FME_HDR_PROP_BITSTREAM_METADATA:
		return fme_hdr_get_bitstream_metadata(fme, &prop->data);
	}

	return -ENOENT;
}

struct feature_ops fme_hdr_ops = {
	.init = fme_hdr_init,
	.uinit = fme_hdr_uinit,
	.get_prop = fme_hdr_get_prop,
};

/* thermal management */
static int fme_thermal_get_threshold1(struct ifpga_fme_hw *fme, u64 *thres1)
{
	struct feature_fme_thermal *thermal;
	struct feature_fme_tmp_threshold temp_threshold;

	thermal = get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_THERMAL_MGMT);

	temp_threshold.csr = readq(&thermal->threshold);
	*thres1 = temp_threshold.tmp_thshold1;

	return 0;
}

static int fme_thermal_set_threshold1(struct ifpga_fme_hw *fme, u64 thres1)
{
	struct feature_fme_thermal *thermal;
	struct feature_fme_header *fme_hdr;
	struct feature_fme_tmp_threshold tmp_threshold;
	struct feature_fme_capability fme_capability;

	thermal = get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_THERMAL_MGMT);
	fme_hdr = get_fme_feature_ioaddr_by_index(fme, FME_FEATURE_ID_HEADER);

	spinlock_lock(&fme->lock);
	tmp_threshold.csr = readq(&thermal->threshold);
	fme_capability.csr = readq(&fme_hdr->capability);

	if (fme_capability.lock_bit == 1) {
		spinlock_unlock(&fme->lock);
		return -EBUSY;
	} else if (thres1 > 100) {
		spinlock_unlock(&fme->lock);
		return -EINVAL;
	} else if (thres1 == 0) {
		tmp_threshold.tmp_thshold1_enable = 0;
		tmp_threshold.tmp_thshold1 = thres1;
	} else {
		tmp_threshold.tmp_thshold1_enable = 1;
		tmp_threshold.tmp_thshold1 = thres1;
	}

	writeq(tmp_threshold.csr, &thermal->threshold);
	spinlock_unlock(&fme->lock);

	return 0;
}

static int fme_thermal_get_threshold2(struct ifpga_fme_hw *fme, u64 *thres2)
{
	struct feature_fme_thermal *thermal;
	struct feature_fme_tmp_threshold temp_threshold;

	thermal = get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_THERMAL_MGMT);

	temp_threshold.csr = readq(&thermal->threshold);
	*thres2 = temp_threshold.tmp_thshold2;

	return 0;
}

static int fme_thermal_set_threshold2(struct ifpga_fme_hw *fme, u64 thres2)
{
	struct feature_fme_thermal *thermal;
	struct feature_fme_header *fme_hdr;
	struct feature_fme_tmp_threshold tmp_threshold;
	struct feature_fme_capability fme_capability;

	thermal = get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_THERMAL_MGMT);
	fme_hdr = get_fme_feature_ioaddr_by_index(fme, FME_FEATURE_ID_HEADER);

	spinlock_lock(&fme->lock);
	tmp_threshold.csr = readq(&thermal->threshold);
	fme_capability.csr = readq(&fme_hdr->capability);

	if (fme_capability.lock_bit == 1) {
		spinlock_unlock(&fme->lock);
		return -EBUSY;
	} else if (thres2 > 100) {
		spinlock_unlock(&fme->lock);
		return -EINVAL;
	} else if (thres2 == 0) {
		tmp_threshold.tmp_thshold2_enable = 0;
		tmp_threshold.tmp_thshold2 = thres2;
	} else {
		tmp_threshold.tmp_thshold2_enable = 1;
		tmp_threshold.tmp_thshold2 = thres2;
	}

	writeq(tmp_threshold.csr, &thermal->threshold);
	spinlock_unlock(&fme->lock);

	return 0;
}

static int fme_thermal_get_threshold_trip(struct ifpga_fme_hw *fme,
					  u64 *thres_trip)
{
	struct feature_fme_thermal *thermal;
	struct feature_fme_tmp_threshold temp_threshold;

	thermal = get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_THERMAL_MGMT);

	temp_threshold.csr = readq(&thermal->threshold);
	*thres_trip = temp_threshold.therm_trip_thshold;

	return 0;
}

static int fme_thermal_get_threshold1_reached(struct ifpga_fme_hw *fme,
					      u64 *thres1_reached)
{
	struct feature_fme_thermal *thermal;
	struct feature_fme_tmp_threshold temp_threshold;

	thermal = get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_THERMAL_MGMT);

	temp_threshold.csr = readq(&thermal->threshold);
	*thres1_reached = temp_threshold.thshold1_status;

	return 0;
}

static int fme_thermal_get_threshold2_reached(struct ifpga_fme_hw *fme,
					      u64 *thres1_reached)
{
	struct feature_fme_thermal *thermal;
	struct feature_fme_tmp_threshold temp_threshold;

	thermal = get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_THERMAL_MGMT);

	temp_threshold.csr = readq(&thermal->threshold);
	*thres1_reached = temp_threshold.thshold2_status;

	return 0;
}

static int fme_thermal_get_threshold1_policy(struct ifpga_fme_hw *fme,
					     u64 *thres1_policy)
{
	struct feature_fme_thermal *thermal;
	struct feature_fme_tmp_threshold temp_threshold;

	thermal = get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_THERMAL_MGMT);

	temp_threshold.csr = readq(&thermal->threshold);
	*thres1_policy = temp_threshold.thshold_policy;

	return 0;
}

static int fme_thermal_set_threshold1_policy(struct ifpga_fme_hw *fme,
					     u64 thres1_policy)
{
	struct feature_fme_thermal *thermal;
	struct feature_fme_tmp_threshold tmp_threshold;

	thermal = get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_THERMAL_MGMT);

	spinlock_lock(&fme->lock);
	tmp_threshold.csr = readq(&thermal->threshold);

	if (thres1_policy == 0) {
		tmp_threshold.thshold_policy = 0;
	} else if (thres1_policy == 1) {
		tmp_threshold.thshold_policy = 1;
	} else {
		spinlock_unlock(&fme->lock);
		return -EINVAL;
	}

	writeq(tmp_threshold.csr, &thermal->threshold);
	spinlock_unlock(&fme->lock);

	return 0;
}

static int fme_thermal_get_temperature(struct ifpga_fme_hw *fme, u64 *temp)
{
	struct feature_fme_thermal *thermal;
	struct feature_fme_temp_rdsensor_fmt1 temp_rdsensor_fmt1;

	thermal = get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_THERMAL_MGMT);

	temp_rdsensor_fmt1.csr = readq(&thermal->rdsensor_fm1);
	*temp = temp_rdsensor_fmt1.fpga_temp;

	return 0;
}

static int fme_thermal_get_revision(struct ifpga_fme_hw *fme, u64 *revision)
{
	struct feature_fme_thermal *fme_thermal
		= get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_THERMAL_MGMT);
	struct feature_header header;

	header.csr = readq(&fme_thermal->header);
	*revision = header.revision;

	return 0;
}

#define FME_THERMAL_CAP_NO_TMP_THRESHOLD	0x1

static int fme_thermal_mgmt_init(struct feature *feature)
{
	struct feature_fme_thermal *fme_thermal;
	struct feature_fme_tmp_threshold_cap thermal_cap;

	UNUSED(feature);

	dev_info(NULL, "FME thermal mgmt Init.\n");

	fme_thermal = (struct feature_fme_thermal *)feature->addr;
	thermal_cap.csr = readq(&fme_thermal->threshold_cap);

	dev_info(NULL, "FME thermal cap %llx.\n",
		 (unsigned long long)fme_thermal->threshold_cap.csr);

	if (thermal_cap.tmp_thshold_disabled)
		feature->cap |= FME_THERMAL_CAP_NO_TMP_THRESHOLD;

	return 0;
}

static void fme_thermal_mgmt_uinit(struct feature *feature)
{
	UNUSED(feature);

	dev_info(NULL, "FME thermal mgmt UInit.\n");
}

static int
fme_thermal_set_prop(struct feature *feature, struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;

	if (feature->cap & FME_THERMAL_CAP_NO_TMP_THRESHOLD)
		return -ENOENT;

	switch (prop->prop_id) {
	case FME_THERMAL_PROP_THRESHOLD1:
		return fme_thermal_set_threshold1(fme, prop->data);
	case FME_THERMAL_PROP_THRESHOLD2:
		return fme_thermal_set_threshold2(fme, prop->data);
	case FME_THERMAL_PROP_THRESHOLD1_POLICY:
		return fme_thermal_set_threshold1_policy(fme, prop->data);
	}

	return -ENOENT;
}

static int
fme_thermal_get_prop(struct feature *feature, struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;

	if (feature->cap & FME_THERMAL_CAP_NO_TMP_THRESHOLD &&
	    prop->prop_id != FME_THERMAL_PROP_TEMPERATURE &&
	    prop->prop_id != FME_THERMAL_PROP_REVISION)
		return -ENOENT;

	switch (prop->prop_id) {
	case FME_THERMAL_PROP_THRESHOLD1:
		return fme_thermal_get_threshold1(fme, &prop->data);
	case FME_THERMAL_PROP_THRESHOLD2:
		return fme_thermal_get_threshold2(fme, &prop->data);
	case FME_THERMAL_PROP_THRESHOLD_TRIP:
		return fme_thermal_get_threshold_trip(fme, &prop->data);
	case FME_THERMAL_PROP_THRESHOLD1_REACHED:
		return fme_thermal_get_threshold1_reached(fme, &prop->data);
	case FME_THERMAL_PROP_THRESHOLD2_REACHED:
		return fme_thermal_get_threshold2_reached(fme, &prop->data);
	case FME_THERMAL_PROP_THRESHOLD1_POLICY:
		return fme_thermal_get_threshold1_policy(fme, &prop->data);
	case FME_THERMAL_PROP_TEMPERATURE:
		return fme_thermal_get_temperature(fme, &prop->data);
	case FME_THERMAL_PROP_REVISION:
		return fme_thermal_get_revision(fme, &prop->data);
	}

	return -ENOENT;
}

struct feature_ops fme_thermal_mgmt_ops = {
	.init = fme_thermal_mgmt_init,
	.uinit = fme_thermal_mgmt_uinit,
	.get_prop = fme_thermal_get_prop,
	.set_prop = fme_thermal_set_prop,
};

static int fme_pwr_get_consumed(struct ifpga_fme_hw *fme, u64 *consumed)
{
	struct feature_fme_power *fme_power
		= get_fme_feature_ioaddr_by_index(fme,
				FME_FEATURE_ID_POWER_MGMT);
	struct feature_fme_pm_status pm_status;

	pm_status.csr = readq(&fme_power->status);

	*consumed = pm_status.pwr_consumed;

	return 0;
}

static int fme_pwr_get_threshold1(struct ifpga_fme_hw *fme, u64 *threshold)
{
	struct feature_fme_power *fme_power
		= get_fme_feature_ioaddr_by_index(fme,
				FME_FEATURE_ID_POWER_MGMT);
	struct feature_fme_pm_ap_threshold pm_ap_threshold;

	pm_ap_threshold.csr = readq(&fme_power->threshold);

	*threshold = pm_ap_threshold.threshold1;

	return 0;
}

static int fme_pwr_set_threshold1(struct ifpga_fme_hw *fme, u64 threshold)
{
	struct feature_fme_power *fme_power
		= get_fme_feature_ioaddr_by_index(fme,
				FME_FEATURE_ID_POWER_MGMT);
	struct feature_fme_pm_ap_threshold pm_ap_threshold;

	spinlock_lock(&fme->lock);
	pm_ap_threshold.csr = readq(&fme_power->threshold);

	if (threshold <= PWR_THRESHOLD_MAX) {
		pm_ap_threshold.threshold1 = threshold;
	} else {
		spinlock_unlock(&fme->lock);
		return -EINVAL;
	}

	writeq(pm_ap_threshold.csr, &fme_power->threshold);
	spinlock_unlock(&fme->lock);

	return 0;
}

static int fme_pwr_get_threshold2(struct ifpga_fme_hw *fme, u64 *threshold)
{
	struct feature_fme_power *fme_power
		= get_fme_feature_ioaddr_by_index(fme,
				FME_FEATURE_ID_POWER_MGMT);
	struct feature_fme_pm_ap_threshold pm_ap_threshold;

	pm_ap_threshold.csr = readq(&fme_power->threshold);

	*threshold = pm_ap_threshold.threshold2;

	return 0;
}

static int fme_pwr_set_threshold2(struct ifpga_fme_hw *fme, u64 threshold)
{
	struct feature_fme_power *fme_power
		= get_fme_feature_ioaddr_by_index(fme,
				FME_FEATURE_ID_POWER_MGMT);
	struct feature_fme_pm_ap_threshold pm_ap_threshold;

	spinlock_lock(&fme->lock);
	pm_ap_threshold.csr = readq(&fme_power->threshold);

	if (threshold <= PWR_THRESHOLD_MAX) {
		pm_ap_threshold.threshold2 = threshold;
	} else {
		spinlock_unlock(&fme->lock);
		return -EINVAL;
	}

	writeq(pm_ap_threshold.csr, &fme_power->threshold);
	spinlock_unlock(&fme->lock);

	return 0;
}

static int fme_pwr_get_threshold1_status(struct ifpga_fme_hw *fme,
					 u64 *threshold_status)
{
	struct feature_fme_power *fme_power
		= get_fme_feature_ioaddr_by_index(fme,
				FME_FEATURE_ID_POWER_MGMT);
	struct feature_fme_pm_ap_threshold pm_ap_threshold;

	pm_ap_threshold.csr = readq(&fme_power->threshold);

	*threshold_status = pm_ap_threshold.threshold1_status;

	return 0;
}

static int fme_pwr_get_threshold2_status(struct ifpga_fme_hw *fme,
					 u64 *threshold_status)
{
	struct feature_fme_power *fme_power
		= get_fme_feature_ioaddr_by_index(fme,
				FME_FEATURE_ID_POWER_MGMT);
	struct feature_fme_pm_ap_threshold pm_ap_threshold;

	pm_ap_threshold.csr = readq(&fme_power->threshold);

	*threshold_status = pm_ap_threshold.threshold2_status;

	return 0;
}

static int fme_pwr_get_rtl(struct ifpga_fme_hw *fme, u64 *rtl)
{
	struct feature_fme_power *fme_power
		= get_fme_feature_ioaddr_by_index(fme,
				FME_FEATURE_ID_POWER_MGMT);
	struct feature_fme_pm_status pm_status;

	pm_status.csr = readq(&fme_power->status);

	*rtl = pm_status.fpga_latency_report;

	return 0;
}

static int fme_pwr_get_xeon_limit(struct ifpga_fme_hw *fme, u64 *limit)
{
	struct feature_fme_power *fme_power
		= get_fme_feature_ioaddr_by_index(fme,
				FME_FEATURE_ID_POWER_MGMT);
	struct feature_fme_pm_xeon_limit xeon_limit;

	xeon_limit.csr = readq(&fme_power->xeon_limit);

	if (!xeon_limit.enable)
		xeon_limit.pwr_limit = 0;

	*limit = xeon_limit.pwr_limit;

	return 0;
}

static int fme_pwr_get_fpga_limit(struct ifpga_fme_hw *fme, u64 *limit)
{
	struct feature_fme_power *fme_power
		= get_fme_feature_ioaddr_by_index(fme,
				FME_FEATURE_ID_POWER_MGMT);
	struct feature_fme_pm_fpga_limit fpga_limit;

	fpga_limit.csr = readq(&fme_power->fpga_limit);

	if (!fpga_limit.enable)
		fpga_limit.pwr_limit = 0;

	*limit = fpga_limit.pwr_limit;

	return 0;
}

static int fme_pwr_get_revision(struct ifpga_fme_hw *fme, u64 *revision)
{
	struct feature_fme_power *fme_power
		= get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_POWER_MGMT);
	struct feature_header header;

	header.csr = readq(&fme_power->header);
	*revision = header.revision;

	return 0;
}

static int fme_power_mgmt_init(struct feature *feature)
{
	UNUSED(feature);

	dev_info(NULL, "FME power mgmt Init.\n");

	return 0;
}

static void fme_power_mgmt_uinit(struct feature *feature)
{
	UNUSED(feature);

	dev_info(NULL, "FME power mgmt UInit.\n");
}

static int fme_power_mgmt_get_prop(struct feature *feature,
				   struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;

	switch (prop->prop_id) {
	case FME_PWR_PROP_CONSUMED:
		return fme_pwr_get_consumed(fme, &prop->data);
	case FME_PWR_PROP_THRESHOLD1:
		return fme_pwr_get_threshold1(fme, &prop->data);
	case FME_PWR_PROP_THRESHOLD2:
		return fme_pwr_get_threshold2(fme, &prop->data);
	case FME_PWR_PROP_THRESHOLD1_STATUS:
		return fme_pwr_get_threshold1_status(fme, &prop->data);
	case FME_PWR_PROP_THRESHOLD2_STATUS:
		return fme_pwr_get_threshold2_status(fme, &prop->data);
	case FME_PWR_PROP_RTL:
		return fme_pwr_get_rtl(fme, &prop->data);
	case FME_PWR_PROP_XEON_LIMIT:
		return fme_pwr_get_xeon_limit(fme, &prop->data);
	case FME_PWR_PROP_FPGA_LIMIT:
		return fme_pwr_get_fpga_limit(fme, &prop->data);
	case FME_PWR_PROP_REVISION:
		return fme_pwr_get_revision(fme, &prop->data);
	}

	return -ENOENT;
}

static int fme_power_mgmt_set_prop(struct feature *feature,
				   struct feature_prop *prop)
{
	struct ifpga_fme_hw *fme = feature->parent;

	switch (prop->prop_id) {
	case FME_PWR_PROP_THRESHOLD1:
		return fme_pwr_set_threshold1(fme, prop->data);
	case FME_PWR_PROP_THRESHOLD2:
		return fme_pwr_set_threshold2(fme, prop->data);
	}

	return -ENOENT;
}

struct feature_ops fme_power_mgmt_ops = {
	.init = fme_power_mgmt_init,
	.uinit = fme_power_mgmt_uinit,
	.get_prop = fme_power_mgmt_get_prop,
	.set_prop = fme_power_mgmt_set_prop,
};
