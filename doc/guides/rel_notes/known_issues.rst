..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Known Issues and Limitations in Legacy Releases
===============================================

This section describes known issues with the DPDK software that aren't covered in the version specific release
notes sections.


Unit Test for Link Bonding may fail at test_tlb_tx_burst()
----------------------------------------------------------

**Description**:
   Unit tests will fail in ``test_tlb_tx_burst()`` function with error for uneven distribution of packets.

**Implication**:
   Unit test link_bonding_autotest will fail.

**Resolution/Workaround**:
   There is no workaround available.

**Affected Environment/Platform**:
   Fedora 20.

**Driver/Module**:
   Link Bonding.


Pause Frame Forwarding does not work properly on igb
----------------------------------------------------

**Description**:
   For igb devices rte_eth_flow_ctrl_set does not work as expected.
   Pause frames are always forwarded on igb, regardless of the ``RFCE``, ``MPMCF`` and ``DPF`` registers.

**Implication**:
   Pause frames will never be rejected by the host on 1G NICs and they will always be forwarded.

**Resolution/Workaround**:
   There is no workaround available.

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   Poll Mode Driver (PMD).


In packets provided by the PMD, some flags are missing
------------------------------------------------------

**Description**:
   In packets provided by the PMD, some flags are missing.
   The application does not have access to information provided by the hardware
   (packet is broadcast, packet is multicast, packet is IPv4 and so on).

**Implication**:
   The ``ol_flags`` field in the ``rte_mbuf`` structure is not correct and should not be used.

**Resolution/Workaround**:
   The application has to parse the Ethernet header itself to get the information, which is slower.

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   Poll Mode Driver (PMD).

The rte_malloc library is not fully implemented
-----------------------------------------------

**Description**:
   The ``rte_malloc`` library is not fully implemented.

**Implication**:
   All debugging features of rte_malloc library described in architecture documentation are not yet implemented.

**Resolution/Workaround**:
   No workaround available.

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   ``rte_malloc``.


HPET reading is slow
--------------------

**Description**:
   Reading the HPET chip is slow.

**Implication**:
   An application that calls ``rte_get_hpet_cycles()`` or ``rte_timer_manage()`` runs slower.

**Resolution/Workaround**:
   The application should not call these functions too often in the main loop.
   An alternative is to use the TSC register through ``rte_rdtsc()`` which is faster,
   but specific to an lcore and is a cycle reference, not a time reference.

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   Environment Abstraction Layer (EAL).


HPET timers do not work on the Osage customer reference platform
----------------------------------------------------------------

**Description**:
   HPET timers do not work on the Osage customer reference platform which includes an Intel® Xeon® processor 5500
   series processor) using the released BIOS from Intel.

**Implication**:
   On Osage boards, the implementation of the ``rte_delay_us()`` function must be changed to not use the HPET timer.

**Resolution/Workaround**:
   This can be addressed by building the system with the ``CONFIG_RTE_LIBEAL_USE_HPET=n``
   configuration option or by using the ``--no-hpet`` EAL option.

**Affected Environment/Platform**:
   The Osage customer reference platform.
   Other vendor platforms with Intel®  Xeon® processor 5500 series processors should
   work correctly, provided the BIOS supports HPET.

**Driver/Module**:
   ``lib/librte_eal/include/rte_cycles.h``


Not all variants of supported NIC types have been used in testing
-----------------------------------------------------------------

