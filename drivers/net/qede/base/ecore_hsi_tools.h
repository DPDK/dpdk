/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_HSI_TOOLS__
#define __ECORE_HSI_TOOLS__
/**********************************/
/* Tools HSI constants and macros */
/**********************************/

/*********************************** Init ************************************/

/* Width of GRC address in bits (addresses are specified in dwords) */
#define GRC_ADDR_BITS			23
#define MAX_GRC_ADDR			((1 << GRC_ADDR_BITS) - 1)

/* indicates an init that should be applied to any phase ID */
#define ANY_PHASE_ID			0xffff

/* init pattern size in bytes */
#define INIT_PATTERN_SIZE_BITS	4
#define MAX_INIT_PATTERN_SIZE	(1 << INIT_PATTERN_SIZE_BITS)

/* Max size in dwords of a zipped array */
#define MAX_ZIPPED_SIZE			8192

/* Global PXP window */
#define NUM_OF_PXP_WIN			19
#define PXP_WIN_DWORD_SIZE_BITS	10
#define PXP_WIN_DWORD_SIZE		(1 << PXP_WIN_DWORD_SIZE_BITS)
#define PXP_WIN_BYTE_SIZE_BITS	(PXP_WIN_DWORD_SIZE_BITS + 2)
#define PXP_WIN_BYTE_SIZE		(PXP_WIN_DWORD_SIZE * 4)

/********************************* GRC Dump **********************************/

/* width of GRC dump register sequence length in bits */
#define DUMP_SEQ_LEN_BITS			8
#define DUMP_SEQ_LEN_MAX_VAL		((1 << DUMP_SEQ_LEN_BITS) - 1)

/* width of GRC dump memory length in bits */
#define DUMP_MEM_LEN_BITS			18
#define DUMP_MEM_LEN_MAX_VAL		((1 << DUMP_MEM_LEN_BITS) - 1)

/* width of register type ID in bits */
#define REG_TYPE_ID_BITS			6
#define REG_TYPE_ID_MAX_VAL			((1 << REG_TYPE_ID_BITS) - 1)

/* width of block ID in bits */
#define BLOCK_ID_BITS				8
#define BLOCK_ID_MAX_VAL			((1 << BLOCK_ID_BITS) - 1)

/******************************** Idle Check *********************************/

/* max number of idle check predicate immediates */
#define MAX_IDLE_CHK_PRED_IMM		3

/* max number of idle check argument registers */
#define MAX_IDLE_CHK_READ_REGS		3

/* max number of idle check loops */
#define MAX_IDLE_CHK_LOOPS			0x10000

/* max idle check address increment */
#define MAX_IDLE_CHK_INCREMENT		0x10000

/* inicates an undefined idle check line index */
#define IDLE_CHK_UNDEFINED_LINE_IDX	0xffffff

/* max number of register values following the idle check header for LSI */
#define IDLE_CHK_MAX_LSI_DUMP_REGS	2

/* arguments for IDLE_CHK_MACRO_TYPE_QM_RD_WR */
#define IDLE_CHK_QM_RD_WR_PTR		0
#define IDLE_CHK_QM_RD_WR_BANK		1

/**************************************/
/* HSI Functions constants and macros */
/**************************************/

/* Number of VLAN priorities */
#define NUM_OF_VLAN_PRIORITIES			8

/* the MCP Trace meta data signautre is duplicated in the
 * perl script that generats the NVRAM images
 */
#define MCP_TRACE_META_IMAGE_SIGNATURE	0x669955aa

/* Maximal number of RAM lines occupied by FW Asserts data */
#define MAX_FW_ASSERTS_RAM_LINES		800

/*
 * Binary buffer header
 */
struct bin_buffer_hdr {
	__le32 offset
	    /* buffer offset in bytes from the beginning of the binary file */;
	__le32 length /* buffer length in bytes */;
};

/*
 * binary buffer types
 */
enum bin_buffer_type {
	BIN_BUF_FW_VER_INFO /* fw_ver_info struct */,
	BIN_BUF_INIT_CMD /* init commands */,
	BIN_BUF_INIT_VAL /* init data */,
	BIN_BUF_INIT_MODE_TREE /* init modes tree */,
	BIN_BUF_IRO /* internal RAM offsets array */,
	MAX_BIN_BUFFER_TYPE
};

/*
 * Chip IDs
 */
enum chip_ids {
	CHIP_BB_A0 /* BB A0 chip ID */,
	CHIP_BB_B0 /* BB B0 chip ID */,
	CHIP_K2 /* AH chip ID */,
	MAX_CHIP_IDS
};

/*
 * memory dump descriptor
 */
struct dbg_dump_mem_desc {
	__le32 dword0;
#define DBG_DUMP_MEM_DESC_ADDRESS_MASK         0xFFFFFF
#define DBG_DUMP_MEM_DESC_ADDRESS_SHIFT        0
#define DBG_DUMP_MEM_DESC_ASIC_CHIP_MASK_MASK  0xF
#define DBG_DUMP_MEM_DESC_ASIC_CHIP_MASK_SHIFT 24
#define DBG_DUMP_MEM_DESC_SIM_CHIP_MASK_MASK   0xF
#define DBG_DUMP_MEM_DESC_SIM_CHIP_MASK_SHIFT  28
	__le32 dword1;
#define DBG_DUMP_MEM_DESC_LENGTH_MASK          0x3FFFF
#define DBG_DUMP_MEM_DESC_LENGTH_SHIFT         0
#define DBG_DUMP_MEM_DESC_REG_TYPE_ID_MASK     0x3F
#define DBG_DUMP_MEM_DESC_REG_TYPE_ID_SHIFT    18
#define DBG_DUMP_MEM_DESC_BLOCK_ID_MASK        0xFF
#define DBG_DUMP_MEM_DESC_BLOCK_ID_SHIFT       24
};

