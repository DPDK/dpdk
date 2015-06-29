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
for **Chelsio T5** 10/40 Gbps family of adapters.

More information can be found at `Chelsio Communications
<http://www.chelsio.com>`_.

Features
--------

CXGBE PMD has the support for:

- Multiple queues for TX and RX.
- Receiver Side Steering (RSS).
- VLAN filtering.
- Checksum offload.
- Promiscuous mode.
- All multicast mode.
- Port hardware statistics.

Limitations
-----------

The Chelsio T5 devices provide two/four ports but expose a single PCI bus
address, thus, librte_pmd_cxgbe registers itself as a
PCI driver that allocates one Ethernet device per detected port.

For this reason, one cannot white/blacklist a single port without also
white/blacklisting the others on the same device.

Configuration
-------------

Compiling CXGBE PMD
~~~~~~~~~~~~~~~~~~~

These options can be modified in the ``.config`` file.

- ``CONFIG_RTE_LIBRTE_CXGBE_PMD`` (default **y**)

  Toggle compilation of librte_pmd_cxgbe driver.

- ``CONFIG_RTE_LIBRTE_CXGBE_DEBUG`` (default **n**)

  Toggle debugging code. Enabling this option adds additional generic debugging
  messages at the cost of lower performance.

- ``CONFIG_RTE_LIBRTE_CXGBE_DEBUG_REG`` (default **n**)

  Toggle debugging code. Enabling this option adds additional registers related
  run-time checks and debugging messages at the cost of lower performance.

- ``CONFIG_RTE_LIBRTE_CXGBE_DEBUG_MBOX`` (default **n**)

  Toggle debugging code. Enabling this option adds additional firmware mailbox
  related run-time checks and debugging messages at the cost of lower
  performance.

- ``CONFIG_RTE_LIBRTE_CXGBE_DEBUG_TX`` (default **n**)

  Toggle debugging code. Enabling this option adds additional transmission data
  path run-time checks and debugging messages at the cost of lower performance.

- ``CONFIG_RTE_LIBRTE_CXGBE_DEBUG_RX`` (default **n**)

  Toggle debugging code. Enabling this option adds additional receiving data
  path run-time checks and debugging messages at the cost of lower performance.

Prerequisites
-------------

- Requires firmware version **1.13.32.0** and higher. Visit
  `Chelsio Communications <http://www.chelsio.com>`_ to get latest firmware.

Sample Application Notes
-------------------------

This section demonstrates how to launch **testpmd** with Chelsio T5
devices managed by librte_pmd_cxgbe.

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
      PMD: rte_cxgbe_pmd: fw: 1.13.33.0, TP: 0.1.4.8
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
