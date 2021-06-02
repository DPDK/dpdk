.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2014-2018 Chelsio Communications.
   All rights reserved.

CXGBE Poll Mode Driver
======================

The CXGBE PMD (**librte_net_cxgbe**) provides poll mode driver support
for **Chelsio Terminator** 10/25/40/100 Gbps family of adapters. CXGBE PMD
has support for the latest Linux and FreeBSD operating systems.

CXGBEVF PMD provides poll mode driver support for SR-IOV Virtual functions
and has support for the latest Linux operating systems.

More information can be found at `Chelsio Communications Official Website
<http://www.chelsio.com>`_.

Features
--------

CXGBE and CXGBEVF PMD has support for:

- Multiple queues for TX and RX
- Receiver Side Steering (RSS)
  Receiver Side Steering (RSS) on IPv4, IPv6, IPv4-TCP/UDP, IPv6-TCP/UDP.
  For 4-tuple, enabling 'RSS on TCP' and 'RSS on TCP + UDP' is supported.
- VLAN filtering
- Checksum offload
- Promiscuous mode
- All multicast mode
- Port hardware statistics
- Jumbo frames
- Flow API - Support for both Wildcard (LE-TCAM) and Exact (HASH) match filters.

Limitations
-----------

The Chelsio Terminator series of devices provide two/four ports but
expose a single PCI bus address, thus, librte_net_cxgbe registers
itself as a PCI driver that allocates one Ethernet device per detected
port.

For this reason, one cannot allow/block a single port without
allowing/blocking the other ports on the same device.

.. _t5-nics:

Supported Chelsio T5 NICs
-------------------------

- 1G NICs: T502-BT
- 10G NICs: T520-BT, T520-CR, T520-LL-CR, T520-SO-CR, T540-CR
- 40G NICs: T580-CR, T580-LP-CR, T580-SO-CR
- Other T5 NICs: T522-CR

.. _t6-nics:

Supported Chelsio T6 NICs
-------------------------

- 25G NICs: T6425-CR, T6225-CR, T6225-LL-CR, T6225-SO-CR
- 100G NICs: T62100-CR, T62100-LP-CR, T62100-SO-CR

Supported SR-IOV Chelsio NICs
-----------------------------

SR-IOV virtual functions are supported on all the Chelsio NICs listed
in :ref:`t5-nics` and :ref:`t6-nics`.

Prerequisites
-------------

- Requires firmware version **1.25.6.0** and higher. Visit
  `Chelsio Download Center <http://service.chelsio.com>`_ to get latest firmware
  bundled with the latest Chelsio Unified Wire package.

  For Linux, installing and loading the latest cxgb4 kernel driver from the
  Chelsio Unified Wire package should get you the latest firmware. More
  information can be obtained from the User Guide that is bundled with the
  Chelsio Unified Wire package.

  For FreeBSD, the latest firmware obtained from the Chelsio Unified Wire
  package must be manually flashed via cxgbetool available in FreeBSD source
  repository.

  Instructions on how to manually flash the firmware are given in section
  :ref:`linux-installation` for Linux and section :ref:`freebsd-installation`
  for FreeBSD.


Runtime Options
---------------

The following ``devargs`` options can be enabled at runtime. They must
be passed as part of EAL arguments. For example,

.. code-block:: console

   dpdk-testpmd -a 02:00.4,keep_ovlan=1 -- -i

Common Runtime Options
~~~~~~~~~~~~~~~~~~~~~~

- ``keep_ovlan`` (default **0**)

  Toggle behavior to keep/strip outer VLAN in Q-in-Q packets. If
  enabled, the outer VLAN tag is preserved in Q-in-Q packets. Otherwise,
  the outer VLAN tag is stripped in Q-in-Q packets.

- ``tx_mode_latency`` (default **0**)

  When set to 1, Tx doesn't wait for max number of packets to get
  coalesced and sends the packets immediately at the end of the
  current Tx burst. When set to 0, Tx waits across multiple Tx bursts
  until the max number of packets have been coalesced. In this case,
  Tx only sends the coalesced packets to hardware once the max
  coalesce limit has been reached.

