/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _IFPGA_DEFINES_H_
#define _IFPGA_DEFINES_H_

#include "ifpga_compat.h"

#define MAX_FPGA_PORT_NUM  4

#define FME_FEATURE_HEADER          "fme_hdr"
#define FME_FEATURE_THERMAL_MGMT    "fme_thermal"
#define FME_FEATURE_POWER_MGMT      "fme_power"
#define FME_FEATURE_GLOBAL_IPERF    "fme_iperf"
#define FME_FEATURE_GLOBAL_ERR      "fme_error"
#define FME_FEATURE_PR_MGMT         "fme_pr"
#define FME_FEATURE_EMIF_MGMT       "fme_emif"
#define FME_FEATURE_HSSI_ETH        "fme_hssi"
#define FME_FEATURE_GLOBAL_DPERF    "fme_dperf"
#define FME_FEATURE_QSPI_FLASH	    "fme_qspi_flash"
#define FME_FEATURE_MAX10_SPI       "fme_max10_spi"
#define FME_FEATURE_NIOS_SPI        "fme_nios_spi"
#define FME_FEATURE_I2C_MASTER      "fme_i2c_master"
#define FME_FEATURE_ETH_GROUP       "fme_eth_group"

#define PORT_FEATURE_HEADER         "port_hdr"
#define PORT_FEATURE_UAFU           "port_uafu"
#define PORT_FEATURE_ERR            "port_err"
#define PORT_FEATURE_UMSG           "port_umsg"
#define PORT_FEATURE_PR             "port_pr"
#define PORT_FEATURE_UINT           "port_uint"
#define PORT_FEATURE_STP            "port_stp"

/*
 * do not check the revision id as id may be dynamic under
 * some cases, e.g, UAFU.
 */
#define SKIP_REVISION_CHECK		0xff

#define FME_HEADER_REVISION		1
#define FME_THERMAL_MGMT_REVISION	0
#define FME_POWER_MGMT_REVISION		1
#define FME_GLOBAL_IPERF_REVISION	1
#define FME_GLOBAL_ERR_REVISION		1
#define FME_PR_MGMT_REVISION		2
#define FME_HSSI_ETH_REVISION		0
#define FME_GLOBAL_DPERF_REVISION	0
#define FME_QSPI_REVISION		0
#define FME_MAX10_SPI                   0
#define FME_I2C_MASTER                  0

#define PORT_HEADER_REVISION		0
/* UAFU's header info depends on the downloaded GBS */
#define PORT_UAFU_REVISION		SKIP_REVISION_CHECK
#define PORT_ERR_REVISION		1
#define PORT_UMSG_REVISION		0
#define PORT_UINT_REVISION		0
#define PORT_STP_REVISION		1

#define FEATURE_TYPE_AFU	0x1
#define FEATURE_TYPE_BBB        0x2
#define FEATURE_TYPE_PRIVATE	0x3
#define FEATURE_TYPE_FIU	0x4

#define FEATURE_FIU_ID_FME	0x0
#define FEATURE_FIU_ID_PORT	0x1

/* Reserved 0xfe for Header, 0xff for AFU*/
#define FEATURE_ID_FIU_HEADER	0xfe
#define FEATURE_ID_AFU		0xff

enum fpga_id_type {
	FME_ID,
	PORT_ID,
	FPGA_ID_MAX,
};

#define FME_FEATURE_ID_HEADER FEATURE_ID_FIU_HEADER
#define FME_FEATURE_ID_THERMAL_MGMT 0x1
#define FME_FEATURE_ID_POWER_MGMT 0x2
#define FME_FEATURE_ID_GLOBAL_IPERF 0x3
#define FME_FEATURE_ID_GLOBAL_ERR 0x4
#define FME_FEATURE_ID_PR_MGMT 0x5
#define FME_FEATURE_ID_HSSI_ETH 0x6
#define FME_FEATURE_ID_GLOBAL_DPERF 0x7
#define FME_FEATURE_ID_QSPI_FLASH 0x8
#define FME_FEATURE_ID_EMIF_MGMT  0x9
#define FME_FEATURE_ID_MAX10_SPI  0xe
#define FME_FEATURE_ID_NIOS_SPI 0xd
#define FME_FEATURE_ID_I2C_MASTER  0xf
#define FME_FEATURE_ID_ETH_GROUP 0x10

#define PORT_FEATURE_ID_HEADER FEATURE_ID_FIU_HEADER
#define PORT_FEATURE_ID_ERROR 0x10
#define PORT_FEATURE_ID_UMSG 0x11
#define PORT_FEATURE_ID_UINT 0x12
#define PORT_FEATURE_ID_STP 0x13
#define PORT_FEATURE_ID_UAFU FEATURE_ID_AFU

/*
 * All headers and structures must be byte-packed to match the spec.
 */
#pragma pack(push, 1)

struct feature_header {
	union {
		u64 csr;
		struct {
			u16 id:12;
			u8  revision:4;
			u32 next_header_offset:24;
			u8  end_of_list:1;
			u32 reserved:19;
			u8  type:4;
		};
	};
};

struct feature_bbb_header {
	struct uuid guid;
};

struct feature_afu_header {
	struct uuid guid;
	union {
		u64 csr;
		struct {
			u64 next_afu:24;
			u64 reserved:40;
		};
	};
};

struct feature_fiu_header {
	struct uuid guid;
	union {
		u64 csr;
		struct {
			u64 next_afu:24;
			u64 reserved:40;
		};
	};
};

struct feature_fme_capability {
	union {
		u64 csr;
		struct {
			u8  fabric_verid;	/* Fabric version ID */
			u8  socket_id:1;	/* Socket id */
			u8  rsvd1:3;		/* Reserved */
			/* pci0 link available yes /no */
			u8  pci0_link_avile:1;
			/* pci1 link available yes /no */
			u8  pci1_link_avile:1;
			/* Coherent (QPI/UPI) link available yes /no */
			u8  qpi_link_avile:1;
			u8  rsvd2:1;		/* Reserved */
			/* IOMMU or VT-d supported  yes/no */
			u8  iommu_support:1;
			u8  num_ports:3;	/* Number of ports */
			u8  sf_fab_ctl:1;	/* Internal validation bit */
			u8  rsvd3:3;		/* Reserved */
			/*
			 * Address width supported in bits
			 * BXT -0x26 , SKX -0x30
			 */
			u8  address_width_bits:6;
			u8  rsvd4:2;		/* Reserved */
			/* Size of cache supported in kb */
			u16 cache_size:12;
			u8  cache_assoc:4;	/* Cache Associativity */
			u16 rsvd5:15;		/* Reserved */
			u8  lock_bit:1;		/* Lock bit */
		};
	};
};

#define FME_AFU_ACCESS_PF		0
#define FME_AFU_ACCESS_VF		1

struct feature_fme_port {
	union {
		u64 csr;
		struct {
			u32 port_offset:24;
			u8  reserved1;
			u8  port_bar:3;
			u32 reserved2:20;
			u8  afu_access_control:1;
			u8  reserved3:4;
			u8  port_implemented:1;
			u8  reserved4:3;
		};
	};
};

