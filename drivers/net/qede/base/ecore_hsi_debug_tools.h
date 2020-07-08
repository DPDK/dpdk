/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016 - 2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#ifndef __ECORE_HSI_DEBUG_TOOLS__
#define __ECORE_HSI_DEBUG_TOOLS__
/****************************************/
/* Debug Tools HSI constants and macros */
/****************************************/


enum block_id {
	BLOCK_GRC,
	BLOCK_MISCS,
	BLOCK_MISC,
	BLOCK_DBU,
	BLOCK_PGLUE_B,
	BLOCK_CNIG,
	BLOCK_CPMU,
	BLOCK_NCSI,
	BLOCK_OPTE,
	BLOCK_BMB,
	BLOCK_PCIE,
	BLOCK_MCP,
	BLOCK_MCP2,
	BLOCK_PSWHST,
	BLOCK_PSWHST2,
	BLOCK_PSWRD,
	BLOCK_PSWRD2,
	BLOCK_PSWWR,
	BLOCK_PSWWR2,
	BLOCK_PSWRQ,
	BLOCK_PSWRQ2,
	BLOCK_PGLCS,
	BLOCK_DMAE,
	BLOCK_PTU,
	BLOCK_TCM,
	BLOCK_MCM,
	BLOCK_UCM,
	BLOCK_XCM,
	BLOCK_YCM,
	BLOCK_PCM,
	BLOCK_QM,
	BLOCK_TM,
	BLOCK_DORQ,
	BLOCK_BRB,
	BLOCK_SRC,
	BLOCK_PRS,
	BLOCK_TSDM,
	BLOCK_MSDM,
	BLOCK_USDM,
	BLOCK_XSDM,
	BLOCK_YSDM,
	BLOCK_PSDM,
	BLOCK_TSEM,
	BLOCK_MSEM,
	BLOCK_USEM,
	BLOCK_XSEM,
	BLOCK_YSEM,
	BLOCK_PSEM,
	BLOCK_RSS,
	BLOCK_TMLD,
	BLOCK_MULD,
	BLOCK_YULD,
	BLOCK_XYLD,
	BLOCK_PRM,
	BLOCK_PBF_PB1,
	BLOCK_PBF_PB2,
	BLOCK_RPB,
	BLOCK_BTB,
	BLOCK_PBF,
	BLOCK_RDIF,
	BLOCK_TDIF,
	BLOCK_CDU,
	BLOCK_CCFC,
	BLOCK_TCFC,
	BLOCK_IGU,
	BLOCK_CAU,
	BLOCK_UMAC,
	BLOCK_XMAC,
	BLOCK_MSTAT,
	BLOCK_DBG,
	BLOCK_NIG,
	BLOCK_WOL,
	BLOCK_BMBN,
	BLOCK_IPC,
	BLOCK_NWM,
	BLOCK_NWS,
	BLOCK_MS,
	BLOCK_PHY_PCIE,
	BLOCK_LED,
	BLOCK_AVS_WRAP,
	BLOCK_PXPREQBUS,
	BLOCK_BAR0_MAP,
	BLOCK_MCP_FIO,
	BLOCK_LAST_INIT,
	BLOCK_PRS_FC,
	BLOCK_PBF_FC,
	BLOCK_NIG_LB_FC,
	BLOCK_NIG_LB_FC_PLLH,
	BLOCK_NIG_TX_FC_PLLH,
	BLOCK_NIG_TX_FC,
	BLOCK_NIG_RX_FC_PLLH,
	BLOCK_NIG_RX_FC,
	MAX_BLOCK_ID
};


/*
 * binary debug buffer types
 */
enum bin_dbg_buffer_type {
	BIN_BUF_DBG_MODE_TREE /* init modes tree */,
	BIN_BUF_DBG_DUMP_REG /* GRC Dump registers */,
	BIN_BUF_DBG_DUMP_MEM /* GRC Dump memories */,
	BIN_BUF_DBG_IDLE_CHK_REGS /* Idle Check registers */,
	BIN_BUF_DBG_IDLE_CHK_IMMS /* Idle Check immediates */,
	BIN_BUF_DBG_IDLE_CHK_RULES /* Idle Check rules */,
	BIN_BUF_DBG_IDLE_CHK_PARSING_DATA /* Idle Check parsing data */,
	BIN_BUF_DBG_ATTN_BLOCKS /* Attention blocks */,
	BIN_BUF_DBG_ATTN_REGS /* Attention registers */,
	BIN_BUF_DBG_ATTN_INDEXES /* Attention indexes */,
	BIN_BUF_DBG_ATTN_NAME_OFFSETS /* Attention name offsets */,
	BIN_BUF_DBG_BLOCKS /* Blocks debug data */,
	BIN_BUF_DBG_BLOCKS_CHIP_DATA /* Blocks debug chip data */,
	BIN_BUF_DBG_BUS_LINES /* Blocks debug bus lines */,
	BIN_BUF_DBG_BLOCKS_USER_DATA /* Blocks debug user data */,
	BIN_BUF_DBG_BLOCKS_CHIP_USER_DATA /* Blocks debug chip user data */,
	BIN_BUF_DBG_BUS_LINE_NAME_OFFSETS /* Debug Bus line name offsets */,
	BIN_BUF_DBG_RESET_REGS /* Reset registers */,
	BIN_BUF_DBG_PARSING_STRINGS /* Debug Tools parsing strings */,
	MAX_BIN_DBG_BUFFER_TYPE
};


