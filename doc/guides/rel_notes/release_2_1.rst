..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2015 Intel Corporation.

DPDK Release 2.1
================


New Features
------------

* **Enabled cloning of indirect mbufs.**

  This feature removes a limitation of ``rte_pktmbuf_attach()`` which
  generated the warning: "mbuf we're attaching to must be direct".

  Now, when attaching to an indirect mbuf it is possible to:

   * Copy all relevant fields (address, length, offload, ...) as before.

   * Get the pointer to the mbuf that embeds the data buffer (direct mbuf),
     and increase the reference counter.

   When detaching the mbuf, we can now retrieve this direct mbuf as the
   pointer is determined from the buffer address.


* **Extended packet type support.**

  In previous releases mbuf packet types were indicated by 6 bits in the
  ``ol_flags``. This was not enough for some supported NICs. For example i40e
  hardware can recognize more than 150 packet types. Not being able to
  identify these additional packet types limits access to hardware offload
  capabilities

  So an extended "unified" packet type was added to support all possible
  PMDs. The 16 bit packet_type in the mbuf structure was changed to 32 bits
  and used for this purpose.

  To avoid breaking ABI compatibility, the code changes for this feature are
  enclosed in a ``RTE_NEXT_ABI`` ifdef. This is enabled by default but can be
  turned off for ABI compatibility with DPDK R2.0.


* **Reworked memzone to be allocated by malloc and also support freeing.**

  In the memory hierarchy, memsegs are groups of physically contiguous
  hugepages, memzones are slices of memsegs, and malloc slices memzones
  into smaller memory chunks.

  This feature modifies ``malloc()`` so it partitions memsegs instead of
  memzones. Now  memzones allocate their memory from the malloc heap.

  Backward compatibility with API and ABI are maintained.

  This allow memzones, and any other structure based on memzones, for example
  mempools, to be freed. Currently only the API from freeing memzones is
  supported.


* **Interrupt mode PMD.**

  This feature introduces a low-latency one-shot RX interrupt into DPDK. It
  also adds a polling and interrupt mode switch control example.

  DPDK userspace interrupt notification and handling mechanism is based on
  UIO/VFIO with the following limitations:

  * Per queue RX interrupt events are only allowed in VFIO which supports
    multiple MSI-X vectors.
  * In UIO, the RX interrupt shares the same vector with other
    interrupts. When the RX interrupt and LSC interrupt are both enabled, only
    the former is available.
  * RX interrupt is only implemented for the linuxapp target.
  * The feature is only currently enabled for tow PMDs: ixgbe and igb.


* **Packet Framework enhancements.**

  Several enhancements were made to the Packet Framework:

  * A new configuration file syntax has been introduced for IP pipeline
    applications. Parsing of the configuration file is changed.
  * Implementation of the IP pipeline application is modified to make it more
    structured and user friendly.
  * Implementation of the command line interface (CLI) for each pipeline type
    has been moved to the separate compilation unit. Syntax of pipeline CLI
    commands has been changed.
  * Initialization of IP pipeline is modified to match the new parameters
    structure.
  * New implementation of pass-through pipeline, firewall pipeline, routing
    pipeline, and flow classification has been added.
  * Master pipeline with CLI interface has been added.
  * Added extended documentation of the IP Pipeline.


* **Added API for IEEE1588 timestamping.**

  This feature adds an ethdev API to enable, disable and read IEEE1588/802.1AS
  PTP timestamps from devices that support it. The following functions were
  added:

  * ``rte_eth_timesync_enable()``
  * ``rte_eth_timesync_disable()``
  * ``rte_eth_timesync_read_rx_timestamp()``
  * ``rte_eth_timesync_read_tx_timestamp()``

  The "ieee1588" forwarding mode in testpmd was also refactored to demonstrate
  the new API.


* **Added multicast address filtering.**

  Added multicast address filtering via a new ethdev function
  ``set_mc_addr_list()``.

  This overcomes a limitation in previous releases where the receipt of
  multicast packets on a given port could only be enabled by invoking the
  ``rte_eth_allmulticast_enable()`` function. This method did not work for VFs
  in SR-IOV architectures when the host PF driver does not allow these
  operation on VFs. In such cases, joined multicast addresses had to be added
  individually to the set of multicast addresses that are filtered by the [VF]
  port.


