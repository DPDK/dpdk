..  BSD LICENSE
    Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
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

.. _Efd_Library:

Elastic Flow Distributor Library
================================

Introduction
------------

In Data Centers today, clustering and scheduling of distributed workloads
is a very common task. Many workloads require a deterministic
partitioning of a flat key space among a cluster of machines. When a
packet enters the cluster, the ingress node will direct the packet to
its handling node. For example, data-centers with disaggregated storage
use storage metadata tables to forward I/O requests to the correct back end
storage cluster, stateful packet inspection will use match incoming
flows to signatures in flow tables to send incoming packets to their
intended deep packet inspection (DPI) devices, and so on.

EFD is a distributor library that uses perfect hashing to determine a
target/value for a given incoming flow key. It has the following
advantages: first, because it uses perfect hashing it does not store the
key itself and hence lookup performance is not dependent on the key
size. Second, the target/value can be any arbitrary value hence the
system designer and/or operator can better optimize service rates and
inter-cluster network traffic locating. Third, since the storage
requirement is much smaller than a hash-based flow table (i.e. better
fit for CPU cache), EFD can scale to millions of flow keys. Finally,
with the current optimized library implementation, performance is fully
scalable with any number of CPU cores.

Flow Based Distribution
-----------------------

Computation Based Schemes
~~~~~~~~~~~~~~~~~~~~~~~~~

Flow distribution and/or load balancing can be simply done using a
stateless computation, for instance using round-robin or a simple
computation based on the flow key as an input. For example, a hash
function can be used to direct a certain flow to a target based on
the flow key (e.g. ``h(key) mod n``) where h(key) is the hash value of the
flow key and n is the number of possible targets.

.. _figure_efd1:

.. figure:: img/efd_i1.*

  Load Balancing Using Front End Node

In this scheme (:numref:`figure_efd1`), the front end server/distributor/load balancer
extracts the flow key from the input packet and applies a computation to determine where
this flow should be directed. Intuitively, this scheme is very simple
and requires no state to be kept at the front end node, and hence,
storage requirements are minimum.

.. _figure_efd2:

.. figure:: img/efd_i2.*

  Consistent Hashing

A widely used flow distributor that belongs to the same category of
computation-based schemes is ``consistent hashing``, shown in :numref:`figure_efd2`.
Target destinations (shown in red) are hashed into the same space as the flow
keys (shown in blue), and keys are mapped to the nearest target in a clockwise
fashion. Dynamically adding and removing targets with consistent hashing
requires only K/n keys to be remapped on average, where K is the number of
keys, and n is the number of targets. In contrast, in a traditional hash-based
scheme, a change in the number of targets causes nearly all keys to be
remapped.

Although computation-based schemes are simple and need very little
storage requirement, they suffer from the drawback that the system
designer/operator can’t fully control the target to assign a specific
key, as this is dictated by the hash function.
Deterministically co-locating of keys together (for example, to minimize
inter-server traffic or to optimize for network traffic conditions,
target load, etc.) is simply not possible.

Flow-Table Based Schemes
~~~~~~~~~~~~~~~~~~~~~~~~

When using a Flow-Table based scheme to handle flow distribution/load
balancing, in contrast with computation-based schemes, the system designer
has the flexibility of assigning a given flow to any given
target. The flow table (e.g. DPDK RTE Hash Library) will simply store
both the flow key and the target value.

.. _figure_efd3:

.. figure:: img/efd_i3.*

  Table Based Flow Distribution

As shown in :numref:`figure_efd3`, when doing a lookup, the flow-table
is indexed with the hash of the flow key and the keys (more than one is possible,
because of hash collision) stored in this index and corresponding values
are retrieved. The retrieved key(s) is matched with the input flow key
and if there is a match the value (target id) is returned.