/*
 * Attention bit mapping
 */
struct dbg_attn_bit_mapping {
	u16 data;
/* The index of an attention in the blocks attentions list
 * (if is_unused_bit_cnt=0), or a number of consecutive unused attention bits
 * (if is_unused_bit_cnt=1)
 */
#define DBG_ATTN_BIT_MAPPING_VAL_MASK                0x7FFF
#define DBG_ATTN_BIT_MAPPING_VAL_SHIFT               0
/* if set, the val field indicates the number of consecutive unused attention
 * bits
 */
#define DBG_ATTN_BIT_MAPPING_IS_UNUSED_BIT_CNT_MASK  0x1
#define DBG_ATTN_BIT_MAPPING_IS_UNUSED_BIT_CNT_SHIFT 15
};


/*
 * Attention block per-type data
 */
struct dbg_attn_block_type_data {
/* Offset of this block attention names in the debug attention name offsets
 * array
 */
	u16 names_offset;
	u16 reserved1;
	u8 num_regs /* Number of attention registers in this block */;
	u8 reserved2;
/* Offset of this blocks attention registers in the attention registers array
 * (in dbg_attn_reg units)
 */
	u16 regs_offset;
};

/*
 * Block attentions
 */
struct dbg_attn_block {
/* attention block per-type data. Count must match the number of elements in
 * dbg_attn_type.
 */
	struct dbg_attn_block_type_data per_type_data[2];
};


/*
 * Attention register result
 */
struct dbg_attn_reg_result {
	u32 data;
/* STS attention register GRC address (in dwords) */
#define DBG_ATTN_REG_RESULT_STS_ADDRESS_MASK   0xFFFFFF
#define DBG_ATTN_REG_RESULT_STS_ADDRESS_SHIFT  0
/* Number of attention indexes in this register */
#define DBG_ATTN_REG_RESULT_NUM_REG_ATTN_MASK  0xFF
#define DBG_ATTN_REG_RESULT_NUM_REG_ATTN_SHIFT 24
/* The offset of this registers attentions within the blocks attentions list
 * (a value in the range 0..number of block attentions-1)
 */
	u16 block_attn_offset;
	u16 reserved;
	u32 sts_val /* Value read from the STS attention register */;
	u32 mask_val /* Value read from the MASK attention register */;
};

/*
 * Attention block result
 */
struct dbg_attn_block_result {
	u8 block_id /* Registers block ID */;
	u8 data;
/* Value from dbg_attn_type enum */
#define DBG_ATTN_BLOCK_RESULT_ATTN_TYPE_MASK  0x3
#define DBG_ATTN_BLOCK_RESULT_ATTN_TYPE_SHIFT 0
/* Number of registers in block in which at least one attention bit is set */
#define DBG_ATTN_BLOCK_RESULT_NUM_REGS_MASK   0x3F
#define DBG_ATTN_BLOCK_RESULT_NUM_REGS_SHIFT  2
/* Offset of this registers block attention names in the attention name offsets
 * array
 */
	u16 names_offset;
/* result data for each register in the block in which at least one attention
 * bit is set
 */
	struct dbg_attn_reg_result reg_results[15];
};



/*
 * mode header
 */
struct dbg_mode_hdr {
	u16 data;
/* indicates if a mode expression should be evaluated (0/1) */
#define DBG_MODE_HDR_EVAL_MODE_MASK         0x1
#define DBG_MODE_HDR_EVAL_MODE_SHIFT        0
/* offset (in bytes) in modes expression buffer. valid only if eval_mode is
 * set.
 */
#define DBG_MODE_HDR_MODES_BUF_OFFSET_MASK  0x7FFF
#define DBG_MODE_HDR_MODES_BUF_OFFSET_SHIFT 1
};

/*
 * Attention register
 */