* **Added Flow Director extensions.**

  Several Flow Director extensions were added such as:

  * Support for RSS and Flow Director hashes in vector RX.
  * Added Flow Director for L2 payload.


* **Added RSS hash key size query per port.**

  This feature supports querying the RSS hash key size of each port. A new
  field ``hash_key_size`` has been added in the ``rte_eth_dev_info`` struct
  for storing hash key size in bytes.


* **Added userspace ethtool support.**

  Added userspace ethtool support to provide a familiar interface for
  applications that manage devices via kernel-space ``ethtool_op`` and
  ``net_device_op``.

  The initial implementation focuses on operations that can be implemented
  through existing ``netdev`` APIs. More operations will be supported in later
  releases.


* **Updated the ixgbe base driver.**

  The ixgbe base driver was updated with several changes including the
  following:

  * Added a new 82599 device id.
  * Added new X550 PHY ids.
  * Added SFP+ dual-speed support.
  * Added wait helper for X550 IOSF accesses.
  * Added X550em features.
  * Added X557 PHY LEDs support.
  * Commands for flow director.
  * Issue firmware command when resetting X550em.

  See the git log for full details of the ixgbe/base changes.


* **Added additional hotplug support.**

  Port hotplug support was added to the following PMDs:

  * e1000/igb.
  * ixgbe.
  * i40e.
  * fm10k.
  * ring.
  * bonding.
  * virtio.

  Port hotplug support was added to BSD.


* **Added ixgbe LRO support.**

  Added LRO support for x540 and 82599 devices.


* **Added extended statistics for ixgbe.**

  Implemented ``xstats_get()`` and ``xstats_reset()`` in dev_ops for
  ixgbe to expose detailed error statistics to DPDK applications.

  These will be implemented for other PMDs in later releases.


* **Added proc_info application.**

  Created a new ``proc_info`` application, by refactoring the existing
  ``dump_cfg`` application, to demonstrate the usage of retrieving statistics,
  and the new extended statistics (see above), for DPDK interfaces.


* **Updated the i40e base driver.**

  The i40e base driver was updated with several changes including the
  following:

  *  Support for building both PF and VF driver together.
  *  Support for CEE DCBX on recent firmware versions.
  *  Replacement of ``i40e_debug_read_register()``.
  *  Rework of ``i40e_hmc_get_object_va``.
  *  Update of shadow RAM read/write functions.
  *  Enhancement of polling NVM semaphore.
  *  Enhancements on adminq init and sending asq command.
  *  Update of get/set LED functions.
  *  Addition of AOC phy types to case statement in get_media_type.
  *  Support for iSCSI capability.
  *  Setting of FLAG_RD when sending driver version to FW.

  See the git log for full details of the i40e/base changes.


* **Added support for port mirroring in i40e.**

  Enabled mirror functionality in the i40e driver.


* **Added support for i40e double VLAN, QinQ, stripping and insertion.**

  Added support to the i40e driver for offloading double VLAN (QinQ) tags to
  the mbuf header, and inserting double vlan tags by hardware to the packets
  to be transmitted.  Added a new field ``vlan_tci_outer`` in the ``rte_mbuf``
  struct, and new flags in ``ol_flags`` to support this feature.



* **Added fm10k promiscuous mode support.**

  Added support for promiscuous/allmulticast enable and disable in the fm10k PF
  function. VF is not supported yet.


* **Added fm10k jumbo frame support.**

  Added support for jumbo frame less than 15K in both VF and PF functions in the
  fm10k pmd.


* **Added fm10k mac vlan filtering support.**

  Added support for the fm10k MAC filter, only available in PF. Updated the
  VLAN filter to add/delete one static entry in the MAC table for each
  combination of VLAN and MAC address.


* **Added support for the Broadcom bnx2x driver.**

  Added support for the Broadcom NetXtreme II bnx2x driver.
  It is supported only on Linux 64-bit and disabled by default.


* **Added support for the Chelsio CXGBE driver.**

  Added support for the CXGBE Poll Mode Driver for the Chelsio Terminator 5
  series of 10G/40G adapters.