/*
 * registers dump descriptor: chip
 */
struct dbg_dump_regs_chip_desc {
	__le32 data;
#define DBG_DUMP_REGS_CHIP_DESC_IS_CHIP_MASK_MASK    0x1
#define DBG_DUMP_REGS_CHIP_DESC_IS_CHIP_MASK_SHIFT   0
#define DBG_DUMP_REGS_CHIP_DESC_ASIC_CHIP_MASK_MASK  0x7FFFFF
#define DBG_DUMP_REGS_CHIP_DESC_ASIC_CHIP_MASK_SHIFT 1
#define DBG_DUMP_REGS_CHIP_DESC_SIM_CHIP_MASK_MASK   0xFF
#define DBG_DUMP_REGS_CHIP_DESC_SIM_CHIP_MASK_SHIFT  24
};

/*
 * registers dump descriptor: raw
 */
struct dbg_dump_regs_raw_desc {
	__le32 data;
#define DBG_DUMP_REGS_RAW_DESC_IS_CHIP_MASK_MASK  0x1
#define DBG_DUMP_REGS_RAW_DESC_IS_CHIP_MASK_SHIFT 0
#define DBG_DUMP_REGS_RAW_DESC_PARAM1_MASK        0x7FFFFF
#define DBG_DUMP_REGS_RAW_DESC_PARAM1_SHIFT       1
#define DBG_DUMP_REGS_RAW_DESC_PARAM2_MASK        0xFF
#define DBG_DUMP_REGS_RAW_DESC_PARAM2_SHIFT       24
};

/*
 * registers dump descriptor: sequence
 */
struct dbg_dump_regs_seq_desc {
	__le32 data;
#define DBG_DUMP_REGS_SEQ_DESC_IS_CHIP_MASK_MASK  0x1
#define DBG_DUMP_REGS_SEQ_DESC_IS_CHIP_MASK_SHIFT 0
#define DBG_DUMP_REGS_SEQ_DESC_ADDRESS_MASK       0x7FFFFF
#define DBG_DUMP_REGS_SEQ_DESC_ADDRESS_SHIFT      1
#define DBG_DUMP_REGS_SEQ_DESC_LENGTH_MASK        0xFF
#define DBG_DUMP_REGS_SEQ_DESC_LENGTH_SHIFT       24
};

/*
 * registers dump descriptor
 */
union dbg_dump_regs_desc {
	struct dbg_dump_regs_raw_desc raw /* dumped registers raw descriptor */
	   ;
	struct dbg_dump_regs_seq_desc seq /* dumped registers seq descriptor */
	   ;
	struct dbg_dump_regs_chip_desc chip
	    /* dumped registers chip descriptor */;
};

/*
 * idle check macro types
 */
enum idle_chk_macro_types {
	IDLE_CHK_MACRO_TYPE_COMPARE /* parametric register comparison */,
	IDLE_CHK_MACRO_TYPE_QM_RD_WR /* compare QM r/w pointers and banks */,
	MAX_IDLE_CHK_MACRO_TYPES
};

/*
 * Idle Check result header
 */
struct idle_chk_result_hdr {
	__le16 rule_idx /* Idle check rule index in CSV file */;
	__le16 loop_idx /* the loop index in which the failure occurred */;
	__le16 num_fw_values;
	__le16 data;
#define IDLE_CHK_RESULT_HDR_NUM_LSI_VALUES_MASK  0xF
#define IDLE_CHK_RESULT_HDR_NUM_LSI_VALUES_SHIFT 0
#define IDLE_CHK_RESULT_HDR_LOOP_VALID_MASK      0x1
#define IDLE_CHK_RESULT_HDR_LOOP_VALID_SHIFT     4
#define IDLE_CHK_RESULT_HDR_SEVERITY_MASK        0x7
#define IDLE_CHK_RESULT_HDR_SEVERITY_SHIFT       5
#define IDLE_CHK_RESULT_HDR_MACRO_TYPE_MASK      0xF
#define IDLE_CHK_RESULT_HDR_MACRO_TYPE_SHIFT     8
#define IDLE_CHK_RESULT_HDR_MACRO_TYPE_ARG_MASK  0xF
#define IDLE_CHK_RESULT_HDR_MACRO_TYPE_ARG_SHIFT 12
};

/*
 * Idle Check rule
 */
struct idle_chk_rule {
	__le32 data;
#define IDLE_CHK_RULE_ASIC_CHIP_MASK_MASK  0xF
#define IDLE_CHK_RULE_ASIC_CHIP_MASK_SHIFT 0
#define IDLE_CHK_RULE_SIM_CHIP_MASK_MASK   0xF
#define IDLE_CHK_RULE_SIM_CHIP_MASK_SHIFT  4
#define IDLE_CHK_RULE_BLOCK_ID_MASK        0xFF
#define IDLE_CHK_RULE_BLOCK_ID_SHIFT       8
#define IDLE_CHK_RULE_MACRO_TYPE_MASK      0xF
#define IDLE_CHK_RULE_MACRO_TYPE_SHIFT     16
#define IDLE_CHK_RULE_SEVERITY_MASK        0x7
#define IDLE_CHK_RULE_SEVERITY_SHIFT       20
#define IDLE_CHK_RULE_RESERVED_MASK        0x1
#define IDLE_CHK_RULE_RESERVED_SHIFT       23
#define IDLE_CHK_RULE_PRED_ID_MASK         0xFF
#define IDLE_CHK_RULE_PRED_ID_SHIFT        24
	__le16 loop;
	__le16 increment
	    /* address increment of first argument register on each iteration */
	   ;
	__le32 reg_addr[3];
	__le32 pred_imm[3]
	    /* immediate values passed as arguments to the idle check rule */;
};

/*
 * idle check severity types
 */
