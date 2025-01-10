/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021-2024 Advanced Micro Devices, Inc.
 */

#ifndef _IONIC_CRYPTO_IF_H_
#define _IONIC_CRYPTO_IF_H_

#define IOCPT_DEV_INFO_SIGNATURE		0x43585660      /* 'CRPT' */
#define IOCPT_DEV_INFO_VERSION			1
#define IOCPT_IFNAMSIZ				16

/**
 * enum iocpt_cmd_opcode - Device commands
 */
enum iocpt_cmd_opcode {
	IOCPT_CMD_NOP				= 0,	/* D, A */

	/* Device commands */
	IOCPT_CMD_IDENTIFY			= 1,	/* D */
	IOCPT_CMD_RESET				= 3,	/* D */

	/* LIF commands */
	IOCPT_CMD_LIF_IDENTIFY			= 20,	/* D */
	IOCPT_CMD_LIF_INIT			= 21,	/* D */
	IOCPT_CMD_LIF_RESET			= 22,	/* D */
	IOCPT_CMD_LIF_GETATTR			= 23,	/* D, A */
	IOCPT_CMD_LIF_SETATTR			= 24,	/* D, A */

	/* Queue commands */
	IOCPT_CMD_Q_IDENTIFY			= 39,	/* D, A */
	IOCPT_CMD_Q_INIT			= 40,	/* D, A */
	IOCPT_CMD_Q_CONTROL			= 41,	/* D, A */

	/* Session commands */
	IOCPT_CMD_SESS_CONTROL			= 45,	/* D, A */
};

/**
 * enum iocpt_status_code - Device command return codes
 */
enum iocpt_status_code {
	IOCPT_RC_SUCCESS	= 0,	/* Success */
	IOCPT_RC_EVERSION	= 1,	/* Incorrect version for request */
	IOCPT_RC_EOPCODE	= 2,	/* Invalid cmd opcode */
	IOCPT_RC_EIO		= 3,	/* I/O error */
	IOCPT_RC_EPERM		= 4,	/* Permission denied */
	IOCPT_RC_EQID		= 5,	/* Bad qid */
	IOCPT_RC_EQTYPE		= 6,	/* Bad qtype */
	IOCPT_RC_ENOENT		= 7,	/* No such element */
	IOCPT_RC_EINTR		= 8,	/* Operation interrupted */
	IOCPT_RC_EAGAIN		= 9,	/* Try again */
	IOCPT_RC_ENOMEM		= 10,	/* Out of memory */
	IOCPT_RC_EFAULT		= 11,	/* Bad address */
	IOCPT_RC_EBUSY		= 12,	/* Device or resource busy */
	IOCPT_RC_EEXIST		= 13,	/* Object already exists */
	IOCPT_RC_EINVAL		= 14,	/* Invalid argument */
	IOCPT_RC_ENOSPC		= 15,	/* No space left or alloc failure */
	IOCPT_RC_ERANGE		= 16,	/* Parameter out of range */
	IOCPT_RC_BAD_ADDR	= 17,	/* Descriptor contains a bad ptr */
	IOCPT_RC_DEV_CMD	= 18,	/* Device cmd attempted on AdminQ */
	IOCPT_RC_ENOSUPP	= 19,	/* Operation not supported */
	IOCPT_RC_ERROR		= 29,	/* Generic error */
};

enum iocpt_notifyq_opcode {
	IOCPT_EVENT_RESET		= 1,
	IOCPT_EVENT_HEARTBEAT		= 2,
	IOCPT_EVENT_LOG			= 3,
};

enum iocpt_lif_type {
	IOCPT_LIF_TYPE_DEFAULT = 0,
};

/**
 * struct iocpt_admin_cmd - General admin command format
 * @opcode:	Opcode for the command
 * @lif_index:	LIF index
 * @cmd_data:	Opcode-specific command bytes
 */
struct iocpt_admin_cmd {
	u8     opcode;
	u8     rsvd;
	__le16 lif_index;
	u8     cmd_data[60];
};

/**
 * struct iocpt_admin_comp - General admin command completion format
 * @status:     Status of the command (enum iocpt_status_code)
 * @comp_index: Index in the descriptor ring for which this is the completion
 * @cmd_data:   Command-specific bytes
 * @color:      Color bit (Always 0 for commands issued to the
 *              Device Cmd Registers)
 */
struct iocpt_admin_comp {
	u8     status;
	u8     rsvd;
	__le16 comp_index;
	u8     cmd_data[11];
	u8     color;
#define IOCPT_COMP_COLOR_MASK  0x80
};

