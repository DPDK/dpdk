/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef _CNXK_SECURITY_H__
#define _CNXK_SECURITY_H__

#include <rte_crypto.h>
#include <rte_security.h>

#include "roc_ie_on.h"
#include "roc_ie_ot.h"

/* Response length calculation data */
struct cnxk_ipsec_outb_rlens {
	uint16_t partial_len;
	uint8_t roundup_byte;
	int8_t roundup_len;
	uint16_t max_extended_len;
};

int __roc_api
cnxk_ipsec_outb_rlens_get(struct cnxk_ipsec_outb_rlens *rlens,
			  struct rte_security_ipsec_xform *ipsec_xfrm,
			  struct rte_crypto_sym_xform *crypto_xfrm);
uint8_t __roc_api
cnxk_ipsec_ivlen_get(enum rte_crypto_cipher_algorithm c_algo,
		     enum rte_crypto_auth_algorithm a_algo,
		     enum rte_crypto_aead_algorithm aead_algo);
uint8_t __roc_api
cnxk_ipsec_icvlen_get(enum rte_crypto_cipher_algorithm c_algo,
		      enum rte_crypto_auth_algorithm a_algo,
		      enum rte_crypto_aead_algorithm aead_algo);

uint8_t __roc_api
cnxk_ipsec_outb_roundup_byte(enum rte_crypto_cipher_algorithm c_algo,
			     enum rte_crypto_aead_algorithm aead_algo);

/* [CN10K, .) */
int __roc_api
cnxk_ot_ipsec_inb_sa_fill(struct roc_ot_ipsec_inb_sa *sa,
			  struct rte_security_ipsec_xform *ipsec_xfrm,
			  struct rte_crypto_sym_xform *crypto_xfrm,
			  bool is_inline);
int __roc_api
cnxk_ot_ipsec_outb_sa_fill(struct roc_ot_ipsec_outb_sa *sa,
			   struct rte_security_ipsec_xform *ipsec_xfrm,
			   struct rte_crypto_sym_xform *crypto_xfrm);
bool __roc_api cnxk_ot_ipsec_inb_sa_valid(struct roc_ot_ipsec_inb_sa *sa);
bool __roc_api cnxk_ot_ipsec_outb_sa_valid(struct roc_ot_ipsec_outb_sa *sa);

/* [CN9K] */
int __roc_api
cnxk_on_ipsec_inb_sa_create(struct rte_security_ipsec_xform *ipsec,
			    struct rte_crypto_sym_xform *crypto_xform,
			    struct roc_ie_on_inb_sa *in_sa);

int __roc_api
cnxk_on_ipsec_outb_sa_create(struct rte_security_ipsec_xform *ipsec,
			     struct rte_crypto_sym_xform *crypto_xform,
			     struct roc_ie_on_outb_sa *out_sa);

#endif /* _CNXK_SECURITY_H__ */
