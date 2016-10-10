..  BSD LICENSE
    Copyright (c) 2015 QLogic Corporation
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

BNX2X Poll Mode Driver
======================

The BNX2X poll mode driver library (**librte_pmd_bnx2x**) implements support
for **QLogic 578xx** 10/20 Gbps family of adapters as well as their virtual
functions (VF) in SR-IOV context. It is supported on several standard Linux
distros like Red Hat 7.x and SLES12 OS. It is compile-tested under FreeBSD OS.

More information can be found at `QLogic Corporation's Official Website
<http://www.qlogic.com>`_.

Supported Features
------------------

BNX2X PMD has support for:

- Base L2 features
- Unicast/multicast filtering
- Promiscuous mode
- Port hardware statistics
- SR-IOV VF

Non-supported Features
----------------------

The features not yet supported include:

- TSS (Transmit Side Scaling)
- RSS (Receive Side Scaling)
- LRO/TSO offload
- Checksum offload
- SR-IOV PF
- Rx TX scatter gather

Co-existence considerations
---------------------------

- BCM578xx being a CNA can have both NIC and Storage personalities.
  However, coexistence with storage protocol drivers (cnic, bnx2fc and
  bnx2fi) is not supported on the same adapter. So storage personality
  has to be disabled on that adapter when used in DPDK applications.

- For SR-IOV case, bnx2x PMD will be used to bind to SR-IOV VF device and
  Linux native kernel driver (bnx2x) will be attached to SR-IOV PF.


Supported QLogic NICs
---------------------

- 578xx

Prerequisites
-------------

- Requires firmware version **7.2.51.0**. It is included in most of the
  standard Linux distros. If it is not available visit
  `QLogic Driver Download Center <http://driverdownloads.qlogic.com>`_
  to get the required firmware.

Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``.config`` file. Please note that
enabling debugging options may affect system performance.

- ``CONFIG_RTE_LIBRTE_BNX2X_PMD`` (default **y**)

  Toggle compilation of bnx2x driver.

- ``CONFIG_RTE_LIBRTE_BNX2X_DEBUG`` (default **n**)

  Toggle display of generic debugging messages.

- ``CONFIG_RTE_LIBRTE_BNX2X_DEBUG_INIT`` (default **n**)

  Toggle display of initialization related messages.

- ``CONFIG_RTE_LIBRTE_BNX2X_DEBUG_TX`` (default **n**)

  Toggle display of transmit fast path run-time messages.

- ``CONFIG_RTE_LIBRTE_BNX2X_DEBUG_RX`` (default **n**)

  Toggle display of receive fast path run-time messages.

- ``CONFIG_RTE_LIBRTE_BNX2X_DEBUG_PERIODIC`` (default **n**)

  Toggle display of register reads and writes.


.. _bnx2x_driver-compilation:

Driver Compilation
~~~~~~~~~~~~~~~~~~

BNX2X PMD for Linux x86_64 gcc target, run the following "make"
command::

   cd <DPDK-source-directory>
   make config T=x86_64-native-linuxapp-gcc install

To compile BNX2X PMD for Linux x86_64 clang target, run the following "make"
command::

   cd <DPDK-source-directory>
   make config T=x86_64-native-linuxapp-clang install

To compile BNX2X PMD for Linux i686 gcc target, run the following "make"
command::

   cd <DPDK-source-directory>
   make config T=i686-native-linuxapp-gcc install

To compile BNX2X PMD for Linux i686 gcc target, run the following "make"
command:

.. code-block:: console

   cd <DPDK-source-directory>
   make config T=i686-native-linuxapp-gcc install

To compile BNX2X PMD for FreeBSD x86_64 clang target, run the following "gmake"
command::

   cd <DPDK-source-directory>
   gmake config T=x86_64-native-bsdapp-clang install

To compile BNX2X PMD for FreeBSD x86_64 gcc target, run the following "gmake"
command::

   cd <DPDK-source-directory>
   gmake config T=x86_64-native-bsdapp-gcc install -Wl,-rpath=/usr/local/lib/gcc49 CC=gcc49

To compile BNX2X PMD for FreeBSD x86_64 gcc target, run the following "gmake"
command:

.. code-block:: console

   cd <DPDK-source-directory>
   gmake config T=x86_64-native-bsdapp-gcc install -Wl,-rpath=/usr/local/lib/gcc49 CC=gcc49