struct feature_fme_fab_status {
	union {
		u64 csr;
		struct {
			u8  upilink_status:4;   /* UPI Link Status */
			u8  rsvd1:4;		/* Reserved */
			u8  pci0link_status:1;  /* pci0 link status */
			u8  rsvd2:3;            /* Reserved */
			u8  pci1link_status:1;  /* pci1 link status */
			u64 rsvd3:51;           /* Reserved */
		};
	};
};

struct feature_fme_genprotrange2_base {
	union {
		u64 csr;
		struct {
			u16 rsvd1;           /* Reserved */
			/* Base Address of memory range */
			u8  protected_base_addrss:4;
			u64 rsvd2:44;           /* Reserved */
		};
	};
};

struct feature_fme_genprotrange2_limit {
	union {
		u64 csr;
		struct {
			u16 rsvd1;           /* Reserved */
			/* Limit Address of memory range */
			u8  protected_limit_addrss:4;
			u16 rsvd2:11;           /* Reserved */
			u8  enable:1;        /* Enable GENPROTRANGE check */
			u32 rsvd3;           /* Reserved */
		};
	};
};

struct feature_fme_dxe_lock {
	union {
		u64 csr;
		struct {
			/*
			 * Determines write access to the DXE region CSRs
			 * 1 - CSR region is locked;
			 * 0 - it is open for write access.
			 */
			u8  dxe_early_lock:1;
			/*
			 * Determines write access to the HSSI CSR
			 * 1 - CSR region is locked;
			 * 0 - it is open for write access.
			 */
			u8  dxe_late_lock:1;
			u64 rsvd:62;
		};
	};
};

#define HSSI_ID_NO_HASSI	0
#define HSSI_ID_PCIE_RP		1
#define HSSI_ID_ETHERNET	2

struct feature_fme_bitstream_id {
	union {
		u64 csr;
		struct {
			u32 gitrepo_hash:32;	/* GIT repository hash */
			/*
			 * HSSI configuration identifier:
			 * 0 - No HSSI
			 * 1 - PCIe-RP
			 * 2 - Ethernet
			 */
			u8  hssi_id:4;
			u16 rsvd1:12;		/* Reserved */
			/* Bitstream version patch number */
			u8  bs_verpatch:4;
			/* Bitstream version minor number */
			u8  bs_verminor:4;
			/* Bitstream version major number */
			u8  bs_vermajor:4;
			/* Bitstream version debug number */
			u8  bs_verdebug:4;
		};
	};
};

struct feature_fme_bitstream_md {
	union {
		u64 csr;
		struct {
			/* Seed number userd for synthesis flow */
			u8  synth_seed:4;
			/* Synthesis date(day number - 2 digits) */
			u8  synth_day:8;
			/* Synthesis date(month number - 2 digits) */
			u8  synth_month:8;
			/* Synthesis date(year number - 2 digits) */
			u8  synth_year:8;
			u64 rsvd:36;		/* Reserved */
		};
	};
};

struct feature_fme_iommu_ctrl {
	union {
		u64 csr;
		struct {
			/* Disables IOMMU prefetcher for C0 channel */
			u8 prefetch_disableC0:1;
			/* Disables IOMMU prefetcher for C1 channel */
			u8 prefetch_disableC1:1;
			/* Disables IOMMU partial cache line writes */
			u8 prefetch_wrdisable:1;
			u8 rsvd1:1;		/* Reserved */
			/*
			 * Select counter and read value from register
			 * iommu_stat.dbg_counters
			 * 0 - Number of 4K page translation response
			 * 1 - Number of 2M page translation response
			 * 2 - Number of 1G page translation response
			 */
			u8 counter_sel:2;
			u32 rsvd2:26;		/* Reserved */
			/* Connected to IOMMU SIP Capabilities */
			u32 capecap_defeature;
		};
	};
};

struct feature_fme_iommu_stat {
	union {
		u64 csr;
		struct {
			/* Translation Enable bit from IOMMU SIP */
			u8 translation_enable:1;
			/* Drain request in progress */
			u8 drain_req_inprog:1;
			/* Invalidation current state */
			u8 inv_state:3;
			/* C0 Response Buffer current state */
			u8 respbuffer_stateC0:3;
			/* C1 Response Buffer current state */
			u8 respbuffer_stateC1:3;
			/* Last request ID to IOMMU SIP */
			u8 last_reqID:4;
			/* Last IOMMU SIP response ID value */
			u8 last_respID:4;
			/* Last IOMMU SIP response status value */
			u8 last_respstatus:3;
			/* C0 Transaction Buffer is not empty */
			u8 transbuf_notEmptyC0:1;
			/* C1 Transaction Buffer is not empty */
			u8 transbuf_notEmptyC1:1;
			/* C0 Request FIFO is not empty */
			u8 reqFIFO_notemptyC0:1;
			/* C1 Request FIFO is not empty */
			u8 reqFIFO_notemptyC1:1;
			/* C0 Response FIFO is not empty */
			u8 respFIFO_notemptyC0:1;
			/* C1 Response FIFO is not empty */
			u8 respFIFO_notemptyC1:1;
			/* C0 Response FIFO overflow detected */
			u8 respFIFO_overflowC0:1;
			/* C1 Response FIFO overflow detected */
			u8 respFIFO_overflowC1:1;
			/* C0 Transaction Buffer overflow detected */
			u8 tranbuf_overflowC0:1;
			/* C1 Transaction Buffer overflow detected */
			u8 tranbuf_overflowC1:1;
			/* Request FIFO overflow detected */
			u8 reqFIFO_overflow:1;
			/* IOMMU memory read in progress */
			u8 memrd_inprog:1;
			/* IOMMU memory write in progress */
			u8 memwr_inprog:1;
			u8 rsvd1:1;	/* Reserved */
			/* Value of counter selected by iommu_ctl.counter_sel */
			u16 dbg_counters:16;
			u16 rsvd2:12;	/* Reserved */
		};
	};
};

struct feature_fme_pcie0_ctrl {
	union {
		u64 csr;
		struct {
			u64 vtd_bar_lock:1;	/* Lock VT-D BAR register */
			u64 rsvd1:3;
			u64 rciep:1;		/* Configure PCIE0 as RCiEP */
			u64 rsvd2:59;
		};
	};
};

struct feature_fme_llpr_smrr_base {
	union {
		u64 csr;
		struct {
			u64 rsvd1:12;
			u64 base:20;	/* SMRR2 memory range base address */
			u64 rsvd2:32;
		};
	};
};

struct feature_fme_llpr_smrr_mask {
	union {
		u64 csr;
		struct {
			u64 rsvd1:11;
			u64 valid:1;	/* LLPR_SMRR rule is valid or not */
			/*
			 * SMRR memory range mask which determines the range
			 * of region being mapped
			 */
			u64 phys_mask:20;
			u64 rsvd2:32;
		};
	};
};

struct feature_fme_llpr_smrr2_base {
	union {
		u64 csr;
		struct {
			u64 rsvd1:12;
			u64 base:20;	/* SMRR2 memory range base address */
			u64 rsvd2:32;
		};
	};
};

struct feature_fme_llpr_smrr2_mask {
	union {
		u64 csr;
		struct {
			u64 rsvd1:11;
			u64 valid:1;	/* LLPR_SMRR2 rule is valid or not */
			/*
			 * SMRR2 memory range mask which determines the range
			 * of region being mapped
			 */
			u64 phys_mask:20;
			u64 rsvd2:32;
		};
	};
};