**Description**:
   The supported network interface cards can come in a number of variants with different device ID's.
   Not all of these variants have been tested with the DPDK.

   The NIC device identifiers used during testing:

   * Intel® Ethernet Controller XL710 for 40GbE QSFP+ [8086:1584]
   * Intel® Ethernet Controller XL710 for 40GbE QSFP+ [8086:1583]
   * Intel® Ethernet Controller X710 for 10GbE SFP+ [8086:1572]
   * Intel® 82576 Gigabit Ethernet Controller [8086:10c9]
   * Intel® 82576 Quad Copper Gigabit Ethernet Controller [8086:10e8]
   * Intel® 82580 Dual Copper Gigabit Ethernet Controller [8086:150e]
   * Intel® I350 Quad Copper Gigabit Ethernet Controller [8086:1521]
   * Intel® 82599 Dual Fibre 10 Gigabit Ethernet Controller [8086:10fb]
   * Intel® Ethernet Server Adapter X520-T2 [8086: 151c]
   * Intel® Ethernet Controller X540-T2 [8086:1528]
   * Intel® 82574L Gigabit Network Connection [8086:10d3]
   * Emulated Intel® 82540EM Gigabit Ethernet Controller [8086:100e]
   * Emulated Intel® 82545EM Gigabit Ethernet Controller [8086:100f]
   * Intel® Ethernet Server Adapter X520-4 [8086:154a]
   * Intel® Ethernet Controller I210 [8086:1533]

**Implication**:
   Risk of issues with untested variants.

**Resolution/Workaround**:
   Use tested NIC variants. For those supported Ethernet controllers, additional device
   IDs may be added to the software if required.

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   Poll-mode drivers


Multi-process sample app requires exact memory mapping
------------------------------------------------------

**Description**:
   The multi-process example application assumes that
   it is possible to map the hugepage memory to the same virtual addresses in client and server applications.
   Occasionally, very rarely with 64-bit, this does not occur and a client application will fail on startup.
   The Linux "address-space layout randomization" security feature can sometimes cause this to occur.

**Implication**:
   A multi-process client application fails to initialize.

**Resolution/Workaround**:
   See the "Multi-process Limitations" section in the DPDK Programmer's Guide for more information.

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   Multi-process example application


Packets are not sent by the 1 GbE/10 GbE SR-IOV driver when the source MAC is not the MAC assigned to the VF NIC
----------------------------------------------------------------------------------------------------------------

**Description**:
   The 1 GbE/10 GbE SR-IOV driver can only send packets when the Ethernet header's source MAC address is the same as
   that of the VF NIC.
   The reason for this is that the Linux ``ixgbe`` driver module in the host OS has its anti-spoofing feature enabled.

**Implication**:
   Packets sent using the 1 GbE/10 GbE SR-IOV driver must have the source MAC address correctly set to that of the VF NIC.
   Packets with other source address values are dropped by the NIC if the application attempts to transmit them.

**Resolution/Workaround**:
   Configure the Ethernet source address in each packet to match that of the VF NIC.

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   1 GbE/10 GbE VF Poll Mode Driver (PMD).


SR-IOV drivers do not fully implement the rte_ethdev API
--------------------------------------------------------

**Description**:
   The SR-IOV drivers only supports the following rte_ethdev API functions:

   * rte_eth_dev_configure()
   * rte_eth_tx_queue_setup()
   * rte_eth_rx_queue_setup()
   * rte_eth_dev_info_get()
   * rte_eth_dev_start()
   * rte_eth_tx_burst()
   * rte_eth_rx_burst()
   * rte_eth_dev_stop()
   * rte_eth_stats_get()
   * rte_eth_stats_reset()
   * rte_eth_link_get()
   * rte_eth_link_get_no_wait()

**Implication**:
   Calling an unsupported function will result in an application error.

**Resolution/Workaround**:
   Do not use other rte_ethdev API functions in applications that use the SR-IOV drivers.

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   VF Poll Mode Driver (PMD).


PMD does not work with --no-huge EAL command line parameter
-----------------------------------------------------------

