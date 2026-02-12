.. SPDX-License-Identifier: BSD-3-Clause
   Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.

HiSilicon Accelerator DMA Driver
================================

Kunpeng SoC Accelerator subsystem supports zip function,
the zip also supports data copy and fill.
This driver exposes this capability to DPDK applications.


Supported Kunpeng SoCs
----------------------

* Kunpeng 920


Device Setup
-------------

In order to use the device in DPDK, user should
insmod `uacce.ko`, `hisi_qm.ko`
and `hisi_zip.ko` (with module parameter ``uacce_mode=1``),
then there will be several subdirectories
whose names start with ``hisi_zip`` in ``/sys/class/uacce/`` directory.

Device Probing and Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

User should use following method to probe device::

   dpdk-app -a uacce:hisi_zip-0,queues=2 ...

``hisi_zip-0`` is the directory name in the ``/sys/class/uacce/`` directory,
queues is runtime config parameter which indicates how many dmadevs are created.

If the probe is successful, two dmadevs are created,
named ``hisi_zip-0-dma0`` and ``hisi_zip-0-dma1``.

.. note::

   In the ``/sys/class/uacce/hisi_zip-x/`` directory,
   user could query API and algorithms,
   this driver can only match the device whose API is ``hisi_qm_v5``
   and algorithms contain ``udma``.

Device Configuration
~~~~~~~~~~~~~~~~~~~~~

Configuration requirements:

* ``ring_size`` obtained from UACCE API and is a fixed value.
* Only one ``vchan`` is supported per ``dmadev``.
* Silent mode is not supported.
* The transfer direction must be set to ``RTE_DMA_DIR_MEM_TO_MEM``.


Device Datapath Capability and Limitation
-----------------------------------------

Support memory copy and fill operations.

.. note::

   Currently, the maximum size of the operation data is limited to 16MB-1B in the driver.
   The device actually supports operations in a larger data size,
   but the driver requires complex operations in the datapth.
   If you have such requirement, please contact the maintainers.
