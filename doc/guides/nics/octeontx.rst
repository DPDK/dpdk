..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Cavium, Inc

OCTEON TX Poll Mode driver
==========================

The OCTEON TX ETHDEV PMD (**librte_net_octeontx**) provides poll mode ethdev
driver support for the inbuilt network device found in the **Cavium OCTEON TX**
SoC family as well as their virtual functions (VF) in SR-IOV context.

More information can be found at `Cavium, Inc Official Website
<http://www.cavium.com/OCTEON-TX_ARM_Processors.html>`_.

Features
--------

Features of the OCTEON TX Ethdev PMD are:

- Packet type information
- Promiscuous mode
- Port hardware statistics
- Jumbo frames
- Scatter-Gather IO support
- Link state information
- MAC/VLAN filtering
- MTU update
- SR-IOV VF
- Multiple queues for TX
- Lock-free Tx queue
- HW offloaded `ethdev Rx queue` to `eventdev event queue` packet injection

Supported OCTEON TX SoCs
------------------------

- CN83xx

Unsupported features
--------------------

The features supported by the device and not yet supported by this PMD include:

- Receive Side Scaling (RSS)
- Scattered and gather for TX and RX
- Ingress classification support
- Egress hierarchical scheduling, traffic shaping, and marking

Prerequisites
-------------

See :doc:`../platform/octeontx` for setup information.

Pre-Installation Configuration
------------------------------


Driver compilation and testing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

#. Running testpmd:

   Follow instructions available in the document
   :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
   to run testpmd.

   Example output:

   .. code-block:: console

      ./<build_dir>/app/dpdk-testpmd -c 700 \
                --base-virtaddr=0x100000000000 \
                --mbuf-pool-ops-name="octeontx_fpavf" \
                --vdev='event_octeontx' \
                --vdev='eth_octeontx,nr_port=2' \
                -- --rxq=1 --txq=1 --nb-core=2 \
                --total-num-mbufs=16384 -i
      .....
      EAL: Detected 24 lcore(s)
      EAL: Probing VFIO support...
      EAL: VFIO support initialized
      .....
      EAL: PCI device 0000:07:00.1 on NUMA socket 0
      EAL:   probe driver: 177d:a04b octeontx_ssovf
      .....
      EAL: PCI device 0001:02:00.7 on NUMA socket 0
      EAL:   probe driver: 177d:a0dd octeontx_pkivf
      .....
      EAL: PCI device 0001:03:01.0 on NUMA socket 0
      EAL:   probe driver: 177d:a049 octeontx_pkovf
      .....
      PMD: octeontx_probe(): created ethdev eth_octeontx for port 0
      PMD: octeontx_probe(): created ethdev eth_octeontx for port 1
      .....
      Configuring Port 0 (socket 0)
      Port 0: 00:0F:B7:11:94:46
      Configuring Port 1 (socket 0)
      Port 1: 00:0F:B7:11:94:47
      .....
      Checking link statuses...
      Port 0 Link Up - speed 40000 Mbps - full-duplex
      Port 1 Link Up - speed 40000 Mbps - full-duplex
      Done
      testpmd>


Initialization
--------------

The OCTEON TX ethdev pmd is exposed as a vdev device which consists of a set
of PKI and PKO PCIe VF devices. On EAL initialization,
PKI/PKO PCIe VF devices will be probed and then the vdev device can be created
from the application code, or from the EAL command line based on
the number of probed/bound PKI/PKO PCIe VF device to DPDK by

* Invoking ``rte_vdev_init("eth_octeontx")`` from the application

* Using ``--vdev="eth_octeontx"`` in the EAL options, which will call
  rte_vdev_init() internally

Device arguments
~~~~~~~~~~~~~~~~
Each ethdev port is mapped to a physical port(LMAC), Application can specify
the number of interesting ports with ``nr_ports`` argument.

Dependency
~~~~~~~~~~
``eth_octeontx`` pmd is depend on ``event_octeontx`` eventdev device and
``octeontx_fpavf`` external mempool handler.

Example:

.. code-block:: console

    ./your_dpdk_application --mbuf-pool-ops-name="octeontx_fpavf" \
                --vdev='event_octeontx' \
                --vdev="eth_octeontx,nr_port=2"

Limitations
-----------

``octeontx_fpavf`` external mempool handler dependency
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The OCTEON TX SoC family NIC has inbuilt HW assisted external mempool manager.
This driver will only work with ``octeontx_fpavf`` external mempool handler
as it is the most performance effective way for packet allocation and Tx buffer
recycling on OCTEON TX SoC platform.

CRC stripping
~~~~~~~~~~~~~

The OCTEON TX SoC family NICs strip the CRC for every packets coming into the
host interface irrespective of the offload configuration.

Maximum packet length
~~~~~~~~~~~~~~~~~~~~~

The OCTEON TX SoC family NICs support a maximum of a 32K jumbo frame. The value
is fixed and cannot be changed. So, even when the ``rxmode.max_rx_pkt_len``
member of ``struct rte_eth_conf`` is set to a value lower than 32k, frames
up to 32k bytes can still reach the host interface.

Maximum mempool size
~~~~~~~~~~~~~~~~~~~~

The maximum mempool size supplied to Rx queue setup should be less than 128K.
When running testpmd on OCTEON TX the application can limit the number of mbufs
by using the option ``--total-num-mbufs=131072``.