CXGBE VF Only Runtime Options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- ``force_link_up`` (default **0**)

  When set to 1, CXGBEVF PMD always forces link as up for all VFs on
  underlying Chelsio NICs. This enables multiple VFs on the same NIC
  to send traffic to each other even when the physical link is down.

CXGBE PF Only Runtime Options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- ``filtermode`` (default **0**)

  Apart from the 4-tuple (IP src/dst addresses and TCP/UDP src/dst port
  addresses), there are only 40-bits available to match other fields in
  packet headers. So, ``filtermode`` devarg allows user to dynamically
  select a 40-bit supported match field combination for LETCAM (wildcard)
  filters.

  Default value of **0** makes driver pick the combination configured in
  the firmware configuration file on the adapter.

  The supported flags and their corresponding values are shown in table below.
  These flags can be OR'd to create 1 of the multiple supported combinations
  for LETCAM filters.

        ==================      ======
        FLAG                    VALUE
        ==================      ======
        Physical Port           0x1
        PFVF                    0x2
        Destination MAC         0x4
        Ethertype               0x8
        Inner VLAN              0x10
        Outer VLAN              0x20
        IP TOS                  0x40
        IP Protocol             0x80
        ==================      ======

  The supported ``filtermode`` combinations and their corresponding OR'd
  values are shown in table below.

        +-----------------------------------+-----------+
        | FILTERMODE COMBINATIONS           |   VALUE   |
        +===================================+===========+
        | Protocol, TOS, Outer VLAN, Port   |     0xE1  |
        +-----------------------------------+-----------+
        | Protocol, TOS, Outer VLAN         |     0xE0  |
        +-----------------------------------+-----------+
        | Protocol, TOS, Inner VLAN, Port   |     0xD1  |
        +-----------------------------------+-----------+
        | Protocol, TOS, Inner VLAN         |     0xD0  |
        +-----------------------------------+-----------+
        | Protocol, TOS, PFVF, Port         |     0xC3  |
        +-----------------------------------+-----------+
        | Protocol, TOS, PFVF               |     0xC2  |
        +-----------------------------------+-----------+
        | Protocol, TOS, Port               |     0xC1  |
        +-----------------------------------+-----------+
        | Protocol, TOS                     |     0xC0  |
        +-----------------------------------+-----------+
        | Protocol, Outer VLAN, Port        |     0xA1  |
        +-----------------------------------+-----------+
        | Protocol, Outer VLAN              |     0xA0  |
        +-----------------------------------+-----------+
        | Protocol, Inner VLAN, Port        |     0x91  |
        +-----------------------------------+-----------+
        | Protocol, Inner VLAN              |     0x90  |
        +-----------------------------------+-----------+
        | Protocol, Ethertype, DstMAC, Port |     0x8D  |
        +-----------------------------------+-----------+
        | Protocol, Ethertype, DstMAC       |     0x8C  |
        +-----------------------------------+-----------+
        | Protocol, Ethertype, Port         |     0x89  |
        +-----------------------------------+-----------+
        | Protocol, Ethertype               |     0x88  |
        +-----------------------------------+-----------+
        | Protocol, DstMAC, PFVF, Port      |     0x87  |
        +-----------------------------------+-----------+
        | Protocol, DstMAC, PFVF            |     0x86  |
        +-----------------------------------+-----------+
        | Protocol, DstMAC, Port            |     0x85  |
        +-----------------------------------+-----------+
        | Protocol, DstMAC                  |     0x84  |
        +-----------------------------------+-----------+
        | Protocol, PFVF, Port              |     0x83  |
        +-----------------------------------+-----------+
        | Protocol, PFVF                    |     0x82  |
        +-----------------------------------+-----------+
        | Protocol, Port                    |     0x81  |
        +-----------------------------------+-----------+
        | Protocol                          |     0x80  |
        +-----------------------------------+-----------+
        | TOS, Outer VLAN, Port             |     0x61  |
        +-----------------------------------+-----------+
        | TOS, Outer VLAN                   |     0x60  |
        +-----------------------------------+-----------+
        | TOS, Inner VLAN, Port             |     0x51  |
        +-----------------------------------+-----------+
        | TOS, Inner VLAN                   |     0x50  |
        +-----------------------------------+-----------+
        | TOS, Ethertype, DstMAC, Port      |     0x4D  |
        +-----------------------------------+-----------+
        | TOS, Ethertype, DstMAC            |     0x4C  |
        +-----------------------------------+-----------+
        | TOS, Ethertype, Port              |     0x49  |
        +-----------------------------------+-----------+
        | TOS, Ethertype                    |     0x48  |
        +-----------------------------------+-----------+
        | TOS, DstMAC, PFVF, Port           |     0x47  |
        +-----------------------------------+-----------+
        | TOS, DstMAC, PFVF                 |     0x46  |
        +-----------------------------------+-----------+
        | TOS, DstMAC, Port                 |     0x45  |
        +-----------------------------------+-----------+
        | TOS, DstMAC                       |     0x44  |
        +-----------------------------------+-----------+
        | TOS, PFVF, Port                   |     0x43  |
        +-----------------------------------+-----------+
        | TOS, PFVF                         |     0x42  |
        +-----------------------------------+-----------+
        | TOS, Port                         |     0x41  |
        +-----------------------------------+-----------+
        | TOS                               |     0x40  |
        +-----------------------------------+-----------+
        | Outer VLAN, Inner VLAN, Port      |     0x31  |
        +-----------------------------------+-----------+
        | Outer VLAN, Ethertype, Port       |     0x29  |
        +-----------------------------------+-----------+
        | Outer VLAN, Ethertype             |     0x28  |
        +-----------------------------------+-----------+
        | Outer VLAN, DstMAC, Port          |     0x25  |
        +-----------------------------------+-----------+
        | Outer VLAN, DstMAC                |     0x24  |
        +-----------------------------------+-----------+
        | Outer VLAN, Port                  |     0x21  |
        +-----------------------------------+-----------+
        | Outer VLAN                        |     0x20  |
        +-----------------------------------+-----------+
        | Inner VLAN, Ethertype, Port       |     0x19  |
        +-----------------------------------+-----------+
        | Inner VLAN, Ethertype             |     0x18  |
        +-----------------------------------+-----------+
        | Inner VLAN, DstMAC, Port          |     0x15  |
        +-----------------------------------+-----------+
        | Inner VLAN, DstMAC                |     0x14  |
        +-----------------------------------+-----------+
        | Inner VLAN, Port                  |     0x11  |
        +-----------------------------------+-----------+
        | Inner VLAN                        |     0x10  |
        +-----------------------------------+-----------+
        | Ethertype, DstMAC, Port           |     0xD   |
        +-----------------------------------+-----------+
        | Ethertype, DstMAC                 |     0xC   |
        +-----------------------------------+-----------+
        | Ethertype, PFVF, Port             |     0xB   |
        +-----------------------------------+-----------+
        | Ethertype, PFVF                   |     0xA   |
        +-----------------------------------+-----------+
        | Ethertype, Port                   |     0x9   |
        +-----------------------------------+-----------+
        | Ethertype                         |     0x8   |
        +-----------------------------------+-----------+
        | DstMAC, PFVF, Port                |     0x7   |
        +-----------------------------------+-----------+
        | DstMAC, PFVF                      |     0x6   |
        +-----------------------------------+-----------+
        | DstMAC, Port                      |     0x5   |
        +-----------------------------------+-----------+
        | Destination MAC                   |     0x4   |
        +-----------------------------------+-----------+
        | PFVF, Port                        |     0x3   |
        +-----------------------------------+-----------+
        | PFVF                              |     0x2   |
        +-----------------------------------+-----------+
        | Physical Port                     |     0x1   +
        +-----------------------------------+-----------+

  For example, to enable matching ``ethertype`` field in Ethernet
  header, and ``protocol`` field in IPv4 header, the ``filtermode``
  combination must be given as:

  .. code-block:: console

     dpdk-testpmd -a 02:00.4,filtermode=0x88 -- -i

