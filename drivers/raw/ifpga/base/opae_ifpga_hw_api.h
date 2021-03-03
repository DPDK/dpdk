/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _OPAE_IFPGA_HW_API_H_
#define _OPAE_IFPGA_HW_API_H_

#include "opae_hw_api.h"

/**
 * struct feature_prop - data structure for feature property
 * @feature_id: id of this feature.
 * @prop_id: id of this property under this feature.
 * @data: property value to set/get.
 */
struct feature_prop {
	u64 feature_id;
	u64 prop_id;
	u64 data;
};

#define IFPGA_FIU_ID_FME	0x0
#define IFPGA_FIU_ID_PORT	0x1

#define IFPGA_FME_FEATURE_ID_HEADER		0x0
#define IFPGA_FME_FEATURE_ID_THERMAL_MGMT	0x1
#define IFPGA_FME_FEATURE_ID_POWER_MGMT		0x2
#define IFPGA_FME_FEATURE_ID_GLOBAL_IPERF	0x3
#define IFPGA_FME_FEATURE_ID_GLOBAL_ERR		0x4
#define IFPGA_FME_FEATURE_ID_PR_MGMT		0x5
#define IFPGA_FME_FEATURE_ID_HSSI		0x6
#define IFPGA_FME_FEATURE_ID_GLOBAL_DPERF	0x7

#define IFPGA_PORT_FEATURE_ID_HEADER		0x0
#define IFPGA_PORT_FEATURE_ID_AFU		0xff
#define IFPGA_PORT_FEATURE_ID_ERROR		0x10
#define IFPGA_PORT_FEATURE_ID_UMSG		0x11
#define IFPGA_PORT_FEATURE_ID_UINT		0x12
#define IFPGA_PORT_FEATURE_ID_STP		0x13

/*
 * PROP format (TOP + SUB + ID)
 *
 * (~0x0) means this field is unused.
 */
#define PROP_TOP	GENMASK(31, 24)
#define PROP_TOP_UNUSED	0xff
#define PROP_SUB	GENMASK(23, 16)
#define PROP_SUB_UNUSED	0xff
#define PROP_ID		GENMASK(15, 0)

#define PROP(_top, _sub, _id) \
	(SET_FIELD(PROP_TOP, _top) | SET_FIELD(PROP_SUB, _sub) |\
	 SET_FIELD(PROP_ID, _id))

/* FME head feature's properties*/
#define FME_HDR_PROP_REVISION		0x1	/* RDONLY */
#define FME_HDR_PROP_PORTS_NUM		0x2	/* RDONLY */
#define FME_HDR_PROP_CACHE_SIZE		0x3	/* RDONLY */
#define FME_HDR_PROP_VERSION			0x4	/* RDONLY */
#define FME_HDR_PROP_SOCKET_ID		0x5	/* RDONLY */
#define FME_HDR_PROP_BITSTREAM_ID		0x6	/* RDONLY */
#define FME_HDR_PROP_BITSTREAM_METADATA	0x7	/* RDONLY */
#define FME_HDR_PROP_PORT_TYPE		0x8	/* RDWR */

/* FME error reporting feature's properties */
/* FME error reporting properties format */
#define ERR_PROP(_top, _id)		PROP(_top, 0xff, _id)
#define ERR_PROP_TOP_UNUSED		PROP_TOP_UNUSED
#define ERR_PROP_TOP_FME_ERR		0x1
#define ERR_PROP_ROOT(_id)		ERR_PROP(0xff, _id)
#define ERR_PROP_FME_ERR(_id)		ERR_PROP(ERR_PROP_TOP_FME_ERR, _id)