* **Enhanced support for Mellanox ConnectX-3 driver (mlx4).**

  *  Support Mellanox OFED 3.0.
  *  Improved performance for both RX and TX operations.
  *  Better link status information.
  *  Outer L3/L4 checksum offload support.
  *  Inner L3/L4 checksum offload support for VXLAN.


* **Enabled VMXNET3 vlan filtering.**

  Added support for the VLAN filter functionality of the VMXNET3 interface.


* **Added support for vhost live migration.**

  Added support to allow live migration of vhost. Without this feature, qemu
  will report the following error: "migrate: Migration disabled: vhost lacks
  VHOST_F_LOG_ALL feature".


* **Added support for pcap jumbo frames.**

  Extended the PCAP PMD to support jumbo frames for RX and TX.


* **Added support for the TILE-Gx architecture.**

  Added support for the EZchip TILE-Gx family of SoCs.


* **Added hardware memory transactions/lock elision for x86.**

  Added the use of hardware memory transactions (HTM) on fast-path for rwlock
  and spinlock (a.k.a. lock elision). The methods are implemented for x86
  using Restricted Transactional Memory instructions (Intel(r) Transactional
  Synchronization Extensions). The implementation fall-backs to the normal
  rwlock if HTM is not available or memory transactions fail. This is not a
  replacement for all rwlock usages since not all critical sections protected
  by locks are friendly to HTM. For example, an attempt to perform a HW I/O
  operation inside a hardware memory transaction always aborts the transaction
  since the CPU is not able to roll-back should the transaction
  fail. Therefore, hardware transactional locks are not advised to be used
  around ``rte_eth_rx_burst()`` and ``rte_eth_tx_burst()`` calls.


* **Updated Jenkins Hash function**

  Updated the version of the Jenkins Hash (jhash) function used in DPDK from
  the 1996 version to the 2006 version. This gives up to 35% better
  performance, compared to the original one.

  Note, the hashes generated by the updated version differ from the hashes
  generated by the previous version.


* **Added software implementation of the Toeplitz RSS hash**

  Added a software implementation of the Toeplitz hash function used by RSS. It
  can be used either for packet distribution on a single queue NIC or for
  simulating RSS computation on a specific NIC (for example after GRE header
  de-encapsulation).


* **Replaced the existing hash library with a Cuckoo hash implementation.**

  Replaced the existing hash library with another approach, using the Cuckoo
  Hash method to resolve collisions (open addressing). This method pushes
  items from a full bucket when a new entry must be added to it, storing the
  evicted entry in an alternative location, using a secondary hash function.

  This gives the user the ability to store more entries when a bucket is full,
  in comparison with the previous implementation.

  The API has not been changed, although new fields have been added in the
  ``rte_hash`` structure, which has been changed to internal use only.

  The main change when creating a new table is that the number of entries per
  bucket is now fixed, so its parameter is ignored now (it is still there to
  maintain the same parameters structure).

  Also, the maximum burst size in lookup_burst function hash been increased to
  64, to improve performance.


* **Optimized KNI RX burst size computation.**

  Optimized KNI RX burst size computation by avoiding checking how many
  entries are in ``kni->rx_q`` prior to actually pulling them from the fifo.


* **Added KNI multicast.**

  Enabled adding multicast addresses to KNI interfaces by adding an empty
  callback for ``set_rx_mode`` (typically used for setting up hardware) so
  that the ioctl succeeds. This is the same thing as the Linux tap interface
  does.


* **Added cmdline polling mode.**

  Added the ability to process console input in the same thread as packet
  processing by using the ``poll()`` function.

* **Added VXLAN Tunnel End point sample application.**

  Added a Tunnel End point (TEP) sample application that simulates a VXLAN
  Tunnel Endpoint (VTEP) termination in DPDK. It is used to demonstrate the
  offload and filtering capabilities of Intel XL710 10/40 GbE NICsfor VXLAN
  packets.


* **Enabled combining of the ``-m`` and ``--no-huge`` EAL options.**

  Added option to allow combining of the ``-m`` and ``--no-huge`` EAL command
  line options.

  This allows user application to run as non-root but with higher memory
  allocations, and removes a constraint on ``--no-huge`` mode being limited to
  64M.



