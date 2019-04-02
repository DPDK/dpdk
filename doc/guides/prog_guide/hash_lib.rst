..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2015 Intel Corporation.
    Copyright(c) 2018 Arm Limited.

.. _Hash_Library:

Hash Library
============

The DPDK provides a Hash Library for creating hash table for fast lookup.
The hash table is a data structure optimized for searching through a set of entries that are each identified by a unique key.
For increased performance the DPDK Hash requires that all the keys have the same number of bytes which is set at the hash creation time.

Hash API Overview
-----------------

The main configuration parameters for the hash table are:

*   Total number of hash entries in the table

*   Size of the key in bytes

*   An extra flag to describe additional settings, for example the multithreading mode of operation and extendable bucket functionality (as will be described later)

The hash table also allows the configuration of some low-level implementation related parameters such as:

*   Hash function to translate the key into a hash value

The main methods exported by the Hash Library are:

*   Add entry with key: The key is provided as input. If the new entry is successfully added to the hash table for the specified key,
    or there is already an entry in the hash table for the specified key, then the position of the entry is returned.
    If the operation was not successful, for example due to lack of free entries in the hash table, then a negative value is returned.

*   Delete entry with key: The key is provided as input. If an entry with the specified key is found in the hash,
    then the entry is removed from the hash table and the position where the entry was found in the hash table is returned.
    If no entry with the specified key exists in the hash table, then a negative value is returned

*   Lookup for entry with key: The key is provided as input. If an entry with the specified key is found in the hash table (i.e., lookup hit),
    then the position of the entry is returned, otherwise (i.e., lookup miss) a negative value is returned.

Apart from the basic methods explained above, the Hash Library API provides a few more advanced methods to query and update the hash table:

*   Add / lookup / delete entry with key and precomputed hash: Both the key and its precomputed hash are provided as input. This allows
    the user to perform these operations faster, as the hash value is already computed.

*   Add / lookup entry with key and data: A data is provided as input for add. Add allows the user to store
    not only the key, but also the data which may be either a 8-byte integer or a pointer to external data (if data size is more than 8 bytes).

*   Combination of the two options above: User can provide key, precomputed hash, and data.

*   Ability to not free the position of the entry in the hash table upon calling delete. This is useful for multi-threaded scenarios where
    readers continue to use the position even after the entry is deleted.

Also, the API contains a method to allow the user to look up entries in batches, achieving higher performance
than looking up individual entries, as the function prefetches next entries at the time it is operating
with the current ones, which reduces significantly the performance overhead of the necessary memory accesses.


The actual data associated with each key can be either managed by the user using a separate table that
mirrors the hash in terms of number of entries and position of each entry,
as shown in the Flow Classification use case described in the following sections,
or stored in the hash table itself.

The example hash tables in the L2/L3 Forwarding sample applications define which port to forward a packet to based on a packet flow identified by the five-tuple lookup.
However, this table could also be used for more sophisticated features and provide many other functions and actions that could be performed on the packets and flows.

Multi-process support
---------------------

The hash library can be used in a multi-process environment.
The only function that can only be used in single-process mode is rte_hash_set_cmp_func(), which sets up
a custom compare function, which is assigned to a function pointer (therefore, it is not supported in
multi-process mode).


Multi-thread support
---------------------

The hash library supports multithreading, and the user specifies the needed mode of operation at the creation time of the hash table
by appropriately setting the flag. In all modes of operation lookups are thread-safe meaning lookups can be called from multiple
threads concurrently.

For concurrent writes, and concurrent reads and writes the following flag values define the corresponding modes of operation:

*  If the multi-writer flag (RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD) is set, multiple threads writing to the table is allowed.
   Key add, delete, and table reset are protected from other writer threads. With only this flag set, readers are not protected from ongoing writes.

*  If the read/write concurrency (RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY) is set, multithread read/write operation is safe
   (i.e., application does not need to stop the readers from accessing the hash table until writers finish their updates. Readers and writers can operate on the table concurrently).
   The library uses a reader-writer lock to provide the concurrency.

*  In addition to these two flag values, if the transactional memory flag (RTE_HASH_EXTRA_FLAGS_TRANS_MEM_SUPPORT) is also set,
   the reader-writer lock will use hardware transactional memory (e.g., Intel® TSX) if supported to guarantee thread safety.
   If the platform supports Intel® TSX, it is advised to set the transactional memory flag, as this will speed up concurrent table operations.
   Otherwise concurrent operations will be slower because of the overhead associated with the software locking mechanisms.

*  If lock free read/write concurrency (RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF) is set, read/write concurrency is provided without using reader-writer lock.
   For platforms (e.g., current ARM based platforms) that do not support transactional memory, it is advised to set this flag to achieve greater scalability in performance.
   If this flag is set, the (RTE_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL) flag is set by default.

*  If the 'do not free on delete' (RTE_HASH_EXTRA_FLAGS_NO_FREE_ON_DEL) flag is set, the position of the entry in the hash table is not freed upon calling delete(). This flag is enabled
   by default when the lock free read/write concurrency flag is set. The application should free the position after all the readers have stopped referencing the position.
   Where required, the application can make use of RCU mechanisms to determine when the readers have stopped referencing the position.

