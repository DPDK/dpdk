..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

*   Number of entries per bucket

The main methods exported by the hash are:

*   Add entry with key: The key is provided as input. If a new entry is successfully added to the hash for the specified key,
    or there is already an entry in the hash for the specified key, then the position of the entry is returned.
    If the operation was not successful, for example due to lack of free entries in the hash, then a negative value is returned;

*   Delete entry with key: The key is provided as input. If an entry with the specified key is found in the hash,
    then the entry is removed from the hash and the position where the entry was found in the hash is returned.
    If no entry with the specified key exists in the hash, then a negative value is returned

*   Lookup for entry with key: The key is provided as input. If an entry with the specified key is found in the hash (lookup hit),
    then the position of the entry is returned, otherwise (lookup miss) a negative value is returned.

The current hash implementation handles the key management only.
The actual data associated with each key has to be managed by the user using a separate table that
mirrors the hash in terms of number of entries and position of each entry,
as shown in the Flow Classification use case describes in the following sections.

The example hash tables in the L2/L3 Forwarding sample applications defines which port to forward a packet to based on a packet flow identified by the five-tuple lookup.
However, this table could also be used for more sophisticated features and provide many other functions and actions that could be performed on the packets and flows.

Implementation Details
----------------------

The hash table is implemented as an array of entries which is further divided into buckets,
with the same number of consecutive array entries in each bucket.
For any input key, there is always a single bucket where that key can be stored in the hash,
therefore only the entries within that bucket need to be examined when the key is looked up.
The lookup speed is achieved by reducing the number of entries to be scanned from the total
number of hash entries down to the number of entries in a hash bucket,
as opposed to the basic method of linearly scanning all the entries in the array.
The hash uses a hash function (configurable) to translate the input key into a 4-byte key signature.
The bucket index is the key signature modulo the number of hash buckets.
Once the bucket is identified, the scope of the hash add,
delete and lookup operations is reduced to the entries in that bucket.

To speed up the search logic within the bucket, each hash entry stores the 4-byte key signature together with the full key for each hash entry.
For large key sizes, comparing the input key against a key from the bucket can take significantly more time than
comparing the 4-byte signature of the input key against the signature of a key from the bucket.
Therefore, the signature comparison is done first and the full key comparison done only when the signatures matches.
The full key comparison is still necessary, as two input keys from the same bucket can still potentially have the same 4-byte hash signature,
although this event is relatively rare for hash functions providing good uniform distributions for the set of input keys.

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
