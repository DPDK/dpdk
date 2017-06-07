..  BSD LICENSE
    Copyright (c) 2016 Solarflare Communications Inc.
    All rights reserved.

    This software was jointly developed between OKTET Labs (under contract
    for Solarflare) and Solarflare Communications, Inc.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
    CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
    OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
    EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Solarflare libefx-based Poll Mode Driver
========================================

The SFC EFX PMD (**librte_pmd_sfc_efx**) provides poll mode driver support
for **Solarflare SFN7xxx and SFN8xxx** family of 10/40 Gbps adapters.
SFC EFX PMD has support for the latest Linux and FreeBSD operating systems.

More information can be found at `Solarflare Communications website
<http://solarflare.com>`_.


Features
--------

SFC EFX PMD has support for:

- Multiple transmit and receive queues

- Link state information including link status change interrupt

- IPv4/IPv6 TCP/UDP transmit checksum offload

- Port hardware statistics

- Extended statistics (see Solarflare Server Adapter User's Guide for
  the statistics description)

- Basic flow control

- MTU update

- Jumbo frames up to 9K

- Promiscuous mode

- Allmulticast mode

- TCP segmentation offload (TSO)

- Multicast MAC filter

- IPv4/IPv6 TCP/UDP receive checksum offload

- Received packet type information

- Receive side scaling (RSS)

- RSS hash

- Scattered Rx DMA for packet that are larger that a single Rx descriptor

- Deferred receive and transmit queue start

- Transmit VLAN insertion (if running firmware variant supports it)

- Flow API


Non-supported Features
----------------------

The features not yet supported include:

- Receive queue interupts

- Priority-based flow control

- Loopback

- Configurable RX CRC stripping (always stripped)

- Header split on receive

- VLAN filtering

- VLAN stripping

- LRO


Limitations
-----------

Due to requirements on receive buffer alignment and usage of the receive
buffer for the auxiliary packet information provided by the NIC up to
extra 269 (14 bytes prefix plus up to 255 bytes for end padding) bytes may be
required in the receive buffer.
It should be taken into account when mbuf pool for receive is created.


Flow API support
----------------

Supported attributes:

- Ingress

Supported pattern items:

- VOID

- ETH (exact match of source/destination addresses, individual/group match
  of destination address, EtherType)

- VLAN (exact match of VID, double-tagging is supported)

- IPV4 (exact match of source/destination addresses,
  IP transport protocol)

- IPV6 (exact match of source/destination addresses,
  IP transport protocol)

- TCP (exact match of source/destination ports)

- UDP (exact match of source/destination ports)

Supported actions:

- VOID

- QUEUE

Validating flow rules depends on the firmware variant.

Ethernet destinaton individual/group match
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Ethernet item supports I/G matching, if only the corresponding bit is set
in the mask of destination address. If destinaton address in the spec is
multicast, it matches all multicast (and broadcast) packets, oherwise it
matches unicast packets that are not filtered by other flow rules.


Supported NICs
--------------

- Solarflare Flareon [Ultra] Server Adapters:

   - Solarflare SFN8522 Dual Port SFP+ Server Adapter

   - Solarflare SFN8542 Dual Port QSFP+ Server Adapter

   - Solarflare SFN7002F Dual Port SFP+ Server Adapter

   - Solarflare SFN7004F Quad Port SFP+ Server Adapter

   - Solarflare SFN7042Q Dual Port QSFP+ Server Adapter

   - Solarflare SFN7122F Dual Port SFP+ Server Adapter

   - Solarflare SFN7124F Quad Port SFP+ Server Adapter

   - Solarflare SFN7142Q Dual Port QSFP+ Server Adapter

   - Solarflare SFN7322F Precision Time Synchronization Server Adapter


Prerequisites
-------------

- Requires firmware version:

   - SFN7xxx: **4.7.1.1001** or higher

   - SFN8xxx: **6.0.2.1004** or higher

Visit `Solarflare Support Downloads <https://support.solarflare.com>`_ to get
Solarflare Utilities (either Linux or FreeBSD) with the latest firmware.
Follow instructions from Solarflare Server Adapter User's Guide to
update firmware and configure the adapter.


Pre-Installation Configuration
------------------------------


Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``.config`` file.
Please note that enabling debugging options may affect system performance.

- ``CONFIG_RTE_LIBRTE_SFC_EFX_PMD`` (default **y**)

  Enable compilation of Solarflare libefx-based poll-mode driver.

- ``CONFIG_RTE_LIBRTE_SFC_EFX_DEBUG`` (default **n**)

  Enable compilation of the extra run-time consistency checks.


Per-Device Parameters
~~~~~~~~~~~~~~~~~~~~~

The following per-device parameters can be passed via EAL PCI device
whitelist option like "-w 02:00.0,arg1=value1,...".

Case-insensitive 1/y/yes/on or 0/n/no/off may be used to specify
boolean parameters value.

- ``rx_datapath`` [auto|efx|ef10] (default **auto**)

  Choose receive datapath implementation.
  **auto** allows the driver itself to make a choice based on firmware
  features available and required by the datapath implementation.
  **efx** chooses libefx-based datapath which supports Rx scatter.
  **ef10** chooses EF10 (SFN7xxx, SFN8xxx) native datapath which is
  more efficient than libefx-based and provides richer packet type
  classification, but lacks Rx scatter support.

- ``tx_datapath`` [auto|efx|ef10|ef10_simple] (default **auto**)

  Choose transmit datapath implementation.
  **auto** allows the driver itself to make a choice based on firmware
  features available and required by the datapath implementation.
  **efx** chooses libefx-based datapath which supports VLAN insertion
  (full-feature firmware variant only), TSO and multi-segment mbufs.
  **ef10** chooses EF10 (SFN7xxx, SFN8xxx) native datapath which is
  more efficient than libefx-based but has no VLAN insertion and TSO
  support yet.
  **ef10_simple** chooses EF10 (SFN7xxx, SFN8xxx) native datapath which
  is even more faster then **ef10** but does not support multi-segment
  mbufs.

- ``perf_profile`` [auto|throughput|low-latency] (default **throughput**)

  Choose hardware tunning to be optimized for either throughput or
  low-latency.
  **auto** allows NIC firmware to make a choice based on
  installed licences and firmware variant configured using **sfboot**.

- ``debug_init`` [bool] (default **n**)

  Enable extra logging during device initialization and startup.

- ``mcdi_logging`` [bool] (default **n**)

  Enable extra logging of the communication with the NIC's management CPU.
  The logging is done using RTE_LOG() with INFO level and PMD type.
  The format is consumed by the Solarflare netlogdecode cross-platform tool.

- ``stats_update_period_ms`` [long] (default **1000**)

  Adjust period in milliseconds to update port hardware statistics.
  The accepted range is 0 to 65535. The value of **0** may be used
  to disable periodic statistics update. One should note that it's
  only possible to set an arbitrary value on SFN8xxx provided that
  firmware version is 6.2.1.1033 or higher, otherwise any positive
  value will select a fixed update period of **1000** milliseconds
