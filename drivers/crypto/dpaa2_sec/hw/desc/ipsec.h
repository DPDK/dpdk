/*-
 * This file is provided under a dual BSD/GPLv2 license. When using or
 * redistributing this file, you may do so under either license.
 *
 *   BSD LICENSE
 *
 * Copyright 2008-2016 Freescale Semiconductor Inc.
 * Copyright 2016 NXP.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *   GPL LICENSE SUMMARY
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __DESC_IPSEC_H__
#define __DESC_IPSEC_H__

#include "hw/rta.h"
#include "common.h"

/**
 * DOC: IPsec Shared Descriptor Constructors
 *
 * Shared descriptors for IPsec protocol.
 */

/* General IPSec ESP encap / decap PDB options */

/**
 * PDBOPTS_ESP_ESN - Extended sequence included
 */
#define PDBOPTS_ESP_ESN		0x10

/**
 * PDBOPTS_ESP_IPVSN - Process IPv6 header
 *
 * Valid only for IPsec legacy mode.
 */
#define PDBOPTS_ESP_IPVSN	0x02

/**
 * PDBOPTS_ESP_TUNNEL - Tunnel mode next-header byte
 *
 * Valid only for IPsec legacy mode.
 */
#define PDBOPTS_ESP_TUNNEL	0x01

/* IPSec ESP Encap PDB options */

/**
 * PDBOPTS_ESP_UPDATE_CSUM - Update ip header checksum
 *
 * Valid only for IPsec legacy mode.
 */
#define PDBOPTS_ESP_UPDATE_CSUM 0x80

/**
 * PDBOPTS_ESP_DIFFSERV - Copy TOS/TC from inner iphdr
 *
 * Valid only for IPsec legacy mode.
 */
#define PDBOPTS_ESP_DIFFSERV	0x40

/**
 * PDBOPTS_ESP_IVSRC - IV comes from internal random gen
 */
#define PDBOPTS_ESP_IVSRC	0x20

/**
 * PDBOPTS_ESP_IPHDRSRC - IP header comes from PDB
 *
 * Valid only for IPsec legacy mode.
 */
#define PDBOPTS_ESP_IPHDRSRC	0x08

/**
 * PDBOPTS_ESP_INCIPHDR - Prepend IP header to output frame
 *
 * Valid only for IPsec legacy mode.
 */
#define PDBOPTS_ESP_INCIPHDR	0x04

/**
 * PDBOPTS_ESP_OIHI_MASK - Mask for Outer IP Header Included
 *
 * Valid only for IPsec new mode.
 */
#define PDBOPTS_ESP_OIHI_MASK	0x0c

/**
 * PDBOPTS_ESP_OIHI_PDB_INL - Prepend IP header to output frame from PDB (where
 *                            it is inlined).
 *
 * Valid only for IPsec new mode.
 */
#define PDBOPTS_ESP_OIHI_PDB_INL 0x0c

/**
 * PDBOPTS_ESP_OIHI_PDB_REF - Prepend IP header to output frame from PDB
 *                            (referenced by pointer).
 *
 * Vlid only for IPsec new mode.
 */
#define PDBOPTS_ESP_OIHI_PDB_REF 0x08

/**
 * PDBOPTS_ESP_OIHI_IF - Prepend IP header to output frame from input frame
 *
 * Valid only for IPsec new mode.
 */
#define PDBOPTS_ESP_OIHI_IF	0x04

/**
 * PDBOPTS_ESP_NAT - Enable RFC 3948 UDP-encapsulated-ESP
 *
 * Valid only for IPsec new mode.
 */
#define PDBOPTS_ESP_NAT		0x02

/**
 * PDBOPTS_ESP_NUC - Enable NAT UDP Checksum
 *
 * Valid only for IPsec new mode.
 */
#define PDBOPTS_ESP_NUC		0x01

/* IPSec ESP Decap PDB options */

/**
 * PDBOPTS_ESP_ARS_MASK - antireplay window mask
 */
#define PDBOPTS_ESP_ARS_MASK	0xc0

/**
 * PDBOPTS_ESP_ARSNONE - No antireplay window
 */
#define PDBOPTS_ESP_ARSNONE	0x00

/**
 * PDBOPTS_ESP_ARS64 - 64-entry antireplay window
 */
#define PDBOPTS_ESP_ARS64	0xc0

/**
 * PDBOPTS_ESP_ARS128 - 128-entry antireplay window
 *
 * Valid only for IPsec new mode.
 */
#define PDBOPTS_ESP_ARS128	0x80

/**
 * PDBOPTS_ESP_ARS32 - 32-entry antireplay window
 */
#define PDBOPTS_ESP_ARS32	0x40

/**
 * PDBOPTS_ESP_VERIFY_CSUM - Validate ip header checksum
 *
 * Valid only for IPsec legacy mode.
 */
#define PDBOPTS_ESP_VERIFY_CSUM 0x20

/**
 * PDBOPTS_ESP_TECN - Implement RRFC6040 ECN tunneling from outer header to
 *                    inner header.
 *
 * Valid only for IPsec new mode.
 */
#define PDBOPTS_ESP_TECN	0x20

/**
 * PDBOPTS_ESP_OUTFMT - Output only decapsulation
 *
 * Valid only for IPsec legacy mode.
 */
#define PDBOPTS_ESP_OUTFMT	0x08

/**
 * PDBOPTS_ESP_AOFL - Adjust out frame len
 *
 * Valid only for IPsec legacy mode and for SEC >= 5.3.
 */
#define PDBOPTS_ESP_AOFL	0x04

/**
 * PDBOPTS_ESP_ETU - EtherType Update
 *
 * Add corresponding ethertype (0x0800 for IPv4, 0x86dd for IPv6) in the output
 * frame.
 * Valid only for IPsec new mode.
 */
#define PDBOPTS_ESP_ETU		0x01

#define PDBHMO_ESP_DECAP_SHIFT		28
#define PDBHMO_ESP_ENCAP_SHIFT		28
#define PDBNH_ESP_ENCAP_SHIFT		16
#define PDBNH_ESP_ENCAP_MASK		(0xff << PDBNH_ESP_ENCAP_SHIFT)
#define PDBHDRLEN_ESP_DECAP_SHIFT	16
#define PDBHDRLEN_MASK			(0x0fff << PDBHDRLEN_ESP_DECAP_SHIFT)
#define PDB_NH_OFFSET_SHIFT		8
#define PDB_NH_OFFSET_MASK		(0xff << PDB_NH_OFFSET_SHIFT)

/**
 * PDBHMO_ESP_DECAP_DTTL - IPsec ESP decrement TTL (IPv4) / Hop limit (IPv6)
 *                         HMO option.
 */
#define PDBHMO_ESP_DECAP_DTTL	(0x02 << PDBHMO_ESP_DECAP_SHIFT)

/**
 * PDBHMO_ESP_ENCAP_DTTL - IPsec ESP increment TTL (IPv4) / Hop limit (IPv6)
 *                         HMO option.
 */
#define PDBHMO_ESP_ENCAP_DTTL	(0x02 << PDBHMO_ESP_ENCAP_SHIFT)

/**
 * PDBHMO_ESP_DIFFSERV - (Decap) DiffServ Copy - Copy the IPv4 TOS or IPv6
 *                       Traffic Class byte from the outer IP header to the
 *                       inner IP header.
 */
#define PDBHMO_ESP_DIFFSERV	(0x01 << PDBHMO_ESP_DECAP_SHIFT)

/**
 * PDBHMO_ESP_SNR - (Encap) - Sequence Number Rollover control
 *
 * Configures behaviour in case of SN / ESN rollover:
 * error if SNR = 1, rollover allowed if SNR = 0.
 * Valid only for IPsec new mode.
 */
#define PDBHMO_ESP_SNR		(0x01 << PDBHMO_ESP_ENCAP_SHIFT)

/**
 * PDBHMO_ESP_DFBIT - (Encap) Copy DF bit - if an IPv4 tunnel mode outer IP
 *                    header is coming from the PDB, copy the DF bit from the
 *                    inner IP header to the outer IP header.
 */
#define PDBHMO_ESP_DFBIT	(0x04 << PDBHMO_ESP_ENCAP_SHIFT)

