..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Marvell International Ltd.

CN9K REE Regexdev Driver
==============================

The CN9K REE PMD (**librte_regex_cn9k**) provides poll mode
regexdev driver support for the inbuilt regex device found in the **Marvell CN9K**
SoC family.

More information about CN9K SoC can be found at `Marvell Official Website
<https://www.marvell.com/embedded-processors/infrastructure-processors/>`_.

Features
--------

Features of the CN9K REE PMD are:

- 36 queues
- Up to 254 matches for each regex operation

Prerequisites and Compilation procedure
---------------------------------------

   See :doc:`../platform/cnxk` for setup information.

Device Setup
------------

The CN9K REE devices will need to be bound to a user-space IO driver
for use. The script ``dpdk-devbind.py`` script included with DPDK can be
used to view the state of the devices and to bind them to a suitable
DPDK-supported kernel driver. When querying the status of the devices,
they will appear under the category of "REGEX devices", i.e. the command
``dpdk-devbind.py --status-dev regex`` can be used to see the state of
those devices alone.

Debugging Options
-----------------

.. _table_cn9k_regex_debug_options:

.. table:: CN9K regex device debug options

   +---+------------+-------------------------------------------------------+
   | # | Component  | EAL log command                                       |
   +===+============+=======================================================+
   | 1 | REE        | --log-level='pmd\.regex\.cn9k,8'                      |
   +---+------------+-------------------------------------------------------+