- ``filtermask`` (default **0**)

  ``filtermask`` devarg works similar to ``filtermode``, but is used
  to configure a filter mode combination for HASH (exact-match) filters.

  .. note::

     The combination chosen for ``filtermask`` devarg **must be a subset** of
     the combination chosen for ``filtermode`` devarg.

  Default value of **0** makes driver pick the combination configured in
  the firmware configuration file on the adapter.

  Note that the filter rule will only be inserted in HASH region, if the
  rule contains **all** the fields specified in the ``filtermask`` combination.
  Otherwise, the filter rule will get inserted in LETCAM region.

  The same combination list explained in the tables in ``filtermode`` devarg
  section earlier applies for ``filtermask`` devarg, as well.

  For example, to enable matching only protocol field in IPv4 header, the
  ``filtermask`` combination must be given as:

  .. code-block:: console

     dpdk-testpmd -a 02:00.4,filtermode=0x88,filtermask=0x80 -- -i

.. _driver-compilation:

Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

Linux
-----

.. _linux-installation:

Linux Installation
~~~~~~~~~~~~~~~~~~

Steps to manually install the latest firmware from the downloaded Chelsio
Unified Wire package for Linux operating system are as follows:

#. Load the kernel module:

   .. code-block:: console

      modprobe cxgb4