struct dbg_attn_reg {
	struct dbg_mode_hdr mode /* Mode header */;
/* The offset of this registers attentions within the blocks attentions list
 * (a value in the range 0..number of block attentions-1)
 */
	u16 block_attn_offset;
	u32 data;
/* STS attention register GRC address (in dwords) */
#define DBG_ATTN_REG_STS_ADDRESS_MASK   0xFFFFFF
#define DBG_ATTN_REG_STS_ADDRESS_SHIFT  0
/* Number of attention in this register */
#define DBG_ATTN_REG_NUM_REG_ATTN_MASK  0xFF
#define DBG_ATTN_REG_NUM_REG_ATTN_SHIFT 24
/* STS_CLR attention register GRC address (in dwords) */
	u32 sts_clr_address;
	u32 mask_address /* MASK attention register GRC address (in dwords) */;
};



/*
 * attention types
 */
enum dbg_attn_type {
	ATTN_TYPE_INTERRUPT,
	ATTN_TYPE_PARITY,
	MAX_DBG_ATTN_TYPE
};


/*
 * Block debug data
 */
struct dbg_block {
	u8 name[15] /* Block name */;
/* The letter (char) of the associated Storm, or 0 if no associated Storm. */
	u8 associated_storm_letter;
};


/*
 * Chip-specific block debug data
 */
struct dbg_block_chip {
	u8 flags;
/* Indicates if the block is removed in this chip (0/1). */
#define DBG_BLOCK_CHIP_IS_REMOVED_MASK           0x1
#define DBG_BLOCK_CHIP_IS_REMOVED_SHIFT          0
/* Indicates if this block has a reset register (0/1). */
#define DBG_BLOCK_CHIP_HAS_RESET_REG_MASK        0x1
#define DBG_BLOCK_CHIP_HAS_RESET_REG_SHIFT       1
/* Indicates if this block should be taken out of reset before GRC Dump (0/1).
 * Valid only if has_reset_reg is set.
 */
#define DBG_BLOCK_CHIP_UNRESET_BEFORE_DUMP_MASK  0x1
#define DBG_BLOCK_CHIP_UNRESET_BEFORE_DUMP_SHIFT 2
/* Indicates if this block has a debug bus (0/1). */
#define DBG_BLOCK_CHIP_HAS_DBG_BUS_MASK          0x1
#define DBG_BLOCK_CHIP_HAS_DBG_BUS_SHIFT         3
/* Indicates if this block has a latency events debug line (0/1). Valid only
 * if has_dbg_bus is set.
 */
#define DBG_BLOCK_CHIP_HAS_LATENCY_EVENTS_MASK   0x1
#define DBG_BLOCK_CHIP_HAS_LATENCY_EVENTS_SHIFT  4
#define DBG_BLOCK_CHIP_RESERVED0_MASK            0x7
#define DBG_BLOCK_CHIP_RESERVED0_SHIFT           5
/* The DBG block client ID of this block/chip. Valid only if has_dbg_bus is
 * set.
 */
	u8 dbg_client_id;
/* The ID of the reset register of this block/chip in the dbg_reset_reg
 * array.
 */
	u8 reset_reg_id;
/* The bit offset of this block/chip in the reset register. Valid only if
 * has_reset_reg is set.
 */
	u8 reset_reg_bit_offset;
	struct dbg_mode_hdr dbg_bus_mode /* Mode header */;
	u16 reserved1;
	u8 reserved2;
/* Number of Debug Bus lines in this block/chip (excluding signature and latency
 * events). Valid only if has_dbg_bus is set.
 */
	u8 num_of_dbg_bus_lines;
/* Offset of this block/chip Debug Bus lines in the Debug Bus lines array. Valid
 * only if has_dbg_bus is set.
 */
	u16 dbg_bus_lines_offset;
/* GRC address of the Debug Bus dbg_select register (in dwords). Valid only if
 * has_dbg_bus is set.
 */
	u32 dbg_select_reg_addr;
/* GRC address of the Debug Bus dbg_dword_enable register (in dwords). Valid
 * only if has_dbg_bus is set.
 */
	u32 dbg_dword_enable_reg_addr;
/* GRC address of the Debug Bus dbg_shift register (in dwords). Valid only if
 * has_dbg_bus is set.
 */
	u32 dbg_shift_reg_addr;
/* GRC address of the Debug Bus dbg_force_valid register (in dwords). Valid only
 * if has_dbg_bus is set.
 */
	u32 dbg_force_valid_reg_addr;
/* GRC address of the Debug Bus dbg_force_frame register (in dwords). Valid only
 * if has_dbg_bus is set.
 */
	u32 dbg_force_frame_reg_addr;
};


/*
 * Chip-specific block user debug data
 */
struct dbg_block_chip_user {
/* Number of debug bus lines in this block (excluding signature and latency
 * events).
 */
	u8 num_of_dbg_bus_lines;
/* Indicates if this block has a latency events debug line (0/1). */
	u8 has_latency_events;
/* Offset of this blocks lines in the debug bus line name offsets array. */
	u16 names_offset;
};


/*
 * Block user debug data
 */
struct dbg_block_user {
	u8 name[16] /* Block name */;
};


