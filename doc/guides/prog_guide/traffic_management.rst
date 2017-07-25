..  BSD LICENSE
    Copyright(c) 2017 Intel Corporation. All rights reserved.
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
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


Traffic Management API
======================


Overview
--------

This is the generic API for the Quality of Service (QoS) Traffic Management of
Ethernet devices, which includes the following main features: hierarchical
scheduling, traffic shaping, congestion management, packet marking. This API
is agnostic of the underlying HW, SW or mixed HW-SW implementation.

Main features:

* Part of DPDK rte_ethdev API
* Capability query API per port, per hierarchy level and per hierarchy node
* Scheduling algorithms: Strict Priority (SP), Weighed Fair Queuing (WFQ)
* Traffic shaping: single/dual rate, private (per node) and
  shared (by multiple nodes) shapers
* Congestion management for hierarchy leaf nodes: algorithms of tail drop, head
  drop, WRED, private (per node) and shared (by multiple nodes) WRED contexts
* Packet marking: IEEE 802.1q (VLAN DEI), IETF RFC 3168 (IPv4/IPv6 ECN for TCP
  and SCTP), IETF RFC 2597 (IPv4 / IPv6 DSCP)


Capability API
--------------

The aim of these APIs is to advertise the capability information (i.e critical
parameter values) that the TM implementation (HW/SW) is able to support for the
application. The APIs supports the information disclosure at the TM level, at
any hierarchical level of the TM and at any node level of the specific
hierarchical level. Such information helps towards rapid understanding of
whether a specific implementation does meet the needs to the user application.

At the TM level, users can get high level idea with the help of various
parameters such as maximum number of nodes, maximum number of hierarchical
levels, maximum number of shapers, maximum number of private shapers, type of
scheduling algorithm (Strict Priority, Weighted Fair Queueing , etc.), etc.,
supported by the implementation.

Likewise, users can query the capability of the TM at the hierarchical level to
have more granular knowledge about the specific level. The various parameters
such as maximum number of nodes at the level, maximum number of leaf/non-leaf
nodes at the level, type of the shaper(dual rate, single rate) supported at
the level if node is non-leaf type etc., are exposed as a result of
hierarchical level capability query.

Finally, the node level capability API offers knowledge about the capability
supported by the node at any specific level. The information whether the
support is available for private shaper, dual rate shaper, maximum and minimum
shaper rate, etc. is exposed by node level capability API.


Scheduling Algorithms
---------------------

The fundamental scheduling algorithms that are supported are Strict Priority
(SP) and Weighted Fair Queuing (WFQ). The SP and WFQ algorithms are supported
at the level of each node of the scheduling hierarchy, regardless of the node
level/position in the tree. The SP algorithm is used to schedule between
sibling nodes with different priority, while WFQ is used to schedule between
groups of siblings that have the same priority.

Algorithms such as Weighed Round Robin (WRR), byte-level WRR, Deficit WRR
(DWRR), etc are considered approximations of the ideal WFQ and are therefore
assimilated to WFQ, although an associated implementation-dependent accuracy,
performance and resource usage trade-off might exist.


Traffic Shaping
---------------

The TM API provides support for single rate and dual rate shapers (rate
limiters) for the hierarchy nodes, subject to the specific implementation
support being available.

Each hierarchy node has zero or one private shaper (only one node using it)
and/or zero, one or several shared shapers (multiple nodes use the same shaper
instance). A private shaper is used to perform traffic shaping for a single
node, while a shared shaper is used to perform traffic shaping for a group of
nodes.

The configuration of private and shared shapers is done through the definition
of shaper profiles. Any shaper profile (single rate or dual rate shaper) can be
used by one or several shaper instances (either private or shared).

Single rate shapers use a single token bucket. Therefore, single rate shaper is
configured by setting the rate of the committed bucket to zero, which
effectively disables this bucket. The peak bucket is used to limit the rate
and the burst size for the single rate shaper. Dual rate shapers use both the
committed and the peak token buckets. The rate of the peak bucket has to be
bigger than zero, as well as greater than or equal to the rate of the committed
bucket.


Congestion Management
---------------------

Congestion management is used to control the admission of packets into a packet
queue or group of packet queues on congestion. The congestion management
algorithms that are supported are: Tail Drop, Head Drop and Weighted Random
Early Detection (WRED). They are made available for every leaf node in the
hierarchy, subject to the specific implementation supporting them.
On request of writing a new packet into the current queue while the queue is
full, the Tail Drop algorithm drops the new packet while leaving the queue
unmodified, as opposed to the Head Drop* algorithm, which drops the packet
at the head of the queue (the oldest packet waiting in the queue) and admits
the new packet at the tail of the queue.