#. Use ifconfig to get the interface name assigned to Chelsio card:

   .. code-block:: console

      ifconfig -a | grep "00:07:43"

   Example output:

   .. code-block:: console

      p1p1      Link encap:Ethernet  HWaddr 00:07:43:2D:EA:C0
      p1p2      Link encap:Ethernet  HWaddr 00:07:43:2D:EA:C8

#. Install cxgbtool:

   .. code-block:: console

      cd <path_to_uwire>/tools/cxgbtool
      make install

#. Use cxgbtool to load the firmware config file onto the card:

   .. code-block:: console

      cxgbtool p1p1 loadcfg <path_to_uwire>/src/network/firmware/t5-config.txt

#. Use cxgbtool to load the firmware image onto the card:

   .. code-block:: console

      cxgbtool p1p1 loadfw <path_to_uwire>/src/network/firmware/t5fw-*.bin

#. Unload and reload the kernel module:

   .. code-block:: console

      modprobe -r cxgb4
      modprobe cxgb4

#. Verify with ethtool:

   .. code-block:: console

      ethtool -i p1p1 | grep "firmware"

   Example output:

   .. code-block:: console

      firmware-version: 1.25.6.0, TP 0.1.23.2

Running testpmd
~~~~~~~~~~~~~~~

This section demonstrates how to launch **testpmd** with Chelsio
devices managed by librte_net_cxgbe in Linux operating system.

#. Load the kernel module:

   .. code-block:: console

      modprobe cxgb4

#. Get the PCI bus addresses of the interfaces bound to cxgb4 driver:

   .. code-block:: console

      dmesg | tail -2

   Example output:

   .. code-block:: console

      cxgb4 0000:02:00.4 p1p1: renamed from eth0
      cxgb4 0000:02:00.4 p1p2: renamed from eth1

   .. note::

      Both the interfaces of a Chelsio 2-port adapter are bound to the
      same PCI bus address.

#. Unload the kernel module:

   .. code-block:: console

      modprobe -ar cxgb4 csiostor