**Description**:
   Currently, the DPDK does not store any information about memory allocated by ``malloc()` (for example, NUMA node,
   physical address), hence PMD drivers do not work when the ``--no-huge`` command line parameter is supplied to EAL.

**Implication**:
   Sending and receiving data with PMD will not work.

**Resolution/Workaround**:
   Use huge page memory or use VFIO to map devices.

**Affected Environment/Platform**:
   Systems running the DPDK on Linux

**Driver/Module**:
   Poll Mode Driver (PMD).


Some hardware off-load functions are not supported by the VF Driver
-------------------------------------------------------------------

**Description**:
   Currently, configuration of the following items is not supported by the VF driver:

   * IP/UDP/TCP checksum offload
   * Jumbo Frame Receipt
   * HW Strip CRC

**Implication**:
   Any configuration for these items in the VF register will be ignored.
   The behavior is dependent on the current PF setting.

**Resolution/Workaround**:
   For the PF (Physical Function) status on which the VF driver depends, there is an option item under PMD in the
   config file.
   For others, the VF will keep the same behavior as PF setting.

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   VF (SR-IOV) Poll Mode Driver (PMD).


Kernel crash on IGB port unbinding
----------------------------------

**Description**:
   Kernel crash may occur when unbinding 1G ports from the igb_uio driver, on 2.6.3x kernels such as shipped
   with Fedora 14.

**Implication**:
   Kernel crash occurs.

**Resolution/Workaround**:
   Use newer kernels or do not unbind ports.

**Affected Environment/Platform**:
   2.6.3x kernels such as  shipped with Fedora 14

**Driver/Module**:
   IGB Poll Mode Driver (PMD).


Twinpond and Ironpond NICs do not report link status correctly
--------------------------------------------------------------

**Description**:
   Twin Pond/Iron Pond NICs do not bring the physical link down when shutting down the port.

**Implication**:
   The link is reported as up even after issuing ``shutdown`` command unless the cable is physically disconnected.

**Resolution/Workaround**:
   None.

**Affected Environment/Platform**:
   Twin Pond and Iron Pond NICs

**Driver/Module**:
   Poll Mode Driver (PMD).


Discrepancies between statistics reported by different NICs
-----------------------------------------------------------

**Description**:
   Gigabit Ethernet devices from Intel include CRC bytes when calculating packet reception statistics regardless
   of hardware CRC stripping state, while 10-Gigabit Ethernet devices from Intel do so only when hardware CRC
   stripping is disabled.

**Implication**:
   There may be a  discrepancy in how different NICs display packet reception statistics.

**Resolution/Workaround**:
   None

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   Poll Mode Driver (PMD).


Error reported opening files on DPDK initialization
---------------------------------------------------

**Description**:
   On DPDK application startup, errors may be reported when opening files as part of the initialization process.
   This occurs if a large number, for example, 500 or more, or if hugepages are used, due to the per-process
   limit on the number of open files.

**Implication**:
   The DPDK application may fail to run.

**Resolution/Workaround**:
   If using 2 MB hugepages, consider switching to a fewer number of 1 GB pages.
   Alternatively, use the ``ulimit`` command to increase the number of files which can be opened by a process.

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   Environment Abstraction Layer (EAL).


Intel® QuickAssist Technology sample application does not work on a 32-bit OS on Shumway
----------------------------------------------------------------------------------------

**Description**:
   The Intel® Communications Chipset 89xx Series device does not fully support NUMA on a 32-bit OS.
   Consequently, the sample application cannot work properly on Shumway, since it requires NUMA on both nodes.

**Implication**:
   The sample application cannot work in 32-bit mode with emulated NUMA, on multi-socket boards.

**Resolution/Workaround**:
   There is no workaround available.

**Affected Environment/Platform**:
   Shumway

**Driver/Module**:
   All.


Differences in how different Intel NICs handle maximum packet length for jumbo frame
------------------------------------------------------------------------------------

**Description**:
   10 Gigabit Ethernet devices from Intel do not take VLAN tags into account when calculating packet size
   while Gigabit Ethernet devices do so for jumbo frames.

**Implication**:
   When receiving packets with VLAN tags, the actual maximum size of useful payload that Intel Gigabit Ethernet
   devices are able to receive is 4 bytes (or 8 bytes in the case of packets with extended VLAN tags) less than
   that of Intel 10 Gigabit Ethernet devices.

**Resolution/Workaround**:
   Increase the configured maximum packet size when using Intel Gigabit Ethernet devices.

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   Poll Mode Driver (PMD).


Binding PCI devices to igb_uio fails on Linux kernel 3.9 when more than one device is used
------------------------------------------------------------------------------------------

**Description**:
   A known bug in the uio driver included in Linux kernel version 3.9 prevents more than one PCI device to be
   bound to the igb_uio driver.

**Implication**:
   The Poll Mode Driver (PMD) will crash on initialization.

**Resolution/Workaround**:
   Use earlier or later kernel versions, or apply the following
   `patch  <https://github.com/torvalds/linux/commit/5ed0505c713805f89473cdc0bbfb5110dfd840cb>`_.

