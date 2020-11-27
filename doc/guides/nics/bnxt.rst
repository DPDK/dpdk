.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2020 Broadcom Inc.

BNXT Poll Mode Driver
=====================

The Broadcom BNXT PMD (**librte_net_bnxt**) implements support for adapters
based on Ethernet controllers and SoCs belonging to the Broadcom
BCM574XX/BCM575XX NetXtreme-E® Family of Ethernet Network Controllers,
the Broadcom BCM588XX Stingray Family of Smart NIC Adapters, and the Broadcom
StrataGX® BCM5873X Series of Communications Processors.

A complete list with links to reference material is in the Appendix section.

CPU Support
-----------

BNXT PMD supports multiple CPU architectures, including x86-32, x86-64, and ARMv8.

Kernel Dependency
-----------------

BNXT PMD requires a kernel module (VFIO or UIO) for setting up a device, mapping
device memory to userspace, registering interrupts, etc.
VFIO is more secure than UIO, relying on IOMMU protection.
UIO requires the IOMMU disabled or configured to pass-through mode.

The BNXT PMD supports operating with:

* Linux vfio-pci
* Linux uio_pci_generic
* Linux igb_uio
* BSD nic_uio

Running BNXT PMD
----------------

Bind the device to one of the kernel modules listed above

.. code-block:: console

    ./dpdk-devbind.py -b vfio-pci|igb_uio|uio_pci_generic bus_id:device_id.function_id

The BNXT PMD can run on PF or VF.

PCI-SIG Single Root I/O Virtualization (SR-IOV) involves the direct assignment
of part of the network port resources to guest operating systems using the
SR-IOV standard.
NIC is logically distributed among multiple virtual machines (VMs), while still
having global data in common to share with the PF and other VFs.

Sysadmin can create and configure VFs:

.. code-block:: console

  echo num_vfs > /sys/bus/pci/devices/domain_id:bus_id:device_id:function_id/sriov_numvfs
  (ex) echo 4 > /sys/bus/pci/devices/0000:82:00:0/sriov_numvfs

Sysadmin also can change the VF property such as MAC address, transparent VLAN,
TX rate limit, and trusted VF:

.. code-block:: console

  ip link set pf_id vf vf_id mac (mac_address) vlan (vlan_id) txrate (rate_value) trust (enable|disable)
  (ex) ip link set 0 vf 0 mac 00:11:22:33:44:55 vlan 0x100 txrate 100 trust disable

Running on VF
~~~~~~~~~~~~~

Flow Bifurcation
^^^^^^^^^^^^^^^^

The Flow Bifurcation splits the incoming data traffic to user space applications
(such as DPDK applications) and/or kernel space programs (such as the Linux
kernel stack).
It can direct some traffic, for example data plane traffic, to DPDK.
Rest of the traffic, for example control plane traffic, would be redirected to
the traditional Linux networking stack.

Refer to https://doc.dpdk.org/guides/howto/flow_bifurcation.html

Benefits of the flow bifurcation include:

* Better performance with less CPU overhead, as user application can directly
  access the NIC for data path
* NIC is still being controlled by the kernel, as control traffic is forwarded
  only to the kernel driver
* Control commands, e.g. ethtool, will work as usual

Running on a VF, the BXNT PMD supports the flow bifurcation with a combination
of SR-IOV and packet classification and/or forwarding capability.
In the simplest case of flow bifurcation, a PF driver configures a NIC to
forward all user traffic directly to VFs with matching destination MAC address,
while the rest of the traffic is forwarded to a PF.
Note that the broadcast packets will be forwarded to both PF and VF.

.. code-block:: console

    (ex) ethtool --config-ntuple ens2f0 flow-type ether dst 00:01:02:03:00:01 vlan 10 vlan-mask 0xf000 action 0x100000000

Trusted VF
^^^^^^^^^^

By default, VFs are *not* allowed to perform privileged operations, such as
modifying the VF’s MAC address in the guest. These security measures are
designed to prevent possible attacks.
However, when a DPDK application can be trusted (e.g., OVS-DPDK, here), these
operations performed by a VF would be legitimate and can be allowed.

To enable VF to request "trusted mode," a new trusted VF concept was introduced
in Linux kernel 4.4 and allowed VFs to become “trusted” and perform some
privileged operations.

The BNXT PMD supports the trusted VF mode of operation. Only a PF can enable the
trusted attribute on the VF. It is preferable to enable the Trusted setting on a
VF before starting applications.
However, the BNXT PMD handles dynamic changes in trusted settings as well.