#. Running testpmd

   Follow instructions available in the document
   :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
   to run testpmd.

   .. note::

      Currently, CXGBE PMD only supports the binding of PF4 for Chelsio NICs.

   Example output:

   .. code-block:: console

      [...]
      EAL: PCI device 0000:02:00.4 on NUMA socket -1
      EAL:   probe driver: 1425:5401 rte_cxgbe_pmd
      EAL:   PCI memory mapped at 0x7fd7c0200000
      EAL:   PCI memory mapped at 0x7fd77cdfd000
      EAL:   PCI memory mapped at 0x7fd7c10b7000
      PMD: rte_cxgbe_pmd: fw: 1.25.6.0, TP: 0.1.23.2
      PMD: rte_cxgbe_pmd: Coming up as MASTER: Initializing adapter
      Interactive-mode selected
      Configuring Port 0 (socket 0)
      Port 0: 00:07:43:2D:EA:C0
      Configuring Port 1 (socket 0)
      Port 1: 00:07:43:2D:EA:C8
      Checking link statuses...
      PMD: rte_cxgbe_pmd: Port0: passive DA port module inserted
      PMD: rte_cxgbe_pmd: Port1: passive DA port module inserted
      Port 0 Link Up - speed 10000 Mbps - full-duplex
      Port 1 Link Up - speed 10000 Mbps - full-duplex
      Done
      testpmd>

   .. note::

      Flow control pause TX/RX is disabled by default and can be enabled via
      testpmd. Refer section :ref:`flow-control` for more details.

Configuring SR-IOV Virtual Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This section demonstrates how to enable SR-IOV virtual functions
on Chelsio NICs and demonstrates how to run testpmd with SR-IOV
virtual functions.

#. Load the kernel module:

   .. code-block:: console

      modprobe cxgb4

#. Get the PCI bus addresses of the interfaces bound to cxgb4 driver:

   .. code-block:: console

      dmesg | tail -2

   Example output:

   .. code-block:: console

      cxgb4 0000:02:00.4 p1p1: renamed from eth0
      cxgb4 0000:02:00.4 p1p2: renamed from eth1

   .. note::

      Both the interfaces of a Chelsio 2-port adapter are bound to the
      same PCI bus address.

#. Use ifconfig to get the interface name assigned to Chelsio card:

   .. code-block:: console

      ifconfig -a | grep "00:07:43"

   Example output:

   .. code-block:: console

      p1p1      Link encap:Ethernet  HWaddr 00:07:43:2D:EA:C0
      p1p2      Link encap:Ethernet  HWaddr 00:07:43:2D:EA:C8

#. Bring up the interfaces:

   .. code-block:: console

      ifconfig p1p1 up
      ifconfig p1p2 up

#. Instantiate SR-IOV Virtual Functions. PF0..3 can be used for
   SR-IOV VFs. Multiple VFs can be instantiated on each of PF0..3.
   To instantiate one SR-IOV VF on each PF0 and PF1:

   .. code-block:: console

      echo 1 > /sys/bus/pci/devices/0000\:02\:00.0/sriov_numvfs
      echo 1 > /sys/bus/pci/devices/0000\:02\:00.1/sriov_numvfs

#. Get the PCI bus addresses of the virtual functions:

   .. code-block:: console

      lspci | grep -i "Chelsio" | grep -i "VF"

   Example output:

   .. code-block:: console

      02:01.0 Ethernet controller: Chelsio Communications Inc T540-CR Unified Wire Ethernet Controller [VF]
      02:01.1 Ethernet controller: Chelsio Communications Inc T540-CR Unified Wire Ethernet Controller [VF]

#. Running testpmd

   Follow instructions available in the document
   :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
   to bind virtual functions and run testpmd.

   Example output:

   .. code-block:: console

      [...]
      EAL: PCI device 0000:02:01.0 on NUMA socket 0
      EAL:   probe driver: 1425:5803 net_cxgbevf
      PMD: rte_cxgbe_pmd: Firmware version: 1.25.6.0
      PMD: rte_cxgbe_pmd: TP Microcode version: 0.1.23.2
      PMD: rte_cxgbe_pmd: Chelsio rev 0
      PMD: rte_cxgbe_pmd: No bootstrap loaded
      PMD: rte_cxgbe_pmd: No Expansion ROM loaded
      PMD: rte_cxgbe_pmd:  0000:02:01.0 Chelsio rev 0 1G/10GBASE-SFP
      EAL: PCI device 0000:02:01.1 on NUMA socket 0
      EAL:   probe driver: 1425:5803 net_cxgbevf
      PMD: rte_cxgbe_pmd: Firmware version: 1.25.6.0
      PMD: rte_cxgbe_pmd: TP Microcode version: 0.1.23.2
      PMD: rte_cxgbe_pmd: Chelsio rev 0
      PMD: rte_cxgbe_pmd: No bootstrap loaded
      PMD: rte_cxgbe_pmd: No Expansion ROM loaded
      PMD: rte_cxgbe_pmd:  0000:02:01.1 Chelsio rev 0 1G/10GBASE-SFP
      Configuring Port 0 (socket 0)
      Port 0: 06:44:29:44:40:00
      Configuring Port 1 (socket 0)
      Port 1: 06:44:29:44:40:10
      Checking link statuses...
      Done
      testpmd>

