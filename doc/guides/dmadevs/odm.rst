.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2024 Marvell.

Odyssey ODM DMA Device Driver
=============================

The ``odm`` DMA device driver provides a poll-mode driver (PMD)
for Marvell Odyssey DMA Hardware Accelerator block found in Odyssey SoC.
The block supports only mem to mem DMA transfers.

ODM DMA device can support up to 32 queues and 16 VFs.

Device Setup
------------

The ODM DMA device is initialized by the ODM PF driver application,
which is responsible for configuring the device and creating VFs.

The driver can be downloaded from GitHub using the following link
`<https://github.com/MarvellEmbeddedProcessors/odm_pf_driver>`_

Instructions for enabling the ODM PF driver are provided in the repository's
README file.

The ``dpdk-devbind.py`` script, included with DPDK,
can be used to show the presence of supported hardware.
Running ``dpdk-devbind.py --status-dev dma``
will show all the Odyssey ODM DMA devices.

Devices using VFIO drivers
~~~~~~~~~~~~~~~~~~~~~~~~~~

The HW devices to be used will need to be bound to a user-space IO driver.
The ``dpdk-devbind.py`` script can be used to view the state of the devices
and to bind them to a suitable DPDK-supported driver, such as ``vfio-pci``.
For example::

   dpdk-devbind.py -b vfio-pci 0000:08:00.1

Device Probing and Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To use the devices from an application, the dmadev API can be used.

Once configured, the device can then be made ready for use
by calling the ``rte_dma_start()`` API.

Performing Data Copies
~~~~~~~~~~~~~~~~~~~~~~

Refer to the :ref:`Enqueue / Dequeue API <dmadev_enqueue_dequeue>`
section of the dmadev library documentation
for details on operation enqueue and submission API usage.

Performance Tuning Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To achieve higher performance, the DMA device needs to be tuned
using the parameters provided by the PF driver application.

The following configuration parameters are exposed
by the ODM PF driver application:

``ENG_SEL``

  ODM DMA device has 2 engines internally. Engine to queue mapping is decided
  by a hardware register which can be configured
  by updating the configuration file::

     ENG_SEL="0xCCCCCCCC"

  Each bit in the register corresponds to one queue.
  Each queue would be associated with one engine.
  If the value of the bit corresponding to the queue is 0, then engine 0 would be picked.
  If it is 1, then engine 1 would be picked.

  In the above command, the register value is set as
  ``1100 1100 1100 1100 1100 1100 1100 1100``
  which allows for alternate engines to be used with alternate VFs
  (assuming the system has 16 VFs with 2 queues each).