Note that control commands, e.g., ethtool, will work via the kernel PF driver,
*not* via the trusted VF driver.

Operations supported by trusted VF:

* MAC address configuration
* Flow rule creation

Operations *not* supported by trusted VF:

* Firmware upgrade
* Promiscuous mode setting

Running on PF
~~~~~~~~~~~~~

Unlike the VF when BNXT PMD runs on a PF there are no restrictions placed on the
features which the PF can enable or request. In a multiport NIC, each port will
have a corresponding PF. Also depending on the configuration of the NIC there
can be more than one PF associated per port.
A sysadmin can load the kernel driver on one PF, and run BNXT PMD on the other
PF or run the PMD on both the PFs. In such cases, the firmware picks one of the
PFs as a master PF.

Much like in the trusted VF, the DPDK application must be *trusted* and expected
to be *well-behaved*.

Features
--------

The BNXT PMD supports the following features:

* Port Control
    * Port MTU
    * LED
    * Flow Control and Autoneg
* Packet Filtering
    * Unicast MAC Filter
    * Multicast MAC Filter
    * VLAN Filtering
    * Allmulticast Mode
    * Promiscuous Mode
* Stateless Offloads
    * CRC Offload
    * Checksum Offload (IPv4, TCP, and UDP)
    * Multi-Queue (TSS and RSS)
    * Segmentation and Reassembly (TSO and LRO)
* VLAN insert strip
* Stats Collection
* Generic Flow Offload

Port Control
~~~~~~~~~~~~

**Port MTU**: BNXT PMD supports the MTU (Maximum Transmission Unit) up to 9,574
bytes:

.. code-block:: console

    testpmd> port config mtu (port_id) mtu_value
    testpmd> show port info (port_id)

**LED**: Application tunes on (or off) a port LED, typically for a port
identification:

.. code-block:: console

    int rte_eth_led_on (uint16_t port_id)
    int rte_eth_led_off (uint16_t port_id)

**Flow Control and Autoneg**: Application tunes on (or off) flow control and/or
auto-negotiation on a port:

.. code-block:: console

    testpmd> set flow_ctrl rx (on|off) (port_id)
    testpmd> set flow_ctrl tx (on|off) (port_id)
    testpmd> set flow_ctrl autoneg (on|off) (port_id)

Note that the BNXT PMD does *not* support some options and ignores them when
requested:

* high_water
* low_water
* pause_time
* mac_ctrl_frame_fwd
* send_xon

Packet Filtering
~~~~~~~~~~~~~~~~

Applications control the packet-forwarding behaviors with packet filters.

The BNXT PMD supports hardware-based packet filtering:

* UC (Unicast) MAC Filters
    * No unicast packets are forwarded to an application except the one with
      DMAC address added to the port
    * At initialization, the station MAC address is added to the port
* MC (Multicast) MAC Filters
    * No multicast packets are forwarded to an application except the one with
      MC address added to the port
    * When the application listens to a multicast group, it adds the MC address
      to the port
* VLAN Filtering Mode
    * When enabled, no packets are forwarded to an application except the ones
      with the VLAN tag assigned to the port
* Allmulticast Mode
    * When enabled, every multicast packet received on the port is forwarded to
      the application
    * Typical usage is routing applications
* Promiscuous Mode
    * When enabled, every packet received on the port is forwarded to the
      application

Unicast MAC Filter
^^^^^^^^^^^^^^^^^^

The application can add (or remove) MAC addresses to enable (or disable)
filtering on MAC address used to accept packets.

.. code-block:: console

    testpmd> show port (port_id) macs
    testpmd> mac_addr (add|remove) (port_id) (XX:XX:XX:XX:XX:XX)

Multicast MAC Filter
^^^^^^^^^^^^^^^^^^^^

The application can add (or remove) Multicast addresses that enable (or disable)
filtering on multicast MAC address used to accept packets.

.. code-block:: console

    testpmd> show port (port_id) mcast_macs
    testpmd> mcast_addr (add|remove) (port_id) (XX:XX:XX:XX:XX:XX)

Application adds (or removes) Multicast addresses to enable (or disable)
allowlist filtering to accept packets.

Note that the BNXT PMD supports up to 16 MC MAC filters. if the user adds more
than 16 MC MACs, the BNXT PMD puts the port into the Allmulticast mode.

VLAN Filtering
^^^^^^^^^^^^^^

The application enables (or disables) VLAN filtering mode. When the mode is
enabled, no packets are forwarded to an application except ones with VLAN tag
assigned for the application.

