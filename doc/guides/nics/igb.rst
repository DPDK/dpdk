..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

IGB Poll Mode Driver
====================

The IGB PMD (**librte_net_e1000**) provides poll mode driver
support for Intel 1GbE nics.

Supported Chipsets and NICs
---------------------------

- Intel 82576EB 10 Gigabit Ethernet Controller
- Intel 82580EB 10 Gigabit Ethernet Controller
- Intel 82580DB 10 Gigabit Ethernet Controller
- Intel Ethernet Controller I210
- Intel Ethernet Controller I350

Features
--------

Features of the IGB PMD are:

* Multiple queues for TX and RX
* Receiver Side Scaling (RSS)
* MAC/VLAN filtering
* Packet type information
* Double VLAN
* IEEE 1588
* TSO offload
* Checksum offload
* TCP segmentation offload
* Jumbo frames supported

Secondary Process Support
-------------------------

IGB Physical Function Driver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Control plane operations are currently not supported in secondary processes.

IGB Virtual Function Driver
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Control plane operations are currently not supported in secondary processes.
