..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Cavium, Inc

ThunderX NICVF Poll Mode Driver
===============================

The ThunderX NICVF PMD (**librte_pmd_thunderx_nicvf**) provides poll mode driver
support for the inbuilt NIC found in the **Cavium ThunderX** SoC family
as well as their virtual functions (VF) in SR-IOV context.

More information can be found at `Cavium, Inc Official Website
<http://www.cavium.com/ThunderX_ARM_Processors.html>`_.

Features
--------

Features of the ThunderX PMD are:

- Multiple queues for TX and RX
- Receive Side Scaling (RSS)
- Packet type information
- Checksum offload
- Promiscuous mode
- Multicast mode
- Port hardware statistics
- Jumbo frames
- Link state information
- Setting up link state.
- Scattered and gather for TX and RX
- VLAN stripping
- SR-IOV VF
- NUMA support
- Multi queue set support (up to 96 queues (12 queue sets)) per port
- Skip data bytes

Supported ThunderX SoCs
-----------------------
- CN88xx
- CN81xx
- CN83xx

Prerequisites
-------------
- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file.
Please note that enabling debugging options may affect system performance.

- ``CONFIG_RTE_LIBRTE_THUNDERX_NICVF_PMD`` (default ``y``)

  Toggle compilation of the ``librte_pmd_thunderx_nicvf`` driver.

- ``CONFIG_RTE_LIBRTE_THUNDERX_NICVF_DEBUG_RX`` (default ``n``)

  Toggle asserts of receive fast path.

- ``CONFIG_RTE_LIBRTE_THUNDERX_NICVF_DEBUG_TX`` (default ``n``)

  Toggle asserts of transmit fast path.

Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

To compile the ThunderX NICVF PMD for Linux arm64 gcc,
use arm64-thunderx-linux-gcc as target.

Linux
-----

SR-IOV: Prerequisites and sample Application Notes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Current ThunderX NIC PF/VF kernel modules maps each physical Ethernet port
automatically to virtual function (VF) and presented them as PCIe-like SR-IOV device.
This section provides instructions to configure SR-IOV with Linux OS.

#. Verify PF devices capabilities using ``lspci``:

   .. code-block:: console

      lspci -vvv

   Example output:

   .. code-block:: console

      0002:01:00.0 Ethernet controller: Cavium Networks Device a01e (rev 01)
      ...
      Capabilities: [100 v1] Alternative Routing-ID Interpretation (ARI)
      ...
      Capabilities: [180 v1] Single Root I/O Virtualization (SR-IOV)
      ...
      Kernel driver in use: thunder-nic
      ...

   .. note::

      Unless ``thunder-nic`` driver is in use make sure your kernel config includes ``CONFIG_THUNDER_NIC_PF`` setting.

#. Verify VF devices capabilities and drivers using ``lspci``:

   .. code-block:: console

      lspci -vvv

   Example output:

   .. code-block:: console

      0002:01:00.1 Ethernet controller: Cavium Networks Device 0011 (rev 01)
      ...
      Capabilities: [100 v1] Alternative Routing-ID Interpretation (ARI)
      ...
      Kernel driver in use: thunder-nicvf
      ...

      0002:01:00.2 Ethernet controller: Cavium Networks Device 0011 (rev 01)
      ...
      Capabilities: [100 v1] Alternative Routing-ID Interpretation (ARI)
      ...
      Kernel driver in use: thunder-nicvf
      ...

   .. note::

      Unless ``thunder-nicvf`` driver is in use make sure your kernel config includes ``CONFIG_THUNDER_NIC_VF`` setting.

#. Pass VF device to VM context (PCIe Passthrough):

   The VF devices may be passed through to the guest VM using qemu or
   virt-manager or virsh etc.

   Example qemu guest launch command:

   .. code-block:: console

      sudo qemu-system-aarch64 -name vm1 \
      -machine virt,gic_version=3,accel=kvm,usb=off \
      -cpu host -m 4096 \
      -smp 4,sockets=1,cores=8,threads=1 \
      -nographic -nodefaults \
      -kernel <kernel image> \
      -append "root=/dev/vda console=ttyAMA0 rw hugepagesz=512M hugepages=3" \
      -device vfio-pci,host=0002:01:00.1 \
      -drive file=<rootfs.ext3>,if=none,id=disk1,format=raw  \
      -device virtio-blk-device,scsi=off,drive=disk1,id=virtio-disk1,bootindex=1 \
      -netdev tap,id=net0,ifname=tap0,script=/etc/qemu-ifup_thunder \
      -device virtio-net-device,netdev=net0 \
      -serial stdio \
      -mem-path /dev/hugepages