.. code-block:: console

    testpmd> vlan set filter (on|off) (port_id)
    testpmd> rx_vlan (add|rm) (vlan_id) (port_id)

Allmulticast Mode
^^^^^^^^^^^^^^^^^

The application enables (or disables) the allmulticast mode. When the mode is
enabled, every multicast packet received is forwarded to the application.

.. code-block:: console

    testpmd> show port info (port_id)
    testpmd> set allmulti (port_id) (on|off)

Promiscuous Mode
^^^^^^^^^^^^^^^^

The application enables (or disables) the promiscuous mode. When the mode is
enabled on a port, every packet received on the port is forwarded to the
application.

.. code-block:: console

    testpmd> show port info (port_id)
    testpmd> set promisc port_id (on|off)

Stateless Offloads
~~~~~~~~~~~~~~~~~~

Like Linux, DPDK provides enabling hardware offload of some stateless processing
(such as checksum calculation) of the stack, alleviating the CPU from having to
burn cycles on every packet.

Listed below are the stateless offloads supported by the BNXT PMD:

* CRC offload (for both TX and RX packets)
* Checksum Offload (for both TX and RX packets)
    * IPv4 Checksum Offload
    * TCP Checksum Offload
    * UDP Checksum Offload
* Segmentation/Reassembly Offloads
    * TCP Segmentation Offload (TSO)
    * Large Receive Offload (LRO)
* Multi-Queue
    * Transmit Side Scaling (TSS)
    * Receive Side Scaling (RSS)

Also, the BNXT PMD supports stateless offloads on inner frames for tunneled
packets. Listed below are the tunneling protocols supported by the BNXT PMD:

* VXLAN
* GRE
* NVGRE

Note that enabling (or disabling) stateless offloads requires applications to
stop DPDK before changing configuration.

CRC Offload
^^^^^^^^^^^

The FCS (Frame Check Sequence) in the Ethernet frame is a four-octet CRC (Cyclic
Redundancy Check) that allows detection of corrupted data within the entire
frame as received on the receiver side.

The BNXT PMD supports hardware-based CRC offload:

* TX: calculate and insert CRC
* RX: check and remove CRC, notify the application on CRC error

Note that the CRC offload is always turned on.

Checksum Offload
^^^^^^^^^^^^^^^^

The application enables hardware checksum calculation for IPv4, TCP, and UDP.

.. code-block:: console

    testpmd> port stop (port_id)
    testpmd> csum set (ip|tcp|udp|outer-ip|outer-udp) (sw|hw) (port_id)
    testpmd> set fwd csum

Multi-Queue
^^^^^^^^^^^

Multi-Queue, also known as TSS (Transmit Side Scaling) or RSS (Receive Side
Scaling), is a common networking technique that allows for more efficient load
balancing across multiple CPU cores.

The application enables multiple TX and RX queues when it is started.

.. code-block:: console

    testpmd -l 1,3,5 --main-lcore 1 --txq=2 –rxq=2 --nb-cores=2

**TSS**

TSS distributes network transmit processing across several hardware-based
transmit queues, allowing outbound network traffic to be processed by multiple
CPU cores.

**RSS**

RSS distributes network receive processing across several hardware-based receive
queues, allowing inbound network traffic to be processed by multiple CPU cores.

The application can select the RSS mode, i.e. select the header fields that are
included for hash calculation. The BNXT PMD supports the RSS mode of
``default|ip|tcp|udp|none``, where default mode is L3 and L4.

For tunneled packets, RSS hash is calculated over inner frame header fields.
Applications may want to select the tunnel header fields for hash calculation,
and it will be supported in 20.08 using RSS level.

.. code-block:: console

    testpmd> port config (port_id) rss (all|default|ip|tcp|udp|none)

    // note that the testpmd defaults the RSS mode to ip
    // ensure to issue the command below to enable L4 header (TCP or UDP) along with IPv4 header
    testpmd> port config (port_id) rss default

    // to check the current RSS configuration, such as RSS function and RSS key
    testpmd> show port (port_id) rss-hash key

    // RSS is enabled by default. However, application can disable RSS as follows
    testpmd> port config (port_id) rss none

Application can change the flow distribution, i.e. remap the received traffic to
CPU cores, using RSS RETA (Redirection Table).

.. code-block:: console

    // application queries the current RSS RETA configuration
    testpmd> show port (port_id) rss reta size (mask0, mask1)

    // application changes the RSS RETA configuration
    testpmd> port config (port_id) rss reta (hash, queue) [, (hash, queue)]

