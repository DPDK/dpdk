/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _NT4GA_ADAPTER_H_
#define _NT4GA_ADAPTER_H_

#include "ntnic_stat.h"
#include "nt4ga_link.h"

typedef struct hw_info_s {
	/* pciids */
	uint16_t pci_vendor_id;
	uint16_t pci_device_id;
	uint16_t pci_sub_vendor_id;
	uint16_t pci_sub_device_id;
	uint16_t pci_class_id;

	/* Derived from pciid */
	nthw_adapter_id_t n_nthw_adapter_id;
	int hw_platform_id;
	int hw_product_type;
	int hw_reserved1;
} hw_info_t;

/*
 * Services provided by the adapter module
 */
#include "nt4ga_filter.h"

typedef struct adapter_info_s {
	struct nt4ga_stat_s nt4ga_stat;
	struct nt4ga_filter_s nt4ga_filter;
	struct nt4ga_link_s nt4ga_link;

	struct hw_info_s hw_info;
	struct fpga_info_s fpga_info;

	char *mp_port_id_str[NUM_ADAPTER_PORTS_MAX];
	char *mp_adapter_id_str;
	char *p_dev_name;

	int adapter_no;
	int n_rx_host_buffers;
	int n_tx_host_buffers;
} adapter_info_t;


#endif	/* _NT4GA_ADAPTER_H_ */