struct feature_fme_llpr_meseg_base {
	union {
		u64 csr;
		struct {
			/* A[45:19] of base address memory range */
			u64 me_base:27;
			u64 rsvd:37;
		};
	};
};

struct feature_fme_llpr_meseg_limit {
	union {
		u64 csr;
		struct {
			/* A[45:19] of limit address memory range */
			u64 me_limit:27;
			u64 rsvd1:4;
			u64 enable:1;	/* Enable LLPR MESEG rule */
			u64 rsvd2:32;
		};
	};
};

struct feature_fme_header {
	struct feature_header header;
	struct feature_afu_header afu_header;
	u64 reserved;
	u64 scratchpad;
	struct feature_fme_capability capability;
	struct feature_fme_port port[MAX_FPGA_PORT_NUM];
	struct feature_fme_fab_status fab_status;
	struct feature_fme_bitstream_id bitstream_id;
	struct feature_fme_bitstream_md bitstream_md;
	struct feature_fme_genprotrange2_base genprotrange2_base;
	struct feature_fme_genprotrange2_limit genprotrange2_limit;
	struct feature_fme_dxe_lock dxe_lock;
	struct feature_fme_iommu_ctrl iommu_ctrl;
	struct feature_fme_iommu_stat iommu_stat;
	struct feature_fme_pcie0_ctrl pcie0_control;
	struct feature_fme_llpr_smrr_base smrr_base;
	struct feature_fme_llpr_smrr_mask smrr_mask;
	struct feature_fme_llpr_smrr2_base smrr2_base;
	struct feature_fme_llpr_smrr2_mask smrr2_mask;
	struct feature_fme_llpr_meseg_base meseg_base;
	struct feature_fme_llpr_meseg_limit meseg_limit;
};

struct feature_port_capability {
	union {
		u64 csr;
		struct {
			u8 port_number:2;	/* Port Number 0-3 */
			u8 rsvd1:6;		/* Reserved */
			u16 mmio_size;		/* User MMIO size in KB */
			u8 rsvd2;		/* Reserved */
			u8 sp_intr_num:4;	/* Supported interrupts num */
			u32 rsvd3:28;		/* Reserved */
		};
	};
};

struct feature_port_control {
	union {
		u64 csr;
		struct {
			u8 port_sftrst:1;	/* Port Soft Reset */
			u8 rsvd1:1;		/* Reserved */
			u8 latency_tolerance:1;/* '1' >= 40us, '0' < 40us */
			u8 rsvd2:1;		/* Reserved */
			u8 port_sftrst_ack:1;	/* HW ACK for Soft Reset */
			u64 rsvd3:59;		/* Reserved */
		};
	};
};

#define PORT_POWER_STATE_NORMAL		0
#define PORT_POWER_STATE_AP1		1
#define PORT_POWER_STATE_AP2		2
#define PORT_POWER_STATE_AP6		6

struct feature_port_status {
	union {
		u64 csr;
		struct {
			u8 port_freeze:1;	/* '1' - freezed '0' - normal */
			u8 rsvd1:7;		/* Reserved */
			u8 power_state:4;	/* Power State */
			u8 ap1_event:1;		/* AP1 event was detected  */
			u8 ap2_event:1;		/* AP2 event was detected  */
			u64 rsvd2:50;		/* Reserved */
		};
	};
};

/* Port Header Register Set */
struct feature_port_header {
	struct feature_header header;
	struct feature_afu_header afu_header;
	u64 port_mailbox;
	u64 scratchpad;
	struct feature_port_capability capability;
	struct feature_port_control control;
	struct feature_port_status status;
	u64 rsvd2;
	u64 user_clk_freq_cmd0;
	u64 user_clk_freq_cmd1;
	u64 user_clk_freq_sts0;
	u64 user_clk_freq_sts1;
};

struct feature_fme_tmp_threshold {
	union {
		u64 csr;
		struct {
			u8  tmp_thshold1:7;	  /* temperature Threshold 1 */
			/* temperature Threshold 1 enable/disable */
			u8  tmp_thshold1_enable:1;
			u8  tmp_thshold2:7;       /* temperature Threshold 2 */
			/* temperature Threshold 2 enable /disable */
			u8  tmp_thshold2_enable:1;
			u8  pro_hot_setpoint:7;   /* Proc Hot set point */
			u8  rsvd4:1;              /* Reserved */
			u8  therm_trip_thshold:7; /* Thermeal Trip Threshold */
			u8  rsvd3:1;              /* Reserved */
			u8  thshold1_status:1;	  /* Threshold 1 Status */
			u8  thshold2_status:1;    /* Threshold 2 Status */
			u8  rsvd5:1;              /* Reserved */
			/* Thermeal Trip Threshold status */
			u8  therm_trip_thshold_status:1;
			u8  rsvd6:4;		  /* Reserved */
			/* Validation mode- Force Proc Hot */
			u8  valmodeforce:1;
			/* Validation mode - Therm trip Hot */
			u8  valmodetherm:1;
			u8  rsvd2:2;              /* Reserved */
			u8  thshold_policy:1;     /* threshold policy */
			u32 rsvd:19;              /* Reserved */
		};
	};
};

/* Temperature Sensor Read values format 1 */
struct feature_fme_temp_rdsensor_fmt1 {
	union {
		u64 csr;
		struct {
			/* Reads out FPGA temperature in celsius */
			u8  fpga_temp:7;
			u8  rsvd0:1;			/* Reserved */
			/* Temperature reading sequence number */
			u16 tmp_reading_seq_num;
			/* Temperature reading is valid */
			u8  tmp_reading_valid:1;
			u8  rsvd1:7;			/* Reserved */
			u16 dbg_mode:10;		/* Debug mode */
			u32 rsvd2:22;			/* Reserved */
		};
	};
};

/* Temperature sensor read values format 2 */
struct feature_fme_temp_rdsensor_fmt2 {
	u64 rsvd;	/* Reserved */
};

/* Temperature Threshold Capability Register */
struct feature_fme_tmp_threshold_cap {
	union {
		u64 csr;
		struct {
			/* Temperature Threshold Unsupported */
			u8  tmp_thshold_disabled:1;
			u64 rsvd:63;			/* Reserved */
		};
	};
};

/* FME THERNAL FEATURE */
struct feature_fme_thermal {
	struct feature_header header;
	struct feature_fme_tmp_threshold threshold;
	struct feature_fme_temp_rdsensor_fmt1 rdsensor_fm1;
	struct feature_fme_temp_rdsensor_fmt2 rdsensor_fm2;
	struct feature_fme_tmp_threshold_cap threshold_cap;
};

/* Power Status register */
struct feature_fme_pm_status {
	union {
		u64 csr;
		struct {
			/* FPGA Power consumed, The format is to be defined */
			u32 pwr_consumed:18;
			/* FPGA Latency Tolerance Reporting */
			u8  fpga_latency_report:1;
			u64 rsvd:45;			/* Reserved */
		};
	};
};