/*
 * Block Debug line data
 */
struct dbg_bus_line {
	u8 data;
/* Number of groups in the line (0-3) */
#define DBG_BUS_LINE_NUM_OF_GROUPS_MASK  0xF
#define DBG_BUS_LINE_NUM_OF_GROUPS_SHIFT 0
/* Indicates if this is a 128b line (0) or a 256b line (1). */
#define DBG_BUS_LINE_IS_256B_MASK        0x1
#define DBG_BUS_LINE_IS_256B_SHIFT       4
#define DBG_BUS_LINE_RESERVED_MASK       0x7
#define DBG_BUS_LINE_RESERVED_SHIFT      5
/* Four 2-bit values, indicating the size of each group minus 1 (i.e.
 * value=0 means size=1, value=1 means size=2, etc), starting from lsb.
 * The sizes are in dwords (if is_256b=0) or in qwords (if is_256b=1).
 */
	u8 group_sizes;
};


/*
 * condition header for registers dump
 */
struct dbg_dump_cond_hdr {
	struct dbg_mode_hdr mode /* Mode header */;
	u8 block_id /* block ID */;
	u8 data_size /* size in dwords of the data following this header */;
};


/*
 * memory data for registers dump
 */
struct dbg_dump_mem {
	u32 dword0;
/* register address (in dwords) */
#define DBG_DUMP_MEM_ADDRESS_MASK       0xFFFFFF
#define DBG_DUMP_MEM_ADDRESS_SHIFT      0
#define DBG_DUMP_MEM_MEM_GROUP_ID_MASK  0xFF /* memory group ID */
#define DBG_DUMP_MEM_MEM_GROUP_ID_SHIFT 24
	u32 dword1;
/* register size (in dwords) */
#define DBG_DUMP_MEM_LENGTH_MASK        0xFFFFFF
#define DBG_DUMP_MEM_LENGTH_SHIFT       0
/* indicates if the register is wide-bus */
#define DBG_DUMP_MEM_WIDE_BUS_MASK      0x1
#define DBG_DUMP_MEM_WIDE_BUS_SHIFT     24
#define DBG_DUMP_MEM_RESERVED_MASK      0x7F
#define DBG_DUMP_MEM_RESERVED_SHIFT     25
};


/*
 * register data for registers dump
 */
struct dbg_dump_reg {
	u32 data;
/* register address (in dwords) */
#define DBG_DUMP_REG_ADDRESS_MASK   0x7FFFFF /* register address (in dwords) */
#define DBG_DUMP_REG_ADDRESS_SHIFT  0
/* indicates if the register is wide-bus */
#define DBG_DUMP_REG_WIDE_BUS_MASK  0x1
#define DBG_DUMP_REG_WIDE_BUS_SHIFT 23
#define DBG_DUMP_REG_LENGTH_MASK    0xFF /* register size (in dwords) */
#define DBG_DUMP_REG_LENGTH_SHIFT   24
};


/*
 * split header for registers dump
 */
struct dbg_dump_split_hdr {
	u32 hdr;
/* size in dwords of the data following this header */
#define DBG_DUMP_SPLIT_HDR_DATA_SIZE_MASK      0xFFFFFF
#define DBG_DUMP_SPLIT_HDR_DATA_SIZE_SHIFT     0
#define DBG_DUMP_SPLIT_HDR_SPLIT_TYPE_ID_MASK  0xFF /* split type ID */
#define DBG_DUMP_SPLIT_HDR_SPLIT_TYPE_ID_SHIFT 24
};


/*
 * condition header for idle check
 */
struct dbg_idle_chk_cond_hdr {
	struct dbg_mode_hdr mode /* Mode header */;
	u16 data_size /* size in dwords of the data following this header */;
};


/*
 * Idle Check condition register
 */
struct dbg_idle_chk_cond_reg {
	u32 data;
/* Register GRC address (in dwords) */
#define DBG_IDLE_CHK_COND_REG_ADDRESS_MASK   0x7FFFFF
#define DBG_IDLE_CHK_COND_REG_ADDRESS_SHIFT  0
/* indicates if the register is wide-bus */
#define DBG_IDLE_CHK_COND_REG_WIDE_BUS_MASK  0x1
#define DBG_IDLE_CHK_COND_REG_WIDE_BUS_SHIFT 23
/* value from block_id enum */
#define DBG_IDLE_CHK_COND_REG_BLOCK_ID_MASK  0xFF
#define DBG_IDLE_CHK_COND_REG_BLOCK_ID_SHIFT 24
	u16 num_entries /* number of registers entries to check */;
	u8 entry_size /* size of registers entry (in dwords) */;
	u8 start_entry /* index of the first entry to check */;
};


/*
 * Idle Check info register
 */