The drawback of using a hash table for flow distribution/load balancing
is the storage requirement, since the flow table need to store keys,
signatures and target values. This doesn't allow this scheme to scale to
millions of flow keys. Large tables will usually not fit in
the CPU cache, and hence, the lookup performance is degraded because of
the latency to access the main memory.

EFD Based Scheme
~~~~~~~~~~~~~~~~

EFD combines the advantages of both flow-table based and computation-based
schemes. It doesn't require the large storage necessary for
flow-table based schemes (because EFD doesn't store the key as explained
below), and it supports any arbitrary value for any given key.

.. _figure_efd4:

.. figure:: img/efd_i4.*

  Searching for Perfect Hash Function

The basic idea of EFD is when a given key is to be inserted, a family of
hash functions is searched until the correct hash function that maps the
input key to the correct value is found, as shown in :numref:`figure_efd4`.
However, rather than explicitly storing all keys and their associated values,
EFD stores only indices of hash functions that map keys to values, and
thereby consumes much less space than conventional flow-based tables.
The lookup operation is very simple, similar to a computational-based
scheme: given an input key the lookup operation is reduced to hashing
that key with the correct hash function.

.. _figure_efd5:

.. figure:: img/efd_i5.*

  Divide and Conquer for Millions of Keys

Intuitively, finding a hash function that maps each of a large number
(millions) of input keys to the correct output value is effectively
impossible, as a result EFD, as shown in :numref:`figure_efd5`,
breaks the problem into smaller pieces (divide and conquer).
EFD divides the entire input key set into many small groups.
Each group consists of approximately 20-28 keys (a configurable parameter
for the library), then, for each small group, a brute force search to find
a hash function that produces the correct outputs for each key in the group.

It should be mentioned that, since the online lookup table for EFD
doesn't store the key itself, the size of the EFD table is independent
of the key size and hence EFD lookup performance which is almost
constant irrespective of the length of the key which is a highly
desirable feature especially for longer keys.

In summary, EFD is a set separation data structure that supports millions of
keys. It is used to distribute a given key to an intended target. By itself
EFD is not a FIB data structure with an exact match the input flow key.

.. _Efd_example:

Example of EFD Library Usage
----------------------------

EFD can be used along the data path of many network functions and middleboxes.
As previously mentioned, it can used as an index table for
<key,value> pairs, meta-data for objects, a flow-level load balancer, etc.
:numref:`figure_efd6` shows an example of using EFD as a flow-level load
balancer, where flows are received at a front end server before being forwarded
to the target back end server for processing. The system designer would
deterministically co-locate flows together in order to minimize cross-server
interaction.
(For example, flows requesting certain webpage objects are co-located
together, to minimize forwarding of common objects across servers).

.. _figure_efd6:

.. figure:: img/efd_i6.*

  EFD as a Flow-Level Load Balancer

As shown in :numref:`figure_efd6`, the front end server will have an EFD table that
stores for each group what is the perfect hash index that satisfies the
correct output. Because the table size is small and fits in cache (since
keys are not stored), it sustains a large number of flows (N*X, where N
is the maximum number of flows served by each back end server of the X
possible targets).

With an input flow key, the group id is computed (for example, using
last few bits of CRC hash) and then the EFD table is indexed with the
group id to retrieve the corresponding hash index to use. Once the index
is retrieved the key is hashed using this hash function and the result
will be the intended correct target where this flow is supposed to be
processed.

It should be noted that as a result of EFD not matching the exact key but
rather distributing the flows to a target back end node based on the
perfect hash index, a key that has not been inserted before
will be distributed to a valid target. Hence, a local table which stores
the flows served at each node is used and is
exact matched with the input key to rule out new never seen before
flows.

.. _Efd_api:

Library API Overview
--------------------

The EFD library API is created with a very similar semantics of a
hash-index or a flow table. The application creates an EFD table for a
given maximum number of flows, a function is called to insert a flow key
with a specific target value, and another function is used to retrieve
target values for a given individual flow key or a bulk of keys.

EFD Table Create
~~~~~~~~~~~~~~~~