TSO
^^^

TSO (TCP Segmentation Offload), also known as LSO (Large Send Offload), enables
the TCP/IP stack to pass to the NIC a larger datagram than the MTU (Maximum
Transmit Unit). NIC breaks it into multiple segments before sending it to the
network.

The BNXT PMD supports hardware-based TSO.

.. code-block:: console

    // display the status of TSO
    testpmd> tso show (port_id)

    // enable/disable TSO
    testpmd> port config (port_id) tx_offload tcp_tso (on|off)

    // set TSO segment size
    testpmd> tso set segment_size (port_id)

The BNXT PMD also supports hardware-based tunneled TSO.

.. code-block:: console

    // display the status of tunneled TSO
    testpmd> tunnel_tso show (port_id)

    // enable/disable tunneled TSO
    testpmd> port config (port_id) tx_offload vxlan_tnl_tso|gre_tnl_tso (on|off)

    // set tunneled TSO segment size
    testpmd> tunnel_tso set segment_size (port_id)

Note that the checksum offload is always assumed to be enabled for TSO.

LRO
^^^

LRO (Large Receive Offload) enables NIC to aggregate multiple incoming TCP/IP
packets from a single stream into a larger buffer, before passing to the
networking stack.

The BNXT PMD supports hardware-based LRO.

.. code-block:: console

    // display the status of LRO
    testpmd> show port (port_id) rx_offload capabilities
    testpmd> show port (port_id) rx_offload configuration

    // enable/disable LRO
    testpmd> port config (port_id) rx_offload tcp_lro (on|off)

    // set max LRO packet (datagram) size
    testpmd> port config (port_id) max-lro-pkt-size (max_size)

The BNXT PMD also supports tunneled LRO.

Some applications, such as routing, should *not* change the packet headers as
they pass through (i.e. received from and sent back to the network). In such a
case, GRO (Generic Receive Offload) should be used instead of LRO.

VLAN Insert/Strip
~~~~~~~~~~~~~~~~~

DPDK application offloads VLAN insert/strip to improve performance. The BNXT PMD
supports hardware-based VLAN insert/strip offload for both single and double
VLAN packets.


VLAN Insert
^^^^^^^^^^^

Application configures the VLAN TPID (Tag Protocol ID). By default, the TPID is
0x8100.

.. code-block:: console

    // configure outer TPID value for a port
    testpmd> vlan set outer tpid (tpid_value) (port_id)

The inner TPID set will be rejected as the BNXT PMD supports inserting only an
outer VLAN. Note that when a packet has a single VLAN, the tag is considered as
outer, i.e. the inner VLAN is relevant only when a packet is double-tagged.

The BNXT PMD supports various TPID values shown below. Any other values will be
rejected.

* ``0x8100``
* ``0x88a8``
* ``0x9100``
* ``0x9200``
* ``0x9300``

The BNXT PMD supports the VLAN insert offload per-packet basis. The application
provides the TCI (Tag Control Info) for a packet via mbuf. In turn, the BNXT PMD
inserts the VLAN tag (via hardware) using the provided TCI along with the
configured TPID.

.. code-block:: console

    // enable VLAN insert offload
    testpmd> port config (port_id) rx_offload vlan_insert|qinq_insert (on|off)

    if (mbuf->ol_flags && PKT_TX_QINQ)       // case-1: insert VLAN to single-tagged packet
        tci_value = mbuf->vlan_tci_outer
    else if (mbuf->ol_flags && PKT_TX_VLAN)  // case-2: insert VLAN to untagged packet
        tci_value = mbuf->vlan_tci

VLAN Strip
^^^^^^^^^^

The application configures the per-port VLAN strip offload.

.. code-block:: console

    // enable VLAN strip on a port
    testpmd> port config (port_id) tx_offload vlan_strip (on|off)

    // notify application VLAN strip via mbuf
    mbuf->ol_flags |= PKT_RX_VLAN | PKT_RX_STRIPPED // outer VLAN is found and stripped
    mbuf->vlan_tci = tci_value                      // TCI of the stripped VLAN

Time Synchronization
~~~~~~~~~~~~~~~~~~~~

System operators may run a PTP (Precision Time Protocol) client application to
synchronize the time on the NIC (and optionally, on the system) to a PTP master.

The BNXT PMD supports a PTP client application to communicate with a PTP master
clock using DPDK IEEE1588 APIs. Note that the PTP client application needs to
run on PF and vector mode needs to be disabled.

