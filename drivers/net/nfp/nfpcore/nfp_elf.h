/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_ELF_H__
#define __NFP_ELF_H__

#include <stdint.h>

int nfp_elf_get_fw_version(uint32_t *fw_version, char *fw_name);

#endif /* __NFP_ELF_H__ */