FreeBSD
-------

.. _freebsd-installation:

FreeBSD Installation
~~~~~~~~~~~~~~~~~~~~

Steps to manually install the latest firmware from the downloaded Chelsio
Unified Wire package for FreeBSD operating system are as follows:

#. Load the kernel module:

   .. code-block:: console

      kldload if_cxgbe

#. Use dmesg to get the t5nex instance assigned to the Chelsio card:

   .. code-block:: console

      dmesg | grep "t5nex"

   Example output:

   .. code-block:: console

      t5nex0: <Chelsio T520-CR> irq 16 at device 0.4 on pci2
      cxl0: <port 0> on t5nex0
      cxl1: <port 1> on t5nex0
      t5nex0: PCIe x8, 2 ports, 14 MSI-X interrupts, 31 eq, 13 iq

   In the example above, a Chelsio T520-CR card is bound to a t5nex0 instance.

#. Install cxgbetool from FreeBSD source repository:

   .. code-block:: console

      cd <path_to_FreeBSD_source>/tools/tools/cxgbetool/
      make && make install

#. Use cxgbetool to load the firmware image onto the card:

   .. code-block:: console

      cxgbetool t5nex0 loadfw <path_to_uwire>/src/network/firmware/t5fw-*.bin

#. Unload and reload the kernel module:

   .. code-block:: console

      kldunload if_cxgbe
      kldload if_cxgbe

#. Verify with sysctl:

   .. code-block:: console

      sysctl -a | grep "t5nex" | grep "firmware"

   Example output:

   .. code-block:: console

      dev.t5nex.0.firmware_version: 1.25.6.0

Running testpmd
~~~~~~~~~~~~~~~

This section demonstrates how to launch **testpmd** with Chelsio
devices managed by librte_net_cxgbe in FreeBSD operating system.

#. Change to DPDK source directory where the target has been compiled in
   section :ref:`driver-compilation`:

   .. code-block:: console

      cd <DPDK-source-directory>

#. Copy the contigmem kernel module to /boot/kernel directory:

   .. code-block:: console

      cp <build_dir>/kernel/freebsd/contigmem.ko /boot/kernel/

#. Add the following lines to /boot/loader.conf:

   .. code-block:: console

      # reserve 2 x 1G blocks of contiguous memory using contigmem driver
      hw.contigmem.num_buffers=2
      hw.contigmem.buffer_size=1073741824
      # load contigmem module during boot process
      contigmem_load="YES"

   The above lines load the contigmem kernel module during boot process and
   allocate 2 x 1G blocks of contiguous memory to be used for DPDK later on.
   This is to avoid issues with potential memory fragmentation during later
   system up time, which may result in failure of allocating the contiguous
   memory required for the contigmem kernel module.

#. Restart the system and ensure the contigmem module is loaded successfully:

   .. code-block:: console

      reboot
      kldstat | grep "contigmem"

   Example output:

   .. code-block:: console

      2    1 0xffffffff817f1000 3118     contigmem.ko

#. Repeat step 1 to ensure that you are in the DPDK source directory.

#. Load the cxgbe kernel module:

   .. code-block:: console

      kldload if_cxgbe

