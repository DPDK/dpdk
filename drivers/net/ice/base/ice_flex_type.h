/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2018
 */

#ifndef _ICE_FLEX_TYPE_H_
#define _ICE_FLEX_TYPE_H_

/* Extraction Sequence (Field Vector) Table */
struct ice_fv_word {
	u8 prot_id;
	u8 off;		/* Offset within the protocol header */
};

#define ICE_MAX_FV_WORDS 48
struct ice_fv {
	struct ice_fv_word ew[ICE_MAX_FV_WORDS];
};

#endif /* _ICE_FLEX_TYPE_H_ */
