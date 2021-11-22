..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2016 The DPDK contributors

DPDK Release 16.04
==================

New Features
------------

* **Added function to check primary process state.**

  A new function ``rte_eal_primary_proc_alive()`` has been added
  to allow the user to detect if a primary process is running.
  Use cases for this feature include fault detection, and monitoring
  using secondary processes.

* **Enabled bulk allocation of mbufs.**

  A new function ``rte_pktmbuf_alloc_bulk()`` has been added to allow the user
  to bulk allocate mbufs.

* **Added device link speed capabilities.**

  The structure ``rte_eth_dev_info`` now has a ``speed_capa`` bitmap, which
  allows the application to determine the supported speeds of each device.

* **Added bitmap of link speeds to advertise.**

  Added a feature to allow the definition of a set of advertised speeds for auto-negotiation,
  explicitly disabling link auto-negotiation (single speed)
  and full auto-negotiation.

* **Added new poll-mode driver for Amazon Elastic Network Adapters (ENA).**

  The driver operates for a variety of ENA adapters through feature negotiation
  with the adapter and upgradable commands set.
  The ENA driver handles PCI Physical and Virtual ENA functions.

* **Restored vmxnet3 TX data ring.**

  TX data ring has been shown to improve small packet forwarding performance
  on the vSphere environment.

* **Added vmxnet3 TX L4 checksum offload.**

  Added support for TCP/UDP checksum offload to vmxnet3.

* **Added vmxnet3 TSO support.**

  Added support for TSO to vmxnet3.

* **Added vmxnet3 support for jumbo frames.**

  Added support for linking multi-segment buffers together to
  handle Jumbo packets.

* **Enabled Virtio 1.0 support.**

  Enabled Virtio 1.0 support for Virtio PMD.

* **Supported Virtio for ARM.**

  Enabled Virtio support for ARMv7/v8. Tested for ARM64.
  Virtio for ARM supports VFIO-noiommu mode only.
  Virtio can work with other non-x86 architectures as well, like PowerPC.

* **Supported Virtio offload in vhost-user.**

  Added the offload and negotiation of checksum and TSO between vhost-user and
  vanilla Linux Virtio guest.

* **Added vhost-user live migration support.**

* **Added vhost driver.**

  Added a virtual PMD that wraps ``librte_vhost``.

* **Added multicast promiscuous mode support on VF for ixgbe.**

  Added multicast promiscuous mode support for the ixgbe VF driver so all VFs
  can receive the multicast packets.

  Please note if you want to use this promiscuous mode, you need both PF and VF
  driver to support it. The reason is that this VF feature is configured in the PF.
  If you use kernel PF driver and the dpdk VF driver, make sure the kernel PF driver supports
  VF multicast promiscuous mode. If you use dpdk PF and  dpdk VF ensure the PF
  driver is the same version as the VF.

* **Added support for E-tag on X550.**

  E-tag is defined in `802.1BR - Bridge Port Extension <http://www.ieee802.org/1/pages/802.1br.html>`_.

  This feature is for the VF, but the settings are on the PF. It means
  the CLIs should be used on the PF, but some of their effects will be shown on the VF.
  The forwarding of E-tag packets based on GRP and E-CID_base will have an effect
  on the PF. Theoretically, the E-tag packets can be forwarded to any pool/queue
  but normally we'd like to forward the packets to the pools/queues belonging
  to the VFs. And E-tag insertion and stripping will have an effect on VFs. When
  a VF receives E-tag packets it should strip the E-tag. When the VF transmits
  packets, it should insert the E-tag. Both actions can be offloaded.

  When we want to use this E-tag support feature, the forwarding should be
  enabled to forward the packets received by the PF to the indicated VFs. And insertion
  and stripping should be enabled for VFs to offload the effort to hardware.

  Features added:

  * Support E-tag offloading of insertion and stripping.
  * Support Forwarding E-tag packets to pools based on
    GRP and E-CID_base.

* **Added support for VxLAN and NVGRE checksum off-load on X550.**

  * Added support for VxLAN and NVGRE RX/TX checksum off-load on
    X550. RX/TX checksum off-load is provided on both inner and
    outer IP header and TCP header.
  * Added functions to support VxLAN port configuration. The
    default VxLAN port number is 4789 but this can be updated
    programmatically.

