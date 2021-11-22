..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2016 The DPDK contributors

DPDK Release 2.2
================

New Features
------------

* **Introduce ARMv7 and ARMv8 architectures.**

  * It is now possible to build DPDK for the ARMv7 and ARMv8 platforms.
  * ARMv7 can be tested with virtual PMDs.
  * ARMv8 can be tested with virtual and physical PMDs.

* **Enabled freeing of ring.**

  A new function ``rte_ring_free()`` has been added to allow the user to free
  a ring if it was created with ``rte_ring_create()``.

* **Added keepalive support to EAL and example application.**

* **Added experimental cryptodev API**

  The cryptographic processing of packets is provided as a preview
  with two drivers for:

  * Intel QuickAssist devices
  * Intel AES-NI multi-buffer library

  Due to its experimental state, the API may change without prior notice.

* **Added ethdev APIs for additional IEEE1588 support.**

  Added functions to read, write and adjust system time in the NIC.
  Added client slave sample application to demonstrate the IEEE1588
  functionality.

* **Extended Statistics.**

  Defined an extended statistics naming scheme to store metadata in the name
  string of each statistic. Refer to the Extended Statistics section of the
  Programmers Guide for more details.

  Implemented the extended statistics API for the following PMDs:

  * ``igb``
  * ``igbvf``
  * ``i40e``
  * ``i40evf``
  * ``fm10k``
  * ``virtio``

* **Added API in ethdev to retrieve RX/TX queue information.**

  *  Added the ability for the upper layer to query RX/TX queue information.
  *  Added new fields in ``rte_eth_dev_info`` to represent information about
     RX/TX descriptors min/max/align numbers, per queue, for the device.

* **Added RSS dynamic configuration to bonding.**

* **Updated the e1000 base driver.**

  The e1000 base driver was updated with several features including the
  following:

  * Added new i218 devices
  * Allowed both ULP and EEE in Sx state
  * Initialized 88E1543 (Marvell 1543) PHY
  * Added flags to set EEE advertisement modes
  * Supported inverted format ETrackId
  * Added bit to disable packetbuffer read
  * Added defaults for i210 RX/TX PBSIZE
  * Check more errors for ESB2 init and reset
  * Check more NVM read errors
  * Return code after setting receive address register
  * Removed all NAHUM6LP_HW tags

* **Added e1000 RX interrupt support.**

* **Added igb TSO support for both PF and VF.**

* **Added RSS enhancements to Intel x550 NIC.**

  * Added support for 512 entry RSS redirection table.
  * Added support for per VF RSS redirection table.

* **Added Flow director enhancements on Intel x550 NIC.**

  * Added 2 new flow director modes on x550.
    One is MAC VLAN mode, the other is tunnel mode.

* **Updated the i40e base driver.**

  The i40e base driver was updated with several changes including the
  following:

  *  Added promiscuous on VLAN support
  *  Added a workaround to drop all flow control frames
  *  Added VF capabilities to virtual channel interface
  *  Added TX Scheduling related AQ commands
  *  Added additional PCTYPES supported for FortPark RSS
  *  Added parsing for CEE DCBX TLVs
  *  Added FortPark specific registers
  *  Added AQ functions to handle RSS Key and LUT programming
  *  Increased PF reset max loop limit

* **Added i40e vector RX/TX.**

* **Added i40e RX interrupt support.**

* **Added i40e flow control support.**

* **Added DCB support to i40e PF driver.**

* **Added RSS/FD input set granularity on Intel X710/XL710.**

* **Added different GRE key length for input set on Intel X710/XL710.**

* **Added flow director support in i40e VF.**

* **Added i40e support of early X722 series.**

  Added early X722 support, for evaluation only, as the hardware is alpha.

* **Added fm10k vector RX/TX.**

* **Added fm10k TSO support for both PF and VF.**

* **Added fm10k VMDQ support.**

* **New NIC Boulder Rapid support.**

  Added support for the Boulder Rapid variant of Intel's fm10k NIC family.

* **Enhanced support for the Chelsio CXGBE driver.**

  *  Added support for Jumbo Frames.
  *  Optimized forwarding performance for Chelsio T5 40GbE cards.

* **Improved enic TX packet rate.**

  Reduced frequency of TX tail pointer updates to the NIC.

* **Added support for link status interrupts in mlx4.**