static inline u8 iocpt_color_match(u8 color, u8 done_color)
{
	return (!!(color & IOCPT_COMP_COLOR_MASK)) == done_color;
}

/**
 * struct iocpt_nop_cmd - NOP command
 * @opcode:	Opcode
 */
struct iocpt_nop_cmd {
	u8     opcode;
	u8     rsvd[63];
};

/**
 * struct iocpt_nop_comp - NOP command completion
 * @status:	Status of the command (enum iocpt_status_code)
 */
struct iocpt_nop_comp {
	u8     status;
	u8     rsvd[15];
};

#define IOCPT_IDENTITY_VERSION_1	1

/**
 * struct iocpt_dev_identify_cmd - Driver/device identify command
 * @opcode:	Opcode
 * @ver:	Highest version of identify supported by driver
 */
struct iocpt_dev_identify_cmd {
	u8     opcode;
	u8     ver;
	u8     rsvd[62];
};

/**
 * struct iocpt_dev_identify_comp - Device identify command completion
 * @status:	Status of the command (enum iocpt_status_code)
 * @ver:	Version of identify returned by device
 */
struct iocpt_dev_identify_comp {
	u8     status;
	u8     ver;
	u8     rsvd[14];
};

/**
 * struct iocpt_dev_reset_cmd - Device reset command
 * Will reset all LIFs on the device.
 * @opcode:	Opcode
 */
struct iocpt_dev_reset_cmd {
	u8     opcode;
	u8     rsvd[63];
};

/**
 * struct iocpt_dev_reset_comp - Reset command completion
 * @status:	Status of the command (enum iocpt_status_code)
 */
struct iocpt_dev_reset_comp {
	u8     status;
	u8     rsvd[15];
};

/**
 * struct iocpt_lif_identify_cmd - LIF identify command
 * @opcode:	Opcode
 * @type:	LIF type (enum iocpt_lif_type)
 * @lif_index:	LIF index
 * @ver:	Version of identify returned by device
 */
struct iocpt_lif_identify_cmd {
	u8     opcode;
	u8     type;
	__le16 lif_index;
	u8     ver;
	u8     rsvd[59];
};

/**
 * struct iocpt_lif_identify_comp - LIF identify command completion
 * @status:  Status of the command (enum iocpt_status_code)
 * @ver:     Version of identify returned by device
 */
struct iocpt_lif_identify_comp {
	u8     status;
	u8     ver;
	u8     rsvd2[14];
};

/**
 * struct iocpt_lif_init_cmd - LIF init command
 * @opcode:	Opcode
 * @type:	LIF type (enum iocpt_lif_type)
 * @lif_index:	LIF index
 * @info_pa:	Destination address for LIF info (struct iocpt_lif_info)
 */
struct iocpt_lif_init_cmd {
	u8     opcode;
	u8     type;
	__le16 lif_index;
	__le32 rsvd;
	__le64 info_pa;
	u8     rsvd2[48];
};

/**
 * struct iocpt_lif_init_comp - LIF init command completion
 * @status:	Status of the command (enum iocpt_status_code)
 * @hw_index:	Hardware index of the initialized LIF
 */
struct iocpt_lif_init_comp {
	u8     status;
	u8     rsvd;
	__le16 hw_index;
	u8     rsvd2[12];
};

/**
 * struct iocpt_lif_reset_cmd - LIF reset command
 * Will reset only the specified LIF.
 * @opcode:	Opcode
 * @lif_index:	LIF index
 */
struct iocpt_lif_reset_cmd {
	u8     opcode;
	u8     rsvd;
	__le16 lif_index;
	__le32 rsvd2[15];
};

/**
 * enum iocpt_lif_attr - List of LIF attributes
 * @IOCPT_LIF_ATTR_STATE:	LIF state attribute
 * @IOCPT_LIF_ATTR_NAME:	LIF name attribute
 * @IOCPT_LIF_ATTR_FEATURES:	LIF features attribute
 * @IOCPT_LIF_ATTR_STATS_CTRL:	LIF statistics control attribute
 */
enum iocpt_lif_attr {
	IOCPT_LIF_ATTR_STATE	    = 0,
	IOCPT_LIF_ATTR_NAME	    = 1,
	IOCPT_LIF_ATTR_FEATURES	    = 4,
	IOCPT_LIF_ATTR_STATS_CTRL   = 6,
};

/**
 * struct iocpt_lif_setattr_cmd - Set LIF attributes on the NIC
 * @opcode:	Opcode
 * @attr:	Attribute type (enum iocpt_lif_attr)
 * @lif_index:	LIF index
 * @state:	LIF state (enum iocpt_lif_state)
 * @name:	The name string, 0 terminated
 * @features:	Features (enum iocpt_hw_features)
 * @stats_ctl:	Stats control commands (enum iocpt_stats_ctl_cmd)
 */
