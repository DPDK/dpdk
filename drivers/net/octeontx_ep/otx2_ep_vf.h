/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef _OTX2_EP_VF_H_
#define _OTX2_EP_VF_H_

int
otx2_ep_vf_setup_device(struct otx_ep_device *sdpvf);

struct otx2_ep_instr_64B {
	/* Pointer where the input data is available. */
	uint64_t dptr;

	/* OTX_EP Instruction Header. */
	union otx_ep_instr_ih ih;

	/** Pointer where the response for a RAW mode packet
	 * will be written by OCTEON TX.
	 */
	uint64_t rptr;

	/* Input Request Header. */
	union otx_ep_instr_irh irh;

	/* Additional headers available in a 64-byte instruction. */
	uint64_t exhdr[4];
};

#endif /*_OTX2_EP_VF_H_ */

