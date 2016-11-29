/*
 * Copyright (c) 2007-2016 Solarflare Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 */

#include "efx.h"
#include "efx_impl.h"

	__checkReturn	efx_rc_t
efx_family(
	__in		uint16_t venid,
	__in		uint16_t devid,
	__out		efx_family_t *efp)
{
	if (venid == EFX_PCI_VENID_SFC) {
		switch (devid) {
#if EFSYS_OPT_SIENA
		case EFX_PCI_DEVID_SIENA_F1_UNINIT:
			/*
			 * Hardware default for PF0 of uninitialised Siena.
			 * manftest must be able to cope with this device id.
			 */
			*efp = EFX_FAMILY_SIENA;
			return (0);

		case EFX_PCI_DEVID_BETHPAGE:
		case EFX_PCI_DEVID_SIENA:
			*efp = EFX_FAMILY_SIENA;
			return (0);
#endif /* EFSYS_OPT_SIENA */

		case EFX_PCI_DEVID_FALCON:	/* Obsolete, not supported */
		default:
			break;
		}
	}

	*efp = EFX_FAMILY_INVALID;
	return (ENOTSUP);
}


#define	EFX_BIU_MAGIC0	0x01234567
#define	EFX_BIU_MAGIC1	0xfedcba98

	__checkReturn	efx_rc_t
efx_nic_biu_test(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;
	efx_rc_t rc;

	/*
	 * Write magic values to scratch registers 0 and 1, then
	 * verify that the values were written correctly.  Interleave
	 * the accesses to ensure that the BIU is not just reading
	 * back the cached value that was last written.
	 */
	EFX_POPULATE_OWORD_1(oword, FRF_AZ_DRIVER_DW0, EFX_BIU_MAGIC0);
	EFX_BAR_TBL_WRITEO(enp, FR_AZ_DRIVER_REG, 0, &oword, B_TRUE);

	EFX_POPULATE_OWORD_1(oword, FRF_AZ_DRIVER_DW0, EFX_BIU_MAGIC1);
	EFX_BAR_TBL_WRITEO(enp, FR_AZ_DRIVER_REG, 1, &oword, B_TRUE);

	EFX_BAR_TBL_READO(enp, FR_AZ_DRIVER_REG, 0, &oword, B_TRUE);
	if (EFX_OWORD_FIELD(oword, FRF_AZ_DRIVER_DW0) != EFX_BIU_MAGIC0) {
		rc = EIO;
		goto fail1;
	}

	EFX_BAR_TBL_READO(enp, FR_AZ_DRIVER_REG, 1, &oword, B_TRUE);
	if (EFX_OWORD_FIELD(oword, FRF_AZ_DRIVER_DW0) != EFX_BIU_MAGIC1) {
		rc = EIO;
		goto fail2;
	}

	/*
	 * Perform the same test, with the values swapped.  This
	 * ensures that subsequent tests don't start with the correct
	 * values already written into the scratch registers.
	 */
	EFX_POPULATE_OWORD_1(oword, FRF_AZ_DRIVER_DW0, EFX_BIU_MAGIC1);
	EFX_BAR_TBL_WRITEO(enp, FR_AZ_DRIVER_REG, 0, &oword, B_TRUE);

	EFX_POPULATE_OWORD_1(oword, FRF_AZ_DRIVER_DW0, EFX_BIU_MAGIC0);
	EFX_BAR_TBL_WRITEO(enp, FR_AZ_DRIVER_REG, 1, &oword, B_TRUE);

	EFX_BAR_TBL_READO(enp, FR_AZ_DRIVER_REG, 0, &oword, B_TRUE);
	if (EFX_OWORD_FIELD(oword, FRF_AZ_DRIVER_DW0) != EFX_BIU_MAGIC1) {
		rc = EIO;
		goto fail3;
	}

	EFX_BAR_TBL_READO(enp, FR_AZ_DRIVER_REG, 1, &oword, B_TRUE);
	if (EFX_OWORD_FIELD(oword, FRF_AZ_DRIVER_DW0) != EFX_BIU_MAGIC0) {
		rc = EIO;
		goto fail4;
	}

	return (0);

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

#if EFSYS_OPT_SIENA

static const efx_nic_ops_t	__efx_nic_siena_ops = {
	siena_nic_probe,		/* eno_probe */
	NULL,				/* eno_board_cfg */
	NULL,				/* eno_set_drv_limits */
	siena_nic_reset,		/* eno_reset */
	siena_nic_init,			/* eno_init */
	NULL,				/* eno_get_vi_pool */
	NULL,				/* eno_get_bar_region */
	siena_nic_fini,			/* eno_fini */
	siena_nic_unprobe,		/* eno_unprobe */
};

#endif	/* EFSYS_OPT_SIENA */


	__checkReturn	efx_rc_t
efx_nic_create(
	__in		efx_family_t family,
	__in		efsys_identifier_t *esip,
	__in		efsys_bar_t *esbp,
	__in		efsys_lock_t *eslp,
	__deref_out	efx_nic_t **enpp)
{
	efx_nic_t *enp;
	efx_rc_t rc;

	EFSYS_ASSERT3U(family, >, EFX_FAMILY_INVALID);
	EFSYS_ASSERT3U(family, <, EFX_FAMILY_NTYPES);

	/* Allocate a NIC object */
	EFSYS_KMEM_ALLOC(esip, sizeof (efx_nic_t), enp);

	if (enp == NULL) {
		rc = ENOMEM;
		goto fail1;
	}

	enp->en_magic = EFX_NIC_MAGIC;

	switch (family) {
#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		enp->en_enop = &__efx_nic_siena_ops;
		enp->en_features =
		    EFX_FEATURE_IPV6 |
		    EFX_FEATURE_LFSR_HASH_INSERT |
		    EFX_FEATURE_LINK_EVENTS |
		    EFX_FEATURE_PERIODIC_MAC_STATS |
		    EFX_FEATURE_MCDI |
		    EFX_FEATURE_LOOKAHEAD_SPLIT |
		    EFX_FEATURE_MAC_HEADER_FILTERS |
		    EFX_FEATURE_TX_SRC_FILTERS;
		break;
#endif	/* EFSYS_OPT_SIENA */

	default:
		rc = ENOTSUP;
		goto fail2;
	}

	enp->en_family = family;
	enp->en_esip = esip;
	enp->en_esbp = esbp;
	enp->en_eslp = eslp;

	*enpp = enp;

	return (0);

fail2:
	EFSYS_PROBE(fail2);

	enp->en_magic = 0;

	/* Free the NIC object */
	EFSYS_KMEM_FREE(esip, sizeof (efx_nic_t), enp);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_nic_probe(
	__in		efx_nic_t *enp)
{
	const efx_nic_ops_t *enop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
#if EFSYS_OPT_MCDI
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MCDI);
#endif	/* EFSYS_OPT_MCDI */
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_PROBE));

	enop = enp->en_enop;
	if ((rc = enop->eno_probe(enp)) != 0)
		goto fail1;

	if ((rc = efx_phy_probe(enp)) != 0)
		goto fail2;

	enp->en_mod_flags |= EFX_MOD_PROBE;

	return (0);