#define FME_ERR_PROP_ERRORS		ERR_PROP_FME_ERR(0x1)
#define FME_ERR_PROP_FIRST_ERROR	ERR_PROP_FME_ERR(0x2)
#define FME_ERR_PROP_NEXT_ERROR		ERR_PROP_FME_ERR(0x3)
#define FME_ERR_PROP_CLEAR		ERR_PROP_FME_ERR(0x4)	/* WO */
#define FME_ERR_PROP_SEU_EMR_LOW        ERR_PROP_FME_ERR(0x5)
#define FME_ERR_PROP_SEU_EMR_HIGH       ERR_PROP_FME_ERR(0x6)
#define FME_ERR_PROP_REVISION		ERR_PROP_ROOT(0x5)
#define FME_ERR_PROP_PCIE0_ERRORS	ERR_PROP_ROOT(0x6)	/* RW */
#define FME_ERR_PROP_PCIE1_ERRORS	ERR_PROP_ROOT(0x7)	/* RW */
#define FME_ERR_PROP_NONFATAL_ERRORS	ERR_PROP_ROOT(0x8)
#define FME_ERR_PROP_CATFATAL_ERRORS	ERR_PROP_ROOT(0x9)
#define FME_ERR_PROP_INJECT_ERRORS	ERR_PROP_ROOT(0xa)	/* RW */

/* FME thermal feature's properties */
#define FME_THERMAL_PROP_THRESHOLD1		0x1	/* RW */
#define FME_THERMAL_PROP_THRESHOLD2		0x2	/* RW */
#define FME_THERMAL_PROP_THRESHOLD_TRIP		0x3	/* RDONLY */
#define FME_THERMAL_PROP_THRESHOLD1_REACHED	0x4	/* RDONLY */
#define FME_THERMAL_PROP_THRESHOLD2_REACHED	0x5	/* RDONLY */
#define FME_THERMAL_PROP_THRESHOLD1_POLICY	0x6	/* RW */
#define FME_THERMAL_PROP_TEMPERATURE		0x7	/* RDONLY */
#define FME_THERMAL_PROP_REVISION		0x8	/* RDONLY */

/* FME power feature's properties */
#define FME_PWR_PROP_CONSUMED			0x1	/* RDONLY */
#define FME_PWR_PROP_THRESHOLD1			0x2	/* RW */
#define FME_PWR_PROP_THRESHOLD2			0x3	/* RW */
#define FME_PWR_PROP_THRESHOLD1_STATUS		0x4	/* RDONLY */
#define FME_PWR_PROP_THRESHOLD2_STATUS		0x5	/* RDONLY */
#define FME_PWR_PROP_RTL			0x6	/* RDONLY */
#define FME_PWR_PROP_XEON_LIMIT			0x7	/* RDONLY */
#define FME_PWR_PROP_FPGA_LIMIT			0x8	/* RDONLY */
#define FME_PWR_PROP_REVISION			0x9	/* RDONLY */

/* FME iperf/dperf PROP format */
#define PERF_PROP_TOP_CACHE			0x1
#define PERF_PROP_TOP_VTD			0x2
#define PERF_PROP_TOP_FAB			0x3
#define PERF_PROP_TOP_UNUSED			PROP_TOP_UNUSED
#define PERF_PROP_SUB_UNUSED			PROP_SUB_UNUSED

#define PERF_PROP_ROOT(_id)		PROP(0xff, 0xff, _id)
#define PERF_PROP_CACHE(_id)		PROP(PERF_PROP_TOP_CACHE, 0xff, _id)
#define PERF_PROP_VTD(_sub, _id)	PROP(PERF_PROP_TOP_VTD, _sub, _id)
#define PERF_PROP_VTD_ROOT(_id)		PROP(PERF_PROP_TOP_VTD, 0xff, _id)
#define PERF_PROP_FAB(_sub, _id)	PROP(PERF_PROP_TOP_FAB, _sub, _id)
#define PERF_PROP_FAB_ROOT(_id)		PROP(PERF_PROP_TOP_FAB, 0xff, _id)

/* FME iperf feature's properties */
#define FME_IPERF_PROP_CLOCK			PERF_PROP_ROOT(0x1)
#define FME_IPERF_PROP_REVISION			PERF_PROP_ROOT(0x2)