#. Enable **VFIO-NOIOMMU** mode (optional):

   .. code-block:: console

      echo 1 > /sys/module/vfio/parameters/enable_unsafe_noiommu_mode

   .. note::

      **VFIO-NOIOMMU** is required only when running in VM context and should not be enabled otherwise.

#. Running testpmd:

   Follow instructions available in the document
   :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
   to run testpmd.

   Example output:

   .. code-block:: console

      ./arm64-thunderx-linux-gcc/app/testpmd -l 0-3 -n 4 -w 0002:01:00.2 \
        -- -i --no-flush-rx \
        --port-topology=loop

      ...

      PMD: rte_nicvf_pmd_init(): librte_pmd_thunderx nicvf version 1.0

      ...
      EAL:   probe driver: 177d:11 rte_nicvf_pmd
      EAL:   using IOMMU type 1 (Type 1)
      EAL:   PCI memory mapped at 0x3ffade50000
      EAL: Trying to map BAR 4 that contains the MSI-X table.
           Trying offsets: 0x40000000000:0x0000, 0x10000:0x1f0000
      EAL:   PCI memory mapped at 0x3ffadc60000
      PMD: nicvf_eth_dev_init(): nicvf: device (177d:11) 2:1:0:2
      PMD: nicvf_eth_dev_init(): node=0 vf=1 mode=tns-bypass sqs=false
           loopback_supported=true
      PMD: nicvf_eth_dev_init(): Port 0 (177d:11) mac=a6:c6:d9:17:78:01
      Interactive-mode selected
      Configuring Port 0 (socket 0)
      ...

      PMD: nicvf_dev_configure(): Configured ethdev port0 hwcap=0x0
      Port 0: A6:C6:D9:17:78:01
      Checking link statuses...
      Port 0 Link Up - speed 10000 Mbps - full-duplex
      Done
      testpmd>

Multiple Queue Set per DPDK port configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There are two types of VFs:

- Primary VF
- Secondary VF

Each port consists of a primary VF and n secondary VF(s). Each VF provides 8 Tx/Rx queues to a port.
When a given port is configured to use more than 8 queues, it requires one (or more) secondary VF.
Each secondary VF adds 8 additional queues to the queue set.