struct iocpt_lif_setattr_cmd {
	u8     opcode;
	u8     attr;
	__le16 lif_index;
	union __rte_packed_begin {
		u8	state;
		char	name[IOCPT_IFNAMSIZ];
		__le64	features;
		u8	stats_ctl;
		u8	rsvd[60];
	} __rte_packed_end;
};

/**
 * struct iocpt_lif_setattr_comp - LIF set attr command completion
 * @status:	Status of the command (enum iocpt_status_code)
 * @comp_index: Index in the descriptor ring for which this is the completion
 * @features:	Features (enum iocpt_hw_features)
 * @color:	Color bit
 */
struct iocpt_lif_setattr_comp {
	u8     status;
	u8     rsvd;
	__le16 comp_index;
	union __rte_packed_begin {
		__le64	features;
		u8	rsvd2[11];
	} __rte_packed_end;
	u8     color;
};

/**
 * struct iocpt_lif_getattr_cmd - Get LIF attributes from the NIC
 * @opcode:	Opcode
 * @attr:	Attribute type (enum iocpt_lif_attr)
 * @lif_index:	LIF index
 */
struct iocpt_lif_getattr_cmd {
	u8     opcode;
	u8     attr;
	__le16 lif_index;
	u8     rsvd[60];
};

/**
 * struct iocpt_lif_getattr_comp - LIF get attr command completion
 * @status:	Status of the command (enum iocpt_status_code)
 * @comp_index: Index in the descriptor ring for which this is the completion
 * @state:	LIF state (enum iocpt_lif_state)
 * @name:	LIF name string, 0 terminated
 * @features:	Features (enum iocpt_hw_features)
 * @color:	Color bit
 */
struct iocpt_lif_getattr_comp {
	u8     status;
	u8     rsvd;
	__le16 comp_index;
	union __rte_packed_begin {
		u8	state;
		__le64	features;
		u8	rsvd2[11];
	} __rte_packed_end;
	u8     color;
};

/**
 * enum iocpt_logical_qtype - Logical Queue Types
 * @IOCPT_QTYPE_ADMINQ:    Administrative Queue
 * @IOCPT_QTYPE_NOTIFYQ:   Notify Queue
 * @IOCPT_QTYPE_CRYPTOQ:   Cryptographic Queue
 * @IOCPT_QTYPE_MAX:       Max queue type supported
 */
enum iocpt_logical_qtype {
	IOCPT_QTYPE_ADMINQ  = 0,
	IOCPT_QTYPE_NOTIFYQ = 1,
	IOCPT_QTYPE_CRYPTOQ = 2,
	IOCPT_QTYPE_MAX     = 8,
};

/**
 * struct iocpt_q_identify_cmd - queue identify command
 * @opcode:     Opcode
 * @type:       Logical queue type (enum iocpt_logical_qtype)
 * @lif_index:  LIF index
 * @ver:        Highest queue type version that the driver supports
 */
struct iocpt_q_identify_cmd {
	u8     opcode;
	u8     type;
	__le16 lif_index;
	u8     ver;
	u8     rsvd2[59];
};

/**
 * struct iocpt_q_identify_comp - queue identify command completion
 * @status:     Status of the command (enum iocpt_status_code)
 * @comp_index: Index in the descriptor ring for which this is the completion
 * @ver:        Queue type version that can be used with FW
 */
struct iocpt_q_identify_comp {
	u8     status;
	u8     rsvd;
	__le16 comp_index;
	u8     ver;
	u8     rsvd2[11];
};

/**
 * struct iocpt_q_init_cmd - Queue init command
 * @opcode:       Opcode
 * @type:         Logical queue type
 * @lif_index:    LIF index
 * @ver:          Queue type version
 * @index:        (LIF, qtype) relative admin queue index
 * @intr_index:   Interrupt control register index, or Event queue index
 * @pid:          Process ID
 * @flags:
 *    IRQ:        Interrupt requested on completion
 *    ENA:        Enable the queue.  If ENA=0 the queue is initialized
 *                but remains disabled, to be later enabled with the
 *                Queue Enable command.  If ENA=1, then queue is
 *                initialized and then enabled.
 *    SG:         Enable Scatter-Gather on the queue.
 * @cos:          Class of service for this queue
 * @ring_size:    Queue ring size, encoded as a log2(size), in
 *                number of descriptors.  The actual ring size is
 *                (1 << ring_size).  For example, to select a ring size
 *                of 64 descriptors write ring_size = 6. The minimum
 *                ring_size value is 2 for a ring of 4 descriptors.
 *                The maximum ring_size value is 12 for a ring of 4k
 *                descriptors.  Values of ring_size <2 and >12 are
 *                reserved.
 * @ring_base:    Queue ring base address
 * @cq_ring_base: Completion queue ring base address
 * @sg_ring_base: Scatter/Gather ring base address
 */