Resolved Issues
---------------

* **acl: Fix ambiguity between test rules.**

  Some test rules had equal priority for the same category. That could cause
  an ambiguity in building the trie and test results.


* **acl: Fix invalid rule wildness calculation for bitmask field type.**


* **acl: Fix matching rule.**


* **acl: Fix unneeded trie splitting for subset of rules.**

  When rebuilding a trie for limited rule-set, don't try to split the rule-set
  even further.


* **app/testpmd: Fix crash when port id out of bound.**

  Fixed issues in testpmd where using a port greater than 32 would cause a seg
  fault.

  Fixes: edab33b1c01d ("app/testpmd: support port hotplug")


* **app/testpmd: Fix reply to a multicast ICMP request.**

  Set the IP source and destination addresses in the IP header of the ICMP
  reply.


* **app/testpmd: fix MAC address in ARP reply.**

  Fixed issue where in the ``icmpecho`` forwarding mode, ARP replies from
  testpmd contain invalid zero-filled MAC addresses.

  Fixes: 31db4d38de72 ("net: change arp header struct declaration")


* **app/testpmd: fix default flow control values.**

  Fixes: 422a20a4e62d ("app/testpmd: fix uninitialized flow control variables")


* **bonding: Fix crash when stopping inactive slave.**


* **bonding: Fix device initialization error handling.**


* **bonding: Fix initial link status of slave.**

  On Fortville NIC, link status change interrupt callback was not executed
  when slave in bonding was (re-)started.


* **bonding: Fix socket id for LACP slave.**

  Fixes: 46fb43683679 ("bond: add mode 4")


* **bonding: Fix device initialization error handling.**


* **cmdline: Fix small memory leak.**

  A function in ``cmdline.c`` had a return that did not free the buf properly.


* **config: Enable same drivers options for Linux and BSD.**

  Enabled vector ixgbe and i40e bulk alloc for BSD as it is already done for
  Linux.

  Fixes: 304caba12643 ("config: fix bsd options")
  Fixes: 0ff3324da2eb ("ixgbe: rework vector pmd following mbuf changes")


* **devargs: Fix crash on failure.**

  This problem occurred when passing an invalid PCI id to the blacklist API in
  devargs.


* **e1000/i40e: Fix descriptor done flag with odd address.**


* **e1000/igb: fix ieee1588 timestamping initialization.**

  Fixed issue with e1000 ieee1588 timestamp initialization. On initialization
  the IEEE1588 functions read the system time to set their timestamp. However,
  on some 1G NICs, for example, i350, system time is disabled by default and
  the IEEE1588 timestamp was always 0.


* **eal/bsd: Fix inappropriate header guards.**


* **eal/bsd: Fix virtio on FreeBSD.**

  Closing the ``/dev/io`` fd caused a SIGBUS in inb/outb instructions as the
  process lost the IOPL privileges once the fd is closed.

  Fixes: 8a312224bcde ("eal/bsd: fix fd leak")


* **eal/linux: Fix comments on vfio MSI.**


* **eal/linux: Fix irq handling with igb_uio.**

  Fixed an issue where the introduction of ``uio_pci_generic`` broke
  interrupt handling with igb_uio.

  Fixes: c112df6875a5 ("eal/linux: toggle interrupt for uio_pci_generic")


* **eal/linux: Fix numa node detection.**


* **eal/linux: Fix socket value for undetermined numa node.**

  Sets zero as the default value of pci device numa_node if the socket could
  not be determined. This provides the same default value as FreeBSD which has
  no NUMA support, and makes the return value of ``rte_eth_dev_socket_id()``
  be consistent with the API description.


* **eal/ppc: Fix cpu cycle count for little endian.**

  On IBM POWER8 PPC64 little endian architecture, the definition of tsc union
  will be different. This fix enables the right output from ``rte_rdtsc()``.


* **ethdev: Fix check of threshold for TX freeing.**

  Fixed issue where the parameter to ``tx_free_thresh`` was not consistent
  between the drivers.


* **ethdev: Fix crash if malloc of user callback fails.**

  If ``rte_zmalloc()`` failed in ``rte_eth_dev_callback_register`` then the
  NULL pointer would be dereferenced.