* **Added partial support (TX only) for secondary processes in mlx4.**

* **Added support for Mellanox ConnectX-4 adapters (mlx5).**

  The mlx5 poll-mode driver implements support for Mellanox ConnectX-4 EN
  and Mellanox ConnectX-4 Lx EN families of 10/25/40/50/100 Gb/s adapters.

  Like mlx4, this PMD is only available for Linux and is disabled by default
  due to external dependencies (libibverbs and libmlx5).

* **Added driver for Netronome nfp-6xxx card.**

  Support for using Netronome nfp-6xxx with PCI VFs.

* **Added virtual szedata2 driver for COMBO cards.**

  Added virtual PMD for COMBO-100G and COMBO-80G cards.
  PMD is disabled in default configuration.

* **Enhanced support for virtio driver.**

  * Virtio ring layout optimization (fixed avail ring)
  * Vector RX
  * Simple TX

* **Added vhost-user multiple queue support.**

* **Added port hotplug support to vmxnet3.**

* **Added port hotplug support to xenvirt.**

* **Added ethtool shim and sample application.**

* **Added experimental performance thread example application.**

  The new sample application demonstrates L3 forwarding with different threading
  models: pthreads, cgroups, or lightweight threads. The example includes
  a simple cooperative scheduler.

  Due to its experimental state this application may change without notice.
  The application is supported only for Linux x86_64.

* **Enhancements to the IP pipeline application.**

  The following features have been added to the ``ip_pipeline``
  application;

  * Added Multiple Producers/Multiple Consumers (MPSC)
    and fragmentation/reassembly support to software rings.

  * Added a dynamic pipeline reconfiguration feature that
    allows binding a pipeline to other threads at runtime
    using CLI commands.

  * Added enable/disable of ``promisc`` mode from ``ip_pipeline``
    configuration file.

  * Added check on RX queues and TX queues of each link
    whether they are used correctly in the ``ip_pipeline``
    configuration file.

  * Added flow id parameters to the flow-classification
    table entries.

  * Added more functions to the routing pipeline:
    ARP table enable/disable, Q-in-Q and MPLS encapsulation,
    add color (traffic-class for QoS) to the MPLS tag.

  * Added flow-actions pipeline for traffic metering/marking
    (for e.g. Two Rate Three Color Marker (trTCM)), policer etc.

  * Modified the pass-through pipeline's actions-handler to
    implement a generic approach to extract fields from the
    packet's header and copy them to packet metadata.


Resolved Issues
---------------

EAL
~~~

* **eal/linux: Fixed epoll timeout.**

  Fixed issue where the ``rte_epoll_wait()`` function didn't return when the
  underlying call to ``epoll_wait()`` timed out.


Drivers
~~~~~~~

* **e1000/base: Synchronize PHY interface on non-ME systems.**

  On power up, the MAC - PHY interface needs to be set to PCIe, even if the
  cable is disconnected. In ME systems, the ME handles this on exit from the
  Sx (Sticky mode) state. In non-ME, the driver handles it. Added a check for
  non-ME system to the driver code that handles it.

* **e1000/base: Increased timeout of reset check.**

  Previously, in ``check_reset_block`` RSPCIPHY was polled for 100 ms before
  determining that the ME veto was set. This was not enough and it was
  increased to 300 ms.

* **e1000/base: Disabled IPv6 extension header parsing on 82575.**

  Disabled IPv6 options as per hardware limitation.

* **e1000/base: Prevent ULP flow if cable connected.**

  Enabling ULP on link down when the cable is connected caused an infinite
  loop of link up/down indications in the NDIS driver.
  The driver now enables ULP only when the cable is disconnected.

* **e1000/base: Support different EEARBC for i210.**

  EEARBC has changed on i210. It means EEARBC has a different address on
  i210 than on other NICs. So, add a new entity named EEARBC_I210 to the
  register list and make sure the right one is being used on i210.

* **e1000/base: Fix K1 configuration.**

  Added fix for the following updates to the K1 configurations:
  TX idle period for entering K1 should be 128 ns.
  Minimum TX idle period in K1 should be 256 ns.

* **e1000/base: Fix link detect flow.**

  Fix link detect flow in case where auto-negotiate is not enabled, by calling
  ``e1000_setup_copper_link_generic`` instead of ``e1000_phy_setup_autoneg``.