/* iperf CACHE properties */
#define FME_IPERF_PROP_CACHE_FREEZE		PERF_PROP_CACHE(0x1) /* RW */
#define FME_IPERF_PROP_CACHE_READ_HIT		PERF_PROP_CACHE(0x2)
#define FME_IPERF_PROP_CACHE_READ_MISS		PERF_PROP_CACHE(0x3)
#define FME_IPERF_PROP_CACHE_WRITE_HIT		PERF_PROP_CACHE(0x4)
#define FME_IPERF_PROP_CACHE_WRITE_MISS		PERF_PROP_CACHE(0x5)
#define FME_IPERF_PROP_CACHE_HOLD_REQUEST	PERF_PROP_CACHE(0x6)
#define FME_IPERF_PROP_CACHE_TX_REQ_STALL	PERF_PROP_CACHE(0x7)
#define FME_IPERF_PROP_CACHE_RX_REQ_STALL	PERF_PROP_CACHE(0x8)
#define FME_IPERF_PROP_CACHE_RX_EVICTION	PERF_PROP_CACHE(0x9)
#define FME_IPERF_PROP_CACHE_DATA_WRITE_PORT_CONTENTION	PERF_PROP_CACHE(0xa)
#define FME_IPERF_PROP_CACHE_TAG_WRITE_PORT_CONTENTION	PERF_PROP_CACHE(0xb)
/* iperf VTD properties */
#define FME_IPERF_PROP_VTD_FREEZE		PERF_PROP_VTD_ROOT(0x1) /* RW */
#define FME_IPERF_PROP_VTD_SIP_IOTLB_4K_HIT	PERF_PROP_VTD_ROOT(0x2)
#define FME_IPERF_PROP_VTD_SIP_IOTLB_2M_HIT	PERF_PROP_VTD_ROOT(0x3)
#define FME_IPERF_PROP_VTD_SIP_IOTLB_1G_HIT	PERF_PROP_VTD_ROOT(0x4)
#define FME_IPERF_PROP_VTD_SIP_SLPWC_L3_HIT	PERF_PROP_VTD_ROOT(0x5)
#define FME_IPERF_PROP_VTD_SIP_SLPWC_L4_HIT	PERF_PROP_VTD_ROOT(0x6)
#define FME_IPERF_PROP_VTD_SIP_RCC_HIT		PERF_PROP_VTD_ROOT(0x7)
#define FME_IPERF_PROP_VTD_SIP_IOTLB_4K_MISS	PERF_PROP_VTD_ROOT(0x8)
#define FME_IPERF_PROP_VTD_SIP_IOTLB_2M_MISS	PERF_PROP_VTD_ROOT(0x9)
#define FME_IPERF_PROP_VTD_SIP_IOTLB_1G_MISS	PERF_PROP_VTD_ROOT(0xa)
#define FME_IPERF_PROP_VTD_SIP_SLPWC_L3_MISS	PERF_PROP_VTD_ROOT(0xb)
#define FME_IPERF_PROP_VTD_SIP_SLPWC_L4_MISS	PERF_PROP_VTD_ROOT(0xc)
#define FME_IPERF_PROP_VTD_SIP_RCC_MISS		PERF_PROP_VTD_ROOT(0xd)
#define FME_IPERF_PROP_VTD_PORT_READ_TRANSACTION(n)	PERF_PROP_VTD(n, 0xe)
#define FME_IPERF_PROP_VTD_PORT_WRITE_TRANSACTION(n)	PERF_PROP_VTD(n, 0xf)
#define FME_IPERF_PROP_VTD_PORT_DEVTLB_READ_HIT(n)	PERF_PROP_VTD(n, 0x10)
#define FME_IPERF_PROP_VTD_PORT_DEVTLB_WRITE_HIT(n)	PERF_PROP_VTD(n, 0x11)
#define FME_IPERF_PROP_VTD_PORT_DEVTLB_4K_FILL(n)	PERF_PROP_VTD(n, 0x12)
#define FME_IPERF_PROP_VTD_PORT_DEVTLB_2M_FILL(n)	PERF_PROP_VTD(n, 0x13)
#define FME_IPERF_PROP_VTD_PORT_DEVTLB_1G_FILL(n)	PERF_PROP_VTD(n, 0x14)
/* iperf FAB properties */
#define FME_IPERF_PROP_FAB_FREEZE		PERF_PROP_FAB_ROOT(0x1) /* RW */
#define FME_IPERF_PROP_FAB_PCIE0_READ		PERF_PROP_FAB_ROOT(0x2)
#define FME_IPERF_PROP_FAB_PORT_PCIE0_READ(n)	PERF_PROP_FAB(n, 0x2)
#define FME_IPERF_PROP_FAB_PCIE0_WRITE		PERF_PROP_FAB_ROOT(0x3)
#define FME_IPERF_PROP_FAB_PORT_PCIE0_WRITE(n)	PERF_PROP_FAB(n, 0x3)
#define FME_IPERF_PROP_FAB_PCIE1_READ		PERF_PROP_FAB_ROOT(0x4)
#define FME_IPERF_PROP_FAB_PORT_PCIE1_READ(n)	PERF_PROP_FAB(n, 0x4)
#define FME_IPERF_PROP_FAB_PCIE1_WRITE		PERF_PROP_FAB_ROOT(0x5)
#define FME_IPERF_PROP_FAB_PORT_PCIE1_WRITE(n)	PERF_PROP_FAB(n, 0x5)
#define FME_IPERF_PROP_FAB_UPI_READ		PERF_PROP_FAB_ROOT(0x6)
#define FME_IPERF_PROP_FAB_PORT_UPI_READ(n)	PERF_PROP_FAB(n, 0x6)
#define FME_IPERF_PROP_FAB_UPI_WRITE		PERF_PROP_FAB_ROOT(0x7)
#define FME_IPERF_PROP_FAB_PORT_UPI_WRITE(n)	PERF_PROP_FAB(n, 0x7)
#define FME_IPERF_PROP_FAB_MMIO_READ		PERF_PROP_FAB_ROOT(0x8)
#define FME_IPERF_PROP_FAB_PORT_MMIO_READ(n)	PERF_PROP_FAB(n, 0x8)
#define FME_IPERF_PROP_FAB_MMIO_WRITE		PERF_PROP_FAB_ROOT(0x9)
#define FME_IPERF_PROP_FAB_PORT_MMIO_WRITE(n)	PERF_PROP_FAB(n, 0x9)
#define FME_IPERF_PROP_FAB_ENABLE		PERF_PROP_FAB_ROOT(0xa) /* RW */
#define FME_IPERF_PROP_FAB_PORT_ENABLE(n)	PERF_PROP_FAB(n, 0xa)   /* RW */

