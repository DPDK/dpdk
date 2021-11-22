..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

DPDK Release 18.02
==================

New Features
------------

* **Added function to allow releasing internal EAL resources on exit.**

  During ``rte_eal_init()`` EAL allocates memory from hugepages to enable its
  core libraries to perform their tasks. The ``rte_eal_cleanup()`` function
  releases these resources, ensuring that no hugepage memory is leaked. It is
  expected that all DPDK applications call ``rte_eal_cleanup()`` before
  exiting. Not calling this function could result in leaking hugepages, leading
  to failure during initialization of secondary processes.

* **Added igb, ixgbe and i40e ethernet driver to support RSS with flow API.**

  Added support for igb, ixgbe and i40e NICs with existing RSS configuration
  using the ``rte_flow`` API.

  Also enabled queue region configuration using the ``rte_flow`` API for i40e.

* **Updated i40e driver to support PPPoE/PPPoL2TP.**

  Updated i40e PMD to support PPPoE/PPPoL2TP with PPPoE/PPPoL2TP supporting
  profiles which can be programmed by dynamic device personalization (DDP)
  process.

* **Added MAC loopback support for i40e.**

  Added MAC loopback support for i40e in order to support test tasks requested
  by users. It will setup ``Tx -> Rx`` loopback link according to the device
  configuration.

* **Added support of run time determination of number of queues per i40e VF.**

  The number of queue per VF is determined by its host PF. If the PCI address
  of an i40e PF is ``aaaa:bb.cc``, the number of queues per VF can be
  configured with EAL parameter like ``-w aaaa:bb.cc,queue-num-per-vf=n``. The
  value n can be 1, 2, 4, 8 or 16. If no such parameter is configured, the
  number of queues per VF is 4 by default.

* **Updated mlx5 driver.**

  Updated the mlx5 driver including the following changes:

  * Enabled compilation as a plugin, thus removed the mandatory dependency with rdma-core.
    With the special compilation, the rdma-core libraries will be loaded only in case
    Mellanox device is being used. For binaries creation the PMD can be enabled, still not
    requiring from every end user to install rdma-core.
  * Improved multi-segment packet performance.
  * Changed driver name to use the PCI address to be compatible with OVS-DPDK APIs.
  * Extended statistics for physical port packet/byte counters.
  * Converted to the new offloads API.
  * Supported device removal check operation.

* **Updated mlx4 driver.**

  Updated the mlx4 driver including the following changes:

  * Enabled compilation as a plugin, thus removed the mandatory dependency with rdma-core.
    With the special compilation, the rdma-core libraries will be loaded only in case
    Mellanox device is being used. For binaries creation the PMD can be enabled, still not
    requiring from every end user to install rdma-core.
  * Improved data path performance.
  * Converted to the new offloads API.
  * Supported device removal check operation.

* **Added NVGRE and UDP tunnels support in Solarflare network PMD.**

  Added support for NVGRE, VXLAN and GENEVE tunnels.

  * Added support for UDP tunnel ports configuration.
  * Added tunneled packets classification.
  * Added inner checksum offload.

* **Added AVF (Adaptive Virtual Function) net PMD.**

  Added a new net PMD called AVF (Adaptive Virtual Function), which supports
  Intel® Ethernet Adaptive Virtual Function (AVF) with features such as:

  * Basic Rx/Tx burst
  * SSE vectorized Rx/Tx burst
  * Promiscuous mode
  * MAC/VLAN offload
  * Checksum offload
  * TSO offload
  * Jumbo frame and MTU setting
  * RSS configuration
  * stats
  * Rx/Tx descriptor status
  * Link status update/event

