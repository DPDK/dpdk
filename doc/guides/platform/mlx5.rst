..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2022 6WIND S.A.
    Copyright (c) 2022 NVIDIA Corporation & Affiliates

.. include:: <isonum.txt>

NVIDIA MLX5 Common Driver
=========================

.. note::

   NVIDIA acquired Mellanox Technologies in 2020.
   The DPDK documentation and code might still include instances
   of or references to Mellanox trademarks (like BlueField and ConnectX)
   that are now NVIDIA trademarks.

The mlx5 common driver library (**librte_common_mlx5**) provides support for
**NVIDIA ConnectX-4**, **NVIDIA ConnectX-4 Lx**, **NVIDIA ConnectX-5**,
**NVIDIA ConnectX-6**, **NVIDIA ConnectX-6 Dx**, **NVIDIA ConnectX-6 Lx**,
**NVIDIA ConnectX-7**, **NVIDIA BlueField**, **NVIDIA BlueField-2** and
**NVIDIA BlueField-3** families of 10/25/40/50/100/200 Gb/s adapters.

Information and documentation for these adapters can be found on the
`NVIDIA website <https://www.nvidia.com/en-us/networking/>`_.
Help is also provided by the
`NVIDIA Networking forum <https://forums.developer.nvidia.com/c/infrastructure/369/>`_.
In addition, there is a `web section dedicated to DPDK
<https://developer.nvidia.com/networking/dpdk>`_.


Design
------

For security reasons and to enhance robustness,
this driver only handles virtual memory addresses.
The way resources allocations are handled by the kernel,
combined with hardware specifications that allow handling virtual memory addresses directly,
ensure that DPDK applications cannot access random physical memory
(or memory that does not belong to the current process).

There are different levels of objects and bypassing abilities
which are used to get the best performance:

- **Verbs** is a complete high-level generic API
- **Direct Verbs** is a device-specific API
- **DevX** allows accessing firmware objects
- **Direct Rules** manages flow steering at the low-level hardware layer

On Linux, above interfaces are provided by linking with `libibverbs` and `libmlx5`.
See :ref:`mlx5_linux_prerequisites` for installation.

On Windows, DevX is the only requirement from the above list.
See :ref:`mlx5_windows_prerequisites` for DevX SDK package installation.


.. _mlx5_classes:

Classes
-------

One mlx5 device can be probed by a number of different PMDs.
To select a specific PMD, its name should be specified as a device parameter
(e.g. ``0000:08:00.1,class=eth``).

In order to allow probing by multiple PMDs,
several classes may be listed separated by a colon.
For example: ``class=crypto:regex`` will probe both Crypto and RegEx PMDs.


Supported Classes
~~~~~~~~~~~~~~~~~

- ``class=compress`` for :doc:`../../compressdevs/mlx5`.
- ``class=crypto`` for :doc:`../../cryptodevs/mlx5`.
- ``class=eth`` for :doc:`../../nics/mlx5`.
- ``class=regex`` for :doc:`../../regexdevs/mlx5`.
- ``class=vdpa`` for :doc:`../../vdpadevs/mlx5`.

By default, the mlx5 device will be probed by the ``eth`` PMD.


Limitations
~~~~~~~~~~~

- ``eth`` and ``vdpa`` PMDs cannot be probed at the same time.
  All other combinations are possible.

- On Windows, only ``eth`` and ``crypto`` are supported.


.. _mlx5_common_compilation:

Compilation Prerequisites
-------------------------

.. _mlx5_linux_prerequisites:

Linux Prerequisites
~~~~~~~~~~~~~~~~~~~

This driver relies on external libraries and kernel drivers for resources
allocations and initialization.
The following dependencies are not part of DPDK and must be installed separately:

- **libibverbs**

  User space Verbs framework used by ``librte_common_mlx5``.
  This library provides a generic interface between the kernel
  and low-level user space drivers such as ``libmlx5``.

  It allows slow and privileged operations (context initialization,
  hardware resources allocations) to be managed by the kernel
  and fast operations to never leave user space.

- **libmlx5**

  Low-level user space driver library for NVIDIA devices,
  it is automatically loaded by ``libibverbs``.

  This library basically implements send/receive calls to the hardware queues.

- **Kernel modules**

  They provide the kernel-side Verbs API and low level device drivers
  that manage actual hardware initialization
  and resources sharing with user-space processes.

  Unlike most other PMDs, these modules must remain loaded and bound to
  their devices:

  - ``mlx5_core``: hardware driver managing NVIDIA devices
    and related Ethernet kernel network devices.
  - ``mlx5_ib``: InfiniBand device driver.
  - ``ib_uverbs``: user space driver for Verbs (entry point for ``libibverbs``).

