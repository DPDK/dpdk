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