* **Added feature supports for live migration from vhost-net to vhost-user.**

  Added feature supports for vhost-user to make live migration from vhost-net
  to vhost-user possible. The features include:

  * ``VIRTIO_F_ANY_LAYOUT``
  * ``VIRTIO_F_EVENT_IDX``
  * ``VIRTIO_NET_F_GUEST_ECN``, ``VIRTIO_NET_F_HOST_ECN``
  * ``VIRTIO_NET_F_GUEST_UFO``, ``VIRTIO_NET_F_HOST_UFO``
  * ``VIRTIO_NET_F_GSO``

  Also added ``VIRTIO_NET_F_GUEST_ANNOUNCE`` feature support in virtio PMD.
  In a scenario where the vhost backend doesn't have the ability to generate
  RARP packets, the VM running virtio PMD can still be live migrated if
  ``VIRTIO_NET_F_GUEST_ANNOUNCE`` feature is negotiated.

* **Updated the AESNI-MB PMD.**

  The AESNI-MB PMD has been updated with additional support for:

  * AES-CCM algorithm.

* **Updated the DPAA_SEC crypto driver to support rte_security.**

  Updated the ``dpaa_sec`` crypto PMD to support ``rte_security`` lookaside
  protocol offload for IPsec.

* **Added Wireless Base Band Device (bbdev) abstraction.**

  The Wireless Baseband Device library is an acceleration abstraction
  framework for 3gpp Layer 1 processing functions that provides a common
  programming interface for seamless operation on integrated or discrete
  hardware accelerators or using optimized software libraries for signal
  processing.

  The current release only supports 3GPP CRC, Turbo Coding and Rate
  Matching operations, as specified in 3GPP TS 36.212.

  See the :doc:`../prog_guide/bbdev` programmer's guide for more details.

* **Added New eventdev Ordered Packet Distribution Library (OPDL) PMD.**

  The OPDL (Ordered Packet Distribution Library) eventdev is a specific
  implementation of the eventdev API. It is particularly suited to packet
  processing workloads that have high throughput and low latency requirements.
  All packets follow the same path through the device. The order in which
  packets follow is determined by the order in which queues are set up.
  Events are left on the ring until they are transmitted. As a result packets
  do not go out of order.

  With this change, applications can use the OPDL PMD via the eventdev api.

* **Added new pipeline use case for dpdk-test-eventdev application.**

  Added a new "pipeline" use case for the ``dpdk-test-eventdev`` application.
  The pipeline case can be used to simulate various stages in a real world
  application from packet receive to transmit while maintaining the packet
  ordering. It can also be used to measure the performance of the event device
  across the stages of the pipeline.

  The pipeline use case has been made generic to work with all the event
  devices based on the capabilities.

* **Updated Eventdev sample application to support event devices based on capability.**

  Updated the Eventdev pipeline sample application to support various types of
  pipelines based on the capabilities of the attached event and ethernet
  devices. Also, renamed the application from software PMD specific
  ``eventdev_pipeline_sw_pmd`` to the more generic ``eventdev_pipeline``.

* **Added Rawdev, a generic device support library.**

  The Rawdev library provides support for integrating any generic device type with
  the DPDK framework. Generic devices are those which do not have a pre-defined
  type within DPDK, for example, ethernet, crypto, event etc.

  A set of northbound APIs have been defined which encompass a generic set of
  operations by allowing applications to interact with device using opaque
  structures/buffers. Also, southbound APIs provide a means of integrating devices
  either as part of a physical bus (PCI, FSLMC etc) or through ``vdev``.

  See the :doc:`../prog_guide/rawdev` programmer's guide for more details.

* **Added new multi-process communication channel.**

  Added a generic channel in EAL for multi-process (primary/secondary) communication.
  Consumers of this channel need to register an action with an action name to response
  a message received; the actions will be identified by the action name and executed
  in the context of a new dedicated thread for this channel. The list of new APIs:

  * ``rte_mp_register`` and ``rte_mp_unregister`` are for action (un)registration.
  * ``rte_mp_sendmsg`` is for sending a message without blocking for a response.
  * ``rte_mp_request`` is for sending a request message and will block until
    it gets a reply message which is sent from the peer by ``rte_mp_reply``.

