.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2024 ZTE Corporation.

ZTE Storage Data Accelerator (ZSDA) Poll Mode Driver
====================================================

The ZSDA compression PMD provides poll mode compression & decompression driver
support for the following hardware accelerator devices:

* ZTE Processing accelerators 1cf2


Features
--------

ZSDA compression PMD has support for:

Compression/Decompression algorithm:

   * DEFLATE - using Fixed and Dynamic Huffman encoding

Checksum generation:

   * CRC32, Adler32

Huffman code type:

   * Fixed
   * Dynamic


Limitations
-----------

* Compressdev level 0, no compression, is not supported.
* No BSD support as BSD ZSDA kernel driver not available.
* Stateful is not supported.


Installation
------------

The ZSDA compression PMD is built by default with a standard DPDK build.


Building PMDs on ZSDA
---------------------

A ZSDA device can host multiple acceleration services:

* data compression

These services are provided to DPDK applications via PMDs
which register to implement the compressdev APIs.
The PMDs use common ZSDA driver code which manages the ZSDA PCI device.


Configuring and Building the DPDK ZSDA PMDs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Further information on configuring, building and installing DPDK is described
:doc:`here <../linux_gsg/build_dpdk>`.


Build Configuration
~~~~~~~~~~~~~~~~~~~

These is the build configuration options affecting ZSDA, and its default values:

.. code-block:: console

   RTE_PMD_ZSDA_MAX_PCI_DEVICES=256


Device and driver naming
------------------------

* The zsda compressdev driver name is "compress_zsda".
  The ``rte_compressdev_devices_get()`` returns the devices exposed by this driver.

* Each zsda compression device has a unique name, in format
  <PCI BDF>, e.g. "0000:cc:00.3_zsda".
  This name can be passed to ``rte_compressdev_get_dev_id()`` to get the device ID.


Enable VFs
----------

Instructions for installation are below,
but first an explanation of the relationships between the PF/VF devices
and the PMDs visible to DPDK applications.

Each ZSDA PF device exposes a number of VF devices.
Each VF device can enable one compressdev PMD.

These ZSDA PMDs share the same underlying device and pci-mgmt code,
but are enumerated independently on their respective APIs
and appear as independent devices to applications.

.. note::

   Each VF can only be used by one DPDK process.
   It is not possible to share the same VF across multiple processes,
   even if these processes are using different acceleration services.
   Conversely one DPDK process can use one or more ZSDA VFs
   and can expose compressdev instances on each of those VFs.

The examples below are based on the 1cf2 device.
If you have a different device, use the corresponding values in the above table.

In BIOS ensure that SR-IOV is enabled and either:

* Disable VT-d or
* Enable VT-d and set ``"intel_iommu=on iommu=pt"`` in the grub file.

you need to expose the Virtual Functions (VFs) using the sysfs file system.

First find the BDFs (Bus-Device-Function) of the physical functions (PFs)
of your device, e.g.::

   lspci -d:8050

You should see output similar to::

   cc:00.4 Processing accelerators: Device 1cf2:8050 (rev 01)
   ce:00.3 Processing accelerators: Device 1cf2:8050 (rev 01)
   d0:00.3 Processing accelerators: Device 1cf2:8050 (rev 01)
   d2:00.3 Processing accelerators: Device 1cf2:8050 (rev 01)

Enable the VFs for each PF by echoing the number of VFs per PF to the PCI driver::

   echo 31 > /sys/bus/pci/device/0000:cc:00.4/sriov_numvfs
   echo 31 > /sys/bus/pci/device/0000:ce:00.3/sriov_numvfs
   echo 31 > /sys/bus/pci/device/0000:d0:00.3/sriov_numvfs
   echo 31 > /sys/bus/pci/device/0000:d2:00.3/sriov_numvfs

Check that the VFs are available for use.
For example ``lspci -d:8051`` should list 124 VF devices available.

To complete the installation follow the instructions in
`Binding the available VFs to the vfio-pci driver`_.

.. note::

   If you see the following warning in ``/var/log/messages`` it can be ignored:
   ``IOMMU should be enabled for SR-IOV to work correctly``.


Binding the available VFs to the vfio-pci driver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. note::

   Please note that due to security issues,
   the usage of older DPDK igb_uio driver is not recommended.
   This document shows how to use the more secure vfio-pci driver.

Unbind the VFs from the stock driver so they can be bound to the vfio-pci driver.

Bind to the vfio-pci driver
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Load the vfio-pci driver, bind the VF PCI device ID to it
using the ``dpdk-devbind.py`` script,
then use the ``--status`` option to confirm
the VF devices are now in use by vfio-pci kernel driver,
e.g. for the 1cf2 device::

   cd to the top-level DPDK directory
   modprobe vfio-pci
   usertools/dpdk-devbind.py -b vfio-pci 0000:cc:01.4
   usertools/dpdk-devbind.py --status

Use ``modprobe vfio-pci disable_denylist=1`` from kernel 5.9 onwards.


Testing
-------

ZSDA compression PMD can be tested by running the test application::

   cd ./<build_dir>/app/test
   ./dpdk-test -l1 -n1 -a <your zsda BDF>
   RTE>>compressdev_autotest


Debugging
---------

ZSDA logging feature can be enabled using the log-level option
(where 8 = maximum log level) on the process cmdline,
as shown in the following example::

   --log-level="gen,8"
