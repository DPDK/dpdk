..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 Marvell International Ltd.

.. include:: <isonum.txt>

CNXK DMA Device Driver
======================

The ``cnxk`` dmadev driver provides a poll-mode driver (PMD) for Marvell DPI DMA
Hardware Accelerator block found in OCTEON 9 and OCTEON 10 family of SoCs.
Each DMA queue is exposed as a VF function when SRIOV is enabled.

The block supports following modes of DMA transfers:

#. Internal - DMA within SoC DRAM to DRAM
#. Inbound  - Host DRAM to SoC DRAM when SoC is in PCIe Endpoint
#. Outbound - SoC DRAM to Host DRAM when SoC is in PCIe Endpoint

Prerequisites and Compilation procedure
---------------------------------------

See :doc:`../platform/cnxk` for setup information.

Device Setup
-------------

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

Device Probing and Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To use the devices from an application, the dmadev API can be used.
CNXK DMA device configuration requirements:

* Only one ``vchan`` is supported per device.
* CNXK DMA devices do not support silent mode.

Once configured, the device can then be made ready for use
by calling the ``rte_dma_start()`` API.

Performing Data Copies
~~~~~~~~~~~~~~~~~~~~~~

Refer to the :ref:`Enqueue / Dequeue APIs <dmadev_enqueue_dequeue>` section
of the dmadev library documentation
for details on operation enqueue and submission API usage.

Performance Tuning Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To achieve higher performance, DMA device needs to be tuned
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
