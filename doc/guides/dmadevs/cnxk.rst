..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 Marvell International Ltd.

.. include:: <isonum.txt>

CNXK DMA Device Driver
======================

The ``cnxk`` dmadev driver provides a poll-mode driver (PMD) for Marvell DPI DMA
Hardware Accelerator block found in OCTEON CN9K, CN10K and CN20K family of SoCs.

Supported OCTEON cnxk SoCs
--------------------------

- CN9XX
- CN10XX
- CN20XX

Supported PCI devices
---------------------

.. list-table::
   :widths: 30 20 50
   :header-rows: 1

   * - SoC family
     - PCI device ID
     - Description
   * - CN9K/CN10K
     - 0xA081
     - DPI VF
   * - CN20K
     - 0xA0E8
     - DPI PF
   * - CN20K
     - 0xA0E9
     - DPI VF

On CN9K/CN10K, each DMA queue is exposed as a VF function when SRIOV is enabled.
On CN20K, both DPI PF and VF devices can be used directly by the PMD.

The block supports following modes of DMA transfers:

#. Internal - DMA within SoC DRAM to DRAM
#. Inbound  - Host DRAM to SoC DRAM when SoC is in PCIe Endpoint
#. Outbound - SoC DRAM to Host DRAM when SoC is in PCIe Endpoint

Prerequisites and Compilation procedure
---------------------------------------

See :doc:`/platform/cnxk` for setup information.

Device Setup
------------

The ``dpdk-devbind.py`` script, included with DPDK,
can be used to show the presence of supported hardware.
Running ``dpdk-devbind.py --status-dev dma`` will show all the CNXK DMA devices.

Devices using VFIO drivers
~~~~~~~~~~~~~~~~~~~~~~~~~~

The HW devices to be used will need to be bound to a user-space IO driver for use.
The ``dpdk-devbind.py`` script can be used to view the state of the devices
and to bind them to a suitable DPDK-supported driver, such as ``vfio-pci``.
For example::

     $ dpdk-devbind.py -b vfio-pci 0000:05:00.1

On CN20K, DPI PF and VF devices can be bound directly to the ``vfio-pci`` driver
and used by the DPDK PMD. The ``octeontx2_dpi.ko`` kernel driver is not required
on CN20K.

Device Probing and Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To use the devices from an application, the dmadev API can be used.
CNXK DMA device configuration requirements:

* CN9K/CN10K: only one ``vchan`` is supported per device.
* CN20K: multiple ``vchans`` are supported per device.
* CNXK DMA devices do not support silent mode.

CN20K runtime config options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following ``devargs`` parameters can be used to configure CN20K DPI devices.
For example::

     -a 0002:02:00.0,num_vchans=16,num_lfs=8

``num_vchans``

  Number of virtual channels to configure per device (default ``8``).
  The value must be a power of 2 and must not exceed ``512``.

``num_lfs``

  Number of local functions to configure per device (default ``num_vchans / 2``).
  Each LF has two hardware rings. The value must be a power of 2
  and must not exceed ``256``.

Once configured, the device can then be made ready for use
by calling the ``rte_dma_start()`` API.

Performing Data Copies
~~~~~~~~~~~~~~~~~~~~~~

Refer to the :ref:`dmadev_enqueue_dequeue` section
of the dmadev library documentation
for details on operation enqueue and submission API usage.

Performance Tuning Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On CN9K/CN10K, to achieve higher performance, DMA device needs to be tuned
using PF kernel driver module parameters.
The PF kernel driver is part of the OCTEON SDK.
Module parameters shall be configured during module insert as in below example::

    $ sudo insmod octeontx2_dpi.ko mps=128 mrrs=128 eng_fifo_buf=0x101008080808

``mps``

  Maximum payload size.
  MPS size shall not exceed the size selected by PCI config.
  Maximum size that shall be configured can be found
  on executing ``lspci`` command for the device.

``mrrs``

  Maximum read request size.
  MRRS size shall not exceed the size selected by PCI config.
  Maximum size that shall be configured can be found
  on executing ``lspci`` command for the device.

``eng_fifo_buf``

  CNXK supports 6 DMA engines and each engine has an associated FIFO.
  By default, all engine's FIFO is configured to 8 KB.
  Engine FIFO size can be tuned using this 64-bit variable,
  where each byte represents an engine.
  In the example above, engine 0-3 FIFO are configure as 8 KB
  and engine 4-5 are configured as 16 KB.

.. note::

   MPS and MRRS performance tuning parameters help achieve higher performance
   only for inbound and outbound DMA transfers.
   The parameter has no effect for internal only DMA transfer.

   Performance tuning via ``octeontx2_dpi.ko`` applies to CN9K/CN10K only.
   CN20K DPI devices do not require the kernel driver and can be used directly
   after binding to ``vfio-pci``.