/**
 * PDBHMO_ESP_DFV - (Decap) - DF bit value
 *
 * If ODF = 1, DF bit in output frame is replaced by DFV.
 * Valid only from SEC Era 5 onwards.
 */
#define PDBHMO_ESP_DFV		(0x04 << PDBHMO_ESP_DECAP_SHIFT)

/**
 * PDBHMO_ESP_ODF - (Decap) Override DF bit in IPv4 header of decapsulated
 *                  output frame.
 *
 * If ODF = 1, DF is replaced with the value of DFV bit.
 * Valid only from SEC Era 5 onwards.
 */
#define PDBHMO_ESP_ODF		(0x08 << PDBHMO_ESP_DECAP_SHIFT)

/**
 * struct ipsec_encap_cbc - PDB part for IPsec CBC encapsulation
 * @iv: 16-byte array initialization vector
 */
struct ipsec_encap_cbc {
	uint8_t iv[16];
};


/**
 * struct ipsec_encap_ctr - PDB part for IPsec CTR encapsulation
 * @ctr_nonce: 4-byte array nonce
 * @ctr_initial: initial count constant
 * @iv: initialization vector
 */
struct ipsec_encap_ctr {
	uint8_t ctr_nonce[4];
	uint32_t ctr_initial;
	uint64_t iv;
};

/**
 * struct ipsec_encap_ccm - PDB part for IPsec CCM encapsulation
 * @salt: 3-byte array salt (lower 24 bits)
 * @ccm_opt: CCM algorithm options - MSB-LSB description:
 *  b0_flags (8b) - CCM B0; use 0x5B for 8-byte ICV, 0x6B for 12-byte ICV,
 *    0x7B for 16-byte ICV (cf. RFC4309, RFC3610)
 *  ctr_flags (8b) - counter flags; constant equal to 0x3
 *  ctr_initial (16b) - initial count constant
 * @iv: initialization vector
 */
struct ipsec_encap_ccm {
	uint8_t salt[4];
	uint32_t ccm_opt;
	uint64_t iv;
};

/**
 * struct ipsec_encap_gcm - PDB part for IPsec GCM encapsulation
 * @salt: 3-byte array salt (lower 24 bits)
 * @rsvd: reserved, do not use
 * @iv: initialization vector
 */
struct ipsec_encap_gcm {
	uint8_t salt[4];
	uint32_t rsvd;
	uint64_t iv;
};

/**
 * struct ipsec_encap_pdb - PDB for IPsec encapsulation
 * @options: MSB-LSB description (both for legacy and new modes)
 *  hmo (header manipulation options) - 4b
 *  reserved - 4b
 *  next header (legacy) / reserved (new) - 8b
 *  next header offset (legacy) / AOIPHO (actual outer IP header offset) - 8b
 *  option flags (depend on selected algorithm) - 8b
 * @seq_num_ext_hi: (optional) IPsec Extended Sequence Number (ESN)
 * @seq_num: IPsec sequence number
 * @spi: IPsec SPI (Security Parameters Index)
 * @ip_hdr_len: optional IP Header length (in bytes)
 *  reserved - 16b
 *  Opt. IP Hdr Len - 16b
 * @ip_hdr: optional IP Header content (only for IPsec legacy mode)
 */
struct ipsec_encap_pdb {
	uint32_t options;
	uint32_t seq_num_ext_hi;
	uint32_t seq_num;
	union {
		struct ipsec_encap_cbc cbc;
		struct ipsec_encap_ctr ctr;
		struct ipsec_encap_ccm ccm;
		struct ipsec_encap_gcm gcm;
	};
	uint32_t spi;
	uint32_t ip_hdr_len;
	uint8_t ip_hdr[0];
};

static inline unsigned int
__rta_copy_ipsec_encap_pdb(struct program *program,
			   struct ipsec_encap_pdb *pdb,
			   uint32_t algtype)
{
	unsigned int start_pc = program->current_pc;

	__rta_out32(program, pdb->options);
	__rta_out32(program, pdb->seq_num_ext_hi);
	__rta_out32(program, pdb->seq_num);

	switch (algtype & OP_PCL_IPSEC_CIPHER_MASK) {
	case OP_PCL_IPSEC_DES_IV64:
	case OP_PCL_IPSEC_DES:
	case OP_PCL_IPSEC_3DES:
	case OP_PCL_IPSEC_AES_CBC:
	case OP_PCL_IPSEC_NULL:
		rta_copy_data(program, pdb->cbc.iv, sizeof(pdb->cbc.iv));
		break;

	case OP_PCL_IPSEC_AES_CTR:
		rta_copy_data(program, pdb->ctr.ctr_nonce,
			      sizeof(pdb->ctr.ctr_nonce));
		__rta_out32(program, pdb->ctr.ctr_initial);
		__rta_out64(program, true, pdb->ctr.iv);
		break;

	case OP_PCL_IPSEC_AES_CCM8:
	case OP_PCL_IPSEC_AES_CCM12:
	case OP_PCL_IPSEC_AES_CCM16:
		rta_copy_data(program, pdb->ccm.salt, sizeof(pdb->ccm.salt));
		__rta_out32(program, pdb->ccm.ccm_opt);
		__rta_out64(program, true, pdb->ccm.iv);
		break;

	case OP_PCL_IPSEC_AES_GCM8:
	case OP_PCL_IPSEC_AES_GCM12:
	case OP_PCL_IPSEC_AES_GCM16:
	case OP_PCL_IPSEC_AES_NULL_WITH_GMAC:
		rta_copy_data(program, pdb->gcm.salt, sizeof(pdb->gcm.salt));
		__rta_out32(program, pdb->gcm.rsvd);
		__rta_out64(program, true, pdb->gcm.iv);
		break;
	}

	__rta_out32(program, pdb->spi);
	__rta_out32(program, pdb->ip_hdr_len);

	return start_pc;
}

/**
 * struct ipsec_decap_cbc - PDB part for IPsec CBC decapsulation
 * @rsvd: reserved, do not use
 */
struct ipsec_decap_cbc {
	uint32_t rsvd[2];
};

/**
 * struct ipsec_decap_ctr - PDB part for IPsec CTR decapsulation
 * @ctr_nonce: 4-byte array nonce
 * @ctr_initial: initial count constant
 */
struct ipsec_decap_ctr {
	uint8_t ctr_nonce[4];
	uint32_t ctr_initial;
};

/**
 * struct ipsec_decap_ccm - PDB part for IPsec CCM decapsulation
 * @salt: 3-byte salt (lower 24 bits)
 * @ccm_opt: CCM algorithm options - MSB-LSB description:
 *  b0_flags (8b) - CCM B0; use 0x5B for 8-byte ICV, 0x6B for 12-byte ICV,
 *    0x7B for 16-byte ICV (cf. RFC4309, RFC3610)
 *  ctr_flags (8b) - counter flags; constant equal to 0x3
 *  ctr_initial (16b) - initial count constant
 */
struct ipsec_decap_ccm {
	uint8_t salt[4];
	uint32_t ccm_opt;
};

/**
 * struct ipsec_decap_gcm - PDB part for IPsec GCN decapsulation
 * @salt: 4-byte salt
 * @rsvd: reserved, do not use
 */
struct ipsec_decap_gcm {
	uint8_t salt[4];
	uint32_t rsvd;
};

/**
 * struct ipsec_decap_pdb - PDB for IPsec decapsulation
 * @options: MSB-LSB description (both for legacy and new modes)
 *  hmo (header manipulation options) - 4b
 *  IP header length - 12b
 *  next header offset (legacy) / AOIPHO (actual outer IP header offset) - 8b
 *  option flags (depend on selected algorithm) - 8b
 * @seq_num_ext_hi: (optional) IPsec Extended Sequence Number (ESN)
 * @seq_num: IPsec sequence number
 * @anti_replay: Anti-replay window; size depends on ARS (option flags);
 *  format must be Big Endian, irrespective of platform
 */
struct ipsec_decap_pdb {
	uint32_t options;
	union {
		struct ipsec_decap_cbc cbc;
		struct ipsec_decap_ctr ctr;
		struct ipsec_decap_ccm ccm;
		struct ipsec_decap_gcm gcm;
	};
	uint32_t seq_num_ext_hi;
	uint32_t seq_num;
	uint32_t anti_replay[4];
};

