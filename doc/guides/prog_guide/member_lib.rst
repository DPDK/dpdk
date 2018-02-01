..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

.. _member_library:

Membership Library
==================

Introduction
------------

The DPDK Membership Library provides an API for DPDK applications to insert a
new member, delete an existing member, or query the existence of a member in a
given set, or a group of sets. For the case of a group of sets, the library
will return not only whether the element has been inserted before in one of
the sets but also which set it belongs to.  The Membership Library is an
extension and generalization of a traditional filter structure (for example
Bloom Filter [Member-bloom]) that has multiple usages in a wide variety of
workloads and applications. In general, the Membership Library is a data
structure that provides a "set-summary" on whether a member belongs to a set,
and as discussed in detail later, there are two advantages of using such a
set-summary rather than operating on a "full-blown" complete list of elements:
first, it has a much smaller storage requirement than storing the whole list of
elements themselves, and secondly checking an element membership (or other
operations) in this set-summary is much faster than checking it for the
original full-blown complete list of elements.

We use the term "Set-Summary" in this guide to refer to the space-efficient,
probabilistic membership data structure that is provided by the library. A
membership test for an element will return the set this element belongs to or
that the element is "not-found" with very high probability of accuracy. Set-summary
is a fundamental data aggregation component that can be used in many network
(and other) applications. It is a crucial structure to address performance and
scalability issues of diverse network applications including overlay networks,
data-centric networks, flow table summaries, network statistics and
traffic monitoring. A set-summary is useful for applications who need to
include a list of elements while a complete list requires too much space
and/or too much processing cost. In these situations, the set-summary works as
a lossy hash-based representation of a set of members. It can dramatically
reduce space requirement and significantly improve the performance of set
membership queries at the cost of introducing a very small membership test error
probability.

.. _figure_membership1:
.. figure:: img/member_i1.*

  Example Usages of Membership Library

There are various usages for a Membership Library in a very
large set of applications and workloads. Interested readers can refer to
[Member-survey] for a survey of possible networking usages. The above figure
provide a small set of examples of using the Membership Library:

* Sub-figure (a)
  depicts a distributed web cache architecture where a collection of proxies
  attempt to share their web caches (cached from a set of back-end web servers) to
  provide faster responses to clients, and the proxies use the Membership
  Library to share summaries of what web pages/objects they are caching. With the
  Membership Library, a proxy receiving an http request will inquire the
  set-summary to find its location and quickly determine whether to retrieve the
  requested web page from a nearby proxy or from a back-end web server.

* Sub-figure (b) depicts another example for using the Membership Library to
  prevent routing loops which is typically done using slow TTL countdown and
  dropping packets when TTL expires. As shown in Sub-figure (b), an embedded
  set-summary in the packet header itself can be used to summarize the set of
  nodes a packet has gone through, and each node upon receiving a packet can check
  whether its id is a member of the set of visited nodes, and if it is, then a
  routing loop is detected.

* Sub-Figure (c) presents another usage of the Membership
  Library to load-balance flows to worker threads with in-order guarantee where a
  set-summary is used to query if a packet belongs to an existing flow or a new
  flow. Packets belonging to a new flow are forwarded to the current least loaded
  worker thread, while those belonging to an existing flow are forwarded to the
  pre-assigned thread to guarantee in-order processing.

* Sub-figure (d) highlights
  yet another usage example in the database domain where a set-summary is used to
  determine joins between sets instead of creating a join by comparing each
  element of a set against the other elements in a different set, a join is done
  on the summaries since they can efficiently encode members of a given set.

Membership Library is a configurable library that is optimized to cover set
membership functionality for both a single set and multi-set scenarios. Two set-summary
schemes are presented including (a) vector of Bloom Filters and (b) Hash-Table based
set-summary schemes with and without false negative probability.
This guide first briefly describes these different types of set-summaries, usage examples for each,
and then it highlights the Membership Library API.

Vector of Bloom Filters
-----------------------

Bloom Filter (BF) [Member-bloom] is a well-known space-efficient
probabilistic data structure that answers set membership queries (test whether
an element is a member of a set) with some probability of false positives and
zero false negatives; a query for an element returns either it is "possibly in
a set" (with very high probability) or "definitely not in a set".