.. code-block:: console

    testpmd> set fwd ieee1588 // enable IEEE 1588 mode

When enabled, the BNXT PMD configures hardware to insert IEEE 1588 timestamps to
the outgoing PTP packets and reports IEEE 1588 timestamps from the incoming PTP
packets to application via mbuf.

.. code-block:: console

    // RX packet completion will indicate whether the packet is PTP
    mbuf->ol_flags |= PKT_RX_IEEE1588_PTP

Statistics Collection
~~~~~~~~~~~~~~~~~~~~~

In Linux, the *ethtool -S* enables us to query the NIC stats. DPDK provides the
similar functionalities via rte_eth_stats and rte_eth_xstats.

The BNXT PMD supports both basic and extended stats collection:

* Basic stats
* Extended stats

Basic Stats
^^^^^^^^^^^

The application collects per-port and per-queue stats using rte_eth_stats APIs.

.. code-block:: console

    testpmd> show port stats (port_id)

Basic stats include:

* ipackets
* ibytes
* opackets
* obytes
* imissed
* ierrors
* oerrors

By default, per-queue stats for 16 queues are supported. For more than 16
queues, BNXT PMD should be compiled with ``RTE_ETHDEV_QUEUE_STAT_CNTRS``
set to the desired number of queues.

Extended Stats
^^^^^^^^^^^^^^

Unlike basic stats, the extended stats are vendor-specific, i.e. each vendor
provides its own set of counters.

The BNXT PMD provides a rich set of counters, including per-flow counters,
per-cos counters, per-priority counters, etc.

.. code-block:: console

    testpmd> show port xstats (port_id)

Shown below is the elaborated sequence to retrieve extended stats:

.. code-block:: console

    // application queries the number of xstats
    len = rte_eth_xstats_get(port_id, NULL, 0);
    // BNXT PMD returns the size of xstats array (i.e. the number of entries)
    // BNXT PMD returns 0, if the feature is compiled out or disabled

    // application allocates memory for xstats
    struct rte_eth_xstats_name *names; // name is 64 character or less
    struct rte_eth_xstats *xstats;
    names = calloc(len, sizeof(*names));
    xstats = calloc(len, sizeof(*xstats));

    // application retrieves xstats // names and values
    ret = rte_eth_xstats_get_names(port_id, *names, len);
    ret = rte_eth_xstats_get(port_id, *xstats, len);

    // application checks the xstats
    // application may repeat the below:
    len = rte_eth_xstats_reset(port_id); // reset the xstats

    // reset can be skipped, if application wants to see accumulated stats
    // run traffic
    // probably stop the traffic
    // retrieve xstats // no need to retrieve xstats names again
    // check xstats

Generic Flow Offload
~~~~~~~~~~~~~~~~~~~~

Applications can get benefit by offloading all or part of flow processing to
hardware. For example, applications can offload packet classification only
(partial offload) or whole match-action (full offload).

DPDK offers the Generic Flow API (rte_flow API) to configure hardware to
perform flow processing.

Listed below are the rte_flow APIs BNXT PMD supports:

* rte_flow_validate
* rte_flow_create
* rte_flow_destroy
* rte_flow_flush

Host Based Flow Table Management
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Starting with 20.05 BNXT PMD supports host based flow table management. This is
a new mechanism that should allow higher flow scalability than what is currently
supported. This new approach also defines a new rte_flow parser, and mapper
which currently supports basic packet classification in the receive path.

The feature uses a newly implemented control-plane firmware interface which
optimizes flow insertions and deletions.

This is a tech preview feature, and is disabled by default. It can be enabled
using bnxt devargs. For ex: "-a 0000:0d:00.0,host-based-truflow=1”.

This feature is currently supported on Whitney+ and Stingray devices.

Notes
-----

- On stopping a device port, all the flows created on a port by the
  application will be flushed from the hardware and any tables maintained
  by the PMD. After stopping the device port, all flows on the port become
  invalid and are not represented in the system anymore.
  Instead of destroying or flushing such flows an application should discard
  all references to these flows and re-create the flows as required after the
  port is restarted.

- While an application is free to use the group id attribute to group flows
  together using a specific criteria, the BNXT PMD currently associates this
  group id to a VNIC id. One such case is grouping of flows which are filtered
  on the same source or destination MAC address. This allows packets of such
  flows to be directed to one or more queues associated with the VNIC id.
  This implementation is supported only when TRUFLOW functionality is disabled.