static inline unsigned int
__rta_copy_ipsec_decap_pdb(struct program *program,
			   struct ipsec_decap_pdb *pdb,
			   uint32_t algtype)
{
	unsigned int start_pc = program->current_pc;
	unsigned int i, ars;

	__rta_out32(program, pdb->options);

	switch (algtype & OP_PCL_IPSEC_CIPHER_MASK) {
	case OP_PCL_IPSEC_DES_IV64:
	case OP_PCL_IPSEC_DES:
	case OP_PCL_IPSEC_3DES:
	case OP_PCL_IPSEC_AES_CBC:
	case OP_PCL_IPSEC_NULL:
		__rta_out32(program, pdb->cbc.rsvd[0]);
		__rta_out32(program, pdb->cbc.rsvd[1]);
		break;

	case OP_PCL_IPSEC_AES_CTR:
		rta_copy_data(program, pdb->ctr.ctr_nonce,
			      sizeof(pdb->ctr.ctr_nonce));
		__rta_out32(program, pdb->ctr.ctr_initial);
		break;

	case OP_PCL_IPSEC_AES_CCM8:
	case OP_PCL_IPSEC_AES_CCM12:
	case OP_PCL_IPSEC_AES_CCM16:
		rta_copy_data(program, pdb->ccm.salt, sizeof(pdb->ccm.salt));
		__rta_out32(program, pdb->ccm.ccm_opt);
		break;

	case OP_PCL_IPSEC_AES_GCM8:
	case OP_PCL_IPSEC_AES_GCM12:
	case OP_PCL_IPSEC_AES_GCM16:
	case OP_PCL_IPSEC_AES_NULL_WITH_GMAC:
		rta_copy_data(program, pdb->gcm.salt, sizeof(pdb->gcm.salt));
		__rta_out32(program, pdb->gcm.rsvd);
		break;
	}

	__rta_out32(program, pdb->seq_num_ext_hi);
	__rta_out32(program, pdb->seq_num);

	switch (pdb->options & PDBOPTS_ESP_ARS_MASK) {
	case PDBOPTS_ESP_ARS128:
		ars = 4;
		break;
	case PDBOPTS_ESP_ARS64:
		ars = 2;
		break;
	case PDBOPTS_ESP_ARS32:
		ars = 1;
		break;
	case PDBOPTS_ESP_ARSNONE:
	default:
		ars = 0;
		break;
	}

	for (i = 0; i < ars; i++)
		__rta_out_be32(program, pdb->anti_replay[i]);

	return start_pc;
}

/**
 * enum ipsec_icv_size - Type selectors for icv size in IPsec protocol
 * @IPSEC_ICV_MD5_SIZE: full-length MD5 ICV
 * @IPSEC_ICV_MD5_TRUNC_SIZE: truncated MD5 ICV
 */
enum ipsec_icv_size {
	IPSEC_ICV_MD5_SIZE = 16,
	IPSEC_ICV_MD5_TRUNC_SIZE = 12
};

/*
 * IPSec ESP Datapath Protocol Override Register (DPOVRD)
 */

#define IPSEC_DECO_DPOVRD_USE		0x80

struct ipsec_deco_dpovrd {
	uint8_t ovrd_ecn;
	uint8_t ip_hdr_len;
	uint8_t nh_offset;
	union {
		uint8_t next_header;	/* next header if encap */
		uint8_t rsvd;		/* reserved if decap */
	};
};

struct ipsec_new_encap_deco_dpovrd {
#define IPSEC_NEW_ENCAP_DECO_DPOVRD_USE	0x8000
	uint16_t ovrd_ip_hdr_len;	/* OVRD + outer IP header material
					 * length
					 */
#define IPSEC_NEW_ENCAP_OIMIF		0x80
	uint8_t oimif_aoipho;		/* OIMIF + actual outer IP header
					 * offset
					 */
	uint8_t rsvd;
};

struct ipsec_new_decap_deco_dpovrd {
	uint8_t ovrd;
	uint8_t aoipho_hi;		/* upper nibble of actual outer IP
					 * header
					 */
	uint16_t aoipho_lo_ip_hdr_len;	/* lower nibble of actual outer IP
					 * header + outer IP header material
					 */
};

static inline void
__gen_auth_key(struct program *program, struct alginfo *authdata)
{
	uint32_t dkp_protid;

	switch (authdata->algtype & OP_PCL_IPSEC_AUTH_MASK) {
	case OP_PCL_IPSEC_HMAC_MD5_96:
	case OP_PCL_IPSEC_HMAC_MD5_128:
		dkp_protid = OP_PCLID_DKP_MD5;
		break;
	case OP_PCL_IPSEC_HMAC_SHA1_96:
	case OP_PCL_IPSEC_HMAC_SHA1_160:
		dkp_protid = OP_PCLID_DKP_SHA1;
		break;
	case OP_PCL_IPSEC_HMAC_SHA2_256_128:
		dkp_protid = OP_PCLID_DKP_SHA256;
		break;
	case OP_PCL_IPSEC_HMAC_SHA2_384_192:
		dkp_protid = OP_PCLID_DKP_SHA384;
		break;
	case OP_PCL_IPSEC_HMAC_SHA2_512_256:
		dkp_protid = OP_PCLID_DKP_SHA512;
		break;
	default:
		KEY(program, KEY2, authdata->key_enc_flags, authdata->key,
		    authdata->keylen, INLINE_KEY(authdata));
		return;
	}

	if (authdata->key_type == RTA_DATA_PTR)
		DKP_PROTOCOL(program, dkp_protid, OP_PCL_DKP_SRC_PTR,
			     OP_PCL_DKP_DST_PTR, (uint16_t)authdata->keylen,
			     authdata->key, authdata->key_type);
	else
		DKP_PROTOCOL(program, dkp_protid, OP_PCL_DKP_SRC_IMM,
			     OP_PCL_DKP_DST_IMM, (uint16_t)authdata->keylen,
			     authdata->key, authdata->key_type);
}