/* AP Thresholds */
struct feature_fme_pm_ap_threshold {
	union {
		u64 csr;
		struct {
			/*
			 * Number of clocks (5ns period) for assertion
			 * of FME_data
			 */
			u8  threshold1:7;
			u8  rsvd1:1;
			u8  threshold2:7;
			u8  rsvd2:1;
			u8  threshold1_status:1;
			u8  threshold2_status:1;
			u64 rsvd3:46;		/* Reserved */
		};
	};
};

/* Xeon Power Limit */
struct feature_fme_pm_xeon_limit {
	union {
		u64 csr;
		struct {
			/* Power limit in Watts in 12.3 format */
			u16 pwr_limit:15;
			/* Indicates that power limit has been written */
			u8  enable:1;
			/* 0 - Turbe range, 1 - Entire range */
			u8  clamping:1;
			/* Time constant in XXYYY format */
			u8  time:7;
			u64 rsvd:40;		/* Reserved */
		};
	};
};

/* FPGA Power Limit */
struct feature_fme_pm_fpga_limit {
	union {
		u64 csr;
		struct {
			/* Power limit in Watts in 12.3 format */
			u16 pwr_limit:15;
			/* Indicates that power limit has been written */
			u8  enable:1;
			/* 0 - Turbe range, 1 - Entire range */
			u8  clamping:1;
			/* Time constant in XXYYY format */
			u8  time:7;
			u64 rsvd:40;		/* Reserved */
		};
	};
};

/* FME POWER FEATURE */
struct feature_fme_power {
	struct feature_header header;
	struct feature_fme_pm_status status;
	struct feature_fme_pm_ap_threshold threshold;
	struct feature_fme_pm_xeon_limit xeon_limit;
	struct feature_fme_pm_fpga_limit fpga_limit;
};

#define CACHE_CHANNEL_RD	0
#define CACHE_CHANNEL_WR	1

enum iperf_cache_events {
	IPERF_CACHE_RD_HIT,
	IPERF_CACHE_WR_HIT,
	IPERF_CACHE_RD_MISS,
	IPERF_CACHE_WR_MISS,
	IPERF_CACHE_RSVD, /* reserved */
	IPERF_CACHE_HOLD_REQ,
	IPERF_CACHE_DATA_WR_PORT_CONTEN,
	IPERF_CACHE_TAG_WR_PORT_CONTEN,
	IPERF_CACHE_TX_REQ_STALL,
	IPERF_CACHE_RX_REQ_STALL,
	IPERF_CACHE_EVICTIONS,
};

/* FPMON Cache Control */
struct feature_fme_ifpmon_ch_ctl {
	union {
		u64 csr;
		struct {
			u8  reset_counters:1;	/* Reset Counters */
			u8  rsvd1:7;		/* Reserved */
			u8  freeze:1;		/* Freeze if set to 1 */
			u8  rsvd2:7;		/* Reserved */
			u8  cache_event:4;	/* Select the cache event */
			u8  cci_chsel:1;	/* Select the channel */
			u64 rsvd3:43;		/* Reserved */
		};
	};
};

/* FPMON Cache Counter */
struct feature_fme_ifpmon_ch_ctr {
	union {
		u64 csr;
		struct {
			/* Cache Counter for even addresse */
			u64 cache_counter:48;
			u16 rsvd:12;		/* Reserved */
			/* Cache Event being reported */
			u8  event_code:4;
		};
	};
};

enum iperf_fab_events {
	IPERF_FAB_PCIE0_RD,
	IPERF_FAB_PCIE0_WR,
	IPERF_FAB_PCIE1_RD,
	IPERF_FAB_PCIE1_WR,
	IPERF_FAB_UPI_RD,
	IPERF_FAB_UPI_WR,
	IPERF_FAB_MMIO_RD,
	IPERF_FAB_MMIO_WR,
};

#define FAB_DISABLE_FILTER     0
#define FAB_ENABLE_FILTER      1

/* FPMON FAB Control */
struct feature_fme_ifpmon_fab_ctl {
	union {
		u64 csr;
		struct {
			u8  reset_counters:1;	/* Reset Counters */
			u8  rsvd:7;		/* Reserved */
			u8  freeze:1;		/* Set to 1 frozen counter */
			u8  rsvd1:7;		/* Reserved */
			u8  fab_evtcode:4;	/* Fabric Event Code */
			u8  port_id:2;		/* Port ID */
			u8  rsvd2:1;		/* Reserved */
			u8  port_filter:1;	/* Port Filter */
			u64 rsvd3:40;		/* Reserved */
		};
	};
};

/* FPMON Event Counter */
struct feature_fme_ifpmon_fab_ctr {
	union {
		u64 csr;
		struct {
			u64 fab_cnt:60;	/* Fabric event counter */
			/* Fabric event code being reported */
			u8  event_code:4;
		};
	};
};

/* FPMON Clock Counter */
struct feature_fme_ifpmon_clk_ctr {
	u64 afu_interf_clock;		/* Clk_16UI (AFU clock) counter. */
};

enum iperf_vtd_events {
	IPERF_VTD_AFU_MEM_RD_TRANS,
	IPERF_VTD_AFU_MEM_WR_TRANS,
	IPERF_VTD_AFU_DEVTLB_RD_HIT,
	IPERF_VTD_AFU_DEVTLB_WR_HIT,
	IPERF_VTD_DEVTLB_4K_FILL,
	IPERF_VTD_DEVTLB_2M_FILL,
	IPERF_VTD_DEVTLB_1G_FILL,
};

/* VT-d control register */
struct feature_fme_ifpmon_vtd_ctl {
	union {
		u64 csr;
		struct {
			u8  reset_counters:1;	/* Reset Counters */
			u8  rsvd:7;		/* Reserved */
			u8  freeze:1;		/* Set to 1 frozen counter */
			u8  rsvd1:7;		/* Reserved */
			u8  vtd_evtcode:4;	/* VTd and TLB event code */
			u64 rsvd2:44;		/* Reserved */
		};
	};
};

/* VT-d event counter */
struct feature_fme_ifpmon_vtd_ctr {
	union {
		u64 csr;
		struct {
			u64 vtd_counter:48;	/* VTd event counter */
			u16 rsvd:12;		/* Reserved */
			u8  event_code:4;	/* VTd event code */
		};
	};
};

enum iperf_vtd_sip_events {
	IPERF_VTD_SIP_IOTLB_4K_HIT,
	IPERF_VTD_SIP_IOTLB_2M_HIT,
	IPERF_VTD_SIP_IOTLB_1G_HIT,
	IPERF_VTD_SIP_SLPWC_L3_HIT,
	IPERF_VTD_SIP_SLPWC_L4_HIT,
	IPERF_VTD_SIP_RCC_HIT,
	IPERF_VTD_SIP_IOTLB_4K_MISS,
	IPERF_VTD_SIP_IOTLB_2M_MISS,
	IPERF_VTD_SIP_IOTLB_1G_MISS,
	IPERF_VTD_SIP_SLPWC_L3_MISS,
	IPERF_VTD_SIP_SLPWC_L4_MISS,
	IPERF_VTD_SIP_RCC_MISS,
};