**Affected Environment/Platform**:
   Linux systems with kernel version 3.9

**Driver/Module**:
   igb_uio module


GCC might generate Intel® AVX instructions for processors without Intel® AVX support
------------------------------------------------------------------------------------

**Description**:
   When compiling DPDK (and any DPDK app), gcc may generate Intel® AVX instructions, even when the
   processor does not support Intel® AVX.

**Implication**:
   Any DPDK app might crash while starting up.

**Resolution/Workaround**:
   Either compile using icc or set ``EXTRA_CFLAGS='-O3'`` prior to compilation.

**Affected Environment/Platform**:
   Platforms which processor does not support Intel® AVX.

**Driver/Module**:
   Environment Abstraction Layer (EAL).

Ethertype filter could receive other packets (non-assigned) in Niantic
----------------------------------------------------------------------

**Description**:
   On Intel®  Ethernet Controller 82599EB When Ethertype filter (priority enable) was set, unmatched packets also
   could be received on the assigned queue, such as ARP packets without 802.1q tags or with the user priority not
   equal to set value.
   Launch the testpmd by disabling RSS and with multiply queues, then add the ethertype filter like the following
   and then start forwarding::

      add_ethertype_filter 0 ethertype 0x0806 priority enable 3 queue 2 index 1

   When sending ARP packets without 802.1q tag and with user priority as non-3 by tester, all the ARP packets can
   be received on the assigned queue.

**Implication**:
   The user priority comparing in Ethertype filter cannot work probably.
   It is a NIC's issue due to the following: "In fact, ETQF.UP is not functional, and the information will
   be added in errata of 82599 and X540."

**Resolution/Workaround**:
   None

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   Poll Mode Driver (PMD).


Cannot set link speed on Intel® 40G Ethernet controller
-------------------------------------------------------

**Description**:
   On Intel® 40G Ethernet Controller you cannot set the link to specific speed.

**Implication**:
   The link speed cannot be changed forcibly, though it can be configured by application.

**Resolution/Workaround**:
   None

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   Poll Mode Driver (PMD).


Devices bound to igb_uio with VT-d enabled do not work on Linux kernel 3.15-3.17
--------------------------------------------------------------------------------

**Description**:
   When VT-d is enabled (``iommu=pt intel_iommu=on``), devices are 1:1 mapped.
   In the Linux kernel unbinding devices from drivers removes that mapping which result in IOMMU errors.
   Introduced in Linux `kernel 3.15 commit
   <https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/drivers/iommu/intel-iommu.c?id=816997d03bca9fabcee65f3481eb0297103eceb7>`_,
   solved in Linux `kernel 3.18 commit
   <https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/drivers/iommu/intel-iommu.c?id=1196c2fb0407683c2df92d3d09f9144d42830894>`_.

**Implication**:
   Devices will not be allowed to access memory, resulting in following kernel errors::

      dmar: DRHD: handling fault status reg 2
      dmar: DMAR:[DMA Read] Request device [02:00.0] fault addr a0c58000
      DMAR:[fault reason 02] Present bit in context entry is clear

**Resolution/Workaround**:
   Use earlier or later kernel versions, or avoid driver binding on boot by blacklisting the driver modules.
   I.e., in the case of ``ixgbe``, we can pass the kernel command line option: ``modprobe.blacklist=ixgbe``.
   This way we do not need to unbind the device to bind it to igb_uio.

**Affected Environment/Platform**:
   Linux systems with kernel versions 3.15 to 3.17.

**Driver/Module**:
   ``igb_uio`` module.


VM power manager may not work on systems with more than 64 cores
----------------------------------------------------------------

