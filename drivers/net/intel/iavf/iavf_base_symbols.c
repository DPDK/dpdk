/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2025 Red Hat, Inc.
 */

#include <eal_export.h>

/* Symbols from the base driver are exported separately below. */
RTE_EXPORT_INTERNAL_SYMBOL(iavf_init_adminq)
RTE_EXPORT_INTERNAL_SYMBOL(iavf_shutdown_adminq)
RTE_EXPORT_INTERNAL_SYMBOL(iavf_clean_arq_element)
RTE_EXPORT_INTERNAL_SYMBOL(iavf_set_mac_type)
RTE_EXPORT_INTERNAL_SYMBOL(iavf_aq_send_msg_to_pf)
RTE_EXPORT_INTERNAL_SYMBOL(iavf_vf_parse_hw_config)
RTE_EXPORT_INTERNAL_SYMBOL(iavf_vf_reset)