/**
 * cnstr_shdsc_ipsec_encap - IPSec ESP encapsulation protocol-level shared
 *                           descriptor.
 * @descbuf: pointer to buffer used for descriptor construction
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: if true, perform descriptor byte swapping on a 4-byte boundary
 * @pdb: pointer to the PDB to be used with this descriptor
 *       This structure will be copied inline to the descriptor under
 *       construction. No error checking will be made. Refer to the
 *       block guide for a details of the encapsulation PDB.
 * @cipherdata: pointer to block cipher transform definitions
 *              Valid algorithm values - one of OP_PCL_IPSEC_*
 * @authdata: pointer to authentication transform definitions
 *            If an authentication key is required by the protocol:
 *            -For SEC Eras 1-5, an MDHA split key must be provided;
 *            Note that the size of the split key itself must be specified.
 *            -For SEC Eras 6+, a "normal" key must be provided; DKP (Derived
 *            Key Protocol) will be used to compute MDHA on the fly in HW.
 *            Valid algorithm values - one of OP_PCL_IPSEC_*
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_ipsec_encap(uint32_t *descbuf, bool ps, bool swap,
			struct ipsec_encap_pdb *pdb,
			struct alginfo *cipherdata,
			struct alginfo *authdata)
{
	struct program prg;
	struct program *p = &prg;

	LABEL(keyjmp);
	REFERENCE(pkeyjmp);
	LABEL(hdr);
	REFERENCE(phdr);

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	phdr = SHR_HDR(p, SHR_SERIAL, hdr, 0);
	__rta_copy_ipsec_encap_pdb(p, pdb, cipherdata->algtype);
	COPY_DATA(p, pdb->ip_hdr, pdb->ip_hdr_len);
	SET_LABEL(p, hdr);
	pkeyjmp = JUMP(p, keyjmp, LOCAL_JUMP, ALL_TRUE, BOTH|SHRD);
	if (authdata->keylen) {
		if (rta_sec_era < RTA_SEC_ERA_6)
			KEY(p, MDHA_SPLIT_KEY, authdata->key_enc_flags,
			    authdata->key, authdata->keylen,
			    INLINE_KEY(authdata));
		else
			__gen_auth_key(p, authdata);
	}
	if (cipherdata->keylen)
		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));
	SET_LABEL(p, keyjmp);
	PROTOCOL(p, OP_TYPE_ENCAP_PROTOCOL,
		 OP_PCLID_IPSEC,
		 (uint16_t)(cipherdata->algtype | authdata->algtype));
	PATCH_JUMP(p, pkeyjmp, keyjmp);
	PATCH_HDR(p, phdr, hdr);
	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_ipsec_decap - IPSec ESP decapsulation protocol-level shared
 *                           descriptor.
 * @descbuf: pointer to buffer used for descriptor construction
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: if true, perform descriptor byte swapping on a 4-byte boundary
 * @pdb: pointer to the PDB to be used with this descriptor
 *       This structure will be copied inline to the descriptor under
 *       construction. No error checking will be made. Refer to the
 *       block guide for details about the decapsulation PDB.
 * @cipherdata: pointer to block cipher transform definitions.
 *              Valid algorithm values - one of OP_PCL_IPSEC_*
 * @authdata: pointer to authentication transform definitions
 *            If an authentication key is required by the protocol:
 *            -For SEC Eras 1-5, an MDHA split key must be provided;
 *            Note that the size of the split key itself must be specified.
 *            -For SEC Eras 6+, a "normal" key must be provided; DKP (Derived
 *            Key Protocol) will be used to compute MDHA on the fly in HW.
 *            Valid algorithm values - one of OP_PCL_IPSEC_*
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_ipsec_decap(uint32_t *descbuf, bool ps, bool swap,
			struct ipsec_decap_pdb *pdb,
			struct alginfo *cipherdata,
			struct alginfo *authdata)
{
	struct program prg;
	struct program *p = &prg;

	LABEL(keyjmp);
	REFERENCE(pkeyjmp);
	LABEL(hdr);
	REFERENCE(phdr);

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	phdr = SHR_HDR(p, SHR_SERIAL, hdr, 0);
	__rta_copy_ipsec_decap_pdb(p, pdb, cipherdata->algtype);
	SET_LABEL(p, hdr);
	pkeyjmp = JUMP(p, keyjmp, LOCAL_JUMP, ALL_TRUE, BOTH|SHRD);
	if (authdata->keylen) {
		if (rta_sec_era < RTA_SEC_ERA_6)
			KEY(p, MDHA_SPLIT_KEY, authdata->key_enc_flags,
			    authdata->key, authdata->keylen,
			    INLINE_KEY(authdata));
		else
			__gen_auth_key(p, authdata);
	}
	if (cipherdata->keylen)
		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));
	SET_LABEL(p, keyjmp);
	PROTOCOL(p, OP_TYPE_DECAP_PROTOCOL,
		 OP_PCLID_IPSEC,
		 (uint16_t)(cipherdata->algtype | authdata->algtype));
	PATCH_JUMP(p, pkeyjmp, keyjmp);
	PATCH_HDR(p, phdr, hdr);
	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_ipsec_encap_des_aes_xcbc - IPSec DES-CBC/3DES-CBC and
 *     AES-XCBC-MAC-96 ESP encapsulation shared descriptor.
 * @descbuf: pointer to buffer used for descriptor construction
 * @pdb: pointer to the PDB to be used with this descriptor
 *       This structure will be copied inline to the descriptor under
 *       construction. No error checking will be made. Refer to the
 *       block guide for a details of the encapsulation PDB.
 * @cipherdata: pointer to block cipher transform definitions
 *              Valid algorithm values - OP_PCL_IPSEC_DES, OP_PCL_IPSEC_3DES.
 * @authdata: pointer to authentication transform definitions
 *            Valid algorithm value: OP_PCL_IPSEC_AES_XCBC_MAC_96.
 *
 * Supported only for platforms with 32-bit address pointers and SEC ERA 4 or
 * higher. The tunnel/transport mode of the IPsec ESP is supported only if the
 * Outer/Transport IP Header is present in the encapsulation output packet.
 * The descriptor performs DES-CBC/3DES-CBC & HMAC-MD5-96 and then rereads
 * the input packet to do the AES-XCBC-MAC-96 calculation and to overwrite
 * the MD5 ICV.
 * The descriptor uses all the benefits of the built-in protocol by computing
 * the IPsec ESP with a hardware supported algorithms combination
 * (DES-CBC/3DES-CBC & HMAC-MD5-96). The HMAC-MD5 authentication algorithm
 * was chosen in order to speed up the computational time for this intermediate
 * step.
 * Warning: The user must allocate at least 32 bytes for the authentication key
 * (in order to use it also with HMAC-MD5-96),even when using a shorter key
 * for the AES-XCBC-MAC-96.
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_ipsec_encap_des_aes_xcbc(uint32_t *descbuf,
				     struct ipsec_encap_pdb *pdb,
				     struct alginfo *cipherdata,
				     struct alginfo *authdata)
{
	struct program prg;
	struct program *p = &prg;

	LABEL(hdr);
	LABEL(shd_ptr);
	LABEL(keyjump);
	LABEL(outptr);
	LABEL(swapped_seqin_fields);
	LABEL(swapped_seqin_ptr);
	REFERENCE(phdr);
	REFERENCE(pkeyjump);
	REFERENCE(move_outlen);
	REFERENCE(move_seqout_ptr);
	REFERENCE(swapped_seqin_ptr_jump);
	REFERENCE(write_swapped_seqin_ptr);

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	phdr = SHR_HDR(p, SHR_SERIAL, hdr, 0);
	__rta_copy_ipsec_encap_pdb(p, pdb, cipherdata->algtype);
	COPY_DATA(p, pdb->ip_hdr, pdb->ip_hdr_len);
	SET_LABEL(p, hdr);
	pkeyjump = JUMP(p, keyjump, LOCAL_JUMP, ALL_TRUE, SHRD | SELF);
	/*
	 * Hard-coded KEY arguments. The descriptor uses all the benefits of
	 * the built-in protocol by computing the IPsec ESP with a hardware
	 * supported algorithms combination (DES-CBC/3DES-CBC & HMAC-MD5-96).
	 * The HMAC-MD5 authentication algorithm was chosen with
	 * the keys options from below in order to speed up the computational
	 * time for this intermediate step.
	 * Warning: The user must allocate at least 32 bytes for
	 * the authentication key (in order to use it also with HMAC-MD5-96),
	 * even when using a shorter key for the AES-XCBC-MAC-96.
	 */
	KEY(p, MDHA_SPLIT_KEY, 0, authdata->key, 32, INLINE_KEY(authdata));
	SET_LABEL(p, keyjump);
	LOAD(p, LDST_SRCDST_WORD_CLRW | CLRW_CLR_C1MODE | CLRW_CLR_C1DATAS |
	     CLRW_CLR_C1CTX | CLRW_CLR_C1KEY | CLRW_RESET_CLS1_CHA, CLRW, 0, 4,
	     IMMED);
	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));
	PROTOCOL(p, OP_TYPE_ENCAP_PROTOCOL, OP_PCLID_IPSEC,
		 (uint16_t)(cipherdata->algtype | OP_PCL_IPSEC_HMAC_MD5_96));
	/* Swap SEQINPTR to SEQOUTPTR. */
	move_seqout_ptr = MOVE(p, DESCBUF, 0, MATH1, 0, 16, WAITCOMP | IMMED);
	MATHB(p, MATH1, AND, ~(CMD_SEQ_IN_PTR ^ CMD_SEQ_OUT_PTR), MATH1,
	      8, IFB | IMMED2);