#. Get the PCI bus addresses of the interfaces bound to t5nex driver:

   .. code-block:: console

      pciconf -l | grep "t5nex"

   Example output:

   .. code-block:: console

      t5nex0@pci0:2:0:4: class=0x020000 card=0x00001425 chip=0x54011425 rev=0x00

   In the above example, the t5nex0 is bound to 2:0:4 bus address.

   .. note::

      Both the interfaces of a Chelsio 2-port adapter are bound to the
      same PCI bus address.

#. Unload the kernel module:

   .. code-block:: console

      kldunload if_cxgbe

#. Set the PCI bus addresses to hw.nic_uio.bdfs kernel environment parameter:

   .. code-block:: console

      kenv hw.nic_uio.bdfs="2:0:4"

   This automatically binds 2:0:4 to nic_uio kernel driver when it is loaded in
   the next step.

   .. note::

      Currently, CXGBE PMD only supports the binding of PF4 for Chelsio NICs.

#. Load nic_uio kernel driver:

   .. code-block:: console

      kldload <build_dir>/kernel/freebsd/nic_uio.ko

#. Start testpmd with basic parameters:

   .. code-block:: console

      ./<build_dir>/app/dpdk-testpmd -l 0-3 -n 4 -a 0000:02:00.4 -- -i

   Example output:

   .. code-block:: console

      [...]
      EAL: PCI device 0000:02:00.4 on NUMA socket 0
      EAL:   probe driver: 1425:5401 rte_cxgbe_pmd
      EAL:   PCI memory mapped at 0x8007ec000
      EAL:   PCI memory mapped at 0x842800000
      EAL:   PCI memory mapped at 0x80086c000
      PMD: rte_cxgbe_pmd: fw: 1.25.6.0, TP: 0.1.23.2
      PMD: rte_cxgbe_pmd: Coming up as MASTER: Initializing adapter
      Interactive-mode selected
      Configuring Port 0 (socket 0)
      Port 0: 00:07:43:2D:EA:C0
      Configuring Port 1 (socket 0)
      Port 1: 00:07:43:2D:EA:C8
      Checking link statuses...
      PMD: rte_cxgbe_pmd: Port0: passive DA port module inserted
      PMD: rte_cxgbe_pmd: Port1: passive DA port module inserted
      Port 0 Link Up - speed 10000 Mbps - full-duplex
      Port 1 Link Up - speed 10000 Mbps - full-duplex
      Done
      testpmd>

.. note::

   Flow control pause TX/RX is disabled by default and can be enabled via
   testpmd. Refer section :ref:`flow-control` for more details.

Sample Application Notes
------------------------

.. _flow-control:

Enable/Disable Flow Control
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Flow control pause TX/RX is disabled by default and can be enabled via
testpmd as follows:

.. code-block:: console

   testpmd> set flow_ctrl rx on tx on 0 0 0 0 mac_ctrl_frame_fwd off autoneg on 0
   testpmd> set flow_ctrl rx on tx on 0 0 0 0 mac_ctrl_frame_fwd off autoneg on 1

To disable again, run:

.. code-block:: console

   testpmd> set flow_ctrl rx off tx off 0 0 0 0 mac_ctrl_frame_fwd off autoneg off 0
   testpmd> set flow_ctrl rx off tx off 0 0 0 0 mac_ctrl_frame_fwd off autoneg off 1

Jumbo Mode
~~~~~~~~~~

There are two ways to enable sending and receiving of jumbo frames via testpmd.
One method involves using the **mtu** command, which changes the mtu of an
individual port without having to stop the selected port. Another method
involves stopping all the ports first and then running **max-pkt-len** command
to configure the mtu of all the ports with a single command.

- To configure each port individually, run the mtu command as follows:

  .. code-block:: console

     testpmd> port config mtu 0 9000
     testpmd> port config mtu 1 9000

- To configure all the ports at once, stop all the ports first and run the
  max-pkt-len command as follows:

  .. code-block:: console

     testpmd> port stop all
     testpmd> port config all max-pkt-len 9000