struct dbg_idle_chk_info_reg {
	u32 data;
/* Register GRC address (in dwords) */
#define DBG_IDLE_CHK_INFO_REG_ADDRESS_MASK   0x7FFFFF
#define DBG_IDLE_CHK_INFO_REG_ADDRESS_SHIFT  0
/* indicates if the register is wide-bus */
#define DBG_IDLE_CHK_INFO_REG_WIDE_BUS_MASK  0x1
#define DBG_IDLE_CHK_INFO_REG_WIDE_BUS_SHIFT 23
/* value from block_id enum */
#define DBG_IDLE_CHK_INFO_REG_BLOCK_ID_MASK  0xFF
#define DBG_IDLE_CHK_INFO_REG_BLOCK_ID_SHIFT 24
	u16 size /* register size in dwords */;
	struct dbg_mode_hdr mode /* Mode header */;
};


/*
 * Idle Check register
 */
union dbg_idle_chk_reg {
	struct dbg_idle_chk_cond_reg cond_reg /* condition register */;
	struct dbg_idle_chk_info_reg info_reg /* info register */;
};


/*
 * Idle Check result header
 */
struct dbg_idle_chk_result_hdr {
	u16 rule_id /* Failing rule index */;
	u16 mem_entry_id /* Failing memory entry index */;
	u8 num_dumped_cond_regs /* number of dumped condition registers */;
	u8 num_dumped_info_regs /* number of dumped condition registers */;
	u8 severity /* from dbg_idle_chk_severity_types enum */;
	u8 reserved;
};


/*
 * Idle Check result register header
 */
struct dbg_idle_chk_result_reg_hdr {
	u8 data;
/* indicates if this register is a memory */
#define DBG_IDLE_CHK_RESULT_REG_HDR_IS_MEM_MASK  0x1
#define DBG_IDLE_CHK_RESULT_REG_HDR_IS_MEM_SHIFT 0
/* register index within the failing rule */
#define DBG_IDLE_CHK_RESULT_REG_HDR_REG_ID_MASK  0x7F
#define DBG_IDLE_CHK_RESULT_REG_HDR_REG_ID_SHIFT 1
	u8 start_entry /* index of the first checked entry */;
	u16 size /* register size in dwords */;
};


/*
 * Idle Check rule
 */
struct dbg_idle_chk_rule {
	u16 rule_id /* Idle Check rule ID */;
	u8 severity /* value from dbg_idle_chk_severity_types enum */;
	u8 cond_id /* Condition ID */;
	u8 num_cond_regs /* number of condition registers */;
	u8 num_info_regs /* number of info registers */;
	u8 num_imms /* number of immediates in the condition */;
	u8 reserved1;
/* offset of this rules registers in the idle check register array
 * (in dbg_idle_chk_reg units)
 */
	u16 reg_offset;
/* offset of this rules immediate values in the immediate values array
 * (in dwords)
 */
	u16 imm_offset;
};


/*
 * Idle Check rule parsing data
 */
struct dbg_idle_chk_rule_parsing_data {
	u32 data;
/* indicates if this register has a FW message */
#define DBG_IDLE_CHK_RULE_PARSING_DATA_HAS_FW_MSG_MASK  0x1
#define DBG_IDLE_CHK_RULE_PARSING_DATA_HAS_FW_MSG_SHIFT 0
/* Offset of this rules strings in the debug strings array (in bytes) */
#define DBG_IDLE_CHK_RULE_PARSING_DATA_STR_OFFSET_MASK  0x7FFFFFFF
#define DBG_IDLE_CHK_RULE_PARSING_DATA_STR_OFFSET_SHIFT 1
};


/*
 * idle check severity types
 */
enum dbg_idle_chk_severity_types {
/* idle check failure should cause an error */
	IDLE_CHK_SEVERITY_ERROR,
/* idle check failure should cause an error only if theres no traffic */
	IDLE_CHK_SEVERITY_ERROR_NO_TRAFFIC,
/* idle check failure should cause a warning */
	IDLE_CHK_SEVERITY_WARNING,
	MAX_DBG_IDLE_CHK_SEVERITY_TYPES
};



/*
 * Reset register
 */
struct dbg_reset_reg {
	u32 data;
#define DBG_RESET_REG_ADDR_MASK        0xFFFFFF /* GRC address (in dwords) */
#define DBG_RESET_REG_ADDR_SHIFT       0
/* indicates if this register is removed (0/1). */
#define DBG_RESET_REG_IS_REMOVED_MASK  0x1
#define DBG_RESET_REG_IS_REMOVED_SHIFT 24
#define DBG_RESET_REG_RESERVED_MASK    0x7F
#define DBG_RESET_REG_RESERVED_SHIFT   25
};


/*
 * Debug Bus block data
 */