* **Added support for new X550EM_a devices.**

  Added support for new X550EM_a devices and their MAC types, X550EM_a and X550EM_a_vf.
  Updated the relevant PMD to use the new devices and MAC types.

* **Added x550em_x V2 device support.**

  Added support for x550em_x V2 device. Only x550em_x V1 was supported before.
  A mask for V1 and V2 is defined and used to support both.

* **Supported link speed auto-negotiation on X550EM_X**

  Normally the auto-negotiation is supported by firmware and software doesn't care about
  it. But on x550em_x, firmware doesn't support auto-negotiation. As the ports of x550em_x
  are 10GbE, if we connect the port with a peer which is 1GbE, the link will always
  be down.
  We added the support for auto-negotiation by software to avoid this link down issue.

* **Added software-firmware sync on X550EM_a.**

  Added support for software-firmware sync for resource sharing.
  Use the PHY token, shared between software-firmware for PHY access on X550EM_a.

* **Updated the i40e base driver.**

  The i40e base driver was updated with changes including the
  following:

  * Use RX control AQ commands to read/write RX control registers.
  * Add new X722 device IDs, and removed X710 one was never used.
  * Expose registers for HASH/FD input set configuring.

* **Enabled PCI extended tag for i40e.**

  Enabled extended tag for i40e by checking and writing corresponding PCI config
  space bytes, to boost the performance.
  The legacy method of reading/writing sysfile supported by kernel module igb_uio
  is now deprecated.

* **Added i40e support for setting mac addresses.**

* **Added dump of i40e registers and EEPROM.**

* **Supported ether type setting of single and double VLAN for i40e**

* **Added VMDQ DCB mode in i40e.**

  Added support for DCB in VMDQ mode to i40e driver.

* **Added i40e VEB switching support.**

* **Added Flow director enhancements in i40e.**

* **Added PF reset event reporting in i40e VF driver.**

* **Added fm10k RX interrupt support.**

* **Optimized fm10k TX.**

  Optimized fm10k TX by freeing multiple mbufs at a time.

* **Handled error flags in fm10k vector RX.**

  Parse error flags in RX descriptor and set error bits in mbuf with vector instructions.

* **Added fm10k FTAG based forwarding support.**

* **Added mlx5 flow director support.**

  Added flow director support (``RTE_FDIR_MODE_PERFECT`` and
  ``RTE_FDIR_MODE_PERFECT_MAC_VLAN``).

  Only available with Mellanox OFED >= 3.2.

* **Added mlx5 RX VLAN stripping support.**

  Added support for RX VLAN stripping.

  Only available with Mellanox OFED >= 3.2.

* **Added mlx5 link up/down callbacks.**

  Implemented callbacks to bring link up and down.

* **Added mlx5 support for operation in secondary processes.**

  Implemented TX support in secondary processes (like mlx4).

* **Added mlx5 RX CRC stripping configuration.**

  Until now, CRC was always stripped. It can now be configured.

  Only available with Mellanox OFED >= 3.2.

* **Added mlx5 optional packet padding by HW.**

  Added an option to make PCI bus transactions rounded to a multiple of a
  cache line size for better alignment.

  Only available with Mellanox OFED >= 3.2.

* **Added mlx5 TX VLAN insertion support.**

  Added support for TX VLAN insertion.

  Only available with Mellanox OFED >= 3.2.

* **Changed szedata2 driver type from vdev to pdev.**

  Previously szedata2 device had to be added by ``--vdev`` option.
  Now szedata2 PMD recognizes the device automatically during EAL
  initialization.

* **Added szedata2 functions for setting link up/down.**

* **Added szedata2 promiscuous and allmulticast modes.**

* **Added af_packet dynamic removal function.**

  An af_packet device can now be detached using the API, like other PMD devices.

* **Increased number of next hops for LPM IPv4 to 2^24.**

  The ``next_hop`` field has been extended from 8 bits to 24 bits for IPv4.

* **Added support of SNOW 3G (UEA2 and UIA2) for Intel Quick Assist devices.**

  Enabled support for the SNOW 3G wireless algorithm for Intel Quick Assist devices.
  Support for cipher-only and  hash-only is also provided
  along with algorithm-chaining operations.

* **Added SNOW3G SW PMD.**

  A new Crypto PMD has been added, which provides SNOW 3G UEA2 ciphering
  and SNOW3G UIA2 hashing.

* **Added AES GCM PMD.**

  Added new Crypto PMD to support AES-GCM authenticated encryption and
  authenticated decryption in software.

