..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Marvell International Ltd.

OCTEON TX2 REE Regexdev Driver
==============================

The OCTEON TX2 REE PMD (**librte_regex_octeontx2**) provides poll mode
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

Device Setup
------------

The OCTEON TX2 REE devices will need to be bound to a user-space IO driver
for use. The script ``dpdk-devbind.py`` script included with DPDK can be
used to view the state of the devices and to bind them to a suitable
DPDK-supported kernel driver. When querying the status of the devices,
they will appear under the category of "REGEX devices", i.e. the command
``dpdk-devbind.py --status-dev regex`` can be used to see the state of
those devices alone.

Debugging Options
-----------------

.. _table_octeontx2_regex_debug_options:

.. table:: OCTEON TX2 regex device debug options

   +---+------------+-------------------------------------------------------+
   | # | Component  | EAL log command                                       |
   +===+============+=======================================================+
   | 1 | REE        | --log-level='pmd\.regex\.octeontx2,8'                 |
   +---+------------+-------------------------------------------------------+