- **Firmware update**

  NVIDIA MLNX_OFED/EN releases include firmware updates.

  Because each release provides new features, these updates must be applied to
  match the kernel modules and libraries they come with.

Libraries and kernel modules can be provided either by the Linux distribution,
or by installing NVIDIA MLNX_OFED/EN which provides compatibility with older kernels.


Upstream Dependencies
^^^^^^^^^^^^^^^^^^^^^

The mlx5 kernel modules are part of upstream Linux.
The minimal supported kernel version is 4.14.
For 32-bit, version 4.14.41 or above is required.

The libraries `libibverbs` and `libmlx5` are part of ``rdma-core``.
It is packaged by most of Linux distributions.
The minimal supported rdma-core version is 16.
For 32-bit, version 18 or above is required.

The rdma-core sources can be downloaded at
https://github.com/linux-rdma/rdma-core

It is possible to build rdma-core as static libraries starting with version 21::

    cd build
    CFLAGS=-fPIC cmake -DENABLE_STATIC=1 -DNO_PYVERBS=1 -DNO_MAN_PAGES=1 -GNinja ..
    ninja
    ninja install


NVIDIA MLNX_OFED/EN
^^^^^^^^^^^^^^^^^^^

The kernel modules and libraries are packaged with other tools
in NVIDIA MLNX_OFED or NVIDIA MLNX_EN.
The minimal supported versions are:

- NVIDIA MLNX_OFED version: **4.5** and above.
- NVIDIA MLNX_EN version: **4.5** and above.
- Firmware version:

  - ConnectX-4: **12.21.1000** and above.
  - ConnectX-4 Lx: **14.21.1000** and above.
  - ConnectX-5: **16.21.1000** and above.
  - ConnectX-5 Ex: **16.21.1000** and above.
  - ConnectX-6: **20.27.0090** and above.
  - ConnectX-6 Dx: **22.27.0090** and above.
  - ConnectX-6 Lx: **26.27.0090** and above.
  - ConnectX-7: **28.33.2028** and above.
  - BlueField: **18.25.1010** and above.
  - BlueField-2: **24.28.1002** and above.
  - BlueField-3: **32.36.3126** and above.

The firmware, the libraries libibverbs, libmlx5, and mlnx-ofed-kernel modules
are packaged in `NVIDIA MLNX_OFED
<https://network.nvidia.com/products/infiniband-drivers/linux/mlnx_ofed/>`_.
After downloading, it can be installed with this command::

   ./mlnxofedinstall --dpdk

`NVIDIA MLNX_EN
<https://network.nvidia.com/products/ethernet-drivers/linux/mlnx_en/>`_
is a smaller package including what is needed for DPDK.
After downloading, it can be installed with this command::

   ./install --dpdk

After installing, the firmware version can be checked::

   ibv_devinfo

.. note::

   Several versions of NVIDIA MLNX_OFED/EN are available. Installing the version
   this DPDK release was developed and tested against is strongly recommended.
   Please check the "Tested Platforms" section in the :doc:`../../rel_notes/index`.


.. _mlx5_windows_prerequisites:

Windows Prerequisites
~~~~~~~~~~~~~~~~~~~~~

The mlx5 PMDs rely on external libraries and kernel drivers
for resource allocation and initialization.


DevX SDK Installation
^^^^^^^^^^^^^^^^^^^^^

The DevX SDK must be installed on the machine building the Windows PMD.
Additional information can be found at
`How to Integrate Windows DevX in Your Development Environment
<https://docs.nvidia.com/networking/display/winof2v290/devx+interface>`_.
The minimal supported WinOF2 version is 2.60.


Compilation Options
-------------------

Compilation on Linux
~~~~~~~~~~~~~~~~~~~~

The ibverbs libraries can be linked with this PMD in a number of ways,
configured by the ``ibverbs_link`` build option:

``shared`` (default)
   The PMD depends on some .so files.

``dlopen``
   Split the dependencies glue in a separate library
   loaded when needed by dlopen (see ``MLX5_GLUE_PATH``).
   It makes dependencies on libibverbs and libmlx5 optional,
   and has no performance impact.

``static``
   Embed static flavor of the dependencies libibverbs and libmlx5
   in the PMD shared library or the executable static binary.


