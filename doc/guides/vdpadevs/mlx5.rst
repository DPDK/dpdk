..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2019 Mellanox Technologies, Ltd

.. include:: <isonum.txt>

MLX5 vDPA driver
================

The MLX5 vDPA (vhost data path acceleration) driver library
(**librte_vdpa_mlx5**) provides support for **Mellanox ConnectX-6**,
**Mellanox ConnectX-6 Dx** and **Mellanox BlueField** families of
10/25/40/50/100/200 Gb/s adapters as well as their virtual functions (VF) in
SR-IOV context.

.. note::

   This driver is enabled automatically when using "meson" build system which
   will detect dependencies.


Design
------

For security reasons and robustness, this driver only deals with virtual
memory addresses. The way resources allocations are handled by the kernel,
combined with hardware specifications that allow to handle virtual memory
addresses directly, ensure that DPDK applications cannot access random
physical memory (or memory that does not belong to the current process).

The PMD can use libibverbs and libmlx5 to access the device firmware
or directly the hardware components.
There are different levels of objects and bypassing abilities
to get the best performances:

- Verbs is a complete high-level generic API
- Direct Verbs is a device-specific API
- DevX allows to access firmware objects
- Direct Rules manages flow steering at low-level hardware layer

Enabling librte_vdpa_mlx5 causes DPDK applications to be linked against
libibverbs.

A Mellanox mlx5 PCI device can be probed by either net/mlx5 driver or vdpa/mlx5
driver but not in parallel. Hence, the user should decide the driver by the
``class`` parameter in the device argument list.
By default, the mlx5 device will be probed by the net/mlx5 driver.

Supported NICs
--------------

* Mellanox\ |reg| ConnectX\ |reg|-6 200G MCX654106A-HCAT (2x200G)
* Mellanox\ |reg| ConnectX\ |reg|-6 Dx EN 25G MCX621102AN-ADAT (2x25G)
* Mellanox\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)
* Mellanox\ |reg| ConnectX\ |reg|-6 Dx EN 200G MCX623105AN-VDAT (1x200G)
* Mellanox\ |reg| BlueField SmartNIC 25G MBF1M332A-ASCAT (2x25G)

Prerequisites
-------------

- Mellanox OFED version: **5.0**
  see :doc:`../../nics/mlx5` guide for more Mellanox OFED details.

Compilation option
~~~~~~~~~~~~~~~~~~

The meson option ``ibverbs_link`` is **shared** by default,
but can be configured to have the following values:

- ``dlopen``

  Build PMD with additional code to make it loadable without hard
  dependencies on **libibverbs** nor **libmlx5**, which may not be installed
  on the target system.

  In this mode, their presence is still required for it to run properly,
  however their absence won't prevent a DPDK application from starting (with
  DPDK shared build disabled) and they won't show up as missing with ``ldd(1)``.

  It works by moving these dependencies to a purpose-built rdma-core "glue"
  plug-in which must be installed in a directory whose name is based
  on ``RTE_EAL_PMD_PATH`` suffixed with ``-glue``.

  This option has no performance impact.

- ``static``

  Embed static flavor of the dependencies **libibverbs** and **libmlx5**
  in the PMD shared library or the executable static binary.

.. note::

   Default armv8a configuration of meson build sets ``RTE_CACHE_LINE_SIZE``
   to 128 then brings performance degradation.

Run-time configuration
~~~~~~~~~~~~~~~~~~~~~~

- **ethtool** operations on related kernel interfaces also affect the PMD.

Driver options
^^^^^^^^^^^^^^

- ``class`` parameter [string]

  Select the class of the driver that should probe the device.
  `vdpa` for the mlx5 vDPA driver.

- ``event_mode`` parameter [int]

  - 0, Completion queue scheduling will be managed by a timer thread which
    automatically adjusts its delays to the coming traffic rate.

  - 1, Completion queue scheduling will be managed by a timer thread with fixed
    delay time.

  - 2, Completion queue scheduling will be managed by interrupts. Each CQ burst
    arms the CQ in order to get an interrupt event in the next traffic burst.

  - Default mode is 0.

- ``event_us`` parameter [int]

  Per mode micro-seconds parameter - relevant only for event mode 0 and 1:

  - 0, A nonzero value to set timer step in micro-seconds. The timer thread
    dynamic delay change steps according to this value. Default value is 1us.

  - 1, A nonzero value to set fixed timer delay in micro-seconds. Default value
    is 100us.

- ``no_traffic_time`` parameter [int]

  A nonzero value defines the traffic off time, in seconds, that moves the
  driver to no-traffic mode. In this mode the timer events are stopped and
  interrupts are configured to the device in order to notify traffic for the
  driver. Default value is 2s.

Error handling
^^^^^^^^^^^^^^

Upon potential hardware errors, mlx5 PMD try to recover, give up if failed 3
times in 3 seconds, virtq will be put in disable state. User should check log
to get error information, or query vdpa statistics counter to know error type
and count report.