**Description**:
   When using VM power manager on a system with more than 64 cores, VM(s) should not use cores 64 or higher.

**Implication**:
   VM power manager should not be used with VM(s) that are using cores 64 or above.

**Resolution/Workaround**:
   Do not use cores 64 or above.

**Affected Environment/Platform**:
   Platforms with more than 64 cores.

**Driver/Module**:
   VM power manager application.


DPDK may not build on some Intel CPUs using clang < 3.7.0
---------------------------------------------------------

**Description**:
   When compiling DPDK with an earlier version than 3.7.0 of clang, CPU flags are not detected on some Intel platforms
   such as Intel Broadwell/Skylake (and possibly future CPUs), and therefore compilation fails due to missing intrinsics.

**Implication**:
   DPDK will not build when using a clang version < 3.7.0.

**Resolution/Workaround**:
   Use clang 3.7.0 or higher, or gcc.

**Affected Environment/Platform**:
   Platforms with Intel Broadwell/Skylake using an old clang version.

**Driver/Module**:
   Environment Abstraction Layer (EAL).


The last EAL argument is replaced by the program name in argv[]
---------------------------------------------------------------

**Description**:
   The last EAL argument is replaced by program name in ``argv[]`` after ``eal_parse_args`` is called.
   This is the intended behavior but it causes the pointer to the last EAL argument to be lost.

**Implication**:
  If the last EAL argument in ``argv[]`` is generated by a malloc function, changing it will cause memory
  issues when freeing the argument.

**Resolution/Workaround**:
   An application should not consider the value in ``argv[]`` as unchanged.

**Affected Environment/Platform**:
   ALL.

**Driver/Module**:
   Environment Abstraction Layer (EAL).


I40e VF may not receive packets in the promiscuous mode
-------------------------------------------------------

**Description**:
   Promiscuous mode is not supported by the DPDK i40e VF driver when using the
   i40e Linux kernel driver as host driver.

**Implication**:
   The i40e VF does not receive packets when the destination MAC address is unknown.

**Resolution/Workaround**:
   Use a explicit destination MAC address that matches the VF.

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   Poll Mode Driver (PMD).


uio pci generic module bind failed in X710/XL710/XXV710
-------------------------------------------------------

**Description**:
   The ``uio_pci_generic`` module is not supported by XL710, since the errata of XL710
   states that the Interrupt Status bit is not implemented. The errata is the item #71
   from the `xl710 controller spec
   <http://www.intel.com/content/www/us/en/embedded/products/networking/xl710-10-40-controller-spec-update.html>`_.
   The hw limitation is the same as other X710/XXV710 NICs.

**Implication**:
   When use ``--bind=uio_pci_generic``, the ``uio_pci_generic`` module probes device and check the Interrupt
   Status bit. Since it is not supported by X710/XL710/XXV710, it return a *failed* value. The statement
   that these products don’t support INTx masking, is indicated in the related `linux kernel commit
   <https://git.kernel.org/cgit/linux/kernel/git/stable/linux-stable.git/commit/drivers/pci/quirks.c?id=8bcf4525c5d43306c5fd07e132bc8650e3491aec>`_.

**Resolution/Workaround**:
   Do not bind the ``uio_pci_generic`` module in X710/XL710/XXV710 NICs.

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   Poll Mode Driver (PMD).


virtio tx_burst() function cannot do TSO on shared packets
----------------------------------------------------------

**Description**:
   The standard TX function of virtio driver does not manage shared
   packets properly when doing TSO. These packets should be read-only
   but the driver modifies them.

   When doing TSO, the virtio standard expects that the L4 checksum is
   set to the pseudo header checksum in the packet data, which is
   different than the DPDK API. The driver patches the L4 checksum to
   conform to the virtio standard, but this solution is invalid when
   dealing with shared packets (clones), because the packet data should
   not be modified.

**Implication**:
   In this situation, the shared data will be modified by the driver,
   potentially causing race conditions with the other users of the mbuf
   data.

**Resolution/Workaround**:
   The workaround in the application is to ensure that the network
   headers in the packet data are not shared.