struct dbg_bus_block_data {
/* 4 bit value, bit i set -> dword/qword i is enabled in block. */
	u8 enable_mask;
/* Number of dwords/qwords to cyclically  right the blocks output (0-3). */
	u8 right_shift;
/* 4 bit value, bit i set -> dword/qword i is forced valid in block. */
	u8 force_valid_mask;
/* 4 bit value, bit i set -> dword/qword i frame bit is forced in block. */
	u8 force_frame_mask;
/* bit i set -> dword i contains this blocks data (after shifting). */
	u8 dword_mask;
	u8 line_num /* Debug line number to select */;
	u8 hw_id /* HW ID associated with the block */;
	u8 flags;
/* 0/1. If 1, the debug line is 256b, otherwise its 128b. */
#define DBG_BUS_BLOCK_DATA_IS_256B_LINE_MASK  0x1
#define DBG_BUS_BLOCK_DATA_IS_256B_LINE_SHIFT 0
#define DBG_BUS_BLOCK_DATA_RESERVED_MASK      0x7F
#define DBG_BUS_BLOCK_DATA_RESERVED_SHIFT     1
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
 * Debug Bus trigger state data
 */
struct dbg_bus_trigger_state_data {
/* Message length (in cycles) to be used for message-based trigger constraints.
 * If set to 0, message length is based only on frame bit received from HW.
 */
	u8 msg_len;
/* A bit for each dword in the debug bus cycle, indicating if this dword appears
 * in a trigger constraint (1) or not (0)
 */
	u8 constraint_dword_mask;
/* Storm ID to trigger on. Valid only when triggering on Storm data.
 * (use enum dbg_storms)
 */
	u8 storm_id;
	u8 reserved;
};

/*
 * Debug Bus memory address
 */
struct dbg_bus_mem_addr {
	u32 lo;
	u32 hi;
};

/*
 * Debug Bus PCI buffer data
 */
struct dbg_bus_pci_buf_data {
	struct dbg_bus_mem_addr phys_addr /* PCI buffer physical address */;
	struct dbg_bus_mem_addr virt_addr /* PCI buffer virtual address */;
	u32 size /* PCI buffer size in bytes */;
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
/* EID range filter params */
	struct dbg_bus_storm_eid_range_params range;
/* EID mask filter params */
	struct dbg_bus_storm_eid_mask_params mask;
};

/*
 * Debug Bus Storm data
 */
struct dbg_bus_storm_data {
	u8 enabled /* indicates if the Storm is enabled for recording */;
	u8 mode /* Storm debug mode, valid only if the Storm is enabled */;
	u8 hw_id /* HW ID associated with the Storm */;
	u8 eid_filter_en /* Indicates if EID filtering is performed (0/1) */;
/* 1 = EID range filter, 0 = EID mask filter. Valid only if eid_filter_en is
 * set,
 */
	u8 eid_range_not_mask;
	u8 cid_filter_en /* Indicates if CID filtering is performed (0/1) */;
/* EID filter params to filter on. Valid only if eid_filter_en is set. */
	union dbg_bus_storm_eid_params eid_filter_params;
	u32 cid /* CID to filter on. Valid only if cid_filter_en is set. */;
};

/*
 * Debug Bus data
 */
struct dbg_bus_data {
	u32 app_version /* The tools version number of the application */;
	u8 state /* The current debug bus state */;
	u8 mode_256b_en /* Indicates if the 256 bit mode is enabled */;
	u8 num_enabled_blocks /* Number of blocks enabled for recording */;
	u8 num_enabled_storms /* Number of Storms enabled for recording */;
	u8 target /* Output target */;
	u8 one_shot_en /* Indicates if one-shot mode is enabled (0/1) */;
	u8 grc_input_en /* Indicates if GRC recording is enabled (0/1) */;
/* Indicates if timestamp recording is enabled (0/1) */
	u8 timestamp_input_en;
	u8 filter_en /* Indicates if the recording filter is enabled (0/1) */;
/* If true, the next added constraint belong to the filter. Otherwise,
 * it belongs to the last added trigger state. Valid only if either filter or
 * triggers are enabled.
 */
	u8 adding_filter;
/* Indicates if the recording filter should be applied before the trigger.
 * Valid only if both filter and trigger are enabled (0/1)
 */
	u8 filter_pre_trigger;
/* Indicates if the recording filter should be applied after the trigger.
 * Valid only if both filter and trigger are enabled (0/1)
 */
	u8 filter_post_trigger;
/* Indicates if the recording trigger is enabled (0/1) */
	u8 trigger_en;
/* A bit for each dword in the debug bus cycle, indicating if this dword
 * appears in a filter constraint (1) or not (0)
 */
	u8 filter_constraint_dword_mask;
	u8 next_trigger_state /* ID of next trigger state to be added */;
/* ID of next filter/trigger constraint to be added */
	u8 next_constraint_id;
/* trigger states data */
	struct dbg_bus_trigger_state_data trigger_states[3];
/* Message length (in cycles) to be used for message-based filter constraints.
 * If set to 0 message length is based only on frame bit received from HW.
 */
	u8 filter_msg_len;
/* Indicates if the other engine sends it NW recording to this engine (0/1) */
	u8 rcv_from_other_engine;
/* A bit for each dword in the debug bus cycle, indicating if this dword is
 * recorded (1) or not (0)
 */
	u8 blocks_dword_mask;
/* Indicates if there are dwords in the debug bus cycle which are recorded
 * by more tan one block (0/1)
 */
	u8 blocks_dword_overlap;
/* The HW IDs of the recorded HW blocks, where bits i*3..i*3+2 contain the
 * HW ID of dword/qword i
 */
	u32 hw_id_mask;
/* Debug Bus PCI buffer data. Valid only when the target is
 * DBG_BUS_TARGET_ID_PCI.
 */
	struct dbg_bus_pci_buf_data pci_buf;
/* Debug Bus data for each block */
	struct dbg_bus_block_data blocks[132];
/* Debug Bus data for each block */
	struct dbg_bus_storm_data storms[6];
};


/*
 * Debug bus states
 */
enum dbg_bus_states {
	DBG_BUS_STATE_IDLE /* debug bus idle state (not recording) */,
/* debug bus is ready for configuration and recording */
	DBG_BUS_STATE_READY,
	DBG_BUS_STATE_RECORDING /* debug bus is currently recording */,
	DBG_BUS_STATE_STOPPED /* debug bus recording has stopped */,
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
/* recording handlers with store messages (fast debug) */
	DBG_BUS_STORM_MODE_RH_WITH_STORE,
	DBG_BUS_STORM_MODE_FOC /* FOC: FIN + DRA Rd (slow debug) */,
	DBG_BUS_STORM_MODE_EXT_STORE /* FOC: External Store (slow) */,
	MAX_DBG_BUS_STORM_MODES
};


/*
 * Debug bus target IDs
 */
enum dbg_bus_targets {
/* records debug bus to DBG block internal buffer */
	DBG_BUS_TARGET_ID_INT_BUF,
	DBG_BUS_TARGET_ID_NIG /* records debug bus to the NW */,
	DBG_BUS_TARGET_ID_PCI /* records debug bus to a PCI buffer */,
	MAX_DBG_BUS_TARGETS
};



/*
 * GRC Dump data
 */
struct dbg_grc_data {
/* Indicates if the GRC parameters were initialized */
	u8 params_initialized;
	u8 reserved1;
	u16 reserved2;
/* Value of each GRC parameter. Array size must match the enum dbg_grc_params.
 */
	u32 param_val[48];
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
	DBG_GRC_PARAM_DUMP_DORQ /* dump DORQ memories (0/1) */,
	DBG_GRC_PARAM_DUMP_CFC /* dump CFC memories (0/1) */,
	DBG_GRC_PARAM_DUMP_IGU /* dump IGU memories (0/1) */,
	DBG_GRC_PARAM_DUMP_BRB /* dump BRB memories (0/1) */,
	DBG_GRC_PARAM_DUMP_BTB /* dump BTB memories (0/1) */,
	DBG_GRC_PARAM_DUMP_BMB /* dump BMB memories (0/1) */,
	DBG_GRC_PARAM_RESERVD1 /* reserved */,
	DBG_GRC_PARAM_DUMP_MULD /* dump MULD memories (0/1) */,
	DBG_GRC_PARAM_DUMP_PRS /* dump PRS memories (0/1) */,
	DBG_GRC_PARAM_DUMP_DMAE /* dump PRS memories (0/1) */,
	DBG_GRC_PARAM_DUMP_TM /* dump TM (timers) memories (0/1) */,
	DBG_GRC_PARAM_DUMP_SDM /* dump SDM memories (0/1) */,
	DBG_GRC_PARAM_DUMP_DIF /* dump DIF memories (0/1) */,
	DBG_GRC_PARAM_DUMP_STATIC /* dump static debug data (0/1) */,
	DBG_GRC_PARAM_UNSTALL /* un-stall Storms after dump (0/1) */,
	DBG_GRC_PARAM_RESERVED2 /* reserved */,
/* MCP Trace meta data size in bytes */
	DBG_GRC_PARAM_MCP_TRACE_META_SIZE,
/* preset: exclude all memories from dump (1 only) */
	DBG_GRC_PARAM_EXCLUDE_ALL,
/* preset: include memories for crash dump (1 only) */
	DBG_GRC_PARAM_CRASH,
/* perform dump only if MFW is responding (0/1) */
	DBG_GRC_PARAM_PARITY_SAFE,
	DBG_GRC_PARAM_DUMP_CM /* dump CM memories (0/1) */,
	DBG_GRC_PARAM_DUMP_PHY /* dump PHY memories (0/1) */,
	DBG_GRC_PARAM_NO_MCP /* dont perform MCP commands (0/1) */,
	DBG_GRC_PARAM_NO_FW_VER /* dont read FW/MFW version (0/1) */,
	DBG_GRC_PARAM_RESERVED3 /* reserved */,
	DBG_GRC_PARAM_DUMP_MCP_HW_DUMP /* dump MCP HW Dump (0/1) */,
	DBG_GRC_PARAM_DUMP_ILT_CDUC /* dump ILT CDUC client (0/1) */,
	DBG_GRC_PARAM_DUMP_ILT_CDUT /* dump ILT CDUT client (0/1) */,
	DBG_GRC_PARAM_DUMP_CAU_EXT /* dump CAU extended memories (0/1) */,
	MAX_DBG_GRC_PARAMS
};


/*
 * Debug status codes
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
	DBG_STATUS_INVALID_FILTER_TRIGGER_DWORDS,
	DBG_STATUS_NO_MATCHING_FRAMING_MODE,
	DBG_STATUS_VFC_READ_ERROR,
	DBG_STATUS_STORM_ALREADY_ENABLED,
	DBG_STATUS_STORM_NOT_ENABLED,
	DBG_STATUS_BLOCK_ALREADY_ENABLED,
	DBG_STATUS_BLOCK_NOT_ENABLED,
	DBG_STATUS_NO_INPUT_ENABLED,
	DBG_STATUS_NO_FILTER_TRIGGER_256B,
	DBG_STATUS_FILTER_ALREADY_ENABLED,
	DBG_STATUS_TRIGGER_ALREADY_ENABLED,
	DBG_STATUS_TRIGGER_NOT_ENABLED,
	DBG_STATUS_CANT_ADD_CONSTRAINT,
	DBG_STATUS_TOO_MANY_TRIGGER_STATES,
	DBG_STATUS_TOO_MANY_CONSTRAINTS,
	DBG_STATUS_RECORDING_NOT_STARTED,
	DBG_STATUS_DATA_DIDNT_TRIGGER,
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
	DBG_STATUS_RESERVED0,
	DBG_STATUS_SEMI_FIFO_NOT_EMPTY,
	DBG_STATUS_IGU_FIFO_BAD_DATA,
	DBG_STATUS_MCP_COULD_NOT_MASK_PRTY,
	DBG_STATUS_FW_ASSERTS_PARSE_FAILED,
	DBG_STATUS_REG_FIFO_BAD_DATA,
	DBG_STATUS_PROTECTION_OVERRIDE_BAD_DATA,
	DBG_STATUS_DBG_ARRAY_NOT_SET,
	DBG_STATUS_RESERVED1,
	DBG_STATUS_NON_MATCHING_LINES,
	DBG_STATUS_INSUFFICIENT_HW_IDS,
	DBG_STATUS_DBG_BUS_IN_USE,
	DBG_STATUS_INVALID_STORM_DBG_MODE,
	DBG_STATUS_OTHER_ENGINE_BB_ONLY,
	DBG_STATUS_FILTER_SINGLE_HW_ID,
	DBG_STATUS_TRIGGER_SINGLE_HW_ID,
	DBG_STATUS_MISSING_TRIGGER_STATE_STORM,
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
	u32 buf_size /* Idle check buffer size in dwords */;
/* Indicates if the idle check buffer size was set (0/1) */
	u8 buf_size_set;
	u8 reserved1;
	u16 reserved2;
};