* **ethdev: Fix illegal port access.**

  To obtain a detachable flag, ``pci_drv`` is accessed in
  ``rte_eth_dev_is_detachable()``. However ``pci_drv`` is only valid if port
  is enabled. Fixed by checking ``rte_eth_dev_is_valid_port()`` first.


* **ethdev: Make tables const.**


* **ethdev: Rename and extend the mirror type.**


* **examples/distributor: Fix debug macro.**

  The macro to turn on additional debug output when the app was compiled with
  ``-DDEBUG`` was broken.

  Fixes: 07db4a975094 ("examples/distributor: new sample app")


* **examples/kni: Fix crash on exit.**


* **examples/vhost: Fix build with debug enabled.**

  Fixes: 72ec8d77ac68 ("examples/vhost: rework duplicated code")


* **fm10k: Fix RETA table initialization.**

  The fm10k driver has 128 RETA entries in 32 registers, but it only
  initialized the first 32 when doing multiple RX queue configurations. This
  fix initializes all 128 entries.


* **fm10k: Fix RX buffer size.**


* **fm10k: Fix TX multi-segment frame.**


* **fm10k: Fix TX queue cleaning after start error.**


* **fm10k: Fix Tx queue cleaning after start error.**


* **fm10k: Fix default mac/vlan in switch.**


* **fm10k: Fix interrupt fault handling.**


* **fm10k: Fix jumbo frame issue.**


* **fm10k: Fix mac/vlan filtering.**


* **fm10k: Fix maximum VF number.**


* **fm10k: Fix maximum queue number for VF.**

  Both PF and VF shared code in function ``fm10k_stats_get()``. The function
  worked with PF, but had problems with VF since it has less queues than PF.

  Fixes: a6061d9e7075 ("fm10k: register PF driver")


* **fm10k: Fix queue disabling.**


* **fm10k: Fix switch synchronization.**


* **i40e/base: Fix error handling of NVM state update.**


* **i40e/base: Fix hardware port number for pass-through.**


* **i40e/base: Rework virtual address retrieval for lan queue.**


* **i40e/base: Update LED blinking.**


* **i40e/base: Workaround for PHY type with firmware < 4.4.**


* **i40e: Disable setting of PHY configuration.**


* **i40e: Fix SCTP flow director.**


* **i40e: Fix check of descriptor done flag.**

  Fixes: 4861cde46116 ("i40e: new poll mode driver")
  Fixes: 05999aab4ca6 ("i40e: add or delete flow director")


* **i40e: Fix condition to get VMDQ info.**


* **i40e: Fix registers access from big endian CPU.**


* **i40evf: Clear command when error occurs.**


* **i40evf: Fix RSS with less RX queues than TX queues.**


* **i40evf: Fix crash when setup TX queues.**


* **i40evf: Fix jumbo frame support.**


* **i40evf: Fix offload capability flags.**

  Added checksum offload capability flags which have already been supported
  for a long time.


* **ivshmem: Fix crash in corner case.**

  Fixed issues where depending on the configured segments it was possible to
  hit a segmentation fault as a result of decrementing an unsigned index with
  value 0.


  Fixes: 40b966a211ab ("ivshmem: library changes for mmaping using ivshmem")


* **ixgbe/base: Fix SFP probing.**


* **ixgbe/base: Fix TX pending clearing.**


* **ixgbe/base: Fix X550 CS4227 address.**


* **ixgbe/base: Fix X550 PCIe master disabling.**


* **ixgbe/base: Fix X550 check.**


* **ixgbe/base: Fix X550 init early return.**


* **ixgbe/base: Fix X550 link speed.**


* **ixgbe/base: Fix X550em CS4227 speed mode.**


* **ixgbe/base: Fix X550em SFP+ link stability.**


* **ixgbe/base: Fix X550em UniPHY link configuration.**


* **ixgbe/base: Fix X550em flow control for KR backplane.**


* **ixgbe/base: Fix X550em flow control to be KR only.**


* **ixgbe/base: Fix X550em link setup without SFP.**


* **ixgbe/base: Fix X550em mux after MAC reset.**

  Fixes: d2e72774e58c ("ixgbe/base: support X550")