enum idle_chk_severity_types {
	IDLE_CHK_SEVERITY_ERROR /* idle check failure should cause an error */,
	IDLE_CHK_SEVERITY_ERROR_NO_TRAFFIC
	    ,
	IDLE_CHK_SEVERITY_WARNING
	    /* idle check failure should cause a warning */,
	MAX_IDLE_CHK_SEVERITY_TYPES
};

/*
 * init array header: raw
 */
struct init_array_raw_hdr {
	__le32 data;
#define INIT_ARRAY_RAW_HDR_TYPE_MASK    0xF
#define INIT_ARRAY_RAW_HDR_TYPE_SHIFT   0
#define INIT_ARRAY_RAW_HDR_PARAMS_MASK  0xFFFFFFF
#define INIT_ARRAY_RAW_HDR_PARAMS_SHIFT 4
};

/*
 * init array header: standard
 */
struct init_array_standard_hdr {
	__le32 data;
#define INIT_ARRAY_STANDARD_HDR_TYPE_MASK  0xF
#define INIT_ARRAY_STANDARD_HDR_TYPE_SHIFT 0
#define INIT_ARRAY_STANDARD_HDR_SIZE_MASK  0xFFFFFFF
#define INIT_ARRAY_STANDARD_HDR_SIZE_SHIFT 4
};

/*
 * init array header: zipped
 */
struct init_array_zipped_hdr {
	__le32 data;
#define INIT_ARRAY_ZIPPED_HDR_TYPE_MASK         0xF
#define INIT_ARRAY_ZIPPED_HDR_TYPE_SHIFT        0
#define INIT_ARRAY_ZIPPED_HDR_ZIPPED_SIZE_MASK  0xFFFFFFF
#define INIT_ARRAY_ZIPPED_HDR_ZIPPED_SIZE_SHIFT 4
};

/*
 * init array header: pattern
 */
struct init_array_pattern_hdr {
	__le32 data;
#define INIT_ARRAY_PATTERN_HDR_TYPE_MASK          0xF
#define INIT_ARRAY_PATTERN_HDR_TYPE_SHIFT         0
#define INIT_ARRAY_PATTERN_HDR_PATTERN_SIZE_MASK  0xF
#define INIT_ARRAY_PATTERN_HDR_PATTERN_SIZE_SHIFT 4
#define INIT_ARRAY_PATTERN_HDR_REPETITIONS_MASK   0xFFFFFF
#define INIT_ARRAY_PATTERN_HDR_REPETITIONS_SHIFT  8
};

/*
 * init array header union
 */
union init_array_hdr {
	struct init_array_raw_hdr raw /* raw init array header */;
	struct init_array_standard_hdr standard /* standard init array header */
	   ;
	struct init_array_zipped_hdr zipped /* zipped init array header */;
	struct init_array_pattern_hdr pattern /* pattern init array header */;
};

/*
 * init array types
 */
enum init_array_types {
	INIT_ARR_STANDARD /* standard init array */,
	INIT_ARR_ZIPPED /* zipped init array */,
	INIT_ARR_PATTERN /* a repeated pattern */,
	MAX_INIT_ARRAY_TYPES
};

/*
 * init operation: callback
 */
struct init_callback_op {
	__le32 op_data;
#define INIT_CALLBACK_OP_OP_MASK        0xF
#define INIT_CALLBACK_OP_OP_SHIFT       0
#define INIT_CALLBACK_OP_RESERVED_MASK  0xFFFFFFF
#define INIT_CALLBACK_OP_RESERVED_SHIFT 4
	__le16 callback_id /* Callback ID */;
	__le16 block_id /* Blocks ID */;
};

/*
 * init operation: delay
 */
struct init_delay_op {
	__le32 op_data;
#define INIT_DELAY_OP_OP_MASK        0xF
#define INIT_DELAY_OP_OP_SHIFT       0
#define INIT_DELAY_OP_RESERVED_MASK  0xFFFFFFF
#define INIT_DELAY_OP_RESERVED_SHIFT 4
	__le32 delay /* delay in us */;
};

/*
 * init operation: if_mode
 */
struct init_if_mode_op {
	__le32 op_data;
#define INIT_IF_MODE_OP_OP_MASK          0xF
#define INIT_IF_MODE_OP_OP_SHIFT         0
#define INIT_IF_MODE_OP_RESERVED1_MASK   0xFFF
#define INIT_IF_MODE_OP_RESERVED1_SHIFT  4
#define INIT_IF_MODE_OP_CMD_OFFSET_MASK  0xFFFF
#define INIT_IF_MODE_OP_CMD_OFFSET_SHIFT 16
	__le16 reserved2;
	__le16 modes_buf_offset
	    /* offset (in bytes) in modes expression buffer */;
};

/*
 * init operation: if_phase
 */
struct init_if_phase_op {
	__le32 op_data;
#define INIT_IF_PHASE_OP_OP_MASK           0xF
#define INIT_IF_PHASE_OP_OP_SHIFT          0
#define INIT_IF_PHASE_OP_DMAE_ENABLE_MASK  0x1
#define INIT_IF_PHASE_OP_DMAE_ENABLE_SHIFT 4
#define INIT_IF_PHASE_OP_RESERVED1_MASK    0x7FF
#define INIT_IF_PHASE_OP_RESERVED1_SHIFT   5
#define INIT_IF_PHASE_OP_CMD_OFFSET_MASK   0xFFFF
#define INIT_IF_PHASE_OP_CMD_OFFSET_SHIFT  16
	__le32 phase_data;
#define INIT_IF_PHASE_OP_PHASE_MASK        0xFF
#define INIT_IF_PHASE_OP_PHASE_SHIFT       0
#define INIT_IF_PHASE_OP_RESERVED2_MASK    0xFF
#define INIT_IF_PHASE_OP_RESERVED2_SHIFT   8
#define INIT_IF_PHASE_OP_PHASE_ID_MASK     0xFFFF
#define INIT_IF_PHASE_OP_PHASE_ID_SHIFT    16
};

