..  BSD LICENSE
    Copyright (c) 2016 QLogic Corporation
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
    * Neither the name of QLogic Corporation nor the names of its
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

QEDE Poll Mode Driver
======================

The QEDE poll mode driver library (**librte_pmd_qede**) implements support
for **QLogic FastLinQ QL4xxxx 25G/40G/100G CNA** family of adapters as well
as their virtual functions (VF) in SR-IOV context. It is supported on
several standard Linux distros like RHEL7.x, SLES12.x and Ubuntu.
It is compile-tested under FreeBSD OS.

More information can be found at `QLogic Corporation's Website
<http://www.qlogic.com>`_.

Supported Features
------------------

- Unicast/Multicast filtering
- Promiscuous mode
- Allmulti mode
- Port hardware statistics
- Jumbo frames
- VLAN offload - Filtering and stripping
- Stateless checksum offloads (IPv4/TCP/UDP)
- Multiple Rx/Tx queues
- RSS (with RETA/hash table/key)
- TSS
- Multiple MAC address
- Default pause flow control
- SR-IOV VF
- MTU change
- Multiprocess aware
- Scatter-Gather

Non-supported Features
----------------------

- SR-IOV PF
- Tunneling offloads
- LRO/TSO
- NPAR

Supported QLogic Adapters
-------------------------

- QLogic FastLinQ QL4xxxx 10G/25G/40G/100G CNAs.

Prerequisites
-------------

- Requires firmware version **8.10.x.** and management firmware
  version **8.10.x or higher**. Firmware may be available
  inbox in certain newer Linux distros under the standard directory
  ``E.g. /lib/firmware/qed/qed_init_values-8.10.9.0.bin``

- If the required firmware files are not available then visit
  `QLogic Driver Download Center <http://driverdownloads.qlogic.com>`_.

Performance note
~~~~~~~~~~~~~~~~

- For better performance, it is recommended to use 4K or higher RX/TX rings.

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``.config`` file. Please note that
enabling debugging options may affect system performance.

- ``CONFIG_RTE_LIBRTE_QEDE_PMD`` (default **y**)

  Toggle compilation of QEDE PMD driver.

- ``CONFIG_RTE_LIBRTE_QEDE_DEBUG_INFO`` (default **n**)

  Toggle display of generic debugging messages.

- ``CONFIG_RTE_LIBRTE_QEDE_DEBUG_DRIVER`` (default **n**)

  Toggle display of ecore related messages.

- ``CONFIG_RTE_LIBRTE_QEDE_DEBUG_TX`` (default **n**)

  Toggle display of transmit fast path run-time messages.

- ``CONFIG_RTE_LIBRTE_QEDE_DEBUG_RX`` (default **n**)

  Toggle display of receive fast path run-time messages.

- ``CONFIG_RTE_LIBRTE_QEDE_FW`` (default **""**)

  Gives absolute path of firmware file.
  ``Eg: "/lib/firmware/qed/qed_init_values_zipped-8.10.9.0.bin"``
  Empty string indicates driver will pick up the firmware file
  from the default location.

Driver Compilation
~~~~~~~~~~~~~~~~~~

To compile QEDE PMD for Linux x86_64 gcc target, run the following ``make``
command::

   cd <DPDK-source-directory>
   make config T=x86_64-native-linuxapp-gcc install

To compile QEDE PMD for Linux x86_64 clang target, run the following ``make``
command::

   cd <DPDK-source-directory>
   make config T=x86_64-native-linuxapp-clang install

To compile QEDE PMD for FreeBSD x86_64 clang target, run the following ``gmake``
command::

   cd <DPDK-source-directory>
   gmake config T=x86_64-native-bsdapp-clang install

To compile QEDE PMD for FreeBSD x86_64 gcc target, run the following ``gmake``
command::

   cd <DPDK-source-directory>
   gmake config T=x86_64-native-bsdapp-gcc install -Wl,-rpath=\
                                        /usr/local/lib/gcc49 CC=gcc49


Sample Application Notes
~~~~~~~~~~~~~~~~~~~~~~~~

This section demonstrates how to launch ``testpmd`` with QLogic 4xxxx
devices managed by ``librte_pmd_qede`` in Linux operating system.

#. Request huge pages:

   .. code-block:: console

      echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages/ \
                                                                nr_hugepages

#. Load ``igb_uio`` driver:

   .. code-block:: console

      insmod ./x86_64-native-linuxapp-gcc/kmod/igb_uio.ko

#. Bind the QLogic 4xxxx adapters to ``igb_uio`` loaded in the
   previous step:

   .. code-block:: console

      ./tools/dpdk-devbind.py --bind igb_uio 0000:84:00.0 0000:84:00.1 \
                                              0000:84:00.2 0000:84:00.3

