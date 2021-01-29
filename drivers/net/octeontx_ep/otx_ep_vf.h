/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef _OTX_EP_VF_H_
#define _OTX_EP_VF_H_

#define OTX_EP_RING_OFFSET                (0x1ull << 17)

/* OTX_EP VF IQ Registers */
#define OTX_EP_R_IN_CONTROL_START         (0x10000)
#define OTX_EP_R_IN_CONTROL(ring)  \
	(OTX_EP_R_IN_CONTROL_START + ((ring) * OTX_EP_RING_OFFSET))

/* OTX_EP VF IQ Masks */
#define OTX_EP_R_IN_CTL_RPVF_MASK       (0xF)
#define	OTX_EP_R_IN_CTL_RPVF_POS        (48)

#define OTX_EP_R_IN_CTL_IDLE            (0x1ull << 28)
#define OTX_EP_R_IN_CTL_RDSIZE          (0x3ull << 25) /* Setting to max(4) */
#define OTX_EP_R_IN_CTL_IS_64B          (0x1ull << 24)
#define OTX_EP_R_IN_CTL_ESR             (0x1ull << 1)
/* OTX_EP VF OQ Registers */
#define OTX_EP_R_OUT_CONTROL_START           (0x10150)
#define OTX_EP_R_OUT_CONTROL(ring)    \
	(OTX_EP_R_OUT_CONTROL_START + ((ring) * OTX_EP_RING_OFFSET))
/* OTX_EP VF OQ Masks */
#define OTX_EP_R_OUT_CTL_ES_I         (1ull << 34)
#define OTX_EP_R_OUT_CTL_NSR_I        (1ull << 33)
#define OTX_EP_R_OUT_CTL_ROR_I        (1ull << 32)
#define OTX_EP_R_OUT_CTL_ES_D         (1ull << 30)
#define OTX_EP_R_OUT_CTL_NSR_D        (1ull << 29)
#define OTX_EP_R_OUT_CTL_ROR_D        (1ull << 28)
#define OTX_EP_R_OUT_CTL_ES_P         (1ull << 26)
#define OTX_EP_R_OUT_CTL_NSR_P        (1ull << 25)
#define OTX_EP_R_OUT_CTL_ROR_P        (1ull << 24)
#define OTX_EP_R_OUT_CTL_IMODE        (1ull << 23)

#define PCI_DEVID_OCTEONTX_EP_VF 0xa303

/* this is a static value set by SLI PF driver in octeon
 * No handshake is available
 * Change this if changing the value in SLI PF driver
 */
#define SDP_GBL_WMARK 0x100

int
otx_ep_vf_setup_device(struct otx_ep_device *otx_ep);
#endif /*_OTX_EP_VF_H_ */