**Affected Environment/Platform**:
   Virtual machines running a virtio driver.

**Driver/Module**:
   Poll Mode Driver (PMD).


igb uio legacy mode can not be used in X710/XL710/XXV710
--------------------------------------------------------

**Description**:
   X710/XL710/XXV710 NICs lack support for indicating INTx is asserted via the interrupt
   bit in the PCI status register. Linux deleted them from INTx support table. The related
   `commit <https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git/commit/drivers/pci/quirks.c?id=8bcf4525c5d43306c5fd07e132bc8650e3491aec>`_.

**Implication**:
   When insmod ``igb_uio`` with ``intr_mode=legacy`` and test link status interrupt. Since
   INTx interrupt is not supported by X710/XL710/XXV710, it will cause Input/Output error
   when reading file descriptor.

**Resolution/Workaround**:
   Do not bind ``igb_uio`` with legacy mode in X710/XL710/XXV710 NICs, or do not use kernel
   version >4.7 when you bind ``igb_uio`` with legacy mode.

**Affected Environment/Platform**:
   ALL.

**Driver/Module**:
   Poll Mode Driver (PMD).


igb_uio can not be used when running l3fwd-power
------------------------------------------------

**Description**:
   Link Status Change(LSC) interrupt and packet receiving interrupt are all enabled in l3fwd-power
   APP. Because of UIO only support one interrupt, so these two kinds of interrupt need to share
   one, and the receiving interrupt have the higher priority, so can't get the right link status.

**Implication**:
   When insmod ``igb_uio`` and running l3fwd-power APP, link status getting doesn't work properly.

**Resolution/Workaround**:
   Use vfio-pci when LSC and packet receiving interrupt enabled.

**Affected Environment/Platform**:
   ALL.

**Driver/Module**:
   ``igb_uio`` module.


Linux kernel 4.10.0 iommu attribute read error
----------------------------------------------

**Description**:
   When VT-d is enabled (``iommu=pt intel_iommu=on``), reading IOMMU attributes from
   /sys/devices/virtual/iommu/dmarXXX/intel-iommu/cap on Linux kernel 4.10.0 error.
   This bug is fixed in `Linux commit a7fdb6e648fb
   <https://patchwork.kernel.org/patch/9595727/>`_,
   This bug is introduced in `Linux commit 39ab9555c241
   <https://patchwork.kernel.org/patch/9554403/>`_,

**Implication**:
   When binding devices to VFIO and attempting to run testpmd application,
   testpmd (and other DPDK applications) will not initialize.

**Resolution/Workaround**:
   Use other linux kernel version. It only happens in linux kernel 4.10.0.

**Affected Environment/Platform**:
   ALL OS of linux kernel 4.10.0.

**Driver/Module**:
   ``vfio-pci`` module.

Netvsc driver and application restart
-------------------------------------

**Description**:
   The Linux kernel uio_hv_generic driver does not completely shutdown and clean up
   resources properly if application using Netvsc PMD exits.

**Implication**:
   When application using Netvsc PMD is restarted it can not complete initialization
   handshake sequence with the host.

**Resolution/Workaround**:
   Either reboot the guest or remove and reinsert the hv_uio_generic module.

**Affected Environment/Platform**:
   Linux Hyper-V.

**Driver/Module**:
   ``uio_hv_generic`` module.


PHY link up fails when rebinding i40e NICs to kernel driver
-----------------------------------------------------------

**Description**:
   Some kernel drivers are not able to handle the link status correctly
   after DPDK application sets the PHY to link down.

**Implication**:
   The link status can't be set to "up" after the NIC is rebound to the
   kernel driver. Before a DPDK application quits it will invoke the
   function ``i40e_dev_stop()`` which will sets the PHY to link down. Some
   kernel drivers may not be able to handle the link status correctly after
   it retakes control of the device. This is a known PHY link configuration
   issue in the i40e kernel driver. The fix has been addressed in the 2.7.4 rc
   version. So if the i40e kernel driver is < 2.7.4 and doesn't have the
   fix backported it will encounter this issue.