The function ``rte_efd_create()`` is used to create and return a pointer
to an EFD table that is sized to hold up to num_flows key.
The online version of the EFD table (the one that does
not store the keys and is used for lookups) will be allocated and
created in the last level cache (LLC) of the socket defined by the
online_socket_bitmask, while the offline EFD table (the one that
stores the keys and is used for key inserts and for computing the
perfect hashing) is allocated and created in the LLC of the socket
defined by offline_socket_bitmask. It should be noted, that for
highest performance the socket id should match that where the thread is
running, i.e. the online EFD lookup table should be created on the same
socket as where the lookup thread is running.

EFD Insert and Update
~~~~~~~~~~~~~~~~~~~~~

The EFD function to insert a key or update a key to a new value is
``rte_efd_update()``. This function will update an existing key to
a new value (target) if the key has already been inserted
before, or will insert the <key,value> pair if this key has not been inserted
before. It will return 0 upon success. It will return
``EFD_UPDATE_WARN_GROUP_FULL (1)`` if the operation is insert, and the
last available space in the key's group was just used. It will return
``EFD_UPDATE_FAILED (2)`` when the insertion or update has failed (either it
failed to find a suitable perfect hash or the group was full). The function
will return ``EFD_UPDATE_NO_CHANGE (3)`` if there is no change to the EFD
table (i.e, same value already exists).

.. Note::

   This function is not multi-thread safe and should only be called
   from one thread.

EFD Lookup
~~~~~~~~~~

To lookup a certain key in an EFD table, the function ``rte_efd_lookup()``
is used to return the value associated with single key.
As previously mentioned, if the key has been inserted, the correct value
inserted is returned, if the key has not been inserted before,
a ‘random’ value (based on hashing of the key) is returned.
For better performance and to decrease the overhead of
function calls per key, it is always recommended to use a bulk lookup
function (simultaneous lookup of multiple keys) instead of a single key
lookup function. ``rte_efd_lookup_bulk()`` is the bulk lookup function,
that looks up num_keys simultaneously stored in the key_list and the
corresponding return values will be returned in the value_list.

.. Note::

   This function is multi-thread safe, but there should not be other threads
   writing in the EFD table, unless locks are used.

EFD Delete
~~~~~~~~~~

To delete a certain key in an EFD table, the function
``rte_efd_delete()`` can be used. The function returns zero upon success
when the key has been found and deleted. Socket_id is the parameter to
use to lookup the existing value, which is ideally the caller's socket id.
The previous value associated with this key will be returned
in the prev_value argument.

.. Note::

   This function is not multi-thread safe and should only be called
   from one thread.

.. _Efd_internals:

Library Internals
-----------------

This section provides the brief high-level idea and an overview
of the library internals to accompany the RFC. The intent of this
section is to explain to readers the high-level implementation of
insert, lookup and group rebalancing in the EFD library.

Insert Function Internals
~~~~~~~~~~~~~~~~~~~~~~~~~

As previously mentioned the EFD divides the whole set of keys into
groups of a manageable size (e.g. 28 keys) and then searches for the
perfect hash that satisfies the intended target value for each key. EFD
stores two version of the <key,value> table:

-  Offline Version (in memory): Only used for the insertion/update
   operation, which is less frequent than the lookup operation. In the
   offline version the exact keys for each group is stored. When a new
   key is added, the hash function is updated that will satisfy the
   value for the new key together with the all old keys already inserted
   in this group.

-  Online Version (in cache): Used for the frequent lookup operation. In
   the online version, as previously mentioned, the keys are not stored
   but rather only the hash index for each group.

.. _figure_efd7:

.. figure:: img/efd_i7.*

  Group Assignment

:numref:`figure_efd7` depicts the group assignment for 7 flow keys as an example.
Given a flow key, a hash function (in our implementation CRC hash) is
used to get the group id. As shown in the figure, the groups can be
unbalanced. (We highlight group rebalancing further below).