/*
 * TODO: RTA currently doesn't support creating a LOAD command
 * with another command as IMM.
 * To be changed when proper support is added in RTA.
 */
	LOAD(p, 0xa00000e5, MATH3, 4, 4, IMMED);
	MATHB(p, MATH3, SHLD, MATH3, MATH3,  8, 0);
	write_swapped_seqin_ptr = MOVE(p, MATH1, 0, DESCBUF, 0, 20, WAITCOMP |
				       IMMED);
	swapped_seqin_ptr_jump = JUMP(p, swapped_seqin_ptr, LOCAL_JUMP,
				      ALL_TRUE, 0);
	LOAD(p, LDST_SRCDST_WORD_CLRW | CLRW_CLR_C1MODE | CLRW_CLR_C1DATAS |
	     CLRW_CLR_C1CTX | CLRW_CLR_C1KEY | CLRW_RESET_CLS1_CHA, CLRW, 0, 4,
	     0);
	SEQOUTPTR(p, 0, 65535, RTO);
	move_outlen = MOVE(p, DESCBUF, 0, MATH0, 4, 8, WAITCOMP | IMMED);
	MATHB(p, MATH0, SUB,
	      (uint64_t)(pdb->ip_hdr_len + IPSEC_ICV_MD5_TRUNC_SIZE),
	      VSEQINSZ, 4, IMMED2);
	MATHB(p, MATH0, SUB, IPSEC_ICV_MD5_TRUNC_SIZE, VSEQOUTSZ, 4, IMMED2);
	KEY(p, KEY1, authdata->key_enc_flags, authdata->key, authdata->keylen,
	    0);
	ALG_OPERATION(p, OP_ALG_ALGSEL_AES, OP_ALG_AAI_XCBC_MAC,
		      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE, DIR_ENC);
	SEQFIFOLOAD(p, SKIP, pdb->ip_hdr_len, 0);
	SEQFIFOLOAD(p, MSG1, 0, VLF | FLUSH1 | LAST1);
	SEQFIFOSTORE(p, SKIP, 0, 0, VLF);
	SEQSTORE(p, CONTEXT1, 0, IPSEC_ICV_MD5_TRUNC_SIZE, 0);
/*
 * TODO: RTA currently doesn't support adding labels in or after Job Descriptor.
 * To be changed when proper support is added in RTA.
 */
	/* Label the Shared Descriptor Pointer */
	SET_LABEL(p, shd_ptr);
	shd_ptr += 1;
	/* Label the Output Pointer */
	SET_LABEL(p, outptr);
	outptr += 3;
	/* Label the first word after JD */
	SET_LABEL(p, swapped_seqin_fields);
	swapped_seqin_fields += 8;
	/* Label the second word after JD */
	SET_LABEL(p, swapped_seqin_ptr);
	swapped_seqin_ptr += 9;

	PATCH_HDR(p, phdr, hdr);
	PATCH_JUMP(p, pkeyjump, keyjump);
	PATCH_JUMP(p, swapped_seqin_ptr_jump, swapped_seqin_ptr);
	PATCH_MOVE(p, move_outlen, outptr);
	PATCH_MOVE(p, move_seqout_ptr, shd_ptr);
	PATCH_MOVE(p, write_swapped_seqin_ptr, swapped_seqin_fields);
	return PROGRAM_FINALIZE(p);
}

/**
 * cnstr_shdsc_ipsec_decap_des_aes_xcbc - IPSec DES-CBC/3DES-CBC and
 *     AES-XCBC-MAC-96 ESP decapsulation shared descriptor.
 * @descbuf: pointer to buffer used for descriptor construction
 * @pdb: pointer to the PDB to be used with this descriptor
 *       This structure will be copied inline to the descriptor under
 *       construction. No error checking will be made. Refer to the
 *       block guide for a details of the encapsulation PDB.
 * @cipherdata: pointer to block cipher transform definitions
 *              Valid algorithm values - OP_PCL_IPSEC_DES, OP_PCL_IPSEC_3DES.
 * @authdata: pointer to authentication transform definitions
 *            Valid algorithm value: OP_PCL_IPSEC_AES_XCBC_MAC_96.
 *
 * Supported only for platforms with 32-bit address pointers and SEC ERA 4 or
 * higher. The tunnel/transport mode of the IPsec ESP is supported only if the
 * Outer/Transport IP Header is present in the decapsulation input packet.
 * The descriptor computes the AES-XCBC-MAC-96 to check if the received ICV
 * is correct, rereads the input packet to compute the MD5 ICV, overwrites
 * the XCBC ICV, and then sends the modified input packet to the
 * DES-CBC/3DES-CBC & HMAC-MD5-96 IPsec.
 * The descriptor uses all the benefits of the built-in protocol by computing
 * the IPsec ESP with a hardware supported algorithms combination
 * (DES-CBC/3DES-CBC & HMAC-MD5-96). The HMAC-MD5 authentication algorithm
 * was chosen in order to speed up the computational time for this intermediate
 * step.
 * Warning: The user must allocate at least 32 bytes for the authentication key
 * (in order to use it also with HMAC-MD5-96),even when using a shorter key
 * for the AES-XCBC-MAC-96.
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_ipsec_decap_des_aes_xcbc(uint32_t *descbuf,
				     struct ipsec_decap_pdb *pdb,
				     struct alginfo *cipherdata,
				     struct alginfo *authdata)
{
	struct program prg;
	struct program *p = &prg;
	uint32_t ip_hdr_len = (pdb->options & PDBHDRLEN_MASK) >>
				PDBHDRLEN_ESP_DECAP_SHIFT;

	LABEL(hdr);
	LABEL(jump_cmd);
	LABEL(keyjump);
	LABEL(outlen);
	LABEL(seqin_ptr);
	LABEL(seqout_ptr);
	LABEL(swapped_seqout_fields);
	LABEL(swapped_seqout_ptr);
	REFERENCE(seqout_ptr_jump);
	REFERENCE(phdr);
	REFERENCE(pkeyjump);
	REFERENCE(move_jump);
	REFERENCE(move_jump_back);
	REFERENCE(move_seqin_ptr);
	REFERENCE(swapped_seqout_ptr_jump);
	REFERENCE(write_swapped_seqout_ptr);

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	phdr = SHR_HDR(p, SHR_SERIAL, hdr, 0);
	__rta_copy_ipsec_decap_pdb(p, pdb, cipherdata->algtype);
	SET_LABEL(p, hdr);
	pkeyjump = JUMP(p, keyjump, LOCAL_JUMP, ALL_TRUE, SHRD | SELF);
	/*
	 * Hard-coded KEY arguments. The descriptor uses all the benefits of
	 * the built-in protocol by computing the IPsec ESP with a hardware
	 * supported algorithms combination (DES-CBC/3DES-CBC & HMAC-MD5-96).
	 * The HMAC-MD5 authentication algorithm was chosen with
	 * the keys options from bellow in order to speed up the computational
	 * time for this intermediate step.
	 * Warning: The user must allocate at least 32 bytes for
	 * the authentication key (in order to use it also with HMAC-MD5-96),
	 * even when using a shorter key for the AES-XCBC-MAC-96.
	 */
	KEY(p, MDHA_SPLIT_KEY, 0, authdata->key, 32, INLINE_KEY(authdata));
	SET_LABEL(p, keyjump);
	LOAD(p, LDST_SRCDST_WORD_CLRW | CLRW_CLR_C1MODE | CLRW_CLR_C1DATAS |
	     CLRW_CLR_C1CTX | CLRW_CLR_C1KEY | CLRW_RESET_CLS1_CHA, CLRW, 0, 4,
	     0);
	KEY(p, KEY1, authdata->key_enc_flags, authdata->key, authdata->keylen,
	    INLINE_KEY(authdata));
	MATHB(p, SEQINSZ, SUB,
	      (uint64_t)(ip_hdr_len + IPSEC_ICV_MD5_TRUNC_SIZE), MATH0, 4,
	      IMMED2);
	MATHB(p, MATH0, SUB, ZERO, VSEQINSZ, 4, 0);
	ALG_OPERATION(p, OP_ALG_ALGSEL_MD5, OP_ALG_AAI_HMAC_PRECOMP,
		      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE, DIR_ENC);
	ALG_OPERATION(p, OP_ALG_ALGSEL_AES, OP_ALG_AAI_XCBC_MAC,
		      OP_ALG_AS_INITFINAL, ICV_CHECK_ENABLE, DIR_DEC);
	SEQFIFOLOAD(p, SKIP, ip_hdr_len, 0);
	SEQFIFOLOAD(p, MSG1, 0, VLF | FLUSH1);
	SEQFIFOLOAD(p, ICV1, IPSEC_ICV_MD5_TRUNC_SIZE, FLUSH1 | LAST1);
	/* Swap SEQOUTPTR to SEQINPTR. */
	move_seqin_ptr = MOVE(p, DESCBUF, 0, MATH1, 0, 16, WAITCOMP | IMMED);
	MATHB(p, MATH1, OR, CMD_SEQ_IN_PTR ^ CMD_SEQ_OUT_PTR, MATH1, 8,
	      IFB | IMMED2);