Compilation on Windows
~~~~~~~~~~~~~~~~~~~~~~

The DevX SDK location must be set through CFLAGS/LDFLAGS,
either::

   meson.exe setup "-Dc_args=-I\"%DEVX_INC_PATH%\"" "-Dc_link_args=-L\"%DEVX_LIB_PATH%\"" ...

or::

   set CFLAGS=-I"%DEVX_INC_PATH%" && set LDFLAGS=-L"%DEVX_LIB_PATH%" && meson.exe setup ...


.. _mlx5_common_env:

Environment Configuration
-------------------------

Linux Environment
~~~~~~~~~~~~~~~~~

The kernel network interfaces are brought up during initialization.
Forcing them down prevents packets reception.

The ethtool operations on the kernel interfaces may also affect the PMD.

Some runtime behaviours may be configured through environment variables.

``MLX5_GLUE_PATH``
   If built with ``ibverbs_link=dlopen``,
   list of directories in which to search for the rdma-core "glue" plug-in,
   separated by colons or semi-colons.

``MLX5_SHUT_UP_BF``
   If Verbs is used (DevX disabled),
   HW queue doorbell register mapping.
   The value 0 means non-cached IO mapping,
   while 1 is a regular memory mapping.

   With regular memory mapping, the register is flushed to HW
   usually when the write-combining buffer becomes full,
   but it depends on CPU design.


Port Link with MLNX_OFED/EN
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Ports links must be set to Ethernet::

   mlxconfig -d <mst device> query | grep LINK_TYPE
   LINK_TYPE_P1                        ETH(2)
   LINK_TYPE_P2                        ETH(2)

   mlxconfig -d <mst device> set LINK_TYPE_P1/2=1/2/3

Link type values are:

* ``1`` Infiniband
* ``2`` Ethernet
* ``3`` VPI (auto-sense)

If link type was changed, firmware must be reset as well::

   mlxfwreset -d <mst device> reset


.. _mlx5_vf:

SR-IOV Virtual Function with MLNX_OFED/EN
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

SR-IOV must be enabled on the NIC.
It can be checked in the following command::

   mlxconfig -d <mst device> query | grep SRIOV_EN
   SRIOV_EN                            True(1)

If needed, configure SR-IOV::

   mlxconfig -d <mst device> set SRIOV_EN=1 NUM_OF_VFS=16
   mlxfwreset -d <mst device> reset

After doing the change, restart the driver::

   /etc/init.d/openibd restart

or::

   service openibd restart

Then the virtual functions can be instantiated::

   echo [num_vfs] > /sys/class/infiniband/mlx5_0/device/sriov_numvfs


.. _mlx5_sub_function:

Sub-Function with MLNX_OFED/EN
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Sub-Function is a portion of the PCI device,
it has its own dedicated queues.
An SF shares PCI-level resources with other SFs and/or with its parent PCI function.

#. Requirement::

      MLNX_OFED version >= 5.4-0.3.3.0

#. Configure SF feature::

      # Run mlxconfig on both PFs on host and ECPFs on BlueField.
      mlxconfig -d <mst device> set PER_PF_NUM_SF=1 PF_TOTAL_SF=252 PF_SF_BAR_SIZE=12

#. Enable switchdev mode::

      mlxdevm dev eswitch set pci/<DBDF> mode switchdev

#. Add SF port::

      mlxdevm port add pci/<DBDF> flavour pcisf pfnum 0 sfnum <sfnum>

      Get SFID from output: pci/<DBDF>/<SFID>

#. Modify MAC address::

      mlxdevm port function set pci/<DBDF>/<SFID> hw_addr <MAC>

#. Activate SF port::

      mlxdevm port function set pci/<DBDF>/<ID> state active

#. Devargs to probe SF device::

      auxiliary:mlx5_core.sf.<num>,class=eth:regex


Enable Switchdev Mode
^^^^^^^^^^^^^^^^^^^^^

Switchdev mode is a mode in E-Switch, that binds between representor and VF or SF.
Representor is a port in DPDK that is connected to a VF or SF in such a way
that assuming there are no offload flows, each packet that is sent from the VF or SF
will be received by the corresponding representor.
While each packet that is sent to a representor will be received by the VF or SF.

After :ref:`configuring VF <mlx5_vf>`, the device must be unbound::

   printf "<device pci address>" > /sys/bus/pci/drivers/mlx5_core/unbind

Then switchdev mode is enabled::

   echo switchdev > /sys/class/net/<net device>/compat/devlink/mode

The device can be bound again at this point.