.. _figure_efd8:

.. figure:: img/efd_i8.*

  Perfect Hash Search - Assigned Keys & Target Value

Focusing on one group that has four keys, :numref:`figure_efd8` depicts the search
algorithm to find the perfect hash function. Assuming that the target
value bit for the keys is as shown in the figure, then the online EFD
table will store a 16 bit hash index and 16 bit lookup table per group
per value bit.

.. _figure_efd9:

.. figure:: img/efd_i9.*

  Perfect Hash Search - Satisfy Target Values

For a given keyX, a hash function ``(h(keyX, seed1) + index * h(keyX, seed2))``
is used to point to certain bit index in the 16bit lookup_table value,
as shown in :numref:`figure_efd9`.
The insert function will brute force search for all possible values for the
hash index until a non conflicting lookup_table is found.

.. _figure_efd10:

.. figure:: img/efd_i10.*

  Finding Hash Index for Conflict Free lookup_table

For example, since both key3 and key7 have a target bit value of 1, it
is okay if the hash function of both keys point to the same bit in the
lookup table. A conflict will occur if a hash index is used that maps
both Key4 and Key7 to the same index in the lookup_table,
as shown in :numref:`figure_efd10`, since their target value bit are not the same.
Once a hash index is found that produces a lookup_table with no
contradictions, this index is stored for this group. This procedure is
repeated for each bit of target value.

Lookup Function Internals
~~~~~~~~~~~~~~~~~~~~~~~~~

The design principle of EFD is that lookups are much more frequent than
inserts, and hence, EFD's design optimizes for the lookups which are
faster and much simpler than the slower insert procedure (inserts are
slow, because of perfect hash search as previously discussed).

.. _figure_efd11:

.. figure:: img/efd_i11.*

  EFD Lookup Operation

:numref:`figure_efd11` depicts the lookup operation for EFD. Given an input key,
the group id is computed (using CRC hash) and then the hash index for this
group is retrieved from the EFD table. Using the retrieved hash index,
the hash function ``h(key, seed1) + index *h(key, seed2)`` is used which will
result in an index in the lookup_table, the bit corresponding to this
index will be the target value bit. This procedure is repeated for each
bit of the target value.

Group Rebalancing Function Internals
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When discussing EFD inserts and lookups, the discussion is simplified by
assuming that a group id is simply a result of hash function. However,
since hashing in general is not perfect and will not always produce a
uniform output, this simplified assumption will lead to unbalanced
groups, i.e., some group will have more keys than other groups.
Typically, and to minimize insert time with an increasing number of keys,
it is preferable that all groups will have a balanced number of keys, so
the brute force search for the perfect hash terminates with a valid hash
index. In order to achieve this target, groups are rebalanced during
runtime inserts, and keys are moved around from a busy group to a less
crowded group as the more keys are inserted.

.. _figure_efd12:

.. figure:: img/efd_i12.*

  Runtime Group Rebalancing

:numref:`figure_efd12` depicts the high level idea of group rebalancing, given an
input key the hash result is split into two parts a chunk id and 8-bit
bin id. A chunk contains 64 different groups and 256 bins (i.e. for any
given bin it can map to 4 distinct groups). When a key is inserted, the
bin id is computed, for example in :numref:`figure_efd12` bin_id=2,
and since each bin can be mapped to one of four different groups (2 bit storage),
the four possible mappings are evaluated and the one that will result in a
balanced key distribution across these four is selected the mapping result
is stored in these two bits.


.. _Efd_references:

References
-----------

1- EFD is based on collaborative research work between Intel and
Carnegie Mellon University (CMU), interested readers can refer to the paper
“Scaling Up Clustered Network Appliances with ScaleBricks;” Dong Zhou et al.
at SIGCOMM 2015 (`http://conferences.sigcomm.org/sigcomm/2015/pdf/papers/p241.pdf`)
for more information.