/*
 * TODO: RTA currently doesn't support creating a LOAD command
 * with another command as IMM.
 * To be changed when proper support is added in RTA.
 */
	LOAD(p, 0xA00000e1, MATH3, 4, 4, IMMED);
	MATHB(p, MATH3, SHLD, MATH3, MATH3,  8, 0);
	write_swapped_seqout_ptr = MOVE(p, MATH1, 0, DESCBUF, 0, 20, WAITCOMP |
					IMMED);
	swapped_seqout_ptr_jump = JUMP(p, swapped_seqout_ptr, LOCAL_JUMP,
				       ALL_TRUE, 0);
/*
 * TODO: To be changed when proper support is added in RTA (can't load
 * a command that is also written by RTA).
 * Change when proper RTA support is added.
 */
	SET_LABEL(p, jump_cmd);
	WORD(p, 0xA00000f3);
	SEQINPTR(p, 0, 65535, RTO);
	MATHB(p, MATH0, SUB, ZERO, VSEQINSZ, 4, 0);
	MATHB(p, MATH0, ADD, ip_hdr_len, VSEQOUTSZ, 4, IMMED2);
	move_jump = MOVE(p, DESCBUF, 0, OFIFO, 0, 8, WAITCOMP | IMMED);
	move_jump_back = MOVE(p, OFIFO, 0, DESCBUF, 0, 8, IMMED);
	SEQFIFOLOAD(p, SKIP, ip_hdr_len, 0);
	SEQFIFOLOAD(p, MSG2, 0, VLF | LAST2);
	SEQFIFOSTORE(p, SKIP, 0, 0, VLF);
	SEQSTORE(p, CONTEXT2, 0, IPSEC_ICV_MD5_TRUNC_SIZE, 0);
	seqout_ptr_jump = JUMP(p, seqout_ptr, LOCAL_JUMP, ALL_TRUE, CALM);

	LOAD(p, LDST_SRCDST_WORD_CLRW | CLRW_CLR_C1MODE | CLRW_CLR_C1DATAS |
	     CLRW_CLR_C1CTX | CLRW_CLR_C1KEY | CLRW_CLR_C2MODE |
	     CLRW_CLR_C2DATAS | CLRW_CLR_C2CTX | CLRW_RESET_CLS1_CHA, CLRW, 0,
	     4, 0);
	SEQINPTR(p, 0, 65535, RTO);
	MATHB(p, MATH0, ADD,
	      (uint64_t)(ip_hdr_len + IPSEC_ICV_MD5_TRUNC_SIZE), SEQINSZ, 4,
	      IMMED2);
	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));
	PROTOCOL(p, OP_TYPE_DECAP_PROTOCOL, OP_PCLID_IPSEC,
		 (uint16_t)(cipherdata->algtype | OP_PCL_IPSEC_HMAC_MD5_96));
/*
 * TODO: RTA currently doesn't support adding labels in or after Job Descriptor.
 * To be changed when proper support is added in RTA.
 */
	/* Label the SEQ OUT PTR */
	SET_LABEL(p, seqout_ptr);
	seqout_ptr += 2;
	/* Label the Output Length */
	SET_LABEL(p, outlen);
	outlen += 4;
	/* Label the SEQ IN PTR */
	SET_LABEL(p, seqin_ptr);
	seqin_ptr += 5;
	/* Label the first word after JD */
	SET_LABEL(p, swapped_seqout_fields);
	swapped_seqout_fields += 8;
	/* Label the second word after JD */
	SET_LABEL(p, swapped_seqout_ptr);
	swapped_seqout_ptr += 9;

	PATCH_HDR(p, phdr, hdr);
	PATCH_JUMP(p, pkeyjump, keyjump);
	PATCH_JUMP(p, seqout_ptr_jump, seqout_ptr);
	PATCH_JUMP(p, swapped_seqout_ptr_jump, swapped_seqout_ptr);
	PATCH_MOVE(p, move_jump, jump_cmd);
	PATCH_MOVE(p, move_jump_back, seqin_ptr);
	PATCH_MOVE(p, move_seqin_ptr, outlen);
	PATCH_MOVE(p, write_swapped_seqout_ptr, swapped_seqout_fields);
	return PROGRAM_FINALIZE(p);
}

/**
 * IPSEC_NEW_ENC_BASE_DESC_LEN - IPsec new mode encap shared descriptor length
 *
 * Accounts only for the "base" commands and is intended to be used by upper
 * layers to determine whether Outer IP Header and/or keys can be inlined or
 * not. To be used as first parameter of rta_inline_query().
 */
#define IPSEC_NEW_ENC_BASE_DESC_LEN	(5 * CAAM_CMD_SZ + \
					 sizeof(struct ipsec_encap_pdb))

/**
 * IPSEC_NEW_NULL_ENC_BASE_DESC_LEN - IPsec new mode encap shared descriptor
 *                                    length for the case of
 *                                    NULL encryption / authentication
 *
 * Accounts only for the "base" commands and is intended to be used by upper
 * layers to determine whether Outer IP Header and/or key can be inlined or
 * not. To be used as first parameter of rta_inline_query().
 */
#define IPSEC_NEW_NULL_ENC_BASE_DESC_LEN	(4 * CAAM_CMD_SZ + \
						 sizeof(struct ipsec_encap_pdb))

/**
 * cnstr_shdsc_ipsec_new_encap -  IPSec new mode ESP encapsulation
 *     protocol-level shared descriptor.
 * @descbuf: pointer to buffer used for descriptor construction
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @pdb: pointer to the PDB to be used with this descriptor
 *       This structure will be copied inline to the descriptor under
 *       construction. No error checking will be made. Refer to the
 *       block guide for details about the encapsulation PDB.
 * @opt_ip_hdr:  pointer to Optional IP Header
 *     -if OIHI = PDBOPTS_ESP_OIHI_PDB_INL, opt_ip_hdr points to the buffer to
 *     be inlined in the PDB. Number of bytes (buffer size) copied is provided
 *     in pdb->ip_hdr_len.
 *     -if OIHI = PDBOPTS_ESP_OIHI_PDB_REF, opt_ip_hdr points to the address of
 *     the Optional IP Header. The address will be inlined in the PDB verbatim.
 *     -for other values of OIHI options field, opt_ip_hdr is not used.
 * @cipherdata: pointer to block cipher transform definitions
 *              Valid algorithm values - one of OP_PCL_IPSEC_*
 * @authdata: pointer to authentication transform definitions.
 *            If an authentication key is required by the protocol, a "normal"
 *            key must be provided; DKP (Derived Key Protocol) will be used to
 *            compute MDHA on the fly in HW.
 *            Valid algorithm values - one of OP_PCL_IPSEC_*
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_ipsec_new_encap(uint32_t *descbuf, bool ps,
			    bool swap,
			    struct ipsec_encap_pdb *pdb,
			    uint8_t *opt_ip_hdr,
			    struct alginfo *cipherdata,
			    struct alginfo *authdata)
{
	struct program prg;
	struct program *p = &prg;

	LABEL(keyjmp);
	REFERENCE(pkeyjmp);
	LABEL(hdr);
	REFERENCE(phdr);

	if (rta_sec_era < RTA_SEC_ERA_8) {
		pr_err("IPsec new mode encap: available only for Era %d or above\n",
		       USER_SEC_ERA(RTA_SEC_ERA_8));
		return -ENOTSUP;
	}

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	phdr = SHR_HDR(p, SHR_SERIAL, hdr, 0);

	__rta_copy_ipsec_encap_pdb(p, pdb, cipherdata->algtype);

	switch (pdb->options & PDBOPTS_ESP_OIHI_MASK) {
	case PDBOPTS_ESP_OIHI_PDB_INL:
		COPY_DATA(p, opt_ip_hdr, pdb->ip_hdr_len);
		break;
	case PDBOPTS_ESP_OIHI_PDB_REF:
		if (ps)
			COPY_DATA(p, opt_ip_hdr, 8);
		else
			COPY_DATA(p, opt_ip_hdr, 4);
		break;
	default:
		break;
	}
	SET_LABEL(p, hdr);

	pkeyjmp = JUMP(p, keyjmp, LOCAL_JUMP, ALL_TRUE, SHRD);
	if (authdata->keylen)
		__gen_auth_key(p, authdata);
	if (cipherdata->keylen)
		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));
	SET_LABEL(p, keyjmp);
	PROTOCOL(p, OP_TYPE_ENCAP_PROTOCOL,
		 OP_PCLID_IPSEC_NEW,
		 (uint16_t)(cipherdata->algtype | authdata->algtype));
	PATCH_JUMP(p, pkeyjmp, keyjmp);
	PATCH_HDR(p, phdr, hdr);
	return PROGRAM_FINALIZE(p);
}

/**
 * IPSEC_NEW_DEC_BASE_DESC_LEN - IPsec new mode decap shared descriptor length
 *
 * Accounts only for the "base" commands and is intended to be used by upper
 * layers to determine whether keys can be inlined or not. To be used as first
 * parameter of rta_inline_query().
 */
