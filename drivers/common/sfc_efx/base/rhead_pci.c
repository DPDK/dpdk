/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 */

#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_RIVERHEAD && EFSYS_OPT_PCI

	__checkReturn			efx_rc_t
rhead_pci_nic_membar_lookup(
	__in				efsys_pci_config_t *espcp,
	__out				efx_bar_region_t *ebrp)
{
	boolean_t xilinx_tbl_found = B_FALSE;
	unsigned int xilinx_tbl_bar;
	efsys_dma_addr_t xilinx_tbl_offset;
	size_t pci_capa_offset = 0;
	boolean_t bar_found = B_FALSE;
	efx_rc_t rc = ENOENT;
	efsys_bar_t xil_eb;

	/*
	 * SF-119689-TC Riverhead Host Interface section 4.2.2. describes
	 * the following discovery steps.
	 */
	while (1) {
		rc = efx_pci_find_next_xilinx_cap_table(espcp, &pci_capa_offset,
							&xilinx_tbl_bar,
							&xilinx_tbl_offset);
		if (rc != 0) {
			/*
			 * SF-119689-TC Riverhead Host Interface section 4.2.2.
			 * defines the following fallbacks for the memory bar
			 * and the offset when no Xilinx capabilities table is
			 * found.
			 */
			if (rc == ENOENT && xilinx_tbl_found == B_FALSE) {
				ebrp->ebr_type = EFX_BAR_TYPE_MEM;
				ebrp->ebr_index = EFX_MEM_BAR_RIVERHEAD;
				ebrp->ebr_offset = 0;
				ebrp->ebr_length = 0;
				bar_found = B_TRUE;
				break;
			} else {
				goto fail1;
			}

		}

		xilinx_tbl_found = B_TRUE;

		EFSYS_PCI_FIND_MEM_BAR(espcp, xilinx_tbl_bar, &xil_eb, &rc);
		if (rc != 0)
			goto fail2;
	}

	if (bar_found == B_FALSE)
		goto fail3;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif /* EFSYS_OPT_RIVERHEAD && EFSYS_OPT_PCI */
