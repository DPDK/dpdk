.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2014-2018 Chelsio Communications.
   All rights reserved.

CXGBE Poll Mode Driver
======================

The CXGBE PMD (**librte_pmd_cxgbe**) provides poll mode driver support
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

Limitations
-----------

The Chelsio Terminator series of devices provide two/four ports but
expose a single PCI bus address, thus, librte_pmd_cxgbe registers
itself as a PCI driver that allocates one Ethernet device per detected
port.

For this reason, one cannot whitelist/blacklist a single port without
whitelisting/blacklisting the other ports on the same device.

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

- Requires firmware version **1.17.14.0** and higher. Visit
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

Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``.config`` file. Please note that
enabling debugging options may affect system performance.

- ``CONFIG_RTE_LIBRTE_CXGBE_PMD`` (default **y**)

  Toggle compilation of librte_pmd_cxgbe driver.

  .. note::

     This controls compilation of both CXGBE and CXGBEVF PMD.

- ``CONFIG_RTE_LIBRTE_CXGBE_DEBUG`` (default **n**)

  Toggle display of generic debugging messages.

- ``CONFIG_RTE_LIBRTE_CXGBE_DEBUG_REG`` (default **n**)

  Toggle display of registers related run-time check messages.

- ``CONFIG_RTE_LIBRTE_CXGBE_DEBUG_MBOX`` (default **n**)

  Toggle display of firmware mailbox related run-time check messages.

- ``CONFIG_RTE_LIBRTE_CXGBE_DEBUG_TX`` (default **n**)

  Toggle display of transmission data path run-time check messages.

- ``CONFIG_RTE_LIBRTE_CXGBE_DEBUG_RX`` (default **n**)

  Toggle display of receiving data path run-time check messages.

- ``CONFIG_RTE_LIBRTE_CXGBE_TPUT`` (default **y**)

  Toggle behaviour to prefer Throughput or Latency.

Runtime Options
~~~~~~~~~~~~~~~

The following ``devargs`` options can be enabled at runtime. They must
be passed as part of EAL arguments. For example,

.. code-block:: console

   testpmd -w 02:00.4,keep_ovlan=1 -- -i

- ``keep_ovlan`` (default **0**)

  Toggle behaviour to keep/strip outer VLAN in Q-in-Q packets. If
  enabled, the outer VLAN tag is preserved in Q-in-Q packets. Otherwise,
  the outer VLAN tag is stripped in Q-in-Q packets.

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

      firmware-version: 1.17.14.0, TP 0.1.4.9

Running testpmd
~~~~~~~~~~~~~~~

This section demonstrates how to launch **testpmd** with Chelsio
devices managed by librte_pmd_cxgbe in Linux operating system.

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
      PMD: rte_cxgbe_pmd: fw: 1.17.14.0, TP: 0.1.4.9
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
      PMD: rte_cxgbe_pmd: Firmware version: 1.17.14.0
      PMD: rte_cxgbe_pmd: TP Microcode version: 0.1.4.9
      PMD: rte_cxgbe_pmd: Chelsio rev 0
      PMD: rte_cxgbe_pmd: No bootstrap loaded
      PMD: rte_cxgbe_pmd: No Expansion ROM loaded
      PMD: rte_cxgbe_pmd:  0000:02:01.0 Chelsio rev 0 1G/10GBASE-SFP
      EAL: PCI device 0000:02:01.1 on NUMA socket 0
      EAL:   probe driver: 1425:5803 net_cxgbevf
      PMD: rte_cxgbe_pmd: Firmware version: 1.17.14.0
      PMD: rte_cxgbe_pmd: TP Microcode version: 0.1.4.9
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

      dev.t5nex.0.firmware_version: 1.17.14.0

Running testpmd
~~~~~~~~~~~~~~~

This section demonstrates how to launch **testpmd** with Chelsio
devices managed by librte_pmd_cxgbe in FreeBSD operating system.

#. Change to DPDK source directory where the target has been compiled in
   section :ref:`driver-compilation`:

   .. code-block:: console

      cd <DPDK-source-directory>

#. Copy the contigmem kernel module to /boot/kernel directory:

   .. code-block:: console

      cp x86_64-native-bsdapp-clang/kmod/contigmem.ko /boot/kernel/

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

      kldload ./x86_64-native-bsdapp-clang/kmod/nic_uio.ko

#. Start testpmd with basic parameters:

   .. code-block:: console

      ./x86_64-native-bsdapp-clang/app/testpmd -l 0-3 -n 4 -w 0000:02:00.4 -- -i

   Example output:

   .. code-block:: console

      [...]
      EAL: PCI device 0000:02:00.4 on NUMA socket 0
      EAL:   probe driver: 1425:5401 rte_cxgbe_pmd
      EAL:   PCI memory mapped at 0x8007ec000
      EAL:   PCI memory mapped at 0x842800000
      EAL:   PCI memory mapped at 0x80086c000
      PMD: rte_cxgbe_pmd: fw: 1.17.14.0, TP: 0.1.4.9
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