struct __rte_packed_begin iocpt_q_init_cmd {
	u8     opcode;
	u8     type;
	__le16 lif_index;
	u8     ver;
	u8     rsvd[3];
	__le32 index;
	__le16 pid;
	__le16 intr_index;
	__le16 flags;
#define IOCPT_QINIT_F_IRQ	0x01	/* Request interrupt on completion */
#define IOCPT_QINIT_F_ENA	0x02	/* Enable the queue */
#define IOCPT_QINIT_F_SG	0x04	/* Enable scatter/gather on queue */
	u8     cos;
#define IOCPT_QSIZE_MIN_LG2	2
#define IOCPT_QSIZE_MAX_LG2	12
	u8     ring_size;
	__le64 ring_base;
	__le64 cq_ring_base;
	__le64 sg_ring_base;
	u8     rsvd2[20];
} __rte_packed_end;

/**
 * struct iocpt_q_init_comp - Queue init command completion
 * @status:     Status of the command (enum iocpt_status_code)
 * @comp_index: Index in the descriptor ring for which this is the completion
 * @hw_index:   Hardware Queue ID
 * @hw_type:    Hardware Queue type
 * @color:      Color
 */
struct iocpt_q_init_comp {
	u8     status;
	u8     rsvd;
	__le16 comp_index;
	__le32 hw_index;
	u8     hw_type;
	u8     rsvd2[6];
	u8     color;
};

enum iocpt_desc_opcode {
	IOCPT_DESC_OPCODE_GCM_AEAD_ENCRYPT = 0,
	IOCPT_DESC_OPCODE_GCM_AEAD_DECRYPT = 1,
	IOCPT_DESC_OPCODE_XTS_ENCRYPT = 2,
	IOCPT_DESC_OPCODE_XTS_DECRYPT = 3,
};

#define IOCPT_DESC_F_AAD_VALID		0x1

/**
 * struct iocpt_desc - Crypto queue descriptor format
 * @opcode:
 *         IOCPT_DESC_OPCODE_GCM_AEAD_ENCRYPT:
 *                   Perform a GCM-AES encrypt operation
 *
 *         IOCPT_DESC_OPCODE_GCM_AEAD_DECRYPT:
 *                   Perform a GCM-AES decrypt operation
 *
 *         IOCPT_DESC_OPCODE_XTS_ENCRYPT:
 *                   Perform an XTS encrypt operation
 *
 *         IOCPT_DESC_OPCODE_XTS_DECRYPT:
 *                   Perform an XTS decrypt operation
 * @flags:
 *         IOCPT_DESC_F_AAD_VALID:
 *                   Source SGL contains an AAD addr and length
 * @num_src_dst_sgs: Number of scatter-gather elements in SG
 *                   descriptor (4 bits for source, 4 bits for destination)
 * @session_tag:     Session tag (key index)
 * @intr_ctx_addr:   Completion interrupt context address
 * @intr_ctx_data:   Completion interrupt context data
 */
struct __rte_packed_begin iocpt_crypto_desc {
	uint8_t  opcode;
	uint8_t  flags;
	uint8_t  num_src_dst_sgs;
#define IOCPT_DESC_NSGE_SRC_MASK	0xf
#define IOCPT_DESC_NSGE_SRC_SHIFT	0
#define IOCPT_DESC_NSGE_DST_MASK	0xf
#define IOCPT_DESC_NSGE_DST_SHIFT	4
	uint8_t  rsvd[9];
	__le32   session_tag;
	__le64   intr_ctx_addr;
	__le64   intr_ctx_data;
} __rte_packed_end;

static inline uint8_t iocpt_encode_nsge_src_dst(uint8_t src, uint8_t dst)
{
	uint8_t nsge_src_dst;

	nsge_src_dst = (src & IOCPT_DESC_NSGE_SRC_MASK) <<
		IOCPT_DESC_NSGE_SRC_SHIFT;
	nsge_src_dst |= (dst & IOCPT_DESC_NSGE_DST_MASK) <<
		IOCPT_DESC_NSGE_DST_SHIFT;

	return nsge_src_dst;
};