During PMD driver initialization, the primary VF's are enumerated by checking the
specific flag (see sqs message in DPDK boot log - sqs indicates secondary queue set).
They are at the beginning of VF list (the remain ones are secondary VF's).

The primary VFs are used as master queue sets. Secondary VFs provide
additional queue sets for primary ones. If a port is configured for more then
8 queues than it will request for additional queues from secondary VFs.

Secondary VFs cannot be shared between primary VFs.

Primary VFs are present on the beginning of the 'Network devices using kernel
driver' list, secondary VFs are on the remaining on the remaining part of the list.

   .. note::

      The VNIC driver in the multiqueue setup works differently than other drivers like `ixgbe`.
      We need to bind separately each specific queue set device with the ``usertools/dpdk-devbind.py`` utility.

   .. note::

      Depending on the hardware used, the kernel driver sets a threshold ``vf_id``. VFs that try to attached with an id below or equal to
      this boundary are considered primary VFs. VFs that try to attach with an id above this boundary are considered secondary VFs.

LBK HW Access
~~~~~~~~~~~~~

Loopback HW Unit (LBK) receives packets from NIC-RX and sends packets back to NIC-TX.
The loopback block has N channels and contains data buffering that is shared across
all channels. Four primary VFs are reserved as loopback ports.

Example device binding
~~~~~~~~~~~~~~~~~~~~~~

If a system has three interfaces, a total of 18 VF devices will be created
on a non-NUMA machine.

   .. note::

      NUMA systems have 12 VFs per port and non-NUMA 6 VFs per port.

   .. code-block:: console

      # usertools/dpdk-devbind.py --status

      Network devices using DPDK-compatible driver
      ============================================
      <none>

      Network devices using kernel driver
      ===================================
      0000:01:10.0 'THUNDERX BGX (Common Ethernet Interface) a026' if= drv=thunder-BGX unused=vfio-pci
      0000:01:10.1 'THUNDERX BGX (Common Ethernet Interface) a026' if= drv=thunder-BGX unused=vfio-pci
      0001:01:00.0 'THUNDERX Network Interface Controller a01e' if= drv=thunder-nic unused=vfio-pci
      0001:01:00.1 'Device a034' if=eth0 drv=thunder-nicvf unused=vfio-pci
      0001:01:00.2 'Device a034' if=eth1 drv=thunder-nicvf unused=vfio-pci
      0001:01:00.3 'Device a034' if=eth2 drv=thunder-nicvf unused=vfio-pci
      0001:01:00.4 'Device a034' if=eth3 drv=thunder-nicvf unused=vfio-pci
      0001:01:00.5 'Device a034' if=eth4 drv=thunder-nicvf unused=vfio-pci
      0001:01:00.6 'Device a034' if=lbk0 drv=thunder-nicvf unused=vfio-pci
      0001:01:00.7 'Device a034' if=lbk1 drv=thunder-nicvf unused=vfio-pci
      0001:01:01.0 'Device a034' if=lbk2 drv=thunder-nicvf unused=vfio-pci
      0001:01:01.1 'Device a034' if=lbk3 drv=thunder-nicvf unused=vfio-pci
      0001:01:01.2 'Device a034' if= drv=thunder-nicvf unused=vfio-pci
      0001:01:01.3 'Device a034' if= drv=thunder-nicvf unused=vfio-pci
      0001:01:01.4 'Device a034' if= drv=thunder-nicvf unused=vfio-pci
      0001:01:01.5 'Device a034' if= drv=thunder-nicvf unused=vfio-pci
      0001:01:01.6 'Device a034' if= drv=thunder-nicvf unused=vfio-pci
      0001:01:01.7 'Device a034' if= drv=thunder-nicvf unused=vfio-pci
      0001:01:02.0 'Device a034' if= drv=thunder-nicvf unused=vfio-pci
      0001:01:02.1 'Device a034' if= drv=thunder-nicvf unused=vfio-pci
      0001:01:02.2 'Device a034' if= drv=thunder-nicvf unused=vfio-pci

      Other network devices
      =====================
      0002:00:03.0 'Device a01f' unused=vfio-pci,uio_pci_generic

   .. note::

      Here total no of primary VFs = 5 (variable, depends on no of ethernet ports present) + 4 (fixed, loopback ports).
      Ethernet ports are indicated as `if=eth0` while loopback ports as `if=lbk0`.

We want to bind two physical interfaces with 24 queues each device, we attach two primary VFs
and four secondary VFs. In our example we choose two 10G interfaces eth1 (0002:01:00.2) and eth2 (0002:01:00.3).
We will choose four secondary queue sets from the ending of the list (0001:01:01.2-0002:01:02.2).


#. Bind two primary VFs to the ``vfio-pci`` driver:

   .. code-block:: console

      usertools/dpdk-devbind.py -b vfio-pci 0002:01:00.2
      usertools/dpdk-devbind.py -b vfio-pci 0002:01:00.3

#. Bind four primary VFs to the ``vfio-pci`` driver:

   .. code-block:: console

      usertools/dpdk-devbind.py -b vfio-pci 0002:01:01.7
      usertools/dpdk-devbind.py -b vfio-pci 0002:01:02.0
      usertools/dpdk-devbind.py -b vfio-pci 0002:01:02.1
      usertools/dpdk-devbind.py -b vfio-pci 0002:01:02.2

The nicvf thunderx driver will make use of attached secondary VFs automatically during the interface configuration stage.

Thunder-nic VF's
~~~~~~~~~~~~~~~~

Use sysfs to distinguish thunder-nic primary VFs and secondary VFs.
   .. code-block:: console

      ls -l /sys/bus/pci/drivers/thunder-nic/
      total 0
      drwxr-xr-x  2 root root     0 Jan 22 11:19 ./
      drwxr-xr-x 86 root root     0 Jan 22 11:07 ../
      lrwxrwxrwx  1 root root     0 Jan 22 11:19 0001:01:00.0 -> '../../../../devices/platform/soc@0/849000000000.pci/pci0001:00/0001:00:10.0/0001:01:00.0'/

   .. code-block:: console

      cat /sys/bus/pci/drivers/thunder-nic/0001\:01\:00.0/sriov_sqs_assignment
      12
      0 0001:01:00.1 vfio-pci +: 12 13
      1 0001:01:00.2 thunder-nicvf -:
      2 0001:01:00.3 thunder-nicvf -:
      3 0001:01:00.4 thunder-nicvf -:
      4 0001:01:00.5 thunder-nicvf -:
      5 0001:01:00.6 thunder-nicvf -:
      6 0001:01:00.7 thunder-nicvf -:
      7 0001:01:01.0 thunder-nicvf -:
      8 0001:01:01.1 thunder-nicvf -:
      9 0001:01:01.2 thunder-nicvf -:
      10 0001:01:01.3 thunder-nicvf -:
      11 0001:01:01.4 thunder-nicvf -:
      12 0001:01:01.5 vfio-pci: 0
      13 0001:01:01.6 vfio-pci: 0
      14 0001:01:01.7 thunder-nicvf: 255
      15 0001:01:02.0 thunder-nicvf: 255
      16 0001:01:02.1 thunder-nicvf: 255
      17 0001:01:02.2 thunder-nicvf: 255
      18 0001:01:02.3 thunder-nicvf: 255
      19 0001:01:02.4 thunder-nicvf: 255
      20 0001:01:02.5 thunder-nicvf: 255
      21 0001:01:02.6 thunder-nicvf: 255
      22 0001:01:02.7 thunder-nicvf: 255
      23 0001:01:03.0 thunder-nicvf: 255
      24 0001:01:03.1 thunder-nicvf: 255
      25 0001:01:03.2 thunder-nicvf: 255
      26 0001:01:03.3 thunder-nicvf: 255
      27 0001:01:03.4 thunder-nicvf: 255
      28 0001:01:03.5 thunder-nicvf: 255
      29 0001:01:03.6 thunder-nicvf: 255
      30 0001:01:03.7 thunder-nicvf: 255
      31 0001:01:04.0 thunder-nicvf: 255

Every column that ends with 'thunder-nicvf: number' can be used as secondary VF.
In printout above all entres after '14 0001:01:01.7 thunder-nicvf: 255' can be used as secondary VF.

Debugging Options
-----------------

EAL command option to change  log level
   .. code-block:: console

      --log-level=pmd.net.thunderx.driver:info
      or
      --log-level=pmd.net.thunderx.driver,7

Module params
--------------

skip_data_bytes
~~~~~~~~~~~~~~~
This feature is used to create a hole between HEADROOM and actual data. Size of hole is specified
in bytes as module param("skip_data_bytes") to pmd.
This scheme is useful when application would like to insert vlan header without disturbing HEADROOM.

Example:
   .. code-block:: console

      -w 0002:01:00.2,skip_data_bytes=8

Limitations
-----------

CRC stripping
~~~~~~~~~~~~~

The ThunderX SoC family NICs strip the CRC for every packets coming into the
host interface irrespective of the offload configuration.

Maximum packet length
~~~~~~~~~~~~~~~~~~~~~

The ThunderX SoC family NICs support a maximum of a 9K jumbo frame. The value
is fixed and cannot be changed. So, even when the ``rxmode.max_rx_pkt_len``
member of ``struct rte_eth_conf`` is set to a value lower than 9200, frames
up to 9200 bytes can still reach the host interface.

Maximum packet segments
~~~~~~~~~~~~~~~~~~~~~~~

The ThunderX SoC family NICs support up to 12 segments per packet when working
in scatter/gather mode. So, setting MTU will result with ``EINVAL`` when the
frame size does not fit in the maximum number of segments.

skip_data_bytes
~~~~~~~~~~~~~~~

Maximum limit of skip_data_bytes is 128 bytes and number of bytes should be multiple of 8.