The BF is a method for representing a set of ``n`` elements (for example flow keys
in network applications domain) to support membership queries. The idea of BF is
to allocate a bit-vector ``v`` with ``m`` bits, which are initially all set to 0. Then
it chooses ``k`` independent hash functions ``h1``, ``h2``, ... ``hk`` with hash values range from
``0`` to ``m-1`` to perform hashing calculations on each element to be inserted. Every time when an
element ``X`` being inserted into the set, the bits at positions ``h1(X)``, ``h2(X)``, ...
``hk(X)`` in ``v`` are set to 1 (any particular bit might be set to 1 multiple times
for multiple different inserted elements). Given a query for any element ``Y``, the
bits at positions ``h1(Y)``, ``h2(Y)``, ... ``hk(Y)`` are checked. If any of them is 0,
then Y is definitely not in the set. Otherwise there is a high probability that
Y is a member of the set with certain false positive probability. As shown in
the next equation, the false positive probability can be made arbitrarily small
by changing the number of hash functions (``k``) and the vector length (``m``).

.. _figure_membership2:
.. figure:: img/member_i2.*

  Bloom Filter False Positive Probability

Without BF, an accurate membership testing could involve a costly hash table
lookup and full element comparison. The advantage of using a BF is to simplify
the membership test into a series of hash calculations and memory accesses for a
small bit-vector, which can be easily optimized. Hence the lookup throughput
(set membership test) can be significantly faster than a normal hash table
lookup with element comparison.

.. _figure_membership3:
.. figure:: img/member_i3.*

  Detecting Routing Loops Using BF

BF is used for applications that need only one set, and the
membership of elements is checked against the BF. The example discussed
in the above figure is one example of potential applications that uses only one
set to capture the node IDs that have been visited so far by the packet. Each
node will then check this embedded BF in the packet header for its own id, and
if the BF indicates that the current node is definitely not in the set then a
loop-free route is guaranteed.


.. _figure_membership4:
.. figure:: img/member_i4.*

  Vector Bloom Filter (vBF) Overview

To support membership test for both multiple sets and a single set,
the library implements a Vector Bloom Filter (vBF) scheme.
vBF basically composes multiple bloom filters into a vector of bloom filers.
The membership test is conducted on all of the
bloom filters concurrently to determine which set(s) it belongs to or none of
them. The basic idea of vBF is shown in the above figure where an element is
used to address multiple bloom filters concurrently and the bloom filter
index(es) with a hit is returned.

.. _figure_membership5:
.. figure:: img/member_i5.*

  vBF for Flow Scheduling to Worker Thread

As previously mentioned, there are many usages of such structures. vBF is used
for applications that need to check membership against multiple sets
simultaneously. The example shown in the above figure uses a set to capture
all flows being assigned for processing at a given worker thread. Upon receiving
a packet the vBF is used to quickly figure out if this packet belongs to a new flow
so as to be forwarded to the current least loaded worker thread, or otherwise it
should be queued for an existing thread to guarantee in-order processing (i.e.
the property of vBF to indicate right away that a given flow is a new one or
not is critical to minimize response time latency).

It should be noted that vBF can be implemented using a set of single bloom
filters with sequential lookup of each BF. However, being able to concurrently
search all set-summaries is a big throughput advantage. In the library, certain
parallelism is realized by the implementation of checking all bloom filters
together.


Hash-Table based Set-Summaries
------------------------------

Hash-table based set-summary (HTSS) is another scheme in the membership library.
Cuckoo filter [Member-cfilter] is an example of HTSS.
HTSS supports multi-set membership testing like
vBF does. However, while vBF is better for a small number of targets, HTSS is more suitable
and can easily outperform vBF when the number of sets is
large, since HTSS uses a single hash table for membership testing while vBF
requires testing a series of Bloom Filters each corresponding to one set.
As a result, generally speaking vBF is more adequate for the case of a small limited number of sets
while HTSS should be used with a larger number of sets.

.. _figure_membership6:
.. figure:: img/member_i6.*

  Using HTSS for Attack Signature Matching

As shown in the above figure, attack signature matching where each set
represents a certain signature length (for correctness of this example, an
attack signature should not be a subset of another one) in the payload is a good
example for using HTSS with 0% false negative (i.e., when an element returns not
found, it has a 100% certainty that it is not a member of any set).  The packet
inspection application benefits from knowing right away that the current payload
does not match any attack signatures in the database to establish its
legitimacy, otherwise a deep inspection of the packet is needed.

HTSS employs a similar but simpler data structure to a traditional hash table,
and the major difference is that HTSS stores only the signatures but not the
full keys/elements which can significantly reduce the footprint of the table.
Along with the signature, HTSS also stores a value to indicate the target set.
When looking up an element, the element is hashed and the HTSS is addressed
to retrieve the signature stored. If the signature matches then the value is
retrieved corresponding to the index of the target set which the element belongs
to. Because signatures can collide, HTSS can still has false positive
probability. Furthermore, if elements are allowed to be
overwritten or evicted when the hash table becomes full, it will also have a
false negative probability. We discuss this case in the next section.