* **ixgbe/base: Fix bus type overwrite.**


* **ixgbe/base: Fix init handling of X550em link down.**


* **ixgbe/base: Fix lan id before first i2c access.**


* **ixgbe/base: Fix mac type checks.**


* **ixgbe/base: Fix tunneled UDP and TCP frames in flow director.**


* **ixgbe: Check mbuf refcnt when clearing a ring.**

  The function to clear the TX ring when a port was being closed, e.g. on exit
  in testpmd, was not checking the mbuf refcnt before freeing it. Since the
  function in the vector driver to clear the ring after TX does not setting
  the pointer to NULL post-free, this caused crashes if mbuf debugging was
  turned on.


* **ixgbe: Fix RX with buffer address not word aligned.**

  Niantic HW expects the Header Buffer Address in the RXD must be word
  aligned.


* **ixgbe: Fix RX with buffer address not word aligned.**


* **ixgbe: Fix Rx queue reset.**

  Fix to reset vector related RX queue fields to their initial values.

  Fixes: c95584dc2b18 ("ixgbe: new vectorized functions for Rx/Tx")


* **ixgbe: Fix TSO in IPv6.**

  When TSO was used with IPv6, the generated frames were incorrect. The L4
  frame was OK, but the length field of IPv6 header was not populated
  correctly.


* **ixgbe: Fix X550 flow director check.**


* **ixgbe: Fix check for split packets.**

  The check for split packets to be reassembled in the vector ixgbe PMD was
  incorrectly only checking the first 16 elements of the array instead of
  all 32.

  Fixes: cf4b4708a88a ("ixgbe: improve slow-path perf with vector scattered Rx")


* **ixgbe: Fix data access on big endian cpu.**


* **ixgbe: Fix flow director flexbytes offset.**


  Fixes: d54a9888267c ("ixgbe: support flexpayload configuration of flow director")


* **ixgbe: Fix number of segments with vector scattered Rx.**

  Fixes: cf4b4708a88a (ixgbe: improve slow-path perf with vector scattered Rx)


* **ixgbe: Fix offload config option name.**

  The RX_OLFLAGS option was renamed from DISABLE to ENABLE in the driver code
  and Linux config. It is now renamed also in the BSD config and
  documentation.

  Fixes: 359f106a69a9 ("ixgbe: prefer enabling olflags rather than not disabling")


* **ixgbe: Fix release queue mbufs.**

  The calculations of what mbufs were valid in the RX and TX queues were
  incorrect when freeing the mbufs for the vector PMD. This led to crashes due
  to invalid reference counts when mbuf debugging was turned on, and possibly
  other more subtle problems (such as mbufs being freed when in use) in other
  cases.


  Fixes: c95584dc2b18 ("ixgbe: new vectorized functions for Rx/Tx")


* **ixgbe: Move PMD specific fields out of base driver.**

  Move ``rx_bulk_alloc_allowed`` and ``rx_vec_allowed`` from ``ixgbe_hw`` to
  ``ixgbe_adapter``.

  Fixes: 01fa1d6215fa ("ixgbe: unify Rx setup")


* **ixgbe: Rename TX queue release function.**


* **ixgbevf: Fix RX function selection.**

  The logic to select ixgbe the VF RX function is different than the PF.


* **ixgbevf: Fix link status for PF up/down events.**


* **kni: Fix RX loop limit.**

  Loop processing packets dequeued from rx_q was using the number of packets
  requested, not how many it actually received.


* **kni: Fix ioctl in containers, like Docker.**


* **kni: Fix multicast ioctl handling.**


* **log: Fix crash after log_history dump.**


* **lpm: Fix big endian support.**


* **lpm: Fix depth small entry add.**


* **mbuf: Fix cloning with private mbuf data.**

  Added a new ``priv_size`` field in mbuf structure that should be initialized
  at mbuf pool creation. This field contains the size of the application
  private data in mbufs.

  Introduced new static inline functions ``rte_mbuf_from_indirect()`` and
  ``rte_mbuf_to_baddr()`` to replace the existing macros, which take the
  private size into account when attaching and detaching mbufs.