* **Added NULL Crypto PMD**

  Added new Crypto PMD to support null crypto operations in software.

* **Improved IP Pipeline Application.**

  The following features have been added to ip_pipeline application;

  * Added CPU utilization measurement and idle cycle rate computation.
  * Added link identification support through existing port-mask option or by
    specifying PCI device in every LINK section in the configuration file.
  * Added load balancing support in passthrough pipeline.

* **Added IPsec security gateway example.**

  Added a new application implementing an IPsec Security Gateway.


Resolved Issues
---------------

Drivers
~~~~~~~

* **ethdev: Fixed overflow for 100Gbps.**

  100Gbps in Mbps (100000) was exceeding the 16-bit max value of ``link_speed``
  in ``rte_eth_link``.

* **ethdev: Fixed byte order consistency between fdir flow and mask.**

  Fixed issue in ethdev library where the structure for setting
  fdir's mask and flow entry was not consistent in byte ordering.

* **cxgbe: Fixed crash due to incorrect size allocated for RSS table.**

  Fixed a segfault that occurs when accessing part of port 0's RSS
  table that gets overwritten by subsequent port 1's part of the RSS
  table due to incorrect size allocated for each entry in the table.

* **cxgbe: Fixed setting wrong device MTU.**

  Fixed an incorrect device MTU being set due to the Ethernet header and
  CRC lengths being added twice.

* **ixgbe: Fixed zeroed VF mac address.**

  Resolved an issue where the VF MAC address is zeroed out in cases where the VF
  driver is loaded while the PF interface is down.
  The solution is to only set it when we get an ACK from the PF.

* **ixgbe: Fixed setting flow director flag twice.**

  Resolved an issue where packets were being dropped when switching to perfect
  filters mode.

* **ixgbe: Set MDIO speed after MAC reset.**

  The MDIO clock speed must be reconfigured after the MAC reset. The MDIO clock
  speed becomes invalid, therefore the driver reads invalid PHY register values.
  The driver now set the MDIO clock speed prior to initializing PHY ops and
  again after the MAC reset.

* **ixgbe: Fixed maximum number of available TX queues.**

  In IXGBE, the maximum number of TX queues varies depending on the NIC operating
  mode. This was not being updated in the device information, providing
  an incorrect number in some cases.

* **i40e: Generated MAC address for each VFs.**

  It generates a MAC address for each VFs during PF host initialization,
  and keeps the VF MAC address the same among different VF launch.

* **i40e: Fixed failure of reading/writing RX control registers.**

  Fixed i40e issue of failing to read/write rx control registers when
  under stress with traffic, which might result in application launch
  failure.

* **i40e: Enabled vector driver by default.**

  Previously, vector driver was disabled by default as it couldn't fill packet type
  info for l3fwd to work well. Now there is an option for l3fwd to analyze
  the packet type so the vector driver is enabled by default.

* **i40e: Fixed link info of VF.**

  Previously, the VF's link speed stayed at 10GbE and status always was up.
  It did not change even when the physical link's status changed.
  Now this issue is fixed to make VF's link info consistent with physical link.

* **mlx5: Fixed possible crash during initialization.**

  A crash could occur when failing to allocate private device context.

* **mlx5: Added port type check.**

  Added port type check to prevent port initialization on non-Ethernet link layers and
  to report an error.

* **mlx5: Applied VLAN filtering to broadcast and IPv6 multicast flows.**

  Prevented reception of multicast frames outside of configured VLANs.

* **mlx5: Fixed RX checksum offload in non L3/L4 packets.**

  Fixed report of bad checksum for packets of unknown type.

* **aesni_mb: Fixed wrong return value when creating a device.**

  The ``cryptodev_aesni_mb_init()`` function was returning the device id of the device created,
  instead of 0 (on success) that ``rte_eal_vdev_init()`` expects.
  This made it impossible to create more than one aesni_mb device
  from the command line.

* **qat: Fixed AES GCM decryption.**

  Allowed AES GCM on the cryptodev API, but in some cases gave invalid results
  due to incorrect IV setting.


Libraries
~~~~~~~~~

* **hash: Fixed CRC32c hash computation for non multiple of 4 bytes sizes.**

  Fix crc32c hash functions to return a valid crc32c value for data lengths
  not a multiple of 4 bytes.