static inline void iocpt_decode_nsge_src_dst(uint8_t nsge_src_dst,
					     uint8_t *src, uint8_t *dst)
{
	*src = (nsge_src_dst >> IOCPT_DESC_NSGE_SRC_SHIFT) &
		IOCPT_DESC_NSGE_SRC_MASK;
	*dst = (nsge_src_dst >> IOCPT_DESC_NSGE_DST_SHIFT) &
		IOCPT_DESC_NSGE_DST_MASK;
};

/**
 * struct iocpt_crypto_sg_elem - Crypto scatter-gather (SG) descriptor element
 * @addr:	DMA address of SG element data buffer
 * @len:	Length of SG element data buffer, in bytes
 */
struct iocpt_crypto_sg_elem {
	__le64  addr;
	__le16  len;
	uint8_t rsvd[6];
};

/**
 * struct iocpt_crypto_sg_desc - Crypto scatter-gather (SG) list
 * @src_elems: Source SG elements; also destination in IP case
 *     AES_GCM:
 *         SGE0: Nonce
 *         SGE1: AAD (see IOCPT_DESC_F_AAD_VALID)
 *         SGE2 to SGE(N): Payload
 *         SGE(N+1): Auth tag
 * @dst_elems: Destination SG elements for OOP case; unused in IP case
 */
struct iocpt_crypto_sg_desc {
#define IOCPT_CRYPTO_MAX_SG_ELEMS	8
#define IOCPT_CRYPTO_NONCE_ELEM		0
#define IOCPT_CRYPTO_AAD_ELEM		1
	struct iocpt_crypto_sg_elem src_elems[IOCPT_CRYPTO_MAX_SG_ELEMS];
	struct iocpt_crypto_sg_elem dst_elems[IOCPT_CRYPTO_MAX_SG_ELEMS];
};

/**
 * struct iocpt_crypto_comp - Crypto queue completion descriptor
 * @status:	Status of the command (enum iocpt_status_code)
 * @comp_index:	Index in the descriptor ring for which this is the completion
 * @color:	Color bit
 */
struct iocpt_crypto_comp {
#define IOCPT_COMP_SUCCESS			0
#define IOCPT_COMP_INVAL_OPCODE_ERROR		1
#define IOCPT_COMP_UNSUPP_OPCODE_ERROR		2
#define IOCPT_COMP_SYMM_SRC_SG_ERROR		3
#define IOCPT_COMP_SYMM_DST_SG_ERROR		4
#define IOCPT_COMP_SYMM_SRC_DST_LEN_MISMATCH	5
#define IOCPT_COMP_SYMM_HW_QAVAIL_ERROR		6
#define IOCPT_COMP_SYMM_AUTH_VERIFY_ERROR	7
#define IOCPT_COMP_SYMM_OTHER_VERIFY_ERROR	8
#define IOCPT_COMP_SYMM_PI_MODE_CHKSUM_ERROR	9
#define IOCPT_COMP_SYMM_HARDWARE_ERROR		10
#define IOCPT_COMP_SYMM_KEY_IDX_ERROR		11
	u8     status;
	u8     rsvd;
	__le16 comp_index;
	u8     rsvd2[11];
	u8     color;
};

/**
 * enum iocpt_hw_features - Feature flags supported by hardware
 * @IOCPT_HW_SYM:   Symmetric crypto operations
 * @IOCPT_HW_ASYM:  Asymmetric crypto operations
 * @IOCPT_HW_CHAIN: Chained crypto operations
 * @IOCPT_HW_IP:    In-Place (destination same as source)
 * @IOCPT_HW_OOP:   Out-Of-Place (destination differs from source)
 */
enum iocpt_hw_features {
	IOCPT_HW_SYM             = BIT(0),
	IOCPT_HW_ASYM            = BIT(1),
	IOCPT_HW_CHAIN           = BIT(2),
	IOCPT_HW_IP              = BIT(3),
	IOCPT_HW_OOP             = BIT(4),
};

/**
 * struct iocpt_q_control_cmd - Queue control command
 * @opcode:	Opcode
 * @type:	Queue type
 * @lif_index:	LIF index
 * @index:	Queue index
 * @oper:	Operation (enum iocpt_q_control_oper)
 */
struct iocpt_q_control_cmd {
	u8     opcode;
	u8     type;
	__le16 lif_index;
	__le32 index;
	u8     oper;
	u8     rsvd2[55];
};

enum iocpt_q_control_oper {
	IOCPT_Q_DISABLE		= 0,
	IOCPT_Q_ENABLE		= 1,
};

