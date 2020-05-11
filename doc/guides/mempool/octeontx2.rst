..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Marvell International Ltd.

OCTEON TX2 NPA Mempool Driver
=============================

The OCTEON TX2 NPA PMD (**librte_mempool_octeontx2**) provides mempool
driver support for the integrated mempool device found in **Marvell OCTEON TX2** SoC family.

More information about OCTEON TX2 SoC can be found at `Marvell Official Website
<https://www.marvell.com/embedded-processors/infrastructure-processors/>`_.

Features
--------

OCTEON TX2 NPA PMD supports:

- Up to 128 NPA LFs
- 1M Pools per LF
- HW mempool manager
- Ethdev Rx buffer allocation in HW to save CPU cycles in the Rx path.
- Ethdev Tx buffer recycling in HW to save CPU cycles in the Tx path.

Prerequisites and Compilation procedure
---------------------------------------

   See :doc:`../platform/octeontx2` for setup information.

Pre-Installation Configuration
------------------------------

Compile time Config Options
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following option can be modified in the ``config`` file.

- ``CONFIG_RTE_LIBRTE_OCTEONTX2_MEMPOOL`` (default ``y``)

  Toggle compilation of the ``librte_mempool_octeontx2`` driver.

Runtime Config Options
~~~~~~~~~~~~~~~~~~~~~~

- ``Maximum number of mempools per application`` (default ``128``)

  The maximum number of mempools per application needs to be configured on
  HW during mempool driver initialization. HW can support up to 1M mempools,
  Since each mempool costs set of HW resources, the ``max_pools`` ``devargs``
  parameter is being introduced to configure the number of mempools required
  for the application.
  For example::

    -w 0002:02:00.0,max_pools=512

  With the above configuration, the driver will set up only 512 mempools for
  the given application to save HW resources.

.. note::

   Since this configuration is per application, the end user needs to
   provide ``max_pools`` parameter to the first PCIe device probed by the given
   application.

- ``Lock NPA contexts in NDC``

   Lock NPA aura and pool contexts in NDC cache.
   The device args take hexadecimal bitmask where each bit represent the
   corresponding aura/pool id.

   For example::

      -w 0002:02:00.0,npa_lock_mask=0xf

Debugging Options
~~~~~~~~~~~~~~~~~

.. _table_octeontx2_mempool_debug_options:

.. table:: OCTEON TX2 mempool debug options

   +---+------------+-------------------------------------------------------+
   | # | Component  | EAL log command                                       |
   +===+============+=======================================================+
   | 1 | NPA        | --log-level='pmd\.mempool.octeontx2,8'                |
   +---+------------+-------------------------------------------------------+

Standalone mempool device
~~~~~~~~~~~~~~~~~~~~~~~~~

   The ``usertools/dpdk-devbind.py`` script shall enumerate all the mempool devices
   available in the system. In order to avoid, the end user to bind the mempool
   device prior to use ethdev and/or eventdev device, the respective driver
   configures an NPA LF and attach to the first probed ethdev or eventdev device.
   In case, if end user need to run mempool as a standalone device
   (without ethdev or eventdev), end user needs to bind a mempool device using
   ``usertools/dpdk-devbind.py``

   Example command to run ``mempool_autotest`` test with standalone OCTEONTX2 NPA device::

     echo "mempool_autotest" | build/app/test -c 0xf0 --mbuf-pool-ops-name="octeontx2_npa"
