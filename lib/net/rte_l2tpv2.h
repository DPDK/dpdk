/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation.
 */

#ifndef _RTE_L2TPV2_H_
#define _RTE_L2TPV2_H_

/**
 * @file
 *
 * L2TP header:
 *
 * `-0--------------------1----------------2-------------------3`
 *
 * `-0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1`
 *
 * `+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+`
 *
 * `|T|L|x|x|S|x|O|P|x|x|x|x|--Ver--|-----------Length (opt)--------|`
 *
 * `+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+`
 *
 * `|-----------Tunnel ID-----------|-----------Session ID----------|`
 *
 * `+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+`
 *
 * `|-----------Ns (opt)------------|-----------Nr (opt)------------|`
 *
 * `+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+`
 *
 * `|---------Offset Size (opt)-----|---------Offset pad... (opt)`
 *
 * `+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+`
 *
 * The Type (T) bit indicates the type of message. It is set to 0 for a data
 * message and 1 for a control message.
 *
 * If the Length (L) bit is 1, the Length field is present. This bit MUST be
 * set to 1 for control messages.
 *
 * The x bits are reserved for future extensions. All reserved bits MUST
 * be set to 0 on outgoing messages and ignored on incoming messages.
 *
 * If the Sequence (S) bit is set to 1 the Ns and Nr fields are present.
 * The S bit MUST be set to 1 for control messages.
 *
 * If the Offset (O) bit is 1, the Offset Size field is present. The O
 * bit MUST be set to 0 for control messages.
 *
 * If the Priority (P) bit is 1, this data message should receive
 * preferential treatment in its local queuing and transmission.
 * The P bit MUST be set to 0 for control messages.
 *
 * Ver MUST be 2, indicating the version of the L2TP data message header.
 *
 * The Length field indicates the total length of the message in octets.
 *
 * Tunnel ID indicates the identifier for the control connection.
 *
 * Session ID indicates the identifier for a session within a tunnel.
 *
 * Ns indicates the sequence number for this data or control message.
 *
 * Nr indicates the sequence number expected in the next control message
 * to be received.
 *
 * The Offset Size field, if present, specifies the number of octets
 * past the L2TP header at which the payload data is expected to start.
 * Actual data within the offset padding is undefined. If the offset
 * field is present, the L2TP header ends after the last octet of the
 * offset padding.
 */

#include <stdint.h>
#include <rte_byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * L2TPv2 Message Type
 */
#define RTE_L2TPV2_MSG_TYPE_CONTROL	0xC802
#define RTE_L2TPV2_MSG_TYPE_DATA	0x0002
#define RTE_L2TPV2_MSG_TYPE_DATA_L	0x4002
#define RTE_L2TPV2_MSG_TYPE_DATA_S	0x0802
#define RTE_L2TPV2_MSG_TYPE_DATA_O	0x0202
#define RTE_L2TPV2_MSG_TYPE_DATA_L_S	0x4802
#define RTE_L2TPV2_MSG_TYPE_DATA_L_O	0x4202
#define RTE_L2TPV2_MSG_TYPE_DATA_S_O	0x0A02
#define RTE_L2TPV2_MSG_TYPE_DATA_L_S_O	0x4A02

/**
 * L2TPv2 Common Header
 */
RTE_STD_C11
struct rte_l2tpv2_common_hdr {
	union {
		/** header flags and protocol version */
		rte_be16_t flags_version;
		__extension__
		struct {
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
			uint16_t ver:4;		/**< protocol version */
			uint16_t res3:4;	/**< reserved */
			uint16_t p:1;		/**< priority option bit */
			uint16_t o:1;		/**< offset option bit */
			uint16_t res2:1;	/**< reserved */
			uint16_t s:1;		/**< ns/nr option bit */
			uint16_t res1:2;	/**< reserved */
			uint16_t l:1;		/**< length option bit */
			uint16_t t:1;		/**< message Type */
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
			uint16_t t:1;		/**< message Type */
			uint16_t l:1;		/**< length option bit */
			uint16_t res1:2;	/**< reserved */
			uint16_t s:1;		/**< ns/nr option bit */
			uint16_t res2:1;	/**< reserved */
			uint16_t o:1;		/**< offset option bit */
			uint16_t p:1;		/**< priority option bit */
			uint16_t res3:4;	/**< reserved */
			uint16_t ver:4;		/**< protocol version */
#endif
		};
	};
};

/**
 * L2TPv2 message Header contains all options(length, ns, nr,
 * offset size, offset padding).
 */
