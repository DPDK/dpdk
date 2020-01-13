..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Marvell International Ltd.

Marvell OCTEON TX2 End Point Rawdev Driver
==========================================

OCTEON TX2 has an internal SDP unit which provides End Point mode of operation
by exposing its IOQs to Host, IOQs are used for packet I/O between Host and
OCTEON TX2. Each OCTEON TX2 SDP PF supports a max of 128 VFs and Each VF is
associated with a set of IOQ pairs.

Features
--------

This OCTEON TX2 End Point mode PMD supports

#. Packet Input - Host to OCTEON TX2 with direct data instruction mode.

#. Packet Output - OCTEON TX2 to Host with info pointer mode.

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file.

- ``CONFIG_RTE_LIBRTE_PMD_OCTEONTX2_EP_RAWDEV`` (default ``y``)

  Toggle compilation of the ``lrte_pmd_octeontx2_ep`` driver.

Initialization
--------------

The number of SDP VFs enabled, can be controlled by setting sysfs
entry `sriov_numvfs` for the corresponding PF driver.

.. code-block:: console

 echo <num_vfs> > /sys/bus/pci/drivers/octeontx2-ep/0000\:04\:00.0/sriov_numvfs

Once the required VFs are enabled, to be accessible from DPDK, VFs need to be
bound to vfio-pci driver.

Device Setup
------------

The OCTEON TX2 SDP End Point VF devices will need to be bound to a
user-space IO driver for use. The script ``dpdk-devbind.py`` script
included with DPDK can be used to view the state of the devices and to bind
them to a suitable DPDK-supported kernel driver. When querying the status
of the devices, they will appear under the category of "Misc (rawdev)
devices", i.e. the command ``dpdk-devbind.py --status-dev misc`` can be
used to see the state of those devices alone.

Device Configuration
--------------------

Configuring SDP EP rawdev device is done using the ``rte_rawdev_configure()``
API, which takes the mempool as parameter. PMD uses this pool to send/receive
packets to/from the HW.

The following code shows how the device is configured

.. code-block:: c

   struct sdp_rawdev_info config = {0};
   struct rte_rawdev_info rdev_info = {.dev_private = &config};
   config.enqdeq_mpool = (void *)rte_mempool_create(...);

   rte_rawdev_configure(dev_id, (rte_rawdev_obj_t)&rdev_info);

Performing Data Transfer
------------------------

To perform data transfer using SDP VF EP rawdev devices use standard
``rte_rawdev_enqueue_buffers()`` and ``rte_rawdev_dequeue_buffers()`` APIs.

Self test
---------

On EAL initialization, SDP VF devices will be probed and populated into the
raw devices. The rawdev ID of the device can be obtained using

* Invoke ``rte_rawdev_get_dev_id("SDPEP:x")`` from the test application
  where x is the VF device's bus id specified in "bus:device.func"(BDF)
  format. Use this index for further rawdev function calls.

* The driver's selftest rawdev API can be used to verify the SDP EP mode
  functional tests which can send/receive the raw data packets to/from the
  EP device.