/* VT-d SIP control register */
struct feature_fme_ifpmon_vtd_sip_ctl {
	union {
		u64 csr;
		struct {
			u8  reset_counters:1;	/* Reset Counters */
			u8  rsvd:7;		/* Reserved */
			u8  freeze:1;		/* Set to 1 frozen counter */
			u8  rsvd1:7;		/* Reserved */
			u8  vtd_evtcode:4;	/* VTd and TLB event code */
			u64 rsvd2:44;		/* Reserved */
		};
	};
};

/* VT-d SIP event counter */
struct feature_fme_ifpmon_vtd_sip_ctr {
	union {
		u64 csr;
		struct {
			u64 vtd_counter:48;	/* VTd event counter */
			u16 rsvd:12;		/* Reserved */
			u8 event_code:4;	/* VTd event code */
		};
	};
};

/* FME IPERF FEATURE */
struct feature_fme_iperf {
	struct feature_header header;
	struct feature_fme_ifpmon_ch_ctl ch_ctl;
	struct feature_fme_ifpmon_ch_ctr ch_ctr0;
	struct feature_fme_ifpmon_ch_ctr ch_ctr1;
	struct feature_fme_ifpmon_fab_ctl fab_ctl;
	struct feature_fme_ifpmon_fab_ctr fab_ctr;
	struct feature_fme_ifpmon_clk_ctr clk;
	struct feature_fme_ifpmon_vtd_ctl vtd_ctl;
	struct feature_fme_ifpmon_vtd_ctr vtd_ctr;
	struct feature_fme_ifpmon_vtd_sip_ctl vtd_sip_ctl;
	struct feature_fme_ifpmon_vtd_sip_ctr vtd_sip_ctr;
};

enum dperf_fab_events {
	DPERF_FAB_PCIE0_RD,
	DPERF_FAB_PCIE0_WR,
	DPERF_FAB_MMIO_RD = 6,
	DPERF_FAB_MMIO_WR,
};

/* FPMON FAB Control */
struct feature_fme_dfpmon_fab_ctl {
	union {
		u64 csr;
		struct {
			u8  reset_counters:1;	/* Reset Counters */
			u8  rsvd:7;		/* Reserved */
			u8  freeze:1;		/* Set to 1 frozen counter */
			u8  rsvd1:7;		/* Reserved */
			u8  fab_evtcode:4;	/* Fabric Event Code */
			u8  port_id:2;		/* Port ID */
			u8  rsvd2:1;		/* Reserved */
			u8  port_filter:1;	/* Port Filter */
			u64 rsvd3:40;		/* Reserved */
		};
	};
};

/* FPMON Event Counter */
struct feature_fme_dfpmon_fab_ctr {
	union {
		u64 csr;
		struct {
			u64 fab_cnt:60;	/* Fabric event counter */
			/* Fabric event code being reported */
			u8  event_code:4;
		};
	};
};

/* FPMON Clock Counter */
struct feature_fme_dfpmon_clk_ctr {
	u64 afu_interf_clock;		/* Clk_16UI (AFU clock) counter. */
};

/* FME DPERF FEATURE */
struct feature_fme_dperf {
	struct feature_header header;
	u64 rsvd[3];
	struct feature_fme_dfpmon_fab_ctl fab_ctl;
	struct feature_fme_dfpmon_fab_ctr fab_ctr;
	struct feature_fme_dfpmon_clk_ctr clk;
};

struct feature_fme_error0 {
#define FME_ERROR0_MASK_DEFAULT 0x40UL  /* pcode workaround */
	union {
		u64 csr;
		struct {
			u8  fabric_err:1;	/* Fabric error */
			u8  fabfifo_overflow:1;	/* Fabric fifo overflow */
			u8  reserved2:3;
			/* AFU PF/VF access mismatch detected */
			u8  afu_acc_mode_err:1;
			u8  reserved6:1;
			/* PCIE0 CDC Parity Error */
			u8  pcie0cdc_parity_err:5;
			/* PCIE1 CDC Parity Error */
			u8  pcie1cdc_parity_err:5;
			/* CVL CDC Parity Error */
			u8  cvlcdc_parity_err:3;
			u8  fpgaseuerr:1;
			u64 rsvd:43;		/* Reserved */
		};
	};
};

/* PCIe0 Error Status register */
struct feature_fme_pcie0_error {
#define FME_PCIE0_ERROR_MASK   0xFFUL
	union {
		u64 csr;
		struct {
			u8  formattype_err:1;	/* TLP format/type error */
			u8  MWAddr_err:1;	/* TLP MW address error */
			u8  MWAddrLength_err:1;	/* TLP MW length error */
			u8  MRAddr_err:1;	/* TLP MR address error */
			u8  MRAddrLength_err:1;	/* TLP MR length error */
			u8  cpl_tag_err:1;	/* TLP CPL tag error */
			u8  cpl_status_err:1;	/* TLP CPL status error */
			u8  cpl_timeout_err:1;	/* TLP CPL timeout */
			u8  cci_parity_err:1;	/* CCI bridge parity error */
			u8  rxpoison_tlp_err:1;	/* Received a TLP with EP set */
			u64 rsvd:52;		/* Reserved */
			u8  vfnumb_err:1;	/* Number of error VF */
			u8  funct_type_err:1;	/* Virtual (1) or Physical */
		};
	};
};

/* PCIe1 Error Status register */
struct feature_fme_pcie1_error {
#define FME_PCIE1_ERROR_MASK   0xFFUL
	union {
		u64 csr;
		struct {
			u8  formattype_err:1;	/* TLP format/type error */
			u8  MWAddr_err:1;	/* TLP MW address error */
			u8  MWAddrLength_err:1;	/* TLP MW length error */
			u8  MRAddr_err:1;	/* TLP MR address error */
			u8  MRAddrLength_err:1;	/* TLP MR length error */
			u8  cpl_tag_err:1;	/* TLP CPL tag error */
			u8  cpl_status_err:1;	/* TLP CPL status error */
			u8  cpl_timeout_err:1;	/* TLP CPL timeout */
			u8  cci_parity_err:1;	/* CCI bridge parity error */
			u8  rxpoison_tlp_err:1;	/* Received a TLP with EP set */
			u64 rsvd:54;		/* Reserved */
		};
	};
};

/* FME First Error register */
struct feature_fme_first_error {
#define FME_FIRST_ERROR_MASK   ((1ULL << 60) - 1)
	union {
		u64 csr;
		struct {
			/*
			 * Indicates the Error Register that was
			 * triggered first
			 */
			u64 err_reg_status:60;
			/*
			 * Holds 60 LSBs from the Error register that was
			 * triggered first
			 */
			u8 errReg_id:4;
		};
	};
};

/* FME Next Error register */
struct feature_fme_next_error {
#define FME_NEXT_ERROR_MASK    ((1ULL << 60) - 1)
	union {
		u64 csr;
		struct {
			/*
			 * Indicates the Error Register that was
			 * triggered second
			 */
			u64 err_reg_status:60;
			/*
			 * Holds 60 LSBs from the Error register that was
			 * triggered second
			 */
			u8  errReg_id:4;
		};
	};
};