Run as Non-Root
^^^^^^^^^^^^^^^

Hugepage and resource limit setup are documented
in the :ref:`common Linux guide <Running_Without_Root_Privileges>`.
This PMD can operate without access to physical addresses,
therefore it does not require ``SYS_ADMIN`` to access ``/proc/self/pagemaps``.
Note that this requirement may still come from other drivers.

Below are additional capabilities that must be granted to the application
with the reasons for the need of each capability:

``NET_RAW``
   For raw Ethernet queue allocation through the kernel driver.

``NET_ADMIN``
   For device configuration, like setting link status or MTU.

``SYS_RAWIO``
   For using group 1 and above (software steering) in Flow API.

They can be manually granted for a specific executable file::

   setcap cap_net_raw,cap_net_admin,cap_sys_rawio+ep <executable>

Alternatively, a service manager or a container runtime
may configure the capabilities for a process.


Windows Environment
~~~~~~~~~~~~~~~~~~~

WinOF2 version 2.60 or higher must be installed on the machine.


WinOF2 Installation
^^^^^^^^^^^^^^^^^^^

The driver can be downloaded from the following site: `WINOF2
<https://network.nvidia.com/products/adapter-software/ethernet/windows/winof-2/>`_.


DevX Enablement
^^^^^^^^^^^^^^^

DevX for Windows must be enabled in the Windows registry.
The keys ``DevxEnabled`` and ``DevxFsRules`` must be set.
Additional information can be found in the WinOF2 user manual.


.. _mlx5_firmware_config:

Firmware Configuration
~~~~~~~~~~~~~~~~~~~~~~

Firmware features can be configured as key/value pairs.

The command to set a value is::

  mlxconfig -d <device> set <key>=<value>

The command to query a value is::

  mlxconfig -d <device> query <key>

The device name for the command ``mlxconfig`` can be either the PCI address,
or the mst device name found with::

  mst status

Below are some firmware configurations listed.

- link type::

    LINK_TYPE_P1
    LINK_TYPE_P2
    value: 1=Infiniband 2=Ethernet 3=VPI(auto-sense)

- enable SR-IOV::

    SRIOV_EN=1

- the maximum number of SR-IOV virtual functions::

    NUM_OF_VFS=<max>

- enable DevX (required by Direct Rules and other features)::

    UCTX_EN=1

- aggressive CQE zipping::

    CQE_COMPRESSION=1

- L3 VXLAN and VXLAN-GPE destination UDP port::

    IP_OVER_VXLAN_EN=1
    IP_OVER_VXLAN_PORT=<udp dport>

- enable VXLAN-GPE tunnel flow matching::

    FLEX_PARSER_PROFILE_ENABLE=0
    or
    FLEX_PARSER_PROFILE_ENABLE=2

- enable IP-in-IP tunnel flow matching::

    FLEX_PARSER_PROFILE_ENABLE=0

- enable MPLS flow matching::

    FLEX_PARSER_PROFILE_ENABLE=1

- enable ICMP(code/type/identifier/sequence number) / ICMP6(code/type) fields matching::

    FLEX_PARSER_PROFILE_ENABLE=2

- enable Geneve flow matching::

   FLEX_PARSER_PROFILE_ENABLE=0
   or
   FLEX_PARSER_PROFILE_ENABLE=1

- enable Geneve TLV option flow matching::

   FLEX_PARSER_PROFILE_ENABLE=0

- enable GTP flow matching::

   FLEX_PARSER_PROFILE_ENABLE=3

- enable eCPRI flow matching::

   FLEX_PARSER_PROFILE_ENABLE=4
   PROG_PARSE_GRAPH=1

- enable dynamic flex parser for flex item::

   FLEX_PARSER_PROFILE_ENABLE=4
   PROG_PARSE_GRAPH=1

- enable realtime timestamp format::

   REAL_TIME_CLOCK_ENABLE=1

- allow locking hairpin RQ data buffer in device memory::

   HAIRPIN_DATA_BUFFER_LOCK=1
   MEMIC_SIZE_LIMIT=0


.. _mlx5_common_driver_options:

Device Arguments
----------------

The driver can be configured per device.
A single argument list can be used for a device managed by multiple PMDs.
The parameters must be passed through the EAL option ``-a``,
as examples below:

- PCI device::

  -a 0000:03:00.2,class=eth:regex,mr_mempool_reg_en=0

- Auxiliary SF::

  -a auxiliary:mlx5_core.sf.2,class=compress,mr_ext_memseg_en=0

