..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2022~2023 Phytium Technology Co., Ltd.

MACB Poll Mode Driver
=====================

The MACB PMD provides poll mode driver support
for the Ethernet interface MAC 1/2.5/10 Gbps adapter.


Supported Chipsets and NICs
---------------------------

Phytium Ethernet interface cdns,phytium-gem-1.0
Phytium Ethernet interface cdns,phytium-gem-2.0


Features
--------

Features of the MACB PMD are:

- Speed capabilities
- Link status
- Set link down or up
- MTU update
- Multiple queues for TX and RX
- CRC offload
- Scatter and gather for Tx and Rx
- Jumbo frames supported
- Promiscuous mode
- Allmulticast mode
- Basic stats


Runtime Configuration
---------------------

The following ``devargs`` options can be enabled at runtime. They can
be passed as part of EAL arguments. For example,

Run the command in non-vector mode:

.. code-block:: console

   ./dpdk-testpmd -l 0,3 --vdev=net_macb0,device=3200c000.ethernet,usephydrv=0 \
    --vdev=net_macb1,device=3200e000.ethernet,usephydrv=0 --iova-mode=pa -- -i \
    -a --rxq=1 --txq=1 --rxd=128 --txd=128

- ``device`` (mandatory, with no default value)

  The name of port (owned by Phytium) that should be enabled in DPDK.
  This options can be repeated resulting in a list of ports to be
  enabled.  For instance below will enable ``eth0`` and ``eth1`` ports.
  Use the network interface 3200c000.ethernet as eth0,
  and the network interface 3200e000.ethernet as eth1.

- ``--vdev=net_macb0, device=3200c000.ethernet, usephydrv=0`` (no default value)

  --vdev specifies the MACB driver, and device specifies the virtual device to be used. The options
  --vdev=net_macb0,device=3200c000.ethernet and --vdev=net_macb1,device=3200e000.ethernet
  can also be written as: --vdev=net_macb,device=3200c000.ethernet,device=3200e000.ethernet.

- ``usephydrv`` (Default value: true)
  The specified macb device does not have an external phy chip.
  If there is indeed a device with phy, this parameter is not added.

Run the command in vector mode:

.. code-block:: console

   ./dpdk-testpmd -l 0,3 --vdev=net_macb0,device=3200c000.ethernet,usephydrv=0 \
    --vdev=net_macb1,device=3200e000.ethernet,usephydrv=0 --iova-mode=pa -- -i \
    -a --rxq=1 --txq=1 --rxd=128 --txd=128 --rxfreet=32 --txrst=32

- ``--rxfreet`` (Default value: 16)
  Enable the optimization of sending vector instructions.

- ``--txrst`` (Default value: 16)
  Enable the optimization of receiving vector instructions.


Usage Example
-------------

MACB PMD requires extra out of tree kernel modules to function properly. The macb_uio
module can be found in the repository `<https://github.com/dpdk-kmods/macb_uio>`.
It can be loaded as shown below:

.. code-block:: console

   git clone https://github.com/dpdk-kmods/macb_uio.git
   cd macb_uio && make
   modprobe uio
   insmod macb_uio.ko

Bind the network interface to the macb_uio driver:

.. code-block:: console

   ./dpdk-pdevbind.sh --bind macb_uio 3200c000.ethernet
   ./dpdk-pdevbind.sh --bind macb_uio 3200e000.ethernet

In order to run testpmd example application following command can be used:

.. code-block:: console

   ./app/dpdk-testpmd -l 0,3 --vdev=net_macb0,device=3200c000.ethernet \
    --vdev=net_macb1,device=3200e000.ethernet --iova-mode=pa -- -i -a

Example output:

.. code-block:: console

   [...]
   EAL: Detected CPU lcores: 4
   EAL: Detected NUMA nodes: 1
   EAL: Detected static linkage of DPDK
   EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
   EAL: Selected IOVA mode 'PA'
   MACB: Phytium mac driver v5.8
   MACB: macb_get_fixed_link_speed_info(): speed info is unknown.
   MACB: Phytium mac driver v5.8
   MACB: macb_get_fixed_link_speed_info(): speed info is unknown.
   Interactive-mode selected
   Auto-start selected
   testpmd: create a new mbuf pool <mb_pool_0>: n=155456, size=2176, socket=0
   testpmd: preferred mempool ops selected: ring_mp_mc
   Configuring Port 0 (socket 0)
   MACB: Rx Burst Bulk Alloc Preconditions: rxq->rx_free_thresh=16, MACB_MAX_RX_BURST=32
   MACB: queue[0] doesn't meet Rx Bulk Alloc preconditions - canceling the feature for port[0]
   MACB: Port[0] doesn't meet Vector Rx preconditions
   Port 0: 24:DC:0F:54:E5:D0
   Configuring Port 1 (socket 0)
   MACB: Rx Burst Bulk Alloc Preconditions: rxq->rx_free_thresh=16, MACB_MAX_RX_BURST=32
   MACB: queue[0] doesn't meet Rx Bulk Alloc preconditions - canceling the feature for port[1]
   MACB: Port[1] doesn't meet Vector Rx preconditions
   Port 1: 24:DC:0F:54:E5:D1
   Checking link statuses...
   MACB: Port 0: Link up at 10 Gbps FDX Autoneg

   Port 0: link state change event
   MACB: Port 1: Link up at 10 Gbps FDX Autoneg

   Port 1: link state change event
   Done
   Start automatic packet forwarding
   io packet forwarding - ports=2 - cores=1 - streams=2 - NUMA support enabled, MP allocation mode: native
   Logical Core 3 (socket 0) forwards packets on 2 streams:
     RX P=0/Q=0 (socket 0) -> TX P=1/Q=0 (socket 0) peer=02:00:00:00:00:01
     RX P=1/Q=0 (socket 0) -> TX P=0/Q=0 (socket 0) peer=02:00:00:00:00:00

     io packet forwarding packets/burst=32
     nb forwarding cores=1 - nb forwarding ports=2
     port 0: RX queue number: 1 Tx queue number: 1
       Rx offloads=0xe Tx offloads=0x0
       RX queue: 0
         RX desc=512 - RX free threshold=16
         RX threshold registers: pthresh=0 hthresh=0  wthresh=0
         RX Offloads=0xe
       TX queue: 0
         TX desc=512 - TX free threshold=32
         TX threshold registers: pthresh=0 hthresh=0  wthresh=0
         TX offloads=0x0 - TX RS bit threshold=0
     port 1: RX queue number: 1 Tx queue number: 1
       Rx offloads=0xe Tx offloads=0x0
       RX queue: 0
         RX desc=512 - RX free threshold=16
         RX threshold registers: pthresh=0 hthresh=0  wthresh=0
         RX Offloads=0xe
       TX queue: 0
         TX desc=512 - TX free threshold=32
         TX threshold registers: pthresh=0 hthresh=0  wthresh=0
         TX offloads=0x0 - TX RS bit threshold=0
   testpmd>


Limitations
-----------

The driver is only available on the ARM64 architecture.