fail2:
	EFSYS_PROBE(fail2);

	enop->eno_unprobe(enp);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_nic_set_drv_limits(
	__inout		efx_nic_t *enp,
	__in		efx_drv_limits_t *edlp)
{
	const efx_nic_ops_t *enop = enp->en_enop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);

	if (enop->eno_set_drv_limits != NULL) {
		if ((rc = enop->eno_set_drv_limits(enp, edlp)) != 0)
			goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_nic_get_bar_region(
	__in		efx_nic_t *enp,
	__in		efx_nic_region_t region,
	__out		uint32_t *offsetp,
	__out		size_t *sizep)
{
	const efx_nic_ops_t *enop = enp->en_enop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NIC);

	if (enop->eno_get_bar_region == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}
	if ((rc = (enop->eno_get_bar_region)(enp,
		    region, offsetp, sizep)) != 0) {
		goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


	__checkReturn	efx_rc_t
efx_nic_get_vi_pool(
	__in		efx_nic_t *enp,
	__out		uint32_t *evq_countp,
	__out		uint32_t *rxq_countp,
	__out		uint32_t *txq_countp)
{
	const efx_nic_ops_t *enop = enp->en_enop;
	efx_nic_cfg_t *encp = &enp->en_nic_cfg;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NIC);

	if (enop->eno_get_vi_pool != NULL) {
		uint32_t vi_count = 0;

		if ((rc = (enop->eno_get_vi_pool)(enp, &vi_count)) != 0)
			goto fail1;

		*evq_countp = vi_count;
		*rxq_countp = vi_count;
		*txq_countp = vi_count;
	} else {
		/* Use NIC limits as default value */
		*evq_countp = encp->enc_evq_limit;
		*rxq_countp = encp->enc_rxq_limit;
		*txq_countp = encp->enc_txq_limit;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


	__checkReturn	efx_rc_t
efx_nic_init(
	__in		efx_nic_t *enp)
{
	const efx_nic_ops_t *enop = enp->en_enop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);

	if (enp->en_mod_flags & EFX_MOD_NIC) {
		rc = EINVAL;
		goto fail1;
	}

	if ((rc = enop->eno_init(enp)) != 0)
		goto fail2;

	enp->en_mod_flags |= EFX_MOD_NIC;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
efx_nic_fini(
	__in		efx_nic_t *enp)
{
	const efx_nic_ops_t *enop = enp->en_enop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_PROBE);
	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_NIC);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_INTR));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_EV));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_RX));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_TX));

	enop->eno_fini(enp);

	enp->en_mod_flags &= ~EFX_MOD_NIC;
}

			void