* **e1000/base: Fix link check for i354 M88E1112 PHY.**

  The ``e1000_check_for_link_media_swap()`` function is supposed to check PHY
  page 0 for copper and PHY page 1 for "other" (fiber) links. The driver
  switched back from page 1 to page 0 too soon, before
  ``e1000_check_for_link_82575()`` is executed and was never finding the link
  on the fiber (other).

  If the link is copper, as the M88E1112 page address is set to 1, it should be
  set back to 0 before checking this link.

* **e1000/base: Fix beacon duration for i217.**

  Fix for I217 Packet Loss issue - The Management Engine sets the FEXTNVM4
  Beacon Duration incorrectly. This fix ensures that the correct value will
  always be set. Correct value for this field is 8 usec.

* **e1000/base: Fix TIPG for non 10 half duplex mode.**

  TIPG value is increased when setting speed to 10 half duplex to prevent
  packet loss. However, it was never decreased again when speed
  changed. This caused performance issues in the NDIS driver.
  Fix this to restore TIPG to default value on non 10 half duplex.

* **e1000/base: Fix reset of DH89XXCC SGMII.**

  For DH89XXCC_SGMII, a write flush leaves registers of this device trashed
  (0xFFFFFFFF). Add check for this device.

  Also, after both Port SW Reset and Device Reset case, the platform should
  wait at least 3ms before reading any registers. Remove this condition since
  waiting is conditionally executed only for Device Reset.

* **e1000/base: Fix redundant PHY power down for i210.**

  Bit 11 of PHYREG 0 is used to power down PHY. The use of PHYREG 16 is
  no longer necessary.

* **e1000/base: fix jumbo frame CRC failures.**

  Change the value of register 776.20[11:2] for jumbo mode from 0x1A to 0x1F.
  This is to enlarge the gap between read and write pointers in the TX FIFO.

* **e1000/base: Fix link flap on 82579.**

  Several customers have reported a link flap issue on 82579. The symptoms
  are random and intermittent link losses when 82579 is connected to specific
  switches. the Issue was root caused as an interoperability problem between
  the NIC and at least some Broadcom PHYs in the Energy Efficient Ethernet
  wake mechanism.

  To fix the issue, we are disabling the Phase Locked Loop shutdown in 100M
  Low Power Idle. This solution will cause an increase of power in 100M EEE
  link. It may cost an additional 28mW in this specific mode.

* **igb: Fixed IEEE1588 frame identification in I210.**

  Fixed issue where the flag ``PKT_RX_IEEE1588_PTP`` was not being set
  in the Intel I210 NIC, as the EtherType in RX descriptor is in bits 8:10 of
  Packet Type and not in the default bits 0:2.

* **igb: Fixed VF start with PF stopped.**

  VF needs the PF interrupt support initialized even if not started.

* **igb: Fixed VF MAC address when using with DPDK PF.**

  Assign a random MAC address in VF when not assigned by PF.

* **igb: Removed CRC bytes from byte counter statistics.**

* **ixgbe: Fixed issue with X550 DCB.**

  Fixed a DCB issue with x550 where for 8 TCs (Traffic Classes), if a packet
  with user priority 6 or 7 was injected to the NIC, then the NIC would only
  put 3 packets into the queue. There was also a similar issue for 4 TCs.

* **ixgbe: Removed burst size restriction of vector RX.**

  Fixed issue where a burst size less than 32 didn't receive anything.

* **ixgbe: Fixed VF start with PF stopped.**

  VF needs the PF interrupt support initialized even if not started.

* **ixgbe: Fixed TX hang when RS distance exceeds HW limit.**

  Fixed an issue where the TX queue can hang when a lot of highly fragmented
  packets have to be sent. As part of that fix, ``tx_rs_thresh`` for ixgbe PMD
  is not allowed to be greater then to 32 to comply with HW restrictions.

* **ixgbe: Fixed rx error statistic counter.**

  Fixed an issue that the rx error counter of ixgbe was not accurate. The
  mac short packet discard count (mspdc) was added to the counter. Mac local
  faults and mac remote faults are removed as they do not count packets but
  errors, and jabber errors were removed as they are already accounted for
  by the CRC error counter. Finally the XEC (l3 / l4 checksum error) counter
  was removed due to errata, see commit 256ff05a9cae for details.

