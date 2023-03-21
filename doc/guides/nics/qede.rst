..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 QLogic Corporation
    Copyright(c) 2017 Cavium, Inc

QEDE Poll Mode Driver
======================

The QEDE poll mode driver library (**librte_net_qede**) implements support
for **QLogic FastLinQ QL4xxxx 10G/25G/40G/50G/100G Intelligent Ethernet Adapters (IEA) and Converged Network Adapters (CNA)** family of adapters as well as SR-IOV virtual functions (VF). It is supported on
several standard Linux distros like RHEL, SLES, Ubuntu etc.
It is compile-tested under FreeBSD OS.

More information can be found at `QLogic Corporation's Website
<http://www.qlogic.com>`_.

Supported QLogic Adapters
-------------------------

- QLogic FastLinQ QL4xxxx 10G/25G/40G/50G/100G Intelligent Ethernet Adapters (IEA) and Converged Network Adapters (CNA)

Supported Features
------------------

- Unicast/Multicast filtering
- Promiscuous mode
- Allmulti mode
- Port hardware statistics
- Jumbo frames
- Multiple MAC address
- MTU change
- Default pause flow control
- Multiprocess aware
- Scatter-Gather
- Multiple Rx/Tx queues
- RSS (with RETA/hash table/key)
- TSS
- Stateless checksum offloads (IPv4/IPv6/TCP/UDP)
- LRO/TSO
- VLAN offload - Filtering and stripping
- N-tuple filter and flow director (limited support)
- NPAR (NIC Partitioning)
- SR-IOV PF and VF
- GRE Tunneling offload
- GENEVE Tunneling offload
- VXLAN Tunneling offload
- MPLSoUDP Tx Tunneling offload
- Generic flow API

Co-existence considerations
---------------------------

- QLogic FastLinQ QL4xxxx CNAs support Ethernet, RDMA, iSCSI and FCoE
  functionalities. These functionalities are supported using
  QLogic Linux kernel drivers qed, qede, qedr, qedi and qedf. DPDK is
  supported on these adapters using qede PMD.

- When SR-IOV is not enabled on the adapter,
  QLogic Linux kernel drivers (qed, qede, qedr, qedi and qedf) and qede
  PMD can’t be attached to different PFs on a given QLogic FastLinQ
  QL4xxx adapter.
  A given adapter needs to be completely used by DPDK or Linux drivers
  Before binding DPDK driver to one or more PFs on the adapter,
  please make sure to unbind Linux drivers from all PFs of the adapter.
  If there are multiple adapters on the system, one or more adapters
  can be used by DPDK driver completely and other adapters can be used
  by Linux drivers completely.

- When SR-IOV is enabled on the adapter,
  Linux kernel drivers (qed, qede, qedr, qedi and qedf) can be bound
  to the PFs of a given adapter and either qede PMD or Linux drivers
  (qed and qede) can be bound to the VFs of the adapter.

- For sharing an adapter between DPDK and Linux drivers, SRIOV needs
  to be enabled. Bind all the PFs to Linux Drivers(qed/qede). Create
  a VF on PFs where DPDK is desired and bind these VFs to qede_pmd.
  Binding of PFs simultaneously to DPDK and Linux drivers on a given
  adapter is not supported.

Prerequisites
-------------

- Requires storm firmware version **8.40.33.0**. Firmware may be available
  inbox in certain newer Linux distros under the standard directory
  ``E.g. /lib/firmware/qed/qed_init_values-8.40.33.0.bin``.
  If the required firmware files are not available then download it from
  `linux-firmware git repository <http://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/tree/qed>`_.

- Requires the NIC be updated minimally with **8.30.x.x** Management firmware(MFW) version supported for that NIC.
  It is highly recommended that the NIC be updated with the latest available management firmware version to get latest feature  set.
  Management Firmware and Firmware Upgrade Utility for Cavium FastLinQ(r) branded adapters can be downloaded from
  `Driver Download Center <http://driverdownloads.qlogic.com/QLogicDriverDownloads_UI/DefaultNewSearch.aspx>`_.
  For downloading Firmware Upgrade Utility, select NIC category, model and Linux distro.
  To update the management firmware, refer to the instructions in the Firmware Upgrade Utility Readme document.
  For OEM branded adapters please follow the instruction provided by the OEM to update the Management Firmware on the NIC.