Set-Summaries with False Negative Probability
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As previously mentioned, traditional set-summaries (e.g. Bloom Filters) do not
have a false negative probability, i.e., it is 100% certain when an element
returns "not to be present" for a given set. However, the Membership Library
also supports a set-summary probabilistic data structure based on HTSS which
allows for false negative probability.

In HTSS, when the hash table becomes full, keys/elements will fail to be added
into the table and the hash table has to be resized to accommodate for these new
elements, which can be expensive. However, if we allow new elements to overwrite
or evict existing elements (as a cache typically does), then the resulting
set-summary will begin to have false negative probability. This is because the
element that was evicted from the set-summary may still be present in the target
set. For subsequent inquiries the set-summary will falsely report the element
not being in the set, hence having a false negative probability.

The major usage of HTSS with false negative is to use it as a cache for
distributing elements to different target sets. By allowing HTSS to evict old
elements, the set-summary can keep track of the most recent elements
(i.e. active) as a cache typically does. Old inactive elements (infrequently
used elements) will automatically and eventually get evicted from the
set-summary. It is worth noting that the set-summary still has false positive
probability, which means the application either can tolerate certain false positive
or it has fall-back path when false positive happens.

.. _figure_membership7:
.. figure:: img/member_i7.*

  Using HTSS with False Negatives for Wild Card Classification

HTSS with false negative (i.e. a cache) also has its wide set of applications.
For example wild card flow classification (e.g. ACL rules) highlighted in the
above figure is an example of such application. In that case each target set
represents a sub-table with rules defined by a certain flow mask. The flow masks
are non-overlapping, and for flows matching more than one rule only the highest
priority one is inserted in the corresponding sub-table (interested readers can
refer to the Open vSwitch (OvS) design of Mega Flow Cache (MFC) [Member-OvS]
for further details). Typically the rules will have a large number of distinct
unique masks and hence, a large number of target sets each corresponding to one
mask. Because the active set of flows varies widely based on the network
traffic, HTSS with false negative will act as a cache for <flowid, target ACL
sub-table> pair for the current active set of flows. When a miss occurs (as
shown in red in the above figure) the sub-tables will be searched sequentially
one by one for a possible match, and when found the flow key and target
sub-table will be inserted into the set-summary (i.e. cache insertion) so
subsequent packets from the same flow don’t incur the overhead of the
sequential search of sub-tables.

Library API Overview
--------------------

The design goal of the Membership Library API is to be as generic as possible to
support all the different types of set-summaries we discussed in previous
sections and beyond. Fundamentally, the APIs need to include creation,
insertion, deletion, and lookup.


Set-summary Create
~~~~~~~~~~~~~~~~~~

The ``rte_member_create()`` function is used to create a set-summary structure, the input parameter
is a struct to pass in parameters that needed to initialize the set-summary, while the function returns the
pointer to the created set-summary or ``NULL`` if the creation failed.

The general input arguments used when creating the set-summary should include ``name``
which is the name of the created set-summary, *type* which is one of the types
supported by the library (e.g. ``RTE_MEMBER_TYPE_HT`` for HTSS or ``RTE_MEMBER_TYPE_VBF`` for vBF), and ``key_len``
which is the length of the element/key. There are other parameters
are only used for certain type of set-summary, or which have a slightly different meaning for different types of set-summary.
For example, ``num_keys`` parameter means the maximum number of entries for Hash table based set-summary.
However, for bloom filter, this value means the expected number of keys that could be
inserted into the bloom filter(s). The value is used to calculate the size of each
bloom filter.

We also pass two seeds: ``prim_hash_seed`` and
``sec_hash_seed`` for the primary and secondary hash functions to calculate two independent hash values.
``socket_id`` parameter is the NUMA socket ID for the memory used to create the
set-summary. For HTSS, another parameter ``is_cache`` is used to indicate
if this set-summary is a cache (i.e. with false negative probability) or not.
For vBF, extra parameters are needed. For example, ``num_set`` is the number of
sets needed to initialize the vector bloom filters. This number is equal to the
number of bloom filters will be created.
``false_pos_rate`` is the false positive rate. num_keys and false_pos_rate will be used to determine
the number of hash functions and the bloom filter size.