Extendable Bucket Functionality support
----------------------------------------
An extra flag is used to enable this functionality (flag is not set by default). When the (RTE_HASH_EXTRA_FLAGS_EXT_TABLE) is set and
in the very unlikely case due to excessive hash collisions that a key has failed to be inserted, the hash table bucket is extended with a linked
list to insert these failed keys. This feature is important for the workloads (e.g. telco workloads) that need to insert up to 100% of the
hash table size and can't tolerate any key insertion failure (even if very few).
Please note that with the 'lock free read/write concurrency' flag enabled, users need to call 'rte_hash_free_key_with_position' API in order to free the empty buckets and
deleted keys, to maintain the 100% capacity guarantee.

Implementation Details (non Extendable Bucket Case)
---------------------------------------------------

The hash table has two main tables:

* First table is an array of buckets each of which consists of multiple entries,
  Each entry contains the signature
  of a given key (explained below), and an index to the second table.

* The second table is an array of all the keys stored in the hash table and its data associated to each key.

The hash library uses the Cuckoo Hash algorithm to resolve collisions.
For any input key, there are two possible buckets (primary and secondary/alternative location)
to store that key in the hash table, therefore only the entries within those two buckets need to be examined
when the key is looked up.
The Hash Library uses a hash function (configurable) to translate the input key into a 4-byte hash value.
The bucket index and a 2-byte signature is derived from the hash value using partial-key hashing [partial-key].

Once the buckets are identified, the scope of the key add,
delete, and lookup operations is reduced to the entries in those buckets (it is very likely that entries are in the primary bucket).

To speed up the search logic within the bucket, each hash entry stores the 2-byte key signature together with the full key for each hash table entry.
For large key sizes, comparing the input key against a key from the bucket can take significantly more time than
comparing the 2-byte signature of the input key against the signature of a key from the bucket.
Therefore, the signature comparison is done first and the full key comparison is done only when the signatures matches.
The full key comparison is still necessary, as two input keys from the same bucket can still potentially have the same 2-byte signature,
although this event is relatively rare for hash functions providing good uniform distributions for the set of input keys.

Example of lookup:

First of all, the primary bucket is identified and entry is likely to be stored there.
If signature was stored there, we compare its key against the one provided and return the position
where it was stored and/or the data associated to that key if there is a match.
If signature is not in the primary bucket, the secondary bucket is looked up, where same procedure
is carried out. If there is no match there either, key is not in the table and a negative value will be returned.

Example of addition:

Like lookup, the primary and secondary buckets are identified. If there is an empty entry in
the primary bucket, a signature is stored in that entry, key and data (if any) are added to
the second table and the index in the second table is stored in the entry of the first table.
If there is no space in the primary bucket, one of the entries on that bucket is pushed to its alternative location,
and the key to be added is inserted in its position.
To know where the alternative bucket of the evicted entry is, a mechanism called partial-key hashing [partial-key] is used.
If there is room in the alternative bucket, the evicted entry
is stored in it. If not, same process is repeated (one of the entries gets pushed) until an empty entry is found.
Notice that despite all the entry movement in the first table, the second table is not touched, which would impact
greatly in performance.

In the very unlikely event that an empty entry cannot be found after certain number of displacements,
key is considered not able to be added (unless extendable bucket flag is set, and in that case the bucket is extended to insert the key, as will be explained later).
With random keys, this method allows the user to get more than 90% table utilization, without
having to drop any stored entry (e.g. using a LRU replacement policy) or allocate more memory (extendable buckets or rehashing).


Example of deletion:

Similar to lookup, the key is searched in its primary and secondary buckets. If the key is found, the
entry is marked as empty. If the hash table was configured with 'no free on delete' or 'lock free read/write concurrency',
the position of the key is not freed. It is the responsibility of the user to free the position after
readers are not referencing the position anymore.


Implementation Details (with Extendable Bucket)
-------------------------------------------------
When the RTE_HASH_EXTRA_FLAGS_EXT_TABLE flag is set, the hash table implementation still uses the same Cuckoo Hash algorithm to store the keys into
the first and second tables. However, in the very unlikely event that a key can't be inserted after certain number of the Cuckoo displacements is
reached, the secondary bucket of this key is extended
with a linked list of extra buckets and the key is stored in this linked list.

In case of lookup for a certain key, as before, the primary bucket is searched for a match and then the secondary bucket is looked up.
If there is no match there either, the extendable buckets (linked list of extra buckets) are searched one by one for a possible match and if there is no match
the key is considered not to be in the table.

The deletion is the same as the case when the RTE_HASH_EXTRA_FLAGS_EXT_TABLE flag is not set. With one exception, if a key is deleted from any bucket
and an empty location is created, the last entry from the extendable buckets associated with this bucket is displaced into
this empty location to possibly shorten the linked list.


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

*   Free flow: Free flow key position. If 'no free on delete' or 'lock-free read/write concurrency' flags are set,
    wait till the readers are not referencing the position returned during add/delete flow and then free the position.
    RCU mechanisms can be used to find out when the readers are not referencing the position anymore.

*   Lookup flow: Lookup for the flow key in the hash.
    If the returned position is valid (flow lookup hit), use the returned position to access the flow entry in the flow table.
    Otherwise (flow lookup miss) there is no flow registered for the current packet.

References
----------

*   Donald E. Knuth, The Art of Computer Programming, Volume 3: Sorting and Searching (2nd Edition), 1998, Addison-Wesley Professional
* [partial-key] Bin Fan, David G. Andersen, and Michael Kaminsky, MemC3: compact and concurrent MemCache with dumber caching and smarter hashing, 2013, NSDI
