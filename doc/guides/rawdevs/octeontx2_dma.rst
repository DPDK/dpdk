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

Device Setup
-------------

The OCTEON TX2 DPI DMA HW devices will need to be bound to a
user-space IO driver for use. The script ``dpdk-devbind.py`` script
included with DPDK can be used to view the state of the devices and to bind
them to a suitable DPDK-supported kernel driver. When querying the status
of the devices, they will appear under the category of "Misc (rawdev)
devices", i.e. the command ``dpdk-devbind.py --status-dev misc`` can be
used to see the state of those devices alone.

Device Configuration
--------------------

Configuring DMA rawdev device is done using the ``rte_rawdev_configure()``
API, which takes the mempool as parameter. PMD uses this pool to submit DMA
commands to HW.

The following code shows how the device is configured

.. code-block:: c

   struct dpi_rawdev_conf_s conf = {0};
   struct rte_rawdev_info rdev_info = {.dev_private = &conf};

   conf.chunk_pool = (void *)rte_mempool_create_empty(...);
   rte_mempool_set_ops_byname(conf.chunk_pool, rte_mbuf_platform_mempool_ops(), NULL);
   rte_mempool_populate_default(conf.chunk_pool);

   rte_rawdev_configure(dev_id, (rte_rawdev_obj_t)&rdev_info);

Performing Data Transfer
------------------------

To perform data transfer using OCTEON TX2 DMA rawdev devices use standard
``rte_rawdev_enqueue_buffers()`` and ``rte_rawdev_dequeue_buffers()`` APIs.

Self test
---------

On EAL initialization, dma devices will be probed and populated into the
raw devices. The rawdev ID of the device can be obtained using

* Invoke ``rte_rawdev_get_dev_id("DPI:x")`` from the application
  where x is the VF device's bus id specified in "bus:device.func" format. Use this
  index for further rawdev function calls.

* This PMD supports driver self test, to test DMA internal mode from test
  application one can directly calls
  ``rte_rawdev_selftest(rte_rawdev_get_dev_id("DPI:x"))``
