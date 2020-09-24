/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 */

#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_RIVERHEAD && EFSYS_OPT_PCI

/*
 * Search for a EF100 resource locator from the given offset of an entry
 * in a Xilinx capabilities table.
 */
static	__checkReturn			efx_rc_t
rhead_xilinx_cap_tbl_find_ef100_locator(
	__in				efsys_bar_t *esbp,
	__in				efsys_dma_addr_t tbl_offset,
	__out				efx_bar_region_t *ef100_ebrp)
{
	efx_rc_t rc;
	efsys_dma_addr_t entry_offset = tbl_offset;

	rc = efx_pci_xilinx_cap_tbl_find(esbp, ESE_GZ_CFGBAR_ENTRY_EF100,
					   B_FALSE, &entry_offset);
	if (rc != 0) {
		/* EF100 locator not found (ENOENT) or other error */
		goto fail1;
	}

	rc = rhead_nic_xilinx_cap_tbl_read_ef100_locator(esbp, entry_offset,
							 ef100_ebrp);
	if (rc != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn			efx_rc_t
rhead_pci_nic_membar_lookup(
	__in				efsys_pci_config_t *espcp,
	__in				const efx_pci_ops_t *epop,
	__out				efx_bar_region_t *ebrp)
{
	boolean_t xilinx_tbl_found = B_FALSE;
	unsigned int xilinx_tbl_bar;
	efsys_dma_addr_t xilinx_tbl_offset;
	size_t pci_capa_offset = 0;
	boolean_t bar_found = B_FALSE;
	efx_rc_t rc = ENOENT;
	efsys_bar_t xil_eb;
	efsys_bar_t nic_eb;
	efx_dword_t magic_ed;
	uint32_t magic;

	/*
	 * SF-119689-TC Riverhead Host Interface section 4.2.2. describes
	 * the following discovery steps.
	 */
	while (1) {
		rc = efx_pci_find_next_xilinx_cap_table(espcp, epop,
							&pci_capa_offset,
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

		rc = epop->epo_find_mem_bar(espcp, xilinx_tbl_bar, &xil_eb);
		if (rc != 0)
			goto fail2;

		rc = rhead_xilinx_cap_tbl_find_ef100_locator(&xil_eb,
							     xilinx_tbl_offset,
							     ebrp);
		if (rc == 0) {
			/* Found valid EF100 locator. */
			bar_found = B_TRUE;
			break;
		} else if (rc != ENOENT) {
			/* Table access failed, so terminate search. */
			goto fail3;
		}
	}

	if (bar_found == B_FALSE)
		goto fail4;

	rc = epop->epo_find_mem_bar(espcp, ebrp->ebr_index, &nic_eb);
	if (rc != 0)
		goto fail5;

	EFSYS_BAR_READD(&nic_eb, ebrp->ebr_offset + ER_GZ_NIC_MAGIC_OFST,
			&magic_ed, B_FALSE);

	magic = EFX_DWORD_FIELD(magic_ed, ERF_GZ_NIC_MAGIC);
	if (magic != EFE_GZ_NIC_MAGIC_EXPECTED) {
		rc = EINVAL;
		goto fail6;
	}

	return (0);

fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif /* EFSYS_OPT_RIVERHEAD && EFSYS_OPT_PCI */