efx_nic_unprobe(
	__in		efx_nic_t *enp)
{
	const efx_nic_ops_t *enop = enp->en_enop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
#if EFSYS_OPT_MCDI
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_MCDI);
#endif	/* EFSYS_OPT_MCDI */
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_NIC));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_INTR));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_EV));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_RX));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_TX));

	efx_phy_unprobe(enp);

	enop->eno_unprobe(enp);

	enp->en_mod_flags &= ~EFX_MOD_PROBE;
}

			void
efx_nic_destroy(
	__in	efx_nic_t *enp)
{
	efsys_identifier_t *esip = enp->en_esip;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, ==, 0);

	enp->en_family = EFX_FAMILY_INVALID;
	enp->en_esip = NULL;
	enp->en_esbp = NULL;
	enp->en_eslp = NULL;

	enp->en_enop = NULL;

	enp->en_magic = 0;

	/* Free the NIC object */
	EFSYS_KMEM_FREE(esip, sizeof (efx_nic_t), enp);
}

	__checkReturn	efx_rc_t
efx_nic_reset(
	__in		efx_nic_t *enp)
{
	const efx_nic_ops_t *enop = enp->en_enop;
	unsigned int mod_flags;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_PROBE);
	/*
	 * All modules except the MCDI, PROBE, NVRAM, VPD, MON
	 * (which we do not reset here) must have been shut down or never
	 * initialized.
	 *
	 * A rule of thumb here is: If the controller or MC reboots, is *any*
	 * state lost. If it's lost and needs reapplying, then the module
	 * *must* not be initialised during the reset.
	 */
	mod_flags = enp->en_mod_flags;
	mod_flags &= ~(EFX_MOD_MCDI | EFX_MOD_PROBE | EFX_MOD_NVRAM |
		    EFX_MOD_VPD | EFX_MOD_MON);
	EFSYS_ASSERT3U(mod_flags, ==, 0);
	if (mod_flags != 0) {
		rc = EINVAL;
		goto fail1;
	}

	if ((rc = enop->eno_reset(enp)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			const efx_nic_cfg_t *
efx_nic_cfg_get(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	return (&(enp->en_nic_cfg));
}

	__checkReturn	efx_rc_t
efx_nic_calculate_pcie_link_bandwidth(
	__in		uint32_t pcie_link_width,
	__in		uint32_t pcie_link_gen,
	__out		uint32_t *bandwidth_mbpsp)
{
	uint32_t lane_bandwidth;
	uint32_t total_bandwidth;
	efx_rc_t rc;

	if ((pcie_link_width == 0) || (pcie_link_width > 16) ||
	    !ISP2(pcie_link_width)) {
		rc = EINVAL;
		goto fail1;
	}

	switch (pcie_link_gen) {
	case EFX_PCIE_LINK_SPEED_GEN1:
		/* 2.5 Gb/s raw bandwidth with 8b/10b encoding */
		lane_bandwidth = 2000;
		break;
	case EFX_PCIE_LINK_SPEED_GEN2:
		/* 5.0 Gb/s raw bandwidth with 8b/10b encoding */
		lane_bandwidth = 4000;
		break;
	case EFX_PCIE_LINK_SPEED_GEN3:
		/* 8.0 Gb/s raw bandwidth with 128b/130b encoding */
		lane_bandwidth = 7877;
		break;
	default:
		rc = EINVAL;
		goto fail2;
	}

	total_bandwidth = lane_bandwidth * pcie_link_width;
	*bandwidth_mbpsp = total_bandwidth;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


	__checkReturn	efx_rc_t
efx_nic_check_pcie_link_speed(
	__in		efx_nic_t *enp,
	__in		uint32_t pcie_link_width,
	__in		uint32_t pcie_link_gen,
	__out		efx_pcie_link_performance_t *resultp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint32_t bandwidth;
	efx_pcie_link_performance_t result;
	efx_rc_t rc;

	if ((encp->enc_required_pcie_bandwidth_mbps == 0) ||
	    (pcie_link_width == 0) || (pcie_link_width == 32) ||
	    (pcie_link_gen == 0)) {
		/*
		 * No usable info on what is required and/or in use. In virtual
		 * machines, sometimes the PCIe link width is reported as 0 or
		 * 32, or the speed as 0.
		 */
		result = EFX_PCIE_LINK_PERFORMANCE_UNKNOWN_BANDWIDTH;
		goto out;
	}

	/* Calculate the available bandwidth in megabits per second */
	rc = efx_nic_calculate_pcie_link_bandwidth(pcie_link_width,
					    pcie_link_gen, &bandwidth);
	if (rc != 0)
		goto fail1;

	if (bandwidth < encp->enc_required_pcie_bandwidth_mbps) {
		result = EFX_PCIE_LINK_PERFORMANCE_SUBOPTIMAL_BANDWIDTH;
	} else if (pcie_link_gen < encp->enc_max_pcie_link_gen) {
		/* The link provides enough bandwidth but not optimal latency */
		result = EFX_PCIE_LINK_PERFORMANCE_SUBOPTIMAL_LATENCY;
	} else {
		result = EFX_PCIE_LINK_PERFORMANCE_OPTIMAL;
	}

out:
	*resultp = result;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}