/* NB: It will take 64 transfers to update a 2048B key */
#define IOCPT_SESS_KEY_LEN_MIN		16
#define IOCPT_SESS_KEY_LEN_MAX_SYMM	32
#define IOCPT_SESS_KEY_LEN_MAX_ASYM	2048
#define IOCPT_SESS_KEY_SEG_LEN		32
#define IOCPT_SESS_KEY_SEG_SHFT		5
#define IOCPT_SESS_KEY_SEG_CNT		\
	(IOCPT_SESS_KEY_LEN_MAX_SYMM >> IOCPT_SESS_KEY_SEG_SHFT)

enum iocpt_sess_type {
	IOCPT_SESS_NONE			= 0,
	IOCPT_SESS_AEAD_AES_GCM		= 1,
};

enum iocpt_sess_control_oper {
	IOCPT_SESS_INIT			= 0,
	IOCPT_SESS_UPDATE_KEY		= 2,
	IOCPT_SESS_DISABLE		= 3,
};

/**
 * struct iocpt_sess_control_cmd - Session control command
 * @opcode:      Opcode
 * @type:        Session type (enum iocpt_sess_type)
 * @lif_index:   LIF index
 * @oper:        Operation (enum iocpt_sess_control_oper)
 * @flags:
 *    END:       Indicates that this is the final segment of the key.
 *               When this flag is set, a write will be triggered from the
 *               controller's memory into the dedicated key-storage area.
 * @key_len:     Crypto key length in bytes
 * @index:       Session index, as allocated by PMD
 * @key_seg_len: Crypto key segment length in bytes
 * @key_seg_idx: Crypto key segment index
 * @key:         Crypto key
 */
struct iocpt_sess_control_cmd {
	u8     opcode;
	u8     type;
	__le16 lif_index;
	u8     oper;
	u8     flags;
#define IOCPT_SCTL_F_END	0x01	/* Final segment of key */
	__le16 key_len;
	__le32 index;
	u8     key_seg_len;
	u8     key_seg_idx;
	u8     rsvd[18];
	u8     key[IOCPT_SESS_KEY_SEG_LEN];
};

/**
 * struct iocpt_sess_control_comp - Session control command completion
 * @status:     Status of the command (enum iocpt_status_code)
 * @comp_index: Index in the descriptor ring for which this is the completion
 * @index:      Session index
 * @hw_type:    Hardware Session type
 * @color:      Color
 */
struct iocpt_sess_control_comp {
	u8     status;
	u8     rsvd;
	__le16 comp_index;
	__le32 index;
	u8     hw_type;
	u8     rsvd2[6];
	u8     color;
};

/**
 * enum iocpt_stats_ctl_cmd - List of commands for stats control
 * @IOCPT_STATS_CTL_RESET:      Reset statistics
 */
enum iocpt_stats_ctl_cmd {
	IOCPT_STATS_CTL_RESET		= 0,
};

/**
 * struct iocpt_dev_status - Device status register
 * @eid:             most recent NotifyQ event id
 */
struct iocpt_dev_status {
	__le64 eid;
	u8     rsvd2[56];
};

enum iocpt_dev_state {
	IOCPT_DEV_DISABLE	= 0,
	IOCPT_DEV_ENABLE	= 1,
	IOCPT_DEV_HANG_RESET	= 2,
};

/**
 * enum iocpt_dev_attr - List of device attributes
 * @IOCPT_DEV_ATTR_STATE:     Device state attribute
 * @IOCPT_DEV_ATTR_NAME:      Device name attribute
 * @IOCPT_DEV_ATTR_FEATURES:  Device feature attributes
 */
enum iocpt_dev_attr {
	IOCPT_DEV_ATTR_STATE    = 0,
	IOCPT_DEV_ATTR_NAME     = 1,
	IOCPT_DEV_ATTR_FEATURES = 2,
};

/**
 * struct iocpt_notify_event - Generic event reporting structure
 * @eid:   event number
 * @ecode: event code
 * @data:  unspecified data about the event
 *
 * This is the generic event report struct from which the other
 * actual events will be formed.
 */
struct iocpt_notify_event {
	__le64 eid;
	__le16 ecode;
	u8     data[54];
};

/**
 * struct iocpt_reset_event - Reset event notification
 * @eid:		event number
 * @ecode:		event code = IOCPT_EVENT_RESET
 * @reset_code:		reset type
 * @state:		0=pending, 1=complete, 2=error
 *
 * Sent when the NIC or some subsystem is going to be or
 * has been reset.
 */
struct iocpt_reset_event {
	__le64 eid;
	__le16 ecode;
	u8     reset_code;
	u8     state;
	u8     rsvd[52];
};

/**
 * struct iocpt_heartbeat_event - Sent periodically by NIC to indicate health
 * @eid:	event number
 * @ecode:	event code = IOCPT_EVENT_HEARTBEAT
 */
