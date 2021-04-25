/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 */

#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_PCI

	__checkReturn			efx_rc_t
efx_pci_config_next_ext_cap(
	__in				efsys_pci_config_t *espcp,
	__in				const efx_pci_ops_t *epop,
	__inout				size_t *offsetp)
{
	efx_dword_t hdr;
	efx_rc_t rc = 0;
	size_t next;

	if (offsetp == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	if (*offsetp == 0) {
		*offsetp = ESE_GZ_PCI_BASE_CONFIG_SPACE_SIZE;
	} else {
		rc = epop->epo_config_readd(espcp, *offsetp +
				(EFX_LOW_BIT(ESF_GZ_PCI_EXPRESS_XCAP_ID) / 8),
				&hdr);
		if (rc != 0) {
			rc = EIO;
			goto fail2;
		}

		next = EFX_DWORD_FIELD(hdr, ESF_GZ_PCI_EXPRESS_XCAP_NEXT);
		if (next < ESE_GZ_PCI_BASE_CONFIG_SPACE_SIZE)
			rc = ENOENT;
		else
			*offsetp = next;
	}

	/*
	 * Returns 0 if the next capability is present otherwise ENOENT
	 * indicating that the function finished correctly.
	 */
	return (rc);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn			efx_rc_t
efx_pci_config_find_next_ext_cap(
	__in				efsys_pci_config_t *espcp,
	__in				const efx_pci_ops_t *epop,
	__in				uint16_t cap_id,
	__inout				size_t *offsetp)
{
	efx_dword_t hdr;
	size_t position;
	efx_rc_t rc;

	if (offsetp == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	position = *offsetp;

	while (1) {
		rc = efx_pci_config_next_ext_cap(espcp, epop, &position);
		if (rc != 0) {
			if (rc == ENOENT)
				break;
			else
				goto fail2;
		}

		rc = epop->epo_config_readd(espcp, position +
				(EFX_LOW_BIT(ESF_GZ_PCI_EXPRESS_XCAP_ID) / 8),
				&hdr);
		if (rc != 0) {
			rc = EIO;
			goto fail3;
		}

		if (EFX_DWORD_FIELD(hdr, ESF_GZ_PCI_EXPRESS_XCAP_ID) ==
		    cap_id) {
			*offsetp = position;
			rc = 0;
			break;
		}
	}

	/*
	 * Returns 0 if found otherwise ENOENT indicating that search finished
	 * correctly.
	 */
	return (rc);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn			efx_rc_t
efx_pci_find_next_xilinx_cap_table(
	__in				efsys_pci_config_t *espcp,
	__in				const efx_pci_ops_t *epop,
	__inout				size_t *pci_cap_offsetp,
	__out				unsigned int *xilinx_tbl_barp,
	__out				efsys_dma_addr_t *xilinx_tbl_offsetp)
{
	size_t cap_offset;
	efx_rc_t rc;

	if (pci_cap_offsetp == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	cap_offset = *pci_cap_offsetp;

	while (1) {
		unsigned int tbl_bar;
		efsys_dma_addr_t tbl_offset;

		rc = efx_pci_config_find_next_ext_cap(espcp, epop,
				ESE_GZ_PCI_EXPRESS_XCAP_ID_VNDR, &cap_offset);
		if (rc != 0) {
			if (rc == ENOENT)
				break;
			else
				goto fail2;
		}

		/*
		 * The found extended PCI capability is a vendor-specific
		 * capability, but not necessarily a Xilinx capabilities table
		 * locator. Try to read it and skip it if the capability is
		 * not the locator.
		 */
		rc = efx_pci_read_ext_cap_xilinx_table(espcp, epop, cap_offset,
						       &tbl_bar, &tbl_offset);
		if (rc == 0) {
			*xilinx_tbl_barp = tbl_bar;
			*xilinx_tbl_offsetp = tbl_offset;
			*pci_cap_offsetp = cap_offset;
			break;
		} else {
			if (rc == ENOENT)
				continue;
			else
				goto fail3;
		}
	}

	/*
	 * Returns 0 if found otherwise ENOENT indicating that search finished
	 * correctly.
	 */
	return (rc);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn			efx_rc_t
efx_pci_read_ext_cap_xilinx_table(
	__in				efsys_pci_config_t *espcp,
	__in				const efx_pci_ops_t *epop,
	__in				size_t cap_offset,
	__out				unsigned int *barp,
	__out				efsys_dma_addr_t *offsetp)
{
	size_t vsec_offset = cap_offset + ESE_GZ_PCI_EXPRESS_XCAP_HDR_SIZE;
	efx_dword_t cap_hdr;
	efx_oword_t vsec;
	uint32_t vsec_len;
	uint32_t vsec_id;
	uint32_t vsec_rev;
	uint32_t offset_low;
	uint32_t offset_high = 0;
	unsigned int bar;
	efsys_dma_addr_t offset;
	efx_rc_t rc;

	rc = epop->epo_config_readd(espcp, cap_offset +
			       (EFX_LOW_BIT(ESF_GZ_PCI_EXPRESS_XCAP_ID) / 8),
			       &cap_hdr);
	if (rc != 0) {
		rc = EIO;
		goto fail1;
	}

	if (EFX_DWORD_FIELD(cap_hdr, ESF_GZ_PCI_EXPRESS_XCAP_VER) !=
	    ESE_GZ_PCI_EXPRESS_XCAP_VER_VSEC) {
		rc = EINVAL;
		goto fail2;
	}

	rc = epop->epo_config_readd(espcp, vsec_offset +
			       (EFX_LOW_BIT(ESF_GZ_VSEC_ID) / 8),
			       &vsec.eo_dword[0]);
	if (rc != 0) {
		rc = EIO;
		goto fail3;
	}

	vsec_len = EFX_OWORD_FIELD32(vsec, ESF_GZ_VSEC_LEN);
	vsec_id = EFX_OWORD_FIELD32(vsec, ESF_GZ_VSEC_ID);
	vsec_rev = EFX_OWORD_FIELD32(vsec, ESF_GZ_VSEC_VER);

	/*
	 * Condition of the vendor-specific extended PCI capability not being
	 * a Xilinx capabilities table locator.
	 */
	if (vsec_id != ESE_GZ_XILINX_VSEC_ID) {
		rc = ENOENT;
		goto fail4;
	}

	if (vsec_rev != ESE_GZ_VSEC_VER_XIL_CFGBAR ||
	    vsec_len < ESE_GZ_VSEC_LEN_MIN) {
		rc = EINVAL;
		goto fail5;
	}

	rc = epop->epo_config_readd(espcp, vsec_offset +
			       (EFX_LOW_BIT(ESF_GZ_VSEC_TBL_BAR) / 8),
			       &vsec.eo_dword[1]);
	if (rc != 0) {
		rc = EIO;
		goto fail6;
	}

	bar = EFX_OWORD_FIELD32(vsec, ESF_GZ_VSEC_TBL_BAR);
	offset_low = EFX_OWORD_FIELD32(vsec, ESF_GZ_VSEC_TBL_OFF_LO);

	if (vsec_len >= ESE_GZ_VSEC_LEN_HIGH_OFFT) {
		rc = epop->epo_config_readd(espcp, vsec_offset +
				(EFX_LOW_BIT(ESF_GZ_VSEC_TBL_OFF_HI) / 8),
				&vsec.eo_dword[2]);
		if (rc != 0) {
			rc = EIO;
			goto fail7;
		}

		offset_high = EFX_OWORD_FIELD32(vsec, ESF_GZ_VSEC_TBL_OFF_HI);
	}

	/* High bits of low offset are discarded by the shift */
	offset = offset_low << ESE_GZ_VSEC_TBL_OFF_LO_BYTES_SHIFT;

	/*
	 * Avoid the 'left shift count >= width of type' warning on systems
	 * without uint64_t support.
	 */
#if EFSYS_HAS_UINT64
	offset |= (uint64_t)offset_high << ESE_GZ_VSEC_TBL_OFF_HI_BYTES_SHIFT;
#else
	_NOTE(ARGUNUSED(offset_high))
#endif

	*offsetp = offset;
	*barp = bar;

	return (0);

fail7:
	EFSYS_PROBE(fail7);
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

	__checkReturn			efx_rc_t
efx_pci_xilinx_cap_tbl_find(
	__in				efsys_bar_t *esbp,
	__in				uint32_t format_id,
	__in				boolean_t skip_first,
	__inout				efsys_dma_addr_t *entry_offsetp)
{
	efsys_dma_addr_t offset;
	boolean_t skip = skip_first;
	efx_qword_t header;
	uint32_t format;
	uint32_t last;
	efx_rc_t rc;

	if (entry_offsetp == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	offset = *entry_offsetp;
	rc = ENOENT;
	/*
	 * SF-119689-TC Riverhead Host Interface section 4.2.2.
	 * describes the following discovery steps.
	 */
	do {
		/*
		 * Xilinx Capabilities Table requires 32bit aligned reads.
		 * See SF-119689-TC section 4.2.2 "Discovery Steps".
		 */
		EFSYS_BAR_READD(esbp, offset +
				(EFX_LOW_BIT(ESF_GZ_CFGBAR_ENTRY_FORMAT) / 8),
				&header.eq_dword[0], B_FALSE);
		EFSYS_BAR_READD(esbp, offset +
				(EFX_LOW_BIT(ESF_GZ_CFGBAR_ENTRY_SIZE) / 8),
				&header.eq_dword[1], B_FALSE);

		format = EFX_QWORD_FIELD32(header, ESF_GZ_CFGBAR_ENTRY_FORMAT);
		last = EFX_QWORD_FIELD32(header, ESF_GZ_CFGBAR_ENTRY_LAST);

		if (skip == B_FALSE && format == format_id) {
			*entry_offsetp = offset;
			rc = 0;
			break;
		}

		offset += EFX_QWORD_FIELD32(header, ESF_GZ_CFGBAR_ENTRY_SIZE);
		skip = B_FALSE;
	} while (last == B_FALSE);

	/*
	 * Returns 0 if found otherwise ENOENT indicating that
	 * search finished correctly.
	 */
	return (rc);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif /* EFSYS_OPT_PCI */
