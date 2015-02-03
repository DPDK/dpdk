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

.. _LPM_Library:

LPM Library
===========

The DPDK LPM library component implements the Longest Prefix Match (LPM) table search method for 32-bit keys
that is typically used to find the best route match in IP forwarding applications.

LPM API Overview
----------------

The main configuration parameter for LPM component instances is the maximum number of rules to support.
An LPM prefix is represented by a pair of parameters (32- bit key, depth), with depth in the range of 1 to 32.
An LPM rule is represented by an LPM prefix and some user data associated with the prefix.
The prefix serves as the unique identifier of the LPM rule.
In this implementation, the user data is 1-byte long and is called next hop,
in correlation with its main use of storing the ID of the next hop in a routing table entry.

The main methods exported by the LPM component are:

*   Add LPM rule: The LPM rule is provided as input.
    If there is no rule with the same prefix present in the table, then the new rule is added to the LPM table.
    If a rule with the same prefix is already present in the table, the next hop of the rule is updated.
    An error is returned when there is no available rule space left.

*   Delete LPM rule: The prefix of the LPM rule is provided as input.
    If a rule with the specified prefix is present in the LPM table, then it is removed.

*   Lookup LPM key: The 32-bit key is provided as input.
    The algorithm selects the rule that represents the best match for the given key and returns the next hop of that rule.
    In the case that there are multiple rules present in the LPM table that have the same 32-bit key,
    the algorithm picks the rule with the highest depth as the best match rule,
    which means that the rule has the highest number of most significant bits matching between the input key and the rule key.

Implementation Details
----------------------

The current implementation uses a variation of the DIR-24-8 algorithm that trades memory usage for improved LPM lookup speed.
The algorithm allows the lookup operation to be performed with typically a single memory read access.
In the statistically rare case when the best match rule is having a depth bigger than 24,
the lookup operation requires two memory read accesses.
Therefore, the performance of the LPM lookup operation is greatly influenced by
whether the specific memory location is present in the processor cache or not.

The main data structure is built using the following elements:

*   A table with 2^24 entries.

*   A number of tables (RTE_LPM_TBL8_NUM_GROUPS) with 2^8 entries.

The first table, called tbl24, is indexed using the first 24 bits of the IP address to be looked up,
while the second table(s), called tbl8, is indexed using the last 8 bits of the IP address.
This means that depending on the outcome of trying to match the IP address of an incoming packet to the rule stored in the tbl24
we might need to continue the lookup process in the second level.

Since every entry of the tbl24 can potentially point to a tbl8, ideally, we would have 2^24 tbl8s,
which would be the same as having a single table with 2^32 entries.
This is not feasible due to resource restrictions.
Instead, this approach takes advantage of the fact that rules longer than 24 bits are very rare.
By splitting the process in two different tables/levels and limiting the number of tbl8s,
we can greatly reduce memory consumption while maintaining a very good lookup speed (one memory access, most of the times).

.. image39 has been renamed

|tbl24_tbl8|

An entry in tbl24 contains the following fields:

*   next hop / index to the tbl8

*   valid flag

*   external entry flag

*   depth of the rule (length)

The first field can either contain a number indicating the tbl8 in which the lookup process should continue
or the next hop itself if the longest prefix match has already been found.
The two flags are used to determine whether the entry is valid or not and
whether the search process have finished or not respectively.
The depth or length of the rule is the number of bits of the rule that is stored in a specific entry.

An entry in a tbl8 contains the following fields:

*   next hop

*   valid

*   valid group

*   depth

Next hop and depth contain the same information as in the tbl24.
The two flags show whether the entry and the table are valid respectively.

The other main data structure is a table containing the main information about the rules (IP and next hop).
This is a higher level table, used for different things:

*   Check whether a rule already exists or not, prior to addition or deletion,
    without having to actually perform a lookup.

*   When deleting, to check whether there is a rule containing the one that is to be deleted.
    This is important, since the main data structure will have to be updated accordingly.

Addition
~~~~~~~~