The Random Early Detection (RED) algorithm works by proactively dropping more
and more input packets as the queue occupancy builds up. When the queue is full
or almost full, RED effectively works as Tail Drop. The Weighted RED (WRED)
algorithm uses a separate set of RED thresholds for each packet color and uses
separate set of RED thresholds for each packet color.

Each hierarchy leaf node with WRED enabled as its congestion management mode
has zero or one private WRED context (only one leaf node using it) and/or zero,
one or several shared WRED contexts (multiple leaf nodes use the same WRED
context). A private WRED context is used to perform congestion management for
a single leaf node, while a shared WRED context is used to perform congestion
management for a group of leaf nodes.

The configuration of WRED private and shared contexts is done through the
definition of WRED profiles. Any WRED profile can be used by one or several
WRED contexts (either private or shared).


Packet Marking
--------------
The TM APIs have been provided to support various types of packet marking such
as VLAN DEI packet marking (IEEE 802.1Q), IPv4/IPv6 ECN marking of TCP and SCTP
packets (IETF RFC 3168) and IPv4/IPv6 DSCP packet marking (IETF RFC 2597).
All VLAN frames of a given color get their DEI bit set if marking is enabled
for this color. In case, when marking for a given color is not enabled, the
DEI bit is left as is (either set or not).

All IPv4/IPv6 packets of a given color with ECN set to 2’b01 or 2’b10 carrying
TCP or SCTP have their ECN set to 2’b11 if the marking feature is enabled for
the current color, otherwise the ECN field is left as is.

All IPv4/IPv6 packets have their color marked into DSCP bits 3 and 4 as
follows: green mapped to Low Drop Precedence (2’b01), yellow to Medium (2’b10)
and red to High (2’b11). Marking needs to be explicitly enabled for each color;
when not enabled for a given color, the DSCP field of all packets with that
color is left as is.


Steps to Setup the Hierarchy
----------------------------

The TM hierarchical tree consists of leaf nodes and non-leaf nodes. Each leaf
node sits on top of a scheduling queue of the current Ethernet port. Therefore,
the leaf nodes have predefined IDs in the range of 0... (N-1), where N is the
number of scheduling queues of the current Ethernet port. The non-leaf nodes
have their IDs generated by the application outside of the above range, which
is reserved for leaf nodes.

Each non-leaf node has multiple inputs (its children nodes) and single output
(which is input to its parent node). It arbitrates its inputs using Strict
Priority (SP) and Weighted Fair Queuing (WFQ) algorithms to schedule input
packets to its output while observing its shaping (rate limiting) constraints.

The children nodes with different priorities are scheduled using the SP
algorithm based on their priority, with 0 as the highest priority. Children
with the same priority are scheduled using the WFQ algorithm according to their
weights. The WFQ weight of a given child node is relative to the sum of the
weights of all its sibling nodes that have the same priority, with 1 as the
lowest weight. For each SP priority, the WFQ weight mode can be set as either
byte-based or packet-based.


Initial Hierarchy Specification
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The hierarchy is specified by incrementally adding nodes to build up the
scheduling tree. The first node that is added to the hierarchy becomes the root
node and all the nodes that are subsequently added have to be added as
descendants of the root node. The parent of the root node has to be specified
as RTE_TM_NODE_ID_NULL and there can only be one node with this parent ID
(i.e. the root node). The unique ID that is assigned to each node when the node
is created is further used to update the node configuration or to connect
children nodes to it.

During this phase, some limited checks on the hierarchy specification can be
conducted, usually limited in scope to the current node, its parent node and
its sibling nodes. At this time, since the hierarchy is not fully defined,
there is typically no real action performed by the underlying implementation.


Hierarchy Commit
~~~~~~~~~~~~~~~~

The hierarchy commit API is called during the port initialization phase (before
the Ethernet port is started) to freeze the start-up hierarchy.  This function
typically performs the following steps:

* It validates the start-up hierarchy that was previously defined for the
  current port through successive node add API invocations.
* Assuming successful validation, it performs all the necessary implementation
  specific operations to install the specified hierarchy on the current port,
  with immediate effect once the port is started.

This function fails when the currently configured hierarchy is not supported by
the Ethernet port, in which case the user can abort or try out another
hierarchy configuration (e.g. a hierarchy with less leaf nodes), which can be
built from scratch or by modifying the existing hierarchy configuration. Note
that this function can still fail due to other causes (e.g. not enough memory
available in the system, etc.), even though the specified hierarchy is
supported in principle by the current port.


Run-Time Hierarchy Updates
~~~~~~~~~~~~~~~~~~~~~~~~~~

The TM API provides support for on-the-fly changes to the scheduling hierarchy,
thus operations such as node add/delete, node suspend/resume, parent node
update, etc., can be invoked after the Ethernet port has been started, subject
to the specific implementation supporting them. The set of dynamic updates
supported by the implementation is advertised through the port capability set.
