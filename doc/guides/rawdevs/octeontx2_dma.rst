..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Marvell International Ltd.

OCTEON TX2 DMA Driver
=====================

OCTEON TX2 has an internal DMA unit which can be used by applications to initiate
DMA transaction internally, from/to host when OCTEON TX2 operates in PCIe End
Point mode. The DMA PF function supports 8 VFs corresponding to 8 DMA queues.
Each DMA queue was exposed as a VF function when SRIOV enabled.

Features
--------

This DMA PMD supports below 3 modes of memory transfers

#. Internal - OCTEON TX2 DRAM to DRAM without core intervention

#. Inbound  - Host DRAM to OCTEON TX2 DRAM without host/OCTEON TX2 cores involvement

#. Outbound - OCTEON TX2 DRAM to Host DRAM without host/OCTEON TX2 cores involvement

Prerequisites and Compilation procedure
---------------------------------------

   See :doc:`../platform/octeontx2` for setup information.


Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file.

- ``CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_DMA_RAWDEV`` (default ``y``)

  Toggle compilation of the ``lrte_pmd_octeontx2_dma`` driver.

Enabling logs
-------------

For enabling logs, use the following EAL parameter:

.. code-block:: console

   ./your_dma_application <EAL args> --log-level=pmd.raw.octeontx2.dpi,<level>

Using ``pmd.raw.octeontx2.dpi`` as log matching criteria, all Event PMD logs
can be enabled which are lower than logging ``level``.

Initialization
--------------

The number of DMA VFs (queues) enabled can be controlled by setting sysfs
entry, `sriov_numvfs` for the corresponding PF driver.

.. code-block:: console

 echo <num_vfs> > /sys/bus/pci/drivers/octeontx2-dpi/0000\:05\:00.0/sriov_numvfs

Once the required VFs are enabled, to be accessible from DPDK, VFs need to be
bound to vfio-pci driver.