Each device class PMD has its own list of specific arguments,
and below are the arguments supported by the common mlx5 layer.

- ``class`` parameter [string]

  Select the classes of the drivers that should probe the device.
  See :ref:`mlx5_classes` for more explanation and details.

  The default value is ``eth``.

- ``mr_ext_memseg_en`` parameter [int]

  A nonzero value enables extending memseg when registering DMA memory. If
  enabled, the number of entries in MR (Memory Region) lookup table on datapath
  is minimized and it benefits performance. On the other hand, it worsens memory
  utilization because registered memory is pinned by kernel driver. Even if a
  page in the extended chunk is freed, that doesn't become reusable until the
  entire memory is freed.

  Enabled by default.

- ``mr_mempool_reg_en`` parameter [int]

  A nonzero value enables implicit registration of DMA memory of all mempools
  except those having ``RTE_MEMPOOL_F_NON_IO``. This flag is set automatically
  for mempools populated with non-contiguous objects or those without IOVA.
  The effect is that when a packet from a mempool is transmitted,
  its memory is already registered for DMA in the PMD and no registration
  will happen on the data path. The tradeoff is extra work on the creation
  of each mempool and increased HW resource use if some mempools
  are not used with MLX5 devices.

  Enabled by default.

- ``sys_mem_en`` parameter [int]

  A non-zero value enables the PMD memory management allocating memory
  from system by default, without explicit rte memory flag.

  By default, the PMD will set this value to 0.

- ``sq_db_nc`` parameter [int]

  The rdma core library can map doorbell register in two ways,
  depending on the environment variable "MLX5_SHUT_UP_BF":

  - As regular cached memory (usually with write combining attribute),
    if the variable is either missing or set to zero.
  - As non-cached memory, if the variable is present and set to not "0" value.

   The same doorbell mapping approach is implemented directly by PMD
   in UAR generation for queues created with DevX.

  The type of mapping may slightly affect the send queue performance,
  the optimal choice strongly relied on the host architecture
  and should be deduced practically.

  If ``sq_db_nc`` is set to zero, the doorbell is forced to be mapped to
  regular memory (with write combining), the PMD will perform the extra write
  memory barrier after writing to doorbell, it might increase the needed CPU
  clocks per packet to send, but latency might be improved.

  If ``sq_db_nc`` is set to one, the doorbell is forced to be mapped to non
  cached memory, the PMD will not perform the extra write memory barrier after
  writing to doorbell, on some architectures it might improve the performance.

  If ``sq_db_nc`` is set to two, the doorbell is forced to be mapped to
  regular memory, the PMD will use heuristics to decide whether a write memory
  barrier should be performed. For bursts with size multiple of recommended one
  (64 pkts) it is supposed the next burst is coming and no need to issue the
  extra memory barrier (it is supposed to be issued in the next coming burst,
  at least after descriptor writing). It might increase latency (on some hosts
  till the next packets transmit) and should be used with care.
  The PMD uses heuristics only for Tx queue, for other semd queues the doorbell
  is forced to be mapped to regular memory as same as ``sq_db_nc`` is set to 0.

  If ``sq_db_nc`` is omitted, the preset (if any) environment variable
  "MLX5_SHUT_UP_BF" value is used. If there is no "MLX5_SHUT_UP_BF", the
  default ``sq_db_nc`` value is zero for ARM64 hosts and one for others.

- ``cmd_fd`` parameter [int]

  File descriptor of ``ibv_context`` created outside the PMD.
  PMD will use this FD to import remote CTX. The ``cmd_fd`` is obtained from
  the ``ibv_context->cmd_fd`` member, which must be dup'd before being passed.
  This parameter is valid only if ``pd_handle`` parameter is specified.

  By default, the PMD will create a new ``ibv_context``.

  .. note::

     When FD comes from another process, it is the user responsibility to
     share the FD between the processes (e.g. by SCM_RIGHTS).

- ``pd_handle`` parameter [int]

  Protection domain handle of ``ibv_pd`` created outside the PMD.
  PMD will use this handle to import remote PD. The ``pd_handle`` can be
  achieved from the original PD by getting its ``ibv_pd->handle`` member value.
  This parameter is valid only if ``cmd_fd`` parameter is specified,
  and its value must be a valid kernel handle for a PD object
  in the context represented by given ``cmd_fd``.

  By default, the PMD will allocate a new PD.

  .. note::

     The ``ibv_pd->handle`` member is different than ``mlx5dv_pd->pdn`` member.