struct rte_l2tpv2_msg_with_all_options {
	rte_be16_t length;		/**< length(16) */
	rte_be16_t tunnel_id;		/**< tunnel ID(16) */
	rte_be16_t session_id;		/**< session ID(16) */
	rte_be16_t ns;			/**< Ns(16) */
	rte_be16_t nr;			/**< Nr(16) */
	rte_be16_t offset_size;		/**< offset size(16) */
	uint8_t   *offset_padding;	/**< offset padding(variable length) */
} __rte_packed;

/**
 * L2TPv2 message Header contains all options except length(ns, nr,
 * offset size, offset padding).
 */
struct rte_l2tpv2_msg_without_length {
	rte_be16_t tunnel_id;		/**< tunnel ID(16) */
	rte_be16_t session_id;		/**< session ID(16) */
	rte_be16_t ns;			/**< Ns(16) */
	rte_be16_t nr;			/**< Nr(16) */
	rte_be16_t offset_size;		/**< offset size(16) */
	uint8_t   *offset_padding;	/**< offset padding(variable length) */
} __rte_packed;

/**
 * L2TPv2 message Header contains all options except ns_nr(length,
 * offset size, offset padding).
 * Ns and Nr MUST be together.
 */
struct rte_l2tpv2_msg_without_ns_nr {
	rte_be16_t length;		/**< length(16) */
	rte_be16_t tunnel_id;		/**< tunnel ID(16) */
	rte_be16_t session_id;		/**< session ID(16) */
	rte_be16_t offset_size;		/**< offset size(16) */
	uint8_t   *offset_padding;	/**< offset padding(variable length) */
};

/**
 * L2TPv2 message Header contains all options except ns_nr(length, ns, nr).
 * offset size and offset padding MUST be together.
 */
struct rte_l2tpv2_msg_without_offset {
	rte_be16_t length;		/**< length(16) */
	rte_be16_t tunnel_id;		/**< tunnel ID(16) */
	rte_be16_t session_id;		/**< session ID(16) */
	rte_be16_t ns;			/**< Ns(16) */
	rte_be16_t nr;			/**< Nr(16) */
};

/**
 * L2TPv2 message Header contains options offset size and offset padding.
 */
struct rte_l2tpv2_msg_with_offset {
	rte_be16_t tunnel_id;		/**< tunnel ID(16) */
	rte_be16_t session_id;		/**< session ID(16) */
	rte_be16_t offset_size;		/**< offset size(16) */
	uint8_t   *offset_padding;	/**< offset padding(variable length) */
} __rte_packed;

/**
 * L2TPv2 message Header contains options ns and nr.
 */
struct rte_l2tpv2_msg_with_ns_nr {
	rte_be16_t tunnel_id;		/**< tunnel ID(16) */
	rte_be16_t session_id;		/**< session ID(16) */
	rte_be16_t ns;			/**< Ns(16) */
	rte_be16_t nr;			/**< Nr(16) */
};

/**
 * L2TPv2 message Header contains option length.
 */
struct rte_l2tpv2_msg_with_length {
	rte_be16_t length;		/**< length(16) */
	rte_be16_t tunnel_id;		/**< tunnel ID(16) */
	rte_be16_t session_id;		/**< session ID(16) */
};

/**
 * L2TPv2 message Header without all options.
 */
struct rte_l2tpv2_msg_without_all_options {
	rte_be16_t tunnel_id;		/**< tunnel ID(16) */
	rte_be16_t session_id;		/**< session ID(16) */
};

/**
 * L2TPv2 Combined Message Header Format: Common Header + Options
 */
RTE_STD_C11
struct rte_l2tpv2_combined_msg_hdr {
	struct rte_l2tpv2_common_hdr common; /**< common header */
	union {
		/** header with all options */
		struct rte_l2tpv2_msg_with_all_options type0;
		/** header with all options except length */
		struct rte_l2tpv2_msg_without_length type1;
		/** header with all options except ns/nr */
		struct rte_l2tpv2_msg_without_ns_nr type2;
		/** header with all options except offset */
		struct rte_l2tpv2_msg_without_offset type3;
		/** header with offset options */
		struct rte_l2tpv2_msg_with_offset type4;
		/** header with ns/nr options */
		struct rte_l2tpv2_msg_with_ns_nr type5;
		/** header with length option */
		struct rte_l2tpv2_msg_with_length type6;
		/** header without all options */
		struct rte_l2tpv2_msg_without_all_options type7;
	};
} __rte_packed;

#ifdef __cplusplus
}
#endif

#endif /* _RTE_L2TPV2_H_ */