/* FME dperf properties */
#define FME_DPERF_PROP_CLOCK			PERF_PROP_ROOT(0x1)
#define FME_DPERF_PROP_REVISION			PERF_PROP_ROOT(0x2)

/* dperf FAB properties */
#define FME_DPERF_PROP_FAB_FREEZE		PERF_PROP_FAB_ROOT(0x1) /* RW */
#define FME_DPERF_PROP_FAB_PCIE0_READ		PERF_PROP_FAB_ROOT(0x2)
#define FME_DPERF_PROP_FAB_PORT_PCIE0_READ(n)	PERF_PROP_FAB(n, 0x2)
#define FME_DPERF_PROP_FAB_PCIE0_WRITE		PERF_PROP_FAB_ROOT(0x3)
#define FME_DPERF_PROP_FAB_PORT_PCIE0_WRITE(n)	PERF_PROP_FAB(n, 0x3)
#define FME_DPERF_PROP_FAB_MMIO_READ		PERF_PROP_FAB_ROOT(0x4)
#define FME_DPERF_PROP_FAB_PORT_MMIO_READ(n)	PERF_PROP_FAB(n, 0x4)
#define FME_DPERF_PROP_FAB_MMIO_WRITE		PERF_PROP_FAB_ROOT(0x5)
#define FME_DPERF_PROP_FAB_PORT_MMIO_WRITE(n)	PERF_PROP_FAB(n, 0x5)
#define FME_DPERF_PROP_FAB_ENABLE		PERF_PROP_FAB_ROOT(0x6) /* RW */
#define FME_DPERF_PROP_FAB_PORT_ENABLE(n)	PERF_PROP_FAB(n, 0x6)   /* RW */