* **ixgbe: Removed CRC bytes from byte counter statistics.**

* **i40e: Fixed base driver allocation when not using first numa node.**

  Fixed i40e issue that occurred when a DPDK application didn't initialize
  ports if memory wasn't available on socket 0.

* **i40e: Fixed maximum of 64 queues per port.**

  Fixed an issue in i40e where it would not support more than 64 queues per
  port, even though the hardware actually supports it. The real number of
  queues may vary, as long as the total number of queues used in PF, VFs, VMDq
  and FD does not exceeds the hardware maximum.

* **i40e: Fixed statistics of packets.**

  Added discarding packets on VSI to the stats and rectify the old statistics.

* **i40e: Fixed issue of not freeing memzone.**

  Fixed an issue of not freeing a memzone in the call to free the memory for
  adminq DMA.

* **i40e: Removed CRC bytes from byte counter statistics.**

* **mlx: Fixed driver loading.**

  The mlx drivers were unable to load when built as a shared library,
  due to a missing symbol in the mempool library.

* **mlx4: Performance improvements.**

  Fixed bugs in TX and RX flows that improves mlx4 performance.

* **mlx4: Fixed TX loss after initialization.**

* **mlx4: Fixed scattered TX with too many segments.**

* **mlx4: Fixed memory registration for indirect mbuf data.**

* **vhost: Fixed Qemu shutdown.**

  Fixed issue with libvirt ``virsh destroy`` not killing the VM.

* **virtio: Fixed crash after changing link state.**

  Fixed IO permission in the interrupt handler.

* **virtio: Fixed crash when releasing queue.**

  Fixed issue when releasing null control queue.


Libraries
~~~~~~~~~

* **hash: Fixed memory allocation of Cuckoo Hash key table.**

  Fixed issue where an incorrect Cuckoo Hash key table size could be
  calculated limiting the size to 4GB.

* **hash: Fixed incorrect lookup if key is all zero.**

  Fixed issue in hash library that occurred if an all zero
  key was not added to the table and the key was looked up,
  resulting in an incorrect hit.

* **hash: Fixed thread scaling by reducing contention.**

  Fixed issue in the hash library where, using multiple cores with
  hardware transactional memory support, thread scaling did not work,
  due to the global ring that is shared by all cores.


Examples
~~~~~~~~

* **l3fwd: Fixed crash with IPv6.**

* **vhost_xen: Fixed compile error.**


Other
~~~~~

* This release drops compatibility with Linux kernel 2.6.33. The minimum
  kernel requirement is now 2.6.34.


Known Issues
------------

* Some drivers do not fill in the packet type when receiving.
  As the l3fwd example application requires this info, the i40e vector
  driver must be disabled to benefit of the packet type with i40e.

* Some (possibly all) VF drivers (e.g. i40evf) do not handle any PF reset
  events/requests in the VF driver. This means that the VF driver may not work
  after a PF reset in the host side. The workaround is to avoid triggering any
  PF reset events/requests on the host side.

* 100G link report support is missing.

* **Mellanox PMDs (mlx4 & mlx5):**

  * PMDs do not support CONFIG_RTE_BUILD_COMBINE_LIBS and
    CONFIG_RTE_BUILD_SHARED_LIB simultaneously.

  * There is performance degradation for small packets when the PMD is
    compiled with ``SGE_WR_N = 4`` compared to the performance when ``SGE_WR_N
    = 1``. If scattered packets are not used it is recommended to compile the
    PMD with ``SGE_WR_N = 1``.

  * When a Multicast or Broadcast packet is sent to the SR-IOV mlx4 VF,
    it is returned back to the port.

  * PMDs report "bad" L4 checksum when IP packet is received.

  * mlx5 PMD reports "bad" checksum although the packet has "good" checksum.
    Will be fixed in upcoming MLNX_OFED release.


API Changes
-----------

* The deprecated flow director API is removed.
  It was replaced by ``rte_eth_dev_filter_ctrl()``.

* The ``dcb_queue`` is renamed to ``dcb_tc`` in following dcb configuration
  structures: ``rte_eth_dcb_rx_conf``, ``rte_eth_dcb_tx_conf``,
  ``rte_eth_vmdq_dcb_conf``, ``rte_eth_vmdq_dcb_tx_conf``.