struct iocpt_heartbeat_event {
	__le64 eid;
	__le16 ecode;
	u8     rsvd[54];
};

/**
 * struct iocpt_log_event - Sent to notify the driver of an internal error
 * @eid:	event number
 * @ecode:	event code = IOCPT_EVENT_LOG
 * @data:	log data
 */
struct iocpt_log_event {
	__le64 eid;
	__le16 ecode;
	u8     data[54];
};

/**
 * union iocpt_lif_config - LIF configuration
 * @state:	    LIF state (enum iocpt_lif_state)
 * @name:	    LIF name
 * @features:	    LIF features active (enum iocpt_hw_features)
 * @queue_count:    Queue counts per queue-type
 */
union iocpt_lif_config {
	struct __rte_packed_begin {
		u8     state;
		u8     rsvd[3];
		char   name[IOCPT_IFNAMSIZ];
		u8     rsvd2[12];
		__le64 features;
		__le32 queue_count[IOCPT_QTYPE_MAX];
	} __rte_packed_end;
	__le32 words[56];
};

/**
 * struct iocpt_lif_status - LIF status register
 * @eid:	     most recent NotifyQ event id
 */
struct iocpt_lif_status {
	__le64 eid;
	u8     rsvd[56];
};

/**
 * struct iocpt_lif_info - LIF info structure
 * @config:	LIF configuration structure
 * @status:	LIF status structure
 * @stats:	LIF statistics structure
 */
struct iocpt_lif_info {
	union iocpt_lif_config config;
	struct iocpt_lif_status status;
};

union iocpt_dev_cmd {
	u32    words[16];
	struct iocpt_admin_cmd cmd;
	struct iocpt_nop_cmd nop;

	struct iocpt_dev_identify_cmd identify;
	struct iocpt_dev_reset_cmd reset;

	struct iocpt_lif_identify_cmd lif_identify;
	struct iocpt_lif_init_cmd lif_init;
	struct iocpt_lif_reset_cmd lif_reset;
	struct iocpt_lif_getattr_cmd lif_getattr;
	struct iocpt_lif_setattr_cmd lif_setattr;

	struct iocpt_q_identify_cmd q_identify;
	struct iocpt_q_init_cmd q_init;
	struct iocpt_q_control_cmd q_control;

	struct iocpt_sess_control_cmd sess_control;
};

union iocpt_dev_cmd_comp {
	u32    words[4];
	u8     status;
	struct iocpt_admin_comp comp;
	struct iocpt_nop_comp nop;

	struct iocpt_dev_identify_comp identify;
	struct iocpt_dev_reset_comp reset;

	struct iocpt_lif_identify_comp lif_identify;
	struct iocpt_lif_init_comp lif_init;
	struct iocpt_lif_getattr_comp lif_getattr;
	struct iocpt_lif_setattr_comp lif_setattr;

	struct iocpt_q_identify_comp q_identify;
	struct iocpt_q_init_comp q_init;

	struct iocpt_sess_control_comp sess_control;
};

/**
 * union iocpt_dev_info_regs - Device info register format (read-only)
 * @signature:       Signature value of 0x43585660 ('CRPT')
 * @version:         Current version of info
 * @asic_type:       Asic type
 * @asic_rev:        Asic revision
 * @fw_status:       Firmware status
 * @fw_heartbeat:    Firmware heartbeat counter
 * @serial_num:      Serial number
 * @fw_version:      Firmware version
 */
union iocpt_dev_info_regs {
#define IOCPT_FWVERS_BUFLEN 32
#define IOCPT_SERIAL_BUFLEN 32
	struct {
		u32    signature;
		u8     version;
		u8     asic_type;
		u8     asic_rev;
#define IOCPT_FW_STS_F_RUNNING	0x1
		u8     fw_status;
		u32    fw_heartbeat;
		char   fw_version[IOCPT_FWVERS_BUFLEN];
		char   serial_num[IOCPT_SERIAL_BUFLEN];
	};
	u32    words[512];
};

/**
 * union iocpt_dev_cmd_regs - Device command register format (read-write)
 * @doorbell:        Device Cmd Doorbell, write-only
 *                   Write a 1 to signal device to process cmd,
 *                   poll done for completion.
 * @done:            Done indicator, bit 0 == 1 when command is complete
 * @cmd:             Opcode-specific command bytes
 * @comp:            Opcode-specific response bytes
 * @data:            Opcode-specific side-data
 */
union iocpt_dev_cmd_regs {
	struct __rte_packed_begin {
		u32    doorbell;
		u32    done;
		union iocpt_dev_cmd         cmd;
		union iocpt_dev_cmd_comp    comp;
		u8     rsvd[48];
		u32    data[478];
	} __rte_packed_end;
	u32    words[512];
};

