..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

.. include:: <isonum.txt>

IOAT Rawdev Driver for Intel\ |reg| QuickData Technology
======================================================================

The ``ioat`` rawdev driver provides a poll-mode driver (PMD) for Intel\ |reg|
QuickData Technology, part of Intel\ |reg| I/O Acceleration Technology
`(Intel I/OAT)
<https://www.intel.com/content/www/us/en/wireless-network/accel-technology.html>`_.
This PMD, when used on supported hardware, allows data copies, for example,
cloning packet data, to be accelerated by that hardware rather than having to
be done by software, freeing up CPU cycles for other tasks.

Hardware Requirements
----------------------

On Linux, the presence of an Intel\ |reg| QuickData Technology hardware can
be detected by checking the output of the ``lspci`` command, where the
hardware will be often listed as "Crystal Beach DMA" or "CBDMA". For
example, on a system with Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @2.20GHz,
lspci shows:

.. code-block:: console

  # lspci | grep DMA
  00:04.0 System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Crystal Beach DMA Channel 0 (rev 01)
  00:04.1 System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Crystal Beach DMA Channel 1 (rev 01)
  00:04.2 System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Crystal Beach DMA Channel 2 (rev 01)
  00:04.3 System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Crystal Beach DMA Channel 3 (rev 01)
  00:04.4 System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Crystal Beach DMA Channel 4 (rev 01)
  00:04.5 System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Crystal Beach DMA Channel 5 (rev 01)
  00:04.6 System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Crystal Beach DMA Channel 6 (rev 01)
  00:04.7 System peripheral: Intel Corporation Xeon E7 v4/Xeon E5 v4/Xeon E3 v4/Xeon D Crystal Beach DMA Channel 7 (rev 01)

On a system with Intel\ |reg| Xeon\ |reg| Gold 6154 CPU @ 3.00GHz, lspci
shows:

.. code-block:: console

  # lspci | grep DMA
  00:04.0 System peripheral: Intel Corporation Sky Lake-E CBDMA Registers (rev 04)
  00:04.1 System peripheral: Intel Corporation Sky Lake-E CBDMA Registers (rev 04)
  00:04.2 System peripheral: Intel Corporation Sky Lake-E CBDMA Registers (rev 04)
  00:04.3 System peripheral: Intel Corporation Sky Lake-E CBDMA Registers (rev 04)
  00:04.4 System peripheral: Intel Corporation Sky Lake-E CBDMA Registers (rev 04)
  00:04.5 System peripheral: Intel Corporation Sky Lake-E CBDMA Registers (rev 04)
  00:04.6 System peripheral: Intel Corporation Sky Lake-E CBDMA Registers (rev 04)
  00:04.7 System peripheral: Intel Corporation Sky Lake-E CBDMA Registers (rev 04)


Compilation
------------

For builds done with ``make``, the driver compilation is enabled by the
``CONFIG_RTE_LIBRTE_PMD_IOAT_RAWDEV`` build configuration option. This is
enabled by default in builds for x86 platforms, and disabled in other
configurations.

For builds using ``meson`` and ``ninja``, the driver will be built when the
target platform is x86-based.