When adding a rule, there are different possibilities.
If the rule's depth is exactly 24 bits, then:

*   Use the rule (IP address) as an index to the tbl24.

*   If the entry is invalid (i.e. it doesn't already contain a rule) then set its next hop to its value,
    the valid flag to 1 (meaning this entry is in use),
    and the external entry flag to 0
    (meaning the lookup process ends at this point, since this is the longest prefix that matches).

If the rule's depth is exactly 32 bits, then:

*   Use the first 24 bits of the rule as an index to the tbl24.

*   If the entry is invalid (i.e. it doesn't already contain a rule) then look for a free tbl8,
    set the index to the tbl8 to this value,
    the valid flag to 1 (meaning this entry is in use), and the external entry flag to 1
    (meaning the lookup process must continue since the rule hasn't been explored completely).

If the rule's depth is any other value, prefix expansion must be performed.
This means the rule is copied to all the entries (as long as they are not in use) which would also cause a match.

As a simple example, let's assume the depth is 20 bits.
This means that there are 2^(24 - 20) = 16 different combinations of the first 24 bits of an IP address that
would cause a match.
Hence, in this case, we copy the exact same entry to every position indexed by one of these combinations.

By doing this we ensure that during the lookup process, if a rule matching the IP address exists,
it is found in either one or two memory accesses,
depending on whether we need to move to the next table or not.
Prefix expansion is one of the keys of this algorithm,
since it improves the speed dramatically by adding redundancy.

Lookup
~~~~~~

The lookup process is much simpler and quicker. In this case:

*   Use the first 24 bits of the IP address as an index to the tbl24.
    If the entry is not in use, then it means we don't have a rule matching this IP.
    If it is valid and the external entry flag is set to 0, then the next hop is returned.

*   If it is valid and the external entry flag is set to 1,
    then we use the tbl8 index to find out the tbl8 to be checked,
    and the last 8 bits of the IP address as an index to this table.
    Similarly, if the entry is not in use, then we don't have a rule matching this IP address.
    If it is valid then the next hop is returned.

Limitations in the Number of Rules
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There are different things that limit the number of rules that can be added.
The first one is the maximum number of rules, which is a parameter passed through the API.
Once this number is reached,
it is not possible to add any more rules to the routing table unless one or more are removed.

The second reason is an intrinsic limitation of the algorithm.
As explained before, to avoid high memory consumption, the number of tbl8s is limited in compilation time
(this value is by default 256).
If we exhaust tbl8s, we won't be able to add any more rules.
How many of them are necessary for a specific routing table is hard to determine in advance.

A tbl8 is consumed whenever we have a new rule with depth bigger than 24,
and the first 24 bits of this rule are not the same as the first 24 bits of a rule previously added.
If they are, then the new rule will share the same tbl8 than the previous one,
since the only difference between the two rules is within the last byte.

With the default value of 256, we can have up to 256 rules longer than 24 bits that differ on their first three bytes.
Since routes longer than 24 bits are unlikely, this shouldn't be a problem in most setups.
Even if it is, however, the number of tbl8s can be modified.

Use Case: IPv4 Forwarding
~~~~~~~~~~~~~~~~~~~~~~~~~

The LPM algorithm is used to implement Classless Inter-Domain Routing (CIDR) strategy used by routers implementing IPv4 forwarding.

References
~~~~~~~~~~

*   RFC1519 Classless Inter-Domain Routing (CIDR): an Address Assignment and Aggregation Strategy,
    `http://www.ietf.org/rfc/rfc1519 <http://www.ietf.org/rfc/rfc1519>`_

*   Pankaj Gupta, Algorithms for Routing Lookups and Packet Classification, PhD Thesis, Stanford University,
    2000  (`http://klamath.stanford.edu/~pankaj/thesis/ thesis_1sided.pdf <http://klamath.stanford.edu/~pankaj/thesis/%20thesis_1sided.pdf>`_ )

.. |tbl24_tbl8| image:: img/tbl24_tbl8.*
