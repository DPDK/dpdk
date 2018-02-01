..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015 Intel Corporation.

.. _Reorder_Library:

Reorder Library
=================

The Reorder Library provides a mechanism for reordering mbufs based on their
sequence number.

Operation
----------

The reorder library is essentially a buffer that reorders mbufs.
The user inserts out of order mbufs into the reorder buffer and pulls in-order
mbufs from it.

At a given time, the reorder buffer contains mbufs whose sequence number are
inside the sequence window. The sequence window is determined by the minimum
sequence number and the number of entries that the buffer was configured to hold.
For example, given a reorder buffer with 200 entries and a minimum sequence
number of 350, the sequence window has low and high limits of 350 and 550
respectively.

When inserting mbufs, the reorder library differentiates between valid, early
and late mbufs depending on the sequence number of the inserted mbuf:

* valid: the sequence number is inside the window.
* late: the sequence number is outside the window and less than the low limit.
* early: the sequence number is outside the window and greater than the high
  limit.

The reorder buffer directly returns late mbufs and tries to accommodate early
mbufs.


Implementation Details
-------------------------

The reorder library is implemented as a pair of buffers, which referred to as
the *Order* buffer and the *Ready* buffer.

On an insert call, valid mbufs are inserted directly into the Order buffer and
late mbufs are returned to the user with an error.

In the case of early mbufs, the reorder buffer will try to move the window
(incrementing the minimum sequence number) so that the mbuf becomes a valid one.
To that end, mbufs in the Order buffer are moved into the Ready buffer.
Any mbufs that have not arrived yet are ignored and therefore will become
late mbufs.
This means that as long as there is room in the Ready buffer, the window will
be moved to accommodate early mbufs that would otherwise be outside the
reordering window.

For example, assuming that we have a buffer of 200 entries with a 350 minimum
sequence number, and we need to insert an early mbuf with 565 sequence number.
That means that we would need to move the windows at least 15 positions to
accommodate the mbuf.
The reorder buffer would try to move mbufs from at least the next 15 slots in
the Order buffer to the Ready buffer, as long as there is room in the Ready buffer.
Any gaps in the Order buffer at that point are skipped, and those packet will
be reported as late packets when they arrive. The process of moving packets
to the Ready buffer continues beyond the minimum required until a gap,
i.e. missing mbuf, in the Order buffer is encountered.

When draining mbufs, the reorder buffer would return  mbufs in the Ready
buffer first and then from the Order buffer until a gap is found (mbufs that
have not arrived yet).

Use Case: Packet Distributor
-------------------------------

An application using the DPDK packet distributor could make use of the reorder
library to transmit packets in the same order they were received.

A basic packet distributor use case would consist of a distributor with
multiple workers cores.
The processing of packets by the workers is not guaranteed to be in order,
hence a reorder buffer can be used to order as many packets as possible.

In such a scenario, the distributor assigns a sequence number to mbufs before
delivering them to the workers.
As the workers finish processing the packets, the distributor inserts those
mbufs into the reorder buffer and finally transmit drained mbufs.

NOTE: Currently the reorder buffer is not thread safe so the same thread is
responsible for inserting and draining mbufs.