Linux
-----

.. _bnx2x_Linux-installation:

Linux Installation
~~~~~~~~~~~~~~~~~~

Sample Application Notes
~~~~~~~~~~~~~~~~~~~~~~~~

This section demonstrates how to launch ``testpmd`` with QLogic 578xx
devices managed by ``librte_pmd_bnx2x`` in Linux operating system.

#. Request huge pages:

   .. code-block:: console

      echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages/nr_hugepages

#. Load ``igb_uio`` or ``vfio-pci`` driver:

   .. code-block:: console

      insmod ./x86_64-native-linuxapp-gcc/kmod/igb_uio.ko

   or

   .. code-block:: console

      modprobe vfio-pci

#. Bind the QLogic adapters to ``igb_uio`` or ``vfio-pci`` loaded in the
   previous step::

      ./tools/dpdk-devbind.py --bind igb_uio 0000:84:00.0 0000:84:00.1

   or

   Setup VFIO permissions for regular users and then bind to ``vfio-pci``:

   .. code-block:: console

      sudo chmod a+x /dev/vfio

      sudo chmod 0666 /dev/vfio/*

      ./tools/dpdk-devbind.py --bind vfio-pci 0000:84:00.0 0000:84:00.1

#. Start ``testpmd`` with basic parameters:

   .. code-block:: console

      ./x86_64-native-linuxapp-gcc/app/testpmd -c 0xf -n 4 -- -i

   Example output:

   .. code-block:: console

      [...]
      EAL: PCI device 0000:84:00.0 on NUMA socket 1
      EAL:   probe driver: 14e4:168e rte_bnx2x_pmd
      EAL:   PCI memory mapped at 0x7f14f6fe5000
      EAL:   PCI memory mapped at 0x7f14f67e5000
      EAL:   PCI memory mapped at 0x7f15fbd9b000
      EAL: PCI device 0000:84:00.1 on NUMA socket 1
      EAL:   probe driver: 14e4:168e rte_bnx2x_pmd
      EAL:   PCI memory mapped at 0x7f14f5fe5000
      EAL:   PCI memory mapped at 0x7f14f57e5000
      EAL:   PCI memory mapped at 0x7f15fbd4f000
      Interactive-mode selected
      Configuring Port 0 (socket 0)
      PMD: bnx2x_dev_tx_queue_setup(): fp[00] req_bd=512, thresh=512,
                   usable_bd=1020, total_bd=1024,
                                tx_pages=4
      PMD: bnx2x_dev_rx_queue_setup(): fp[00] req_bd=128, thresh=0,
                   usable_bd=510, total_bd=512,
                                rx_pages=1, cq_pages=8
      PMD: bnx2x_print_adapter_info():
      [...]
      Checking link statuses...
      Port 0 Link Up - speed 10000 Mbps - full-duplex
      Port 1 Link Up - speed 10000 Mbps - full-duplex
      Done
      testpmd>

SR-IOV: Prerequisites and sample Application Notes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This section provides instructions to configure SR-IOV with Linux OS.

#. Verify SR-IOV and ARI capabilities are enabled on the adapter using ``lspci``:

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

      modprobe bnx2x

   Example output:

   .. code-block:: console

      systemd-udevd[4848]: renamed network interface eth0 to ens5f0
      systemd-udevd[4848]: renamed network interface eth1 to ens5f1

#. Bring up the PF ports:

   .. code-block:: console

      ifconfig ens5f0 up
      ifconfig ens5f1 up

#. Create VF device(s):

   Echo the number of VFs to be created into "sriov_numvfs" sysfs entry
   of the parent PF.

   Example output:

   .. code-block:: console

      echo 2 > /sys/devices/pci0000:00/0000:00:03.0/0000:81:00.0/sriov_numvfs


#. Assign VF MAC address:

   Assign MAC address to the VF using iproute2 utility. The syntax is:
   ip link set <PF iface> vf <VF id> mac <macaddr>

   Example output:

   .. code-block:: console

      ip link set ens5f0 vf 0 mac 52:54:00:2f:9d:e8


#. PCI Passthrough:

   The VF devices may be passed through to the guest VM using virt-manager or
   virsh etc. bnx2x PMD should be used to bind the VF devices in the guest VM
   using the instructions outlined in the Application notes below.