/*
 * init mode operators
 */
enum init_mode_ops {
	INIT_MODE_OP_NOT /* init mode not operator */,
	INIT_MODE_OP_OR /* init mode or operator */,
	INIT_MODE_OP_AND /* init mode and operator */,
	MAX_INIT_MODE_OPS
};

/*
 * init operation: raw
 */
struct init_raw_op {
	__le32 op_data;
#define INIT_RAW_OP_OP_MASK      0xF
#define INIT_RAW_OP_OP_SHIFT     0
#define INIT_RAW_OP_PARAM1_MASK  0xFFFFFFF
#define INIT_RAW_OP_PARAM1_SHIFT 4
	__le32 param2 /* Init param 2 */;
};

/*
 * init array params
 */
struct init_op_array_params {
	__le16 size /* array size in dwords */;
	__le16 offset /* array start offset in dwords */;
};

/*
 * Write init operation arguments
 */
union init_write_args {
	__le32 inline_val
	    /* value to write, used when init source is INIT_SRC_INLINE */;
	__le32 zeros_count;
	__le32 array_offset
	    /* array offset to write, used when init source is INIT_SRC_ARRAY */
	   ;
	struct init_op_array_params runtime;
};

/*
 * init operation: write
 */
struct init_write_op {
	__le32 data;
#define INIT_WRITE_OP_OP_MASK        0xF
#define INIT_WRITE_OP_OP_SHIFT       0
#define INIT_WRITE_OP_SOURCE_MASK    0x7
#define INIT_WRITE_OP_SOURCE_SHIFT   4
#define INIT_WRITE_OP_RESERVED_MASK  0x1
#define INIT_WRITE_OP_RESERVED_SHIFT 7
#define INIT_WRITE_OP_WIDE_BUS_MASK  0x1
#define INIT_WRITE_OP_WIDE_BUS_SHIFT 8
#define INIT_WRITE_OP_ADDRESS_MASK   0x7FFFFF
#define INIT_WRITE_OP_ADDRESS_SHIFT  9
	union init_write_args args /* Write init operation arguments */;
};

/*
 * init operation: read
 */
struct init_read_op {
	__le32 op_data;
#define INIT_READ_OP_OP_MASK         0xF
#define INIT_READ_OP_OP_SHIFT        0
#define INIT_READ_OP_POLL_TYPE_MASK  0xF
#define INIT_READ_OP_POLL_TYPE_SHIFT 4
#define INIT_READ_OP_RESERVED_MASK   0x1
#define INIT_READ_OP_RESERVED_SHIFT  8
#define INIT_READ_OP_ADDRESS_MASK    0x7FFFFF
#define INIT_READ_OP_ADDRESS_SHIFT   9
	__le32 expected_val
	    /* expected polling value, used only when polling is done */;
};

/*
 * Init operations union
 */
union init_op {
	struct init_raw_op raw /* raw init operation */;
	struct init_write_op write /* write init operation */;
	struct init_read_op read /* read init operation */;
	struct init_if_mode_op if_mode /* if_mode init operation */;
	struct init_if_phase_op if_phase /* if_phase init operation */;
	struct init_callback_op callback /* callback init operation */;
	struct init_delay_op delay /* delay init operation */;
};

/*
 * Init command operation types
 */
enum init_op_types {
	INIT_OP_READ /* GRC read init command */,
	INIT_OP_WRITE /* GRC write init command */,
	INIT_OP_IF_MODE
	    /* Skip init commands if the init modes expression doesn't match */,
	INIT_OP_IF_PHASE
	    /* Skip init commands if the init phase doesn't match */,
	INIT_OP_DELAY /* delay init command */,
	INIT_OP_CALLBACK /* callback init command */,
	MAX_INIT_OP_TYPES
};

/*
 * init polling types
 */
enum init_poll_types {
	INIT_POLL_NONE /* No polling */,
	INIT_POLL_EQ /* init value is included in the init command */,
	INIT_POLL_OR /* init value is all zeros */,
	INIT_POLL_AND /* init value is an array of values */,
	MAX_INIT_POLL_TYPES
};

/*
 * init source types
 */
enum init_source_types {
	INIT_SRC_INLINE /* init value is included in the init command */,
	INIT_SRC_ZEROS /* init value is all zeros */,
	INIT_SRC_ARRAY /* init value is an array of values */,
	INIT_SRC_RUNTIME /* init value is provided during runtime */,
	MAX_INIT_SOURCE_TYPES
};

/*
 * Internal RAM Offsets macro data
 */
struct iro {
	__le32 base /* RAM field offset */;
	__le16 m1 /* multiplier 1 */;
	__le16 m2 /* multiplier 2 */;
	__le16 m3 /* multiplier 3 */;
	__le16 size /* RAM field size */;
};

/*
 * register descriptor
 */
struct reg_desc {
	__le32 data;
#define REG_DESC_ADDRESS_MASK  0xFFFFFF
#define REG_DESC_ADDRESS_SHIFT 0
#define REG_DESC_SIZE_MASK     0xFF
#define REG_DESC_SIZE_SHIFT    24
};

/*
 * Debug Bus block data
 */
struct dbg_bus_block_data {
	u8 enabled /* Indicates if the block is enabled for recording (0/1) */;
	u8 hw_id /* HW ID associated with the block */;
	u8 line_num /* Debug line number to select */;
	u8 right_shift /* Number of units to  right the debug data (0-3) */;
	u8 cycle_en /* 4-bit value: bit i set -> unit i is enabled. */;
	u8 force_valid /* 4-bit value: bit i set -> unit i is forced valid. */;
	u8 force_frame
	    /* 4-bit value: bit i set -> unit i frame bit is forced. */;
	u8 reserved;
};

/*
 * Debug Bus Clients
 */
