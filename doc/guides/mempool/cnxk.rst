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

CN9k NPA supports:

- Burst alloc of up to 32 pointers.

CN10k NPA supports:

- Batch dequeue of up to 512 pointers with single instruction.
- Batch enqueue of up to 15 pointers with single instruction.

Prerequisites and Compilation procedure
---------------------------------------

   See :doc:`../platform/cnxk` for setup information.

Pre-Installation Configuration
------------------------------


Runtime Config Options
~~~~~~~~~~~~~~~~~~~~~~

- ``Maximum number of mempools per application`` (default ``128``)

  The maximum number of mempools per application needs to be configured on
  HW during mempool driver initialization. HW can support up to 1M mempools,
  Since each mempool costs set of HW resources, the ``max_pools`` ``devargs``
  parameter is being introduced to configure the number of mempools required
  for the application.
  For example::

    -a 0002:02:00.0,max_pools=512

  With the above configuration, the driver will set up only 512 mempools for
  the given application to save HW resources.

.. note::

   Since this configuration is per application, the end user needs to
   provide ``max_pools`` parameter to the first PCIe device probed by the given
   application.

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

   Example command to run ``mempool_autotest`` test with standalone CN10K NPA device::

     echo "mempool_autotest" | <build_dir>/app/test/dpdk-test -c 0xf0 --mbuf-pool-ops-name="cn10k_mempool_ops"
