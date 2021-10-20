.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2021 HiSilicon Limited

DMA Device Library
==================

The DMA library provides a DMA device framework for management and provisioning
of hardware and software DMA poll mode drivers, defining generic API which
support a number of different DMA operations.


Design Principles
-----------------

The DMA framework provides a generic DMA device framework which supports both
physical (hardware) and virtual (software) DMA devices, as well as a generic DMA
API which allows DMA devices to be managed and configured, and supports DMA
operations to be provisioned on DMA poll mode driver.

.. _figure_dmadev:

.. figure:: img/dmadev.*

The above figure shows the model on which the DMA framework is built on:

 * The DMA controller could have multiple hardware DMA channels (aka. hardware
   DMA queues), each hardware DMA channel should be represented by a dmadev.
 * The dmadev could create multiple virtual DMA channels, each virtual DMA
   channel represents a different transfer context.
 * The DMA operation request must be submitted to the virtual DMA channel.


Device Management
-----------------

Device Creation
~~~~~~~~~~~~~~~

Physical DMA controllers are discovered during the PCI probe/enumeration of the
EAL function which is executed at DPDK initialization, this is based on their
PCI BDF (bus/bridge, device, function). Specific physical DMA controllers, like
other physical devices in DPDK can be listed using the EAL command line options.

The dmadevs are dynamically allocated by using the function
``rte_dma_pmd_allocate`` based on the number of hardware DMA channels.


Device Identification
~~~~~~~~~~~~~~~~~~~~~

Each DMA device, whether physical or virtual is uniquely designated by two
identifiers:

- A unique device index used to designate the DMA device in all functions
  exported by the DMA API.

- A device name used to designate the DMA device in console messages, for
  administration or debugging purposes.


Device Features and Capabilities
--------------------------------

DMA devices may support different feature sets. The ``rte_dma_info_get`` API
can be used to get the device info and supported features.

Silent mode is a special device capability which does not require the
application to invoke dequeue APIs.

.. _dmadev_enqueue_dequeue:


Enqueue / Dequeue APIs
~~~~~~~~~~~~~~~~~~~~~~

Enqueue APIs such as ``rte_dma_copy`` and ``rte_dma_fill`` can be used to
enqueue operations to hardware. If an enqueue is successful, a ``ring_idx`` is
returned. This ``ring_idx`` can be used by applications to track per operation
metadata in an application-defined circular ring.

The ``rte_dma_submit`` API is used to issue doorbell to hardware.
Alternatively the ``RTE_DMA_OP_FLAG_SUBMIT`` flag can be passed to the enqueue
APIs to also issue the doorbell to hardware.

The following code demonstrates how to enqueue a burst of copies to the
device and start the hardware processing of them:

.. code-block:: C

   struct rte_mbuf *srcs[DMA_BURST_SZ], *dsts[DMA_BURST_SZ];
   unsigned int i;

   for (i = 0; i < RTE_DIM(srcs); i++) {
      if (rte_dma_copy(dev_id, vchan, rte_pktmbuf_iova(srcs[i]),
            rte_pktmbuf_iova(dsts[i]), COPY_LEN, 0) < 0) {
         PRINT_ERR("Error with rte_dma_copy for buffer %u\n", i);
         return -1;
      }
   }
   rte_dma_submit(dev_id, vchan);

There are two dequeue APIs ``rte_dma_completed`` and
``rte_dma_completed_status``, these are used to obtain the results of the
enqueue requests. ``rte_dma_completed`` will return the number of successfully
completed operations. ``rte_dma_completed_status`` will return the number of
completed operations along with the status of each operation (filled into the
``status`` array passed by user). These two APIs can also return the last
completed operation's ``ring_idx`` which could help user track operations within
their own application-defined rings.


Querying Device Statistics
~~~~~~~~~~~~~~~~~~~~~~~~~~

The statistics from a dmadev device can be got via the statistics functions,
i.e. ``rte_dma_stats_get()``. The statistics returned for each device instance are:

* ``submitted``: The number of operations submitted to the device.
* ``completed``: The number of operations which have completed (successful and failed).
* ``errors``: The number of operations that completed with error.