/* RAS Non Fatal Error Status register */
struct feature_fme_ras_nonfaterror {
	union {
		u64 csr;
		struct {
			/* thremal threshold AP1 */
			u8  temp_thresh_ap1:1;
			/* thremal threshold AP2 */
			u8  temp_thresh_ap2:1;
			u8  pcie_error:1;	/* pcie Error */
			u8  portfatal_error:1;	/* port fatal error */
			u8  proc_hot:1;		/* Indicates a ProcHot event */
			/* Indicates an AFU PF/VF access mismatch */
			u8  afu_acc_mode_err:1;
			/* Injected nonfata Error */
			u8  injected_nonfata_err:1;
			u8  rsvd1:2;
			/* Temperature threshold triggered AP6*/
			u8  temp_thresh_AP6:1;
			/* Power threshold triggered AP1 */
			u8  power_thresh_AP1:1;
			/* Power threshold triggered AP2 */
			u8  power_thresh_AP2:1;
			/* Indicates a MBP event */
			u8  mbp_err:1;
			u64 rsvd2:51;		/* Reserved */
		};
	};
};

/* RAS Catastrophic Fatal Error Status register */
struct feature_fme_ras_catfaterror {
	union {
		u64 csr;
		struct {
			/* KTI Link layer error detected */
			u8  ktilink_fatal_err:1;
			/* tag-n-cache error detected */
			u8  tagcch_fatal_err:1;
			/* CCI error detected */
			u8  cci_fatal_err:1;
			/* KTI Protocol error detected */
			u8  ktiprpto_fatal_err:1;
			/* Fatal DRAM error detected */
			u8  dram_fatal_err:1;
			/* IOMMU detected */
			u8  iommu_fatal_err:1;
			/* Fabric Fatal Error */
			u8  fabric_fatal_err:1;
			/* PCIe possion Error */
			u8  pcie_poison_err:1;
			/* Injected fatal Error */
			u8  inject_fata_err:1;
			/* Catastrophic CRC Error */
			u8  crc_catast_err:1;
			/* Catastrophic Thermal Error */
			u8  therm_catast_err:1;
			/* Injected Catastrophic Error */
			u8  injected_catast_err:1;
			/* SEU error on BMC */
			u8  bmc_seu_catast_err:1;
			u64 rsvd:51;
		};
	};
};

/* RAS Error injection register */
struct feature_fme_ras_error_inj {
#define FME_RAS_ERROR_INJ_MASK      0x7UL
	union {
		u64 csr;
		struct {
			u8  catast_error:1;	/* Catastrophic error flag */
			u8  fatal_error:1;	/* Fatal error flag */
			u8  nonfatal_error:1;	/* NonFatal error flag */
			u64 rsvd:61;		/* Reserved */
		};
	};
};

/* FME error capabilities */
struct feature_fme_error_capability {
	union {
	u64 csr;
		struct {
			u8 support_intr:1;
			/* MSI-X vector table entry number */
			u16 intr_vector_num:12;
			u64 rsvd:50;	/* Reserved */
			u64 seu_support:1;
		};
	};
};

/* FME ERR FEATURE */
struct feature_fme_err {
	struct feature_header header;
	struct feature_fme_error0 fme_err_mask;
	struct feature_fme_error0 fme_err;
	struct feature_fme_pcie0_error pcie0_err_mask;
	struct feature_fme_pcie0_error pcie0_err;
	struct feature_fme_pcie1_error pcie1_err_mask;
	struct feature_fme_pcie1_error pcie1_err;
	struct feature_fme_first_error fme_first_err;
	struct feature_fme_next_error fme_next_err;
	struct feature_fme_ras_nonfaterror ras_nonfat_mask;
	struct feature_fme_ras_nonfaterror ras_nonfaterr;
	struct feature_fme_ras_catfaterror ras_catfat_mask;
	struct feature_fme_ras_catfaterror ras_catfaterr;
	struct feature_fme_ras_error_inj ras_error_inj;
	struct feature_fme_error_capability fme_err_capability;
	u64 seu_emr_l;
	u64 seu_emr_h;
};

/* FME Partial Reconfiguration Control */
struct feature_fme_pr_ctl {
	union {
		u64 csr;
		struct {
			u8  pr_reset:1;		/* Reset PR Engine */
			u8  rsvd3:3;		/* Reserved */
			u8  pr_reset_ack:1;	/* Reset PR Engine Ack */
			u8  rsvd4:3;		/* Reserved */
			u8  pr_regionid:2;	/* PR Region ID */
			u8  rsvd1:2;		/* Reserved */
			u8  pr_start_req:1;	/* PR Start Request */
			u8  pr_push_complete:1;	/* PR Data push complete */
			u8  pr_kind:1;		/* PR Data push complete */
			u32 rsvd:17;		/* Reserved */
			u32 config_data;	/* Config data TBD */
		};
	};
};

/* FME Partial Reconfiguration Status */
struct feature_fme_pr_status {
	union {
		u64 csr;
		struct {
			u16 pr_credit:9;	/* PR Credits */
			u8  rsvd2:7;		/* Reserved */
			u8  pr_status:1;	/* PR status */
			u8  rsvd:3;		/* Reserved */
			/* Altra PR Controller Block status */
			u8  pr_controller_status:3;
			u8  rsvd1:1;            /* Reserved */
			u8  pr_host_status:4;   /* PR Host status */
			u8  rsvd3:4;		/* Reserved */
			/* Security Block Status fields (TBD) */
			u32 security_bstatus;
		};
	};
};

/* FME Partial Reconfiguration Data */
struct feature_fme_pr_data {
	union {
		u64 csr;	/* PR data from the raw-binary file */
		struct {
			/* PR data from the raw-binary file */
			u32 pr_data_raw;
			u32 rsvd;
		};
	};
};

/* FME PR Public Key */
struct feature_fme_pr_key {
	u64 key;		/* FME PR Public Hash */
};

/* FME PR FEATURE */
struct feature_fme_pr {
	struct feature_header header;
	/*Partial Reconfiguration control */
	struct feature_fme_pr_ctl	ccip_fme_pr_control;

	/* Partial Reconfiguration Status */
	struct feature_fme_pr_status	ccip_fme_pr_status;

	/* Partial Reconfiguration data */
	struct feature_fme_pr_data	ccip_fme_pr_data;

	/* Partial Reconfiguration data */
	u64				ccip_fme_pr_err;

	u64 rsvd1[3];

	/* Partial Reconfiguration data registers */
	u64 fme_pr_data1;
	u64 fme_pr_data2;
	u64 fme_pr_data3;
	u64 fme_pr_data4;
	u64 fme_pr_data5;
	u64 fme_pr_data6;
	u64 fme_pr_data7;
	u64 fme_pr_data8;

	u64 rsvd2[5];

	/* PR Interface ID */
	u64 fme_pr_intfc_id_l;
	u64 fme_pr_intfc_id_h;

	/* MSIX filed to be Added */
};

/* FME HSSI Control */
struct feature_fme_hssi_eth_ctrl {
	union {
		u64 csr;
		struct {
			u32 data:32;		/* HSSI data */
			u16 address:16;		/* HSSI address */
			/*
			 * HSSI comamnd
			 * 0x0 - No request
			 * 0x08 - SW register RD request
			 * 0x10 - SW register WR request
			 * 0x40 - Auxiliar bus RD request
			 * 0x80 - Auxiliar bus WR request
			 */
			u16 cmd:16;
		};
	};
};