enum dbg_bus_clients {
	DBG_BUS_CLIENT_RBCN,
	DBG_BUS_CLIENT_RBCP,
	DBG_BUS_CLIENT_RBCR,
	DBG_BUS_CLIENT_RBCT,
	DBG_BUS_CLIENT_RBCU,
	DBG_BUS_CLIENT_RBCF,
	DBG_BUS_CLIENT_RBCX,
	DBG_BUS_CLIENT_RBCS,
	DBG_BUS_CLIENT_RBCH,
	DBG_BUS_CLIENT_RBCZ,
	DBG_BUS_CLIENT_OTHER_ENGINE,
	DBG_BUS_CLIENT_TIMESTAMP,
	DBG_BUS_CLIENT_CPU,
	DBG_BUS_CLIENT_RBCY,
	DBG_BUS_CLIENT_RBCQ,
	DBG_BUS_CLIENT_RBCM,
	DBG_BUS_CLIENT_RBCB,
	DBG_BUS_CLIENT_RBCW,
	DBG_BUS_CLIENT_RBCV,
	MAX_DBG_BUS_CLIENTS
};

/*
 * Debug Bus constraint operation types
 */
enum dbg_bus_constraint_ops {
	DBG_BUS_CONSTRAINT_OP_EQ /* equal */,
	DBG_BUS_CONSTRAINT_OP_NE /* not equal */,
	DBG_BUS_CONSTRAINT_OP_LT /* less than */,
	DBG_BUS_CONSTRAINT_OP_LTC /* less than (cyclic) */,
	DBG_BUS_CONSTRAINT_OP_LE /* less than or equal */,
	DBG_BUS_CONSTRAINT_OP_LEC /* less than or equal (cyclic) */,
	DBG_BUS_CONSTRAINT_OP_GT /* greater than */,
	DBG_BUS_CONSTRAINT_OP_GTC /* greater than (cyclic) */,
	DBG_BUS_CONSTRAINT_OP_GE /* greater than or equal */,
	DBG_BUS_CONSTRAINT_OP_GEC /* greater than or equal (cyclic) */,
	MAX_DBG_BUS_CONSTRAINT_OPS
};

/*
 * Debug Bus memory address
 */
struct dbg_bus_mem_addr {
	__le32 lo;
	__le32 hi;
};

/*
 * Debug Bus PCI buffer data
 */
struct dbg_bus_pci_buf_data {
	struct dbg_bus_mem_addr phys_addr /* PCI buffer physical address */;
	struct dbg_bus_mem_addr virt_addr /* PCI buffer virtual address */;
	__le32 size /* PCI buffer size in bytes */;
};

/*
 * Debug Bus Storm EID range filter params
 */
struct dbg_bus_storm_eid_range_params {
	u8 min /* Minimal event ID to filter on */;
	u8 max /* Maximal event ID to filter on */;
};

/*
 * Debug Bus Storm EID mask filter params
 */
struct dbg_bus_storm_eid_mask_params {
	u8 val /* Event ID value */;
	u8 mask /* Event ID mask. 1s in the mask = dont care bits. */;
};

/*
 * Debug Bus Storm EID filter params
 */
union dbg_bus_storm_eid_params {
	struct dbg_bus_storm_eid_range_params range
	    /* EID range filter params */;
	struct dbg_bus_storm_eid_mask_params mask /* EID mask filter params */;
};

/*
 * Debug Bus Storm data
 */
struct dbg_bus_storm_data {
	u8 fast_enabled;
	u8 fast_mode
	    /* Fast debug Storm mode, valid only if fast_enabled is set */;
	u8 slow_enabled;
	u8 slow_mode
	    /* Slow debug Storm mode, valid only if slow_enabled is set */;
	u8 hw_id /* HW ID associated with the Storm */;
	u8 eid_filter_en /* Indicates if EID filtering is performed (0/1) */;
	u8 eid_range_not_mask;
	u8 cid_filter_en /* Indicates if CID filtering is performed (0/1) */;
	union dbg_bus_storm_eid_params eid_filter_params;
	__le16 reserved;
	__le32 cid /* CID to filter on. Valid only if cid_filter_en is set. */;
};

/*
 * Debug Bus data
 */
struct dbg_bus_data {
	__le32 app_version /* The tools version number of the application */;
	u8 state /* The current debug bus state */;
	u8 hw_dwords /* HW dwords per cycle */;
	u8 next_hw_id /* Next HW ID to be associated with an input */;
	u8 num_enabled_blocks /* Number of blocks enabled for recording */;
	u8 num_enabled_storms /* Number of Storms enabled for recording */;
	u8 target /* Output target */;
	u8 next_trigger_state /* ID of next trigger state to be added */;
	u8 next_constraint_id
	    /* ID of next filter/trigger constraint to be added */;
	u8 one_shot_en /* Indicates if one-shot mode is enabled (0/1) */;
	u8 grc_input_en /* Indicates if GRC recording is enabled (0/1) */;
	u8 timestamp_input_en
	    /* Indicates if timestamp recording is enabled (0/1) */;
	u8 filter_en /* Indicates if the recording filter is enabled (0/1) */;
	u8 trigger_en /* Indicates if the recording trigger is enabled (0/1) */
	   ;
	u8 adding_filter;
	u8 filter_pre_trigger;
	u8 filter_post_trigger;
	u8 unify_inputs;
	u8 rcv_from_other_engine;
	struct dbg_bus_pci_buf_data pci_buf;
	__le16 reserved;
	struct dbg_bus_block_data blocks[80] /* Debug Bus data for each block */
	   ;
	struct dbg_bus_storm_data storms[6] /* Debug Bus data for each block */
	   ;
};

/*
 * Debug bus filter types
 */