- An application can issue a VXLAN decap offload request using rte_flow API
  either as a single rte_flow request or a combination of two stages.
  The PMD currently supports the two stage offload design.
  In this approach the offload request may come as two flow offload requests
  Flow1 & Flow2.  The match criteria for Flow1 is O_DMAC, O_SMAC, O_DST_IP,
  O_UDP_DPORT and actions are COUNT, MARK, JUMP. The match criteria for Flow2
  is O_SRC_IP, O_DST_IP, VNI and inner header fields.
  Flow1 and Flow2 flow offload requests can come in any order. If Flow2 flow
  offload request comes first then Flow2 can’t be offloaded as there is
  no O_DMAC information in Flow2. In this case, Flow2 will be deferred until
  Flow1 flow offload request arrives. When Flow1 flow offload request is
  received it will have O_DMAC information. Using Flow1’s O_DMAC, driver
  creates an L2 context entry in the hardware as part of offloading Flow1.
  Flow2 will now use Flow1’s O_DMAC to get the L2 context id associated with
  this O_DMAC and other flow fields that are cached already at the time
  of deferring Flow2 for offloading. Flow2 that arrive after Flow1 is offloaded
  will be directly programmed and not cached.

- PMD supports thread-safe rte_flow operations.

Note: A VNIC represents a virtual interface in the hardware. It is a resource
in the RX path of the chip and is used to setup various target actions such as
RSS, MAC filtering etc. for the physical function in use.

Virtual Function Port Representors
----------------------------------
The BNXT PMD supports the creation of VF port representors for the control
and monitoring of BNXT virtual function devices. Each port representor
corresponds to a single virtual function of that device that is connected to a
VF. When there is no hardware flow offload, each packet transmitted by the VF
will be received by the corresponding representor. Similarly each packet that is
sent to a representor will be received by the VF. Applications can take
advantage of this feature when SRIOV is enabled. The representor will allow the
first packet that is transmitted by the VF to be received by the DPDK
application which can then decide if the flow should be offloaded to the
hardware. Once the flow is offloaded in the hardware, any packet matching the
flow will be received by the VF while the DPDK application will not receive it
any more. The BNXT PMD supports creation and handling of the port representors
when the PMD is initialized on a PF or trusted-VF. The user can specify the list
of VF IDs of the VFs for which the representors are needed by using the
``devargs`` option ``representor``.::

  -a DBDF,representor=[0,1,4]

Note that currently hot-plugging of representor ports is not supported so all
the required representors must be specified on the creation of the PF or the
trusted VF.

Representors on Stingray SoC
----------------------------
A representor created on X86 host typically represents a VF running in the same
X86 domain. But in case of the SoC, the application can run on the CPU complex
inside the SoC. The representor can be created on the SoC to represent a PF or a
VF running in the x86 domain. Since the representator creation requires passing
the bus:device.function of the PCI device endpoint which is not necessarily in the
same host domain, additional dev args have been added to the PMD.

* rep_is_vf - false to indicate VF representor
* rep_is_pf - true to indicate PF representor
* rep_based_pf - Physical index of the PF
* rep_q_r2f - Logical COS Queue index for the rep to endpoint direction
* rep_q_f2r - Logical COS Queue index for the endpoint to rep direction
* rep_fc_r2f - Flow control for the representor to endpoint direction
* rep_fc_f2r - Flow control for the endpoint to representor direction

The sample command line with the new ``devargs`` looks like this::

  -a 0000:06:02.0,host-based-truflow=1,representor=[1],rep-based-pf=8,\
	rep-is-pf=1,rep-q-r2f=1,rep-fc-r2f=0,rep-q-f2r=1,rep-fc-f2r=1

.. code-block:: console

	testpmd -l1-4 -n2 -a 0008:01:00.0,host-based-truflow=1,\
	representor=[0], rep-based-pf=8,rep-is-pf=0,rep-q-r2f=1,rep-fc-r2f=1,\
	rep-q-f2r=0,rep-fc-f2r=1 --log-level="pmd.*",8 -- -i --rxq=3 --txq=3

Number of flows supported
-------------------------
The number of flows that can be support can be changed using the devargs
parameter ``max_num_kflows``. The default number of flows supported is 16K each
in ingress and egress path.

Selecting EM vs EEM
-------------------
Broadcom devices can support filter creation in the onchip memory or the
external memory. This is referred to as EM or EEM mode respectively.
The decision for internal/external EM support is based on the ``devargs``
parameter ``max_num_kflows``.  If this is set by the user, external EM is used.
Otherwise EM support is enabled with flows created in internal memory.