#define IPSEC_NEW_DEC_BASE_DESC_LEN	(5 * CAAM_CMD_SZ + \
					 sizeof(struct ipsec_decap_pdb))

/**
 * IPSEC_NEW_NULL_DEC_BASE_DESC_LEN - IPsec new mode decap shared descriptor
 *                                    length for the case of
 *                                    NULL decryption / authentication
 *
 * Accounts only for the "base" commands and is intended to be used by upper
 * layers to determine whether key can be inlined or not. To be used as first
 * parameter of rta_inline_query().
 */
#define IPSEC_NEW_NULL_DEC_BASE_DESC_LEN	(4 * CAAM_CMD_SZ + \
						 sizeof(struct ipsec_decap_pdb))

/**
 * cnstr_shdsc_ipsec_new_decap - IPSec new mode ESP decapsulation protocol-level
 *     shared descriptor.
 * @descbuf: pointer to buffer used for descriptor construction
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: must be true when core endianness doesn't match SEC endianness
 * @pdb: pointer to the PDB to be used with this descriptor
 *       This structure will be copied inline to the descriptor under
 *       construction. No error checking will be made. Refer to the
 *       block guide for details about the decapsulation PDB.
 * @cipherdata: pointer to block cipher transform definitions
 *              Valid algorithm values 0 one of OP_PCL_IPSEC_*
 * @authdata: pointer to authentication transform definitions.
 *            If an authentication key is required by the protocol, a "normal"
 *            key must be provided; DKP (Derived Key Protocol) will be used to
 *            compute MDHA on the fly in HW.
 *            Valid algorithm values - one of OP_PCL_IPSEC_*
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_ipsec_new_decap(uint32_t *descbuf, bool ps,
			    bool swap,
			    struct ipsec_decap_pdb *pdb,
			    struct alginfo *cipherdata,
			    struct alginfo *authdata)
{
	struct program prg;
	struct program *p = &prg;

	LABEL(keyjmp);
	REFERENCE(pkeyjmp);
	LABEL(hdr);
	REFERENCE(phdr);

	if (rta_sec_era < RTA_SEC_ERA_8) {
		pr_err("IPsec new mode decap: available only for Era %d or above\n",
		       USER_SEC_ERA(RTA_SEC_ERA_8));
		return -ENOTSUP;
	}

	PROGRAM_CNTXT_INIT(p, descbuf, 0);
	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);
	phdr = SHR_HDR(p, SHR_SERIAL, hdr, 0);
	__rta_copy_ipsec_decap_pdb(p, pdb, cipherdata->algtype);
	SET_LABEL(p, hdr);
	pkeyjmp = JUMP(p, keyjmp, LOCAL_JUMP, ALL_TRUE, SHRD);
	if (authdata->keylen)
		__gen_auth_key(p, authdata);
	if (cipherdata->keylen)
		KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
		    cipherdata->keylen, INLINE_KEY(cipherdata));
	SET_LABEL(p, keyjmp);
	PROTOCOL(p, OP_TYPE_DECAP_PROTOCOL,
		 OP_PCLID_IPSEC_NEW,
		 (uint16_t)(cipherdata->algtype | authdata->algtype));
	PATCH_JUMP(p, pkeyjmp, keyjmp);
	PATCH_HDR(p, phdr, hdr);
	return PROGRAM_FINALIZE(p);
}

/**
 * IPSEC_AUTH_VAR_BASE_DESC_LEN - IPsec encap/decap shared descriptor length
 *				for the case of variable-length authentication
 *				only data.
 *				Note: Only for SoCs with SEC_ERA >= 3.
 *
 * Accounts only for the "base" commands and is intended to be used by upper
 * layers to determine whether keys can be inlined or not. To be used as first
 * parameter of rta_inline_query().
 */
#define IPSEC_AUTH_VAR_BASE_DESC_LEN	(27 * CAAM_CMD_SZ)

/**
 * IPSEC_AUTH_VAR_AES_DEC_BASE_DESC_LEN - IPsec AES decap shared descriptor
 *                              length for variable-length authentication only
 *                              data.
 *                              Note: Only for SoCs with SEC_ERA >= 3.
 *
 * Accounts only for the "base" commands and is intended to be used by upper
 * layers to determine whether key can be inlined or not. To be used as first
 * parameter of rta_inline_query().
 */
#define IPSEC_AUTH_VAR_AES_DEC_BASE_DESC_LEN	\
				(IPSEC_AUTH_VAR_BASE_DESC_LEN + CAAM_CMD_SZ)

/**
 * IPSEC_AUTH_BASE_DESC_LEN - IPsec encap/decap shared descriptor length
 *
 * Accounts only for the "base" commands and is intended to be used by upper
 * layers to determine whether key can be inlined or not. To be used as first
 * parameter of rta_inline_query().
 */
#define IPSEC_AUTH_BASE_DESC_LEN	(19 * CAAM_CMD_SZ)

/**
 * IPSEC_AUTH_AES_DEC_BASE_DESC_LEN - IPsec AES decap shared descriptor length
 *
 * Accounts only for the "base" commands and is intended to be used by upper
 * layers to determine whether key can be inlined or not. To be used as first
 * parameter of rta_inline_query().
 */
#define IPSEC_AUTH_AES_DEC_BASE_DESC_LEN	(IPSEC_AUTH_BASE_DESC_LEN + \
						CAAM_CMD_SZ)

/**
 * cnstr_shdsc_authenc - authenc-like descriptor
 * @descbuf: pointer to buffer used for descriptor construction
 * @ps: if 36/40bit addressing is desired, this parameter must be true
 * @swap: if true, perform descriptor byte swapping on a 4-byte boundary
 * @cipherdata: pointer to block cipher transform definitions.
 *              Valid algorithm values one of OP_ALG_ALGSEL_* {DES, 3DES, AES}
 *              Valid modes for:
 *                  AES: OP_ALG_AAI_* {CBC, CTR}
 *                  DES, 3DES: OP_ALG_AAI_CBC
 * @authdata: pointer to authentication transform definitions.
 *            Valid algorithm values - one of OP_ALG_ALGSEL_* {MD5, SHA1,
 *            SHA224, SHA256, SHA384, SHA512}
 * Note: The key for authentication is supposed to be given as plain text.
 * Note: There's no support for keys longer than the block size of the
 *       underlying hash function, according to the selected algorithm.
 *
 * @ivlen: length of the IV to be read from the input frame, before any data
 *         to be processed
 * @auth_only_len: length of the data to be authenticated-only (commonly IP
 *                 header, IV, Sequence number and SPI)
 * Note: Extended Sequence Number processing is NOT supported
 *
 * @trunc_len: the length of the ICV to be written to the output frame. If 0,
 *             then the corresponding length of the digest, according to the
 *             selected algorithm shall be used.
 * @dir: Protocol direction, encapsulation or decapsulation (DIR_ENC/DIR_DEC)
 *
 * Note: Here's how the input frame needs to be formatted so that the processing
 *       will be done correctly:
 * For encapsulation:
 *     Input:
 * +----+----------------+---------------------------------------------+
 * | IV | Auth-only data | Padded data to be authenticated & Encrypted |
 * +----+----------------+---------------------------------------------+
 *     Output:
 * +--------------------------------------+
 * | Authenticated & Encrypted data | ICV |
 * +--------------------------------+-----+

 * For decapsulation:
 *     Input:
 * +----+----------------+--------------------------------+-----+
 * | IV | Auth-only data | Authenticated & Encrypted data | ICV |
 * +----+----------------+--------------------------------+-----+
 *     Output:
 * +----+--------------------------+
 * | Decrypted & authenticated data |
 * +----+--------------------------+
 *
 * Note: This descriptor can use per-packet commands, encoded as below in the
 *       DPOVRD register:
 * 32    24    16               0
 * +------+---------------------+
 * | 0x80 | 0x00| auth_only_len |
 * +------+---------------------+
 *
 * This mechanism is available only for SoCs having SEC ERA >= 3. In other
 * words, this will not work for P4080TO2
 *
 * Note: The descriptor does not add any kind of padding to the input data,
 *       so the upper layer needs to ensure that the data is padded properly,
 *       according to the selected cipher. Failure to do so will result in
 *       the descriptor failing with a data-size error.
 *
 * Return: size of descriptor written in words or negative number on error
 */