* **hash: Fixed hash library to support multi-process mode.**

  Fix hash library to support multi-process mode, using a jump table,
  instead of storing a function pointer to the key compare function.
  Multi-process mode only works with the built-in compare functions,
  however a custom compare function (not in the jump table) can only
  be used in single-process mode.

* **hash: Fixed return value when allocating an existing hash table.**

  Changed the ``rte_hash*_create()`` functions to return ``NULL`` and set
  ``rte_errno`` to ``EEXIST`` when the object name already exists. This is
  the behavior described in the API documentation in the header file.
  The previous behavior was to return a pointer to the existing object in
  that case, preventing the caller from knowing if the object had to be freed
  or not.

* **lpm: Fixed return value when allocating an existing object.**

  Changed the ``rte_lpm*_create()`` functions to return ``NULL`` and set
  ``rte_errno`` to ``EEXIST`` when the object name already exists. This is
  the behavior described in the API documentation in the header file.
  The previous behavior was to return a pointer to the existing object in
  that case, preventing the caller from knowing if the object had to be freed
  or not.

* **librte_port: Fixed segmentation fault for ring and ethdev writer nodrop.**

  Fixed core dump issue on txq and swq when dropless is set to yes.


Examples
~~~~~~~~

* **l3fwd-power: Fixed memory leak for non-IP packet.**

  Fixed issue in l3fwd-power where, on receiving packets of types
  other than IPv4 or IPv6, the mbuf was not released, and caused
  a memory leak.

* **l3fwd: Fixed using packet type blindly.**

  l3fwd makes use of packet type information without querying if devices or PMDs
  really set it. For those devices that don't set ptypes, add an option to parse it.

* **examples/vhost: Fixed frequent mbuf allocation failure.**

  The vhost-switch often fails to allocate mbuf when dequeue from vring because it
  wrongly calculates the number of mbufs needed.


API Changes
-----------

* The ethdev statistics counter ``imissed`` is considered to be independent of ``ierrors``.
  All drivers are now counting the missed packets only once, i.e. drivers will
  not increment ierrors anymore for missed packets.

* The ethdev structure ``rte_eth_dev_info`` was changed to support device
  speed capabilities.

* The ethdev structures ``rte_eth_link`` and ``rte_eth_conf`` were changed to
  support the new link API.

* The functions ``rte_eth_dev_udp_tunnel_add`` and ``rte_eth_dev_udp_tunnel_delete``
  have been renamed into ``rte_eth_dev_udp_tunnel_port_add`` and
  ``rte_eth_dev_udp_tunnel_port_delete``.

* The ``outer_mac`` and ``inner_mac`` fields in structure
  ``rte_eth_tunnel_filter_conf`` are changed from pointer to struct in order
  to keep code's readability.

* The fields in ethdev structure ``rte_eth_fdir_masks`` were changed
  to be in big endian.

* A parameter ``vlan_type`` has been added to the function
  ``rte_eth_dev_set_vlan_ether_type``.

* The af_packet device init function is no longer public. The device should be attached
  via the API.

* The LPM ``next_hop`` field is extended from 8 bits to 24 bits for IPv4
  while keeping ABI compatibility.

* A new ``rte_lpm_config`` structure is used so the LPM library will allocate
  exactly the amount of memory which is necessary to hold application’s rules.
  The previous ABI is kept for compatibility.

* The prototype for the pipeline input port, output port and table action
  handlers are updated: the pipeline parameter is added,
  the packets mask parameter has been either removed or made input-only.


ABI Changes
-----------

* The RETA entry size in ``rte_eth_rss_reta_entry64`` has been increased
  from 8-bit to 16-bit.

* The ethdev flow director structure ``rte_eth_fdir_flow`` structure was
  changed. New fields were added to extend flow director's input set.

* The cmdline buffer size has been increase from 256 to 512.


Shared Library Versions
-----------------------

The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

   + libethdev.so.3
     librte_acl.so.2
     librte_cfgfile.so.2
   + librte_cmdline.so.2
     librte_distributor.so.1
     librte_eal.so.2
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_ivshmem.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
     librte_lpm.so.2
     librte_mbuf.so.2
     librte_mempool.so.1
     librte_meter.so.1
   + librte_pipeline.so.3
     librte_pmd_bond.so.1
     librte_pmd_ring.so.2
     librte_port.so.2
     librte_power.so.1
     librte_reorder.so.1
     librte_ring.so.1
     librte_sched.so.1
     librte_table.so.2
     librte_timer.so.1
     librte_vhost.so.2