enum dbg_bus_filter_types {
	DBG_BUS_FILTER_TYPE_OFF /* filter always off */,
	DBG_BUS_FILTER_TYPE_PRE /* filter before trigger only */,
	DBG_BUS_FILTER_TYPE_POST /* filter after trigger only */,
	DBG_BUS_FILTER_TYPE_ON /* filter always on */,
	MAX_DBG_BUS_FILTER_TYPES
};

/*
 * Debug bus frame modes
 */
enum dbg_bus_frame_modes {
	DBG_BUS_FRAME_MODE_0HW_4ST = 0 /* 0 HW dwords, 4 Storm dwords */,
	DBG_BUS_FRAME_MODE_4HW_0ST = 3 /* 4 HW dwords, 0 Storm dwords */,
	DBG_BUS_FRAME_MODE_8HW_0ST = 4 /* 8 HW dwords, 0 Storm dwords */,
	MAX_DBG_BUS_FRAME_MODES
};

/*
 * Debug bus input types
 */
enum dbg_bus_input_types {
	DBG_BUS_INPUT_TYPE_STORM,
	DBG_BUS_INPUT_TYPE_BLOCK,
	MAX_DBG_BUS_INPUT_TYPES
};

/*
 * Debug bus other engine mode
 */
enum dbg_bus_other_engine_modes {
	DBG_BUS_OTHER_ENGINE_MODE_NONE,
	DBG_BUS_OTHER_ENGINE_MODE_DOUBLE_BW_TX,
	DBG_BUS_OTHER_ENGINE_MODE_DOUBLE_BW_RX,
	DBG_BUS_OTHER_ENGINE_MODE_CROSS_ENGINE_TX,
	DBG_BUS_OTHER_ENGINE_MODE_CROSS_ENGINE_RX,
	MAX_DBG_BUS_OTHER_ENGINE_MODES
};

/*
 * Debug bus post-trigger recording types
 */
enum dbg_bus_post_trigger_types {
	DBG_BUS_POST_TRIGGER_RECORD /* start recording after trigger */,
	DBG_BUS_POST_TRIGGER_DROP /* drop data after trigger */,
	MAX_DBG_BUS_POST_TRIGGER_TYPES
};

/*
 * Debug bus pre-trigger recording types
 */
enum dbg_bus_pre_trigger_types {
	DBG_BUS_PRE_TRIGGER_START_FROM_ZERO /* start recording from time 0 */,
	DBG_BUS_PRE_TRIGGER_NUM_CHUNKS
	    /* start recording some chunks before trigger */,
	DBG_BUS_PRE_TRIGGER_DROP /* drop data before trigger */,
	MAX_DBG_BUS_PRE_TRIGGER_TYPES
};

/*
 * Debug bus SEMI frame modes
 */
enum dbg_bus_semi_frame_modes {
	DBG_BUS_SEMI_FRAME_MODE_0SLOW_4FAST =
	    0 /* 0 slow dwords, 4 fast dwords */,
	DBG_BUS_SEMI_FRAME_MODE_4SLOW_0FAST =
	    3 /* 4 slow dwords, 0 fast dwords */,
	MAX_DBG_BUS_SEMI_FRAME_MODES
};

/*
 * Debug bus states
 */
enum dbg_bus_states {
	DBG_BUS_STATE_BEFORE_RECORD /* before debug bus the recording starts */
	    ,
	DBG_BUS_STATE_DURING_RECORD /* during debug bus recording */,
	DBG_BUS_STATE_AFTER_RECORD /* after debug bus recording */,
	MAX_DBG_BUS_STATES
};

/*
 * Debug Bus Storm modes
 */
enum dbg_bus_storm_modes {
	DBG_BUS_STORM_MODE_PRINTF /* store data (fast debug) */,
	DBG_BUS_STORM_MODE_PRAM_ADDR /* pram address (fast debug) */,
	DBG_BUS_STORM_MODE_DRA_RW /* DRA read/write data (fast debug) */,
	DBG_BUS_STORM_MODE_DRA_W /* DRA write data (fast debug) */,
	DBG_BUS_STORM_MODE_LD_ST_ADDR /* load/store address (fast debug) */,
	DBG_BUS_STORM_MODE_DRA_FSM /* DRA state machines (fast debug) */,
	DBG_BUS_STORM_MODE_RH /* recording handlers (fast debug) */,
	DBG_BUS_STORM_MODE_FOC /* FOC: FIN + DRA Rd (slow debug) */,
	DBG_BUS_STORM_MODE_EXT_STORE /* FOC: External Store (slow) */,
	MAX_DBG_BUS_STORM_MODES
};

/*
 * Debug bus target IDs
 */
enum dbg_bus_targets {
	DBG_BUS_TARGET_ID_INT_BUF
	    /* records debug bus to DBG block internal buffer */,
	DBG_BUS_TARGET_ID_NIG /* records debug bus to the NW */,
	DBG_BUS_TARGET_ID_PCI /* records debug bus to a PCI buffer */,
	MAX_DBG_BUS_TARGETS
};

/*
 * GRC Dump data
 */
struct dbg_grc_data {
	u8 is_updated /* Indicates if the GRC Dump data is updated (0/1) */;
	u8 chip_id /* Chip ID */;
	u8 chip_mask /* Chip mask */;
	u8 reserved;
	__le32 max_dump_dwords /* Max GRC Dump size in dwords */;
	__le32 param_val[40];
	u8 param_set_by_user[40];
};

/*
 * Debug GRC params
 */
