..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2026 NXP

NXP i.MX95 eDMA5 DMA Driver
===========================

The ``imx_edma5`` DMA driver is a poll-mode driver (PMD) for the NXP i.MX95
Enhanced Direct Memory Access controller version 5 (eDMA5).
It exposes each eDMA5 controller instance as a DPDK dmadev device and
can be used through the generic DMA device (dmadev) API.

The i.MX95 SoC integrates multiple eDMA instances.
The eDMA5 instances provide 64 hardware channels, 64-bit addressing and
a 64-byte Transfer Control Descriptor (TCD64).
This driver targets the eDMA5 instances only (device tree compatible ``fsl,imx95-edma5``).

Supported Features
------------------

- Memory-to-memory copy (``RTE_DMA_DIR_MEM_TO_MEM``).
- Single-operation copy (``rte_dma_copy``).
- Scatter-gather copy (``rte_dma_copy_sg``).
- Per virtual channel statistics.

Each configured virtual channel (vchan) is mapped one-to-one onto a hardware eDMA5 channel.
Software-initiated (SWSTART) single-block transfers are programmed into the per-channel TCD and
completion is detected by polling the TCD DONE status.

Prerequisites
-------------

The eDMA5 register window is memory-mapped into the userspace process through
the DPDK platform bus using the Linux ``vfio-platform`` mechanism.
The device tree node targeted by this driver must be released from the kernel ``fsl-edma``
driver (its status set to ``disabled`` or the node unbound) before it can be used by DPDK.

Bind the platform device to ``vfio-platform``, for example::

   echo vfio-platform > /sys/bus/platform/devices/<node>/driver_override
   echo <node> > /sys/bus/platform/drivers/vfio-platform/bind

where ``<node>`` is the platform device name of the eDMA5 instance
(for example ``42000000.dma-controller``).

Compilation
-----------

The driver is built as part of the standard DPDK meson build on Linux targets.
No extra configuration option is required.

Limitations
-----------

- Only the memory-to-memory transfer direction is supported.
- The scatter-gather path iterates the source and destination segment lists independently and
  programs one hardware transfer per consumed segment;
  full TCD scatter-gather linking is not yet implemented.
- The driver operates in poll mode only; completion interrupts are not used.
