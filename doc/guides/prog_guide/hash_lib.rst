..  BSD LICENSE
    Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

.. _Hash_Library:

Hash Library
============

The DPDK provides a Hash Library for creating hash table for fast lookup.
The hash table is a data structure optimized for searching through a set of entries that are each identified by a unique key.
For increased performance the DPDK Hash requires that all the keys have the same number of bytes which is set at the hash creation time.

Hash API Overview
-----------------

The main configuration parameters for the hash are:

*   Total number of hash entries

*   Size of the key in bytes

The hash also allows the configuration of some low-level implementation related parameters such as:

*   Hash function to translate the key into a bucket index

The main methods exported by the hash are:

*   Add entry with key: The key is provided as input. If a new entry is successfully added to the hash for the specified key,
    or there is already an entry in the hash for the specified key, then the position of the entry is returned.
    If the operation was not successful, for example due to lack of free entries in the hash, then a negative value is returned;

*   Delete entry with key: The key is provided as input. If an entry with the specified key is found in the hash,
    then the entry is removed from the hash and the position where the entry was found in the hash is returned.
    If no entry with the specified key exists in the hash, then a negative value is returned

*   Lookup for entry with key: The key is provided as input. If an entry with the specified key is found in the hash (lookup hit),
    then the position of the entry is returned, otherwise (lookup miss) a negative value is returned.

Apart from these method explained above, the API allows the user three more options:

*   Add / lookup / delete with key and precomputed hash: Both the key and its precomputed hash are provided as input. This allows
    the user to perform these operations faster, as hash is already computed.

*   Add / lookup with key and data: A pair of key-value is provided as input. This allows the user to store
    not only the key, but also data which may be either a 8-byte integer or a pointer to external data (if data size is more than 8 bytes).

*   Combination of the two options above: User can provide key, precomputed hash and data.

Also, the API contains a method to allow the user to look up entries in bursts, achieving higher performance
than looking up individual entries, as the function prefetches next entries at the time it is operating
with the first ones, which reduces significantly the impact of the necessary memory accesses.
Notice that this method uses a pipeline of 8 entries (4 stages of 2 entries), so it is highly recommended
to use at least 8 entries per burst.

The actual data associated with each key can be either managed by the user using a separate table that
mirrors the hash in terms of number of entries and position of each entry,
as shown in the Flow Classification use case describes in the following sections,
or stored in the hash table itself.

The example hash tables in the L2/L3 Forwarding sample applications defines which port to forward a packet to based on a packet flow identified by the five-tuple lookup.
However, this table could also be used for more sophisticated features and provide many other functions and actions that could be performed on the packets and flows.

Multi-process support
---------------------

The hash library can be used in a multi-process environment, minding that only lookups are thread-safe.
The only function that can only be used in single-process mode is rte_hash_set_cmp_func(), which sets up
a custom compare function, which is assigned to a function pointer (therefore, it is not supported in
multi-process mode).

Implementation Details
----------------------

The hash table has two main tables:

* First table is an array of entries which is further divided into buckets,
  with the same number of consecutive array entries in each bucket. Each entry contains the computed primary
  and secondary hashes of a given key (explained below), and an index to the second table.

* The second table is an array of all the keys stored in the hash table and its data associated to each key.

The hash library uses the cuckoo hash method to resolve collisions.
For any input key, there are two possible buckets (primary and secondary/alternative location)
where that key can be stored in the hash, therefore only the entries within those bucket need to be examined
when the key is looked up.
The lookup speed is achieved by reducing the number of entries to be scanned from the total
number of hash entries down to the number of entries in the two hash buckets,
as opposed to the basic method of linearly scanning all the entries in the array.
The hash uses a hash function (configurable) to translate the input key into a 4-byte key signature.
The bucket index is the key signature modulo the number of hash buckets.

Once the buckets are identified, the scope of the hash add,
delete and lookup operations is reduced to the entries in those buckets (it is very likely that entries are in the primary bucket).

To speed up the search logic within the bucket, each hash entry stores the 4-byte key signature together with the full key for each hash entry.
For large key sizes, comparing the input key against a key from the bucket can take significantly more time than
comparing the 4-byte signature of the input key against the signature of a key from the bucket.
Therefore, the signature comparison is done first and the full key comparison done only when the signatures matches.
The full key comparison is still necessary, as two input keys from the same bucket can still potentially have the same 4-byte hash signature,
although this event is relatively rare for hash functions providing good uniform distributions for the set of input keys.

Example of lookup:

First of all, the primary bucket is identified and entry is likely to be stored there.
If signature was stored there, we compare its key against the one provided and return the position
where it was stored and/or the data associated to that key if there is a match.
If signature is not in the primary bucket, the secondary bucket is looked up, where same procedure
is carried out. If there is no match there either, key is considered not to be in the table.

Example of addition:

Like lookup, the primary and secondary buckets are identified. If there is an empty slot in
the primary bucket, primary and secondary signatures are stored in that slot, key and data (if any) are added to
the second table and an index to the position in the second table is stored in the slot of the first table.
If there is no space in the primary bucket, one of the entries on that bucket is pushed to its alternative location,
and the key to be added is inserted in its position.
To know where the alternative bucket of the evicted entry is, the secondary signature is looked up and alternative bucket index
is calculated from doing the modulo, as seen above. If there is room in the alternative bucket, the evicted entry
is stored in it. If not, same process is repeated (one of the entries gets pushed) until a non full bucket is found.
Notice that despite all the entry movement in the first table, the second table is not touched, which would impact
greatly in performance.

In the very unlikely event that table enters in a loop where same entries are being evicted indefinitely,
key is considered not able to be stored.
With random keys, this method allows the user to get around 90% of the table utilization, without
having to drop any stored entry (LRU) or allocate more memory (extended buckets).

Entry distribution in hash table
--------------------------------

As mentioned above, Cuckoo hash implementation pushes elements out of their bucket,
if there is a new entry to be added which primary location coincides with their current bucket,
being pushed to their alternative location.
Therefore, as user adds more entries to the hash table, distribution of the hash values
in the buckets will change, being most of them in their primary location and a few in
their secondary location, which the later will increase, as table gets busier.
This information is quite useful, as performance may be lower as more entries
are evicted to their secondary location.

See the tables below showing example entry distribution as table utilization increases.

.. _table_hash_lib_1:

.. table:: Entry distribution measured with an example table with 1024 random entries using jhash algorithm

   +--------------+-----------------------+-------------------------+
   | % Table used | % In Primary location | % In Secondary location |
   +==============+=======================+=========================+
   |      25      |         100           |           0             |
   +--------------+-----------------------+-------------------------+
   |      50      |         96.1          |           3.9           |
   +--------------+-----------------------+-------------------------+
   |      75      |         88.2          |           11.8          |
   +--------------+-----------------------+-------------------------+
   |      80      |         86.3          |           13.7          |
   +--------------+-----------------------+-------------------------+
   |      85      |         83.1          |           16.9          |
   +--------------+-----------------------+-------------------------+
   |      90      |         77.3          |           22.7          |
   +--------------+-----------------------+-------------------------+
   |      95.8    |         64.5          |           35.5          |
   +--------------+-----------------------+-------------------------+

|

.. _table_hash_lib_2:

.. table:: Entry distribution measured with an example table with 1 million random entries using jhash algorithm

   +--------------+-----------------------+-------------------------+
   | % Table used | % In Primary location | % In Secondary location |
   +==============+=======================+=========================+
   |      50      |         96            |           4             |
   +--------------+-----------------------+-------------------------+
   |      75      |         86.9          |           13.1          |
   +--------------+-----------------------+-------------------------+
   |      80      |         83.9          |           16.1          |
   +--------------+-----------------------+-------------------------+
   |      85      |         80.1          |           19.9          |
   +--------------+-----------------------+-------------------------+
   |      90      |         74.8          |           25.2          |
   +--------------+-----------------------+-------------------------+
   |      94.5    |         67.4          |           32.6          |
   +--------------+-----------------------+-------------------------+

.. note::

   Last values on the tables above are the average maximum table
   utilization with random keys and using Jenkins hash function.

Use Case: Flow Classification
-----------------------------

Flow classification is used to map each input packet to the connection/flow it belongs to.
This operation is necessary as the processing of each input packet is usually done in the context of their connection,
so the same set of operations is applied to all the packets from the same flow.

Applications using flow classification typically have a flow table to manage, with each separate flow having an entry associated with it in this table.
The size of the flow table entry is application specific, with typical values of 4, 16, 32 or 64 bytes.

Each application using flow classification typically has a mechanism defined to uniquely identify a flow based on
a number of fields read from the input packet that make up the flow key.
One example is to use the DiffServ 5-tuple made up of the following fields of the IP and transport layer packet headers:
Source IP Address, Destination IP Address, Protocol, Source Port, Destination Port.

The DPDK hash provides a generic method to implement an application specific flow classification mechanism.
Given a flow table implemented as an array, the application should create a hash object with the same number of entries as the flow table and
with the hash key size set to the number of bytes in the selected flow key.

The flow table operations on the application side are described below:

*   Add flow: Add the flow key to hash.
    If the returned position is valid, use it to access the flow entry in the flow table for adding a new flow or
    updating the information associated with an existing flow.
    Otherwise, the flow addition failed, for example due to lack of free entries for storing new flows.

*   Delete flow: Delete the flow key from the hash. If the returned position is valid,
    use it to access the flow entry in the flow table to invalidate the information associated with the flow.

*   Lookup flow: Lookup for the flow key in the hash.
    If the returned position is valid (flow lookup hit), use the returned position to access the flow entry in the flow table.
    Otherwise (flow lookup miss) there is no flow registered for the current packet.

References
----------

*   Donald E. Knuth, The Art of Computer Programming, Volume 3: Sorting and Searching (2nd Edition), 1998, Addison-Wesley Professional
