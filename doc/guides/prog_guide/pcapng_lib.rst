..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 Microsoft Corporation

Packet Capture Next Generation Library
======================================

Exchanging packet traces becomes more and more critical every day.
The de facto standard for this is the format defined by libpcap;
but that format is rather old and is lacking in functionality
for more modern applications.
The `Pcapng file format`_ is the default capture file format
for modern network capture processing tools
such as `wireshark`_ (can also be read by `tcpdump`_).

The Pcapng library is an API for formatting packet data
into a Pcapng file.
The format conforms to the current `Pcapng RFC`_ standard.
It is designed to be integrated with the packet capture library.

Usage
-----

The output stream is created with ``rte_pcapng_fdopen``,
and should be closed with ``rte_pcapng_close``.

The library requires a DPDK mempool to allocate mbufs.
The mbufs need to be able to accommodate additional space
for the pcapng packet format header and trailer information;
the function ``rte_pcapng_mbuf_size`` should be used
to determine the lower bound based on MTU.

Collecting packets is done in two parts.
The function ``rte_pcapng_copy`` is used to format and copy mbuf data
and ``rte_pcapng_write_packets`` writes a burst of packets to the output file.

The function ``rte_pcapng_write_stats`` can be used
to write statistics information into the output file.
The summary statistics information is automatically added
by ``rte_pcapng_close``.

.. _Tcpdump: https://tcpdump.org/
.. _Wireshark: https://wireshark.org/
.. _Pcapng file format: https://github.com/pcapng/pcapng/
.. _Pcapng RFC: https://datatracker.ietf.org/doc/html/draft-tuexen-opsawg-pcapng