* **Added GRO support for VxLAN-tunneled packets.**

  Added GRO support for VxLAN-tunneled packets. Supported VxLAN packets
  must contain an outer IPv4 header and inner TCP/IPv4 headers. VxLAN
  GRO doesn't check if input packets have correct checksums and doesn't
  update checksums for output packets. Additionally, it assumes the
  packets are complete (i.e., ``MF==0 && frag_off==0``), when IP
  fragmentation is possible (i.e., ``DF==0``).

* **Increased default Rx and Tx ring size in sample applications.**

  Increased the default ``RX_RING_SIZE`` and ``TX_RING_SIZE`` to 1024 entries
  in testpmd and the sample applications to give better performance in the
  general case. The user should experiment with various Rx and Tx ring sizes
  for their specific application to get best performance.

* **Added new DPDK build system using the tools "meson" and "ninja" [EXPERIMENTAL].**

  Added support for building DPDK using ``meson`` and ``ninja``, which gives
  additional features, such as automatic build-time configuration, over the
  current build system using ``make``. For instructions on how to do a DPDK build
  using the new system, see the instructions in ``doc/build-sdk-meson.txt``.

  .. note::

      This new build system support is incomplete at this point and is added
      as experimental in this release. The existing build system using ``make``
      is unaffected by these changes, and can continue to be used for this
      and subsequent releases until such time as it's deprecation is announced.


Shared Library Versions
-----------------------

The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

     librte_acl.so.2
   + librte_bbdev.so.1
     librte_bitratestats.so.2
     librte_bus_dpaa.so.1
     librte_bus_fslmc.so.1
     librte_bus_pci.so.1
     librte_bus_vdev.so.1
     librte_cfgfile.so.2
     librte_cmdline.so.2
     librte_cryptodev.so.4
     librte_distributor.so.1
     librte_eal.so.6
     librte_ethdev.so.8
     librte_eventdev.so.3
     librte_flow_classify.so.1
     librte_gro.so.1
     librte_gso.so.1
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
     librte_latencystats.so.1
     librte_lpm.so.2
     librte_mbuf.so.3
     librte_mempool.so.3
     librte_meter.so.1
     librte_metrics.so.1
     librte_net.so.1
     librte_pci.so.1
     librte_pdump.so.2
     librte_pipeline.so.3
     librte_pmd_bnxt.so.2
     librte_pmd_bond.so.2
     librte_pmd_i40e.so.2
     librte_pmd_ixgbe.so.2
     librte_pmd_ring.so.2
     librte_pmd_softnic.so.1
     librte_pmd_vhost.so.2
     librte_port.so.3
     librte_power.so.1
   + librte_rawdev.so.1
     librte_reorder.so.1
     librte_ring.so.1
     librte_sched.so.1
     librte_security.so.1
     librte_table.so.3
     librte_timer.so.1
     librte_vhost.so.3


Tested Platforms
----------------