* **mbuf: Fix data room size calculation in pool init.**

  Deduct the mbuf data room size from ``mempool->elt_size`` and ``priv_size``,
  instead of using an hardcoded value that is not related to the real buffer
  size.

  To use ``rte_pktmbuf_pool_init()``, the user can either:

  * Give a NULL parameter to rte_pktmbuf_pool_init(): in this case, the
    private size is assumed to be 0, and the room size is ``mp->elt_size`` -
    ``sizeof(struct rte_mbuf)``.
  * Give the ``rte_pktmbuf_pool_private`` filled with appropriate
    data_room_size and priv_size values.


* **mbuf: Fix init when private size is not zero.**

  Allow the user to use the default ``rte_pktmbuf_init()`` function even if
  the mbuf private size is not 0.


* **mempool: Add structure for object headers.**

  Each object stored in mempools are prefixed by a header, allowing for
  instance to retrieve the mempool pointer from the object. When debug is
  enabled, a cookie is also added in this header that helps to detect
  corruptions and double-frees.

  Introduced a structure that materializes the content of this header,
  and will simplify future patches adding things in this header.


* **mempool: Fix pages computation to determine number of objects.**


* **mempool: Fix returned value after counting objects.**

  Fixes: 148f963fb532 ("xen: core library changes")


* **mlx4: Avoid requesting TX completion events to improve performance.**

  Instead of requesting a completion event for each TX burst, request it on a
  fixed schedule once every MLX4_PMD_TX_PER_COMP_REQ (currently 64) packets to
  improve performance.


* **mlx4: Fix compilation as a shared library and on 32 bit platforms.**


* **mlx4: Fix possible crash on scattered mbuf allocation failure.**

  Fixes issue where failing to allocate a segment, ``mlx4_rx_burst_sp()``
  could call ``rte_pktmbuf_free()`` on an incomplete scattered mbuf whose next
  pointer in the last segment is not set.


* **mlx4: Fix support for multiple vlan filters.**

  This fixes the "Multiple RX VLAN filters can be configured, but only the
  first one works" bug.


* **pcap: Fix storage of name and type in queues.**

  pcap_rx_queue/pcap_tx_queue should store it's own copy of name/type values,
  not the pointer to temporary allocated space.


* **pci: Fix memory leaks and needless increment of map address.**


* **pci: Fix uio mapping differences between linux and bsd.**


* **port: Fix unaligned access to metadata.**

  Fix RTE_MBUF_METADATA macros to allow for unaligned accesses to meta-data
  fields.


* **ring: Fix return of new port id on creation.**


* **timer: Fix race condition.**

  Eliminate problematic race condition in ``rte_timer_manage()`` that can lead
  to corruption of per-lcore pending-lists (implemented as skip-lists).


* **vfio: Fix overflow of BAR region offset and size.**

  Fixes: 90a1633b2347 ("eal/Linux: allow to map BARs with MSI-X tables")


* **vhost: Fix enqueue/dequeue to handle chained vring descriptors.**


* **vhost: Fix race for connection fd.**


* **vhost: Fix virtio freeze due to missed interrupt.**


* **virtio: Fix crash if CQ is not negotiated.**

  Fix NULL dereference if virtio control queue is not negotiated.


* **virtio: Fix ring size negotiation.**

  Negotiate the virtio ring size. The host may allow for very large rings but
  application may only want a smaller ring. Conversely, if the number of
  descriptors requested exceeds the virtio host queue size, then just silently
  use the smaller host size.

  This fixes issues with virtio in non-QEMU environments. For example Google
  Compute Engine allows up to 16K elements in ring.


* **vmxnet3: Fix link state handling.**


Known Issues
------------

* When running the ``vmdq`` sample or ``vhost`` sample applications with the
  Intel(R) XL710 (i40e) NIC, the configuration option
  ``CONFIG_RTE_MAX_QUEUES_PER_PORT`` should be increased from 256 to 1024.


* VM power manager may not work on systems with more than 64 cores.


API Changes
-----------

* The order that user supplied RX and TX callbacks are called in has been
  changed to the order that they were added (fifo) in line with end-user
  expectations. The previous calling order was the reverse of this (lifo) and
  was counter intuitive for users. The actual API is unchanged.


ABI Changes
-----------

* The ``rte_hash`` structure has been changed to internal use only.