/* FME HSSI Status */
struct feature_fme_hssi_eth_stat {
	union {
		u64 csr;
		struct {
			u32 data:32;		/* HSSI data */
			u8  acknowledge:1;	/* HSSI acknowledge */
			u8  spare:1;		/* HSSI spare */
			u32 rsvd:30;		/* Reserved */
		};
	};
};

/* FME HSSI FEATURE */
struct feature_fme_hssi {
	struct feature_header header;
	struct feature_fme_hssi_eth_ctrl	hssi_control;
	struct feature_fme_hssi_eth_stat	hssi_status;
};

#define PORT_ERR_MASK		0xfff0703ff001f
struct feature_port_err_key {
	union {
		u64 csr;
		struct {
			/* Tx Channel0: Overflow */
			u8 tx_ch0_overflow:1;
			/* Tx Channel0: Invalid request encoding */
			u8 tx_ch0_invaldreq :1;
			/* Tx Channel0: Request with cl_len=3 not supported */
			u8 tx_ch0_cl_len3:1;
			/* Tx Channel0: Request with cl_len=2 not aligned 2CL */
			u8 tx_ch0_cl_len2:1;
			/* Tx Channel0: Request with cl_len=4 not aligned 4CL */
			u8 tx_ch0_cl_len4:1;

			u16 rsvd1:4;			/* Reserved */

			/* AFU MMIO RD received while PORT is in reset */
			u8 mmio_rd_whilerst:1;
			/* AFU MMIO WR received while PORT is in reset */
			u8 mmio_wr_whilerst:1;

			u16 rsvd2:5;			/* Reserved */

			/* Tx Channel1: Overflow */
			u8 tx_ch1_overflow:1;
			/* Tx Channel1: Invalid request encoding */
			u8 tx_ch1_invaldreq:1;
			/* Tx Channel1: Request with cl_len=3 not supported */
			u8 tx_ch1_cl_len3:1;
			/* Tx Channel1: Request with cl_len=2 not aligned 2CL */
			u8 tx_ch1_cl_len2:1;
			/* Tx Channel1: Request with cl_len=4 not aligned 4CL */
			u8 tx_ch1_cl_len4:1;

			/* Tx Channel1: Insufficient data payload */
			u8 tx_ch1_insuff_data:1;
			/* Tx Channel1: Data payload overrun */
			u8 tx_ch1_data_overrun:1;
			/* Tx Channel1 : Incorrect address */
			u8 tx_ch1_incorr_addr:1;
			/* Tx Channel1 : NON-Zero SOP Detected */
			u8 tx_ch1_nzsop:1;
			/* Tx Channel1 : Illegal VC_SEL, atomic request VLO */
			u8 tx_ch1_illegal_vcsel:1;

			u8 rsvd3:6;			/* Reserved */

			/* MMIO Read Timeout in AFU */
			u8 mmioread_timeout:1;

			/* Tx Channel2: FIFO Overflow */
			u8 tx_ch2_fifo_overflow:1;

			/* MMIO read is not matching pending request */
			u8 unexp_mmio_resp:1;

			u8 rsvd4:5;			/* Reserved */

			/* Number of pending Requests: counter overflow */
			u8 tx_req_counter_overflow:1;
			/* Req with Address violating SMM Range */
			u8 llpr_smrr_err:1;
			/* Req with Address violating second SMM Range */
			u8 llpr_smrr2_err:1;
			/* Req with Address violating ME Stolen message */
			u8 llpr_mesg_err:1;
			/* Req with Address violating Generic Protected Range */
			u8 genprot_range_err:1;
			/* Req with Address violating Legacy Range low */
			u8 legrange_low_err:1;
			/* Req with Address violating Legacy Range High */
			u8 legrange_high_err:1;
			/* Req with Address violating VGA memory range */
			u8 vgmem_range_err:1;
			u8 page_fault_err:1;		/* Page fault */
			u8 pmr_err:1;			/* PMR Error */
			u8 ap6_event:1;			/* AP6 event */
			/* VF FLR detected on Port with PF access control */
			u8 vfflr_access_err:1;
			u16 rsvd5:12;			/* Reserved */
		};
	};
};

/* Port first error register, not contain all error bits in error register. */
struct feature_port_first_err_key {
	union {
		u64 csr;
		struct {
			u8 tx_ch0_overflow:1;
			u8 tx_ch0_invaldreq :1;
			u8 tx_ch0_cl_len3:1;
			u8 tx_ch0_cl_len2:1;
			u8 tx_ch0_cl_len4:1;
			u8 rsvd1:4;			/* Reserved */
			u8 mmio_rd_whilerst:1;
			u8 mmio_wr_whilerst:1;
			u8 rsvd2:5;			/* Reserved */
			u8 tx_ch1_overflow:1;
			u8 tx_ch1_invaldreq:1;
			u8 tx_ch1_cl_len3:1;
			u8 tx_ch1_cl_len2:1;
			u8 tx_ch1_cl_len4:1;
			u8 tx_ch1_insuff_data:1;
			u8 tx_ch1_data_overrun:1;
			u8 tx_ch1_incorr_addr:1;
			u8 tx_ch1_nzsop:1;
			u8 tx_ch1_illegal_vcsel:1;
			u8 rsvd3:6;			/* Reserved */
			u8 mmioread_timeout:1;
			u8 tx_ch2_fifo_overflow:1;
			u8 rsvd4:6;			/* Reserved */
			u8 tx_req_counter_overflow:1;
			u32 rsvd5:23;			/* Reserved */
		};
	};
};

/* Port malformed Req0 */
struct feature_port_malformed_req0 {
	u64 header_lsb;
};

/* Port malformed Req1 */
struct feature_port_malformed_req1 {
	u64 header_msb;
};

/* Port debug register */
struct feature_port_debug {
	u64 port_debug;
};

/* Port error capabilities */
struct feature_port_err_capability {
	union {
		u64 csr;
		struct {
			u8  support_intr:1;
			/* MSI-X vector table entry number */
			u16 intr_vector_num:12;
			u64 rsvd:51;            /* Reserved */
		};
	};
};

/* PORT FEATURE ERROR */
struct feature_port_error {
	struct feature_header header;
	struct feature_port_err_key error_mask;
	struct feature_port_err_key port_error;
	struct feature_port_first_err_key port_first_error;
	struct feature_port_malformed_req0 malreq0;
	struct feature_port_malformed_req1 malreq1;
	struct feature_port_debug port_debug;
	struct feature_port_err_capability error_capability;
};

/* Port UMSG Capability */
struct feature_port_umsg_cap {
	union {
		u64 csr;
		struct {
			/* Number of umsg allocated to this port */
			u8 umsg_allocated;
			/* Enable / Disable UMsg engine for this port */
			u8 umsg_enable:1;
			/* Usmg initialization status */
			u8 umsg_init_complete:1;
			/* IOMMU can not translate the umsg base address */
			u8 umsg_trans_error:1;
			u64 rsvd:53;		/* Reserved */
		};
	};
};

/* Port UMSG base address */
struct feature_port_umsg_baseaddr {
	union {
		u64 csr;
		struct {
			u64 base_addr:48;	/* 48 bit physical address */
			u16 rsvd;		/* Reserved */
		};
	};
};

