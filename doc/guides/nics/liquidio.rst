..  BSD LICENSE
    Copyright(c) 2017 Cavium, Inc.. All rights reserved.
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
    * Neither the name of Cavium, Inc. nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER(S) OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

LiquidIO VF Poll Mode Driver
============================

The LiquidIO VF PMD library (librte_pmd_lio) provides poll mode driver support for
Cavium LiquidIO® II server adapter VFs. PF management and VF creation can be
done using kernel driver.

More information can be found at `Cavium Official Website
<http://cavium.com/LiquidIO_Adapters.html>`_.

Supported LiquidIO Adapters
-----------------------------

- LiquidIO II CN2350 210SV/225SV
- LiquidIO II CN2360 210SV/225SV


Pre-Installation Configuration
------------------------------

The following options can be modified in the ``config`` file.
Please note that enabling debugging options may affect system performance.

- ``CONFIG_RTE_LIBRTE_LIO_PMD`` (default ``y``)

  Toggle compilation of LiquidIO PMD.

- ``CONFIG_RTE_LIBRTE_LIO_DEBUG_DRIVER`` (default ``n``)

  Toggle display of generic debugging messages.

- ``CONFIG_RTE_LIBRTE_LIO_DEBUG_INIT`` (default ``n``)

  Toggle display of initialization related messages.

- ``CONFIG_RTE_LIBRTE_LIO_DEBUG_RX`` (default ``n``)

  Toggle display of receive fast path run-time messages.

- ``CONFIG_RTE_LIBRTE_LIO_DEBUG_TX`` (default ``n``)

  Toggle display of transmit fast path run-time messages.

- ``CONFIG_RTE_LIBRTE_LIO_DEBUG_MBOX`` (default ``n``)

  Toggle display of mailbox messages.

- ``CONFIG_RTE_LIBRTE_LIO_DEBUG_REGS`` (default ``n``)

  Toggle display of register reads and writes.


SR-IOV: Prerequisites and Sample Application Notes
--------------------------------------------------

This section provides instructions to configure SR-IOV with Linux OS.

#. Verify SR-IOV and ARI capabilities are enabled on the adapter using ``lspci``:

   .. code-block:: console

      lspci -s <slot> -vvv

   Example output:

   .. code-block:: console

      [...]
      Capabilities: [148 v1] Alternative Routing-ID Interpretation (ARI)
      [...]
      Capabilities: [178 v1] Single Root I/O Virtualization (SR-IOV)
      [...]
      Kernel driver in use: LiquidIO

#. Load the kernel module:

   .. code-block:: console

      modprobe liquidio

#. Bring up the PF ports:

   .. code-block:: console

      ifconfig p4p1 up
      ifconfig p4p2 up

#. Change PF MTU if required:

   .. code-block:: console

      ifconfig p4p1 mtu 9000
      ifconfig p4p2 mtu 9000

#. Create VF device(s):

   Echo number of VFs to be created into ``"sriov_numvfs"`` sysfs entry
   of the parent PF.

   .. code-block:: console

      echo 1 > /sys/bus/pci/devices/0000:03:00.0/sriov_numvfs
      echo 1 > /sys/bus/pci/devices/0000:03:00.1/sriov_numvfs

#. Assign VF MAC address:

   Assign MAC address to the VF using iproute2 utility. The syntax is::

      ip link set <PF iface> vf <VF id> mac <macaddr>

   Example output:

   .. code-block:: console

      ip link set p4p1 vf 0 mac F2:A8:1B:5E:B4:66

#. Assign VF(s) to VM.

   The VF devices may be passed through to the guest VM using qemu or
   virt-manager or virsh etc.

   Example qemu guest launch command:

   .. code-block:: console

      ./qemu-system-x86_64 -name lio-vm -machine accel=kvm \
      -cpu host -m 4096 -smp 4 \
      -drive file=<disk_file>,if=none,id=disk1,format=<type> \
      -device virtio-blk-pci,scsi=off,drive=disk1,id=virtio-disk1,bootindex=1 \
      -device vfio-pci,host=03:00.3 -device vfio-pci,host=03:08.3

#. Running testpmd

   Refer to the document
   :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>` to run
   ``testpmd`` application.

   .. note::

      Use ``igb_uio`` instead of ``vfio-pci`` in VM.

   Example output:

   .. code-block:: console

      [...]
      EAL: PCI device 0000:03:00.3 on NUMA socket 0
      EAL:   probe driver: 177d:9712 net_liovf
      EAL:   using IOMMU type 1 (Type 1)
      PMD: net_liovf[03:00.3]INFO: DEVICE : CN23XX VF
      EAL: PCI device 0000:03:08.3 on NUMA socket 0
      EAL:   probe driver: 177d:9712 net_liovf
      PMD: net_liovf[03:08.3]INFO: DEVICE : CN23XX VF
      Interactive-mode selected
      USER1: create a new mbuf pool <mbuf_pool_socket_0>: n=171456, size=2176, socket=0
      Configuring Port 0 (socket 0)
      PMD: net_liovf[03:00.3]INFO: Starting port 0
      Port 0: F2:A8:1B:5E:B4:66
      Configuring Port 1 (socket 0)
      PMD: net_liovf[03:08.3]INFO: Starting port 1
      Port 1: 32:76:CC:EE:56:D7
      Checking link statuses...
      Port 0 Link Up - speed 10000 Mbps - full-duplex
      Port 1 Link Up - speed 10000 Mbps - full-duplex
      Done
      testpmd>


Limitations
-----------

VF MTU
~~~~~~

VF MTU is limited by PF MTU. Raise PF value before configuring VF for larger packet size.

VLAN offload
~~~~~~~~~~~~

Tx VLAN insertion is not supported and consequently VLAN offload feature is
marked partial.

Ring size
~~~~~~~~~

Number of descriptors for Rx/Tx ring should be in the range 128 to 512.

CRC striping
~~~~~~~~~~~~

LiquidIO adapters strip ethernet FCS of every packet coming to the host
interface. So, CRC will be stripped even when the ``rxmode.hw_strip_crc``
member is set to 0 in ``struct rte_eth_conf``.