Application Support
-------------------

Firmware
~~~~~~~~

The BNXT PMD supports the application to retrieve the firmware version.

.. code-block:: console

    testpmd> show port info (port_id)

Note that the applications cannot update the firmware using BNXT PMD.

Multiple Processes
~~~~~~~~~~~~~~~~~~

When two or more DPDK applications (e.g., testpmd and dpdk-pdump) share a single
instance of DPDK, the BNXT PMD supports a single primary application and one or
more secondary applications. Note that the DPDK-layer (not the PMD) ensures
there is only one primary application.

There are two modes:

Manual mode

* Application notifies whether it is primary or secondary using *proc-type* flag
* 1st process should be spawned with ``--proc-type=primary``
* All subsequent processes should be spawned with ``--proc-type=secondary``

Auto detection mode

* Application is using ``proc-type=auto`` flag
* A process is spawned as a secondary if a primary is already running

The BNXT PMD uses the info to skip a device initialization, i.e. performs a
device initialization only when being brought up by a primary application.

Runtime Queue Setup
~~~~~~~~~~~~~~~~~~~

Typically, a DPDK application allocates TX and RX queues statically: i.e. queues
are allocated at start. However, an application may want to increase (or
decrease) the number of queues dynamically for various reasons, e.g. power
savings.

The BNXT PMD supports applications to increase or decrease queues at runtime.

.. code-block:: console

    testpmd> port config all (rxq|txq) (num_queues)

Note that a DPDK application must allocate default queues (one for TX and one
for RX at minimum) at initialization.

Descriptor Status
~~~~~~~~~~~~~~~~~

Applications may use the descriptor status for various reasons, e.g. for power
savings. For example, an application may stop polling and change to interrupt
mode when the descriptor status shows no packets to service for a while.

The BNXT PMD supports the application to retrieve both TX and RX descriptor
status.

.. code-block:: console

    testpmd> show port (port_id) (rxq|txq) (queue_id) desc (desc_id) status

Bonding
~~~~~~~

DPDK implements a light-weight library to allow PMDs to be bonded together and provide a single logical PMD to the application.

.. code-block:: console

    testpmd -l 0-3 -n4 --vdev 'net_bonding0,mode=0,slave=<PCI B:D.F device 1>,slave=<PCI B:D.F device 2>,mac=XX:XX:XX:XX:XX:XX’ – --socket_num=1 – -i --port-topology=chained
    (ex) testpmd -l 1,3,5,7,9 -n4 --vdev 'net_bonding0,mode=0,slave=0000:82:00.0,slave=0000:82:00.1,mac=00:1e:67:1d:fd:1d' – --socket-num=1 – -i --port-topology=chained

Vector Processing
-----------------

Vector processing provides significantly improved performance over scalar
processing (see Vector Processor, here).

The BNXT PMD supports the vector processing using SSE (Streaming SIMD
Extensions) instructions on x86 platforms. It also supports NEON intrinsics for
vector processing on ARM CPUs. The BNXT vPMD (vector mode PMD) is available for
Intel/AMD and ARM CPU architectures.

This improved performance comes from several optimizations:

* Batching
    * TX: processing completions in bulk
    * RX: allocating mbufs in bulk
* Chained mbufs are *not* supported, i.e. a packet should fit a single mbuf
* Some stateless offloads are *not* supported with vector processing
    * TX: no offloads will be supported
    * RX: reduced RX offloads (listed below) will be supported::

       DEV_RX_OFFLOAD_VLAN_STRIP
       DEV_RX_OFFLOAD_KEEP_CRC
       DEV_RX_OFFLOAD_JUMBO_FRAME
       DEV_RX_OFFLOAD_IPV4_CKSUM
       DEV_RX_OFFLOAD_UDP_CKSUM
       DEV_RX_OFFLOAD_TCP_CKSUM
       DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM
       DEV_RX_OFFLOAD_RSS_HASH
       DEV_RX_OFFLOAD_VLAN_FILTER

The BNXT Vector PMD is enabled in DPDK builds by default.

However, a decision to enable vector mode will be made when the port transitions
from stopped to started. Any TX offloads or some RX offloads (other than listed
above) will disable the vector mode.
Offload configuration changes that impact vector mode must be made when the port
is stopped.

Note that TX (or RX) vector mode can be enabled independently from RX (or TX)
vector mode.

Also vector mode is allowed when jumbo is enabled
as long as the MTU setting does not require scattered Rx.

Appendix
--------

Supported Chipsets and Adapters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