- SR-IOV requires Linux PF driver version **8.20.x.x** or higher.
  If the required PF driver is not available then download it from
  `QLogic Driver Download Center <http://driverdownloads.qlogic.com/QLogicDriverDownloads_UI/DefaultNewSearch.aspx>`_.
  For downloading PF driver, select adapter category, model and Linux distro.

Performance note
~~~~~~~~~~~~~~~~

- For better performance, it is recommended to use 4K or higher RX/TX rings.

Compilation Options
~~~~~~~~~~~~~~~~~~~

The following option can be modified in the ``config/rte_config.h`` file.

- ``RTE_LIBRTE_QEDE_FW`` (default **""**)

  Gives absolute path of firmware file.
  ``Eg: "/lib/firmware/qed/qed_init_values-8.40.33.0.bin"``
  Empty string indicates driver will pick up the firmware file
  from the default location /lib/firmware/qed.
  CAUTION this option is more for custom firmware, it is not
  recommended for use under normal condition.

The following options can be enabled with Meson flags.

- ``RTE_LIBRTE_QEDE_DEBUG_TX`` (default **disabled**)

  Toggle display of transmit fast path run-time messages.

- ``RTE_LIBRTE_QEDE_DEBUG_RX`` (default **disabled**)

  Toggle display of receive fast path run-time messages.

Config notes
~~~~~~~~~~~~

When there are multiple adapters and/or large number of Rx/Tx queues
configured on the adapters, the default (2560) number of memzone
descriptors may not be enough. Please increase the number of memzone
descriptors to a higher number as needed. When sufficient number of
memzone descriptors are not configured, user can potentially run into
following error.

   .. code-block:: console

      EAL: memzone_reserve_aligned_thread_unsafe(): No more room in config

Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

RTE Flow Support
----------------

QLogic FastLinQ QL4xxxx NICs has support for the following patterns and
actions.

Patterns:

.. _table_qede_supported_flow_item_types:

.. table:: Item types

   +----+--------------------------------+
   | #  | Pattern Type                   |
   +====+================================+
   | 1  | RTE_FLOW_ITEM_TYPE_IPV4        |
   +----+--------------------------------+
   | 2  | RTE_FLOW_ITEM_TYPE_IPV6        |
   +----+--------------------------------+
   | 3  | RTE_FLOW_ITEM_TYPE_UDP         |
   +----+--------------------------------+
   | 4  | RTE_FLOW_ITEM_TYPE_TCP         |
   +----+--------------------------------+

Actions:

.. _table_qede_supported_ingress_action_types:

.. table:: Ingress action types

   +----+--------------------------------+
   | #  | Action Type                    |
   +====+================================+
   | 1  | RTE_FLOW_ACTION_TYPE_QUEUE     |
   +----+--------------------------------+
   | 2  | RTE_FLOW_ACTION_TYPE_DROP      |
   +----+--------------------------------+

SR-IOV: Prerequisites and Sample Application Notes
--------------------------------------------------

This section provides instructions to configure SR-IOV with Linux OS.

**Note**: librte_net_qede will be used to bind to SR-IOV VF device and Linux native kernel driver (qede) will function as SR-IOV PF driver. Requires PF driver to be 8.20.x.x or higher.

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
   using the instructions from Driver compilation and testing section above.


#. Running testpmd
   (Supply ``--log-level="pmd.net.qede.driver:info`` to view informational messages):

   Refer to the document
   :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>` to run
   ``testpmd`` application.

   Example output:

   .. code-block:: console

      dpdk-testpmd -l 0,4-11 -n 4 -- -i --nb-cores=8 --portmask=0xf --rxd=4096 \
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
