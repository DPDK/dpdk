/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include "ifpga_feature_dev.h"
#include "opae_spi.h"
#include "opae_intel_max10.h"
#include "opae_i2c.h"
#include "opae_at24_eeprom.h"

#define PWR_THRESHOLD_MAX       0x7F

int fme_get_prop(struct ifpga_fme_hw *fme, struct feature_prop *prop)
{
	struct ifpga_feature *feature;

	if (!fme)
		return -ENOENT;

	feature = get_fme_feature_by_id(fme, prop->feature_id);

	if (feature && feature->ops && feature->ops->get_prop)
		return feature->ops->get_prop(feature, prop);

	return -ENOENT;
}

int fme_set_prop(struct ifpga_fme_hw *fme, struct feature_prop *prop)
{
	struct ifpga_feature *feature;

	if (!fme)
		return -ENOENT;

	feature = get_fme_feature_by_id(fme, prop->feature_id);

	if (feature && feature->ops && feature->ops->set_prop)
		return feature->ops->set_prop(feature, prop);

	return -ENOENT;
}

int fme_set_irq(struct ifpga_fme_hw *fme, u32 feature_id, void *irq_set)
{
	struct ifpga_feature *feature;

	if (!fme)
		return -ENOENT;

	feature = get_fme_feature_by_id(fme, feature_id);

	if (feature && feature->ops && feature->ops->set_irq)
		return feature->ops->set_irq(feature, irq_set);

	return -ENOENT;
}

/* fme private feature head */
static int fme_hdr_init(struct ifpga_feature *feature)
{
	struct feature_fme_header *fme_hdr;

	fme_hdr = (struct feature_fme_header *)feature->addr;

	dev_info(NULL, "FME HDR Init.\n");
	dev_info(NULL, "FME cap %llx.\n",
		 (unsigned long long)fme_hdr->capability.csr);

	return 0;
}

static void fme_hdr_uinit(struct ifpga_feature *feature)
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
fme_hdr_get_prop(struct ifpga_feature *feature, struct feature_prop *prop)
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

