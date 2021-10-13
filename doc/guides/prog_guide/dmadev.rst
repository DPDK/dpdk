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