enum dbg_grc_params {
	DBG_GRC_PARAM_DUMP_TSTORM /* dump Tstorm memories (0/1) */,
	DBG_GRC_PARAM_DUMP_MSTORM /* dump Mstorm memories (0/1) */,
	DBG_GRC_PARAM_DUMP_USTORM /* dump Ustorm memories (0/1) */,
	DBG_GRC_PARAM_DUMP_XSTORM /* dump Xstorm memories (0/1) */,
	DBG_GRC_PARAM_DUMP_YSTORM /* dump Ystorm memories (0/1) */,
	DBG_GRC_PARAM_DUMP_PSTORM /* dump Pstorm memories (0/1) */,
	DBG_GRC_PARAM_DUMP_REGS /* dump non-memory registers (0/1) */,
	DBG_GRC_PARAM_DUMP_RAM /* dump Storm internal RAMs (0/1) */,
	DBG_GRC_PARAM_DUMP_PBUF /* dump Storm passive buffer (0/1) */,
	DBG_GRC_PARAM_DUMP_IOR /* dump Storm IORs (0/1) */,
	DBG_GRC_PARAM_DUMP_VFC /* dump VFC memories (0/1) */,
	DBG_GRC_PARAM_DUMP_CM_CTX /* dump CM contexts (0/1) */,
	DBG_GRC_PARAM_DUMP_PXP /* dump PXP memories (0/1) */,
	DBG_GRC_PARAM_DUMP_RSS /* dump RSS memories (0/1) */,
	DBG_GRC_PARAM_DUMP_CAU /* dump CAU memories (0/1) */,
	DBG_GRC_PARAM_DUMP_QM /* dump QM memories (0/1) */,
	DBG_GRC_PARAM_DUMP_MCP /* dump MCP memories (0/1) */,
	DBG_GRC_PARAM_RESERVED /* reserved */,
	DBG_GRC_PARAM_DUMP_CFC /* dump CFC memories (0/1) */,
	DBG_GRC_PARAM_DUMP_IGU /* dump IGU memories (0/1) */,
	DBG_GRC_PARAM_DUMP_BRB /* dump BRB memories (0/1) */,
	DBG_GRC_PARAM_DUMP_BTB /* dump BTB memories (0/1) */,
	DBG_GRC_PARAM_DUMP_BMB /* dump BMB memories (0/1) */,
	DBG_GRC_PARAM_DUMP_NIG /* dump NIG memories (0/1) */,
	DBG_GRC_PARAM_DUMP_MULD /* dump MULD memories (0/1) */,
	DBG_GRC_PARAM_DUMP_PRS /* dump PRS memories (0/1) */,
	DBG_GRC_PARAM_DUMP_DMAE /* dump PRS memories (0/1) */,
	DBG_GRC_PARAM_DUMP_TM /* dump TM (timers) memories (0/1) */,
	DBG_GRC_PARAM_DUMP_SDM /* dump SDM memories (0/1) */,
	DBG_GRC_PARAM_DUMP_STATIC /* dump static debug data (0/1) */,
	DBG_GRC_PARAM_UNSTALL /* un-stall Storms after dump (0/1) */,
	DBG_GRC_PARAM_NUM_LCIDS /* number of LCIDs (0..320) */,
	DBG_GRC_PARAM_NUM_LTIDS /* number of LTIDs (0..320) */,
	DBG_GRC_PARAM_EXCLUDE_ALL
	    /* preset: exclude all memories from dump (1 only) */,
	DBG_GRC_PARAM_CRASH
	    /* preset: include memories for crash dump (1 only) */,
	DBG_GRC_PARAM_PARITY_SAFE
	    /* perform dump only if MFW is responding (0/1) */,
	DBG_GRC_PARAM_DUMP_CM /* dump CM memories (0/1) */,
	MAX_DBG_GRC_PARAMS
};

/*
 * Debug reset registers
 */
enum dbg_reset_regs {
	DBG_RESET_REG_MISCS_PL_UA,
	DBG_RESET_REG_MISCS_PL_HV,
	DBG_RESET_REG_MISC_PL_UA,
	DBG_RESET_REG_MISC_PL_HV,
	DBG_RESET_REG_MISC_PL_PDA_VMAIN_1,
	DBG_RESET_REG_MISC_PL_PDA_VMAIN_2,
	DBG_RESET_REG_MISC_PL_PDA_VAUX,
	MAX_DBG_RESET_REGS
};

/*
 * @DPDK Debug status codes
 */
enum dbg_status {
	DBG_STATUS_OK,
	DBG_STATUS_APP_VERSION_NOT_SET,
	DBG_STATUS_UNSUPPORTED_APP_VERSION,
	DBG_STATUS_DBG_BLOCK_NOT_RESET,
	DBG_STATUS_INVALID_ARGS,
	DBG_STATUS_OUTPUT_ALREADY_SET,
	DBG_STATUS_INVALID_PCI_BUF_SIZE,
	DBG_STATUS_PCI_BUF_ALLOC_FAILED,
	DBG_STATUS_PCI_BUF_NOT_ALLOCATED,
	DBG_STATUS_TOO_MANY_INPUTS,
	DBG_STATUS_INPUT_OVERLAP,
	DBG_STATUS_HW_ONLY_RECORDING,
	DBG_STATUS_STORM_ALREADY_ENABLED,
	DBG_STATUS_STORM_NOT_ENABLED,
	DBG_STATUS_BLOCK_ALREADY_ENABLED,
	DBG_STATUS_BLOCK_NOT_ENABLED,
	DBG_STATUS_NO_INPUT_ENABLED,
	DBG_STATUS_NO_FILTER_TRIGGER_64B,
	DBG_STATUS_FILTER_ALREADY_ENABLED,
	DBG_STATUS_TRIGGER_ALREADY_ENABLED,
	DBG_STATUS_TRIGGER_NOT_ENABLED,
	DBG_STATUS_CANT_ADD_CONSTRAINT,
	DBG_STATUS_TOO_MANY_TRIGGER_STATES,
	DBG_STATUS_TOO_MANY_CONSTRAINTS,
	DBG_STATUS_RECORDING_NOT_STARTED,
	DBG_STATUS_NO_DATA_TRIGGERED,
	DBG_STATUS_NO_DATA_RECORDED,
	DBG_STATUS_DUMP_BUF_TOO_SMALL,
	DBG_STATUS_DUMP_NOT_CHUNK_ALIGNED,
	DBG_STATUS_UNKNOWN_CHIP,
	DBG_STATUS_VIRT_MEM_ALLOC_FAILED,
	DBG_STATUS_BLOCK_IN_RESET,
	DBG_STATUS_INVALID_TRACE_SIGNATURE,
	DBG_STATUS_INVALID_NVRAM_BUNDLE,
	DBG_STATUS_NVRAM_GET_IMAGE_FAILED,
	DBG_STATUS_NON_ALIGNED_NVRAM_IMAGE,
	DBG_STATUS_NVRAM_READ_FAILED,
	DBG_STATUS_IDLE_CHK_PARSE_FAILED,
	DBG_STATUS_MCP_TRACE_BAD_DATA,
	DBG_STATUS_MCP_TRACE_NO_META,
	DBG_STATUS_MCP_COULD_NOT_HALT,
	DBG_STATUS_MCP_COULD_NOT_RESUME,
	DBG_STATUS_DMAE_FAILED,
	DBG_STATUS_SEMI_FIFO_NOT_EMPTY,
	DBG_STATUS_IGU_FIFO_BAD_DATA,
	DBG_STATUS_MCP_COULD_NOT_MASK_PRTY,
	DBG_STATUS_FW_ASSERTS_PARSE_FAILED,
	DBG_STATUS_REG_FIFO_BAD_DATA,
	DBG_STATUS_PROTECTION_OVERRIDE_BAD_DATA,
	MAX_DBG_STATUS
};

