..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Marvell International Ltd.

OCTEON TX2 REE Regexdev Driver
==============================

The OCTEON TX2 REE PMD (**librte_pmd_octeontx2_regex**) provides poll mode
regexdev driver support for the inbuilt regex device found in the **Marvell OCTEON TX2**
SoC family.

More information about OCTEON TX2 SoC can be found at `Marvell Official Website
<https://www.marvell.com/embedded-processors/infrastructure-processors/>`_.

Features
--------

Features of the OCTEON TX2 REE PMD are:

- 36 queues
- Up to 254 matches for each regex operation

Prerequisites and Compilation procedure
---------------------------------------

   See :doc:`../platform/octeontx2` for setup information.

Debugging Options
-----------------

.. _table_octeontx2_regex_debug_options:

.. table:: OCTEON TX2 regex device debug options

   +---+------------+-------------------------------------------------------+
   | # | Component  | EAL log command                                       |
   +===+============+=======================================================+
   | 1 | REE        | --log-level='pmd\.regex\.octeontx2,8'                 |
   +---+------------+-------------------------------------------------------+
