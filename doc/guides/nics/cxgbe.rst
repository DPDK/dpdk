..  BSD LICENSE
    Copyright 2015 Chelsio Communications.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Chelsio Communications nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

CXGBE Poll Mode Driver
======================

The CXGBE PMD (**librte_pmd_cxgbe**) provides poll mode driver support
for **Chelsio T5** 10/40 Gbps family of adapters. CXGBE PMD has support
for the latest Linux and FreeBSD operating systems.

More information can be found at `Chelsio Communications Official Website
<http://www.chelsio.com>`_.

Features
--------

CXGBE PMD has support for:

- Multiple queues for TX and RX
- Receiver Side Steering (RSS)
- VLAN filtering
- Checksum offload
- Promiscuous mode
- All multicast mode
- Port hardware statistics

Limitations
-----------

The Chelsio T5 devices provide two/four ports but expose a single PCI bus
address, thus, librte_pmd_cxgbe registers itself as a
PCI driver that allocates one Ethernet device per detected port.

For this reason, one cannot whitelist/blacklist a single port without
whitelisting/blacklisting the other ports on the same device.

Supported Chelsio T5 NICs
-------------------------

- 1G NICs: T502-BT
- 10G NICs: T520-BT, T520-CR, T520-LL-CR, T520-SO-CR, T540-CR
- 40G NICs: T580-CR, T580-LP-CR, T580-SO-CR
- Other T5 NICs: T522-CR

Prerequisites
-------------

- Requires firmware version **1.13.32.0** and higher. Visit
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

.. _driver-compilation:

Driver Compilation
~~~~~~~~~~~~~~~~~~

To compile CXGBE PMD for Linux x86_64 gcc target, run the following "make"
command:

.. code-block:: console

   cd <DPDK-source-directory>
   make config T=x86_64-native-linuxapp-gcc install

To compile CXGBE PMD for FreeBSD x86_64 clang target, run the following "gmake"
command:

.. code-block:: console

   cd <DPDK-source-directory>
   gmake config T=x86_64-native-bsdapp-clang install

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

      firmware-version: 1.13.32.0, TP 0.1.4.8

Sample Application Notes
~~~~~~~~~~~~~~~~~~~~~~~~

This section demonstrates how to launch **testpmd** with Chelsio T5
devices managed by librte_pmd_cxgbe in Linux operating system.

#. Change to DPDK source directory where the target has been compiled in
   section :ref:`driver-compilation`:

   .. code-block:: console

      cd <DPDK-source-directory>

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

      Both the interfaces of a Chelsio T5 2-port adapter are bound to the
      same PCI bus address.

#. Unload the kernel module:

   .. code-block:: console

      modprobe -ar cxgb4 csiostor

#. Request huge pages:

   .. code-block:: console

      echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages/nr_hugepages

#. Load igb_uio or vfio-pci driver:

   .. code-block:: console

      insmod ./x86_64-native-linuxapp-gcc/kmod/igb_uio.ko

   or

   .. code-block:: console

      modprobe vfio-pci

#. Bind the Chelsio T5 adapters to igb_uio or vfio-pci loaded in the previous
   step:

   .. code-block:: console

      ./tools/dpdk_nic_bind.py --bind igb_uio 0000:02:00.4

   or

   Setup VFIO permissions for regular users and then bind to vfio-pci:

   .. code-block:: console

      sudo chmod a+x /dev/vfio

      sudo chmod 0666 /dev/vfio/*

      ./tools/dpdk_nic_bind.py --bind vfio-pci 0000:02:00.4

   .. note::

      Currently, CXGBE PMD only supports the binding of PF4 for Chelsio T5 NICs.

#. Start testpmd with basic parameters:

   .. code-block:: console

      ./x86_64-native-linuxapp-gcc/app/testpmd -c 0xf -n 4 -w 0000:02:00.4 -- -i

   Example output:

   .. code-block:: console

      [...]
      EAL: PCI device 0000:02:00.4 on NUMA socket -1
      EAL:   probe driver: 1425:5401 rte_cxgbe_pmd
      EAL:   PCI memory mapped at 0x7fd7c0200000
      EAL:   PCI memory mapped at 0x7fd77cdfd000
      EAL:   PCI memory mapped at 0x7fd7c10b7000
      PMD: rte_cxgbe_pmd: fw: 1.13.32.0, TP: 0.1.4.8
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
   testpmd as follows:

   .. code-block:: console

      testpmd> set flow_ctrl rx on tx on 0 0 0 0 mac_ctrl_frame_fwd off autoneg on 0
      testpmd> set flow_ctrl rx on tx on 0 0 0 0 mac_ctrl_frame_fwd off autoneg on 1

   To disable again, use:

   .. code-block:: console

      testpmd> set flow_ctrl rx off tx off 0 0 0 0 mac_ctrl_frame_fwd off autoneg off 0
      testpmd> set flow_ctrl rx off tx off 0 0 0 0 mac_ctrl_frame_fwd off autoneg off 1

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

      dev.t5nex.0.firmware_version: 1.13.32.0

Sample Application Notes
~~~~~~~~~~~~~~~~~~~~~~~~

This section demonstrates how to launch **testpmd** with Chelsio T5
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

      Both the interfaces of a Chelsio T5 2-port adapter are bound to the
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

      Currently, CXGBE PMD only supports the binding of PF4 for Chelsio T5 NICs.

#. Load nic_uio kernel driver:

   .. code-block:: console

      kldload ./x86_64-native-bsdapp-clang/kmod/nic_uio.ko

#. Start testpmd with basic parameters:

   .. code-block:: console

      ./x86_64-native-bsdapp-clang/app/testpmd -c 0xf -n 4 -w 0000:02:00.4 -- -i

   Example output:

   .. code-block:: console

      [...]
      EAL: PCI device 0000:02:00.4 on NUMA socket 0
      EAL:   probe driver: 1425:5401 rte_cxgbe_pmd
      EAL:   PCI memory mapped at 0x8007ec000
      EAL:   PCI memory mapped at 0x842800000
      EAL:   PCI memory mapped at 0x80086c000
      PMD: rte_cxgbe_pmd: fw: 1.13.32.0, TP: 0.1.4.8
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
   testpmd as follows:

   .. code-block:: console

      testpmd> set flow_ctrl rx on tx on 0 0 0 0 mac_ctrl_frame_fwd off autoneg on 0
      testpmd> set flow_ctrl rx on tx on 0 0 0 0 mac_ctrl_frame_fwd off autoneg on 1

   To disable again, use:

   .. code-block:: console

      testpmd> set flow_ctrl rx off tx off 0 0 0 0 mac_ctrl_frame_fwd off autoneg off 0
      testpmd> set flow_ctrl rx off tx off 0 0 0 0 mac_ctrl_frame_fwd off autoneg off 1