struct ifpga_feature_ops fme_hdr_ops = {
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

static int fme_thermal_mgmt_init(struct ifpga_feature *feature)
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

static void fme_thermal_mgmt_uinit(struct ifpga_feature *feature)
{
	UNUSED(feature);

	dev_info(NULL, "FME thermal mgmt UInit.\n");
}

static int
fme_thermal_set_prop(struct ifpga_feature *feature, struct feature_prop *prop)
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
fme_thermal_get_prop(struct ifpga_feature *feature, struct feature_prop *prop)
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

struct ifpga_feature_ops fme_thermal_mgmt_ops = {
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

static int fme_power_mgmt_init(struct ifpga_feature *feature)
{
	UNUSED(feature);

	dev_info(NULL, "FME power mgmt Init.\n");

	return 0;
}

static void fme_power_mgmt_uinit(struct ifpga_feature *feature)
{
	UNUSED(feature);

	dev_info(NULL, "FME power mgmt UInit.\n");
}

static int fme_power_mgmt_get_prop(struct ifpga_feature *feature,
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

static int fme_power_mgmt_set_prop(struct ifpga_feature *feature,
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

struct ifpga_feature_ops fme_power_mgmt_ops = {
	.init = fme_power_mgmt_init,
	.uinit = fme_power_mgmt_uinit,
	.get_prop = fme_power_mgmt_get_prop,
	.set_prop = fme_power_mgmt_set_prop,
};

static int fme_hssi_eth_init(struct ifpga_feature *feature)
{
	UNUSED(feature);
	return 0;
}

static void fme_hssi_eth_uinit(struct ifpga_feature *feature)
{
	UNUSED(feature);
}

struct ifpga_feature_ops fme_hssi_eth_ops = {
	.init = fme_hssi_eth_init,
	.uinit = fme_hssi_eth_uinit,
};

static int fme_emif_init(struct ifpga_feature *feature)
{
	UNUSED(feature);
	return 0;
}

static void fme_emif_uinit(struct ifpga_feature *feature)
{
	UNUSED(feature);
}

struct ifpga_feature_ops fme_emif_ops = {
	.init = fme_emif_init,
	.uinit = fme_emif_uinit,
};

static const char *board_type_to_string(u32 type)
{
	switch (type) {
	case VC_8_10G:
		return "VC_8x10G";
	case VC_4_25G:
		return "VC_4x25G";
	case VC_2_1_25:
		return "VC_2x1x25G";
	case VC_4_25G_2_25G:
		return "VC_4x25G+2x25G";
	case VC_2_2_25G:
		return "VC_2x2x25G";
	}

	return "unknown";
}

static int board_type_to_info(u32 type,
		struct ifpga_fme_board_info *info)
{
	switch (type) {
	case VC_8_10G:
		info->nums_of_retimer = 2;
		info->ports_per_retimer = 4;
		info->nums_of_fvl = 2;
		info->ports_per_fvl = 4;
		break;
	case VC_4_25G:
		info->nums_of_retimer = 1;
		info->ports_per_retimer = 4;
		info->nums_of_fvl = 2;
		info->ports_per_fvl = 2;
		break;
	case VC_2_1_25:
		info->nums_of_retimer = 2;
		info->ports_per_retimer = 1;
		info->nums_of_fvl = 1;
		info->ports_per_fvl = 2;
		break;
	case VC_2_2_25G:
		info->nums_of_retimer = 2;
		info->ports_per_retimer = 2;
		info->nums_of_fvl = 2;
		info->ports_per_fvl = 2;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int fme_get_board_interface(struct ifpga_fme_hw *fme)
{
	struct fme_bitstream_id id;

	if (fme_hdr_get_bitstream_id(fme, &id.id))
		return -EINVAL;

	fme->board_info.type = id.interface;
	fme->board_info.build_hash = id.hash;
	fme->board_info.debug_version = id.debug;
	fme->board_info.major_version = id.major;
	fme->board_info.minor_version = id.minor;

	dev_info(fme, "board type: %s major_version:%u minor_version:%u build_hash:%u\n",
			board_type_to_string(fme->board_info.type),
			fme->board_info.major_version,
			fme->board_info.minor_version,
			fme->board_info.build_hash);

	if (board_type_to_info(fme->board_info.type, &fme->board_info))
		return -EINVAL;

	dev_info(fme, "get board info: nums_retimers %d ports_per_retimer %d nums_fvl %d ports_per_fvl %d\n",
			fme->board_info.nums_of_retimer,
			fme->board_info.ports_per_retimer,
			fme->board_info.nums_of_fvl,
			fme->board_info.ports_per_fvl);

	return 0;
}

static int spi_self_checking(void)
{
	u32 val;
	int ret;

	ret = max10_reg_read(0x30043c, &val);
	if (ret)
		return -EIO;

	if (val != 0x87654321) {
		dev_err(NULL, "Read MAX10 test register fail: 0x%x\n", val);
		return -EIO;
	}

	dev_info(NULL, "Read MAX10 test register success, SPI self-test done\n");

	return 0;
}

static int fme_spi_init(struct ifpga_feature *feature)
{
	struct ifpga_fme_hw *fme = (struct ifpga_fme_hw *)feature->parent;
	struct altera_spi_device *spi_master;
	struct intel_max10_device *max10;
	int ret = 0;

	dev_info(fme, "FME SPI Master (Max10) Init.\n");
	dev_debug(fme, "FME SPI base addr %p.\n",
			feature->addr);
	dev_debug(fme, "spi param=0x%llx\n",
			(unsigned long long)opae_readq(feature->addr + 0x8));

	spi_master = altera_spi_alloc(feature->addr, TYPE_SPI);
	if (!spi_master)
		return -ENODEV;

	altera_spi_init(spi_master);

	max10 = intel_max10_device_probe(spi_master, 0);
	if (!max10) {
		ret = -ENODEV;
		dev_err(fme, "max10 init fail\n");
		goto spi_fail;
	}

	fme->max10_dev = max10;

	/* SPI self test */
	if (spi_self_checking()) {
		ret = -EIO;
		goto max10_fail;
	}

	return ret;

max10_fail:
	intel_max10_device_remove(fme->max10_dev);
spi_fail:
	altera_spi_release(spi_master);
	return ret;
}

static void fme_spi_uinit(struct ifpga_feature *feature)
{
	struct ifpga_fme_hw *fme = (struct ifpga_fme_hw *)feature->parent;

	if (fme->max10_dev)
		intel_max10_device_remove(fme->max10_dev);
}

struct ifpga_feature_ops fme_spi_master_ops = {
	.init = fme_spi_init,
	.uinit = fme_spi_uinit,
};

static int nios_spi_wait_init_done(struct altera_spi_device *dev)
{
	u32 val = 0;
	unsigned long timeout = msecs_to_timer_cycles(10000);
	unsigned long ticks;

	do {
		if (spi_reg_read(dev, NIOS_SPI_INIT_DONE, &val))
			return -EIO;
		if (val)
			break;

		ticks = rte_get_timer_cycles();
		if (time_after(ticks, timeout))
			return -ETIMEDOUT;
		msleep(100);
	} while (!val);

	return 0;
}

static int nios_spi_check_error(struct altera_spi_device *dev)
{
	u32 value = 0;

	if (spi_reg_read(dev, NIOS_SPI_INIT_STS0, &value))
		return -EIO;

	dev_debug(dev, "SPI init status0 0x%x\n", value);

	/* Error code: 0xFFF0 to 0xFFFC */
	if (value >= 0xFFF0 && value <= 0xFFFC)
		return -EINVAL;

	value = 0;
	if (spi_reg_read(dev, NIOS_SPI_INIT_STS1, &value))
		return -EIO;

	dev_debug(dev, "SPI init status1 0x%x\n", value);

	/* Error code: 0xFFF0 to 0xFFFC */
	if (value >= 0xFFF0 && value <= 0xFFFC)
		return -EINVAL;

	return 0;
}

static int fme_nios_spi_init(struct ifpga_feature *feature)
{
	struct ifpga_fme_hw *fme = (struct ifpga_fme_hw *)feature->parent;
	struct altera_spi_device *spi_master;
	struct intel_max10_device *max10;
	int ret = 0;

	dev_info(fme, "FME SPI Master (NIOS) Init.\n");
	dev_debug(fme, "FME SPI base addr %p.\n",
			feature->addr);
	dev_debug(fme, "spi param=0x%llx\n",
			(unsigned long long)opae_readq(feature->addr + 0x8));

	spi_master = altera_spi_alloc(feature->addr, TYPE_NIOS_SPI);
	if (!spi_master)
		return -ENODEV;

	/**
	 * 1. wait A10 NIOS initial finished and
	 * release the SPI master to Host
	 */
	ret = nios_spi_wait_init_done(spi_master);
	if (ret != 0) {
		dev_err(fme, "FME NIOS_SPI init fail\n");
		goto release_dev;
	}

	dev_info(fme, "FME NIOS_SPI initial done\n");

	/* 2. check if error occur? */
	if (nios_spi_check_error(spi_master))
		dev_info(fme, "NIOS_SPI INIT done, but found some error\n");

	/* 3. init the spi master*/
	altera_spi_init(spi_master);

	/* init the max10 device */
	max10 = intel_max10_device_probe(spi_master, 0);
	if (!max10) {
		ret = -ENODEV;
		dev_err(fme, "max10 init fail\n");
		goto release_dev;
	}

	fme_get_board_interface(fme);

	fme->max10_dev = max10;

	/* SPI self test */
	if (spi_self_checking())
		goto spi_fail;

	return ret;

spi_fail:
	intel_max10_device_remove(fme->max10_dev);
release_dev:
	altera_spi_release(spi_master);
	return -ENODEV;
}

static void fme_nios_spi_uinit(struct ifpga_feature *feature)
{
	struct ifpga_fme_hw *fme = (struct ifpga_fme_hw *)feature->parent;

	if (fme->max10_dev)
		intel_max10_device_remove(fme->max10_dev);
}

struct ifpga_feature_ops fme_nios_spi_master_ops = {
	.init = fme_nios_spi_init,
	.uinit = fme_nios_spi_uinit,
};

static int i2c_mac_rom_test(struct altera_i2c_dev *dev)
{
	char buf[20];
	int ret;
	char read_buf[20] = {0,};
	const char *string = "1a2b3c4d5e";

	opae_memcpy(buf, string, strlen(string));

	ret = at24_eeprom_write(dev, AT24512_SLAVE_ADDR, 0,
			(u8 *)buf, strlen(string));
	if (ret < 0) {
		dev_err(NULL, "write i2c error:%d\n", ret);
		return ret;
	}

	ret = at24_eeprom_read(dev, AT24512_SLAVE_ADDR, 0,
			(u8 *)read_buf, strlen(string));
	if (ret < 0) {
		dev_err(NULL, "read i2c error:%d\n", ret);
		return ret;
	}

	if (memcmp(buf, read_buf, strlen(string))) {
		dev_err(NULL, "%s test fail!\n", __func__);
		return -EFAULT;
	}

	dev_info(NULL, "%s test successful\n", __func__);

	return 0;
}

static int fme_i2c_init(struct ifpga_feature *feature)
{
	struct feature_fme_i2c *i2c;
	struct ifpga_fme_hw *fme = (struct ifpga_fme_hw *)feature->parent;

	i2c = (struct feature_fme_i2c *)feature->addr;

	dev_info(NULL, "FME I2C Master Init.\n");

	fme->i2c_master = altera_i2c_probe(i2c);
	if (!fme->i2c_master)
		return -ENODEV;

	/* MAC ROM self test */
	i2c_mac_rom_test(fme->i2c_master);

	return 0;
}

static void fme_i2c_uninit(struct ifpga_feature *feature)
{
	struct ifpga_fme_hw *fme = (struct ifpga_fme_hw *)feature->parent;

	altera_i2c_remove(fme->i2c_master);
}

struct ifpga_feature_ops fme_i2c_master_ops = {
	.init = fme_i2c_init,
	.uinit = fme_i2c_uninit,
};

static int fme_eth_group_init(struct ifpga_feature *feature)
{
	struct ifpga_fme_hw *fme = (struct ifpga_fme_hw *)feature->parent;
	struct eth_group_device *dev;

	dev = (struct eth_group_device *)eth_group_probe(feature->addr);
	if (!dev)
		return -ENODEV;

	fme->eth_dev[dev->group_id] = dev;

	fme->eth_group_region[dev->group_id].addr =
		feature->addr;
	fme->eth_group_region[dev->group_id].phys_addr =
		feature->phys_addr;
	fme->eth_group_region[dev->group_id].len =
		feature->size;

	fme->nums_eth_dev++;

	dev_info(NULL, "FME PHY Group %d Init.\n", dev->group_id);
	dev_info(NULL, "found %d eth group, addr %p phys_addr 0x%llx len %u\n",
			dev->group_id, feature->addr,
			(unsigned long long)feature->phys_addr,
			feature->size);

	return 0;
}

static void fme_eth_group_uinit(struct ifpga_feature *feature)
{
	UNUSED(feature);
}

struct ifpga_feature_ops fme_eth_group_ops = {
	.init = fme_eth_group_init,
	.uinit = fme_eth_group_uinit,
};

int fme_mgr_read_mac_rom(struct ifpga_fme_hw *fme, int offset,
		void *buf, int size)
{
	struct altera_i2c_dev *dev;

	dev = fme->i2c_master;
	if (!dev)
		return -ENODEV;

	return at24_eeprom_read(dev, AT24512_SLAVE_ADDR, offset, buf, size);
}

int fme_mgr_write_mac_rom(struct ifpga_fme_hw *fme, int offset,
		void *buf, int size)
{
	struct altera_i2c_dev *dev;

	dev = fme->i2c_master;
	if (!dev)
		return -ENODEV;

	return at24_eeprom_write(dev, AT24512_SLAVE_ADDR, offset, buf, size);
}

static struct eth_group_device *get_eth_group_dev(struct ifpga_fme_hw *fme,
		u8 group_id)
{
	struct eth_group_device *dev;

	if (group_id > (MAX_ETH_GROUP_DEVICES - 1))
		return NULL;

	dev = (struct eth_group_device *)fme->eth_dev[group_id];
	if (!dev)
		return NULL;

	if (dev->status != ETH_GROUP_DEV_ATTACHED)
		return NULL;

	return dev;
}

int fme_mgr_get_eth_group_nums(struct ifpga_fme_hw *fme)
{
	return fme->nums_eth_dev;
}

int fme_mgr_get_eth_group_info(struct ifpga_fme_hw *fme,
		u8 group_id, struct opae_eth_group_info *info)
{
	struct eth_group_device *dev;

	dev = get_eth_group_dev(fme, group_id);
	if (!dev)
		return -ENODEV;

	info->group_id = group_id;
	info->speed = dev->speed;
	info->nums_of_mac = dev->mac_num;
	info->nums_of_phy = dev->phy_num;

	return 0;
}

int fme_mgr_eth_group_read_reg(struct ifpga_fme_hw *fme, u8 group_id,
		u8 type, u8 index, u16 addr, u32 *data)
{
	struct eth_group_device *dev;

	dev = get_eth_group_dev(fme, group_id);
	if (!dev)
		return -ENODEV;

	return eth_group_read_reg(dev, type, index, addr, data);
}

int fme_mgr_eth_group_write_reg(struct ifpga_fme_hw *fme, u8 group_id,
		u8 type, u8 index, u16 addr, u32 data)
{
	struct eth_group_device *dev;

	dev = get_eth_group_dev(fme, group_id);
	if (!dev)
		return -ENODEV;

	return eth_group_write_reg(dev, type, index, addr, data);
}

static int fme_get_eth_group_speed(struct ifpga_fme_hw *fme,
		u8 group_id)
{
	struct eth_group_device *dev;

	dev = get_eth_group_dev(fme, group_id);
	if (!dev)
		return -ENODEV;

	return dev->speed;
}

int fme_mgr_get_retimer_info(struct ifpga_fme_hw *fme,
		struct opae_retimer_info *info)
{
	struct intel_max10_device *dev;

	dev = (struct intel_max10_device *)fme->max10_dev;
	if (!dev)
		return -ENODEV;

	info->nums_retimer = fme->board_info.nums_of_retimer;
	info->ports_per_retimer = fme->board_info.ports_per_retimer;
	info->nums_fvl = fme->board_info.nums_of_fvl;
	info->ports_per_fvl = fme->board_info.ports_per_fvl;

	/* The speed of PKVL is identical the eth group's speed */
	info->support_speed = fme_get_eth_group_speed(fme,
			LINE_SIDE_GROUP_ID);

	return 0;
}

int fme_mgr_get_retimer_status(struct ifpga_fme_hw *fme,
		struct opae_retimer_status *status)
{
	struct intel_max10_device *dev;
	unsigned int val;

	dev = (struct intel_max10_device *)fme->max10_dev;
	if (!dev)
		return -ENODEV;

	if (max10_reg_read(PKVL_LINK_STATUS, &val)) {
		dev_err(dev, "%s: read pkvl status fail\n", __func__);
		return -EINVAL;
	}

	/* The speed of PKVL is identical the eth group's speed */
	status->speed = fme_get_eth_group_speed(fme,
			LINE_SIDE_GROUP_ID);

	status->line_link_bitmap = val;

	dev_debug(dev, "get retimer status: speed:%d. line_link_bitmap:0x%x\n",
			status->speed,
			status->line_link_bitmap);

	return 0;
}
