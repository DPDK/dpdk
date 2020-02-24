..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2019 Mellanox Technologies, Ltd

MLX5 vDPA driver
================

The MLX5 vDPA (vhost data path acceleration) driver library
(**librte_pmd_mlx5_vdpa**) provides support for **Mellanox ConnectX-6**,
**Mellanox ConnectX-6DX** and **Mellanox BlueField** families of
10/25/40/50/100/200 Gb/s adapters as well as their virtual functions (VF) in
SR-IOV context.

.. note::

   Due to external dependencies, this driver is disabled in default
   configuration of the "make" build. It can be enabled with
   ``CONFIG_RTE_LIBRTE_MLX5_VDPA_PMD=y`` or by using "meson" build system which
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

Enabling librte_pmd_mlx5_vdpa causes DPDK applications to be linked against
libibverbs.

A Mellanox mlx5 PCI device can be probed by either net/mlx5 driver or vdpa/mlx5
driver but not in parallel. Hence, the user should decide the driver by the
``class`` parameter in the device argument list.
By default, the mlx5 device will be probed by the net/mlx5 driver.

Supported NICs
--------------

* Mellanox(R) ConnectX(R)-6 200G MCX654106A-HCAT (4x200G)
* Mellanox(R) ConnectX(R)-6DX EN 100G MCX623106AN-CDAT (2*100G)
* Mellanox(R) ConnectX(R)-6DX EN 200G MCX623105AN-VDAT (1*200G)
* Mellanox(R) BlueField SmartNIC 25G MBF1M332A-ASCAT (2*25G)

Prerequisites
-------------

- Mellanox OFED version: **5.0**
  see :doc:`../../nics/mlx5` guide for more Mellanox OFED details.

Compilation options
~~~~~~~~~~~~~~~~~~~

These options can be modified in the ``.config`` file.

- ``CONFIG_RTE_LIBRTE_MLX5_VDPA_PMD`` (default **n**)

  Toggle compilation of librte_pmd_mlx5 itself.

- ``CONFIG_RTE_IBVERBS_LINK_DLOPEN`` (default **n**)

  Build PMD with additional code to make it loadable without hard
  dependencies on **libibverbs** nor **libmlx5**, which may not be installed
  on the target system.

  In this mode, their presence is still required for it to run properly,
  however their absence won't prevent a DPDK application from starting (with
  ``CONFIG_RTE_BUILD_SHARED_LIB`` disabled) and they won't show up as
  missing with ``ldd(1)``.

  It works by moving these dependencies to a purpose-built rdma-core "glue"
  plug-in which must either be installed in a directory whose name is based
  on ``CONFIG_RTE_EAL_PMD_PATH`` suffixed with ``-glue`` if set, or in a
  standard location for the dynamic linker (e.g. ``/lib``) if left to the
  default empty string (``""``).

  This option has no performance impact.

- ``CONFIG_RTE_IBVERBS_LINK_STATIC`` (default **n**)

  Embed static flavor of the dependencies **libibverbs** and **libmlx5**
  in the PMD shared library or the executable static binary.

.. note::

   For BlueField, target should be set to ``arm64-bluefield-linux-gcc``. This
   will enable ``CONFIG_RTE_LIBRTE_MLX5_VDPA_PMD`` and set
   ``RTE_CACHE_LINE_SIZE`` to 64. Default armv8a configuration of make build and
   meson build set it to 128 then brings performance degradation.

Run-time configuration
~~~~~~~~~~~~~~~~~~~~~~

- **ethtool** operations on related kernel interfaces also affect the PMD.

- ``class`` parameter [string]

  Select the class of the driver that should probe the device.
  `vdpa` for the mlx5 vDPA driver.

