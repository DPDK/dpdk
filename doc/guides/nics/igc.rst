..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Intel Corporation.

IGC Poll Mode Driver
======================

The IGC PMD (**librte_net_igc**) provides poll mode driver support for Foxville
I225 Series Network Adapters.

- For information about I225, please refer to: `Intel® Ethernet Controller I225 Series
  <https://ark.intel.com/content/www/us/en/ark/products/series/184686/intel-ethernet-controller-i225-series.html>`_.


Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.


Supported Chipsets and NICs
---------------------------

Foxville LM (I225 LM): Client 2.5G LAN vPro Corporate
Foxville V (I225 V): Client 2.5G LAN Consumer
Foxville I (I225 I): Client 2.5G Industrial Temp
Foxville V (I225 K): Client 2.5G LAN Consumer


Sample Application Notes
------------------------

Vlan filter
~~~~~~~~~~~

VLAN stripping off only works with inner vlan.
Only the outer VLAN TPID can be set to a vlan other than 0x8100.

If extend VLAN is enabled:

- The VLAN header in a packet that carries a single VLAN header is treated as the external VLAN.

- Foxville expects that any transmitted packet to have at least the external VLAN added by the
  software. For those packets where an external VLAN is not present, any offload that relates to
  inner fields to the EtherType might not be provided.

- If VLAN TX-OFFLOAD is enabled and the packet does not contain an external VLAN, the packet is
  dropped, and if configured, the queue from which the packet was sent is disabled.

To start ``testpmd``, add vlan 10 to port, set vlan stripping off on, set extend on, set TPID of
outer VLAN to 0x9100:

.. code-block:: console

   ./app/dpdk-testpmd -l 4-8 -- -i
   ...

   testpmd> vlan set filter on 0
   testpmd> rx_vlan add 10 0
   testpmd> vlan set strip off 0
   testpmd> vlan set extend on 0
   testpmd> vlan set outer tpid 0x9100 0


Flow Director
~~~~~~~~~~~~~

The Flow Director works in receive mode to identify specific flows or sets of flows and route
them to specific queues.

The Flow Director filters includes the following types:

- ether-type filter
- 2-tuple filter(destination L4 protocol and destination L4 port)
- TCP SYN filter
- RSS filter

Start ``testpmd``:

.. code-block:: console

   ./dpdk-testpmd -l 4-8 -- i --rxq=4 --txq=4 --pkt-filter-mode=perfect --disable-rss

Add a rule to direct packet whose ``ether-type=0x801`` to queue 1:

.. code-block:: console

   testpmd> flow create 0 ingress pattern eth type is 0x801 / end actions queue index 1 / end

Add a rule to direct packet whose ``ip-protocol=0x6(TCP), tcp_port=0x80`` to queue 1:

.. code-block:: console

   testpmd> flow create 0 ingress pattern eth / ipv4 proto is 6 / tcp dst is 0x80 / end actions queue index 1 / end

Add a rule to direct packet whose ``ip-protocol=0x6(TCP), SYN flag is set`` to queue 1:

.. code-block:: console

   testpmd> flow validate 0 ingress pattern tcp flags spec 0x02 flags mask 0x02 / end actions queue index 1 / end

Add a rule to enable ipv4-udp RSS:

.. code-block:: console

   testpmd> flow create 0 ingress pattern end actions rss types ipv4-udp end / end