* The ``rte_eth_rx_queue_count()`` function now returns "int" instead of
  "uint32_t" to allow the use of negative values as error codes on return.

* The function ``rte_eal_pci_close_one()`` is removed.
  It was replaced by ``rte_eal_pci_detach()``.

* The deprecated ACL API ``ipv4vlan`` is removed.

* The deprecated hash function ``rte_jhash2()`` is removed.
  It was replaced by ``rte_jhash_32b()``.

* The deprecated KNI functions are removed:
  ``rte_kni_create()``, ``rte_kni_get_port_id()`` and ``rte_kni_info_get()``.

* The deprecated ring PMD functions are removed:
  ``rte_eth_ring_pair_create()`` and ``rte_eth_ring_pair_attach()``.

* The devargs union field ``virtual`` is renamed to ``virt`` for C++
  compatibility.


ABI Changes
-----------

* The EAL and ethdev structures ``rte_intr_handle`` and ``rte_eth_conf`` were
  changed to support RX interrupt. This was already included in 2.1 under the
  ``CONFIG_RTE_NEXT_ABI`` #define.

* The ethdev flow director entries for SCTP were changed.
  This was already included in 2.1 under the ``CONFIG_RTE_NEXT_ABI`` #define.

* The ethdev flow director structure ``rte_eth_fdir_flow_ext`` structure was
  changed. New fields were added to support flow director filtering in VF.

* The size of the ethdev structure ``rte_eth_hash_filter_info`` is changed
  by adding a new element ``rte_eth_input_set_conf`` in a union.

* New fields ``rx_desc_lim`` and ``tx_desc_lim`` are added into
  ``rte_eth_dev_info`` structure.

* For debug builds, the functions ``rte_eth_rx_burst()``, ``rte_eth_tx_burst()``
  ``rte_eth_rx_descriptor_done()`` and ``rte_eth_rx_queue_count()`` will
  no longer be separate functions in the DPDK libraries. Instead, they will
  only be present in the ``rte_ethdev.h`` header file.

* The maximum number of queues per port ``CONFIG_RTE_MAX_QUEUES_PER_PORT`` is
  increased to 1024.

* The mbuf structure was changed to support the unified packet type.
  This was already included in 2.1 under the ``CONFIG_RTE_NEXT_ABI`` #define.

* The dummy malloc library is removed. The content was moved into EAL in 2.1.

* The LPM structure is changed. The deprecated field ``mem_location`` is
  removed.

* librte_table LPM: A new parameter to hold the table name will be added to
  the LPM table parameter structure.

* librte_table hash: The key mask parameter is added to the hash table
  parameter structure for 8-byte key and 16-byte key extendable bucket
  and LRU tables.

* librte_port: Macros to access the packet meta-data stored within the packet
  buffer has been adjusted to cover the packet mbuf structure.

* librte_cfgfile: Allow longer names and values by increasing the constants
  ``CFG_NAME_LEN`` and ``CFG_VALUE_LEN`` to 64 and 256 respectively.

* vhost: a new field enabled is added to the ``vhost_virtqueue`` structure.

* vhost: a new field ``virt_qp_nb`` is added to ``virtio_net`` structure, and
  the ``virtqueue`` field is moved to the end of virtio_net structure.

* vhost: a new operation ``vring_state_changed`` is added to
  ``virtio_net_device_ops`` structure.

* vhost: a few spaces are reserved both at ``vhost_virtqueue`` and
  ``virtio_net`` structure for future extension.


Shared Library Versions
-----------------------

The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

   + libethdev.so.2
   + librte_acl.so.2
   + librte_cfgfile.so.2
     librte_cmdline.so.1
     librte_distributor.so.1
   + librte_eal.so.2
   + librte_hash.so.2
     librte_ip_frag.so.1
     librte_ivshmem.so.1
     librte_jobstats.so.1
   + librte_kni.so.2
     librte_kvargs.so.1
   + librte_lpm.so.2
   + librte_mbuf.so.2
     librte_mempool.so.1
     librte_meter.so.1
   + librte_pipeline.so.2
     librte_pmd_bond.so.1
   + librte_pmd_ring.so.2
   + librte_port.so.2
     librte_power.so.1
     librte_reorder.so.1
     librte_ring.so.1
     librte_sched.so.1
   + librte_table.so.2
     librte_timer.so.1
   + librte_vhost.so.2