**Resolution/Workaround**:
   First try to remove and reinsert the i40e kernel driver. If that fails
   reboot the system.

**Affected Environment/Platform**:
   All.

**Driver/Module**:
   Poll Mode Driver (PMD).


Restricted vdev ethdev operations supported in secondary process
----------------------------------------------------------------
**Description**
   In current virtual device sharing model, Ethernet device data structure will be
   shared between primary and secondary process. Only those Ethernet device operations
   which based on it are workable in secondary process.

**Implication**
   Some Ethernet device operations like device start/stop will be failed on virtual
   device in secondary process.

**Affected Environment/Platform**:
   ALL.

**Driver/Module**:
   Virtual Device Poll Mode Driver (PMD).


Kernel crash when hot-unplug igb_uio device while DPDK application is running
-----------------------------------------------------------------------------

**Description**:
   When device has been bound to igb_uio driver and application is running,
   hot-unplugging the device may cause kernel crash.

**Reason**:
   When device is hot-unplugged, igb_uio driver will be removed which will destroy UIO resources.
   Later trying to access any uio resource will cause kernel crash.

**Resolution/Workaround**:
   If using DPDK for PCI HW hot-unplug, prefer to bind device with VFIO instead of IGB_UIO.

**Affected Environment/Platform**:
    ALL.

**Driver/Module**:
   ``igb_uio`` module.


AVX-512 support disabled
------------------------

**Description**:
   ``AVX-512`` support has been disabled on some conditions.
   This shouldn't be confused with ``CONFIG_RTE_ENABLE_AVX512`` config option which is already
   disabled by default. This config option defines if ``AVX-512`` specific implementations of
   some file to be used or not. What has been disabled is compiler feature to produce ``AVX-512``
   instructions from any source code.

   On DPDK v18.11 ``AVX-512`` is disabled for all ``GCC`` builds which reported to cause a performance
   drop.

   On DPDK v19.02 ``AVX-512`` disable scope is reduced to ``GCC`` and ``binutils version 2.30`` based
   on information accrued from the GCC community defect.

**Reason**:
   Generated ``AVX-512`` code cause crash:
   https://bugs.dpdk.org/show_bug.cgi?id=97
   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88096

**Resolution/Workaround**:
   * Update ``binutils`` to newer version than ``2.30``.

   OR

   * Use different compiler, like ``clang`` for this case.

**Affected Environment/Platform**:
    ``GCC`` and ``binutils version 2.30``.

**Driver/Module**:
    ALL.


Unsuitable IOVA mode may be picked as the default
-------------------------------------------------

**Description**
   Not all kernel drivers and not all devices support all IOVA modes. EAL will
   attempt to pick a reasonable default based on a number of factors, but there
   may be cases where the default may be unsuitable (for example, hotplugging
   devices using `igb_uio` driver while having picked IOVA as VA mode on EAL
   initialization).

**Implication**
   Some devices (hotplugged or otherwise) may not work due to incompatible IOVA
   mode being automatically picked by EAL.

**Resolution/Workaround**:
   It is possible to force EAL to pick a particular IOVA mode by using the
   `--iova-mode` command-line parameter. If conflicting requirements are present
   (such as one device requiring IOVA as PA and one requiring IOVA as VA mode),
   there is no workaround.

**Affected Environment/Platform**:
   Linux.

**Driver/Module**:
   ALL.

Vhost multi-queue reconnection failed with QEMU version >= 4.2.0
----------------------------------------------------------------

**Description**
   It's a QEMU regression bug (bad commit: c6beefd674ff). QEMU only saves
   acked features for one vhost-net when vhost quits. When vhost reconnects
   to virtio-net/virtio-pmd in multi-queue situations, the features been
   set multiple times are not consistent.

**Implication**
   Vhost cannot reconnect back to virtio-net/virtio-pmd normally.

**Resolution/Workaround**:
   It is possible to filter the incorrect acked features at vhost-user side.

**Affected Environment/Platform**:
   ALL.

**Driver/Module**:
   Virtual Device Poll Mode Driver (PMD).