Tested Platforms
----------------

#. SuperMicro 1U

   - BIOS: 1.0c
   - Processor: Intel(R) Atom(TM) CPU C2758 @ 2.40GHz

#. SuperMicro 1U

   - BIOS: 1.0a
   - Processor: Intel(R) Xeon(R) CPU D-1540 @ 2.00GHz
   - Onboard NIC: Intel(R) X552/X557-AT (2x10G)

     - Firmware-version: 0x800001cf
     - Device ID (PF/VF): 8086:15ad /8086:15a8

   - kernel driver version: 4.2.5 (ixgbe)

#. SuperMicro 1U

   - BIOS: 1.0a
   - Processor: Intel(R) Xeon(R) CPU E5-4667 v3 @ 2.00GHz

#. Intel(R) Server board S2600GZ

   - BIOS: SE5C600.86B.02.02.0002.122320131210
   - Processor: Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz

#. Intel(R) Server board W2600CR

   - BIOS: SE5C600.86B.02.01.0002.082220131453
   - Processor: Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz

#. Intel(R) Server board S2600CWT

   - BIOS: SE5C610.86B.01.01.0009.060120151350
   - Processor: Intel(R) Xeon(R) CPU E5-2699 v3 @ 2.30GHz

#. Intel(R) Server board S2600WTT

   - BIOS: SE5C610.86B.01.01.0005.101720141054
   - Processor: Intel(R) Xeon(R) CPU E5-2699 v3 @ 2.30GHz

#. Intel(R) Server board S2600WTT

   - BIOS: SE5C610.86B.11.01.0044.090120151156
   - Processor: Intel(R) Xeon(R) CPU E5-2695 v4 @ 2.10GHz


Tested NICs
-----------

#. Intel(R) Ethernet Controller X540-AT2

   - Firmware version: 0x80000389
   - Device id (pf): 8086:1528
   - Driver version: 3.23.2 (ixgbe)

#. Intel(R) 82599ES 10 Gigabit Ethernet Controller

   - Firmware version: 0x61bf0001
   - Device id (pf/vf): 8086:10fb / 8086:10ed
   - Driver version: 4.0.1-k (ixgbe)

#. Intel(R) Corporation Ethernet Connection X552/X557-AT 10GBASE-T

   - Firmware version: 0x800001cf
   - Device id (pf/vf): 8086:15ad / 8086:15a8
   - Driver version: 4.2.5 (ixgbe)

#. Intel(R) Ethernet Converged Network Adapter X710-DA4 (4x10G)

   - Firmware version: 5.02 0x80002284
   - Device id (pf/vf): 8086:1572 / 8086:154c
   - Driver version: 1.4.26 (i40e)

#. Intel(R) Ethernet Converged Network Adapter X710-DA2 (2x10G)

   - Firmware version: 5.02 0x80002282
   - Device id (pf/vf): 8086:1572 / 8086:154c
   - Driver version: 1.4.25 (i40e)

#. Intel(R) Ethernet Converged Network Adapter XL710-QDA1 (1x40G)

   - Firmware version: 5.02 0x80002281
   - Device id (pf/vf): 8086:1584 / 8086:154c
   - Driver version: 1.4.25 (i40e)

#. Intel(R) Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

   - Firmware version: 5.02 0x80002285
   - Device id (pf/vf): 8086:1583 / 8086:154c
   - Driver version: 1.4.25 (i40e)

#. Intel(R) 82576EB Gigabit Ethernet Controller

   - Firmware version: 1.2.1
   - Device id (pf): 8086:1526
   - Driver version: 5.2.13-k (igb)

#. Intel(R) Ethernet Controller I210

   - Firmware version: 3.16, 0x80000500, 1.304.0
   - Device id (pf): 8086:1533
   - Driver version: 5.2.13-k (igb)

#. Intel(R) Corporation I350 Gigabit Network Connection

   - Firmware version: 1.48, 0x800006e7
   - Device id (pf/vf): 8086:1521 / 8086:1520
   - Driver version: 5.2.13-k (igb)


#. Intel(R) Ethernet Multi-host Controller FM10000

   - Firmware version: N/A
   - Device id (pf/vf): 8086:15d0
   - Driver version: 0.17.0.9 (fm10k)