static inline int
cnstr_shdsc_authenc(uint32_t *descbuf, bool ps, bool swap,
		    struct alginfo *cipherdata,
		    struct alginfo *authdata,
		    uint16_t ivlen, uint16_t auth_only_len,
		    uint8_t trunc_len, uint8_t dir)
{
	struct program prg;
	struct program *p = &prg;
	const bool need_dk = (dir == DIR_DEC) &&
			     (cipherdata->algtype == OP_ALG_ALGSEL_AES) &&
			     (cipherdata->algmode == OP_ALG_AAI_CBC);

	LABEL(skip_patch_len);
	LABEL(keyjmp);
	LABEL(skipkeys);
	LABEL(aonly_len_offset);
	REFERENCE(pskip_patch_len);
	REFERENCE(pkeyjmp);
	REFERENCE(pskipkeys);
	REFERENCE(read_len);
	REFERENCE(write_len);

	PROGRAM_CNTXT_INIT(p, descbuf, 0);

	if (swap)
		PROGRAM_SET_BSWAP(p);
	if (ps)
		PROGRAM_SET_36BIT_ADDR(p);

	/*
	 * Since we currently assume that key length is equal to hash digest
	 * size, it's ok to truncate keylen value.
	 */
	trunc_len = trunc_len && (trunc_len < authdata->keylen) ?
			trunc_len : (uint8_t)authdata->keylen;

	SHR_HDR(p, SHR_SERIAL, 1, SC);

	/*
	 * M0 will contain the value provided by the user when creating
	 * the shared descriptor. If the user provided an override in
	 * DPOVRD, then M0 will contain that value
	 */
	MATHB(p, MATH0, ADD, auth_only_len, MATH0, 4, IMMED2);

	if (rta_sec_era >= RTA_SEC_ERA_3) {
		/*
		 * Check if the user wants to override the auth-only len
		 */
		MATHB(p, DPOVRD, ADD, 0x80000000, MATH2, 4, IMMED2);

		/*
		 * No need to patch the length of the auth-only data read if
		 * the user did not override it
		 */
		pskip_patch_len = JUMP(p, skip_patch_len, LOCAL_JUMP, ALL_TRUE,
				  MATH_N);

		/* Get auth-only len in M0 */
		MATHB(p, MATH2, AND, 0xFFFF, MATH0, 4, IMMED2);

		/*
		 * Since M0 is used in calculations, don't mangle it, copy
		 * its content to M1 and use this for patching.
		 */
		MATHB(p, MATH0, ADD, MATH1, MATH1, 4, 0);

		read_len = MOVE(p, DESCBUF, 0, MATH1, 0, 6, WAITCOMP | IMMED);
		write_len = MOVE(p, MATH1, 0, DESCBUF, 0, 8, WAITCOMP | IMMED);

		SET_LABEL(p, skip_patch_len);
	}
	/*
	 * MATH0 contains the value in DPOVRD w/o the MSB, or the initial
	 * value, as provided by the user at descriptor creation time
	 */
	if (dir == DIR_ENC)
		MATHB(p, MATH0, ADD, ivlen, MATH0, 4, IMMED2);
	else
		MATHB(p, MATH0, ADD, ivlen + trunc_len, MATH0, 4, IMMED2);

	pkeyjmp = JUMP(p, keyjmp, LOCAL_JUMP, ALL_TRUE, SHRD);

	KEY(p, KEY2, authdata->key_enc_flags, authdata->key, authdata->keylen,
	    INLINE_KEY(authdata));

	/* Insert Key */
	KEY(p, KEY1, cipherdata->key_enc_flags, cipherdata->key,
	    cipherdata->keylen, INLINE_KEY(cipherdata));

	/* Do operation */
	ALG_OPERATION(p, authdata->algtype, OP_ALG_AAI_HMAC,
		      OP_ALG_AS_INITFINAL,
		      dir == DIR_ENC ? ICV_CHECK_DISABLE : ICV_CHECK_ENABLE,
		      dir);

	if (need_dk)
		ALG_OPERATION(p, OP_ALG_ALGSEL_AES, cipherdata->algmode,
			      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE, dir);
	pskipkeys = JUMP(p, skipkeys, LOCAL_JUMP, ALL_TRUE, 0);

	SET_LABEL(p, keyjmp);

	ALG_OPERATION(p, authdata->algtype, OP_ALG_AAI_HMAC_PRECOMP,
		      OP_ALG_AS_INITFINAL,
		      dir == DIR_ENC ? ICV_CHECK_DISABLE : ICV_CHECK_ENABLE,
		      dir);

	if (need_dk) {
		ALG_OPERATION(p, OP_ALG_ALGSEL_AES, cipherdata->algmode |
			      OP_ALG_AAI_DK, OP_ALG_AS_INITFINAL,
			      ICV_CHECK_DISABLE, dir);
		SET_LABEL(p, skipkeys);
	} else {
		SET_LABEL(p, skipkeys);
		ALG_OPERATION(p, cipherdata->algtype, cipherdata->algmode,
			      OP_ALG_AS_INITFINAL, ICV_CHECK_DISABLE, dir);
	}

	/*
	 * Prepare the length of the data to be both encrypted/decrypted
	 * and authenticated/checked
	 */
	MATHB(p, SEQINSZ, SUB, MATH0, VSEQINSZ, 4, 0);

	MATHB(p, VSEQINSZ, SUB, MATH3, VSEQOUTSZ, 4, 0);

	/* Prepare for writing the output frame */
	SEQFIFOSTORE(p, MSG, 0, 0, VLF);

	SET_LABEL(p, aonly_len_offset);

	/* Read IV */
	if (cipherdata->algmode == OP_ALG_AAI_CTR)
		SEQLOAD(p, CONTEXT1, 16, ivlen, 0);
	else
		SEQLOAD(p, CONTEXT1, 0, ivlen, 0);

	/*
	 * Read data needed only for authentication. This is overwritten above
	 * if the user requested it.
	 */
	SEQFIFOLOAD(p, MSG2, auth_only_len, 0);

	if (dir == DIR_ENC) {
		/*
		 * Read input plaintext, encrypt and authenticate & write to
		 * output
		 */
		SEQFIFOLOAD(p, MSGOUTSNOOP, 0, VLF | LAST1 | LAST2 | FLUSH1);

		/* Finally, write the ICV */
		SEQSTORE(p, CONTEXT2, 0, trunc_len, 0);
	} else {
		/*
		 * Read input ciphertext, decrypt and authenticate & write to
		 * output
		 */
		SEQFIFOLOAD(p, MSGINSNOOP, 0, VLF | LAST1 | LAST2 | FLUSH1);

		/* Read the ICV to check */
		SEQFIFOLOAD(p, ICV2, trunc_len, LAST2);
	}

	PATCH_JUMP(p, pkeyjmp, keyjmp);
	PATCH_JUMP(p, pskipkeys, skipkeys);
	PATCH_JUMP(p, pskipkeys, skipkeys);

	if (rta_sec_era >= RTA_SEC_ERA_3) {
		PATCH_JUMP(p, pskip_patch_len, skip_patch_len);
		PATCH_MOVE(p, read_len, aonly_len_offset);
		PATCH_MOVE(p, write_len, aonly_len_offset);
	}

	return PROGRAM_FINALIZE(p);
}

#endif /* __DESC_IPSEC_H__ */