struct feature_port_umsg_mode {
	union {
		u64 csr;
		struct {
			u32 umsg_hint_enable;	/* UMSG hint enable/disable */
			u32 rsvd;		/* Reserved */
		};
	};
};

/* PORT FEATURE UMSG */
struct feature_port_umsg {
	struct feature_header header;
	struct feature_port_umsg_cap capability;
	struct feature_port_umsg_baseaddr baseaddr;
	struct feature_port_umsg_mode mode;
};

#define UMSG_EN_POLL_INVL 10 /* us */
#define UMSG_EN_POLL_TIMEOUT 1000 /* us */

/* Port UINT Capability */
struct feature_port_uint_cap {
	union {
		u64 csr;
		struct {
			u16 intr_num:12;	/* Supported interrupts num */
			/* First MSI-X vector table entry number */
			u16 first_vec_num:12;
			u64 rsvd:40;
		};
	};
};

/* PORT FEATURE UINT */
struct feature_port_uint {
	struct feature_header header;
	struct feature_port_uint_cap capability;
};

/* STP region supports mmap operation, so use page aligned size. */
#define PORT_FEATURE_STP_REGION_SIZE \
	IFPGA_PAGE_ALIGN(sizeof(struct feature_port_stp))

/* Port STP status register (for debug only)*/
struct feature_port_stp_status {
	union {
		u64 csr;
		struct {
			/* SLD Hub end-point read/write timeout */
			u8 sld_ep_timeout:1;
			/* Remote STP in reset/disable */
			u8 rstp_disabled:1;
			u8 unsupported_read:1;
			/* MMIO timeout detected and faked with a response */
			u8 mmio_timeout:1;
			u8 txfifo_count:4;
			u8 rxfifo_count:4;
			u8 txfifo_overflow:1;
			u8 txfifo_underflow:1;
			u8 rxfifo_overflow:1;
			u8 rxfifo_underflow:1;
			/* Number of MMIO write requests */
			u16 write_requests;
			/* Number of MMIO read requests */
			u16 read_requests;
			/* Number of MMIO read responses */
			u16 read_responses;
		};
	};
};

/*
 * PORT FEATURE STP
 * Most registers in STP region are not touched by driver, but mmapped to user
 * space. So they are not defined in below data structure, as its actual size
 * is 0x18c per spec.
 */
struct feature_port_stp {
	struct feature_header header;
	struct feature_port_stp_status stp_status;
};

/**
 * enum fpga_pr_states - fpga PR states
 * @FPGA_PR_STATE_UNKNOWN: can't determine state
 * @FPGA_PR_STATE_WRITE_INIT: preparing FPGA for programming
 * @FPGA_PR_STATE_WRITE_INIT_ERR: Error during WRITE_INIT stage
 * @FPGA_PR_STATE_WRITE: writing image to FPGA
 * @FPGA_PR_STATE_WRITE_ERR: Error while writing FPGA
 * @FPGA_PR_STATE_WRITE_COMPLETE: Doing post programming steps
 * @FPGA_PR_STATE_WRITE_COMPLETE_ERR: Error during WRITE_COMPLETE
 * @FPGA_PR_STATE_OPERATING: FPGA PR done
 */
enum fpga_pr_states {
	/* canot determine state states */
	FPGA_PR_STATE_UNKNOWN,

	/* write sequence: init, write, complete */
	FPGA_PR_STATE_WRITE_INIT,
	FPGA_PR_STATE_WRITE_INIT_ERR,
	FPGA_PR_STATE_WRITE,
	FPGA_PR_STATE_WRITE_ERR,
	FPGA_PR_STATE_WRITE_COMPLETE,
	FPGA_PR_STATE_WRITE_COMPLETE_ERR,

	/* FPGA PR done */
	FPGA_PR_STATE_DONE,
};

/*
 * FPGA Manager flags
 * FPGA_MGR_PARTIAL_RECONFIG: do partial reconfiguration if supported
 */
#define FPGA_MGR_PARTIAL_RECONFIG	BIT(0)

/**
 * struct fpga_pr_info - specific information to a FPGA PR
 * @flags: boolean flags as defined above
 * @pr_err: PR error code
 * @state: fpga manager state
 * @port_id: port id
 */
struct fpga_pr_info {
	u32 flags;
	u64 pr_err;
	enum fpga_pr_states state;
	int port_id;
};

#define DEFINE_FPGA_PR_ERR_MSG(_name_)			\
static const char * const _name_[] = {			\
	"PR operation error detected",			\
	"PR CRC error detected",			\
	"PR incompatiable bitstream error detected",	\
	"PR IP protocol error detected",		\
	"PR FIFO overflow error detected",		\
	"PR timeout error detected",			\
	"PR secure load error detected",		\
}

#define RST_POLL_INVL 10 /* us */
#define RST_POLL_TIMEOUT 1000 /* us */

#define PR_WAIT_TIMEOUT   15000000

#define PR_HOST_STATUS_IDLE	0
#define PR_MAX_ERR_NUM	7

DEFINE_FPGA_PR_ERR_MSG(pr_err_msg);

/*
 * green bitstream header must be byte-packed to match the
 * real file format.
 */
struct bts_header {
	u64 guid_h;
	u64 guid_l;
	u32 metadata_len;
};

#define GBS_GUID_H		0x414750466e6f6558
#define GBS_GUID_L		0x31303076534247b7
#define is_valid_bts(bts_hdr)				\
	(((bts_hdr)->guid_h == GBS_GUID_H) &&		\
	((bts_hdr)->guid_l == GBS_GUID_L))

#define check_support(n) (n == 1 ? "support" : "no")

/* bitstream id definition */
struct fme_bitstream_id {
	union {
		u64 id;
		struct {
			u8 build_patch:8;
			u8 build_minor:8;
			u8 build_major:8;
			u8 fvl_bypass:1;
			u8 mac_lightweight:1;
			u8 disagregate:1;
			u8 lightweiht:1;
			u8 seu:1;
			u8 ptp:1;
			u8 reserve:2;
			u8 interface:4;
			u32 afu_revision:12;
			u8 patch:4;
			u8 minor:4;
			u8 major:4;
			u8 reserved:4;
		};
	};
};

enum board_interface {
	VC_8_10G = 0,
	VC_4_25G = 1,
	VC_2_1_25 = 2,
	VC_4_25G_2_25G = 3,
	VC_2_2_25G = 4,
};

enum pac_major {
	VISTA_CREEK = 0,
	RUSH_CREEK = 1,
	DARBY_CREEK = 2,
};

enum pac_minor {
	DCP_1_0 = 0,
	DCP_1_1 = 1,
	DCP_1_2 = 2,
};

struct opae_board_info {
	enum pac_major major;
	enum pac_minor minor;
	enum board_interface type;

	/* PAC features */
	u8 fvl_bypass;
	u8 mac_lightweight;
	u8 disaggregate;
	u8 lightweight;
	u8 seu;
	u8 ptp;

	u32 boot_page;
	u32 max10_version;
	u32 nios_fw_version;
	u32 nums_of_retimer;
	u32 ports_per_retimer;
	u32 nums_of_fvl;
	u32 ports_per_fvl;
};

#pragma pack(pop)
#endif /* _BASE_IFPGA_DEFINES_H_ */