/**
 * union iocpt_dev_regs - Device register format for bar 0 page 0
 * @info:            Device info registers
 * @devcmd:          Device command registers
 */
union iocpt_dev_regs {
	struct __rte_packed_begin {
		union iocpt_dev_info_regs info;
		union iocpt_dev_cmd_regs  devcmd;
	} __rte_packed_end;
	__le32 words[1024];
};

union iocpt_adminq_cmd {
	struct iocpt_admin_cmd cmd;
	struct iocpt_nop_cmd nop;
	struct iocpt_q_identify_cmd q_identify;
	struct iocpt_q_init_cmd q_init;
	struct iocpt_q_control_cmd q_control;
	struct iocpt_lif_setattr_cmd lif_setattr;
	struct iocpt_lif_getattr_cmd lif_getattr;
	struct iocpt_sess_control_cmd sess_control;
};

union iocpt_adminq_comp {
	struct iocpt_admin_comp comp;
	struct iocpt_nop_comp nop;
	struct iocpt_q_identify_comp q_identify;
	struct iocpt_q_init_comp q_init;
	struct iocpt_lif_setattr_comp lif_setattr;
	struct iocpt_lif_getattr_comp lif_getattr;
	struct iocpt_sess_control_comp sess_control;
};

union iocpt_notify_comp {
	struct iocpt_notify_event event;
	struct iocpt_reset_event reset;
	struct iocpt_heartbeat_event heartbeat;
	struct iocpt_log_event log;
};

/**
 * union iocpt_dev_identity - device identity information
 * @version:          Version of device identify
 * @type:             Identify type (0 for now)
 * @state:            Device state
 * @nlifs:            Number of LIFs provisioned
 * @nintrs:           Number of interrupts provisioned
 * @ndbpgs_per_lif:   Number of doorbell pages per LIF
 * @intr_coal_mult:   Interrupt coalescing multiplication factor
 *                    Scale user-supplied interrupt coalescing
 *                    value in usecs to device units using:
 *                    device units = usecs * mult / div
 * @intr_coal_div:    Interrupt coalescing division factor
 *                    Scale user-supplied interrupt coalescing
 *                    value in usecs to device units using:
 *                    device units = usecs * mult / div
 */
union iocpt_dev_identity {
	struct {
		u8     version;
		u8     type;
		u8     state;
		u8     rsvd;
		__le32 nlifs;
		__le32 nintrs;
		__le32 ndbpgs_per_lif;
		__le32 intr_coal_mult;
		__le32 intr_coal_div;
		u8     rsvd2[8];
	};
	__le32 words[8];
};

/**
 * union iocpt_lif_identity - LIF identity information (type-specific)
 *
 * @features:           LIF features (see enum iocpt_hw_features)
 * @version:            Identify structure version
 * @hw_index:           LIF hardware index
 * @max_nb_sessions:    Maximum number of sessions supported
 * @config:             LIF config struct with features, q counts
 */
union iocpt_lif_identity {
	struct __rte_packed_begin {
		__le64 features;

		u8 version;
		u8 hw_index;
		u8 rsvd[2];
		__le32 max_nb_sessions;
		u8 rsvd2[120];
		union iocpt_lif_config config;
	} __rte_packed_end;
	__le32 words[90];
};

/**
 * union iocpt_q_identity - queue identity information
 *     @version:        Queue type version that can be used with FW
 *     @supported:      Bitfield of queue versions, first bit = ver 0
 *     @features:       Queue features
 *     @desc_sz:        Descriptor size
 *     @comp_sz:        Completion descriptor size
 *     @sg_desc_sz:     Scatter/Gather descriptor size
 *     @max_sg_elems:   Maximum number of Scatter/Gather elements
 *     @sg_desc_stride: Number of Scatter/Gather elements per descriptor
 */
union iocpt_q_identity {
	struct {
		u8      version;
		u8      supported;
		u8      rsvd[6];
#define IOCPT_QIDENT_F_CQ	0x01	/* queue has completion ring */
#define IOCPT_QIDENT_F_SG	0x02	/* queue has scatter/gather ring */
		__le64  features;
		__le16  desc_sz;
		__le16  comp_sz;
		__le16  sg_desc_sz;
		__le16  max_sg_elems;
		__le16  sg_desc_stride;
	};
	__le32 words[20];
};

struct iocpt_identity {
	union iocpt_dev_identity dev;
	union iocpt_lif_identity lif;
	union iocpt_q_identity q;
};

#endif /* _IONIC_CRYPTO_IF_H_ */