Set-summary Element Insertion
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``rte_member_add()`` function is used to insert an element/key into a set-summary structure. If it fails an
error is returned. For success the returned value is dependent on the
set-summary mode to provide extra information for the users. For vBF
mode, a return value of 0 means a successful insert. For HTSS mode without false negative, the insert
could fail with ``-ENOSPC`` if the table is full. With false negative (i.e. cache mode),
for insert that does not cause any eviction (i.e. no overwriting happens to an
existing entry) the return value is 0. For insertion that causes eviction, the return
value is 1 to indicate such situation, but it is not an error.

The input arguments for the function should include the ``key`` which is a pointer to the element/key that needs to
be added to the set-summary, and ``set_id`` which is the set id associated
with the key that needs to be added.


Set-summary Element Lookup
~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``rte_member_lookup()`` function looks up a single key/element in the set-summary structure. It
returns as soon as the first match is found. The return value is 1 if a
match is found and 0 otherwise. The arguments for the function include ``key`` which is a pointer to the
element/key that needs to be looked up, and ``set_id`` which is used to return the
first target set id where the key has matched, if any.

The ``rte_member_lookup_bulk()`` function is used to look up a bulk of keys/elements in the
set-summary structure for their first match. Each key lookup returns as soon as the first match is found. The
return value is the number of keys that find a match. The arguments of the function include ``keys``
which is a pointer to a bulk of keys that are to be looked up,
``num_keys`` is the number
of keys that will be looked up, and ``set_ids`` are the return target set
ids for the first match found for each of the input keys. ``set_ids`` is an array
needs to be sized according to the ``num_keys``. If there is no match, the set id
for that key will be set to RTE_MEMBER_NO_MATCH.

The ``rte_member_lookup_multi()`` function looks up a single key/element in the
set-summary structure for multiple matches. It
returns ALL the matches (possibly more than one) found for this key when it
is matched against all target sets (it is worth noting that for cache mode HTSS,
the current implementation matches at most one target set). The return value is
the number of matches
that was found for this key (for cache mode HTSS the return value
should be at most 1). The arguments for the function include ``key`` which is a pointer to the
element/key that needs to be looked up, ``max_match_per_key`` which is to indicate the maximum number of matches
the user expects to find for each key, and ``set_id`` which is used to return all
target set ids where the key has matched, if any. The ``set_id`` array should be sized
according to ``max_match_per_key``. For vBF, the maximum number of matches per key is equal
to the number of sets. For HTSS, the maximum number of matches per key is equal to two time
entry count per bucket. ``max_match_per_key`` should be equal or smaller than the maximum number of
possible matches.

The ``rte_membership_lookup_multi_bulk()`` function looks up a bulk of keys/elements in the
set-summary structure for multiple matches, each key lookup returns ALL the matches (possibly more
than one) found for this key when it is matched against all target sets (cache mode HTSS
matches at most one target set). The
return value is the number of keys that find one or more matches in the
set-summary structure. The arguments of the
function include ``keys`` which is
a pointer to a bulk of keys that are to be looked up, ``num_keys`` is the number
of keys that will be looked up, ``max_match_per_key`` is the possible
maximum number of matches for each key, ``match_count`` which is the returned number
of matches for each key, and ``set_ids`` are the returned target set
ids for all matches found for each keys. ``set_ids`` is 2-D array
containing a 1-D array for each key (the size of 1-D array per key should be set by the user according to ``max_match_per_key``).
``max_match_per_key`` should be equal or smaller than the maximum number of
possible matches, similar to ``rte_member_lookup_multi``.


Set-summary Element Delete
~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``rte_membership_delete()`` function deletes an element/key from a set-summary structure, if it fails
an error is returned. The input arguments should include ``key`` which is a pointer to the
element/key that needs to be deleted from the set-summary, and ``set_id``
which is the set id associated with the key to delete. It is worth noting that current
implementation of vBF does not support deletion [1]_. An error code ``-EINVAL`` will be returned.

.. [1] Traditional bloom filter does not support proactive deletion. Supporting proactive deletion require additional implementation and performance overhead.

References
-----------

[Member-bloom] B H Bloom, "Space/Time Trade-offs in Hash Coding with Allowable Errors," Communications of the ACM, 1970.

[Member-survey] A Broder and M Mitzenmacher, "Network Applications of Bloom Filters: A Survey," in Internet Mathematics, 2005.

[Member-cfilter] B Fan, D G Andersen and M Kaminsky, "Cuckoo Filter: Practically Better Than Bloom," in Conference on emerging Networking Experiments and Technologies, 2014.

[Member-OvS] B Pfaff, "The Design and Implementation of Open vSwitch," in NSDI, 2015.
