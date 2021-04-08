..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(C) 2021 Marvell.

cnxk NPA Mempool Driver
=======================

The cnxk NPA PMD (**librte_mempool_cnxk**) provides mempool driver support for
the integrated mempool device found in **Marvell OCTEON CN9K/CN10K** SoC family.

More information about cnxk SoC can be found at `Marvell Official Website
<https://www.marvell.com/embedded-processors/infrastructure-processors/>`_.

Features
--------

cnxk NPA PMD supports:

- Up to 128 NPA LFs
- 1M Pools per LF
- HW mempool manager
- Ethdev Rx buffer allocation in HW to save CPU cycles in the Rx path.
- Ethdev Tx buffer recycling in HW to save CPU cycles in the Tx path.

Prerequisites and Compilation procedure
---------------------------------------

   See :doc:`../platform/cnxk` for setup information.

Pre-Installation Configuration
------------------------------


Debugging Options
~~~~~~~~~~~~~~~~~

.. _table_cnxk_mempool_debug_options:

.. table:: cnxk mempool debug options

   +---+------------+-------------------------------------------------------+
   | # | Component  | EAL log command                                       |
   +===+============+=======================================================+
   | 1 | NPA        | --log-level='pmd\.mempool.cnxk,8'                     |
   +---+------------+-------------------------------------------------------+

Standalone mempool device
~~~~~~~~~~~~~~~~~~~~~~~~~

   The ``usertools/dpdk-devbind.py`` script shall enumerate all the mempool
   devices available in the system. In order to avoid, the end user to bind the
   mempool device prior to use ethdev and/or eventdev device, the respective
   driver configures an NPA LF and attach to the first probed ethdev or eventdev
   device. In case, if end user need to run mempool as a standalone device
   (without ethdev or eventdev), end user needs to bind a mempool device using
   ``usertools/dpdk-devbind.py``
