..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 HiSilicon Limited.

HISILICON Kunpeng DMA Driver
============================

Kunpeng SoC has an internal DMA unit which can be used by application
to accelerate data copies.
The DMA PF function supports multiple DMA channels.


Supported Kunpeng SoCs
----------------------

* Kunpeng 920
* Kunpeng 930


Device Setup
-------------

Kunpeng DMA devices will need to be bound to a suitable DPDK-supported
user-space IO driver such as ``vfio-pci`` in order to be used by DPDK.

Device Probing and Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once probed successfully, the device will appear as four ``dmadev``
which can be accessed using API from the ``rte_dmadev`` library.

The name of the ``dmadev`` created is like "B:D.F-chX", e.g. DMA 0000:7b:00.0
will create four ``dmadev``,
the 1st ``dmadev`` name is "0000:7b:00.0-ch0",
and the 2nd ``dmadev`` name is "0000:7b:00.0-ch1".

Device Configuration
~~~~~~~~~~~~~~~~~~~~~

Kunpeng DMA configuration requirements:

* ``ring_size`` must be a power of two, between 32 and 8192.
* Only one ``vchan`` is supported per ``dmadev``.
* Silent mode is not supported.
* The transfer direction must be set to ``RTE_DMA_DIR_MEM_TO_MEM``.