#. Start ``testpmd`` with basic parameters:
   (Enable QEDE_DEBUG_INFO=y to view informational messages)

   .. code-block:: console

      testpmd -c 0xff1 -n 4 -- -i --nb-cores=8 --portmask=0xf --rxd=4096 \
      --txd=4096 --txfreet=4068 --enable-rx-cksum --rxq=4 --txq=4 \
      --rss-ip --rss-udp

      [...]

    EAL: PCI device 0000:84:00.0 on NUMA socket 1
    EAL:   probe driver: 1077:1634 rte_qede_pmd
    EAL:   Not managed by a supported kernel driver, skipped
    EAL: PCI device 0000:84:00.1 on NUMA socket 1
    EAL:   probe driver: 1077:1634 rte_qede_pmd
    EAL:   Not managed by a supported kernel driver, skipped
    EAL: PCI device 0000:88:00.0 on NUMA socket 1
    EAL:   probe driver: 1077:1656 rte_qede_pmd
    EAL:   PCI memory mapped at 0x7f738b200000
    EAL:   PCI memory mapped at 0x7f738b280000
    EAL:   PCI memory mapped at 0x7f738b300000
    PMD: Chip details : BB1
    PMD: Driver version : QEDE PMD 8.7.9.0_1.0.0
    PMD: Firmware version : 8.7.7.0
    PMD: Management firmware version : 8.7.8.0
    PMD: Firmware file : /lib/firmware/qed/qed_init_values_zipped-8.7.7.0.bin
    [QEDE PMD: (84:00.0:dpdk-port-0)]qede_common_dev_init:macaddr \
                                                        00:0e:1e:d2:09:9c
      [...]
    [QEDE PMD: (84:00.0:dpdk-port-0)]qede_tx_queue_setup:txq 0 num_desc 4096 \
                                                tx_free_thresh 4068 socket 0
    [QEDE PMD: (84:00.0:dpdk-port-0)]qede_tx_queue_setup:txq 1 num_desc 4096 \
                                                tx_free_thresh 4068 socket 0
    [QEDE PMD: (84:00.0:dpdk-port-0)]qede_tx_queue_setup:txq 2 num_desc 4096 \
                                                 tx_free_thresh 4068 socket 0
    [QEDE PMD: (84:00.0:dpdk-port-0)]qede_tx_queue_setup:txq 3 num_desc 4096 \
                                                 tx_free_thresh 4068 socket 0
    [QEDE PMD: (84:00.0:dpdk-port-0)]qede_rx_queue_setup:rxq 0 num_desc 4096 \
                                                rx_buf_size=2148 socket 0
    [QEDE PMD: (84:00.0:dpdk-port-0)]qede_rx_queue_setup:rxq 1 num_desc 4096 \
                                                rx_buf_size=2148 socket 0
    [QEDE PMD: (84:00.0:dpdk-port-0)]qede_rx_queue_setup:rxq 2 num_desc 4096 \
                                                rx_buf_size=2148 socket 0
    [QEDE PMD: (84:00.0:dpdk-port-0)]qede_rx_queue_setup:rxq 3 num_desc 4096 \
                                                rx_buf_size=2148 socket 0
    [QEDE PMD: (84:00.0:dpdk-port-0)]qede_dev_start:port 0
    [QEDE PMD: (84:00.0:dpdk-port-0)]qede_dev_start:link status: down
      [...]
    Checking link statuses...
    Port 0 Link Up - speed 25000 Mbps - full-duplex
    Port 1 Link Up - speed 25000 Mbps - full-duplex
    Port 2 Link Up - speed 25000 Mbps - full-duplex
    Port 3 Link Up - speed 25000 Mbps - full-duplex
    Done
    testpmd>


SR-IOV: Prerequisites and Sample Application Notes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This section provides instructions to configure SR-IOV with Linux OS.

**Note**: librte_pmd_qede will be used to bind to SR-IOV VF device and Linux native kernel driver (QEDE) will function as SR-IOV PF driver. Requires PF driver to be 8.10.x.x or higher.

#. Verify SR-IOV and ARI capability is enabled on the adapter using ``lspci``:

   .. code-block:: console

      lspci -s <slot> -vvv

   Example output:

   .. code-block:: console

      [...]
      Capabilities: [1b8 v1] Alternative Routing-ID Interpretation (ARI)
      [...]
      Capabilities: [1c0 v1] Single Root I/O Virtualization (SR-IOV)
      [...]
      Kernel driver in use: igb_uio

#. Load the kernel module:

   .. code-block:: console

      modprobe qede

   Example output:

   .. code-block:: console

      systemd-udevd[4848]: renamed network interface eth0 to ens5f0
      systemd-udevd[4848]: renamed network interface eth1 to ens5f1

#. Bring up the PF ports:

   .. code-block:: console

      ifconfig ens5f0 up
      ifconfig ens5f1 up

#. Create VF device(s):

   Echo the number of VFs to be created into ``"sriov_numvfs"`` sysfs entry
   of the parent PF.

   Example output:

   .. code-block:: console

      echo 2 > /sys/devices/pci0000:00/0000:00:03.0/0000:81:00.0/sriov_numvfs


#. Assign VF MAC address:

   Assign MAC address to the VF using iproute2 utility. The syntax is::

      ip link set <PF iface> vf <VF id> mac <macaddr>

   Example output:

   .. code-block:: console

      ip link set ens5f0 vf 0 mac 52:54:00:2f:9d:e8


#. PCI Passthrough:

   The VF devices may be passed through to the guest VM using ``virt-manager`` or
   ``virsh``. QEDE PMD should be used to bind the VF devices in the guest VM
   using the instructions outlined in the Application notes above.