/*PORT hdr feature's properties*/
#define PORT_HDR_PROP_REVISION			0x1	/* RDONLY */
#define PORT_HDR_PROP_PORTIDX			0x2	/* RDONLY */
#define PORT_HDR_PROP_LATENCY_TOLERANCE		0x3	/* RDONLY */
#define PORT_HDR_PROP_AP1_EVENT			0x4	/* RW */
#define PORT_HDR_PROP_AP2_EVENT			0x5	/* RW */
#define PORT_HDR_PROP_POWER_STATE		0x6	/* RDONLY */
#define PORT_HDR_PROP_USERCLK_FREQCMD		0x7	/* RW */
#define PORT_HDR_PROP_USERCLK_FREQCNTRCMD	0x8	/* RW */
#define PORT_HDR_PROP_USERCLK_FREQSTS		0x9	/* RDONLY */
#define PORT_HDR_PROP_USERCLK_CNTRSTS		0xa	/* RDONLY */

/*PORT error feature's properties*/
#define PORT_ERR_PROP_REVISION			0x1	/* RDONLY */
#define PORT_ERR_PROP_ERRORS			0x2	/* RDONLY */
#define PORT_ERR_PROP_FIRST_ERROR		0x3	/* RDONLY */
#define PORT_ERR_PROP_FIRST_MALFORMED_REQ_LSB	0x4	/* RDONLY */
#define PORT_ERR_PROP_FIRST_MALFORMED_REQ_MSB	0x5	/* RDONLY */
#define PORT_ERR_PROP_CLEAR			0x6	/* WRONLY */

int opae_manager_ifpga_get_prop(struct opae_manager *mgr,
				struct feature_prop *prop);
int opae_manager_ifpga_set_prop(struct opae_manager *mgr,
				struct feature_prop *prop);
int opae_bridge_ifpga_get_prop(struct opae_bridge *br,
			       struct feature_prop *prop);
int opae_bridge_ifpga_set_prop(struct opae_bridge *br,
			       struct feature_prop *prop);

/*
 * Retrieve information about the fpga fme.
 * Driver fills the info in provided struct fpga_fme_info.
 */
struct fpga_fme_info {
	u32 capability;		/* The capability of FME device */
#define FPGA_FME_CAP_ERR_IRQ	(1 << 0) /* Support fme error interrupt */
};

int opae_manager_ifpga_get_info(struct opae_manager *mgr,
				struct fpga_fme_info *fme_info);

/* Set eventfd information for ifpga FME error interrupt */
struct fpga_fme_err_irq_set {
	s32 evtfd;		/* Eventfd handler */
};

int opae_manager_ifpga_set_err_irq(struct opae_manager *mgr,
				   struct fpga_fme_err_irq_set *err_irq_set);

/*
 * Retrieve information about the fpga port.
 * Driver fills the info in provided struct fpga_port_info.
 */
struct fpga_port_info {
	u32 capability;	/* The capability of port device */
#define FPGA_PORT_CAP_ERR_IRQ	(1 << 0) /* Support port error interrupt */
#define FPGA_PORT_CAP_UAFU_IRQ	(1 << 1) /* Support uafu error interrupt */
	u32 num_umsgs;	/* The number of allocated umsgs */
	u32 num_uafu_irqs;	/* The number of uafu interrupts */
};

int opae_bridge_ifpga_get_info(struct opae_bridge *br,
			       struct fpga_port_info *port_info);
/*
 * Retrieve region information about the fpga port.
 * Driver needs to fill the index of struct fpga_port_region_info.
 */
struct fpga_port_region_info {
	u32 index;
#define PORT_REGION_INDEX_STP	(1 << 1)	/* Signal Tap Region */
	u64 size;	/* Region Size */
	u8 *addr;	/* Base address of the region */
};

int opae_bridge_ifpga_get_region_info(struct opae_bridge *br,
				      struct fpga_port_region_info *info);

/* Set eventfd information for ifpga port error interrupt */
struct fpga_port_err_irq_set {
	s32 evtfd;		/* Eventfd handler */
};

int opae_bridge_ifpga_set_err_irq(struct opae_bridge *br,
				  struct fpga_port_err_irq_set *err_irq_set);

#endif /* _OPAE_IFPGA_HW_API_H_ */
