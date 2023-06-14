/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 Marvell.
 */

#ifndef _ROC_AES_H_
#define _ROC_AES_H_

/*
 * Derive k1, k2, k3 from 128 bit AES key
 */
void __roc_api roc_aes_xcbc_key_derive(const uint8_t *auth_key, uint8_t *derived_key);
void __roc_api roc_aes_hash_key_derive(const uint8_t *key, uint16_t len, uint8_t *hash_key);

#endif /* _ROC_AES_H_ */
