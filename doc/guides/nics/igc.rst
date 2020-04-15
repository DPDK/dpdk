..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Intel Corporation.

IGC Poll Mode Driver
======================

The IGC PMD (librte_pmd_igc) provides poll mode driver support for Foxville
I225 Series Network Adapters.

- For information about I225, please refer to:
  `https://ark.intel.com/content/www/us/en/ark/products/series/184686/
  intel-ethernet-controller-i225-series.html`

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file.
Please note that enabling debugging options may affect system performance.

- ``CONFIG_RTE_LIBRTE_IGC_PMD`` (default ``y``)

  Toggle compilation of the ``librte_pmd_igc`` driver.

- ``CONFIG_RTE_LIBRTE_IGC_DEBUG_*`` (default ``n``)

  Toggle display of generic debugging messages.


Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.


Supported Chipsets and NICs
---------------------------

Foxville LM (I225 LM): Client 2.5G LAN vPro Corporate
Foxville V (I225 V): Client 2.5G LAN Consumer
Foxville I (I225 I): Client 2.5G Industrial Temp
Foxville V (I225 K): Client 2.5G LAN Consumer
