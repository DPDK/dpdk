..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(C) 2019 Marvell International Ltd.

OCTEON TX2 Poll Mode driver
===========================

The OCTEON TX2 ETHDEV PMD (**librte_pmd_octeontx2**) provides poll mode ethdev
driver support for the inbuilt network device found in **Marvell OCTEON TX2**
SoC family as well as for their virtual functions (VF) in SR-IOV context.

More information can be found at `Marvell Official Website
<https://www.marvell.com/embedded-processors/infrastructure-processors>`_.

Features
--------

Features of the OCTEON TX2 Ethdev PMD are:


Prerequisites
-------------

See :doc:`../platform/octeontx2` for setup information.

Compile time Config Options
---------------------------

The following options may be modified in the ``config`` file.

- ``CONFIG_RTE_LIBRTE_OCTEONTX2_PMD`` (default ``y``)

  Toggle compilation of the ``librte_pmd_octeontx2`` driver.