/*
 * Debug Storms IDs
 */
enum dbg_storms {
	DBG_TSTORM_ID,
	DBG_MSTORM_ID,
	DBG_USTORM_ID,
	DBG_XSTORM_ID,
	DBG_YSTORM_ID,
	DBG_PSTORM_ID,
	MAX_DBG_STORMS
};

/*
 * Idle Check data
 */
struct idle_chk_data {
	__le32 buf_size /* Idle check buffer size in dwords */;
	u8 buf_size_set
	    /* Indicates if the idle check buffer size was set (0/1) */;
	u8 reserved1;
	__le16 reserved2;
};

/*
 * Idle Check data
 */
struct mcp_trace_data {
	__le32 buf_size /* MCP Trace buffer size in dwords */;
	u8 buf_size_set
	    /* Indicates if the MCP Trace buffer size was set (0/1) */;
	u8 reserved1;
	__le16 reserved2;
};

/*
 * Debug Tools data (per HW function)
 */
struct dbg_tools_data {
	struct dbg_grc_data grc /* GRC Dump data */;
	struct dbg_bus_data bus /* Debug Bus data */;
	struct idle_chk_data idle_chk /* Idle Check data */;
	struct mcp_trace_data mcp_trace /* MCP Trace data */;
	u8 block_in_reset[80] /* Indicates if a block is in reset state (0/1) */
	   ;
	u8 chip_id /* Chip ID (from enum chip_ids) */;
	u8 chip_mask
	    /* Chip mask = bit index chip_id is set, the rest are cleared */;
	u8 initialized /* Indicates if the data was initialized */;
	u8 reset_state_updated
	    /* Indicates if blocks reset state is updated (0/1) */;
};

/*
 * BRB RAM init requirements
 */
struct init_brb_ram_req {
	__le32 guranteed_per_tc /* guaranteed size per TC, in bytes */;
	__le32 headroom_per_tc /* headroom size per TC, in bytes */;
	__le32 min_pkt_size /* min packet size, in bytes */;
	__le32 max_ports_per_engine /* min packet size, in bytes */;
	u8 num_active_tcs[MAX_NUM_PORTS] /* number of active TCs per port */;
};

/*
 * ETS per-TC init requirements
 */
struct init_ets_tc_req {
	u8 use_sp;
	u8 use_wfq;
	__le16 weight /* An arbitration weight. Valid only if use_wfq is set. */
	   ;
};

/*
 * ETS init requirements
 */
struct init_ets_req {
	__le32 mtu /* Max packet size (in bytes) */;
	struct init_ets_tc_req tc_req[NUM_OF_TCS]
	    /* ETS initialization requirements per TC. */;
};

/*
 * NIG LB RL init requirements
 */
struct init_nig_lb_rl_req {
	__le16 lb_mac_rate;
	__le16 lb_rate;
	__le32 mtu /* Max packet size (in bytes) */;
	__le16 tc_rate[NUM_OF_PHYS_TCS];
};

/*
 * NIG TC mapping for each priority
 */
struct init_nig_pri_tc_map_entry {
	u8 tc_id /* the mapped TC ID */;
	u8 valid /* indicates if the mapping entry is valid */;
};

/*
 * NIG priority to TC map init requirements
 */
struct init_nig_pri_tc_map_req {
	struct init_nig_pri_tc_map_entry pri[NUM_OF_VLAN_PRIORITIES];
};

/*
 * QM per-port init parameters
 */
struct init_qm_port_params {
	u8 active /* Indicates if this port is active */;
	u8 num_active_phys_tcs /* number of physical TCs used by this port */;
	__le16 num_pbf_cmd_lines
	    /* number of PBF command lines that can be used by this port */;
	__le16 num_btb_blocks
	    /* number of BTB blocks that can be used by this port */;
	__le16 reserved;
};

/*
 * QM per-PQ init parameters
 */
struct init_qm_pq_params {
	u8 vport_id /* VPORT ID */;
	u8 tc_id /* TC ID */;
	u8 wrr_group /* WRR group */;
	u8 reserved;
};

/*
 * QM per-vport init parameters
 */
struct init_qm_vport_params {
	__le32 vport_rl;
	__le16 vport_wfq;
	__le16 first_tx_pq_id[NUM_OF_TCS]
	    /* the first Tx PQ ID associated with this VPORT for each TC. */;
};

#endif /* __ECORE_HSI_TOOLS__ */
