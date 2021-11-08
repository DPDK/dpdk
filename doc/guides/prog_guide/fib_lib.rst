..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 Intel Corporation.

FIB Library
===========

The FIB library provides a fast Longest Prefix Match (LPM) search for 32-bit
keys or 128-bit for IPv6. It can be used in a variety of applications,
the most typical of which is IPv4/IPv6 forwarding.

.. note::

   The API and implementation are very similar for IPv4 ``rte_fib`` API and IPv6 ``rte_fib6``
   API, therefore only the ``rte_fib`` API will be discussed here.
   Everything within this document except for the size of the prefixes is applicable to  the
   ``rte_fib6`` API.


FIB API Overview
----------------

The main configuration struct contains:

* Type of :ref:`dataplane algorithm <fib_dataplane_algorithms>`.

* Default next hop ID.

* The maximum number of routes.

* Settings for the data algorithm (:ref:`will be discussed later <fib_dataplane_algorithms>`).

A FIB rule consists of a prefix and an associated next hop ID. The prefix consists
of an IPv4 network address (``uint32_t``) and the corresponding prefix length.
The prefix serves as the key and the next hop ID as the value while doing an LPM
search within FIB. The size of the next hop ID is variable and must be configured
during initialization.

The main methods within the ``rte_fib`` API are:

* ``rte_fib_add()``: Add a new route with a corresponding next hop ID to the
  table or update the next hop ID if the prefix already exists in a table.

* ``rte_fib_delete()``: Delete an existing route from the table.

* ``rte_fib_lookup_bulk()``: Provides a bulk Longest Prefix Match (LPM) lookup function
  for a set of IP addresses, it will return a set of corresponding next hop IDs.


Implementation details
----------------------

Internally FIB contains the ``rte_rib`` data struct to help maintain the dataplane struct.
The dataplane struct is opaque, so that users can add their own algorithm implementations.

.. _fib_dataplane_algorithms:


Dataplane Algorithms
--------------------


Dummy
~~~~~

This algorithm uses ``rte_rib`` as a dataplane struct. Lookups are relatively slow,
but extra memory isn't used for the dataplane struct. This algorithm should only
be used for testing and debugging purposes.

This algorithm will be used if the ``RTE_FIB_DUMMY`` type is configured as the
dataplane algorithm on FIB creation.


DIR-24-8
~~~~~~~~

The current implementation of this algorithm uses a variation of the DIR-24-8
algorithm that trades memory usage for improved LPM lookup speed.
This algorithm allows the lookup operation to be performed using only a single
memory read access in most cases. In the statistically rare case where the best
match rule has a depth larger than 24, the lookup operation will require two
memory read accesses.

This algorithm will be used if the ``RTE_FIB_DIR24_8`` type is configured as the
dataplane algorithm on FIB creation.

The main FIB configuration struct stores the dataplane parameters inside ``dir24_8``
within the ``rte_fib_conf`` and it consists of:

* ``nh_sz``: The size of the entry containing the next hop ID.
  This could be 1, 2, 4 or 8 bytes long.
  Note that all available bits except one are used to store the actual next hop ID.

* ``num_tbl8``: The number of tbl8 groups, each group consists of 256 entries
  corresponding to the ``nh_sz`` size.

The main elements of the dataplane struct for the DIR-24-8 algorithm are:

* TBL24: An array with 2\ :sup:`24` entries, corresponding to the ``nh_sz`` size.

* TBL8: An array of ``num_tbl8`` tbl8 groups.

The lookup algorithms logic can be seen in :numref:`figure_dir_24_8_alg`:

.. _figure_dir_24_8_alg:

.. figure:: img/dir_24_8_alg.*

   DIR-24-8 Lookup algorithm

The first table ``tbl24``, is indexed using the first 24 bits of the IP address to be looked up,
while the second table(s) ``tbl8``, is indexed using the last 8 bits of the IP address.
This means that depending on the outcome of trying to match the IP address of an incoming packet
to a rule stored in the tbl24 we might need to continue the lookup process in the second level.

Since every entry of the tbl24 can potentially point to a tbl8,
ideally we would have 2\ :sup:`24` tbl8s, which would be the same as having a
single table with 2\ :sup:`32` entries. This is not feasible due to resource restrictions.
Instead, this approach takes advantage of the fact that rules longer than 24 bits are very rare.
By splitting the process into two different tables/levels and limiting the number of tbl8s,
we can greatly reduce memory consumption while maintaining a very good lookup speed.
This method generally results in one memory access per lookup.

An entry in a tbl8 contains the following fields:

* The next hop ID.

* 1 bit indicating if the lookup should proceed inside the tbl8.


Use cases
---------

The FIB library is useful for any use cases that rely on the Longest Prefix Match (LPM)
algorithm such as IP forwarding or packet classification.

More complex use cases are also possible, as it is possible to have next hop IDs
which are 63 bits long (using ``RTE_FIB_DIR24_8_8B`` as a next hop size).
These use cases could include storing two next hop IDs inside the 63 bits of the next hop.
This may be useful to provide a fallback next hop ID, ASN or forwarding class
corresponding to a given prefix without having to perform an additional lookup.