* Intel(R) platforms with Intel(R) NICs combinations

   * CPU

     * Intel(R) Atom(TM) CPU C2758 @ 2.40GHz
     * Intel(R) Xeon(R) CPU D-1540 @ 2.00GHz
     * Intel(R) Xeon(R) CPU D-1541 @ 2.10GHz
     * Intel(R) Xeon(R) CPU E5-4667 v3 @ 2.00GHz
     * Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz
     * Intel(R) Xeon(R) CPU E5-2699 v4 @ 2.20GHz
     * Intel(R) Xeon(R) CPU E5-2695 v4 @ 2.10GHz
     * Intel(R) Xeon(R) CPU E5-2658 v2 @ 2.40GHz
     * Intel(R) Xeon(R) CPU E5-2658 v3 @ 2.20GHz
     * Intel(R) Xeon(R) Platinum 8180 CPU @ 2.50GHz

   * OS:

     * CentOS 7.2
     * Fedora 25
     * Fedora 26
     * Fedora 27
     * FreeBSD 11
     * Red Hat Enterprise Linux Server release 7.3
     * SUSE Enterprise Linux 12
     * Wind River Linux 8
     * Ubuntu 14.04
     * Ubuntu 16.04
     * Ubuntu 16.10
     * Ubuntu 17.10

   * NICs:

     * Intel(R) 82599ES 10 Gigabit Ethernet Controller

       * Firmware version: 0x61bf0001
       * Device id (pf/vf): 8086:10fb / 8086:10ed
       * Driver version: 5.2.3 (ixgbe)

     * Intel(R) Corporation Ethernet Connection X552/X557-AT 10GBASE-T

       * Firmware version: 0x800003e7
       * Device id (pf/vf): 8086:15ad / 8086:15a8
       * Driver version: 4.4.6 (ixgbe)

     * Intel(R) Ethernet Converged Network Adapter X710-DA4 (4x10G)

       * Firmware version: 6.01 0x80003221
       * Device id (pf/vf): 8086:1572 / 8086:154c
       * Driver version: 2.4.3 (i40e)

     * Intel Corporation Ethernet Connection X722 for 10GBASE-T

       * firmware-version: 6.01 0x80003221
       * Device id: 8086:37d2 / 8086:154c
       * Driver version: 2.4.3 (i40e)

     * Intel(R) Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

       * Firmware version: 6.01 0x80003221
       * Device id (pf/vf): 8086:158b / 8086:154c
       * Driver version: 2.4.3 (i40e)

     * Intel(R) Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

       * Firmware version: 6.01 0x8000321c
       * Device id (pf/vf): 8086:1583 / 8086:154c
       * Driver version: 2.4.3 (i40e)

     * Intel(R) Corporation I350 Gigabit Network Connection

       * Firmware version: 1.63, 0x80000dda
       * Device id (pf/vf): 8086:1521 / 8086:1520
       * Driver version: 5.3.0-k (igb)

* Intel(R) platforms with Mellanox(R) NICs combinations

   * CPU:

     * Intel(R) Xeon(R) CPU E5-2697A v4 @ 2.60GHz
     * Intel(R) Xeon(R) CPU E5-2697 v3 @ 2.60GHz
     * Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz
     * Intel(R) Xeon(R) CPU E5-2650 v4 @ 2.20GHz
     * Intel(R) Xeon(R) CPU E5-2640 @ 2.50GHz
     * Intel(R) Xeon(R) CPU E5-2620 v4 @ 2.10GHz

   * OS:

     * Red Hat Enterprise Linux Server release 7.5 Beta (Maipo)
     * Red Hat Enterprise Linux Server release 7.4 (Maipo)
     * Red Hat Enterprise Linux Server release 7.3 (Maipo)
     * Red Hat Enterprise Linux Server release 7.2 (Maipo)
     * Ubuntu 17.10
     * Ubuntu 16.10
     * Ubuntu 16.04

   * MLNX_OFED: 4.2-1.0.0.0
   * MLNX_OFED: 4.3-0.1.6.0

   * NICs:

     * Mellanox(R) ConnectX(R)-3 Pro 40G MCX354A-FCC_Ax (2x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1007
       * Firmware version: 2.42.5000

     * Mellanox(R) ConnectX(R)-4 10G MCX4111A-XCAT (1x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 25G MCX4111A-ACAT (1x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 40G MCX4131A-BCAT/MCX413A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 40G MCX415A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 50G MCX4131A-GCAT/MCX413A-GCAT (1x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 50G MCX414A-BCAT (2x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-GCAT/MCX416A-BCAT/MCX416A-GCAT (2x50G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-CCAT (1x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 100G MCX416A-CCAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 Lx 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.21.1000 and above

     * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1017
       * Firmware version: 16.21.1000 and above

     * Mellanox(R) ConnectX-5 Ex EN 100G MCX516A-CDAT (2x100G)

       * Host interface: PCI Express 4.0 x16
       * Device ID: 15b3:1019
       * Firmware version: 16.21.1000 and above

* ARM platforms with Mellanox(R) NICs combinations

   * CPU:

     * Qualcomm ARM 1.1 2500MHz

   * OS:

     * Ubuntu 16.04

   * MLNX_OFED: 4.2-1.0.0.0

   * NICs:

     * Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.21.1000

     * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1017
       * Firmware version: 16.21.1000
