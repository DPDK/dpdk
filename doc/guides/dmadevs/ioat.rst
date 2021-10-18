..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 Intel Corporation.

.. include:: <isonum.txt>

IOAT DMA Device Driver
=======================

The ``ioat`` dmadev driver provides a poll-mode driver (PMD) for Intel\
|reg| QuickData Technology which is part of part of Intel\ |reg| I/O
Acceleration Technology (`Intel I/OAT
<https://www.intel.com/content/www/us/en/wireless-network/accel-technology.html>`_).
This PMD, when used on supported hardware, allows data copies, for example,
cloning packet data, to be accelerated by IOAT hardware rather than having to
be done by software, freeing up CPU cycles for other tasks.

Hardware Requirements
----------------------

The ``dpdk-devbind.py`` script, included with DPDK, can be used to show the
presence of supported hardware. Running ``dpdk-devbind.py --status-dev dma``
will show all the DMA devices on the system, IOAT devices are included in this
list. For Intel\ |reg| IOAT devices, the hardware will often be listed as
"Crystal Beach DMA", or "CBDMA" or on some newer systems '0b00' due to the
absence of pci-id database entries for them at this point.

.. note::
        Error handling is not supported by this driver on hardware prior to
        Intel Ice Lake. Unsupported systems include Broadwell, Skylake and
        Cascade Lake.

Compilation
------------

For builds using ``meson`` and ``ninja``, the driver will be built when the
target platform is x86-based. No additional compilation steps are necessary.

Device Setup
-------------

Intel\ |reg| IOAT devices will need to be bound to a suitable DPDK-supported
user-space IO driver such as ``vfio-pci`` in order to be used by DPDK.

The ``dpdk-devbind.py`` script can be used to view the state of the devices using::

   $ dpdk-devbind.py --status-dev dma

The ``dpdk-devbind.py`` script can also be used to bind devices to a suitable driver.
For example::

	$ dpdk-devbind.py -b vfio-pci 00:01.0 00:01.1

Device Probing and Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For devices bound to a suitable DPDK-supported driver (``vfio-pci``), the HW
devices will be found as part of the device scan done at application
initialization time without the need to pass parameters to the application.

If the application does not require all the devices available an allowlist can
be used in the same way that other DPDK devices use them.

For example::

	$ dpdk-test -a <b:d:f>

Once probed successfully, the device will appear as a ``dmadev``, that is a
"DMA device type" inside DPDK, and can be accessed using APIs from the
``rte_dmadev`` library.

Using IOAT DMAdev Devices
--------------------------

To use IOAT devices from an application, the ``dmadev`` API can be used.

Device Configuration
~~~~~~~~~~~~~~~~~~~~~

IOAT configuration requirements:

* ``ring_size`` must be a power of two, between 64 and 4096.
* Only one ``vchan`` is supported per device.
* Silent mode is not supported.
* The transfer direction must be set to ``RTE_DMA_DIR_MEM_TO_MEM`` to copy from memory to memory.

Once configured, the device can then be made ready for use by calling the
``rte_dma_start()`` API.

Performing Data Copies
~~~~~~~~~~~~~~~~~~~~~~~

Refer to the :ref:`Enqueue / Dequeue APIs <dmadev_enqueue_dequeue>` section of the dmadev library
documentation for details on operation enqueue, submission and completion API usage.

It is expected that, for efficiency reasons, a burst of operations will be enqueued to the
device via multiple enqueue calls between calls to the ``rte_dma_submit()`` function.

When gathering completions, ``rte_dma_completed()`` should be used, up until the point an error
occurs with an operation. If an error was encountered, ``rte_dma_completed_status()`` must be used
to reset the device and continue processing operations. This function will also gather the status
of each individual operation which is filled in to the ``status`` array provided as parameter
by the application.

The status codes supported by IOAT are:

* ``RTE_DMA_STATUS_SUCCESSFUL``: The operation was successful.
* ``RTE_DMA_STATUS_INVALID_SRC_ADDR``: The operation failed due to an invalid source address.
* ``RTE_DMA_STATUS_INVALID_DST_ADDR``: The operation failed due to an invalid destination address.
* ``RTE_DMA_STATUS_INVALID_LENGTH``: The operation failed due to an invalid descriptor length.
* ``RTE_DMA_STATUS_DESCRIPTOR_READ_ERROR``: The device could not read the descriptor.
* ``RTE_DMA_STATUS_ERROR_UNKNOWN``: The operation failed due to an unspecified error.

The following code shows how to retrieve the number of successfully completed
copies within a burst and then uses ``rte_dma_completed_status()`` to check
which operation failed and reset the device to continue processing operations:

.. code-block:: C

   enum rte_dma_status_code status[COMP_BURST_SZ];
   uint16_t count, idx, status_count;
   bool error = 0;

   count = rte_dma_completed(dev_id, vchan, COMP_BURST_SZ, &idx, &error);

   if (error){
      status_count = rte_dma_completed_status(dev_id, vchan, COMP_BURST_SZ, &idx, status);
   }