/*
 * Pretend parameters
 */
struct pretend_params {
	u8 split_type /* Pretend split type (from enum init_split_types) */;
	u8 reserved;
	u16 split_id /* Preted split ID (within the pretend split type) */;
};

/*
 * Debug Tools data (per HW function)
 */
struct dbg_tools_data {
	struct dbg_grc_data grc /* GRC Dump data */;
	struct dbg_bus_data bus /* Debug Bus data */;
	struct idle_chk_data idle_chk /* Idle Check data */;
	u8 mode_enable[40] /* Indicates if a mode is enabled (0/1) */;
/* Indicates if a block is in reset state (0/1) */
	u8 block_in_reset[132];
	u8 chip_id /* Chip ID (from enum chip_ids) */;
	u8 hw_type /* HW Type */;
	u8 num_ports /* Number of ports in the chip */;
	u8 num_pfs_per_port /* Number of PFs in each port */;
	u8 num_vfs /* Number of VFs in the chip */;
	u8 initialized /* Indicates if the data was initialized */;
	u8 use_dmae /* Indicates if DMAE should be used */;
	u8 reserved;
	struct pretend_params pretend /* Current pretend parameters */;
/* Numbers of registers that were read since last log */
	u32 num_regs_read;
};


#endif /* __ECORE_HSI_DEBUG_TOOLS__ */
