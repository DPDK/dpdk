/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 Marvell.
 */

#ifndef _ROC_HASH_H_
#define _ROC_HASH_H_

/*
 * Compute a partial hash with the assumption that msg is the first block.
 * Based on implementation from RFC 3174
 */
void __roc_api roc_hash_md5_gen(uint8_t *msg, uint32_t *hash);
void __roc_api roc_hash_sha1_gen(uint8_t *msg, uint32_t *hash);
void __roc_api roc_hash_sha256_gen(uint8_t *msg, uint32_t *hash, int hash_size);
void __roc_api roc_hash_sha512_gen(uint8_t *msg, uint64_t *hash, int hash_size);

#endif /* _ROC_HASH_H_ */