BCM5730x NetXtreme-C® Family of Ethernet Network Controllers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Information about Ethernet adapters in the NetXtreme family of adapters can be
found in the `NetXtreme® Brand section <https://www.broadcom.com/products/ethernet-connectivity/network-adapters/>`_ of the `Broadcom website <http://www.broadcom.com/>`_.

* ``M150c ... Single-port 40/50 Gigabit Ethernet Adapter``
* ``P150c ... Single-port 40/50 Gigabit Ethernet Adapter``
* ``P225c ... Dual-port 10/25 Gigabit Ethernet Adapter``

BCM574xx/575xx NetXtreme-E® Family of Ethernet Network Controllers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Information about Ethernet adapters in the NetXtreme family of adapters can be
found in the `NetXtreme® Brand section <https://www.broadcom.com/products/ethernet-connectivity/network-adapters/>`_ of the `Broadcom website <http://www.broadcom.com/>`_.

* ``M125P .... Single-port OCP 2.0 10/25 Gigabit Ethernet Adapter``
* ``M150P .... Single-port OCP 2.0 50 Gigabit Ethernet Adapter``
* ``M150PM ... Single-port OCP 2.0 Multi-Host 50 Gigabit Ethernet Adapter``
* ``M210P .... Dual-port OCP 2.0 10 Gigabit Ethernet Adapter``
* ``M210TP ... Dual-port OCP 2.0 10 Gigabit Ethernet Adapter``
* ``M1100G ... Single-port OCP 2.0 10/25/50/100 Gigabit Ethernet Adapter``
* ``N150G .... Single-port OCP 3.0 50 Gigabit Ethernet Adapter``
* ``M225P .... Dual-port OCP 2.0 10/25 Gigabit Ethernet Adapter``
* ``N210P .... Dual-port OCP 3.0 10 Gigabit Ethernet Adapter``
* ``N210TP ... Dual-port OCP 3.0 10 Gigabit Ethernet Adapter``
* ``N225P .... Dual-port OCP 3.0 10/25 Gigabit Ethernet Adapter``
* ``N250G .... Dual-port OCP 3.0 50 Gigabit Ethernet Adapter``
* ``N410SG ... Quad-port OCP 3.0 10 Gigabit Ethernet Adapter``
* ``N410SGBT . Quad-port OCP 3.0 10 Gigabit Ethernet Adapter``
* ``N425G .... Quad-port OCP 3.0 10/25 Gigabit Ethernet Adapter``
* ``N1100G ... Single-port OCP 3.0 10/25/50/100 Gigabit Ethernet Adapter``
* ``N2100G ... Dual-port OCP 3.0 10/25/50/100 Gigabit Ethernet Adapter``
* ``N2200G ... Dual-port OCP 3.0 10/25/50/100/200 Gigabit Ethernet Adapter``
* ``P150P .... Single-port 50 Gigabit Ethernet Adapter``
* ``P210P .... Dual-port 10 Gigabit Ethernet Adapter``
* ``P210TP ... Dual-port 10 Gigabit Ethernet Adapter``
* ``P225P .... Dual-port 10/25 Gigabit Ethernet Adapter``
* ``P410SG ... Quad-port 10 Gigabit Ethernet Adapter``
* ``P410SGBT . Quad-port 10 Gigabit Ethernet Adapter``
* ``P425G .... Quad-port 10/25 Gigabit Ethernet Adapter``
* ``P1100G ... Single-port 10/25/50/100 Gigabit Ethernet Adapter``
* ``P2100G ... Dual-port 10/25/50/100 Gigabit Ethernet Adapter``
* ``P2200G ... Dual-port 10/25/50/100/200 Gigabit Ethernet Adapter``

BCM588xx NetXtreme-S® Family of SmartNIC Network Controllers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Information about the Stingray family of SmartNIC adapters can be found in the
`Stingray® Brand section <https://www.broadcom.com/products/ethernet-connectivity/smartnic/>`_ of the `Broadcom website <http://www.broadcom.com/>`_.

* ``PS225 ... Dual-port 25 Gigabit Ethernet SmartNIC``

BCM5873x StrataGX® Family of Communications Processors
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

These ARM-based processors target a broad range of networking applications,
including virtual CPE (vCPE) and NFV appliances, 10G service routers and
gateways, control plane processing for Ethernet switches, and network-attached
storage (NAS).

* ``StrataGX BCM58732 ... Octal-Core 3.0GHz 64-bit ARM®v8 Cortex®-A72 based SoC``
